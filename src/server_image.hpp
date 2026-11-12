// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <link.h>
#endif

namespace imomoe::server_fixes {

struct ServerImage {
    bool present{};
    bool ok{};
    std::uintptr_t base{};
    std::string reason;
#ifndef _WIN32
    const char* path{};
    const ElfW(Phdr)* phdr{};
    std::size_t phnum{};
#endif
};

#ifdef _WIN32
namespace server_image_detail {
inline ServerImage locate(const wchar_t* module, const char* name) {
    auto* const base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(module));
    if (!base) return {false, false, 0, std::string(name) + " not loaded"};
    const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return {true, false, reinterpret_cast<std::uintptr_t>(base), std::string("bad ") + name + " DOS header"};
    }
    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return {true, false, reinterpret_cast<std::uintptr_t>(base), std::string("bad ") + name + " PE header"};
    }
    return {true, true, reinterpret_cast<std::uintptr_t>(base), {}};
}
} // namespace server_image_detail

inline ServerImage LocateServerImage() {
    return server_image_detail::locate(L"server.dll", "server.dll");
}

inline ServerImage LocateEngineImage() {
    return server_image_detail::locate(L"engine2.dll", "engine2.dll");
}
#else
namespace server_image_detail {

struct Search {
    const char* name;
    int count{};
    ServerImage module;
};

inline int find_module(struct dl_phdr_info* info, std::size_t, void* opaque) {
    auto& search = *static_cast<Search*>(opaque);
    const char* path = info->dlpi_name;
    if (!path) return 0;
    const char* name = std::strrchr(path, '/');
    name = name ? name + 1 : path;
    if (std::strcmp(name, search.name) != 0) return 0;
    ++search.count;
    if (search.count == 1) {
        search.module.present = true;
        search.module.base = static_cast<std::uintptr_t>(info->dlpi_addr);
        search.module.path = path;
        search.module.phdr = info->dlpi_phdr;
        search.module.phnum = info->dlpi_phnum;
    }
    return search.count > 1 ? 1 : 0;
}

inline ServerImage locate(const char* name) {
    Search search{name, 0, {}};
    (void)::dl_iterate_phdr(&find_module, &search);
    if (search.count == 0) return {false, false, 0, std::string(name) + " not loaded"};
    if (search.count != 1) return {true, false, 0, std::string("duplicate ") + name + " modules"};
    search.module.ok = true;
    return search.module;
}
} // namespace server_image_detail

inline ServerImage LocateServerImage() {
    return server_image_detail::locate("libserver.so");
}

inline ServerImage LocateEngineImage() {
    return server_image_detail::locate("libengine2.so");
}
#endif

inline bool ContainsReadableRange(const ServerImage& image, std::uintptr_t address, std::size_t size) noexcept {
    if (!image.ok || address < image.base) return false;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) ||
        reinterpret_cast<std::uintptr_t>(memory.AllocationBase) != image.base ||
        memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD)) return false;
    switch (memory.Protect & 0xff) {
        case PAGE_READONLY: case PAGE_READWRITE: case PAGE_WRITECOPY:
        case PAGE_EXECUTE_READ: case PAGE_EXECUTE_READWRITE: case PAGE_EXECUTE_WRITECOPY:
            return size <= memory.RegionSize &&
                address - reinterpret_cast<std::uintptr_t>(memory.BaseAddress) <= memory.RegionSize - size;
        default: return false;
    }
#else
    const auto rva = address - image.base;
    for (std::size_t i = 0; i < image.phnum; ++i) {
        const auto& segment = image.phdr[i];
        if (segment.p_type == PT_LOAD && (segment.p_flags & PF_R) && rva >= segment.p_vaddr &&
            size <= segment.p_memsz && rva - segment.p_vaddr <= segment.p_memsz - size) return true;
    }
    return false;
#endif
}

} // namespace imomoe::server_fixes
