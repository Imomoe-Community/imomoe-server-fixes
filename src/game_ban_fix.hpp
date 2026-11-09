// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

namespace imomoe::server_fixes::game_ban_fix {

struct Status {
	bool available;
	const char* reason;
};

Status Update( bool enabled ) noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::game_ban_fix
