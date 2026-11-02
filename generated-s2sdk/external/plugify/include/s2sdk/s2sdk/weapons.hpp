#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: weapons)

namespace s2sdk {

  using _GetWeaponItemDefinition = WeaponDefIndex (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetWeaponItemDefinition __s2sdk_GetWeaponItemDefinition;
namespace s2sdk {
  /**
   * @brief Retrieves the weapon definition index for a given entity handle.
   * @param entityHandle (int32): The handle of the entity from which to retrieve the weapon def index.
   * @return uint16: The weapon definition index as a `uint16_t`, or 0 if the entity handle is invalid.
   */
  inline WeaponDefIndex GetWeaponItemDefinition(int32_t entityHandle) {
    return __s2sdk_GetWeaponItemDefinition(entityHandle);
  }

} // namespace s2sdk
