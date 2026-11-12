#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: clients)

namespace s2sdk {

  using _IsClientSourceTV = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsClientSourceTV __s2sdk_IsClientSourceTV;
namespace s2sdk {
  /**
   * @brief Checks if a certain player is the SourceTV bot.
   * @param playerSlot (int32): The index of the player's slot.
   * @return bool: true if the client is the SourceTV bot, false otherwise.
   */
  inline bool IsClientSourceTV(int32_t playerSlot) {
    return __s2sdk_IsClientSourceTV(playerSlot);
  }

  using _PlayerSlotToEntHandle = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_PlayerSlotToEntHandle __s2sdk_PlayerSlotToEntHandle;
namespace s2sdk {
  /**
   * @brief Returns the entity handle associated with a player slot index.
   * @param playerSlot (int32): Index of the player slot.
   * @return int32: The index of the entity, or -1 if the handle is invalid.
   */
  inline int32_t PlayerSlotToEntHandle(int32_t playerSlot) {
    return __s2sdk_PlayerSlotToEntHandle(playerSlot);
  }

  using _IsClientInGame = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsClientInGame __s2sdk_IsClientInGame;
namespace s2sdk {
  /**
   * @brief Checks if a certain player has entered the game.
   * @param playerSlot (int32): The index of the player's slot.
   * @return bool: true if the player is in the game, false otherwise.
   */
  inline bool IsClientInGame(int32_t playerSlot) {
    return __s2sdk_IsClientInGame(playerSlot);
  }

  using _IsClientAlive = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsClientAlive __s2sdk_IsClientAlive;
namespace s2sdk {
  /**
   * @brief Checks if the client is alive or dead.
   * @param playerSlot (int32): The index of the player's slot.
   * @return bool: true if the client is alive, false if dead.
   */
  inline bool IsClientAlive(int32_t playerSlot) {
    return __s2sdk_IsClientAlive(playerSlot);
  }

