// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

// Compile the production callback/module bodies into this focused fixture so
// the tests exercise the callbacks that the SDK listeners register.
#include "../src/spawn_capacity.cpp"
#include "../src/spectator_compat.cpp"
#include "../src/hostage_compat.hpp"

#include <appframework/iappsystem.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

class FixturePlugin final : public plg::Plugin {
public:
	FixturePlugin() = default;
	~FixturePlugin() = default;
};

plg::string FakePluginLocation( plg::hndl ) {
	return plg::string( "C:/server-fixes-fixture" );
}

void FakePlugifyLog( std::string_view, plg::Severity, const plg::source_location& ) {
}

plg::zone FakeBeginZone( std::string_view, const plg::source_location& ) {
	return 0;
}

void FakeEndZone( plg::zone ) {
}

} // namespace

namespace plg {

LogFn Log{};
BeginZoneFn BeginZone{};
EndZoneFn EndZone{};

namespace plugin {
GetLocationFn GetLocation{};
hndl handle{};
} // namespace plugin

Plugin* GetPlugin() {
	static FixturePlugin plugin;
	return &plugin;
}

} // namespace plg

namespace {

using imomoe::server_fixes::spawn_capacity::Team;

struct FakeEntity {
	std::int32_t handle{};
	std::string classname;
	plg::vec3 origin{};
	plg::vec3 angles{};
	bool alive{ true };
	bool nativeValid{ true };
	bool enabled{ true };
	bool removalQueued{};
	bool roundtripReady{ true };
	bool registered{ true };
};

struct FakeEvent {
	std::int32_t slot{};
	std::int32_t reason{};
};

std::deque<FakeEntity> g_entities;
std::int32_t g_nextEntityHandle{ 1000 };
std::int32_t g_playerControllerHandle{ 100 };
std::int32_t g_playerPawnHandle{ 200 };
std::int32_t g_actualMaxPlayers{ 12 };
std::int32_t g_playerTeam{ 3 };
bool g_playerInGame{ true };
bool g_playerAlive{ true };
std::int32_t g_otherTPlayers{};
std::int32_t g_retryDispatches{};
std::int32_t g_rawClientCommandDispatches{};
std::int32_t g_foreignRawCommandVetoCalls{};
bool g_foreignRawCommandVeto{};
std::int32_t g_nativeRefreshCalls{};
std::string g_currentMapName{ "de_poseidon" };
std::filesystem::path g_gameDirectory;
alignas( 8 ) std::array<std::byte, 4096> g_rulesStorage{};
void* g_rulesObject{ g_rulesStorage.data() };
std::array<std::vector<std::int32_t>, 2> g_primarySpawnHandles;
bool g_liveSpawnOccupancy{};
bool g_invalidCreatedSpawnGeometry{};
void* g_currentRules{ g_rulesObject };
std::int64_t g_rulesSpectatorLimit{};
std::int64_t g_rulesTerroristSpawnCount{};
std::int64_t g_rulesCounterTerroristSpawnCount{};
std::int64_t g_rulesMaxTerrorists{ 12 };
std::int64_t g_rulesMaxCounterTerrorists{ 12 };
std::int32_t g_schemaOffsetReadCalls{};
bool g_rejectCreatedSpawnPoints{};
bool g_newbornReverseMapping{};
bool g_skipSpawnDispatch{};
bool g_invalidCreateResult{};
std::int32_t g_dispatchSpawnCalls{};
bool g_delayEntityRemoval{};
std::int32_t g_removeEntityCalls{};
std::int32_t g_spectatorConVarWrites{};
std::int32_t g_gameConfigLoads{};
std::int32_t g_gameConfigCloses{};
s2sdk::ConfigId g_lastClosedGameConfig{};
std::string g_loadedGameConfigPath;
std::string g_spectatorConVar{ "0" };
s2sdk::OnMapEndCallback g_spawnMapStartCallback{};
s2sdk::OnMapEndCallback g_spectatorMapEndCallback{};
std::vector<s2sdk::OnMapEndCallback> g_mapStartListeners;
std::vector<s2sdk::OnMapEndCallback> g_mapEndListeners;
s2sdk::OnClientDisconnectCallback g_disconnectCallback{};
bool g_spectatorSchemaChainAvailable{ true };
struct SchemaSetCall {
	std::int64_t value{};
	bool changeState{};
	std::int32_t element{};
};
std::vector<SchemaSetCall> g_schemaSetCalls;
std::vector<std::string> g_commandReplies;
std::vector<std::string> g_registeredCommands;
struct FakeCommandListener {
	std::string name;
	s2sdk::ConCommandCallback callback{};
	s2sdk::HookMode mode{};
};
std::vector<FakeCommandListener> g_commandListeners;
bool g_fakeRemoveCommandFails{};
bool g_fakeRemoveCommandReturnsFalseAfterRemoval{};

FakeEntity* FindEntity( std::int32_t handle ) noexcept {
	const auto found = std::find_if( g_entities.begin(), g_entities.end(), [handle]( const FakeEntity& entity ) {
		return entity.handle == handle;
	} );
	return found == g_entities.end() ? nullptr : &*found;
}

FakeEntity* FindEntity( const void* pointer ) noexcept {
	const auto found = std::find_if( g_entities.begin(), g_entities.end(), [pointer]( const FakeEntity& entity ) {
		return &entity == pointer;
	} );
	return found == g_entities.end() ? nullptr : &*found;
}

FakeEntity& AddEntity( std::string classname ) {
	g_entities.push_back( { .handle = g_nextEntityHandle++, .classname = std::move( classname ) } );
	return g_entities.back();
}

std::int32_t FakePlayerSlotToHandle( std::int32_t slot ) {
	return slot == 0 ? g_playerControllerHandle : -1;
}

bool FakeClientInGame( std::int32_t slot ) {
	return ( slot == 0 && g_playerInGame ) || ( slot > 0 && slot <= g_otherTPlayers );
}

bool FakeClientAlive( std::int32_t slot ) {
	return slot == 0 && g_playerAlive;
}

void* FakeClientPawn( std::int32_t slot ) {
	if ( slot != 0 ) {
		return nullptr;
	}
	const auto found = std::find_if( g_entities.begin(), g_entities.end(), []( const FakeEntity& entity ) {
		return entity.handle == g_playerPawnHandle;
	} );
	return found == g_entities.end() ? nullptr : &*found;
}

s2sdk::CSTeam FakeClientTeam( std::int32_t slot ) {
	if ( slot == 0 ) {
		return static_cast<s2sdk::CSTeam>( g_playerTeam );
	}
	return slot > 0 && slot <= g_otherTPlayers ? s2sdk::CSTeam::T : s2sdk::CSTeam::None;
}

std::int32_t FakeMaxClients() {
	return g_actualMaxPlayers;
}

std::int32_t FakeFindEntityByClassname( std::int32_t cursor, const plg::string& classname ) {
	bool afterCursor = cursor == -1;
	for ( const auto& entity : g_entities ) {
		if ( !entity.alive ) {
			continue;
		}
		if ( !afterCursor ) {
			if ( entity.handle == cursor ) {
				afterCursor = true;
			}
			continue;
		}
		if ( entity.classname == std::string_view( classname.data(), classname.size() ) ) {
			return entity.handle;
		}
	}
	return -1;
}

std::int32_t FakeCreateEntityByName( const plg::string& classname ) {
	if ( g_invalidCreateResult ) {
		return -1;
	}
	auto& entity = AddEntity( std::string( classname.data(), classname.size() ) );
	entity.roundtripReady = !g_newbornReverseMapping;
	entity.registered = false;
	entity.nativeValid = !g_newbornReverseMapping;
	return entity.handle;
}

void FakeDispatchSpawn( std::int32_t handle, const plg::vector<plg::string>& keys,
	const plg::vector<plg::any>& values ) {
	++g_dispatchSpawnCalls;
	if ( g_skipSpawnDispatch ) {
		return;
	}
	auto* const entity = FindEntity( handle );
	if ( !entity || keys.size() != 2 || values.size() != 2 ) {
		return;
	}
	entity->origin = plg::get<plg::vec3>( values[0] );
	entity->angles = plg::get<plg::vec3>( values[1] );
	entity->nativeValid = !g_invalidCreatedSpawnGeometry;
	entity->registered = !g_rejectCreatedSpawnPoints;
	entity->roundtripReady = true;
}

void FakeRemoveEntity( std::int32_t handle ) {
	if ( auto* const entity = FindEntity( handle ) ) {
		++g_removeEntityCalls;
		if ( g_delayEntityRemoval ) {
			entity->removalQueued = true;
		} else {
			entity->alive = false;
		}
	}
}

void CompleteEngineRemovalFrame() {
	for ( auto& entity : g_entities ) {
		if ( entity.removalQueued ) {
			entity.alive = false;
		}
	}
}

plg::string FakeEntityClassname( std::int32_t handle ) {
	const auto* const entity = FindEntity( handle );
	return entity ? plg::string( entity->classname ) : plg::string{};
}

plg::vec3 FakeEntityOrigin( std::int32_t handle ) {
	const auto* const entity = FindEntity( handle );
	return entity ? entity->origin : plg::vec3{};
}

plg::vec3 FakeEntityAngles( std::int32_t handle ) {
	const auto* const entity = FindEntity( handle );
	return entity ? entity->angles : plg::vec3{};
}

std::int32_t FakePointerToHandle( void* pointer ) {
	const auto* const entity = FindEntity( pointer );
	return entity && entity->roundtripReady ? entity->handle : -1;
}

void* FakeHandleToPointer( std::int32_t handle ) {
	auto* const entity = FindEntity( handle );
	return entity && entity->alive ? entity : nullptr;
}

bool FakeValidHandle( std::int32_t handle ) {
	const auto* const entity = FindEntity( handle );
	return entity && entity->alive;
}

void FakePrintToServer( const plg::string& ) {
}

bool ForeignRawJoinTeamCommandVeto( CPlayerSlot slot, const CCommand& command ) {
	if ( !g_foreignRawCommandVeto || static_cast<std::int32_t>( slot ) != 0 || command.ArgC() != 2 ||
		std::string_view( command.Arg( 0 ) ) != "jointeam" || std::string_view( command.Arg( 1 ) ) != "2" ) {
		return false;
	}
	++g_foreignRawCommandVetoCalls;
	return true;
}

class FakeServerGameClients final : public CBaseAppSystem<IServerGameClients> {
public:
	void PreShutdown() override {
	}

	AppSystemBuildType_t GetBuildType() override {
		return APP_SYSTEM_BUILD_RELEASE;
	}

