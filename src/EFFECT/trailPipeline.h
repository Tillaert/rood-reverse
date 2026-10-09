/* Shared trail effect pipeline.
 *
 * A trail is a bundle of ribbons swept along a cubic spline from the caster to
 * the target. Every trail overlay drives the same six stages from one entry
 * point, so the pipeline lives here and the overlay supplies:
 *
 *   TRAIL_SAMPLE_COUNT  - ring buffer depth, 9 or 5 (see trails.h)
 *   VS_TRAIL_ORIGIN     - where each trail starts; differs per overlay
 *   VS_TRAIL_ENTRY      - the overlay's effect entry point
 *
 * The intermediate stage names default to their canonical addresses and may be
 * overridden before including this header when an overlay lays them out
 * elsewhere. Include this after the origin function so the emitted function
 * order matches the original binary.
 */

#define VS_TRAIL_COLLAPSE_INIT (TRAIL_SAMPLE_COUNT - 1)

#ifndef VS_TRAIL_TARGET
#define VS_TRAIL_TARGET func_800FA07C
#endif
#ifndef VS_TRAIL_ORIENT
#define VS_TRAIL_ORIENT func_800FA294
#endif
#ifndef VS_TRAIL_INIT
#define VS_TRAIL_INIT func_800FA76C
#endif
#ifndef VS_TRAIL_ADVANCE
#define VS_TRAIL_ADVANCE func_800FA8E4
#endif
#ifndef VS_TRAIL_RENDER
#define VS_TRAIL_RENDER func_800FAA70
#endif

/* Resolve the spline end point the trails converge on. */
static void VS_TRAIL_TARGET(func_800FA098_arg0* arg0, func_800FA098_arg1* arg1,
    D_800F53B8_t* arg2, func_800FA76C_arg3* arg3)
{
    int i;

    vs_battle_lerpSvector(arg0->unk34[6],
        vs_battle_sampleCurve(arg0->unkF, arg3->curveSampleDistance), &arg1->unk13C);
    vs_battle_lerpSvector(arg0->unk34[7],
        vs_battle_sampleCurve(arg0->unk10, arg3->curveSampleDistance), &arg1->unk144);

    switch (arg1->flags & 0x1C0) {
    case 0:
        break;

    case 0xC0:
        applyVector(&arg1->unk13C, arg2->unk1C[arg0->unkC9].unk38.t[0],
            arg2->unk1C[arg0->unkC9].unk38.t[1], arg2->unk1C[arg0->unkC9].unk38.t[2], +=);
        break;

    case 0x80:
        applyVector(&arg1->unk13C, D_800F5230.position.vx, D_800F5230.position.vy,
            D_800F5230.position.vz, +=);
        break;
    }

    for (i = 0; i < arg3->trailcount; ++i) {
        SVECTOR* v = &arg3->trails[i].splinePoints[3];
        setVector(v,
            arg1->unk13C.vx + vs_battle_randUniformInt(arg1->unk144.vx, -arg1->unk144.vx),
            arg1->unk13C.vy + vs_battle_randUniformInt(arg1->unk144.vy, -arg1->unk144.vy),
            arg1->unk13C.vz
                + vs_battle_randUniformInt(arg1->unk144.vz, -arg1->unk144.vz));
    }
}

