#define TRAIL_SAMPLE_COUNT 9
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

static void func_800F98E0(func_800FA098_arg0* arg0, func_800FA098_arg1* arg1,
    D_800F53B8_t* arg2, func_800FA76C_arg3* arg3)
{
    int sp10;
    int sp14;
    MATRIX* matrix;
    SVECTOR* splinePoint;
    int temp_a1;
    int temp_lo;
    int temp_s0_2;
    int temp_s3;
    int temp_v0;
    int temp_v0_4;
    int i;
    u_short var_a0;
    u_short var_v1;
    u_short var_v1_2;

    vs_battle_lerpVector(arg0->unk18,
        vs_battle_sampleCurve(arg0->unk8, arg3->curveSampleDistance), &arg1->unk98);

    matrix = &arg2->unk1C[arg0->unkC8].unk38;

    // Determines trail origin:
    switch (arg1->flags & 7) {
    case 0:
        break;
    // Fixed point D_800F5310
    case 1:
        vs_battle_addVecToSvec(&arg1->unk98, D_800F5310, &arg1->unk98);
        break;
    // Caster
    case 2:
        applyVector(&arg1->unk98, matrix->t[0], matrix->t[1], matrix->t[2], +=);
        break;
    }

    vs_battle_lerpVector(arg0->unk24,
        vs_battle_sampleCurve(arg0->unk9, arg3->curveSampleDistance), &arg1->unkA8);
    SetRotMatrix(matrix);
    gte_zrtr();

    switch (arg1->flags & 0x38) {
    case 0:
        for (i = 0; i < arg3->trailcount; ++i) {

            splinePoint = &arg3->trails[i].splinePoints[0];

            temp_v0 = rand();
            arg1->unk170 = rsin(temp_v0);
            arg1->unk178 = rcos(temp_v0);

            temp_v0 = rand();
            arg1->unk174 = rsin(temp_v0);
            arg1->unk17C = rcos(temp_v0);

            setVector(splinePoint,
                ((((arg1->unkA8.vx * arg1->unk178) >> 0xC) * arg1->unk17C) >> 0xC),
                ((((arg1->unkA8.vy * arg1->unk178) >> 0xC) * arg1->unk174) >> 0xC),
                ((arg1->unkA8.vz * arg1->unk170) >> 0xC));

            if (arg1->flags & 0x40000) {
                ApplyRotMatrix(splinePoint, &arg1->unk34);
                copyVector(splinePoint, &arg1->unk34);
            }

            addVector(splinePoint, &arg1->unk98);
        }
        return;

    case 8:
        for (i = 0; i < arg3->trailcount; ++i) {

            splinePoint = &arg3->trails[i].splinePoints[0];

            switch (arg1->flags & 0x1800) {
            case 0x0:
            case 0x800:
                switch (rand() % 6) {
                case 0:
                    splinePoint->vx = arg1->unkA8.vx;
                    if (0) {
                    case 1:
                        splinePoint->vx = -arg1->unkA8.vx;
                    }

                    splinePoint->vy =
                        vs_battle_randUniformInt(arg1->unkA8.vy, -arg1->unkA8.vy);
                    splinePoint->vz =
                        vs_battle_randUniformInt(arg1->unkA8.vz, -arg1->unkA8.vz);
                    break;

                case 2:
                    splinePoint->vy = arg1->unkA8.vy;
                    if (0) {
                    case 3:
                        splinePoint->vy = -arg1->unkA8.vy;
                    }

                    splinePoint->vx =
                        vs_battle_randUniformInt(arg1->unkA8.vx, -arg1->unkA8.vx);
                    splinePoint->vz =
                        vs_battle_randUniformInt(arg1->unkA8.vz, -arg1->unkA8.vz);
                    break;

                case 4:
                    splinePoint->vz = arg1->unkA8.vz;
                    if (0) {
                    case 5:
                        splinePoint->vz = -arg1->unkA8.vz;
                    }

                    splinePoint->vx =
                        vs_battle_randUniformInt(arg1->unkA8.vx, -arg1->unkA8.vx);
                    splinePoint->vy =
                        vs_battle_randUniformInt(arg1->unkA8.vy, -arg1->unkA8.vy);
                    break;
                }

                break;

            case 0x1000:
                temp_a1 = D_800FE3D4 + 1;
                D_800FE3D4 = temp_a1 & 7;

                var_a0 = arg1->unkA8.vx;
                if (!(temp_a1 & 1)) {
                    var_a0 = -var_a0;
                }

                splinePoint->vx = var_a0;

                var_v1 = arg1->unkA8.vy;
                if (!(temp_a1 & 2)) {
                    var_v1 = -var_v1;
                }
                splinePoint->vy = var_v1;

                var_v1_2 = arg1->unkA8.vz;
                if (!(temp_a1 & 4)) {
                    var_v1_2 = -var_v1_2;
                }
                splinePoint->vz = var_v1_2;

                break;
            }

            if (arg1->flags & 0x40000) {
                ApplyRotMatrix(splinePoint, &arg1->unk34);
                copyVector(splinePoint, &arg1->unk34);
            }

            addVector(splinePoint, &arg1->unk98);
        }

        return;

    case 16:
    case 24:
    case 32:
    case 40:
        arg1->unk184 = 4;
        arg1->unk188 = 4;
        break;

    case 48:
        arg1->unk184 = arg0->unkCA;
        if (arg1->unk184 == 0) {
            arg1->unk184 = 1;
        }
        arg1->unk188 = arg0->unkCB;
        if (arg1->unk188 == 0) {
            arg1->unk188 = arg1->unk184;
        }
        break;

    default:
        return;
    }

    temp_lo = ONE / arg1->unk184;

    if ((arg1->flags & 0x1800) != 0x1000) {
        D_800FE3D4 = D_800FE3D4 % arg1->unk188;
        temp_s0_2 = (D_800FE3D4 * temp_lo) + ONE / 8;
        sp10 = (rcos(temp_s0_2) * arg1->unkA8.vx) >> 0xC;
        sp14 = (rsin(temp_s0_2) * arg1->unkA8.vz) >> 0xC;
    }

    for (i = 0; i < arg3->trailcount; ++i) {
        splinePoint = &arg3->trails[i].splinePoints[0];
        D_800FE3D4 = (D_800FE3D4 + 1) % arg1->unk188;
        temp_s0_2 = (D_800FE3D4 * temp_lo) + ONE / 8;
        temp_s3 = (rcos(temp_s0_2) * arg1->unkA8.vx) >> 0xC;
        temp_s0_2 = (rsin(temp_s0_2) * arg1->unkA8.vz) >> 0xC;

        if ((arg1->flags & 0x1800) == 0x1000) {
            splinePoint->vx = temp_s3;
            splinePoint->vz = temp_s0_2;
        } else {
            temp_v0_4 = rand() >> 3;
            splinePoint->vx = (((temp_s3 - sp10) * temp_v0_4) >> 0xC) + sp10;
            splinePoint->vz = (((temp_s0_2 - sp14) * temp_v0_4) >> 0xC) + sp14;
        }

        splinePoint->vy = 0;

        if (arg1->flags & 0x40000) {
            ApplyRotMatrix(splinePoint, &arg1->unk34);
            copyVector(splinePoint, &arg1->unk34);
        }

        addVector(splinePoint, &arg1->unk98);
    }
}

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
