// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "spectator_compat_policy.hpp"

#include <cstdlib>
#include <iostream>

int main() {
	using imomoe::server_fixes::spectator_compat::HasUsableCapacity;
	using imomoe::server_fixes::spectator_compat::ShouldRestoreOwnedValue;

	if ( !HasUsableCapacity( 32 ) || HasUsableCapacity( 0 ) || HasUsableCapacity( -1 ) ) {
		std::cerr << "spectator capacity must use only a positive actual maxplayers value\n";
		return EXIT_FAILURE;
	}
	if ( !ShouldRestoreOwnedValue( true, true, 32, 32 ) ||
		ShouldRestoreOwnedValue( true, true, 31, 32 ) ||
		ShouldRestoreOwnedValue( true, false, 32, 32 ) ||
		ShouldRestoreOwnedValue( false, true, 32, 32 ) ) {
		std::cerr << "spectator restoration must write only while the plugin still owns the exact value\n";
		return EXIT_FAILURE;
	}
	std::cout << "spectator compatibility policy checks passed\n";
	return EXIT_SUCCESS;
}
