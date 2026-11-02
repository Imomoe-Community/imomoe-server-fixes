// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace imomoe::server_fixes::spawn_capacity {

enum class Team : std::int32_t {
	Terrorist = 2,
	CounterTerrorist = 3,
};

struct JoinRetryEpoch {
	std::uint64_t request{};
	std::uint64_t round{ 1 };
	std::uint64_t life{ 1 };

	constexpr std::uint64_t BeginRequest() noexcept {
		++request;
		if ( request == 0 ) {
			++request;
		}
		return request;
	}

	constexpr void BeginRound() noexcept {
		++round;
		if ( round == 0 ) {
			++round;
		}
		(void)BeginRequest();
	}

	constexpr void BeginLife() noexcept {
		++life;
		if ( life == 0 ) {
			++life;
		}
		(void)BeginRequest();
	}

	constexpr bool Matches( std::uint64_t requestGeneration, std::uint64_t roundGeneration,
		std::uint64_t lifeGeneration ) const noexcept {
		return request == requestGeneration && round == roundGeneration && life == lifeGeneration;
	}
};

struct JoinRetryIdentity {
	std::uint64_t requestGeneration{};
	std::uint64_t roundGeneration{};
	std::uint64_t lifeGeneration{};
	std::int32_t controllerHandle{ -1 };
	std::int32_t pawnHandle{ -1 };
	std::int32_t originalTeam{};
	bool alive{};
};

constexpr bool SameJoinRetryIdentity( const JoinRetryIdentity& expected, const JoinRetryIdentity& current ) noexcept {
	return expected.requestGeneration == current.requestGeneration &&
		expected.roundGeneration == current.roundGeneration && expected.lifeGeneration == current.lifeGeneration &&
		expected.controllerHandle == current.controllerHandle && expected.pawnHandle == current.pawnHandle &&
		expected.originalTeam == current.originalTeam && expected.alive == current.alive;
}

struct MapLifecycle {
	std::uint64_t generation{};
	bool mapEndPending{};
	bool mapStartAfterMapEnd{};

	constexpr void OnMapEnd() noexcept {
		++generation;
		if ( generation == 0 ) {
			++generation;
		}
		mapEndPending = true;
		mapStartAfterMapEnd = false;
	}

	constexpr bool OnMapStart() noexcept {
		if ( mapEndPending ) {
			mapStartAfterMapEnd = true;
			return true;
		}
		return false;
	}

	constexpr bool CanRebuildMap() const noexcept {
		return mapEndPending && mapStartAfterMapEnd;
	}

	constexpr void CompleteMapRebuild() noexcept {
		mapEndPending = false;
		mapStartAfterMapEnd = false;
	}
};

// Build 26334290 jointeam_failed reasons: team capacity is 2/3; mp_limitteams is 7/8.
inline constexpr std::int32_t kFailureTerroristFull = 2;
inline constexpr std::int32_t kFailureCounterTerroristFull = 3;
inline constexpr std::int32_t kFailureTerroristLimit = 7;
inline constexpr std::int32_t kFailureCounterTerroristLimit = 8;

constexpr bool IsPlayableTeam( std::int32_t team ) noexcept {
	return team == static_cast<std::int32_t>( Team::Terrorist ) ||
		team == static_cast<std::int32_t>( Team::CounterTerrorist );
}

constexpr bool IsSafeMapName( std::string_view map ) noexcept {
	if ( map.empty() || map.size() > 128 ) {
		return false;
	}
	for ( const auto value : map ) {
		const bool alpha = ( value >= 'a' && value <= 'z' ) || ( value >= 'A' && value <= 'Z' );
		const bool digit = value >= '0' && value <= '9';
		if ( !alpha && !digit && value != '_' && value != '-' ) {
			return false;
		}
	}
	return true;
}

constexpr bool IsCapacityFailure( Team team, std::int32_t reason ) noexcept {
	return ( team == Team::Terrorist && reason == kFailureTerroristFull ) ||
		( team == Team::CounterTerrorist && reason == kFailureCounterTerroristFull );
}

constexpr std::int32_t AdditionalPointsNeeded( std::int32_t maxPlayers, std::int32_t nativePoints,
	std::int32_t manualPoints ) noexcept {
	if ( maxPlayers <= 0 || nativePoints < 0 || manualPoints < 0 ) {
		return 0;
	}
	const auto current = static_cast<std::int64_t>( nativePoints ) + static_cast<std::int64_t>( manualPoints );
	return current < maxPlayers ? maxPlayers - static_cast<std::int32_t>( current ) : 0;
}

constexpr std::int32_t EffectiveTeamCapacity( std::int32_t spawnPoints, std::int32_t maxTeam ) noexcept {
	return spawnPoints < 0 || maxTeam < 0 ? 0 : std::min( spawnPoints, maxTeam );
}

struct RetryFacts {
	bool enabled{};
	bool connected{};
	bool sameRequestIdentity{};
	bool spawnCapacityExtended{};
	bool retryAlreadyUsed{};
	std::int32_t requestedTeam{};
	std::int32_t currentTeam{};
	std::int32_t teamPlayers{};
	std::int32_t maxPlayers{};
	std::int32_t teamCapacity{};
	std::int32_t failureReason{};
};

constexpr bool ShouldRetry( const RetryFacts& facts ) noexcept {
	if ( !facts.enabled || !facts.connected || !facts.sameRequestIdentity || !facts.spawnCapacityExtended ||
		facts.retryAlreadyUsed || !IsPlayableTeam( facts.requestedTeam ) ||
		facts.currentTeam == facts.requestedTeam || facts.maxPlayers <= 0 ||
		facts.teamPlayers >= facts.maxPlayers || facts.teamPlayers >= facts.teamCapacity ) {
		return false;
	}
	const auto team = static_cast<Team>( facts.requestedTeam );
	return IsCapacityFailure( team, facts.failureReason );
}

constexpr bool IsServerConsoleMutation( bool enabled, bool consoleContext, std::int32_t slot ) noexcept {
	return enabled && consoleContext && slot < 0;
}

} // namespace imomoe::server_fixes::spawn_capacity
