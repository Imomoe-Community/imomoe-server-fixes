// SPDX-License-Identifier: AGPL-3.0-or-later
#include "compat_guard_policy.hpp"

#include <cstdlib>

int main() {
	using namespace imomoe::server_fixes::compat_guards;
	using namespace std::chrono_literals;
	const auto now = Allowance::Clock::time_point{};
	Allowance oneClient;
	Allowance otherClient;
	if ( !oneClient.Take( now, 10s, 2 ) || !oneClient.Take( now + 1s, 10s, 2 ) ||
		oneClient.Take( now + 9s, 10s, 2 ) || !otherClient.Take( now + 9s, 10s, 2 ) ||
		!oneClient.Take( now + 10s, 10s, 2 ) ) {
		return EXIT_FAILURE;
	}
	if ( !HasValidVoiceShape( 240, 0, 4096, 64 ) || !HasValidVoiceShape( 240, 2, 4096, 64 ) ||
		HasValidVoiceShape( 240, 2, 128, 64 ) || HasValidVoiceShape( 0, 0, 4096, 64 ) ) {
		return EXIT_FAILURE;
	}
	TokenBucket session;
	TokenBucket separateSession;
	if ( !session.Take( now, 4, 2 ) || !session.Take( now, 4, 2 ) ||
		session.Take( now + 125ms, 4, 2 ) || !separateSession.Take( now + 125ms, 4, 2 ) ||
		!session.Take( now + 250ms, 4, 2 ) || session.Take( now + 250ms, 4, 2 ) ||
		!session.Take( now + 10s, 4, 2 ) || !session.Take( now + 10s, 4, 2 ) ||
		session.Take( now + 10s, 4, 2 ) ) {
		return EXIT_FAILURE;
	}
	KeyValuesBudget slot;
	if ( slot.Take( 3, 129, now, 4, 1, 128 ) || !slot.Take( 3, 128, now, 4, 1, 128 ) ||
		slot.Take( 3, 16, now, 4, 1, 128 ) || !slot.Take( 4, 16, now, 4, 1, 128 ) ) {
		return EXIT_FAILURE;
	}
	slot = {};
	if ( !slot.Take( 4, 16, now, 4, 1, 128 ) ) {
		return EXIT_FAILURE;
	}
	if ( VoiceStreamId( 1, 0, 1 ) == VoiceStreamId( 1, 0, 2 ) ||
		VoiceStreamId( 1, 0, 1 ) == VoiceStreamId( 1, 1, 1 ) ||
		VoiceStreamId( 1, 0, 1 ) == VoiceStreamId( 2, 0, 1 ) ) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
