#pragma once

#include <cstdint>
// Generated from s2sdk.pplugin

namespace s2sdk {

  // Enum representing various flags for ConVars and ConCommands.
  enum class ConVarFlag : int64_t {
    None = 0, // The default, no flags at all.
    LinkedConcommand = 1, // Linked to a ConCommand.
    DevelopmentOnly = 2, // Hidden in released products. Automatically removed if ALLOW_DEVELOPMENT_CVARS is defined.
    GameDll = 4, // Defined by the game DLL.
    ClientDll = 8, // Defined by the client DLL.
    Hidden = 16, // Hidden. Doesn't appear in find or auto-complete. Like DEVELOPMENTONLY but cannot be compiled out.
    Protected = 32, // Server cvar; data is not sent since it's sensitive (e.g., passwords).
    SpOnly = 64, // This cvar cannot be changed by clients connected to a multiplayer server.
    Archive = 128, // Saved to vars.rc.
    Notify = 256, // Notifies players when changed.
    UserInfo = 512, // Changes the client's info string.
    Missing0 = 1024, // Hides the cvar from lookups.
    Unlogged = 2048, // If this is a server cvar, changes are not logged to the file or console.
    Missing1 = 4096, // Hides the cvar from lookups.
    Replicated = 8192, // Server-enforced setting on clients.
    Cheat = 16384, // Only usable in singleplayer/debug or multiplayer with sv_cheats.
    PerUser = 32768, // Causes auto-generated varnameN for splitscreen slots.
    Demo = 65536, // Records this cvar when starting a demo file.
    DontRecord = 131072, // Excluded from demo files.
    Missing2 = 262144, // Reserved for future use.
    Release = 524288, // Cvars tagged with this are available to customers.
    MenuBarItem = 1048576, // Marks the cvar as a menu bar item.
    Missing3 = 2097152, // Reserved for future use.
    NotConnected = 4194304, // Cannot be changed by a client connected to a server.
    VconsoleFuzzyMatching = 8388608, // Enables fuzzy matching for vconsole.
    ServerCanExecute = 16777216, // The server can execute this command on clients.
    ClientCanExecute = 33554432, // Allows clients to execute this command.
    ServerCannotQuery = 67108864, // The server cannot query this cvar's value.
    VconsoleSetFocus = 134217728, // Sets focus in the vconsole.
    ClientCmdCanExecute = 268435456, // IVEngineClient::ClientCmd can execute this command.
    ExecutePerTick = 536870912, // Executes the cvar every tick.
  };

  // Enum representing the possible results of an operation.
  enum class ResultType : int32_t {
    Continue = 0, // The action continues to be processed without interruption.
    Changed = 1, // Indicates that the action has altered the state or behavior during execution.
    Handled = 2, // The action has been successfully handled, and no further action is required.
    Stop = 3, // The action processing is halted, and no further steps will be executed.
  };

  // The command execution context.
  enum class ConCommandContext : int32_t {
    Console = 0, // The command execute from the client's console.
    Chat = 1, // The command execute from the client's chat.
  };

  // Enum representing the type of callback.
  enum class HookMode : uint8_t {
    Pre = 0, // Callback will be executed before the original function
    Post = 1, // Callback will be executed after the original function
  };

  // Enum representing the type of callback.
  enum class EventHookError : int32_t {
    Okay = 0, // Indicates that the event hook was successfully created.
    InvalidEvent = 1, // Indicates that the event name provided is invalid or does not exist.
    NotActive = 2, // Indicates that the event system is not currently active or initialized.
    InvalidCallback = 3, // Indicates that the callback function provided is invalid or not compatible with the event system.
  };

  // Enum representing the possible teams in Counter-Strike.
  enum class CSTeam : int32_t {
    None = 0, // No team.
    Spectator = 1, // Spectator team.
    T = 2, // Terrorist team.
    CT = 3, // Counter-Terrorist team.
  };