/* Square up the four control points into a ribbon about the trail axis. */
static void VS_TRAIL_ORIENT(func_800FA098_arg0* arg0, func_800FA098_arg1* arg1,
    D_800F53B8_t* arg2 __attribute__((unused)), func_800FA76C_arg3* arg3)
{
    SVECTOR sp10;
    SVECTOR sp18;
    int temp_s0;
    int i;
    int var_s0;
    int var_s1;
    int var_s3;
    int var_s4;

    do {
        for (i = 0; i < arg3->trailcount; ++i) {

            SVECTOR* temp_s5 = arg3->trails[i].splinePoints;

            setVector(&arg1->unk34, temp_s5[3].vx - temp_s5[0].vx,
                temp_s5[3].vy - temp_s5[0].vy, temp_s5[3].vz - temp_s5[0].vz);

            temp_s0 = vs_gte_rsqrt((arg1->unk34.vx * arg1->unk34.vx)
                                   + (arg1->unk34.vy * arg1->unk34.vy)
                                   + (arg1->unk34.vz * arg1->unk34.vz));
            vs_battle_lerp2DVector(arg0->unk94[0],
                vs_battle_sampleCurve(arg0->unk12, arg3->curveSampleDistance),
                arg1->unk124);
            vs_battle_lerp2DVector(arg0->unk94[1],
                vs_battle_sampleCurve(arg0->unk13, arg3->curveSampleDistance),
                arg1->unk12C);
            vs_battle_lerp2DVector(arg0->unk94[2],
                vs_battle_sampleCurve(arg0->unkC, arg3->curveSampleDistance),
                arg1->unk118);
            vs_battle_lerpVector(arg0->unk34[1],
                vs_battle_sampleCurve(arg0->unkB, arg3->curveSampleDistance),
                &arg1->unkC8);
            vs_battle_lerpVector(arg0->unk34[0],
                vs_battle_sampleCurve(arg0->unkA, arg3->curveSampleDistance),
                &arg1->unkE8);
            vs_battle_lerpSvector(arg0->unk34[2],
                vs_battle_sampleCurve(arg0->unkD, arg3->curveSampleDistance),
                &arg1->unkF8);

            if (arg1->unkF8.vx != 0) {
                int var_v0_2 =
                    (arg1->unk118[0] + (rand() % arg1->unkF8.vx)) - (arg1->unkF8.vx / 2);
                sp10.vz = (temp_s0 * var_v0_2) / 256;
            } else {
                sp10.vz = (temp_s0 * arg1->unk118[0]) / 256;
            }

            if (arg1->unkF8.vy != 0) {
                int var_v0_3 =
                    (arg1->unk118[1] + (rand() % arg1->unkF8.vy)) - (arg1->unkF8.vy / 2);
                sp18.vz = temp_s0 + ((temp_s0 * var_v0_3) / 256);
            } else {
                sp18.vz = temp_s0 + ((temp_s0 * arg1->unk118[1]) / 256);
            }

            var_s3 = arg1->unk124[0];

            var_s0 = arg1->unk124[1] != 0;
            if (var_s0) {
                var_s3 += rand() % arg1->unk124[1];
            }

            var_s4 = arg1->unk12C[0];

            if (arg1->unk12C[1] != 0) {
                var_s4 += rand() % arg1->unk12C[1];
            }

            var_s0 = arg1->unkE8.vx;

            if (arg1->unkC8.vx != 0) {
                var_s0 += (rand() % (arg1->unkC8.vx * 2)) - arg1->unkC8.vx;
            }

            var_s1 = arg1->unkE8.vy;

            if (arg1->unkC8.vy != 0) {
                if (&arg1->unk34) {
                    var_s1 += (rand() % (arg1->unkC8.vy * 2)) - arg1->unkC8.vy;
                } else {
                    var_s1 += (rand() % (arg1->unkC8.vy * 2)) - arg1->unkC8.vy;
                }
            }

            sp10.vx = (rcos(var_s0) * var_s3) >> 0xC;
            sp10.vy = (rsin(var_s0) * var_s3) >> 0xC;
            sp18.vx = (rcos(var_s1) * var_s4) >> 0xC;
            sp18.vy = (rsin(var_s1) * var_s4) >> 0xC;

            vs_battle_svecToVec(temp_s5, &arg1->unk34);
            vs_battle_svecToVec(temp_s5 + 3, &arg1->unk44);
            vs_battle_lookAt(&arg1->unk34, &arg1->unk44, &arg1->unk64);
            gte_SetRotMatrix(&arg1->unk64);
            gte_SetTransVector2(&arg1->unk34);
            gte_ldv0(&sp10);
            gte_rtv0tr2();
            gte_stlvnl(&arg1->unk34);
            vs_battle_vecToSvec(&arg1->unk34, temp_s5 + 1);
            gte_ldv0(&sp18);
            gte_rtv0tr2();
            gte_stlvnl(&arg1->unk44);
            vs_battle_vecToSvec(&arg1->unk44, temp_s5 + 2);
        }
    } while (0);
}

