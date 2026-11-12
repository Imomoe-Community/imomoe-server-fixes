// SPDX-License-Identifier: AGPL-3.0-or-later
#include "compat_guards.hpp"
#include "compat_guard_policy.hpp"
#include "chat_policy.hpp"
#include "spawn_capacity.hpp"
#include "runtime_lifecycle.hpp"

#include <s2sdk/s2sdk.hpp>
#include <netmessages.pb.h>

#include <cstdint>
#include <random>
#include <string_view>
#include <unordered_map>

namespace imomoe::server_fixes::compat_guards {
namespace {

using namespace std::chrono_literals;
using s2sdk::ResultType;

std::uint64_t g_voiceFix{}, g_voiceRate{}, g_voiceBytes{}, g_voicePackets{}, g_teamLimit{}, g_noclip{}, g_cheats{};
std::uint64_t g_workshopVoice{}, g_playerGhost{};
int g_voiceRateLimit{}, g_voiceByteLimit{}, g_voicePacketLimit{}, g_teamSwitchLimit{};
bool g_voiceHooked{}, g_teamHooked{}, g_disconnectHooked{}, g_noclipReady{}, g_noclipEnabled{};
bool g_voiceFixEnabled{}, g_voiceRateRequested{};
bool g_cheatsHooked{}, g_noclipPending{};
bool g_workshopVoiceEnabled{}, g_workshopVoiceHooked{}, g_voiceRegistryProbed{};
bool g_ghostEnabled{}, g_ghostHooked{}, g_ghostPending{};
std::int16_t g_voiceMessageId{};
std::uint32_t g_voiceMapSeed{};
std::unordered_map<int, Allowance> g_voiceBudget;
std::unordered_map<int, Allowance> g_teamBudget;

void OnDisconnect( int slot, s2sdk::NetworkDisconnectionReason ) noexcept {
	g_voiceBudget.erase( slot );
	g_teamBudget.erase( slot );
}

ResultType OnVoice( int slot, void* voice ) noexcept {
	// The paired S2SDK supplies the generated protobuf pointer, not a raw
	// packet buffer. Inspect field presence without decoding or copying audio.
	if ( g_voiceFixEnabled && !static_cast<const CCLCMsg_VoiceData*>( voice )->has_audio() ) {
		return ResultType::Handled;
	}
	try {
		if ( g_voiceRateLimit == 0 ) {
			return ResultType::Continue;
		}
		if ( !g_voiceBudget[slot].Take( Allowance::Clock::now(), 1s, g_voiceRateLimit ) ) {
			return ResultType::Handled;
		}
		const auto packets = static_cast<std::size_t>( g_voicePacketLimit );
		if ( s2sdk::VoiceDataGetNumPackets( voice ) > packets ) {
			return ResultType::Handled;
		}
		const auto bytes = s2sdk::VoiceDataGetAudio( voice ).size();
		const auto maxBytes = static_cast<std::size_t>( g_voiceByteLimit );
		if ( bytes == 0 || bytes > maxBytes ) {
			return ResultType::Handled;
		}
		const auto offsets = s2sdk::VoiceDataGetPacketOffsets( voice ).size();
		return HasValidVoiceShape( bytes, offsets, maxBytes, packets ) ? ResultType::Continue : ResultType::Handled;
	} catch ( ... ) {
		return ResultType::Continue;
	}
}

ResultType OnCommand( int slot, s2sdk::ConCommandContext context, const plg::vector<plg::string>& args ) noexcept {
	try {
		if ( g_teamSwitchLimit == 0 || slot < 0 || context != s2sdk::ConCommandContext::Console || args.empty() ) {
			return ResultType::Continue;
		}
		const std::string_view name( args.front().data(), args.front().size() );
		// When spawn capacity observes jointeam, it performs admission before recording the attempt.
		if ( !chat::EqualsAsciiCaseInsensitive( name, "spectate" ) &&
			( !chat::EqualsAsciiCaseInsensitive( name, "jointeam" ) || spawn_capacity::ObservesJoinCommands() ) ) {
			return ResultType::Continue;
		}
		return AllowTeamSwitch( slot ) ? ResultType::Continue : ResultType::Stop;
	} catch ( ... ) {
		return ResultType::Continue;
	}
}

int PlayerPawnHandle( int controller ) {
	auto* entity = s2sdk::EntHandleToEntPointer( controller );
	return entity ? s2sdk::GetEntSchemaEnt2( entity, plg::string( "CCSPlayerController" ),
		plg::string( "m_hPlayerPawn" ), 0 ) : -1;
}

void ResetNoclip() {
	static const plg::string controllerClass( "CBasePlayerController" ), noclipEnabled( "m_bNoClipEnabled" );
	static const plg::string entityClass( "CBaseEntity" ), actualMoveType( "m_nActualMoveType" );
	constexpr auto noclip = static_cast<std::int64_t>( s2sdk::MoveType::Noclip );
	constexpr auto walk = static_cast<std::int64_t>( s2sdk::MoveType::Walk );
	const int max = s2sdk::GetMaxClients();
	for ( int slot = 0; slot < max; ++slot ) {
		if ( !s2sdk::IsClientInGame( slot ) ) {
			continue;
		}
		const int controller = s2sdk::PlayerSlotToEntHandle( slot );
		auto* player = s2sdk::EntHandleToEntPointer( controller );
		if ( !player ) {
			continue;
		}
		// The engine applies the controller's noclip flag again when assigning a new pawn.
		if ( s2sdk::GetEntSchema2( player, controllerClass, noclipEnabled, 0 ) != 0 ) {
			s2sdk::SetEntSchema2( player, controllerClass, noclipEnabled, 0, true, 0 );
			if ( s2sdk::PlayerSlotToEntHandle( slot ) != controller ) {
				continue;
			}
		}
		if ( !s2sdk::IsClientAlive( slot ) ) {
			continue;
		}
		const int handle = PlayerPawnHandle( controller );
		auto* pawn = s2sdk::EntHandleToEntPointer( handle );
		if ( !pawn ) {
			continue;
		}
		if ( s2sdk::GetEntityMoveType( handle ) != s2sdk::MoveType::Noclip &&
			s2sdk::GetEntSchema2( pawn, entityClass, actualMoveType, 0 ) != noclip ) {
			continue;
		}
		s2sdk::SetEntityMoveType( handle, s2sdk::MoveType::Walk );
		auto* current = s2sdk::EntHandleToEntPointer( handle );
		if ( current && s2sdk::PlayerSlotToEntHandle( slot ) == controller && PlayerPawnHandle( controller ) == handle &&
			s2sdk::GetEntSchema2( current, entityClass, actualMoveType, 0 ) == noclip ) {
			s2sdk::SetEntSchema2( current, entityClass, actualMoveType, walk, false, 0 );
		}
	}
}

void CheatsChanged( std::uint64_t, const plg::string& next, const plg::string& previous ) noexcept {
	if ( g_noclipReady ) {
		const auto enabled = []( const plg::string& value ) {
			const std::string_view text( value.data(), value.size() );
			return text == "true" || text == "1";
		};
		g_noclipPending = enabled( previous ) && !enabled( next );
	}
}

bool ClearNoclip() {
	g_noclipReady = false;
	g_noclipPending = false;
	if ( g_cheatsHooked && __s2sdk_UnhookConVarChange && s2sdk::UnhookConVarChange( g_cheats, CheatsChanged ) ) {
		g_cheatsHooked = false;
	}
	if ( !g_cheatsHooked ) {
		g_cheats = 0;
	}
	return !g_cheatsHooked;
}

ResultType OnWorkshopVoice( int slot, void* message ) noexcept {
	try {
		if ( !g_voiceMessageId || s2sdk::UserMessageGetMessageID( message ) != g_voiceMessageId ) {
			return ResultType::Continue;
		}
		static const plg::string entity( "entity" ), xuid( "xuid" );
		std::int32_t speaker{};
		if ( s2sdk::PbGetInt32( message, entity, speaker ) && speaker > 0 && speaker <= UINT16_MAX ) {
			const auto stream = VoiceStreamId( g_voiceMapSeed, static_cast<std::uint16_t>( slot ),
				static_cast<std::uint16_t>( speaker ) );
			(void)s2sdk::PbSetUInt64( message, xuid, stream );
		}
	} catch ( ... ) {
	}
	return ResultType::Continue;
}

bool ClearWorkshopVoice() {
	g_voiceMessageId = 0;
	g_voiceRegistryProbed = false;
	if ( g_workshopVoiceHooked && __s2sdk_UnhookSendNetMessage &&
		s2sdk::UnhookSendNetMessage( OnWorkshopVoice ) ) {
		g_workshopVoiceHooked = false;
	}
	return !g_workshopVoiceHooked;
}

ResultType OnFreezeEnd( const plg::string&, void*, bool ) noexcept {
	g_ghostPending = g_ghostEnabled && runtime_lifecycle::SchemaQueriesReady();
	return ResultType::Continue;
}

bool ClearGhost() {
	g_ghostPending = false;
	if ( g_ghostHooked && __s2sdk_UnhookEvent ) {
		const auto result = s2sdk::UnhookEvent( plg::string( "round_freeze_end" ), OnFreezeEnd, s2sdk::HookMode::Post );
		g_ghostHooked = result != s2sdk::EventHookError::Okay && result != s2sdk::EventHookError::NotActive &&
			result != s2sdk::EventHookError::InvalidCallback;
	}
	return !g_ghostHooked;
}

void ResetGhosts() {
	const auto generation = runtime_lifecycle::MapGeneration();
	const int max = s2sdk::GetMaxClients();
	for ( int slot = 0; slot < max; ++slot ) {
		if ( !s2sdk::IsClientInGame( slot ) || s2sdk::GetClientTeam( slot ) != s2sdk::CSTeam::Spectator ||
			!s2sdk::IsClientAlive( slot ) ) {
			continue;
		}
		const auto controller = s2sdk::PlayerSlotToEntHandle( slot );
		const auto pawn = PlayerPawnHandle( controller );
		if ( pawn == -1 || s2sdk::GetEntityTeam( pawn ) != s2sdk::CSTeam::Spectator ) {
			continue;
		}
		s2sdk::ChangeClientTeam( slot, s2sdk::CSTeam::None );
		if ( runtime_lifecycle::MapGeneration() != generation ) {
			return;
		}
		if ( s2sdk::PlayerSlotToEntHandle( slot ) == controller && s2sdk::IsClientConnected( slot ) ) {
			s2sdk::ChangeClientTeam( slot, s2sdk::CSTeam::Spectator );
		}
	}
}

void UpdateMapRepairs( bool mapReady ) {
	g_workshopVoiceEnabled = g_workshopVoice && __s2sdk_GetConVarBool && s2sdk::GetConVarBool( g_workshopVoice );
	if ( !mapReady || !g_workshopVoiceEnabled || !__s2sdk_GetCurrentWorkshopMap ||
		!__s2sdk_HookSendNetMessage || !__s2sdk_UnhookSendNetMessage ||
		!__s2sdk_UserMessageFindMessageIdByName || !__s2sdk_UserMessageGetMessageID ||
		!__s2sdk_PbGetInt32 || !__s2sdk_PbSetUInt64 ) {
		(void)ClearWorkshopVoice();
	} else if ( !g_voiceRegistryProbed && s2sdk::GetCurrentWorkshopMap() != 0 ) {
		g_voiceRegistryProbed = true;
		g_voiceMessageId = s2sdk::UserMessageFindMessageIdByName( plg::string( "CSVCMsg_VoiceData" ) );
		if ( g_voiceMessageId > 0 ) {
			g_voiceMapSeed = std::random_device{}();
			if ( !g_workshopVoiceHooked ) {
				g_workshopVoiceHooked = s2sdk::HookSendNetMessage( OnWorkshopVoice );
			}
		} else {
			g_voiceMessageId = 0;
		}
	}

	g_ghostEnabled = g_playerGhost && __s2sdk_GetConVarBool && s2sdk::GetConVarBool( g_playerGhost );
	if ( !mapReady || !g_ghostEnabled || !__s2sdk_HookEvent || !__s2sdk_UnhookEvent || !__s2sdk_GetMaxClients ||
		!__s2sdk_GetClientTeam || !__s2sdk_IsClientAlive || !__s2sdk_PlayerSlotToEntHandle ||
		!__s2sdk_EntHandleToEntPointer || !__s2sdk_GetEntSchemaEnt2 || !__s2sdk_GetEntityTeam || !__s2sdk_ChangeClientTeam ||
		!__s2sdk_IsClientConnected ) {
		(void)ClearGhost();
		return;
	}
	if ( !g_ghostHooked ) {
		g_ghostHooked = s2sdk::HookEvent( plg::string( "round_freeze_end" ), OnFreezeEnd,
			s2sdk::HookMode::Post ) == s2sdk::EventHookError::Okay;
	}
	if ( g_ghostPending ) {
		g_ghostPending = false;
		ResetGhosts();
	}
}

std::uint64_t CreateLimit( const char* name, const char* description, int value, int min ) {
	return s2sdk::CreateConVarInt32( plg::string( name ), value, plg::string( description ),
		s2sdk::ConVarFlag::SpOnly, true, min, false, 0 );
}

bool ClearVoice() {
	if ( g_voiceHooked && __s2sdk_UnhookClientVoiceData && s2sdk::UnhookClientVoiceData( OnVoice ) ) {
		g_voiceHooked = false;
	}
	g_voiceBudget.clear();
	return !g_voiceHooked;
}

bool ClearTeam() {
	if ( g_teamHooked && __s2sdk_RemoveCommandListener &&
		s2sdk::RemoveCommandListener( plg::string( "" ), OnCommand, s2sdk::HookMode::Pre ) ) {
		g_teamHooked = false;
	}
	g_teamBudget.clear();
	return !g_teamHooked;
}

} // namespace

bool AllowTeamSwitch( int slot ) noexcept {
	try {
		return !g_teamHooked || !g_teamSwitchLimit || !s2sdk::IsClientInGame( slot ) ||
			g_teamBudget[slot].Take( Allowance::Clock::now(), 10s, g_teamSwitchLimit );
	} catch ( ... ) {
		return true;
	}
}

void Init() noexcept {
	try {
		if ( __s2sdk_CreateConVarInt32 && __s2sdk_GetConVarInt32 ) {
			g_voiceRate = CreateLimit( "sf_voice_max_per_second", "Voice messages per client per second; 0 disables limits.", 0, 0 );
			g_voiceBytes = CreateLimit( "sf_voice_max_bytes", "Maximum audio payload in bytes.", 4096, 1 );
			g_voicePackets = CreateLimit( "sf_voice_max_packets", "Maximum packets per voice message.", 64, 1 );
			g_teamLimit = CreateLimit( "sf_team_switch_limit", "Team/spectator commands per client per ten seconds; 0 disables guard.", 0, 0 );
		}
		g_voiceFix = s2sdk::CreateConVarBool( plg::string( "sf_voice_fix" ), false,
			plg::string( "Ignore client voice messages without an audio field." ), s2sdk::ConVarFlag::SpOnly,
			false, false, false, false );
		g_noclip = s2sdk::CreateConVarBool( plg::string( "sf_noclip_reset" ), false,
			plg::string( "Restore walking when sv_cheats is turned off." ), s2sdk::ConVarFlag::SpOnly,
			false, false, false, false );
		g_workshopVoice = s2sdk::CreateConVarBool( plg::string( "sf_workshop_voice" ), false,
			plg::string( "Separate Workshop voice streams by speaker." ), s2sdk::ConVarFlag::SpOnly,
			false, false, false, false );
		g_playerGhost = s2sdk::CreateConVarBool( plg::string( "sf_player_ghost" ), false,
			plg::string( "Reconcile spectators with a leftover live pawn at freeze end." ), s2sdk::ConVarFlag::SpOnly,
			false, false, false, false );
	} catch ( ... ) {
	}
}

void Update( bool mapReady ) noexcept {
	try {
		g_voiceFixEnabled = g_voiceFix && __s2sdk_GetConVarBool && s2sdk::GetConVarBool( g_voiceFix );
		const auto requestedRate = g_voiceRate ? s2sdk::GetConVarInt32( g_voiceRate ) : 0;
		g_voiceRateRequested = requestedRate > 0;
		g_teamSwitchLimit = g_teamLimit ? s2sdk::GetConVarInt32( g_teamLimit ) : 0;
		const bool rateBindings = g_voiceBytes && g_voicePackets && __s2sdk_VoiceDataGetAudio &&
			__s2sdk_VoiceDataGetNumPackets && __s2sdk_VoiceDataGetPacketOffsets;
		const bool rateWanted = g_voiceRateRequested && rateBindings;
		if ( ( rateWanted || g_teamSwitchLimit ) && !g_disconnectHooked &&
			__s2sdk_OnClientDisconnect_Register && __s2sdk_OnClientDisconnect_Unregister ) {
			s2sdk::OnClientDisconnect_Register( OnDisconnect );
			g_disconnectHooked = true;
		}
		if ( rateWanted && g_disconnectHooked ) {
			g_voiceByteLimit = s2sdk::GetConVarInt32( g_voiceBytes );
			g_voicePacketLimit = s2sdk::GetConVarInt32( g_voicePackets );
			g_voiceRateLimit = requestedRate;
		} else if ( g_voiceRateLimit ) {
			g_voiceRateLimit = 0;
			g_voiceBudget.clear();
		}
		if ( g_voiceFixEnabled || g_voiceRateLimit ) {
			if ( !g_voiceHooked && __s2sdk_HookClientVoiceData && __s2sdk_UnhookClientVoiceData ) {
				g_voiceHooked = s2sdk::HookClientVoiceData( OnVoice );
			}
		} else if ( g_voiceHooked ) {
			(void)ClearVoice();
		}
		if ( g_teamSwitchLimit && g_disconnectHooked ) {
			if ( !g_teamHooked ) {
				g_teamHooked = s2sdk::AddCommandListener( plg::string( "" ), OnCommand, s2sdk::HookMode::Pre );
			}
		} else if ( g_teamHooked ) {
			(void)ClearTeam();
		}
		if ( !( g_voiceHooked && g_voiceRateLimit ) && !g_teamHooked && g_disconnectHooked ) {
			s2sdk::OnClientDisconnect_Unregister( OnDisconnect );
			g_disconnectHooked = false;
		}

		UpdateMapRepairs( mapReady );
		g_noclipEnabled = g_noclip && __s2sdk_GetConVarBool && s2sdk::GetConVarBool( g_noclip );
		if ( !mapReady || !g_noclipEnabled || !__s2sdk_FindConVar || !__s2sdk_GetMaxClients ||
			!__s2sdk_IsClientAlive || !__s2sdk_PlayerSlotToEntHandle || !__s2sdk_EntHandleToEntPointer ||
			!__s2sdk_GetEntSchemaEnt2 || !__s2sdk_GetEntityMoveType ||
			!__s2sdk_SetEntityMoveType || !__s2sdk_GetEntSchema2 || !__s2sdk_SetEntSchema2 ||
			!__s2sdk_HookConVarChange || !__s2sdk_UnhookConVarChange ) {
			(void)ClearNoclip();
			return;
		}
		if ( !g_cheats ) {
			g_cheats = s2sdk::FindConVar( plg::string( "sv_cheats" ) );
		}
		if ( !g_cheats ) {
			return;
		}
		if ( !g_cheatsHooked ) {
			g_cheatsHooked = s2sdk::HookConVarChange( g_cheats, CheatsChanged );
		}
		g_noclipReady = g_cheatsHooked;
		if ( g_noclipPending ) {
			g_noclipPending = false;
			ResetNoclip();
		}
	} catch ( ... ) {
	}
}

void OnMapEnd() noexcept {
	g_voiceBudget.clear();
	g_teamBudget.clear();
	g_noclipReady = false;
	g_noclipPending = false;
	g_ghostPending = false;
	g_voiceMessageId = 0;
	g_voiceRegistryProbed = false;
}

bool Shutdown() noexcept {
	g_voiceRateLimit = 0;
	g_voiceRateRequested = false;
	g_voiceFixEnabled = false;
	g_teamSwitchLimit = 0;
	g_ghostEnabled = false;
	OnMapEnd();
	try {
		const bool voiceClean = ClearVoice();
		const bool teamClean = ClearTeam();
		const bool noclipClean = ClearNoclip();
		const bool workshopVoiceClean = ClearWorkshopVoice();
		const bool ghostClean = ClearGhost();
		if ( g_disconnectHooked && __s2sdk_OnClientDisconnect_Unregister ) {
			s2sdk::OnClientDisconnect_Unregister( OnDisconnect );
			g_disconnectHooked = false;
		}
		return voiceClean && teamClean && noclipClean && workshopVoiceClean && ghostClean && !g_disconnectHooked;
	} catch ( ... ) {
		return false;
	}
}

Status VoiceStatus() noexcept {
	const bool enabled = g_voiceFixEnabled || g_voiceRateRequested;
	const bool ready = g_voiceHooked && enabled && ( !g_voiceRateRequested || g_voiceRateLimit != 0 );
	return { "voice_guard", enabled, ready,
		ready ? "registered" : enabled ? "voice or lifecycle binding unavailable" : "disabled" };
}

Status TeamStatus() noexcept {
	return { "team_switch_guard", g_teamSwitchLimit != 0, g_teamHooked,
		g_teamHooked ? "registered" : g_teamSwitchLimit ? "command listener unavailable" : "disabled" };
}

Status NoclipStatus() noexcept {
	return { "noclip_reset", g_noclipEnabled, g_noclipReady,
		g_noclipReady ? "ready" : g_noclipEnabled ? "waiting for map, schema or ConVar" : "disabled" };
}

Status WorkshopVoiceStatus() noexcept {
	const bool ready = g_workshopVoiceHooked && g_voiceMessageId != 0;
	return { "workshop_voice", g_workshopVoiceEnabled, ready, ready ? "ready" :
		g_workshopVoiceEnabled ? "waiting for Workshop map and voice registry" : "disabled" };
}

Status GhostStatus() noexcept {
	return { "player_ghost", g_ghostEnabled, g_ghostHooked, g_ghostHooked ? "ready" :
		g_ghostEnabled ? "waiting for map or event binding" : "disabled" };
}

} // namespace imomoe::server_fixes::compat_guards