	void OnClientConnected( CPlayerSlot, const char*, uint64, const char*, const char*, bool ) override {
	}
	bool ClientConnect( CPlayerSlot, const char*, uint64, const char*, bool, CBufferString* ) override {
		return true;
	}
	void ClientPutInServer( CPlayerSlot, const char*, int, uint64 ) override {
	}
	void ClientActive( CPlayerSlot, bool, const char*, uint64 ) override {
	}
	void ClientFullyConnect( CPlayerSlot ) override {
	}
	void ClientDisconnect( CPlayerSlot, ENetworkDisconnectionReason, const char*, uint64, const char* ) override {
	}
	void ClientCommand( CPlayerSlot slot, const CCommand& command ) override {
		++g_rawClientCommandDispatches;
		if ( command.ArgC() != 2 || std::string_view( command.Arg( 0 ) ) != "jointeam" ||
			std::string_view( command.Arg( 1 ) ) != "2" ) {
			return;
		}
		++g_retryDispatches;
		if ( ForeignRawJoinTeamCommandVeto( slot, command ) ) {
			return;
		}
		plg::vector<plg::string> arguments;
		arguments.push_back( plg::string( command.Arg( 0 ) ) );
		arguments.push_back( plg::string( command.Arg( 1 ) ) );
		(void)imomoe::server_fixes::spawn_capacity::OnJoinCommandPre( static_cast<std::int32_t>( slot ),
			s2sdk::ConCommandContext::Console, arguments );
		g_playerTeam = static_cast<std::int32_t>( Team::Terrorist );
	}
	void ClientStringTableData( CPlayerSlot, void* ) override {
	}
	void ClientSettingsChanged( CPlayerSlot ) override {
	}
	void ClientSetupVisibility( CPlayerSlot, vis_info_t* ) override {
	}
	void ProcessUsercmds( CPlayerSlot, const CCLCMsg_Move_t&, bool ) override {
	}
	bool IsPlayerSlotOccupied( CPlayerSlot ) override {
		return false;
	}
	bool IsPlayerAlive( CPlayerSlot ) override {
		return false;
	}
	int GetPlayerScore( CPlayerSlot ) override {
		return 0;
	}
	void ClientEarPosition( CPlayerSlot, Vector* ) override {
	}
	void GetBugReportInfo( CBufferString& ) override {
	}
	void ClientVoice( CPlayerSlot ) override {
	}
	void ClientCommandKeyValues( CPlayerSlot, KeyValues* ) override {
	}
	void ClientDiagnostic( CPlayerSlot, const CCLCMsg_Diagnostic_t&, INetworkGameServer* ) override {
	}
	bool IsGamePausable() override {
		return false;
	}
	void HLTVClientFullyConnect( CPlayerSlot, const CSteamID& ) override {
	}
	bool CanHLTVClientConnect( CPlayerSlot, const CSteamID&, ENetworkDisconnectionReason* ) override {
		return false;
	}
	void StartHLTVServer( CEntityIndex ) override {
	}
	void SendHLTVStatusMessage( IHLTVServer*, bool, bool, const char*, int, int, int ) override {
	}
	IHLTVDirector* GetHLTVDirector() override {
		return nullptr;
	}
	uint32 GetPlayerTickBase( CPlayerSlot ) override {
		return 0;
	}
	void GetClientVisibilityInfo( CPlayerSlot, vis_info_t* ) override {
	}
	void ClientSvcUserMessage( CPlayerSlot, int, uint32, const void* ) override {
	}
	int GetPlayerHltvDelay( CPlayerSlot, CEntityIndex& ) override {
		return 0;
	}
	bool ClientReplayEvent( CPlayerSlot, const ClientReplayEventParams_t& ) override {
		return false;
	}
	bool CheckHltvPasswordMatch( const char*, const char*, const CSteamID&, void* ) override {
		return false;
	}
	bool ShouldSendMessageDuringHltvReplay( const NetMessageInfo_t*, const CNetMessage* ) override {
		return false;
	}
	EncryptedMessageKeyType_t GetMessageEncryptionKey( const NetMessageInfo_t*, CNetMessage* ) override {
		return kEncryptedMessageKeyType_None;
	}
	bool ValidateScriptCommands( const char*, CBufferString* ) override {
		return true;
	}
	bool ShouldDisconnectClientOnSteamAuthFailure( CPlayerSlot, ENetworkDisconnectionReason ) override {
		return false;
	}
};

FakeServerGameClients g_fakeServerGameClients;

void* FakeQueryInterface( const plg::string& module, const plg::string& name ) {
	return std::string_view( module.data(), module.size() ) == "server" &&
		std::string_view( name.data(), name.size() ) == INTERFACEVERSION_SERVERGAMECLIENTS ?
		&g_fakeServerGameClients : nullptr;
}

std::int32_t FakeEventPlayerSlot( void* event, const plg::string& ) {
	return static_cast<FakeEvent*>( event )->slot;
}

std::int32_t FakeEventInt( void* event, const plg::string& ) {
	return static_cast<FakeEvent*>( event )->reason;
}

std::int64_t FakeNativeSpawnRefresh( void* rules, void* spawn ) {
	if ( spawn ) std::abort();
	if ( rules == g_currentRules ) {
		++g_nativeRefreshCalls;
		g_rulesTerroristSpawnCount = std::count_if( g_entities.begin(), g_entities.end(), []( const FakeEntity& entity ) {
			return entity.alive && entity.enabled && entity.registered && entity.classname == "info_player_terrorist";
		} );
		g_rulesCounterTerroristSpawnCount = std::count_if( g_entities.begin(), g_entities.end(), []( const FakeEntity& entity ) {
			return entity.alive && entity.enabled && entity.registered && entity.classname == "info_player_counterterrorist";
		} );
	}
	return 0;
}

std::uint8_t FakeNativeSpawnEligibility( void* rules, void* pointer, std::uint8_t* ) {
	const auto* const entity = FindEntity( pointer );
	return rules == g_currentRules && entity && entity->alive && entity->enabled && entity->nativeValid && !g_liveSpawnOccupancy ? 1u : 0u;
}

s2sdk::ConfigId FakeLoadGameConfig( const plg::vector<plg::string>& paths ) {
	++g_gameConfigLoads;
	g_loadedGameConfigPath.clear();
	if ( !paths.empty() ) {
		g_loadedGameConfigPath.assign( paths.front().data(), paths.front().size() );
	}
	return 42u;
}

void* FakeGetGameConfigSignature( s2sdk::ConfigId id, const plg::string& name ) {
	if ( id != 42u ) {
		return nullptr;
	}
	const std::string_view signature( name.data(), name.size() );
	if ( signature == "ServerFixes::RegisterOrRecountSpawnPoints" ) {
		return reinterpret_cast<void*>( &FakeNativeSpawnRefresh );
	}
	if ( signature == "ServerFixes::SpawnPointEligible" ) {
		return reinterpret_cast<void*>( &FakeNativeSpawnEligibility );
	}
	return nullptr;
}

void FakeCloseGameConfig( s2sdk::ConfigId id ) {
	if ( id == 42u ) {
		++g_gameConfigCloses;
		g_lastClosedGameConfig = id;
	}
}

plg::string FakeCurrentMap() {
	return plg::string( g_currentMapName );
}

plg::string FakeGameDirectory() {
	return plg::string( g_gameDirectory.string() );
}

std::uint64_t FakeFindConVar( const plg::string& name ) {
	return std::string_view( name.data(), name.size() ) == "mp_spectators_max" ? 7u : 0u;
}

plg::string FakeGetConVarValue( std::uint64_t handle ) {
	return handle == 7u ? plg::string( g_spectatorConVar ) : plg::string{};
}

void FakeSetConVarValue( std::uint64_t handle, const plg::string& value, bool, bool ) {
	if ( handle == 7u ) {
		++g_spectatorConVarWrites;
		g_spectatorConVar.assign( value.data(), value.size() );
	}
}

void* FakeGameRules() {
	using namespace imomoe::server_fixes::spawn_capacity;
	if ( g_currentRules == g_rulesObject ) {
		for ( std::size_t index = 0; index < 2; ++index ) {
			auto& handles = g_primarySpawnHandles[index];
			handles.clear();
			for ( const auto& entity : g_entities ) {
				if ( entity.alive && entity.registered &&
					entity.classname == ( index == 0 ? "info_player_terrorist" : "info_player_counterterrorist" ) ) {
					handles.push_back( entity.handle );
				}
			}
			const auto count = static_cast<std::int32_t>( handles.size() );
			const NativeSpawnList list{ count, 0, handles.data(), count, 0 };
			std::memcpy( g_rulesStorage.data() + ( index == 0 ? 3968 : 3944 ), &list, sizeof( list ) );
		}
	}
	return g_currentRules;
}

std::int32_t FakeGameConfigOffset( s2sdk::ConfigId, const plg::string& name ) {
	const std::string_view value( name.data(), name.size() );
	if ( value == "ServerFixes::TSpawnList" ) return 3968;
	if ( value == "ServerFixes::CTSpawnList" ) return 3944;
	return -1;
}

std::int32_t FakeSchemaOffset( const plg::string& className, const plg::string& field ) {
	++g_schemaOffsetReadCalls;
	const std::string_view classView( className.data(), className.size() );
	const std::string_view name( field.data(), field.size() );
	if ( classView == "SpawnPoint" ) return name == "m_bEnabled" ? 9000 : -1;
	if ( classView != "CCSGameRules" ) return -1;
	if ( name == "m_iSpawnPointCount_Terrorist" ) return 3724;
	if ( name == "m_iSpawnPointCount_CT" ) return 3728;
	if ( name == "m_iMaxNumTerrorists" ) return 3732;
	if ( name == "m_iMaxNumCTs" ) return 3736;
	if ( name == "m_iSpectatorSlotCount" ) return 0;
	if ( name == "__m_pChainEntity" ) return g_spectatorSchemaChainAvailable ? 16 : -1;
	return -1;
}

std::int64_t FakeGetEntSchema( void* pointer, const plg::string& className, const plg::string& field, std::int32_t ) {
	const std::string_view classView( className.data(), className.size() );
	const std::string_view name( field.data(), field.size() );
	if ( classView == "SpawnPoint" && name == "m_bEnabled" ) {
		const auto* const entity = FindEntity( pointer );
		return entity && entity->enabled ? 1 : 0;
	}
	if ( name == "m_iSpawnPointCount_Terrorist" ) return g_rulesTerroristSpawnCount;
	if ( name == "m_iSpawnPointCount_CT" ) return g_rulesCounterTerroristSpawnCount;
	if ( name == "m_iMaxNumTerrorists" ) return g_rulesMaxTerrorists;
	if ( name == "m_iMaxNumCTs" ) return g_rulesMaxCounterTerrorists;
	return g_rulesSpectatorLimit;
}

void FakeSetEntSchema( void* pointer, const plg::string& className, const plg::string& field, std::int64_t value,
	bool changeState, std::int32_t element ) {
	const std::string_view classView( className.data(), className.size() );
	const std::string_view name( field.data(), field.size() );
	if ( classView == "SpawnPoint" && name == "m_bEnabled" ) {
		if ( auto* const entity = FindEntity( pointer ) ) {
			entity->enabled = value != 0;
		}
	} else if ( name == "m_iMaxNumTerrorists" ) {
		g_rulesMaxTerrorists = value;
	} else if ( name == "m_iMaxNumCTs" ) {
		g_rulesMaxCounterTerrorists = value;
	} else if ( name == "m_iSpectatorSlotCount" ) {
		g_schemaSetCalls.push_back( { .value = value, .changeState = changeState, .element = element } );
		g_rulesSpectatorLimit = value;
	}
}

void FakeReplyToCommand( s2sdk::ConCommandContext, std::int32_t, const plg::string& message ) {
	g_commandReplies.emplace_back( message.data(), message.size() );
}

bool FakeAddConsoleCommand( const plg::string& name, const plg::string&, s2sdk::ConVarFlag,
	s2sdk::ConCommandCallback, s2sdk::HookMode ) {
	g_registeredCommands.emplace_back( name.data(), name.size() );
	return true;
}

bool FakeRemoveCommand( const plg::string& name, s2sdk::ConCommandCallback ) {
	if ( g_fakeRemoveCommandFails ) {
		return false;
	}
	const std::string_view view( name.data(), name.size() );
	g_registeredCommands.erase( std::remove_if( g_registeredCommands.begin(), g_registeredCommands.end(),
		[view]( const std::string& registered ) { return registered == view; } ), g_registeredCommands.end() );
	return !g_fakeRemoveCommandReturnsFalseAfterRemoval;
}

bool FakeHasConsoleCommandCallbacks( const plg::string& name ) {
	const std::string_view view( name.data(), name.size() );
	return std::any_of( g_registeredCommands.begin(), g_registeredCommands.end(),
		[view]( const std::string& registered ) { return registered == view; } );
}

plg::vector<plg::string> FakeGetAllConCommands( s2sdk::ConVarFlag ) {
	plg::vector<plg::string> result;
	for ( const auto& name : g_registeredCommands ) {
		result.push_back( plg::string( name ) );
	}
	return result;
}

bool FakeAddCommandListener( const plg::string& name, s2sdk::ConCommandCallback callback, s2sdk::HookMode mode ) {
	g_commandListeners.push_back( {
		.name = std::string( name.data(), name.size() ),
		.callback = callback,
		.mode = mode,
	} );
	return true;
}

bool FakeRemoveCommandListener( const plg::string& name, s2sdk::ConCommandCallback callback, s2sdk::HookMode mode ) {
	const std::string_view view( name.data(), name.size() );
	const auto found = std::find_if( g_commandListeners.begin(), g_commandListeners.end(),
		[view, callback, mode]( const FakeCommandListener& listener ) {
			return listener.name == view && listener.callback == callback && listener.mode == mode;
		} );
	if ( found == g_commandListeners.end() ) {
		return false;
	}
	g_commandListeners.erase( found );
	return true;
}

s2sdk::EventHookError FakeHookEvent( const plg::string&, s2sdk::EventCallback, s2sdk::HookMode ) {
	return s2sdk::EventHookError::Okay;
}

s2sdk::EventHookError FakeUnhookEvent( const plg::string&, s2sdk::EventCallback, s2sdk::HookMode ) {
	return s2sdk::EventHookError::Okay;
}

void FakeOnMapEndRegister( s2sdk::OnMapEndCallback callback ) {
	g_spectatorMapEndCallback = callback;
	g_mapEndListeners.push_back( callback );
}

void FakeOnMapEndUnregister( s2sdk::OnMapEndCallback callback ) {
	g_mapEndListeners.erase( std::remove( g_mapEndListeners.begin(), g_mapEndListeners.end(), callback ),
		g_mapEndListeners.end() );
	if ( callback == g_spectatorMapEndCallback ) {
		g_spectatorMapEndCallback = g_mapEndListeners.empty() ? nullptr : g_mapEndListeners.back();
	}
}

void FakeOnMapStartRegister( s2sdk::OnMapEndCallback callback ) {
	g_spawnMapStartCallback = callback;
	g_mapStartListeners.push_back( callback );
}

void FakeOnMapStartUnregister( s2sdk::OnMapEndCallback callback ) {
	g_mapStartListeners.erase( std::remove( g_mapStartListeners.begin(), g_mapStartListeners.end(), callback ),
		g_mapStartListeners.end() );
	if ( callback == g_spawnMapStartCallback ) {
		g_spawnMapStartCallback = g_mapStartListeners.empty() ? nullptr : g_mapStartListeners.back();
	}
}

void FakeOnClientDisconnectRegister( s2sdk::OnClientDisconnectCallback callback ) {
	g_disconnectCallback = callback;
}

void FakeOnClientDisconnectUnregister( s2sdk::OnClientDisconnectCallback callback ) {
	if ( callback == g_disconnectCallback ) {
		g_disconnectCallback = nullptr;
	}
}

void DispatchMapStart() {
	const auto listeners = g_mapStartListeners;
	for ( const auto callback : listeners ) {
		if ( callback ) {
			callback();
		}
	}
}

void DispatchMapEnd() {
	const auto listeners = g_mapEndListeners;
	for ( const auto callback : listeners ) {
		if ( callback ) {
			callback();
		}
	}
}

void InstallImports() {
	plg::plugin::GetLocation = FakePluginLocation;
	__s2sdk_AddConsoleCommand = FakeAddConsoleCommand;
	__s2sdk_RemoveCommand = FakeRemoveCommand;
	__s2sdk_HasConsoleCommandCallbacks = FakeHasConsoleCommandCallbacks;
	__s2sdk_GetAllConCommands = FakeGetAllConCommands;
	__s2sdk_AddCommandListener = FakeAddCommandListener;
	__s2sdk_RemoveCommandListener = FakeRemoveCommandListener;
	__s2sdk_HookEvent = FakeHookEvent;
	__s2sdk_UnhookEvent = FakeUnhookEvent;
	__s2sdk_PlayerSlotToEntHandle = FakePlayerSlotToHandle;
	__s2sdk_IsClientInGame = FakeClientInGame;
	__s2sdk_IsClientAlive = FakeClientAlive;
	__s2sdk_GetClientPawn = FakeClientPawn;
	__s2sdk_GetClientTeam = FakeClientTeam;
	__s2sdk_GetMaxClients = FakeMaxClients;
	__s2sdk_FindEntityByClassname = FakeFindEntityByClassname;
	__s2sdk_CreateEntityByName = FakeCreateEntityByName;
	__s2sdk_DispatchSpawn2 = FakeDispatchSpawn;
	__s2sdk_RemoveEntity = FakeRemoveEntity;
	__s2sdk_GetEntityClassname = FakeEntityClassname;
	__s2sdk_GetEntityAbsOrigin = FakeEntityOrigin;
	__s2sdk_GetEntityEyeAngles = FakeEntityAngles;
	__s2sdk_EntPointerToEntHandle = FakePointerToHandle;
	__s2sdk_EntHandleToEntPointer = FakeHandleToPointer;
	__s2sdk_IsValidEntHandle = FakeValidHandle;
	__s2sdk_QueryInterface = FakeQueryInterface;
	__s2sdk_GetEventPlayerSlot = FakeEventPlayerSlot;
	__s2sdk_GetEventInt = FakeEventInt;
	__s2sdk_PrintToServer = FakePrintToServer;
	__s2sdk_GetCurrentMap = FakeCurrentMap;
	__s2sdk_GetGameDirectory = FakeGameDirectory;
	__s2sdk_FindConVar = FakeFindConVar;
	__s2sdk_GetConVarValue = FakeGetConVarValue;
	__s2sdk_SetConVarValue = FakeSetConVarValue;
	__s2sdk_GetGameRules = FakeGameRules;
	__s2sdk_GetSchemaOffset = FakeSchemaOffset;
	__s2sdk_GetEntSchema2 = FakeGetEntSchema;
	__s2sdk_SetEntSchema2 = FakeSetEntSchema;
	__s2sdk_OnMapStart_Register = FakeOnMapStartRegister;
	__s2sdk_OnMapStart_Unregister = FakeOnMapStartUnregister;
	__s2sdk_OnMapEnd_Register = FakeOnMapEndRegister;
	__s2sdk_OnMapEnd_Unregister = FakeOnMapEndUnregister;
	__s2sdk_OnClientDisconnect_Register = FakeOnClientDisconnectRegister;
	__s2sdk_OnClientDisconnect_Unregister = FakeOnClientDisconnectUnregister;
	__s2sdk_LoadGameConfigFile = FakeLoadGameConfig;
	__s2sdk_GetGameConfigSignature = FakeGetGameConfigSignature;
	__s2sdk_GetGameConfigOffset = FakeGameConfigOffset;
	__s2sdk_CloseGameConfigFile = FakeCloseGameConfig;
	__s2sdk_ReplyToCommand = FakeReplyToCommand;
}

void ResetFakeEntities() {
	g_newbornReverseMapping = false;
	g_skipSpawnDispatch = false;
	g_invalidCreateResult = false;
	g_dispatchSpawnCalls = 0;
	g_entities.clear();
	g_registeredCommands.clear();
	g_nextEntityHandle = 1000;
	g_playerControllerHandle = 100;
	g_playerPawnHandle = 200;
	g_entities.push_back( { .handle = g_playerControllerHandle, .classname = "player" } );
	g_entities.push_back( { .handle = g_playerPawnHandle, .classname = "CCSPlayerPawn" } );
}

void ResetSpawnModule( std::string map ) {
	g_liveSpawnOccupancy = false;
	g_invalidCreatedSpawnGeometry = false;
	using namespace imomoe::server_fixes::spawn_capacity;
	CompleteEngineRemovalFrame();
	(void)Shutdown();
	g_spawnMapStartCallback = nullptr;
	g_delayEntityRemoval = false;
	ResetFakeEntities();
	g_enabled = true;
	g_mapPrepared = true;
	g_mapDataLoaded = true;
	g_mapLoadAttempted = true;
	g_rebuildPending = false;
	g_capacityReconcilePending = false;
	g_nativeRefreshPending = false;
	g_currentMap = std::move( map );
	g_currentMapName = g_currentMap;
	g_mapReason.clear();
	g_moduleReason.clear();
	g_manualPoints.clear();
	g_nativePoints.clear();
	g_nativeSourceRules = nullptr;
	g_nativeSourceMapGeneration = 0;
	g_ownedSpawns.clear();
	g_pendingRetries.clear();
	g_mapLifecycle = {};
	g_nextManualId = 1;
	g_capacityMaxClients = 0;
	g_effectiveTeamCapacities = {};
	g_spawnCapacitySchemaOffsets = { -2, -2, -2, -2 };
	g_spawnEnabledSchemaOffset = -2;
	g_teamMaximumLeases = {};
	g_teamMaximumYielded = {};
	g_teamMaximumCapacityBlocked = false;
	g_capacityReadbackAvailable = false;
	g_nativeSpawnRecount = nullptr;
	g_spawnListOffsets = { -1, -1 };
	g_nativeSpawnEligibility = nullptr;
	g_spawnGameConfig = 0;
	g_delayEntityRemoval = false;
	g_removeEntityCalls = 0;
	g_gameConfigLoads = 0;
	g_gameConfigCloses = 0;
	g_lastClosedGameConfig = 0;
	g_loadedGameConfigPath.clear();
	g_commandReplies.clear();
	ResizePlayerState( g_actualMaxPlayers );
	InvalidateJoinRequests( false );
	std::fill( g_retryDispatchActive.begin(), g_retryDispatchActive.end(), false );
	g_playerTeam = static_cast<std::int32_t>( s2sdk::CSTeam::CT );
	g_playerInGame = true;
	g_playerAlive = true;
	g_otherTPlayers = 0;
	g_retryDispatches = 0;
	g_rawClientCommandDispatches = 0;
	g_foreignRawCommandVetoCalls = 0;
	g_foreignRawCommandVeto = false;
	g_nativeRefreshCalls = 0;
	g_rulesTerroristSpawnCount = 0;
	g_rulesCounterTerroristSpawnCount = 0;
	g_rulesMaxTerrorists = g_actualMaxPlayers;
	g_rulesMaxCounterTerrorists = g_actualMaxPlayers;
	g_rejectCreatedSpawnPoints = false;
}

void Require( bool condition, std::string_view message ) {
	if ( !condition ) {
		std::cerr << "FAILED: " << message << '\n';
		std::exit( EXIT_FAILURE );
	}
}

FakeEntity& AddNativeSource( Team team, bool nativeValid = true, plg::vec3 origin = { 128.0f, 64.0f, 0.0f } ) {
	auto& source = AddEntity( team == Team::Terrorist ? "info_player_terrorist" : "info_player_counterterrorist" );
	source.origin = origin;
	source.angles = { 0.0f, 180.0f, 0.0f };
	source.nativeValid = nativeValid;
	source.registered = nativeValid;
	imomoe::server_fixes::spawn_capacity::SpawnPoint point{
		.id = 1,
		.team = team,
		.origin = source.origin,
		.angles = source.angles,
		.entityHandle = source.handle,
	};
	if ( nativeValid ) imomoe::server_fixes::spawn_capacity::g_nativePoints.push_back( point );
	return source;
}

void TestActualJoinCallbacksRejectStaleQueuedRetry() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	plg::vector<plg::string> firstRequest;
	firstRequest.push_back( plg::string( "jointeam" ) );
	firstRequest.push_back( plg::string( "2" ) );
	(void)OnJoinCommandPre( 0, s2sdk::ConCommandContext::Console, firstRequest );
	FakeEvent capacityFailure{ .slot = 0, .reason = kFailureTerroristFull };
	(void)OnJoinFailedEvent( plg::string( "jointeam_failed" ), &capacityFailure, false );
	Require( g_pendingRetries.size() == 1, "the actual Pre/event sequence queues its observed capacity failure" );