  // Enum representing the possible reasons for a round ending in Counter-Strike.
  enum class CSRoundEndReason : int32_t {
    TargetBombed = 1, // Target successfully bombed.
    VIPEscaped = 2, // The VIP has escaped (not present in CS:GO).
    VIPKilled = 3, // VIP has been assassinated (not present in CS:GO).
    TerroristsEscaped = 4, // The terrorists have escaped.
    CTStoppedEscape = 5, // The CTs have prevented most of the terrorists from escaping.
    TerroristsStopped = 6, // Escaping terrorists have all been neutralized.
    BombDefused = 7, // The bomb has been defused.
    CTWin = 8, // Counter-Terrorists win.
    TerroristWin = 9, // Terrorists win.
    Draw = 10, // Round draw.
    HostagesRescued = 11, // All hostages have been rescued.
    TargetSaved = 12, // Target has been saved.
    HostagesNotRescued = 13, // Hostages have not been rescued.
    TerroristsNotEscaped = 14, // Terrorists have not escaped.
    VIPNotEscaped = 15, // VIP has not escaped (not present in CS:GO).
    GameStart = 16, // Game commencing.
    TerroristsSurrender = 17, // Terrorists surrender.
    CTSurrender = 18, // CTs surrender.
    TerroristsPlanted = 19, // Terrorists planted the bomb.
    CTsReachedHostage = 20, // CTs reached the hostage.
    SurvivalWin = 21, // Survival mode win.
    SurvivalDraw = 22, // Survival mode draw.
  };

  // Enum representing different weapon definition indices.
  enum class WeaponDefIndex : uint16_t {
    Invalid = 0,
    Deagle = 1,
    Elite = 2,
    FiveSeven = 3,
    Glock = 4,
    AK47 = 7,
    AUG = 8,
    AWP = 9,
    FAMAS = 10,
    G3SG1 = 11,
    GalilAR = 13,
    M249 = 14,
    M4A1 = 16,
    MAC10 = 17,
    P90 = 19,
    MP5SD = 23,
    UMP45 = 24,
    XM1014 = 25,
    Bizon = 26,
    MAG7 = 27,
    Negev = 28,
    SawedOff = 29,
    Tec9 = 30,
    Taser = 31,
    HKP2000 = 32,
    MP7 = 33,
    MP9 = 34,
    Nova = 35,
    P250 = 36,
    SCAR20 = 38,
    SG556 = 39,
    SSG08 = 40,
    KnifeGG = 41,
    Knife = 42,
    Flashbang = 43,
    HEGrenade = 44,
    SmokeGrenade = 45,
    Molotov = 46,
    Decoy = 47,
    IncGrenade = 48,
    C4 = 49,
    Kevlar = 50,
    AssaultSuit = 51,
    HeavyAssaultSuit = 52,
    Defuser = 55,
    KnifeT = 59,
    M4A1Silencer = 60,
    USPSilencer = 61,
    CZ75A = 63,
    Revolver = 64,
    Bayonet = 500,
    KnifeCSS = 503,
    KnifeFlip = 505,
    KnifeGut = 506,
    KnifeKarambit = 507,
    KnifeM9Bayonet = 508,
    KnifeTactical = 509,
    KnifeFalchion = 512,
    KnifeBowie = 514,
    KnifeButterfly = 515,
    KnifePush = 516,
    KnifeCord = 517,
    KnifeCanis = 518,
    KnifeUrsus = 519,
    KnifeGypsyJackknife = 520,
    KnifeOutdoor = 521,
    KnifeStiletto = 522,
    KnifeWidowmaker = 523,
    KnifeSkeleton = 525,
    KnifeKukri = 526
  };

  // Enum representing various movement types for entities.
  enum class MoveType : int32_t {
    None = 0, // Never moves.
    Isometric = 1, // Previously isometric movement type.
    Walk = 2, // Player only - moving on the ground.
    Fly = 3, // No gravity, but still collides with stuff.
    Flygravity = 4, // Flies through the air and is affected by gravity.
    Vphysics = 5, // Uses VPHYSICS for simulation.
    Push = 6, // No clip to world, push and crush.
    Noclip = 7, // No gravity, no collisions, still has velocity/avelocity.
    Ladder = 8, // Used by players only when going onto a ladder.
    Observer = 9, // Observer movement, depends on player's observer mode.
    Custom = 10, // Allows the entity to describe its own physics.
  };

