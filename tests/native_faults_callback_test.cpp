// Copyright (C) 2026 Mai Ooizumi
// SPDX-License-Identifier: AGPL-3.0-or-later


#include "native_faults.cpp"

#include <cstrike15/cs_movedata.h>
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

void CheckHammerIdBranch() {
	for ( const auto displacement : { std::uint8_t{ 0x5e }, std::uint8_t{ 0x5d } } ) {
		const std::array<std::uint8_t, 3> code{ 0x74, displacement, 0x48 };
		const auto patch = HammerIdBranchPatch( code );
		Require( patch == std::vector<std::uint8_t>{ 0x74, 0 }, "Hammer ID skip must land at the next instruction" );
	}
	const std::array<std::uint8_t, 2> foreignCode{ 0x75, 0x5e };
	Require( HammerIdBranchPatch( foreignCode ).empty(), "Hammer ID must not alter an unrecognized branch" );
}

void CheckCstvQuotaScope() {
	auto& module = State( Fix::CstvBotQuota );
	Require( !Update( Fix::CstvBotQuota, true, false ).available && !module.attempted,
		"CSTV must wait for map readiness before resolving schema or attaching hooks" );
	std::array<unsigned char, 4> cstv{ 0, 0, 1, 0 }, bot{};
	g_hltvOffset = 2;
	__polyhook_GetArgumentPointer = []( polyhook::ParametersHandle, std::uint64_t index ) {
		Require( index == 1, "team candidate takes the controller as its second argument" );
		return g_args[1];
	};
	__polyhook_SetReturnBool = []( polyhook::ReturnHandle, bool value ) { g_consumed = !value; };
	const auto check = [&]( void* controller, bool skip ) {
		g_args[1] = controller;
		g_consumed = false;
		const auto action = OnBotTeamCandidate( nullptr, nullptr, 3, nullptr, CallbackType::Pre );
		Require( action == ( skip ? ResultType::Supercede : ResultType::Ignored ) && g_consumed == skip,
			"only CSTV inside bot removal should be skipped" );
	};
	const auto select = []( CallbackType phase ) { OnBotSelection( nullptr, nullptr, 1, nullptr, phase ); };
	module.armed = true;
	g_cstvMapReady = true;
	check( cstv.data(), false );
	select( CallbackType::Pre );
	select( CallbackType::Pre );
	check( cstv.data(), true );
	check( bot.data(), false );
	check( nullptr, false );
	select( CallbackType::Post );
	check( cstv.data(), true );
	select( CallbackType::Post );
	check( cstv.data(), false );
	select( CallbackType::Pre );
	OnMapEnd();
	check( cstv.data(), false );
	g_cstvMapReady = true;
	check( cstv.data(), false );
}

void CheckWaterJumpHandoff() {
	bool stayOnGround = false;
	__polyhook_GetArgumentPointer = []( polyhook::ParametersHandle, std::uint64_t index ) {
		return g_args.at( static_cast<std::size_t>( index ) );
	};
	__polyhook_GetArgumentBool = []( polyhook::ParametersHandle parameters, std::uint64_t index ) {
		Require( index == 2, "the third categorize parameter controls whether to stay on ground" );
		return *static_cast<bool*>( parameters );
	};
	int services{}, move{}, anotherMove{};
	auto& module = State( Fix::WaterJump );
	const auto water = [&] {
		g_args = { &services, &move, nullptr };
		AfterWaterMove( nullptr, nullptr, 2, nullptr, CallbackType::Post );
	};
	const auto categorize = [&]( void* moveData ) {
		g_args[1] = moveData;
		return OnCategorizePosition( nullptr, &stayOnGround, 3, nullptr, CallbackType::Pre );
	};
	water();
	Require( categorize( &move ) == ResultType::Ignored, "disabled water movement must not change categorization" );
	module.armed = true;
	water();
	Require( categorize( &move ) == ResultType::Supercede, "water move must skip the matching immediate categorize" );
	Require( categorize( &move ) == ResultType::Ignored, "the water skip must be consumed only once" );
	water();
	Require( categorize( &anotherMove ) == ResultType::Ignored && categorize( &move ) == ResultType::Ignored,
		"a different movement context must run natively and consume the handoff" );
	water();
	stayOnGround = true;
	Require( categorize( &move ) == ResultType::Ignored, "explicit stay-on-ground categorization must run natively" );
	stayOnGround = false;
	water();
	OnMapEnd();
	Require( categorize( &move ) == ResultType::Ignored, "the old water context must not cross a map boundary" );
}

