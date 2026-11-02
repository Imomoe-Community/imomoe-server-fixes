// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "chat_policy.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

using imomoe::server_fixes::chat::CommandTag;
using imomoe::server_fixes::chat::Evaluate;

namespace {

void Check( bool condition, std::string_view name ) {
	if ( !condition ) {
		std::cerr << "FAILED: " << name << '\n';
		std::exit( 1 );
	}
}

} // namespace

int main() {
	const auto slotZeroRawSay = Evaluate( true, 0, false, false, "SaY" );
	Check( slotZeroRawSay.block && slotZeroRawSay.tag == CommandTag::Say, "slot 0 raw say is blocked" );

	const auto teamSay = Evaluate( true, 3, false, false, "SAY_TEAM" );
	Check( teamSay.block && teamSay.tag == CommandTag::SayTeam, "say_team matching is ASCII case-insensitive" );

	const auto chatAlias = Evaluate( true, 5, false, true, "unrelated-argument" );
	Check( chatAlias.block && chatAlias.tag == CommandTag::ChatAlias, "SDK Chat context is blocked" );

	Check( !Evaluate( false, 0, false, true, "say" ).block, "disabled capability allows chat" );
	Check( !Evaluate( true, -1, false, false, "say" ).block, "server and RCON slot is unchanged" );
	Check( !Evaluate( true, 0, true, false, "say" ).block, "FULL player remains unrestricted" );
	Check( !Evaluate( true, 4, false, false, "status" ).block, "non-chat command is unchanged" );
	Check( !Evaluate( true, 4, false, false, "" ).block, "empty command is unchanged" );
	Check( !Evaluate( true, 0, false, false, "say\n" ).block, "command comparison does not trim input" );

	std::cout << "chat admission policy checks passed\n";
	return 0;
}
