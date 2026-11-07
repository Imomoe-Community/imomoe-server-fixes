// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "spawn_capacity.hpp"
#include "runtime_lifecycle.hpp"

#include "spawn_capacity_policy.hpp"
#include "spawn_capacity_storage.hpp"

#include <s2sdk/s2sdk.hpp>
#include <eiface.h>
#include <tier0/characterset.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace imomoe::server_fixes::spawn_capacity {
namespace {

constexpr std::size_t kMaxManualSpawnPoints = 256;
constexpr std::size_t kMaxSpawnEntitiesToEnumerate = 4096;
constexpr std::string_view kSpawnAddCommand = "sf_spawn_add";
constexpr std::string_view kSpawnListCommand = "sf_spawn_list";
constexpr std::string_view kSpawnRemoveCommand = "sf_spawn_remove";
constexpr std::string_view kSpawnSaveCommand = "sf_spawn_save";
constexpr std::string_view kJoinTeamCommand = "jointeam";
constexpr std::string_view kJoinFailedEvent = "jointeam_failed";
constexpr std::string_view kRoundPrestartEvent = "round_prestart";
constexpr std::string_view kPlayerDeathEvent = "player_death";
constexpr std::string_view kPlayerSpawnEvent = "player_spawn";
constexpr std::string_view kGameRulesClass = "CCSGameRules";
constexpr std::string_view kSpawnEnabledField = "m_bEnabled";
constexpr std::string_view kSpawnEntitySchemaClass = "SpawnPoint";
constexpr std::array<std::string_view, 4> kSpawnCapacityFields{
	"m_iSpawnPointCount_Terrorist", "m_iSpawnPointCount_CT", "m_iMaxNumTerrorists", "m_iMaxNumCTs",
};
constexpr std::array<std::string_view, 2> kTeamMaximumFields{ "m_iMaxNumTerrorists", "m_iMaxNumCTs" };

struct NativeTeamCapacity {
	std::int32_t spawnPoints{};
	std::int32_t teamMaximum{};
	std::int32_t effective{};
};

struct SpawnPoint {
	std::uint32_t id{};
	Team team{ Team::Terrorist };
	plg::vec3 origin{};
	plg::vec3 angles{};
	std::int32_t entityHandle{ -1 };
};

struct OwnedSpawn {
	std::int32_t handle{ -1 };
	std::uint32_t manualId{};
	Team team{ Team::Terrorist };
	bool automatic{};
	bool disabledForRemoval{};
	bool disabledByUs{};
	bool nativeEligibleBeforeRemoval{};
	bool nativeExclusionVerified{};
	bool removalRequested{};
	bool batchRetirementPending{};
	std::int64_t enabledBeforeRemoval{ -1 };
	std::int32_t nativePointsBeforeRemoval{};
	plg::vec3 validatedOrigin{};
	bool locationValidated{};
};

struct TeamMaximumLease {
	void* rules{};
	std::string map;
	std::uint64_t mapGeneration{};
	bool captured{};
	bool ownsCurrent{};
	bool observed{};
	std::int64_t previous{};
	std::int64_t lastApplied{};
};

struct JoinAttempt {
	std::int32_t requestedTeam{};
	JoinRetryIdentity identity;
	std::int32_t failureReason{};
	std::string map;
	bool retryEligible{};
};

struct PendingRetry {
	std::int32_t slot{};
	std::int32_t requestedTeam{};
	JoinRetryIdentity identity;
	std::int32_t failureReason{};
	std::string map;
};

std::vector<SpawnPoint> g_nativePoints;
void* g_nativeSourceRules{};
std::uint64_t g_nativeSourceMapGeneration{};
std::vector<SpawnPoint> g_manualPoints;
std::vector<OwnedSpawn> g_ownedSpawns;
std::vector<PendingRetry> g_pendingRetries;
std::vector<std::optional<JoinAttempt>> g_joinAttempts;
std::vector<JoinRetryEpoch> g_joinEpochs;
std::vector<bool> g_retryDispatchActive;
std::array<NativeTeamCapacity, 2> g_effectiveTeamCapacities{};
std::array<std::int32_t, 4> g_spawnCapacitySchemaOffsets{ -2, -2, -2, -2 };
std::int32_t g_spawnEnabledSchemaOffset{ -2 };
std::array<TeamMaximumLease, 2> g_teamMaximumLeases{};
std::array<bool, 2> g_teamMaximumYielded{};
bool g_teamMaximumCapacityBlocked{};
MapLifecycle g_mapLifecycle;
std::string g_currentMap;
std::string g_mapReason{ "not initialized" };
std::string g_moduleReason{ "not initialized" };
std::uint32_t g_nextManualId{ 1 };
std::int32_t g_capacityMaxClients{};
s2sdk::ConfigId g_spawnGameConfig{};
void* g_nativeSpawnRecount{};
void* g_nativeSpawnEligibility{};
std::array<std::int32_t, 2> g_spawnListOffsets{ -1, -1 };
bool g_enabled{};
bool g_mapPrepared{};
bool g_joinPreRegistered{};
bool g_joinFailedRegistered{};
bool g_roundPrestartRegistered{};
bool g_playerDeathRegistered{};
bool g_playerSpawnRegistered{};
bool g_mapStartRegistered{};
bool g_mapEndRegistered{};
bool g_runtimeHooksRegistered{};
bool g_mapLoadAttempted{};
bool g_mapDataLoaded{};
bool g_rebuildPending{};
bool g_capacityReconcilePending{};
bool g_manualPointsDirty{};
bool g_nativeRefreshPending{};
bool g_capacityReadbackAvailable{};
std::uint64_t g_mapGeneration{};

s2sdk::ResultType OnSpawnAddCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept;
s2sdk::ResultType OnSpawnListCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept;
s2sdk::ResultType OnSpawnRemoveCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept;
s2sdk::ResultType OnSpawnSaveCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept;
s2sdk::ResultType OnJoinCommandPre( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept;
s2sdk::ResultType OnJoinFailedEvent( const plg::string& name, void* event, bool dontBroadcast ) noexcept;
s2sdk::ResultType OnRoundPrestartEvent( const plg::string& name, void* event, bool dontBroadcast ) noexcept;
s2sdk::ResultType OnPlayerLifeBoundaryEvent( const plg::string& name, void* event, bool dontBroadcast ) noexcept;
void OnMapStart() noexcept;
void OnMapEnd() noexcept;

std::string_view View( const plg::string& value ) noexcept {
	return { value.data(), value.size() };
}

bool EqualsAsciiCaseInsensitive( std::string_view left, std::string_view right ) noexcept {
	if ( left.size() != right.size() ) {
		return false;
	}
	for ( std::size_t i = 0; i < left.size(); ++i ) {
		const auto l = left[i] >= 'A' && left[i] <= 'Z' ? static_cast<char>( left[i] - 'A' + 'a' ) : left[i];
		const auto r = right[i] >= 'A' && right[i] <= 'Z' ? static_cast<char>( right[i] - 'A' + 'a' ) : right[i];
		if ( l != r ) {
			return false;
		}
	}
	return true;
}

bool HasRequiredImports() noexcept {
	return __s2sdk_AddConsoleCommand != nullptr && __s2sdk_RemoveCommand != nullptr &&
		__s2sdk_HasConsoleCommandCallbacks != nullptr && __s2sdk_GetAllConCommands != nullptr &&
		__s2sdk_AddCommandListener != nullptr && __s2sdk_RemoveCommandListener != nullptr &&
		__s2sdk_HookEvent != nullptr && __s2sdk_UnhookEvent != nullptr &&
		__s2sdk_GetEventPlayerSlot != nullptr && __s2sdk_GetEventInt != nullptr &&
		__s2sdk_QueryInterface != nullptr && __s2sdk_PlayerSlotToEntHandle != nullptr &&
		__s2sdk_IsClientInGame != nullptr && __s2sdk_IsClientAlive != nullptr &&
		__s2sdk_GetClientPawn != nullptr && __s2sdk_GetClientTeam != nullptr &&
		__s2sdk_GetMaxClients != nullptr && __s2sdk_FindEntityByClassname != nullptr &&
		__s2sdk_CreateEntityByName != nullptr && __s2sdk_DispatchSpawn2 != nullptr &&
		__s2sdk_RemoveEntity != nullptr && __s2sdk_GetEntityClassname != nullptr &&
		__s2sdk_GetEntityAbsOrigin != nullptr && __s2sdk_GetEntityEyeAngles != nullptr &&
		__s2sdk_EntPointerToEntHandle != nullptr && __s2sdk_EntHandleToEntPointer != nullptr &&
		__s2sdk_IsValidEntHandle != nullptr && __s2sdk_GetCurrentMap != nullptr &&
		__s2sdk_GetGameDirectory != nullptr && __s2sdk_OnMapStart_Register != nullptr &&
		__s2sdk_OnMapStart_Unregister != nullptr && __s2sdk_OnMapEnd_Register != nullptr &&
		__s2sdk_OnMapEnd_Unregister != nullptr && __s2sdk_GetSchemaOffset != nullptr &&
		__s2sdk_GetEntSchema2 != nullptr && __s2sdk_SetEntSchema2 != nullptr &&
		__s2sdk_GetGameRules != nullptr && __s2sdk_LoadGameConfigFile != nullptr &&
		__s2sdk_GetGameConfigSignature != nullptr && __s2sdk_CloseGameConfigFile != nullptr;
}

bool IsValidHandle( std::int32_t handle ) noexcept {
	if ( handle == -1 || !__s2sdk_IsValidEntHandle ) {
		return false;
	}
	try {
		return s2sdk::IsValidEntHandle( handle );
	} catch ( ... ) {
		return false;
	}
}

bool IsCompleteHandle( std::int32_t handle ) noexcept {
	if ( !IsValidHandle( handle ) ) {
		return false;
	}
	try {
		void* const entity = s2sdk::EntHandleToEntPointer( handle );
		return entity != nullptr && s2sdk::EntPointerToEntHandle( entity ) == handle;
	} catch ( ... ) {
		return false;
	}
}

bool EnsureSpawnCapacitySchema( std::string& error ) noexcept {
	if ( !__s2sdk_GetSchemaOffset || !__s2sdk_GetEntSchema2 ) {
		error = "S2SDK schema imports for native spawn capacity are unavailable";
		return false;
	}
	try {
		for ( std::size_t index = 0; index < kSpawnCapacityFields.size(); ++index ) {
			if ( g_spawnCapacitySchemaOffsets[index] == -2 ) {
				g_spawnCapacitySchemaOffsets[index] = s2sdk::GetSchemaOffset( plg::string( kGameRulesClass ),
					plg::string( kSpawnCapacityFields[index] ) );
			}
			if ( g_spawnCapacitySchemaOffsets[index] < 0 ) {
				error = "current CCSGameRules native spawn-capacity schema is unavailable";
				return false;
			}
		}
		return true;
	} catch ( ... ) {
		error = "resolving native spawn-capacity schema threw";
		return false;
	}
}

bool EnsureSpawnEnabledSchema( std::string& error ) noexcept {
	if ( !__s2sdk_GetSchemaOffset || !__s2sdk_GetEntSchema2 || !__s2sdk_SetEntSchema2 ) {
		error = "S2SDK schema imports for native spawn enabled state are unavailable";
		return false;
	}
	try {
		if ( g_spawnEnabledSchemaOffset == -2 ) {
			g_spawnEnabledSchemaOffset = s2sdk::GetSchemaOffset( plg::string( kSpawnEntitySchemaClass ),
				plg::string( kSpawnEnabledField ) );
		}
		if ( g_spawnEnabledSchemaOffset < 0 ) {
			error = "SpawnPoint::m_bEnabled schema is unavailable for spawn retirement";
			return false;
		}
		return true;
	} catch ( ... ) {
		error = "resolving native spawn enabled-state schema threw";
		return false;
	}
}

bool ReadNativeTeamCapacities( std::array<NativeTeamCapacity, 2>& capacities, std::string& error ) noexcept {
	if ( !__s2sdk_GetGameRules || !EnsureSpawnCapacitySchema( error ) ) {
		if ( !__s2sdk_GetGameRules ) {
			error = "S2SDK GetGameRules import is unavailable for native spawn capacity";
		}
		return false;
	}
	try {
		void* const rules = s2sdk::GetGameRules();
		if ( !rules ) {
			error = "current GameRules instance is unavailable for native spawn capacity";
			return false;
		}
		const auto read = [rules]( std::string_view field ) {
			return s2sdk::GetEntSchema2( rules, plg::string( kGameRulesClass ), plg::string( field ), 0 );
		};
		const auto terroristSpawnPoints = read( kSpawnCapacityFields[0] );
		const auto counterTerroristSpawnPoints = read( kSpawnCapacityFields[1] );
		const auto maxTerrorists = read( kSpawnCapacityFields[2] );
		const auto maxCounterTerrorists = read( kSpawnCapacityFields[3] );
		constexpr auto kMaxInt = static_cast<std::int64_t>( std::numeric_limits<std::int32_t>::max() );
		if ( terroristSpawnPoints < 0 || terroristSpawnPoints > kMaxInt ||
			counterTerroristSpawnPoints < 0 || counterTerroristSpawnPoints > kMaxInt ||
			maxTerrorists < 0 || maxTerrorists > kMaxInt ||
			maxCounterTerrorists < 0 || maxCounterTerrorists > kMaxInt ) {
			error = "Build 26445667 native spawn capacity fields returned an invalid value";
			return false;
		}
		capacities = {
			NativeTeamCapacity{
				.spawnPoints = static_cast<std::int32_t>( terroristSpawnPoints ),
				.teamMaximum = static_cast<std::int32_t>( maxTerrorists ),
				.effective = EffectiveTeamCapacity( static_cast<std::int32_t>( terroristSpawnPoints ),
					static_cast<std::int32_t>( maxTerrorists ) ),
			},
			NativeTeamCapacity{
				.spawnPoints = static_cast<std::int32_t>( counterTerroristSpawnPoints ),
				.teamMaximum = static_cast<std::int32_t>( maxCounterTerrorists ),
				.effective = EffectiveTeamCapacity( static_cast<std::int32_t>( counterTerroristSpawnPoints ),
					static_cast<std::int32_t>( maxCounterTerrorists ) ),
			},
		};
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "reading native spawn capacity failed: " ) + exception.what();
	} catch ( ... ) {
		error = "reading native spawn capacity failed";
	}
	return false;
}

std::size_t TeamIndex( Team team ) noexcept {
	return team == Team::Terrorist ? 0u : 1u;
}

bool ReadTeamMaximum( void* rules, Team team, std::int64_t& value, std::string& error ) {
	if ( !rules || !EnsureSpawnCapacitySchema( error ) ) {
		if ( !rules ) {
			error = "current GameRules instance is unavailable for team-capacity ownership";
		}
		return false;
	}
	try {
		value = s2sdk::GetEntSchema2( rules, plg::string( kGameRulesClass ),
			plg::string( kTeamMaximumFields[TeamIndex( team )] ), 0 );
		if ( value < 0 || value > std::numeric_limits<std::int32_t>::max() ) {
			error = "native per-team maximum returned an invalid value";
			return false;
		}
		return true;
	} catch ( ... ) {
		error = "reading native per-team maximum failed";
		return false;
	}
}

void SetTeamMaximum( void* rules, Team team, std::int64_t value ) {
	s2sdk::SetEntSchema2( rules, plg::string( kGameRulesClass ),
		plg::string( kTeamMaximumFields[TeamIndex( team )] ), value, false, 0 );
}

bool EnsureTeamMaximumCapacity( void* rules, std::string_view map, Team team, std::int32_t target,
	bool& externalOwner, std::string& error ) noexcept {
	const auto index = TeamIndex( team );
	std::int64_t current{};
	if ( !ReadTeamMaximum( rules, team, current, error ) ) {
		return false;
	}
	auto& lease = g_teamMaximumLeases[index];
	if ( lease.captured && ( lease.rules != rules || lease.map != map || lease.mapGeneration != g_mapGeneration ) ) {
		lease = {};
	}
	if ( g_teamMaximumYielded[index] ) {
		externalOwner = true;
		if ( current < target ) {
			error = std::string( team == Team::Terrorist ? "T" : "CT" ) +
				" max-team capacity has an external owner below actual maxplayers";
			return false;
		}
		return current >= target;
	}
	if ( lease.captured && lease.observed && current != lease.lastApplied ) {
		lease.ownsCurrent = false;
		g_teamMaximumYielded[index] = true;
		externalOwner = true;
		if ( current < target ) {
			error = std::string( team == Team::Terrorist ? "T" : "CT" ) +
				" max-team capacity has an external owner below actual maxplayers";
			return false;
		}
		return current >= target;
	}
	if ( current >= target ) {
		if ( !lease.captured ) {
			lease.rules = rules;
			lease.map.assign( map );
			lease.mapGeneration = g_mapGeneration;
			lease.captured = true;
			lease.previous = current;
		}
		lease.observed = true;
		lease.lastApplied = current;
		return true;
	}
	if ( !lease.captured ) {
		lease.rules = rules;
		lease.map.assign( map );
		lease.mapGeneration = g_mapGeneration;
		lease.captured = true;
		lease.previous = current;
	}
	try {
		SetTeamMaximum( rules, team, target );
		std::int64_t applied{};
		if ( !ReadTeamMaximum( rules, team, applied, error ) ) {
			return false;
		}
		if ( applied != target ) {
			error = "native per-team maximum did not retain actual maxplayers";
			return false;
		}
		lease.ownsCurrent = true;
		lease.observed = true;
		lease.lastApplied = applied;
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "setting native per-team maximum failed: " ) + exception.what();
	} catch ( ... ) {
		error = "setting native per-team maximum failed";
	}
	return false;
}

