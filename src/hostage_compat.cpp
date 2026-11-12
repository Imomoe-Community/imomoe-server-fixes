// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "hostage_compat.hpp"

#include "hostage_dispatch_read.hpp"
#include "hostage_policy.hpp"
#include "owned_code_patch.hpp"
#include "runtime_lifecycle.hpp"
#include "server_fixes_gamedata.hpp"

#include <polyhook/polyhook.hpp>
#include <s2sdk/s2sdk.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace imomoe::server_fixes::hostage {
namespace {

constexpr std::string_view kRescueZoneClass = "func_hostage_rescue";
constexpr std::string_view kPlayerClass = "player";
constexpr std::string_view kHostageClass = "hostage_entity";
constexpr std::string_view kPawnClass = "CCSPlayerPawn";
constexpr std::string_view kPlayerPawnClass = "CBasePlayerPawn";
constexpr std::string_view kHostageServicesClass = "CCSPlayer_HostageServices";
constexpr std::string_view kHostageEntityClass = "CHostage";
constexpr std::size_t kMaxPlayerSlots = 256;
constexpr std::size_t kMaxTouchDepth = 128;

struct SlotGenerations {
	std::uint64_t connection{};
	std::uint64_t life{};
};

struct CarriedHostage {
	std::int32_t hostage{-1};
	std::int32_t prop{-1};
};

struct TouchFrame {
	void* entity{};
	void* other{};
	bool relevant{};
	bool nativePawnRescueCallReached{};
	bool pawnRouteObservationCertain{ true };
	bool handlerObserved{};
	bool handlerObservationCertain{ true };
	std::int32_t zone{-1};
	std::int32_t slot{-1};
	AttemptKey key{};
	bool hasCarriedHostage{};
};

struct HookLease {
	void* address{};
	polyhook::HookHandle handle{};
	polyhook::CallbackType type{polyhook::CallbackType::Pre};
	polyhook::CallbackHandler callback{};
	bool callbackRegistered{};
	bool midHook{};
};

struct EventBinding {
	const char* name{};
	bool registered{};
};

thread_local std::array<TouchFrame, kMaxTouchDepth> g_touchFrames{};
thread_local std::size_t g_touchDepth{};
thread_local std::size_t g_touchOverflowDepth{};

OwnedCodePatch g_tTargetGate;
bool g_tGateAttempted{};
AttemptLedger g_attempts;
std::vector<HookLease> g_hookLeases;
std::unordered_set<std::int32_t> g_rescueZones;
std::array<SlotGenerations, kMaxPlayerSlots> g_slotGenerations{};
std::uint64_t g_generationSequence{1};
std::uint64_t g_roundGeneration{};
std::optional<float> g_observedRoundStart;
bool g_initialized{};
bool g_pluginActive{};
bool g_moduleEnabled{};
bool g_ctObserversActive{};
bool g_roundActive{};
bool g_disconnectRegistered{};
s2sdk::ConfigId g_gameConfig{};
std::int32_t g_touchVTableOffset{-1};
std::uintptr_t g_dispatchTargetOffset{};
std::uint64_t g_enabledConVarHandle{};
std::uint64_t g_onlyCtsConVar{};
std::uint64_t g_gameTypeConVar{};
std::uint64_t g_gameModeConVar{};

struct SchemaOffsets {
	std::int32_t hostageServices;
	std::int32_t controller;
	std::int32_t carriedHostage;
	std::int32_t carriedProp;
	std::int32_t isRescued;
	std::int32_t hostageState;
	std::int32_t mapHasRescueZone;
	std::int32_t triggerDisabled;
	std::int32_t roundStartTime;
	std::int32_t roundWinStatus;
	std::int32_t warmupPeriod;
};

SchemaOffsets g_schema{};
bool g_ctLifecycleReady{};
void* g_hostageRescueHandler{};
void* g_pawnRescueBody{};
void* g_pawnRescueMarker{};
std::string g_initializationError{"not initialized"};
std::string g_tGateError{"not initialized"};
std::string g_ctError{"not initialized"};
std::chrono::steady_clock::time_point g_lastTouchSync{};

std::array<EventBinding, 4> g_events{{
	{ "round_start", false },
	{ "round_end", false },
	{ "player_spawn", false },
	{ "player_death", false },
}};

bool IsValidHandle( std::int32_t handle ) noexcept {
	return IsEntityHandleValuePresent( handle ) && s2sdk::IsValidEntHandle( handle );
}

void* EntityPointer( std::int32_t handle ) noexcept {
	if ( !IsValidHandle( handle ) ) {
		return nullptr;
	}
	void* const entity = s2sdk::EntHandleToEntPointer( handle );
	return entity && s2sdk::EntPointerToEntHandle( entity ) == handle ? entity : nullptr;
}

std::int32_t EntityHandle( void* entity ) noexcept {
	if ( !entity ) {
		return -1;
	}
	const auto handle = s2sdk::EntPointerToEntHandle( entity );
	return IsValidHandle( handle ) && s2sdk::EntHandleToEntPointer( handle ) == entity ? handle : -1;
}

std::string EntityClass( std::int32_t handle ) noexcept {
	if ( !EntityPointer( handle ) ) {
		return {};
	}
	try {
		const auto name = s2sdk::GetEntityClassname( handle );
		return std::string( name.data(), name.size() );
	} catch ( ... ) {
		return {};
	}
}

std::uint64_t FindConVar( std::string_view name ) noexcept {
	try {
		return __s2sdk_FindConVar ? s2sdk::FindConVar( plg::string( name ) ) : 0;
	} catch ( ... ) {
		return 0;
	}
}

bool ReadIntConVar( std::uint64_t handle, std::int32_t& value ) noexcept {
	if ( !handle || !__s2sdk_GetConVarValue ) {
		return false;
	}
	try {
		const auto text = s2sdk::GetConVarValue( handle );
		const std::string_view view( text.data(), text.size() );
		const auto parsed = std::from_chars( view.data(), view.data() + view.size(), value );
		return parsed.ec == std::errc{} && parsed.ptr == view.data() + view.size();
	} catch ( ... ) {
		return false;
	}
}

bool ReadBoolConVar( std::uint64_t handle, bool& value ) noexcept {
	if ( !handle || !__s2sdk_GetConVarValue ) {
		return false;
	}
	try {
		const auto text = s2sdk::GetConVarValue( handle );
		const std::string_view view( text.data(), text.size() );
		if ( view == "1" || view == "true" || view == "TRUE" || view == "yes" || view == "on" ) {
			value = true;
			return true;
		}
		if ( view == "0" || view == "false" || view == "FALSE" || view == "no" || view == "off" ) {
			value = false;
			return true;
		}
	} catch ( ... ) {
	}
	return false;
}

std::uint64_t CachedConVar( std::uint64_t& handle, std::string_view name ) noexcept {
	if ( !handle ) {
		handle = FindConVar( name );
	}
	return handle;
}

bool IsSupportedCustomGame() noexcept {
	std::int32_t gameType{};
	std::int32_t gameMode{};
	return ReadIntConVar( CachedConVar( g_gameTypeConVar, "game_type" ), gameType ) &&
		ReadIntConVar( CachedConVar( g_gameModeConVar, "game_mode" ), gameMode ) &&
		gameType == 3 && gameMode == 0;
}

// Offsets are resolved once after map activation. Read the verified field
// widths without repeating S2SDK schema-name lookups in Touch.
template <typename T>
T ReadSchema( const void* object, std::int32_t offset ) noexcept {
	T value;
	std::memcpy( &value, static_cast<const std::byte*>( object ) + offset, sizeof( value ) );
	return value;
}

bool MapHasRescueZone() noexcept {
	void* const rules = s2sdk::GetGameRules();
	return rules && ReadSchema<std::uint8_t>( rules, g_schema.mapHasRescueZone ) != 0;
}

bool EnabledRescueZone( std::int32_t handle ) noexcept {
	if ( EntityClass( handle ) != kRescueZoneClass ) {
		return false;
	}
	void* const zone = EntityPointer( handle );
	return zone && ReadSchema<std::uint8_t>( zone, g_schema.triggerDisabled ) == 0;
}

std::optional<float> ActiveRoundStart() {
	void* const rules = s2sdk::GetGameRules();
	if ( !rules || g_schema.roundStartTime < 0 || g_schema.roundWinStatus < 0 || g_schema.warmupPeriod < 0 ||
		ReadSchema<std::int32_t>( rules, g_schema.roundWinStatus ) != 0 ||
		ReadSchema<std::uint8_t>( rules, g_schema.warmupPeriod ) != 0 ) {
		return std::nullopt;
	}
	const auto started = ReadSchema<float>( rules, g_schema.roundStartTime );
	return started > 0.0f ? std::optional{ started } : std::nullopt;
}

CarriedHostage ReadCarriedHostage( std::int32_t pawnHandle ) noexcept {
	try {
		void* const pawn = EntityPointer( pawnHandle );
		if ( !pawn || EntityClass( pawnHandle ) != kPlayerClass ) {
			return {};
		}
		const auto services = ReadSchema<void*>( pawn, g_schema.hostageServices );
		if ( !services ) {
			return {};
		}
		const auto hostageHandle = ReadSchema<std::int32_t>( services, g_schema.carriedHostage );
		const auto propHandle = ReadSchema<std::int32_t>( services, g_schema.carriedProp );
		void* const hostage = EntityPointer( hostageHandle );
		void* const prop = EntityPointer( propHandle );
		if ( !hostage || !prop || EntityClass( hostageHandle ) != kHostageClass ||
			ReadSchema<std::uint8_t>( hostage, g_schema.isRescued ) != 0 ||
			ReadSchema<std::int32_t>( hostage, g_schema.hostageState ) != 3 ||
			s2sdk::GetEntityParent( propHandle ) != pawnHandle ) {
			return {};
		}
		return { hostageHandle, propHandle };
	} catch ( ... ) {
		return {};
	}
}

bool FindPlayerForPawn( std::int32_t pawnHandle, std::int32_t& slot, std::int32_t& controllerHandle ) noexcept {
	try {
		const auto maxClients = s2sdk::GetMaxClients();
		if ( maxClients < 0 ) {
			return false;
		}
		for ( std::int32_t candidate = 0; candidate < maxClients && candidate < static_cast<std::int32_t>( kMaxPlayerSlots ); ++candidate ) {
			if ( !s2sdk::IsClientInGame( candidate ) ) {
				continue;
			}
			void* const pawn = s2sdk::GetClientPawn( candidate );
			if ( !pawn || EntityHandle( pawn ) != pawnHandle ) {
				continue;
			}
			const auto controller = s2sdk::PlayerSlotToEntHandle( candidate );
			if ( !IsValidHandle( controller ) ||
				ReadSchema<std::int32_t>( pawn, g_schema.controller ) != controller ) {
				return false;
			}
			slot = candidate;
			controllerHandle = controller;
			return true;
		}
	} catch ( ... ) {
	}
	return false;
}

bool ReadLivePlayer( std::int32_t slot, std::int32_t expectedController, std::int32_t expectedPawn,
	bool requireCt, bool requireAlive ) noexcept {
	try {
		if ( slot < 0 || slot >= s2sdk::GetMaxClients() || slot >= static_cast<std::int32_t>( kMaxPlayerSlots ) ||
			!s2sdk::IsClientInGame( slot ) || ( requireAlive && !s2sdk::IsClientAlive( slot ) ) ) {
			return false;
		}
		void* const pawn = s2sdk::GetClientPawn( slot );
		const auto controller = s2sdk::PlayerSlotToEntHandle( slot );
		if ( !pawn || EntityHandle( pawn ) != expectedPawn || controller != expectedController ||
			!IsValidHandle( controller ) || EntityClass( expectedPawn ) != kPlayerClass ||
			ReadSchema<std::int32_t>( pawn, g_schema.controller ) != controller ) {
			return false;
		}
		return !requireCt || static_cast<std::int32_t>( s2sdk::GetClientTeam( slot ) ) == 3;
	} catch ( ... ) {
		return false;
	}
}

bool ImportsAvailable() noexcept {
	return __s2sdk_LoadGameConfigFile && __s2sdk_CloseGameConfigFile &&
		__s2sdk_GetGameConfigOffset && __s2sdk_GetGameConfigSignature &&
		__s2sdk_FindConVar && __s2sdk_GetConVarValue && __s2sdk_GetGameRules &&
		__s2sdk_GetSchemaOffset &&
		__s2sdk_GetEntityClassname && __s2sdk_GetEntityParent &&
		__s2sdk_IsValidEntHandle && __s2sdk_EntHandleToEntPointer &&
		__s2sdk_EntPointerToEntHandle && __s2sdk_GetFirstActiveEntity &&
		__s2sdk_GetNextActiveEntity && __s2sdk_GetMaxClients &&
		__s2sdk_IsClientInGame && __s2sdk_IsClientAlive && __s2sdk_GetClientPawn &&
		__s2sdk_GetClientTeam && __s2sdk_PlayerSlotToEntHandle &&
		__s2sdk_AcceptEntityInput && __s2sdk_HookEvent && __s2sdk_UnhookEvent &&
		__s2sdk_GetEventPlayerSlot && __s2sdk_OnClientDisconnect_Register &&
		__s2sdk_OnClientDisconnect_Unregister;
}

bool PolyHookImportsAvailable() noexcept {
	return __polyhook_HookDetour && __polyhook_HookDetour2 && __polyhook_FindDetour &&
		__polyhook_AddCallback && __polyhook_RemoveCallback &&
		__polyhook_UnhookDetour && __polyhook_IsCallbackRegistered &&
		__polyhook_AreCallbacksRegistered && __polyhook_GetArgumentPointer &&
		__polyhook_GetRegisterPointer;
}

bool PawnRescueCallMatches() {
	const auto image = LocateServerImage();
	if ( !ServerCodeUnmodified( image, g_pawnRescueMarker, 23 ) ||
		!ServerCodeUnmodified( image, g_pawnRescueBody, 8 ) ) {
		return false;
	}
	// The marker is a three-byte MOV followed by the rel32 CALL. Hook the MOV
	// so PolyHook does not follow the CALL, and confirm the separately resolved body.
	const auto* call = static_cast<const std::byte*>( g_pawnRescueMarker ) + 3;
	if ( *call != std::byte{ 0xE8 } ) {
		return false;
	}
	std::int32_t displacement{};
	std::memcpy( &displacement, call + 1, sizeof( displacement ) );
	return call + 5 + displacement == g_pawnRescueBody;
}

bool LoadGameConfig() noexcept {
	if ( g_gameConfig != 0 ) {
		return true;
	}
	if ( !__s2sdk_LoadGameConfigFile ) {
		g_initializationError = "S2SDK gameconfig imports unavailable";
		return false;
	}
	try {
		g_gameConfig = LoadServerFixesGameData();
		if ( g_gameConfig == 0 ) {
			g_initializationError = "could not load gamedata/imomoe_server_fixes.jsonc";
			return false;
		}
	} catch ( ... ) {
		g_initializationError = "loading ServerFixes gamedata threw";
		return false;
	}
	return true;
}

void ResolveTTargetGate( bool enabled ) noexcept {
	if ( !enabled ) {
		g_tGateAttempted = false;
		return;
	}
	if ( g_tGateAttempted || g_tTargetGate.configured() ) {
		return;
	}
	g_tGateAttempted = true;
	try {
		if ( __s2sdk_CloseGameConfigFile && __s2sdk_GetGameConfigSignature && LoadGameConfig() ) {
			(void)g_tTargetGate.configure( s2sdk::GetGameConfigSignature( g_gameConfig, "ServerFixes::HostageTeamGate" ),
				14, []( std::span<const std::uint8_t> code ) -> std::vector<std::uint8_t> {
					// cmp byte ptr [reg+disp32], CT; jnz rel8 (Windows) or jz rel32 (Linux).
					// Test the T bit and invert the condition without moving the branch.
					const std::size_t rex = code[0] == 0x41 ? 1 : 0;
					const auto cmp = code.subspan( rex );
					const bool nearJump = cmp[7] == 0x0F && cmp[8] == 0x84;
					if ( cmp[0] != 0x80 || cmp[1] != 0xBD || cmp[6] != 3 || ( !nearJump && cmp[7] != 0x75 ) ) {
						return {};
					}
					const auto* begin = code.data();
					std::vector<std::uint8_t> patched( begin, begin + rex + ( nearJump ? 13 : 9 ) );
					patched[rex] = 0xF6; // TEST r/m8, imm8
					patched[rex + 1] &= 0xC7; // Change the ModR/M extension from /7 to /0.
					patched[rex + 6] = 2;
					patched[rex + ( nearJump ? 8 : 7 )] ^= 1;
					return patched;
				} );
		}
	} catch ( ... ) {
		// Leave only the T gate unavailable; CT observers resolve independently.
	}
}

int ResolveDispatchTargetOffset() {
	const int operand = s2sdk::GetGameConfigOffset( g_gameConfig, "ServerFixes::HostageDispatchTargetOperand" );
	std::array<std::byte, 32> code{};
	if ( operand < 3 || static_cast<std::size_t>( operand ) >= code.size() ||
		!ReadServerCode( LocateServerImage(), g_hostageRescueHandler, std::span( code ).first( static_cast<std::size_t>( operand ) + 1 ) ) ) {
		return 0;
	}
	const auto byte = [&]( int at ) { return std::to_integer<unsigned char>( code[static_cast<std::size_t>( at )] ); };
	// The native handler loads the handle pointer with a MOV using disp8.
	if ( ( byte( operand - 3 ) & 0xfe ) != 0x48 || byte( operand - 2 ) != 0x8b ||
		( byte( operand - 1 ) & 0xc7 ) != 0x40 || byte( operand ) > 0x7f ) {
		return 0;
	}
	return byte( operand );
}

bool ResolveBuildInputs() noexcept {
	if ( !__s2sdk_GetGameConfigOffset || !__s2sdk_GetGameConfigSignature ) {
		g_initializationError = "S2SDK gamedata lookup imports unavailable";
		return false;
	}
	try {
		g_touchVTableOffset = s2sdk::GetGameConfigOffset( g_gameConfig, plg::string( "CBaseEntity::Touch" ) );
		g_hostageRescueHandler = s2sdk::GetGameConfigSignature( g_gameConfig,
			plg::string( "ServerFixes::HostageOnRescueZoneTouch" ) );
		const auto dispatchTargetOffset = ResolveDispatchTargetOffset();
		g_pawnRescueMarker = s2sdk::GetGameConfigSignature( g_gameConfig, "ServerFixes::PawnRescueBeforeBodyCall" );
		g_pawnRescueBody = s2sdk::GetGameConfigSignature( g_gameConfig, "ServerFixes::PawnRescueBody" );
		if ( g_touchVTableOffset < 0 || dispatchTargetOffset <= 0 || !g_hostageRescueHandler || !PawnRescueCallMatches() ) {
			g_initializationError = "current Touch, pawn rescue marker, or native rescue-handler gamedata is unavailable";
			return false;
		}
		g_dispatchTargetOffset = static_cast<std::uintptr_t>( dispatchTargetOffset );
		const auto offset = []( std::string_view type, std::string_view field ) {
			return s2sdk::GetSchemaOffset( plg::string( type ), plg::string( field ) );
		};
		g_schema = {
			.hostageServices = offset( kPawnClass, "m_pHostageServices" ),
			.controller = offset( kPlayerPawnClass, "m_hController" ),
			.carriedHostage = offset( kHostageServicesClass, "m_hCarriedHostage" ),
			.carriedProp = offset( kHostageServicesClass, "m_hCarriedHostageProp" ),
			.isRescued = offset( kHostageEntityClass, "m_isRescued" ),
			.hostageState = offset( kHostageEntityClass, "m_nHostageState" ),
			.mapHasRescueZone = offset( "CCSGameRules", "m_bMapHasRescueZone" ),
			.triggerDisabled = offset( "CBaseTrigger", "m_bDisabled" ),
			.roundStartTime = offset( "CCSGameRules", "m_fRoundStartTime" ),
			.roundWinStatus = offset( "CCSGameRules", "m_iRoundWinStatus" ),
			.warmupPeriod = offset( "CCSGameRules", "m_bWarmupPeriod" ),
		};
		if ( g_schema.hostageServices < 0 || g_schema.controller < 0 || g_schema.carriedHostage < 0 ||
			g_schema.carriedProp < 0 || g_schema.isRescued < 0 || g_schema.hostageState < 0 ||
			g_schema.mapHasRescueZone < 0 || g_schema.triggerDisabled < 0 ) {
			g_initializationError = "a required hostage/player schema field is unavailable";
			return false;
		}
		g_initializationError.clear();
		return true;
	} catch ( ... ) {
		g_initializationError = "resolving current hostage gamedata threw";
		return false;
	}
}

bool SyncTouchHooks() noexcept;
bool DetachAllPolyHookCallbacks() noexcept;
void ObserveHostageRescueHandler( polyhook::ParametersHandle parameters ) noexcept;
void ObservePawnRescueMarker( polyhook::ParametersHandle parameters ) noexcept;
polyhook::ResultType TouchCallback( polyhook::HookHandle, polyhook::ParametersHandle, std::int32_t,
	polyhook::ReturnHandle, polyhook::CallbackType ) noexcept;
polyhook::ResultType HostageRescueHandlerCallback( polyhook::HookHandle, polyhook::ParametersHandle, std::int32_t,
	polyhook::ReturnHandle, polyhook::CallbackType ) noexcept;
polyhook::ResultType PawnRescueMarkerCallback( polyhook::HookHandle, polyhook::ParametersHandle, std::int32_t,
	polyhook::ReturnHandle, polyhook::CallbackType ) noexcept;
s2sdk::ResultType GameEventCallback( const plg::string&, void*, bool ) noexcept;
void ClientDisconnectCallback( std::int32_t, s2sdk::NetworkDisconnectionReason ) noexcept;

bool EnsureCallback( void* address, polyhook::DataType returnType,
	const std::array<polyhook::DataType, 5>& argumentTypes, std::size_t argumentCount,
	polyhook::CallbackType type, polyhook::CallbackHandler callback, const char* name, bool midHook = false ) noexcept {
	if ( !address || !callback || !PolyHookImportsAvailable() ) {
		return false;
	}
	try {
		auto record = std::find_if( g_hookLeases.begin(), g_hookLeases.end(), [&]( const HookLease& lease ) {
			return lease.address == address && lease.type == type && lease.callback == callback &&
				lease.midHook == midHook;
		} );
		if ( record != g_hookLeases.end() ) {
			auto current = polyhook::FindDetour( address );
			if ( !current ) {
				record->handle = nullptr;
				record->callbackRegistered = false;
			} else if ( current != record->handle ) {
				record->handle = current;
			}
			if ( record->handle ) {
				record->callbackRegistered = polyhook::IsCallbackRegistered( record->handle, type, callback );
			}
			if ( midHook && record->handle && !record->callbackRegistered ) {
				// PolyHook exposes no query for whether an existing address uses a
				// register-snapshot mid-hook or a typed function detour.
				return false;
			}
			if ( record->callbackRegistered && polyhook::IsCallbackRegistered( record->handle, type, callback ) ) {
				return true;
			}
			if ( !record->handle ) {
				if ( midHook ) {
					record->handle = polyhook::HookDetour2( address, plg::string( name ) );
				} else {
					auto args = plg::vector<polyhook::DataType>( argumentTypes.begin(),
						argumentTypes.begin() + static_cast<std::ptrdiff_t>( argumentCount ) );
					record->handle = polyhook::HookDetour( address, returnType, args, -1, plg::string( name ) );
				}
			}
			if ( !record->handle ) {
				return false;
			}
			if ( !polyhook::IsCallbackRegistered( record->handle, type, callback ) ) {
				(void)polyhook::AreCallbacksRegistered( record->handle );
				(void)polyhook::AddCallback( record->handle, type, callback );
			}
			record->callbackRegistered = polyhook::IsCallbackRegistered( record->handle, type, callback );
			return record->callbackRegistered;
		}

		g_hookLeases.reserve( g_hookLeases.size() + 1 );
		auto handle = polyhook::FindDetour( address );
		if ( handle ) {
			(void)polyhook::AreCallbacksRegistered( handle );
			if ( polyhook::IsCallbackRegistered( handle, type, callback ) ) {
				g_hookLeases.push_back( { address, handle, type, callback, true, midHook } );
				return true;
			}
			if ( midHook ) {
				// Do not interpret an unknown existing hook's parameter ABI as a
				// register snapshot for this instruction-level marker.
				return false;
			}
		} else {
			if ( midHook ) {
				handle = polyhook::HookDetour2( address, plg::string( name ) );
			} else {
				auto args = plg::vector<polyhook::DataType>( argumentTypes.begin(),
					argumentTypes.begin() + static_cast<std::ptrdiff_t>( argumentCount ) );
				handle = polyhook::HookDetour( address, returnType, args, -1, plg::string( name ) );
			}
			if ( !handle ) {
				return false;
			}
		}
		g_hookLeases.push_back( { address, handle, type, callback, false, midHook } );
		auto& lease = g_hookLeases.back();
		if ( !polyhook::IsCallbackRegistered( handle, type, callback ) ) {
			(void)polyhook::AddCallback( handle, type, callback );
		}
		lease.callbackRegistered = polyhook::IsCallbackRegistered( handle, type, callback );
		return lease.callbackRegistered;
	} catch ( ... ) {
		return false;
	}
}

bool DetachLease( HookLease& lease ) noexcept {
	if ( !lease.address ) {
		lease.handle = nullptr;
		lease.callbackRegistered = false;
		return true;
	}
	if ( !PolyHookImportsAvailable() ) {
		return false;
	}
	if ( !lease.handle ) {
		lease.callbackRegistered = false;
		return true;
	}
	try {
		auto current = polyhook::FindDetour( lease.address );
		if ( !current ) {
			lease.handle = nullptr;
			lease.callbackRegistered = false;
			return true;
		}
		if ( current != lease.handle ) {
			lease.handle = current;
			lease.callbackRegistered = polyhook::IsCallbackRegistered( current, lease.type, lease.callback );
		}
		if ( polyhook::IsCallbackRegistered( lease.handle, lease.type, lease.callback ) ) {
			(void)polyhook::RemoveCallback( lease.handle, lease.type, lease.callback );
			if ( polyhook::IsCallbackRegistered( lease.handle, lease.type, lease.callback ) ) {
				lease.callbackRegistered = true;
				return false;
			}
		}
		lease.callbackRegistered = false;
		if ( polyhook::AreCallbacksRegistered( lease.handle ) ) {
			// Another plugin still owns this shared detour. Leave it installed and
			// retain our raw address/handle so a later enable can rejoin it.
			return true;
		}
		(void)polyhook::UnhookDetour( lease.address );
		if ( polyhook::FindDetour( lease.address ) ) {
			return false;
		}
		lease.handle = nullptr;
		return true;
	} catch ( ... ) {
		return false;
	}
}

bool DetachAllPolyHookCallbacks() noexcept {
	bool success = true;
	for ( auto& lease : g_hookLeases ) {
		if ( !DetachLease( lease ) ) {
			success = false;
		}
	}
	return success;
}

bool CurrentTouchAddress( std::int32_t entityHandle, void*& address ) noexcept {
	address = nullptr;
	if ( g_touchVTableOffset < 0 ) {
		return false;
	}
	void* const entity = EntityPointer( entityHandle );
	if ( !entity || EntityHandle( entity ) != entityHandle ) {
		return false;
	}
	auto** const vtable = *reinterpret_cast<void***>( entity );
	if ( !vtable || !vtable[g_touchVTableOffset] ) {
		return false;
	}
	address = vtable[g_touchVTableOffset];
	return true;
}

std::vector<std::int32_t> CurrentRescueZones() {
	std::vector<std::int32_t> zones;
	std::int32_t current = s2sdk::GetFirstActiveEntity();
	for ( std::size_t visited = 0; visited < 131072 && IsValidHandle( current ); ++visited ) {
		if ( EnabledRescueZone( current ) ) {
			zones.push_back( current );
		}
		const auto next = s2sdk::GetNextActiveEntity( current );
		if ( next == current ) {
			break;
		}
		current = next;
	}
	return zones;
}

bool AttachTouchCallbacks( void* address ) noexcept {
	static constexpr std::array argumentTypes{
		polyhook::DataType::Pointer, polyhook::DataType::Pointer,
		polyhook::DataType::Void, polyhook::DataType::Void, polyhook::DataType::Void,
	};
	const bool pre = EnsureCallback( address, polyhook::DataType::Void, argumentTypes, 2,
		polyhook::CallbackType::Pre, TouchCallback, "ServerFixes::CBaseEntity::TouchPre" );
	const bool post = EnsureCallback( address, polyhook::DataType::Void, argumentTypes, 2,
		polyhook::CallbackType::Post, TouchCallback, "ServerFixes::CBaseEntity::TouchPost" );
	if ( !pre || !post ) {
		for ( auto& lease : g_hookLeases ) {
			if ( lease.address == address && lease.callback == TouchCallback ) {
				(void)DetachLease( lease );
			}
		}
		return false;
	}
	return true;
}

bool DetachHandlerObservers() noexcept;

bool AttachHandlerObservers() noexcept {
	static constexpr std::array argumentTypes{
		polyhook::DataType::Pointer, polyhook::DataType::Pointer, polyhook::DataType::Pointer,
		polyhook::DataType::Pointer, polyhook::DataType::Pointer,
	};
	const bool pawnReady = EnsureCallback( g_pawnRescueMarker, polyhook::DataType::Void, argumentTypes, 0,
		polyhook::CallbackType::Pre, PawnRescueMarkerCallback,
		"ServerFixes::PawnOnRescueZoneTouch::FinalArgumentMove", true );
	const bool hostageReady = EnsureCallback( g_hostageRescueHandler, polyhook::DataType::Int64, argumentTypes, 5,
		polyhook::CallbackType::Pre, HostageRescueHandlerCallback, "ServerFixes::HostageOnRescueZoneTouch" );
	if ( !pawnReady || !hostageReady ) {
		(void)DetachHandlerObservers();
		return false;
	}
	return true;
}

bool DetachTouchCallbacks() noexcept {
	bool success = true;
	for ( auto& lease : g_hookLeases ) {
		if ( lease.callback == TouchCallback && !DetachLease( lease ) ) {
			success = false;
		}
	}
	return success;
}

bool DetachHandlerObservers() noexcept {
	bool success = true;
	for ( auto& lease : g_hookLeases ) {
		if ( ( lease.callback == HostageRescueHandlerCallback || lease.callback == PawnRescueMarkerCallback ) &&
			!DetachLease( lease ) ) {
			success = false;
		}
	}
	return success;
}

bool SyncTouchHooks() noexcept {
	if ( !g_moduleEnabled || !g_pluginActive || !PolyHookImportsAvailable() ||
		!g_hostageRescueHandler ) {
		return false;
	}
	try {
		if ( !IsSupportedCustomGame() ) {
			const bool clean = DetachTouchCallbacks();
			g_rescueZones.clear();
			g_ctError = clean ? "CT fallback is restricted to game_type=3 game_mode=0" :
				"Touch callback removal pending outside game_type=3 game_mode=0";
			return clean;
		}
		if ( !MapHasRescueZone() ) {
			const bool clean = DetachTouchCallbacks();
			g_rescueZones.clear();
			g_ctError = clean ? "m_bMapHasRescueZone is false" : "Touch callback removal pending on map without rescue zones";
			return clean;
		}
		const auto zones = CurrentRescueZones();
		std::unordered_set<std::int32_t> newZoneSet( zones.begin(), zones.end() );
		std::unordered_set<void*> wantedAddresses;
		for ( const auto zone : zones ) {
			void* address{};
			if ( CurrentTouchAddress( zone, address ) ) {
				wantedAddresses.insert( address );
			}
		}

		bool success = !wantedAddresses.empty();
		for ( void* address : wantedAddresses ) {
			if ( !AttachTouchCallbacks( address ) ) {
				success = false;
			}
		}
		for ( auto& lease : g_hookLeases ) {
			if ( lease.callback == TouchCallback && !wantedAddresses.contains( lease.address ) &&
				!DetachLease( lease ) ) {
				success = false;
			}
		}
		g_rescueZones = std::move( newZoneSet );
		if ( zones.empty() ) {
			g_ctError = "no enabled func_hostage_rescue on this map";
		} else if ( wantedAddresses.empty() ) {
			g_ctError = "no current rescue-zone Touch implementation";
		} else if ( !success ) {
			g_ctError = "Touch observer registration or cleanup failed";
		} else {
			g_ctError.clear();
		}
		g_lastTouchSync = std::chrono::steady_clock::now();
		return success;
	} catch ( ... ) {
		g_ctError = "synchronizing rescue-zone Touch observers threw";
		return false;
	}
}

bool EnsureEventHooks() {
	if ( g_ctLifecycleReady ) {
		return true;
	}
	if ( !g_disconnectRegistered ) {
		s2sdk::OnClientDisconnect_Register( ClientDisconnectCallback );
		g_disconnectRegistered = true;
	}
	for ( auto& event : g_events ) {
		if ( !event.registered ) {
			if ( s2sdk::HookEvent( plg::string( event.name ), GameEventCallback, s2sdk::HookMode::Post ) !=
				s2sdk::EventHookError::Okay ) {
				g_ctError = "one or more required round/player game events could not be hooked";
				return false;
			}
			event.registered = true;
		}
	}
	// Preserve a submitted attempt across off/on in the same round.
	const auto roundStart = ActiveRoundStart();
	if ( roundStart && g_observedRoundStart && roundStart != g_observedRoundStart ) {
		++g_roundGeneration;
		g_attempts.Clear();
	}
	g_roundActive = roundStart.has_value();
	if ( roundStart ) {
		g_observedRoundStart = roundStart;
	}
	g_ctLifecycleReady = true;
	return true;
}

bool EnsureInitialized() noexcept {
	if ( !runtime_lifecycle::SchemaQueriesReady() ) {
		g_initializationError = "waiting for map start before querying engine schemas";
		return false;
	}
	if ( g_initialized ) {
		return true;
	}
	if ( !ImportsAvailable() || !PolyHookImportsAvailable() ) {
		g_initializationError = "required S2SDK or PolyHook import unavailable";
		return false;
	}
	g_initialized = LoadGameConfig() && ResolveBuildInputs();
	return g_initialized;
}

bool TouchPair( std::int32_t first, std::int32_t second, std::int32_t& zone,
	std::int32_t& pawnHandle ) noexcept {
	const bool firstZone = g_rescueZones.contains( first );
	const bool secondZone = g_rescueZones.contains( second );
	if ( firstZone == secondZone ) {
		return false;
	}
	zone = firstZone ? first : second;
	pawnHandle = firstZone ? second : first;
	return true;
}

bool CaptureTouchFrame( TouchFrame& frame, void* entity, void* other ) noexcept {
	if ( !g_moduleEnabled || !g_pluginActive || !g_roundActive || !entity || !other ) {
		return false;
	}
	const auto firstHandle = EntityHandle( entity );
	const auto secondHandle = EntityHandle( other );
	std::int32_t zone{};
	std::int32_t pawnHandle{};
	if ( !IsValidHandle( firstHandle ) || !IsValidHandle( secondHandle ) ||
		!TouchPair( firstHandle, secondHandle, zone, pawnHandle ) ||
		!EnabledRescueZone( zone ) ) {
		return false;
	}
	std::int32_t slot{};
	std::int32_t controller{};
	if ( !FindPlayerForPawn( pawnHandle, slot, controller ) ||
		static_cast<std::int32_t>( s2sdk::GetClientTeam( slot ) ) != 3 ) {
		return false;
	}
	const auto hostage = ReadCarriedHostage( pawnHandle );
	frame.relevant = true;
	frame.zone = zone;
	frame.slot = slot;
	frame.key = {
		.mapGeneration = runtime_lifecycle::MapGeneration(),
		.roundGeneration = g_roundGeneration,
		.connectionGeneration = g_slotGenerations[static_cast<std::size_t>( slot )].connection,
		.lifeGeneration = g_slotGenerations[static_cast<std::size_t>( slot )].life,
		.zone = zone,
		.controller = controller,
		.pawn = pawnHandle,
		.hostage = hostage.hostage,
		.carriedProp = hostage.prop,
		};
	frame.hasCarriedHostage = IsEntityHandleValuePresent( hostage.hostage ) && IsEntityHandleValuePresent( hostage.prop );
	return true;
}

void FinalizeTouchFrame( TouchFrame& frame ) noexcept {
	try {
		if ( !frame.relevant || frame.handlerObserved || !frame.handlerObservationCertain ||
			!frame.pawnRouteObservationCertain ||
			!frame.nativePawnRescueCallReached || !frame.hasCarriedHostage || !g_moduleEnabled ||
			!g_pluginActive || !g_ctObserversActive || !g_roundActive || !IsSupportedCustomGame() ||
			!MapHasRescueZone() || frame.slot < 0 || frame.slot >= static_cast<std::int32_t>( kMaxPlayerSlots ) ||
			runtime_lifecycle::MapGeneration() != frame.key.mapGeneration || g_roundGeneration != frame.key.roundGeneration ||
			g_slotGenerations[static_cast<std::size_t>( frame.slot )].connection != frame.key.connectionGeneration ||
			g_slotGenerations[static_cast<std::size_t>( frame.slot )].life != frame.key.lifeGeneration ||
			!EnabledRescueZone( frame.zone ) ||
			!ReadLivePlayer( frame.slot, frame.key.controller, frame.key.pawn, true, true ) ) {
			return;
		}
		const auto carried = ReadCarriedHostage( frame.key.pawn );
		if ( carried.hostage != frame.key.hostage || carried.prop != frame.key.carriedProp ) {
			return;
		}
		RescueFacts facts{
			.enabled = g_moduleEnabled,
			.roundActive = g_roundActive,
			.sameMapRoundConnectionAndLife = true,
			.nativePawnRescueCallReached = frame.nativePawnRescueCallReached,
			.nativePawnRouteObservationCertain = frame.pawnRouteObservationCertain,
			.supportedCustomGame = true,
			.mapHasRescueZone = true,
			.zoneIsValidAndEnabled = true,
			.playerIsAliveCounterTerrorist = true,
			.fullCarriedHostageAndProp = true,
			.hostageIsNotRescued = true,
			.nativeRescueHandlerWasInvoked = frame.handlerObserved,
			.nativeRescueHandlerObservationCertain = frame.handlerObservationCertain,
		};
		bool enabledNow{};
		if ( !ReadBoolConVar( g_enabledConVarHandle, enabledNow ) || !enabledNow ) {
			return;
		}
		const auto action = g_attempts.Finalize( frame.key, facts );
		if ( action != FinalizeAction::SubmitNativeInput ) {
			return;
		}
		const plg::any value{};
		s2sdk::AcceptEntityInput( frame.key.hostage, plg::string( "OnRescueZoneTouch" ), -1, -1,
			value, s2sdk::FieldType::Auto, 0 );
	} catch ( ... ) {
		// The complete identity tuple stays reserved: retrying an uncertain engine
		// input could duplicate the native reward/event/count path.
		g_ctError = "CT rescue post-processing threw; native input is not retried";
	}
}

polyhook::ResultType TouchCallback( polyhook::HookHandle, polyhook::ParametersHandle parameters,
	std::int32_t, polyhook::ReturnHandle, polyhook::CallbackType type ) noexcept {
	if ( type == polyhook::CallbackType::Pre ) {
		if ( g_touchOverflowDepth != 0 || g_touchDepth == g_touchFrames.size() ) {
			++g_touchOverflowDepth;
			return polyhook::ResultType::Ignored;
		}
		TouchFrame frame{};
		if ( CanObserveRescueTouch( g_pluginActive, g_moduleEnabled, g_ctObserversActive ) && !g_rescueZones.empty() ) {
			try {
				frame.entity = polyhook::GetArgumentPointer( parameters, 0 );
				frame.other = polyhook::GetArgumentPointer( parameters, 1 );
				(void)CaptureTouchFrame( frame, frame.entity, frame.other );
			} catch ( ... ) {
				frame.relevant = false;
			}
		}
		g_touchFrames[g_touchDepth++] = frame;
		return polyhook::ResultType::Ignored;
	}
	if ( type == polyhook::CallbackType::Post ) {
		if ( g_touchOverflowDepth != 0 ) {
			--g_touchOverflowDepth;
			return polyhook::ResultType::Ignored;
		}
		if ( g_touchDepth == 0 ) {
			return polyhook::ResultType::Ignored;
		}
		auto frame = g_touchFrames[--g_touchDepth];
		if ( !frame.relevant || !g_moduleEnabled || !g_ctObserversActive ) {
			return polyhook::ResultType::Ignored;
		}
		try {
			const auto entity = polyhook::GetArgumentPointer( parameters, 0 );
			const auto other = polyhook::GetArgumentPointer( parameters, 1 );
			if ( frame.entity == entity && frame.other == other ) {
				FinalizeTouchFrame( frame );
			} else {
				g_touchDepth = 0;
			}
		} catch ( ... ) {
			g_touchDepth = 0;
		}
	}
	return polyhook::ResultType::Ignored;
}

bool HasRelevantTouchFrame() noexcept {
	return g_pluginActive && g_moduleEnabled && g_ctObserversActive && !g_rescueZones.empty() &&
		std::any_of( g_touchFrames.begin(), g_touchFrames.begin() + static_cast<std::ptrdiff_t>( g_touchDepth ),
			[]( const TouchFrame& frame ) { return frame.relevant && frame.hasCarriedHostage; } );
}

void ObserveHostageRescueHandler( polyhook::ParametersHandle parameters ) noexcept {
	if ( !HasRelevantTouchFrame() ) {
		return;
	}
	const auto markUncertain = [] {
		for ( std::size_t index = 0; index < g_touchDepth; ++index ) {
			auto& frame = g_touchFrames[index];
			if ( frame.relevant && frame.hasCarriedHostage ) {
				frame.handlerObservationCertain = false;
			}
		}
	};
	if ( !parameters ) {
		markUncertain();
		return;
	}
	try {
		std::int32_t targetHandle{};
		const auto* const dispatchArgument = static_cast<const std::byte*>(
			polyhook::GetArgumentPointer( parameters, 4 ) );
		if ( !ReadHostageDispatchTarget( dispatchArgument, g_dispatchTargetOffset, targetHandle ) || !IsValidHandle( targetHandle ) ) {
			markUncertain();
			return;
		}
		for ( std::size_t index = 0; index < g_touchDepth; ++index ) {
			auto& frame = g_touchFrames[index];
			if ( frame.relevant && frame.hasCarriedHostage && IsMatchingHostageTarget( frame.key, targetHandle ) ) {
				frame.handlerObserved = true;
			}
		}
	} catch ( ... ) {
		// An unclassifiable invocation cannot prove that this touch missed the
		// hostage input, so leave the native path unmodified.
		markUncertain();
	}
}

void ObservePawnRescueMarker( polyhook::ParametersHandle parameters ) noexcept {
	if ( !HasRelevantTouchFrame() ) {
		return;
	}
	if ( !parameters ) {
		return;
	}
	try {
		auto registers = static_cast<polyhook::RegistersHandle>( parameters );
#if defined(_WIN32)
		void* const pawn = polyhook::GetRegisterPointer( registers, polyhook::RegisterType::RDI );
#elif defined(__linux__)
		void* const pawn = polyhook::GetRegisterPointer( registers, polyhook::RegisterType::RBX );
#else
#error "Hostage rescue marker register selection requires Windows x64 or Linux x64"
#endif
		if ( !pawn ) {
			return;
		}
		const auto pawnHandle = EntityHandle( pawn );
		if ( !IsValidHandle( pawnHandle ) || EntityPointer( pawnHandle ) != pawn ) {
			return;
		}
		for ( std::size_t depth = g_touchDepth; depth > 0; --depth ) {
			auto& frame = g_touchFrames[depth - 1];
			if ( frame.entity != pawn && frame.other != pawn ) {
				continue;
			}
			if ( !frame.relevant || frame.key.pawn != pawnHandle ) {
				continue;
			}
			if ( !frame.hasCarriedHostage ) {
				return;
			}
			if ( polyhook::FindDetour( g_pawnRescueBody ) ) {
				// A detour at the direct callee can supercede the call. The MOV marker
				// alone is not proof that the native pawn body will execute.
				frame.pawnRouteObservationCertain = false;
				return;
			}
			frame.nativePawnRescueCallReached = true;
			return;
		}
	} catch ( ... ) {
		// If the direct-call state cannot be classified, leave the frame unmarked.
	}
}

polyhook::ResultType HostageRescueHandlerCallback( polyhook::HookHandle, polyhook::ParametersHandle parameters,
	std::int32_t, polyhook::ReturnHandle, polyhook::CallbackType type ) noexcept {
	if ( type == polyhook::CallbackType::Pre ) {
		ObserveHostageRescueHandler( parameters );
	}
	return polyhook::ResultType::Ignored;
}

polyhook::ResultType PawnRescueMarkerCallback( polyhook::HookHandle, polyhook::ParametersHandle parameters,
	std::int32_t, polyhook::ReturnHandle, polyhook::CallbackType type ) noexcept {
	if ( type == polyhook::CallbackType::Pre ) {
		ObservePawnRescueMarker( parameters );
	}
	return polyhook::ResultType::Ignored;
}

std::int32_t SlotFromEvent( void* event ) noexcept {
	try {
		return event && __s2sdk_GetEventPlayerSlot ?
			s2sdk::GetEventPlayerSlot( event, plg::string( "userid" ) ) : -1;
	} catch ( ... ) {
		return -1;
	}
}

void BumpLifeGeneration( std::int32_t slot ) noexcept {
	if ( slot >= 0 && slot < static_cast<std::int32_t>( kMaxPlayerSlots ) ) {
		g_slotGenerations[static_cast<std::size_t>( slot )].life = ++g_generationSequence;
	}
}

void BumpConnectionGeneration( std::int32_t slot ) noexcept {
	if ( slot >= 0 && slot < static_cast<std::int32_t>( kMaxPlayerSlots ) ) {
		g_slotGenerations[static_cast<std::size_t>( slot )].connection = ++g_generationSequence;
		g_slotGenerations[static_cast<std::size_t>( slot )].life = ++g_generationSequence;
	}
}

s2sdk::ResultType GameEventCallback( const plg::string& name, void* event, bool ) noexcept {
	if ( !g_moduleEnabled || !g_ctLifecycleReady ) {
		return s2sdk::ResultType::Continue;
	}
	try {
		const std::string_view eventName( name.data(), name.size() );
		if ( eventName == "round_start" ) {
			++g_roundGeneration;
			g_roundActive = true;
			g_observedRoundStart = ActiveRoundStart();
			g_attempts.Clear();
			if ( g_moduleEnabled ) {
				(void)SyncTouchHooks();
			}
		} else if ( eventName == "round_end" ) {
			++g_roundGeneration;
			g_roundActive = false;
			g_observedRoundStart.reset();
			g_attempts.Clear();
		} else if ( eventName == "player_spawn" || eventName == "player_death" ) {
			BumpLifeGeneration( SlotFromEvent( event ) );
		}
	} catch ( ... ) {
		g_ctError = "round/player lifecycle event callback failed";
	}
	return s2sdk::ResultType::Continue;
}

void ClientDisconnectCallback( std::int32_t slot, s2sdk::NetworkDisconnectionReason ) noexcept {
	if ( g_moduleEnabled && g_ctLifecycleReady ) {
		BumpConnectionGeneration( slot );
	}
}

bool UnregisterEventHooks() noexcept {
	bool success = true;
	for ( auto& event : g_events ) {
		if ( !event.registered ) {
			continue;
		}
		try {
			if ( !__s2sdk_UnhookEvent || s2sdk::UnhookEvent( plg::string( event.name ), GameEventCallback,
				s2sdk::HookMode::Post ) != s2sdk::EventHookError::Okay ) {
				success = false;
			} else {
				event.registered = false;
			}
		} catch ( ... ) {
			success = false;
		}
	}
	return success;
}

bool UnregisterDisconnect() noexcept {
	if ( !g_disconnectRegistered ) {
		return true;
	}
	try {
		if ( __s2sdk_OnClientDisconnect_Unregister ) {
			s2sdk::OnClientDisconnect_Unregister( ClientDisconnectCallback );
			g_disconnectRegistered = false;
			return true;
		}
	} catch ( ... ) {
	}
	return false;
}

bool UpdateTTargetGate( bool enabled, bool mpOnlyKnown, bool mpOnlyCts ) noexcept {
	g_tTargetGate.game_thread_seen();
	if ( enabled && !g_tTargetGate.configured() ) {
		g_tGateError = "T target gate signature or instruction layout unavailable in gamedata";
		return false;
	}
	const bool desired = ShouldPatchTTargetGateWhenPolicyUnknown( enabled, mpOnlyKnown, mpOnlyCts );
	if ( !desired ) {
		if ( g_tTargetGate.active() ) {
			try {
				g_tTargetGate.disable();
			} catch ( const std::exception& error ) {
				g_tGateError = error.what();
				return false;
			} catch ( ... ) {
				g_tGateError = "could not restore T target selector";
				return false;
			}
		}
		if ( !enabled ) {
			g_tGateError = "disabled by configuration";
		} else if ( !mpOnlyKnown ) {
			g_tGateError = "mp_only_cts_rescue_hostages unavailable; native CT-only gate retained";
		} else if ( mpOnlyCts ) {
			g_tGateError = "native CT-only policy is active";
		} else {
			g_tGateError = "native target gate is not patched";
		}
		return true;
	}
	try {
		g_tTargetGate.enable();
		g_tGateError.clear();
		return true;
	} catch ( const std::exception& error ) {
		g_tGateError = error.what();
	} catch ( ... ) {
		g_tGateError = "could not enable T target selector";
	}
	return false;
}

bool UpdateCtObservers( bool enabled ) {
	g_ctObserversActive = false;
	if ( !enabled ) {
		g_touchDepth = g_touchOverflowDepth = 0;
		g_ctLifecycleReady = false;
		g_roundActive = false;
		g_rescueZones.clear();
		g_lastTouchSync = {};
		const bool callbacksClean = DetachAllPolyHookCallbacks();
		const bool eventsClean = UnregisterEventHooks();
		const bool disconnectClean = UnregisterDisconnect();
		const bool clean = callbacksClean && eventsClean && disconnectClean;
		g_ctError = clean ? "disabled by configuration" : "hostage observer or event removal pending";
		return clean;
	}
	if ( !EnsureInitialized() ) {
		g_ctError = g_initializationError;
		return false;
	}
	if ( !EnsureEventHooks() ) {
		return false;
	}
	if ( !AttachHandlerObservers() ) {
		g_ctError = "native rescue-handler observer registration failed";
		(void)DetachTouchCallbacks();
		return false;
	}
	const auto now = std::chrono::steady_clock::now();
	if ( g_lastTouchSync.time_since_epoch().count() == 0 || now - g_lastTouchSync >= std::chrono::seconds( 1 ) ) {
		if ( !SyncTouchHooks() ) {
			if ( g_ctError == "no enabled func_hostage_rescue on this map" ) {
				g_ctObserversActive = true;
				return true;
			}
			(void)DetachTouchCallbacks();
			return false;
		}
	}
	if ( !g_ctError.empty() && g_ctError != "CT fallback is restricted to game_type=3 game_mode=0" &&
		g_ctError != "m_bMapHasRescueZone is false" ) {
		return false;
	}
	g_ctObserversActive = true;
	return true;
}

std::string CurrentStatusReason( bool enabled, bool tOk, bool ctOk ) {
	std::string reason = enabled ? "enabled" : "disabled";
	reason += "; CT=";
	reason += ctOk ? "ready" : ( g_ctError.empty() ? "unavailable" : g_ctError );
	reason += "; T=";
	reason += tOk ? ( g_tGateError.empty() ? "ready" : g_tGateError ) :
		( g_tGateError.empty() ? "unavailable" : g_tGateError );
	return reason;
}

} // namespace