  using _GetClientGravity = float (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientGravity __s2sdk_GetClientGravity;
namespace s2sdk {
  /**
   * @brief Retrieves the gravity scale of an client.
   * @param playerSlot (int32): The index of the player's slot whose gravity scale is to be retrieved.
   * @return float: The gravity scale of the client, or 0.0f if the client is invalid.
   */
  inline float GetClientGravity(int32_t playerSlot) {
    return __s2sdk_GetClientGravity(playerSlot);
  }

  using _SetClientGravity = void (*)(int32_t, float);
}
extern "C" PLUGIN_API s2sdk::_SetClientGravity __s2sdk_SetClientGravity;
namespace s2sdk {
  /**
   * @brief Sets the gravity scale of an client.
   * @param playerSlot (int32): The index of the player's slot whose gravity scale is to be set.
   * @param gravity (float): The new gravity scale to set for the client.
   */
  inline void SetClientGravity(int32_t playerSlot, float gravity) {
    return __s2sdk_SetClientGravity(playerSlot, gravity);
  }

  using _GetClientHealth = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientHealth __s2sdk_GetClientHealth;
namespace s2sdk {
  /**
   * @brief Retrieves the health of an client.
   * @param playerSlot (int32): The index of the player's slot whose health is to be retrieved.
   * @return int32: The health of the client, or 0 if the client is invalid.
   */
  inline int32_t GetClientHealth(int32_t playerSlot) {
    return __s2sdk_GetClientHealth(playerSlot);
  }

  using _SetClientHealth = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetClientHealth __s2sdk_SetClientHealth;
namespace s2sdk {
  /**
   * @brief Sets the health of an client.
   * @param playerSlot (int32): The index of the player's slot whose health is to be set.
   * @param health (int32): The new health value to set for the client.
   */
  inline void SetClientHealth(int32_t playerSlot, int32_t health) {
    return __s2sdk_SetClientHealth(playerSlot, health);
  }

  using _GetClientPawn = void* (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientPawn __s2sdk_GetClientPawn;
namespace s2sdk {
  /**
   * @brief Retrieves the pawn entity pointer associated with a client.
   * @param playerSlot (int32): The index of the player's slot.
   * @return ptr64: A pointer to the client's pawn entity, or nullptr if the client or controller is invalid.
   */
  inline void* GetClientPawn(int32_t playerSlot) {
    return __s2sdk_GetClientPawn(playerSlot);
  }

  using _GetClientActiveWeapon = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientActiveWeapon __s2sdk_GetClientActiveWeapon;
namespace s2sdk {
  /**
   * @brief Retrieves the handle of the client's currently active weapon.
   * @param playerSlot (int32): The index of the player's slot.
   * @return int32: The entity handle of the active weapon, or INVALID_EHANDLE_INDEX if the client is invalid or has no active weapon.
   */
  inline int32_t GetClientActiveWeapon(int32_t playerSlot) {
    return __s2sdk_GetClientActiveWeapon(playerSlot);
  }

  using _GetClientWeapons = plg::vector<int32_t> (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientWeapons __s2sdk_GetClientWeapons;
namespace s2sdk {
  /**
   * @brief Retrieves a list of weapon handles owned by the client.
   * @param playerSlot (int32): The index of the player's slot.
   * @return int32[]: A vector of entity handles for the client's weapons, or an empty vector if the client is invalid or has no weapons.
   */
  inline plg::vector<int32_t> GetClientWeapons(int32_t playerSlot) {
    return __s2sdk_GetClientWeapons(playerSlot);
  }

  using _RemoveWeapon = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_RemoveWeapon __s2sdk_RemoveWeapon;
namespace s2sdk {
  /**
   * @brief Removes a player's weapon.
   * @param playerSlot (int32): The index of the player's slot.
   * @param weaponHandle (int32): The handle of weapon to remove.
   */
  inline void RemoveWeapon(int32_t playerSlot, int32_t weaponHandle) {
    return __s2sdk_RemoveWeapon(playerSlot, weaponHandle);
  }

  using _GiveNamedItem = int32_t (*)(int32_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GiveNamedItem __s2sdk_GiveNamedItem;
namespace s2sdk {
  /**
   * @brief Gives a named item (e.g., weapon) to a client.
   * @param playerSlot (int32): The index of the player's slot.
   * @param itemName (string): The name of the item to give.
   * @return int32: The entity handle of the created item, or INVALID_EHANDLE_INDEX if the client or item is invalid.
   */
  inline int32_t GiveNamedItem(int32_t playerSlot, const plg::string& itemName) {
    return __s2sdk_GiveNamedItem(playerSlot, itemName);
  }

  using _GetClientArmor = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientArmor __s2sdk_GetClientArmor;
namespace s2sdk {
  /**
   * @brief Returns the client's armor value.
   * @param playerSlot (int32): The index of the player's slot.
   * @return int32: The armor value of the client.
   */
  inline int32_t GetClientArmor(int32_t playerSlot) {
    return __s2sdk_GetClientArmor(playerSlot);
  }

  using _SetClientArmor = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetClientArmor __s2sdk_SetClientArmor;
namespace s2sdk {
  /**
   * @brief Sets the client's armor value.
   * @param playerSlot (int32): The index of the player's slot.
   * @param armor (int32): The armor value to set.
   */
  inline void SetClientArmor(int32_t playerSlot, int32_t armor) {
    return __s2sdk_SetClientArmor(playerSlot, armor);
  }

  using _GetClientTeam = CSTeam (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientTeam __s2sdk_GetClientTeam;
namespace s2sdk {
  /**
   * @brief Retrieves the team number of an client.
   * @param playerSlot (int32): The index of the player's slot whose team number is to be retrieved.
   * @return int32: The team number of the client, or 0 if the client is invalid.
   */
  inline CSTeam GetClientTeam(int32_t playerSlot) {
    return __s2sdk_GetClientTeam(playerSlot);
  }

  using _GetClientRenderColor = plg::vec4 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientRenderColor __s2sdk_GetClientRenderColor;
namespace s2sdk {
  /**
   * @brief Retrieves the render color of an client.
   * @param playerSlot (int32): The index of the player's slot whose render color is to be retrieved.
   * @return vec4: The raw color value of the client's render color, or 0 if the client is invalid.
   */
  inline plg::vec4 GetClientRenderColor(int32_t playerSlot) {
    return __s2sdk_GetClientRenderColor(playerSlot);
  }

  using _SetClientRenderColor = void (*)(int32_t, const plg::vec4&);
}
extern "C" PLUGIN_API s2sdk::_SetClientRenderColor __s2sdk_SetClientRenderColor;
namespace s2sdk {
  /**
   * @brief Sets the render color of an client.
   * @param playerSlot (int32): The index of the player's slot whose render color is to be set.
   * @param color (vec4): The new raw color value to set for the client's render color.
   */
  inline void SetClientRenderColor(int32_t playerSlot, const plg::vec4& color) {
    return __s2sdk_SetClientRenderColor(playerSlot, color);
  }

  using _RespawnClient = void (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_RespawnClient __s2sdk_RespawnClient;
namespace s2sdk {
  /**
   * @brief Respawns a player.
   * @param playerSlot (int32): The index of the player's slot to respawn.
   */
  inline void RespawnClient(int32_t playerSlot) {
    return __s2sdk_RespawnClient(playerSlot);
  }

  using _ForceRespawnClient = void (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_ForceRespawnClient __s2sdk_ForceRespawnClient;
namespace s2sdk {
  /**
   * @brief Forces a full engine respawn for a player, including when the current pawn is alive. Supported in CS2.
   * @param playerSlot (int32): The index of the player's slot to respawn.
   */
  inline void ForceRespawnClient(int32_t playerSlot) {
    return __s2sdk_ForceRespawnClient(playerSlot);
  }

  using _GetClientSpeed = float (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientSpeed __s2sdk_GetClientSpeed;
namespace s2sdk {
  /**
   * @brief Returns the client's speed value.
   * @param playerSlot (int32): The index of the player's slot.
   * @return float: The speed value of the client.
   */
  inline float GetClientSpeed(int32_t playerSlot) {
    return __s2sdk_GetClientSpeed(playerSlot);
  }

  using _SetClientSpeed = void (*)(int32_t, float);
}
extern "C" PLUGIN_API s2sdk::_SetClientSpeed __s2sdk_SetClientSpeed;
namespace s2sdk {
  /**
   * @brief Sets the client's speed value.
   * @param playerSlot (int32): The index of the player's slot.
   * @param speed (float): The speed value to set.
   */
  inline void SetClientSpeed(int32_t playerSlot, float speed) {
    return __s2sdk_SetClientSpeed(playerSlot, speed);
  }

  using _GetClientAbsVelocity = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientAbsVelocity __s2sdk_GetClientAbsVelocity;
namespace s2sdk {
  /**
   * @brief Retrieves the absolute velocity of an client.
   * @param playerSlot (int32): The index of the player's slot whose absolute velocity is to be retrieved.
   * @return vec3: A vector where the absolute velocity will be stored.
   */
  inline plg::vec3 GetClientAbsVelocity(int32_t playerSlot) {
    return __s2sdk_GetClientAbsVelocity(playerSlot);
  }

  using _SetClientAbsVelocity = void (*)(int32_t, const plg::vec3&);
}
extern "C" PLUGIN_API s2sdk::_SetClientAbsVelocity __s2sdk_SetClientAbsVelocity;
namespace s2sdk {
  /**
   * @brief Sets the absolute velocity of an client.
   * @param playerSlot (int32): The index of the player's slot whose absolute velocity is to be set.
   * @param velocity (vec3): The new absolute velocity to set for the client.
   */
  inline void SetClientAbsVelocity(int32_t playerSlot, const plg::vec3& velocity) {
    return __s2sdk_SetClientAbsVelocity(playerSlot, velocity);
  }

  using _GetClientEyeAngles = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientEyeAngles __s2sdk_GetClientEyeAngles;
namespace s2sdk {
  /**
   * @brief Get the qangles that this client is looking at.
   * @param playerSlot (int32): The index of the player's slot
   * @return vec3: Eye angles as a vector (pitch, yaw, roll)
   */
  inline plg::vec3 GetClientEyeAngles(int32_t playerSlot) {
    return __s2sdk_GetClientEyeAngles(playerSlot);
  }

  using _GetClientButtons = uint64_t (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientButtons __s2sdk_GetClientButtons;
namespace s2sdk {
  /**
   * @brief Retrieves the state of a specific button for a client.
   * @param playerSlot (int32): The index of the player's slot.
   * @param buttonIndex (int32): The index of the button (0-2).
   * @return uint64: uint64_t The state of the specified button, or 0 if the client or button index is invalid.
   */
  inline uint64_t GetClientButtons(int32_t playerSlot, int32_t buttonIndex) {
    return __s2sdk_GetClientButtons(playerSlot, buttonIndex);
  }

  using _GetClientFlags = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientFlags __s2sdk_GetClientFlags;
namespace s2sdk {
  /**
   * @brief Retrieves the flags of an client.
   * @param playerSlot (int32): The index of the player's slot whose flags are to be retrieved.
   * @return int32: The flags of the client, or 0 if the client is invalid.
   */
  inline int32_t GetClientFlags(int32_t playerSlot) {
    return __s2sdk_GetClientFlags(playerSlot);
  }

  using _IsFakeClient = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsFakeClient __s2sdk_IsFakeClient;
namespace s2sdk {
  /**
   * @brief Checks if a certain player is a fake client.
   * @param playerSlot (int32): The index of the player's slot.
   * @return bool: true if the client is a fake client, false otherwise.
   */
  inline bool IsFakeClient(int32_t playerSlot) {
    return __s2sdk_IsFakeClient(playerSlot);
  }

  using _GetClientAbsOrigin = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientAbsOrigin __s2sdk_GetClientAbsOrigin;
namespace s2sdk {
  /**
   * @brief Retrieves the absolute origin of an client.
   * @param playerSlot (int32): The index of the player's slot whose absolute origin is to be retrieved.
   * @return vec3: A vector where the absolute origin will be stored.
   */
  inline plg::vec3 GetClientAbsOrigin(int32_t playerSlot) {
    return __s2sdk_GetClientAbsOrigin(playerSlot);
  }

  using _GetClientEyePosition = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientEyePosition __s2sdk_GetClientEyePosition;
namespace s2sdk {
  /**
   * @brief Get vector to eye position - absolute coords.
   * @param playerSlot (int32): The index of the player's slot
   * @return vec3: Eye position in absolute/world coordinates
   */
  inline plg::vec3 GetClientEyePosition(int32_t playerSlot) {
    return __s2sdk_GetClientEyePosition(playerSlot);
  }

  using _DropWeapon = void (*)(int32_t, int32_t, const plg::vec3&, const plg::vec3&);
}
extern "C" PLUGIN_API s2sdk::_DropWeapon __s2sdk_DropWeapon;
namespace s2sdk {
  /**
   * @brief Forces a player to drop their weapon.
   * @param playerSlot (int32): The index of the player's slot.
   * @param weaponHandle (int32): The handle of weapon to drop.
   * @param target (vec3): Target direction.
   * @param velocity (vec3): Velocity to toss weapon or zero to just drop weapon.
   */
  inline void DropWeapon(int32_t playerSlot, int32_t weaponHandle, const plg::vec3& target, const plg::vec3& velocity) {
    return __s2sdk_DropWeapon(playerSlot, weaponHandle, target, velocity);
  }

  using _SelectWeapon = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SelectWeapon __s2sdk_SelectWeapon;
namespace s2sdk {
  /**
   * @brief Selects a player's weapon.
   * @param playerSlot (int32): The index of the player's slot.
   * @param weaponHandle (int32): The handle of weapon to bump.
   */
  inline void SelectWeapon(int32_t playerSlot, int32_t weaponHandle) {
    return __s2sdk_SelectWeapon(playerSlot, weaponHandle);
  }

  using _SwitchWeapon = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SwitchWeapon __s2sdk_SwitchWeapon;
namespace s2sdk {
  /**
   * @brief Switches a player's weapon.
   * @param playerSlot (int32): The index of the player's slot.
   * @param weaponHandle (int32): The handle of weapon to switch.
   */
  inline void SwitchWeapon(int32_t playerSlot, int32_t weaponHandle) {
    return __s2sdk_SwitchWeapon(playerSlot, weaponHandle);
  }

  using _IsClientAuthorized = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsClientAuthorized __s2sdk_IsClientAuthorized;
namespace s2sdk {
  /**
   * @brief Checks if a certain player has been authenticated.
   * @param playerSlot (int32): The index of the player's slot.
   * @return bool: true if the player is authenticated, false otherwise.
   */
  inline bool IsClientAuthorized(int32_t playerSlot) {
    return __s2sdk_IsClientAuthorized(playerSlot);
  }

  using _GetClientSteamID64 = uint64_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientSteamID64 __s2sdk_GetClientSteamID64;
namespace s2sdk {
  /**
   * @brief Returns the client's SteamID64 - a unique 64-bit identifier of a Steam account.
   * @param playerSlot (int32): The index of the player's slot.
   * @return uint64: uint64_t The client's SteamID64.
   */
  inline uint64_t GetClientSteamID64(int32_t playerSlot) {
    return __s2sdk_GetClientSteamID64(playerSlot);
  }

  using _GetClientLanguageCode = plg::string (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientLanguageCode __s2sdk_GetClientLanguageCode;
namespace s2sdk {
  /**
   * @brief Retrieves a client's language as an ISO code. Unlike GetClientLanguage, which returns the raw cl_language value ("english", "schinese"), this returns the code translation files are keyed by ("en", "zh-CN"). Empty until the client answers the cl_language query, and always empty for bots. Reliably available starting from OnClientAuthenticated and OnClientPutInServer.
   * @param playerSlot (int32): The index of the player's slot.
   * @return string: The client's ISO language code.
   */
  inline plg::string GetClientLanguageCode(int32_t playerSlot) {
    return __s2sdk_GetClientLanguageCode(playerSlot);
  }

  using _GetClientKills = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientKills __s2sdk_GetClientKills;
namespace s2sdk {
  /**
   * @brief Retrieves the number of kills for a client.
   * @param playerSlot (int32): The index of the player's slot.
   * @return int32: The number of kills the client has, or 0 if the player slot is invalid.
   */
  inline int32_t GetClientKills(int32_t playerSlot) {
    return __s2sdk_GetClientKills(playerSlot);
  }

  using _GetClientDeaths = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetClientDeaths __s2sdk_GetClientDeaths;
namespace s2sdk {
  /**
   * @brief Retrieves the number of deaths for a client.
   * @param playerSlot (int32): The index of the player's slot.
   * @return int32: The number of deaths the client has, or 0 if the player slot is invalid.
   */
  inline int32_t GetClientDeaths(int32_t playerSlot) {
    return __s2sdk_GetClientDeaths(playerSlot);
  }

  using _ChangeClientTeam = void (*)(int32_t, CSTeam);
}
extern "C" PLUGIN_API s2sdk::_ChangeClientTeam __s2sdk_ChangeClientTeam;
namespace s2sdk {
  /**
   * @brief Changes the player's team.
   * @param playerSlot (int32): The index of the player's slot.
   * @param team (int32): The team index to change the client to.
   */
  inline void ChangeClientTeam(int32_t playerSlot, CSTeam team) {
    return __s2sdk_ChangeClientTeam(playerSlot, team);
  }

  using _IsClientConnected = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsClientConnected __s2sdk_IsClientConnected;
namespace s2sdk {
  /**
   * @brief Checks if a certain player is connected.
   * @param playerSlot (int32): The index of the player's slot.
   * @return bool: true if the player is connected, false otherwise.
   */
  inline bool IsClientConnected(int32_t playerSlot) {
    return __s2sdk_IsClientConnected(playerSlot);
  }

} // namespace s2sdk
