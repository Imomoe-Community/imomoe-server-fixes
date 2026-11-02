// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <string_view>

namespace imomoe::server_fixes::chat {

enum class CommandTag {
	None,
	ChatAlias,
	Say,
	SayTeam,
};

struct Decision {
	bool block;
	CommandTag tag;
};

constexpr unsigned char AsciiLower( unsigned char value ) noexcept {
	return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>( value + ( 'a' - 'A' ) ) : value;
}

constexpr bool EqualsAsciiCaseInsensitive( std::string_view left, std::string_view right ) noexcept {
	if ( left.size() != right.size() ) {
		return false;
	}

	for ( std::size_t index = 0; index < left.size(); ++index ) {
		if ( AsciiLower( static_cast<unsigned char>( left[ index ] ) ) !=
			 AsciiLower( static_cast<unsigned char>( right[ index ] ) ) ) {
			return false;
		}
	}

	return true;
}

constexpr CommandTag Classify( bool chatContext, std::string_view firstArgument ) noexcept {
	if ( chatContext ) {
		return CommandTag::ChatAlias;
	}
	if ( EqualsAsciiCaseInsensitive( firstArgument, "say" ) ) {
		return CommandTag::Say;
	}
	if ( EqualsAsciiCaseInsensitive( firstArgument, "say_team" ) ) {
		return CommandTag::SayTeam;
	}
	return CommandTag::None;
}

constexpr Decision Evaluate( bool enabled, int slot, bool inGame, bool chatContext,
	std::string_view firstArgument ) noexcept {
	if ( !enabled || slot < 0 || inGame ) {
		return { false, CommandTag::None };
	}

	const auto tag = Classify( chatContext, firstArgument );
	return { tag != CommandTag::None, tag };
}

constexpr std::string_view Label( CommandTag tag ) noexcept {
	switch ( tag ) {
	case CommandTag::ChatAlias: return "<chat-alias>";
	case CommandTag::Say: return "say";
	case CommandTag::SayTeam: return "say_team";
	default: return "<none>";
	}
}

} // namespace imomoe::server_fixes::chat
