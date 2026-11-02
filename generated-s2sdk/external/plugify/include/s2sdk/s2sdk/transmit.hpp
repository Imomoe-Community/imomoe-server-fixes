#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: transmit)

namespace s2sdk {

  using _GetHiddenTransmitEntities = plg::vector<int32_t> (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetHiddenTransmitEntities __s2sdk_GetHiddenTransmitEntities;
namespace s2sdk {
  /**
   * @brief Gets all hidden entity handles for a player.
   * @param playerSlot (int32): The player slot to query.
   * @return int32[]: Array of hidden entity handles.
   */
  inline plg::vector<int32_t> GetHiddenTransmitEntities(int32_t playerSlot) {
    return __s2sdk_GetHiddenTransmitEntities(playerSlot);
  }

  using _HideTransmitEntity = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_HideTransmitEntity __s2sdk_HideTransmitEntity;
namespace s2sdk {
  /**
   * @brief Hides a single entity from a player's transmit list.
   * @param playerSlot (int32): The player slot to hide the entity from.
   * @param entityHandle (int32): Entity handle to hide.
   */
  inline void HideTransmitEntity(int32_t playerSlot, int32_t entityHandle) {
    return __s2sdk_HideTransmitEntity(playerSlot, entityHandle);
  }

  using _ShowTransmitEntity = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_ShowTransmitEntity __s2sdk_ShowTransmitEntity;
namespace s2sdk {
  /**
   * @brief Shows a previously hidden entity to a player.
   * @param playerSlot (int32): The player slot to show the entity to.
   * @param entityHandle (int32): Entity handle to show.
   */
  inline void ShowTransmitEntity(int32_t playerSlot, int32_t entityHandle) {
    return __s2sdk_ShowTransmitEntity(playerSlot, entityHandle);
  }

} // namespace s2sdk
