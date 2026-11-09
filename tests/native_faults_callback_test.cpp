// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later


#include "native_faults.cpp"

#include <cstdlib>
#include <iostream>

namespace plg {

LogFn Log = []( std::string_view, Severity, const source_location& ) {};
BeginZoneFn BeginZone = []( std::string_view, const source_location& ) -> zone { return 0; };
EndZoneFn EndZone = []( zone ) {};

namespace plugin {
GetLocationFn GetLocation{};
hndl handle{};
} // namespace plugin

Plugin* GetPlugin() {
	return nullptr; // No SDK loader in this callback test.
}

} // namespace plg

namespace {

using namespace imomoe::server_fixes::native_faults;

void Require( bool condition, const char* message ) {
	if ( !condition ) {
		std::cerr << message << '\n';
		std::exit( 1 );
	}
}

std::array<void*, 3> g_args{};
void* g_rax{};
bool g_consumed{};
bool g_parented{};
const void* g_originWritten{};

void CheckInputs() {
	__polyhook_GetArgumentPointer = []( polyhook::ParametersHandle, std::uint64_t index ) {
		return g_args.at( static_cast<std::size_t>( index ) );
	};
	__polyhook_SetReturnBool = []( polyhook::ReturnHandle, bool value ) { g_consumed = value; };
	CEntityIdentity entity{};
	CUtlSymbolLarge input;
	g_args = { &entity, &input, nullptr };
	const auto check = [&]( const char* classname, const char* inputName, void* activator, bool expected ) {
		entity.m_designerName = classname;
		input = inputName;
		g_args[2] = activator;
		g_consumed = false;
		const auto action = OnInput( nullptr, nullptr, 7, nullptr, CallbackType::Pre );
		Require( action == ( expected ? ResultType::Supercede : ResultType::Ignored ) && g_consumed == expected,
			"wrong TestActivator dispatch result" );
	};
	check( "filter_activator_team", "TestActivator", nullptr, false );
	State( Fix::Filter ).armed = true;
	check( "FILTER_activator_team", "testACTIVATOR", nullptr, true );
	check( "filter_activator_team", "TestActivator", &entity, false );
	check( "filter_activator_team", "TestActivatorExtra", nullptr, false );
	check( "filter_activator_team", "Enable", nullptr, false );
	check( "logic_relay", "TestActivator", nullptr, false );

	std::array<float, 3> target{ 90.0f, -25.0f, 100.0f };
	g_args = { &entity, &target, nullptr };
	g_getParent = []( const void* beam ) -> void* { return g_parented ? const_cast<void*>( beam ) : nullptr; };
	g_setLocalOrigin = []( void*, const void* pos ) { g_originWritten = pos; };
	const auto origin = [] { return OnBeam<false>( nullptr, nullptr, 2, nullptr, CallbackType::Pre ); };
	Require( origin() == ResultType::Ignored && !g_originWritten, "disabled beam wrote origin" );
	State( Fix::Beam ).armed = true;
	g_parented = true;
	Require( origin() == ResultType::Ignored && !g_originWritten, "parented beam missed native path" );
	g_parented = false;
	Require( origin() == ResultType::Supercede && g_originWritten == &target,
		"unparented beam did not pass world position to native writer" );
}

void CheckPrecache() {
	__polyhook_GetRegisterPointer = []( polyhook::RegistersHandle, polyhook::RegisterType reg ) {
		Require( reg == polyhook::RegisterType::RAX, "precache must read the string lookup result" );
		return g_rax;
	};
	__polyhook_SetRegisterPointer = []( polyhook::RegistersHandle, polyhook::RegisterType reg, void* value ) {
		Require( reg == polyhook::RegisterType::RAX, "precache must only replace the string lookup result" );
		g_rax = value;
	};
	const auto check = []( const char* name, bool skip ) {
		g_rax = const_cast<char*>( name );
		Require( OnEquipPrecache( nullptr, nullptr, 0, nullptr, CallbackType::Pre ) == ResultType::Ignored,
			"the rest of the precache loop must run" );
		Require( skip ? ( g_rax && *static_cast<const char*>( g_rax ) == '\0' ) : g_rax == name,
			"wrong item precache argument" );
	};
	check( "item_kevlar", false );
	State( Fix::Equip ).armed = true;
	check( "Item_kevlar", true );
	check( "weapon_ak47", false );
	check( "item", false );
	check( nullptr, false );
}

struct FakeDetour {
	bool exists{};
	bool otherCallback{};
	bool rejectRemoval{};
	bool rejectUnhook{};
	polyhook::CallbackHandler callback{};
} g_detour;

void CheckHookOwnership() {
	__polyhook_FindDetour = []( void* ) -> void* { return g_detour.exists ? &g_detour : nullptr; };
	__polyhook_HookDetour = []( void*, DataType, const plg::vector<DataType>&, std::int32_t, const plg::string& ) -> void* {
		Require( !g_detour.exists, "overlaid a foreign detour" );
		g_detour.exists = true;
		return &g_detour;
	};
	__polyhook_AddCallback = []( void*, CallbackType, polyhook::CallbackHandler callback ) {
		g_detour.callback = callback;
		return true;
	};
	__polyhook_IsCallbackRegistered = []( void*, CallbackType, polyhook::CallbackHandler callback ) {
		return g_detour.callback == callback;
	};
	__polyhook_RemoveCallback = []( void*, CallbackType, polyhook::CallbackHandler ) {
		if ( g_detour.rejectRemoval ) {
			return false;
		}
		g_detour.callback = nullptr;
		return true;
	};
	__polyhook_AreCallbacksRegistered = []( void* ) { return g_detour.callback || g_detour.otherCallback; };
	__polyhook_UnhookDetour = []( void* ) {
		Require( !g_detour.otherCallback && !g_detour.callback, "unhooked a callback still in use" );
		if ( g_detour.rejectUnhook ) {
			return false;
		}
		g_detour.exists = false;
		return true;
	};
	std::array<std::byte, 32> code{};
	auto& module = State( Fix::Filter );
	const auto attach = [&] {
		return module.hooks[0].Attach( code.data(), "test", OnInput, DataType::Bool, {} );
	};
	g_detour = { .exists = true, .otherCallback = true };
	Require( attach() && module.Release() && g_detour.exists, "foreign hook was not preserved" );
	g_detour = {};
	Require( !attach(), "could not attach unowned entry" );
	module.armed = true;
	g_detour.rejectRemoval = true;
	Require( !module.Release() && !module.armed && g_detour.callback == OnInput,
		"failed removal lost ownership or left behavior armed" );
	g_detour.rejectRemoval = false;
	g_detour.rejectUnhook = true;
	Require( !module.Release() && g_detour.exists && !g_detour.callback, "failed unhook was not retained for retry" );
	g_detour.rejectUnhook = false;
	Require( module.Release() && !g_detour.exists, "shutdown did not retry its pending unhook" );
	Require( !attach(), "could not reattach after clean removal" );
	g_detour.otherCallback = true;
	Require( module.Release() && !g_detour.callback && g_detour.exists,
		"disabling a shared entry must remove only our callback" );
}

} // namespace

int main() {
	CheckInputs();
	CheckPrecache();
	CheckHookOwnership();
	Require( Shutdown(), "native fixes did not shut down" );
}
