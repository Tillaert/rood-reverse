#pragma once
#include "src/BATTLE/BATTLE.PRG/5BF94.h"
#include <stddef.h>
#include <libgte.h>

/* Ring buffer depth for a trail's spline samples. Overlays use 9 or 5, which
 * changes the trail stride (0x78 vs 0x58) and every offset derived from it. */
#ifndef TRAIL_SAMPLE_COUNT
#define TRAIL_SAMPLE_COUNT 9
#endif

typedef struct {
    SVECTOR splinePoints[4];
    u_char stepCount;
    u_char splineSampleCount;
    u_char trailCollapseCounter;
    u_char currentSplineSample;
    SVECTOR splineSamples[TRAIL_SAMPLE_COUNT];
    int splineProgress;
    int speed;
    int unk;
} _trail;

typedef struct {
    u_char unk0;
    u_char unk1;
    u_char trailcount;
    u_char unk3;
    _trail* trails;
    int curveSampleDistance;
    func_800D6CF_t unkC;
    u_char sampledTransparencyCurve[0];
} func_800FA76C_arg3;

/* A trail segment is emitted as a quad plus the draw mode that follows it. */
typedef struct {
    u_char unk0;
    u_char unk1;
    u_char unk2;
    u_char unk3;
    POLY_GT4 unk4;
    int unk38;
} vs_trailPrim;
