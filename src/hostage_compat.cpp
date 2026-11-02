// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "hostage_compat.hpp"

#include "hostage_dispatch_read.hpp"
#include "hostage_gate_patch.hpp"
#include "hostage_policy.hpp"

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
#include <limits>
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

::hostage::GatePatch g_tTargetGate;
SchemaReadiness g_schemaReadiness;
AttemptLedger g_attempts;
std::vector<HookLease> g_hookLeases;
std::unordered_set<std::int32_t> g_rescueZones;
std::array<SlotGenerations, kMaxPlayerSlots> g_slotGenerations{};
std::uint64_t g_generationSequence{1};
std::uint64_t g_mapGeneration{1};
std::uint64_t g_roundGeneration{};
bool g_initialized{};
bool g_pluginActive{};
bool g_moduleEnabled{};
bool g_ctObserversActive{};
bool g_roundActive{};
bool g_eventListenersRegistered{};
s2sdk::ConfigId g_gameConfig{};
std::int32_t g_touchVTableOffset{-1};
std::uint64_t g_enabledConVarHandle{};
void* g_hostageRescueHandler{};
void* g_pawnRescueDispatcher{};
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

bool ReadIntConVar( std::string_view name, std::int32_t& value ) noexcept {
	const auto handle = FindConVar( name );
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

bool ReadBoolConVar( std::string_view name, bool& value ) noexcept {
	const auto handle = FindConVar( name );
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

bool ReadEnabledConVar() noexcept {
	if ( !g_enabledConVarHandle || !__s2sdk_GetConVarValue ) {
		g_moduleEnabled = false;
		g_ctObserversActive = false;
		return false;
	}
	try {
		const auto text = s2sdk::GetConVarValue( g_enabledConVarHandle );
		const std::string_view value( text.data(), text.size() );
		if ( value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "on" ) {
			g_moduleEnabled = true;
			return true;
		}
		if ( value == "0" || value == "false" || value == "FALSE" || value == "no" || value == "off" ) {
			g_moduleEnabled = false;
			g_ctObserversActive = false;
			return false;
		}
	} catch ( ... ) {
	}
	g_moduleEnabled = false;
	g_ctObserversActive = false;
	return false;
}

bool IsSupportedCustomGame() noexcept {
	std::int32_t gameType{};
	std::int32_t gameMode{};
	return ReadIntConVar( "game_type", gameType ) && ReadIntConVar( "game_mode", gameMode ) &&
		gameType == 3 && gameMode == 0;
}

bool MapHasRescueZone() noexcept {
	if ( !__s2sdk_GetGameRules || !__s2sdk_GetEntSchema2 ||
		s2sdk::GetSchemaOffset( plg::string( "CCSGameRules" ), plg::string( "m_bMapHasRescueZone" ) ) < 0 ) {
		return false;
	}
	void* const rules = s2sdk::GetGameRules();
	return rules && s2sdk::GetEntSchema2( rules, plg::string( "CCSGameRules" ),
		plg::string( "m_bMapHasRescueZone" ), 0 ) != 0;
}

bool EnabledRescueZone( std::int32_t handle ) noexcept {
	if ( EntityClass( handle ) != kRescueZoneClass ) {
		return false;
	}
	void* const zone = EntityPointer( handle );
	return zone && s2sdk::GetSchemaOffset( plg::string( "CBaseTrigger" ), plg::string( "m_bDisabled" ) ) >= 0 &&
		s2sdk::GetEntSchema2( zone, plg::string( "CBaseTrigger" ), plg::string( "m_bDisabled" ), 0 ) == 0;
}

CarriedHostage ReadCarriedHostage( std::int32_t pawnHandle ) noexcept {
	try {
		void* const pawn = EntityPointer( pawnHandle );
		if ( !pawn || EntityClass( pawnHandle ) != kPlayerClass ||
			s2sdk::GetSchemaOffset( plg::string( kPawnClass ), plg::string( "m_pHostageServices" ) ) < 0 ) {
			return {};
		}
		const auto servicesValue = s2sdk::GetEntSchema2( pawn, plg::string( kPawnClass ),
			plg::string( "m_pHostageServices" ), 0 );
		auto* const services = reinterpret_cast<void*>( static_cast<std::uintptr_t>( servicesValue ) );
		if ( !services ) {
			return {};
		}
		if ( s2sdk::GetSchemaOffset( plg::string( kHostageServicesClass ), plg::string( "m_hCarriedHostage" ) ) < 0 ||
			s2sdk::GetSchemaOffset( plg::string( kHostageServicesClass ), plg::string( "m_hCarriedHostageProp" ) ) < 0 ) {
			return {};
		}
		const auto hostageHandle = s2sdk::GetEntSchemaEnt2( services,
			plg::string( kHostageServicesClass ), plg::string( "m_hCarriedHostage" ), 0 );
		const auto propHandle = s2sdk::GetEntSchemaEnt2( services,
			plg::string( kHostageServicesClass ), plg::string( "m_hCarriedHostageProp" ), 0 );
		void* const hostage = EntityPointer( hostageHandle );
		void* const prop = EntityPointer( propHandle );
		if ( !hostage || !prop || EntityClass( hostageHandle ) != kHostageClass ||
			s2sdk::GetSchemaOffset( plg::string( kHostageEntityClass ), plg::string( "m_isRescued" ) ) < 0 ||
			s2sdk::GetSchemaOffset( plg::string( kHostageEntityClass ), plg::string( "m_nHostageState" ) ) < 0 ||
			s2sdk::GetEntSchema2( hostage, plg::string( kHostageEntityClass ), plg::string( "m_isRescued" ), 0 ) != 0 ||
			s2sdk::GetEntSchema2( hostage, plg::string( kHostageEntityClass ), plg::string( "m_nHostageState" ), 0 ) != 3 ||
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
				s2sdk::GetSchemaOffset( plg::string( kPlayerPawnClass ), plg::string( "m_hController" ) ) < 0 ||
				s2sdk::GetEntSchemaEnt2( pawn, plg::string( kPlayerPawnClass ), plg::string( "m_hController" ), 0 ) != controller ) {
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
			s2sdk::GetSchemaOffset( plg::string( kPlayerPawnClass ), plg::string( "m_hController" ) ) < 0 ||
			s2sdk::GetEntSchemaEnt2( pawn, plg::string( kPlayerPawnClass ), plg::string( "m_hController" ), 0 ) != controller ) {
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
		__s2sdk_GetSchemaOffset && __s2sdk_GetEntSchema2 && __s2sdk_GetEntSchemaEnt2 &&
		__s2sdk_GetEntityClassname && __s2sdk_GetEntityParent &&
		__s2sdk_IsValidEntHandle && __s2sdk_EntHandleToEntPointer &&
		__s2sdk_EntPointerToEntHandle && __s2sdk_GetFirstActiveEntity &&
		__s2sdk_GetNextActiveEntity && __s2sdk_GetMaxClients &&
		__s2sdk_IsClientInGame && __s2sdk_IsClientAlive && __s2sdk_GetClientPawn &&
		__s2sdk_GetClientTeam && __s2sdk_PlayerSlotToEntHandle &&
		__s2sdk_AcceptEntityInput && __s2sdk_HookEvent && __s2sdk_UnhookEvent &&
		__s2sdk_GetEventPlayerSlot && __s2sdk_OnClientDisconnect_Register &&
		__s2sdk_OnClientDisconnect_Unregister && __s2sdk_OnMapStart_Register &&
		__s2sdk_OnMapStart_Unregister && __s2sdk_OnMapEnd_Register && __s2sdk_OnMapEnd_Unregister;
}

bool PolyHookImportsAvailable() noexcept {
	return __polyhook_HookDetour && __polyhook_HookDetour2 && __polyhook_FindDetour &&
		__polyhook_AddCallback && __polyhook_RemoveCallback &&
		__polyhook_UnhookDetour && __polyhook_IsCallbackRegistered &&
		__polyhook_AreCallbacksRegistered && __polyhook_GetArgumentPointer &&
		__polyhook_GetRegisterPointer;
}

bool ResolvePawnRescueMarker() noexcept {
	if ( !g_pawnRescueDispatcher ) {
		return false;
	}
	auto* const code = static_cast<std::byte*>( g_pawnRescueDispatcher );
	// Current CS2 Build 26334290 IDA databases: the dispatcher's final argument move is
	// immediately followed by its direct pawn-body call. PolyHook follows an E8 call when
	// hooking the call instruction itself, so place the register-snapshot mid-hook on this
	// preceding MOV and verify the whole MOV/call/return/callee route before attaching it.
#if defined(_WIN32)
	constexpr std::ptrdiff_t kCallOffset = 0x165;
	constexpr std::ptrdiff_t kMarkerOffset = 0x162;
	constexpr std::array<std::uint8_t, 3> kExpectedMarker{ 0x48, 0x8B, 0xCF }; // mov rcx, rdi
	constexpr std::array<std::uint8_t, 5> kExpectedCall{ 0xE8, 0x56, 0xDD, 0x04, 0x00 };
	constexpr std::array<std::uint8_t, 5> kExpectedReturn{ 0x48, 0x8B, 0x5C, 0x24, 0x50 };
	constexpr std::array<std::uint8_t, 8> kExpectedBody{ 0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B };
#elif defined(__linux__)
	constexpr std::ptrdiff_t kCallOffset = 0xFF;
	constexpr std::ptrdiff_t kMarkerOffset = 0xFC;
	constexpr std::array<std::uint8_t, 3> kExpectedMarker{ 0x48, 0x89, 0xDF }; // mov rdi, rbx
	constexpr std::array<std::uint8_t, 5> kExpectedCall{ 0xE8, 0x2C, 0xFB, 0xFF, 0xFF };
	constexpr std::array<std::uint8_t, 4> kExpectedReturn{ 0x48, 0x83, 0xC4, 0x20 };
	constexpr std::array<std::uint8_t, 8> kExpectedBody{ 0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56 };
#else
#error "Hostage rescue MOV marker is supported only on Windows x64 and Linux x64"
#endif
	auto* const marker = code + kMarkerOffset;
	auto* const directCall = code + kCallOffset;
	if ( marker + static_cast<std::ptrdiff_t>( kExpectedMarker.size() ) != directCall ||
		std::memcmp( marker, kExpectedMarker.data(), kExpectedMarker.size() ) != 0 ||
		std::memcmp( directCall, kExpectedCall.data(), kExpectedCall.size() ) != 0 ||
		std::memcmp( directCall + static_cast<std::ptrdiff_t>( kExpectedCall.size() ),
			kExpectedReturn.data(), kExpectedReturn.size() ) != 0 ) {
		return false;
	}
	std::int32_t relativeTarget{};
	std::memcpy( &relativeTarget, directCall + 1, sizeof( relativeTarget ) );
	const auto returnAddress = reinterpret_cast<std::uintptr_t>( directCall + kExpectedCall.size() );
	std::uintptr_t bodyAddress{};
	if ( relativeTarget >= 0 ) {
		const auto displacement = static_cast<std::uintptr_t>( relativeTarget );
		if ( returnAddress > std::numeric_limits<std::uintptr_t>::max() - displacement ) {
			return false;
		}
		bodyAddress = returnAddress + displacement;
	} else {
		const auto displacement = static_cast<std::uintptr_t>( -static_cast<std::int64_t>( relativeTarget ) );
		if ( returnAddress < displacement ) {
			return false;
		}
		bodyAddress = returnAddress - displacement;
	}
	auto* const body = reinterpret_cast<const std::uint8_t*>( bodyAddress );
	if ( std::memcmp( body, kExpectedBody.data(), kExpectedBody.size() ) != 0 ) {
		return false;
	}
	g_pawnRescueMarker = marker;
	g_pawnRescueBody = reinterpret_cast<void*>( bodyAddress );
	return true;
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
		plg::string configPath = plg::GetPlugin()->GetLocation();
		configPath += "/gamedata/imomoe_server_fixes.jsonc";
		plg::vector<plg::string> paths;
		paths.push_back( configPath );
		g_gameConfig = s2sdk::LoadGameConfigFile( paths );
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

bool ResolveBuildInputs() noexcept {
	if ( !__s2sdk_GetGameConfigOffset || !__s2sdk_GetGameConfigSignature ) {
		g_initializationError = "S2SDK gamedata lookup imports unavailable";
		return false;
	}
	try {
		g_touchVTableOffset = s2sdk::GetGameConfigOffset( g_gameConfig, plg::string( "CBaseEntity::Touch" ) );
		g_hostageRescueHandler = s2sdk::GetGameConfigSignature( g_gameConfig,
			plg::string( "ServerFixes::HostageOnRescueZoneTouch" ) );
		g_pawnRescueDispatcher = s2sdk::GetGameConfigSignature( g_gameConfig,
			plg::string( "ServerFixes::PawnOnRescueZoneTouch" ) );
		if ( g_touchVTableOffset < 0 || !g_hostageRescueHandler || !g_pawnRescueDispatcher ||
			!ResolvePawnRescueMarker() ) {
			g_initializationError = "current Touch, pawn rescue marker, or native rescue-handler gamedata is unavailable";
			return false;
		}
		for ( const auto& [className, fieldName] : {
			std::pair{ kPawnClass, std::string_view( "m_pHostageServices" ) },
			std::pair{ kPlayerPawnClass, std::string_view( "m_hController" ) },
			std::pair{ kHostageServicesClass, std::string_view( "m_hCarriedHostage" ) },
			std::pair{ kHostageServicesClass, std::string_view( "m_hCarriedHostageProp" ) },
			std::pair{ kHostageEntityClass, std::string_view( "m_isRescued" ) },
			std::pair{ kHostageEntityClass, std::string_view( "m_nHostageState" ) },
			std::pair{ std::string_view( "CCSGameRules" ), std::string_view( "m_bMapHasRescueZone" ) },
			std::pair{ std::string_view( "CBaseTrigger" ), std::string_view( "m_bDisabled" ) },
		} ) {
			if ( s2sdk::GetSchemaOffset( plg::string( className ), plg::string( fieldName ) ) < 0 ) {
				g_initializationError = "a required hostage/player schema field is unavailable";
				return false;
			}
		}
		g_initializationError.clear();
		return true;
	} catch ( ... ) {
		g_initializationError = "resolving current hostage gamedata threw";
		return false;
	}
}

bool EnsureEventHooks() noexcept;
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
void MapStartCallback() noexcept;
void MapEndCallback() noexcept;

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

bool EnsureEventHooks() noexcept {
	if ( !__s2sdk_OnClientDisconnect_Register || !__s2sdk_OnClientDisconnect_Unregister ||
		!__s2sdk_OnMapStart_Register || !__s2sdk_OnMapStart_Unregister ||
		!__s2sdk_OnMapEnd_Register || !__s2sdk_OnMapEnd_Unregister ) {
		g_ctError = "S2SDK disconnect/map lifecycle listeners unavailable";
		return false;
	}
	if ( !g_eventListenersRegistered ) {
		s2sdk::OnClientDisconnect_Register( ClientDisconnectCallback );
		s2sdk::OnMapStart_Register( MapStartCallback );
		s2sdk::OnMapEnd_Register( MapEndCallback );
		g_eventListenersRegistered = true;
	}
	if ( std::all_of( g_events.begin(), g_events.end(), []( const EventBinding& event ) { return event.registered; } ) ) {
		return true;
	}
	if ( !__s2sdk_HookEvent ) {
		g_ctError = "S2SDK game-event hooks unavailable";
		return false;
	}
	bool success = true;
	for ( auto& event : g_events ) {
		if ( event.registered ) {
			continue;
		}
		try {
			if ( s2sdk::HookEvent( plg::string( event.name ), GameEventCallback, s2sdk::HookMode::Post ) ==
				s2sdk::EventHookError::Okay ) {
				event.registered = true;
			} else {
				success = false;
			}
		} catch ( ... ) {
			success = false;
		}
	}
	if ( !success || !std::all_of( g_events.begin(), g_events.end(), []( const EventBinding& event ) { return event.registered; } ) ) {
		g_ctError = "one or more required round/player game events could not be hooked";
		return false;
	}
	return true;
}

bool EnsureInitialized() noexcept {
	const bool eventsReady = EnsureEventHooks();
	bool initialized = false;
	const bool schemaWorkStarted = g_schemaReadiness.RunSchemaDependent( [&] {
		if ( !ImportsAvailable() ) {
			g_initializationError = "required S2SDK entity, schema, event, or gamedata import unavailable";
			return;
		}
		if ( !PolyHookImportsAvailable() ) {
			g_initializationError = "required PolyHook callback/detour import unavailable";
			return;
		}
		if ( g_initialized ) {
			initialized = true;
			return;
		}
		if ( !LoadGameConfig() || !ResolveBuildInputs() ) {
			return;
		}
		g_initialized = true;
		if ( !eventsReady ) {
			g_ctError = g_ctError.empty() ? "round/player lifecycle events unavailable" : g_ctError;
		}
		g_initializationError.clear();
		initialized = true;
	} );
	if ( !schemaWorkStarted ) {
		g_initializationError = "waiting for map start before querying engine schemas";
	}
	return schemaWorkStarted && initialized;
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
		.mapGeneration = g_mapGeneration,
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
			g_mapGeneration != frame.key.mapGeneration || g_roundGeneration != frame.key.roundGeneration ||
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
		if ( ReadEnabledConVar() && CanObserveRescueTouch( g_pluginActive, g_moduleEnabled, g_ctObserversActive ) ) {
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
		try {
			const auto entity = polyhook::GetArgumentPointer( parameters, 0 );
			const auto other = polyhook::GetArgumentPointer( parameters, 1 );
			auto frame = g_touchFrames[--g_touchDepth];
			g_touchFrames[g_touchDepth] = {};
			if ( frame.entity == entity && frame.other == other ) {
				if ( ReadEnabledConVar() && CanObserveRescueTouch( g_pluginActive, g_moduleEnabled, g_ctObserversActive ) ) {
					FinalizeTouchFrame( frame );
				}
			} else {
				g_touchDepth = 0;
				g_touchFrames.fill( {} );
			}
		} catch ( ... ) {
			g_touchDepth = 0;
			g_touchFrames.fill( {} );
		}
	}
	return polyhook::ResultType::Ignored;
}

void ObserveHostageRescueHandler( polyhook::ParametersHandle parameters ) noexcept {
	if ( !g_pluginActive || !g_ctObserversActive || !ReadEnabledConVar() || g_touchDepth == 0 ) {
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
		if ( !ReadHostageDispatchTarget( dispatchArgument, targetHandle ) || !IsValidHandle( targetHandle ) ) {
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
	if ( !g_pluginActive || !g_ctObserversActive || !ReadEnabledConVar() || g_touchDepth == 0 ) {
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
	if ( !g_pluginActive ) {
		return s2sdk::ResultType::Continue;
	}
	try {
		const std::string_view eventName( name.data(), name.size() );
		if ( eventName == "round_start" ) {
			++g_roundGeneration;
			g_roundActive = true;
			g_attempts.Clear();
			if ( g_moduleEnabled ) {
				(void)SyncTouchHooks();
			}
		} else if ( eventName == "round_end" ) {
			++g_roundGeneration;
			g_roundActive = false;
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
	if ( g_pluginActive ) {
		BumpConnectionGeneration( slot );
	}
}

void MapStartCallback() noexcept {
	if ( g_pluginActive ) {
		g_schemaReadiness.OnMapStart();
	}
}

void MapEndCallback() noexcept {
	if ( !g_pluginActive ) {
		return;
	}
	++g_mapGeneration;
	++g_roundGeneration;
	g_roundActive = false;
	g_schemaReadiness.OnMapEnd();
	g_attempts.Clear();
	g_rescueZones.clear();
	if ( !DetachTouchCallbacks() ) {
		g_ctError = "Touch callback removal pending after map end";
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

bool UnregisterEventListeners() noexcept {
	if ( !g_eventListenersRegistered ) {
		return true;
	}
	if ( __s2sdk_OnClientDisconnect_Unregister ) {
		s2sdk::OnClientDisconnect_Unregister( ClientDisconnectCallback );
	}
	if ( __s2sdk_OnMapStart_Unregister ) {
		s2sdk::OnMapStart_Unregister( MapStartCallback );
	}
	if ( __s2sdk_OnMapEnd_Unregister ) {
		s2sdk::OnMapEnd_Unregister( MapEndCallback );
	}
	g_eventListenersRegistered = false;
	return __s2sdk_OnClientDisconnect_Unregister && __s2sdk_OnMapStart_Unregister &&
		__s2sdk_OnMapEnd_Unregister;
}

bool UpdateTTargetGate( bool enabled ) noexcept {
	g_tTargetGate.game_thread_seen();
	bool mpOnlyKnown{};
	bool mpOnlyCts{};
	mpOnlyKnown = ReadBoolConVar( "mp_only_cts_rescue_hostages", mpOnlyCts );
	const bool desired = ShouldPatchTTargetGateWhenPolicyUnknown( enabled, mpOnlyKnown, mpOnlyCts );
	if ( !desired ) {
		if ( g_tTargetGate.active() ) {
			try {
				(void)g_tTargetGate.disable();
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
		(void)g_tTargetGate.enable();
		g_tGateError.clear();
		return true;
	} catch ( const std::exception& error ) {
		g_tGateError = error.what();
	} catch ( ... ) {
		g_tGateError = "could not enable T target selector";
	}
	return false;
}

bool UpdateCtObservers( bool enabled ) noexcept {
	if ( !enabled ) {
		g_ctObserversActive = false;
		const bool clean = DetachAllPolyHookCallbacks();
		g_ctError = clean ? "disabled by configuration" : "Touch/rescue observer removal pending";
		g_rescueZones.clear();
		g_lastTouchSync = {};
		return clean;
	}
	g_ctObserversActive = false;
	if ( !g_initialized || !g_pluginActive || !g_eventListenersRegistered ||
		!std::all_of( g_events.begin(), g_events.end(), []( const EventBinding& event ) { return event.registered; } ) ) {
		if ( !g_ctError.size() ) {
			g_ctError = g_initializationError;
		}
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
		const bool tUpdated = UpdateTTargetGate( enabled );
		const bool initialized = EnsureInitialized();
		if ( !initialized ) {
			g_ctObserversActive = false;
			(void)DetachAllPolyHookCallbacks();
			g_ctError = g_initializationError;
		}
		const bool ctUpdated = initialized && UpdateCtObservers( enabled );
		bool mpOnlyCts{};
		const bool mpOnlyAvailable = ReadBoolConVar( "mp_only_cts_rescue_hostages", mpOnlyCts );
		const bool tAvailable = g_tTargetGate.supports_current_build() && g_tTargetGate.code_recognized() &&
			( !enabled || ( mpOnlyAvailable && ( mpOnlyCts || g_tTargetGate.active() ) ) );
		const bool ctAvailable = g_initialized && g_eventListenersRegistered &&
			std::all_of( g_events.begin(), g_events.end(), []( const EventBinding& event ) { return event.registered; } ) &&
		g_touchVTableOffset >= 0 && g_hostageRescueHandler && g_pawnRescueMarker &&
		g_pawnRescueBody && PolyHookImportsAvailable() &&
			(g_ctError.empty() || g_ctError == "disabled by configuration" ||
				g_ctError == "no enabled func_hostage_rescue on this map" ||
				g_ctError == "m_bMapHasRescueZone is false" ||
				g_ctError == "CT fallback is restricted to game_type=3 game_mode=0");
		return { ( tUpdated && tAvailable ) || ( ctUpdated && ctAvailable ),
			CurrentStatusReason( enabled, tUpdated && tAvailable, ctUpdated && ctAvailable ) };
	} catch ( ... ) {
		g_ctError = "hostage compatibility update threw";
		return { false, CurrentStatusReason( enabled, false, false ) };
	}
}

std::uint64_t MapGeneration() noexcept {
	return g_mapGeneration;
}

bool SchemaQueriesReady() noexcept {
	return g_schemaReadiness.CanQuery();
}

bool Shutdown() noexcept {
	g_pluginActive = false;
	g_moduleEnabled = false;
	g_ctObserversActive = false;
	bool gateClean = true;
	if ( g_tTargetGate.active() ) {
		try {
			(void)g_tTargetGate.disable();
		} catch ( ... ) {
			gateClean = false;
		}
	}
	const bool callbacksClean = DetachAllPolyHookCallbacks();
	const bool eventsClean = UnregisterEventHooks();
	const bool listenersClean = UnregisterEventListeners();
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
	g_schemaReadiness.OnMapEnd();
	g_attempts.Clear();
	g_rescueZones.clear();
	return true;
}

} // namespace imomoe::server_fixes::hostage