/* Seed each trail with a random step count and its speed/acceleration budget. */
static void VS_TRAIL_INIT(func_800FA098_arg0* arg0, func_800FA098_arg1* arg1,
    D_800F53B8_t* arg2 __attribute__((unused)), func_800FA76C_arg3* arg3)
{
    int speed;
    int i;

    vs_battle_lerp2DVector(arg0->unk94[4], 0, arg1->unk134);

    speed = func_800CFE1C(arg0->unk30, 0) << 0xC;

    for (i = 0; i < arg3->trailcount; ++i) {
        int stepCount = vs_battle_randUniformInt(arg1->unk134[0], arg1->unk134[1]);
        if (stepCount == 0) {
            stepCount = 1;
        }
        arg3->trails[i].stepCount = stepCount;
        arg3->trails[i].splineSampleCount = 1;
        arg3->trails[i].trailCollapseCounter = VS_TRAIL_COLLAPSE_INIT;
        arg3->trails[i].currentSplineSample = 0;
        arg3->trails[i].splineSamples[0] = arg3->trails[i].splinePoints[0];
        arg3->trails[i].splineProgress = 0;
        arg3->trails[i].speed = speed;
        arg3->trails[i].unk =
            ((0x06000000 / (stepCount * stepCount)) - ((speed * 2) / stepCount));
    }
}

/* Advance the ring buffer one sample, collapsing the tail as the trail ages out. */
static void VS_TRAIL_ADVANCE(func_800FA098_arg0* arg0 __attribute__((unused)),
    func_800FA098_arg1* arg1 __attribute__((unused)),
    D_800F53B8_t* arg2 __attribute__((unused)), func_800FA76C_arg3* arg3)
{
    int i;

    for (i = 0; i < arg3->trailcount; ++i) {

        _trail* temp_s0 = &arg3->trails[i];

        if (temp_s0->trailCollapseCounter == 0) {
            continue;
        }

        if (arg3->curveSampleDistance >= temp_s0->stepCount) {
            --temp_s0->trailCollapseCounter;
            if (temp_s0->trailCollapseCounter < temp_s0->stepCount) {
                --temp_s0->splineSampleCount;
            }
        } else {
            int currentStep = temp_s0->stepCount - arg3->curveSampleDistance;
            int speed = temp_s0->speed;

            speed += ((0x06000000 - (temp_s0->splineProgress * 2))
                         / (currentStep * currentStep))
                   - ((speed * 2) / currentStep);
            temp_s0->speed = speed;
            temp_s0->splineProgress += speed;
            temp_s0->currentSplineSample =
                (temp_s0->currentSplineSample + 1) % TRAIL_SAMPLE_COUNT;

            vs_battle_splineInterpolate(&temp_s0->splinePoints[0],
                &temp_s0->splinePoints[1], &temp_s0->splinePoints[2],
                &temp_s0->splinePoints[3], (temp_s0->splineProgress >> 0xC) - ONE,
                &temp_s0->splineSamples[temp_s0->currentSplineSample]);

            if (temp_s0->splineSampleCount < TRAIL_SAMPLE_COUNT) {
                ++temp_s0->splineSampleCount;
            }
        }
    }
}

