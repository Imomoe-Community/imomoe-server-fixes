// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include <plg/plugin.hpp>

#include <plugin_export.h>
#include <s2sdk/s2sdk.hpp>

#include "chat_policy.hpp"
#include "chat_listener.hpp"
#include "hostage_compat.hpp"
#include "spectator_compat.hpp"
#include "spawn_capacity.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace {

constexpr std::uint32_t kOwnsChatAdmission = 1u << 0;
constexpr std::uint32_t kOwnsHostageCompatibility = 1u << 1;
constexpr const char* kChatConVar = "sf_chat_admission";
constexpr const char* kHostageConVar = "sf_hostage_compat";
constexpr const char* kSpawnCapacityConVar = "sf_spawn_capacity";
constexpr const char* kSpectatorCompatConVar = "sf_spectator_compat";
constexpr const char* kStatusCommand = "sf_status";

struct ModuleStatus {
	bool enabled = true;
	bool available = false;
	std::string reason = "not initialized";
};

ModuleStatus g_chat;
ModuleStatus g_hostage;
ModuleStatus g_spawnCapacity;
ModuleStatus g_spectatorCompat;
std::uint64_t g_chatConVar = 0;
std::uint64_t g_hostageConVar = 0;
std::uint64_t g_spawnCapacityConVar = 0;
std::uint64_t g_spectatorCompatConVar = 0;
bool g_chatListenerRegistered = false;
bool g_statusCommandRegistered = false;
bool g_lifecycleActive = false;
imomoe::server_fixes::chat::ListenerState g_statusListener;

bool IsTrueValue( std::string_view value ) noexcept {
	return value == "1" || value == "true" || value == "TRUE" || value == "yes" || value == "on";
}

bool ReadEnabled( std::uint64_t handle, bool fallback ) noexcept {
	if ( handle == 0 || __s2sdk_GetConVarValue == nullptr ) {
		return fallback;
	}
	try {
		const auto value = s2sdk::GetConVarValue( handle );
		return IsTrueValue( std::string_view( value.data(), value.size() ) );
	} catch ( ... ) {
		return fallback;
	}
}

std::string_view FirstArgument( const plg::vector<plg::string>& arguments ) noexcept {
	if ( arguments.empty() ) {
		return {};
	}
	const auto& first = arguments.front();
	return { first.data(), first.size() };
}

void Log( std::string_view message ) noexcept {
	try {
		s2sdk::PrintToServer( plg::string( message ) );
	} catch ( ... ) {
	}
}

s2sdk::ResultType OnClientCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& arguments ) noexcept {
	try {
		if ( !imomoe::server_fixes::chat::CanRunCallback( g_lifecycleActive, true ) ) {
			return s2sdk::ResultType::Continue;
		}
		g_chat.enabled = ReadEnabled( g_chatConVar, g_chat.enabled );
		const bool chatContext = context == s2sdk::ConCommandContext::Chat;
		const bool inGame = slot >= 0 && s2sdk::IsClientInGame( slot );
		const auto decision = imomoe::server_fixes::chat::Evaluate(
			imomoe::server_fixes::chat::CanRunCallback( g_lifecycleActive, g_chat.enabled ), slot, inGame,
			chatContext, FirstArgument( arguments ) );
		if ( !decision.block ) {
			return s2sdk::ResultType::Continue;
		}

		const std::string line = "[ServerFixes] blocked pre-ingame chat slot=" + std::to_string( slot ) +
			" command=" + std::string( imomoe::server_fixes::chat::Label( decision.tag ) );
		Log( line );
		return s2sdk::ResultType::Stop;
	} catch ( ... ) {
		Log( "[ServerFixes] chat guard callback failed; command allowed" );
		return s2sdk::ResultType::Continue;
	}
}

void RefreshModuleStates() noexcept;

bool RegisterChatListener() noexcept {
	imomoe::server_fixes::chat::ListenerState state{
		.registered = g_chatListenerRegistered,
		.available = g_chat.available,
		.reason = g_chat.reason,
	};
	const bool registered = imomoe::server_fixes::chat::RegisterListener( state, [] {
		return s2sdk::AddCommandListener( plg::string( "" ), OnClientCommand, s2sdk::HookMode::Pre );
	} );
	g_chatListenerRegistered = state.registered;
	g_chat.available = state.available;
	g_chat.reason = state.reason;
	return registered;
}

