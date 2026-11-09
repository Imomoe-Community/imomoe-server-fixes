// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "game_ban_fix.hpp"
#include "game_ban_pass.hpp"
#include "server_code.hpp"
#include "server_fixes_gamedata.hpp"

#include <polyhook/polyhook.hpp>
#include <s2sdk/s2sdk.hpp>
#include <tier0/platform.h>

#include <charconv>
#include <cstring>

namespace imomoe::server_fixes::game_ban_fix {
namespace {

using polyhook::CallbackType;
using polyhook::ResultType;

#if defined(_WIN32)
constexpr auto kControllerRegister = polyhook::RegisterType::RSI;
#elif defined(__linux__)
constexpr auto kControllerRegister = polyhook::RegisterType::R15;
#endif

struct Hook {
	void* address{};
	polyhook::HookHandle owned{};
};

Hook g_think;
Hook g_identity;
GcBanMap* g_bans{};
s2sdk::ConfigId g_config{};
std::ptrdiff_t g_controllerSteamId{};
std::uint64_t g_cooldown{};
BanPass g_pass;
unsigned int g_depth{};
bool g_armed{};
bool g_processPass{};
bool g_attempted{};
const char* g_error{};

void LogFailure( const char* reason ) noexcept {
	if ( __s2sdk_PrintToServer ) {
		try {
			s2sdk::PrintToServer( plg::string( "[ServerFixes] GameBanFix unavailable: " ) + reason );
		} catch ( ... ) {
		}
	}
}

ResultType OnThink( polyhook::HookHandle, polyhook::ParametersHandle, std::int32_t,
	polyhook::ReturnHandle, CallbackType type ) noexcept {
	if ( !g_armed ) {
		return ResultType::Ignored;
	}
	if ( type == CallbackType::Pre ) {
		if ( g_depth++ != 0 || !g_bans->Count() ) {
			return ResultType::Ignored;
		}
		g_processPass = true;
		try {
			const auto value = s2sdk::GetConVarValue( g_cooldown );
			int mode{};
			const auto* end = value.data() + value.size();
			const auto parsed = std::from_chars( value.data(), end, mode );
			if ( parsed.ec == std::errc{} && parsed.ptr == end ) {
				g_pass.FilterCooldowns( *g_bans, mode );
			}
		} catch ( ... ) {
		}
	} else if ( g_depth && --g_depth == 0 && g_processPass ) {
		g_pass.Finish( *g_bans, Plat_FloatTime() );
		g_processPass = false;
	}
	return ResultType::Ignored;
}

ResultType OnCheckIdentity( polyhook::HookHandle, polyhook::ParametersHandle registers, std::int32_t,
	polyhook::ReturnHandle, CallbackType ) noexcept {
	if ( !g_processPass || !registers || !polyhook::GetRegisterUInt8( registers, polyhook::RegisterType::RAX ) ) {
		return ResultType::Ignored;
	}
	try {
		const auto* controller = static_cast<const std::byte*>(
			polyhook::GetRegisterPointer( registers, kControllerRegister ) );
		if ( !controller ) {
			return ResultType::Ignored;
		}
		std::uint64_t steamId{};
		std::memcpy( &steamId, controller + g_controllerSteamId, sizeof( steamId ) );
		if ( !g_bans->HasElement( static_cast<std::uint32_t>( steamId ) ) ) {
			return ResultType::Ignored;
		}
		// The controller can outlive a connection. Compare its ID with the
		// authenticated engine client at the exact slot being visited.
		const auto slot = polyhook::GetRegisterInt32( registers, polyhook::RegisterType::RBX );
		if ( !s2sdk::IsClientAuthorized( slot ) || s2sdk::GetClientSteamID64( slot ) != steamId ) {
			polyhook::SetRegisterUInt8( registers, polyhook::RegisterType::RAX, 0 );
		}
	} catch ( ... ) {
	}
	return ResultType::Ignored;
}

bool Add( polyhook::HookHandle handle, CallbackType type, polyhook::CallbackHandler callback ) {
	(void)polyhook::AddCallback( handle, type, callback );
	return polyhook::IsCallbackRegistered( handle, type, callback );
}

bool Detach( Hook& hook, polyhook::CallbackHandler callback, bool post ) {
	if ( !hook.owned ) {
		return true;
	}
	const auto handle = polyhook::FindDetour( hook.address );
	if ( handle == hook.owned ) {
		const auto remove = [&]( CallbackType type ) {
			if ( polyhook::IsCallbackRegistered( handle, type, callback ) ) {
				(void)polyhook::RemoveCallback( handle, type, callback );
			}
			return !polyhook::IsCallbackRegistered( handle, type, callback );
		};
		const bool preClean = remove( CallbackType::Pre );
		const bool postClean = !post || remove( CallbackType::Post );
		if ( !preClean || !postClean ) {
			return false;
		}
		if ( !polyhook::AreCallbacksRegistered( handle ) ) {
			(void)polyhook::UnhookDetour( hook.address );
			if ( polyhook::FindDetour( hook.address ) == handle ) {
				return false;
			}
		}
	}
	hook = {};
	return true;
}

const char* Attach() {
	if ( !__s2sdk_LoadGameConfigFile || !__s2sdk_CloseGameConfigFile || !__s2sdk_GetGameConfigSignature ||
		!__s2sdk_GetGameConfigOffset ||
		!__s2sdk_FindConVar || !__s2sdk_GetConVarValue || !__polyhook_HookDetour || !__polyhook_HookDetour2 ||
		!__polyhook_FindDetour || !__polyhook_AddCallback || !__polyhook_RemoveCallback ||
		!__polyhook_IsCallbackRegistered || !__polyhook_AreCallbacksRegistered || !__polyhook_UnhookDetour ||
		!__polyhook_GetRegisterPointer || !__polyhook_GetRegisterInt32 ||
		!__polyhook_GetRegisterUInt8 || !__polyhook_SetRegisterUInt8 ||
		!__s2sdk_IsClientAuthorized || !__s2sdk_GetClientSteamID64 ) {
		return "required S2SDK or PolyHook imports unavailable";
	}
	const auto image = LocateServerImage();
	if ( !image.ok ) {
		return "unsupported or unloaded server image (requires CS2 Build 26445667)";
	}
	g_cooldown = s2sdk::FindConVar( plg::string( "sv_kick_players_with_cooldown" ) );
	if ( !g_cooldown ) {
		return "sv_kick_players_with_cooldown not found";
	}
	g_config = LoadServerFixesGameData();
	if ( !g_config ) {
		return "could not load ServerFixes gamedata";
	}
	g_think.address = s2sdk::GetGameConfigSignature( g_config, "ServerFixes::CheckSteamBan" );
	g_identity.address = s2sdk::GetGameConfigSignature( g_config, "ServerFixes::GcBanIdentity" );
	auto* mapRef = static_cast<std::byte*>(
		s2sdk::GetGameConfigSignature( g_config, "ServerFixes::GcBanMapReference" ) );
	g_controllerSteamId = s2sdk::GetGameConfigOffset( g_config, "CBasePlayerController::m_steamID" );
	if ( !g_think.address || !g_identity.address || !mapRef || g_controllerSteamId <= 0 ) {
		return "GC ban map, hook signature, or controller Steam ID offset unavailable in gamedata";
	}
	if ( !ServerCodeUnmodified( image, g_think.address, 23 ) ||
		!ServerCodeUnmodified( image, g_identity.address, 23 ) ||
		!ServerCodeUnmodified( image, mapRef, 7 ) ) {
		return "GC ban hook or map reference instructions have changed";
	}
	// RIP-relative LEA: disp32 starts after three opcode bytes and is relative to the next instruction.
	std::int32_t displacement{};
	std::memcpy( &displacement, mapRef + 3, sizeof( displacement ) );
	auto* bans = reinterpret_cast<GcBanMap*>( mapRef + 7 + displacement );
	// Test the native Steam-ID validity result before the GC lookup. The
	// instruction consumes AL, and EBX still holds the enumerated slot.
	if ( polyhook::FindDetour( g_think.address ) || polyhook::FindDetour( g_identity.address ) ) {
		return "CheckSteamBan already has a detour; disable the other GC ban fix";
	}
	g_think.owned = polyhook::HookDetour( g_think.address, polyhook::DataType::Void, {}, -1,
		plg::string( "ServerFixes::CheckSteamBan" ) );
	if ( !g_think.owned ) {
		return "could not install CheckSteamBan detour";
	}
	g_identity.owned = polyhook::HookDetour2( g_identity.address, plg::string( "ServerFixes::GcBanIdentity" ) );
	if ( !g_identity.owned || !Add( g_identity.owned, CallbackType::Pre, OnCheckIdentity ) ||
		!Add( g_think.owned, CallbackType::Post, OnThink ) || !Add( g_think.owned, CallbackType::Pre, OnThink ) ) {
		return "could not install GC identity guard or CheckSteamBan callbacks";
	}
	g_bans = bans;
	g_armed = true;
	return nullptr;
}

bool Release() noexcept {
	g_armed = false;
	if ( g_processPass ) {
		g_pass.Finish( *g_bans, Plat_FloatTime() );
	}
	g_processPass = false;
	g_depth = 0;
	g_bans = nullptr;
	try {
		const bool thinkClean = Detach( g_think, OnThink, true );
		const bool identityClean = Detach( g_identity, OnCheckIdentity, false );
		if ( g_config ) {
			s2sdk::CloseGameConfigFile( g_config );
			g_config = {};
		}
		return thinkClean && identityClean;
	} catch ( ... ) {
		return false;
	}
}

} // namespace

Status Update( bool enabled ) noexcept {
	if ( !enabled ) {
		const bool clean = Shutdown();
		return { false, clean ? "disabled by configuration" : "hook cleanup pending" };
	}
	if ( !g_attempted ) {
		g_attempted = true;
		try {
			g_error = Attach();
		} catch ( ... ) {
			g_error = "GameBanFix initialization threw";
		}
		if ( g_error ) {
			(void)Release();
			LogFailure( g_error );
		}
	}
	if ( g_armed ) {
		return { true, "active" };
	}
	return { false, ( g_think.owned || g_identity.owned ) ? "initialization failed; hook cleanup pending" : g_error };
}

bool Shutdown() noexcept {
	const bool clean = Release();
	g_attempted = !clean;
	return clean;
}

} // namespace imomoe::server_fixes::game_ban_fix
