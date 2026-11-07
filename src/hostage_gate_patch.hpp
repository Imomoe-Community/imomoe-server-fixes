// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <link.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace hostage {

class GatePatch final {
public:
    bool active() const noexcept { return active_; }

    bool code_recognized() {
        const auto& module = cached_module();
        if (!module.ok) return false;
        const auto* const target = reinterpret_cast<const std::uint8_t*>(module.base + kGateRva);
        return equals(target, kOriginal) || equals(target, kPatched);
    }

    void invalidate_module() noexcept { module_.reset(); }

    void game_thread_seen() noexcept { game_thread_ = current_thread(); }

    void enable() {
        const bool had_module = module_ && module_->present;
        const auto& observed = cached_module();
        if (active_ && !protection_pending_ && observed.ok && observed.base == base_ && observed.identity == identity_
            && equals(reinterpret_cast<const std::uint8_t*>(base_ + kGateRva), kPatched)) return;
        require_game_thread();
        // Recheck the mapping before changing bytes or recovering protection.
        if (had_module && observed.ok) module_ = locate_module();
        const auto& module = *module_;
        if (!module.ok) {
            if (!module.present) clear_ownership();
            throw std::runtime_error("unsupported module: " + module.reason);
        }
        auto* const target = reinterpret_cast<std::uint8_t*>(module.base + kGateRva);
        if (active_) {
            if (module.base != base_) {
                clear_ownership();
            } else if (module.identity != identity_) {
                throw std::runtime_error("original hostage module identity changed at the owned base; refusing a stale write");
            } else if (equals(target, kPatched)) {
                if (protection_pending_) {
                    if (!restore_protection(target, original_protection_)) {
                        throw std::runtime_error("hostage gate is active, but original page protection is still pending; retry sf_hostage_compat 0");
                    }
                    protection_pending_ = false;
                }
                return;
            } else if (equals(target, kOriginal)) {
                if (protection_pending_ && !restore_protection(target, original_protection_)) {
                    throw std::runtime_error("original gate bytes are present, but page protection restoration is pending");
                }
                clear_ownership();
            } else {
                throw std::runtime_error("hostage gate was changed by another writer; preserving those bytes");
            }
        }
        if (!equals(target, kOriginal)) {
            throw std::runtime_error("hostage gate bytes differ from the expected unpatched build; no write made");
        }

        const auto write = write_code(module, target, kPatched, false, 0);
        if (!write.bytes_written) {
            throw std::runtime_error("could not make the hostage gate code page writable");
        }
        if (!equals(target, kPatched)) {
            throw std::runtime_error("hostage gate write readback was unexpected; stop the server and inspect this build");
        }

        // Own the patch as soon as the expected bytes are present, even if the
        // OS rejected the final protection restore. That leaves `off` able to
        // retry restoration instead of losing the only recovery path.
        active_ = true;
        base_ = module.base;
        identity_ = module.identity;
        original_protection_ = write.original_protection;
        protection_pending_ = !write.protection_restored;
        if (protection_pending_) {
            throw std::runtime_error("hostage gate is active, but original page protection was not restored; retry sf_hostage_compat 0");
        }
    }

    void disable() {
        if (!active_) return;
        require_game_thread();
        module_ = locate_module();
        const auto& module = *module_;
        if (!module.present) {
            clear_ownership();
            return;
        }
        if (module.base != base_) {
            // There is exactly one module with this loader name. Its base moved,
            // so the patched mapping was unloaded rather than reused at an old
            // address. Do not touch the newly loaded image.
            clear_ownership();
            return;
        }
        if (!module.ok) {
            throw std::runtime_error("hostage module is still present at the owned base but no longer matches the analyzed build; refusing a stale write");
        }
        if (module.identity != identity_) {
            throw std::runtime_error("original hostage module identity changed at the owned base; refusing a stale write");
        }

        auto* const target = reinterpret_cast<std::uint8_t*>(base_ + kGateRva);
        if (equals(target, kOriginal)) {
            if (protection_pending_ && !restore_protection(target, original_protection_)) {
                throw std::runtime_error("original gate bytes are back, but page protection restoration is still pending; retry sf_hostage_compat 0");
            }
            clear_ownership();
            return;
        }
        if (!equals(target, kPatched)) {
            throw std::runtime_error("hostage gate was changed by another writer; preserving those bytes");
        }

        const auto write = write_code(module, target, kOriginal, true, original_protection_);
        if (!write.bytes_written) {
            throw std::runtime_error("could not make the owned hostage gate writable; ownership retained");
        }
        if (!equals(target, kOriginal)) {
            throw std::runtime_error("hostage gate restoration readback failed; ownership retained");
        }
        protection_pending_ = !write.protection_restored;
        if (protection_pending_) {
            throw std::runtime_error("original gate bytes are restored, but page protection restoration is pending; retry sf_hostage_compat 0");
        }
        clear_ownership();
    }

private:
    struct Module {
        bool present{};
        bool ok{};
        std::uintptr_t base{};
        std::string identity;
        std::string reason;
#ifndef _WIN32
        const ElfW(Phdr)* phdr{};
        std::size_t phnum{};
#endif
    };