	// A later native request supersedes the queued failure even when controller,
	// pawn, team, and alive state are unchanged.
	plg::vector<plg::string> laterRequest;
	laterRequest.push_back( plg::string( "jointeam" ) );
	laterRequest.push_back( plg::string( "3" ) );
	(void)OnJoinCommandPre( 0, s2sdk::ConCommandContext::Console, laterRequest );
	ProcessPendingRetries();
	Require( g_retryDispatches == 0, "the pending-retry consumer must discard the superseded request" );
	Require( std::none_of( g_ownedSpawns.begin(), g_ownedSpawns.end(), []( const OwnedSpawn& spawn ) {
		return spawn.automatic;
	} ), "a stale request must not create an automatic capacity point" );
}

void QueueCapacityFailureForCurrentClient() {
	using namespace imomoe::server_fixes::spawn_capacity;
	plg::vector<plg::string> request;
	request.push_back( plg::string( "jointeam" ) );
	request.push_back( plg::string( "2" ) );
	(void)OnJoinCommandPre( 0, s2sdk::ConCommandContext::Console, request );
	FakeEvent failure{ .slot = 0, .reason = kFailureTerroristFull };
	(void)OnJoinFailedEvent( plg::string( "jointeam_failed" ), &failure, false );
	Require( g_pendingRetries.size() == 1, "the real Pre/event sequence queues one rejected request" );
}