/* Emit the ribbon quads, walking the sample ring back to front. */
void VS_TRAIL_RENDER(func_800FA098_arg0* arg0, func_800FA098_arg1* arg1,
    D_800F53B8_t* arg2, func_800FA76C_arg3* arg3)
{
    vs_trailPrim sp10;
    DVECTOR sp50[TRAIL_SAMPLE_COUNT + 1];
    int sp78[TRAIL_SAMPLE_COUNT + 1];
    int spA0[TRAIL_SAMPLE_COUNT - 1];
    CVECTOR spC0;
    int _[TRAIL_SAMPLE_COUNT + 1] __attribute__((unused));
    DVECTOR* spF0;
    int i;
    int j;
    pFileTexture* temp_s7;

    vs_battle_lerpSvector(arg0->unk34[4],
        vs_battle_sampleCurve(arg0->unkE, arg3->curveSampleDistance), &arg1->unk108);

    if (arg0->rCurve != 0) {
        j = vs_battle_sampleCurve(arg0->rCurve, arg3->curveSampleDistance) * 2;
        spC0.r = j <= 255 ? j : 255;
    } else {
        spC0.r = 128;
    }

    if (arg0->gCurve != 0) {
        j = vs_battle_sampleCurve(arg0->gCurve, arg3->curveSampleDistance) * 2;
        spC0.g = j <= 255 ? j : 255;
    } else {
        spC0.g = 128;
    }

    if (arg0->bCurve != 0) {
        j = vs_battle_sampleCurve(arg0->bCurve, arg3->curveSampleDistance) * 2;
        spC0.b = j <= 255 ? j : 255;
    } else {
        spC0.b = 128;
    }

    spC0.cd = primPolyGT4SemiTrans;

    if (arg1->flags & 0x20000) {
        SetRotMatrix(&arg2->unk1C[arg0->unkC8].unk58);
        SetTransMatrix(&arg2->unk1C[arg0->unkC8].unk58);
    } else {
        SetRotMatrix(&vs_scratch.viewMatrix);
        SetTransMatrix(&vs_scratch.viewMatrix);
    }

    func_800D6D24(&arg3->unkC);

    temp_s7 = &D_800F569C->textures[arg3->unkC.textureInfo->dataIndex];

    for (i = 0; i < arg3->trailcount; ++i) {
        _trail* temp_s1 = &arg3->trails[i];
        SVECTOR* temp = temp_s1->splineSamples;
        vs_trailPrim* temp_a3;
        int temp_s0;
        int temp_a1_2;
        int temp_s3;
        int spF8;
        int spFC;

        if (temp_s1->splineSampleCount < 2) {
            continue;
        }

        temp_s3 = temp_s1->currentSplineSample;

        for (j = 0; j < temp_s1->splineSampleCount; ++j) {
            gte_ldv0(&temp[temp_s3]);
            gte_rtps2();
            gte_stsxy(&sp50[j]);
            gte_stszotz(&sp78[j]);
            temp_s3 = (temp_s3 + VS_TRAIL_COLLAPSE_INIT) % TRAIL_SAMPLE_COUNT;
        }

        for (j = 0; j < (temp_s1->splineSampleCount - 1); ++j) {
            DVECTOR* p = &sp50[j];
            spA0[j] = ratan2(p[1].vy - p[0].vy, p[1].vx - p[0].vx);
            spA0[j] -= ONE / 4;
        }

        temp_s3 = arg1->unk108.vx / 2;
        temp_s0 = (rcos(spA0[0]) * temp_s3) >> 0xC;
        temp_a1_2 = (rsin(spA0[0]) * temp_s3) >> 0xC;
        spF0 = sp50;
        sp10.unk4.x0 = spF0[0].vx + temp_s0;
        sp10.unk4.y0 = spF0[0].vy + temp_a1_2;
        sp10.unk4.x2 = spF0[0].vx - temp_s0;
        sp10.unk4.y2 = spF0[0].vy - temp_a1_2;

        sp10.unk4.u0 = sp10.unk4.u2 =
            temp_s7->u
            + ((temp_s7->w * temp_s1->trailCollapseCounter) / VS_TRAIL_COLLAPSE_INIT);
        spF8 = temp_s7->v;
        spFC = temp_s7->v + temp_s7->h;
        sp10.unk4.v0 = spF8;
        sp10.unk4.v2 = spFC;
        sp10.unk4.r0 =
            (spC0.r * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter]) >> 7;
        sp10.unk4.g0 =
            (spC0.g * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter]) >> 7;
        sp10.unk4.b0 =
            (spC0.b * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter]) >> 7;
        sp10.unk4.code = spC0.cd;

        for (j = 0; j < (temp_s1->splineSampleCount - 2); ++j) {
            DVECTOR* temp2;
            int var_s0_2 = (spA0[j] + spA0[j + 1]) / 2;

            if (((spA0[j + 1] - spA0[j]) + (ONE / 2 - 1)) >= (ONE - 1u)) {
                var_s0_2 += ONE / 2;
            }

            temp_s0 = (rcos(var_s0_2) * temp_s3) >> 0xC;
            temp_a1_2 = (rsin(var_s0_2) * temp_s3) >> 0xC;
            temp2 = &spF0[j];
            sp10.unk4.x1 = temp2[1].vx + temp_s0;
            sp10.unk4.y1 = temp2[1].vy + temp_a1_2;
            sp10.unk4.x3 = temp2[1].vx - temp_s0;
            sp10.unk4.y3 = temp2[1].vy - temp_a1_2;

            sp10.unk4.u1 = sp10.unk4.u3 =
                temp_s7->u
                + ((temp_s7->w * ((temp_s1->trailCollapseCounter - j) - 1))
                    / VS_TRAIL_COLLAPSE_INIT);
            sp10.unk4.v1 = spF8;
            sp10.unk4.v3 = spFC;
            sp10.unk4.r1 =
                ((spC0.r
                     * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter - j
                                                      - 1])
                    >> 7);
            sp10.unk4.g1 =
                ((spC0.g
                     * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter - j
                                                      - 1])
                    >> 7);
            sp10.unk4.b1 =
                ((spC0.b
                     * arg3->sampledTransparencyCurve[temp_s1->trailCollapseCounter - j
                                                      - 1])
                    >> 7);
            sp10.unk4.p1 = spC0.cd;

            temp_s0 = (sp78[j] + sp78[j + 1]) / 2;

            if ((temp_s0 < ONE / 2) && (vs_main_nearClip < temp_s0)) {
                temp_a3 = vs_scratch.unk0;
                vs_scratch.unk0 = temp_a3 + 1;
                setlen(temp_a3, 0xE);
                temp_a3->unk4.code = primPolyGT4SemiTrans;
                temp_a3->unk4.tag = _get_mode(0, 0, 0);
                temp_a3->unk38 = _get_mode(0, 1, 0);
                *(int*)&temp_a3->unk0 =
                    (((u_long*)vs_scratch.unk4)[temp_s0] & 0xFFFFFF) | 0x0E000000;
                ((u_long*)vs_scratch.unk4)[temp_s0] =
                    (((u_long*)vs_scratch.unk4)[temp_s0] & 0xFF000000)
                    | ((u_long)temp_a3 & 0xFFFFFF);
                *(int*)&temp_a3->unk4.r0 = *(int*)&sp10.unk4.r0;
                *(int*)&temp_a3->unk4.r1 = *(int*)&sp10.unk4.r1;
                *(int*)&temp_a3->unk4.r2 = *(int*)&sp10.unk4.r0;
                *(int*)&temp_a3->unk4.r3 = *(int*)&sp10.unk4.r1;
                *(int*)&temp_a3->unk4.x0 = *(int*)&sp10.unk4.x0;
                *(int*)&temp_a3->unk4.x1 = *(int*)&sp10.unk4.x1;
                *(int*)&temp_a3->unk4.x2 = *(int*)&sp10.unk4.x2;
                *(int*)&temp_a3->unk4.x3 = *(int*)&sp10.unk4.x3;
                *(short*)&temp_a3->unk4.u0 = *(short*)&sp10.unk4.u0;
                *(short*)&temp_a3->unk4.u1 = *(short*)&sp10.unk4.u1;
                *(short*)&temp_a3->unk4.u2 = *(short*)&sp10.unk4.u2;
                *(short*)&temp_a3->unk4.u3 = *(short*)&sp10.unk4.u3;
                *(short*)&temp_a3->unk4.tpage = temp_s7->tpage;
                *(short*)&temp_a3->unk4.clut = temp_s7->clut;
            }

            *(int*)&sp10.unk4.x0 = *(int*)&sp10.unk4.x1;
            *(int*)&sp10.unk4.x2 = *(int*)&sp10.unk4.x3;
            *(short*)&sp10.unk4.u0 = *(short*)&sp10.unk4.u1;
            *(short*)&sp10.unk4.u2 = *(short*)&sp10.unk4.u3;
            *(int*)&sp10.unk4.r0 = *(int*)&sp10.unk4.r1;
        }

        temp_s0 = (rcos(spA0[j]) * temp_s3) >> 0xC;
        temp_a1_2 = (rsin(spA0[j]) * temp_s3) >> 0xC;
        sp10.unk4.x1 = ((u_short*)spF0)[j * 2 + 2] + temp_s0;
        sp10.unk4.y1 = ((u_short*)spF0)[j * 2 + 3] + temp_a1_2;
        sp10.unk4.x3 = ((u_short*)spF0)[j * 2 + 2] - temp_s0;
        sp10.unk4.y3 = ((u_short*)spF0)[j * 2 + 3] - temp_a1_2;
        sp10.unk4.u1 = sp10.unk4.u3 = temp_s7->u;
        sp10.unk4.v1 = spF8;
        sp10.unk4.v3 = spFC;
        sp10.unk4.r1 = (spC0.r * arg3->sampledTransparencyCurve[0]) >> 7;
        sp10.unk4.g1 = (spC0.g * arg3->sampledTransparencyCurve[0]) >> 7;
        sp10.unk4.b1 = (spC0.b * arg3->sampledTransparencyCurve[0]) >> 7;

        temp_s0 = (sp78[j] + sp78[j + 1]) / 2;

        if ((temp_s0 < ONE / 2) && (vs_main_nearClip < temp_s0)) {
            temp_a3 = vs_scratch.unk0;
            vs_scratch.unk0 = temp_a3 + 1;
            setlen(temp_a3, 0xE);
            setcode(&temp_a3->unk4, primPolyGT4SemiTrans);
            temp_a3->unk4.tag = _get_mode(0, 0, 0);
            temp_a3->unk38 = _get_mode(0, 1, 0);
            *(int*)&temp_a3->unk0 =
                (((u_long*)vs_scratch.unk4)[temp_s0] & 0xFFFFFF) | 0x0E000000;
            ((u_long*)vs_scratch.unk4)[temp_s0] =
                (((u_long*)vs_scratch.unk4)[temp_s0] & 0xFF000000)
                | ((u_long)temp_a3 & 0xFFFFFF);
            *(int*)&temp_a3->unk4.r0 = *(int*)&sp10.unk4.r0;
            *(int*)&temp_a3->unk4.r1 = *(int*)&sp10.unk4.r1;
            *(int*)&temp_a3->unk4.r2 = *(int*)&sp10.unk4.r0;
            *(int*)&temp_a3->unk4.r3 = *(int*)&sp10.unk4.r1;
            *(int*)&temp_a3->unk4.x0 = *(int*)&sp10.unk4.x0;
            *(int*)&temp_a3->unk4.x1 = *(int*)&sp10.unk4.x1;
            *(int*)&temp_a3->unk4.x2 = *(int*)&sp10.unk4.x2;
            *(int*)&temp_a3->unk4.x3 = *(int*)&sp10.unk4.x3;
            *(short*)&temp_a3->unk4.u0 = *(short*)&sp10.unk4.u0;
            *(short*)&temp_a3->unk4.u1 = *(short*)&sp10.unk4.u1;
            *(short*)&temp_a3->unk4.u2 = *(short*)&sp10.unk4.u2;
            *(short*)&temp_a3->unk4.u3 = *(short*)&sp10.unk4.u3;
            *(short*)&temp_a3->unk4.tpage = temp_s7->tpage;
            *(short*)&temp_a3->unk4.clut = temp_s7->clut;
        }
    }
}