bool UnregisterChatListener() noexcept {
	imomoe::server_fixes::chat::ListenerState state{
		.registered = g_chatListenerRegistered,
		.available = g_chat.available,
		.reason = g_chat.reason,
	};
	const bool unregistered = imomoe::server_fixes::chat::UnregisterListener( state, [] {
		return s2sdk::RemoveCommandListener( plg::string( "" ), OnClientCommand, s2sdk::HookMode::Pre );
	} );
	g_chatListenerRegistered = state.registered;
	g_chat.available = state.available;
	g_chat.reason = state.reason;
	return unregistered;
}

std::string ModuleLine( std::string_view name, const ModuleStatus& module ) {
	std::string result( name );
	result += " enabled=";
	result += module.enabled ? "1" : "0";
	result += " available=";
	result += module.available ? "1" : "0";
	result += " reason=\"";
	result += module.reason.empty() ? "unknown" : module.reason;
	result += '"';
	return result;
}

s2sdk::ResultType OnStatusCommand( std::int32_t slot, s2sdk::ConCommandContext context,
	const plg::vector<plg::string>& ) noexcept {
	if ( !imomoe::server_fixes::chat::CanRunCallback( g_lifecycleActive, true ) ) {
		return s2sdk::ResultType::Continue;
	}
	try {
		RefreshModuleStates();
		const auto chat = ModuleLine( "chat_admission", g_chat );
		const auto hostage = ModuleLine( "hostage_compat", g_hostage );
		const auto spawnCapacity = ModuleLine( "spawn_capacity", g_spawnCapacity );
		const auto spectatorCompat = ModuleLine( "spectator_compat", g_spectatorCompat );
		s2sdk::ReplyToCommand( context, slot, plg::string( "[ServerFixes] " + chat ) );
		s2sdk::ReplyToCommand( context, slot, plg::string( "[ServerFixes] " + hostage ) );
		s2sdk::ReplyToCommand( context, slot, plg::string( "[ServerFixes] " + spawnCapacity ) );
		s2sdk::ReplyToCommand( context, slot, plg::string( "[ServerFixes] " + spectatorCompat ) );
	} catch ( ... ) {
		Log( "[ServerFixes] status command failed" );
	}
	return s2sdk::ResultType::Handled;
}

bool RegisterStatusCommand() noexcept {
	try {
		const plg::string name( kStatusCommand );
		for ( const auto& existing : s2sdk::GetAllConCommands( s2sdk::ConVarFlag::None ) ) {
			if ( imomoe::server_fixes::chat::EqualsAsciiCaseInsensitive(
				std::string_view( existing.data(), existing.size() ), kStatusCommand ) ) {
				return false;
			}
		}
		const bool commandAdded = s2sdk::AddConsoleCommand( name, plg::string( "Show ServerFixes capability status" ),
			s2sdk::ConVarFlag::None, OnStatusCommand, s2sdk::HookMode::Pre );
		const bool callbackPresent = s2sdk::HasConsoleCommandCallbacks( name );
		const bool registered = imomoe::server_fixes::chat::RecordStatusCommandAddResult(
			g_statusListener, g_statusCommandRegistered, commandAdded, callbackPresent, [&] {
				return s2sdk::RemoveCommandListener( name, OnStatusCommand, s2sdk::HookMode::Pre );
			} );
		return registered;
	} catch ( ... ) {
		g_statusListener.reason = "status command registration threw";
		return false;
	}
}

bool UnregisterStatusCommand() noexcept {
	bool listenerClean = true;
	if ( g_statusListener.registered ) {
		const plg::string name( kStatusCommand );
		listenerClean = imomoe::server_fixes::chat::UnregisterListener( g_statusListener, [&] {
			return s2sdk::RemoveCommandListener( name, OnStatusCommand, s2sdk::HookMode::Pre );
		} );
	}
	if ( !listenerClean ) {
		return false;
	}
	const plg::string name( kStatusCommand );
	return imomoe::server_fixes::chat::CompleteStatusCommandCleanup( g_statusCommandRegistered,
		[&] { return s2sdk::HasConsoleCommandCallbacks( name ); },
		[&] { return s2sdk::RemoveCommand( name, OnStatusCommand ); } );
}