void TestNegativePlayerHandlesRetainJoinRetryIdentity() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	constexpr std::int32_t negativeControllerHandle = -0x1234567;
	constexpr std::int32_t negativePawnHandle = -0x2345678;
	FindEntity( g_playerControllerHandle )->handle = negativeControllerHandle;
	g_playerControllerHandle = negativeControllerHandle;
	FindEntity( g_playerPawnHandle )->handle = negativePawnHandle;
	g_playerPawnHandle = negativePawnHandle;

	plg::vector<plg::string> request;
	request.push_back( plg::string( "jointeam" ) );
	request.push_back( plg::string( "2" ) );
	(void)OnJoinCommandPre( 0, s2sdk::ConCommandContext::Console, request );
	Require( g_joinAttempts[0].has_value() &&
		g_joinAttempts[0]->identity.controllerHandle == negativeControllerHandle &&
		g_joinAttempts[0]->identity.pawnHandle == negativePawnHandle,
		"valid negative fullhandles must be retained in the captured join identity" );

	FakeEvent capacityFailure{ .slot = 0, .reason = kFailureTerroristFull };
	(void)OnJoinFailedEvent( plg::string( "jointeam_failed" ), &capacityFailure, false );
	Require( g_pendingRetries.size() == 1 &&
		g_pendingRetries.front().identity.controllerHandle == negativeControllerHandle &&
		g_pendingRetries.front().identity.pawnHandle == negativePawnHandle,
		"the matching failure event must queue a request whose controller and pawn handles are negative" );
	ProcessPendingRetries();
	Require( g_retryDispatches == 1,
		"the unchanged negative fullhandle identity must pass the retry's current-player check" );
}

void TestActualLifeEventInvalidatesQueuedRetry() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	QueueCapacityFailureForCurrentClient();
	const auto lifeBefore = g_joinEpochs[0].life;
	FakeEvent lifeBoundary{ .slot = 0, .reason = 0 };
	(void)OnPlayerLifeBoundaryEvent( plg::string( "player_death" ), &lifeBoundary, false );
	Require( g_pendingRetries.empty() && g_joinEpochs[0].life != lifeBefore,
		"the registered player-life callback clears queued requests and advances the life identity" );
	ProcessPendingRetries();
	Require( g_retryDispatches == 0 && g_ownedSpawns.empty(),
		"a capacity failure from the prior life cannot add a spawn or replay jointeam" );
}

void TestActualJoinCallbacksRetryOneMatchingCapacityFailure() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	plg::vector<plg::string> request;
	request.push_back( plg::string( "jointeam" ) );
	request.push_back( plg::string( "2" ) );
	(void)OnJoinCommandPre( 0, s2sdk::ConCommandContext::Console, request );
	FakeEvent failure{ .slot = 0, .reason = kFailureTerroristFull };
	(void)OnJoinFailedEvent( plg::string( "jointeam_failed" ), &failure, false );
	Require( g_pendingRetries.size() == 1, "the real capacity failure callback should queue one retry" );
	ProcessPendingRetries();
	ProcessPendingRetries();
	Require( g_retryDispatches == 1 && g_rawClientCommandDispatches == 1,
		"the real retry consumer dispatches the same jointeam command once through IServerGameClients::ClientCommand" );
	Require( g_nativeRefreshCalls == 1, "the created point refreshes native spawn selection before retry" );
	Require( g_ownedSpawns.size() == 1 && g_ownedSpawns.front().automatic,
		"the retry consumes one owned point on the denied team" );
}

void TestPendingRetryPreservesForeignRawClientCommandVeto() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	QueueCapacityFailureForCurrentClient();
	g_foreignRawCommandVeto = true;
	ProcessPendingRetries();
	Require( g_retryDispatches == 1 && g_rawClientCommandDispatches == 1 && g_foreignRawCommandVetoCalls == 1,
		"a foreign raw ClientCommand callback must receive the replayed jointeam request" );
	Require( g_playerTeam == static_cast<std::int32_t>( Team::CounterTerrorist ),
		"the foreign raw ClientCommand veto must prevent native team conversion" );
	Require( g_pendingRetries.empty() && !g_retryDispatchActive[0],
		"a vetoed same-request retry is consumed once and cannot recurse or queue itself again" );
	ProcessPendingRetries();
	Require( g_retryDispatches == 1 && g_foreignRawCommandVetoCalls == 1,
		"a foreign vetoed replay must not be retried a second time" );
}

void TestJointeamUsesGlobalPreAndFailureEventAsRetryBoundary() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	Require( g_commandListeners.empty(), "the previous fixture must have removed all command listeners" );
	std::string error;
	Require( RegisterRuntimeHooks( error ), "the actual runtime-hook registration path should install cleanly" );
	Require( g_commandListeners.size() == 1 && g_commandListeners.front().name.empty() &&
		g_commandListeners.front().callback == OnJoinCommandPre &&
		g_commandListeners.front().mode == s2sdk::HookMode::Pre,
		"spawn capacity registers one global ClientCommand Pre callback and no unsupported named/Post jointeam listener" );

	const auto callback = g_commandListeners.front().callback;
	const auto requestBeforeOtherCommand = g_joinEpochs[0].request;
	plg::vector<plg::string> unrelatedCommand;
	unrelatedCommand.push_back( plg::string( "say" ) );
	unrelatedCommand.push_back( plg::string( "hello" ) );
	(void)callback( 0, s2sdk::ConCommandContext::Console, unrelatedCommand );
	Require( g_joinEpochs[0].request == requestBeforeOtherCommand && !g_joinAttempts[0],
		"the global Pre listener must ignore non-jointeam commands without invalidating a pending join identity" );

	plg::vector<plg::string> request;
	request.push_back( plg::string( "jointeam" ) );
	request.push_back( plg::string( "2" ) );
	(void)callback( 0, s2sdk::ConCommandContext::Console, request );
	Require( g_joinAttempts[0].has_value(), "the registered global callback must record a valid jointeam request" );
	FakeEvent balancingFailure{ .slot = 0, .reason = kFailureTerroristLimit };
	(void)OnJoinFailedEvent( plg::string( "jointeam_failed" ), &balancingFailure, false );
	Require( g_pendingRetries.empty() && !g_joinAttempts[0],
		"a non-capacity jointeam_failed outcome must consume the request without queuing a retry" );

	(void)callback( 0, s2sdk::ConCommandContext::Console, request );
	FakeEvent capacityFailure{ .slot = 0, .reason = kFailureTerroristFull };
	(void)OnJoinFailedEvent( plg::string( "jointeam_failed" ), &capacityFailure, false );
	(void)OnJoinFailedEvent( plg::string( "jointeam_failed" ), &capacityFailure, false );
	Require( g_pendingRetries.size() == 1 && !g_joinAttempts[0],
		"only one matching capacity failure event may queue the captured request, without a command Post callback" );

	Require( UnregisterRuntimeHooks(), "the registered global callback and failure observer must clean up" );
	Require( g_commandListeners.empty(), "runtime-hook cleanup must remove the global Pre listener by its empty name" );
}

void TestAdminCommandCleanupUsesCallbackAbsencePostcondition() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	std::string error;
	Require( RegisterAdminCommands( error ), "the production command registration path should register the four owned commands" );
	Require( g_registeredCommands.size() == 4, "the fixture should observe all owned spawn administration commands" );

	g_fakeRemoveCommandFails = true;
	Require( !UnregisterAdminCommands( error ) && g_adminCommandsRegistered &&
		error.find( "RemoveCommand returned false" ) != std::string::npos,
		"cleanup must retain RemoveCommand=false context while callbacks remain registered" );

	g_fakeRemoveCommandFails = false;
	g_fakeRemoveCommandReturnsFalseAfterRemoval = true;
	error.clear();
	Require( UnregisterAdminCommands( error ) && !g_adminCommandsRegistered && g_registeredCommands.empty() && error.empty(),
		"RemoveCommand=false is clean when HasConsoleCommandCallbacks confirms the owned callbacks are absent" );
	g_fakeRemoveCommandReturnsFalseAfterRemoval = false;
}

