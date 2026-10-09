#define TRAIL_SAMPLE_COUNT 9
#define VS_TRAIL_SPIN D_800FBA00
#define VS_TRAIL_ORIGIN func_800F98E0
#define VS_TRAIL_ENTRY func_800FB4C0

#include "common.h"
#include "src/EFFECT/trails.h"
#include "src/SLUS_010.40/32154.h"
#include "vs_inline_c.h"
#include "gpu.h"
#include <inline_c.h>
#include <libgte.h>
#include <rand.h>

extern int D_800FBA00;

/* Where each trail starts */
#include "src/EFFECT/trailOrigin.h"

/* Text rays from caster to target */
#include "src/EFFECT/trailPipeline.h"
