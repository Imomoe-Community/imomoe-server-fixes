// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ramp_policy.hpp"

#include <cstdlib>

int main() {
	using namespace imomoe::server_fixes::ramp;
	const Vec ramp{ 0.8f, 0.0f, 0.6f };
	const Vec floor{ 0.6f, 0.0f, 0.8f };
	const Sweep free{ {}, 1.0f, false };
	const Sweep bad{ {}, 0.0f, true };
	const Sweep rampHit{ ramp, 0.5f, false };
	const Sweep falseFloor{ floor, 0.5f, false };
	if ( !NeedsSlideRepair( ramp, bad ) || !NeedsSlideRepair( ramp, falseFloor ) ||
		NeedsSlideRepair( ramp, rampHit ) || NeedsSlideRepair( ramp, free ) ||
		NeedsSlideRepair( floor, bad ) || !FollowsRamp( ramp, rampHit ) ||
		FollowsRamp( ramp, falseFloor ) || FollowsRamp( ramp, bad ) ||
		!NeedsGroundRepair( ramp, falseFloor, -90.0f ) || NeedsGroundRepair( ramp, falseFloor, -20.0f ) ) {
		return EXIT_FAILURE;
	}
	int probes{};
	if ( !FindCorrection( ramp, [&]( Vec shift ) {
		++probes;
		return shift.Dot( ramp ) > kPierce * 0.7f;
	} ) || probes != 3 ) {
		return EXIT_FAILURE;
	}
	probes = 0;
	if ( FindCorrection( ramp, [&]( Vec shift ) {
		++probes;
		return shift.Dot( ramp ) <= 0.0f || shift.Dot( shift ) > kPierce * kPierce * 1.01f;
	} ) || probes == 0 || probes > 108 ) {
		return EXIT_FAILURE;
	}
	const auto landing = RestoreLandingSpeed( { 30.0f, 0.0f, -100.0f }, { 24.0f, 0.0f, 0.0f }, floor );
	if ( !landing || landing->x <= 30.0f || landing->z != 0.0f ||
		RestoreLandingSpeed( { 30.0f, 0.0f, -100.0f }, { 90.0f, 0.0f, 0.0f }, floor ) ||
		RestoreLandingSpeed( { 30.0f, 0.0f, -100.0f }, {}, { 0.0f, 0.0f, 1.0f } ) ) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
