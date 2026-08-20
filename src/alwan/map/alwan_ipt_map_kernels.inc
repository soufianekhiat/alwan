/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map IPT kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * XYZ -> IPT Interleave
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_ipt, _map_interleave)(alwan_map_lane *ipt_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count)
{
    if (!xyz_in || !ipt_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const expo = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(IPT_V_EXPONENT));
    alwan_map_simd const zero = alwan_map_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);

            /* XYZ -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(IPT_V_XYZ_TO_LMS), vx, vy, vz);

            /* Sign-preserving power: sign(x) * |x|^0.43 */
            alwan_map_simd_mask ml = alwan_map_simd_cmpge(vl, zero);
            alwan_map_simd_mask mm = alwan_map_simd_cmpge(vm, zero);
            alwan_map_simd_mask ms = alwan_map_simd_cmpge(vs, zero);
            alwan_map_simd lp = alwan_map_simd_pow(alwan_map_simd_abs(vl), expo);
            alwan_map_simd mp = alwan_map_simd_pow(alwan_map_simd_abs(vm), expo);
            alwan_map_simd sp = alwan_map_simd_pow(alwan_map_simd_abs(vs), expo);
            lp = alwan_map_simd_select(ml, lp, alwan_map_simd_neg(lp));
            mp = alwan_map_simd_select(mm, mp, alwan_map_simd_neg(mp));
            sp = alwan_map_simd_select(ms, sp, alwan_map_simd_neg(sp));

            /* LMS' -> IPT */
            alwan_map_simd oI, oP, oT;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oI, &oP, &oT, &ALWAN_MAP_LIT(IPT_V_LMS_P_TO_IPT), lp, mp, sp);

            alwan_map_simd_store(&c0[i], oI);
            alwan_map_simd_store(&c1[i], oP);
            alwan_map_simd_store(&c2[i], oT);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_ipt) r = ALWAN_MAP_LITV(alwan_xyz_to_ipt)(v);
            c0[i] = (alwan_map_lane)r.I; c1[i] = (alwan_map_lane)r.P; c2[i] = (alwan_map_lane)r.T;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(ipt_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)ipt_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_xyz) xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_ipt) ipt;
        ipt = ALWAN_MAP_LITV(alwan_xyz_to_ipt)(xyz);
        out_ptr[0] = ipt.I; out_ptr[1] = ipt.P; out_ptr[2] = ipt.T;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * IPT -> XYZ Interleave
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_ipt_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *ipt_in, size_t in_stride, size_t count)
{
    if (!ipt_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const inv_expo = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(IPT_V_EXPONENT)));
    alwan_map_simd const zero = alwan_map_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, ipt_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vI = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vP = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vT = alwan_map_simd_load(&c2[i]);

            /* IPT -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, &ALWAN_MAP_LIT(IPT_V_IPT_TO_LMS_P), vI, vP, vT);

            /* Inverse sign-preserving power: sign(x) * |x|^(1/0.43) */
            alwan_map_simd_mask ml = alwan_map_simd_cmpge(lp, zero);
            alwan_map_simd_mask mm = alwan_map_simd_cmpge(mp, zero);
            alwan_map_simd_mask ms = alwan_map_simd_cmpge(sp, zero);
            alwan_map_simd vl = alwan_map_simd_pow(alwan_map_simd_abs(lp), inv_expo);
            alwan_map_simd vm = alwan_map_simd_pow(alwan_map_simd_abs(mp), inv_expo);
            alwan_map_simd vs = alwan_map_simd_pow(alwan_map_simd_abs(sp), inv_expo);
            vl = alwan_map_simd_select(ml, vl, alwan_map_simd_neg(vl));
            vm = alwan_map_simd_select(mm, vm, alwan_map_simd_neg(vm));
            vs = alwan_map_simd_select(ms, vs, alwan_map_simd_neg(vs));

            /* LMS -> XYZ */
            alwan_map_simd ox, oy, oz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(IPT_V_LMS_TO_XYZ), vl, vm, vs);

            alwan_map_simd_store(&c0[i], ox);
            alwan_map_simd_store(&c1[i], oy);
            alwan_map_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_ipt) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_ipt_to_xyz)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)ipt_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)xyz_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_ipt) ipt = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_xyz) xyz;
        xyz = ALWAN_MAP_LITV(alwan_ipt_to_xyz)(ipt);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_ipt, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const expo = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(IPT_V_EXPONENT));
    alwan_map_simd const zero = alwan_map_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);

            /* XYZ -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(IPT_V_XYZ_TO_LMS), vx, vy, vz);

            /* Sign-preserving power: sign(x) * |x|^0.43 */
            alwan_map_simd_mask ml = alwan_map_simd_cmpge(vl, zero);
            alwan_map_simd_mask mm = alwan_map_simd_cmpge(vm, zero);
            alwan_map_simd_mask ms = alwan_map_simd_cmpge(vs, zero);
            alwan_map_simd lp = alwan_map_simd_pow(alwan_map_simd_abs(vl), expo);
            alwan_map_simd mp = alwan_map_simd_pow(alwan_map_simd_abs(vm), expo);
            alwan_map_simd sp = alwan_map_simd_pow(alwan_map_simd_abs(vs), expo);
            lp = alwan_map_simd_select(ml, lp, alwan_map_simd_neg(lp));
            mp = alwan_map_simd_select(mm, mp, alwan_map_simd_neg(mp));
            sp = alwan_map_simd_select(ms, sp, alwan_map_simd_neg(sp));

            /* LMS' -> IPT */
            alwan_map_simd oI, oP, oT;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oI, &oP, &oT, &ALWAN_MAP_LIT(IPT_V_LMS_P_TO_IPT), lp, mp, sp);

            alwan_map_simd_store(&c0[i], oI);
            alwan_map_simd_store(&c1[i], oP);
            alwan_map_simd_store(&c2[i], oT);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_ipt) r = ALWAN_MAP_LITV(alwan_xyz_to_ipt)(v);
            c0[i] = (alwan_map_lane)r.I; c1[i] = (alwan_map_lane)r.P; c2[i] = (alwan_map_lane)r.T;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_ipt) ipt;
        ipt = ALWAN_MAP_LITV(alwan_xyz_to_ipt)(xyz);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = ipt.I;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = ipt.P;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = ipt.T;
    }