bool RefreshObservedTeamMaximums( void* rules, std::string_view map, std::int32_t target,
	std::string& error ) noexcept {
	if ( !g_capacityReadbackAvailable ) {
		return true;
	}
	try {
		if ( target <= 0 || !rules ) {
			error = target <= 0 ? "GetMaxClients returned no usable player capacity" :
				"current GameRules instance is unavailable for team-capacity readback";
			g_teamMaximumCapacityBlocked = true;
			return false;
		}
		auto capacities = g_effectiveTeamCapacities;
		constexpr std::array<Team, 2> teams{ Team::Terrorist, Team::CounterTerrorist };
		bool sufficient = true;
		for ( const auto team : teams ) {
			const auto index = TeamIndex( team );
			auto& lease = g_teamMaximumLeases[index];
			if ( !lease.captured || !lease.observed || lease.rules != rules || lease.map != map ||
				lease.mapGeneration != g_mapGeneration ) {
				error = "native per-team maximum ownership cannot be checked against the current spawn readback";
				g_teamMaximumCapacityBlocked = true;
				return false;
			}
			std::int64_t current{};
			if ( !ReadTeamMaximum( rules, team, current, error ) ) {
				g_teamMaximumCapacityBlocked = true;
				return false;
			}
			if ( current != lease.lastApplied ) {
				lease.ownsCurrent = false;
				g_teamMaximumYielded[index] = true;
			}
			capacities[index].teamMaximum = static_cast<std::int32_t>( current );
			capacities[index].effective = EffectiveTeamCapacity( capacities[index].spawnPoints,
				capacities[index].teamMaximum );
			if ( current < target || capacities[index].effective < target ) {
				sufficient = false;
				if ( error.empty() ) {
					error = std::string( team == Team::Terrorist ? "T" : "CT" ) +
						( g_teamMaximumYielded[index] ?
							" max-team capacity has an external owner below actual maxplayers" :
							" max-team capacity is below actual maxplayers" );
				}
			}
		}
		g_effectiveTeamCapacities = capacities;
		g_teamMaximumCapacityBlocked = !sufficient;
		if ( !sufficient ) {
			return false;
		}
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "reading current native team capacity failed: " ) + exception.what();
	} catch ( ... ) {
		error = "reading current native team capacity failed";
	}
	g_teamMaximumCapacityBlocked = true;
	return false;
}

bool RestoreTeamMaximumLease( Team team, std::string& error ) noexcept {
	const auto index = TeamIndex( team );
	auto& lease = g_teamMaximumLeases[index];
	if ( !lease.captured ) {
		return true;
	}
	if ( !lease.ownsCurrent ) {
		lease = {};
		return true;
	}
	if ( !__s2sdk_GetGameRules || !__s2sdk_GetCurrentMap || !__s2sdk_GetEntSchema2 || !__s2sdk_SetEntSchema2 ) {
		error = "native per-team maximum ownership cannot be checked or restored";
		return false;
	}
	try {
		void* const rules = s2sdk::GetGameRules();
		const auto currentMap = s2sdk::GetCurrentMap();
		if ( !rules || rules != lease.rules || View( currentMap ) != lease.map ||
			g_mapGeneration != lease.mapGeneration ) {
			lease = {};
			return true;
		}
		std::int64_t current{};
		if ( !ReadTeamMaximum( rules, team, current, error ) ) {
			return false;
		}
		if ( current != lease.lastApplied ) {
			g_teamMaximumYielded[index] = true;
			lease = {};
			return true;
		}
		SetTeamMaximum( rules, team, lease.previous );
		std::int64_t restored{};
		if ( !ReadTeamMaximum( rules, team, restored, error ) || restored != lease.previous ) {
			if ( error.empty() ) {
				error = "native per-team maximum restoration did not read back";
			}
			return false;
		}
		lease = {};
		return true;
	} catch ( ... ) {
		error = "native per-team maximum restoration threw";
		return false;
	}
}

bool RestoreTeamMaximums( std::string& error ) noexcept {
	std::string terroristError;
	std::string counterTerroristError;
	const bool terroristClean = RestoreTeamMaximumLease( Team::Terrorist, terroristError );
	const bool counterTerroristClean = RestoreTeamMaximumLease( Team::CounterTerrorist, counterTerroristError );
	if ( !terroristClean ) {
		error = std::move( terroristError );
	}
	if ( !counterTerroristClean ) {
		if ( !error.empty() ) {
			error += "; ";
		}
		error += counterTerroristError;
	}
	return terroristClean && counterTerroristClean;
}

void ResetTeamMaximumCycle() noexcept {
	g_teamMaximumLeases = {};
	g_teamMaximumYielded = {};
	g_teamMaximumCapacityBlocked = false;
}

bool ResolveNativeSpawnRecount( std::string& error ) noexcept {
	if ( g_nativeSpawnRecount && g_nativeSpawnEligibility && g_spawnListOffsets[0] >= 0 &&
		g_spawnListOffsets[1] >= 0 ) {
		return true;
	}
	if ( !__s2sdk_LoadGameConfigFile || !__s2sdk_GetGameConfigSignature || !__s2sdk_GetGameConfigOffset ||
		!__s2sdk_CloseGameConfigFile ) {
		error = "S2SDK gamedata imports for native spawn-list refresh are unavailable";
		return false;
	}
	try {
		if ( g_spawnGameConfig == 0 ) {
			plg::string configPath = plg::GetPlugin()->GetLocation();
			configPath += "/gamedata/imomoe_server_fixes.jsonc";
			plg::vector<plg::string> paths;
			paths.push_back( configPath );
			g_spawnGameConfig = s2sdk::LoadGameConfigFile( paths );
			if ( g_spawnGameConfig == 0 ) {
				error = "could not load gamedata/imomoe_server_fixes.jsonc for native spawn-list refresh";
				return false;
			}
		}
		g_nativeSpawnRecount = s2sdk::GetGameConfigSignature( g_spawnGameConfig,
			plg::string( "ServerFixes::RegisterOrRecountSpawnPoints" ) );
		if ( !g_nativeSpawnRecount ) {
			error = "current native spawn-list refresh signature is unavailable";
			return false;
		}
		g_nativeSpawnEligibility = s2sdk::GetGameConfigSignature( g_spawnGameConfig,
			plg::string( "ServerFixes::SpawnPointEligible" ) );
		if ( !g_nativeSpawnEligibility ) {
			error = "current native spawn-point eligibility signature is unavailable";
			return false;
		}
		g_spawnListOffsets = {
			s2sdk::GetGameConfigOffset( g_spawnGameConfig, "ServerFixes::TSpawnList" ),
			s2sdk::GetGameConfigOffset( g_spawnGameConfig, "ServerFixes::CTSpawnList" ),
		};
		if ( g_spawnListOffsets[0] < 0 || g_spawnListOffsets[1] < 0 ) {
			error = "current native primary spawn-list offsets are unavailable";
			return false;
		}
		return true;
	} catch ( ... ) {
		error = "resolving native spawn-list refresh gamedata threw";
		return false;
	}
}

bool CloseSpawnGameConfig() noexcept {
	if ( g_spawnGameConfig == 0 ) {
		g_nativeSpawnRecount = nullptr;
		g_nativeSpawnEligibility = nullptr;
		return true;
	}
	if ( !__s2sdk_CloseGameConfigFile ) {
		return false;
	}
	try {
		s2sdk::CloseGameConfigFile( g_spawnGameConfig );
		g_spawnGameConfig = 0;
		g_nativeSpawnRecount = nullptr;
		g_nativeSpawnEligibility = nullptr;
		return true;
	} catch ( ... ) {
		return false;
	}
}

bool RecountNativeSpawnLists( std::string& error ) noexcept {
	if ( !ResolveNativeSpawnRecount( error ) ) {
		return false;
	}
	if ( !__s2sdk_GetGameRules ) {
		error = "S2SDK GetGameRules import is unavailable for native spawn-list refresh";
		return false;
	}
	try {
		void* const rules = s2sdk::GetGameRules();
		if ( !rules ) {
			error = "current GameRules instance is unavailable for native spawn-list refresh";
			return false;
		}
		// SpawnPoint::Spawn already registers new full handles. The null argument
		// rebuilds enabled/identity-checked capacity without the LevelInit scanner's
		// live-player occupancy test.
		using RecountFunction = void ( * )( void*, void* );
		const auto recount = reinterpret_cast<RecountFunction>( g_nativeSpawnRecount );
		recount( rules, nullptr );
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "native spawn-list refresh failed: " ) + exception.what();
	} catch ( ... ) {
		error = "native spawn-list refresh failed";
	}
	return false;
}

bool NativeSpawnPointEligible( std::int32_t handle, std::string& error ) noexcept {
	if ( !IsCompleteHandle( handle ) ) {
		return false;
	}
	if ( !ResolveNativeSpawnRecount( error ) || !EnsureSpawnEnabledSchema( error ) ) {
		return false;
	}
	try {
		void* const rules = s2sdk::GetGameRules();
		void* const entity = s2sdk::EntHandleToEntPointer( handle );
		if ( !rules || !entity ) {
			return false;
		}
		const auto enabled = s2sdk::GetEntSchema2( entity, plg::string( kSpawnEntitySchemaClass ),
			plg::string( kSpawnEnabledField ), 0 );
		if ( enabled == 0 ) {
			return false;
		}
		using EligibilityFunction = std::uint8_t ( * )( void*, void*, std::uint8_t* );
		const auto isEligible = reinterpret_cast<EligibilityFunction>( g_nativeSpawnEligibility );
		return isEligible( rules, entity, nullptr ) != 0;
	} catch ( const std::exception& exception ) {
		error = std::string( "native spawn-point eligibility check failed: " ) + exception.what();
	} catch ( ... ) {
		error = "native spawn-point eligibility check failed";
	}
	return false;
}

std::string_view ClassnameFor( Team team ) noexcept {
	return team == Team::Terrorist ? "info_player_terrorist" : "info_player_counterterrorist";
}

// Read-only Build 26445667 primary-vector layout, verified in both native
// registration helpers. Capacity counts remain exclusively engine-produced.
struct NativeSpawnList {
	std::int32_t count{};
	std::int32_t padding{};
	const std::int32_t* handles{};
	std::int32_t capacity{};
	std::uint32_t flags{};
};
static_assert( sizeof( NativeSpawnList ) == 0x18 );
static_assert( offsetof( NativeSpawnList, handles ) == 0x8 );

bool NativeSpawnRegistered( std::int32_t handle, Team team, std::string& error ) noexcept {
	if ( !IsCompleteHandle( handle ) || !ResolveNativeSpawnRecount( error ) || !EnsureSpawnEnabledSchema( error ) ) {
		return false;
	}
	try {
		void* const rules = s2sdk::GetGameRules();
		void* const entity = s2sdk::EntHandleToEntPointer( handle );
		if ( !rules || !entity || View( s2sdk::GetEntityClassname( handle ) ) != ClassnameFor( team ) ||
			s2sdk::GetEntSchema2( entity, plg::string( kSpawnEntitySchemaClass ), plg::string( kSpawnEnabledField ), 0 ) == 0 ) {
			return false;
		}
		NativeSpawnList list;
		std::memcpy( &list, static_cast<const std::byte*>( rules ) + g_spawnListOffsets[TeamIndex( team )], sizeof( list ) );
		if ( list.count < 0 || list.capacity < list.count || ( list.count != 0 && !list.handles ) ) {
			error = "native primary spawn-list layout is invalid";
			return false;
		}
		for ( std::int32_t i = 0; i < list.count; ++i ) {
			if ( list.handles[i] == handle ) {
				return true;
			}
		}
	} catch ( ... ) {
		error = "reading native spawn registration failed";
	}
	return false;
}