/* Drive one tick of the whole pipeline for every trail in the bundle. */
int VS_TRAIL_ENTRY(func_800D4910_t* arg0, u_int arg1, int arg2)
{
    func_800FB4C0_t sp18;
    func_800FA098_arg0* temp_s3;
    int trail;
    int j;
    func_800FA76C_arg3* temp_v0;

    func_800FA098_arg1* s4 = (func_800FA098_arg1*)0x1F8001D0;
    func_800FA76C_arg3* temp_s0 = arg0->unk8;
    D_800F53B8_t* temp_s5 = D_800F53BC;
    int s6 = 1;

    switch (arg1) {
    case 1:
        temp_v0 = vs_main_allocHeapR(sizeof *temp_v0 + 12);
        arg0->unk8 = temp_v0;
        temp_v0->unk1 = arg2 >> 8;
        temp_v0->unk0 = arg2;
        temp_v0->curveSampleDistance = 0;
        temp_s3 = &D_800F569C->block5Data->unk4[temp_v0->unk1];
        trail = temp_s3->unkC0[0];

        if (trail > VS_TRAIL_COLLAPSE_INIT) {
            trail = VS_TRAIL_COLLAPSE_INIT;
        }

        temp_v0->trails = vs_main_allocHeapR(trail * sizeof *temp_v0->trails);
        temp_v0->trailcount = trail;
        temp_v0->unk3 = 0;

        if (temp_s3->transparencyCurve != 0) {

            // bad var reuse, trail = length of curve
            trail = D_800F569C->curveBlock->curveSizes[temp_s3->transparencyCurve];

            if (trail > 1) {
                --trail;
            }

            for (j = 0; j < TRAIL_SAMPLE_COUNT; ++j) {
                temp_v0->sampledTransparencyCurve[j] =
                    D_800F569C->curves[temp_s3->transparencyCurve]
                                      [(j * trail) / VS_TRAIL_COLLAPSE_INIT];
            }

        } else {
            for (j = 0; j < TRAIL_SAMPLE_COUNT; ++j) {
                temp_v0->sampledTransparencyCurve[j] = 128;
            }
        }

        func_800D6CCC((int*)&temp_v0->unkC);
        func_800D6CF0(&temp_v0->unkC, temp_s3->unk1, temp_s3->unk0);

        break;

    case 2:
        temp_s3 = &D_800F569C->block5Data->unk4[temp_s0->unk1];
        s4->flags = temp_s3->flags;

        if (temp_s0->unk3 == 0) {
            VS_TRAIL_ORIGIN(temp_s3, s4, temp_s5, temp_s0);
            VS_TRAIL_TARGET(temp_s3, s4, temp_s5, temp_s0);
            VS_TRAIL_ORIENT(temp_s3, s4, temp_s5, temp_s0);
            VS_TRAIL_INIT(temp_s3, s4, temp_s5, temp_s0);
            temp_s0->unk3 = 1;
        }

        VS_TRAIL_ADVANCE(temp_s3, s4, temp_s5, temp_s0);
        VS_TRAIL_RENDER(temp_s3, s4, temp_s5, temp_s0);

        if (temp_s3->unk3 != 0) {
            for (trail = 0; trail < temp_s0->trailcount; ++trail) {

                sp18.unk60 = 0;

                if (temp_s3->flags & 0x20000) {

                    SetRotMatrix(&temp_s5->unk1C[temp_s3->unkC8].unk38);
                    SetTransMatrix(&temp_s5->unk1C[temp_s3->unkC8].unk38);

                    setVector(&s4->unk2C,
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vx,
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vy,
                        temp_s0->trails[trail]
                            .splineSamples[temp_s0->trails[trail].currentSplineSample]
                            .vz);

                    gte_ldv0(&s4->unk2C);
                    gte_rtv0tr2();
                    gte_stlvnl(&s4->unk34);

                    setVector(&sp18.unkC, s4->unk34.vx * ONE, s4->unk34.vy * ONE,
                        s4->unk34.vz * ONE);
                } else {
                    setVector(&sp18.unkC,
                        temp_s0->trails[trail]
                                .splineSamples[temp_s0->trails[trail].currentSplineSample]
                                .vx
                            * ONE,
                        temp_s0->trails[trail]
                                .splineSamples[temp_s0->trails[trail].currentSplineSample]
                                .vy
                            * ONE,
                        temp_s0->trails[trail]
                                .splineSamples[temp_s0->trails[trail].currentSplineSample]
                                .vz
                            * ONE);
                }
                func_800D2ADC(temp_s5, temp_s3->unk3 - 1, 0, 0, &sp18);
            }
        }

        ++temp_s0->curveSampleDistance;

        if (temp_s0->unk0 != 0) {
            --temp_s0->unk0;
            if (!temp_s0->unk0) {
                vs_main_freeHeapR(temp_s0->trails);
                s6 = 0;
            }
        }
        break;

    case 3:
        temp_s0->unk0 = 1;
        break;

    case 4:
        vs_main_freeHeapR(temp_s0->trails);
        s6 = 0;
        break;
    }

    return s6;
}

#undef VS_TRAIL_COLLAPSE_INIT
#undef VS_TRAIL_TARGET
#undef VS_TRAIL_ORIENT
#undef VS_TRAIL_INIT
#undef VS_TRAIL_ADVANCE
#undef VS_TRAIL_RENDER
#undef VS_TRAIL_ENTRY
