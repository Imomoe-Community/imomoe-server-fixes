// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <chrono>
#include <algorithm>
#include <optional>
#include <string_view>

namespace imomoe::server_fixes::map_maintenance {

class BlankMapRecovery {
public:
	using Clock = std::chrono::steady_clock;

	void OnNextLevel( bool hasTarget, Clock::time_point now ) noexcept {
		if ( hasTarget ) {
			pending_ = false;
		} else if ( !pending_ && !attempted_ ) {
			pending_ = true;
			// Give the engine's normal map selection and command queue the first turn.
			due_ = now + std::chrono::seconds( 1 );
		}
	}

	bool Take( Clock::time_point now, bool nativeTransitionPending ) noexcept {
		if ( nativeTransitionPending ) {
			pending_ = false;
		}
		if ( !pending_ || now < due_ ) {
			return false;
		}
		pending_ = false;
		attempted_ = true;
		return true;
	}

	bool Pending() const noexcept { return pending_; }

private:
	Clock::time_point due_{};
	bool pending_{};
	bool attempted_{};
};

// A stock map name is a single console argument; Workshop maps use a typed ID.
constexpr bool IsStockMapArgument( std::string_view name ) noexcept {
	return !name.empty() && name.find_first_not_of(
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" ) == std::string_view::npos;
}

struct RefreshConfig {
	std::chrono::seconds idleAfter;
	std::chrono::seconds idleGrace;
	std::chrono::seconds ageLimit;
	std::chrono::seconds matchWait;
	std::chrono::seconds warning;
	std::chrono::seconds roundWait;
	std::chrono::seconds matchEndGrace;
};

struct MatchState {
	bool warmup{};
	bool ended{};
	float start{};
	int rounds{};
	float roundStart{};
};

enum class RefreshStep { None, Armed, WarnRound, ReloadIdle, ReloadNewMatch, ReloadAfterWarning };

class LongMapClock {
public:
	using Clock = std::chrono::steady_clock;

	void OnMapStart( Clock::time_point now ) noexcept {
		*this = {};
		lastHuman_ = now;
		mapSince_ = now;
	}

	RefreshStep Observe( Clock::time_point now, std::chrono::duration<float> age,
		bool hasHuman, bool joining, const MatchState& match, const RefreshConfig& config ) noexcept {
		if ( hasHuman || joining ) {
			lastHuman_ = now;
		}
		if ( match.ended ) {
			if ( !matchEndedAt_ ) {
				matchEndedAt_ = now;
			}
		} else {
			matchEndedAt_.reset();
		}
		const bool nativeMatchWindow = matchEndedAt_ && now - *matchEndedAt_ < config.matchEndGrace;
		if ( issued_ || joining ) {
			return RefreshStep::None;
		}
		if ( !hasHuman ) {
			if ( !nativeMatchWindow && config.idleAfter.count() > 0 && now - mapSince_ >= config.idleAfter &&
				now - lastHuman_ >= config.idleGrace ) {
				return Reload( RefreshStep::ReloadIdle );
			}
			return RefreshStep::None;
		}
		if ( config.ageLimit.count() == 0 || age < config.ageLimit ) {
			phase_ = Phase::Tracking;
			return RefreshStep::None;
		}
		if ( match.warmup || ( phase_ != Phase::Tracking && ( match.start != match_.start || match.rounds < match_.rounds ) ) ) {
			return Reload( RefreshStep::ReloadNewMatch );
		}
		if ( phase_ == Phase::Tracking ) {
			match_ = match;
			deadline_ = now + config.matchWait;
			phase_ = Phase::AwaitMatch;
			return RefreshStep::Armed;
		}
		if ( phase_ != Phase::AwaitRound && now < deadline_ ) {
			return RefreshStep::None;
		}
		if ( phase_ == Phase::AwaitMatch ) {
			deadline_ = now + config.warning;
			phase_ = Phase::Warning;
			return RefreshStep::WarnRound;
		}
		if ( phase_ == Phase::Warning ) {
			lastRound_ = match;
			deadline_ = now + config.roundWait;
			phase_ = Phase::AwaitRound;
		}
		const bool roundBoundary = !match.ended && match.rounds > lastRound_.rounds &&
			match.roundStart == lastRound_.roundStart;
		lastRound_ = match;
		if ( roundBoundary || ( now >= deadline_ && !nativeMatchWindow ) ) {
			return Reload( RefreshStep::ReloadAfterWarning );
		}
		return RefreshStep::None;
	}

private:
	enum class Phase { Tracking, AwaitMatch, Warning, AwaitRound };

	RefreshStep Reload( RefreshStep action ) noexcept {
		issued_ = true;
		return action;
	}

	Phase phase_{};
	MatchState match_;
	MatchState lastRound_;
	Clock::time_point lastHuman_{};
	Clock::time_point mapSince_{};
	Clock::time_point deadline_{};
	std::optional<Clock::time_point> matchEndedAt_;
	bool issued_{};
};

inline float RemainingTimeLimit( float limit, float gameTime, float matchStart ) noexcept {
	if ( limit <= 0.0f ) {
		return 0.0f; // unlimited
	}
	const float elapsed = gameTime - matchStart;
	// Zero would remove the limit rather than expire the match shortly.
	return std::clamp( limit - elapsed / 60.0f, std::min( limit, 0.1f ), limit );
}

} // namespace imomoe::server_fixes::map_maintenance
