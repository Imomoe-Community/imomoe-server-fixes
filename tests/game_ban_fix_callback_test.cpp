// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "game_ban_fix.cpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace plg {

LogFn Log = []( std::string_view, Severity, const source_location& ) {};

BeginZoneFn BeginZone = []( std::string_view, const source_location& ) -> zone { return 0; };
EndZoneFn EndZone = []( zone ) {};

namespace plugin {
GetLocationFn GetLocation{};
hndl handle{};
} // namespace plugin

Plugin* GetPlugin() {
	return nullptr; // These tests exercise callbacks, not loader-owned gamedata.
}

} // namespace plg

namespace {

using namespace imomoe::server_fixes::game_ban_fix;

void Require( bool condition, std::string_view message ) {
	if ( !condition ) {
		std::cerr << message << '\n';
		std::exit( 1 );
	}
}

const char* g_value = "0";
std::array<std::byte, static_cast<std::size_t>( kControllerSteamId ) + sizeof( std::uint64_t )> g_controller{};
std::uint64_t g_liveSteamId{};
std::uint8_t g_nativeValid{1};
bool g_authorized{};

plg::string ReadValue( std::uint64_t ) {
	return plg::string( g_value );
}

void* ReadController( polyhook::RegistersHandle, polyhook::RegisterType reg ) {
	Require( reg == kControllerRegister, "wrong controller register" );
	return g_controller.data();
}

std::int32_t ReadSlot( polyhook::RegistersHandle, polyhook::RegisterType reg ) {
	Require( reg == polyhook::RegisterType::RBX, "wrong slot register" );
	return 9;
}

std::uint8_t ReadValidity( polyhook::RegistersHandle, polyhook::RegisterType reg ) {
	Require( reg == polyhook::RegisterType::RAX, "wrong native validity register" );
	return g_nativeValid;
}

void WriteValidity( polyhook::RegistersHandle, polyhook::RegisterType reg, std::uint8_t value ) {
	Require( reg == polyhook::RegisterType::RAX, "wrong native validity register" );
	g_nativeValid = value;
}

void Think( CallbackType type ) {
	Require( OnThink( nullptr, nullptr, 0, nullptr, type ) == ResultType::Ignored, "native pass must run" );
}

bool CanCheck( std::uint64_t steamId, bool authorized, std::uint64_t liveSteamId, bool nativeValid = true ) {
	g_authorized = authorized;
	g_liveSteamId = liveSteamId;
	g_nativeValid = nativeValid;
	std::memcpy( g_controller.data() + kControllerSteamId, &steamId, sizeof( steamId ) );
	Require( OnCheckIdentity( nullptr, g_controller.data(), 0, nullptr, CallbackType::Pre ) == ResultType::Ignored,
		"native instructions must still run" );
	return g_nativeValid != 0;
}

} // namespace

int main() {
	GcBanMap map;
	g_bans = &map;
	g_armed = true;
	g_cooldown = 1;
	__s2sdk_GetConVarValue = ReadValue;
	__polyhook_GetRegisterPointer = ReadController;
	__polyhook_GetRegisterInt32 = ReadSlot;
	__polyhook_GetRegisterUInt8 = ReadValidity;
	__polyhook_SetRegisterUInt8 = WriteValidity;
	__s2sdk_IsClientAuthorized = []( std::int32_t slot ) {
		Require( slot == 9, "authorization must check the exact native slot" );
		return g_authorized;
	};
	__s2sdk_GetClientSteamID64 = []( std::int32_t slot ) {
		Require( slot == 9, "Steam ID must come from the exact native slot" );
		return g_liveSteamId;
	};

	const double activeUntil = Plat_FloatTime() + 300;
	const auto cooldown20 = map.Insert( 20, GcBan{ .reason = 20, .reasonUntil = activeUntil } );
	const auto cooldown22 = map.Insert( 22, GcBan{ .reason = 22, .reasonUntil = activeUntil,
		.separateBanUntil = activeUntil, .communicationCooldown = 1, .cheatingPenaltyLevel = 2 } );
	const auto cooldown23 = map.Insert( 23, GcBan{ .reason = 23, .reasonUntil = activeUntil } );
	const auto ordinary = map.Insert( 100, GcBan{ .reason = 19, .reasonUntil = activeUntil } );
	const auto expired = Plat_FloatTime() - 1;
	map.Insert( 101, GcBan{ .reason = 15, .reasonUntil = expired, .separateBanUntil = expired } );
	const auto flagged = map.Insert( 102, GcBan{ .reason = 10, .reasonUntil = expired, .communicationCooldown = 1 } );

	Think( CallbackType::Pre );
	Think( CallbackType::Post ); // Native returns early on a non-official server.
	Require( map.Element( cooldown20 ).reason == 20, "mode 0 lost a live penalty" );
	Require( !map.HasElement( 101 ) && !map.Element( flagged ).reason && map.Element( flagged ).communicationCooldown,
		"post-hook did not clear only expired enforcement" );

	g_value = "1";
	Think( CallbackType::Pre );
	Require( !map.Element( cooldown20 ).reason && !map.Element( cooldown22 ).reason && !map.Element( cooldown23 ).reason,
		"mode 1 did not mask all three competitive reasons" );
	Require( map.Element( ordinary ).reason == 19 && map.Element( cooldown22 ).separateBanUntil == activeUntil,
		"mode 1 suppressed an independent sanction" );
	constexpr std::uint64_t banned = 0x0110000100000064;
	Require( !CanCheck( banned, false, banned ), "pending slot exposed an enforcement record" );
	Require( !CanCheck( banned, true, banned + 1 ), "reused slot exposed another player's penalty" );
	Require( CanCheck( banned, true, banned ), "native check skipped an authenticated banned player" );
	Require( !CanCheck( banned, true, banned, false ), "native invalid identity was promoted" );
	Require( CanCheck( banned + 50, false, 0 ), "unrelated native identity was changed" );
	Think( CallbackType::Post );
	Require( map.Element( ordinary ).reason == 19, "active ban was lost before reconnect" );
	Require( map.Element( cooldown22 ).reason == 22 && map.Element( cooldown22 ).cheatingPenaltyLevel == 2,
		"post-hook did not restore the filtered reason or preserve penalty flags" );

	g_value = "2";
	Think( CallbackType::Pre );
	Require( map.Element( cooldown20 ).reason == 20 && map.Element( cooldown22 ).reason == 22 &&
		map.Element( cooldown23 ).reason == 23, "mode 2 changed a competitive reason" );
	Think( CallbackType::Post );
	Think( CallbackType::Post );
	Require( map.HasElement( 100 ) && Shutdown(), "live ban or callback state was lost" );

	const auto failed = Update( true ); // Hook/gamedata imports are deliberately absent.
	Require( !failed.available && std::string_view( failed.reason ).find( "imports unavailable" ) != std::string_view::npos &&
		!g_armed && !g_think.owned && !g_identity.owned, "initialization failure did not leave the fix disabled" );
	Require( Shutdown(), "failed initialization left callback state behind" );
	return 0;
}
