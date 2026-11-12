#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: protobuf)

namespace s2sdk {

  using _UserMessageCreateFromId = void* (*)(int16_t);
}
extern "C" PLUGIN_API s2sdk::_UserMessageCreateFromId __s2sdk_UserMessageCreateFromId;
namespace s2sdk {
  /**
   * @brief Creates a UserMessage from a message ID.
   * @param messageId (int16): The ID of the message.
   * @return ptr64: A pointer to the newly created UserMessage.
   */
  inline void* UserMessageCreateFromId(int16_t messageId) {
    return __s2sdk_UserMessageCreateFromId(messageId);
  }

  using _UserMessageDestroy = void (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_UserMessageDestroy __s2sdk_UserMessageDestroy;
namespace s2sdk {
  /**
   * @brief Destroys a UserMessage and frees its memory.
   * @param userMessage (ptr64): The UserMessage to destroy.
   */
  inline void UserMessageDestroy(void* userMessage) {
    return __s2sdk_UserMessageDestroy(userMessage);
  }

  using _UserMessageSend = void (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_UserMessageSend __s2sdk_UserMessageSend;
namespace s2sdk {
  /**
   * @brief Sends a UserMessage to the specified recipients.
   * @param userMessage (ptr64): The UserMessage to send.
   */
  inline void UserMessageSend(void* userMessage) {
    return __s2sdk_UserMessageSend(userMessage);
  }

  using _UserMessageGetMessageID = int16_t (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_UserMessageGetMessageID __s2sdk_UserMessageGetMessageID;
namespace s2sdk {
  /**
   * @brief Gets the ID of the message.
   * @param userMessage (ptr64): The UserMessage instance.
   * @return int16: The ID of the message.
   */
  inline int16_t UserMessageGetMessageID(void* userMessage) {
    return __s2sdk_UserMessageGetMessageID(userMessage);
  }

  using _UserMessageFindMessageIdByName = int16_t (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_UserMessageFindMessageIdByName __s2sdk_UserMessageFindMessageIdByName;
namespace s2sdk {
  /**
   * @brief Finds a message ID by its name.
   * @param messageName (string): The name of the message.
   * @return int16: The ID of the message, or 0 if the message was not found.
   */
  inline int16_t UserMessageFindMessageIdByName(const plg::string& messageName) {
    return __s2sdk_UserMessageFindMessageIdByName(messageName);
  }

  using _UserMessageAddRecipient = void (*)(void*, int32_t);
}
extern "C" PLUGIN_API s2sdk::_UserMessageAddRecipient __s2sdk_UserMessageAddRecipient;
namespace s2sdk {
  /**
   * @brief Adds a single recipient (player) to the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param playerSlot (int32): The slot index of the player to add as a recipient.
   */
  inline void UserMessageAddRecipient(void* userMessage, int32_t playerSlot) {
    return __s2sdk_UserMessageAddRecipient(userMessage, playerSlot);
  }

  using _PbSetUInt32 = bool (*)(void*, const plg::string&, uint32_t);
}
extern "C" PLUGIN_API s2sdk::_PbSetUInt32 __s2sdk_PbSetUInt32;
namespace s2sdk {
  /**
   * @brief Sets an unsigned 32-bit integer value for a field in the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param fieldName (string): The name of the field.
   * @param value (uint32): The value to set.
   * @return bool: True if the field was successfully set, false otherwise.
   */
  inline bool PbSetUInt32(void* userMessage, const plg::string& fieldName, uint32_t value) {
    return __s2sdk_PbSetUInt32(userMessage, fieldName, value);
  }

  using _PbSetFloat = bool (*)(void*, const plg::string&, float);
}
extern "C" PLUGIN_API s2sdk::_PbSetFloat __s2sdk_PbSetFloat;
namespace s2sdk {
  /**
   * @brief Sets a float value for a field in the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param fieldName (string): The name of the field.
   * @param value (float): The value to set.
   * @return bool: True if the field was successfully set, false otherwise.
   */
  inline bool PbSetFloat(void* userMessage, const plg::string& fieldName, float value) {
    return __s2sdk_PbSetFloat(userMessage, fieldName, value);
  }

  using _HookUserMessage = bool (*)(int16_t, UserMessageCallback, HookMode);
}
extern "C" PLUGIN_API s2sdk::_HookUserMessage __s2sdk_HookUserMessage;
namespace s2sdk {
  /**
   * @brief Hooks a user message with a callback.
   * @param messageId (int16): The ID of the message to hook.
   * @param callback (function): The callback function to invoke when the message is received.
   * @param mode (uint8): Whether to hook the message in the post mode (after processing) or pre mode (before processing).
   * @return bool: True if the hook was successfully added, false otherwise.
   */
  inline bool HookUserMessage(int16_t messageId, UserMessageCallback callback, HookMode mode) {
    return __s2sdk_HookUserMessage(messageId, callback, mode);
  }

  using _UnhookUserMessage = bool (*)(int16_t, UserMessageCallback, HookMode);
}
extern "C" PLUGIN_API s2sdk::_UnhookUserMessage __s2sdk_UnhookUserMessage;
namespace s2sdk {
  /**
   * @brief Unhooks a previously hooked user message.
   * @param messageId (int16): The ID of the message to unhook.
   * @param callback (function): The callback function to remove.
   * @param mode (uint8): Whether the hook was in post mode (after processing) or pre mode (before processing).
   * @return bool: True if the hook was successfully removed, false otherwise.
   */
  inline bool UnhookUserMessage(int16_t messageId, UserMessageCallback callback, HookMode mode) {
    return __s2sdk_UnhookUserMessage(messageId, callback, mode);
  }

  using _UserMessageCreateFromName = void* (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_UserMessageCreateFromName __s2sdk_UserMessageCreateFromName;
namespace s2sdk {
  /**
   * @brief Creates a UserMessage from a message name.
   * @param messageName (string): The name of the message.
   * @return ptr64: A pointer to the newly created UserMessage.
   */
  inline void* UserMessageCreateFromName(const plg::string& messageName) {
    return __s2sdk_UserMessageCreateFromName(messageName);
  }

  using _UserMessageGetMessageName = plg::string (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_UserMessageGetMessageName __s2sdk_UserMessageGetMessageName;
namespace s2sdk {
  /**
   * @brief Gets the name of the message.
   * @param userMessage (ptr64): The UserMessage instance.
   * @return string: The name of the message as a string.
   */
  inline plg::string UserMessageGetMessageName(void* userMessage) {
    return __s2sdk_UserMessageGetMessageName(userMessage);
  }

  using _UserMessageAddAllPlayers = void (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_UserMessageAddAllPlayers __s2sdk_UserMessageAddAllPlayers;
namespace s2sdk {
  /**
   * @brief Adds all connected players as recipients to the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   */
  inline void UserMessageAddAllPlayers(void* userMessage) {
    return __s2sdk_UserMessageAddAllPlayers(userMessage);
  }

  using _UserMessageGetRepeatedFieldCount = int32_t (*)(void*, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_UserMessageGetRepeatedFieldCount __s2sdk_UserMessageGetRepeatedFieldCount;
namespace s2sdk {
  /**
   * @brief Gets the count of repeated fields in a field of the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param fieldName (string): The name of the field.
   * @return int32: The count of repeated fields, or -1 if the field is not repeated or does not exist.
   */
  inline int32_t UserMessageGetRepeatedFieldCount(void* userMessage, const plg::string& fieldName) {
    return __s2sdk_UserMessageGetRepeatedFieldCount(userMessage, fieldName);
  }

  using _PbGetRepeatedString = bool (*)(void*, const plg::string&, int32_t, plg::string&);
}
extern "C" PLUGIN_API s2sdk::_PbGetRepeatedString __s2sdk_PbGetRepeatedString;
namespace s2sdk {
  /**
   * @brief Gets a repeated string value from a field in the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param fieldName (string): The name of the field.
   * @param index (int32): The index of the repeated field.
   * @param out (string&): The output string.
   * @return bool: True if the field was successfully retrieved, false otherwise.
   */
  inline bool PbGetRepeatedString(void* userMessage, const plg::string& fieldName, int32_t index, plg::string& out) {
    return __s2sdk_PbGetRepeatedString(userMessage, fieldName, index, out);
  }

  using _PbAddString = bool (*)(void*, const plg::string&, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_PbAddString __s2sdk_PbAddString;
namespace s2sdk {
  /**
   * @brief Adds a string value to a repeated field in the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param fieldName (string): The name of the field.
   * @param value (string): The value to add.
   * @return bool: True if the value was successfully added, false otherwise.
   */
  inline bool PbAddString(void* userMessage, const plg::string& fieldName, const plg::string& value) {
    return __s2sdk_PbAddString(userMessage, fieldName, value);
  }

  using _HookSendNetMessage = bool (*)(SendNetMessageHookCallback);
}
extern "C" PLUGIN_API s2sdk::_HookSendNetMessage __s2sdk_HookSendNetMessage;
namespace s2sdk {
  /**
   * @brief Hooks every net message sent to a single client, allowing to block it for that client.
   * @param callback (function): The callback function to invoke before the message is sent.
   * @return bool: True if the hook was successfully added, false otherwise.
   */
  inline bool HookSendNetMessage(SendNetMessageHookCallback callback) {
    return __s2sdk_HookSendNetMessage(callback);
  }

  using _UnhookSendNetMessage = bool (*)(SendNetMessageHookCallback);
}
extern "C" PLUGIN_API s2sdk::_UnhookSendNetMessage __s2sdk_UnhookSendNetMessage;
namespace s2sdk {
  /**
   * @brief Unhooks a previously hooked callback from HookSendNetMessage.
   * @param callback (function): The callback function to remove.
   * @return bool: True if the hook was successfully removed, false otherwise.
   */
  inline bool UnhookSendNetMessage(SendNetMessageHookCallback callback) {
    return __s2sdk_UnhookSendNetMessage(callback);
  }

  using _PbGetInt32 = bool (*)(void*, const plg::string&, int32_t&);
}
extern "C" PLUGIN_API s2sdk::_PbGetInt32 __s2sdk_PbGetInt32;
namespace s2sdk {
  /**
   * @brief Gets a 32-bit integer value from a field in the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param fieldName (string): The name of the field.
   * @param out (int32&): The output value.
   * @return bool: True if the field was successfully retrieved, false otherwise.
   */
  inline bool PbGetInt32(void* userMessage, const plg::string& fieldName, int32_t& out) {
    return __s2sdk_PbGetInt32(userMessage, fieldName, out);
  }

  using _PbSetUInt64 = bool (*)(void*, const plg::string&, uint64_t);
}
extern "C" PLUGIN_API s2sdk::_PbSetUInt64 __s2sdk_PbSetUInt64;
namespace s2sdk {
  /**
   * @brief Sets an unsigned 64-bit integer value for a field in the UserMessage.
   * @param userMessage (ptr64): The UserMessage instance.
   * @param fieldName (string): The name of the field.
   * @param value (uint64): The value to set.
   * @return bool: True if the field was successfully set, false otherwise.
   */
  inline bool PbSetUInt64(void* userMessage, const plg::string& fieldName, uint64_t value) {
    return __s2sdk_PbSetUInt64(userMessage, fieldName, value);
  }

} // namespace s2sdk
