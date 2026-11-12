#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: addons)

namespace s2sdk {

  using _GetCurrentWorkshopMap = uint64_t (*)();
}
extern "C" PLUGIN_API s2sdk::_GetCurrentWorkshopMap __s2sdk_GetCurrentWorkshopMap;
namespace s2sdk {
  /**
   * @brief Retrieves the workshop ID of the current map.
   * @return uint64: The workshop ID of the current map, or 0 if the current map is not a workshop map.
   */
  inline uint64_t GetCurrentWorkshopMap() {
    return __s2sdk_GetCurrentWorkshopMap();
  }

} // namespace s2sdk
