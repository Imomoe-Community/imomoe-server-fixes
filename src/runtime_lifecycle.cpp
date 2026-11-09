// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "runtime_lifecycle.hpp"

#include <s2sdk/s2sdk.hpp>

namespace imomoe::server_fixes::runtime_lifecycle {
namespace {

SchemaReadiness g_schemaReadiness;
std::uint64_t g_mapGeneration{ 1 };
bool g_registrationComplete{};
bool g_mapStartRegistered{};
bool g_mapEndRegistered{};
void ( *g_onMapEnd )() noexcept{};

void MapStartCallback() noexcept {
	if ( g_registrationComplete ) {
		g_schemaReadiness.OnMapStart();
	}
}

void MapEndCallback() noexcept {
	g_schemaReadiness.OnMapEnd();
	++g_mapGeneration;
	if ( g_onMapEnd ) {
		g_onMapEnd();
	}
}

} // namespace

bool EnsureRegistered( void ( *onMapEnd )() noexcept ) noexcept {
	if ( g_registrationComplete ) {
		return true;
	}
	if ( !Shutdown() ) {
		return false;
	}
	if ( !__s2sdk_OnMapStart_Register || !__s2sdk_OnMapStart_Unregister ||
		!__s2sdk_OnMapEnd_Register || !__s2sdk_OnMapEnd_Unregister ) {
		return false;
	}
	g_onMapEnd = onMapEnd;
	try {
		s2sdk::OnMapEnd_Register( MapEndCallback );
		g_mapEndRegistered = true;
		s2sdk::OnMapStart_Register( MapStartCallback );
		g_mapStartRegistered = true;
		g_registrationComplete = true;
		return true;
	} catch ( ... ) {
		(void)Shutdown();
		return false;
	}
}

bool SchemaQueriesReady() noexcept {
	return g_schemaReadiness.CanQuery();
}

std::uint64_t MapGeneration() noexcept {
	return g_mapGeneration;
}

bool Shutdown() noexcept {
	g_registrationComplete = false;
	g_onMapEnd = nullptr;
	g_schemaReadiness.OnMapEnd();
	const auto unregister = []( bool& registered, auto remove, auto callback ) noexcept {
		if ( !registered ) {
			return true;
		}
		try {
			if ( remove ) {
				remove( callback );
				registered = false;
				return true;
			}
		} catch ( ... ) {
		}
		return false;
	};
	const bool startClean = unregister( g_mapStartRegistered, __s2sdk_OnMapStart_Unregister, MapStartCallback );
	const bool endClean = unregister( g_mapEndRegistered, __s2sdk_OnMapEnd_Unregister, MapEndCallback );
	return startClean && endClean;
}

} // namespace imomoe::server_fixes::runtime_lifecycle
