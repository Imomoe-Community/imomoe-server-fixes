// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ramp_movement.hpp"
#include "ramp_policy.hpp"

#include <s2sdk/s2sdk/entities.hpp>
#include <cstrike15/cs_movedata.h>
#include <entity2/entityinstance.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace imomoe::server_fixes::ramp {
namespace {

// The matched native trace and movement callers consume these same fields.
static_assert( sizeof( CGameTrace ) == 192 );
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof" // Pin the compiler-supported SDK ABI, not portable layout.
#endif
static_assert( offsetof( CGameTrace, m_vStartPos ) == 120 );
static_assert( offsetof( CGameTrace, m_vEndPos ) == 132 );
static_assert( offsetof( CGameTrace, m_vHitNormal ) == 144 );
static_assert( offsetof( CGameTrace, m_flFraction ) == 172 );
static_assert( offsetof( CGameTrace, m_bStartInSolid ) == 187 );
static_assert( offsetof( CMoveDataBase, m_vecVelocity ) == 56 );
static_assert( offsetof( CMoveDataBase, m_groundNormal ) == 188 );
static_assert( offsetof( CMoveDataBase, m_vecAbsOrigin ) == 200 );
static_assert( offsetof( CMoveDataBase, m_nTargetTick ) == 216 );
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

struct Contact {
	CEntityHandle pawn;
	CEntityHandle surface;
	Vec normal;
	Vec point;
	int tick{};
};

struct Scope {
	void* services{};
	CMoveData* move{};
	std::byte* pawn{};
	Contact* last{};
	Vec incoming;
	std::optional<std::pair<Vector, Vector>> groundNudge;
};

HullTrace g_trace{};
int g_cacheOffset{}, g_pawnOffset{}, g_groundOffset{}, g_moveTypeOffset{}, g_waterOffset{};
std::vector<Contact> g_contacts;
thread_local bool g_ready{};
thread_local float g_standableZ{ kStandableZ };
thread_local std::array<Scope, 2> g_scopes;

Scope& Active( Phase phase ) noexcept {
	return g_scopes[static_cast<std::size_t>( phase )];
}

template <typename T>
T Read( const std::byte* object, int offset ) noexcept {
	T value;
	std::memcpy( &value, object + offset, sizeof value );
	return value;
}

Vec ToVec( const Vector& v ) noexcept { return { v.x, v.y, v.z }; }
Vector ToNative( Vec v ) noexcept { return { v.x, v.y, v.z }; }

Sweep View( const CGameTrace& hit ) noexcept {
	return { ToVec( hit.m_vHitNormal ), hit.m_flFraction, hit.m_bStartInSolid };
}

CEntityHandle EntityHandle( void* entity ) noexcept {
	return entity ? CEntityHandle{ static_cast<std::uint32_t>( s2sdk::EntPointerToEntHandle( entity ) ) } : CEntityHandle{};
}

CEntityHandle SurfaceHandle( const CGameTrace& trace ) noexcept {
	const auto entity = EntityHandle( trace.m_pEnt );
	// The collision result can represent the world as a null entity or slot 0.
	return !trace.m_pEnt || ( entity.IsValid() && entity.GetEntryIndex() == 0 ) ? CEntityHandle{ 0, 0 } : entity;
}

bool NearContact( const Contact& last, const Vector& start, const Vector& end, int tick ) noexcept {
	const auto elapsed = static_cast<std::int64_t>( tick ) - last.tick;
	const Vec delta = ToVec( start ) - last.point;
	const Vec step = ToVec( end ) - ToVec( start );
	constexpr float kGroundProbeDistance = 2.0f;
	return elapsed >= 0 && elapsed <= 2 && IsRamp( last.normal, g_standableZ ) &&
		std::abs( delta.Dot( last.normal ) ) <= kGroundProbeDistance &&
		delta.Dot( delta ) <= std::max( kGroundProbeDistance * kGroundProbeDistance, 4.0f * step.Dot( step ) );
}

bool ProvesRampContact( const Contact& last, const Vector& point, const void* bounds, CTraceFilter* filter ) {
	const Vector offset = ToNative( last.normal * kPierce );
	const Vector from = point + offset;
	const Vector towardRamp = point - offset;
	CGameTrace contact;
	g_trace( &from, &towardRamp, bounds, filter, &contact );
	const Sweep hit = View( contact );
	return Valid( hit ) && hit.fraction < 1.0f && hit.normal.Dot( last.normal ) >= kSamePlane &&
		SurfaceHandle( contact ) == last.surface;
}

} // namespace

void Configure( HullTrace trace, int cacheOffset, int pawnOffset, int groundOffset, int moveTypeOffset, int waterOffset ) {
	g_contacts.assign( NUM_ENT_ENTRIES, {} );
	g_trace = trace;
	g_cacheOffset = cacheOffset;
	g_pawnOffset = pawnOffset;
	g_groundOffset = groundOffset;
	g_moveTypeOffset = moveTypeOffset;
	g_waterOffset = waterOffset;
}

void SetReady( bool ready, float standableZ ) noexcept {
	if ( g_ready && !ready ) {
		ResetMap();
	}
	g_ready = ready;
	g_standableZ = standableZ;
}

void ResetMap() noexcept {
	g_ready = false;
	g_scopes = {};
	std::fill( g_contacts.begin(), g_contacts.end(), Contact{} );
}

