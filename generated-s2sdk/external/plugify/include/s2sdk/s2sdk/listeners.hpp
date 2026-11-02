#pragma once

#include "enums.hpp"
#include "aliases.hpp"
#include "delegates.hpp"
#include <plugin_export.h>

// Generated from s2sdk.pplugin (group: listeners)

namespace s2sdk {

  using _OnClientDisconnect_Register = void (*)(OnClientDisconnectCallback);
}
extern "C" PLUGIN_API s2sdk::_OnClientDisconnect_Register __s2sdk_OnClientDisconnect_Register;
namespace s2sdk {
  /**
   * @brief Register callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnClientDisconnect_Register(OnClientDisconnectCallback callback) {
    return __s2sdk_OnClientDisconnect_Register(callback);
  }

  using _OnClientDisconnect_Unregister = void (*)(OnClientDisconnectCallback);
}
extern "C" PLUGIN_API s2sdk::_OnClientDisconnect_Unregister __s2sdk_OnClientDisconnect_Unregister;
namespace s2sdk {
  /**
   * @brief Unregister callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnClientDisconnect_Unregister(OnClientDisconnectCallback callback) {
    return __s2sdk_OnClientDisconnect_Unregister(callback);
  }

  using _OnBuildGameSessionManifest_Register = void (*)(OnBuildGameSessionManifestCallback);
}
extern "C" PLUGIN_API s2sdk::_OnBuildGameSessionManifest_Register __s2sdk_OnBuildGameSessionManifest_Register;
namespace s2sdk {
  /**
   * @brief Register callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnBuildGameSessionManifest_Register(OnBuildGameSessionManifestCallback callback) {
    return __s2sdk_OnBuildGameSessionManifest_Register(callback);
  }

  using _OnBuildGameSessionManifest_Unregister = void (*)(OnBuildGameSessionManifestCallback);
}
extern "C" PLUGIN_API s2sdk::_OnBuildGameSessionManifest_Unregister __s2sdk_OnBuildGameSessionManifest_Unregister;
namespace s2sdk {
  /**
   * @brief Unregister callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnBuildGameSessionManifest_Unregister(OnBuildGameSessionManifestCallback callback) {
    return __s2sdk_OnBuildGameSessionManifest_Unregister(callback);
  }

  using _OnMapStart_Register = void (*)(OnMapEndCallback);
}
extern "C" PLUGIN_API s2sdk::_OnMapStart_Register __s2sdk_OnMapStart_Register;
namespace s2sdk {
  /**
   * @brief Register callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnMapStart_Register(OnMapEndCallback callback) {
    return __s2sdk_OnMapStart_Register(callback);
  }

  using _OnMapStart_Unregister = void (*)(OnMapEndCallback);
}
extern "C" PLUGIN_API s2sdk::_OnMapStart_Unregister __s2sdk_OnMapStart_Unregister;
namespace s2sdk {
  /**
   * @brief Unregister callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnMapStart_Unregister(OnMapEndCallback callback) {
    return __s2sdk_OnMapStart_Unregister(callback);
  }

  using _OnMapEnd_Register = void (*)(OnMapEndCallback);
}
extern "C" PLUGIN_API s2sdk::_OnMapEnd_Register __s2sdk_OnMapEnd_Register;
namespace s2sdk {
  /**
   * @brief Register callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnMapEnd_Register(OnMapEndCallback callback) {
    return __s2sdk_OnMapEnd_Register(callback);
  }

  using _OnMapEnd_Unregister = void (*)(OnMapEndCallback);
}
extern "C" PLUGIN_API s2sdk::_OnMapEnd_Unregister __s2sdk_OnMapEnd_Unregister;
namespace s2sdk {
  /**
   * @brief Unregister callback to event.
   * @param callback (function): Function callback.
   */
  inline void OnMapEnd_Unregister(OnMapEndCallback callback) {
    return __s2sdk_OnMapEnd_Unregister(callback);
  }

} // namespace s2sdk
