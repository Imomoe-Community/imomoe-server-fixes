// SPDX-License-Identifier: AGPL-3.0-or-later
#include "map_maintenance.hpp"
#include "map_maintenance_policy.hpp"
#include "runtime_lifecycle.hpp"
#include "server_code.hpp"
#include "server_fixes_gamedata.hpp"

#include <s2sdk/s2sdk.hpp>
#include <engine/hoststate.h>
#include <iserver.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <string>

namespace imomoe::server_fixes::map_maintenance {
namespace {

constexpr char kNextLevelChanged[] = "nextlevel_changed";
BlankMapRecovery g_recovery;
const std::byte* g_host{};
const std::byte* g_server{};
s2sdk::ConfigId g_transitionConfig{};
int g_pendingRequestOffset{};
int g_queueCountOffset{};
int g_networkRequestOffset{};
const char* g_transitionError{};
const char* g_attachError{};
bool g_attempted{};
bool g_hooked{};
bool g_armed{};

using namespace std::chrono_literals;
using Clock = LongMapClock::Clock;

struct MapTarget {
	std::string name;
	std::uint64_t workshop;
	bool operator==( const MapTarget& ) const = default;
};

struct TimeCarry {
	MapTarget map;
	std::uint64_t generation;
	Clock::time_point expires;
	float remaining;
	float baseline;
	float oldGameTime;
	bool checkClock;
};

struct TimeLease {
	float previous;
	float applied;
};

// Order matches ReadRefreshConfig. Integer seconds/minutes keep the CFG explicit.
constexpr struct {
	const char* name;
	int value;
	int max;
	const char* description;
} kRefreshSettings[]{
	{ "sf_animation_idle_reload_seconds", 1800, 86400, "Idle map reload interval in seconds; 0 disables idle reloads." },
	{ "sf_animation_idle_grace_seconds", 300, 86400, "Minimum time without human clients before an idle reload." },
	{ "sf_animation_max_map_minutes", 180, 1440, "Engine clock age that arms an occupied-map refresh in minutes; 0 disables it." },
	{ "sf_animation_match_wait_minutes", 120, 1440, "Wait for a new match before warning about a round-end refresh." },
	{ "sf_animation_warn_minutes", 2, 1440, "Warning time before an occupied map can refresh at round end." },
	{ "sf_animation_round_wait_minutes", 10, 1440, "Maximum wait after the warning if the current mode does not end a round." },
};
std::array<std::uint64_t, std::size( kRefreshSettings )> g_refreshVars{};
LongMapClock g_clock;
std::optional<TimeCarry> g_carry;
std::optional<TimeLease> g_timeLease;
std::uint64_t g_timelimit{};
std::uint64_t g_matchRestartDelay{};
Clock::time_point g_nextPoll;
struct {
	int warmup;
	int phase;
	int matchEndedPhase;
	int start;
	int rounds;
	int roundStart;
} g_rules{};
bool g_mapSampled{};
const char* g_slowMapError{};

s2sdk::ResultType OnNextLevelChanged( const plg::string&, void* event, bool ) noexcept {
	if ( g_armed && event && runtime_lifecycle::SchemaQueriesReady() ) {
		try {
			g_recovery.OnNextLevel( !s2sdk::GetEventString( event, "nextlevel" ).empty(),
				BlankMapRecovery::Clock::now() );
		} catch ( ... ) {
		}
	}
	return s2sdk::ResultType::Continue;
}

template <class T>
T ReadField( const std::byte* object, int offset ) noexcept;

template <class T>
std::optional<T> ReadOperand( const ServerImage& image, const void* anchor, int offset ) {
	std::array<std::byte, 64> code{};
	if ( offset < 0 || static_cast<std::size_t>( offset ) > code.size() - sizeof( T ) ) {
		return std::nullopt;
	}
	const auto window = std::span( code ).first( static_cast<std::size_t>( offset ) + sizeof( T ) );
	if ( !ReadServerCode( image, anchor, window ) || std::memcmp( anchor, window.data(), window.size() ) != 0 ) {
		return std::nullopt;
	}
	return ReadField<T>( code.data(), offset );
}

std::optional<int> HostFieldOffset( const ServerImage& image, const std::byte* host, bool pending ) {
	const auto* anchor = s2sdk::GetGameConfigSignature( g_transitionConfig,
		pending ? "ServerFixes::HostPendingRequest" : "ServerFixes::HostQueuedRequestCount" );
	const auto offset = [&]( const char* key ) { return s2sdk::GetGameConfigOffset( g_transitionConfig, key ); };
	const int operand = offset( pending ? "ServerFixes::HostPendingRequestOperand" : "ServerFixes::HostQueuedRequestCountOperand" );
#ifdef _WIN32
	const auto disp = ReadOperand<std::int32_t>( image, anchor, operand );
	const int end = offset( pending ? "ServerFixes::HostPendingRequestRipEnd" : "ServerFixes::HostQueuedRequestCountRipEnd" );
	if ( !disp || end < operand + static_cast<int>( sizeof( std::int32_t ) ) ) return std::nullopt;
	const auto address = reinterpret_cast<std::intptr_t>( anchor ) + end + *disp;
	const auto delta = address - reinterpret_cast<std::intptr_t>( host );
	if ( delta <= 0 || delta > std::numeric_limits<int>::max() ) return std::nullopt;
	return static_cast<int>( delta );
#else
	(void)host;
	if ( !pending ) return ReadOperand<std::int32_t>( image, anchor, operand );
	const auto base = ReadOperand<std::int8_t>( image, anchor, offset( "ServerFixes::HostPendingRequestBaseOperand" ) );
	const auto disp = ReadOperand<std::int8_t>( image, anchor, operand );
	return base && disp ? std::optional<int>( *base + *disp ) : std::nullopt;
#endif
}

const char* ResolveTransitions() {
	if ( !__s2sdk_QueryInterface || !__s2sdk_GetCurrentMap || !__s2sdk_GetCurrentWorkshopMap ||
		!__s2sdk_IsMapValid || !__s2sdk_ServerCommandEx || !__s2sdk_LoadGameConfigFile ||
		!__s2sdk_CloseGameConfigFile || !__s2sdk_GetGameConfigSignature || !__s2sdk_GetGameConfigOffset ) {
		return "required map transition imports unavailable";
	}
	if ( !g_transitionConfig ) {
		g_transitionConfig = LoadServerFixesGameData();
	}
	if ( !g_transitionConfig ) {
		return "could not load ServerFixes gamedata";
	}
	const auto* host = static_cast<const std::byte*>( s2sdk::QueryInterface( "engine2", HOSTSTATEMGR_INTERFACE_VERSION ) );
	const auto* server = static_cast<const std::byte*>( s2sdk::QueryInterface( "engine2", NETWORKSERVERSERVICE_INTERFACE_VERSION ) );
	if ( !host || !server ) {
		return "engine transition interfaces unavailable";
	}
	const auto image = LocateEngineImage();
	const auto pendingOffset = HostFieldOffset( image, host, true );
	const auto queuedOffset = HostFieldOffset( image, host, false );
	const auto fits = [&]( const std::byte* object, std::optional<int> offset, std::size_t width ) {
		return offset && *offset > 0 && ContainsReadableRange( image,
			reinterpret_cast<std::uintptr_t>( object ) + static_cast<std::uintptr_t>( *offset ), width );
	};
	if ( !fits( host, pendingOffset, sizeof( void* ) ) || !fits( host, queuedOffset, sizeof( int ) ) ) {
		return "host transition layout did not match gamedata";
	}
	// The getter is a null check of the queued change-level request. Read that
	// field directly; S2SDK can replace the service's vtable with a hooked copy.
	const auto networkOffset = ReadOperand<std::int32_t>( image,
		s2sdk::GetGameConfigSignature( g_transitionConfig, "ServerFixes::IsChangeLevelPending" ),
		s2sdk::GetGameConfigOffset( g_transitionConfig, "ServerFixes::NetworkRequestOperand" ) );
	if ( !fits( server, networkOffset, sizeof( void* ) ) ) {
		return "network transition layout did not match gamedata";
	}
	g_pendingRequestOffset = *pendingOffset;
	g_queueCountOffset = *queuedOffset;
	g_networkRequestOffset = *networkOffset;
	g_host = host;
	g_server = server;
	return nullptr;
}

const char* EnsureTransitions() {
	if ( !g_host && !g_transitionError ) {
		g_transitionError = ResolveTransitions();
	}
	return g_transitionError;
}

bool TransitionPending() {
	return ReadField<const void*>( g_host, g_pendingRequestOffset ) ||
		ReadField<int>( g_host, g_queueCountOffset ) != 0 ||
		ReadField<const void*>( g_server, g_networkRequestOffset );
}

const char* Attach() {
	if ( !__s2sdk_HookEvent || !__s2sdk_UnhookEvent || !__s2sdk_GetEventString ) {
		return "required nextlevel_changed imports unavailable";
	}
	if ( const auto* error = EnsureTransitions() ) {
		return error;
	}
	g_hooked = s2sdk::HookEvent( kNextLevelChanged, OnNextLevelChanged, s2sdk::HookMode::Post ) == s2sdk::EventHookError::Okay;
	return g_hooked ? nullptr : "could not observe nextlevel_changed";
}

MapTarget CurrentMap() {
	const auto name = s2sdk::GetCurrentMap();
	return { std::string( name.data(), name.size() ), s2sdk::GetCurrentWorkshopMap() };
}

bool CanReload( const MapTarget& map ) {
	return map.workshop || ( IsStockMapArgument( map.name ) && s2sdk::IsMapValid( map.name.c_str() ) );
}

void ReloadMap( const MapTarget& map ) {
	if ( map.workshop ) {
		const auto command = "host_workshop_map " + std::to_string( map.workshop );
		(void)s2sdk::ServerCommandEx( command.c_str() );
	} else {
		const auto command = "changelevel " + map.name;
		(void)s2sdk::ServerCommandEx( command.c_str() );
	}
}

std::optional<RefreshConfig> ReadRefreshConfig() {
	std::array<int, std::size( kRefreshSettings )> values{};
	for ( std::size_t i = 0; i < values.size(); ++i ) {
		if ( !g_refreshVars[i] ) {
			return std::nullopt;
		}
		const int value = s2sdk::GetConVarInt32( g_refreshVars[i] );
		if ( value < 0 || value > kRefreshSettings[i].max ) {
			return std::nullopt;
		}
		values[i] = value;
	}
	using std::chrono::minutes;
	using std::chrono::seconds;
	const auto nativeDelay = seconds( std::clamp( s2sdk::GetConVarInt32( g_matchRestartDelay ), 1, 9999 ) ) + 1s;
	return RefreshConfig{ seconds( values[0] ), seconds( values[1] ), minutes( values[2] ),
		minutes( values[3] ), minutes( values[4] ), minutes( values[5] ), nativeDelay };
}

struct Presence {
	bool human{};
	bool joining{};
};

Presence ObserveHumans() {
	Presence result;
	const int max = s2sdk::GetMaxClients();
	for ( int slot = 0; slot < max; ++slot ) {
		if ( s2sdk::IsClientConnected( slot ) && !s2sdk::IsFakeClient( slot ) && !s2sdk::IsClientSourceTV( slot ) ) {
			result.human = true;
			if ( !s2sdk::IsClientInGame( slot ) ) {
				result.joining = true;
				break;
			}
		}
	}
	return result;
}

template <class T>
T ReadField( const std::byte* object, int offset ) noexcept {
	T value;
	std::memcpy( &value, object + offset, sizeof value );
	return value;
}

std::optional<MatchState> ObserveMatch() {
	const auto* rules = static_cast<const std::byte*>( s2sdk::GetGameRules() );
	if ( !rules ) {
		return std::nullopt;
	}
	return MatchState{ ReadField<bool>( rules, g_rules.warmup ), ReadField<int>( rules, g_rules.phase ) == g_rules.matchEndedPhase,
		ReadField<float>( rules, g_rules.start ), ReadField<int>( rules, g_rules.rounds ),
		ReadField<float>( rules, g_rules.roundStart ) };
}

bool RestoreTimeLimit() noexcept {
	if ( !g_timeLease ) {
		return true;
	}
	try {
		if ( s2sdk::GetConVarFloat( g_timelimit ) == g_timeLease->applied ) {
			s2sdk::SetConVarFloat( g_timelimit, g_timeLease->previous, false, false );
			if ( s2sdk::GetConVarFloat( g_timelimit ) == g_timeLease->applied ) {
				return false;
			}
		}
		g_timeLease.reset();
		return true;
	} catch ( ... ) {
		return false;
	}
}

void FinishReload( Clock::time_point now, float gameTime ) {
	if ( !g_carry ) {
		return;
	}
	if ( g_carry->generation == runtime_lifecycle::MapGeneration() ) {
		if ( now > g_carry->expires ) {
			g_carry.reset();
			g_slowMapError = "requested map reload did not start";
		}
		return;
	}
	const auto carry = std::move( *g_carry );
	g_carry.reset();
	if ( now > carry.expires || runtime_lifecycle::MapGeneration() != carry.generation + 1 || CurrentMap() != carry.map ) {
		return;
	}
	if ( carry.checkClock && gameTime >= carry.oldGameTime ) {
		g_slowMapError = "reloading did not reset the game clock";
		return;
	}
	const float current = s2sdk::GetConVarFloat( g_timelimit );
	if ( current == carry.baseline && carry.remaining > 0.0f && carry.remaining < current ) {
		g_timeLease = TimeLease{ current, carry.remaining };
		s2sdk::SetConVarFloat( g_timelimit, carry.remaining, false, false );
		if ( s2sdk::GetConVarFloat( g_timelimit ) != carry.remaining ) {
			g_timeLease.reset();
		}
	}
}

void RequestRefresh( RefreshStep action, Clock::time_point now, float gameTime, const MatchState& match ) {
	auto map = CurrentMap();
	if ( !CanReload( map ) ) {
		g_slowMapError = "current map could not be reloaded";
		return;
	}
	const float limit = s2sdk::GetConVarFloat( g_timelimit );
	const float baseline = g_timeLease && limit == g_timeLease->applied ? g_timeLease->previous : limit;
	// Native timeleft is unlimited in warmup; otherwise it uses
	// mp_timelimit and m_flGameStartTime, not m_fMatchStartTime.
	const float remaining = action == RefreshStep::ReloadNewMatch || match.warmup ? 0.0f :
		RemainingTimeLimit( limit, gameTime, match.start );
	g_carry = TimeCarry{ map, runtime_lifecycle::MapGeneration(), now + 5min,
		remaining, baseline, gameTime, action != RefreshStep::ReloadIdle };
	s2sdk::PrintToServer( "[ServerFixes] Reloading the current map to refresh its animation clock." );
	ReloadMap( map );
}

bool StopBlankMap() noexcept;

} // namespace

void Init() noexcept {
	if ( !__s2sdk_CreateConVarInt32 ) {
		return;
	}
	try {
		for ( std::size_t i = 0; i < g_refreshVars.size(); ++i ) {
			const auto& option = kRefreshSettings[i];
			g_refreshVars[i] = s2sdk::CreateConVarInt32( option.name, option.value, option.description,
				s2sdk::ConVarFlag::SpOnly, true, 0, true, option.max );
		}
	} catch ( ... ) {
		g_refreshVars = {};
	}
}

native_faults::Status UpdateBlankMap( bool enabled, bool mapReady, bool runRecovery ) noexcept {
	if ( !enabled ) {
		const bool clean = StopBlankMap();
		return { false, clean ? "disabled by configuration" : "nextlevel_changed cleanup pending" };
	}
	if ( !mapReady ) {
		return { false, "waiting for a running map" };
	}
	try {
		if ( !g_attempted ) {
			g_attempted = true;
			g_attachError = Attach();
			g_armed = g_hooked;
		}
		if ( !g_armed ) {
			return { false, g_attachError };
		}
		if ( runRecovery && g_recovery.Pending() ) {
			if ( g_recovery.Take( BlankMapRecovery::Clock::now(), TransitionPending() ) ) {
				// Dispatch now, not after an administrator command already queued
				// in the engine buffer. Never dispatch from sf_status or the event.
				const auto map = CurrentMap();
				if ( CanReload( map ) ) {
					ReloadMap( map );
				}
			}
		}
		return { true, "active" };
	} catch ( ... ) {
		g_attachError = "blank-map recovery threw";
		return { false, g_attachError };
	}
}

native_faults::Status UpdateSlowAnimation( bool enabled, bool mapReady, bool runRecovery ) noexcept {
	if ( !enabled ) {
		g_mapSampled = false;
		g_carry.reset();
		g_slowMapError = nullptr;
		const bool clean = RestoreTimeLimit();
		return { false, clean ? "disabled by configuration" : "mp_timelimit restoration pending" };
	}
	if ( !mapReady ) {
		return { false, "waiting for a running map" };
	}
	if ( !__s2sdk_GetConVarInt32 || !__s2sdk_FindConVar || !__s2sdk_GetConVarFloat || !__s2sdk_SetConVarFloat ||
		!__s2sdk_GetGameTime || !__s2sdk_GetGameRules || !__s2sdk_GetSchemaOffset || !__s2sdk_GetMaxClients ||
		!__s2sdk_IsClientConnected || !__s2sdk_IsFakeClient || !__s2sdk_IsClientSourceTV || !__s2sdk_IsClientInGame ||
		!__s2sdk_PrintToChatAll || !__s2sdk_PrintToServer ) {
		return { false, "required map clock imports unavailable" };
	}
	try {
		if ( const auto* error = EnsureTransitions() ) {
			return { false, error };
		}
		if ( g_slowMapError ) {
			return { false, g_slowMapError };
		}
		const auto now = Clock::now();
		if ( !runRecovery || ( g_mapSampled && now < g_nextPoll ) ) {
			return { g_mapSampled, g_mapSampled ? "active" : "waiting for a clock sample" };
		}
		g_nextPoll = now + 1s;
		if ( !g_mapSampled ) {
			g_clock.OnMapStart( now );
			constexpr auto rules = "CCSGameRules";
			g_rules = { s2sdk::GetSchemaOffset( rules, "m_bWarmupPeriod" ),
				s2sdk::GetSchemaOffset( rules, "m_gamePhase" ),
				s2sdk::GetGameConfigOffset( g_transitionConfig, "CCSGameRules::MatchEndedPhase" ),
				s2sdk::GetSchemaOffset( rules, "m_flGameStartTime" ),
				s2sdk::GetSchemaOffset( rules, "m_totalRoundsPlayed" ), s2sdk::GetSchemaOffset( rules, "m_fRoundStartTime" ) };
			g_timelimit = s2sdk::FindConVar( "mp_timelimit" );
			g_matchRestartDelay = s2sdk::FindConVar( "mp_match_restart_delay" );
			if ( !g_timelimit || !g_matchRestartDelay || g_rules.warmup <= 0 || g_rules.phase <= 0 || g_rules.matchEndedPhase < 0 ||
				g_rules.start <= 0 || g_rules.rounds <= 0 ||
				g_rules.roundStart <= 0 ) {
				g_slowMapError = "match timing fields unavailable";
				return { false, g_slowMapError };
			}
			g_mapSampled = true;
			return { true, "active" };
		}
		if ( TransitionPending() ) {
			return { true, "native map transition pending" };
		}
		const float gameTime = s2sdk::GetGameTime();
		FinishReload( now, gameTime );
		if ( g_slowMapError ) {
			return { false, g_slowMapError };
		}
		const auto config = ReadRefreshConfig();
		const auto match = ObserveMatch();
		if ( !config || !match ) {
			return { false, config ? "waiting for match rules" : "clock settings unavailable or out of range" };
		}
		const auto players = ObserveHumans();
		switch ( const auto action = g_clock.Observe( now, std::chrono::duration<float>( gameTime ),
			players.human, players.joining, *match, *config ) ) {
		case RefreshStep::None: break;
		case RefreshStep::Armed:
			s2sdk::PrintToChatAll( "[ServerFixes] This map has been running a long time. It will reload at the next match boundary." );
			break;
		case RefreshStep::WarnRound: {
			const auto warning = "[ServerFixes] The map will reload at a round end after " +
				std::to_string( std::chrono::duration_cast<std::chrono::minutes>( config->warning ).count() ) + " minute(s).";
			s2sdk::PrintToChatAll( warning.c_str() );
			break;
		}
		default:
			RequestRefresh( action, now, gameTime, *match );
			break;
		}
		return { !g_slowMapError, g_slowMapError ? g_slowMapError : "active" };
	} catch ( ... ) {
		g_slowMapError = "map clock update threw";
		return { false, g_slowMapError };
	}
}

void OnMapEnd() noexcept {
	g_recovery = {};
	g_transitionError = nullptr;
	g_mapSampled = false;
	g_slowMapError = RestoreTimeLimit() ? nullptr : "mp_timelimit restoration pending";
}

namespace {
bool StopBlankMap() noexcept {
	g_armed = false;
	g_recovery = {};
	if ( g_hooked ) {
		try {
			const auto result = s2sdk::UnhookEvent( kNextLevelChanged, OnNextLevelChanged, s2sdk::HookMode::Post );
			if ( result != s2sdk::EventHookError::Okay && result != s2sdk::EventHookError::NotActive &&
				result != s2sdk::EventHookError::InvalidCallback ) {
				g_attachError = "nextlevel_changed cleanup pending";
				return false;
			}
			g_hooked = false;
		} catch ( ... ) {
			g_attachError = "nextlevel_changed cleanup pending";
			return false;
		}
	}
	g_attempted = false;
	g_attachError = nullptr;
	return true;
}
} // namespace

bool Shutdown() noexcept {
	const bool blankClean = StopBlankMap();
	OnMapEnd();
	g_carry.reset();
	g_host = nullptr;
	g_server = nullptr;
	if ( g_transitionConfig ) {
		try {
			s2sdk::CloseGameConfigFile( g_transitionConfig );
			g_transitionConfig = {};
		} catch ( ... ) {
			return false;
		}
	}
	return blankClean && !g_timeLease;
}

} // namespace imomoe::server_fixes::map_maintenance
