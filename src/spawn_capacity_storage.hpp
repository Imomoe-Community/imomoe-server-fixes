// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <istream>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace imomoe::server_fixes::spawn_capacity {

struct ManualPointRow {
	std::uint32_t id{};
	std::int32_t team{};
	float x{};
	float y{};
	float z{};
	float pitch{};
	float yaw{};
	float roll{};
};

inline bool WriteManualPointRow( std::ostream& output, const ManualPointRow& point ) {
	output << point.id << ' ' << point.team << ' ' << std::setprecision( std::numeric_limits<float>::max_digits10 )
		<< point.x << ' ' << point.y << ' ' << point.z << ' '
		<< point.pitch << ' ' << point.yaw << ' ' << point.roll << '\n';
	return static_cast<bool>( output );
}

inline bool ParseManualPointRow( std::string_view line, ManualPointRow& point ) {
	if ( line.size() > 512 ) {
		return false;
	}
	std::istringstream row{ std::string( line ) };
	row.imbue( std::locale::classic() );
	std::string idText;
	if ( !( row >> idText >> point.team >> point.x >> point.y >> point.z >> point.pitch >> point.yaw >> point.roll ) ) {
		return false;
	}
	std::string extra;
	if ( row >> extra || !row.eof() || row.bad() ) {
		return false;
	}
	const auto parsedId = std::from_chars( idText.data(), idText.data() + idText.size(), point.id );
	const auto values = { point.x, point.y, point.z, point.pitch, point.yaw, point.roll };
	return !idText.empty() && parsedId.ec == std::errc{} && parsedId.ptr == idText.data() + idText.size() &&
		point.id != 0 && std::all_of( values.begin(), values.end(), []( float value ) { return std::isfinite( value ); } );
}

} // namespace imomoe::server_fixes::spawn_capacity
