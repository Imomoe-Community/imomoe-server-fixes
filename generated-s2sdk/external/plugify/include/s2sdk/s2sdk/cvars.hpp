#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: cvars)

namespace s2sdk {

  using _FindConVar = uint64_t (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_FindConVar __s2sdk_FindConVar;
namespace s2sdk {
  /**
   * @brief Searches for a console variable.
   * @param name (string): The name of the console variable to search for.
   * @return uint64: A handle to the console variable data if found; otherwise, nullptr.
   */
  inline uint64_t FindConVar(const plg::string& name) {
    return __s2sdk_FindConVar(name);
  }

  using _GetConVarValue = plg::string (*)(uint64_t);
}
extern "C" PLUGIN_API s2sdk::_GetConVarValue __s2sdk_GetConVarValue;
namespace s2sdk {
  /**
   * @brief Retrieves the current value of a console variable and stores it in the output string.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @return string: The output value in string format.
   */
  inline plg::string GetConVarValue(uint64_t conVarHandle) {
    return __s2sdk_GetConVarValue(conVarHandle);
  }

  using _SendConVarValue = void (*)(int32_t, uint64_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_SendConVarValue __s2sdk_SendConVarValue;
namespace s2sdk {
  /**
   * @brief Replicates a console variable value to a specific client. This does not change the actual console variable value.
   * @param playerSlot (int32): The index of the client to replicate the value to.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @param value (string): The value to send to the client.
   */
  inline void SendConVarValue(int32_t playerSlot, uint64_t conVarHandle, const plg::string& value) {
    return __s2sdk_SendConVarValue(playerSlot, conVarHandle, value);
  }

  using _SetConVarValue = void (*)(uint64_t, const plg::string&, bool, bool);
}
extern "C" PLUGIN_API s2sdk::_SetConVarValue __s2sdk_SetConVarValue;
namespace s2sdk {
  /**
   * @brief Sets the value of a console variable.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @param value (string): The string value to set for the console variable.
   * @param replicate (bool): If set to true, the new convar value will be set on all clients. This will only work if the convar has the FCVAR_REPLICATED flag and actually exists on clients.
   * @param notify (bool): If set to true, clients will be notified that the convar has changed. This will only work if the convar has the FCVAR_NOTIFY flag.
   */
  inline void SetConVarValue(uint64_t conVarHandle, const plg::string& value, bool replicate, bool notify) {
    return __s2sdk_SetConVarValue(conVarHandle, value, replicate, notify);
  }

  using _CreateConVarBool = uint64_t (*)(const plg::string&, bool, const plg::string&, ConVarFlag, bool, bool, bool, bool);
}
extern "C" PLUGIN_API s2sdk::_CreateConVarBool __s2sdk_CreateConVarBool;
namespace s2sdk {
  /**
   * @brief Creates a new boolean console variable.
   * @param name (string): The name of the console variable.
   * @param defaultValue (bool): The default value for the console variable.
   * @param description (string): A brief description of the console variable.
   * @param flags (int64): Flags that define the behavior of the console variable.
   * @param hasMin (bool): Indicates if a minimum value is provided.
   * @param min (bool): The minimum value if hasMin is true.
   * @param hasMax (bool): Indicates if a maximum value is provided.
   * @param max (bool): The maximum value if hasMax is true.
   * @return uint64: A handle to the created console variable data.
   */
  inline uint64_t CreateConVarBool(const plg::string& name, bool defaultValue, const plg::string& description, ConVarFlag flags, bool hasMin, bool min, bool hasMax, bool max) {
    return __s2sdk_CreateConVarBool(name, defaultValue, description, flags, hasMin, min, hasMax, max);
  }

} // namespace s2sdk
