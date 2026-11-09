// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include "server_image.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>

namespace imomoe::server_fixes {

// S2SDK scans a module snapshot. Check live code against its backing image before
// installing a hook, including when a detour belongs to a different provider.
inline bool ServerCodeUnmodified( const ServerImage& image, const void* target, std::size_t size ) {
	std::array<std::byte, 32> original{};
	const auto address = reinterpret_cast<std::uintptr_t>( target );
	if ( !image.ok || address < image.base || size > original.size() ) {
		return false;
	}
	const auto rva = address - image.base;
	const auto contains = [&]( std::uintptr_t start, std::size_t length ) {
		return rva >= start && size <= length && rva - start <= length - size;
	};
	std::filesystem::path path;
	std::optional<std::uint64_t> fileOffset;
#ifdef _WIN32
	const auto* base = reinterpret_cast<const std::byte*>( image.base );
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>( base );
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>( base + dos->e_lfanew );
	const auto* sections = IMAGE_FIRST_SECTION( nt );
	for ( unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i ) {
		const auto& section = sections[i];
		if ( ( section.Characteristics & IMAGE_SCN_MEM_EXECUTE ) && contains( section.VirtualAddress, section.SizeOfRawData ) ) {
			fileOffset = section.PointerToRawData + ( rva - section.VirtualAddress );
			break;
		}
	}
	std::wstring name( 32768, L'\0' );
	const auto length = GetModuleFileNameW( reinterpret_cast<HMODULE>( image.base ), name.data(), static_cast<DWORD>( name.size() ) );
	if ( !length || length >= name.size() ) {
		return false;
	}
	name.resize( length );
	path = name;
#else
	for ( std::size_t i = 0; i < image.phnum; ++i ) {
		const auto& segment = image.phdr[i];
		if ( segment.p_type == PT_LOAD && ( segment.p_flags & PF_X ) && contains( segment.p_vaddr, segment.p_filesz ) ) {
			fileOffset = segment.p_offset + ( rva - segment.p_vaddr );
			break;
		}
	}
	path = image.path;
#endif
	if ( !fileOffset ) {
		return false;
	}
	std::ifstream file( path, std::ios::binary );
	return file.seekg( static_cast<std::streamoff>( *fileOffset ) ).read(
		reinterpret_cast<char*>( original.data() ), static_cast<std::streamsize>( size ) ) &&
		std::memcmp( target, original.data(), size ) == 0;
}

} // namespace imomoe::server_fixes