bool HasRegisteredTemplate( const SpawnPoint& point, std::string& error ) noexcept;

bool IsValidPoint( const SpawnPoint& point ) noexcept {
	constexpr float kWorldBound = 32768.0f;
	constexpr float kAngleBound = 36000.0f;
	const std::array<float, 3> coordinates{ point.origin.x, point.origin.y, point.origin.z };
	const std::array<float, 3> angles{ point.angles.x, point.angles.y, point.angles.z };
	return point.id != 0 && IsPlayableTeam( static_cast<std::int32_t>( point.team ) ) &&
		std::all_of( coordinates.begin(), coordinates.end(), []( float value ) {
			return std::isfinite( value ) && std::abs( value ) <= kWorldBound;
		} ) &&
		std::all_of( angles.begin(), angles.end(), []( float value ) {
			return std::isfinite( value ) && std::abs( value ) <= kAngleBound;
		} );
}

std::string FormatVector( const plg::vec3& value ) {
	std::ostringstream output;
	output.imbue( std::locale::classic() );
	output << std::setprecision( std::numeric_limits<float>::max_digits10 )
		<< value.x << ' ' << value.y << ' ' << value.z;
	return output.str();
}

bool SamePosition( const plg::vec3& left, const plg::vec3& right ) noexcept {
	constexpr float kPositionTolerance = 0.125f;
	return std::isfinite( left.x ) && std::isfinite( left.y ) && std::isfinite( left.z ) &&
		std::abs( left.x - right.x ) <= kPositionTolerance &&
		std::abs( left.y - right.y ) <= kPositionTolerance &&
		std::abs( left.z - right.z ) <= kPositionTolerance;
}

void Log( std::string_view message ) noexcept {
	try {
		s2sdk::PrintToServer( plg::string( message ) );
	} catch ( ... ) {
	}
}

void Reply( std::int32_t slot, s2sdk::ConCommandContext context, std::string_view message ) noexcept {
	try {
		s2sdk::ReplyToCommand( context, slot, plg::string( message ) );
	} catch ( ... ) {
		Log( "[ServerFixes] spawn_capacity command reply failed" );
	}
}

std::filesystem::path ManualPointFilePath( std::string_view map ) {
	if ( !IsSafeMapName( map ) ) {
		throw std::runtime_error( "current map name is not a safe file name" );
	}
	const auto gameDirectory = s2sdk::GetGameDirectory();
	std::u8string gameDirectoryUtf8;
	gameDirectoryUtf8.reserve( gameDirectory.size() );
	for ( std::size_t index = 0; index < gameDirectory.size(); ++index ) {
		gameDirectoryUtf8.push_back( static_cast<char8_t>( static_cast<unsigned char>( gameDirectory[index] ) ) );
	}
	const std::filesystem::path gamePath( gameDirectoryUtf8 );
	return gamePath / "cfg" / "imomoe_server_fixes" / "spawn_capacity" / ( std::string( map ) + ".cfg" );
}

bool SaveManualPoints( const std::vector<SpawnPoint>& points, std::string& error ) noexcept {
	try {
		if ( g_currentMap.empty() ) {
			error = "no current map";
			return false;
		}
		const auto path = ManualPointFilePath( g_currentMap );
		std::error_code fileError;
		std::filesystem::create_directories( path.parent_path(), fileError );
		if ( fileError ) {
			error = "cannot create map point directory: " + fileError.message();
			return false;
		}
		auto temporaryPath = path;
		temporaryPath += ".tmp";
		{
			std::ofstream output( temporaryPath, std::ios::binary | std::ios::trunc );
			if ( !output ) {
				error = "cannot open temporary map point file";
				return false;
			}
			output.imbue( std::locale::classic() );
			output << "# id team x y z pitch yaw roll\n";
			output << std::setprecision( std::numeric_limits<float>::max_digits10 );
			for ( const auto& point : points ) {
				if ( !IsValidPoint( point ) ) {
					error = "refusing to save an invalid point";
					return false;
				}
				const ManualPointRow row{
					.id = point.id,
					.team = static_cast<std::int32_t>( point.team ),
					.x = point.origin.x,
					.y = point.origin.y,
					.z = point.origin.z,
					.pitch = point.angles.x,
					.yaw = point.angles.y,
					.roll = point.angles.z,
				};
				if ( !WriteManualPointRow( output, row ) ) {
					error = "could not write map point row";
					return false;
				}
			}
			output.flush();
			if ( !output ) {
				error = "could not flush map point file";
				return false;
			}
		}

#ifdef _WIN32
		if ( !MoveFileExW( temporaryPath.c_str(), path.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH ) ) {
			error = "cannot replace map point file (Windows error " + std::to_string( GetLastError() ) + ")";
			return false;
		}
#else
		std::filesystem::rename( temporaryPath, path, fileError );
		if ( fileError ) {
			error = "cannot replace map point file: " + fileError.message();
			return false;
		}
#endif
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "map point save failed: " ) + exception.what();
		return false;
	} catch ( ... ) {
		error = "map point save failed";
		return false;
	}
}

bool LoadManualPoints( std::string_view map, std::vector<SpawnPoint>& points, std::string& error ) noexcept {
	try {
		const auto path = ManualPointFilePath( map );
		std::error_code fileError;
		const bool exists = std::filesystem::exists( path, fileError );
		if ( fileError ) {
			error = "cannot inspect map point file: " + fileError.message();
			return false;
		}
		if ( !exists ) {
			points.clear();
			return true;
		}
		std::ifstream input( path, std::ios::binary );
		if ( !input ) {
			error = "cannot open map point file";
			return false;
		}
		input.imbue( std::locale::classic() );
		std::vector<SpawnPoint> parsed;
		std::string line;
		std::size_t lineNumber = 0;
		while ( std::getline( input, line ) ) {
			++lineNumber;
			const auto first = line.find_first_not_of( " \t\r\n" );
			if ( first == std::string::npos || line[first] == '#' ) {
				continue;
			}
			ManualPointRow row;
			if ( !ParseManualPointRow( line, row ) || !IsPlayableTeam( row.team ) ||
				parsed.size() >= kMaxManualSpawnPoints ) {
				error = "invalid map point entry at line " + std::to_string( lineNumber );
				return false;
			}
			SpawnPoint point{
				.id = row.id,
				.team = static_cast<Team>( row.team ),
				.origin = { row.x, row.y, row.z },
				.angles = { row.pitch, row.yaw, row.roll },
			};
			if ( !IsValidPoint( point ) || std::any_of( parsed.begin(), parsed.end(), [&]( const SpawnPoint& existing ) {
				return existing.id == point.id;
			} ) ) {
				error = "invalid or duplicate map point at line " + std::to_string( lineNumber );
				return false;
			}
			parsed.push_back( point );
		}
		if ( input.bad() ) {
			error = "could not read map point file";
			return false;
		}
		points = std::move( parsed );
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "map point load failed: " ) + exception.what();
		return false;
	} catch ( ... ) {
		error = "map point load failed";
		return false;
	}
}

bool ParseFloat( std::string_view text, float& value ) noexcept {
	if ( text.empty() ) {
		return false;
	}
	const auto parsed = std::from_chars( text.data(), text.data() + text.size(), value );
	return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite( value );
}

bool ParseManualTeam( std::string_view text, Team& team ) noexcept {
	if ( text == "2" || EqualsAsciiCaseInsensitive( text, "T" ) ||
		EqualsAsciiCaseInsensitive( text, "terrorist" ) ) {
		team = Team::Terrorist;
		return true;
	}
	if ( text == "3" || EqualsAsciiCaseInsensitive( text, "CT" ) ||
		EqualsAsciiCaseInsensitive( text, "counterterrorist" ) ) {
		team = Team::CounterTerrorist;
		return true;
	}
	return false;
}

bool ParseJoinTeam( const plg::vector<plg::string>& arguments, std::int32_t& team ) noexcept {
	std::size_t first = 0;
	if ( !arguments.empty() && EqualsAsciiCaseInsensitive( View( arguments.front() ), kJoinTeamCommand ) ) {
		first = 1;
	}
	if ( arguments.size() != first + 1 ) {
		return false;
	}
	const auto value = View( arguments[first] );
	if ( value == "2" ) {
		team = static_cast<std::int32_t>( Team::Terrorist );
		return true;
	}
	if ( value == "3" ) {
		team = static_cast<std::int32_t>( Team::CounterTerrorist );
		return true;
	}
	return false;
}

std::vector<std::string_view> CommandArguments( const plg::vector<plg::string>& arguments,
	std::string_view command ) {
	std::size_t first = 0;
	if ( !arguments.empty() && EqualsAsciiCaseInsensitive( View( arguments.front() ), command ) ) {
		first = 1;
	}
	std::vector<std::string_view> result;
	result.reserve( arguments.size() - first );
	for ( std::size_t index = first; index < arguments.size(); ++index ) {
		result.push_back( View( arguments[index] ) );
	}
	return result;
}

std::int32_t PlayerControllerHandle( std::int32_t slot ) noexcept {
	try {
		const auto handle = s2sdk::PlayerSlotToEntHandle( slot );
		return IsCompleteHandle( handle ) ? handle : -1;
	} catch ( ... ) {
		return -1;
	}
}

std::int32_t PlayerPawnHandle( std::int32_t slot ) noexcept {
	try {
		void* const pawn = s2sdk::GetClientPawn( slot );
		if ( !pawn ) {
			return -1;
		}
		const auto handle = s2sdk::EntPointerToEntHandle( pawn );
		return IsCompleteHandle( handle ) ? handle : -1;
	} catch ( ... ) {
		return -1;
	}
}

bool ResizePlayerState( std::int32_t maxClients ) noexcept {
	if ( maxClients <= 0 ) {
		return false;
	}
	try {
		const auto slotCount = static_cast<std::size_t>( maxClients );
		if ( g_joinAttempts.size() != slotCount ) {
			g_pendingRetries.clear();
			g_joinAttempts.assign( slotCount, std::nullopt );
			g_joinEpochs.assign( slotCount, JoinRetryEpoch{} );
			g_retryDispatchActive.assign( slotCount, false );
			return true;
		}
		g_joinEpochs.resize( slotCount );
		g_retryDispatchActive.resize( slotCount, false );
		return true;
	} catch ( ... ) {
		return false;
	}
}

bool IsTrackedClientSlot( std::int32_t slot ) noexcept {
	if ( slot < 0 ) {
		return false;
	}
	try {
		const auto index = static_cast<std::size_t>( slot );
		return slot < s2sdk::GetMaxClients() && index < g_joinAttempts.size() && index < g_joinEpochs.size();
	} catch ( ... ) {
		return false;
	}
}

void InvalidateJoinRequests( bool roundBoundary ) noexcept {
	g_pendingRetries.clear();
	for ( std::size_t index = 0; index < g_joinEpochs.size(); ++index ) {
		if ( roundBoundary ) {
			g_joinEpochs[index].BeginRound();
		} else {
			(void)g_joinEpochs[index].BeginRequest();
		}
		g_joinAttempts[index].reset();
	}
}

JoinRetryIdentity CurrentJoinRetryIdentity( std::int32_t slot ) {
	const auto index = static_cast<std::size_t>( slot );
	return {
		.requestGeneration = g_joinEpochs[index].request,
		.roundGeneration = g_joinEpochs[index].round,
		.lifeGeneration = g_joinEpochs[index].life,
		.controllerHandle = PlayerControllerHandle( slot ),
		.pawnHandle = PlayerPawnHandle( slot ),
		.originalTeam = static_cast<std::int32_t>( s2sdk::GetClientTeam( slot ) ),
		.alive = s2sdk::IsClientAlive( slot ),
	};
}

std::int32_t CountTeamPlayers( Team team ) noexcept {
	try {
		const auto maxClients = s2sdk::GetMaxClients();
		if ( maxClients <= 0 ) {
			return -1;
		}
		std::int32_t count = 0;
		for ( std::int32_t slot = 0; slot < maxClients; ++slot ) {
			if ( s2sdk::IsClientInGame( slot ) &&
				static_cast<std::int32_t>( s2sdk::GetClientTeam( slot ) ) == static_cast<std::int32_t>( team ) ) {
				++count;
			}
		}
		return count;
	} catch ( ... ) {
		return -1;
	}
}

bool DisableOwnedSpawnForRemoval( OwnedSpawn& spawn,
	const std::array<NativeTeamCapacity, 2>& before, std::string& error ) noexcept {
	if ( !IsCompleteHandle( spawn.handle ) || spawn.disabledForRemoval ) {
		return true;
	}
	if ( !EnsureSpawnEnabledSchema( error ) ) {
		return false;
	}
	try {
		void* const entity = s2sdk::EntHandleToEntPointer( spawn.handle );
		if ( !entity ) {
			error = "owned spawn entity became unavailable before disable";
			return false;
		}
		const auto classname = s2sdk::GetEntityClassname( spawn.handle );
		if ( View( classname ) != ClassnameFor( spawn.team ) ) {
			error = "owned spawn handle no longer identifies its original team entity";
			return false;
		}
		spawn.enabledBeforeRemoval = s2sdk::GetEntSchema2( entity, plg::string( kSpawnEntitySchemaClass ),
			plg::string( kSpawnEnabledField ), 0 );
		spawn.nativePointsBeforeRemoval = before[TeamIndex( spawn.team )].spawnPoints;
		spawn.nativeEligibleBeforeRemoval = NativeSpawnRegistered( spawn.handle, spawn.team, error );
		if ( !error.empty() ) {
			return false;
		}
		if ( spawn.enabledBeforeRemoval != 0 ) {
			s2sdk::SetEntSchema2( entity, plg::string( kSpawnEntitySchemaClass ),
				plg::string( kSpawnEnabledField ), 0, false, 0 );
			const auto disabled = s2sdk::GetEntSchema2( entity, plg::string( kSpawnEntitySchemaClass ),
				plg::string( kSpawnEnabledField ), 0 );
			if ( disabled != 0 ) {
				error = "owned spawn entity did not retain m_bEnabled=0 before removal";
				return false;
			}
			spawn.disabledByUs = true;
		}
		spawn.disabledForRemoval = true;
		g_nativeRefreshPending = true;
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "disabling owned spawn before removal failed: " ) + exception.what();
	} catch ( ... ) {
		error = "disabling owned spawn before removal failed";
	}
	return false;
}

