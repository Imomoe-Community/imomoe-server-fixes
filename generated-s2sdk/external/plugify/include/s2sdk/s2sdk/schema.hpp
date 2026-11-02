#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: schema)

namespace s2sdk {

  using _GetSchemaOffset = int32_t (*)(const plg::string&, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_GetSchemaOffset __s2sdk_GetSchemaOffset;
namespace s2sdk {
  /**
   * @brief Get the offset of a member in a given schema class.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the member whose offset is to be retrieved.
   * @return int32: The offset of the member in the class, or -1 if the offset is not found.
   */
  inline int32_t GetSchemaOffset(const plg::string& className, const plg::string& memberName) {
    return __s2sdk_GetSchemaOffset(className, memberName);
  }

  using _GetEntSchema2 = int64_t (*)(void*, const plg::string&, const plg::string&, int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntSchema2 __s2sdk_GetEntSchema2;
namespace s2sdk {
  /**
   * @brief Retrieves an integer value from an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   * @return int64: An integer value at the given schema offset.
   */
  inline int64_t GetEntSchema2(void* entity, const plg::string& className, const plg::string& memberName, int32_t element) {
    return __s2sdk_GetEntSchema2(entity, className, memberName, element);
  }

  using _SetEntSchema2 = void (*)(void*, const plg::string&, const plg::string&, int64_t, bool, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetEntSchema2 __s2sdk_SetEntSchema2;
namespace s2sdk {
  /**
   * @brief Sets an integer value in an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param value (int64): The integer value to set.
   * @param changeState (bool): If true, change will be sent over the network.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   */
  inline void SetEntSchema2(void* entity, const plg::string& className, const plg::string& memberName, int64_t value, bool changeState, int32_t element) {
    return __s2sdk_SetEntSchema2(entity, className, memberName, value, changeState, element);
  }

  using _GetEntSchemaFloat2 = double (*)(void*, const plg::string&, const plg::string&, int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntSchemaFloat2 __s2sdk_GetEntSchemaFloat2;
namespace s2sdk {
  /**
   * @brief Retrieves a float value from an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   * @return double: A float value at the given schema offset.
   */
  inline double GetEntSchemaFloat2(void* entity, const plg::string& className, const plg::string& memberName, int32_t element) {
    return __s2sdk_GetEntSchemaFloat2(entity, className, memberName, element);
  }

  using _SetEntSchemaFloat2 = void (*)(void*, const plg::string&, const plg::string&, double, bool, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetEntSchemaFloat2 __s2sdk_SetEntSchemaFloat2;
namespace s2sdk {
  /**
   * @brief Sets a float value in an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param value (double): The float value to set.
   * @param changeState (bool): If true, change will be sent over the network.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   */
  inline void SetEntSchemaFloat2(void* entity, const plg::string& className, const plg::string& memberName, double value, bool changeState, int32_t element) {
    return __s2sdk_SetEntSchemaFloat2(entity, className, memberName, value, changeState, element);
  }

  using _GetEntSchemaVector3D2 = plg::vec3 (*)(void*, const plg::string&, const plg::string&, int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntSchemaVector3D2 __s2sdk_GetEntSchemaVector3D2;
namespace s2sdk {
  /**
   * @brief Retrieves a vector value from an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   * @return vec3: A vector value at the given schema offset.
   */
  inline plg::vec3 GetEntSchemaVector3D2(void* entity, const plg::string& className, const plg::string& memberName, int32_t element) {
    return __s2sdk_GetEntSchemaVector3D2(entity, className, memberName, element);
  }

  using _SetEntSchemaVector3D2 = void (*)(void*, const plg::string&, const plg::string&, const plg::vec3&, bool, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetEntSchemaVector3D2 __s2sdk_SetEntSchemaVector3D2;
namespace s2sdk {
  /**
   * @brief Sets a vector value in an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param value (vec3): The vector value to set.
   * @param changeState (bool): If true, change will be sent over the network.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   */
  inline void SetEntSchemaVector3D2(void* entity, const plg::string& className, const plg::string& memberName, const plg::vec3& value, bool changeState, int32_t element) {
    return __s2sdk_SetEntSchemaVector3D2(entity, className, memberName, value, changeState, element);
  }

  using _GetEntSchemaEnt2 = int32_t (*)(void*, const plg::string&, const plg::string&, int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntSchemaEnt2 __s2sdk_GetEntSchemaEnt2;
namespace s2sdk {
  /**
   * @brief Retrieves an entity handle from an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   * @return int32: A string value at the given schema offset.
   */
  inline int32_t GetEntSchemaEnt2(void* entity, const plg::string& className, const plg::string& memberName, int32_t element) {
    return __s2sdk_GetEntSchemaEnt2(entity, className, memberName, element);
  }

  using _SetEntSchemaEnt2 = void (*)(void*, const plg::string&, const plg::string&, int32_t, bool, int32_t);
}
extern "C" PLUGIN_API s2sdk::_SetEntSchemaEnt2 __s2sdk_SetEntSchemaEnt2;
namespace s2sdk {
  /**
   * @brief Sets an entity handle in an entity's schema.
   * @param entity (ptr64): Pointer to the instance of the class where the value is to be set.
   * @param className (string): The name of the class.
   * @param memberName (string): The name of the schema member.
   * @param value (int32): The entity handle to set.
   * @param changeState (bool): If true, change will be sent over the network.
   * @param element (int32): Element # (starting from 0) if schema is an array.
   */
  inline void SetEntSchemaEnt2(void* entity, const plg::string& className, const plg::string& memberName, int32_t value, bool changeState, int32_t element) {
    return __s2sdk_SetEntSchemaEnt2(entity, className, memberName, value, changeState, element);
  }

  using _NetworkStateChanged = void (*)(int32_t, const plg::string&, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_NetworkStateChanged __s2sdk_NetworkStateChanged;
namespace s2sdk {
  /**
   * @brief Updates the networked state of a schema field for a given entity handle.
   * @param entityHandle (int32): The handle of the entity from which the value is to be retrieved.
   * @param className (string): The name of the class that contains the member.
   * @param memberName (string): The name of the member to be set.
   */
  inline void NetworkStateChanged(int32_t entityHandle, const plg::string& className, const plg::string& memberName) {
    return __s2sdk_NetworkStateChanged(entityHandle, className, memberName);
  }

} // namespace s2sdk
