// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <string>

namespace imomoe::server_fixes::spectator_compat {

struct Status {
	bool available{};
	std::string reason;
};

Status Update( bool enabled, bool schemaQueriesReady ) noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::spectator_compat
