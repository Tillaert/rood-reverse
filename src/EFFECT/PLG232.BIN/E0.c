#define TRAIL_SAMPLE_COUNT 9
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

void func_800F98E0(func_800FA098_arg0* arg0, func_800FA098_arg1* arg1, D_800F53B8_t* arg2,
    func_800FA76C_arg3* arg3);
INCLUDE_ASM("build/src/EFFECT/PLG232.BIN/nonmatchings/E0", func_800F98E0);

/* Text rays from caster to target */
#include "src/EFFECT/trailPipeline.h"