bool VerifyOwnedSpawnExcluded( OwnedSpawn& spawn, const std::array<NativeTeamCapacity, 2>& after,
	std::string& error ) noexcept {
	if ( !IsCompleteHandle( spawn.handle ) ) {
		spawn.nativeExclusionVerified = true;
		return true;
	}
	try {
		void* const entity = s2sdk::EntHandleToEntPointer( spawn.handle );
		if ( !entity || s2sdk::GetEntSchema2( entity, plg::string( kSpawnEntitySchemaClass ),
			plg::string( kSpawnEnabledField ), 0 ) != 0 ) {
			error = "owned spawn entity remained enabled after retirement";
			return false;
		}
		if ( NativeSpawnRegistered( spawn.handle, spawn.team, error ) ) {
			error = "native spawn eligibility still accepts a retired owned entity";
			return false;
		}
		if ( !error.empty() ) {
			return false;
		}
		if ( spawn.nativeEligibleBeforeRemoval &&
			after[TeamIndex( spawn.team )].spawnPoints >= spawn.nativePointsBeforeRemoval ) {
			error = "native refresh did not remove the disabled owned point from valid spawn counts";
			return false;
		}
		spawn.nativeExclusionVerified = true;
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "verifying native spawn exclusion failed: " ) + exception.what();
	} catch ( ... ) {
		error = "verifying native spawn exclusion failed";
	}
	return false;
}

bool RequestEngineOwnedRemoval( OwnedSpawn& spawn, std::string& error ) noexcept {
	if ( !IsCompleteHandle( spawn.handle ) || spawn.removalRequested ) {
		return true;
	}
	if ( !spawn.disabledForRemoval || !spawn.nativeExclusionVerified ) {
		error = "owned spawn must be excluded from native capacity before RemoveEntity";
		return false;
	}
	try {
		// S2SDK RemoveEntity calls CBaseEntity::Remove()->Kill(); it queues engine-owned
		// deletion, so a still-valid handle remains tracked until a later update prunes it.
		s2sdk::RemoveEntity( spawn.handle );
		spawn.removalRequested = true;
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "requesting engine-owned spawn deletion failed: " ) + exception.what();
	} catch ( ... ) {
		error = "requesting engine-owned spawn deletion failed";
	}
	return false;
}

bool RequestOwnedRemoval( OwnedSpawn& spawn, std::string& error ) noexcept {
	if ( !IsCompleteHandle( spawn.handle ) || spawn.removalRequested ) {
		return true;
	}
	try {
		if ( !spawn.disabledForRemoval ) {
			if ( !RecountNativeSpawnLists( error ) ) {
				return false;
			}
			std::array<NativeTeamCapacity, 2> before{};
			if ( !ReadNativeTeamCapacities( before, error ) || !DisableOwnedSpawnForRemoval( spawn, before, error ) ) {
				return false;
			}
		}
		if ( !spawn.nativeExclusionVerified ) {
			if ( !RecountNativeSpawnLists( error ) ) {
				return false;
			}
			std::array<NativeTeamCapacity, 2> after{};
			if ( !ReadNativeTeamCapacities( after, error ) || !VerifyOwnedSpawnExcluded( spawn, after, error ) ) {
				return false;
			}
			g_nativeRefreshPending = false;
		}
		return RequestEngineOwnedRemoval( spawn, error );
	} catch ( const std::exception& exception ) {
		error = std::string( "owned spawn removal failed: " ) + exception.what();
	} catch ( ... ) {
		error = "owned spawn removal failed";
	}
	return false;
}

bool RemoveSpawnHandle( std::int32_t handle, Team team, std::string& error ) noexcept {
	OwnedSpawn untracked{ .handle = handle, .team = team };
	return RequestOwnedRemoval( untracked, error );
}

void PruneRemovedSpawns() noexcept {
	const auto previousSize = g_ownedSpawns.size();
	bool removedUnverifiedSpawn = false;
	g_ownedSpawns.erase( std::remove_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), [&]( const OwnedSpawn& spawn ) {
		const bool removed = !IsCompleteHandle( spawn.handle );
		removedUnverifiedSpawn = removedUnverifiedSpawn || ( removed && !spawn.nativeExclusionVerified );
		return removed;
	} ), g_ownedSpawns.end() );
	if ( g_ownedSpawns.size() != previousSize ) {
		g_nativeRefreshPending = g_nativeRefreshPending || removedUnverifiedSpawn;
	}
	if ( g_nativeRefreshPending && g_nativeSpawnRecount ) {
		std::string error;
		if ( RecountNativeSpawnLists( error ) ) {
			g_nativeRefreshPending = false;
		} else {
			g_mapPrepared = false;
			g_capacityReconcilePending = true;
			if ( !error.empty() ) {
				g_mapReason = std::move( error );
			}
		}
	}
}

bool HasPendingOwnedRemoval( Team team ) noexcept {
	return std::any_of( g_ownedSpawns.begin(), g_ownedSpawns.end(), [team]( const OwnedSpawn& spawn ) {
		return spawn.team == team && IsCompleteHandle( spawn.handle ) &&
			( ( spawn.batchRetirementPending && !spawn.removalRequested ) ||
				( spawn.disabledForRemoval && !spawn.nativeExclusionVerified ) );
	} );
}

bool StageSpawnEntity( const SpawnPoint& point, bool automatic, std::int32_t& handle, std::string& error ) noexcept {
	if ( !IsValidPoint( point ) ) {
		error = "spawn point values are invalid";
		return false;
	}
	const bool hasTemplate = HasRegisteredTemplate( point, error );
	if ( !error.empty() ) {
		return false;
	}
	const auto cleanupCreation = [&]() noexcept {
		try {
			auto owned = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), [handle]( const OwnedSpawn& spawn ) {
				return spawn.handle == handle;
			} );
			std::string cleanupError;
			if ( IsCompleteHandle( handle ) ) {
				const bool removed = owned != g_ownedSpawns.end() ? RequestOwnedRemoval( *owned, cleanupError ) :
					RemoveSpawnHandle( handle, point.team, cleanupError );
				if ( !removed ) {
					if ( automatic && owned != g_ownedSpawns.end() ) {
						owned->batchRetirementPending = true;
						g_capacityReconcilePending = true;
					}
					if ( !cleanupError.empty() ) {
						error += "; cleanup: " + cleanupError;
					}
				}
			} else if ( IsValidHandle( handle ) ) {
				// This raw full handle came from this creation, not a delayed task. The SDK can
				// resolve an unspawned allocation before pointer-to-handle readback is initialized.
				s2sdk::RemoveEntity( handle );
				if ( owned != g_ownedSpawns.end() ) {
					owned->removalRequested = true;
				}
				if ( !RecountNativeSpawnLists( cleanupError ) ) {
					error += "; cleanup: " + cleanupError;
				}
			}
		} catch ( ... ) {
			error += "; cleanup of the newly created entity failed";
		}
	};
	try {
		handle = s2sdk::CreateEntityByName( plg::string( ClassnameFor( point.team ) ) );
		// DispatchSpawn2 completes the new entity's reverse handle mapping on both platforms.
		// Before dispatch, validate the SDK-issued full handle through its entity-system lookup.
		if ( !IsValidHandle( handle ) || !s2sdk::EntHandleToEntPointer( handle ) ) {
			error = "CreateEntityByName did not return a live spawn handle";
			return false;
		}
		g_ownedSpawns.push_back( { .handle = handle, .manualId = automatic ? 0u : point.id,
			.team = point.team, .automatic = automatic } );
		plg::vector<plg::string> keys;
		keys.push_back( plg::string( "origin" ) );
		keys.push_back( plg::string( "angles" ) );
		plg::vector<plg::any> values;
		values.push_back( plg::any( point.origin ) );
		values.push_back( plg::any( point.angles ) );
		s2sdk::DispatchSpawn2( handle, keys, values );
		if ( !IsCompleteHandle( handle ) ) {
			error = "DispatchSpawn2 did not produce a complete spawn identity";
			cleanupCreation();
			return false;
		}
		const auto actualClass = s2sdk::GetEntityClassname( handle );
		if ( View( actualClass ) != ClassnameFor( point.team ) ||
			!SamePosition( s2sdk::GetEntityAbsOrigin( handle ), point.origin ) ) {
			error = "spawn entity did not retain its team class or requested origin";
			cleanupCreation();
			return false;
		}
		// Reusing an accepted template stays valid when players occupy its position.
		// A genuinely new manual location still needs the original native validation.
		if ( !hasTemplate && !NativeSpawnPointEligible( handle, error ) ) {
			if ( error.empty() ) {
				error = "new spawn location failed native validation";
			}
			cleanupCreation();
			return false;
		}
		const auto owned = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), [handle]( const OwnedSpawn& spawn ) {
			return spawn.handle == handle;
		} );
		if ( owned != g_ownedSpawns.end() ) {
			owned->validatedOrigin = point.origin;
		}
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "spawn entity creation failed: " ) + exception.what();
	} catch ( ... ) {
		error = "spawn entity creation failed";
	}
	cleanupCreation();
	return false;
}

std::vector<SpawnPoint> CaptureNativePoints( Team team ) {
	std::vector<SpawnPoint> points;
	const auto classname = plg::string( ClassnameFor( team ) );
	std::int32_t cursor = -1;
	for ( std::size_t visited = 0; visited < kMaxSpawnEntitiesToEnumerate; ++visited ) {
		cursor = s2sdk::FindEntityByClassname( cursor, classname );
		if ( cursor == -1 ) {
			break;
		}
		if ( !IsCompleteHandle( cursor ) ) {
			continue;
		}
		const auto actualClass = s2sdk::GetEntityClassname( cursor );
		if ( View( actualClass ) != ClassnameFor( team ) ) {
			continue;
		}
		SpawnPoint point{
			.id = 1,
			.team = team,
			.origin = s2sdk::GetEntityAbsOrigin( cursor ),
			.angles = s2sdk::GetEntityEyeAngles( cursor ),
			.entityHandle = cursor,
		};
		const bool owned = std::any_of( g_ownedSpawns.begin(), g_ownedSpawns.end(), [cursor]( const OwnedSpawn& spawn ) {
			return spawn.handle == cursor;
		} );
		std::string registrationError;
		const bool previouslyValidated = std::any_of( g_nativePoints.begin(), g_nativePoints.end(),
			[&point]( const SpawnPoint& source ) {
				return source.entityHandle == point.entityHandle && source.team == point.team &&
					SamePosition( source.origin, point.origin );
			} );
		if ( !owned && NativeSpawnRegistered( cursor, team, registrationError ) &&
			( previouslyValidated || NativeSpawnPointEligible( cursor, registrationError ) ) && std::isfinite( point.origin.x ) && std::isfinite( point.origin.y ) && std::isfinite( point.origin.z ) &&
			std::isfinite( point.angles.x ) && std::isfinite( point.angles.y ) && std::isfinite( point.angles.z ) ) {
			points.push_back( point );
		}
	}
	return points;
}

bool HasRegisteredTemplate( const SpawnPoint& point, std::string& error ) noexcept {
	try {
		for ( const auto& source : g_nativePoints ) {
			if ( source.team == point.team && NativeSpawnRegistered( source.entityHandle, source.team, error ) &&
				SamePosition( source.origin, point.origin ) &&
				SamePosition( s2sdk::GetEntityAbsOrigin( source.entityHandle ), point.origin ) ) {
				return true;
			}
		}
		for ( const auto& owned : g_ownedSpawns ) {
			if ( owned.team == point.team && owned.locationValidated &&
				SamePosition( owned.validatedOrigin, point.origin ) && !owned.batchRetirementPending && !owned.disabledForRemoval && !owned.removalRequested &&
				NativeSpawnRegistered( owned.handle, owned.team, error ) &&
				SamePosition( s2sdk::GetEntityAbsOrigin( owned.handle ), point.origin ) ) {
				return true;
			}
		}
	} catch ( ... ) {
		error = "reading registered spawn template failed";
	}
	return false;
}

std::vector<SpawnPoint> PointsForTeam( const std::vector<SpawnPoint>& points, Team team ) {
	std::vector<SpawnPoint> result;
	for ( const auto& point : points ) {
		if ( point.team == team ) {
			result.push_back( point );
		}
	}
	return result;
}

std::int32_t CountPoints( const std::vector<SpawnPoint>& points, Team team ) noexcept {
	return static_cast<std::int32_t>( std::count_if( points.begin(), points.end(), [team]( const SpawnPoint& point ) {
		return point.team == team;
	} ) );
}

bool IsActiveOwnedSpawn( const OwnedSpawn& spawn ) noexcept {
	return !spawn.batchRetirementPending && !spawn.disabledForRemoval && !spawn.removalRequested && IsCompleteHandle( spawn.handle );
}

std::int32_t CountOwned( Team team, bool automatic ) noexcept {
	return static_cast<std::int32_t>( std::count_if( g_ownedSpawns.begin(), g_ownedSpawns.end(),
		[team, automatic]( const OwnedSpawn& spawn ) {
			return spawn.team == team && spawn.automatic == automatic && IsActiveOwnedSpawn( spawn );
		} ) );
}

bool RemoveOwnedRecord( std::vector<OwnedSpawn>::iterator record ) noexcept {
	if ( record == g_ownedSpawns.end() ) {
		return true;
	}
	std::string error;
	if ( !RequestOwnedRemoval( *record, error ) ) {
		if ( !error.empty() ) {
			g_mapReason = std::move( error );
		}
		return false;
	}
	if ( !IsCompleteHandle( record->handle ) ) {
		g_ownedSpawns.erase( record );
	}
	return true;
}

bool RemoveOwnedBatch( std::span<OwnedSpawn* const> spawns, std::string& error,
	const std::array<NativeTeamCapacity, 2>* freshBaseline = nullptr ) noexcept {
	if ( std::any_of( spawns.begin(), spawns.end(), []( const OwnedSpawn* spawn ) {
		return IsCompleteHandle( spawn->handle ) && !spawn->disabledForRemoval;
	} ) ) {
		std::array<NativeTeamCapacity, 2> before{};
		if ( freshBaseline ) {
			before = *freshBaseline;
		} else if ( !RecountNativeSpawnLists( error ) || !ReadNativeTeamCapacities( before, error ) ) {
			return false;
		}
		for ( auto* spawn : spawns ) {
			if ( spawn->disabledForRemoval ) {
				continue;
			}
			if ( !DisableOwnedSpawnForRemoval( *spawn, before, error ) ) {
				return false;
			}
			// Each registered point must reduce the count, not merely share the
			// first point's threshold. The per-record threshold survives retries.
			if ( spawn->disabledForRemoval && spawn->nativeEligibleBeforeRemoval ) {
				--before[TeamIndex( spawn->team )].spawnPoints;
			}
		}
	}
	if ( std::any_of( spawns.begin(), spawns.end(), []( const OwnedSpawn* spawn ) {
		return IsCompleteHandle( spawn->handle ) && spawn->disabledForRemoval && !spawn->nativeExclusionVerified;
	} ) ) {
		std::array<NativeTeamCapacity, 2> after{};
		if ( !RecountNativeSpawnLists( error ) || !ReadNativeTeamCapacities( after, error ) ) {
			return false;
		}
		for ( auto* spawn : spawns ) {
			if ( spawn->disabledForRemoval && !spawn->nativeExclusionVerified &&
				!VerifyOwnedSpawnExcluded( *spawn, after, error ) ) {
				return false;
			}
		}
		g_nativeRefreshPending = false;
	}
	bool removed = true;
	for ( auto* spawn : spawns ) {
		removed = RequestEngineOwnedRemoval( *spawn, error ) && removed;
	}
	return removed;
}

