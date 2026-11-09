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

template <typename Remove>
bool RecordStatusCommandAddResult( ListenerState& listener, bool& commandRegistered,
	bool commandAdded, bool callbackPresent, Remove&& remove ) noexcept {
	if ( commandAdded ) {
		commandRegistered = true;
		if ( !callbackPresent ) {
			listener.available = false;
			listener.reason = "status command callback registration failed";
			return false;
		}
		listener.registered = true;
		listener.available = true;
		listener.reason = "ready";
		return true;
	}

	// S2SDK can attach a listener to an inactive engine entry even if Add fails.
	listener.registered = true;
	listener.available = callbackPresent;
	if ( !UnregisterListener( listener, [&] {
		return std::forward<Remove>( remove )() || !callbackPresent;
	} ) ) {
		return false;
	}
	listener.available = false;
	listener.reason = "status command registration failed";
	return false;
}

template <typename HasCallbacks, typename RemoveCommand>
bool CompleteStatusCommandCleanup( bool& commandRegistered, HasCallbacks&& hasCallbacks,
	RemoveCommand&& removeCommand ) noexcept {
	if ( !commandRegistered ) {
		return true;
	}
	try {
		// Removing the final listener can destroy the command and unregister it
		// from the engine. If another plugin has a listener, leave that command
		// alone; this plugin no longer owns a callback on it.
		if ( hasCallbacks() ) {
			commandRegistered = false;
			return true;
		}
		if ( removeCommand() ) {
			if ( !hasCallbacks() ) {
				commandRegistered = false;
				return true;
			}
			return false;
		}
		// The command manager may already have removed it when the last
		// listener was unregistered. A false remove result is success only after
		// verifying that no listener remains.
		if ( !hasCallbacks() ) {
			commandRegistered = false;
			return true;
		}
	} catch ( ... ) {
	}
	return false;
}

} // namespace imomoe::server_fixes::chat
