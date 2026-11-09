// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>

namespace imomoe::server_fixes::runtime_lifecycle {

class SchemaReadiness final {
public:
	// GetSchemaOffset is unsafe until S2SDK's post-ActivateServer map-start callback.
	void OnMapStart() noexcept { ready_ = true; }
	void OnMapEnd() noexcept { ready_ = false; }
	bool CanQuery() const noexcept { return ready_; }

	template <typename Action>
	bool RunSchemaDependent( Action&& action ) const {
		if ( !ready_ ) {
			return false;
		}
		action();
		return true;
	}

private:
	bool ready_{};
};

// Register before any module performs schema-dependent work. Map end closes
// schema access before notifying the consumer to drop map-local callbacks.
bool EnsureRegistered( void ( *onMapEnd )() noexcept = nullptr ) noexcept;
bool SchemaQueriesReady() noexcept;
std::uint64_t MapGeneration() noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::runtime_lifecycle
