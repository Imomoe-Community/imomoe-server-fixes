#pragma once

#include "enums.hpp"
#include "aliases.hpp"
// Generated from s2sdk.pplugin

namespace s2sdk {

  // Handles the execution of a command triggered by a caller. This function processes the command, interprets its context, and handles any provided arguments.
  using ConCommandCallback = ResultType (*)(int32_t, ConCommandContext, const plg::vector<plg::string>&);


  // Handles events triggered by the game event system. This function processes the event data, determines the necessary action, and optionally prevents event broadcasting.
  using EventCallback = ResultType (*)(const plg::string&, void*, bool);


  // Callback function for user messages.
  using UserMessageCallback = ResultType (*)(void*);


  // Called when a client is disconnecting from the server.
  using OnClientDisconnectCallback = void (*)(int32_t, NetworkDisconnectionReason);


  // Called before server activation to build game session manifest.
  using OnBuildGameSessionManifestCallback = void (*)();


  // Called on every map start.
  using OnMapStartCallback = void (*)();


  // Called on every map end.
  using OnMapEndCallback = void (*)();


  // Callback function for voice packets sent by clients. Runs after the Speak_Muted check, so packets of muted players don't reach it.
  using ClientVoiceDataHookCallback = ResultType (*)(int32_t, void*);


  // Handles changes to a console variable's value. This function is called whenever the value of a specific console variable is modified.
  using ConVarCallback = void (*)(uint64_t, const plg::string&, const plg::string&);


  // Callback function for net messages sent to a single client. Runs before OnSendNetMessage, blocked messages don't reach it.
  using SendNetMessageHookCallback = ResultType (*)(int32_t, void*);


} // namespace s2sdk
