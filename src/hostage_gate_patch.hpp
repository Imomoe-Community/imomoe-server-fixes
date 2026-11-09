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
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace hostage {

class GatePatch final {
public:
    bool active() const noexcept { return active_; }

    bool configured() const noexcept { return !original_.empty(); }

    bool configure(const void* address) {
        const auto& module = cached_module();
        if (!imomoe::server_fixes::ServerCodeUnmodified(module, address, 14)) return false;
        const auto* code = static_cast<const std::uint8_t*>(address);
        // cmp byte ptr [reg+disp32], CT; jnz rel8 (Windows) or jz rel32 (Linux).
        // Change the comparison to test for the T bit and invert the condition.
        const std::size_t rex = code[0] == 0x41 ? 1 : 0;
        const auto* cmp = code + rex;
        if (cmp[0] != 0x80 || cmp[1] != 0xBD || cmp[6] != 3) return false;
        const bool near_jump = cmp[7] == 0x0F && cmp[8] == 0x84;
        if (!near_jump && cmp[7] != 0x75) return false;
        const auto length = rex + (near_jump ? 13 : 9);
        std::vector<std::uint8_t> original(code, code + length);
        auto patched = original;
        patched[rex] = 0xF6; // TEST r/m8, imm8
        patched[rex + 1] &= 0xC7; // Change the ModR/M opcode extension from /7 to /0.
        patched[rex + 6] = 2;
        patched[rex + (near_jump ? 8 : 7)] ^= 1;
        gate_rva_ = reinterpret_cast<std::uintptr_t>(address) - module.base;
        patched_ = std::move(patched);
        original_ = std::move(original);
        return true;
    }

    void reset_configuration() noexcept {
        original_.clear();
        patched_.clear();
        invalidate_module();
    }

    bool code_recognized() {
        if (!configured()) return false;
        const auto& module = cached_module();
        if (!module.ok) return false;
        const auto* const target = reinterpret_cast<const std::uint8_t*>(module.base + gate_rva_);
        return equals(target, original_) || equals(target, patched_);
    }

    void invalidate_module() noexcept { module_.reset(); }

    void game_thread_seen() noexcept { game_thread_ = current_thread(); }

    void enable() {
        if (!configured()) throw std::runtime_error("hostage gate gamedata unavailable");
        const bool had_module = module_ && module_->present;
        const auto& observed = cached_module();
        if (active_ && !protection_pending_ && observed.ok && observed.base == base_ && observed.identity == identity_
            && equals(reinterpret_cast<const std::uint8_t*>(base_ + gate_rva_), patched_)) return;
        require_game_thread();
        // Recheck the mapping before changing bytes or recovering protection.
        if (had_module && observed.ok) module_ = imomoe::server_fixes::LocateServerImage();
        const auto& module = *module_;
        if (!module.ok) {
            if (!module.present) clear_ownership();
            throw std::runtime_error("unsupported module: " + module.reason);
        }
        auto* const target = reinterpret_cast<std::uint8_t*>(module.base + gate_rva_);
        if (active_) {
            if (module.base != base_) {
                clear_ownership();
            } else if (module.identity != identity_) {
                throw std::runtime_error("original hostage module identity changed at the owned base; refusing a stale write");
            } else if (equals(target, patched_)) {
                if (protection_pending_) {
                    if (!restore_protection(target, original_protection_)) {
                        throw std::runtime_error("hostage gate is active, but original page protection is still pending; retry sf_hostage_compat 0");
                    }
                    protection_pending_ = false;
                }
                return;
            } else if (equals(target, original_)) {
                if (protection_pending_ && !restore_protection(target, original_protection_)) {
                    throw std::runtime_error("original gate bytes are present, but page protection restoration is pending");
                }
                clear_ownership();
            } else {
                throw std::runtime_error("hostage gate was changed by another writer; preserving those bytes");
            }
        }
        if (!equals(target, original_)) {
            throw std::runtime_error("hostage gate bytes differ from the expected unpatched build; no write made");
        }

        const auto write = write_code(module, target, patched_, false, 0);
        if (!write.bytes_written) {
            throw std::runtime_error("could not make the hostage gate code page writable");
        }
        if (!equals(target, patched_)) {
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

        auto* const target = reinterpret_cast<std::uint8_t*>(base_ + gate_rva_);
        if (equals(target, original_)) {
            if (protection_pending_ && !restore_protection(target, original_protection_)) {
                throw std::runtime_error("original gate bytes are back, but page protection restoration is still pending; retry sf_hostage_compat 0");
            }
            clear_ownership();
            return;
        }
        if (!equals(target, patched_)) {
            throw std::runtime_error("hostage gate was changed by another writer; preserving those bytes");
        }

        const auto write = write_code(module, target, original_, true, original_protection_);
        if (!write.bytes_written) {
            throw std::runtime_error("could not make the owned hostage gate writable; ownership retained");
        }
        if (!equals(target, original_)) {
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
        return VirtualProtect(target, original_.size(), static_cast<DWORD>(protection), &ignored) != 0;
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
        if (!page_range(target, original_.size(), first, span)) return false;
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

    void clear_ownership() noexcept {
        active_ = false;
        protection_pending_ = false;
        base_ = 0;
        identity_.clear();
        original_protection_ = 0;
    }

    std::optional<Module> module_;
    std::uintptr_t gate_rva_{};
    std::vector<std::uint8_t> original_, patched_;
    bool active_{};
    bool protection_pending_{};
    std::uintptr_t base_{};
    std::string identity_;
    int original_protection_{};
    Thread game_thread_{};
};

} // namespace hostage
