// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>

namespace imomoe::server_fixes::spectator_compat {

constexpr bool HasUsableCapacity( std::int32_t maxPlayers ) noexcept {
	return maxPlayers > 0;
}

constexpr bool ShouldRestoreOwnedValue( bool captured, bool ownsCurrent, std::int64_t current,
	std::int64_t lastApplied ) noexcept {
	return captured && ownsCurrent && current == lastApplied;
}

} // namespace imomoe::server_fixes::spectator_compat
