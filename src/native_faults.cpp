// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "native_faults.hpp"
#include "compat_guard_policy.hpp"
#include "owned_code_patch.hpp"
#include "ramp_movement.hpp"
#include "server_code.hpp"
#include "server_fixes_gamedata.hpp"

#include <polyhook/polyhook.hpp>
#include <s2sdk/s2sdk/cvars.hpp>
#include <s2sdk/s2sdk/entities.hpp>
#include <s2sdk/s2sdk/schema.hpp>
#include <entity2/entityinstance.h>
#include <entity2/entitynetwork.h>
#include <netmessages.h>
#include <serversideclient.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

namespace imomoe::server_fixes::native_faults {
namespace {

using polyhook::CallbackType;
using polyhook::DataType;
using polyhook::ResultType;

enum class HookPhases { Pre, Post, Both };

struct Hook {
	void* address{};
	polyhook::HookHandle owned{};
	polyhook::CallbackHandler callback{};
	HookPhases phases{};

	const char* Attach( void* target, const char* name,
		polyhook::CallbackHandler handler, DataType result, const plg::vector<DataType>& args,
		bool mid = false, HookPhases phasesToAttach = HookPhases::Pre ) {
		if ( !target ) {
			return "native signature unavailable or live code changed";
		}
		if ( polyhook::FindDetour( target ) ) {
			return "native entry already detoured; hook ABI/ownership cannot be shared";
		}
		address = target;
		callback = handler;
		phases = phasesToAttach;
		owned = mid ? polyhook::HookDetour2( address, plg::string( name ) ) :
			polyhook::HookDetour( address, result, args, -1, plg::string( name ) );
		if ( !owned ) {
			return "could not install native hook";
		}
		const auto add = [&]( CallbackType type ) {
			(void)polyhook::AddCallback( owned, type, callback );
			return polyhook::IsCallbackRegistered( owned, type, callback );
		};
		return ( phases == HookPhases::Pre || add( CallbackType::Post ) ) &&
			( phases == HookPhases::Post || add( CallbackType::Pre ) ) ?
			nullptr : "could not attach native callback";
	}

	bool Release() noexcept {
		if ( !owned ) {
			return true;
		}
		try {
			const auto current = polyhook::FindDetour( address );
			const auto remove = [&]( CallbackType type ) {
				if ( polyhook::IsCallbackRegistered( current, type, callback ) ) {
					(void)polyhook::RemoveCallback( current, type, callback );
					return !polyhook::IsCallbackRegistered( current, type, callback );
				}
				return true;
			};
			if ( current && ( ( phases != HookPhases::Post && !remove( CallbackType::Pre ) ) ||
				( phases != HookPhases::Pre && !remove( CallbackType::Post ) ) ) ) {
				return false;
			}
			if ( current == owned && !polyhook::AreCallbacksRegistered( current ) ) {
				(void)polyhook::UnhookDetour( address );
				if ( polyhook::FindDetour( address ) == current ) {
					return false;
				}
			}
			*this = {};
			return true;
		} catch ( ... ) {
			return false;
		}
	}
};

struct Module {
	std::array<Hook, 3> hooks;
	OwnedCodePatch* patch{};
	bool armed{};
	bool attempted{};
	const char* error{};
	std::string patchError;

	void RememberPatchError( const std::exception& failure ) noexcept {
		try {
			patchError = failure.what();
		} catch ( ... ) {
			patchError.clear();
		}
	}

	const char* Reason( const char* fallback ) const noexcept {
		return patchError.empty() ? fallback : patchError.c_str();
	}

	bool Release() noexcept {
		armed = false;
		bool clean = true;
		for ( auto hook = hooks.rbegin(); hook != hooks.rend(); ++hook ) {
			clean = hook->Release() && clean;
		}
		if ( patch ) {
			try {
				patch->disable();
				patch->reset_configuration();
				patch = nullptr;
			} catch ( const std::exception& failure ) {
				RememberPatchError( failure );
				return false;
			} catch ( ... ) {
				return false;
			}
		}
		return clean;
	}
};

std::array<Module, static_cast<std::size_t>( Fix::Count )> g_modules;
Hook g_categorizeHook;
bool g_categorizeReady{};
OwnedCodePatch g_hammerIdPatch;
s2sdk::ConfigId g_config{};
void* ( *g_getParent )( const void* ){};
void ( *g_setLocalOrigin )( void*, const void* ){};
std::uint32_t g_beamEndOffset{};
char g_emptyEquipName[] = "";
std::int32_t g_hltvOffset{ -1 };
thread_local bool g_cstvMapReady{};
thread_local unsigned g_botSelectionDepth{};

struct MoveCall {
	void* services{};
	void* move{};

