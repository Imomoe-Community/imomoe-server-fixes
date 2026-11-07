// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace imomoe::server_fixes::hostage {

struct Status {
	bool available{};
	std::string reason;
};

Status Update( bool enabled, std::uint64_t enabledConVarHandle ) noexcept;
void OnMapEnd() noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::hostage
