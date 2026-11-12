#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: engine)

namespace s2sdk {

  using _IsMapValid = bool (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_IsMapValid __s2sdk_IsMapValid;
namespace s2sdk {
  /**
   * @brief Returns whether a specified map is valid or not.
   * @param mapname (string): The name of the map to check for validity.
   * @return bool: True if the map is valid, false otherwise.
   */
  inline bool IsMapValid(const plg::string& mapname) {
    return __s2sdk_IsMapValid(mapname);
  }

  using _GetGameTickCount = int32_t (*)();
}
extern "C" PLUGIN_API s2sdk::_GetGameTickCount __s2sdk_GetGameTickCount;
namespace s2sdk {
  /**
   * @brief Returns the game's internal tick count.
   * @return int32: The current tick count of the game.
   */
  inline int32_t GetGameTickCount() {
    return __s2sdk_GetGameTickCount();
  }

  using _GetMaxClients = int32_t (*)();
}
extern "C" PLUGIN_API s2sdk::_GetMaxClients __s2sdk_GetMaxClients;
namespace s2sdk {
  /**
   * @brief Returns the maximum number of clients that can connect to the server.
   * @return int32: The maximum client count, or -1 if global variables are not initialized.
   */
  inline int32_t GetMaxClients() {
    return __s2sdk_GetMaxClients();
  }

  using _Precache = void (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_Precache __s2sdk_Precache;
namespace s2sdk {
  /**
   * @brief Precaches a given file.
   * @param resource (string): The name of the resource to be precached.
   */
  inline void Precache(const plg::string& resource) {
    return __s2sdk_Precache(resource);
  }

  using _GetGameTime = float (*)();
}
extern "C" PLUGIN_API s2sdk::_GetGameTime __s2sdk_GetGameTime;
namespace s2sdk {
  /**
   * @brief Returns the game time based on the game tick.
   * @return float: The current game time.
   */
  inline float GetGameTime() {
    return __s2sdk_GetGameTime();
  }

  using _EmitSoundToClient = void (*)(int32_t, const plg::vector<int32_t>&, const plg::string&, float, float);
}
extern "C" PLUGIN_API s2sdk::_EmitSoundToClient __s2sdk_EmitSoundToClient;
namespace s2sdk {
  /**
   * @brief Emits a sound to one or more clients.
   * @param entityHandle (int32): The handle of the entity that emits the sound.
   * @param playersSlot (int32[]): Player slot indices that will hear the sound.
   * @param sound (string): The name of the sound to emit.
   * @param volume (float): The volume of the sound.
   * @param pitch (float): The pitch of the sound.
   */
  inline void EmitSoundToClient(int32_t entityHandle, const plg::vector<int32_t>& playersSlot, const plg::string& sound, float volume, float pitch) {
    return __s2sdk_EmitSoundToClient(entityHandle, playersSlot, sound, volume, pitch);
  }

  using _StopSound = void (*)(int32_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_StopSound __s2sdk_StopSound;
namespace s2sdk {
  /**
   * @brief Stops a sound from a specified entity.
   * @param entityHandle (int32): The handle of the entity that will stop the sound.
   * @param sound (string): The name of the sound to stop.
   */
  inline void StopSound(int32_t entityHandle, const plg::string& sound) {
    return __s2sdk_StopSound(entityHandle, sound);
  }

  using _QueryInterface = void* (*)(const plg::string&, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_QueryInterface __s2sdk_QueryInterface;
namespace s2sdk {
  /**
   * @brief Queries an interface from a specified module.
   * @param module (string): The name of the module to query the interface from.
   * @param name (string): The name of the interface to find.
   * @return ptr64: A pointer to the queried interface.
   */
  inline void* QueryInterface(const plg::string& module, const plg::string& name) {
    return __s2sdk_QueryInterface(module, name);
  }

  using _GetCurrentMap = plg::string (*)();
}
extern "C" PLUGIN_API s2sdk::_GetCurrentMap __s2sdk_GetCurrentMap;
namespace s2sdk {
  /**
   * @brief Returns the current map name.
   * @return string: A reference to a string where the current map name will be stored.
   */
  inline plg::string GetCurrentMap() {
    return __s2sdk_GetCurrentMap();
  }

  using _GetGameDirectory = plg::string (*)();
}
extern "C" PLUGIN_API s2sdk::_GetGameDirectory __s2sdk_GetGameDirectory;
namespace s2sdk {
  /**
   * @brief Returns the path of the game's directory.
   * @return string: A reference to a string where the game directory path will be stored.
   */
  inline plg::string GetGameDirectory() {
    return __s2sdk_GetGameDirectory();
  }

} // namespace s2sdk