void TestNativeRefreshReadbackFollowsActualTeamAllowance() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const auto previousMaxPlayers = g_actualMaxPlayers;
	g_actualMaxPlayers = 32;
	ResetSpawnModule( "de_poseidon" );
	g_rulesMaxTerrorists = 5;
	g_rulesMaxCounterTerrorists = 5;
	for ( std::int32_t index = 0; index < 5; ++index ) {
		AddNativeSource( Team::Terrorist );
		AddNativeSource( Team::CounterTerrorist );
	}
	std::string error;
	Require( ReconcileAutomaticCapacity( error ), "reconciliation should raise old 5/5 team caps and fill each actual allowance" );
	Require( g_rulesMaxTerrorists == 32 && g_rulesMaxCounterTerrorists == 32,
		"both old 5-player team maximums must be raised to actual maxplayers 32" );
	Require( g_effectiveTeamCapacities[0].spawnPoints == 32 && g_effectiveTeamCapacities[0].teamMaximum == 32 &&
		g_effectiveTeamCapacities[0].effective == 32,
		"T capacity must read back 32 valid native points and a 32-player max-team value" );
	Require( g_effectiveTeamCapacities[1].spawnPoints == 32 && g_effectiveTeamCapacities[1].teamMaximum == 32 &&
		g_effectiveTeamCapacities[1].effective == 32,
		"CT capacity must read back 32 valid native points and a 32-player max-team value" );
	Require( CountOwned( Team::Terrorist, true ) == 27 && CountOwned( Team::CounterTerrorist, true ) == 27,
		"each team receives only the 27 duplicate points missing from its five native points" );
	Require( Shutdown(), "shutdown must remove capacity points and restore owned team maximums" );
	Require( g_rulesMaxTerrorists == 5 && g_rulesMaxCounterTerrorists == 5,
		"shutdown restores both team maximums to their captured 5-player baselines" );
	g_actualMaxPlayers = previousMaxPlayers;
}

void TestTeamMaximumExternalOverrideIsYieldedAndPreserved() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const auto previousMaxPlayers = g_actualMaxPlayers;
	g_actualMaxPlayers = 32;
	ResetSpawnModule( "de_poseidon" );
	g_rulesMaxTerrorists = 5;
	g_rulesMaxCounterTerrorists = 5;
	AddNativeSource( Team::Terrorist );
	AddNativeSource( Team::CounterTerrorist );
	std::string error;
	Require( ReconcileAutomaticCapacity( error ), "team capacity should be initialized before the external override" );
	g_rulesMaxTerrorists = 40;
	Require( ReconcileAutomaticCapacity( error ), "a valid larger external T cap should remain usable" );
	Require( g_rulesMaxTerrorists == 40 && g_teamMaximumYielded[0],
		"the module must yield the T maximum after an external integer override" );
	Require( ReconcileAutomaticCapacity( error ), "the yielded owner should remain stable across further updates" );
	Require( g_rulesMaxTerrorists == 40 && g_teamMaximumYielded[0],
		"subsequent updates must not overwrite or reacquire the external T maximum" );
	Require( Shutdown(), "shutdown must clean owned capacity while preserving an external T override" );
	Require( g_rulesMaxTerrorists == 40 && g_rulesMaxCounterTerrorists == 5,
		"shutdown preserves the external T cap and restores the still-owned CT cap" );
	g_actualMaxPlayers = previousMaxPlayers;
}

void TestUpdateReportsExternalTeamCapacityAndKeepsEffectiveReadbackCurrent() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const auto previousMaxPlayers = g_actualMaxPlayers;
	g_actualMaxPlayers = 32;
	ResetSpawnModule( "de_poseidon" );
	g_rulesMaxTerrorists = 5;
	g_rulesMaxCounterTerrorists = 5;
	AddNativeSource( Team::Terrorist );
	AddNativeSource( Team::CounterTerrorist );
	std::string error;
	Require( ReconcileAutomaticCapacity( error ), "team capacity should be initialized before the external override" );
	const auto initialRefreshCalls = g_nativeRefreshCalls;
	const auto initialStatus = Update( true );
	Require( initialStatus.available && initialStatus.reason.find( "T=32 CT=32 / maxplayers=32" ) != std::string::npos,
		"the actual Update path should publish the ready native allowance before an external change" );

	g_rulesMaxTerrorists = 40;
	auto status = Update( true );
	Require( status.available && status.reason.find( "T max-team capacity has an external owner" ) != std::string::npos,
		"the actual Update path should report a sufficient external T max-team override" );
	Require( g_teamMaximumYielded[0] && g_effectiveTeamCapacities[0].teamMaximum == 40 &&
		g_effectiveTeamCapacities[0].effective == 32,
		"status must combine the live team maximum with the cached native-valid spawn count" );

	g_rulesMaxTerrorists = 5;
	for ( int update = 0; update < 3; ++update ) {
		status = Update( true );
		Require( !status.available && status.reason.find( "T max-team capacity has an external owner below actual maxplayers" ) !=
			std::string::npos,
			"normal updates must latch an insufficient external cap and make the module unavailable" );
		Require( g_effectiveTeamCapacities[0].teamMaximum == 5 && g_effectiveTeamCapacities[0].effective == 5,
			"the actual Update path must reflect the current team gate in status readback" );
	}
	Require( g_nativeRefreshCalls == initialRefreshCalls,
		"repeated status updates must read team maxima without running the native spawn scanner" );

	g_rulesMaxTerrorists = 32;
	status = Update( true );
	Require( status.available && status.reason.find( "T max-team capacity has an external owner" ) != std::string::npos,
		"the same yielded owner may restore usable capacity without being reacquired" );
	Require( g_rulesMaxTerrorists == 32 && g_teamMaximumYielded[0] &&
		g_nativeRefreshCalls == initialRefreshCalls,
		"recovery must preserve the current external value and reuse cached native spawn counts" );
	Require( Shutdown(), "shutdown must preserve the external T cap and restore the still-owned CT cap" );
	Require( g_rulesMaxTerrorists == 32 && g_rulesMaxCounterTerrorists == 5,
		"shutdown preserves the yielded T value and restores the still-owned CT cap" );
	g_actualMaxPlayers = previousMaxPlayers;
}

void TestManualSpawnRecoversMapWithoutValidNativeSourceAndSkipsInvalidFirst() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const auto previousMaxPlayers = g_actualMaxPlayers;
	const auto previousGameDirectory = g_gameDirectory;
	g_actualMaxPlayers = 2;
	ResetSpawnModule( "de_poseidon" );
	g_rulesMaxTerrorists = 2;
	g_rulesMaxCounterTerrorists = 2;
	const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
	g_gameDirectory = std::filesystem::temp_directory_path() /
		( "serverfixes-no-valid-spawn-source-" + std::to_string( ticks ) );
	Require( !std::filesystem::exists( g_gameDirectory ), "the manual recovery fixture must use a fresh storage root" );

	// Capture the invalid T point before the valid T point, then leave CT with no
	// valid source. Map preparation must fail without making the console recovery
	// command depend on g_mapPrepared.
	auto& invalidT = AddEntity( "info_player_terrorist" );
	invalidT.origin = { 1.0f, 2.0f, 3.0f };
	invalidT.nativeValid = false;
	invalidT.registered = false;
	auto& validT = AddEntity( "info_player_terrorist" );
	validT.origin = { 101.0f, 102.0f, 103.0f };
	auto& invalidCt = AddEntity( "info_player_counterterrorist" );
	invalidCt.origin = { 201.0f, 202.0f, 203.0f };
	invalidCt.nativeValid = false;
	invalidCt.registered = false;

	CompleteMapLoad();
	Require( g_mapDataLoaded && !g_mapPrepared,
		"automatic preparation must report unavailable while CT has no valid native or saved manual source" );
	Require( g_nativePoints.size() == 1,
		"native source capture must keep only engine-registered valid templates, excluding invalid candidates" );

	plg::vector<plg::string> addArguments;
	addArguments.push_back( plg::string( "sf_spawn_add" ) );
	addArguments.push_back( plg::string( "CT" ) );
	addArguments.push_back( plg::string( "512" ) );
	addArguments.push_back( plg::string( "0" ) );
	addArguments.push_back( plg::string( "32" ) );
	(void)OnSpawnAddCommand( -1, s2sdk::ConCommandContext::Console, addArguments );
	Require( g_manualPoints.size() == 1 && g_manualPoints.front().team == Team::CounterTerrorist && g_mapPrepared,
		"server-console sf_spawn_add must recover readiness for the team lacking valid native points" );
	const auto automaticT = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), []( const OwnedSpawn& spawn ) {
		return spawn.automatic && spawn.team == Team::Terrorist && IsCompleteHandle( spawn.handle );
	} );
	Require( automaticT != g_ownedSpawns.end(), "the valid second T source must produce the required T capacity point" );
	const auto* const clonedT = FindEntity( automaticT->handle );
	Require( clonedT && clonedT->origin.x == 101.0f,
		"native source selection must skip the invalid first point and clone the later eligible source" );

	plg::vector<plg::string> saveArguments;
	saveArguments.push_back( plg::string( "sf_spawn_save" ) );
	(void)OnSpawnSaveCommand( -1, s2sdk::ConCommandContext::Console, saveArguments );
	std::vector<SpawnPoint> loaded;
	std::string error;
	Require( LoadManualPoints( g_currentMap, loaded, error ) && loaded.size() == 1 &&
		loaded.front().team == Team::CounterTerrorist && loaded.front().origin.x == 512.0f,
		"the console recovery point must persist through the production map loader" );
	Require( Shutdown(), "shutdown must remove recovered points and close the owned gamedata" );
	Require( g_gameConfigCloses == 1, "spawn-capacity shutdown must close its loaded gamedata handle" );
	const auto expectedDirectory = std::filesystem::temp_directory_path() / g_gameDirectory.filename();
	Require( g_gameDirectory.lexically_normal() == expectedDirectory.lexically_normal() &&
		g_gameDirectory.filename().string().starts_with( "serverfixes-no-valid-spawn-source-" ),
		"manual recovery test cleanup must remain inside its uniquely named temporary directory" );
	std::filesystem::remove_all( g_gameDirectory );
	g_gameDirectory = previousGameDirectory;
	g_actualMaxPlayers = previousMaxPlayers;
}

void TestNegativeNativeSpawnHandlesAreEnumeratedAndEligible() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	g_nextEntityHandle = -1000;
	const auto negativeHandle = AddNativeSource( Team::Terrorist ).handle;
	AddNativeSource( Team::Terrorist );

	const auto captured = CaptureNativePoints( Team::Terrorist );
	Require( captured.size() == 2 && captured.front().entityHandle == negativeHandle,
		"classname enumeration must continue after a valid negative signed fullhandle" );
	g_nativePoints = captured;
	std::string error;
	const auto eligible = ValidNativeSourcesForTeam( Team::Terrorist, error );
	Require( error.empty() && eligible.size() == 2 && eligible.front().entityHandle == negativeHandle,
		"SDK validity, handle-pointer roundtrip, and native spawn eligibility must accept the negative entity" );
}

void TestCreationDispatchCompletesReverseHandleMapping() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const SpawnPoint point{ .id = 1, .team = Team::Terrorist, .origin = { 128.0f, 64.0f, 0.0f } };
	for ( const auto nextHandle : { 1000, -2000 } ) {
		ResetSpawnModule( "de_poseidon" );
		g_newbornReverseMapping = true;
		g_nextEntityHandle = nextHandle;
		std::string error;
		Require( CreateSpawnEntity( point, true, error ),
			"a newly allocated full handle must be dispatched before requiring reverse mapping" );
		Require( g_dispatchSpawnCalls == 1 && g_ownedSpawns.size() == 1 &&
			IsCompleteHandle( nextHandle ) && SamePosition( FakeEntityOrigin( nextHandle ), point.origin ) &&
			g_rulesTerroristSpawnCount == 1,
			"one dispatch must establish the complete owned identity, position and native capacity" );
	}

	ResetSpawnModule( "de_poseidon" );
	g_newbornReverseMapping = true;
	g_skipSpawnDispatch = true;
	std::string error;
	Require( !CreateSpawnEntity( point, true, error ) && g_dispatchSpawnCalls == 1 &&
		g_removeEntityCalls == 1 && !FindEntity( 1000 )->alive && g_rulesTerroristSpawnCount == 0,
		"a dispatch that leaves its new allocation unspawned must fail and release that owned raw handle" );

	ResetSpawnModule( "de_poseidon" );
	g_invalidCreateResult = true;
	error.clear();
	Require( !CreateSpawnEntity( point, true, error ) && g_dispatchSpawnCalls == 0 && g_ownedSpawns.empty(),
		"an invalid Create result must never be dispatched or claimed as a live entity" );
}

