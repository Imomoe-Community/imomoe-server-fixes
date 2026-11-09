// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "native_faults.hpp"
#include "server_code.hpp"
#include "server_fixes_gamedata.hpp"

#include <polyhook/polyhook.hpp>
#include <entity2/entityinstance.h>
#include <entity2/entitynetwork.h>

#include <array>
#include <cstring>
#include <string_view>

namespace imomoe::server_fixes::native_faults {
namespace {

using polyhook::CallbackType;
using polyhook::DataType;
using polyhook::ResultType;

struct Hook {
	void* address{};
	polyhook::HookHandle owned{};
	polyhook::CallbackHandler callback{};

	const char* Attach( void* target, const char* name,
		polyhook::CallbackHandler handler, DataType result, const plg::vector<DataType>& args,
		bool mid = false ) {
		if ( !target ) {
			return "native signature unavailable or live code changed";
		}
		if ( polyhook::FindDetour( target ) ) {
			return "native entry already detoured; hook ABI/ownership cannot be shared";
		}
		address = target;
		callback = handler;
		owned = mid ? polyhook::HookDetour2( address, plg::string( name ) ) :
			polyhook::HookDetour( address, result, args, -1, plg::string( name ) );
		if ( !owned ) {
			return "could not install native hook";
		}
		(void)polyhook::AddCallback( owned, CallbackType::Pre, callback );
		return polyhook::IsCallbackRegistered( owned, CallbackType::Pre, callback ) ? nullptr : "could not attach native callback";
	}

	bool Release() noexcept {
		if ( !owned ) {
			return true;
		}
		try {
			const auto current = polyhook::FindDetour( address );
			if ( current && polyhook::IsCallbackRegistered( current, CallbackType::Pre, callback ) ) {
				(void)polyhook::RemoveCallback( current, CallbackType::Pre, callback );
				if ( polyhook::IsCallbackRegistered( current, CallbackType::Pre, callback ) ) {
					return false;
				}
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
	std::array<Hook, 2> hooks;
	bool armed{};
	bool attempted{};
	const char* error{};

	bool Release() noexcept {
		armed = false;
		const bool second = hooks[1].Release();
		const bool first = hooks[0].Release();
		return first && second;
	}
};

std::array<Module, static_cast<std::size_t>( Fix::Count )> g_modules;
s2sdk::ConfigId g_config{};
void* ( *g_getParent )( const void* ){};
void ( *g_setLocalOrigin )( void*, const void* ){};
std::uint32_t g_beamEndOffset{};
char g_emptyEquipName[] = "";

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

const char* Attach( Fix fix, Module& module ) {
	if ( fix == Fix::FreeClient ) {
		return "no verified reusable client at native rejection; no hook installed";
	}
	if ( !__polyhook_FindDetour || !__polyhook_AddCallback || !__polyhook_RemoveCallback ||
		!__polyhook_IsCallbackRegistered || !__polyhook_AreCallbacksRegistered || !__polyhook_UnhookDetour ) {
		return "required PolyHook imports unavailable";
	}
	const auto image = LocateServerImage();
	if ( !image.ok ) {
		return "unsupported or unloaded server image";
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
	default:
		return "unsupported fix";
	}
}

} // namespace

Status Update( Fix fix, bool enabled ) noexcept {
	auto& module = State( fix );
	if ( !enabled ) {
		const bool clean = module.Release();
		module.attempted = !clean;
		module.error = clean ? nullptr : "hook cleanup pending";
		return { false, clean ? "disabled by configuration" : module.error };
	}
	if ( !module.attempted ) {
		module.attempted = true;
		try {
			module.error = Attach( fix, module );
		} catch ( ... ) {
			module.error = "native hook initialization threw";
		}
		module.armed = !module.error;
		if ( !module.armed && !module.Release() ) {
			module.error = "initialization failed; hook cleanup pending";
		}
	}
	return { module.armed, module.armed ? "active" : module.error };
}

bool Shutdown() noexcept {
	bool clean = true;
	for ( auto& module : g_modules ) {
		const bool released = module.Release();
		module.attempted = !released;
		module.error = released ? nullptr : "hook cleanup pending";
		clean = released && clean;
	}
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