    const Module& cached_module() {
        if (!module_ || !module_->present) module_ = locate_module();
        return *module_;
    }

    struct WriteResult {
        bool bytes_written{};
        bool protection_restored{};
        int original_protection{};
    };

#ifdef _WIN32
    using Thread = DWORD;
    static constexpr std::uintptr_t kGateRva = 0xAC7735;
    static constexpr std::array<std::uint8_t, 9> kOriginal{0x80, 0xBD, 0x44, 0x03, 0x00, 0x00, 0x03, 0x75, 0x30};
    static constexpr std::array<std::uint8_t, 9> kPatched{0xF6, 0x85, 0x44, 0x03, 0x00, 0x00, 0x02, 0x74, 0x30};
    static Thread current_thread() noexcept { return GetCurrentThreadId(); }

    static Module locate_module() {
        auto* const base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(L"server.dll"));
        if (!base) return {false, false, 0, {}, "server.dll not loaded"};
        const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
            return {true, false, reinterpret_cast<std::uintptr_t>(base), {}, "bad server.dll DOS header"};
        }
        const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
            || nt->FileHeader.TimeDateStamp != 0x6AEBC501 || nt->OptionalHeader.SizeOfImage != 0x257C000) {
            return {true, false, reinterpret_cast<std::uintptr_t>(base), {}, "server.dll is not the analyzed build 26445667 image"};
        }
        return {true, true, reinterpret_cast<std::uintptr_t>(base), "timestamp=0x6aebc501,size=0x257c000", {}};
    }

    template <std::size_t N>
    static WriteResult write_code(const Module&, std::uint8_t* target, const std::array<std::uint8_t, N>& bytes,
                                  bool use_saved_protection, int saved_protection) noexcept {
        DWORD observed{};
        if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &observed)) return {};
        const DWORD restore_to = use_saved_protection ? static_cast<DWORD>(saved_protection) : observed;
        std::memcpy(target, bytes.data(), bytes.size());
        const BOOL flushed = FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
        DWORD ignored{};
        const BOOL restored = VirtualProtect(target, bytes.size(), restore_to, &ignored);
        return {true, flushed && restored, static_cast<int>(restore_to)};
    }

    static bool restore_protection(std::uint8_t* target, int protection) noexcept {
        DWORD ignored{};
        return VirtualProtect(target, kOriginal.size(), static_cast<DWORD>(protection), &ignored) != 0;
    }