Status Update( bool enabled, std::uint64_t enabledConVarHandle ) noexcept {
	g_pluginActive = true;
	g_enabledConVarHandle = enabledConVarHandle;
	g_moduleEnabled = enabled;
	try {
		bool mpOnlyCts{};
		const bool mpOnlyAvailable = enabled &&
			ReadBoolConVar( CachedConVar( g_onlyCtsConVar, "mp_only_cts_rescue_hostages" ), mpOnlyCts );
		ResolveTTargetGate( enabled );
		const bool tUpdated = UpdateTTargetGate( enabled, mpOnlyAvailable, mpOnlyCts );
		const bool ctUpdated = UpdateCtObservers( enabled );
		const bool tAvailable = g_tTargetGate.code_recognized() &&
			( !enabled || ( mpOnlyAvailable && ( mpOnlyCts || g_tTargetGate.active() ) ) );
		const bool ctAvailable = enabled ? g_ctObserversActive : g_initialized;

		return { ( tUpdated && tAvailable ) || ( ctUpdated && ctAvailable ),
			CurrentStatusReason( enabled, tUpdated && tAvailable, ctUpdated && ctAvailable ) };
	} catch ( ... ) {
		g_ctError = "hostage compatibility update threw";
		return { false, CurrentStatusReason( enabled, false, false ) };
	}
}

