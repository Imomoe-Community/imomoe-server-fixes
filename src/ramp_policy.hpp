// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cmath>
#include <optional>

namespace imomoe::server_fixes::ramp {

// Collision-normal tolerance and subunit nudge used by the Source 2 ramp fix.
inline constexpr float kSamePlane = 0.98f;
inline constexpr float kStandableZ = 0.7f;
inline constexpr float kPierce = 0.0625f;

struct Vec {
	float x{}, y{}, z{};
	constexpr float Dot( Vec rhs ) const noexcept { return x * rhs.x + y * rhs.y + z * rhs.z; }
	constexpr Vec operator-( Vec rhs ) const noexcept { return { x - rhs.x, y - rhs.y, z - rhs.z }; }
	constexpr Vec operator*( float scale ) const noexcept { return { x * scale, y * scale, z * scale }; }
	constexpr float HorizontalSquared() const noexcept { return x * x + y * y; }
};

struct Sweep {
	Vec normal;
	float fraction;
	bool startsSolid;
};

inline bool HasPlane( Vec normal ) noexcept {
	const float length = normal.Dot( normal );
	return length >= 0.98f && length <= 1.02f;
}

inline bool IsRamp( Vec normal, float standableZ = kStandableZ ) noexcept {
	return normal.z > 0.0f && normal.z < standableZ && HasPlane( normal );
}

inline bool Valid( Sweep hit ) noexcept {
	return !hit.startsSolid && hit.fraction >= 0.0f && hit.fraction <= 1.0f &&
		( hit.fraction == 1.0f || HasPlane( hit.normal ) );
}

inline bool NeedsSlideRepair( Vec plane, Sweep hit, float standableZ = kStandableZ ) noexcept {
	return IsRamp( plane, standableZ ) && ( !Valid( hit ) || ( hit.fraction < 1.0f && hit.normal.Dot( plane ) < kSamePlane ) );
}

inline bool NeedsGroundRepair( Vec plane, Sweep hit, float verticalSpeed, float standableZ = kStandableZ ) noexcept {
	return IsRamp( plane, standableZ ) && verticalSpeed <= -64.0f && Valid( hit ) && hit.fraction < 0.95f &&
		hit.normal.z > standableZ && hit.normal.Dot( plane ) < kSamePlane;
}

// Try the tracked ramp normal first, then the outward directions around a seam.
// The native caller owns the hull/filter and verifies clearance at the accepted endpoint.
template <typename Probe>
bool FindCorrection( Vec plane, Probe&& probe ) {
	const auto tryDirection = [&]( Vec direction ) {
		constexpr float scales[]{ 0.25f, 0.5f, 0.75f, 1.0f };
		for ( float scale : scales ) {
			if ( probe( direction * ( kPierce * scale ) ) ) {
				return true;
			}
		}
		return false;
	};
	if ( tryDirection( plane ) ) {
		return true;
	}
	constexpr float axes[]{ 0.0f, -1.0f, 1.0f };
	for ( float x : axes ) {
		for ( float y : axes ) {
			for ( float z : axes ) {
				const Vec direction{ x, y, z };
				if ( direction.Dot( plane ) > 0.0f && tryDirection( direction * ( 1.0f / std::sqrt( direction.Dot( direction ) ) ) ) ) {
					return true;
				}
			}
		}
	}
	return false;
}

inline bool FollowsRamp( Vec plane, Sweep corrected ) noexcept {
	return Valid( corrected ) && ( corrected.fraction == 1.0f ||
		( corrected.fraction > 0.1f && corrected.normal.Dot( plane ) >= kSamePlane ) );
}

inline std::optional<Vec> RestoreLandingSpeed( Vec incoming, Vec native, Vec floor, float standableZ = kStandableZ ) noexcept {
	if ( !HasPlane( floor ) || floor.z < standableZ || floor.z >= 1.0f || incoming.Dot( floor ) >= 0.0f ) {
		return std::nullopt;
	}
	auto projected = incoming - floor * incoming.Dot( floor );
	if ( projected.HorizontalSquared() < incoming.HorizontalSquared() ||
		projected.HorizontalSquared() <= native.HorizontalSquared() ) {
		return std::nullopt;
	}
	projected.z = native.z;
	return projected;
}

} // namespace imomoe::server_fixes::ramp
