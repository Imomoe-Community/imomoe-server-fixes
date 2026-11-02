#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: gamerules)

namespace s2sdk {

  using _GetGameRules = void* (*)();
}
extern "C" PLUGIN_API s2sdk::_GetGameRules __s2sdk_GetGameRules;
namespace s2sdk {
  /**
   * @brief Retrieves the pointer to the current game rules instance.
   * @return ptr64: A pointer to the game rules object.
   */
  inline void* GetGameRules() {
    return __s2sdk_GetGameRules();
  }

  using _TerminateRound = void (*)(float, CSRoundEndReason);
}
extern "C" PLUGIN_API s2sdk::_TerminateRound __s2sdk_TerminateRound;
namespace s2sdk {
  /**
   * @brief Forces the round to end with a specified reason and delay.
   * @param delay (float): Time (in seconds) to delay before the next round starts.
   * @param reason (int32): The reason for ending the round, defined by the CSRoundEndReason enum.
   */
  inline void TerminateRound(float delay, CSRoundEndReason reason) {
    return __s2sdk_TerminateRound(delay, reason);
  }

} // namespace s2sdk
