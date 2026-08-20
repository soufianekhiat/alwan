/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map JzAzBz kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * XYZ -> JzAzBz Interleave
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_jzazbz, _map_interleave)(alwan_map_lane *jzazbz_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count)
{
    if (!xyz_in || !jzazbz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const vB      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_B));
    alwan_map_simd const vBm1    = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_B) - 1.0));
    alwan_map_simd const vG      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_G));
    alwan_map_simd const vGm1    = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_G) - 1.0));
    alwan_map_simd const vD1     = alwan_map_simd_set1((alwan_map_lane)(1.0 + ALWAN_MAP_LIT(JZAZBZ_V_D)));
    alwan_map_simd const vD      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D));
    alwan_map_simd const vD0     = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D0));
    alwan_map_simd const one     = alwan_map_simd_set1((alwan_map_lane)1.0);
    alwan_map_simd const zero    = alwan_map_simd_zero();

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

            /* Chromatic adaptation: xa = B*x - (B-1)*z, ya = G*y - (G-1)*x */
            alwan_map_simd xa = alwan_map_simd_fmsub(vB, vx, alwan_map_simd_mul(vBm1, vz));
            alwan_map_simd ya = alwan_map_simd_fmsub(vG, vy, alwan_map_simd_mul(vGm1, vx));

            /* XYZ(adapted) -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(JZAZBZ_V_XYZ_TO_LMS), xa, ya, vz);

            /* PQ OETF */
            vl = ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(vl);
            vm = ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(vm);
            vs = ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(vs);

            /* LMS' -> IzAzBz */
            alwan_map_simd iz, az, bz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&iz, &az, &bz, &ALWAN_MAP_LIT(JZAZBZ_V_LMS_P_TO_IZAZBZ), vl, vm, vs);

            /* Jz = (1+D)*Iz / (1+D*Iz) - D0, clamped to >= 0 */
            alwan_map_simd jz = alwan_map_simd_sub(
                alwan_map_simd_div(alwan_map_simd_mul(vD1, iz),
                                   alwan_map_simd_add(one, alwan_map_simd_mul(vD, iz))),
                vD0);
            jz = alwan_map_simd_select(alwan_map_simd_cmplt(jz, zero), zero, jz);

            alwan_map_simd_store(&c0[i], jz);
            alwan_map_simd_store(&c1[i], az);
            alwan_map_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_jzazbz) r = ALWAN_MAP_LITV(alwan_xyz_to_jzazbz)(v);
            c0[i] = (alwan_map_lane)r.Jz; c1[i] = (alwan_map_lane)r.az; c2[i] = (alwan_map_lane)r.bz;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(jzazbz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)jzazbz_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_xyz) xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_jzazbz) jz;
        ALWAN_MAP_LIT(alwan_xyz_to_jzazbz)(&jz, &xyz);
        out_ptr[0] = jz.Jz; out_ptr[1] = jz.az; out_ptr[2] = jz.bz;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * JzAzBz -> XYZ Interleave
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_jzazbz_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *jzazbz_in, size_t in_stride, size_t count)
{
    if (!jzazbz_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const vD      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D));
    alwan_map_simd const vD0     = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D0));
    alwan_map_simd const vD1     = alwan_map_simd_set1((alwan_map_lane)(1.0 + ALWAN_MAP_LIT(JZAZBZ_V_D)));
    alwan_map_simd const inv_B   = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(JZAZBZ_V_B)));
    alwan_map_simd const inv_G   = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(JZAZBZ_V_G)));
    alwan_map_simd const Bm1     = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_B) - 1.0));
    alwan_map_simd const Gm1     = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_G) - 1.0));

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, jzazbz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd jz = alwan_map_simd_load(&c0[i]);
            alwan_map_simd az = alwan_map_simd_load(&c1[i]);
            alwan_map_simd bz = alwan_map_simd_load(&c2[i]);

            /* Recover Iz from Jz: Iz = (Jz + D0) / (1 + D - D*(Jz + D0)) */
            alwan_map_simd jz_d0 = alwan_map_simd_add(jz, vD0);
            alwan_map_simd iz = alwan_map_simd_div(jz_d0,
                alwan_map_simd_sub(vD1, alwan_map_simd_mul(vD, jz_d0)));

            /* IzAzBz -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, &ALWAN_MAP_LIT(JZAZBZ_V_IZAZBZ_TO_LMS_P), iz, az, bz);

            /* PQ EOTF */
            alwan_map_simd vl = ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(lp);
            alwan_map_simd vm = ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(mp);
            alwan_map_simd vs = ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(sp);

            /* LMS -> XYZ(adapted) */
            alwan_map_simd xa, ya, za;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&xa, &ya, &za, &ALWAN_MAP_LIT(JZAZBZ_V_LMS_TO_XYZ), vl, vm, vs);

            /* Inverse chromatic adaptation:
             * x = (xa + (B-1)*za) / B
             * y = (ya + (G-1)*x) / G
             * z = za */
            alwan_map_simd vx = alwan_map_simd_mul(alwan_map_simd_fmadd(Bm1, za, xa), inv_B);
            alwan_map_simd vy = alwan_map_simd_mul(alwan_map_simd_fmadd(Gm1, vx, ya), inv_G);

            alwan_map_simd_store(&c0[i], vx);
            alwan_map_simd_store(&c1[i], vy);
            alwan_map_simd_store(&c2[i], za);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_jzazbz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_jzazbz_to_xyz)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)jzazbz_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)xyz_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_jzazbz) jz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_xyz) xyz;
        ALWAN_MAP_LIT(alwan_jzazbz_to_xyz)(&xyz, &jz);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * JzAzBz -> JzCzHz Interleave
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_jzazbz_to_jzczhz, _map_interleave)(alwan_map_lane *jzczhz_out, size_t out_stride, alwan_map_lane const *jzazbz_in, size_t in_stride, size_t count)
{
    if (!jzazbz_in || !jzczhz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, jzazbz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd jz = alwan_map_simd_load(&c0[i]);
            alwan_map_simd az = alwan_map_simd_load(&c1[i]);
            alwan_map_simd bz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd Cz = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(az, az, alwan_map_simd_mul(bz, bz)));
            alwan_map_simd hz = alwan_map_simd_atan2(bz, az);
            alwan_map_simd_store(&c0[i], jz);
            alwan_map_simd_store(&c1[i], Cz);
            alwan_map_simd_store(&c2[i], hz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_jzazbz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_jzczhz) r = ALWAN_MAP_LITV(alwan_jzazbz_to_jzczhz)(v);
            c0[i] = (alwan_map_lane)r.Jz; c1[i] = (alwan_map_lane)r.Cz; c2[i] = (alwan_map_lane)r.hz;
        }

        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 0.15915494309189533577, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(jzczhz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)jzazbz_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)jzczhz_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_jzazbz) jz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_jzczhz) jzch;
        ALWAN_MAP_LIT(alwan_jzazbz_to_jzczhz)(&jzch, &jz);
        out_ptr[0] = jzch.Jz; out_ptr[1] = jzch.Cz; out_ptr[2] = jzch.hz;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * JzCzHz -> JzAzBz Interleave
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_jzczhz_to_jzazbz, _map_interleave)(alwan_map_lane *jzazbz_out, size_t out_stride, alwan_map_lane const *jzczhz_in, size_t in_stride, size_t count)
{
    if (!jzczhz_in || !jzazbz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, jzczhz_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 6.28318530717958647692, -3.14159265358979323846);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd jz = alwan_map_simd_load(&c0[i]);
            alwan_map_simd Cz = alwan_map_simd_load(&c1[i]);
            alwan_map_simd hz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd az = alwan_map_simd_mul(Cz, alwan_map_simd_cos(hz));
            alwan_map_simd bz = alwan_map_simd_mul(Cz, alwan_map_simd_sin(hz));
            alwan_map_simd_store(&c0[i], jz);
            alwan_map_simd_store(&c1[i], az);
            alwan_map_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_jzczhz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_jzazbz) r = ALWAN_MAP_LITV(alwan_jzczhz_to_jzazbz)(v);
            c0[i] = (alwan_map_lane)r.Jz; c1[i] = (alwan_map_lane)r.az; c2[i] = (alwan_map_lane)r.bz;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(jzazbz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)jzczhz_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)jzazbz_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_jzczhz) jzch = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_jzazbz) jz;
        ALWAN_MAP_LIT(alwan_jzczhz_to_jzazbz)(&jz, &jzch);
        out_ptr[0] = jz.Jz; out_ptr[1] = jz.az; out_ptr[2] = jz.bz;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_jzazbz, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const vB      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_B));
    alwan_map_simd const vBm1    = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_B) - 1.0));
    alwan_map_simd const vG      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_G));
    alwan_map_simd const vGm1    = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_G) - 1.0));
    alwan_map_simd const vD1     = alwan_map_simd_set1((alwan_map_lane)(1.0 + ALWAN_MAP_LIT(JZAZBZ_V_D)));
    alwan_map_simd const vD      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D));
    alwan_map_simd const vD0     = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D0));
    alwan_map_simd const one     = alwan_map_simd_set1((alwan_map_lane)1.0);
    alwan_map_simd const zero    = alwan_map_simd_zero();

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

            /* Chromatic adaptation: xa = B*x - (B-1)*z, ya = G*y - (G-1)*x */
            alwan_map_simd xa = alwan_map_simd_fmsub(vB, vx, alwan_map_simd_mul(vBm1, vz));
            alwan_map_simd ya = alwan_map_simd_fmsub(vG, vy, alwan_map_simd_mul(vGm1, vx));

            /* XYZ(adapted) -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(JZAZBZ_V_XYZ_TO_LMS), xa, ya, vz);

            /* PQ OETF */
            vl = ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(vl);
            vm = ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(vm);
            vs = ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(vs);

            /* LMS' -> IzAzBz */
            alwan_map_simd iz, az, bz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&iz, &az, &bz, &ALWAN_MAP_LIT(JZAZBZ_V_LMS_P_TO_IZAZBZ), vl, vm, vs);

            /* Jz = (1+D)*Iz / (1+D*Iz) - D0, clamped to >= 0 */
            alwan_map_simd jz = alwan_map_simd_sub(
                alwan_map_simd_div(alwan_map_simd_mul(vD1, iz),
                                   alwan_map_simd_add(one, alwan_map_simd_mul(vD, iz))),
                vD0);
            jz = alwan_map_simd_select(alwan_map_simd_cmplt(jz, zero), zero, jz);

            alwan_map_simd_store(&c0[i], jz);
            alwan_map_simd_store(&c1[i], az);
            alwan_map_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_jzazbz) r = ALWAN_MAP_LITV(alwan_xyz_to_jzazbz)(v);
            c0[i] = (alwan_map_lane)r.Jz; c1[i] = (alwan_map_lane)r.az; c2[i] = (alwan_map_lane)r.bz;
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
        ALWAN_MAP_LIT(alwan_jzazbz) jz;
        ALWAN_MAP_LIT(alwan_xyz_to_jzazbz)(&jz, &xyz);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = jz.Jz;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = jz.az;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = jz.bz;
    }
