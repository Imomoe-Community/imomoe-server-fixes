// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_set>

namespace imomoe::server_fixes::hostage {

class SchemaReadiness final {
public:
	// GetSchemaOffset is unsafe until S2SDK's post-ActivateServer map-start callback.
	void OnMapStart() noexcept { ready_ = true; }
	void OnMapEnd() noexcept { ready_ = false; }
	bool CanQuery() const noexcept { return ready_; }

	template <typename Action>
	bool RunSchemaDependent( Action&& action ) const {
		if ( !ready_ ) {
			return false;
		}
		action();
		return true;
	}

private:
	bool ready_{};
};

struct AttemptKey {
	std::uint64_t mapGeneration{};
	std::uint64_t roundGeneration{};
	std::uint64_t connectionGeneration{};
	std::uint64_t lifeGeneration{};
	std::int32_t zone{-1};
	std::int32_t controller{-1};
	std::int32_t pawn{-1};
	std::int32_t hostage{-1};
	std::int32_t carriedProp{-1};

	friend bool operator==( const AttemptKey&, const AttemptKey& ) = default;
};

constexpr bool IsEntityHandleValuePresent( std::int32_t handle ) noexcept {
	return handle != -1;
}

constexpr bool IsMatchingHostageTarget( const AttemptKey& attempt, std::int32_t targetHandle ) noexcept {
	return IsEntityHandleValuePresent( attempt.hostage ) && IsEntityHandleValuePresent( targetHandle ) &&
		targetHandle == attempt.hostage;
}

struct AttemptKeyHash {
	std::size_t operator()( const AttemptKey& key ) const noexcept {
	std::size_t value = 0xcbf29ce484222325ULL;
	const auto mix = [&value]( std::uint64_t part ) {
		value ^= static_cast<std::size_t>( part );
		value *= 0x100000001b3ULL;
	};
	mix( key.mapGeneration );
	mix( key.roundGeneration );
	mix( key.connectionGeneration );
	mix( key.lifeGeneration );
	mix( static_cast<std::uint32_t>( key.zone ) );
	mix( static_cast<std::uint32_t>( key.controller ) );
	mix( static_cast<std::uint32_t>( key.pawn ) );
	mix( static_cast<std::uint32_t>( key.hostage ) );
	mix( static_cast<std::uint32_t>( key.carriedProp ) );
	return value;
	}
};

struct RescueFacts {
	bool enabled{};
	bool roundActive{};
	bool sameMapRoundConnectionAndLife{};
	bool nativePawnRescueCallReached{};
	bool nativePawnRouteObservationCertain{ true };
	bool supportedCustomGame{};
	bool mapHasRescueZone{};
	bool zoneIsValidAndEnabled{};
	bool playerIsAliveCounterTerrorist{};
	bool fullCarriedHostageAndProp{};
	bool hostageIsNotRescued{};
	bool nativeRescueHandlerWasInvoked{};
	bool nativeRescueHandlerObservationCertain{ true };
};

enum class FinalizeAction : std::uint8_t {
	Skip,
	NativeHandlerObserved,
	Duplicate,
	SubmitNativeInput,
};

class AttemptLedger final {
public:
	FinalizeAction Finalize( const AttemptKey& key, const RescueFacts& facts ) {
		if ( facts.nativeRescueHandlerWasInvoked ) {
			return FinalizeAction::NativeHandlerObserved;
		}
		if ( !facts.nativePawnRescueCallReached || !facts.nativePawnRouteObservationCertain ||
			!facts.nativeRescueHandlerObservationCertain ||
			!facts.enabled || !facts.roundActive ||
			!facts.sameMapRoundConnectionAndLife ||
			!facts.supportedCustomGame || !facts.mapHasRescueZone || !facts.zoneIsValidAndEnabled ||
			!facts.playerIsAliveCounterTerrorist || !facts.fullCarriedHostageAndProp ||
			!facts.hostageIsNotRescued ) {
			return FinalizeAction::Skip;
		}
		if ( !attempts_.insert( key ).second ) {
			return FinalizeAction::Duplicate;
		}
		return FinalizeAction::SubmitNativeInput;
	}

	void Clear() noexcept { attempts_.clear(); }
	std::size_t Size() const noexcept { return attempts_.size(); }

private:
	std::unordered_set<AttemptKey, AttemptKeyHash> attempts_;
};

constexpr bool ShouldPatchTTargetGate( bool moduleEnabled, bool mpOnlyCtsRescueHostages ) noexcept {
	return moduleEnabled && !mpOnlyCtsRescueHostages;
}

constexpr bool ShouldPatchTTargetGateWhenPolicyUnknown( bool moduleEnabled, bool mpOnlyKnown,
	bool mpOnlyCtsRescueHostages ) noexcept {
	return moduleEnabled && mpOnlyKnown && !mpOnlyCtsRescueHostages;
}

constexpr bool CanObserveRescueTouch( bool lifecycleActive, bool moduleEnabled,
	bool observersActive ) noexcept {
	return lifecycleActive && moduleEnabled && observersActive;
}

constexpr bool OriginalTTargetGateAllows( std::int32_t team ) noexcept {
	return team == 3;
}

constexpr bool PatchedTTargetGateAllows( std::int32_t team ) noexcept {
	return team == 2 || OriginalTTargetGateAllows( team );
}

} // namespace imomoe::server_fixes::hostage
