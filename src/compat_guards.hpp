// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

namespace imomoe::server_fixes::compat_guards {

struct Status {
	const char* name;
	bool enabled;
	bool available;
	const char* reason;
};

void Init() noexcept;
void Update( bool mapReady ) noexcept;
void OnMapEnd() noexcept;
bool Shutdown() noexcept;
bool AllowTeamSwitch( int slot ) noexcept;
Status VoiceStatus() noexcept;
Status TeamStatus() noexcept;
Status NoclipStatus() noexcept;
Status WorkshopVoiceStatus() noexcept;
Status GhostStatus() noexcept;

} // namespace imomoe::server_fixes::compat_guards
