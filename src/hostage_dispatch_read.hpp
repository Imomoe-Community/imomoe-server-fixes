// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#include <sys/uio.h>
#include <unistd.h>
#else
#error "Hostage dispatch pointer reads are supported only on Windows and Linux"
#endif

namespace imomoe::server_fixes::hostage {

inline bool ReadHostageDispatchTarget( const void* dispatchArgument, std::int32_t& targetHandle ) noexcept {
	if ( !dispatchArgument ) {
		return false;
	}
	const auto base = reinterpret_cast<std::uintptr_t>( dispatchArgument );
	if ( base > std::numeric_limits<std::uintptr_t>::max() - 16 ) {
		return false;
	}

	std::uintptr_t handleStorage{};
	const auto readBytes = []( const void* source, void* destination, std::size_t size ) noexcept {
#if defined(_WIN32)
		SIZE_T bytesRead{};
		return ::ReadProcessMemory( ::GetCurrentProcess(), source, destination, size, &bytesRead ) != 0 &&
			bytesRead == size;
#else
		iovec local{ destination, size };
		iovec remote{ const_cast<void*>( source ), size };
		const auto bytesRead = ::syscall( SYS_process_vm_readv, ::getpid(), &local, 1, &remote, 1, 0 );
		return bytesRead == static_cast<long>( size );
#endif
	};
	if ( !readBytes( reinterpret_cast<const void*>( base + 8 ), &handleStorage, sizeof( handleStorage ) ) ||
		!handleStorage ) {
		return false;
	}
	std::int32_t decodedHandle{};
	if ( !readBytes( reinterpret_cast<const void*>( handleStorage ), &decodedHandle, sizeof( decodedHandle ) ) ) {
		return false;
	}
	targetHandle = decodedHandle;
	return true;
}

} // namespace imomoe::server_fixes::hostage
