// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "spawn_capacity_policy.hpp"
#include "spawn_capacity_storage.hpp"

#include <cstdlib>
#include <iostream>
#include <locale>
#include <sstream>

using imomoe::server_fixes::spawn_capacity::AdditionalPointsNeeded;
using imomoe::server_fixes::spawn_capacity::EffectiveTeamCapacity;
using imomoe::server_fixes::spawn_capacity::IsCapacityFailure;
using imomoe::server_fixes::spawn_capacity::IsSafeMapName;
using imomoe::server_fixes::spawn_capacity::IsServerConsoleMutation;
using imomoe::server_fixes::spawn_capacity::JoinRetryEpoch;
using imomoe::server_fixes::spawn_capacity::JoinRetryIdentity;
using imomoe::server_fixes::spawn_capacity::MapLifecycle;
using imomoe::server_fixes::spawn_capacity::ManualPointRow;
using imomoe::server_fixes::spawn_capacity::ParseManualPointRow;
using imomoe::server_fixes::spawn_capacity::RetryFacts;
using imomoe::server_fixes::spawn_capacity::ShouldRetry;
using imomoe::server_fixes::spawn_capacity::SameJoinRetryIdentity;
using imomoe::server_fixes::spawn_capacity::Team;
using imomoe::server_fixes::spawn_capacity::WriteManualPointRow;

