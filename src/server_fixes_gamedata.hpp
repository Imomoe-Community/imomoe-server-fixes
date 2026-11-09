// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <plg/plugin.hpp>
#include <s2sdk/s2sdk/gameconfig.hpp>

namespace imomoe::server_fixes {

inline s2sdk::ConfigId LoadServerFixesGameData() {
	plg::string path = plg::GetPlugin()->GetLocation();
	path += "/gamedata/imomoe_server_fixes.jsonc";
	return s2sdk::LoadGameConfigFile( { path } );
}

} // namespace imomoe::server_fixes
