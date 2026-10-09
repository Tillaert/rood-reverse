#define TRAIL_SAMPLE_COUNT 9
#define VS_TRAIL_SPIN D_800FE3D4
#define VS_TRAIL_ORIGIN func_800F98E0
#define VS_TRAIL_ENTRY vs_spiritSurge_renderTrails

#include "common.h"
#include "src/EFFECT/trails.h"
#include "src/SLUS_010.40/32154.h"
#include "vs_inline_c.h"
#include "gpu.h"
#include <inline_c.h>
#include <libgte.h>
#include <rand.h>

extern int D_800FE3D4;

/* Where each trail starts */
#include "src/EFFECT/trailOrigin.h"

/* Text rays from caster to target */
#include "src/EFFECT/trailPipeline.h"

/**
 * Concentric text circles over caster
 */
int func_800FBA00(func_800D4910_t* arg0, u_int arg1, int arg2);
INCLUDE_ASM("build/src/EFFECT/PLG049.BIN/nonmatchings/E0", func_800FBA00);

/**
 * Target impact
 */
#define VS_TILED_RING_FUNCTION func_800FC624
#define VS_TILED_RING_COLORS D_800FE3F8
#include "src/EFFECT/tiledRings.h"

/**
 * Column of light and particles on cast
 */
int func_800FD51C(func_800D4910_t* arg0, u_int arg1, int arg2);
INCLUDE_ASM("build/src/EFFECT/PLG049.BIN/nonmatchings/E0", func_800FD51C);
