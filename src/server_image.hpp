// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

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
    std::string identity;
    std::string reason;
#ifndef _WIN32
    const char* path{};
    const ElfW(Phdr)* phdr{};
    std::size_t phnum{};
#endif
};

#ifdef _WIN32
inline ServerImage LocateServerImage() {
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
#else
namespace server_image_detail {

inline constexpr std::string_view kExpectedBuildId = "5ea0e482b46eb71c4f9615c48b34ddc36f55ca40";

struct Search {
    int count{};
    ServerImage module;
};

inline int find_server(struct dl_phdr_info* info, std::size_t, void* opaque) {
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
        search.module.path = path;
        search.module.phdr = info->dlpi_phdr;
        search.module.phnum = info->dlpi_phnum;
    }
    return search.count > 1 ? 1 : 0;
}

inline bool build_id(const ServerImage& module, std::string& id) {
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

} // namespace server_image_detail

inline ServerImage LocateServerImage() {
    server_image_detail::Search search;
    (void)::dl_iterate_phdr(&server_image_detail::find_server, &search);
    if (search.count == 0) return {false, false, 0, {}, "libserver.so not loaded"};
    if (search.count != 1) return {true, false, 0, {}, "duplicate libserver.so modules"};
    if (!server_image_detail::build_id(search.module, search.module.identity)) {
        search.module.reason = "libserver.so GNU build ID unavailable";
        return search.module;
    }
    if (search.module.identity != server_image_detail::kExpectedBuildId) {
        search.module.reason = "libserver.so build ID is not the analyzed build 26445667 image";
        return search.module;
    }
    search.module.ok = true;
    return search.module;
}
#endif

} // namespace imomoe::server_fixes
