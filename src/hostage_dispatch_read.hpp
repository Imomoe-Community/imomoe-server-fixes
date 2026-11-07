// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstring>
#include <limits>

namespace imomoe::server_fixes::hostage {

inline bool ReadHostageDispatchTarget( const void* dispatchArgument, std::int32_t& targetHandle ) noexcept {
	constexpr std::uintptr_t kMinUserAddress = 0x10000;
	constexpr std::uintptr_t kHandlePointerOffset = 8;
	const auto base = reinterpret_cast<std::uintptr_t>( dispatchArgument );
	if ( base < kMinUserAddress || base % alignof( std::uintptr_t ) != 0 ||
		base > std::numeric_limits<std::uintptr_t>::max() - kHandlePointerOffset - sizeof( std::uintptr_t ) ) {
		return false;
	}

	// The matched native handler reads these same fields on entry.
	std::uintptr_t handleStorage{};
	std::memcpy( &handleStorage, reinterpret_cast<const void*>( base + kHandlePointerOffset ), sizeof( handleStorage ) );
	if ( handleStorage < kMinUserAddress || handleStorage % alignof( std::int32_t ) != 0 ) {
		return false;
	}
	std::memcpy( &targetHandle, reinterpret_cast<const void*>( handleStorage ), sizeof( targetHandle ) );
	return true;
}

} // namespace imomoe::server_fixes::hostage