std::int32_t g_jumpTestHandle{};

std::uint32_t g_rampPawnHandle{};
void* g_rampForeign{};
int g_rampQueries{};
bool g_rampShorterSweep{};

void CheckRampMovement() {
	namespace ramp = imomoe::server_fixes::ramp;
	struct Pawn {
		int padding{};
		CEntityHandle ground;
		std::uint8_t moveType{ static_cast<std::uint8_t>( s2sdk::MoveType::Walk ) };
		float waterLevel{};
	} pawn;
	struct Services { void* padding; Pawn* pawn; int cache; } services{ nullptr, &pawn, 0 };
	g_rampPawnHandle = CEntityHandle{ 6, 1 }.ToInt();
	__s2sdk_EntPointerToEntHandle = []( void* p ) {
		return static_cast<std::int32_t>( p == g_rampForeign ? CEntityHandle{ 7, 1 }.ToInt() : g_rampPawnHandle );
	};
	ramp::Configure( []( const Vector* from, const Vector* to, const void*, CTraceFilter*, CGameTrace* hit ) {
		++g_rampQueries;
		hit->m_bStartInSolid = false;
		hit->m_vStartPos = *from;
		const auto distanceSquared = ( *to - *from ).LengthSqr();
		hit->m_flFraction = distanceSquared == 0.0f ? 1.0f :
			g_rampShorterSweep && distanceSquared < 0.001f ? 0.2f : 0.75f;
		hit->m_vEndPos = *from + ( *to - *from ) * hit->m_flFraction;
		hit->m_vHitNormal = { 0.8f, 0.0f, 0.6f };
	}, static_cast<int>( offsetof( Services, cache ) ), static_cast<int>( offsetof( Services, pawn ) ),
		static_cast<int>( offsetof( Pawn, ground ) ), static_cast<int>( offsetof( Pawn, moveType ) ),
		static_cast<int>( offsetof( Pawn, waterLevel ) ) );
	CMoveData move{};
	move.m_nTargetTick = 11;
	const Vector start( 2.0f, 0.0f, 3.0f ), end( 2.0f, 0.0f, 1.0f );
	move.m_vecAbsOrigin = start;
	move.m_vecVelocity = { 100.0f, 0.0f, -90.0f };
	CTraceFilter filter;
	int bounds{};
	CGameTrace hit;
	g_rampForeign = &bounds;
	const auto sample = [&]( const void* cache, float x, float z ) {
		hit.m_vStartPos = start;
		hit.m_vEndPos = start;
		hit.m_flFraction = 0.5f;
		hit.m_vHitNormal = { x, 0.0f, z };
		ramp::AfterTrace( cache, &hit, &start, &end, &bounds, &filter );
	};
	ramp::SetReady( true );
	ramp::Begin( ramp::Phase::Slide, &services, &move );
	sample( &services.cache, 0.8f, 0.6f );
	sample( &bounds, 0.6f, 0.8f );
	Require( g_rampQueries == 0 && hit.m_vHitNormal.z == 0.8f, "ramp repaired a trace belonging to another cache" );
	hit.m_pEnt = reinterpret_cast<CEntityInstance*>( g_rampForeign );
	sample( &services.cache, 0.6f, 0.8f );
	Require( g_rampQueries == 0 && hit.m_vHitNormal.z == 0.8f, "ramp bypassed collision on a different entity" );
	hit.m_pEnt = nullptr;
	const Vector shortEnd = start + Vector( 0.02f, 0.0f, 0.0f );
	hit.m_vEndPos = ( start + shortEnd ) * 0.5f;
	g_rampShorterSweep = true;
	ramp::AfterTrace( &services.cache, &hit, &start, &shortEnd, &bounds, &filter );
	Require( g_rampQueries > 1 && hit.m_flFraction == 0.5f && hit.m_vStartPos == start,
		"ramp counted its nudge as forward sweep progress" );
	g_rampShorterSweep = false;
	g_rampQueries = 0;
	sample( &services.cache, 0.6f, 0.8f );
	Require( g_rampQueries == 3 && hit.m_vStartPos != start && hit.m_vHitNormal.z == 0.6f,
		"ramp did not supply the verified native trace to movement" );
	ramp::End( ramp::Phase::Slide, &services, &move );
	ramp::Begin( ramp::Phase::Ground, &services, &move );
	const Vector materialStart = start + Vector( 0.0f, 0.0f, 4.0f );
	hit.m_vHitNormal = { 0.6f, 0.0f, 0.8f };
	ramp::AfterTrace( &services.cache, &hit, &materialStart, &end, &bounds, &filter );
	Require( g_rampQueries == 3 && hit.m_vHitNormal.z == 0.8f, "ramp repaired a material lookup instead of a support trace" );
	sample( &services.cache, 0.6f, 0.8f );
	ramp::End( ramp::Phase::Ground, &services, &move );
	Require( g_rampQueries == 6 && move.m_vecAbsOrigin != start, "false ground on a fast ramp descent was not corrected" );

	g_rampPawnHandle = CEntityHandle{ 6, 2 }.ToInt();
	move.m_vecAbsOrigin = start;
	move.m_vecVelocity = { 30.0f, 0.0f, -100.0f };
	ramp::Begin( ramp::Phase::Ground, &services, &move );
	sample( &services.cache, -0.6f, 0.0f ); // A rejected candidate cannot determine the landing normal.
	pawn.ground = CEntityHandle{ 0, 1 };
	move.m_groundNormal = { 0.6f, 0.0f, 0.8f };
	move.m_vecVelocity = { 24.0f, 0.0f, 0.0f };
	ramp::End( ramp::Phase::Ground, &services, &move );
	Require( g_rampQueries == 6 && move.m_vecAbsOrigin == start && move.m_vecVelocity.x > 30.0f &&
		move.m_vecVelocity.z == 0.0f, "new pawn inherited a ramp or did not retain slope landing speed" );

	pawn.ground = CEntityHandle{};
	pawn.waterLevel = 1.0f;
	ramp::Begin( ramp::Phase::Slide, &services, &move );
	sample( &services.cache, 0.8f, 0.6f );
	sample( &services.cache, 0.6f, 0.8f );
	ramp::End( ramp::Phase::Slide, &services, &move );
	Require( g_rampQueries == 6, "ramp repair ran during water movement" );
	pawn.waterLevel = 0.0f;
	ramp::ResetMap();
	ramp::Begin( ramp::Phase::Slide, &services, &move );
	sample( &services.cache, 0.6f, 0.8f );
	ramp::End( ramp::Phase::Slide, &services, &move );
	Require( g_rampQueries == 6, "ramp retained map-local contact state after map end" );
}

