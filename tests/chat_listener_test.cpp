// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "chat_listener.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void Check( bool condition, std::string_view name ) {
	if ( !condition ) {
		std::cerr << "FAILED: " << name << '\n';
		std::exit( 1 );
	}
}

} // namespace

int main() {
	using imomoe::server_fixes::chat::ListenerState;
	using imomoe::server_fixes::chat::CanRunCallback;
	using imomoe::server_fixes::chat::RegisterListener;
	using imomoe::server_fixes::chat::UnregisterListener;

	ListenerState state;
	Check( !RegisterListener( state, [] { return false; } ), "false AddCommandListener is a registration failure" );
	Check( !state.registered && !state.available, "failed registration does not fake owned state" );
	Check( std::string_view( state.reason ) == "global command listener registration failed", "failed registration retains its reason" );

	Check( RegisterListener( state, [] { return true; } ), "successful AddCommandListener registers" );
	Check( state.registered && state.available && std::string_view( state.reason ) == "ready", "successful registration records ownership" );

	Check( !UnregisterListener( state, [] { return false; } ), "false RemoveCommandListener is a cleanup failure" );
	Check( state.registered && state.available, "failed removal keeps pending registration state" );
	Check( std::string_view( state.reason ) == "listener removal failed; callback remains registered but inactive", "failed removal retains its reason" );

	Check( UnregisterListener( state, [] { return true; } ), "successful removal clears registration" );
	Check( !state.registered && state.available && std::string_view( state.reason ) == "disabled", "successful removal records the disabled state" );

	bool lifecycleActive = true;
	const bool liveCvar = true;
	Check( CanRunCallback( lifecycleActive, liveCvar ), "an active plugin honors an enabled chat ConVar" );
	lifecycleActive = false;
	Check( !CanRunCallback( lifecycleActive, liveCvar ), "End keeps a residual callback inactive even when the ConVar remains true" );
	Check( !CanRunCallback( lifecycleActive, liveCvar ), "updates after End do not reopen chat registration" );

	std::cout << "chat listener lifecycle checks passed\n";
	return 0;
}