	bool operator==( const MoveCall& ) const = default;
};
thread_local MoveCall g_afterWaterMove;

struct JumpStamp {
	CEntityHandle pawn;
	std::int32_t tick{};
};
std::vector<JumpStamp> g_lastJumps;
std::int32_t g_jumpOwnerOffset{}, g_jumpPawnOffset{}, g_moveTickOffset{};
thread_local bool g_jumpMapReady{};
std::uint64_t g_rampStandable{};

struct KeyValuesLimits {
	int rate{};
	int burst{};
	int bytes{};
};
constexpr int kKeyValuesNativeMaxBytes = 1500;
std::array<std::uint64_t, 3> g_keyValuesConVars;
std::mutex g_keyValuesMutex;
KeyValuesLimits g_keyValuesLimits;
std::array<compat_guards::KeyValuesBudget, ABSOLUTE_PLAYER_LIMIT> g_keyValuesClients;

void ConfigureKeyValues( KeyValuesLimits limits ) {
	const std::lock_guard lock( g_keyValuesMutex );
	if ( g_keyValuesLimits.rate && !limits.rate ) {
		g_keyValuesClients = {};
	}
	g_keyValuesLimits = limits;
}

Module& State( Fix fix ) noexcept {
	return g_modules[static_cast<std::size_t>( fix )];
}

bool StartsWithIcase( std::string_view value, std::string_view prefix ) noexcept {
	if ( value.size() < prefix.size() ) {
		return false;
	}
	for ( std::size_t i = 0; i < prefix.size(); ++i ) {
		const auto c = value[i];
		if ( ( c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c ) != prefix[i] ) {
			return false;
		}
	}
	return true;
}

template <bool End>
ResultType OnBeam( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle, CallbackType ) noexcept {
	if ( !State( Fix::Beam ).armed ) {
		return ResultType::Ignored;
	}
	auto* beam = static_cast<CEntityInstance*>( polyhook::GetArgumentPointer( parameters, 0 ) );
	const auto* position = polyhook::GetArgumentPointer( parameters, 1 );
	// Use the same parent lookup as both native setters, not a parent handle heuristic.
	if ( g_getParent( beam ) ) {
		return ResultType::Ignored;
	}
	if constexpr ( End ) {
		// The native parented branch also notifies m_vecEndPos before writing it.
		auto* end = reinterpret_cast<std::byte*>( beam ) + g_beamEndOffset;
		std::array<float, 3> current{}, target{};
		std::memcpy( current.data(), end, sizeof( current ) );
		std::memcpy( target.data(), position, sizeof( target ) );
		if ( current != target ) {
			beam->NetworkStateChanged( NetworkStateChanged_t( g_beamEndOffset ) );
			std::memcpy( end, target.data(), sizeof( target ) );
		}
	} else {
		// Without a parent, local origin is world origin. Use the native scene/physics writer.
		g_setLocalOrigin( beam, position );
	}
	return ResultType::Supercede;
}

ResultType OnInput( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle result, CallbackType ) noexcept {
	if ( !State( Fix::Filter ).armed || polyhook::GetArgumentPointer( parameters, 2 ) ) {
		return ResultType::Ignored;
	}
	const auto* input = static_cast<const CUtlSymbolLarge*>( polyhook::GetArgumentPointer( parameters, 1 ) );
	const std::string_view name{ input->String() };
	constexpr std::string_view testActivator = "testactivator";
	if ( name.size() != testActivator.size() || !StartsWithIcase( name, testActivator ) ) {
		return ResultType::Ignored;
	}
	const auto* entity = static_cast<const CEntityIdentity*>( polyhook::GetArgumentPointer( parameters, 0 ) );
	if ( !StartsWithIcase( entity->GetClassname(), "filter_" ) ) {
		return ResultType::Ignored;
	}
	// Consumed without running the input handler or firing OnPass/OnFail.
	polyhook::SetReturnBool( result, true );
	return ResultType::Supercede;
}

ResultType OnEquipPrecache( polyhook::HookHandle, polyhook::ParametersHandle registers, std::int32_t,
	polyhook::ReturnHandle, CallbackType ) noexcept {
	if ( State( Fix::Equip ).armed ) {
		constexpr auto reg = polyhook::RegisterType::RAX;
		const auto* name = static_cast<const char*>( polyhook::GetRegisterPointer( registers, reg ) );
		if ( name && StartsWithIcase( name, "item_" ) ) {
			// This is the transient GetString result in Precache. The native empty-name
			// branch skips the call, and Spawn still reads the original entity keyvalues.
			polyhook::SetRegisterPointer( registers, reg, g_emptyEquipName );
		}
	}
	return ResultType::Ignored;
}

std::vector<std::uint8_t> HammerIdBranchPatch( std::span<const std::uint8_t> code ) {
	if ( code[0] != 0x74 || code[1] == 0 ) {
		return {};
	}
	// Redirect only the final skip to its fallthrough. Both native predicates
	// and the original typed hammerUniqueId read/write stay intact.
	return { code[0], 0 };
}

ResultType OnBotSelection( polyhook::HookHandle, polyhook::ParametersHandle, std::int32_t,
	polyhook::ReturnHandle, CallbackType type ) noexcept {
	if ( type == CallbackType::Pre ) {
		if ( g_cstvMapReady && State( Fix::CstvBotQuota ).armed ) {
			++g_botSelectionDepth;
		}
	} else if ( g_botSelectionDepth ) {
		--g_botSelectionDepth;
	}
	return ResultType::Ignored;
}

ResultType OnBotTeamCandidate( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle result, CallbackType ) noexcept {
	if ( !g_botSelectionDepth || !g_cstvMapReady || !State( Fix::CstvBotQuota ).armed ) {
		return ResultType::Ignored;
	}
	// Argument 1 is the live controller used immediately by the native predicate.
	const auto* controller = static_cast<const unsigned char*>( polyhook::GetArgumentPointer( parameters, 1 ) );
	if ( !controller || controller[g_hltvOffset] == 0 ) {
		return ResultType::Ignored;
	}
	polyhook::SetReturnBool( result, false );
	return ResultType::Supercede;
}

MoveCall MovementArguments( polyhook::ParametersHandle parameters ) noexcept {
	return { polyhook::GetArgumentPointer( parameters, 0 ), polyhook::GetArgumentPointer( parameters, 1 ) };
}

ResultType AfterWaterMove( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle, CallbackType ) noexcept {
	if ( State( Fix::WaterJump ).armed ) {
		g_afterWaterMove = MovementArguments( parameters );
	}
	return ResultType::Ignored;
}

ResultType OnCategorizePosition( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle, CallbackType phase ) noexcept {
	const auto args = MovementArguments( parameters );
	auto* move = static_cast<CMoveData*>( args.move );
	if ( phase == CallbackType::Post ) {
		ramp::End( ramp::Phase::Ground, args.services, move );
		return ResultType::Ignored;
	}
	// The pinned caller invokes CategorizePosition immediately after WaterMove,
	// with the same arguments and stayOnGround=false. Consume that handoff once.
	const auto previous = std::exchange( g_afterWaterMove, {} );
	const bool stayOnGround = polyhook::GetArgumentBool( parameters, 2 );
	const bool skip = previous.services && State( Fix::WaterJump ).armed && previous == args && !stayOnGround;
	ramp::Begin( ramp::Phase::Ground, args.services, skip || stayOnGround ? nullptr : move );
	return skip ? ResultType::Supercede : ResultType::Ignored;
}

ResultType OnRampTryMove( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle, CallbackType phase ) noexcept {
	const auto args = MovementArguments( parameters );
	auto* move = static_cast<CMoveData*>( args.move );
	if ( phase == CallbackType::Pre ) {
		ramp::Begin( ramp::Phase::Slide, args.services, move );
	} else {
		ramp::End( ramp::Phase::Slide, args.services, move );
	}
	return ResultType::Ignored;
}

ResultType AfterRampHullTrace( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle, CallbackType ) noexcept {
	const auto argument = [&]( std::uint64_t n ) { return polyhook::GetArgumentPointer( parameters, n ); };
	ramp::AfterTrace( argument( 0 ), static_cast<CGameTrace*>( argument( 1 ) ),
		static_cast<const Vector*>( argument( 2 ) ), static_cast<const Vector*>( argument( 3 ) ),
		argument( 4 ), static_cast<CTraceFilter*>( argument( 5 ) ) );
	return ResultType::Ignored;
}

const char* EnsureCategorize( void* entry ) {
	if ( g_categorizeReady ) {
		return nullptr;
	}
	const char* error = g_categorizeHook.Attach( entry, "ServerFixes::CategorizePosition", OnCategorizePosition,
		DataType::Void, { DataType::Pointer, DataType::Pointer, DataType::Bool }, false, HookPhases::Both );
	g_categorizeReady = !error;
	return error;
}

bool ReleaseCategorizeIfUnused() noexcept {
	if ( State( Fix::WaterJump ).armed || State( Fix::RampBug ).armed ) {
		return true;
	}
	g_categorizeReady = false;
	return g_categorizeHook.Release();
}

template <typename T>
T ReadMoveField( const void* object, std::int32_t offset ) noexcept {
	T value;
	std::memcpy( &value, static_cast<const std::byte*>( object ) + offset, sizeof( value ) );
	return value;
}

std::optional<JumpStamp> JumpArguments( polyhook::ParametersHandle parameters, bool fromJumpState ) noexcept {
	if ( !g_jumpMapReady || !State( Fix::JumpSpam ).armed ) {
		return std::nullopt;
	}
	auto args = MovementArguments( parameters );
	if ( !args.services || !args.move ) {
		return std::nullopt;
	}
	if ( fromJumpState ) {
		// Both jump-state objects carry the same movement-service back pointer.
		args.services = ReadMoveField<void*>( args.services, g_jumpOwnerOffset );
	}
	if ( !args.services ) {
		return std::nullopt;
	}
	void* pawn = ReadMoveField<void*>( args.services, g_jumpPawnOffset );
	if ( !pawn ) {
		return std::nullopt;
	}
	const CEntityHandle handle{ static_cast<std::uint32_t>( s2sdk::EntPointerToEntHandle( pawn ) ) };
	if ( !handle.IsValid() ) {
		return std::nullopt;
	}
	return JumpStamp{ handle, ReadMoveField<std::int32_t>( args.move, g_moveTickOffset ) };
}

ResultType BeforeJumpAttempt( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle, CallbackType ) noexcept {
	if ( const auto jump = JumpArguments( parameters, true ) ) {
		const auto& last = g_lastJumps[static_cast<std::size_t>( jump->pawn.GetEntryIndex() )];
		if ( last.pawn == jump->pawn && last.tick == jump->tick ) {
			return ResultType::Supercede;
		}
	}
	return ResultType::Ignored;
}

ResultType OnJumpCompleted( polyhook::HookHandle, polyhook::ParametersHandle parameters, std::int32_t,
	polyhook::ReturnHandle, CallbackType ) noexcept {
	// Both native paths reach this helper only after applying a real jump.
	// Record before later native notifications can re-enter movement.
	if ( const auto jump = JumpArguments( parameters, false ) ) {
		g_lastJumps[static_cast<std::size_t>( jump->pawn.GetEntryIndex() )] = *jump;
	}
	return ResultType::Ignored;
}

ResultType OnKeyValues( polyhook::HookHandle, polyhook::ParametersHandle parameters,
	std::int32_t, polyhook::ReturnHandle result, CallbackType ) noexcept {
	const auto& client = *static_cast<const CServerSideClient*>( polyhook::GetArgumentPointer( parameters, 0 ) );
	const auto slot = client.GetPlayerSlot();
	const auto user = client.GetUserID().Get();
	if ( !slot.IsValid() || user == -1 ) {
		return ResultType::Ignored;
	}
	const auto& message = *static_cast<const CCLCMsg_CmdKeyValues_t*>( polyhook::GetArgumentPointer( parameters, 1 ) );
	{
		const std::lock_guard lock( g_keyValuesMutex );
		const auto limits = g_keyValuesLimits;
		if ( !limits.rate ) {
			return ResultType::Ignored;
		}
		auto& budget = g_keyValuesClients[static_cast<std::size_t>( slot.Get() )];
		if ( budget.Take( user, message.data().size(), compat_guards::TokenBucket::Clock::now(),
			limits.rate, limits.burst, limits.bytes ) ) {
			return ResultType::Ignored;
		}
	}
	// A dropped command is handled, not a failed network message.
	polyhook::SetReturnBool( result, true );
	return ResultType::Supercede;
}

const char* Attach( Fix fix, Module& module ) {
	if ( fix == Fix::FreeClient ) {
		return "no verified reusable client at native rejection; no hook installed";
	}
	if ( fix != Fix::HammerId && ( !__polyhook_FindDetour || !__polyhook_AddCallback || !__polyhook_RemoveCallback ||
		!__polyhook_IsCallbackRegistered || !__polyhook_AreCallbacksRegistered || !__polyhook_UnhookDetour ) ) {
		return "required PolyHook imports unavailable";
	}
	const auto image = fix == Fix::KeyValues ? LocateEngineImage() : LocateServerImage();
	if ( !image.ok ) {
		return fix == Fix::KeyValues ? "unsupported or unloaded engine2 image" : "unsupported or unloaded server image";
	}
	if ( !__s2sdk_LoadGameConfigFile || !__s2sdk_CloseGameConfigFile || !__s2sdk_GetGameConfigSignature ) {
		return "required S2SDK gamedata imports unavailable";
	}
	// S2SDK preloads and caches every signature at the first open. Keep that
	// lease while our detours are installed so later modules do not rescan them.
	if ( !g_config ) {
		g_config = LoadServerFixesGameData();
	}
	if ( !g_config ) {
		return "could not load ServerFixes gamedata";
	}
	const auto signature = [&]( const char* name ) -> void* {
		auto* address = s2sdk::GetGameConfigSignature( g_config, name );
		// PolyHook's longest in-place detour can overwrite 23 bytes.
		return ServerCodeUnmodified( image, address, 23 ) ? address : nullptr;
	};
	switch ( fix ) {
	case Fix::Beam: {
		if ( !__polyhook_HookDetour || !__polyhook_GetArgumentPointer || !__s2sdk_GetGameConfigOffset ) {
			return "required beam hook or offset imports unavailable";
		}
		g_setLocalOrigin = reinterpret_cast<decltype( g_setLocalOrigin )>( signature( "ServerFixes::BeamWriteLocalOrigin" ) );
		g_getParent = reinterpret_cast<decltype( g_getParent )>( signature( "ServerFixes::BeamGetParent" ) );
		const auto offset = s2sdk::GetGameConfigOffset( g_config, "CBeam::m_vecEndPos" );
		if ( !g_setLocalOrigin || !g_getParent || offset <= 0 ) {
			return "beam parent query, local origin setter, or endpoint offset unavailable in gamedata";
		}
		g_beamEndOffset = static_cast<std::uint32_t>( offset );
		if ( const char* error = module.hooks[0].Attach( signature( "ServerFixes::BeamSetOrigin" ), "ServerFixes::BeamSetOrigin",
			OnBeam<false>, DataType::Void, { DataType::Pointer, DataType::Pointer } ) ) {
			return error;
		}
		return module.hooks[1].Attach( signature( "ServerFixes::BeamSetEndPos" ), "ServerFixes::BeamSetEndPos",
			OnBeam<true>, DataType::Void, { DataType::Pointer, DataType::Pointer } );
	}
	case Fix::Filter:
		if ( !__polyhook_HookDetour || !__polyhook_GetArgumentPointer || !__polyhook_SetReturnBool ) {
			return "required input hook imports unavailable";
		}
		return module.hooks[0].Attach( signature( "ServerFixes::FilterAcceptInput" ), "ServerFixes::FilterAcceptInput",
			OnInput, DataType::Bool, { DataType::Pointer, DataType::Pointer, DataType::Pointer, DataType::Pointer,
				DataType::Pointer, DataType::Pointer, DataType::Pointer } );
	case Fix::Equip:
		if ( !__polyhook_HookDetour2 || !__polyhook_GetRegisterPointer || !__polyhook_SetRegisterPointer ) {
			return "required precache hook imports unavailable";
		}
		// Start before the result is saved: neither platform has incoming branches
		// in the overwrite window, including PolyHook's 23-byte in-place fallback.
		return module.hooks[0].Attach( signature( "ServerFixes::PlayerEquipPrecacheLookup" ), "ServerFixes::PlayerEquipPrecacheLookup",
			OnEquipPrecache, DataType::Void, {}, true );
	case Fix::HammerId:
		if ( !g_hammerIdPatch.configure( s2sdk::GetGameConfigSignature( g_config, "ServerFixes::HammerIdSkip" ),
			24, HammerIdBranchPatch ) ) {
			return "Hammer ID branch unavailable or live code changed";
		}
		module.patch = &g_hammerIdPatch;
		g_hammerIdPatch.game_thread_seen();
		g_hammerIdPatch.enable();
		return nullptr;
	case Fix::CstvBotQuota: {
		if ( !__polyhook_HookDetour || !__polyhook_GetArgumentPointer || !__polyhook_SetReturnBool ||
			!__s2sdk_GetSchemaOffset ) {
			return "required CSTV hook or schema imports unavailable";
		}
		g_hltvOffset = s2sdk::GetSchemaOffset( "CBasePlayerController", "m_bIsHLTV" );
		if ( g_hltvOffset < 0 ) {
			return "CBasePlayerController::m_bIsHLTV is unavailable";
		}
		if ( const char* error = module.hooks[0].Attach( signature( "ServerFixes::SelectAndKickBot" ),
			"ServerFixes::SelectAndKickBot", OnBotSelection, DataType::Bool, { DataType::Int32 }, false, HookPhases::Both ) ) {
			return error;
		}
		return module.hooks[1].Attach( signature( "ServerFixes::BotTeamCandidate" ), "ServerFixes::BotTeamCandidate",
			OnBotTeamCandidate, DataType::Bool, { DataType::Pointer, DataType::Pointer, DataType::Pointer } );
	}
	case Fix::WaterJump:
		if ( !__polyhook_HookDetour || !__polyhook_GetArgumentPointer || !__polyhook_GetArgumentBool ) {
			return "required water movement hook imports unavailable";
		}
		if ( const char* error = module.hooks[0].Attach( signature( "ServerFixes::WaterMove" ),
			"ServerFixes::WaterMove", AfterWaterMove, DataType::Void, { DataType::Pointer, DataType::Pointer },
			false, HookPhases::Post ) ) {
			return error;
		}
		return EnsureCategorize( signature( "ServerFixes::CategorizePosition" ) );
	case Fix::RampBug: {
		if ( !__polyhook_HookDetour || !__polyhook_GetArgumentPointer || !__polyhook_GetArgumentBool ||
			!__s2sdk_GetSchemaOffset || !__s2sdk_GetGameConfigOffset || !__s2sdk_EntPointerToEntHandle ||
			!__s2sdk_FindConVar || !__s2sdk_GetConVarFloat ) {
			return "required ramp movement imports unavailable";
		}
		auto* trace = signature( "ServerFixes::RampUncachedHullTrace" );
		const auto cache = s2sdk::GetGameConfigOffset( g_config, "ServerFixes::RampMovementTraceCache" );
		const auto pawn = s2sdk::GetSchemaOffset( "CPlayerPawnComponent", "__m_pChainEntity" );
		const auto ground = s2sdk::GetSchemaOffset( "CBaseEntity", "m_hGroundEntity" );
		const auto moveType = s2sdk::GetSchemaOffset( "CBaseEntity", "m_nActualMoveType" );
		const auto water = s2sdk::GetSchemaOffset( "CBaseEntity", "m_flWaterLevel" );
		g_rampStandable = s2sdk::FindConVar( "sv_standable_normal" );
		if ( !trace || cache <= 0 || pawn <= 0 || ground <= 0 || moveType <= 0 || water <= 0 || !g_rampStandable ) {
			return "native ramp trace or movement fields unavailable";
		}
		ramp::Configure( reinterpret_cast<ramp::HullTrace>( trace ), cache, pawn, ground, moveType, water );
		using enum DataType;
		if ( const char* error = module.hooks[0].Attach( signature( "ServerFixes::RampTryPlayerMove" ),
			"ServerFixes::RampTryPlayerMove", OnRampTryMove, Void,
			{ Pointer, Pointer, Pointer, Pointer, Pointer }, false, HookPhases::Both ) ) {
			return error;
		}
		// Preserve the native general-purpose return register in all callers.
		if ( const char* error = module.hooks[1].Attach( signature( "ServerFixes::RampCachedHullTrace" ),
			"ServerFixes::RampCachedHullTrace", AfterRampHullTrace, UInt64,
			{ Pointer, Pointer, Pointer, Pointer, Pointer, Pointer }, false, HookPhases::Post ) ) {
			return error;
		}
		return EnsureCategorize( signature( "ServerFixes::CategorizePosition" ) );
	}
	case Fix::JumpSpam: {
		if ( !__polyhook_HookDetour || !__polyhook_GetArgumentPointer || !__s2sdk_EntPointerToEntHandle ||
			!__s2sdk_GetSchemaOffset || !__s2sdk_GetGameConfigOffset ) {
			return "required jump hook or schema imports unavailable";
		}
		g_jumpPawnOffset = s2sdk::GetSchemaOffset( "CPlayerPawnComponent", "__m_pChainEntity" );
		g_jumpOwnerOffset = s2sdk::GetGameConfigOffset( g_config, "ServerFixes::JumpStateMovementServices" );
		g_moveTickOffset = s2sdk::GetGameConfigOffset( g_config, "ServerFixes::MoveDataTick" );
		if ( g_jumpPawnOffset <= 0 || g_jumpOwnerOffset <= 0 || g_moveTickOffset <= 0 ) {
			return "jump owner or move tick unavailable";
		}
		g_lastJumps.assign( NUM_ENT_ENTRIES, {} );
		const plg::vector<DataType> args{ DataType::Pointer, DataType::Pointer };
		for ( std::size_t i = 0; i < 2; ++i ) {
			const char* name = i == 0 ? "ServerFixes::LegacyJumpAttempt" : "ServerFixes::ModernJumpAttempt";
			if ( const char* error = module.hooks[i].Attach( signature( name ), name, BeforeJumpAttempt, DataType::Void, args ) ) {
				return error;
			}
		}
		return module.hooks[2].Attach( signature( "ServerFixes::CompleteJump" ), "ServerFixes::CompleteJump",
			OnJumpCompleted, DataType::Void, args );
	}
	case Fix::KeyValues:
		if ( !__polyhook_HookDetour || !__polyhook_GetArgumentPointer || !__polyhook_SetReturnBool ) {
			return "required KeyValues hook imports unavailable";
		}
		return module.hooks[0].Attach( signature( "ServerFixes::ProcessCmdKeyValues" ), "ServerFixes::ProcessCmdKeyValues",
			OnKeyValues, DataType::Bool, { DataType::Pointer, DataType::Pointer } );
	default:
		return "unsupported fix";
	}
}

} // namespace

void Init() noexcept {
	if ( !__s2sdk_CreateConVarInt32 || !__s2sdk_GetConVarInt32 ) {
		return;
	}
	try {
		const auto limit = []( const char* name, int value, const char* description, int max = 0 ) {
			return s2sdk::CreateConVarInt32( name, value, description, s2sdk::ConVarFlag::SpOnly, true, 1, max != 0, max );
		};
		g_keyValuesConVars = {
			limit( "sf_keyvalues_per_second", 10, "KeyValues commands per connection per second." ),
			limit( "sf_keyvalues_burst", 20, "Maximum burst of KeyValues commands per connection." ),
			limit( "sf_keyvalues_max_bytes", kKeyValuesNativeMaxBytes, "Maximum KeyValues command size in bytes.", kKeyValuesNativeMaxBytes ),
		};
	} catch ( ... ) {
		g_keyValuesConVars = {};
	}
}

Status Update( Fix fix, bool enabled, bool mapReady ) noexcept {
	auto& module = State( fix );
	const bool usesCategorize = fix == Fix::WaterJump || fix == Fix::RampBug;
	if ( fix == Fix::RampBug && ( !enabled || !mapReady ) ) {
		ramp::SetReady( false );
		if ( enabled ) {
			return { false, "waiting for map start before querying engine schemas" };
		}
	}
	if ( fix == Fix::CstvBotQuota ) {
		g_cstvMapReady = enabled && mapReady;
		if ( !g_cstvMapReady ) {
			g_botSelectionDepth = 0;
		}
		if ( enabled && !mapReady ) {
			return { false, "waiting for map start before querying engine schemas" };
		}
	}
	if ( fix == Fix::JumpSpam ) {
		g_jumpMapReady = enabled && mapReady;
		if ( enabled && !mapReady ) {
			return { false, "waiting for map start before querying engine schemas" };
		}
	}
	if ( !enabled ) {
		if ( fix == Fix::WaterJump ) {
			g_afterWaterMove = {};
		}
		if ( fix == Fix::KeyValues && module.attempted ) {
			ConfigureKeyValues( {} );
		}
		bool clean = module.Release();
		if ( usesCategorize ) {
			clean = ReleaseCategorizeIfUnused() && clean;
		}
		module.attempted = !clean;
		module.error = clean ? nullptr : "native fix cleanup pending";
		if ( clean ) {
			module.patchError.clear();
		}
		return { false, clean ? "disabled by configuration" : module.Reason( module.error ) };
	}
	if ( fix == Fix::KeyValues &&
		std::ranges::find( g_keyValuesConVars, 0 ) != g_keyValuesConVars.end() ) {
		return { false, "could not create KeyValues limit ConVars" };
	}
	if ( !module.attempted ) {
		module.attempted = true;
		try {
			module.error = Attach( fix, module );
		} catch ( const std::exception& failure ) {
			if ( fix == Fix::HammerId ) {
				module.RememberPatchError( failure );
			}
			module.error = "native fix initialization threw";
		} catch ( ... ) {
			module.error = "native fix initialization threw";
		}
		module.armed = !module.error;
		if ( !module.armed ) {
			bool clean = module.Release();
			if ( usesCategorize ) {
				clean = ReleaseCategorizeIfUnused() && clean;
			}
			if ( !clean ) {
				module.error = "initialization failed; native fix cleanup pending";
			}
		}
	}
	if ( fix == Fix::RampBug && module.armed ) {
		try {
			const float standable = s2sdk::GetConVarFloat( g_rampStandable );
			if ( standable >= 0.0f && standable <= 1.0f ) {
				ramp::SetReady( true, standable );
			} else {
				ramp::SetReady( false );
				return { false, "sv_standable_normal is outside [0,1]" };
			}
		} catch ( ... ) {
			ramp::SetReady( false );
			return { false, "could not read sv_standable_normal" };
		}
	}
	if ( fix == Fix::KeyValues && module.armed ) {
		try {
			const KeyValuesLimits limits{ s2sdk::GetConVarInt32( g_keyValuesConVars[0] ),
				s2sdk::GetConVarInt32( g_keyValuesConVars[1] ), s2sdk::GetConVarInt32( g_keyValuesConVars[2] ) };
			if ( limits.rate < 1 || limits.burst < 1 || limits.bytes < 1 || limits.bytes > kKeyValuesNativeMaxBytes ) {
				ConfigureKeyValues( {} );
				return { false, "KeyValues limits must be positive and the byte cap cannot exceed 1500" };
			}
			ConfigureKeyValues( limits );
		} catch ( ... ) {
			ConfigureKeyValues( {} );
			return { false, "could not read KeyValues limit ConVars" };
		}
	}
	return { module.armed, module.armed ? "active" : module.Reason( module.error ) };
}

void OnMapEnd() noexcept {
	g_cstvMapReady = false;
	g_botSelectionDepth = 0;
	g_afterWaterMove = {};
	ramp::ResetMap();
	g_jumpMapReady = false;
	std::fill( g_lastJumps.begin(), g_lastJumps.end(), JumpStamp{} );
}

bool Shutdown() noexcept {
	OnMapEnd();
	ConfigureKeyValues( {} );
	bool clean = true;
	for ( auto& module : g_modules ) {
		const bool released = module.Release();
		module.attempted = !released;
		module.error = released ? nullptr : "native fix cleanup pending";
		clean = released && clean;
	}
	clean = ReleaseCategorizeIfUnused() && clean;
	if ( clean && g_config ) {
		try {
			s2sdk::CloseGameConfigFile( g_config );
			g_config = {};
		} catch ( ... ) {
			return false;
		}
	}
	return clean;
}

} // namespace imomoe::server_fixes::native_faults