void RefreshModuleStates() noexcept {
	if ( !g_lifecycleActive ) {
		return;
	}
	g_chat.enabled = ReadEnabled( g_chatConVar, g_chat.enabled );
	g_hostage.enabled = ReadEnabled( g_hostageConVar, g_hostage.enabled );
	g_spawnCapacity.enabled = ReadEnabled( g_spawnCapacityConVar, g_spawnCapacity.enabled );
	g_spectatorCompat.enabled = ReadEnabled( g_spectatorCompatConVar, g_spectatorCompat.enabled );
	const auto hostageStatus = imomoe::server_fixes::hostage::Update( g_hostage.enabled, g_hostageConVar );
	g_hostage.available = hostageStatus.available;
	g_hostage.reason = hostageStatus.reason;
	const bool schemaQueriesReady = imomoe::server_fixes::hostage::SchemaQueriesReady();
	if ( schemaQueriesReady ) {
		const auto spawnStatus = imomoe::server_fixes::spawn_capacity::Update( g_spawnCapacity.enabled );
		g_spawnCapacity.available = spawnStatus.available;
		g_spawnCapacity.reason = spawnStatus.reason;
	} else {
		g_spawnCapacity.available = false;
		g_spawnCapacity.reason = "waiting for map start before querying engine schemas";
	}
	const auto spectatorStatus = imomoe::server_fixes::spectator_compat::Update(
		g_spectatorCompat.enabled, schemaQueriesReady );
	g_spectatorCompat.available = spectatorStatus.available;
	g_spectatorCompat.reason = spectatorStatus.reason;
	if ( g_chat.enabled ) {
		(void)RegisterChatListener();
	} else {
		(void)UnregisterChatListener();
	}
}

class ServerFixesPlugin final : public plg::Plugin {
public:
	plg::PluginResult OnPluginStart() override {
		g_lifecycleActive = false;
		try {
			if ( __s2sdk_CreateConVarBool == nullptr || __s2sdk_GetConVarValue == nullptr ||
				__s2sdk_AddCommandListener == nullptr || __s2sdk_RemoveCommandListener == nullptr ||
				__s2sdk_AddConsoleCommand == nullptr || __s2sdk_RemoveCommand == nullptr ||
				__s2sdk_GetAllConCommands == nullptr || __s2sdk_HasConsoleCommandCallbacks == nullptr ||
				__s2sdk_IsClientInGame == nullptr ) {
				return plg::PluginResult{ "Required S2SDK imports are unavailable" };
			}
			g_chatConVar = s2sdk::CreateConVarBool( plg::string( kChatConVar ), true,
				plg::string( "Blocks say and say_team until a real client is fully in game." ),
				s2sdk::ConVarFlag::SpOnly, false, false, false, false );
			g_hostageConVar = s2sdk::CreateConVarBool( plg::string( kHostageConVar ), true,
				plg::string( "Enables verified hostage compatibility behavior." ),
				s2sdk::ConVarFlag::SpOnly, false, false, false, false );
			g_spawnCapacityConVar = s2sdk::CreateConVarBool( plg::string( kSpawnCapacityConVar ), true,
				plg::string( "Enables map spawn capacity and manual map spawn points." ),
				s2sdk::ConVarFlag::SpOnly, false, false, false, false );
			g_spectatorCompatConVar = s2sdk::CreateConVarBool( plg::string( kSpectatorCompatConVar ), true,
				plg::string( "Synchronizes spectator capacity with actual maxplayers." ),
				s2sdk::ConVarFlag::SpOnly, false, false, false, false );
			if ( g_chatConVar == 0 || g_hostageConVar == 0 || g_spawnCapacityConVar == 0 ||
				g_spectatorCompatConVar == 0 ) {
				return plg::PluginResult{ "Could not create ServerFixes ConVars" };
			}

			g_lifecycleActive = true;
			g_chat.enabled = ReadEnabled( g_chatConVar, true );
			g_hostage.enabled = ReadEnabled( g_hostageConVar, true );
			g_spawnCapacity.enabled = ReadEnabled( g_spawnCapacityConVar, true );
			g_spectatorCompat.enabled = ReadEnabled( g_spectatorCompatConVar, true );
			g_hostage.available = false;
			g_hostage.reason = "not initialized";
			g_spawnCapacity.available = false;
			g_spawnCapacity.reason = "not initialized";
			g_spectatorCompat.available = false;
			g_spectatorCompat.reason = "not initialized";

			if ( g_chat.enabled ) {
				if ( !RegisterChatListener() ) {
					g_lifecycleActive = false;
					g_chat.enabled = false;
					const bool chatClean = UnregisterChatListener();
					if ( !chatClean ) {
						return plg::PluginResult{ "ServerFixes startup failed; chat listener cleanup could not be verified" };
					}
					return plg::PluginResult{ "ServerFixes could not register the chat admission listener" };
				}
			} else {
				g_chat.available = __s2sdk_AddCommandListener != nullptr &&
					__s2sdk_RemoveCommandListener != nullptr;
				g_chat.reason = g_chat.available ? "disabled" : "command listener import unavailable";
			}

			if ( !RegisterStatusCommand() ) {
				g_lifecycleActive = false;
				g_chat.enabled = false;
				const bool hostageClean = imomoe::server_fixes::hostage::Shutdown();
				const bool spawnClean = imomoe::server_fixes::spawn_capacity::Shutdown();
				const bool spectatorClean = imomoe::server_fixes::spectator_compat::Shutdown();
				const bool chatClean = UnregisterChatListener();
				const bool statusClean = UnregisterStatusCommand();
				if ( !hostageClean || !spawnClean || !spectatorClean || !chatClean || !statusClean ) {
					return plg::PluginResult{ "ServerFixes status registration failed; callback cleanup could not be verified" };
				}
				return plg::PluginResult{ "ServerFixes could not register sf_status" };
			}
			RefreshModuleStates();
			return {};
		} catch ( ... ) {
			g_lifecycleActive = false;
			g_chat.enabled = false;
			g_hostage.enabled = false;
			g_spawnCapacity.enabled = false;
			(void)imomoe::server_fixes::hostage::Shutdown();
			(void)imomoe::server_fixes::spawn_capacity::Shutdown();
			(void)imomoe::server_fixes::spectator_compat::Shutdown();
			(void)UnregisterChatListener();
			(void)UnregisterStatusCommand();
			return plg::PluginResult{ "ServerFixes startup threw" };
		}
	}