  // Represents the possible types of data that can be passed as a value in input actions.
  enum class FieldType : int32_t {
    Auto = 0, // Automatically detect the type of the value.
    Float32 = 1, // A 32-bit floating-point number.
    Float64 = 2, // A 64-bit floating-point number.
    Int32 = 3, // A 32-bit signed integer.
    UInt32 = 4, // A 32-bit unsigned integer.
    Int64 = 5, // A 64-bit signed integer.
    UInt64 = 6, // A 64-bit unsigned integer.
    Boolean = 7, // A boolean value (true or false).
    Character = 8, // A single character.
    String = 9, // A managed string object.
    CString = 10, // A null-terminated C-style string.
    HScript = 11, // A script handle, typically for scripting integration.
    EHandle = 12, // An entity handle, used to reference an entity within the system.
    Resource = 13, // A resource handle, such as a file or asset reference.
    Vector3d = 14, // A 3D vector, typically representing position or direction.
    Vector2d = 15, // A 2D vector, for planar data or coordinates.
    Vector4d = 16, // A 4D vector, often used for advanced mathematical representations.
    Color32 = 17, // A 32-bit color value (RGBA).
    QAngle = 18, // A quaternion-based angle representation.
    Quaternion = 19, // A quaternion, used for rotation and orientation calculations.
  };