#else
    using Thread = long;
    static constexpr std::uintptr_t kGateRva = 0x15AF2EB;
    static constexpr std::array<std::uint8_t, 14> kOriginal{
        0x41, 0x80, 0xBD, 0x24, 0x06, 0x00, 0x00, 0x03, 0x0F, 0x84, 0x07, 0x01, 0x00, 0x00};
    static constexpr std::array<std::uint8_t, 14> kPatched{
        0x41, 0xF6, 0x85, 0x24, 0x06, 0x00, 0x00, 0x02, 0x0F, 0x85, 0x07, 0x01, 0x00, 0x00};
    static constexpr std::string_view kExpectedBuildId = "5ea0e482b46eb71c4f9615c48b34ddc36f55ca40";
    static Thread current_thread() noexcept { return static_cast<Thread>(::syscall(SYS_gettid)); }

    struct Search {
        int count{};
        Module module;
    };

    static int find_server(struct dl_phdr_info* info, std::size_t, void* opaque) {
        auto& search = *static_cast<Search*>(opaque);
        const char* path = info->dlpi_name;
        if (!path) return 0;
        const char* name = std::strrchr(path, '/');
        name = name ? name + 1 : path;
        if (std::strcmp(name, "libserver.so") != 0) return 0;
        ++search.count;
        if (search.count == 1) {
            search.module.present = true;
            search.module.base = static_cast<std::uintptr_t>(info->dlpi_addr);
            search.module.phdr = info->dlpi_phdr;
            search.module.phnum = info->dlpi_phnum;
        }
        return search.count > 1 ? 1 : 0;
    }

    static bool build_id(const Module& module, std::string& id) {
        for (std::size_t i = 0; i < module.phnum; ++i) {
            const auto& phdr = module.phdr[i];
            if (phdr.p_type != PT_NOTE) continue;
            const auto* cursor = reinterpret_cast<const std::uint8_t*>(module.base + phdr.p_vaddr);
            const auto* const end = cursor + phdr.p_memsz;
            while (cursor <= end && static_cast<std::size_t>(end - cursor) >= sizeof(ElfW(Nhdr))) {
                ElfW(Nhdr) header{};
                std::memcpy(&header, cursor, sizeof(header));
                cursor += sizeof(header);
                const auto name_size = (static_cast<std::size_t>(header.n_namesz) + 3U) & ~std::size_t{3U};
                const auto desc_size = (static_cast<std::size_t>(header.n_descsz) + 3U) & ~std::size_t{3U};
                if (name_size > static_cast<std::size_t>(end - cursor)
                    || desc_size > static_cast<std::size_t>(end - cursor) - name_size) return false;
                const auto* const name = cursor;
                const auto* const desc = cursor + name_size;
                if (header.n_type == NT_GNU_BUILD_ID && header.n_namesz == 4
                    && std::memcmp(name, "GNU", 4) == 0 && header.n_descsz <= 64) {
                    static constexpr char hex[] = "0123456789abcdef";
                    id.reserve(header.n_descsz * 2U);
                    for (std::size_t j = 0; j < header.n_descsz; ++j) {
                        id.push_back(hex[desc[j] >> 4]);
                        id.push_back(hex[desc[j] & 0x0F]);
                    }
                    return true;
                }
                cursor += name_size + desc_size;
            }
        }
        return false;
    }

    static bool page_range(std::uint8_t* target, std::size_t length, std::uintptr_t& first,
                           std::size_t& span) noexcept {
        const long raw_page_size = ::sysconf(_SC_PAGESIZE);
        if (raw_page_size <= 0) return false;
        const auto page_size = static_cast<std::uintptr_t>(raw_page_size);
        const auto mask = page_size - 1U;
        const auto address = reinterpret_cast<std::uintptr_t>(target);
        first = address & ~mask;
        const auto end = (address + length + mask) & ~mask;
        span = static_cast<std::size_t>(end - first);
        return span != 0;
    }

    static bool segment_protection(const Module& module, std::uint8_t* target, std::size_t length,
                                   int& protection) noexcept {
        const auto address = reinterpret_cast<std::uintptr_t>(target);
        if (address < module.base) return false;
        const auto rva = address - module.base;
        for (std::size_t i = 0; i < module.phnum; ++i) {
            const auto& phdr = module.phdr[i];
            if (phdr.p_type != PT_LOAD || rva < phdr.p_vaddr || length > phdr.p_memsz
                || rva - phdr.p_vaddr > phdr.p_memsz - length) continue;
            protection = ((phdr.p_flags & PF_R) ? PROT_READ : 0)
                | ((phdr.p_flags & PF_W) ? PROT_WRITE : 0)
                | ((phdr.p_flags & PF_X) ? PROT_EXEC : 0);
            return true;
        }
        return false;
    }

    static Module locate_module() {
        Search search;
        (void)::dl_iterate_phdr(&find_server, &search);
        if (search.count == 0) return {false, false, 0, {}, "libserver.so not loaded"};
        if (search.count != 1) return {true, false, 0, {}, "duplicate libserver.so modules"};
        if (!build_id(search.module, search.module.identity)) {
            search.module.reason = "libserver.so GNU build ID unavailable";
            return search.module;
        }
        if (search.module.identity != kExpectedBuildId) {
            search.module.reason = "libserver.so build ID is not the analyzed build 26445667 image";
            return search.module;
        }
        search.module.ok = true;
        return search.module;
    }

    template <std::size_t N>
    static WriteResult write_code(const Module& module, std::uint8_t* target, const std::array<std::uint8_t, N>& bytes,
                                  bool use_saved_protection, int saved_protection) noexcept {
        int protection = 0;
        if (!module.ok || !segment_protection(module, target, bytes.size(), protection)) return {};
        std::uintptr_t first{};
        std::size_t span{};
        if (!page_range(target, bytes.size(), first, span)) return {};
        const int restore_to = use_saved_protection ? saved_protection : protection;
        if (::mprotect(reinterpret_cast<void*>(first), span, protection | PROT_WRITE) != 0) return {};
        std::memcpy(target, bytes.data(), bytes.size());
        __builtin___clear_cache(reinterpret_cast<char*>(target), reinterpret_cast<char*>(target + bytes.size()));
        const bool restored = ::mprotect(reinterpret_cast<void*>(first), span, restore_to) == 0;
        return {true, restored, restore_to};
    }

    static bool restore_protection(std::uint8_t* target, int protection) noexcept {
        std::uintptr_t first{};
        std::size_t span{};
        if (!page_range(target, kOriginal.size(), first, span)) return false;
        return ::mprotect(reinterpret_cast<void*>(first), span, protection) == 0;
    }
#endif

    template <std::size_t N>
    static bool equals(const std::uint8_t* target, const std::array<std::uint8_t, N>& bytes) noexcept {
        return std::memcmp(target, bytes.data(), bytes.size()) == 0;
    }

    void require_game_thread() const {
        if (game_thread_ == 0 || game_thread_ != current_thread()) {
            throw std::runtime_error("run this command after GameFrame has recorded the game thread");
        }
    }

    void clear_ownership() noexcept {
        active_ = false;
        protection_pending_ = false;
        base_ = 0;
        identity_.clear();
        original_protection_ = 0;
    }

    std::optional<Module> module_;
    bool active_{};
    bool protection_pending_{};
    std::uintptr_t base_{};
    std::string identity_;
    int original_protection_{};
    Thread game_thread_{};
};

} // namespace hostage
