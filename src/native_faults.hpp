// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

namespace imomoe::server_fixes::native_faults {

enum class Fix { Beam, Filter, Equip, FreeClient, HammerId, CstvBotQuota, WaterJump, JumpSpam, KeyValues, BlankMap, SlowAnimation, RampBug, Count };

struct Status {
	bool available;
	const char* reason;
};

void Init() noexcept;
Status Update( Fix fix, bool enabled, bool mapReady ) noexcept;
void OnMapEnd() noexcept;
bool Shutdown() noexcept;

} // namespace imomoe::server_fixes::native_faults
