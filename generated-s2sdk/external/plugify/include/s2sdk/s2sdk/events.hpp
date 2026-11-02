#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: events)

namespace s2sdk {

  using _HookEvent = EventHookError (*)(const plg::string&, EventCallback, HookMode);
}
extern "C" PLUGIN_API s2sdk::_HookEvent __s2sdk_HookEvent;
namespace s2sdk {
  /**
   * @brief Creates a hook for when a game event is fired.
   * @param name (string): The name of the event to hook.
   * @param callback (function): The callback function to call when the event is fired.
   * @param type (uint8): Whether the hook was in post mode (after processing) or pre mode (before processing).
   * @return int32: An integer indicating the result of the hook operation.
   */
  inline EventHookError HookEvent(const plg::string& name, EventCallback callback, HookMode type) {
    return __s2sdk_HookEvent(name, callback, type);
  }

  using _UnhookEvent = EventHookError (*)(const plg::string&, EventCallback, HookMode);
}
extern "C" PLUGIN_API s2sdk::_UnhookEvent __s2sdk_UnhookEvent;
namespace s2sdk {
  /**
   * @brief Removes a hook for when a game event is fired.
   * @param name (string): The name of the event to unhook.
   * @param callback (function): The callback function to remove.
   * @param type (uint8): Whether the hook was in post mode (after processing) or pre mode (before processing).
   * @return int32: An integer indicating the result of the unhook operation.
   */
  inline EventHookError UnhookEvent(const plg::string& name, EventCallback callback, HookMode type) {
    return __s2sdk_UnhookEvent(name, callback, type);
  }

  using _GetEventBool = bool (*)(void*, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetEventBool __s2sdk_GetEventBool;
namespace s2sdk {
  /**
   * @brief Retrieves the boolean value of a game event's key.
   * @param event (ptr64): A pointer to the IGameEvent object containing event data.
   * @param key (string): The key for which to retrieve the boolean value.
   * @return bool: The boolean value associated with the key.
   */
  inline bool GetEventBool(void* event, const plg::string& key) {
    return __s2sdk_GetEventBool(event, key);
  }

  using _GetEventInt = int32_t (*)(void*, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetEventInt __s2sdk_GetEventInt;
namespace s2sdk {
  /**
   * @brief Retrieves the integer value of a game event's key.
   * @param event (ptr64): A pointer to the IGameEvent object containing event data.
   * @param key (string): The key for which to retrieve the integer value.
   * @return int32: The integer value associated with the key.
   */
  inline int32_t GetEventInt(void* event, const plg::string& key) {
    return __s2sdk_GetEventInt(event, key);
  }

  using _GetEventPlayerSlot = int32_t (*)(void*, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetEventPlayerSlot __s2sdk_GetEventPlayerSlot;
namespace s2sdk {
  /**
   * @brief Retrieves the player slot of a game event's key.
   * @param event (ptr64): A pointer to the IGameEvent object containing event data.
   * @param key (string): The key for which to retrieve the player index.
   * @return int32: The player slot associated with the key.
   */
  inline int32_t GetEventPlayerSlot(void* event, const plg::string& key) {
    return __s2sdk_GetEventPlayerSlot(event, key);
  }

  using _GetEventString = plg::string (*)(void*, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetEventString __s2sdk_GetEventString;
namespace s2sdk {
  /**
   * @brief Retrieves the string value of a game event's key.
   * @param event (ptr64): A pointer to the IGameEvent object containing event data.
   * @param key (string): The key for which to retrieve the string value.
   * @return string: A string where the result will be stored.
   */
  inline plg::string GetEventString(void* event, const plg::string& key) {
    return __s2sdk_GetEventString(event, key);
  }

  using _SetEventString = void (*)(void*, const plg::string&, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_SetEventString __s2sdk_SetEventString;
namespace s2sdk {
  /**
   * @brief Sets the string value of a game event's key.
   * @param event (ptr64): A pointer to the IGameEvent object containing event data.
   * @param key (string): The key for which to set the string value.
   * @param value (string): The string value to set.
   */
  inline void SetEventString(void* event, const plg::string& key, const plg::string& value) {
    return __s2sdk_SetEventString(event, key, value);
  }

  using _GetEventFloat = float (*)(void*, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetEventFloat __s2sdk_GetEventFloat;
namespace s2sdk {
  /**
   * @brief Retrieves the float value of a game event's key.
   * @param event (ptr64): A pointer to the IGameEvent object containing event data.
   * @param key (string): The key for which to retrieve the float value.
   * @return float: The float value associated with the key.
   */
  inline float GetEventFloat(void* event, const plg::string& key) {
    return __s2sdk_GetEventFloat(event, key);
  }

} // namespace s2sdk