namespace {

void Require( bool condition, const char* message ) {
	if ( !condition ) {
		std::cerr << message << '\n';
		std::exit( EXIT_FAILURE );
	}
}

void TestCapacityUsesActualSlotLimit() {
	Require( AdditionalPointsNeeded( 32, 10, 3 ) == 19, "capacity must fill to actual maxplayers" );
	Require( AdditionalPointsNeeded( 16, 10, 3 ) == 3, "capacity must follow a lower maxplayers value" );
	Require( AdditionalPointsNeeded( 10, 10, 0 ) == 0, "existing points at capacity need no duplicates" );
	Require( AdditionalPointsNeeded( 8, 10, 2 ) == 0, "point counts above maxplayers need no duplicates" );
	Require( AdditionalPointsNeeded( 0, 1, 0 ) == 0, "unknown maxplayers must not create points" );
	Require( AdditionalPointsNeeded( 32, -1, 0 ) == 0, "invalid point counts must not create points" );
	Require( AdditionalPointsNeeded( 32, 2147483647, 2147483647 ) == 0,
		"large combined counts must not overflow into extra capacity" );
	Require( EffectiveTeamCapacity( 19, 32 ) == 19 && EffectiveTeamCapacity( 32, 16 ) == 16,
		"native allowance is the lesser of valid spawn count and the unchanged team maximum" );
	Require( EffectiveTeamCapacity( -1, 16 ) == 0 && EffectiveTeamCapacity( 16, -1 ) == 0,
		"invalid native capacity readbacks must not create an allowance" );
}

void TestOnlyNativeTeamCapacityFailuresAreRetryable() {
	using namespace imomoe::server_fixes::spawn_capacity;
	Require( IsCapacityFailure( Team::Terrorist, kFailureTerroristFull ), "T full reason should be recognized" );
	Require( IsCapacityFailure( Team::CounterTerrorist, kFailureCounterTerroristFull ),
		"CT full reason should be recognized" );
	Require( !IsCapacityFailure( Team::Terrorist, kFailureCounterTerroristFull ),
		"a mismatched target team must not be retried" );
	Require( !IsCapacityFailure( Team::Terrorist, kFailureTerroristLimit ),
		"mp_limitteams rejection must not be retried" );
	Require( !IsCapacityFailure( Team::CounterTerrorist, kFailureCounterTerroristLimit ),
		"CT mp_limitteams rejection must not be retried" );
	Require( !IsCapacityFailure( Team::Terrorist, 4 ),
		"spectator/other rejection must not be treated as a team capacity failure" );

	RetryFacts valid{
		.enabled = true,
		.connected = true,
		.sameRequestIdentity = true,
		.spawnCapacityExtended = true,
		.retryAlreadyUsed = false,
		.requestedTeam = static_cast<int>( Team::Terrorist ),
		.currentTeam = 0,
		.teamPlayers = 10,
		.maxPlayers = 32,
		.teamCapacity = 32,
		.failureReason = kFailureTerroristFull,
	};
	Require( ShouldRetry( valid ), "one legitimate capacity-rejected request should be retryable" );

	valid.enabled = false;
	Require( !ShouldRetry( valid ), "disabled module must not retry" );
	valid.enabled = true;
	valid.connected = false;
	Require( !ShouldRetry( valid ), "disconnected requester must not retry" );
	valid.connected = true;
	valid.sameRequestIdentity = false;
	Require( !ShouldRetry( valid ), "reused slot identity must not retry" );
	valid.sameRequestIdentity = true;
	valid.spawnCapacityExtended = false;
	Require( !ShouldRetry( valid ), "failed point creation must not retry" );
	valid.spawnCapacityExtended = true;
	valid.retryAlreadyUsed = true;
	Require( !ShouldRetry( valid ), "a rejected retry must not trigger another retry" );
	valid.retryAlreadyUsed = false;
	valid.failureReason = kFailureTerroristLimit;
	Require( !ShouldRetry( valid ), "team-balancing rejection must not retry" );
	valid.failureReason = kFailureTerroristFull;
	valid.currentTeam = static_cast<int>( Team::Terrorist );
	Require( !ShouldRetry( valid ), "a player already on the requested team must not retry" );
	valid.currentTeam = 0;
	valid.teamPlayers = 32;
	Require( !ShouldRetry( valid ), "actual server capacity must not be bypassed" );
	valid.teamPlayers = 10;
	valid.teamCapacity = 10;
	Require( !ShouldRetry( valid ), "the retry must not bypass the native effective team allowance" );
}

void TestManualCommandsAreServerConsoleOnly() {
	Require( IsServerConsoleMutation( true, true, -1 ), "server console may administer points" );
	Require( !IsServerConsoleMutation( true, true, 0 ), "player console may not mutate map points" );
	Require( !IsServerConsoleMutation( true, false, -1 ), "chat invocation may not mutate map points" );
	Require( !IsServerConsoleMutation( false, true, -1 ), "disabled module may not mutate map points" );
}

void TestMapNamesStayWithinOnePerMapFile() {
	Require( IsSafeMapName( "de_poseidon" ), "ordinary map names should be accepted" );
	Require( IsSafeMapName( "workshop-123456-map_name" ), "workshop-style map names should be accepted" );
	Require( !IsSafeMapName( "" ), "empty map names should be rejected" );
	Require( !IsSafeMapName( "../cfg/server" ), "path separators must not enter the save path" );
	Require( !IsSafeMapName( "map name" ), "spaces must not enter the save path" );
}

void TestSavedManualPointRowLoadsWithoutAnExtraColumn() {
	const ManualPointRow saved{
		.id = 17,
		.team = static_cast<int>( Team::CounterTerrorist ),
		.x = 128.25f,
		.y = -256.5f,
		.z = 32.0f,
		.pitch = 0.0f,
		.yaw = 180.0f,
		.roll = -0.25f,
	};
	std::ostringstream file;
	file.imbue( std::locale::classic() );
	Require( WriteManualPointRow( file, saved ), "the production serializer should write a manual point row" );
	const auto serialized = file.str();
	Require( !serialized.empty() && serialized.back() == '\n' &&
		serialized.find( "\n" ) == serialized.size() - 1,
		"the saved row should have its normal final newline and no extra column" );
	const auto line = serialized.substr( 0, serialized.size() - 1 );
	ManualPointRow loaded;
	Require( ParseManualPointRow( line, loaded ), "the production parser should accept the serializer's exact row" );
	Require( loaded.id == saved.id && loaded.team == saved.team && loaded.x == saved.x &&
		loaded.y == saved.y && loaded.z == saved.z && loaded.pitch == saved.pitch &&
		loaded.yaw == saved.yaw && loaded.roll == saved.roll,
		"save/load should preserve the full manual point" );
	Require( ParseManualPointRow( line + "   ", loaded ), "trailing whitespace should remain valid" );
	Require( !ParseManualPointRow( line + " extra", loaded ), "an extra saved column should be rejected" );
	Require( !ParseManualPointRow( "17 3 128.25 -256.5 32 0 180", loaded ),
		"a truncated saved row should be rejected" );
}

void TestNewJoinRequestsAndRoundBoundariesInvalidatePendingRetry() {
	JoinRetryEpoch epoch;
	const auto failedTeamRequest = epoch.BeginRequest();
	const JoinRetryIdentity pending{
		.requestGeneration = failedTeamRequest,
		.roundGeneration = epoch.round,
		.lifeGeneration = epoch.life,
		.controllerHandle = -0x1001,
		.pawnHandle = -0x2001,
		.originalTeam = static_cast<int>( Team::CounterTerrorist ),
		.alive = true,
	};
	Require( SameJoinRetryIdentity( pending, pending ), "the rejected command identity should match before queue dispatch" );

	// Every subsequent jointeam Pre callback advances this epoch before parsing its arguments.
	(void)epoch.BeginRequest(); // e.g. jointeam 3 succeeds, or jointeam 1/0 is a native request.
	const JoinRetryIdentity laterRequest{
		.requestGeneration = epoch.request,
		.roundGeneration = epoch.round,
		.lifeGeneration = epoch.life,
		.controllerHandle = pending.controllerHandle,
		.pawnHandle = pending.pawnHandle,
		.originalTeam = pending.originalTeam,
		.alive = pending.alive,
	};
	Require( !SameJoinRetryIdentity( pending, laterRequest ),
		"a queued capacity failure must not survive any later jointeam request from that slot" );

	const auto sameRequest = epoch.BeginRequest();
	const JoinRetryIdentity sameLife{
		.requestGeneration = sameRequest,
		.roundGeneration = epoch.round,
		.lifeGeneration = epoch.life,
		.controllerHandle = pending.controllerHandle,
		.pawnHandle = pending.pawnHandle,
		.originalTeam = pending.originalTeam,
		.alive = pending.alive,
	};
	Require( SameJoinRetryIdentity( sameLife, sameLife ), "the same request and full life identity remains eligible" );
	Require( !SameJoinRetryIdentity( sameLife, JoinRetryIdentity{
		.requestGeneration = sameRequest,
		.roundGeneration = epoch.round,
		.lifeGeneration = epoch.life,
		.controllerHandle = pending.controllerHandle,
		.pawnHandle = pending.pawnHandle + 1,
		.originalTeam = pending.originalTeam,
		.alive = pending.alive,
	} ), "a replacement pawn handle must invalidate the queued request" );
	Require( !SameJoinRetryIdentity( sameLife, JoinRetryIdentity{
		.requestGeneration = sameRequest,
		.roundGeneration = epoch.round,
		.lifeGeneration = epoch.life,
		.controllerHandle = pending.controllerHandle,
		.pawnHandle = pending.pawnHandle,
		.originalTeam = pending.originalTeam,
		.alive = false,
	} ), "a death or respawn state change must invalidate the queued request" );

	const JoinRetryIdentity samePawnNewLife{
		.requestGeneration = sameRequest,
		.roundGeneration = epoch.round,
		.lifeGeneration = epoch.life + 1,
		.controllerHandle = pending.controllerHandle,
		.pawnHandle = pending.pawnHandle,
		.originalTeam = pending.originalTeam,
		.alive = pending.alive,
	};
	Require( !SameJoinRetryIdentity( sameLife, samePawnNewLife ),
		"a death/respawn event must invalidate a request even if the Pawn handle and alive bit are reused" );

	epoch.BeginRound();
	const JoinRetryIdentity nextRound{
		.requestGeneration = sameRequest,
		.roundGeneration = epoch.round,
		.lifeGeneration = epoch.life,
		.controllerHandle = pending.controllerHandle,
		.pawnHandle = pending.pawnHandle,
		.originalTeam = pending.originalTeam,
		.alive = pending.alive,
	};
	Require( !SameJoinRetryIdentity( sameLife, nextRound ), "a round boundary must invalidate pending join retries" );
}

void TestSameNameMapEndRebuildsAtMapStart() {
	struct FakeMap {
		MapLifecycle lifecycle;
		std::string currentMap{ "de_poseidon" };
		int nativePoints{ 10 };
		int savedManualPoints{ 1 };
		int maxPlayers{ 32 };
		int ownedManualEntities{ 1 };
		int ownedAutomaticEntities{ 21 };
		bool ready{ true };

		void OnMapEndCallback() {
			lifecycle.OnMapEnd();
			ready = false;
			ownedManualEntities = 0;
			ownedAutomaticEntities = 0;
		}

		bool OnMapStartCallback() {
			if ( !lifecycle.OnMapStart() ) {
				return false;
			}
			ownedManualEntities = savedManualPoints;
			ownedAutomaticEntities = AdditionalPointsNeeded( maxPlayers, nativePoints, savedManualPoints );
			ready = nativePoints + ownedManualEntities + ownedAutomaticEntities >= maxPlayers;
			lifecycle.CompleteMapRebuild();
			return true;
		}
	} fake;

	fake.OnMapEndCallback();
	Require( !fake.ready, "map end makes the old entity inventory unavailable" );
	Require( fake.OnMapStartCallback(), "the map-start callback should rebuild before a team round even when the map name is unchanged" );
	Require( fake.currentMap == "de_poseidon" && fake.ready && fake.ownedManualEntities == 1 &&
		fake.ownedAutomaticEntities == 21,
		"a same-name reload restores saved manual points and reconciles automatic entities once" );
	Require( !fake.OnMapStartCallback() && fake.ownedAutomaticEntities == 21,
		"an ordinary map-start observation does not rebuild or duplicate the same points" );
}

} // namespace

int main() {
	TestCapacityUsesActualSlotLimit();
	TestOnlyNativeTeamCapacityFailuresAreRetryable();
	TestManualCommandsAreServerConsoleOnly();
	TestMapNamesStayWithinOnePerMapFile();
	TestSavedManualPointRowLoadsWithoutAnExtraColumn();
	TestNewJoinRequestsAndRoundBoundariesInvalidatePendingRetry();
	TestSameNameMapEndRebuildsAtMapStart();
	std::cout << "spawn capacity policy checks passed\n";
}
