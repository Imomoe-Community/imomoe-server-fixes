// SPDX-License-Identifier: AGPL-3.0-or-later
#include "map_maintenance_policy.hpp"

#include <cstdlib>

int main() {
	using namespace imomoe::server_fixes::map_maintenance;
	using namespace std::chrono_literals;
	BlankMapRecovery recovery;
	const auto now = BlankMapRecovery::Clock::time_point{};

	recovery.OnNextLevel( false, now );
	if ( recovery.Take( now + 999ms, false ) || !recovery.Take( now + 1s, false ) ) {
		return EXIT_FAILURE;
	}
	recovery.OnNextLevel( false, now + 2s );
	if ( recovery.Take( now + 3s, false ) ) {
		return EXIT_FAILURE;
	}

	recovery = {};
	recovery.OnNextLevel( false, now );
	if ( recovery.Take( now + 100ms, true ) || recovery.Take( now + 2s, false ) ) {
		return EXIT_FAILURE;
	}
	recovery.OnNextLevel( false, now );
	recovery.OnNextLevel( true, now + 100ms );
	if ( recovery.Take( now + 2s, false ) || !IsStockMapArgument( "de_dust2" ) ||
		IsStockMapArgument( "de_dust2 other_arg" ) || IsStockMapArgument( "" ) ) {
		return EXIT_FAILURE;
	}

	const RefreshConfig config{ 30s, 10s, 90s, 30s, 10s, 20s, 26s };
	LongMapClock clock;
	clock.OnMapStart( now );
	if ( clock.Observe( now + 30s, 30s, false, true, {}, config ) != RefreshStep::None ||
		clock.Observe( now + 39s, 39s, false, false, {}, config ) != RefreshStep::None ||
		clock.Observe( now + 40s, 40s, false, false, {}, config ) != RefreshStep::ReloadIdle ||
		clock.Observe( now + 41s, 41s, false, false, {}, config ) != RefreshStep::None ) {
		return EXIT_FAILURE;
	}

	clock.OnMapStart( now );
	MatchState match{ false, false, 3.0f, 7, 69.0f };
	if ( clock.Observe( now + 89s, 89s, true, false, match, config ) != RefreshStep::None ||
		clock.Observe( now + 90s, 90s, true, false, match, config ) != RefreshStep::Armed ||
		clock.Observe( now + 120s, 120s, true, false, match, config ) != RefreshStep::WarnRound ||
		clock.Observe( now + 130s, 130s, true, false, match, config ) != RefreshStep::None ) {
		return EXIT_FAILURE;
	}
	++match.rounds;
	match.roundStart = 131.0f;
	if ( clock.Observe( now + 132s, 132s, true, false, match, config ) != RefreshStep::None ) {
		return EXIT_FAILURE;
	}
	++match.rounds;
	if ( clock.Observe( now + 134s, 134s, true, false, match, config ) != RefreshStep::ReloadAfterWarning ) {
		return EXIT_FAILURE;
	}

	clock.OnMapStart( now );
	if ( clock.Observe( now + 90s, 90s, true, false, match, config ) != RefreshStep::Armed ) {
		return EXIT_FAILURE;
	}
	match.start = 92.0f;
	match.rounds = 0;
	if ( clock.Observe( now + 93s, 93s, true, false, match, config ) != RefreshStep::ReloadNewMatch ) {
		return EXIT_FAILURE;
	}
	clock.OnMapStart( now );
	if ( clock.Observe( now + 90s, 90s, true, false, match, config ) != RefreshStep::Armed ||
		clock.Observe( now + 120s, 120s, true, false, match, config ) != RefreshStep::WarnRound ||
		clock.Observe( now + 130s, 130s, true, false, match, config ) != RefreshStep::None ) {
		return EXIT_FAILURE;
	}
	match.ended = true;
	if ( clock.Observe( now + 149s, 149s, true, false, match, config ) != RefreshStep::None ||
		clock.Observe( now + 150s, 150s, true, false, match, config ) != RefreshStep::None ||
		clock.Observe( now + 175s, 175s, true, false, match, config ) != RefreshStep::ReloadAfterWarning ) {
		return EXIT_FAILURE;
	}
	if ( RemainingTimeLimit( 10.0f, 180.0f, 60.0f ) != 8.0f ||
		RemainingTimeLimit( 10.0f, 180.0f, 0.0f ) != 7.0f ||
		RemainingTimeLimit( 1.0f, 180.0f, 60.0f ) != 0.1f ||
		RemainingTimeLimit( 0.0f, 180.0f, 60.0f ) != 0.0f ) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
