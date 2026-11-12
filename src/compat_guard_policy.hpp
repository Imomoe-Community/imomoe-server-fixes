// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace imomoe::server_fixes::compat_guards {

// Keep a bounded allowance per client. A rejected request does not extend the window.
class Allowance {
public:
	using Clock = std::chrono::steady_clock;

	bool Take( Clock::time_point now, Clock::duration window, int limit ) noexcept {
		if ( now >= resetAt_ ) {
			resetAt_ = now + window;
			used_ = 0;
		}
		if ( used_ >= limit ) {
			return false;
		}
		++used_;
		return true;
	}

private:
	Clock::time_point resetAt_ = Clock::time_point::min();
	int used_{};
};

// Refill continuously and keep unused capacity up to the configured burst.
class TokenBucket {
public:
	using Clock = std::chrono::steady_clock;

	bool Take( Clock::time_point now, int rate, int burst ) noexcept {
		if ( last_ == Clock::time_point::min() ) {
			tokens_ = burst;
		} else {
			const double elapsed = std::chrono::duration<double>( now - last_ ).count();
			tokens_ = std::min( static_cast<double>( burst ), tokens_ + elapsed * rate );
		}
		last_ = now;
		if ( tokens_ < 1.0 ) {
			return false;
		}
		tokens_ -= 1.0;
		return true;
	}

private:
	Clock::time_point last_ = Clock::time_point::min();
	double tokens_{};
};

class KeyValuesBudget {
public:
	bool Take( int userId, std::size_t bytes, TokenBucket::Clock::time_point now,
		int rate, int burst, int maxBytes ) noexcept {
		if ( userId_ != userId ) {
			userId_ = userId;
			bucket_ = {};
		}
		return bytes <= static_cast<std::size_t>( maxBytes ) && bucket_.Take( now, rate, burst );
	}

private:
	int userId_{ -1 };
	TokenBucket bucket_;
};

constexpr bool HasValidVoiceShape( std::size_t bytes, std::size_t offsets,
	std::size_t maxBytes, std::size_t maxOffsets ) noexcept {
	return bytes != 0 && bytes <= maxBytes && offsets <= maxOffsets && offsets <= bytes;
}

constexpr std::uint64_t VoiceStreamId( std::uint32_t mapSeed, std::uint16_t recipient,
	std::uint16_t speaker ) noexcept {
	return ( static_cast<std::uint64_t>( mapSeed ) << 32 ) |
		( static_cast<std::uint64_t>( recipient ) << 16 ) | speaker;
}

} // namespace imomoe::server_fixes::compat_guards