void Begin( Phase phase, void* services, CMoveData* move ) noexcept {
	auto& active = Active( phase );
	active = {};
	if ( !g_ready || !services || !move || move->m_bHasZeroFrametime ) {
		return;
	}
	auto* pawn = Read<std::byte*>( static_cast<const std::byte*>( services ), g_pawnOffset );
	const auto handle = EntityHandle( pawn );
	if ( !handle.IsValid() ) {
		return;
	}
	auto& last = g_contacts[static_cast<std::size_t>( handle.GetEntryIndex() )];
	if ( last.pawn != handle ) {
		last = {};
		last.pawn = handle;
	}
	if ( Read<std::uint8_t>( pawn, g_moveTypeOffset ) != static_cast<std::uint8_t>( s2sdk::MoveType::Walk ) ||
		Read<CEntityHandle>( pawn, g_groundOffset ).IsValid() || Read<float>( pawn, g_waterOffset ) > 0.0f ) {
		last.normal = {};
		return;
	}
	active.services = services;
	active.move = move;
	active.pawn = pawn;
	active.last = &last;
	active.incoming = ToVec( move->m_vecVelocity );
}

void End( Phase phase, void* services, CMoveData* move ) noexcept {
	auto scope = std::exchange( Active( phase ), {} );
	if ( phase != Phase::Ground || !scope.move || scope.move != move || scope.services != services ) {
		return;
	}
	auto* pawn = Read<std::byte*>( static_cast<const std::byte*>( services ), g_pawnOffset );
	if ( pawn != scope.pawn || EntityHandle( pawn ) != scope.last->pawn ) {
		return;
	}
	const auto ground = Read<CEntityHandle>( scope.pawn, g_groundOffset );
	if ( !ground.IsValid() ) {
		if ( scope.groundNudge && move->m_vecAbsOrigin == scope.groundNudge->first ) {
			move->m_vecAbsOrigin = scope.groundNudge->second;
		}
		return;
	}
	scope.last->normal = {};
	// SetGroundEntity copies the selected support trace's normal here. The
	// same world entity can contain several faces and the search can reject hits.
	if ( const auto velocity = RestoreLandingSpeed( scope.incoming, ToVec( move->m_vecVelocity ),
		ToVec( move->m_groundNormal ), g_standableZ ) ) {
		move->m_vecVelocity = ToNative( *velocity );
	}
}

void AfterTrace( const void* cache, CGameTrace* hit, const Vector* start, const Vector* end,
	const void* bounds, CTraceFilter* filter ) noexcept {
	// The native cached helper is shared by unrelated queries. Only consume
	// the actual cache belonging to the enclosing movement/category call.
	auto& slide = Active( Phase::Slide );
	auto& ground = Active( Phase::Ground );
	const auto owns = [&]( const Scope& scope ) {
		return scope.move && static_cast<const std::byte*>( scope.services ) + g_cacheOffset == cache;
	};
	auto* scope = owns( slide ) ? &slide : owns( ground ) ? &ground : nullptr;
	if ( !scope || !hit || !start || !end || !bounds || !filter || *start == *end ) {
		return;
	}
	const bool downward = scope == &ground;
	// Categorize also samples material from origin + 4; only support sweeps
	// start at the actual movement origin.
	if ( downward && ( *start != scope->move->m_vecAbsOrigin ||
		start->x != end->x || start->y != end->y || end->z >= start->z ) ) {
		return;
	}
	auto& last = *scope->last;
	const int tick = scope->move->m_nTargetTick;
	const Sweep original = View( *hit );
	if ( NearContact( last, *start, *end, tick ) &&
		( downward ? NeedsGroundRepair( last.normal, original, scope->incoming.z, g_standableZ ) :
			NeedsSlideRepair( last.normal, original, g_standableZ ) ) &&
		SurfaceHandle( *hit ) == last.surface && ProvesRampContact( last, hit->m_vEndPos, bounds, filter ) ) {
		CGameTrace corrected;
		Vector correctedStart;
		const Vec step = ToVec( *end ) - ToVec( *start );
		const float originalProgress = Valid( original ) ? step.Dot( step ) * original.fraction : 0.0f;
		if ( FindCorrection( last.normal, [&]( Vec offset ) {
			const Vector shift = ToNative( offset );
			correctedStart = *start + shift;
			const Vector shiftedEnd = *end + shift;
			corrected.Init();
			g_trace( &correctedStart, &shiftedEnd, bounds, filter, &corrected );
			if ( !FollowsRamp( last.normal, View( corrected ) ) ) {
				return false;
			}
			const Vec finish = ToVec( corrected.m_vEndPos );
			// The sweep and the resulting motion must advance independently of
			// the nudge; otherwise a short sweep could cross a real obstacle.
			if ( !downward && std::min( ( finish - ToVec( correctedStart ) ).Dot( step ),
				( finish - ToVec( *start ) ).Dot( step ) ) <= originalProgress ) {
				return false;
			}
			if ( corrected.m_flFraction < 1.0f && SurfaceHandle( corrected ) != last.surface ) {
				return false;
			}
			const Vector& position = downward ? correctedStart : corrected.m_vEndPos;
			CGameTrace clearance;
			g_trace( &position, &position, bounds, filter, &clearance );
			return !clearance.m_bStartInSolid && clearance.m_flFraction == 1.0f;
		} ) ) {
			// The native movement solver consumes the entire result, including
			// its surface and entity handles. Keep those from the real re-trace.
			std::memcpy( static_cast<void*>( hit ), &corrected, sizeof corrected );
			if ( downward ) {
				scope->groundNudge = std::pair{ *start, correctedStart };
			}
		}
	}
	const Sweep final = View( *hit );
	if ( !Valid( final ) || final.fraction == 1.0f || !IsRamp( final.normal, g_standableZ ) ) {
		return;
	}
	const auto surface = SurfaceHandle( *hit );
	if ( !surface.IsValid() ) {
		return;
	}
	last.normal = final.normal;
	last.point = ToVec( hit->m_vEndPos );
	last.surface = surface;
	last.tick = tick;
}

} // namespace imomoe::server_fixes::ramp