void TestNegativeCreatedSpawnHandlesRemainOwnedThroughRemoval() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const auto previousMaxPlayers = g_actualMaxPlayers;
	g_actualMaxPlayers = 2;
	ResetSpawnModule( "de_poseidon" );
	g_rulesMaxTerrorists = 2;
	g_rulesMaxCounterTerrorists = 2;
	AddNativeSource( Team::Terrorist );
	AddNativeSource( Team::CounterTerrorist );
	g_nextEntityHandle = -2000;

	std::string error;
	Require( ReconcileAutomaticCapacity( error ), "negative-handle native sources must allow automatic capacity preparation" );
	Require( g_ownedSpawns.size() == 2 && std::all_of( g_ownedSpawns.begin(), g_ownedSpawns.end(),
		[]( const OwnedSpawn& spawn ) {
			return spawn.automatic && spawn.handle < 0 && spawn.handle != -1 && IsCompleteHandle( spawn.handle );
		} ), "negative CreateEntityByName results must be configured and tracked as complete owned handles" );

	g_delayEntityRemoval = true;
	Require( Shutdown(), "shutdown must retire negative owned handles through the normal exclusion/removal path" );
	Require( g_removeEntityCalls == 2 && g_ownedSpawns.size() == 2 &&
		std::all_of( g_ownedSpawns.begin(), g_ownedSpawns.end(), []( const OwnedSpawn& spawn ) {
			const auto* const entity = FindEntity( spawn.handle );
			return spawn.handle < 0 && spawn.removalRequested && entity && entity->removalQueued &&
				spawn.disabledForRemoval && spawn.nativeExclusionVerified;
		} ), "negative owned handles must remain tracked until engine-queued removal completes" );
	CompleteEngineRemovalFrame();
	PruneRemovedSpawns();
	Require( g_ownedSpawns.empty(), "negative owned-handle records must retire after the entity system invalidates them" );
	g_delayEntityRemoval = false;
	g_actualMaxPlayers = previousMaxPlayers;
}

void TestDelayedEntityRemovalIsSafeAtShutdown() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const auto previousMaxPlayers = g_actualMaxPlayers;
	g_actualMaxPlayers = 2;
	ResetSpawnModule( "de_poseidon" );
	g_rulesMaxTerrorists = 2;
	g_rulesMaxCounterTerrorists = 2;
	AddNativeSource( Team::Terrorist );
	AddNativeSource( Team::CounterTerrorist );
	std::string error;
	Require( ReconcileAutomaticCapacity( error ), "capacity points must exist before delayed-removal shutdown" );
	Require( CountOwned( Team::Terrorist, true ) == 1 && CountOwned( Team::CounterTerrorist, true ) == 1,
		"the delayed-removal fixture must own one additional spawn per team" );

	g_delayEntityRemoval = true;
	Require( Shutdown(), "Shutdown must rely on native exclusion before queuing engine-owned deletion" );
	Require( g_removeEntityCalls == 2, "shutdown must issue one removal request for each owned spawn" );
	Require( std::all_of( g_ownedSpawns.begin(), g_ownedSpawns.end(), []( const OwnedSpawn& spawn ) {
		const auto* const entity = FindEntity( spawn.handle );
		return entity && entity->alive && entity->removalQueued && !entity->enabled &&
			spawn.disabledForRemoval && spawn.nativeExclusionVerified && spawn.removalRequested;
	} ), "queued entities may remain handle-valid only after disabling and native-exclusion verification" );
	Require( g_rulesTerroristSpawnCount == 1 && g_rulesCounterTerroristSpawnCount == 1,
		"a deferred Remove request must not leave either owned point in the native spawn counts" );
	Require( g_gameConfigCloses == 1, "safe queued deletion must not keep the gamedata handle open at unload" );
	CompleteEngineRemovalFrame();
	PruneRemovedSpawns();
	Require( g_ownedSpawns.empty(), "the simulated engine removal frame must retire the pending owned records" );
	g_delayEntityRemoval = false;
	g_actualMaxPlayers = previousMaxPlayers;
}

void TestReenableWhileExcludedDeletionRemainsEngineOwned() {
	using namespace imomoe::server_fixes::spawn_capacity;
	const auto previousMaxPlayers = g_actualMaxPlayers;
	g_actualMaxPlayers = 2;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	AddNativeSource( Team::CounterTerrorist );
	std::string error;
	Require( ReconcileAutomaticCapacity( error ), "initial capacity must be ready before off/on" );
	g_delayEntityRemoval = true;
	Require( Update( false ).available && g_rulesTerroristSpawnCount == 1 && g_rulesCounterTerroristSpawnCount == 1,
		"disable must exclude its points from native capacity before requesting deferred deletion" );
	Require( Update( true ).available && g_rulesTerroristSpawnCount == 2 && g_rulesCounterTerroristSpawnCount == 2,
		"reenable must restore capacity without waiting for already-excluded engine-owned deletion" );
	Require( g_ownedSpawns.size() == 4 && CountOwned( Team::Terrorist, true ) == 1 &&
		CountOwned( Team::CounterTerrorist, true ) == 1,
		"retired handles remain tracked separately from the new active capacity" );
	const SpawnPoint manual{ .id = 77, .team = Team::Terrorist, .origin = { 192.0f, 64.0f, 0.0f } };
	g_manualPoints.push_back( manual );
	Require( CreateSpawnEntity( manual, false, error ), "create the previous manual cohort" );
	auto previousManual = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), []( const OwnedSpawn& spawn ) {
		return !spawn.automatic && spawn.manualId == 77;
	} );
	Require( RemoveOwnedRecord( previousManual ), "retire the previous manual cohort without physical deletion" );
	Require( CreateSpawnEntity( manual, false, error ), "create the active manual point with the same id" );
	const auto currentManualHandle = g_ownedSpawns.back().handle;
	Require( ReconcileAutomaticCapacity( error ), "active manual capacity replaces an automatic point" );
	const auto cohortsBeforeRound = g_ownedSpawns.size();
	ReconcileSpawnEntitiesAfterRound();
	Require( g_mapPrepared && g_ownedSpawns.size() == cohortsBeforeRound,
		"next round must select the active manual cohort and not block or duplicate it" );
	plg::vector<plg::string> removeArguments;
	removeArguments.push_back( "sf_spawn_remove" );
	removeArguments.push_back( "77" );
	(void)OnSpawnRemoveCommand( -1, s2sdk::ConCommandContext::Console, removeArguments );
	const auto currentManual = std::find_if( g_ownedSpawns.begin(), g_ownedSpawns.end(), [currentManualHandle]( const OwnedSpawn& spawn ) {
		return spawn.handle == currentManualHandle;
	} );
	Require( g_manualPoints.empty() && currentManual != g_ownedSpawns.end() && currentManual->removalRequested &&
		g_rulesTerroristSpawnCount == 2 && CountOwned( Team::Terrorist, true ) == 1,
		"manual remove must retire the active same-id entity and replenish automatic capacity" );
	Require( Shutdown(), "both retired and active cohorts must remain safe at shutdown" );
	CompleteEngineRemovalFrame();
	PruneRemovedSpawns();
	Require( g_ownedSpawns.empty(), "engine deletion eventually retires both cohorts" );
	g_delayEntityRemoval = false;
	g_actualMaxPlayers = previousMaxPlayers;
}

void TestCapacityRetryDoesNotBypassNativeTeamMaximum() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	g_rulesMaxTerrorists = 1;
	g_otherTPlayers = 1;
	std::string error;
	Require( RecountNativeSpawnLists( error ), "the fake engine should establish the baseline native team allowance" );
	QueueCapacityFailureForCurrentClient();
	ProcessPendingRetries();
	Require( g_retryDispatches == 0, "the retry must not override a full native team maximum" );
	Require( g_ownedSpawns.size() == 1 && g_ownedSpawns.front().automatic &&
		g_ownedSpawns.front().removalRequested,
		"an added point that cannot increase native effective capacity is removed instead of reported as a retry" );
}

void TestNativeCapacityReadbackRejectsUnregisteredSpawnEntity() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	AddNativeSource( Team::Terrorist );
	AddNativeSource( Team::CounterTerrorist );
	g_actualMaxPlayers = 2;
	g_rulesMaxTerrorists = 2;
	g_rulesMaxCounterTerrorists = 2;
	g_rejectCreatedSpawnPoints = true;
	std::string error;
	Require( !ReconcileAutomaticCapacity( error ), "capacity must not report ready when native validation omits a new point" );
	Require( !error.empty() && !g_ownedSpawns.empty() && g_ownedSpawns.front().removalRequested,
		"the invalid clone is removed and the effective-capacity readback failure remains visible" );
	g_actualMaxPlayers = 12;
}

void TestOccupiedRegisteredSpawnsKeepCapacityAcrossRoundAndRebuild() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	g_actualMaxPlayers = 4;
	g_rulesMaxTerrorists = 4;
	g_rulesMaxCounterTerrorists = 4;
	AddNativeSource( Team::Terrorist );
	AddNativeSource( Team::CounterTerrorist );
	std::string error;
	Require( ReconcileAutomaticCapacity( error ), "empty-map native registration must establish capacity" );
	auto& foreign = AddEntity( "info_player_terrorist" );
	foreign.origin = { 4096.0f, 4096.0f, -4096.0f };
	foreign.nativeValid = false;
	foreign.registered = true;
	g_nativePoints = CaptureNativePoints( Team::Terrorist );
	Require( std::none_of( g_nativePoints.begin(), g_nativePoints.end(), [&]( const SpawnPoint& candidate ) {
		return candidate.entityHandle == foreign.handle;
	} ), "an unconditionally registered but geometrically invalid foreign point must not become a template" );
	foreign.alive = false;
	// Recapture CT while unoccupied before exercising the real occupied round path.
	const auto ctSources = CaptureNativePoints( Team::CounterTerrorist );
	g_nativePoints.insert( g_nativePoints.end(), ctSources.begin(), ctSources.end() );
	const auto movedHandle = g_ownedSpawns.front().handle;
	auto* moved = FindEntity( movedHandle );
	const auto validOrigin = moved->origin;
	moved->origin = { 8192.0f, 8192.0f, -8192.0f };
	const SpawnPoint movedPoint{ .id = 50, .team = Team::Terrorist, .origin = moved->origin };
	Require( !HasRegisteredTemplate( movedPoint, error ),
		"externally moved owned points must not transfer validation to a different location" );
	moved->origin = validOrigin;
	g_invalidCreatedSpawnGeometry = true;
	const SpawnPoint invalidManual{ .id = 51, .team = Team::Terrorist, .origin = { 16384.0f, 16384.0f, -16384.0f } };
	Require( !CreateSpawnEntity( invalidManual, false, error ),
		"automatic native registration must not make a new invalid manual location valid" );
	g_invalidCreatedSpawnGeometry = false;
	error.clear();
	PruneRemovedSpawns();
	g_liveSpawnOccupancy = true;
	Require( !NativeSpawnPointEligible( g_nativePoints.front().entityHandle, error ),
		"the original native predicate must still reject current live-player occupancy" );
	const auto ownedBefore = g_ownedSpawns.size();
	(void)OnRoundPrestartEvent( plg::string( "round_prestart" ), nullptr, false );
	Require( g_mapPrepared && g_rulesTerroristSpawnCount == 4 && g_rulesCounterTerroristSpawnCount == 4 &&
		g_ownedSpawns.size() == ownedBefore, "occupied points must retain native capacity without round duplication" );
	const auto lost = g_ownedSpawns.front().handle;
	FindEntity( lost )->alive = false;
	(void)OnRoundPrestartEvent( plg::string( "round_prestart" ), nullptr, false );
	Require( g_mapPrepared && g_rulesTerroristSpawnCount == 4 && g_rulesCounterTerroristSpawnCount == 4,
		"a genuinely lost owned point must be rebuilt from the registered occupied template" );
	g_delayEntityRemoval = true;
	Require( Shutdown() && g_rulesTerroristSpawnCount == 1 && g_rulesCounterTerroristSpawnCount == 1,
		"disable must immediately exclude retired points even while occupied and physically pending" );
	g_enabled = true;
	g_mapDataLoaded = true;
	g_mapLifecycle = {};
	CompleteMapLoad();
	Require( g_mapPrepared && g_rulesTerroristSpawnCount == 4 && g_rulesCounterTerroristSpawnCount == 4,
		"runtime enable must rebuild occupied registered sources without waiting for another map" );
	g_liveSpawnOccupancy = false;
	g_actualMaxPlayers = 12;
}