bool RemoveAllOwned() noexcept {
	try {
		std::vector<OwnedSpawn*> spawns;
		spawns.reserve( g_ownedSpawns.size() );
		for ( auto& spawn : g_ownedSpawns ) {
			spawns.push_back( &spawn );
		}
		std::string error;
		const bool removed = RemoveOwnedBatch( spawns, error );
		if ( !removed && !error.empty() ) {
			g_mapReason = std::move( error );
		}
		PruneRemovedSpawns();
		return removed;
	} catch ( ... ) {
		g_mapReason = "owned spawn cleanup failed";
		return false;
	}
}

bool FinishSpawnCreation( Team team, std::span<const std::int32_t> created,
	std::array<NativeTeamCapacity, 2>& capacities, std::string& error ) {
	if ( created.empty() ) {
		return true;
	}
	const auto index = TeamIndex( team );
	const auto before = capacities[index].spawnPoints;
	const bool refreshed = RecountNativeSpawnLists( error ) && ReadNativeTeamCapacities( capacities, error );
	std::vector<OwnedSpawn*> rejected;
	std::size_t registeredCount{};
	for ( const auto handle : created ) {
		const auto spawn = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), [handle]( const OwnedSpawn& candidate ) {
			return candidate.handle == handle;
		} );
		std::string registrationError;
		if ( refreshed && spawn != g_ownedSpawns.end() && NativeSpawnRegistered( handle, team, registrationError ) ) {
			spawn->locationValidated = true;
			++registeredCount;
			continue;
		}
		if ( spawn != g_ownedSpawns.end() ) {
			rejected.push_back( &*spawn );
		}
		if ( error.empty() ) {
			error = registrationError.empty() ? "native spawn-list refresh did not register a new team spawn point" :
				std::move( registrationError );
		}
	}
	if ( refreshed && capacities[index].spawnPoints - before < static_cast<std::int32_t>( registeredCount ) ) {
		if ( error.empty() ) {
			error = "native spawn-list refresh did not increase team spawn capacity";
		}
		rejected.clear();
		for ( auto& spawn : g_ownedSpawns ) {
			if ( std::find( created.begin(), created.end(), spawn.handle ) != created.end() ) {
				spawn.locationValidated = false;
				rejected.push_back( &spawn );
			}
		}
	}
	if ( !rejected.empty() ) {
		for ( auto* spawn : rejected ) {
			spawn->batchRetirementPending = true;
		}
		std::string cleanupError;
		if ( !RemoveOwnedBatch( rejected, cleanupError, refreshed ? &capacities : nullptr ) ) {
			g_capacityReconcilePending = true;
			error += "; cleanup: " + cleanupError;
		}
	}
	return error.empty();
}

bool CreateSpawnEntity( const SpawnPoint& point, bool automatic, std::string& error ) noexcept {
	if ( !IsValidPoint( point ) ) {
		error = "spawn point values are invalid";
		return false;
	}
	if ( !ResolveNativeSpawnRecount( error ) ) {
		return false;
	}
	if ( !__s2sdk_GetGameRules ) {
		error = "S2SDK GetGameRules import is unavailable for native spawn-list refresh";
		return false;
	}
	try {
		if ( !s2sdk::GetGameRules() ) {
			error = "current GameRules instance is unavailable for native spawn-list refresh";
			return false;
		}
	} catch ( ... ) {
		error = "could not read the current GameRules instance for native spawn-list refresh";
		return false;
	}
	std::array<NativeTeamCapacity, 2> beforeCapacities{};
	if ( !ReadNativeTeamCapacities( beforeCapacities, error ) ) {
		return false;
	}
	try {
		std::int32_t handle = -1;
		return StageSpawnEntity( point, automatic, handle, error ) &&
			FinishSpawnCreation( point.team, std::span( &handle, 1 ), beforeCapacities, error );
	} catch ( ... ) {
		error = "verifying newly created spawn failed";
		return false;
	}
}

std::vector<SpawnPoint> ValidNativeSourcesForTeam( Team team, std::string& error ) {
	std::vector<SpawnPoint> nativeSources;
	for ( const auto& point : g_nativePoints ) {
		if ( point.team == team && NativeSpawnRegistered( point.entityHandle, team, error ) ) {
			nativeSources.push_back( point );
		}
	}
	return nativeSources;
}

std::vector<SpawnPoint> SourcesForTeam( Team team, std::string& error ) {
	auto nativeSources = ValidNativeSourcesForTeam( team, error );
	return nativeSources.empty() ? PointsForTeam( g_manualPoints, team ) : std::move( nativeSources );
}

bool AddCapacityPoint( Team team, std::string& error ) noexcept {
	try {
		std::array<NativeTeamCapacity, 2> beforeCapacities{};
		if ( !ReadNativeTeamCapacities( beforeCapacities, error ) ) {
			return false;
		}
		const auto beforeEffective = beforeCapacities[TeamIndex( team )].effective;
		const auto sources = SourcesForTeam( team, error );
		if ( sources.empty() ) {
			if ( error.empty() ) {
				error = "no native or saved manual spawn source exists for the rejected team";
			}
			return false;
		}
		SpawnPoint point = sources[static_cast<std::size_t>( CountOwned( team, true ) ) % sources.size()];
		point.id = 1;
		if ( !CreateSpawnEntity( point, true, error ) ) {
			return false;
		}
		auto added = std::find_if( g_ownedSpawns.rbegin(), g_ownedSpawns.rend(), [team]( const OwnedSpawn& spawn ) {
			return spawn.team == team && spawn.automatic && IsActiveOwnedSpawn( spawn );
		} );
		std::array<NativeTeamCapacity, 2> afterCapacities{};
		if ( !ReadNativeTeamCapacities( afterCapacities, error ) ) {
			if ( added != g_ownedSpawns.rend() ) {
				std::string cleanupError;
				if ( !RequestOwnedRemoval( *added, cleanupError ) && !cleanupError.empty() ) {
					error += "; cleanup: " + cleanupError;
				}
			}
			return false;
		}
		if ( afterCapacities[TeamIndex( team )].effective <= beforeEffective ) {
			error = "native effective team spawn capacity did not increase after adding a point";
			if ( added != g_ownedSpawns.rend() ) {
				std::string cleanupError;
				if ( !RequestOwnedRemoval( *added, cleanupError ) && !cleanupError.empty() ) {
					error += "; cleanup: " + cleanupError;
				}
			}
			return false;
		}
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "could not extend team spawn capacity: " ) + exception.what();
		return false;
	} catch ( ... ) {
		error = "could not extend team spawn capacity";
		return false;
	}
}

bool ReconcileAutomaticCapacity( std::string& error ) noexcept {
	g_capacityReadbackAvailable = false;
	try {
		const auto maxPlayers = s2sdk::GetMaxClients();
		if ( maxPlayers <= 0 ) {
			error = "GetMaxClients returned no usable player capacity";
			return false;
		}
		const auto currentMap = s2sdk::GetCurrentMap();
		const std::string mapName( View( currentMap ) );
		void* const rules = s2sdk::GetGameRules();
		if ( !rules ) {
			error = "current GameRules instance is unavailable for team-capacity reconciliation";
			return false;
		}
		if ( !RecountNativeSpawnLists( error ) ) {
			return false;
		}
		std::array<NativeTeamCapacity, 2> capacities{};
		if ( !ReadNativeTeamCapacities( capacities, error ) ) {
			return false;
		}
		constexpr std::array<Team, 2> teams{ Team::Terrorist, Team::CounterTerrorist };
		std::string missingSources;
		for ( const auto team : teams ) {
			const auto index = TeamIndex( team );
			bool externalOwner = false;
			if ( !EnsureTeamMaximumCapacity( rules, mapName, team, maxPlayers, externalOwner, error ) ) {
				if ( externalOwner ) {
					error = std::string( team == Team::Terrorist ? "T" : "CT" ) +
						" max-team capacity has an external owner below actual maxplayers";
				}
				return false;
			}
			if ( !ReadNativeTeamCapacities( capacities, error ) ) {
				return false;
			}
			if ( capacities[index].teamMaximum < maxPlayers ) {
				error = std::string( team == Team::Terrorist ? "T" : "CT" ) +
					" max-team capacity remains below actual maxplayers";
				return false;
			}
			const auto targetEffective = maxPlayers;
			auto automaticCount = CountOwned( team, true );
			const auto nativeAndManual = capacities[index].spawnPoints - automaticCount;
			if ( nativeAndManual < 0 ) {
				error = "native spawn count is smaller than the registered ServerFixes-owned points";
				return false;
			}
			const auto targetAutomatic = AdditionalPointsNeeded( targetEffective, nativeAndManual, 0 );
			if ( automaticCount > targetAutomatic ) {
				std::vector<OwnedSpawn*> excess;
				excess.reserve( static_cast<std::size_t>( automaticCount - targetAutomatic ) );
				for ( auto it = g_ownedSpawns.rbegin(); it != g_ownedSpawns.rend() && automaticCount > targetAutomatic; ++it ) {
					if ( it->team == team && it->automatic && IsActiveOwnedSpawn( *it ) ) {
						it->batchRetirementPending = true;
						excess.push_back( &*it );
						--automaticCount;
					}
				}
				if ( !RemoveOwnedBatch( excess, error, &capacities ) ) {
					g_capacityReconcilePending = true;
					return false;
				}
				PruneRemovedSpawns();
				if ( !ReadNativeTeamCapacities( capacities, error ) ) {
					return false;
				}
			}
			if ( automaticCount < targetAutomatic ) {
				if ( HasPendingOwnedRemoval( team ) ) {
					g_capacityReconcilePending = true;
					error = "automatic capacity adjustment is waiting for owned spawn removal";
					return false;
				}
				const auto sources = SourcesForTeam( team, error );
				if ( sources.empty() ) {
					if ( !error.empty() ) return false;
					if ( !missingSources.empty() ) missingSources += "; ";
					missingSources += std::string( team == Team::Terrorist ? "T" : "CT" ) +
						" has no valid native or saved manual spawn source; add a point with sf_spawn_add";
					continue;
				}
				std::vector<std::int32_t> created;
				created.reserve( static_cast<std::size_t>( targetAutomatic - automaticCount ) );
				std::string creationError;
				for ( ; automaticCount < targetAutomatic; ++automaticCount ) {
					SpawnPoint point = sources[static_cast<std::size_t>( automaticCount ) % sources.size()];
					point.id = 1;
					std::int32_t handle = -1;
					if ( !StageSpawnEntity( point, true, handle, creationError ) ) {
						break;
					}
					created.push_back( handle );
				}
				const bool finished = FinishSpawnCreation( team, created, capacities, error );
				if ( !creationError.empty() ) {
					error = finished ? std::move( creationError ) : creationError + "; " + error;
					return false;
				}
				if ( !finished ) {
					return false;
				}
			}
			if ( capacities[index].effective < targetEffective ) {
				error = "native effective team capacity is below its maxplayers/team-policy target";
				return false;
			}
		}
		g_capacityMaxClients = maxPlayers;
		g_effectiveTeamCapacities = capacities;
		if ( !missingSources.empty() ) {
			error = std::move( missingSources );
			return false;
		}
		g_capacityReadbackAvailable = true;
		g_capacityReconcilePending = false;
		return true;
	} catch ( const std::exception& exception ) {
		error = std::string( "automatic capacity setup failed: " ) + exception.what();
		return false;
	} catch ( ... ) {
		error = "automatic capacity setup failed";
		return false;
	}
}

std::string ReadyMapReason() {
	std::string reason;
	try {
		std::string sourceError;
		const auto tNativeSources = ValidNativeSourcesForTeam( Team::Terrorist, sourceError ).size();
		const auto ctNativeSources = ValidNativeSourcesForTeam( Team::CounterTerrorist, sourceError ).size();
		const auto tSources = tNativeSources + static_cast<std::size_t>( CountPoints( g_manualPoints, Team::Terrorist ) );
		const auto ctSources = ctNativeSources +
			static_cast<std::size_t>( CountPoints( g_manualPoints, Team::CounterTerrorist ) );
		reason = ( tSources == 0 || ctSources == 0 ) ?
			"ready; add a manual point for the team without a native spawn source" :
			( tNativeSources == 0 || ctNativeSources == 0 ?
				"ready; native/manual effective team capacity" : "ready; native effective team capacity" );
		reason += " T=" +
			std::to_string( g_effectiveTeamCapacities[0].effective ) + " CT=" +
			std::to_string( g_effectiveTeamCapacities[1].effective ) + " / maxplayers=" +
			std::to_string( g_capacityMaxClients );
		if ( g_teamMaximumYielded[0] ) {
			reason += "; T max-team capacity has an external owner";
		}
		if ( g_teamMaximumYielded[1] ) {
			reason += "; CT max-team capacity has an external owner";
		}
		if ( g_manualPointsDirty ) {
			reason += "; manual point edits are not saved";
		}
	} catch ( ... ) {
		return "ready; status text allocation failed";
	}
	return reason;
}

void SpawnMapEntities() noexcept {
	g_mapPrepared = false;
	std::string error;
	if ( !g_manualPoints.empty() && !RecountNativeSpawnLists( error ) ) {
		g_mapReason = std::move( error );
		(void)RemoveAllOwned();
		return;
	}
	for ( const auto& point : g_manualPoints ) {
		if ( !CreateSpawnEntity( point, false, error ) ) {
			g_mapReason = std::move( error );
			(void)RemoveAllOwned();
			return;
		}
	}
	if ( !ReconcileAutomaticCapacity( error ) ) {
		g_mapReason = std::move( error );
		// Keep working manual points and capacity already established for the other team.
		return;
	}
	g_mapPrepared = true;
}

