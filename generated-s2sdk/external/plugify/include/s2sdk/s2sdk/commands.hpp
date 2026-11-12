#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: commands)

namespace s2sdk {

  using _ServerCommandEx = plg::string (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_ServerCommandEx __s2sdk_ServerCommandEx;
namespace s2sdk {
  /**
   * @brief Executes a server command right away as if it were on the server console (or RCON) and returns the printed text. Console commands and convars only: commands are split on ';' and new lines, aliases are not expanded.
   * @param command (string): The command to execute on the server.
   * @return string: String to store command result into.
   */
  inline plg::string ServerCommandEx(const plg::string& command) {
    return __s2sdk_ServerCommandEx(command);
  }

  using _AddConsoleCommand = bool (*)(const plg::string&, const plg::string&, ConVarFlag, ConCommandCallback, HookMode);
}
extern "C" PLUGIN_API s2sdk::_AddConsoleCommand __s2sdk_AddConsoleCommand;
namespace s2sdk {
  /**
   * @brief Creates a console command or hooks an already existing one.
   * @param name (string): The name of the console command.
   * @param description (string): A brief description of what the command does.
   * @param flags (int64): Command flags that define the behavior of the command.
   * @param callback (function): A callback function that is invoked when the command is executed.
   * @param type (uint8): Whether the hook was in post mode (after processing) or pre mode (before processing).
   * @return bool: true if the command was successfully created; otherwise, false.
   */
  inline bool AddConsoleCommand(const plg::string& name, const plg::string& description, ConVarFlag flags, ConCommandCallback callback, HookMode type) {
    return __s2sdk_AddConsoleCommand(name, description, flags, callback, type);
  }

  using _RemoveCommand = bool (*)(const plg::string&, ConCommandCallback);
}
extern "C" PLUGIN_API s2sdk::_RemoveCommand __s2sdk_RemoveCommand;
namespace s2sdk {
  /**
   * @brief Removes a console command from the system.
   * @param name (string): The name of the command to be removed.
   * @param callback (function): The callback function associated with the command to be removed.
   * @return bool: true if the command was successfully removed; otherwise, false.
   */
  inline bool RemoveCommand(const plg::string& name, ConCommandCallback callback) {
    return __s2sdk_RemoveCommand(name, callback);
  }

  using _GetAllConCommands = plg::vector<plg::string> (*)(ConVarFlag);
}
extern "C" PLUGIN_API s2sdk::_GetAllConCommands __s2sdk_GetAllConCommands;
namespace s2sdk {
  /**
   * @brief  Returns the names of all registered console commands and cvars.
   * @param flags (int64): Additional flags for the console variable.
   * @return string[]: The vector of command/cvar names.
   */
  inline plg::vector<plg::string> GetAllConCommands(ConVarFlag flags) {
    return __s2sdk_GetAllConCommands(flags);
  }

  using _RemoveCommandListener = bool (*)(const plg::string&, ConCommandCallback, HookMode);
}
extern "C" PLUGIN_API s2sdk::_RemoveCommandListener __s2sdk_RemoveCommandListener;
namespace s2sdk {
  /**
   * @brief Removes a callback that fires when a command is sent to the server.
   * @param name (string): The name of the command.
   * @param callback (function): The callback function to be removed.
   * @param type (uint8): Whether the hook was in post mode (after processing) or pre mode (before processing).
   * @return bool: Returns true if the callback was successfully removed, false otherwise.
   */
  inline bool RemoveCommandListener(const plg::string& name, ConCommandCallback callback, HookMode type) {
    return __s2sdk_RemoveCommandListener(name, callback, type);
  }

  using _HasConsoleCommandCallbacks = bool (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_HasConsoleCommandCallbacks __s2sdk_HasConsoleCommandCallbacks;
namespace s2sdk {
  /**
   * @brief Local compatibility bridge: checks live callbacks, not retained command names.
   * @param name (string): Command name.
   * @return bool: Whether the engine command has callbacks registered.
   */
  inline bool HasConsoleCommandCallbacks(const plg::string& name) {
    return __s2sdk_HasConsoleCommandCallbacks(name);
  }

  using _AddCommandListener = bool (*)(const plg::string&, ConCommandCallback, HookMode);
}
extern "C" PLUGIN_API s2sdk::_AddCommandListener __s2sdk_AddCommandListener;
namespace s2sdk {
  /**
   * @brief Adds a callback that will fire when a command is sent to the server.
   * @param name (string): The name of the command.
   * @param callback (function): The callback function that will be invoked when the command is executed.
   * @param type (uint8): Whether the hook was in post mode (after processing) or pre mode (before processing).
   * @return bool: Returns true if the callback was successfully added, false otherwise.
   */
  inline bool AddCommandListener(const plg::string& name, ConCommandCallback callback, HookMode type) {
    return __s2sdk_AddCommandListener(name, callback, type);
  }

  using _FakeClientCommand = void (*)(int32_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_FakeClientCommand __s2sdk_FakeClientCommand;
namespace s2sdk {
  /**
   * @brief Executes a client command on the server without network communication.
   * @param playerSlot (int32): The index of the client.
   * @param command (string): The command to be executed by the client.
   */
  inline void FakeClientCommand(int32_t playerSlot, const plg::string& command) {
    return __s2sdk_FakeClientCommand(playerSlot, command);
  }

} // namespace s2sdk
