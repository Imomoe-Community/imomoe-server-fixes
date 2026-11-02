#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: gameconfig)

namespace s2sdk {

  using _LoadGameConfigFile = ConfigId (*)(const plg::vector<plg::string>&);
}
extern "C" PLUGIN_API s2sdk::_LoadGameConfigFile __s2sdk_LoadGameConfigFile;
namespace s2sdk {
  /**
   * @brief Loads a game configuration file.
   * @param paths (string[]): The paths to the game configuration file to be loaded.
   * @return uint32: A id to the loaded game configuration object, or 0 if loading fails.
   */
  inline ConfigId LoadGameConfigFile(const plg::vector<plg::string>& paths) {
    return __s2sdk_LoadGameConfigFile(paths);
  }

  using _CloseGameConfigFile = void (*)(ConfigId);
}
extern "C" PLUGIN_API s2sdk::_CloseGameConfigFile __s2sdk_CloseGameConfigFile;
namespace s2sdk {
  /**
   * @brief Closes a game configuration file.
   * @param id (uint32): An id to the game configuration to be closed.
   */
  inline void CloseGameConfigFile(ConfigId id) {
    return __s2sdk_CloseGameConfigFile(id);
  }

  using _GetGameConfigOffset = int32_t (*)(ConfigId, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetGameConfigOffset __s2sdk_GetGameConfigOffset;
namespace s2sdk {
  /**
   * @brief Retrieves the offset associated with a name from the game configuration.
   * @param id (uint32): An id to the game configuration from which to retrieve the offset.
   * @param name (string): The name whose offset is to be retrieved.
   * @return int32: The offset associated with the specified name.
   */
  inline int32_t GetGameConfigOffset(ConfigId id, const plg::string& name) {
    return __s2sdk_GetGameConfigOffset(id, name);
  }

  using _GetGameConfigSignature = void* (*)(ConfigId, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetGameConfigSignature __s2sdk_GetGameConfigSignature;
namespace s2sdk {
  /**
   * @brief Retrieves the signature associated with a name from the game configuration.
   * @param id (uint32): An id to the game configuration from which to retrieve the signature.
   * @param name (string): The name whose signature is to be resolved and retrieved.
   * @return ptr64: A pointer to the signature associated with the specified name.
   */
  inline void* GetGameConfigSignature(ConfigId id, const plg::string& name) {
    return __s2sdk_GetGameConfigSignature(id, name);
  }

  using _GetGameConfigVTable = void* (*)(ConfigId, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetGameConfigVTable __s2sdk_GetGameConfigVTable;
namespace s2sdk {
  /**
   * @brief Retrieves a vtable associated with the game configuration.
   * @param id (uint32): An id to the game configuration from which to retrieve the vtable.
   * @param name (string): The name of the vtable to be retrieved.
   * @return ptr64: A pointer to the vtable associated with the specified name
   */
  inline void* GetGameConfigVTable(ConfigId id, const plg::string& name) {
    return __s2sdk_GetGameConfigVTable(id, name);
  }

} // namespace s2sdk