void CheckJumpSpamStamp() {
	auto& module = State( Fix::JumpSpam );
	Require( !Update( Fix::JumpSpam, true, false ).available && !module.attempted,
		"jump hooks must wait for movement-owner schema readiness" );
	int pawn{};
	struct Services { void* padding; void* pawn; } services{ nullptr, &pawn };
	struct JumpState { void* padding; void* owner; } jumpState{ nullptr, &services };
	struct MoveData { std::int32_t padding; std::int32_t tick; } move{ 0, 100 };
	g_jumpPawnOffset = static_cast<std::int32_t>( offsetof( Services, pawn ) );
	g_jumpOwnerOffset = static_cast<std::int32_t>( offsetof( JumpState, owner ) );
	g_moveTickOffset = static_cast<std::int32_t>( offsetof( MoveData, tick ) );
	g_jumpTestHandle = static_cast<std::int32_t>( CEntityHandle{ 3, 1 }.ToInt() );
	g_lastJumps.assign( NUM_ENT_ENTRIES, {} );
	__s2sdk_EntPointerToEntHandle = []( void* entity ) {
		Require( entity == g_args[2], "jump identity must be the movement service's pawn" );
		return g_jumpTestHandle;
	};
	__polyhook_GetArgumentPointer = []( polyhook::ParametersHandle, std::uint64_t index ) {
		return g_args.at( static_cast<std::size_t>( index ) );
	};
	const auto attempt = [&] {
		g_args = { &jumpState, &move, &pawn };
		return BeforeJumpAttempt( nullptr, nullptr, 2, nullptr, CallbackType::Pre );
	};
	const auto completed = [&] {
		g_args = { &services, &move, &pawn };
		Require( OnJumpCompleted( nullptr, nullptr, 2, nullptr, CallbackType::Pre ) == ResultType::Ignored,
			"native completion and notifications must still run" );
	};
	module.armed = true;
	g_jumpMapReady = true;
	Require( attempt() == ResultType::Ignored && attempt() == ResultType::Ignored,
		"an attempt that never reaches native completion must not spend the jump" );
	completed();
	Require( attempt() == ResultType::Supercede, "a second successful jump in the same tick must be blocked" );
	++move.tick;
	Require( attempt() == ResultType::Ignored, "the next move tick may jump" );
	completed();
	g_jumpTestHandle = static_cast<std::int32_t>( CEntityHandle{ 3, 2 }.ToInt() );
	Require( attempt() == ResultType::Ignored, "a new pawn at the same entity index must not inherit a jump stamp" );
	completed();
	OnMapEnd();
	g_jumpMapReady = true;
	Require( attempt() == ResultType::Ignored, "a new map must not inherit a previous jump stamp" );
}

