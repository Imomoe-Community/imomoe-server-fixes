// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "native_faults.hpp"

namespace imomoe::server_fixes::map_maintenance {

void Init() noexcept;
native_faults::Status UpdateBlankMap( bool enabled, bool mapReady, bool runRecovery ) noexcept;
native_faults::Status UpdateSlowAnimation( bool enabled, bool mapReady, bool runRecovery ) noexcept;
void OnMapEnd() noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::map_maintenance
