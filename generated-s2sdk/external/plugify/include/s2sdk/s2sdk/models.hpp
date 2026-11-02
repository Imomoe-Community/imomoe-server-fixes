#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: models)

namespace s2sdk {

  using _GetEntityAttachmentAngles = plg::vec3 (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityAttachmentAngles __s2sdk_GetEntityAttachmentAngles;
namespace s2sdk {
  /**
   * @brief Retrieves the attachment angles of an entity.
   * @param entityHandle (int32): The handle of the entity whose attachment angles are to be retrieved.
   * @param attachmentIndex (int32): The attachment index.
   * @return vec3: A vector representing the attachment angles (pitch, yaw, roll).
   */
  inline plg::vec3 GetEntityAttachmentAngles(int32_t entityHandle, int32_t attachmentIndex) {
    return __s2sdk_GetEntityAttachmentAngles(entityHandle, attachmentIndex);
  }

  using _GetEntityAttachmentOrigin = plg::vec3 (*)(int32_t, int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityAttachmentOrigin __s2sdk_GetEntityAttachmentOrigin;
namespace s2sdk {
  /**
   * @brief Retrieves the origin vector of an entity's attachment.
   * @param entityHandle (int32): The handle of the entity whose attachment origin is to be retrieved.
   * @param attachmentIndex (int32): The attachment index.
   * @return vec3: A vector representing the origin of the attachment.
   */
  inline plg::vec3 GetEntityAttachmentOrigin(int32_t entityHandle, int32_t attachmentIndex) {
    return __s2sdk_GetEntityAttachmentOrigin(entityHandle, attachmentIndex);
  }

  using _GetEntityModelScale = float (*)(int32_t);
}
extern "C" PLUGIN_API s2sdk::_GetEntityModelScale __s2sdk_GetEntityModelScale;
namespace s2sdk {
  /**
   * @brief Retrieves the model scale of an entity.
   * @param entityHandle (int32): The handle of the entity.
   * @return float: The model scale factor.
   */
  inline float GetEntityModelScale(int32_t entityHandle) {
    return __s2sdk_GetEntityModelScale(entityHandle);
  }

  using _SetEntityModelScale = void (*)(int32_t, float);
}
extern "C" PLUGIN_API s2sdk::_SetEntityModelScale __s2sdk_SetEntityModelScale;
namespace s2sdk {
  /**
   * @brief Sets the model scale of an entity.
   * @param entityHandle (int32): The handle of the entity.
   * @param scale (float): The new scale factor.
   */
  inline void SetEntityModelScale(int32_t entityHandle, float scale) {
    return __s2sdk_SetEntityModelScale(entityHandle, scale);
  }

  using _ScriptLookupAttachment = int32_t (*)(int32_t, const plg::string&);
}
extern "C" PLUGIN_API s2sdk::_ScriptLookupAttachment __s2sdk_ScriptLookupAttachment;
namespace s2sdk {
  /**
   * @brief Retrieves an attachment index by name.
   * @param entityHandle (int32): The handle of the entity.
   * @param attachmentName (string): The name of the attachment.
   * @return int32: The attachment index, or -1 if not found.
   */
  inline int32_t ScriptLookupAttachment(int32_t entityHandle, const plg::string& attachmentName) {
    return __s2sdk_ScriptLookupAttachment(entityHandle, attachmentName);
  }

} // namespace s2sdk
