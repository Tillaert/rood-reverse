/* Shared ribbon-segment orienter.
 *
 * Each node in the caller's list carries two source points; this works out the
 * vector between them, samples the effect's curves by how far along the ribbon
 * the node sits, jitters the radius and the twist, then uses the GTE to swing
 * the offsets into the node's own frame and write the transformed points back.
 *
 * Every reference is to a shared battle symbol, so the twenty-one copies across
 * the overlays compile to the same code -- the overlay supplies only the entry
 * point name. */

#include "src/BATTLE/BATTLE.PRG/5BF94.h"
#include "src/SLUS_010.40/32154.h"
#include "vs_inline_c.h"
#include "gpu.h"
#include <inline_c.h>
#include <rand.h>

/* The overlay may hold more than one ribbon, so this header gets included twice
 * into the same translation unit. The types are guarded; the function is not. */
#ifndef VS_RIBBON_TYPES
#define VS_RIBBON_TYPES

/* One ribbon segment. The two source points bracket the two transformed points
 * the orienter writes back. */
typedef struct {
    SVECTOR from;
    SVECTOR outA;
    SVECTOR outB;
    SVECTOR to;
} vs_ribbonNode;

typedef struct {
    short unk0;
    short count;
    vs_ribbonNode* nodes;
    int unk8;
    int distance;
} vs_ribbonList;

#endif /* VS_RIBBON_TYPES */

void VS_RIBBON_FUNCTION(func_800FA098_arg0* effect, func_800FA098_arg1* scratch,
    int unused, vs_ribbonList* list)
{
    vs_ribbonNode* node;
    int i;
    int rsqrt;
    int curve;
    int scaleA;
    int scaleB;
    int angleA;
    int angleB;
    int alongA;
    int alongB;
    VECTOR* eye;
    VECTOR* target;
    SVECTOR offsets[2];

    for (i = 0; i < list->count; ++i) {
        node = &list->nodes[i];

        /* The span between the two source points goes into the eye vector, which
         * the conversion below overwrites. The reciprocal root works off the
         * squares, which never land in memory. */
        scratch->unk34.vx = node->to.vx - node->from.vx;
        scratch->unk34.vy = node->to.vy - node->from.vy;
        scratch->unk34.vz = node->to.vz - node->from.vz;
        rsqrt = vs_gte_rsqrt(scratch->unk34.vx * scratch->unk34.vx
                             + scratch->unk34.vy * scratch->unk34.vy
                             + scratch->unk34.vz * scratch->unk34.vz);

        curve = vs_battle_sampleCurve(effect->unk12, list->distance);
        vs_battle_lerp2DVector(&effect->unk94[0][0], curve, &scratch->unk124[0]);
        curve = vs_battle_sampleCurve(effect->unk13, list->distance);
        vs_battle_lerp2DVector(&effect->unk94[1][0], curve, &scratch->unk12C[0]);
        curve = vs_battle_sampleCurve(effect->unkC, list->distance);
        vs_battle_lerp2DVector(&effect->unk94[2][0], curve, &scratch->unk118[0]);
        curve = vs_battle_sampleCurve(effect->unkB, list->distance);
        vs_battle_lerpVector(&effect->unk34[1][0], curve, &scratch->unkC8);
        curve = vs_battle_sampleCurve(effect->unkA, list->distance);
        vs_battle_lerpVector(&effect->unk34[0][0], curve, &scratch->unkE8);
        curve = vs_battle_sampleCurve(effect->unkD, list->distance);
        vs_battle_lerpSvector(&effect->unk34[2][0], curve, &scratch->unkF8);

        /* The two offsets the GTE swings are each (cos, sin, along) -- the trig
         * lands in x and y and the jittered span rides in z. */
        /* The test reads the short directly; the arithmetic reloads it widened,
         * which is why the two loads do not collapse into one. */
        if (scratch->unkF8.vx != 0) {
            alongA =
                scratch->unk118[0] + rand() % scratch->unkF8.vx - scratch->unkF8.vx / 2;
        } else {
            alongA = scratch->unk118[0];
        }
        offsets[0].vz = (rsqrt * alongA) / 256;

        if (scratch->unkF8.vy != 0) {
            alongB =
                scratch->unk118[1] + rand() % scratch->unkF8.vy - scratch->unkF8.vy / 2;
        } else {
            alongB = scratch->unk118[1];
        }
        offsets[1].vz = rsqrt + ((rsqrt * alongB) / 256);

        /* Scale and angle for each of the two offsets. */
        scaleA = scratch->unk124[0];
        if (scratch->unk124[1] != 0) {
            scaleA += rand() % scratch->unk124[1];
        }
        scaleB = scratch->unk12C[0];
        if (scratch->unk12C[1] != 0) {
            scaleB += rand() % scratch->unk12C[1];
        }
        angleA = scratch->unkE8.vx;
        if (scratch->unkC8.vx != 0) {
            angleA += rand() % (scratch->unkC8.vx * 2) - scratch->unkC8.vx;
        }
        angleB = scratch->unkE8.vy;
        if (scratch->unkC8.vy != 0) {
            angleB += rand() % (scratch->unkC8.vy * 2) - scratch->unkC8.vy;
        }

        /* Both offsets are (cos, sin, along) in the same handedness. */
        offsets[0].vx = (rcos(angleA) * scaleA) >> 12;
        offsets[0].vy = (rsin(angleA) * scaleA) >> 12;
        offsets[1].vx = (rcos(angleB) * scaleB) >> 12;
        offsets[1].vy = (rsin(angleB) * scaleB) >> 12;

        eye = &scratch->unk34;
        target = &scratch->unk44;
        vs_battle_svecToVec((SVECTOR*)node, eye);
        vs_battle_svecToVec((SVECTOR*)&node->to, target);
        vs_battle_lookAt(eye, target, &scratch->unk64);

        /* The eye has to stay in a callee-saved register across the whole loop,
         * and GCC 2.8 decides that by score: floor(log2(refs)) * refs / lifetime.
         * Measured here the eye comes out at 11 refs / 192 live = 1718 against
         * the ribbon list's 18 refs / 390 = 1846, so the list wins $s7 and the
         * eye falls to $fp -- the reverse of the original. Twelve refs puts the
         * eye at 1875 and the ordering right. The empty asm below is the twelfth
         * reference; it constrains the value to a register and emits nothing. */
        __asm__ volatile("" ::"r"(eye));

        gte_SetRotMatrix(&scratch->unk64);
        gte_SetTransVector2(eye);
        gte_ldv0(&offsets[0]);
        gte_rtv0tr2();
        gte_stlvnl(eye);
        vs_battle_vecToSvec(eye, &node->outA);
        gte_ldv0(&offsets[1]);
        gte_rtv0tr2();
        gte_stlvnl(target);
        vs_battle_vecToSvec(target, &node->outB);
    }
}