  // Enum representing reasons for network disconnection.
  enum class NetworkDisconnectionReason : int32_t {
    Invalid = 0, // Invalid.
    Shutdown = 1, // Shutdown.
    DisconnectByUser = 2, // Disconnect by user.
    DisconnectByServer = 3, // Disconnect by server.
    Lost = 4, // Lost.
    Overflow = 5, // Overflow.
    SteamBanned = 6, // Steam banned.
    SteamInuse = 7, // Steam inuse.
    SteamTicket = 8, // Steam ticket.
    SteamLogon = 9, // Steam logon.
    SteamAuthcancelled = 10, // Steam authcancelled.
    SteamAuthalreadyused = 11, // Steam authalreadyused.
    SteamAuthinvalid = 12, // Steam authinvalid.
    SteamVacbanstate = 13, // Steam vacbanstate.
    SteamLoggedInElsewhere = 14, // Steam logged in elsewhere.
    SteamVacCheckTimedout = 15, // Steam vac check timedout.
    SteamDropped = 16, // Steam dropped.
    SteamOwnership = 17, // Steam ownership.
    ServerinfoOverflow = 18, // Serverinfo overflow.
    TickmsgOverflow = 19, // Tickmsg overflow.
    StringtablemsgOverflow = 20, // Stringtablemsg overflow.
    DeltaentmsgOverflow = 21, // Deltaentmsg overflow.
    TempentmsgOverflow = 22, // Tempentmsg overflow.
    SoundsmsgOverflow = 23, // Soundsmsg overflow.
    Snapshotoverflow = 24, // Snapshotoverflow.
    Snapshoterror = 25, // Snapshoterror.
    Reliableoverflow = 26, // Reliableoverflow.
    Baddeltatick = 27, // Baddeltatick.
    Nomoresplits = 28, // Nomoresplits.
    Timedout = 29, // Timedout.
    Disconnected = 30, // Disconnected.
    Leavingsplit = 31, // Leavingsplit.
    Differentclasstables = 32, // Differentclasstables.
    Badrelaypassword = 33, // Badrelaypassword.
    Badspectatorpassword = 34, // Badspectatorpassword.
    Hltvrestricted = 35, // Hltvrestricted.
    Nospectators = 36, // Nospectators.
    Hltvunavailable = 37, // Hltvunavailable.
    Hltvstop = 38, // Hltvstop.
    Kicked = 39, // Kicked.
    Banadded = 40, // Banadded.
    Kickbanadded = 41, // Kickbanadded.
    Hltvdirect = 42, // Hltvdirect.
    PureserverClientextra = 43, // Pureserver clientextra.
    PureserverMismatch = 44, // Pureserver mismatch.
    Usercmd = 45, // Usercmd.
    RejectedByGame = 46, // Rejected by game.
    MessageParseError = 47, // Message parse error.
    InvalidMessageError = 48, // Invalid message error.
    BadServerPassword = 49, // Bad server password.
    DirectConnectReservation = 50, // Direct connect reservation.
    ConnectionFailure = 51, // Connection failure.
    NoPeerGroupHandlers = 52, // No peer group handlers.
    Reconnection = 53, // Reconnection.
    Loopshutdown = 54, // Loopshutdown.
    Loopdeactivate = 55, // Loopdeactivate.
    HostEndgame = 56, // Host endgame.
    LoopLevelloadActivate = 57, // Loop levelload activate.
    CreateServerFailed = 58, // Create server failed.
    Exiting = 59, // Exiting.
    RequestHoststateIdle = 60, // Request hoststate idle.
    RequestHoststateHltvrelay = 61, // Request hoststate hltvrelay.
    ClientConsistencyFail = 62, // Client consistency fail.
    ClientUnableToCrcMap = 63, // Client unable to crc map.
    ClientNoMap = 64, // Client no map.
    ClientDifferentMap = 65, // Client different map.
    ServerRequiresSteam = 66, // Server requires steam.
    SteamDenyMisc = 67, // Steam deny misc.
    SteamDenyBadAntiCheat = 68, // Steam deny bad anti cheat.
    ServerShutdown = 69, // Server shutdown.
    ReplayIncompatible = 71, // Replay incompatible.
    ConnectRequestTimedout = 72, // Connect request timedout.
    ServerIncompatible = 73, // Server incompatible.
    LocalproblemManyrelays = 74, // Localproblem manyrelays.
    LocalproblemHostedserverprimaryrelay = 75, // Localproblem hostedserverprimaryrelay.
    LocalproblemNetworkconfig = 76, // Localproblem networkconfig.
    LocalproblemOther = 77, // Localproblem other.
    RemoteTimeout = 79, // Remote timeout.
    RemoteTimeoutConnecting = 80, // Remote timeout connecting.
    RemoteOther = 81, // Remote other.
    RemoteBadcrypt = 82, // Remote badcrypt.
    RemoteCertnottrusted = 83, // Remote certnottrusted.
    Unusual = 84, // Unusual.
    InternalError = 85, // Internal error.
    RejectBadchallenge = 128, // Reject badchallenge.
    RejectNolobby = 129, // Reject nolobby.
    RejectBackgroundMap = 130, // Reject background map.
    RejectSinglePlayer = 131, // Reject single player.
    RejectHiddenGame = 132, // Reject hidden game.
    RejectLanrestrict = 133, // Reject lanrestrict.
    RejectBadpassword = 134, // Reject badpassword.
    RejectServerfull = 135, // Reject serverfull.
    RejectInvalidreservation = 136, // Reject invalidreservation.
    RejectFailedchannel = 137, // Reject failedchannel.
    RejectConnectFromLobby = 138, // Reject connect from lobby.
    RejectReservedForLobby = 139, // Reject reserved for lobby.
    RejectInvalidkeylength = 140, // Reject invalidkeylength.
    RejectOldprotocol = 141, // Reject oldprotocol.
    RejectNewprotocol = 142, // Reject newprotocol.
    RejectInvalidconnection = 143, // Reject invalidconnection.
    RejectInvalidcertlen = 144, // Reject invalidcertlen.
    RejectInvalidsteamcertlen = 145, // Reject invalidsteamcertlen.
    RejectSteam = 146, // Reject steam.
    RejectServerauthdisabled = 147, // Reject serverauthdisabled.
    RejectServercdkeyauthinvalid = 148, // Reject servercdkeyauthinvalid.
    RejectBanned = 149, // Reject banned.
    KickedTeamkilling = 150, // Kicked teamkilling.
    KickedTkStart = 151, // Kicked tk start.
    KickedUntrustedaccount = 152, // Kicked untrustedaccount.
    KickedConvictedaccount = 153, // Kicked convictedaccount.
    KickedCompetitivecooldown = 154, // Kicked competitivecooldown.
    KickedTeamhurting = 155, // Kicked teamhurting.
    KickedHostagekilling = 156, // Kicked hostagekilling.
    KickedVotedoff = 157, // Kicked votedoff.
    KickedIdle = 158, // Kicked idle.
    KickedSuicide = 159, // Kicked suicide.
    KickedNosteamlogin = 160, // Kicked nosteamlogin.
    KickedNosteamticket = 161, // Kicked nosteamticket.
    KickedInputautomation = 162, // Kicked inputautomation.
    KickedVacnetabnormalbehavior = 163, // Kicked vacnetabnormalbehavior.
    KickedInsecureclient = 164, // Kicked insecureclient.
  };


  /// Ownership type for RAII wrappers
  enum class Ownership : bool { Borrowed, Owned };

} // namespace s2sdk