void CompleteMapLoad() noexcept {
	g_mapPrepared = false;
	g_mapReason.clear();
	if ( !IsSafeMapName( g_currentMap ) ) {
		g_mapReason = "current map name is empty or invalid";
		return;
	}
	std::string error;
	if ( !LoadManualPoints( g_currentMap, g_manualPoints, error ) ) {
		g_mapReason = std::move( error );
		return;
	}
	g_manualPointsDirty = false;
	for ( const auto& point : g_manualPoints ) {
		if ( point.id >= g_nextManualId && point.id != std::numeric_limits<std::uint32_t>::max() ) {
			g_nextManualId = point.id + 1;
		}
	}
	try {
		auto points = CaptureNativePoints( Team::Terrorist );
		const auto ctPoints = CaptureNativePoints( Team::CounterTerrorist );
		points.insert( points.end(), ctPoints.begin(), ctPoints.end() );
		g_nativePoints = std::move( points );
	} catch ( const std::exception& exception ) {
		g_mapReason = std::string( "could not enumerate native spawn points: " ) + exception.what();
		return;
	} catch ( ... ) {
		g_mapReason = "could not enumerate native spawn points";
		return;
	}
	g_mapDataLoaded = true;
	SpawnMapEntities();
}

void ProcessMapRebuild() noexcept {
	if ( !g_rebuildPending ) {
		return;
	}
	// Disabled, native-excluded points can remain handle-valid after an engine-owned
	// removal request. They stay tracked, but do not prevent rebuilding usable capacity.
	const bool removalQueued = RemoveAllOwned();
	if ( !removalQueued || g_nativeRefreshPending ) {
		if ( g_nativeRefreshPending ) {
			g_mapReason = "waiting for the native spawn-list refresh after owned point cleanup";
		}
		return;
	}
	g_rebuildPending = false;
	if ( g_mapDataLoaded ) {
		SpawnMapEntities();
	} else {
		CompleteMapLoad();
	}
}

void LoadMap( std::string map ) noexcept {
	g_mapPrepared = false;
	g_mapLoadAttempted = true;
	g_mapDataLoaded = false;
	g_rebuildPending = true;
	g_capacityReconcilePending = false;
	g_mapReason.clear();
	InvalidateJoinRequests( true );
	std::fill( g_retryDispatchActive.begin(), g_retryDispatchActive.end(), false );
	// Preserve same-map geometry qualification across an explicit off/on, not
	// across a new map/Rules instance. Full handles and positions are rechecked.
	void* const sourceRules = s2sdk::GetGameRules();
	if ( map != g_currentMap || sourceRules != g_nativeSourceRules ||
		g_nativeSourceMapGeneration != runtime_lifecycle::MapGeneration() ) {
		g_nativePoints.clear();
	}
	g_nativeSourceRules = sourceRules;
	g_nativeSourceMapGeneration = runtime_lifecycle::MapGeneration();
	g_currentMap = std::move( map );
	g_manualPoints.clear();
	g_nextManualId = 1;
	g_capacityMaxClients = 0;
	g_effectiveTeamCapacities = {};
	g_capacityReadbackAvailable = false;
	g_teamMaximumCapacityBlocked = false;
	g_manualPointsDirty = false;
	ProcessMapRebuild();
}

bool RegisterRuntimeHooks( std::string& error ) noexcept {
	try {
		if ( !g_mapStartRegistered ) {
			s2sdk::OnMapStart_Register( OnMapStart );
			g_mapStartRegistered = true;
		}
		if ( !g_mapEndRegistered ) {
			s2sdk::OnMapEnd_Register( OnMapEnd );
			g_mapEndRegistered = true;
		}
		if ( !g_roundPrestartRegistered ) {
			g_roundPrestartRegistered = s2sdk::HookEvent( plg::string( kRoundPrestartEvent ),
				OnRoundPrestartEvent, s2sdk::HookMode::Post ) == s2sdk::EventHookError::Okay;
			if ( !g_roundPrestartRegistered ) {
				error = "could not register round_prestart observer";
				return false;
			}
		}
		if ( !g_playerDeathRegistered ) {
			g_playerDeathRegistered = s2sdk::HookEvent( plg::string( kPlayerDeathEvent ),
				OnPlayerLifeBoundaryEvent, s2sdk::HookMode::Post ) == s2sdk::EventHookError::Okay;
			if ( !g_playerDeathRegistered ) {
				error = "could not register player_death identity observer";
				return false;
			}
		}
		if ( !g_playerSpawnRegistered ) {
			g_playerSpawnRegistered = s2sdk::HookEvent( plg::string( kPlayerSpawnEvent ),
				OnPlayerLifeBoundaryEvent, s2sdk::HookMode::Post ) == s2sdk::EventHookError::Okay;
			if ( !g_playerSpawnRegistered ) {
				error = "could not register player_spawn identity observer";
				return false;
			}
		}
		if ( !g_joinFailedRegistered ) {
			g_joinFailedRegistered = s2sdk::HookEvent( plg::string( kJoinFailedEvent ),
				OnJoinFailedEvent, s2sdk::HookMode::Post ) == s2sdk::EventHookError::Okay;
			if ( !g_joinFailedRegistered ) {
				error = "could not register jointeam_failed observer";
				return false;
			}
		}
		if ( !g_joinPreRegistered ) {
			g_joinPreRegistered = s2sdk::AddCommandListener( plg::string( "" ),
				OnJoinCommandPre, s2sdk::HookMode::Pre );
			if ( !g_joinPreRegistered ) {
				error = "could not register global client-command pre listener";
				return false;
			}
		}
		g_runtimeHooksRegistered = true;
		return true;
	} catch ( ... ) {
		error = "runtime hook registration threw";
		return false;
	}
}

bool UnregisterRuntimeHooks() noexcept {
	bool clean = true;
	try {
		if ( g_roundPrestartRegistered ) {
			const auto result = s2sdk::UnhookEvent( plg::string( kRoundPrestartEvent ), OnRoundPrestartEvent,
				s2sdk::HookMode::Post );
			if ( result != s2sdk::EventHookError::Okay ) {
				clean = false;
			} else {
				g_roundPrestartRegistered = false;
			}
		}
		if ( g_playerSpawnRegistered ) {
			const auto result = s2sdk::UnhookEvent( plg::string( kPlayerSpawnEvent ), OnPlayerLifeBoundaryEvent,
				s2sdk::HookMode::Post );
			if ( result != s2sdk::EventHookError::Okay ) {
				clean = false;
			} else {
				g_playerSpawnRegistered = false;
			}
		}
		if ( g_playerDeathRegistered ) {
			const auto result = s2sdk::UnhookEvent( plg::string( kPlayerDeathEvent ), OnPlayerLifeBoundaryEvent,
				s2sdk::HookMode::Post );
			if ( result != s2sdk::EventHookError::Okay ) {
				clean = false;
			} else {
				g_playerDeathRegistered = false;
			}
		}
		if ( g_joinFailedRegistered ) {
			const auto result = s2sdk::UnhookEvent( plg::string( kJoinFailedEvent ), OnJoinFailedEvent,
				s2sdk::HookMode::Post );
			if ( result != s2sdk::EventHookError::Okay ) {
				clean = false;
			} else {
				g_joinFailedRegistered = false;
			}
		}
		if ( g_joinPreRegistered ) {
			const bool removed = s2sdk::RemoveCommandListener( plg::string( "" ),
				OnJoinCommandPre, s2sdk::HookMode::Pre );
			clean = removed && clean;
			if ( removed ) {
				g_joinPreRegistered = false;
			}
		}
		if ( g_mapStartRegistered ) {
			s2sdk::OnMapStart_Unregister( OnMapStart );
			g_mapStartRegistered = false;
		}
		if ( g_mapEndRegistered ) {
			s2sdk::OnMapEnd_Unregister( OnMapEnd );
			g_mapEndRegistered = false;
		}
	} catch ( ... ) {
		clean = false;
	}
	if ( clean ) {
		g_runtimeHooksRegistered = false;
	}
	return clean;
}

void DispatchJoinTeamThroughRawServerCommand( std::int32_t slot, std::int32_t team ) {
	void* const rawInterface = s2sdk::QueryInterface( plg::string( "server" ),
		plg::string( INTERFACEVERSION_SERVERGAMECLIENTS ) );
	if ( !rawInterface ) {
		throw std::runtime_error( "Source2GameClients001 interface is unavailable for normal jointeam dispatch" );
	}
	const std::string command = "jointeam " + std::to_string( team );
	CCommand arguments;
	characterset_t breakSet{};
	CharacterSetBuild( &breakSet, "" );
	if ( !arguments.Tokenize( command.c_str(), &breakSet ) ) {
		throw std::runtime_error( "could not tokenize the jointeam retry command" );
	}
	static_cast<IServerGameClients*>( rawInterface )->ClientCommand( CPlayerSlot( slot ), arguments );
}

void ProcessPendingRetries() noexcept {
	if ( g_pendingRetries.empty() || !g_enabled || !g_mapPrepared || g_rebuildPending || g_capacityReconcilePending ) {
		return;
	}
	auto pending = std::move( g_pendingRetries );
	g_pendingRetries.clear();
	for ( const auto& retry : pending ) {
		try {
			if ( !IsTrackedClientSlot( retry.slot ) || retry.map != g_currentMap ||
				!IsCapacityFailure( static_cast<Team>( retry.requestedTeam ), retry.failureReason ) ||
				!s2sdk::IsClientInGame( retry.slot ) ||
				!SameJoinRetryIdentity( retry.identity, CurrentJoinRetryIdentity( retry.slot ) ) ) {
				continue;
			}
			const auto index = static_cast<std::size_t>( retry.slot );
			const auto currentTeam = static_cast<std::int32_t>( s2sdk::GetClientTeam( retry.slot ) );
			if ( currentTeam == retry.requestedTeam ) {
				continue;
			}
			const auto maxPlayers = s2sdk::GetMaxClients();
			const auto teamPlayers = CountTeamPlayers( static_cast<Team>( retry.requestedTeam ) );
			if ( maxPlayers <= 0 || teamPlayers < 0 || teamPlayers >= maxPlayers ) {
				continue;
			}
			std::string error;
			const bool extended = AddCapacityPoint( static_cast<Team>( retry.requestedTeam ), error );
			std::array<NativeTeamCapacity, 2> capacities{};
			if ( !ReadNativeTeamCapacities( capacities, error ) ) {
				if ( !error.empty() ) {
					Log( "[ServerFixes] spawn_capacity could not read the updated native allowance: " + error );
				}
				continue;
			}
			const RetryFacts facts{
				.enabled = g_enabled,
				.connected = teamPlayers >= 0 && s2sdk::IsClientInGame( retry.slot ),
				.sameRequestIdentity = SameJoinRetryIdentity( retry.identity, CurrentJoinRetryIdentity( retry.slot ) ) &&
					retry.map == g_currentMap,
				.spawnCapacityExtended = extended,
				.retryAlreadyUsed = false,
				.requestedTeam = retry.requestedTeam,
				.currentTeam = currentTeam,
				.teamPlayers = teamPlayers,
				.maxPlayers = maxPlayers,
				.teamCapacity = capacities[TeamIndex( static_cast<Team>( retry.requestedTeam ) )].effective,
				.failureReason = retry.failureReason,
			};
			if ( !ShouldRetry( facts ) ) {
				if ( !error.empty() ) {
					Log( "[ServerFixes] spawn_capacity did not retry jointeam: " + error );
				}
				continue;
			}
			g_retryDispatchActive[index] = true;
			try {
				DispatchJoinTeamThroughRawServerCommand( retry.slot, retry.requestedTeam );
			} catch ( ... ) {
				g_retryDispatchActive[index] = false;
				Log( "[ServerFixes] spawn_capacity same-request retry threw" );
				continue;
			}
			g_retryDispatchActive[index] = false;
			Log( "[ServerFixes] spawn_capacity dispatched one normal jointeam retry after adding a team spawn" );
		} catch ( ... ) {
			Log( "[ServerFixes] spawn_capacity pending join retry failed" );
		}
	}
}

bool CommandExists( std::string_view name ) {
	for ( const auto& existing : s2sdk::GetAllConCommands( s2sdk::ConVarFlag::None ) ) {
		if ( EqualsAsciiCaseInsensitive( View( existing ), name ) ) {
			return true;
		}
	}
	return false;
}

struct AdminCommand {
	std::string_view name;
	s2sdk::ConCommandCallback callback;
	bool owned{};
};

std::array<AdminCommand, 4> g_adminCommands{{
	{ kSpawnAddCommand, OnSpawnAddCommand },
	{ kSpawnListCommand, OnSpawnListCommand },
	{ kSpawnRemoveCommand, OnSpawnRemoveCommand },
	{ kSpawnSaveCommand, OnSpawnSaveCommand },
}};
bool g_adminRegistrationComplete{};

bool UnregisterAdminCommands( std::string& error ) noexcept {
	g_adminRegistrationComplete = false;
	error.clear();
	bool clean = true;
	for ( auto& [name, callback, owned] : g_adminCommands ) {
		if ( !owned ) {
			continue;
		}
		bool removed = false;
		bool removalThrew = false;
		try {
			removed = __s2sdk_RemoveCommand && s2sdk::RemoveCommand( plg::string( name ), callback );
		} catch ( ... ) {
			removalThrew = true;
		}

		bool stillPresent = true;
		bool presenceKnown = false;
		try {
			if ( __s2sdk_HasConsoleCommandCallbacks ) {
				stillPresent = s2sdk::HasConsoleCommandCallbacks( plg::string( name ) );
				presenceKnown = true;
			}
		} catch ( ... ) {
		}
		if ( presenceKnown && !stillPresent ) {
			owned = false;
			continue;
		}

		clean = false;
		if ( error.empty() ) {
			try {
				error = "could not confirm removal of console command " + std::string( name ) +
					( !presenceKnown ? "; callback-presence readback failed" :
						( removalThrew ? "; RemoveCommand threw and callback remains" :
							( removed ? "; RemoveCommand returned true but callback remains" :
								"; RemoveCommand returned false and callback remains" ) ) );
			} catch ( ... ) {
			}
		}
	}
	return clean;
}

