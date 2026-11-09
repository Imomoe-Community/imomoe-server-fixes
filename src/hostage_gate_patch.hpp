// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "server_image.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#ifndef _WIN32
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
        if (had_module && observed.ok) module_ = imomoe::server_fixes::LocateServerImage();
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
        module_ = imomoe::server_fixes::LocateServerImage();
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
    using Module = imomoe::server_fixes::ServerImage;

    const Module& cached_module() {
        if (!module_ || !module_->present) module_ = imomoe::server_fixes::LocateServerImage();
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
    static Thread current_thread() noexcept { return static_cast<Thread>(::syscall(SYS_gettid)); }

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
