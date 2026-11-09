// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "spectator_compat.hpp"

#include "spectator_compat_policy.hpp"

#include <s2sdk/s2sdk.hpp>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace imomoe::server_fixes::spectator_compat {
namespace {

constexpr std::string_view kConVarName = "mp_spectators_max";
constexpr std::string_view kGameRulesClass = "CCSGameRules";
constexpr std::string_view kSlotCountField = "m_iSpectatorSlotCount";
constexpr std::string_view kGameRulesChainField = "__m_pChainEntity";

struct ConVarLease {
	bool captured{};
	bool ownsCurrent{};
	bool yielded{};
	bool observed{};
	std::string previous;
	std::int64_t lastApplied{};
};

struct GameRulesLease {
	void* rules{};
	std::string map;
	bool captured{};
	bool ownsCurrent{};
	bool observed{};
	std::int64_t previous{};
	std::int64_t lastApplied{};
};

ConVarLease g_conVarLease;
GameRulesLease g_gameRulesLease;
bool g_gameRulesYielded{};
std::uint64_t g_conVarHandle{};
std::int32_t g_schemaOffset{ -2 };
std::int32_t g_schemaChainOffset{ -2 };
bool g_mapEndRegistered{};
std::string g_reason{ "not initialized" };

void OnMapEnd() noexcept;

std::string_view View( const plg::string& value ) noexcept {
	return { value.data(), value.size() };
}

bool ParseInteger( std::string_view text, std::int64_t& value ) noexcept {
	if ( text.empty() ) {
		return false;
	}
	const auto parsed = std::from_chars( text.data(), text.data() + text.size(), value );
	return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool ImportsAvailable() noexcept {
	return __s2sdk_FindConVar && __s2sdk_GetConVarValue && __s2sdk_SetConVarValue &&
		__s2sdk_GetMaxClients && __s2sdk_GetGameRules && __s2sdk_GetCurrentMap &&
		__s2sdk_GetSchemaOffset && __s2sdk_GetEntSchema2 && __s2sdk_SetEntSchema2 &&
		__s2sdk_OnMapEnd_Register && __s2sdk_OnMapEnd_Unregister;
}

bool RegisterMapEnd() noexcept {
	if ( g_mapEndRegistered ) {
		return true;
	}
	try {
		s2sdk::OnMapEnd_Register( OnMapEnd );
		g_mapEndRegistered = true;
		return true;
	} catch ( ... ) {
		return false;
	}
}

bool UnregisterMapEnd() noexcept {
	if ( !g_mapEndRegistered ) {
		return true;
	}
	try {
		s2sdk::OnMapEnd_Unregister( OnMapEnd );
		g_mapEndRegistered = false;
		return true;
	} catch ( ... ) {
		return false;
	}
}

std::uint64_t SpectatorConVar() noexcept {
	if ( g_conVarHandle != 0 ) {
		return g_conVarHandle;
	}
	try {
		g_conVarHandle = s2sdk::FindConVar( plg::string( kConVarName ) );
	} catch ( ... ) {
		g_conVarHandle = 0;
	}
	return g_conVarHandle;
}

bool ReadConVar( std::uint64_t handle, std::string& raw, std::int64_t& parsed, std::string& error ) {
	if ( handle == 0 ) {
		error = "mp_spectators_max was not found";
		return false;
	}
	const auto value = s2sdk::GetConVarValue( handle );
	raw.assign( value.data(), value.size() );
	if ( !ParseInteger( raw, parsed ) ) {
		error = "mp_spectators_max is not a valid integer";
		return false;
	}
	return true;
}

bool ApplyConVarCapacity( std::uint64_t handle, std::int64_t& target, bool& externalOwner, std::string& error ) {
	std::string raw;
	std::int64_t current{};
	if ( !ReadConVar( handle, raw, current, error ) ) {
		return false;
	}
	if ( g_conVarLease.yielded || ( g_conVarLease.observed && current != g_conVarLease.lastApplied ) ) {
		g_conVarLease.yielded = true;
		g_conVarLease.ownsCurrent = false;
		externalOwner = true;
		target = current;
		return true;
	}
	if ( !g_conVarLease.captured ) {
		g_conVarLease.previous = raw;
		g_conVarLease.captured = true;
	}

	if ( current == target ) {
		if ( g_conVarLease.ownsCurrent && current == g_conVarLease.lastApplied ) {
			g_conVarLease.lastApplied = target;
		}
		g_conVarLease.observed = true;
		g_conVarLease.lastApplied = target;
		return true;
	}

	const auto value = plg::string( std::to_string( target ) );
	s2sdk::SetConVarValue( handle, value, false, false );
	std::string appliedRaw;
	std::int64_t applied{};
	if ( !ReadConVar( handle, appliedRaw, applied, error ) ) {
		return false;
	}
	if ( applied != current ) {
		g_conVarLease.ownsCurrent = true;
		g_conVarLease.lastApplied = applied;
	}
	if ( applied != target ) {
		error = "mp_spectators_max did not retain the actual maxplayers value";
		return false;
	}
	g_conVarLease.ownsCurrent = true;
	g_conVarLease.lastApplied = target;
	g_conVarLease.observed = true;
	return true;
}

bool EnsureSchema( std::string& error ) {
	if ( g_schemaOffset == -2 ) {
		g_schemaOffset = s2sdk::GetSchemaOffset( plg::string( kGameRulesClass ), plg::string( kSlotCountField ) );
	}
	if ( g_schemaOffset < 0 ) {
		error = "CCSGameRules::m_iSpectatorSlotCount schema is unavailable";
		return false;
	}
	if ( g_schemaChainOffset == -2 ) {
		g_schemaChainOffset = s2sdk::GetSchemaOffset( plg::string( kGameRulesClass ), plg::string( kGameRulesChainField ) );
	}
	if ( g_schemaChainOffset < 0 ) {
		error = "CCSGameRules network-variable chain schema is unavailable";
		return false;
	}
	return true;
}

std::int64_t PreviousConVarCapacity() noexcept {
	std::int64_t previous{};
	return g_conVarLease.captured && ParseInteger( g_conVarLease.previous, previous ) ? previous : -1;
}

bool ApplyGameRulesCapacity( void* rules, std::string map, std::int64_t target, bool& externalOwner,
	std::string& error ) {
	if ( !rules || !EnsureSchema( error ) ) {
		if ( !rules ) {
			error = "current GameRules instance is unavailable";
		}
		return false;
	}
	const auto current = s2sdk::GetEntSchema2( rules, plg::string( kGameRulesClass ),
		plg::string( kSlotCountField ), 0 );
	if ( g_gameRulesYielded ) {
		externalOwner = true;
		if ( current < target ) {
			error = "GameRules spectator slot count has an external owner below mp_spectators_max";
			return false;
		}
		return true;
	}
	const bool newContext = !g_gameRulesLease.captured || g_gameRulesLease.rules != rules || g_gameRulesLease.map != map;
	if ( newContext ) {
		const auto previousConVar = PreviousConVarCapacity();
		const bool inheritedOurCVar = g_gameRulesLease.ownsCurrent &&
			current == g_gameRulesLease.lastApplied && previousConVar >= 0;
		const bool initializedFromOurCVar = g_conVarLease.ownsCurrent && current == target && previousConVar >= 0;
		g_gameRulesLease.rules = rules;
		g_gameRulesLease.map = std::move( map );
		g_gameRulesLease.captured = true;
		g_gameRulesLease.ownsCurrent = false;
		g_gameRulesLease.previous = inheritedOurCVar || initializedFromOurCVar ? previousConVar : current;
		g_gameRulesLease.observed = true;
		g_gameRulesLease.lastApplied = current;
	} else if ( g_gameRulesLease.observed && current != g_gameRulesLease.lastApplied ) {
		if ( current == target ) {
			// The engine may already have mirrored the ConVar change.
			g_gameRulesLease.lastApplied = current;
			g_gameRulesLease.ownsCurrent = g_conVarLease.ownsCurrent && current != g_gameRulesLease.previous;
			if ( !g_conVarLease.ownsCurrent ) {
				g_gameRulesLease.previous = current;
			}
			return true;
		}
		g_gameRulesLease.ownsCurrent = false;
		g_gameRulesYielded = true;
		externalOwner = true;
		if ( current < target ) {
			error = "GameRules spectator slot count has an external owner below mp_spectators_max";
			return false;
		}
		return true;
	}

	if ( current != target ) {
		s2sdk::SetEntSchema2( rules, plg::string( kGameRulesClass ), plg::string( kSlotCountField ),
			target, true, 0 );
		const auto applied = s2sdk::GetEntSchema2( rules, plg::string( kGameRulesClass ),
			plg::string( kSlotCountField ), 0 );
		if ( applied != current ) {
			g_gameRulesLease.ownsCurrent = true;
			g_gameRulesLease.lastApplied = applied;
		}
		if ( applied != target ) {
			error = "GameRules spectator slot count did not retain the target capacity";
			return false;
		}
		g_gameRulesLease.observed = true;
	}
	if ( current == target && g_gameRulesLease.ownsCurrent && g_gameRulesLease.lastApplied == current ) {
		g_gameRulesLease.lastApplied = target;
		return true;
	}
	g_gameRulesLease.lastApplied = target;
	g_gameRulesLease.ownsCurrent = g_gameRulesLease.previous != target &&
		( g_conVarLease.ownsCurrent || current != target );
	g_gameRulesLease.observed = true;
	return true;
}

bool RestoreGameRules( std::string& error, bool schemaQueriesReady ) noexcept {
	if ( !g_gameRulesLease.captured ) {
		return true;
	}
	if ( !g_gameRulesLease.ownsCurrent ) {
		g_gameRulesLease = {};
		return true;
	}
	if ( !schemaQueriesReady ) {
		error = "GameRules capacity restoration is waiting for map start";
		return false;
	}
	if ( !__s2sdk_GetGameRules || !__s2sdk_GetEntSchema2 || !__s2sdk_SetEntSchema2 ) {
		error = "GameRules ownership cannot be checked or restored with current imports";
		return false;
	}
	try {
		void* const rules = s2sdk::GetGameRules();
		if ( !rules ) {
			g_gameRulesLease = {};
			return true;
		}
		const auto currentMap = s2sdk::GetCurrentMap();
		const std::string map( View( currentMap ) );
		if ( rules != g_gameRulesLease.rules || map != g_gameRulesLease.map ) {
			g_gameRulesLease = {};
			return true;
		}
		const auto current = s2sdk::GetEntSchema2( rules, plg::string( kGameRulesClass ),
			plg::string( kSlotCountField ), 0 );
		if ( !ShouldRestoreOwnedValue( g_gameRulesLease.captured, g_gameRulesLease.ownsCurrent,
			current, g_gameRulesLease.lastApplied ) ) {
			g_gameRulesLease = {};
			return true;
		}
		s2sdk::SetEntSchema2( rules, plg::string( kGameRulesClass ), plg::string( kSlotCountField ),
			g_gameRulesLease.previous, true, 0 );
		const auto restored = s2sdk::GetEntSchema2( rules, plg::string( kGameRulesClass ),
			plg::string( kSlotCountField ), 0 );
		if ( restored != g_gameRulesLease.previous ) {
			error = "GameRules spectator slot count restoration did not read back";
			return false;
		}
		g_gameRulesLease = {};
		return true;
	} catch ( ... ) {
		error = "GameRules spectator slot count restoration threw";
		return false;
	}
}

bool RestoreConVar( std::string& error ) noexcept {
	if ( !g_conVarLease.captured ) {
		return true;
	}
	if ( !g_conVarLease.ownsCurrent ) {
		g_conVarLease = {};
		g_conVarHandle = 0;
		return true;
	}
	if ( !__s2sdk_GetConVarValue || !__s2sdk_SetConVarValue || !__s2sdk_FindConVar ) {
		error = "mp_spectators_max ownership cannot be checked or restored with current imports";
		return false;
	}
	try {
		const auto handle = SpectatorConVar();
		if ( handle == 0 ) {
			error = "mp_spectators_max was not found while checking ownership";
			return false;
		}
		const auto value = s2sdk::GetConVarValue( handle );
		std::string raw( View( value ) );
		std::int64_t current{};
		if ( !ParseInteger( raw, current ) ) {
			// A non-integer value cannot still be the integer this module wrote.
			// Relinquish ownership rather than overwriting another owner's value.
			g_conVarLease = {};
			g_conVarHandle = 0;
			return true;
		}
		if ( !ShouldRestoreOwnedValue( g_conVarLease.captured, g_conVarLease.ownsCurrent,
			current, g_conVarLease.lastApplied ) ) {
			g_conVarLease = {};
			g_conVarHandle = 0;
			return true;
		}
		s2sdk::SetConVarValue( handle, plg::string( g_conVarLease.previous ), false, false );
		std::string restored;
		std::int64_t restoredValue{};
		if ( !ReadConVar( handle, restored, restoredValue, error ) || restored != g_conVarLease.previous ) {
			error = "mp_spectators_max restoration did not read back";
			return false;
		}
		g_conVarLease = {};
		g_conVarHandle = 0;
		return true;
	} catch ( ... ) {
		error = "mp_spectators_max restoration threw";
		return false;
	}
}

bool RestoreOwnedConVarAtMapEnd( std::string& error ) {
	if ( !g_conVarLease.captured || !g_conVarLease.ownsCurrent ) {
		// In particular, do not clear a previously yielded external owner here.
		return true;
	}
	if ( !__s2sdk_GetConVarValue || !__s2sdk_SetConVarValue || !__s2sdk_FindConVar ) {
		error = "mp_spectators_max cannot be checked or restored at map end";
		return false;
	}
	try {
		const auto handle = SpectatorConVar();
		if ( handle == 0 ) {
			error = "mp_spectators_max was not found at map end";
			return false;
		}
		const auto value = s2sdk::GetConVarValue( handle );
		const std::string raw( View( value ) );
		std::int64_t current{};
		if ( !ParseInteger( raw, current ) || current != g_conVarLease.lastApplied ) {
			// The ConVar changed since our last observation. Preserve it and latch
			// the normal external-yield state for the remainder of this enabled cycle.
			g_conVarLease.ownsCurrent = false;
			g_conVarLease.yielded = true;
			g_conVarLease.observed = true;
			return true;
		}

		s2sdk::SetConVarValue( handle, plg::string( g_conVarLease.previous ), false, false );
		std::string restored;
		std::int64_t restoredValue{};
		if ( !ReadConVar( handle, restored, restoredValue, error ) || restored != g_conVarLease.previous ) {
			if ( restored != g_conVarLease.previous ) {
				std::int64_t actual{};
				if ( !ParseInteger( restored, actual ) || actual != g_conVarLease.lastApplied ) {
					g_conVarLease.ownsCurrent = false;
					g_conVarLease.yielded = true;
					g_conVarLease.observed = true;
				}
			}
			if ( error.empty() ) {
				error = "mp_spectators_max map-end restoration did not read back";
			}
			return false;
		}

		// Keep the original baseline for subsequent maps, and treat the restored
		// raw value as the last observation so a later external write still yields.
		g_conVarLease.ownsCurrent = false;
		g_conVarLease.lastApplied = restoredValue;
		g_conVarLease.observed = true;
		return true;
	} catch ( ... ) {
		error = "mp_spectators_max map-end restoration threw";
		return false;
	}
}

bool RestoreOwnedValues( std::string& error, bool schemaQueriesReady ) noexcept {
	std::string gameRulesError;
	std::string conVarError;
	const bool gameRulesClean = RestoreGameRules( gameRulesError, schemaQueriesReady );
	const bool conVarClean = RestoreConVar( conVarError );
	if ( !gameRulesClean ) {
		error = std::move( gameRulesError );
		if ( !conVarClean ) {
			error += "; ";
			error += conVarError;
		}
		return false;
	}
	if ( !conVarClean ) {
		error = std::move( conVarError );
		return false;
	}
	return true;
}

bool RestoreAndUnregister( std::string& error, bool schemaQueriesReady ) noexcept {
	std::string restoreError;
	const bool valuesClean = RestoreOwnedValues( restoreError, schemaQueriesReady );
	const bool callbackClean = UnregisterMapEnd();
	if ( !valuesClean ) {
		error = std::move( restoreError );
	}
	if ( !callbackClean ) {
		if ( !error.empty() ) {
			error += "; ";
		}
		error += "map-end callback removal failed";
	}
	return valuesClean && callbackClean;
}

void ResetCapacityCycle() noexcept {
	g_conVarLease = {};
	g_gameRulesLease = {};
	g_gameRulesYielded = false;
	g_conVarHandle = 0;
}

void SetReason( std::string reason ) noexcept {
	try {
		g_reason = std::move( reason );
	} catch ( ... ) {
		g_reason = "status text allocation failed";
	}
}

void LogMapEndFailure( std::string_view error ) noexcept {
	try {
		std::string message( "[ServerFixes] spectator_compat map-end restoration failed: " );
		message.append( error );
		s2sdk::PrintToServer( plg::string( message ) );
	} catch ( ... ) {
	}
}

void OnMapEnd() noexcept {
	// GameRules belongs to a level instance. Do not query or carry its lease over
	// the generation boundary. Restore the raw source while it is still safe so a
	// new GameRules instance cannot inherit our temporary capacity value.
	try {
		std::string error;
		if ( !RestoreOwnedConVarAtMapEnd( error ) ) {
			SetReason( "map-end spectator ConVar restoration needs retry: " + error );
			LogMapEndFailure( error );
		}
	} catch ( ... ) {
		SetReason( "map-end failure" );
	}
	g_gameRulesLease = {};
}

} // namespace

Status Update( bool enabled, bool schemaQueriesReady ) noexcept {
	try {
		if ( !enabled ) {
			std::string error;
			if ( !RestoreAndUnregister( error, schemaQueriesReady ) ) {
				SetReason( "disabled; owned capacity restoration needs retry: " + error );
				return { false, g_reason };
			}
			ResetCapacityCycle();
			SetReason( "disabled by configuration" );
			return { true, g_reason };
		}
		if ( !schemaQueriesReady ) {
			SetReason( "waiting for map start before querying GameRules capacity" );
			return { false, g_reason };
		}
		if ( !ImportsAvailable() ) {
			SetReason( "required S2SDK capacity or GameRules import unavailable" );
			return { false, g_reason };
		}
		if ( !RegisterMapEnd() ) {
			SetReason( "could not register GameRules map-generation cleanup" );
			return { false, g_reason };
		}
		const auto maxPlayers = s2sdk::GetMaxClients();
		if ( !HasUsableCapacity( maxPlayers ) ) {
			SetReason( "GetMaxClients returned no usable server capacity" );
			return { false, g_reason };
		}
		const auto handle = SpectatorConVar();
		if ( handle == 0 ) {
			SetReason( "mp_spectators_max was not found" );
			return { false, g_reason };
		}
		std::string error;
		void* const rules = s2sdk::GetGameRules();
		if ( !rules || !EnsureSchema( error ) ) {
			if ( !rules ) {
				error = "current GameRules instance is unavailable";
			}
			SetReason( std::move( error ) );
			return { false, g_reason };
		}
		std::int64_t target = maxPlayers;
		bool conVarExternalOwner = false;
		if ( !ApplyConVarCapacity( handle, target, conVarExternalOwner, error ) ) {
			SetReason( std::move( error ) );
			return { false, g_reason };
		}
		const auto currentMap = s2sdk::GetCurrentMap();
		const std::string map( View( currentMap ) );
		std::string previousError;
		bool gameRulesExternalOwner = false;
		if ( !ApplyGameRulesCapacity( rules, map, target, gameRulesExternalOwner, previousError ) ) {
			SetReason( std::move( previousError ) );
			return { false, g_reason };
		}
		std::string reason = gameRulesExternalOwner ? "enabled; GameRules spectator capacity has an external owner" :
			( conVarExternalOwner ? "enabled; GameRules follows mp_spectators_max" :
				"enabled; spectator capacity follows actual maxplayers" );
		if ( conVarExternalOwner ) {
			reason += "; mp_spectators_max has an external owner";
		}
		SetReason( std::move( reason ) );
		return { true, g_reason };
	} catch ( const std::exception& exception ) {
		SetReason( std::string( "spectator compatibility update failed: " ) + exception.what() );
		return { false, g_reason };
	} catch ( ... ) {
		SetReason( "spectator compatibility update failed" );
		return { false, g_reason };
	}
}

bool Shutdown() noexcept {
	std::string error;
	const bool clean = RestoreAndUnregister( error, true );
	if ( clean ) {
		ResetCapacityCycle();
	}
	return clean;
}

} // namespace imomoe::server_fixes::spectator_compat