void TestProductionManualSaveLoadRoundTrip() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_mirage" );
	const auto previousGameDirectory = g_gameDirectory;
	const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
	g_gameDirectory = std::filesystem::temp_directory_path() /
		( "serverfixes-spawn-callback-" + std::to_string( ticks ) );
	Require( !std::filesystem::exists( g_gameDirectory ), "the temporary storage root must be new" );
	g_manualPoints = { SpawnPoint{
		.id = 17,
		.team = Team::CounterTerrorist,
		.origin = { 128.25f, -256.5f, 32.0f },
		.angles = { 0.0f, 180.0f, -0.25f },
	} };
	g_currentMap = "de_mirage";
	std::string error;
	Require( SaveManualPoints( g_manualPoints, error ), "production save must write a valid manual map point" );
	std::vector<SpawnPoint> loaded;
	Require( LoadManualPoints( g_currentMap, loaded, error ), "production map loader must accept its saved final row" );
	Require( loaded.size() == 1 && loaded[0].id == 17 && loaded[0].team == Team::CounterTerrorist &&
		loaded[0].origin.x == 128.25f && loaded[0].origin.y == -256.5f && loaded[0].origin.z == 32.0f &&
		loaded[0].angles.x == 0.0f && loaded[0].angles.y == 180.0f && loaded[0].angles.z == -0.25f,
		"production save/load must round-trip the complete point without adding a field" );
	const auto expectedDirectory = std::filesystem::temp_directory_path() / g_gameDirectory.filename();
	Require( g_gameDirectory.lexically_normal() == expectedDirectory.lexically_normal() &&
		g_gameDirectory.filename().string().starts_with( "serverfixes-spawn-callback-" ),
		"test cleanup must remain inside its uniquely named temporary directory" );
	std::filesystem::remove_all( g_gameDirectory );
	g_gameDirectory = previousGameDirectory;
}

void TestActualSameNameMapCallbacksRebuildWithoutDuplicates() {
	using namespace imomoe::server_fixes::spawn_capacity;
	using imomoe::server_fixes::hostage::SchemaQueriesReady;
	ResetSpawnModule( "de_mirage" );
	Require( imomoe::server_fixes::hostage::Shutdown(), "hostage lifecycle state should begin clean" );
	Require( !SchemaQueriesReady(), "schema access starts closed before the SDK map-start callback" );
	g_schemaOffsetReadCalls = 0;
	const auto previousGameDirectory = g_gameDirectory;
	const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
	g_gameDirectory = std::filesystem::temp_directory_path() /
		( "serverfixes-map-callback-" + std::to_string( ticks ) );
	Require( !std::filesystem::exists( g_gameDirectory ), "the map callback storage root must be new" );
	g_manualPoints = { SpawnPoint{
		.id = 17,
		.team = Team::CounterTerrorist,
		.origin = { 256.0f, 64.0f, 0.0f },
		.angles = { 0.0f, 90.0f, 0.0f },
	} };
	std::string error;
	Require( SaveManualPoints( g_manualPoints, error ), "map callback fixture must persist one manual point" );
	(void)imomoe::server_fixes::hostage::Update( false, 0 );
	Require( !SchemaQueriesReady() && g_mapStartListeners.size() == 1 && g_schemaOffsetReadCalls == 0,
		"the production host update must register its map-start listener without querying schemas early" );
	DispatchMapStart();
	Require( SchemaQueriesReady(),
		"the registered production hostage callback must open schema readiness at the SDK map-start dispatch" );
	(void)imomoe::server_fixes::hostage::Update( false, 0 );
	// The spawn module is deliberately first updated only after the host opens
	// schema readiness, matching RefreshModuleStates on its next normal tick.
	// ResetSpawnModule normally marks a prior map-load attempt for other tests;
	// real startup has not called spawn::Update while the central gate is closed.
	g_mapLoadAttempted = false;
	const auto initialStatus = Update( true );
	Require( initialStatus.available && g_mapPrepared && g_ownedSpawns.size() == 1,
		"the first spawn-module update after map start must load the initial manual point without a native source" );
	Require( g_spawnMapStartCallback != nullptr,
		"the enabled spawn module must register its production map-start callback for later map generations" );
	const auto oldHandle = g_ownedSpawns.front().handle;

	DispatchMapEnd();
	Require( !SchemaQueriesReady(), "the registered production map-end callback must close schema readiness" );
	if ( auto* const oldEntity = FindEntity( oldHandle ) ) {
		oldEntity->alive = false;
	}
	DispatchMapStart();
	Require( SchemaQueriesReady(), "the next SDK map-start dispatch must reopen schema readiness" );
	(void)imomoe::server_fixes::hostage::Update( false, 0 );
	const auto mapStartStatus = Update( true );
	Require( g_currentMap == "de_mirage" && g_mapPrepared && g_manualPoints.size() == 1 && g_ownedSpawns.size() == 1,
		"the actual map-start callback must restore manual capacity before the first team round" );
	Require( mapStartStatus.available,
		"the production update after map start must expose manual recovery without waiting for round_prestart" );
	Require( g_ownedSpawns.front().manualId == 17 && g_ownedSpawns.front().handle != oldHandle,
		"the reloaded generation restores one saved manual point, not the old entity" );
	const auto rebuiltHandle = g_ownedSpawns.front().handle;
	(void)OnRoundPrestartEvent( plg::string( "round_prestart" ), nullptr, false );
	Require( g_ownedSpawns.size() == 1 && g_ownedSpawns.front().handle == rebuiltHandle,
		"the first team round must not duplicate the already restored manual point" );

	(void)Shutdown();
	Require( imomoe::server_fixes::hostage::Shutdown(),
		"hostage shutdown must unregister its map lifecycle listeners" );
	Require( g_spawnMapStartCallback == nullptr,
		"spawn-module shutdown must unregister its map-start callback" );
	Require( g_mapStartListeners.empty() && g_mapEndListeners.empty(),
		"both production modules must release their lifecycle callbacks" );
	const auto expectedDirectory = std::filesystem::temp_directory_path() / g_gameDirectory.filename();
	Require( g_gameDirectory.lexically_normal() == expectedDirectory.lexically_normal() &&
		g_gameDirectory.filename().string().starts_with( "serverfixes-map-callback-" ),
		"map callback cleanup must remain inside its uniquely named temporary directory" );
	std::filesystem::remove_all( g_gameDirectory );
	g_gameDirectory = previousGameDirectory;
}

void TestDisabledModuleSameNameReloadInvalidatesTemplateQualification() {
	using namespace imomoe::server_fixes::spawn_capacity;
	ResetSpawnModule( "de_poseidon" );
	Require( imomoe::server_fixes::hostage::Shutdown(), "central lifecycle begins clean" );
	(void)imomoe::server_fixes::hostage::Update( false, 0 );
	DispatchMapStart();
	const auto source = AddNativeSource( Team::Terrorist ).handle;
	AddNativeSource( Team::CounterTerrorist );
	LoadMap( "de_poseidon" );
	Require( g_mapPrepared, "source qualification must first be established on the old map" );
	Require( Update( false ).available && !g_mapEndRegistered,
		"disabled A3 releases feature callbacks while the central map observer remains active" );
	const auto oldGeneration = imomoe::server_fixes::hostage::MapGeneration();
	DispatchMapEnd();
	Require( imomoe::server_fixes::hostage::MapGeneration() != oldGeneration,
		"the central observer must see map end even while A3 and hostage fixes are disabled" );
	// Model reused Rules address, map name and raw full handle in a new map.
	FindEntity( source )->nativeValid = false;
	DispatchMapStart();
	(void)Update( true );
	Require( std::none_of( g_nativePoints.begin(), g_nativePoints.end(), [source]( const SpawnPoint& point ) {
		return point.entityHandle == source;
	} ), "new map generations cannot inherit old geometry qualification through address/handle reuse" );
	Require( Shutdown() && imomoe::server_fixes::hostage::Shutdown(), "both module and central callbacks must unload" );
}

void ResetSpectatorModule() {
	(void)imomoe::server_fixes::spectator_compat::Shutdown();
	g_actualMaxPlayers = 12;
	g_currentMapName = "de_mirage";
	g_currentRules = &g_rulesObject;
	g_rulesSpectatorLimit = 0;
	g_spectatorConVar = "0";
	g_spectatorMapEndCallback = nullptr;
	g_spectatorSchemaChainAvailable = true;
	g_schemaSetCalls.clear();
	imomoe::server_fixes::spectator_compat::g_schemaOffset = -2;
	imomoe::server_fixes::spectator_compat::g_schemaChainOffset = -2;
}