void OnMapEnd() noexcept {
	g_touchDepth = g_touchOverflowDepth = 0;
	g_ctLifecycleReady = false;
	g_ctObserversActive = false;
	++g_roundGeneration;
	g_roundActive = false;
	g_observedRoundStart.reset();
	g_attempts.Clear();
	g_rescueZones.clear();
	g_lastTouchSync = {};
	if ( !DetachTouchCallbacks() ) {
		g_ctError = "Touch callback removal pending after map end";
	}
}

bool Shutdown() noexcept {
	g_touchDepth = g_touchOverflowDepth = 0;
	g_pluginActive = false;
	g_moduleEnabled = false;
	g_ctObserversActive = false;
	bool gateClean = true;
	if ( g_tTargetGate.active() ) {
		try {
			g_tTargetGate.disable();
		} catch ( ... ) {
			gateClean = false;
		}
	}
	const bool callbacksClean = DetachAllPolyHookCallbacks();
	const bool eventsClean = UnregisterEventHooks();
	const bool listenersClean = UnregisterDisconnect();
	if ( !gateClean || !callbacksClean || !eventsClean || !listenersClean ) {
		g_ctError = "hostage compatibility callbacks or target gate remain pending cleanup";
		return false;
	}
	if ( g_gameConfig != 0 && __s2sdk_CloseGameConfigFile ) {
		s2sdk::CloseGameConfigFile( g_gameConfig );
	}
	g_gameConfig = 0;
	g_initialized = false;
	g_enabledConVarHandle = 0;
	g_roundActive = false;
	g_observedRoundStart.reset();
	g_ctLifecycleReady = false;
	g_lastTouchSync = {};
	g_tTargetGate.reset_configuration();
	g_tGateAttempted = false;
	g_onlyCtsConVar = g_gameTypeConVar = g_gameModeConVar = 0;
	g_attempts.Clear();
	g_rescueZones.clear();
	return true;
}

} // namespace imomoe::server_fixes::hostage