struct FakeDetour {
	bool exists{};
	bool otherCallback{};
	bool rejectRemoval{};
	bool rejectUnhook{};
	polyhook::CallbackHandler callback{};
	polyhook::CallbackHandler post{};
} g_detour;

polyhook::CallbackHandler& CallbackAt( CallbackType type ) {
	return type == CallbackType::Pre ? g_detour.callback : g_detour.post;
}

void CheckHookOwnership() {
	__polyhook_FindDetour = []( void* ) -> void* { return g_detour.exists ? &g_detour : nullptr; };
	__polyhook_HookDetour = []( void*, DataType, const plg::vector<DataType>&, std::int32_t, const plg::string& ) -> void* {
		Require( !g_detour.exists, "overlaid a foreign detour" );
		g_detour.exists = true;
		return &g_detour;
	};
	__polyhook_AddCallback = []( void*, CallbackType type, polyhook::CallbackHandler callback ) {
		CallbackAt( type ) = callback;
		return true;
	};
	__polyhook_IsCallbackRegistered = []( void*, CallbackType type, polyhook::CallbackHandler callback ) {
		return CallbackAt( type ) == callback;
	};
	__polyhook_RemoveCallback = []( void*, CallbackType type, polyhook::CallbackHandler ) {
		if ( g_detour.rejectRemoval ) {
			return false;
		}
		CallbackAt( type ) = nullptr;
		return true;
	};
	__polyhook_AreCallbacksRegistered = []( void* ) { return g_detour.callback || g_detour.post || g_detour.otherCallback; };
	__polyhook_UnhookDetour = []( void* ) {
		Require( !g_detour.otherCallback && !g_detour.callback && !g_detour.post, "unhooked a callback still in use" );
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
	g_detour = {};
	auto& quota = State( Fix::CstvBotQuota );
	Require( !quota.hooks[0].Attach( code.data(), "test", OnBotSelection, DataType::Bool,
		{ DataType::Int32 }, false, HookPhases::Both ),
		"could not attach paired selection callbacks" );
	Require( g_detour.callback == OnBotSelection && g_detour.post == OnBotSelection, "selection needs both callbacks" );
	Require( quota.Release() && !g_detour.exists && !g_detour.callback && !g_detour.post, "paired callbacks leaked on disable" );
	auto& water = State( Fix::WaterJump );
	Require( !water.hooks[0].Attach( code.data(), "test", AfterWaterMove, DataType::Void,
		{ DataType::Pointer, DataType::Pointer }, false, HookPhases::Post ), "could not attach water movement Post" );
	Require( !g_detour.callback && g_detour.post == AfterWaterMove, "water movement only needs Post" );
	Require( water.Release() && !g_detour.exists && !g_detour.post, "water movement Post leaked on disable" );
	auto& rampModule = State( Fix::RampBug );
	Require( !EnsureCategorize( code.data() ) && g_detour.callback == OnCategorizePosition &&
		g_detour.post == OnCategorizePosition, "categorization needs one paired hook" );
	water.armed = true;
	rampModule.armed = true;
	Require( !EnsureCategorize( nullptr ), "the second module attempted to overlay categorization" );
	water.armed = false;
	Require( ReleaseCategorizeIfUnused() && g_detour.exists, "disabling water removed ramp categorization" );
	rampModule.armed = false;
	Require( ReleaseCategorizeIfUnused() && !g_detour.exists, "last categorization consumer did not release the hook" );
}

} // namespace

int main() {
	CheckInputs();
	CheckPrecache();
	CheckHammerIdBranch();
	CheckCstvQuotaScope();
	CheckWaterJumpHandoff();
	CheckJumpSpamStamp();
	CheckRampMovement();
	CheckHookOwnership();
	Require( Shutdown(), "native fixes did not shut down" );
}
