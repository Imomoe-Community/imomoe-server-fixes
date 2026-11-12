#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: cvars)

namespace s2sdk {

  using _GetConVarFloat = float (*)(uint64_t);
}
extern "C" PLUGIN_API s2sdk::_GetConVarFloat __s2sdk_GetConVarFloat;
namespace s2sdk {
  /**
   * @brief Retrieves the current value of a float console variable.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @return float: The current float value of the console variable.
   */
  inline float GetConVarFloat(uint64_t conVarHandle) {
    return __s2sdk_GetConVarFloat(conVarHandle);
  }

  using _SetConVarFloat = void (*)(uint64_t, float, bool, bool);
}
extern "C" PLUGIN_API s2sdk::_SetConVarFloat __s2sdk_SetConVarFloat;
namespace s2sdk {
  /**
   * @brief Sets the value of a floating-point console variable.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @param value (float): The value to set for the console variable.
   * @param replicate (bool): If set to true, the new convar value will be set on all clients. This will only work if the convar has the FCVAR_REPLICATED flag and actually exists on clients.
   * @param notify (bool): If set to true, clients will be notified that the convar has changed. This will only work if the convar has the FCVAR_NOTIFY flag.
   */
  inline void SetConVarFloat(uint64_t conVarHandle, float value, bool replicate, bool notify) {
    return __s2sdk_SetConVarFloat(conVarHandle, value, replicate, notify);
  }

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

  using _CreateConVarInt32 = uint64_t (*)(const plg::string&, int32_t, const plg::string&, ConVarFlag, bool, int32_t, bool, int32_t);
}
extern "C" PLUGIN_API s2sdk::_CreateConVarInt32 __s2sdk_CreateConVarInt32;
namespace s2sdk {
  /**
   * @brief Creates a new 32-bit signed integer console variable.
   * @param name (string): The name of the console variable.
   * @param defaultValue (int32): The default value for the console variable.
   * @param description (string): A brief description of the console variable.
   * @param flags (int64): Flags that define the behavior of the console variable.
   * @param hasMin (bool): Indicates if a minimum value is provided.
   * @param min (int32): The minimum value if hasMin is true.
   * @param hasMax (bool): Indicates if a maximum value is provided.
   * @param max (int32): The maximum value if hasMax is true.
   * @return uint64: A handle to the created console variable data.
   */
  inline uint64_t CreateConVarInt32(const plg::string& name, int32_t defaultValue, const plg::string& description, ConVarFlag flags, bool hasMin, int32_t min, bool hasMax, int32_t max) {
    return __s2sdk_CreateConVarInt32(name, defaultValue, description, flags, hasMin, min, hasMax, max);
  }

  using _GetConVarBool = bool (*)(uint64_t);
}
extern "C" PLUGIN_API s2sdk::_GetConVarBool __s2sdk_GetConVarBool;
namespace s2sdk {
  /**
   * @brief Retrieves the current value of a boolean console variable.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @return bool: The current boolean value of the console variable.
   */
  inline bool GetConVarBool(uint64_t conVarHandle) {
    return __s2sdk_GetConVarBool(conVarHandle);
  }

  using _GetConVarInt32 = int32_t (*)(uint64_t);
}
extern "C" PLUGIN_API s2sdk::_GetConVarInt32 __s2sdk_GetConVarInt32;
namespace s2sdk {
  /**
   * @brief Retrieves the current value of a signed 32-bit integer console variable.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @return int32: The current int32_t value of the console variable.
   */
  inline int32_t GetConVarInt32(uint64_t conVarHandle) {
    return __s2sdk_GetConVarInt32(conVarHandle);
  }

  using _HookConVarChange = bool (*)(uint64_t, ConVarCallback);
}
extern "C" PLUGIN_API s2sdk::_HookConVarChange __s2sdk_HookConVarChange;
namespace s2sdk {
  /**
   * @brief Creates a hook for when a console variable's value is changed.
   * @param conVarHandle (uint64): TThe handle to the console variable data.
   * @param callback (function): The callback function to be executed when the variable's value changes.
   * @return bool: True if the callback was registered, false if the convar is invalid or the callback is already registered.
   */
  inline bool HookConVarChange(uint64_t conVarHandle, ConVarCallback callback) {
    return __s2sdk_HookConVarChange(conVarHandle, callback);
  }

  using _UnhookConVarChange = bool (*)(uint64_t, ConVarCallback);
}
extern "C" PLUGIN_API s2sdk::_UnhookConVarChange __s2sdk_UnhookConVarChange;
namespace s2sdk {
  /**
   * @brief Removes a hook for when a console variable's value is changed.
   * @param conVarHandle (uint64): The handle to the console variable data.
   * @param callback (function): The callback function to be removed.
   * @return bool: True if the callback was removed, false if the convar is invalid or the callback was not registered.
   */
  inline bool UnhookConVarChange(uint64_t conVarHandle, ConVarCallback callback) {
    return __s2sdk_UnhookConVarChange(conVarHandle, callback);
  }

} // namespace s2sdk