	plg::PluginResult OnPluginUpdate( std::chrono::milliseconds ) override {
		if ( !g_lifecycleActive ) {
			(void)imomoe::server_fixes::hostage::Shutdown();
			(void)imomoe::server_fixes::spawn_capacity::Shutdown();
			(void)imomoe::server_fixes::spectator_compat::Shutdown();
			if ( g_chatListenerRegistered ) {
				(void)UnregisterChatListener();
			}
			if ( g_statusListener.registered || g_statusCommandRegistered ) {
				(void)UnregisterStatusCommand();
			}
			return {};
		}
		RefreshModuleStates();
		return {};
	}

	plg::PluginResult OnPluginEnd() override {
		g_lifecycleActive = false;
		try {
			g_chat.enabled = false;
			g_hostage.enabled = false;
			g_spawnCapacity.enabled = false;
			g_spectatorCompat.enabled = false;
			const bool hostageClean = imomoe::server_fixes::hostage::Shutdown();
			const bool spawnClean = imomoe::server_fixes::spawn_capacity::Shutdown();
			const bool spectatorClean = imomoe::server_fixes::spectator_compat::Shutdown();
			const bool chatClean = UnregisterChatListener();
			const bool statusClean = UnregisterStatusCommand();
			if ( !hostageClean || !spawnClean || !spectatorClean || !chatClean || !statusClean ) {
				return plg::PluginResult{ "ServerFixes callbacks or owned changes could not be removed" };
			}
			return {};
		} catch ( ... ) {
			(void)imomoe::server_fixes::hostage::Shutdown();
			(void)imomoe::server_fixes::spawn_capacity::Shutdown();
			(void)imomoe::server_fixes::spectator_compat::Shutdown();
			(void)UnregisterChatListener();
			(void)UnregisterStatusCommand();
			return plg::PluginResult{ "ServerFixes cleanup threw" };
		}
	}
};

ServerFixesPlugin g_plugin;

} // namespace

PLUGIFY_PLUGIN( PLUGIN_API, &g_plugin )

extern "C" PLUGIN_API std::uint32_t GetOwnedFixes() {
	return kOwnsChatAdmission | kOwnsHostageCompatibility;
}