void TestSpectatorExternalNonIntegerAndMapGenerationOwnership() {
	using imomoe::server_fixes::spectator_compat::Shutdown;
	using imomoe::server_fixes::spectator_compat::Update;
	ResetSpectatorModule();
	const auto initial = Update( true, true );
	Require( initial.available && g_spectatorConVar == "12" && g_rulesSpectatorLimit == 12,
		"spectator capacity must follow actual maxplayers in both effective locations" );
	Require( g_schemaSetCalls.size() == 1 && g_schemaSetCalls.front().value == 12 &&
		g_schemaSetCalls.front().changeState && g_schemaSetCalls.front().element == 0,
		"capacity application must notify the GameRules network state through the validated chain" );
	Require( g_spectatorMapEndCallback != nullptr, "the module must own a map-generation callback while enabled" );

	// A malformed external write is not this module's last integer. Shutdown
	// must relinquish it instead of restoring over that other value.
	g_spectatorConVar = "server-owned-value";
	(void)Update( true, true );
	Require( Shutdown(), "a changed non-integer ConVar should not block safe cleanup" );
	Require( g_spectatorConVar == "server-owned-value" && g_rulesSpectatorLimit == 0,
		"cleanup restores owned GameRules state but preserves the external non-integer ConVar" );
	Require( g_schemaSetCalls.size() == 2 && g_schemaSetCalls.back().value == 0 &&
		g_schemaSetCalls.back().changeState && g_schemaSetCalls.back().element == 0,
		"owned GameRules restoration must also notify through the validated network chain" );
	Require( g_spectatorMapEndCallback == nullptr, "shutdown unregisters the map callback" );

	ResetSpectatorModule();
	Require( Update( true, true ).available, "spectator capacity should initialize before a level reload" );
	const auto mapEnd = g_spectatorMapEndCallback;
	Require( mapEnd != nullptr, "map-end callback should be available for the generation boundary" );
	mapEnd();
	void* const nextRules = reinterpret_cast<void*>( 0x2 );
	g_currentRules = nextRules;
	g_rulesSpectatorLimit = 4;
	Require( Update( true, true ).available && g_rulesSpectatorLimit == 12,
		"the same map name's new GameRules instance must receive the current maxplayers limit" );
	Require( Shutdown() && g_rulesSpectatorLimit == 4 && g_spectatorConVar == "0",
		"unload restores the new generation's own field baseline and the original ConVar" );
}

void TestSpectatorDisableBetweenMapEndAndMapStartRestoresConVarFirst() {
	using imomoe::server_fixes::spectator_compat::Update;
	ResetSpectatorModule();
	Require( Update( true, true ).available && g_spectatorConVar == "12" && g_rulesSpectatorLimit == 12,
		"spectator ownership must be established before the map transition" );
	const auto mapEnd = g_spectatorMapEndCallback;
	Require( mapEnd != nullptr, "the enabled module must register its map-end lease reset callback" );
	const auto conVarWritesBeforeMapEnd = g_spectatorConVarWrites;
	const auto schemaOffsetReadsBeforeMapEnd = g_schemaOffsetReadCalls;
	mapEnd();
	Require( !imomoe::server_fixes::spectator_compat::g_gameRulesLease.captured,
		"map end must discard the prior GameRules generation before any new-map restoration" );
	Require( g_spectatorConVar == "0" && g_spectatorConVarWrites == conVarWritesBeforeMapEnd + 1 &&
		g_rulesSpectatorLimit == 12 && g_schemaOffsetReadCalls == schemaOffsetReadsBeforeMapEnd,
		"map end must restore the still-owned raw ConVar without querying or mutating the old GameRules field" );
	const auto writesAfterMapEnd = g_schemaSetCalls.size();
	void* const nextRules = reinterpret_cast<void*>( 0x2 );
	g_currentRules = nextRules;
	// The new native GameRules field is initialized from mp_spectators_max.
	g_rulesSpectatorLimit = std::stoll( g_spectatorConVar );
	const auto disabled = Update( false, false );
	Require( disabled.available && g_spectatorConVar == "0" && g_rulesSpectatorLimit == 0,
		"the first disabled update after native GameRules initialization must leave the prior raw baseline effective" );
	Require( g_schemaSetCalls.size() == writesAfterMapEnd && g_schemaOffsetReadCalls == schemaOffsetReadsBeforeMapEnd &&
		g_spectatorMapEndCallback == nullptr,
		"pre-map-start disable performs no schema query or write and still unregisters the production map callback" );
	Require( Update( false, true ).available && g_spectatorConVar == "0" && g_rulesSpectatorLimit == 0,
		"the next schema-ready update preserves the raw-initialized effective spectator count" );
}

void TestSpectatorExternalYieldSurvivesMapEnd() {
	using namespace imomoe::server_fixes::spectator_compat;
	ResetSpectatorModule();
	Require( Update( true, true ).available && g_spectatorConVar == "12" && g_rulesSpectatorLimit == 12,
		"spectator capacity must be initialized before an external ConVar owner takes over" );
	g_spectatorConVar = "17";
	const auto yielded = Update( true, true );
	Require( yielded.available && g_conVarLease.yielded && !g_conVarLease.ownsCurrent,
		"the external integer write must yield ConVar ownership before map end" );
	const auto writesAfterYield = g_spectatorConVarWrites;
	const auto mapEnd = g_spectatorMapEndCallback;
	Require( mapEnd != nullptr, "the enabled module must have registered its map-end callback" );
	mapEnd();
	Require( g_spectatorConVar == "17" && g_spectatorConVarWrites == writesAfterYield && g_conVarLease.yielded,
		"map end must preserve both the external raw value and the latched yield state" );
	const auto afterMapEnd = Update( true, true );
	Require( afterMapEnd.available &&
		afterMapEnd.reason.find( "mp_spectators_max has an external owner" ) != std::string::npos &&
		g_spectatorConVar == "17" && g_spectatorConVarWrites == writesAfterYield,
		"the same enabled cycle must not reacquire the external ConVar after map end" );
	Require( Update( false, true ).available && g_spectatorConVar == "17",
		"disable after map end must continue to preserve the external ConVar" );
}

void TestSpectatorIntegerConVarOverrideYieldsUntilExplicitOffOn() {
	using imomoe::server_fixes::spectator_compat::Update;
	ResetSpectatorModule();
	Require( Update( true, true ).available && g_spectatorConVar == "12" && g_rulesSpectatorLimit == 12,
		"spectator capacity must be initialized before the external integer override" );
	const auto writesBeforeOverride = g_spectatorConVarWrites;
	g_spectatorConVar = "17";
	const auto yielded = Update( true, true );
	Require( yielded.available && yielded.reason.find( "mp_spectators_max has an external owner" ) != std::string::npos,
		"an integer ConVar override above actual capacity must be reported as external ownership" );
	for ( int update = 0; update < 3; ++update ) {
		Require( Update( true, true ).available, "a sufficient external ConVar value should remain usable across updates" );
	}
	Require( g_spectatorConVar == "17" && g_spectatorConVarWrites == writesBeforeOverride,
		"repeated updates must not overwrite or reacquire an externally changed integer ConVar" );
	Require( Update( false, true ).available && g_spectatorConVar == "17" && g_rulesSpectatorLimit == 0,
		"disable preserves the external ConVar while restoring the still-owned GameRules value" );
	Require( Update( true, true ).available && g_spectatorConVar == "12" && g_rulesSpectatorLimit == 12,
		"an explicit off/on cycle may capture the external integer value and reapply actual maxplayers" );
	Require( Update( false, true ).available && g_spectatorConVar == "17" && g_rulesSpectatorLimit == 0,
		"the next disable restores the newly captured external ConVar baseline" );
}

void TestSpectatorGameRulesIntegerOverrideYieldsUntilExplicitOffOn() {
	using imomoe::server_fixes::spectator_compat::Update;
	ResetSpectatorModule();
	Require( Update( true, true ).available && g_spectatorConVar == "12" && g_rulesSpectatorLimit == 12,
		"spectator capacity must be initialized before the external GameRules override" );
	g_rulesSpectatorLimit = 19;
	const auto yielded = Update( true, true );
	Require( yielded.available && yielded.reason.find( "GameRules spectator capacity has an external owner" ) != std::string::npos,
		"a sufficient effective GameRules override must be reported as external ownership" );
	const auto writesAfterOverride = g_schemaSetCalls.size();
	for ( int update = 0; update < 3; ++update ) {
		Require( Update( true, true ).available, "a sufficient external GameRules value should remain usable across updates" );
	}
	Require( g_rulesSpectatorLimit == 19 && g_schemaSetCalls.size() == writesAfterOverride,
		"repeated updates must not overwrite or reacquire an externally changed effective value" );
	Require( Update( false, true ).available && g_rulesSpectatorLimit == 19 && g_spectatorConVar == "0",
		"disable preserves the external GameRules value while restoring the still-owned ConVar" );
	Require( Update( true, true ).available && g_rulesSpectatorLimit == 12 && g_spectatorConVar == "12",
		"an explicit off/on cycle may recapture the effective value and resynchronize actual maxplayers" );
	Require( Update( false, true ).available && g_rulesSpectatorLimit == 19 && g_spectatorConVar == "0",
		"the next disable restores the newly captured GameRules baseline" );
}

void TestSpectatorRequiresNetworkChainBeforeMutation() {
	using imomoe::server_fixes::spectator_compat::Shutdown;
	using imomoe::server_fixes::spectator_compat::Update;
	ResetSpectatorModule();
	g_spectatorSchemaChainAvailable = false;
	const auto status = Update( true, true );
	Require( !status.available && g_spectatorConVar == "0" && g_rulesSpectatorLimit == 0 &&
		g_schemaSetCalls.empty(),
		"missing GameRules chain metadata must fail closed before any capacity mutation or network notification" );
	Require( Shutdown(), "a schema-availability failure must still release the map callback cleanly" );
}

} // namespace

int main() {
	plg::Log = FakePlugifyLog;
	plg::BeginZone = FakeBeginZone;
	plg::EndZone = FakeEndZone;
	InstallImports();
	ResetFakeEntities();
	g_currentRules = g_rulesObject;
	const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
	g_gameDirectory = std::filesystem::temp_directory_path() /
		( "serverfixes-spawn-callback-root-" + std::to_string( ticks ) );
	TestActualJoinCallbacksRejectStaleQueuedRetry();
	TestActualLifeEventInvalidatesQueuedRetry();
	TestActualJoinCallbacksRetryOneMatchingCapacityFailure();
	TestNegativePlayerHandlesRetainJoinRetryIdentity();
	TestPendingRetryPreservesForeignRawClientCommandVeto();
	TestJointeamUsesGlobalPreAndFailureEventAsRetryBoundary();
	TestAdminCommandCleanupUsesCallbackAbsencePostcondition();
	TestNativeRefreshReadbackFollowsActualTeamAllowance();
	TestTeamMaximumExternalOverrideIsYieldedAndPreserved();
	TestUpdateReportsExternalTeamCapacityAndKeepsEffectiveReadbackCurrent();
	TestManualSpawnRecoversMapWithoutValidNativeSourceAndSkipsInvalidFirst();
	TestNegativeNativeSpawnHandlesAreEnumeratedAndEligible();
	TestCreationDispatchCompletesReverseHandleMapping();
	TestNegativeCreatedSpawnHandlesRemainOwnedThroughRemoval();
	TestDelayedEntityRemovalIsSafeAtShutdown();
	TestReenableWhileExcludedDeletionRemainsEngineOwned();
	TestCapacityRetryDoesNotBypassNativeTeamMaximum();
	TestNativeCapacityReadbackRejectsUnregisteredSpawnEntity();
	TestOccupiedRegisteredSpawnsKeepCapacityAcrossRoundAndRebuild();
	TestProductionManualSaveLoadRoundTrip();
	TestActualSameNameMapCallbacksRebuildWithoutDuplicates();
	TestDisabledModuleSameNameReloadInvalidatesTemplateQualification();
	TestSpectatorExternalNonIntegerAndMapGenerationOwnership();
	TestSpectatorDisableBetweenMapEndAndMapStartRestoresConVarFirst();
	TestSpectatorExternalYieldSurvivesMapEnd();
	TestSpectatorIntegerConVarOverrideYieldsUntilExplicitOffOn();
	TestSpectatorGameRulesIntegerOverrideYieldsUntilExplicitOffOn();
	TestSpectatorRequiresNetworkChainBeforeMutation();
	std::cout << "spawn capacity and spectator callback checks passed\n";
	return EXIT_SUCCESS;
}