bool RegisterAdminCommands( std::string& error ) noexcept {
	if ( g_adminRegistrationComplete ) {
		return true;
	}
	if ( !UnregisterAdminCommands( error ) ) {
		return false;
	}
	try {
		for ( auto& [name, callback, owned] : g_adminCommands ) {
			if ( CommandExists( name ) ) {
				error = "console command already exists: " + std::string( name );
				break;
			}
			const plg::string commandName( name );
			const plg::string description( "Manage this map's ServerFixes-owned spawn points" );
			owned = true; // Add can fail or throw after installing a callback.
			if ( !s2sdk::AddConsoleCommand( commandName, description, s2sdk::ConVarFlag::None,
				callback, s2sdk::HookMode::Pre ) || !s2sdk::HasConsoleCommandCallbacks( commandName ) ) {
				error = "could not register console command: " + std::string( name );
				break;
			}
		}
	} catch ( ... ) {
		error = "spawn administration command registration threw";
	}
	if ( error.empty() ) {
		g_adminRegistrationComplete = true;
		return true;
	}
	std::string cleanupError;
	if ( !UnregisterAdminCommands( cleanupError ) ) {
		error += "; " + cleanupError;
	}
	return false;
}

bool ParseManualPointId( std::string_view text, std::uint32_t& id ) noexcept {
	if ( text.empty() ) {
		return false;
	}
	const auto parsed = std::from_chars( text.data(), text.data() + text.size(), id );
	return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && id != 0;
}

bool IsAdminCommandAllowed( std::int32_t slot, s2sdk::ConCommandContext context ) noexcept {
	if ( !g_enabled ) {
		Reply( slot, context, "[ServerFixes] spawn_capacity is disabled" );
		return false;
	}
	if ( !IsServerConsoleMutation( g_enabled, context == s2sdk::ConCommandContext::Console, slot ) ) {
		Reply( slot, context, "[ServerFixes] spawn point administration is available only in the server console" );
		return false;
	}
	if ( !g_mapDataLoaded || g_mapLifecycle.mapEndPending || g_rebuildPending || !IsSafeMapName( g_currentMap ) ) {
		Reply( slot, context, "[ServerFixes] current map data is not ready for spawn-point administration: " + g_mapReason );
		return false;
	}
	return true;
}

std::uint32_t NextManualPointId() noexcept {
	auto candidate = g_nextManualId;
	for ( std::size_t attempt = 0; attempt <= g_manualPoints.size(); ++attempt ) {
		if ( candidate != 0 && std::none_of( g_manualPoints.begin(), g_manualPoints.end(), [candidate]( const SpawnPoint& point ) {
			return point.id == candidate;
		} ) ) {
			return candidate;
		}
		++candidate;
	}
	return 0;
}

s2sdk::ResultType OnSpawnAddCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept {
	try {
		if ( !IsAdminCommandAllowed( slot, context ) ) {
			return s2sdk::ResultType::Handled;
		}
		const auto values = CommandArguments( arguments, kSpawnAddCommand );
		if ( values.size() != 4 && values.size() != 7 ) {
			Reply( slot, context, "Usage: sf_spawn_add <T|CT> <x> <y> <z> [pitch yaw roll]" );
			return s2sdk::ResultType::Handled;
		}
		if ( g_manualPoints.size() >= kMaxManualSpawnPoints ) {
			Reply( slot, context, "[ServerFixes] this map already has the maximum saved manual spawn points" );
			return s2sdk::ResultType::Handled;
		}
		SpawnPoint point;
		point.id = NextManualPointId();
		if ( point.id == 0 || !ParseManualTeam( values[0], point.team ) ||
			!ParseFloat( values[1], point.origin.x ) || !ParseFloat( values[2], point.origin.y ) ||
			!ParseFloat( values[3], point.origin.z ) ||
			( values.size() == 7 && ( !ParseFloat( values[4], point.angles.x ) ||
				!ParseFloat( values[5], point.angles.y ) || !ParseFloat( values[6], point.angles.z ) ) ) ||
			!IsValidPoint( point ) ) {
			Reply( slot, context, "[ServerFixes] invalid team or point coordinates" );
			return s2sdk::ResultType::Handled;
		}
		std::string error;
		if ( !RecountNativeSpawnLists( error ) ) {
			Reply( slot, context, "[ServerFixes] could not refresh native spawns before adding the point: " + error );
			return s2sdk::ResultType::Handled;
		}
		g_manualPoints.push_back( point );
		if ( !CreateSpawnEntity( point, false, error ) ) {
			g_manualPoints.pop_back();
			Reply( slot, context, "[ServerFixes] could not add manual point: " + error );
			return s2sdk::ResultType::Handled;
		}
		g_nextManualId = point.id == std::numeric_limits<std::uint32_t>::max() ? 0u : point.id + 1;
		g_manualPointsDirty = true;
		if ( !ReconcileAutomaticCapacity( error ) ) {
			g_mapPrepared = false;
			g_mapReason = error;
			Reply( slot, context, "[ServerFixes] added manual point " + std::to_string( point.id ) +
				"; capacity adjustment is pending: " + error + "; use sf_spawn_save to persist it" );
		} else {
			g_mapPrepared = true;
			Reply( slot, context, "[ServerFixes] added manual point " + std::to_string( point.id ) +
				" for " + std::string( point.team == Team::Terrorist ? "T" : "CT" ) +
				"; use sf_spawn_save to persist it" );
		}
	} catch ( ... ) {
		Reply( slot, context, "[ServerFixes] adding the manual spawn point failed" );
	}
	return s2sdk::ResultType::Handled;
}

s2sdk::ResultType OnSpawnListCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept {
	try {
		if ( !IsAdminCommandAllowed( slot, context ) ) {
			return s2sdk::ResultType::Handled;
		}
		if ( !CommandArguments( arguments, kSpawnListCommand ).empty() ) {
			Reply( slot, context, "Usage: sf_spawn_list" );
			return s2sdk::ResultType::Handled;
		}
		Reply( slot, context, "[ServerFixes] manual points for " + g_currentMap +
			" (unsaved changes: " + std::string( g_manualPointsDirty ? "yes" : "no" ) + ")" );
		if ( g_manualPoints.empty() ) {
			Reply( slot, context, "[ServerFixes] no manual points are configured for this map" );
		} else {
			for ( const auto& point : g_manualPoints ) {
				Reply( slot, context, "[ServerFixes] id=" + std::to_string( point.id ) +
					" team=" + std::string( point.team == Team::Terrorist ? "T" : "CT" ) +
					" origin=" + FormatVector( point.origin ) + " angles=" + FormatVector( point.angles ) );
			}
		}
		Reply( slot, context, "[ServerFixes] active owned entities: automatic T=" +
			std::to_string( CountOwned( Team::Terrorist, true ) ) + " CT=" +
			std::to_string( CountOwned( Team::CounterTerrorist, true ) ) );
	} catch ( ... ) {
		Reply( slot, context, "[ServerFixes] listing spawn points failed" );
	}
	return s2sdk::ResultType::Handled;
}

s2sdk::ResultType OnSpawnRemoveCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept {
	try {
		if ( !IsAdminCommandAllowed( slot, context ) ) {
			return s2sdk::ResultType::Handled;
		}
		const auto values = CommandArguments( arguments, kSpawnRemoveCommand );
		std::uint32_t id{};
		if ( values.size() != 1 || !ParseManualPointId( values.front(), id ) ) {
			Reply( slot, context, "Usage: sf_spawn_remove <manual-point-id>" );
			return s2sdk::ResultType::Handled;
		}
		const auto point = std::find_if( g_manualPoints.begin(), g_manualPoints.end(), [id]( const SpawnPoint& candidate ) {
			return candidate.id == id;
		} );
		if ( point == g_manualPoints.end() ) {
			Reply( slot, context, "[ServerFixes] manual point not found; native map points are never removed" );
			return s2sdk::ResultType::Handled;
		}
		const auto removedPoint = *point;
		const auto team = point->team;
		g_manualPoints.erase( point );
		const auto owned = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), [id]( const OwnedSpawn& spawn ) {
			return !spawn.automatic && spawn.manualId == id && IsActiveOwnedSpawn( spawn );
		} );
		if ( owned != g_ownedSpawns.end() && !RemoveOwnedRecord( owned ) ) {
			g_manualPoints.push_back( removedPoint );
			Reply( slot, context, "[ServerFixes] could not request removal of manual point " + std::to_string( id ) );
			return s2sdk::ResultType::Handled;
		}
		g_manualPointsDirty = true;
		std::string error;
		const bool pendingRemoval = HasPendingOwnedRemoval( team );
		if ( pendingRemoval ) {
			g_capacityReconcilePending = true;
			g_mapPrepared = false;
			error = "waiting for the removed manual spawn entity to leave the entity system";
		} else if ( !ReconcileAutomaticCapacity( error ) ) {
			g_mapPrepared = false;
			g_mapReason = error;
		}
		if ( error.empty() ) {
			g_mapPrepared = true;
			Reply( slot, context, "[ServerFixes] removed manual point " + std::to_string( id ) +
				"; use sf_spawn_save to persist the change" );
		} else {
			g_mapReason = error;
			Reply( slot, context, "[ServerFixes] removed manual point " + std::to_string( id ) +
				"; capacity adjustment is pending: " + error + "; use sf_spawn_save to persist the change" );
		}
	} catch ( ... ) {
		Reply( slot, context, "[ServerFixes] removing the manual spawn point failed" );
	}
	return s2sdk::ResultType::Handled;
}

s2sdk::ResultType OnSpawnSaveCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept {
	try {
		if ( !IsAdminCommandAllowed( slot, context ) ) {
			return s2sdk::ResultType::Handled;
		}
		if ( !CommandArguments( arguments, kSpawnSaveCommand ).empty() ) {
			Reply( slot, context, "Usage: sf_spawn_save" );
			return s2sdk::ResultType::Handled;
		}
		std::string error;
		if ( !SaveManualPoints( g_manualPoints, error ) ) {
			Reply( slot, context, "[ServerFixes] could not save manual points: " + error );
			return s2sdk::ResultType::Handled;
		}
		g_manualPointsDirty = false;
		Reply( slot, context, "[ServerFixes] saved " + std::to_string( g_manualPoints.size() ) +
			" manual point(s) for " + g_currentMap );
	} catch ( ... ) {
		Reply( slot, context, "[ServerFixes] saving manual spawn points failed" );
	}
	return s2sdk::ResultType::Handled;
}

s2sdk::ResultType OnJoinCommandPre( std::int32_t slot, s2sdk::ConCommandContext,
	const plg::vector<plg::string>& arguments ) noexcept {
	try {
		if ( arguments.empty() || !EqualsAsciiCaseInsensitive( View( arguments.front() ), kJoinTeamCommand ) ) {
			return s2sdk::ResultType::Continue;
		}
		if ( !IsTrackedClientSlot( slot ) ) {
			return s2sdk::ResultType::Continue;
		}
		const auto index = static_cast<std::size_t>( slot );
		auto& epoch = g_joinEpochs[index];
		const auto requestGeneration = epoch.BeginRequest();
		g_joinAttempts[index].reset();
		if ( g_retryDispatchActive[index] || !g_enabled || !g_mapPrepared || !s2sdk::IsClientInGame( slot ) ) {
			return s2sdk::ResultType::Continue;
		}
		std::int32_t requestedTeam{};
		if ( !ParseJoinTeam( arguments, requestedTeam ) ) {
			return s2sdk::ResultType::Continue;
		}
		const auto controller = PlayerControllerHandle( slot );
		const auto pawn = PlayerPawnHandle( slot );
		const auto currentTeam = static_cast<std::int32_t>( s2sdk::GetClientTeam( slot ) );
		const bool alive = s2sdk::IsClientAlive( slot );
		if ( controller == -1 || pawn == -1 || currentTeam == requestedTeam ) {
			return s2sdk::ResultType::Continue;
		}
		g_joinAttempts[index] = JoinAttempt{
			.requestedTeam = requestedTeam,
			.identity = {
				.requestGeneration = requestGeneration,
				.roundGeneration = epoch.round,
				.lifeGeneration = epoch.life,
				.controllerHandle = controller,
				.pawnHandle = pawn,
				.originalTeam = currentTeam,
				.alive = alive,
			},
			.failureReason = -1,
			.map = g_currentMap,
			.retryEligible = true,
		};
	} catch ( ... ) {
		Log( "[ServerFixes] could not record jointeam request" );
	}
	return s2sdk::ResultType::Continue;
}

s2sdk::ResultType OnJoinFailedEvent( const plg::string&, void* event, bool ) noexcept {
	try {
		if ( !g_enabled || event == nullptr ) {
			return s2sdk::ResultType::Continue;
		}
		const auto slot = s2sdk::GetEventPlayerSlot( event, plg::string( "userid" ) );
		if ( !IsTrackedClientSlot( slot ) ) {
			return s2sdk::ResultType::Continue;
		}
		const auto index = static_cast<std::size_t>( slot );
		auto& storedAttempt = g_joinAttempts[index];
		if ( !storedAttempt ) {
			return s2sdk::ResultType::Continue;
		}
		auto& attempt = *storedAttempt;
		if ( attempt.map != g_currentMap ||
			!SameJoinRetryIdentity( attempt.identity, CurrentJoinRetryIdentity( slot ) ) ) {
			return s2sdk::ResultType::Continue;
		}

		attempt.failureReason = s2sdk::GetEventInt( event, plg::string( "reason" ) );
		if ( !attempt.retryEligible ||
			!IsCapacityFailure( static_cast<Team>( attempt.requestedTeam ), attempt.failureReason ) ||
			!s2sdk::IsClientInGame( slot ) ||
			static_cast<std::int32_t>( s2sdk::GetClientTeam( slot ) ) == attempt.requestedTeam ) {
			storedAttempt.reset();
			return s2sdk::ResultType::Continue;
		}
		const bool alreadyQueued = std::any_of( g_pendingRetries.begin(), g_pendingRetries.end(), [&]( const PendingRetry& retry ) {
			return retry.slot == slot && retry.requestedTeam == attempt.requestedTeam &&
				SameJoinRetryIdentity( retry.identity, attempt.identity );
		} );
		if ( !alreadyQueued ) {
			g_pendingRetries.push_back( {
				.slot = slot,
				.requestedTeam = attempt.requestedTeam,
				.identity = attempt.identity,
				.failureReason = attempt.failureReason,
				.map = attempt.map,
			} );
		}
		storedAttempt.reset();
	} catch ( ... ) {
		Log( "[ServerFixes] jointeam_failed observation failed" );
	}
	return s2sdk::ResultType::Continue;
}

