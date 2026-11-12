// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

class CMoveData;
class CGameTrace;
class CTraceFilter;
class Vector;

namespace imomoe::server_fixes::ramp {

enum class Phase { Slide, Ground };
using HullTrace = void ( * )( const Vector*, const Vector*, const void*, CTraceFilter*, CGameTrace* );

void Configure( HullTrace trace, int cacheOffset, int pawnOffset, int groundOffset, int moveTypeOffset, int waterOffset );
void SetReady( bool ready, float standableZ = 0.7f ) noexcept;
void ResetMap() noexcept;
void Begin( Phase phase, void* services, CMoveData* move ) noexcept;
void End( Phase phase, void* services, CMoveData* move ) noexcept;
void AfterTrace( const void* cache, CGameTrace* hit, const Vector* start, const Vector* end,
	const void* bounds, CTraceFilter* filter ) noexcept;

} // namespace imomoe::server_fixes::ramp
