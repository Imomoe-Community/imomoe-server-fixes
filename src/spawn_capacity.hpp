// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <string>

namespace imomoe::server_fixes::spawn_capacity {

struct Status {
	bool available{};
	std::string reason;
};

Status Update( bool enabled, bool includeDiagnostics = false ) noexcept;
bool ObservesJoinCommands() noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::spawn_capacity