bool RebuildAfterMapEnd() noexcept {
	if ( !g_enabled || !g_mapLifecycle.CanRebuildMap() ) {
		return false;
	}
	try {
		const auto currentMap = s2sdk::GetCurrentMap();
		const std::string mapName( View( currentMap ) );
		if ( !IsSafeMapName( mapName ) ) {
			g_mapReason = "waiting for the new map name after map end";
			return false;
		}
		LoadMap( mapName );
		g_mapLifecycle.CompleteMapRebuild();
		return true;
	} catch ( const std::exception& exception ) {
		g_mapReason = std::string( "same-map rebuild failed: " ) + exception.what();
	} catch ( ... ) {
		g_mapReason = "same-map rebuild failed";
	}
	return false;
}

void ReconcileSpawnEntitiesAfterRound() noexcept {
	if ( !g_enabled || !g_mapDataLoaded || g_rebuildPending || g_mapLifecycle.mapEndPending ) {
		return;
	}
	try {
		PruneRemovedSpawns();
		const bool pendingOwnedRemoval = HasPendingOwnedRemoval( Team::Terrorist ) ||
			HasPendingOwnedRemoval( Team::CounterTerrorist );
		if ( pendingOwnedRemoval || g_nativeRefreshPending ) {
			g_mapPrepared = false;
			g_mapReason = "waiting for owned spawn cleanup before round reconciliation";
			return;
		}
		std::string error;
		if ( !RecountNativeSpawnLists( error ) ) {
			g_mapPrepared = false;
			g_mapReason = std::move( error );
			return;
		}
		auto points = CaptureNativePoints( Team::Terrorist );
		const auto ctPoints = CaptureNativePoints( Team::CounterTerrorist );
		points.insert( points.end(), ctPoints.begin(), ctPoints.end() );
		g_nativePoints = std::move( points );
		for ( const auto& point : g_manualPoints ) {
			const auto owned = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), [&]( const OwnedSpawn& spawn ) {
				return !spawn.automatic && spawn.manualId == point.id && IsActiveOwnedSpawn( spawn );
			} );
			if ( owned != g_ownedSpawns.end() ) {
				continue;
			}
			if ( !CreateSpawnEntity( point, false, error ) ) {
				g_mapPrepared = false;
				g_mapReason = std::move( error );
				return;
			}
		}
		if ( !ReconcileAutomaticCapacity( error ) ) {
			g_mapPrepared = false;
			g_mapReason = std::move( error );
			return;
		}
		g_mapPrepared = true;
	} catch ( const std::exception& exception ) {
		g_mapPrepared = false;
		g_mapReason = std::string( "round spawn reconciliation failed: " ) + exception.what();
	} catch ( ... ) {
		g_mapPrepared = false;
		g_mapReason = "round spawn reconciliation failed";
	}
}

s2sdk::ResultType OnRoundPrestartEvent( const plg::string&, void*, bool ) noexcept {
	if ( !g_enabled ) {
		return s2sdk::ResultType::Continue;
	}
	InvalidateJoinRequests( true );
	ReconcileSpawnEntitiesAfterRound();
	return s2sdk::ResultType::Continue;
}

s2sdk::ResultType OnPlayerLifeBoundaryEvent( const plg::string&, void* event, bool ) noexcept {
	try {
		if ( !g_enabled || event == nullptr ) {
			return s2sdk::ResultType::Continue;
		}
		const auto slot = s2sdk::GetEventPlayerSlot( event, plg::string( "userid" ) );
		if ( !IsTrackedClientSlot( slot ) ) {
			return s2sdk::ResultType::Continue;
		}
		const auto index = static_cast<std::size_t>( slot );
		g_joinEpochs[index].BeginLife();
		g_joinAttempts[index].reset();
		g_pendingRetries.erase( std::remove_if( g_pendingRetries.begin(), g_pendingRetries.end(),
			[slot]( const PendingRetry& retry ) { return retry.slot == slot; } ), g_pendingRetries.end() );
	} catch ( ... ) {
		Log( "[ServerFixes] player life boundary observer failed" );
	}
	return s2sdk::ResultType::Continue;
}

void OnMapEnd() noexcept {
	g_nativePoints.clear();
	g_nativeSourceRules = nullptr;
	if ( !g_enabled ) {
		return;
	}
	std::string capacityError;
	if ( !RestoreTeamMaximums( capacityError ) ) {
		g_mapReason = std::move( capacityError );
	}
	++g_mapGeneration;
	if ( g_mapGeneration == 0 ) {
		++g_mapGeneration;
	}
	g_mapLifecycle.OnMapEnd();
	InvalidateJoinRequests( true );
	g_mapPrepared = false;
	g_mapDataLoaded = false;
	g_rebuildPending = false;
	g_capacityReconcilePending = false;
	g_effectiveTeamCapacities = {};
	g_capacityReadbackAvailable = false;
	g_teamMaximumCapacityBlocked = false;
	try {
		g_mapReason = "map ended; waiting for map start before rebuilding map spawns";
	} catch ( ... ) {
	}
}

void OnMapStart() noexcept {
	if ( !g_enabled ) {
		return;
	}
	if ( g_mapLifecycle.OnMapStart() ) {
		(void)RebuildAfterMapEnd();
	}
}

void SetModuleReason( std::string reason ) noexcept {
	try {
		g_moduleReason = std::move( reason );
	} catch ( ... ) {
		g_moduleReason = "status text allocation failed";
	}
}

} // namespace

Status Update( bool enabled, bool includeDiagnostics ) noexcept {
	try {
		if ( !enabled ) {
			g_enabled = false;
			g_mapPrepared = false;
			g_rebuildPending = false;
			g_capacityReconcilePending = false;
			InvalidateJoinRequests( false );
			std::fill( g_retryDispatchActive.begin(), g_retryDispatchActive.end(), false );
			const bool hooksClean = UnregisterRuntimeHooks();
			std::string commandCleanupError;
			const bool commandsClean = UnregisterAdminCommands( commandCleanupError );
			const bool spawnsRequested = RemoveAllOwned();
			std::string teamRestoreError;
			const bool teamMaximumsRestored = RestoreTeamMaximums( teamRestoreError );
			PruneRemovedSpawns();
			const bool configClosed = spawnsRequested && teamMaximumsRestored ? CloseSpawnGameConfig() : false;
			g_mapLoadAttempted = false;
			g_mapDataLoaded = false;
			g_mapLifecycle.CompleteMapRebuild();
			if ( !hooksClean || !commandsClean || !spawnsRequested || !teamMaximumsRestored || !configClosed ) {
				if ( !teamRestoreError.empty() ) {
					g_mapReason = teamRestoreError;
				}
				SetModuleReason( commandsClean ? "disabled; runtime hook or owned spawn cleanup needs retry" :
					"disabled; " + commandCleanupError );
				return { false, g_moduleReason };
			}
			ResetTeamMaximumCycle();
			SetModuleReason( g_ownedSpawns.empty() ? "disabled by configuration" :
				"disabled; owned spawn removal is pending" );
			return { true, g_moduleReason };
		}

		if ( !HasRequiredImports() ) {
			g_enabled = false;
			g_mapPrepared = false;
			InvalidateJoinRequests( false );
			(void)UnregisterRuntimeHooks();
			std::string commandCleanupError;
			const bool commandsClean = UnregisterAdminCommands( commandCleanupError );
			const bool spawnsRemoved = RemoveAllOwned();
			std::string teamRestoreError;
			const bool teamMaximumsRestored = RestoreTeamMaximums( teamRestoreError );
			if ( spawnsRemoved && teamMaximumsRestored ) {
				(void)CloseSpawnGameConfig();
				ResetTeamMaximumCycle();
			}
			g_mapLoadAttempted = false;
			g_mapDataLoaded = false;
			SetModuleReason( commandsClean ? "required S2SDK import unavailable" :
				"required S2SDK import unavailable; " + commandCleanupError );
			return { false, g_moduleReason };
		}

		PruneRemovedSpawns();
		g_enabled = true;
		const auto maxClients = s2sdk::GetMaxClients();
		if ( maxClients > 0 && !ResizePlayerState( maxClients ) ) {
			SetModuleReason( "could not allocate state for the server's actual maxplayers" );
			return { false, g_moduleReason };
		}

		std::string registrationError;
		const bool commandsRegistered = RegisterAdminCommands( registrationError );
		if ( !commandsRegistered || !RegisterRuntimeHooks( registrationError ) ) {
			g_enabled = false;
			g_mapPrepared = false;
			InvalidateJoinRequests( false );
			const bool hooksClean = UnregisterRuntimeHooks();
			std::string commandCleanupError;
			const bool commandsClean = commandsRegistered ? UnregisterAdminCommands( commandCleanupError ) :
				std::none_of( g_adminCommands.begin(), g_adminCommands.end(), []( const AdminCommand& command ) {
					return command.owned;
				} );
			const bool spawnsClean = RemoveAllOwned();
			std::string teamRestoreError;
			const bool teamMaximumsRestored = RestoreTeamMaximums( teamRestoreError );
			const bool configClosed = spawnsClean && teamMaximumsRestored ? CloseSpawnGameConfig() : false;
			if ( spawnsClean && teamMaximumsRestored && configClosed ) {
				ResetTeamMaximumCycle();
			}
			SetModuleReason( registrationError.empty() ? "spawn capacity listener registration failed" :
				registrationError );
			if ( !commandsClean && !commandCleanupError.empty() ) {
				SetModuleReason( g_moduleReason + "; " + commandCleanupError );
			}
			if ( !hooksClean || !commandsClean || !spawnsClean || !teamMaximumsRestored || !configClosed ) {
				SetModuleReason( g_moduleReason + "; partial registration cleanup remains pending" );
			}
			g_mapLoadAttempted = false;
			g_mapDataLoaded = false;
			return { false, g_moduleReason };
		}

		const auto currentMap = s2sdk::GetCurrentMap();
		const std::string mapName( View( currentMap ) );
		if ( g_mapLifecycle.CanRebuildMap() ) {
			(void)RebuildAfterMapEnd();
		} else if ( !g_mapLifecycle.mapEndPending && ( !g_mapLoadAttempted || mapName != g_currentMap ) ) {
			LoadMap( mapName );
		}
		if ( !g_mapLifecycle.mapEndPending ) {
			ProcessMapRebuild();
		} else {
			g_mapPrepared = false;
			g_mapReason = "map ended; waiting for map start before rebuilding map spawns";
		}
		if ( ( g_mapPrepared || g_capacityMaxClients > 0 ) && g_mapDataLoaded &&
			!g_rebuildPending && !g_mapLifecycle.mapEndPending && !g_capacityReconcilePending ) {
			const auto currentMaxClients = s2sdk::GetMaxClients();
			if ( currentMaxClients > 0 && currentMaxClients != g_capacityMaxClients ) {
				std::string error;
				if ( !ReconcileAutomaticCapacity( error ) ) {
					g_mapPrepared = false;
					g_mapReason = std::move( error );
				} else {
					g_mapPrepared = true;
				}
			}
		}

		if ( g_capacityReconcilePending && !g_rebuildPending && !g_mapLifecycle.mapEndPending ) {
			std::vector<OwnedSpawn*> retiring;
			for ( auto& spawn : g_ownedSpawns ) {
				if ( spawn.batchRetirementPending && !spawn.removalRequested ) {
					retiring.push_back( &spawn );
				}
			}
			if ( !retiring.empty() ) {
				std::string error;
				if ( !RemoveOwnedBatch( retiring, error ) ) {
					g_mapReason = std::move( error );
				}
				PruneRemovedSpawns();
			}
		}
		const bool removalPending = HasPendingOwnedRemoval( Team::Terrorist ) ||
			HasPendingOwnedRemoval( Team::CounterTerrorist );
		if ( g_capacityReconcilePending && !removalPending && g_enabled && g_mapDataLoaded &&
			!g_rebuildPending && !g_mapLifecycle.mapEndPending ) {
			g_capacityReconcilePending = false;
			std::string error;
			if ( !ReconcileAutomaticCapacity( error ) ) {
				g_mapPrepared = false;
				g_mapReason = std::move( error );
			} else {
				g_mapPrepared = true;
			}
		}
		if ( g_capacityReadbackAvailable && g_mapDataLoaded &&
			!g_mapLifecycle.mapEndPending ) {
			const bool wasTeamCapacityBlocked = g_teamMaximumCapacityBlocked;
			const auto maxPlayers = s2sdk::GetMaxClients();
			const auto rules = s2sdk::GetGameRules();
			std::string error;
			if ( !RefreshObservedTeamMaximums( rules, g_currentMap, maxPlayers, error ) ) {
				g_mapPrepared = false;
				g_mapReason = std::move( error );
			} else {
				if ( wasTeamCapacityBlocked && !g_rebuildPending && !g_capacityReconcilePending &&
					!g_mapLifecycle.mapEndPending && !HasPendingOwnedRemoval( Team::Terrorist ) &&
					!HasPendingOwnedRemoval( Team::CounterTerrorist ) ) {
					g_mapPrepared = true;
				}
			}
		}
		ProcessPendingRetries();

		if ( g_mapPrepared ) {
			return { true, includeDiagnostics ? "enabled; " + ReadyMapReason() : "enabled" };
		}
		SetModuleReason( g_mapReason.empty() ? "map spawn capacity is not ready" : g_mapReason );
		return { false, g_moduleReason };
	} catch ( const std::exception& exception ) {
		SetModuleReason( std::string( "spawn capacity update failed: " ) + exception.what() );
		return { false, g_moduleReason };
	} catch ( ... ) {
		SetModuleReason( "spawn capacity update failed" );
		return { false, g_moduleReason };
	}
}

bool Shutdown() noexcept {
	g_enabled = false;
	g_mapPrepared = false;
	g_rebuildPending = false;
	g_capacityReconcilePending = false;
	InvalidateJoinRequests( false );
	std::fill( g_retryDispatchActive.begin(), g_retryDispatchActive.end(), false );
	const bool hooksClean = UnregisterRuntimeHooks();
	std::string commandCleanupError;
	const bool commandsClean = UnregisterAdminCommands( commandCleanupError );
	const bool spawnsRequested = RemoveAllOwned();
	std::string teamRestoreError;
	const bool teamMaximumsRestored = RestoreTeamMaximums( teamRestoreError );
	PruneRemovedSpawns();
	const bool configClosed = spawnsRequested && teamMaximumsRestored ? CloseSpawnGameConfig() : false;
	if ( hooksClean && commandsClean && spawnsRequested && teamMaximumsRestored && configClosed ) {
		ResetTeamMaximumCycle();
		return true;
	}
	if ( !commandCleanupError.empty() || !teamRestoreError.empty() ) {
		const auto detail = !commandCleanupError.empty() ? commandCleanupError : teamRestoreError;
		SetModuleReason( "shutdown cleanup incomplete: " + detail );
	}
	return false;
}

} // namespace imomoe::server_fixes::spawn_capacity
