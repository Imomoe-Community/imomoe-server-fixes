#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: entities)

namespace s2sdk {

  using _EntPointerToEntHandle = int32_t (*)(void*);
}
extern "C" PLUGIN_API s2sdk::_EntPointerToEntHandle __s2sdk_EntPointerToEntHandle;
namespace s2sdk {
  /**
   * @brief Converts an entity pointer into an entity handle.
   * @param entity (ptr64): A pointer to the entity to convert.
   * @return int32: The entity handle as an integer, or INVALID_EHANDLE_INDEX if the entity is nullptr.
   */
  inline int32_t EntPointerToEntHandle(void* entity) {
    return __s2sdk_EntPointerToEntHandle(entity);
  }

  using _EntHandleToEntPointer = void* (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_EntHandleToEntPointer __s2sdk_EntHandleToEntPointer;
namespace s2sdk {
  /**
   * @brief Retrieves the entity pointer from an entity handle.
   * @param entityHandle (int32): The entity handle to convert.
   * @return ptr64: A pointer to the entity instance, or nullptr if the handle is invalid.
   */
  inline void* EntHandleToEntPointer(int32_t entityHandle) {
    return __s2sdk_EntHandleToEntPointer(entityHandle);
  }

  using _IsValidEntHandle = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsValidEntHandle __s2sdk_IsValidEntHandle;
namespace s2sdk {
  /**
   * @brief Checks if the provided entity handle is valid.
   * @param entityHandle (int32): The entity handle to check.
   * @return bool: True if the entity handle is valid, false otherwise.
   */
  inline bool IsValidEntHandle(int32_t entityHandle) {
    return __s2sdk_IsValidEntHandle(entityHandle);
  }

  using _GetEntityClassname = plg::string (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityClassname __s2sdk_GetEntityClassname;
namespace s2sdk {
  /**
   * @brief Retrieves the class name of an entity.
   * @param entityHandle (int32): The handle of the entity whose class name is to be retrieved.
   * @return string: A string where the class name will be stored.
   */
  inline plg::string GetEntityClassname(int32_t entityHandle) {
    return __s2sdk_GetEntityClassname(entityHandle);
  }

  using _GetEntityOwner = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityOwner __s2sdk_GetEntityOwner;
namespace s2sdk {
  /**
   * @brief Retrieves the owner of an entity.
   * @param entityHandle (int32): The handle of the entity whose owner is to be retrieved.
   * @return int32: The handle of the owner entity, or INVALID_EHANDLE_INDEX if the entity is invalid.
   */
  inline int32_t GetEntityOwner(int32_t entityHandle) {
    return __s2sdk_GetEntityOwner(entityHandle);
  }

  using _RemoveEntity = void (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_RemoveEntity __s2sdk_RemoveEntity;
namespace s2sdk {
  /**
   * @brief Marks an entity for deletion.
   * @param entityHandle (int32): The handle of the entity to be deleted.
   */
  inline void RemoveEntity(int32_t entityHandle) {
    return __s2sdk_RemoveEntity(entityHandle);
  }

  using _EntIndexToEntHandle = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_EntIndexToEntHandle __s2sdk_EntIndexToEntHandle;
namespace s2sdk {
  /**
   * @brief Converts an entity index into an entity handle.
   * @param entityIndex (int32): The index of the entity to convert.
   * @return int32: The entity handle as an integer, or -1 if the entity index is invalid.
   */
  inline int32_t EntIndexToEntHandle(int32_t entityIndex) {
    return __s2sdk_EntIndexToEntHandle(entityIndex);
  }

  using _GetFirstActiveEntity = int32_t (*)();
}
extern "C" PLUGIN_API s2sdk::_GetFirstActiveEntity __s2sdk_GetFirstActiveEntity;
namespace s2sdk {
  /**
   * @brief Retrieves the pointer to the first active entity.
   * @return int32: A handle to the first active entity.
   */
  inline int32_t GetFirstActiveEntity() {
    return __s2sdk_GetFirstActiveEntity();
  }

  using _GetNextActiveEntity = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetNextActiveEntity __s2sdk_GetNextActiveEntity;
namespace s2sdk {
  /**
   * @brief Retrieves the next active entity.
   * @param entityHandle (int32)
   * @return int32: Handle to the next entity.
   */
  inline int32_t GetNextActiveEntity(int32_t entityHandle) {
    return __s2sdk_GetNextActiveEntity(entityHandle);
  }

  using _FindEntityByClassname = int32_t (*)(int32_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_FindEntityByClassname __s2sdk_FindEntityByClassname;
namespace s2sdk {
  /**
   * @brief Finds an entity by classname with iteration.
   * @param startFrom (int32): The handle of the entity to start from, or INVALID_EHANDLE_INDEX to start fresh.
   * @param classname (string): The class name to search for.
   * @return int32: The handle of the found entity, or INVALID_EHANDLE_INDEX if none found.
   */
  inline int32_t FindEntityByClassname(int32_t startFrom, const plg::string& classname) {
    return __s2sdk_FindEntityByClassname(startFrom, classname);
  }

  using _CreateEntityByName = int32_t (*)(const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_CreateEntityByName __s2sdk_CreateEntityByName;
namespace s2sdk {
  /**
   * @brief Creates an entity by string name but does not spawn it.
   * @param className (string): The class name of the entity to create.
   * @return int32: The entity handle of the created entity, or INVALID_EHANDLE_INDEX if the entity could not be created.
   */
  inline int32_t CreateEntityByName(const plg::string& className) {
    return __s2sdk_CreateEntityByName(className);
  }

  using _DispatchSpawn = void (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_DispatchSpawn __s2sdk_DispatchSpawn;
namespace s2sdk {
  /**
   * @brief Spawns an entity into the game.
   * @param entityHandle (int32): The handle of the entity to spawn.
   */
  inline void DispatchSpawn(int32_t entityHandle) {
    return __s2sdk_DispatchSpawn(entityHandle);
  }

  using _DispatchSpawn2 = void (*)(int32_t, const plg::vector<plg::string>&, const plg::vector<plg::any>&);
}
extern "C" PLUGIN_API s2sdk::_DispatchSpawn2 __s2sdk_DispatchSpawn2;
namespace s2sdk {
  /**
   * @brief Spawns an entity into the game with key-value properties.
   * @param entityHandle (int32): The handle of the entity to spawn.
   * @param keys (string[]): A vector of keys representing the property names to set on the entity.
   * @param values (any[]): A vector of values corresponding to the keys, representing the property values to set on the entity.
   */
  inline void DispatchSpawn2(int32_t entityHandle, const plg::vector<plg::string>& keys, const plg::vector<plg::any>& values) {
    return __s2sdk_DispatchSpawn2(entityHandle, keys, values);
  }

  using _GetEntityMoveType = MoveType (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityMoveType __s2sdk_GetEntityMoveType;
namespace s2sdk {
  /**
   * @brief Retrieves the movement type of an entity.
   * @param entityHandle (int32): The handle of the entity whose movement type is to be retrieved.
   * @return int32: The movement type of the entity, or 0 if the entity is invalid.
   */
  inline MoveType GetEntityMoveType(int32_t entityHandle) {
    return __s2sdk_GetEntityMoveType(entityHandle);
  }

  using _SetEntityMoveType = void (*)(int32_t, MoveType);
}
extern "C" PLUGIN_API s2sdk::_SetEntityMoveType __s2sdk_SetEntityMoveType;
namespace s2sdk {
  /**
   * @brief Sets the movement type of an entity.
   * @param entityHandle (int32): The handle of the entity whose movement type is to be set.
   * @param moveType (int32): The new movement type to set for the entity.
   */
  inline void SetEntityMoveType(int32_t entityHandle, MoveType moveType) {
    return __s2sdk_SetEntityMoveType(entityHandle, moveType);
  }

  using _SetEntityGravity = void (*)(int32_t, float);
}
extern "C" PLUGIN_API s2sdk::_SetEntityGravity __s2sdk_SetEntityGravity;
namespace s2sdk {
  /**
   * @brief Sets the gravity scale of an entity.
   * @param entityHandle (int32): The handle of the entity whose gravity scale is to be set.
   * @param gravity (float): The new gravity scale to set for the entity.
   */
  inline void SetEntityGravity(int32_t entityHandle, float gravity) {
    return __s2sdk_SetEntityGravity(entityHandle, gravity);
  }

  using _SetEntityOwner = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetEntityOwner __s2sdk_SetEntityOwner;
namespace s2sdk {
  /**
   * @brief Sets the owner of an entity.
   * @param entityHandle (int32): The handle of the entity whose owner is to be set.
   * @param ownerHandle (int32): The handle of the new owner entity.
   */
  inline void SetEntityOwner(int32_t entityHandle, int32_t ownerHandle) {
    return __s2sdk_SetEntityOwner(entityHandle, ownerHandle);
  }

  using _GetEntityParent = int32_t (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityParent __s2sdk_GetEntityParent;
namespace s2sdk {
  /**
   * @brief Retrieves the parent of an entity.
   * @param entityHandle (int32): The handle of the entity whose parent is to be retrieved.
   * @return int32: The handle of the parent entity, or INVALID_EHANDLE_INDEX if the entity is invalid.
   */
  inline int32_t GetEntityParent(int32_t entityHandle) {
    return __s2sdk_GetEntityParent(entityHandle);
  }

  using _SetEntityParent = void (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetEntityParent __s2sdk_SetEntityParent;
namespace s2sdk {
  /**
   * @brief Sets the parent of an entity.
   * @param entityHandle (int32): The handle of the entity whose parent is to be set.
   * @param parentHandle (int32): The handle of the new parent entity. (Can be invalid to clean parent)
   */
  inline void SetEntityParent(int32_t entityHandle, int32_t parentHandle) {
    return __s2sdk_SetEntityParent(entityHandle, parentHandle);
  }

  using _SetEntityParentAttachment = void (*)(int32_t, int32_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_SetEntityParentAttachment __s2sdk_SetEntityParentAttachment;
namespace s2sdk {
  /**
   * @brief Sets the parent of an entity to attachment by name.
   * @param entityHandle (int32): The handle of the entity whose parent is to be set.
   * @param parentHandle (int32): The handle of the new parent entity.
   * @param attachmentName (string): The name of the entity's attachment.
   */
  inline void SetEntityParentAttachment(int32_t entityHandle, int32_t parentHandle, const plg::string& attachmentName) {
    return __s2sdk_SetEntityParentAttachment(entityHandle, parentHandle, attachmentName);
  }

  using _GetEntityAbsOrigin = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityAbsOrigin __s2sdk_GetEntityAbsOrigin;
namespace s2sdk {
  /**
   * @brief Retrieves the absolute origin of an entity.
   * @param entityHandle (int32): The handle of the entity whose absolute origin is to be retrieved.
   * @return vec3: A vector where the absolute origin will be stored.
   */
  inline plg::vec3 GetEntityAbsOrigin(int32_t entityHandle) {
    return __s2sdk_GetEntityAbsOrigin(entityHandle);
  }

  using _SetEntityAbsOrigin = void (*)(int32_t, const plg::vec3&);
}
extern "C" PLUGIN_API s2sdk::_SetEntityAbsOrigin __s2sdk_SetEntityAbsOrigin;
namespace s2sdk {
  /**
   * @brief Sets the absolute origin of an entity.
   * @param entityHandle (int32): The handle of the entity whose absolute origin is to be set.
   * @param origin (vec3): The new absolute origin to set for the entity.
   */
  inline void SetEntityAbsOrigin(int32_t entityHandle, const plg::vec3& origin) {
    return __s2sdk_SetEntityAbsOrigin(entityHandle, origin);
  }

  using _GetEntityLocalOrigin = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityLocalOrigin __s2sdk_GetEntityLocalOrigin;
namespace s2sdk {
  /**
   * @brief Retrieves the local origin of an entity.
   * @param entityHandle (int32): The handle of the entity whose local origin is to be retrieved.
   * @return vec3: A vector where the local origin will be stored.
   */
  inline plg::vec3 GetEntityLocalOrigin(int32_t entityHandle) {
    return __s2sdk_GetEntityLocalOrigin(entityHandle);
  }

  using _SetEntityLocalOrigin = void (*)(int32_t, const plg::vec3&);
}
extern "C" PLUGIN_API s2sdk::_SetEntityLocalOrigin __s2sdk_SetEntityLocalOrigin;
namespace s2sdk {
  /**
   * @brief Sets the local origin of an entity.
   * @param entityHandle (int32): The handle of the entity whose local origin is to be set.
   * @param origin (vec3): The new local origin to set for the entity.
   */
  inline void SetEntityLocalOrigin(int32_t entityHandle, const plg::vec3& origin) {
    return __s2sdk_SetEntityLocalOrigin(entityHandle, origin);
  }

  using _GetEntityAbsVelocity = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityAbsVelocity __s2sdk_GetEntityAbsVelocity;
namespace s2sdk {
  /**
   * @brief Retrieves the absolute velocity of an entity.
   * @param entityHandle (int32): The handle of the entity whose absolute velocity is to be retrieved.
   * @return vec3: A vector where the absolute velocity will be stored.
   */
  inline plg::vec3 GetEntityAbsVelocity(int32_t entityHandle) {
    return __s2sdk_GetEntityAbsVelocity(entityHandle);
  }

  using _SetEntityAbsVelocity = void (*)(int32_t, const plg::vec3&);
}
extern "C" PLUGIN_API s2sdk::_SetEntityAbsVelocity __s2sdk_SetEntityAbsVelocity;
namespace s2sdk {
  /**
   * @brief Sets the absolute velocity of an entity.
   * @param entityHandle (int32): The handle of the entity whose absolute velocity is to be set.
   * @param velocity (vec3): The new absolute velocity to set for the entity.
   */
  inline void SetEntityAbsVelocity(int32_t entityHandle, const plg::vec3& velocity) {
    return __s2sdk_SetEntityAbsVelocity(entityHandle, velocity);
  }

  using _SetEntityAngVelocity = void (*)(int32_t, const plg::vec3&);
}
extern "C" PLUGIN_API s2sdk::_SetEntityAngVelocity __s2sdk_SetEntityAngVelocity;
namespace s2sdk {
  /**
   * @brief Sets the angular velocity of an entity.
   * @param entityHandle (int32): The handle of the entity whose angular velocity is to be set.
   * @param velocity (vec3): The new angular velocity to set for the entity.
   */
  inline void SetEntityAngVelocity(int32_t entityHandle, const plg::vec3& velocity) {
    return __s2sdk_SetEntityAngVelocity(entityHandle, velocity);
  }

  using _SetEntityModel = void (*)(int32_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_SetEntityModel __s2sdk_SetEntityModel;
namespace s2sdk {
  /**
   * @brief Sets the model name of an entity.
   * @param entityHandle (int32): The handle of the entity whose model name is to be set.
   * @param model (string): The new model name to set for the entity.
   */
  inline void SetEntityModel(int32_t entityHandle, const plg::string& model) {
    return __s2sdk_SetEntityModel(entityHandle, model);
  }

  using _TeleportEntity = void (*)(int32_t, const plg::vec3&, const plg::vec3&, const plg::vec3&);
}
extern "C" PLUGIN_API s2sdk::_TeleportEntity __s2sdk_TeleportEntity;
namespace s2sdk {
  /**
   * @brief Teleports an entity to a specified location and orientation.
   * @param entityHandle (int32): The handle of the entity to teleport.
   * @param origin (vec3): A pointer to a Vector representing the new absolute position. Use nan vector to not set.
   * @param angles (vec3): A pointer to a QAngle representing the new orientation. Use nan vector to not set.
   * @param velocity (vec3): A pointer to a Vector representing the new velocity. Use nan vector to not set.
   */
  inline void TeleportEntity(int32_t entityHandle, const plg::vec3& origin, const plg::vec3& angles, const plg::vec3& velocity) {
    return __s2sdk_TeleportEntity(entityHandle, origin, angles, velocity);
  }

  using _AcceptEntityInput = void (*)(int32_t, const plg::string&, int32_t, int32_t, const plg::any&, FieldType, int32_t);
}
extern "C" PLUGIN_API s2sdk::_AcceptEntityInput __s2sdk_AcceptEntityInput;
namespace s2sdk {
  /**
   * @brief Invokes a named input method on a specified entity.
   * @param entityHandle (int32): The handle of the target entity that will receive the input.
   * @param inputName (string): The name of the input action to invoke.
   * @param activatorHandle (int32): The handle of the entity that initiated the sequence of actions.
   * @param callerHandle (int32): The handle of the entity sending this event.
   * @param value (any): The value associated with the input action.
   * @param type (int32): The type or classification of the value.
   * @param outputId (int32): An identifier for tracking the output of this operation.
   */
  inline void AcceptEntityInput(int32_t entityHandle, const plg::string& inputName, int32_t activatorHandle, int32_t callerHandle, const plg::any& value, FieldType type, int32_t outputId) {
    return __s2sdk_AcceptEntityInput(entityHandle, inputName, activatorHandle, callerHandle, value, type, outputId);
  }

  using _GetEntityAngRotation = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityAngRotation __s2sdk_GetEntityAngRotation;
namespace s2sdk {
  /**
   * @brief Retrieves the angular rotation of an entity.
   * @param entityHandle (int32): The handle of the entity whose angular rotation is to be retrieved.
   * @return vec3: A vector where the angular rotation will be stored.
   */
  inline plg::vec3 GetEntityAngRotation(int32_t entityHandle) {
    return __s2sdk_GetEntityAngRotation(entityHandle);
  }

  using _IsEntityPlayerPawn = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsEntityPlayerPawn __s2sdk_IsEntityPlayerPawn;
namespace s2sdk {
  /**
   * @brief Checks if an entity is a player pawn.
   * @param entityHandle (int32): The handle of the entity.
   * @return bool: True if the entity is a player pawn, false otherwise.
   */
  inline bool IsEntityPlayerPawn(int32_t entityHandle) {
    return __s2sdk_IsEntityPlayerPawn(entityHandle);
  }

  using _IsEntityPlayerController = bool (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_IsEntityPlayerController __s2sdk_IsEntityPlayerController;
namespace s2sdk {
  /**
   * @brief Checks if an entity is a player controller.
   * @param entityHandle (int32): The handle of the entity.
   * @return bool: True if the entity is a player controller, false otherwise.
   */
  inline bool IsEntityPlayerController(int32_t entityHandle) {
    return __s2sdk_IsEntityPlayerController(entityHandle);
  }

  using _GetEntityEyePosition = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityEyePosition __s2sdk_GetEntityEyePosition;
namespace s2sdk {
  /**
   * @brief Get vector to eye position - absolute coords.
   * @param entityHandle (int32): The handle of the entity
   * @return vec3: Eye position in absolute/world coordinates
   */
  inline plg::vec3 GetEntityEyePosition(int32_t entityHandle) {
    return __s2sdk_GetEntityEyePosition(entityHandle);
  }

  using _GetEntityEyeAngles = plg::vec3 (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityEyeAngles __s2sdk_GetEntityEyeAngles;
namespace s2sdk {
  /**
   * @brief Get the qangles that this entity is looking at.
   * @param entityHandle (int32): The handle of the entity
   * @return vec3: Eye angles as a vector (pitch, yaw, roll)
   */
  inline plg::vec3 GetEntityEyeAngles(int32_t entityHandle) {
    return __s2sdk_GetEntityEyeAngles(entityHandle);
  }

} // namespace s2sdk