#endif
    return ALWAN_OK;
}

alwan_status ALWAN_MAP_LITCAT(alwan_jzazbz_to_xyz, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd const vD      = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D));
    alwan_map_simd const vD0     = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_LIT(JZAZBZ_V_D0));
    alwan_map_simd const vD1     = alwan_map_simd_set1((alwan_map_lane)(1.0 + ALWAN_MAP_LIT(JZAZBZ_V_D)));
    alwan_map_simd const inv_B   = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(JZAZBZ_V_B)));
    alwan_map_simd const inv_G   = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(JZAZBZ_V_G)));
    alwan_map_simd const Bm1     = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_B) - 1.0));
    alwan_map_simd const Gm1     = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(JZAZBZ_V_G) - 1.0));

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd jz = alwan_map_simd_load(&c0[i]);
            alwan_map_simd az = alwan_map_simd_load(&c1[i]);
            alwan_map_simd bz = alwan_map_simd_load(&c2[i]);

            /* Recover Iz from Jz: Iz = (Jz + D0) / (1 + D - D*(Jz + D0)) */
            alwan_map_simd jz_d0 = alwan_map_simd_add(jz, vD0);
            alwan_map_simd iz = alwan_map_simd_div(jz_d0,
                alwan_map_simd_sub(vD1, alwan_map_simd_mul(vD, jz_d0)));

            /* IzAzBz -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, &ALWAN_MAP_LIT(JZAZBZ_V_IZAZBZ_TO_LMS_P), iz, az, bz);

            /* PQ EOTF */
            alwan_map_simd vl = ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(lp);
            alwan_map_simd vm = ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(mp);
            alwan_map_simd vs = ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(sp);

            /* LMS -> XYZ(adapted) */
            alwan_map_simd xa, ya, za;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&xa, &ya, &za, &ALWAN_MAP_LIT(JZAZBZ_V_LMS_TO_XYZ), vl, vm, vs);

            /* Inverse chromatic adaptation:
             * x = (xa + (B-1)*za) / B
             * y = (ya + (G-1)*x) / G
             * z = za */
            alwan_map_simd vx = alwan_map_simd_mul(alwan_map_simd_fmadd(Bm1, za, xa), inv_B);
            alwan_map_simd vy = alwan_map_simd_mul(alwan_map_simd_fmadd(Gm1, vx, ya), inv_G);

            alwan_map_simd_store(&c0[i], vx);
            alwan_map_simd_store(&c1[i], vy);
            alwan_map_simd_store(&c2[i], za);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_jzazbz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_jzazbz_to_xyz)(v);
            c0[i] = (alwan_map_lane)r.x; c1[i] = (alwan_map_lane)r.y; c2[i] = (alwan_map_lane)r.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_jzazbz) jz = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_xyz) xyz;
        ALWAN_MAP_LIT(alwan_jzazbz_to_xyz)(&xyz, &jz);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}

