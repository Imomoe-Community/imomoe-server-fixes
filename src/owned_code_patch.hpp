// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "server_code.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace imomoe::server_fixes {

class OwnedCodePatch final {
public:
    bool active() const noexcept { return owned_.has_value(); }

    bool configured() const noexcept { return !original_.empty(); }

    template <typename Transform>
    bool configure(const void* address, std::size_t checked_size, Transform&& transform) {
        const auto module = LocateServerImage();
        if (active() || !ServerCodeUnmodified(module, address, checked_size)) return false;
        const auto* code = static_cast<const std::uint8_t*>(address);
        auto patched = transform(std::span{code, checked_size});
        if (patched.empty() || patched.size() > checked_size) return false;
        original_.assign(code, code + checked_size);
        rva_ = reinterpret_cast<std::uintptr_t>(address) - module.base;
        patched_ = std::move(patched);
        return true;
    }

    void reset_configuration() noexcept {
        original_.clear();
        patched_.clear();
    }

    bool code_recognized() const {
        if (!configured()) return false;
        if (owned_) {
            const auto* address = target(*owned_);
            return equals(address, unpatched()) || equals(address, patched_);
        }
        const auto module = LocateServerImage();
        const auto* address = target(module);
        return ContainsReadableRange(module, reinterpret_cast<std::uintptr_t>(address), original_.size()) &&
            equals(address, original_);
    }

    void game_thread_seen() noexcept { game_thread_ = current_thread(); }

    void enable() {
        if (!configured()) throw std::runtime_error("patch gamedata unavailable");
        if (owned_ && !protection_pending_ && equals(target(*owned_), patched_)) return;
        require_game_thread();
        if (owned_) {
            auto* address = target(*owned_);
            if (equals(address, patched_)) {
                finish_protection(address);
                return;
            }
            disable();
        }

        auto module = LocateServerImage();
        auto* address = target(module);
        if (!ServerCodeUnmodified(module, address, original_.size()) || !equals(address, original_)) {
            throw std::runtime_error("live instructions differ from the expected unpatched window");
        }
        // Keep this loader mapping alive while owning its bytes. Restore it
        // even if the file on disk has since been replaced by an update.
        if (!pin_mapping(module, address)) throw std::runtime_error("could not retain patch mapping");
        owned_.emplace(std::move(module));
        const auto write = write_code(*owned_, address, patched_, false, 0);
        if (!write.bytes_written) {
            clear_ownership();
            throw std::runtime_error("could not make the code page writable");
        }
        original_protection_ = write.original_protection;
        protection_pending_ = !write.protection_restored;
        if (!equals(address, patched_)) {
            throw std::runtime_error("unexpected patch readback; ownership retained");
        }
        if (protection_pending_) {
            throw std::runtime_error("patch is active but original page protection was not restored");
        }
    }

    void disable() {
        if (!owned_) return;
        require_game_thread();
        auto* address = target(*owned_);
        if (!equals(address, unpatched())) {
            if (!equals(address, patched_)) {
                throw std::runtime_error("code changed by another writer; preserving those bytes");
            }
            const auto write = write_code(*owned_, address, unpatched(), true, original_protection_);
            if (!write.bytes_written || !equals(address, unpatched())) {
                throw std::runtime_error("restoration failed; ownership retained");
            }
            protection_pending_ = !write.protection_restored;
        }
        finish_protection(address);
        clear_ownership();
    }

private:
    using Module = ServerImage;

    std::uint8_t* target(const Module& module) const noexcept {
        return reinterpret_cast<std::uint8_t*>(module.base + rva_);
    }

    std::span<const std::uint8_t> unpatched() const noexcept {
        return std::span(original_).first(patched_.size());
    }

    void finish_protection(std::uint8_t* address) {
        if (protection_pending_ && !restore_protection(address, original_protection_)) {
            throw std::runtime_error("original page protection is still pending; ownership retained");
        }
        protection_pending_ = false;
    }

    struct WriteResult {
        bool bytes_written{};
        bool protection_restored{};
        int original_protection{};
    };

#ifdef _WIN32
    using Thread = DWORD;
    static Thread current_thread() noexcept { return GetCurrentThreadId(); }

    static WriteResult write_code(const Module&, std::uint8_t* target, std::span<const std::uint8_t> bytes,
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

    bool restore_protection(std::uint8_t* target, int protection) noexcept {
        DWORD ignored{};
        return VirtualProtect(target, patched_.size(), static_cast<DWORD>(protection), &ignored) != 0;
    }
#else
    using Thread = long;
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

    static WriteResult write_code(const Module& module, std::uint8_t* target, std::span<const std::uint8_t> bytes,
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

    bool restore_protection(std::uint8_t* target, int protection) noexcept {
        std::uintptr_t first{};
        std::size_t span{};
        if (!page_range(target, patched_.size(), first, span)) return false;
        return ::mprotect(reinterpret_cast<void*>(first), span, protection) == 0;
    }
#endif

    static bool equals(const std::uint8_t* target, std::span<const std::uint8_t> bytes) noexcept {
        return std::memcmp(target, bytes.data(), bytes.size()) == 0;
    }

    void require_game_thread() const {
        if (game_thread_ == 0 || game_thread_ != current_thread()) {
            throw std::runtime_error("run this command after GameFrame has recorded the game thread");
        }
    }

    bool pin_mapping(const Module& module, const void* address) noexcept {
#ifdef _WIN32
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<LPCWSTR>(address), &pin_)) return false;
        if (reinterpret_cast<std::uintptr_t>(pin_) == module.base) return true;
        FreeLibrary(pin_);
#else
        (void)address;
        pin_ = dlopen(module.path, RTLD_NOLOAD | RTLD_LAZY);
        if (!pin_) return false;
        link_map* mapping{};
        if (dlinfo(pin_, RTLD_DI_LINKMAP, &mapping) == 0 && mapping && mapping->l_addr == module.base) return true;
        dlclose(pin_);
#endif
        pin_ = {};
        return false;
    }

    void clear_ownership() noexcept {
        owned_.reset();
#ifdef _WIN32
        FreeLibrary(pin_);
#else
        dlclose(pin_);
#endif
        pin_ = {};
        protection_pending_ = false;
        original_protection_ = 0;
    }

    std::optional<Module> owned_;
#ifdef _WIN32
    HMODULE pin_{};
#else
    void* pin_{};
#endif
    std::uintptr_t rva_{};
    std::vector<std::uint8_t> original_, patched_;
    bool protection_pending_{};
    int original_protection_{};
    Thread game_thread_{};
};

} // namespace imomoe::server_fixes
