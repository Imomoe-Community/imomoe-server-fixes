// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <tier0/utlmap.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace imomoe::server_fixes::game_ban_fix {

// The GC penalty notification supplies the reason, duration
// and two account flags. A separate connection path supplies the second timer.
struct GcBan {
	std::uint32_t reason{};
	double reasonUntil{};
	double separateBanUntil{};
	std::uint8_t communicationCooldown{};
	std::uint8_t cheatingPenaltyLevel{};
};

using GcBanMap = CUtlOrderedMap<std::uint32_t, GcBan>;
static_assert( sizeof( GcBan ) == 0x20 && offsetof( GcBan, reasonUntil ) == 0x08 &&
	offsetof( GcBan, separateBanUntil ) == 0x10 && offsetof( GcBan, communicationCooldown ) == 0x18 &&
	offsetof( GcBan, cheatingPenaltyLevel ) == 0x19 );
static_assert( sizeof( GcBanMap ) == 0x28 && sizeof( UtlRBTreeNode_t<GcBanMap::Node_t, int> ) == 0x38 );

class BanPass {
public:
	void FilterCooldowns( GcBanMap& map, int cooldownMode ) {
		if ( cooldownMode >= 2 ) {
			return;
		}
		m_masked.reserve( map.Count() );
		for ( auto i = map.FirstInorder(); i != map.InvalidIndex(); i = map.NextInorder( i ) ) {
			auto& ban = map.Element( i );
			if ( ban.reason == 20 || ban.reason == 22 || ban.reason == 23 ) {
				m_masked.push_back( { map.Key( i ), ban } );
				ban.reason = 0;
			}
		}
	}

	// This map also holds unprocessed bans and communication state. Never
	// discard a live penalty on the assumption that KickClient stored it elsewhere.
	void Finish( GcBanMap& map, double now ) noexcept {
		for ( const auto& [account, masked] : m_masked ) {
			const auto index = map.Find( account );
			if ( index != map.InvalidIndex() ) {
				auto& ban = map.Element( index );
				if ( !ban.reason && ban.reasonUntil == masked.reasonUntil ) {
					ban.reason = masked.reason;
				}
			}
		}
		m_masked.clear();

		for ( auto i = map.FirstInorder(); i != map.InvalidIndex(); ) {
			const auto next = map.NextInorder( i );
			auto& ban = map.Element( i );
			if ( ban.reasonUntil <= now ) {
				ban.reason = 0;
				ban.reasonUntil = 0;
			}
			if ( ban.separateBanUntil <= now ) {
				ban.separateBanUntil = 0;
			}
			if ( !ban.reason && ban.reasonUntil == 0 && ban.separateBanUntil == 0 &&
				!ban.communicationCooldown && !ban.cheatingPenaltyLevel ) {
				map.RemoveAt( i );
			}
			i = next;
		}
	}

private:
	struct Entry {
		std::uint32_t account;
		GcBan ban;
	};
	std::vector<Entry> m_masked;
};

} // namespace imomoe::server_fixes::game_ban_fix
