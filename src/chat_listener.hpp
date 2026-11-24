// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <string>
#include <utility>

namespace imomoe::server_fixes::chat {

struct ListenerState {
	bool registered = false;
	bool available = false;
	std::string reason = "not initialized";
};

constexpr bool CanRunCallback( bool lifecycleActive, bool enabled ) noexcept {
	return lifecycleActive && enabled;
}

template <typename Add>
bool RegisterListener( ListenerState& state, Add&& add ) noexcept {
	if ( state.registered ) {
		return true;
	}

	try {
		if ( !std::forward<Add>( add )() ) {
			state.available = false;
			state.reason = "global command listener registration failed";
			return false;
		}
	} catch ( ... ) {
		state.available = false;
		state.reason = "global command listener registration threw";
		return false;
	}

	state.registered = true;
	state.available = true;
	state.reason = "ready";
	return true;
}

template <typename Remove>
bool UnregisterListener( ListenerState& state, Remove&& remove ) noexcept {
	if ( !state.registered ) {
		return true;
	}

	try {
		if ( !std::forward<Remove>( remove )() ) {
			state.reason = "listener removal failed; callback remains registered but inactive";
			return false;
		}
	} catch ( ... ) {
		state.reason = "listener removal threw; callback remains registered but inactive";
		return false;
	}

	state.registered = false;
	state.reason = state.available ? "disabled" : state.reason;
	return true;
}

} // namespace imomoe::server_fixes::chat
