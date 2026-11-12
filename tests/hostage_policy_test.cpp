// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "hostage_policy.hpp"
#include "runtime_lifecycle.hpp"
#include "hostage_dispatch_read.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void Check( bool condition, std::string_view name ) {
	if ( !condition ) {
		std::cerr << "FAILED: " << name << '\n';
		std::exit( 1 );
	}
}

using namespace imomoe::server_fixes::hostage;

AttemptKey Key() {
	return { 1, 2, 3, 4, 50, 60, 70, 80, 90 };
}

RescueFacts LegalFacts() {
	return { true, true, true, true, true, true, true, true, true, true, true, false, true };
}

} // namespace

int main() {
	imomoe::server_fixes::runtime_lifecycle::SchemaReadiness schemaReadiness;
	int schemaQueries{};
	Check( !schemaReadiness.CanQuery() && !schemaReadiness.RunSchemaDependent( [&] { ++schemaQueries; } ) &&
		schemaQueries == 0, "schema access and its work are blocked before the first map lifecycle callback" );
	schemaReadiness.OnMapStart();
	Check( schemaReadiness.CanQuery() && schemaReadiness.RunSchemaDependent( [&] { ++schemaQueries; } ) &&
		schemaQueries == 1, "the post-ActivateServer map-start callback enables schema-dependent work" );
	schemaReadiness.OnMapEnd();
	Check( !schemaReadiness.CanQuery() && !schemaReadiness.RunSchemaDependent( [&] { ++schemaQueries; } ) &&
		schemaQueries == 1, "map end closes the schema access window during map teardown" );
	schemaReadiness.OnMapStart();
	Check( schemaReadiness.CanQuery() && schemaReadiness.RunSchemaDependent( [&] { ++schemaQueries; } ) &&
		schemaQueries == 2, "the next map-start callback re-enables schema-dependent work before any round" );

	AttemptLedger ledger;
	auto normal = LegalFacts();
	normal.nativeRescueHandlerWasInvoked = true;
	Check( ledger.Finalize( Key(), normal ) == FinalizeAction::NativeHandlerObserved,
		"native handler call, including a rejecting handler, causes zero fallback inputs" );
	Check( ledger.Size() == 0, "normal native rescue path reserves no compensation" );
	auto vetoedTouch = LegalFacts();
	vetoedTouch.nativePawnRescueCallReached = false;
	Check( ledger.Finalize( { 1, 2, 3, 4, 49, 60, 70, 79, 90 }, vetoedTouch ) == FinalizeAction::Skip,
		"a foreign Touch Pre veto cannot reach the native pawn rescue callsite or trigger compensation" );
	Check( ledger.Size() == 0, "Touch Post after foreign Supercede submits no fallback input" );
	auto vetoedPawnBody = LegalFacts();
	vetoedPawnBody.nativePawnRouteObservationCertain = false;
	Check( ledger.Finalize( { 1, 2, 3, 4, 48, 60, 70, 78, 90 }, vetoedPawnBody ) == FinalizeAction::Skip,
		"a detoured pawn body with an inner Pre Supercede is uncertain, so its reached callsite cannot trigger fallback" );
	Check( ledger.Size() == 0, "inner body Supercede submits no rescue input" );

	constexpr std::size_t dispatchOffset = 16;
	alignas( std::uintptr_t ) std::array<std::byte, dispatchOffset + sizeof( void* )> dispatchArgument{};
	std::int32_t dispatchHandle = 0x71234567;
	void* handleStorage = &dispatchHandle;
	std::memcpy( dispatchArgument.data() + dispatchOffset, &handleStorage, sizeof( handleStorage ) );
	std::int32_t decodedHandle{};
	Check( ReadHostageDispatchTarget( dispatchArgument.data(), dispatchOffset, decodedHandle ) && decodedHandle == dispatchHandle,
		"the checked dispatch reader copies a readable full entity handle" );
	Check( !ReadHostageDispatchTarget( reinterpret_cast<const void*>( std::uintptr_t{ 1 } ), dispatchOffset, decodedHandle ),
		"a dispatch argument below the user address range is rejected" );
	void* unreadableHandleStorage = reinterpret_cast<void*>( std::uintptr_t{ 1 } );
	std::memcpy( dispatchArgument.data() + dispatchOffset, &unreadableHandleStorage, sizeof( unreadableHandleStorage ) );
	Check( !ReadHostageDispatchTarget( dispatchArgument.data(), dispatchOffset, decodedHandle ),
		"a nested pointer below the user address range is rejected" );
	Check( IsMatchingHostageTarget( Key(), 80 ), "the Hostage API target handle matches this carried hostage" );
	Check( !IsMatchingHostageTarget( Key(), 81 ) && !IsMatchingHostageTarget( Key(), -1 ),
		"a different or invalid target cannot suppress this hostage's missing-dispatch fallback" );
	auto highBitHandle = Key();
	highBitHandle.hostage = std::bit_cast<std::int32_t>( std::uint32_t{ 0x80012345 } );
	Check( IsEntityHandleValuePresent( highBitHandle.hostage ) &&
		IsMatchingHostageTarget( highBitHandle, highBitHandle.hostage ),
		"a valid high-bit full entity handle remains present and matches despite its signed ABI view" );
	Check( !IsEntityHandleValuePresent( -1 ), "only INVALID_EHANDLE_INDEX is treated as the missing handle sentinel" );

	auto uncertainObservation = LegalFacts();
	uncertainObservation.nativeRescueHandlerObservationCertain = false;
	Check( ledger.Finalize( { 1, 2, 3, 4, 49, 60, 70, 79, 90 }, uncertainObservation ) == FinalizeAction::Skip,
		"an unclassifiable native handler call cannot be mistaken for a confirmed missing dispatch" );
	Check( ledger.Size() == 0, "uncertain handler observation reserves no compensation" );

	const auto missed = LegalFacts();
	Check( ledger.Finalize( Key(), missed ) == FinalizeAction::SubmitNativeInput,
		"confirmed missing native handler submits one necessary input" );
	Check( ledger.Size() == 1, "fallback reserves its complete identity tuple before calling the engine" );
	Check( ledger.Finalize( Key(), missed ) == FinalizeAction::Duplicate,
		"repeat touch and reentry cannot submit a second input for the same life" );
	Check( ledger.Size() == 1, "duplicate touch does not increase fallback count" );

	auto successful = LegalFacts();
	successful.hostageIsNotRescued = false;
	Check( ledger.Finalize( { 1, 2, 3, 4, 50, 60, 70, 81, 90 }, successful ) == FinalizeAction::Skip,
		"a rescued hostage is not submitted again" );

	const auto teamDeathmatchWinDisabled = LegalFacts();
	Check( ledger.Finalize( { 1, 2, 3, 4, 51, 60, 70, 82, 90 }, teamDeathmatchWinDisabled ) == FinalizeAction::SubmitNativeInput,
		"disabled native round wins do not suppress a necessary engine rescue input" );
	Check( ledger.Size() == 2, "one input is reserved per complete hostage and zone identity" );
	// The only emitted action is the engine input; this policy has no round-end or win action.

	Check( !ShouldPatchTTargetGate( false, false ), "disabled hostage capability restores the T gate" );
	Check( !ShouldPatchTTargetGate( true, true ), "mp_only_cts_rescue_hostages keeps the native CT-only gate" );
	Check( ShouldPatchTTargetGate( true, false ), "enabled capability opens the T selector only when native policy allows it" );
	Check( !ShouldPatchTTargetGateWhenPolicyUnknown( true, false, false ),
		"unknown mp_only_cts_rescue_hostages restores the native selector" );
	Check( ShouldPatchTTargetGateWhenPolicyUnknown( true, true, false ),
		"known false mp_only_cts_rescue_hostages permits the T selector patch" );
	Check( !PatchedTTargetGateAllows( 0 ) && !PatchedTTargetGateAllows( 1 ), "patched selector still rejects spectators and unassigned players" );
	Check( PatchedTTargetGateAllows( 2 ) && PatchedTTargetGateAllows( 3 ), "patched selector allows T and preserves CT" );
	Check( CanObserveRescueTouch( true, true, true ), "active hostage module observes its Touch callbacks" );
	Check( !CanObserveRescueTouch( false, true, true ) && !CanObserveRescueTouch( true, false, true ) &&
		!CanObserveRescueTouch( true, true, false ),
		"shutdown, disabled configuration, or incomplete observer setup cannot enter CT compensation" );

	std::cout << "hostage compensation and T-gate policy checks passed\n";
	return 0;
}