#endif
    return ALWAN_OK;
}

alwan_status ALWAN_MAP_LITCAT(alwan_ipt_to_xyz, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const inv_expo = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(IPT_V_EXPONENT)));
    alwan_map_simd const zero = alwan_map_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vI = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vP = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vT = alwan_map_simd_load(&c2[i]);

            /* IPT -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, &ALWAN_MAP_LIT(IPT_V_IPT_TO_LMS_P), vI, vP, vT);

            /* Inverse sign-preserving power: sign(x) * |x|^(1/0.43) */
            alwan_map_simd_mask ml = alwan_map_simd_cmpge(lp, zero);
            alwan_map_simd_mask mm = alwan_map_simd_cmpge(mp, zero);
            alwan_map_simd_mask ms = alwan_map_simd_cmpge(sp, zero);
            alwan_map_simd vl = alwan_map_simd_pow(alwan_map_simd_abs(lp), inv_expo);
            alwan_map_simd vm = alwan_map_simd_pow(alwan_map_simd_abs(mp), inv_expo);
            alwan_map_simd vs = alwan_map_simd_pow(alwan_map_simd_abs(sp), inv_expo);
            vl = alwan_map_simd_select(ml, vl, alwan_map_simd_neg(vl));
            vm = alwan_map_simd_select(mm, vm, alwan_map_simd_neg(vm));
            vs = alwan_map_simd_select(ms, vs, alwan_map_simd_neg(vs));

            /* LMS -> XYZ */
            alwan_map_simd ox, oy, oz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(IPT_V_LMS_TO_XYZ), vl, vm, vs);

            alwan_map_simd_store(&c0[i], ox);
            alwan_map_simd_store(&c1[i], oy);
            alwan_map_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_ipt) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_ipt_to_xyz)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_ipt) ipt = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_xyz) xyz;
        xyz = ALWAN_MAP_LITV(alwan_ipt_to_xyz)(ipt);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}
