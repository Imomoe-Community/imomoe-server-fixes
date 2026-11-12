#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: voice)

namespace s2sdk {

  using _HookClientVoiceData = bool (*)(ClientVoiceDataHookCallback);
}
extern "C" PLUGIN_API s2sdk::_HookClientVoiceData __s2sdk_HookClientVoiceData;
namespace s2sdk {
  /**
   * @brief Hooks every voice packet a client sends, before the engine passes it on. The packet can be changed or blocked.
   * @param callback (function): The callback function to invoke.
   * @return bool: True if the hook was successfully added, false otherwise.
   */
  inline bool HookClientVoiceData(ClientVoiceDataHookCallback callback) {
    return __s2sdk_HookClientVoiceData(callback);
  }

  using _UnhookClientVoiceData = bool (*)(ClientVoiceDataHookCallback);
}
extern "C" PLUGIN_API s2sdk::_UnhookClientVoiceData __s2sdk_UnhookClientVoiceData;
namespace s2sdk {
  /**
   * @brief Unhooks a previously hooked callback from HookClientVoiceData.
   * @param callback (function): The callback function to remove.
   * @return bool: True if the hook was successfully removed, false otherwise.
   */
  inline bool UnhookClientVoiceData(ClientVoiceDataHookCallback callback) {
    return __s2sdk_UnhookClientVoiceData(callback);
  }

  using _VoiceDataGetAudio = plg::vector<uint8_t> (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_VoiceDataGetAudio __s2sdk_VoiceDataGetAudio;
namespace s2sdk {
  /**
   * @brief Gets the compressed audio of a voice packet.
   * @param voiceData (ptr64): The voice packet from HookClientVoiceData.
   * @return uint8[]: The compressed audio bytes.
   */
  inline plg::vector<uint8_t> VoiceDataGetAudio(void* voiceData) {
    return __s2sdk_VoiceDataGetAudio(voiceData);
  }

  using _VoiceDataGetNumPackets = uint32_t (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_VoiceDataGetNumPackets __s2sdk_VoiceDataGetNumPackets;
namespace s2sdk {
  /**
   * @brief Gets the number of codec packets in a voice packet.
   * @param voiceData (ptr64): The voice packet from HookClientVoiceData.
   * @return uint32: The number of codec packets.
   */
  inline uint32_t VoiceDataGetNumPackets(void* voiceData) {
    return __s2sdk_VoiceDataGetNumPackets(voiceData);
  }

  using _VoiceDataGetPacketOffsets = plg::vector<uint32_t> (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_VoiceDataGetPacketOffsets __s2sdk_VoiceDataGetPacketOffsets;
namespace s2sdk {
  /**
   * @brief Gets the offsets of the codec packets inside the audio of a voice packet.
   * @param voiceData (ptr64): The voice packet from HookClientVoiceData.
   * @return uint32[]: The packet offsets in bytes.
   */
  inline plg::vector<uint32_t> VoiceDataGetPacketOffsets(void* voiceData) {
    return __s2sdk_VoiceDataGetPacketOffsets(voiceData);
  }

} // namespace s2sdk
