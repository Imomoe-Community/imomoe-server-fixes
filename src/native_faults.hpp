// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

namespace imomoe::server_fixes::native_faults {

enum class Fix { Beam, Filter, Equip, FreeClient, Count };

struct Status {
	bool available;
	const char* reason;
};

Status Update( Fix fix, bool enabled ) noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::native_faults