alwan_status ALWAN_MAP_LITCAT(alwan_jzazbz_to_jzczhz, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd jz = alwan_map_simd_load(&c0[i]);
            alwan_map_simd az = alwan_map_simd_load(&c1[i]);
            alwan_map_simd bz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd Cz = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(az, az, alwan_map_simd_mul(bz, bz)));
            alwan_map_simd hz = alwan_map_simd_atan2(bz, az);
            alwan_map_simd_store(&c0[i], jz);
            alwan_map_simd_store(&c1[i], Cz);
            alwan_map_simd_store(&c2[i], hz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_jzazbz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_jzczhz) r = ALWAN_MAP_LITV(alwan_jzazbz_to_jzczhz)(v);
            c0[i] = (alwan_map_lane)r.Jz; c1[i] = (alwan_map_lane)r.Cz; c2[i] = (alwan_map_lane)r.hz;
        }

        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 0.15915494309189533577, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_jzazbz) jz = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_jzczhz) jzch;
        ALWAN_MAP_LIT(alwan_jzazbz_to_jzczhz)(&jzch, &jz);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = jzch.Jz;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = jzch.Cz;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = jzch.hz;
    }
#endif
    return ALWAN_OK;
}

alwan_status ALWAN_MAP_LITCAT(alwan_jzczhz_to_jzazbz, _map_planar)(alwan_map_lane *out_ch0, size_t out_stride, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2, alwan_map_lane const *in_ch0, size_t in_stride, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2, size_t count)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 6.28318530717958647692, -3.14159265358979323846);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd jz = alwan_map_simd_load(&c0[i]);
            alwan_map_simd Cz = alwan_map_simd_load(&c1[i]);
            alwan_map_simd hz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd az = alwan_map_simd_mul(Cz, alwan_map_simd_cos(hz));
            alwan_map_simd bz = alwan_map_simd_mul(Cz, alwan_map_simd_sin(hz));
            alwan_map_simd_store(&c0[i], jz);
            alwan_map_simd_store(&c1[i], az);
            alwan_map_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_jzczhz) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_jzazbz) r = ALWAN_MAP_LITV(alwan_jzczhz_to_jzazbz)(v);
            c0[i] = (alwan_map_lane)r.Jz; c1[i] = (alwan_map_lane)r.az; c2[i] = (alwan_map_lane)r.bz;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_jzczhz) jzch = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_jzazbz) jz;
        ALWAN_MAP_LIT(alwan_jzczhz_to_jzazbz)(&jz, &jzch);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = jz.Jz;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = jz.az;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = jz.bz;
    }
#endif
    return ALWAN_OK;
}
