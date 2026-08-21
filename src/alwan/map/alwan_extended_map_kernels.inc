/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Extended Color Space kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * XYZ <-> IgPgTg
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_igpgtg_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                         alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M1 = ALWAN_MAP_LIT(ALWAN_EXT_XYZ_TO_LMS_IGPGTG).m;
        alwan_map_lane const *M2 = ALWAN_MAP_LIT(ALWAN_EXT_LMS_TO_IGPGTG).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)(M1[0])), a01 = alwan_map_simd_set1((alwan_map_lane)(M1[1])), a02 = alwan_map_simd_set1((alwan_map_lane)(M1[2]));
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)(M1[3])), a11 = alwan_map_simd_set1((alwan_map_lane)(M1[4])), a12 = alwan_map_simd_set1((alwan_map_lane)(M1[5]));
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)(M1[6])), a21 = alwan_map_simd_set1((alwan_map_lane)(M1[7])), a22 = alwan_map_simd_set1((alwan_map_lane)(M1[8]));
        alwan_map_simd b00 = alwan_map_simd_set1((alwan_map_lane)(M2[0])), b01 = alwan_map_simd_set1((alwan_map_lane)(M2[1])), b02 = alwan_map_simd_set1((alwan_map_lane)(M2[2]));
        alwan_map_simd b10 = alwan_map_simd_set1((alwan_map_lane)(M2[3])), b11 = alwan_map_simd_set1((alwan_map_lane)(M2[4])), b12 = alwan_map_simd_set1((alwan_map_lane)(M2[5]));
        alwan_map_simd b20 = alwan_map_simd_set1((alwan_map_lane)(M2[6])), b21 = alwan_map_simd_set1((alwan_map_lane)(M2[7])), b22 = alwan_map_simd_set1((alwan_map_lane)(M2[8]));
        alwan_map_simd inv_s0 = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[0]));
        alwan_map_simd inv_s1 = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[1]));
        alwan_map_simd inv_s2 = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[2]));
        alwan_map_simd exp_v = alwan_map_simd_set1((alwan_map_lane)(0.427));
        alwan_map_simd zero = alwan_map_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd L = alwan_map_simd_fmadd(a00, vx, alwan_map_simd_fmadd(a01, vy, alwan_map_simd_mul(a02, vz)));
            alwan_map_simd M = alwan_map_simd_fmadd(a10, vx, alwan_map_simd_fmadd(a11, vy, alwan_map_simd_mul(a12, vz)));
            alwan_map_simd S = alwan_map_simd_fmadd(a20, vx, alwan_map_simd_fmadd(a21, vy, alwan_map_simd_mul(a22, vz)));
            alwan_map_simd Ls = alwan_map_simd_mul(L, inv_s0);
            alwan_map_simd Ms = alwan_map_simd_mul(M, inv_s1);
            alwan_map_simd Ss = alwan_map_simd_mul(S, inv_s2);
            alwan_map_simd absL = alwan_map_simd_abs(Ls), absM = alwan_map_simd_abs(Ms), absS = alwan_map_simd_abs(Ss);
            alwan_map_simd pL = alwan_map_simd_pow(absL, exp_v);
            alwan_map_simd pM = alwan_map_simd_pow(absM, exp_v);
            alwan_map_simd pS = alwan_map_simd_pow(absS, exp_v);
            alwan_map_simd_mask posL = alwan_map_simd_cmpge(Ls, zero);
            alwan_map_simd_mask posM = alwan_map_simd_cmpge(Ms, zero);
            alwan_map_simd_mask posS = alwan_map_simd_cmpge(Ss, zero);
            alwan_map_simd sL = alwan_map_simd_select(posL, pL, alwan_map_simd_neg(pL));
            alwan_map_simd sM = alwan_map_simd_select(posM, pM, alwan_map_simd_neg(pM));
            alwan_map_simd sS = alwan_map_simd_select(posS, pS, alwan_map_simd_neg(pS));
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(b00, sL, alwan_map_simd_fmadd(b01, sM, alwan_map_simd_mul(b02, sS))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(b10, sL, alwan_map_simd_fmadd(b11, sM, alwan_map_simd_mul(b12, sS))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b20, sL, alwan_map_simd_fmadd(b21, sM, alwan_map_simd_mul(b22, sS))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_igpgtg) r = ALWAN_MAP_LITV(alwan_xyz_to_igpgtg)(xyz);
        d0[i] = (alwan_map_lane)r.Ig; d1[i] = (alwan_map_lane)r.Pg; d2[i] = (alwan_map_lane)r.Tg;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_igpgtg, _map_interleave)(alwan_map_lane *igpgtg_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !igpgtg_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_igpgtg_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(igpgtg_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__igpgtg_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                         alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M1 = ALWAN_MAP_LIT(ALWAN_EXT_IGPGTG_TO_LMS).m;
        alwan_map_lane const *M2 = ALWAN_MAP_LIT(ALWAN_EXT_LMS_TO_XYZ_IGPGTG).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)(M1[0])), a01 = alwan_map_simd_set1((alwan_map_lane)(M1[1])), a02 = alwan_map_simd_set1((alwan_map_lane)(M1[2]));
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)(M1[3])), a11 = alwan_map_simd_set1((alwan_map_lane)(M1[4])), a12 = alwan_map_simd_set1((alwan_map_lane)(M1[5]));
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)(M1[6])), a21 = alwan_map_simd_set1((alwan_map_lane)(M1[7])), a22 = alwan_map_simd_set1((alwan_map_lane)(M1[8]));
        alwan_map_simd b00 = alwan_map_simd_set1((alwan_map_lane)(M2[0])), b01 = alwan_map_simd_set1((alwan_map_lane)(M2[1])), b02 = alwan_map_simd_set1((alwan_map_lane)(M2[2]));
        alwan_map_simd b10 = alwan_map_simd_set1((alwan_map_lane)(M2[3])), b11 = alwan_map_simd_set1((alwan_map_lane)(M2[4])), b12 = alwan_map_simd_set1((alwan_map_lane)(M2[5]));
        alwan_map_simd b20 = alwan_map_simd_set1((alwan_map_lane)(M2[6])), b21 = alwan_map_simd_set1((alwan_map_lane)(M2[7])), b22 = alwan_map_simd_set1((alwan_map_lane)(M2[8]));
        alwan_map_simd s0 = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[0]));
        alwan_map_simd s1 = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[1]));
        alwan_map_simd s2 = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[2]));
        alwan_map_simd inv_exp = alwan_map_simd_set1((alwan_map_lane)(1.0 / 0.427));
        alwan_map_simd zero = alwan_map_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_map_simd vIg = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vPg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vTg = alwan_map_simd_load(&c2[i]);
            /* mat3: IgPgTg -> LMS */
            alwan_map_simd L = alwan_map_simd_fmadd(a00, vIg, alwan_map_simd_fmadd(a01, vPg, alwan_map_simd_mul(a02, vTg)));
            alwan_map_simd M = alwan_map_simd_fmadd(a10, vIg, alwan_map_simd_fmadd(a11, vPg, alwan_map_simd_mul(a12, vTg)));
            alwan_map_simd S = alwan_map_simd_fmadd(a20, vIg, alwan_map_simd_fmadd(a21, vPg, alwan_map_simd_mul(a22, vTg)));
            /* spow(LMS, 1/0.427) * scale */
            alwan_map_simd absL = alwan_map_simd_abs(L), absM = alwan_map_simd_abs(M), absS = alwan_map_simd_abs(S);
            alwan_map_simd pL = alwan_map_simd_pow(absL, inv_exp);
            alwan_map_simd pM = alwan_map_simd_pow(absM, inv_exp);
            alwan_map_simd pS = alwan_map_simd_pow(absS, inv_exp);
            alwan_map_simd_mask posL = alwan_map_simd_cmpge(L, zero);
            alwan_map_simd_mask posM = alwan_map_simd_cmpge(M, zero);
            alwan_map_simd_mask posS = alwan_map_simd_cmpge(S, zero);
            alwan_map_simd sL = alwan_map_simd_mul(s0, alwan_map_simd_select(posL, pL, alwan_map_simd_neg(pL)));
            alwan_map_simd sM = alwan_map_simd_mul(s1, alwan_map_simd_select(posM, pM, alwan_map_simd_neg(pM)));
            alwan_map_simd sS = alwan_map_simd_mul(s2, alwan_map_simd_select(posS, pS, alwan_map_simd_neg(pS)));
            /* mat3: LMS -> XYZ */
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(b00, sL, alwan_map_simd_fmadd(b01, sM, alwan_map_simd_mul(b02, sS))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(b10, sL, alwan_map_simd_fmadd(b11, sM, alwan_map_simd_mul(b12, sS))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b20, sL, alwan_map_simd_fmadd(b21, sM, alwan_map_simd_mul(b22, sS))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_igpgtg) igpgtg = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_igpgtg_to_xyz)(igpgtg);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_igpgtg_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *igpgtg_in, size_t in_stride, size_t count) {
    if (!igpgtg_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, igpgtg_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__igpgtg_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> ICaCb
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_icacb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M1 = ALWAN_MAP_LIT(ALWAN_EXT_XYZ_TO_LMS_ICACB).m;
        alwan_map_lane const *M2 = ALWAN_MAP_LIT(ALWAN_EXT_LMS_TO_ICACB).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)(M1[0])), a01 = alwan_map_simd_set1((alwan_map_lane)(M1[1])), a02 = alwan_map_simd_set1((alwan_map_lane)(M1[2]));
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)(M1[3])), a11 = alwan_map_simd_set1((alwan_map_lane)(M1[4])), a12 = alwan_map_simd_set1((alwan_map_lane)(M1[5]));
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)(M1[6])), a21 = alwan_map_simd_set1((alwan_map_lane)(M1[7])), a22 = alwan_map_simd_set1((alwan_map_lane)(M1[8]));
        alwan_map_simd b00 = alwan_map_simd_set1((alwan_map_lane)(M2[0])), b01 = alwan_map_simd_set1((alwan_map_lane)(M2[1])), b02 = alwan_map_simd_set1((alwan_map_lane)(M2[2]));
        alwan_map_simd b10 = alwan_map_simd_set1((alwan_map_lane)(M2[3])), b11 = alwan_map_simd_set1((alwan_map_lane)(M2[4])), b12 = alwan_map_simd_set1((alwan_map_lane)(M2[5]));
        alwan_map_simd b20 = alwan_map_simd_set1((alwan_map_lane)(M2[6])), b21 = alwan_map_simd_set1((alwan_map_lane)(M2[7])), b22 = alwan_map_simd_set1((alwan_map_lane)(M2[8]));
        /* PQ constants */
        alwan_map_simd inv_Lp = alwan_map_simd_set1((alwan_map_lane)(1.0 / 10000.0));
        alwan_map_simd pq_m1 = alwan_map_simd_set1((alwan_map_lane)(0.1593017578125));
        alwan_map_simd pq_m2 = alwan_map_simd_set1((alwan_map_lane)(78.84375));
        alwan_map_simd pq_c1 = alwan_map_simd_set1((alwan_map_lane)(0.8359375));
        alwan_map_simd pq_c2 = alwan_map_simd_set1((alwan_map_lane)(18.8515625));
        alwan_map_simd pq_c3 = alwan_map_simd_set1((alwan_map_lane)(18.6875));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            /* mat3: XYZ -> LMS */
            alwan_map_simd L = alwan_map_simd_fmadd(a00, vx, alwan_map_simd_fmadd(a01, vy, alwan_map_simd_mul(a02, vz)));
            alwan_map_simd M = alwan_map_simd_fmadd(a10, vx, alwan_map_simd_fmadd(a11, vy, alwan_map_simd_mul(a12, vz)));
            alwan_map_simd S = alwan_map_simd_fmadd(a20, vx, alwan_map_simd_fmadd(a21, vy, alwan_map_simd_mul(a22, vz)));
            /* PQ inverse EOTF per channel: N = pow((c1 + c2*Y_p)/(c3*Y_p + 1), m2) where Y_p = pow(C/Lp, m1) */
            #define PQ_INV(ch) { \
                alwan_map_simd Yp = alwan_map_simd_pow(alwan_map_simd_mul(ch, inv_Lp), pq_m1); \
                alwan_map_simd num = alwan_map_simd_fmadd(pq_c2, Yp, pq_c1); \
                alwan_map_simd den = alwan_map_simd_fmadd(pq_c3, Yp, one); \
                ch = alwan_map_simd_pow(alwan_map_simd_div(num, den), pq_m2); \
            }
            PQ_INV(L) PQ_INV(M) PQ_INV(S)
            #undef PQ_INV
            /* mat3: LMS -> ICaCb */
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(b00, L, alwan_map_simd_fmadd(b01, M, alwan_map_simd_mul(b02, S))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(b10, L, alwan_map_simd_fmadd(b11, M, alwan_map_simd_mul(b12, S))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b20, L, alwan_map_simd_fmadd(b21, M, alwan_map_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_icacb) r = ALWAN_MAP_LITV(alwan_xyz_to_icacb)(xyz);
        d0[i] = (alwan_map_lane)r.I; d1[i] = (alwan_map_lane)r.Ca; d2[i] = (alwan_map_lane)r.Cb;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_icacb, _map_interleave)(alwan_map_lane *icacb_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !icacb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_icacb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(icacb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__icacb_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M1 = ALWAN_MAP_LIT(ALWAN_EXT_ICACB_TO_LMS).m;
        alwan_map_lane const *M2 = ALWAN_MAP_LIT(ALWAN_EXT_LMS_TO_XYZ_ICACB).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)(M1[0])), a01 = alwan_map_simd_set1((alwan_map_lane)(M1[1])), a02 = alwan_map_simd_set1((alwan_map_lane)(M1[2]));
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)(M1[3])), a11 = alwan_map_simd_set1((alwan_map_lane)(M1[4])), a12 = alwan_map_simd_set1((alwan_map_lane)(M1[5]));
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)(M1[6])), a21 = alwan_map_simd_set1((alwan_map_lane)(M1[7])), a22 = alwan_map_simd_set1((alwan_map_lane)(M1[8]));
        alwan_map_simd b00 = alwan_map_simd_set1((alwan_map_lane)(M2[0])), b01 = alwan_map_simd_set1((alwan_map_lane)(M2[1])), b02 = alwan_map_simd_set1((alwan_map_lane)(M2[2]));
        alwan_map_simd b10 = alwan_map_simd_set1((alwan_map_lane)(M2[3])), b11 = alwan_map_simd_set1((alwan_map_lane)(M2[4])), b12 = alwan_map_simd_set1((alwan_map_lane)(M2[5]));
        alwan_map_simd b20 = alwan_map_simd_set1((alwan_map_lane)(M2[6])), b21 = alwan_map_simd_set1((alwan_map_lane)(M2[7])), b22 = alwan_map_simd_set1((alwan_map_lane)(M2[8]));
        /* PQ EOTF constants */
        alwan_map_simd pq_Lp = alwan_map_simd_set1((alwan_map_lane)(10000.0));
        alwan_map_simd pq_c1 = alwan_map_simd_set1((alwan_map_lane)(0.8359375));
        alwan_map_simd pq_c2 = alwan_map_simd_set1((alwan_map_lane)(18.8515625));
        alwan_map_simd pq_c3 = alwan_map_simd_set1((alwan_map_lane)(18.6875));
        alwan_map_simd pq_inv_m2 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 78.84375));
        alwan_map_simd pq_inv_m1 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 0.1593017578125));
        alwan_map_simd zero = alwan_map_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_map_simd vI = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vCa = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vCb = alwan_map_simd_load(&c2[i]);
            /* mat3: ICaCb -> LMS */
            alwan_map_simd L = alwan_map_simd_fmadd(a00, vI, alwan_map_simd_fmadd(a01, vCa, alwan_map_simd_mul(a02, vCb)));
            alwan_map_simd M = alwan_map_simd_fmadd(a10, vI, alwan_map_simd_fmadd(a11, vCa, alwan_map_simd_mul(a12, vCb)));
            alwan_map_simd S = alwan_map_simd_fmadd(a20, vI, alwan_map_simd_fmadd(a21, vCa, alwan_map_simd_mul(a22, vCb)));
            /* PQ EOTF per channel: C = Lp * pow(max(0, (N^(1/m2) - c1)) / (c2 - c3*N^(1/m2)), 1/m1) */
            #define PQ_FWD(ch) { \
                alwan_map_simd Np = alwan_map_simd_pow(ch, pq_inv_m2); \
                alwan_map_simd num = alwan_map_simd_sub(Np, pq_c1); \
                alwan_map_simd den = alwan_map_simd_fmsub(pq_c2, alwan_map_simd_set1((alwan_map_lane)(1.0)), alwan_map_simd_mul(pq_c3, Np)); \
                num = alwan_map_simd_select(alwan_map_simd_cmplt(num, zero), zero, num); \
                alwan_map_simd Yp = alwan_map_simd_select(alwan_map_simd_cmple(den, zero), zero, alwan_map_simd_div(num, den)); \
                ch = alwan_map_simd_mul(pq_Lp, alwan_map_simd_pow(Yp, pq_inv_m1)); \
            }
            PQ_FWD(L) PQ_FWD(M) PQ_FWD(S)
            #undef PQ_FWD
            /* mat3: LMS -> XYZ */
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(b00, L, alwan_map_simd_fmadd(b01, M, alwan_map_simd_mul(b02, S))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(b10, L, alwan_map_simd_fmadd(b11, M, alwan_map_simd_mul(b12, S))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b20, L, alwan_map_simd_fmadd(b21, M, alwan_map_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_icacb) icacb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_icacb_to_xyz)(icacb);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_icacb_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *icacb_in, size_t in_stride, size_t count) {
    if (!icacb_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, icacb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__icacb_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> hdr-CIELAB
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_hdr_cielab_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                             alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    /* Precompute epsilon and Km */
    alwan_map_lane sf = (alwan_map_lane)(1.25 - 0.25 * (0.2 / 0.184));
    alwan_map_lane lf = (alwan_map_lane)(alwan_map_scalar_log((alwan_map_lane)318.0) / alwan_map_scalar_log((alwan_map_lane)100.0));
    alwan_map_lane epsilon_s = (alwan_map_lane)(0.58 / (sf * lf));
    alwan_map_lane Km_s = (alwan_map_lane)(alwan_map_scalar_pow((alwan_map_lane)2.0, epsilon_s));
    /* Used only inside the SIMD path; reference unconditionally so MSVC
     * /WX builds with ALWAN_MAP_SIMD_WIDTH == 1 (Windows ARM) don't
     * fail on C4189. */
    (void)Km_s;

    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd inv_wx = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_EXT_HDR_D65_WHITE)[0]));
        alwan_map_simd inv_wy = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_EXT_HDR_D65_WHITE)[1]));
        alwan_map_simd inv_wz = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_EXT_HDR_D65_WHITE)[2]));
        alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)(epsilon_s));
        alwan_map_simd v_Km = alwan_map_simd_set1((alwan_map_lane)(Km_s));
        alwan_map_simd v_Vmax = alwan_map_simd_set1((alwan_map_lane)(247.0));
        alwan_map_simd v_off = alwan_map_simd_set1((alwan_map_lane)(0.02));
        alwan_map_simd k5 = alwan_map_simd_set1((alwan_map_lane)(5.0));
        alwan_map_simd k2 = alwan_map_simd_set1((alwan_map_lane)(2.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            /* Normalize by D65 white */
            alwan_map_simd xr = alwan_map_simd_mul(vx, inv_wx);
            alwan_map_simd yr = alwan_map_simd_mul(vy, inv_wy);
            alwan_map_simd zr = alwan_map_simd_mul(vz, inv_wz);
            /* Fairchild lightness: f(x) = Vmax * x^eps / (Km + x^eps) + 0.02 */
            alwan_map_simd xr_e = alwan_map_simd_pow(xr, v_eps);
            alwan_map_simd yr_e = alwan_map_simd_pow(yr, v_eps);
            alwan_map_simd zr_e = alwan_map_simd_pow(zr, v_eps);
            alwan_map_simd fx = alwan_map_simd_fmadd(v_Vmax, alwan_map_simd_div(xr_e, alwan_map_simd_add(v_Km, xr_e)), v_off);
            alwan_map_simd fy = alwan_map_simd_fmadd(v_Vmax, alwan_map_simd_div(yr_e, alwan_map_simd_add(v_Km, yr_e)), v_off);
            alwan_map_simd fz = alwan_map_simd_fmadd(v_Vmax, alwan_map_simd_div(zr_e, alwan_map_simd_add(v_Km, zr_e)), v_off);
            /* L = fy, a = 5*(fx-fy), b = 2*(fy-fz) */
            alwan_map_simd_store(&d0[i], fy);
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(k5, alwan_map_simd_sub(fx, fy)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_mul(k2, alwan_map_simd_sub(fy, fz)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_lab) r = ALWAN_MAP_LITV(alwan_xyz_to_hdr_cielab)(xyz);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.a; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_hdr_cielab, _map_interleave)(alwan_map_lane *hdr_lab_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !hdr_lab_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_hdr_cielab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hdr_lab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hdr_cielab_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                             alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    /* Precompute epsilon and inverse */
    alwan_map_lane sf = (alwan_map_lane)(1.25 - 0.25 * (0.2 / 0.184));
    alwan_map_lane lf = (alwan_map_lane)(alwan_map_scalar_log((alwan_map_lane)318.0) / alwan_map_scalar_log((alwan_map_lane)100.0));
    alwan_map_lane epsilon_s = (alwan_map_lane)(0.58 / (sf * lf));
    alwan_map_lane Km_s = (alwan_map_lane)(alwan_map_scalar_pow((alwan_map_lane)2.0, epsilon_s));
    alwan_map_lane inv_eps = (alwan_map_lane)(1.0 / epsilon_s);
    /* SIMD-only constants -- silence MSVC C4189 on Windows ARM. */
    (void)Km_s; (void)inv_eps;

    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd wx = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_EXT_HDR_D65_WHITE)[0]));
        alwan_map_simd wy = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_EXT_HDR_D65_WHITE)[1]));
        alwan_map_simd wz = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_EXT_HDR_D65_WHITE)[2]));
        alwan_map_simd v_Km = alwan_map_simd_set1((alwan_map_lane)(Km_s));
        alwan_map_simd v_Vmax = alwan_map_simd_set1((alwan_map_lane)(247.0));
        alwan_map_simd v_off = alwan_map_simd_set1((alwan_map_lane)(0.02));
        alwan_map_simd v_inv_eps = alwan_map_simd_set1((alwan_map_lane)(inv_eps));
        alwan_map_simd k5 = alwan_map_simd_set1((alwan_map_lane)(5.0));
        alwan_map_simd k2 = alwan_map_simd_set1((alwan_map_lane)(2.0));
        alwan_map_simd eps_guard = alwan_map_simd_set1((alwan_map_lane)(ALWAN_EPSILON));
        for (; i + W <= n; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            /* Recover fx, fz from L,a,b: fx = (a + 5L)/5, fz = (-b + 2L)/2 */
            alwan_map_simd fy = vL;
            alwan_map_simd fx = alwan_map_simd_div(alwan_map_simd_fmadd(k5, vL, va), k5);
            alwan_map_simd fz = alwan_map_simd_div(alwan_map_simd_sub(alwan_map_simd_mul(k2, vL), vb), k2);
            /* Inverse Fairchild: v = f-0.02; S = (v*Km)/(Vmax-v); result = spow(S, 1/eps) */
            #define INV_FAIR(f, w) { \
                alwan_map_simd v_val = alwan_map_simd_sub(f, v_off); \
                alwan_map_simd denom = alwan_map_simd_sub(v_Vmax, v_val); \
                alwan_map_simd abs_denom = alwan_map_simd_abs(denom); \
                alwan_map_simd_mask denom_tiny = alwan_map_simd_cmplt(abs_denom, eps_guard); \
                alwan_map_simd S = alwan_map_simd_select(denom_tiny, v_Km, alwan_map_simd_div(alwan_map_simd_mul(v_val, v_Km), denom)); \
                alwan_map_simd abs_S = alwan_map_simd_abs(S); \
                alwan_map_simd_mask pos = alwan_map_simd_cmpge(S, alwan_map_simd_zero()); \
                alwan_map_simd pw = alwan_map_simd_pow(abs_S, v_inv_eps); \
                f = alwan_map_simd_mul(alwan_map_simd_select(pos, pw, alwan_map_simd_neg(pw)), w); \
            }
            INV_FAIR(fx, wx) INV_FAIR(fy, wy) INV_FAIR(fz, wz)
            #undef INV_FAIR
            alwan_map_simd_store(&d0[i], fx);
            alwan_map_simd_store(&d1[i], fy);
            alwan_map_simd_store(&d2[i], fz);
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_lab) hdr_lab = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_hdr_cielab_to_xyz)(hdr_lab);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_hdr_cielab_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *hdr_lab_in, size_t in_stride, size_t count) {
    if (!hdr_lab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hdr_lab_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hdr_cielab_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> hdr-IPT
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_hdr_ipt_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                          alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    /* Precompute epsilon and Km */
    alwan_map_lane lf = (alwan_map_lane)(alwan_map_scalar_log((alwan_map_lane)318.0) / alwan_map_scalar_log((alwan_map_lane)100.0));
    alwan_map_lane sf = (alwan_map_lane)(1.25 - 0.25 * (0.2 / 0.184));
    alwan_map_lane epsilon_s = (alwan_map_lane)(0.59 / (sf * lf));
    alwan_map_lane Km_s = (alwan_map_lane)(alwan_map_scalar_pow((alwan_map_lane)2.0, epsilon_s));
    /* SIMD-only constant -- silence MSVC C4189 on Windows ARM. */
    (void)Km_s;

    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M1 = ALWAN_MAP_LIT(ALWAN_EXT_XYZ_TO_LMS_IPT).m;
        alwan_map_lane const *M2 = ALWAN_MAP_LIT(ALWAN_EXT_LMS_TO_IPT_HDR).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)(M1[0])), a01 = alwan_map_simd_set1((alwan_map_lane)(M1[1])), a02 = alwan_map_simd_set1((alwan_map_lane)(M1[2]));
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)(M1[3])), a11 = alwan_map_simd_set1((alwan_map_lane)(M1[4])), a12 = alwan_map_simd_set1((alwan_map_lane)(M1[5]));
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)(M1[6])), a21 = alwan_map_simd_set1((alwan_map_lane)(M1[7])), a22 = alwan_map_simd_set1((alwan_map_lane)(M1[8]));
        alwan_map_simd b00 = alwan_map_simd_set1((alwan_map_lane)(M2[0])), b01 = alwan_map_simd_set1((alwan_map_lane)(M2[1])), b02 = alwan_map_simd_set1((alwan_map_lane)(M2[2]));
        alwan_map_simd b10 = alwan_map_simd_set1((alwan_map_lane)(M2[3])), b11 = alwan_map_simd_set1((alwan_map_lane)(M2[4])), b12 = alwan_map_simd_set1((alwan_map_lane)(M2[5]));
        alwan_map_simd b20 = alwan_map_simd_set1((alwan_map_lane)(M2[6])), b21 = alwan_map_simd_set1((alwan_map_lane)(M2[7])), b22 = alwan_map_simd_set1((alwan_map_lane)(M2[8]));
        alwan_map_simd v_eps = alwan_map_simd_set1((alwan_map_lane)(epsilon_s));
        alwan_map_simd v_Km = alwan_map_simd_set1((alwan_map_lane)(Km_s));
        alwan_map_simd v_Vmax = alwan_map_simd_set1((alwan_map_lane)(247.0));
        alwan_map_simd v_off = alwan_map_simd_set1((alwan_map_lane)(0.02));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd neg_one = alwan_map_simd_set1((alwan_map_lane)(-1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            /* mat3: XYZ -> LMS */
            alwan_map_simd L = alwan_map_simd_fmadd(a00, vx, alwan_map_simd_fmadd(a01, vy, alwan_map_simd_mul(a02, vz)));
            alwan_map_simd M = alwan_map_simd_fmadd(a10, vx, alwan_map_simd_fmadd(a11, vy, alwan_map_simd_mul(a12, vz)));
            alwan_map_simd S = alwan_map_simd_fmadd(a20, vx, alwan_map_simd_fmadd(a21, vy, alwan_map_simd_mul(a22, vz)));
            /* sign-preserving Fairchild lightness: sign(x) * |lightness(x, eps)| */
            /* sign(x) = x>0 ? 1 : x<0 ? -1 : 0 (three-way, zero at origin) */
            /* lightness(x,e) = Vmax * spow(x,e) / (Km + spow(x,e)) + 0.02 */
            #define SPOW_LIGHT(ch) { \
                alwan_map_simd_mask gt = alwan_map_simd_cmpgt(ch, zero); \
                alwan_map_simd_mask lt = alwan_map_simd_cmplt(ch, zero); \
                alwan_map_simd sign_v = alwan_map_simd_select(gt, one, alwan_map_simd_select(lt, neg_one, zero)); \
                alwan_map_simd abs_ch = alwan_map_simd_abs(ch); \
                alwan_map_simd pow_abs = alwan_map_simd_pow(abs_ch, v_eps); \
                alwan_map_simd Y_p = alwan_map_simd_select(gt, pow_abs, alwan_map_simd_select(lt, alwan_map_simd_neg(pow_abs), zero)); \
                alwan_map_simd light = alwan_map_simd_fmadd(v_Vmax, alwan_map_simd_div(Y_p, alwan_map_simd_add(v_Km, Y_p)), v_off); \
                ch = alwan_map_simd_mul(sign_v, alwan_map_simd_abs(light)); \
            }
            SPOW_LIGHT(L) SPOW_LIGHT(M) SPOW_LIGHT(S)
            #undef SPOW_LIGHT
            /* mat3: LMS -> IPT */
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(b00, L, alwan_map_simd_fmadd(b01, M, alwan_map_simd_mul(b02, S))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(b10, L, alwan_map_simd_fmadd(b11, M, alwan_map_simd_mul(b12, S))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b20, L, alwan_map_simd_fmadd(b21, M, alwan_map_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_ipt) r = ALWAN_MAP_LITV(alwan_xyz_to_hdr_ipt)(xyz);
        d0[i] = (alwan_map_lane)r.I; d1[i] = (alwan_map_lane)r.P; d2[i] = (alwan_map_lane)r.T;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_hdr_ipt, _map_interleave)(alwan_map_lane *hdr_ipt_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !hdr_ipt_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_hdr_ipt_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hdr_ipt_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hdr_ipt_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                          alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2, size_t n) {
    /* Precompute epsilon and inverse */
    alwan_map_lane lf = (alwan_map_lane)(alwan_map_scalar_log((alwan_map_lane)318.0) / alwan_map_scalar_log((alwan_map_lane)100.0));
    alwan_map_lane sf = (alwan_map_lane)(1.25 - 0.25 * (0.2 / 0.184));
    alwan_map_lane epsilon_s = (alwan_map_lane)(0.59 / (sf * lf));
    alwan_map_lane Km_s = (alwan_map_lane)(alwan_map_scalar_pow((alwan_map_lane)2.0, epsilon_s));
    alwan_map_lane inv_eps = (alwan_map_lane)(1.0 / epsilon_s);
    /* SIMD-only constants -- silence MSVC C4189 on Windows ARM. */
    (void)Km_s; (void)inv_eps;

    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M1 = ALWAN_MAP_LIT(ALWAN_EXT_IPT_TO_LMS_HDR).m;
        alwan_map_lane const *M2 = ALWAN_MAP_LIT(ALWAN_EXT_LMS_TO_XYZ_IPT).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)(M1[0])), a01 = alwan_map_simd_set1((alwan_map_lane)(M1[1])), a02 = alwan_map_simd_set1((alwan_map_lane)(M1[2]));
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)(M1[3])), a11 = alwan_map_simd_set1((alwan_map_lane)(M1[4])), a12 = alwan_map_simd_set1((alwan_map_lane)(M1[5]));
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)(M1[6])), a21 = alwan_map_simd_set1((alwan_map_lane)(M1[7])), a22 = alwan_map_simd_set1((alwan_map_lane)(M1[8]));
        alwan_map_simd b00 = alwan_map_simd_set1((alwan_map_lane)(M2[0])), b01 = alwan_map_simd_set1((alwan_map_lane)(M2[1])), b02 = alwan_map_simd_set1((alwan_map_lane)(M2[2]));
        alwan_map_simd b10 = alwan_map_simd_set1((alwan_map_lane)(M2[3])), b11 = alwan_map_simd_set1((alwan_map_lane)(M2[4])), b12 = alwan_map_simd_set1((alwan_map_lane)(M2[5]));
        alwan_map_simd b20 = alwan_map_simd_set1((alwan_map_lane)(M2[6])), b21 = alwan_map_simd_set1((alwan_map_lane)(M2[7])), b22 = alwan_map_simd_set1((alwan_map_lane)(M2[8]));
        alwan_map_simd v_Km = alwan_map_simd_set1((alwan_map_lane)(Km_s));
        alwan_map_simd v_Vmax = alwan_map_simd_set1((alwan_map_lane)(247.0));
        alwan_map_simd v_off = alwan_map_simd_set1((alwan_map_lane)(0.02));
        alwan_map_simd v_inv_eps = alwan_map_simd_set1((alwan_map_lane)(inv_eps));
        alwan_map_simd eps_guard = alwan_map_simd_set1((alwan_map_lane)(ALWAN_EPSILON));
        alwan_map_simd zero = alwan_map_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_map_simd vI = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vP = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vT = alwan_map_simd_load(&c2[i]);
            /* mat3: IPT -> LMS */
            alwan_map_simd L = alwan_map_simd_fmadd(a00, vI, alwan_map_simd_fmadd(a01, vP, alwan_map_simd_mul(a02, vT)));
            alwan_map_simd M = alwan_map_simd_fmadd(a10, vI, alwan_map_simd_fmadd(a11, vP, alwan_map_simd_mul(a12, vT)));
            alwan_map_simd S = alwan_map_simd_fmadd(a20, vI, alwan_map_simd_fmadd(a21, vP, alwan_map_simd_mul(a22, vT)));
            /* sign-preserving inverse Fairchild: sign(ch)*|spow(S,1/e)|
             * where S = (v*Km)/(Vmax-v), v = ch - 0.02 (on the SIGNED value,
             * matching the scalar luminance_fairchild2011_v path). */
            #define SPOW_LUMINANCE(ch) { \
                alwan_map_simd_mask orig_pos = alwan_map_simd_cmpge(ch, zero); \
                alwan_map_simd v_val = alwan_map_simd_sub(ch, v_off); \
                alwan_map_simd denom = alwan_map_simd_sub(v_Vmax, v_val); \
                alwan_map_simd abs_denom = alwan_map_simd_abs(denom); \
                alwan_map_simd_mask denom_tiny = alwan_map_simd_cmplt(abs_denom, eps_guard); \
                alwan_map_simd Sv = alwan_map_simd_select(denom_tiny, v_Km, alwan_map_simd_div(alwan_map_simd_mul(v_val, v_Km), denom)); \
                alwan_map_simd abs_Sv = alwan_map_simd_abs(Sv); \
                alwan_map_simd pw = alwan_map_simd_pow(abs_Sv, v_inv_eps); \
                alwan_map_simd_mask Sv_pos = alwan_map_simd_cmpge(Sv, zero); \
                alwan_map_simd spow_result = alwan_map_simd_select(Sv_pos, pw, alwan_map_simd_neg(pw)); \
                alwan_map_simd abs_result = alwan_map_simd_abs(spow_result); \
                ch = alwan_map_simd_select(orig_pos, abs_result, alwan_map_simd_neg(abs_result)); \
            }
            SPOW_LUMINANCE(L) SPOW_LUMINANCE(M) SPOW_LUMINANCE(S)
            #undef SPOW_LUMINANCE
            /* mat3: LMS -> XYZ */
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(b00, L, alwan_map_simd_fmadd(b01, M, alwan_map_simd_mul(b02, S))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(b10, L, alwan_map_simd_fmadd(b11, M, alwan_map_simd_mul(b12, S))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b20, L, alwan_map_simd_fmadd(b21, M, alwan_map_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_ipt) hdr_ipt = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_hdr_ipt_to_xyz)(hdr_ipt);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_hdr_ipt_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *hdr_ipt_in, size_t in_stride, size_t count) {
    if (!hdr_ipt_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hdr_ipt_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hdr_ipt_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> CIE 1960 UCS
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_ucs_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd k23 = alwan_map_simd_set1((alwan_map_lane)(2.0 / 3.0));
        alwan_map_simd kn05 = alwan_map_simd_set1((alwan_map_lane)(-0.5));
        alwan_map_simd k15 = alwan_map_simd_set1((alwan_map_lane)(1.5));
        alwan_map_simd k05 = alwan_map_simd_set1((alwan_map_lane)(0.5));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            /* U = (2/3)*x, V = y, W = -0.5*x + 1.5*y + 0.5*z */
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(k23, vx));
            alwan_map_simd_store(&d1[i], vy);
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(kn05, vx, alwan_map_simd_fmadd(k15, vy, alwan_map_simd_mul(k05, vz))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_ucs) r = ALWAN_MAP_LITV(alwan_xyz_to_ucs)(xyz);
        d0[i] = (alwan_map_lane)r.U; d1[i] = (alwan_map_lane)r.V; d2[i] = (alwan_map_lane)r.W;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_ucs, _map_interleave)(alwan_map_lane *ucs_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !ucs_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_ucs_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(ucs_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__ucs_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd k15 = alwan_map_simd_set1((alwan_map_lane)(1.5));
        alwan_map_simd k2 = alwan_map_simd_set1((alwan_map_lane)(2.0));
        alwan_map_simd kn3 = alwan_map_simd_set1((alwan_map_lane)(-3.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vU = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vV = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vW = alwan_map_simd_load(&c2[i]);
            /* x = 1.5*U, y = V, z = 2*W + 1.5*U - 3*V */
            alwan_map_simd vx = alwan_map_simd_mul(k15, vU);
            alwan_map_simd_store(&d0[i], vx);
            alwan_map_simd_store(&d1[i], vV);
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(k2, vW, alwan_map_simd_fmadd(kn3, vV, vx)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_ucs) ucs = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_ucs_to_xyz)(ucs);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_ucs_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *ucs_in, size_t in_stride, size_t count) {
    if (!ucs_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, ucs_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__ucs_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> OSA-UCS
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_osa_ucs_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                          alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                          size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *Mo = ALWAN_MAP_LIT(XYZ_TO_RGB_OSA).m;
        alwan_map_simd m00 = alwan_map_simd_set1((alwan_map_lane)(Mo[0])), m01 = alwan_map_simd_set1((alwan_map_lane)(Mo[1])), m02 = alwan_map_simd_set1((alwan_map_lane)(Mo[2]));
        alwan_map_simd m10 = alwan_map_simd_set1((alwan_map_lane)(Mo[3])), m11 = alwan_map_simd_set1((alwan_map_lane)(Mo[4])), m12 = alwan_map_simd_set1((alwan_map_lane)(Mo[5]));
        alwan_map_simd m20 = alwan_map_simd_set1((alwan_map_lane)(Mo[6])), m21 = alwan_map_simd_set1((alwan_map_lane)(Mo[7])), m22 = alwan_map_simd_set1((alwan_map_lane)(Mo[8]));
        alwan_map_simd guard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd third = alwan_map_simd_set1((alwan_map_lane)(1.0 / 3.0));
        alwan_map_simd two_thirds = alwan_map_simd_set1((alwan_map_lane)(2.0 / 3.0));
        alwan_map_simd inv_sqrt2 = alwan_map_simd_set1((alwan_map_lane)(0.70710678118654752440));
        alwan_map_simd k_144 = alwan_map_simd_set1((alwan_map_lane)(14.4));
        alwan_map_simd k_59 = alwan_map_simd_set1((alwan_map_lane)(5.9));
        alwan_map_simd k_042 = alwan_map_simd_set1((alwan_map_lane)(0.042));
        /* Polynomial k coefficients */
        alwan_map_simd ka = alwan_map_simd_set1((alwan_map_lane)(4.4934)), kb = alwan_map_simd_set1((alwan_map_lane)(4.3034));
        alwan_map_simd kc = alwan_map_simd_set1((alwan_map_lane)(-4.276)), kd = alwan_map_simd_set1((alwan_map_lane)(-1.3744));
        alwan_map_simd ke = alwan_map_simd_set1((alwan_map_lane)(-2.5643)), kf = alwan_map_simd_set1((alwan_map_lane)(1.8103));
        /* j,g coefficients */
        alwan_map_simd j0 = alwan_map_simd_set1((alwan_map_lane)(1.7)), j1 = alwan_map_simd_set1((alwan_map_lane)(8.0)), j2 = alwan_map_simd_set1((alwan_map_lane)(-9.7));
        alwan_map_simd g0 = alwan_map_simd_set1((alwan_map_lane)(-13.7)), g1 = alwan_map_simd_set1((alwan_map_lane)(17.7)), g2 = alwan_map_simd_set1((alwan_map_lane)(-4.0));
        alwan_map_simd k30 = alwan_map_simd_set1((alwan_map_lane)(30.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            /* xyY chromaticity */
            alwan_map_simd sum = alwan_map_simd_add(alwan_map_simd_add(vx, vy), vz);
            alwan_map_simd_mask is_black = alwan_map_simd_cmplt(sum, guard);
            alwan_map_simd sum_safe = alwan_map_simd_select(is_black, one, sum);
            alwan_map_simd cx = alwan_map_simd_div(vx, sum_safe);
            alwan_map_simd cy_v = alwan_map_simd_div(vy, sum_safe);
            /* Polynomial k: 4.4934*cx^2 + 4.3034*cy^2 - 4.276*cx*cy - 1.3744*cx - 2.5643*cy + 1.8103 */
            alwan_map_simd cx2 = alwan_map_simd_mul(cx, cx);
            alwan_map_simd cy2 = alwan_map_simd_mul(cy_v, cy_v);
            alwan_map_simd cxy = alwan_map_simd_mul(cx, cy_v);
            alwan_map_simd k_val = alwan_map_simd_fmadd(ka, cx2, alwan_map_simd_fmadd(kb, cy2, alwan_map_simd_fmadd(kc, cxy, alwan_map_simd_fmadd(kd, cx, alwan_map_simd_fmadd(ke, cy_v, kf)))));
            alwan_map_simd Y0 = alwan_map_simd_max(zero, alwan_map_simd_mul(vy, k_val));
            /* Lambda = 5.9*(Y0^(1/3) - 2/3 + 0.042*(Y0-30)^(1/3)) */
            alwan_map_simd Y0_cbrt = alwan_map_simd_pow(Y0, third);
            alwan_map_simd Y0m30 = alwan_map_simd_sub(Y0, k30);
            alwan_map_simd abs_Y0m30 = alwan_map_simd_abs(Y0m30);
            alwan_map_simd cbrt_abs = alwan_map_simd_pow(abs_Y0m30, third);
            alwan_map_simd_mask pos_y0m30 = alwan_map_simd_cmpge(Y0m30, zero);
            alwan_map_simd Y0m30_cbrt = alwan_map_simd_select(pos_y0m30, cbrt_abs, alwan_map_simd_neg(cbrt_abs));
            alwan_map_simd Y0_es = alwan_map_simd_sub(Y0_cbrt, two_thirds);
            alwan_map_simd lambda = alwan_map_simd_mul(k_59, alwan_map_simd_fmadd(k_042, Y0m30_cbrt, Y0_es));
            /* mat3: XYZ -> RGB_osa */
            alwan_map_simd ro = alwan_map_simd_fmadd(m00, vx, alwan_map_simd_fmadd(m01, vy, alwan_map_simd_mul(m02, vz)));
            alwan_map_simd go = alwan_map_simd_fmadd(m10, vx, alwan_map_simd_fmadd(m11, vy, alwan_map_simd_mul(m12, vz)));
            alwan_map_simd bo = alwan_map_simd_fmadd(m20, vx, alwan_map_simd_fmadd(m21, vy, alwan_map_simd_mul(m22, vz)));
            /* Sign-preserving cube root */
            alwan_map_simd abs_r = alwan_map_simd_abs(ro), abs_g = alwan_map_simd_abs(go), abs_b = alwan_map_simd_abs(bo);
            alwan_map_simd r_cbrt = alwan_map_simd_pow(abs_r, third);
            alwan_map_simd g_cbrt = alwan_map_simd_pow(abs_g, third);
            alwan_map_simd b_cbrt = alwan_map_simd_pow(abs_b, third);
            alwan_map_simd_mask r_pos = alwan_map_simd_cmpge(ro, zero);
            alwan_map_simd_mask g_pos = alwan_map_simd_cmpge(go, zero);
            alwan_map_simd_mask b_pos = alwan_map_simd_cmpge(bo, zero);
            r_cbrt = alwan_map_simd_select(r_pos, r_cbrt, alwan_map_simd_neg(r_cbrt));
            g_cbrt = alwan_map_simd_select(g_pos, g_cbrt, alwan_map_simd_neg(g_cbrt));
            b_cbrt = alwan_map_simd_select(b_pos, b_cbrt, alwan_map_simd_neg(b_cbrt));
            /* Chroma coefficient C */
            alwan_map_simd abs_Y0es = alwan_map_simd_abs(Y0_es);
            alwan_map_simd_mask y0es_tiny = alwan_map_simd_cmplt(abs_Y0es, guard);
            alwan_map_simd C = alwan_map_simd_select(y0es_tiny, one, alwan_map_simd_div(lambda, alwan_map_simd_mul(k_59, Y0_es)));
            /* L, j, g */
            alwan_map_simd L_val = alwan_map_simd_mul(alwan_map_simd_sub(lambda, k_144), inv_sqrt2);
            alwan_map_simd j_val = alwan_map_simd_mul(C, alwan_map_simd_fmadd(j0, r_cbrt, alwan_map_simd_fmadd(j1, g_cbrt, alwan_map_simd_mul(j2, b_cbrt))));
            alwan_map_simd g_val = alwan_map_simd_mul(C, alwan_map_simd_fmadd(g0, r_cbrt, alwan_map_simd_fmadd(g1, g_cbrt, alwan_map_simd_mul(g2, b_cbrt))));
            /* Black guard */
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(is_black, zero, L_val));
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(is_black, zero, j_val));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(is_black, zero, g_val));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_osa_ucs) r = ALWAN_MAP_LITV(alwan_xyz_to_osa_ucs)(xyz);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.j; d2[i] = (alwan_map_lane)r.g;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_osa_ucs, _map_interleave)(alwan_map_lane *osa_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !osa_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_osa_ucs_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(osa_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__osa_ucs_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                          alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                          size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd sqrt2 = alwan_map_simd_set1((alwan_map_lane)(1.41421356237309504880));
        alwan_map_simd k_144 = alwan_map_simd_set1((alwan_map_lane)(14.4));
        alwan_map_simd inv_59 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 5.9));
        alwan_map_simd two_thirds = alwan_map_simd_set1((alwan_map_lane)(2.0 / 3.0));
        alwan_map_simd k_59 = alwan_map_simd_set1((alwan_map_lane)(5.9));
        alwan_map_simd guard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        /* Approximate inverse RGB' coefficients */
        alwan_map_simd jr0 = alwan_map_simd_set1((alwan_map_lane)(0.01)), jr1 = alwan_map_simd_set1((alwan_map_lane)(-0.02));
        alwan_map_simd jg0 = alwan_map_simd_set1((alwan_map_lane)(0.12)), jg1 = alwan_map_simd_set1((alwan_map_lane)(0.06));
        alwan_map_simd jb0 = alwan_map_simd_set1((alwan_map_lane)(-0.10)), jb1 = alwan_map_simd_set1((alwan_map_lane)(-0.05));
        for (; i + W <= n; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vj = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c2[i]);
            /* lambda = L*sqrt(2) + 14.4 */
            alwan_map_simd lambda = alwan_map_simd_fmadd(vL, sqrt2, k_144);
            /* Y0_cbrt = lambda/5.9 + 2/3 */
            alwan_map_simd Y0_cbrt = alwan_map_simd_fmadd(lambda, inv_59, two_thirds);
            /* C = lambda / (5.9 * (Y0_cbrt - 2/3)) */
            alwan_map_simd Y0_es = alwan_map_simd_sub(Y0_cbrt, two_thirds);
            alwan_map_simd_mask y0_gt = alwan_map_simd_cmpgt(Y0_cbrt, two_thirds);
            alwan_map_simd C = alwan_map_simd_select(y0_gt, alwan_map_simd_div(lambda, alwan_map_simd_mul(k_59, Y0_es)), one);
            /* Guard C too small */
            alwan_map_simd abs_C = alwan_map_simd_abs(C);
            alwan_map_simd_mask c_tiny = alwan_map_simd_cmplt(abs_C, guard);
            alwan_map_simd C_safe = alwan_map_simd_select(c_tiny, one, C);
            alwan_map_simd j_norm = alwan_map_simd_div(vj, C_safe);
            alwan_map_simd g_norm = alwan_map_simd_div(vg, C_safe);
            /* Recover cube roots */
            alwan_map_simd r_cbrt = alwan_map_simd_fmadd(jr0, j_norm, alwan_map_simd_fmadd(jr1, g_norm, Y0_cbrt));
            alwan_map_simd g_cbrt = alwan_map_simd_fmadd(jg0, j_norm, alwan_map_simd_fmadd(jg1, g_norm, Y0_cbrt));
            alwan_map_simd b_cbrt = alwan_map_simd_fmadd(jb0, j_norm, alwan_map_simd_fmadd(jb1, g_norm, Y0_cbrt));
            /* Cube (sign-preserving): x^3 */
            alwan_map_simd r2 = alwan_map_simd_mul(r_cbrt, r_cbrt), r3 = alwan_map_simd_mul(r2, r_cbrt);
            alwan_map_simd g2 = alwan_map_simd_mul(g_cbrt, g_cbrt), g3 = alwan_map_simd_mul(g2, g_cbrt);
            alwan_map_simd b2 = alwan_map_simd_mul(b_cbrt, b_cbrt), b3 = alwan_map_simd_mul(b2, b_cbrt);
            /* mat3: RGB -> XYZ (load matrix elements on demand, reducing pre-loop register pressure) */
            alwan_map_simd ox, oy, oz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(RGB_TO_XYZ_OSA), r3, g3, b3);
            /* Clamp negatives + C_tiny guard */
            ox = alwan_map_simd_max(zero, ox); oy = alwan_map_simd_max(zero, oy); oz = alwan_map_simd_max(zero, oz);
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(c_tiny, zero, ox));
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(c_tiny, zero, oy));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(c_tiny, zero, oz));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_osa_ucs) osa = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_osa_ucs_to_xyz)(osa);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_osa_ucs_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *osa_in, size_t in_stride, size_t count) {
    if (!osa_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, osa_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__osa_ucs_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> Hunter Lab
 * ---------------------------------------------------------------- */

/* Local Hunter Lab D65 constants (scalar literals, cast to alwan_map_lane) */
#ifndef ALWAN_HUNTER_D65_XN
#define ALWAN_HUNTER_D65_XN  95.02
#define ALWAN_HUNTER_D65_YN  100.0
#define ALWAN_HUNTER_D65_ZN  108.82
#define ALWAN_HUNTER_KA_D65  172.30
#define ALWAN_HUNTER_KB_D65  67.20
#define ALWAN_HUNTER_DEFS_LOCAL_
#endif

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_hunter_lab_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                             alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                             size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd inv_xn = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_HUNTER_D65_XN));
        alwan_map_simd inv_yn = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_HUNTER_D65_YN));
        alwan_map_simd inv_zn = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_HUNTER_D65_ZN));
        alwan_map_simd k100 = alwan_map_simd_set1((alwan_map_lane)(100.0));
        alwan_map_simd ka = alwan_map_simd_set1((alwan_map_lane)(ALWAN_HUNTER_KA_D65));
        alwan_map_simd kb = alwan_map_simd_set1((alwan_map_lane)(ALWAN_HUNTER_KB_D65));
        alwan_map_simd guard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd y_ratio = alwan_map_simd_mul(vy, inv_yn);
            alwan_map_simd sqrt_yr = alwan_map_simd_sqrt(y_ratio);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(sqrt_yr, guard);
            alwan_map_simd safe = alwan_map_simd_select(tiny, zero, one);
            alwan_map_simd inv_sqrt = alwan_map_simd_select(tiny, zero, alwan_map_simd_div(one, sqrt_yr));
            alwan_map_simd x_ratio = alwan_map_simd_mul(vx, inv_xn);
            alwan_map_simd z_ratio = alwan_map_simd_mul(vz, inv_zn);
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_mul(k100, sqrt_yr), safe));
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(alwan_map_simd_mul(ka, alwan_map_simd_sub(x_ratio, y_ratio)), alwan_map_simd_mul(inv_sqrt, safe)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_mul(alwan_map_simd_mul(kb, alwan_map_simd_sub(y_ratio, z_ratio)), alwan_map_simd_mul(inv_sqrt, safe)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hunter_lab) r = ALWAN_MAP_LITV(alwan_xyz_to_hunter_lab)(xyz);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.a; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_hunter_lab, _map_interleave)(alwan_map_lane *hl_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !hl_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_hunter_lab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hl_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hunter_lab_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                             alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                             size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd inv100 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 100.0));
        alwan_map_simd yn = alwan_map_simd_set1((alwan_map_lane)(ALWAN_HUNTER_D65_YN));
        alwan_map_simd xn = alwan_map_simd_set1((alwan_map_lane)(ALWAN_HUNTER_D65_XN));
        alwan_map_simd zn = alwan_map_simd_set1((alwan_map_lane)(ALWAN_HUNTER_D65_ZN));
        alwan_map_simd inv_ka = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_HUNTER_KA_D65));
        alwan_map_simd inv_kb = alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_HUNTER_KB_D65));
        for (; i + W <= n; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd l_norm = alwan_map_simd_mul(vL, inv100);
            alwan_map_simd l2 = alwan_map_simd_mul(l_norm, l_norm);
            /* y = l_norm^2 * YN */
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(l2, yn));
            /* a_term = (a/KA) * l_norm; x = (a_term + l_norm^2) * XN */
            alwan_map_simd a_term = alwan_map_simd_mul(alwan_map_simd_mul(va, inv_ka), l_norm);
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_add(a_term, l2), xn));
            /* b_term = (b/KB) * l_norm; z = -(b_term - l_norm^2) * ZN */
            alwan_map_simd b_term = alwan_map_simd_mul(alwan_map_simd_mul(vb, inv_kb), l_norm);
            alwan_map_simd_store(&d2[i], alwan_map_simd_mul(alwan_map_simd_sub(l2, b_term), zn));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_hunter_lab) hl = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_hunter_lab_to_xyz)(hl);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_hunter_lab_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *hl_in, size_t in_stride, size_t count) {
    if (!hl_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hl_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hunter_lab_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* Clean up local Hunter Lab constant definitions */
#ifdef ALWAN_HUNTER_DEFS_LOCAL_
#undef ALWAN_HUNTER_D65_XN
#undef ALWAN_HUNTER_D65_YN
#undef ALWAN_HUNTER_D65_ZN
#undef ALWAN_HUNTER_KA_D65
#undef ALWAN_HUNTER_KB_D65
#undef ALWAN_HUNTER_DEFS_LOCAL_
#endif

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_hunter_lab_custom_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                                    alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                                    size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    ALWAN_MAP_LIT(alwan_vec2) coeffs = ALWAN_MAP_LITV(alwan_hunter_coefficients)(*white_xyz);
    /* coeffs is consumed only inside the SIMD branch via ka/kb; the
     * scalar fallback recomputes via ALWAN_MAP_LITV(...). Reference
     * unconditionally so MSVC /WX builds with ALWAN_MAP_SIMD_WIDTH == 1
     * (Windows ARM) don't fail on C4101. */
    (void)coeffs;
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd inv_xn = alwan_map_simd_set1((alwan_map_lane)(1.0 / white_xyz->x));
        alwan_map_simd inv_yn = alwan_map_simd_set1((alwan_map_lane)(1.0 / white_xyz->y));
        alwan_map_simd inv_zn = alwan_map_simd_set1((alwan_map_lane)(1.0 / white_xyz->z));
        alwan_map_simd k10 = alwan_map_simd_set1((alwan_map_lane)(10.0));
        alwan_map_simd ka = alwan_map_simd_set1((alwan_map_lane)(coeffs.v[0]));
        alwan_map_simd kb = alwan_map_simd_set1((alwan_map_lane)(coeffs.v[1]));
        alwan_map_simd guard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd y_ratio = alwan_map_simd_mul(vy, inv_yn);
            alwan_map_simd sqrt_yr = alwan_map_simd_sqrt(y_ratio);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(sqrt_yr, guard);
            alwan_map_simd safe = alwan_map_simd_select(tiny, zero, one);
            alwan_map_simd inv_sqrt = alwan_map_simd_select(tiny, zero, alwan_map_simd_div(one, sqrt_yr));
            alwan_map_simd x_ratio = alwan_map_simd_mul(vx, inv_xn);
            alwan_map_simd z_ratio = alwan_map_simd_mul(vz, inv_zn);
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_mul(k10, sqrt_yr), safe));
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(alwan_map_simd_mul(ka, alwan_map_simd_sub(x_ratio, y_ratio)), alwan_map_simd_mul(inv_sqrt, safe)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_mul(alwan_map_simd_mul(kb, alwan_map_simd_sub(y_ratio, z_ratio)), alwan_map_simd_mul(inv_sqrt, safe)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hunter_lab) r = ALWAN_MAP_LITV(alwan_xyz_to_hunter_lab_custom)(xyz, *white_xyz);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.a; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_hunter_lab_custom, _map_interleave)(alwan_map_lane *hl_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    if (!xyz_in || !hl_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_hunter_lab_custom_kernel)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hl_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hunter_lab_to_xyz_custom_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                                    alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                                    size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    ALWAN_MAP_LIT(alwan_vec2) coeffs = ALWAN_MAP_LITV(alwan_hunter_coefficients)(*white_xyz);
    /* See forward-form note above: silence MSVC C4101 on Windows ARM. */
    (void)coeffs;
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd inv10 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 10.0));
        alwan_map_simd v_yn = alwan_map_simd_set1((alwan_map_lane)(white_xyz->y));
        alwan_map_simd v_xn = alwan_map_simd_set1((alwan_map_lane)(white_xyz->x));
        alwan_map_simd v_zn = alwan_map_simd_set1((alwan_map_lane)(white_xyz->z));
        alwan_map_simd inv_ka = alwan_map_simd_set1((alwan_map_lane)(1.0 / coeffs.v[0]));
        alwan_map_simd inv_kb = alwan_map_simd_set1((alwan_map_lane)(1.0 / coeffs.v[1]));
        for (; i + W <= n; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd l_norm = alwan_map_simd_mul(vL, inv10);
            alwan_map_simd l2 = alwan_map_simd_mul(l_norm, l_norm);
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(l2, v_yn));
            alwan_map_simd a_term = alwan_map_simd_mul(alwan_map_simd_mul(va, inv_ka), l_norm);
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_add(a_term, l2), v_xn));
            alwan_map_simd b_term = alwan_map_simd_mul(alwan_map_simd_mul(vb, inv_kb), l_norm);
            alwan_map_simd_store(&d2[i], alwan_map_simd_mul(alwan_map_simd_sub(l2, b_term), v_zn));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_hunter_lab) hl = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_hunter_lab_to_xyz_custom)(hl, *white_xyz);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_hunter_lab_to_xyz_custom, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *hl_in, size_t in_stride, size_t count, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    if (!hl_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hl_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__hunter_lab_to_xyz_custom_kernel)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> ProLab
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_prolab_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                         alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                         size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        /* Precompute: normalize by D65 white then apply Q */
        alwan_map_lane const *Q = ALWAN_MAP_LIT(ALWAN_PROLAB_MATRIX_Q).m;
        alwan_map_lane inv_wx = (alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_PROLAB_D65_WHITE)[0]);
        alwan_map_lane inv_wy = (alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_PROLAB_D65_WHITE)[1]);
        alwan_map_lane inv_wz = (alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_PROLAB_D65_WHITE)[2]);
        /* Fused coefficients: Q * diag(1/wx, 1/wy, 1/wz, 1) */
        alwan_map_simd m00 = alwan_map_simd_set1((alwan_map_lane)(Q[0]*inv_wx)), m01 = alwan_map_simd_set1((alwan_map_lane)(Q[1]*inv_wy)), m02 = alwan_map_simd_set1((alwan_map_lane)(Q[2]*inv_wz)), m03 = alwan_map_simd_set1((alwan_map_lane)(Q[3]));
        alwan_map_simd m10 = alwan_map_simd_set1((alwan_map_lane)(Q[4]*inv_wx)), m11 = alwan_map_simd_set1((alwan_map_lane)(Q[5]*inv_wy)), m12 = alwan_map_simd_set1((alwan_map_lane)(Q[6]*inv_wz)), m13 = alwan_map_simd_set1((alwan_map_lane)(Q[7]));
        alwan_map_simd m20 = alwan_map_simd_set1((alwan_map_lane)(Q[8]*inv_wx)), m21 = alwan_map_simd_set1((alwan_map_lane)(Q[9]*inv_wy)), m22 = alwan_map_simd_set1((alwan_map_lane)(Q[10]*inv_wz)), m23 = alwan_map_simd_set1((alwan_map_lane)(Q[11]));
        alwan_map_simd m30 = alwan_map_simd_set1((alwan_map_lane)(Q[12]*inv_wx)), m31 = alwan_map_simd_set1((alwan_map_lane)(Q[13]*inv_wy)), m32 = alwan_map_simd_set1((alwan_map_lane)(Q[14]*inv_wz)), m33 = alwan_map_simd_set1((alwan_map_lane)(Q[15]));
        alwan_map_simd guard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd r0 = alwan_map_simd_fmadd(m00, vx, alwan_map_simd_fmadd(m01, vy, alwan_map_simd_fmadd(m02, vz, m03)));
            alwan_map_simd r1 = alwan_map_simd_fmadd(m10, vx, alwan_map_simd_fmadd(m11, vy, alwan_map_simd_fmadd(m12, vz, m13)));
            alwan_map_simd r2 = alwan_map_simd_fmadd(m20, vx, alwan_map_simd_fmadd(m21, vy, alwan_map_simd_fmadd(m22, vz, m23)));
            alwan_map_simd w  = alwan_map_simd_fmadd(m30, vx, alwan_map_simd_fmadd(m31, vy, alwan_map_simd_fmadd(m32, vz, m33)));
            alwan_map_simd abs_w = alwan_map_simd_abs(w);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(abs_w, guard);
            alwan_map_simd inv_w = alwan_map_simd_select(tiny, one, alwan_map_simd_div(one, w));
            /* Fallback to normalized input on degenerate w */
            alwan_map_simd nx = alwan_map_simd_mul(vx, alwan_map_simd_set1((alwan_map_lane)(inv_wx)));
            alwan_map_simd ny = alwan_map_simd_mul(vy, alwan_map_simd_set1((alwan_map_lane)(inv_wy)));
            alwan_map_simd nz = alwan_map_simd_mul(vz, alwan_map_simd_set1((alwan_map_lane)(inv_wz)));
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(tiny, nx, alwan_map_simd_mul(r0, inv_w)));
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(tiny, ny, alwan_map_simd_mul(r1, inv_w)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(tiny, nz, alwan_map_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_prolab) r = ALWAN_MAP_LITV(alwan_xyz_to_prolab)(xyz);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.a; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_prolab, _map_interleave)(alwan_map_lane *prolab_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count) {
    if (!xyz_in || !prolab_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_prolab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(prolab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__prolab_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                         alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                         size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *Qi = ALWAN_MAP_LIT(ALWAN_PROLAB_MATRIX_Q_INV).m;
        alwan_map_lane wx = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_PROLAB_D65_WHITE)[0]);
        alwan_map_lane wy = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_PROLAB_D65_WHITE)[1]);
        alwan_map_lane wz = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_PROLAB_D65_WHITE)[2]);
        /* Fused coefficients: diag(wx,wy,wz) * Q_inv */
        alwan_map_simd m00 = alwan_map_simd_set1((alwan_map_lane)(Qi[0]*wx)), m01 = alwan_map_simd_set1((alwan_map_lane)(Qi[1]*wx)), m02 = alwan_map_simd_set1((alwan_map_lane)(Qi[2]*wx)), m03 = alwan_map_simd_set1((alwan_map_lane)(Qi[3]*wx));
        alwan_map_simd m10 = alwan_map_simd_set1((alwan_map_lane)(Qi[4]*wy)), m11 = alwan_map_simd_set1((alwan_map_lane)(Qi[5]*wy)), m12 = alwan_map_simd_set1((alwan_map_lane)(Qi[6]*wy)), m13 = alwan_map_simd_set1((alwan_map_lane)(Qi[7]*wy));
        alwan_map_simd m20 = alwan_map_simd_set1((alwan_map_lane)(Qi[8]*wz)), m21 = alwan_map_simd_set1((alwan_map_lane)(Qi[9]*wz)), m22 = alwan_map_simd_set1((alwan_map_lane)(Qi[10]*wz)), m23 = alwan_map_simd_set1((alwan_map_lane)(Qi[11]*wz));
        alwan_map_simd m30 = alwan_map_simd_set1((alwan_map_lane)(Qi[12])), m31 = alwan_map_simd_set1((alwan_map_lane)(Qi[13])), m32 = alwan_map_simd_set1((alwan_map_lane)(Qi[14])), m33 = alwan_map_simd_set1((alwan_map_lane)(Qi[15]));
        alwan_map_simd guard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            /* Compute w first for perspective divide */
            alwan_map_simd w  = alwan_map_simd_fmadd(m30, vL, alwan_map_simd_fmadd(m31, va, alwan_map_simd_fmadd(m32, vb, m33)));
            alwan_map_simd abs_w = alwan_map_simd_abs(w);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(abs_w, guard);
            alwan_map_simd inv_w = alwan_map_simd_select(tiny, one, alwan_map_simd_div(one, w));
            /* Rows with fused white multiplication */
            alwan_map_simd r0 = alwan_map_simd_fmadd(m00, vL, alwan_map_simd_fmadd(m01, va, alwan_map_simd_fmadd(m02, vb, m03)));
            alwan_map_simd r1 = alwan_map_simd_fmadd(m10, vL, alwan_map_simd_fmadd(m11, va, alwan_map_simd_fmadd(m12, vb, m13)));
            alwan_map_simd r2 = alwan_map_simd_fmadd(m20, vL, alwan_map_simd_fmadd(m21, va, alwan_map_simd_fmadd(m22, vb, m23)));
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(tiny, alwan_map_simd_mul(vL, alwan_map_simd_set1((alwan_map_lane)(wx))), alwan_map_simd_mul(r0, inv_w)));
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(tiny, alwan_map_simd_mul(va, alwan_map_simd_set1((alwan_map_lane)(wy))), alwan_map_simd_mul(r1, inv_w)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(tiny, alwan_map_simd_mul(vb, alwan_map_simd_set1((alwan_map_lane)(wz))), alwan_map_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_prolab) prolab = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_prolab_to_xyz)(prolab);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_prolab_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *prolab_in, size_t in_stride, size_t count) {
    if (!prolab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, prolab_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__prolab_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_prolab_custom_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                                alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                                size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    /* Precompute safe white point reciprocals */
    alwan_map_lane const eps = (alwan_map_lane)(1e-10);
    alwan_map_lane inv_wx = (alwan_map_lane)((alwan_map_scalar_abs(white_xyz->x) > eps) ? 1.0 / white_xyz->x : 1.0 / eps);
    alwan_map_lane inv_wy = (alwan_map_lane)((alwan_map_scalar_abs(white_xyz->y) > eps) ? 1.0 / white_xyz->y : 1.0 / eps);
    alwan_map_lane inv_wz = (alwan_map_lane)((alwan_map_scalar_abs(white_xyz->z) > eps) ? 1.0 / white_xyz->z : 1.0 / eps);
    /* Folded into the SIMD Q*inv_w matrix below; reference unconditionally
     * so MSVC /WX builds with ALWAN_MAP_SIMD_WIDTH == 1 don't fail on C4189. */
    (void)inv_wx; (void)inv_wy; (void)inv_wz;
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *Q = ALWAN_MAP_LIT(ALWAN_PROLAB_MATRIX_Q).m;
        alwan_map_simd m00 = alwan_map_simd_set1((alwan_map_lane)(Q[0]*inv_wx)), m01 = alwan_map_simd_set1((alwan_map_lane)(Q[1]*inv_wy)), m02 = alwan_map_simd_set1((alwan_map_lane)(Q[2]*inv_wz)), m03 = alwan_map_simd_set1((alwan_map_lane)(Q[3]));
        alwan_map_simd m10 = alwan_map_simd_set1((alwan_map_lane)(Q[4]*inv_wx)), m11 = alwan_map_simd_set1((alwan_map_lane)(Q[5]*inv_wy)), m12 = alwan_map_simd_set1((alwan_map_lane)(Q[6]*inv_wz)), m13 = alwan_map_simd_set1((alwan_map_lane)(Q[7]));
        alwan_map_simd m20 = alwan_map_simd_set1((alwan_map_lane)(Q[8]*inv_wx)), m21 = alwan_map_simd_set1((alwan_map_lane)(Q[9]*inv_wy)), m22 = alwan_map_simd_set1((alwan_map_lane)(Q[10]*inv_wz)), m23 = alwan_map_simd_set1((alwan_map_lane)(Q[11]));
        alwan_map_simd m30 = alwan_map_simd_set1((alwan_map_lane)(Q[12]*inv_wx)), m31 = alwan_map_simd_set1((alwan_map_lane)(Q[13]*inv_wy)), m32 = alwan_map_simd_set1((alwan_map_lane)(Q[14]*inv_wz)), m33 = alwan_map_simd_set1((alwan_map_lane)(Q[15]));
        alwan_map_simd vguard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            alwan_map_simd r0 = alwan_map_simd_fmadd(m00, vx, alwan_map_simd_fmadd(m01, vy, alwan_map_simd_fmadd(m02, vz, m03)));
            alwan_map_simd r1 = alwan_map_simd_fmadd(m10, vx, alwan_map_simd_fmadd(m11, vy, alwan_map_simd_fmadd(m12, vz, m13)));
            alwan_map_simd r2 = alwan_map_simd_fmadd(m20, vx, alwan_map_simd_fmadd(m21, vy, alwan_map_simd_fmadd(m22, vz, m23)));
            alwan_map_simd w  = alwan_map_simd_fmadd(m30, vx, alwan_map_simd_fmadd(m31, vy, alwan_map_simd_fmadd(m32, vz, m33)));
            alwan_map_simd abs_w = alwan_map_simd_abs(w);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(abs_w, vguard);
            alwan_map_simd inv_w = alwan_map_simd_select(tiny, one, alwan_map_simd_div(one, w));
            alwan_map_simd nx = alwan_map_simd_mul(vx, alwan_map_simd_set1((alwan_map_lane)(inv_wx)));
            alwan_map_simd ny = alwan_map_simd_mul(vy, alwan_map_simd_set1((alwan_map_lane)(inv_wy)));
            alwan_map_simd nz = alwan_map_simd_mul(vz, alwan_map_simd_set1((alwan_map_lane)(inv_wz)));
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(tiny, nx, alwan_map_simd_mul(r0, inv_w)));
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(tiny, ny, alwan_map_simd_mul(r1, inv_w)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(tiny, nz, alwan_map_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_prolab) r = ALWAN_MAP_LITV(alwan_xyz_to_prolab_custom)(xyz, *white_xyz);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.a; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_prolab_custom, _map_interleave)(alwan_map_lane *prolab_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    if (!xyz_in || !prolab_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_prolab_custom_kernel)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(prolab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__prolab_to_xyz_custom_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                                alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                                size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *Qi = ALWAN_MAP_LIT(ALWAN_PROLAB_MATRIX_Q_INV).m;
        alwan_map_lane wx = (alwan_map_lane)white_xyz->x, wy = (alwan_map_lane)white_xyz->y, wz = (alwan_map_lane)white_xyz->z;
        alwan_map_simd m00 = alwan_map_simd_set1((alwan_map_lane)(Qi[0]*wx)), m01 = alwan_map_simd_set1((alwan_map_lane)(Qi[1]*wx)), m02 = alwan_map_simd_set1((alwan_map_lane)(Qi[2]*wx)), m03 = alwan_map_simd_set1((alwan_map_lane)(Qi[3]*wx));
        alwan_map_simd m10 = alwan_map_simd_set1((alwan_map_lane)(Qi[4]*wy)), m11 = alwan_map_simd_set1((alwan_map_lane)(Qi[5]*wy)), m12 = alwan_map_simd_set1((alwan_map_lane)(Qi[6]*wy)), m13 = alwan_map_simd_set1((alwan_map_lane)(Qi[7]*wy));
        alwan_map_simd m20 = alwan_map_simd_set1((alwan_map_lane)(Qi[8]*wz)), m21 = alwan_map_simd_set1((alwan_map_lane)(Qi[9]*wz)), m22 = alwan_map_simd_set1((alwan_map_lane)(Qi[10]*wz)), m23 = alwan_map_simd_set1((alwan_map_lane)(Qi[11]*wz));
        alwan_map_simd m30 = alwan_map_simd_set1((alwan_map_lane)(Qi[12])), m31 = alwan_map_simd_set1((alwan_map_lane)(Qi[13])), m32 = alwan_map_simd_set1((alwan_map_lane)(Qi[14])), m33 = alwan_map_simd_set1((alwan_map_lane)(Qi[15]));
        alwan_map_simd guard = alwan_map_simd_set1((alwan_map_lane)(1e-10));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd w  = alwan_map_simd_fmadd(m30, vL, alwan_map_simd_fmadd(m31, va, alwan_map_simd_fmadd(m32, vb, m33)));
            alwan_map_simd abs_w = alwan_map_simd_abs(w);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(abs_w, guard);
            alwan_map_simd inv_w = alwan_map_simd_select(tiny, one, alwan_map_simd_div(one, w));
            alwan_map_simd r0 = alwan_map_simd_fmadd(m00, vL, alwan_map_simd_fmadd(m01, va, alwan_map_simd_fmadd(m02, vb, m03)));
            alwan_map_simd r1 = alwan_map_simd_fmadd(m10, vL, alwan_map_simd_fmadd(m11, va, alwan_map_simd_fmadd(m12, vb, m13)));
            alwan_map_simd r2 = alwan_map_simd_fmadd(m20, vL, alwan_map_simd_fmadd(m21, va, alwan_map_simd_fmadd(m22, vb, m23)));
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(tiny, alwan_map_simd_mul(vL, alwan_map_simd_set1((alwan_map_lane)(wx))), alwan_map_simd_mul(r0, inv_w)));
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(tiny, alwan_map_simd_mul(va, alwan_map_simd_set1((alwan_map_lane)(wy))), alwan_map_simd_mul(r1, inv_w)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(tiny, alwan_map_simd_mul(vb, alwan_map_simd_set1((alwan_map_lane)(wz))), alwan_map_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_prolab) prolab = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_prolab_to_xyz_custom)(prolab, *white_xyz);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_prolab_to_xyz_custom, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *prolab_in, size_t in_stride, size_t count, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    if (!prolab_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, prolab_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__prolab_to_xyz_custom_kernel)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> UVW (with white point)
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__xyz_to_uvw_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                      size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    /* Precompute white point chromaticity */
    alwan_map_lane sum_n = (alwan_map_lane)(white_xyz->x + 15.0 * white_xyz->y + 3.0 * white_xyz->z);
    alwan_map_lane un_s = (alwan_map_lane)((alwan_map_scalar_abs(sum_n) < ALWAN_EPSILON) ? 0.0 : (4.0 * white_xyz->x) / sum_n);
    alwan_map_lane vn_s = (alwan_map_lane)((alwan_map_scalar_abs(sum_n) < ALWAN_EPSILON) ? 0.0 : (6.0 * white_xyz->y) / sum_n);
    /* Broadcast into SIMD un_w/vn_w; silence MSVC C4189 on Windows ARM. */
    (void)un_s; (void)vn_s;
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd eps = alwan_map_simd_set1((alwan_map_lane)(ALWAN_EPSILON));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd k4 = alwan_map_simd_set1((alwan_map_lane)(4.0));
        alwan_map_simd k6 = alwan_map_simd_set1((alwan_map_lane)(6.0));
        alwan_map_simd k15 = alwan_map_simd_set1((alwan_map_lane)(15.0));
        alwan_map_simd k3 = alwan_map_simd_set1((alwan_map_lane)(3.0));
        alwan_map_simd k13 = alwan_map_simd_set1((alwan_map_lane)(13.0));
        alwan_map_simd k25 = alwan_map_simd_set1((alwan_map_lane)(25.0));
        alwan_map_simd kn17 = alwan_map_simd_set1((alwan_map_lane)(-17.0));
        alwan_map_simd k100 = alwan_map_simd_set1((alwan_map_lane)(100.0));
        alwan_map_simd v_un = alwan_map_simd_set1((alwan_map_lane)(un_s));
        alwan_map_simd v_vn = alwan_map_simd_set1((alwan_map_lane)(vn_s));
        alwan_map_simd inv_wy = alwan_map_simd_set1((alwan_map_lane)(white_xyz->y > ALWAN_EPSILON ? 1.0 / white_xyz->y : 0.0));
        for (; i + W <= n; i += W) {
            alwan_map_simd vx = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vy = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vz = alwan_map_simd_load(&c2[i]);
            /* sum = x + 15*y + 3*z */
            alwan_map_simd sum = alwan_map_simd_fmadd(k15, vy, alwan_map_simd_fmadd(k3, vz, vx));
            alwan_map_simd abs_sum = alwan_map_simd_abs(sum);
            alwan_map_simd_mask sum_tiny = alwan_map_simd_cmplt(abs_sum, eps);
            alwan_map_simd inv_sum = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), sum);
            alwan_map_simd u = alwan_map_simd_select(sum_tiny, zero, alwan_map_simd_mul(alwan_map_simd_mul(k4, vx), inv_sum));
            alwan_map_simd v = alwan_map_simd_select(sum_tiny, zero, alwan_map_simd_mul(alwan_map_simd_mul(k6, vy), inv_sum));
            /* Yp = (y / white.y) * 100 (luminance factor in percent); W = 25*cbrt(Yp) - 17 */
            alwan_map_simd Yp = alwan_map_simd_mul(alwan_map_simd_mul(vy, inv_wy), k100);
            alwan_map_simd_mask y_tiny = alwan_map_simd_cmplt(Yp, eps);
            alwan_map_simd Wstar = alwan_map_simd_select(y_tiny, kn17,
                                alwan_map_simd_fmadd(k25, alwan_map_simd_cbrt(Yp), kn17));
            /* U* = 13*W*(u-un), V* = 13*W*(v-vn) */
            alwan_map_simd k13W = alwan_map_simd_mul(k13, Wstar);
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(k13W, alwan_map_simd_sub(u, v_un)));
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(k13W, alwan_map_simd_sub(v, v_vn)));
            alwan_map_simd_store(&d2[i], Wstar);
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_uvw) r = ALWAN_MAP_LITV(alwan_xyz_to_uvw)(xyz, *white_xyz);
        d0[i] = (alwan_map_lane)r.U; d1[i] = (alwan_map_lane)r.V; d2[i] = (alwan_map_lane)r.W;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_xyz_to_uvw, _map_interleave)(alwan_map_lane *uvw_out, size_t out_stride, alwan_map_lane const *xyz_in, size_t in_stride, size_t count, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    if (!xyz_in || !uvw_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_uvw_kernel)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(uvw_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__uvw_to_xyz_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                      size_t n, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    /* Precompute white point chromaticity */
    alwan_map_lane sum_n = (alwan_map_lane)(white_xyz->x + 15.0 * white_xyz->y + 3.0 * white_xyz->z);
    alwan_map_lane un_s = (alwan_map_lane)((alwan_map_scalar_abs(sum_n) < ALWAN_EPSILON) ? 0.0 : (4.0 * white_xyz->x) / sum_n);
    alwan_map_lane vn_s = (alwan_map_lane)((alwan_map_scalar_abs(sum_n) < ALWAN_EPSILON) ? 0.0 : (6.0 * white_xyz->y) / sum_n);
    /* Broadcast into SIMD un_w/vn_w; silence MSVC C4189 on Windows ARM. */
    (void)un_s; (void)vn_s;
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd eps = alwan_map_simd_set1((alwan_map_lane)(ALWAN_EPSILON));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd k13 = alwan_map_simd_set1((alwan_map_lane)(13.0));
        alwan_map_simd k17 = alwan_map_simd_set1((alwan_map_lane)(17.0));
        alwan_map_simd inv25 = alwan_map_simd_set1((alwan_map_lane)(1.0 / 25.0));
        alwan_map_simd k6 = alwan_map_simd_set1((alwan_map_lane)(6.0));
        alwan_map_simd inv4 = alwan_map_simd_set1((alwan_map_lane)(0.25));
        alwan_map_simd k8 = alwan_map_simd_set1((alwan_map_lane)(8.0));
        alwan_map_simd kn2 = alwan_map_simd_set1((alwan_map_lane)(-2.0));
        alwan_map_simd kn20 = alwan_map_simd_set1((alwan_map_lane)(-20.0));
        alwan_map_simd wy001 = alwan_map_simd_set1((alwan_map_lane)(white_xyz->y * 0.01));
        alwan_map_simd v_un = alwan_map_simd_set1((alwan_map_lane)(un_s));
        alwan_map_simd v_vn = alwan_map_simd_set1((alwan_map_lane)(vn_s));
        for (; i + W <= n; i += W) {
            alwan_map_simd vU = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vV = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vW = alwan_map_simd_load(&c2[i]);
            /* Y from W*: Y_cbrt = (W+17)/25; Yp = Y_cbrt^3 (percent); Y = Yp * white.y * 0.01 */
            alwan_map_simd W17 = alwan_map_simd_add(vW, k17);
            alwan_map_simd_mask w17_tiny = alwan_map_simd_cmplt(W17, eps);
            alwan_map_simd Y_cbrt = alwan_map_simd_select(w17_tiny, zero, alwan_map_simd_mul(W17, inv25));
            alwan_map_simd Yp = alwan_map_simd_mul(alwan_map_simd_mul(Y_cbrt, Y_cbrt), Y_cbrt);
            alwan_map_simd Y = alwan_map_simd_mul(Yp, wy001);
            /* Recover u, v from U*, V*, W* */
            alwan_map_simd W13 = alwan_map_simd_mul(k13, vW);
            alwan_map_simd abs_W = alwan_map_simd_abs(vW);
            alwan_map_simd_mask w_tiny = alwan_map_simd_cmplt(abs_W, eps);
            alwan_map_simd u = alwan_map_simd_select(w_tiny, v_un,
                            alwan_map_simd_add(alwan_map_simd_div(vU, W13), v_un));
            alwan_map_simd v = alwan_map_simd_select(w_tiny, v_vn,
                            alwan_map_simd_add(alwan_map_simd_div(vV, W13), v_vn));
            /* X = 6*u*Y / (4*v), Z = (8 - 2*u - 20*v)*Y / (4*v) */
            alwan_map_simd abs_v = alwan_map_simd_abs(v);
            alwan_map_simd_mask v_tiny = alwan_map_simd_cmplt(abs_v, eps);
            alwan_map_simd inv_4v = alwan_map_simd_div(inv4, v);
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(v_tiny, zero,
                                alwan_map_simd_mul(alwan_map_simd_mul(k6, alwan_map_simd_mul(u, Y)), inv_4v)));
            alwan_map_simd_store(&d1[i], Y);
            /* (8 - 2*u - 20*v) */
            alwan_map_simd numer = alwan_map_simd_fmadd(kn2, u, alwan_map_simd_fmadd(kn20, v, k8));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(v_tiny, zero,
                                alwan_map_simd_mul(alwan_map_simd_mul(numer, Y), inv_4v)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_uvw) uvw = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_uvw_to_xyz)(uvw, *white_xyz);
        d0[i] = (alwan_map_lane)r.x; d1[i] = (alwan_map_lane)r.y; d2[i] = (alwan_map_lane)r.z;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_uvw_to_xyz, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, alwan_map_lane const *uvw_in, size_t in_stride, size_t count, ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) {
    if (!uvw_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, uvw_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__uvw_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> Prismatic
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__rgb_to_prismatic_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                            alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                            size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd eps = alwan_map_simd_set1((alwan_map_lane)(ALWAN_EPSILON));
        alwan_map_simd zero = alwan_map_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd L = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd sum_rgb = alwan_map_simd_add(alwan_map_simd_add(vr, vg), vb);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(sum_rgb, eps);
            alwan_map_simd inv_sum = alwan_map_simd_div(alwan_map_simd_set1((alwan_map_lane)(1.0)), sum_rgb);
            alwan_map_simd_store(&d0[i], L);
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(tiny, zero, alwan_map_simd_mul(vr, inv_sum)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(tiny, zero, alwan_map_simd_mul(vg, inv_sum)));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_prismatic) r = ALWAN_MAP_LITV(alwan_rgb_to_prismatic)(rgb);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.s; d2[i] = (alwan_map_lane)r.h;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_rgb_to_prismatic, _map_interleave)(alwan_map_lane *prismatic_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !prismatic_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_prismatic_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(prismatic_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__prismatic_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                            alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                            size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd eps = alwan_map_simd_set1((alwan_map_lane)(ALWAN_EPSILON));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd zero = alwan_map_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vs = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vh = alwan_map_simd_load(&c2[i]);
            /* R_comp = 1 - s - h */
            alwan_map_simd R_comp = alwan_map_simd_sub(alwan_map_simd_sub(one, vs), vh);
            alwan_map_simd max_pqr = ALWAN_MAP_NAME(alwan__simd_max3)(vs, vh, R_comp);
            alwan_map_simd_mask tiny = alwan_map_simd_cmplt(max_pqr, eps);
            alwan_map_simd sum_rgb = alwan_map_simd_select(tiny, zero, alwan_map_simd_div(vL, max_pqr));
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(vs, sum_rgb));
            alwan_map_simd_store(&d1[i], alwan_map_simd_mul(vh, sum_rgb));
            alwan_map_simd_store(&d2[i], alwan_map_simd_mul(R_comp, sum_rgb));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_prismatic) prismatic = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_prismatic_to_rgb)(prismatic);
        d0[i] = (alwan_map_lane)r.r; d1[i] = (alwan_map_lane)r.g; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_prismatic_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *prismatic_in, size_t in_stride, size_t count) {
    if (!prismatic_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, prismatic_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__prismatic_to_rgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HCL  (SIMD kernels)
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__rgb_to_hcl_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd const gamma_inv_Y0 = alwan_map_simd_set1((alwan_map_lane)(3.0 / 100.0));
        alwan_map_simd const eps   = alwan_map_simd_set1((alwan_map_lane)ALWAN_EPSILON);
        alwan_map_simd const zero  = alwan_map_simd_zero();
        alwan_map_simd const one   = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd const half  = alwan_map_simd_set1((alwan_map_lane)(0.5));
        alwan_map_simd const third = alwan_map_simd_set1((alwan_map_lane)(1.0 / 3.0));
        alwan_map_simd const two_third  = alwan_map_simd_set1((alwan_map_lane)(2.0 / 3.0));
        alwan_map_simd const four_third = alwan_map_simd_set1((alwan_map_lane)(4.0 / 3.0));
        alwan_map_simd const pi    = alwan_map_simd_set1((alwan_map_lane)ALWAN_PI);
        alwan_map_simd const half_pi     = alwan_map_simd_set1((alwan_map_lane)(ALWAN_PI * 0.5));
        alwan_map_simd const neg_half_pi = alwan_map_simd_set1((alwan_map_lane)(-ALWAN_PI * 0.5));
        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);

            /* Q = exp(min * gamma / (max * Y0)) when max > eps, else 1 */
            alwan_map_simd_mask mx_ok = alwan_map_simd_cmpgt(mx, eps);
            alwan_map_simd safe_mx = alwan_map_simd_select(mx_ok, mx, one);
            alwan_map_simd Q = alwan_map_simd_select(mx_ok,
                alwan_map_simd_exp(alwan_map_simd_mul(
                    alwan_map_simd_div(mn, safe_mx), gamma_inv_Y0)),
                one);

            /* L = (Q*max + (Q-1)*min) / 2 */
            alwan_map_simd L = alwan_map_simd_mul(half,
                alwan_map_simd_add(alwan_map_simd_mul(Q, mx),
                    alwan_map_simd_mul(alwan_map_simd_sub(Q, one), mn)));

            /* C = Q * (|r-g| + |g-b| + |b-r|) / 3 */
            alwan_map_simd r_g = alwan_map_simd_sub(vr, vg);
            alwan_map_simd g_b = alwan_map_simd_sub(vg, vb);
            alwan_map_simd b_r = alwan_map_simd_sub(vb, vr);
            alwan_map_simd C = alwan_map_simd_mul(Q, alwan_map_simd_mul(third,
                alwan_map_simd_add(alwan_map_simd_abs(r_g),
                    alwan_map_simd_add(alwan_map_simd_abs(g_b), alwan_map_simd_abs(b_r)))));

            /* h_temp = atan(g_b / r_g); fallback +/-pi/2 when r_g~0.
             * Must use atan (not atan2) -- the 4-way H correction below
             * assumes atan output range [-pi/2, pi/2]. */
            alwan_map_simd_mask rg_small = alwan_map_simd_cmplt(alwan_map_simd_abs(r_g), eps);
            alwan_map_simd ratio = alwan_map_simd_div(g_b, alwan_map_simd_select(rg_small, one, r_g));
            alwan_map_simd h_temp = alwan_map_simd_select(rg_small,
                alwan_map_simd_select(alwan_map_simd_cmpge(g_b, zero), half_pi, neg_half_pi),
                alwan_map_simd_atan2(ratio, one));

            alwan_map_simd two_h_3  = alwan_map_simd_mul(two_third,  h_temp);
            alwan_map_simd four_h_3 = alwan_map_simd_mul(four_third, h_temp);

            /* H based on signs of r_g and g_b (branchless 4-way select) */
            alwan_map_simd_mask rg_pos = alwan_map_simd_cmpge(r_g, zero);
            alwan_map_simd_mask gb_pos = alwan_map_simd_cmpge(g_b, zero);
            alwan_map_simd H_rg_pos = alwan_map_simd_select(gb_pos, two_h_3, four_h_3);
            alwan_map_simd H_rg_neg = alwan_map_simd_select(gb_pos,
                alwan_map_simd_add(pi, four_h_3),
                alwan_map_simd_sub(two_h_3, pi));
            alwan_map_simd H = alwan_map_simd_select(alwan_map_simd_cmpgt(C, eps),
                alwan_map_simd_select(rg_pos, H_rg_pos, H_rg_neg),
                zero);

            alwan_map_simd_store(&d0[i], H);
            alwan_map_simd_store(&d1[i], C);
            alwan_map_simd_store(&d2[i], L);
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_hcl) r = ALWAN_MAP_LITV(alwan_rgb_to_hcl)(rgb);
        d0[i] = (alwan_map_lane)r.H; d1[i] = (alwan_map_lane)r.C; d2[i] = (alwan_map_lane)r.L;
    }
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__hcl_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                      alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd const eps  = alwan_map_simd_set1((alwan_map_lane)ALWAN_EPSILON);
        alwan_map_simd const zero = alwan_map_simd_zero();
        alwan_map_simd const one  = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd const two  = alwan_map_simd_set1((alwan_map_lane)(2.0));
        alwan_map_simd const three = alwan_map_simd_set1((alwan_map_lane)(3.0));
        alwan_map_simd const four  = alwan_map_simd_set1((alwan_map_lane)(4.0));
        alwan_map_simd const gamma_inv_Y0 = alwan_map_simd_set1((alwan_map_lane)(3.0 / 100.0));
        alwan_map_simd const pi    = alwan_map_simd_set1((alwan_map_lane)ALWAN_PI);
        alwan_map_simd const r_p60  = alwan_map_simd_set1((alwan_map_lane)(ALWAN_PI / 3.0));
        alwan_map_simd const r_p120 = alwan_map_simd_set1((alwan_map_lane)(2.0 * ALWAN_PI / 3.0));
        alwan_map_simd const r_n60  = alwan_map_simd_set1((alwan_map_lane)(-ALWAN_PI / 3.0));
        alwan_map_simd const r_n120 = alwan_map_simd_set1((alwan_map_lane)(-2.0 * ALWAN_PI / 3.0));
        alwan_map_simd const three_half    = alwan_map_simd_set1((alwan_map_lane)(1.5));
        alwan_map_simd const three_quarter = alwan_map_simd_set1((alwan_map_lane)(0.75));
        for (; i + W <= n; i += W) {
            alwan_map_simd H = alwan_map_simd_load(&c0[i]);
            alwan_map_simd C = alwan_map_simd_load(&c1[i]);
            alwan_map_simd L = alwan_map_simd_load(&c2[i]);

            /* Q = exp((1 - 3C/(4L)) * gamma/Y0) when L > eps */
            alwan_map_simd_mask L_ok = alwan_map_simd_cmpgt(L, eps);
            alwan_map_simd safe_L = alwan_map_simd_select(L_ok, L, one);
            alwan_map_simd Q = alwan_map_simd_select(L_ok,
                alwan_map_simd_exp(alwan_map_simd_mul(
                    alwan_map_simd_sub(one, alwan_map_simd_div(
                        alwan_map_simd_mul(three, C),
                        alwan_map_simd_mul(four, safe_L))),
                    gamma_inv_Y0)),
                one);

            /* Min = (4L - 3C) / (4Q - 2) */
            alwan_map_simd denom = alwan_map_simd_sub(alwan_map_simd_mul(four, Q), two);
            alwan_map_simd_mask denom_ok = alwan_map_simd_cmpgt(alwan_map_simd_abs(denom), eps);
            alwan_map_simd Min = alwan_map_simd_select(L_ok,
                alwan_map_simd_select(denom_ok,
                    alwan_map_simd_div(
                        alwan_map_simd_sub(alwan_map_simd_mul(four, L), alwan_map_simd_mul(three, C)),
                        alwan_map_simd_select(denom_ok, denom, one)),
                    zero),
                zero);

            /* Max = Min + 3C/(2Q) */
            alwan_map_simd_mask Q_ok = alwan_map_simd_cmpgt(Q, eps);
            alwan_map_simd Max = alwan_map_simd_select(L_ok,
                alwan_map_simd_select(Q_ok,
                    alwan_map_simd_add(Min, alwan_map_simd_div(
                        alwan_map_simd_mul(three_half, C),
                        alwan_map_simd_select(Q_ok, Q, one))),
                    zero),
                zero);

            /* Sector 0 [0,60): tan(3H/2) */
            alwan_map_simd arg0 = alwan_map_simd_mul(three_half, H);
            alwan_map_simd s0 = alwan_map_simd_sin(arg0), c0_ = alwan_map_simd_cos(arg0);
            alwan_map_simd safe_c0 = alwan_map_simd_select(alwan_map_simd_cmpgt(alwan_map_simd_abs(c0_), eps), c0_, one);
            alwan_map_simd t0 = alwan_map_simd_div(s0, safe_c0);
            alwan_map_simd r0 = Max;
            alwan_map_simd g0 = alwan_map_simd_div(alwan_map_simd_add(alwan_map_simd_mul(Max, t0), Min),
                                            alwan_map_simd_add(one, t0));
            alwan_map_simd b0 = Min;

            /* Sectors 1,2 [60,180): tan(3(H-pi)/4) */
            alwan_map_simd arg1 = alwan_map_simd_mul(three_quarter, alwan_map_simd_sub(H, pi));
            alwan_map_simd s1 = alwan_map_simd_sin(arg1), c1_ = alwan_map_simd_cos(arg1);
            alwan_map_simd safe_c1 = alwan_map_simd_select(alwan_map_simd_cmpgt(alwan_map_simd_abs(c1_), eps), c1_, one);
            alwan_map_simd t1 = alwan_map_simd_div(s1, safe_c1);
            alwan_map_simd_mask t1_ok = alwan_map_simd_cmpgt(alwan_map_simd_abs(t1), eps);
            alwan_map_simd safe_t1 = alwan_map_simd_select(t1_ok, t1, one);

            alwan_map_simd r1 = alwan_map_simd_select(t1_ok,
                alwan_map_simd_div(alwan_map_simd_sub(
                    alwan_map_simd_mul(Max, alwan_map_simd_add(one, t1)), Min), safe_t1),
                Max);
            alwan_map_simd g1 = Max;
            alwan_map_simd b1 = Min;

            alwan_map_simd r2 = Min;
            alwan_map_simd g2 = Max;
            alwan_map_simd b2 = alwan_map_simd_sub(
                alwan_map_simd_mul(Max, alwan_map_simd_add(one, t1)),
                alwan_map_simd_mul(Min, t1));

            /* Sectors 3,4 [-120,0): tan(3H/4) */
            alwan_map_simd arg3 = alwan_map_simd_mul(three_quarter, H);
            alwan_map_simd s3 = alwan_map_simd_sin(arg3), c3_ = alwan_map_simd_cos(arg3);
            alwan_map_simd safe_c3 = alwan_map_simd_select(alwan_map_simd_cmpgt(alwan_map_simd_abs(c3_), eps), c3_, one);
            alwan_map_simd t3 = alwan_map_simd_div(s3, safe_c3);
            alwan_map_simd_mask t3_ok = alwan_map_simd_cmpgt(alwan_map_simd_abs(t3), eps);
            alwan_map_simd safe_t3 = alwan_map_simd_select(t3_ok, t3, one);

            alwan_map_simd r3 = Max;
            alwan_map_simd g3 = Min;
            alwan_map_simd b3 = alwan_map_simd_sub(
                alwan_map_simd_mul(Min, alwan_map_simd_add(one, t3)),
                alwan_map_simd_mul(Max, t3));

            alwan_map_simd r4 = alwan_map_simd_select(t3_ok,
                alwan_map_simd_div(alwan_map_simd_sub(
                    alwan_map_simd_mul(Min, alwan_map_simd_add(one, t3)), Max), safe_t3),
                Min);
            alwan_map_simd g4 = Min;
            alwan_map_simd b4 = Max;

            /* Sector 5 [-180,-120): tan(3(H+pi)/2) */
            alwan_map_simd arg5 = alwan_map_simd_mul(three_half, alwan_map_simd_add(H, pi));
            alwan_map_simd s5 = alwan_map_simd_sin(arg5), c5_ = alwan_map_simd_cos(arg5);
            alwan_map_simd safe_c5 = alwan_map_simd_select(alwan_map_simd_cmpgt(alwan_map_simd_abs(c5_), eps), c5_, one);
            alwan_map_simd t5 = alwan_map_simd_div(s5, safe_c5);
            alwan_map_simd r5 = Min;
            alwan_map_simd g5 = alwan_map_simd_div(alwan_map_simd_add(alwan_map_simd_mul(Min, t5), Max),
                                            alwan_map_simd_add(one, t5));
            alwan_map_simd b5 = Max;

            /* Branchless 6-way sector selection via nested selects */
            alwan_map_simd_mask h_ge_n120 = alwan_map_simd_cmpge(H, r_n120);
            alwan_map_simd r_45 = alwan_map_simd_select(h_ge_n120, r4, r5);
            alwan_map_simd g_45 = alwan_map_simd_select(h_ge_n120, g4, g5);
            alwan_map_simd b_45 = alwan_map_simd_select(h_ge_n120, b4, b5);

            alwan_map_simd_mask h_ge_n60 = alwan_map_simd_cmpge(H, r_n60);
            alwan_map_simd r_345 = alwan_map_simd_select(h_ge_n60, r3, r_45);
            alwan_map_simd g_345 = alwan_map_simd_select(h_ge_n60, g3, g_45);
            alwan_map_simd b_345 = alwan_map_simd_select(h_ge_n60, b3, b_45);

            alwan_map_simd_mask h_lt_120 = alwan_map_simd_cmplt(H, r_p120);
            alwan_map_simd r_12 = alwan_map_simd_select(h_lt_120, r1, r2);
            alwan_map_simd g_12 = alwan_map_simd_select(h_lt_120, g1, g2);
            alwan_map_simd b_12 = alwan_map_simd_select(h_lt_120, b1, b2);

            alwan_map_simd_mask h_lt_60 = alwan_map_simd_cmplt(H, r_p60);
            alwan_map_simd r_012 = alwan_map_simd_select(h_lt_60, r0, r_12);
            alwan_map_simd g_012 = alwan_map_simd_select(h_lt_60, g0, g_12);
            alwan_map_simd b_012 = alwan_map_simd_select(h_lt_60, b0, b_12);

            alwan_map_simd_mask h_pos = alwan_map_simd_cmpge(H, zero);
            alwan_map_simd_store(&d0[i], alwan_map_simd_select(h_pos, r_012, r_345));
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(h_pos, g_012, g_345));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(h_pos, b_012, b_345));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_hcl) hcl = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_hcl_to_rgb)(hcl);
        d0[i] = (alwan_map_lane)r.r; d1[i] = (alwan_map_lane)r.g; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_rgb_to_hcl, _map_interleave)(alwan_map_lane *hcl_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !hcl_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_hcl_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_P_NORM_AFFINE(d0, tile, 0.15915494309189533577, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(hcl_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_MAP_LITCAT(alwan_hcl_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *hcl_in, size_t in_stride, size_t count) {
    if (!hcl_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, hcl_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_AFFINE(c0, tile, 6.28318530717958647692, -3.14159265358979323846);
        ALWAN_MAP_LIT(alwan__hcl_to_rgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> IHLS  (SIMD kernels)
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__rgb_to_ihls_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                       alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M = ALWAN_MAP_LIT(ALWAN_EXT_IHLS_RGB_TO_YC1C2).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)M[0]);
        alwan_map_simd a01 = alwan_map_simd_set1((alwan_map_lane)M[1]);
        alwan_map_simd a02 = alwan_map_simd_set1((alwan_map_lane)M[2]);
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)M[3]);
        alwan_map_simd a11 = alwan_map_simd_set1((alwan_map_lane)M[4]);
        alwan_map_simd a12 = alwan_map_simd_set1((alwan_map_lane)M[5]);
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)M[6]);
        alwan_map_simd a21 = alwan_map_simd_set1((alwan_map_lane)M[7]);
        alwan_map_simd a22 = alwan_map_simd_set1((alwan_map_lane)M[8]);
        alwan_map_simd const eps    = alwan_map_simd_set1((alwan_map_lane)ALWAN_EPSILON);
        alwan_map_simd const zero   = alwan_map_simd_zero();
        alwan_map_simd const one    = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd const neg_one = alwan_map_simd_set1((alwan_map_lane)(-1.0));
        alwan_map_simd const two_pi = alwan_map_simd_set1((alwan_map_lane)(2.0 * ALWAN_PI));
        for (; i + W <= n; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd mx = ALWAN_MAP_NAME(alwan__simd_max3)(vr, vg, vb);
            alwan_map_simd mn = ALWAN_MAP_NAME(alwan__simd_min3)(vr, vg, vb);
            alwan_map_simd delta = alwan_map_simd_sub(mx, mn);

            /* Y, C1, C2 = M * [r,g,b] */
            alwan_map_simd Y  = alwan_map_simd_fmadd(a00, vr, alwan_map_simd_fmadd(a01, vg, alwan_map_simd_mul(a02, vb)));
            alwan_map_simd C1 = alwan_map_simd_fmadd(a10, vr, alwan_map_simd_fmadd(a11, vg, alwan_map_simd_mul(a12, vb)));
            alwan_map_simd C2 = alwan_map_simd_fmadd(a20, vr, alwan_map_simd_fmadd(a21, vg, alwan_map_simd_mul(a22, vb)));

            /* C_mag = sqrt(C1^2 + C2^2) */
            alwan_map_simd C_mag = alwan_map_simd_sqrt(alwan_map_simd_fmadd(C1, C1, alwan_map_simd_mul(C2, C2)));

            /* cosH = clamp(C1 / C_mag, -1, 1) */
            alwan_map_simd_mask mag_ok = alwan_map_simd_cmpgt(C_mag, eps);
            alwan_map_simd cosH = alwan_map_simd_select(mag_ok,
                alwan_map_simd_div(C1, alwan_map_simd_select(mag_ok, C_mag, one)), one);
            cosH = alwan_map_simd_clamp(cosH, neg_one, one);

            /* acos(cosH) = atan2(sqrt(1-cosH^2), cosH) */
            alwan_map_simd sinH = alwan_map_simd_sqrt(
                alwan_map_simd_sub(one, alwan_map_simd_mul(cosH, cosH)));
            alwan_map_simd H_temp = alwan_map_simd_atan2(sinH, cosH);

            /* If C2 > 0: H = 2pi - H_temp; else H = H_temp; achromatic: 0 */
            alwan_map_simd H = alwan_map_simd_select(mag_ok,
                alwan_map_simd_select(alwan_map_simd_cmple(C2, zero),
                    H_temp, alwan_map_simd_sub(two_pi, H_temp)),
                zero);

            alwan_map_simd_store(&d0[i], H);
            alwan_map_simd_store(&d1[i], Y);
            alwan_map_simd_store(&d2[i], delta);
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_ihls) r = ALWAN_MAP_LITV(alwan_rgb_to_ihls)(rgb);
        d0[i] = (alwan_map_lane)r.H; d1[i] = (alwan_map_lane)r.L; d2[i] = (alwan_map_lane)r.S;
    }
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__ihls_to_rgb_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                       alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_lane const *M = ALWAN_MAP_LIT(ALWAN_EXT_IHLS_YC1C2_TO_RGB).m;
        alwan_map_simd a00 = alwan_map_simd_set1((alwan_map_lane)M[0]);
        alwan_map_simd a01 = alwan_map_simd_set1((alwan_map_lane)M[1]);
        alwan_map_simd a02 = alwan_map_simd_set1((alwan_map_lane)M[2]);
        alwan_map_simd a10 = alwan_map_simd_set1((alwan_map_lane)M[3]);
        alwan_map_simd a11 = alwan_map_simd_set1((alwan_map_lane)M[4]);
        alwan_map_simd a12 = alwan_map_simd_set1((alwan_map_lane)M[5]);
        alwan_map_simd a20 = alwan_map_simd_set1((alwan_map_lane)M[6]);
        alwan_map_simd a21 = alwan_map_simd_set1((alwan_map_lane)M[7]);
        alwan_map_simd a22 = alwan_map_simd_set1((alwan_map_lane)M[8]);
        alwan_map_simd const eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_EPSILON);
        alwan_map_simd const zero = alwan_map_simd_zero();
        alwan_map_simd const pi_3     = alwan_map_simd_set1((alwan_map_lane)(ALWAN_PI / 3.0));
        alwan_map_simd const two_pi_3 = alwan_map_simd_set1((alwan_map_lane)(2.0 * ALWAN_PI / 3.0));
        alwan_map_simd const sqrt3_half = alwan_map_simd_set1((alwan_map_lane)0.86602540378443864676);
        for (; i + W <= n; i += W) {
            alwan_map_simd H = alwan_map_simd_load(&c0[i]);
            alwan_map_simd Y = alwan_map_simd_load(&c1[i]);
            alwan_map_simd S = alwan_map_simd_load(&c2[i]);

            /* k = floor(H / (pi/3)), H_s = H - k*(pi/3) */
            alwan_map_simd k = alwan_map_simd_floor(alwan_map_simd_div(H, pi_3));
            alwan_map_simd H_s = alwan_map_simd_sub(H, alwan_map_simd_mul(k, pi_3));

            /* sin_val = sin(2pi/3 - H_s) */
            alwan_map_simd sin_val = alwan_map_simd_sin(alwan_map_simd_sub(two_pi_3, H_s));

            /* C_mag = sqrt(3)*S / (2*sin_val) when |sin_val| >= eps */
            alwan_map_simd_mask sin_ok = alwan_map_simd_cmpge(alwan_map_simd_abs(sin_val), eps);
            alwan_map_simd C_mag = alwan_map_simd_select(sin_ok,
                alwan_map_simd_div(alwan_map_simd_mul(sqrt3_half, S),
                    alwan_map_simd_select(sin_ok, sin_val, alwan_map_simd_set1((alwan_map_lane)(1.0)))),
                zero);

            /* C1 = C_mag*cos(H), C2 = -C_mag*sin(H) */
            alwan_map_simd C1 = alwan_map_simd_mul(C_mag, alwan_map_simd_cos(H));
            alwan_map_simd C2 = alwan_map_simd_neg(alwan_map_simd_mul(C_mag, alwan_map_simd_sin(H)));

            /* RGB = M_inv * [Y, C1, C2] */
            alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(a00, Y, alwan_map_simd_fmadd(a01, C1, alwan_map_simd_mul(a02, C2))));
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(a10, Y, alwan_map_simd_fmadd(a11, C1, alwan_map_simd_mul(a12, C2))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(a20, Y, alwan_map_simd_fmadd(a21, C1, alwan_map_simd_mul(a22, C2))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_ihls) ihls = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_ihls_to_rgb)(ihls);
        d0[i] = (alwan_map_lane)r.r; d1[i] = (alwan_map_lane)r.g; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_rgb_to_ihls, _map_interleave)(alwan_map_lane *ihls_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !ihls_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__rgb_to_ihls_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_P_NORM_MUL(d0, tile, 0.15915494309189533577);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(ihls_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_MAP_LITCAT(alwan_ihls_to_rgb, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *ihls_in, size_t in_stride, size_t count) {
    if (!ihls_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, ihls_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_MUL(c0, tile, 6.28318530717958647692);
        ALWAN_MAP_LIT(alwan__ihls_to_rgb_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Lab <-> DIN99 (with int variant)
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__lab_to_din99_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                        size_t n, int variant) {
    /* Precompute variant-dependent constants */
    int v = (variant < 0) ? 0 : (variant > 3) ? 3 : variant;
    alwan_map_lane c3_rad = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][2] * ALWAN_PI / 180.0);
    alwan_map_lane c7_rad = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][6] * ALWAN_PI / 180.0);
    alwan_map_lane cos_c3 = (alwan_map_lane)(alwan_map_scalar_cos(c3_rad));
    alwan_map_lane sin_c3 = (alwan_map_lane)(alwan_map_scalar_sin(c3_rad));
    /* Used only inside the SIMD branch; silence MSVC C4189 on Windows ARM. */
    (void)c7_rad; (void)cos_c3; (void)sin_c3;
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W_s = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd v_c0 = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][0]));
        alwan_map_simd v_c1 = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][1]));
        alwan_map_simd v_c3f = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][3]));
        alwan_map_simd v_c4 = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][4]));
        alwan_map_simd v_c5 = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][5]));
        alwan_map_simd v_c7_scale = alwan_map_simd_set1((alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][7]));
        alwan_map_simd v_cos = alwan_map_simd_set1((alwan_map_lane)(cos_c3));
        alwan_map_simd v_sin = alwan_map_simd_set1((alwan_map_lane)(sin_c3));
        alwan_map_simd v_nsin = alwan_map_simd_set1((alwan_map_lane)(-sin_c3));
        alwan_map_simd v_c7r = alwan_map_simd_set1((alwan_map_lane)(c7_rad));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        alwan_map_simd zero = alwan_map_simd_zero();
        alwan_map_simd chroma_guard = alwan_map_simd_set1((alwan_map_lane)(1e-12));
        for (; i + W_s <= n; i += W_s) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            /* L99 = c0 * ln(1 + c1*L) */
            alwan_map_simd L99 = alwan_map_simd_mul(v_c0, alwan_map_simd_log(alwan_map_simd_fmadd(v_c1, vL, one)));
            /* Achromatic guard */
            alwan_map_simd chroma_sq = alwan_map_simd_fmadd(va, va, alwan_map_simd_mul(vb, vb));
            alwan_map_simd_mask achromatic = alwan_map_simd_cmplt(chroma_sq, chroma_guard);
            /* e = cos*a + sin*b; f = c3f * (-sin*a + cos*b) */
            alwan_map_simd e = alwan_map_simd_fmadd(v_cos, va, alwan_map_simd_mul(v_sin, vb));
            alwan_map_simd f = alwan_map_simd_mul(v_c3f, alwan_map_simd_fmadd(v_nsin, va, alwan_map_simd_mul(v_cos, vb)));
            /* G = sqrt(e*e + f*f) */
            alwan_map_simd G = alwan_map_simd_sqrt(alwan_map_simd_fmadd(e, e, alwan_map_simd_mul(f, f)));
            /* h_ef = atan2(f, e) + c7_rad */
            alwan_map_simd h_ef = alwan_map_simd_add(alwan_map_simd_atan2(f, e), v_c7r);
            /* C99 = c4 * ln(1 + c5*G) / c7_scale */
            alwan_map_simd C99 = alwan_map_simd_div(alwan_map_simd_mul(v_c4, alwan_map_simd_log(alwan_map_simd_fmadd(v_c5, G, one))), v_c7_scale);
            /* a99, b99 with achromatic guard */
            alwan_map_simd_store(&d0[i], L99);
            alwan_map_simd_store(&d1[i], alwan_map_simd_select(achromatic, zero, alwan_map_simd_mul(C99, alwan_map_simd_cos(h_ef))));
            alwan_map_simd_store(&d2[i], alwan_map_simd_select(achromatic, zero, alwan_map_simd_mul(C99, alwan_map_simd_sin(h_ef))));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_lab) lab = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_din99) r = ALWAN_MAP_LITV(alwan_lab_to_din99)(lab, variant);
        d0[i] = (alwan_map_lane)r.L99; d1[i] = (alwan_map_lane)r.a99; d2[i] = (alwan_map_lane)r.b99;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_lab_to_din99, _map_interleave)(alwan_map_lane *din99_out, size_t out_stride, alwan_map_lane const *lab_in, size_t in_stride, size_t count, int variant) {
    if (!lab_in || !din99_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, lab_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__lab_to_din99_kernel)(d0, d1, d2, c0, c1, c2, tile, variant);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(din99_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__din99_to_lab_kernel)(alwan_map_lane *d0, alwan_map_lane *d1, alwan_map_lane *d2,
                                        alwan_map_lane const *c0, alwan_map_lane const *c1, alwan_map_lane const *c2,
                                        size_t n, int variant) {
    /* Precompute variant-dependent constants */
    int v = (variant < 0) ? 0 : (variant > 3) ? 3 : variant;
    alwan_map_lane c3_rad = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][2] * ALWAN_PI / 180.0);
    alwan_map_lane c7_rad = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][6] * ALWAN_PI / 180.0);
    alwan_map_lane cos_c3 = (alwan_map_lane)(alwan_map_scalar_cos(c3_rad));
    alwan_map_lane sin_c3 = (alwan_map_lane)(alwan_map_scalar_sin(c3_rad));
    alwan_map_lane inv_c3f = (alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][3]);
    alwan_map_lane c7_over_c4 = (alwan_map_lane)(ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][7] / ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][4]);
    alwan_map_lane inv_c5 = (alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][5]);
    alwan_map_lane inv_c0 = (alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][0]);
    alwan_map_lane inv_c1 = (alwan_map_lane)(1.0 / ALWAN_MAP_LIT(ALWAN_DIN99_COEFFS)[v][1]);
    /* These constants feed only the SIMD branch below; reference them
     * unconditionally so MSVC /WX builds with ALWAN_MAP_SIMD_WIDTH == 1
     * (e.g. Windows ARM) don't fail on C4189. */
    (void)c7_rad; (void)cos_c3; (void)sin_c3; (void)inv_c3f;
    (void)c7_over_c4; (void)inv_c5; (void)inv_c1; (void)inv_c0;
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    {
        size_t const W_s = ALWAN_MAP_SIMD_WIDTH;
        alwan_map_simd v_c7r = alwan_map_simd_set1((alwan_map_lane)(c7_rad));
        alwan_map_simd v_c7_c4 = alwan_map_simd_set1((alwan_map_lane)(c7_over_c4));
        alwan_map_simd v_inv_c5 = alwan_map_simd_set1((alwan_map_lane)(inv_c5));
        alwan_map_simd v_cos = alwan_map_simd_set1((alwan_map_lane)(cos_c3));
        alwan_map_simd v_sin = alwan_map_simd_set1((alwan_map_lane)(sin_c3));
        alwan_map_simd v_nsin = alwan_map_simd_set1((alwan_map_lane)(-sin_c3));
        alwan_map_simd v_inv_c3f = alwan_map_simd_set1((alwan_map_lane)(inv_c3f));
        alwan_map_simd v_inv_c0 = alwan_map_simd_set1((alwan_map_lane)(inv_c0));
        alwan_map_simd v_inv_c1 = alwan_map_simd_set1((alwan_map_lane)(inv_c1));
        alwan_map_simd one = alwan_map_simd_set1((alwan_map_lane)(1.0));
        for (; i + W_s <= n; i += W_s) {
            alwan_map_simd vL99 = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va99 = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb99 = alwan_map_simd_load(&c2[i]);
            /* h99 = atan2(b99, a99) - c7_rad */
            alwan_map_simd h99 = alwan_map_simd_sub(alwan_map_simd_atan2(vb99, va99), v_c7r);
            /* C99 = sqrt(a99^2 + b99^2) */
            alwan_map_simd C99 = alwan_map_simd_sqrt(alwan_map_simd_fmadd(va99, va99, alwan_map_simd_mul(vb99, vb99)));
            /* G = (exp(c7/c4 * C99) - 1) / c5 */
            alwan_map_simd G = alwan_map_simd_mul(alwan_map_simd_sub(alwan_map_simd_exp(alwan_map_simd_mul(v_c7_c4, C99)), one), v_inv_c5);
            /* e = G*cos(h99), f = G*sin(h99) */
            alwan_map_simd e = alwan_map_simd_mul(G, alwan_map_simd_cos(h99));
            alwan_map_simd f = alwan_map_simd_mul(G, alwan_map_simd_sin(h99));
            /* a = e*cos_c3 - (f/c3f)*sin_c3; b = e*sin_c3 + (f/c3f)*cos_c3 */
            alwan_map_simd f_scaled = alwan_map_simd_mul(f, v_inv_c3f);
            alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(e, v_cos, alwan_map_simd_mul(f_scaled, v_nsin)));
            alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(e, v_sin, alwan_map_simd_mul(f_scaled, v_cos)));
            /* L = (exp(L99/c0) - 1) / c1 */
            alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_sub(alwan_map_simd_exp(alwan_map_simd_mul(vL99, v_inv_c0)), one), v_inv_c1));
        }
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_din99) din99 = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
        ALWAN_MAP_LIT(alwan_lab) r = ALWAN_MAP_LITV(alwan_din99_to_lab)(din99, variant);
        d0[i] = (alwan_map_lane)r.L; d1[i] = (alwan_map_lane)r.a; d2[i] = (alwan_map_lane)r.b;
    }
}

alwan_status ALWAN_MAP_LITCAT(alwan_din99_to_lab, _map_interleave)(alwan_map_lane *lab_out, size_t out_stride, alwan_map_lane const *din99_in, size_t in_stride, size_t count, int variant) {
    if (!din99_in || !lab_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, din99_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__din99_to_lab_kernel)(d0, d1, d2, c0, c1, c2, tile, variant);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(lab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants (tiled, reusing SIMD kernels)
 * ================================================================ */

/* --- Simple kernel planar functions (no extra params) --- */

#define ALWAN_MAP_P_PLANAR_FROM_KERNEL(name, kernel) \
alwan_status name(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, \
         alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, \
         size_t count) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, kernel); \
    return ALWAN_OK; \
}

/* XYZ <-> IgPgTg */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_igpgtg, _map_planar),  ALWAN_MAP_LIT(alwan__xyz_to_igpgtg_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_igpgtg_to_xyz, _map_planar),  ALWAN_MAP_LIT(alwan__igpgtg_to_xyz_kernel))

/* XYZ <-> ICaCb */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_icacb, _map_planar),   ALWAN_MAP_LIT(alwan__xyz_to_icacb_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_icacb_to_xyz, _map_planar),   ALWAN_MAP_LIT(alwan__icacb_to_xyz_kernel))

/* XYZ <-> hdr-CIELAB */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_hdr_cielab, _map_planar), ALWAN_MAP_LIT(alwan__xyz_to_hdr_cielab_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_hdr_cielab_to_xyz, _map_planar), ALWAN_MAP_LIT(alwan__hdr_cielab_to_xyz_kernel))

/* XYZ <-> hdr-IPT */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_hdr_ipt, _map_planar), ALWAN_MAP_LIT(alwan__xyz_to_hdr_ipt_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_hdr_ipt_to_xyz, _map_planar), ALWAN_MAP_LIT(alwan__hdr_ipt_to_xyz_kernel))

/* XYZ <-> UCS */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_ucs, _map_planar),     ALWAN_MAP_LIT(alwan__xyz_to_ucs_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_ucs_to_xyz, _map_planar),     ALWAN_MAP_LIT(alwan__ucs_to_xyz_kernel))

/* XYZ <-> OSA-UCS */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_osa_ucs, _map_planar), ALWAN_MAP_LIT(alwan__xyz_to_osa_ucs_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_osa_ucs_to_xyz, _map_planar), ALWAN_MAP_LIT(alwan__osa_ucs_to_xyz_kernel))

/* XYZ <-> Hunter Lab */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_hunter_lab, _map_planar),  ALWAN_MAP_LIT(alwan__xyz_to_hunter_lab_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_hunter_lab_to_xyz, _map_planar),  ALWAN_MAP_LIT(alwan__hunter_lab_to_xyz_kernel))

/* XYZ <-> ProLab */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_xyz_to_prolab, _map_planar),  ALWAN_MAP_LIT(alwan__xyz_to_prolab_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_prolab_to_xyz, _map_planar),  ALWAN_MAP_LIT(alwan__prolab_to_xyz_kernel))

/* RGB <-> Prismatic */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_rgb_to_prismatic, _map_planar),  ALWAN_MAP_LIT(alwan__rgb_to_prismatic_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_prismatic_to_rgb, _map_planar),  ALWAN_MAP_LIT(alwan__prismatic_to_rgb_kernel))

/* RGB <-> HCL */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_rgb_to_hcl, _map_planar),  ALWAN_MAP_LIT(alwan__rgb_to_hcl_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_hcl_to_rgb, _map_planar),  ALWAN_MAP_LIT(alwan__hcl_to_rgb_kernel))

/* RGB <-> IHLS */
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_rgb_to_ihls, _map_planar), ALWAN_MAP_LIT(alwan__rgb_to_ihls_kernel))
ALWAN_MAP_P_PLANAR_FROM_KERNEL(ALWAN_MAP_LITCAT(alwan_ihls_to_rgb, _map_planar), ALWAN_MAP_LIT(alwan__ihls_to_rgb_kernel))

#undef ALWAN_MAP_P_PLANAR_FROM_KERNEL

/* --- Extra-param planar functions (white_xyz / variant) --- */

#define ALWAN_MAP_P_PLANAR_TILED_WHITE(name, kernel) \
alwan_status name(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, \
         alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, \
         size_t count, \
         ALWAN_MAP_LIT(alwan_xyz) const *white_xyz) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    size_t off_ = 0; \
    while (off_ < count) { \
        size_t tile_ = count - off_; \
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS; \
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS]; \
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS]; \
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_); \
        kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, white_xyz); \
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_); \
        off_ += tile_; \
    } \
    return ALWAN_OK; \
}

/* XYZ <-> Hunter Lab custom */
ALWAN_MAP_P_PLANAR_TILED_WHITE(ALWAN_MAP_LITCAT(alwan_xyz_to_hunter_lab_custom, _map_planar),  ALWAN_MAP_LIT(alwan__xyz_to_hunter_lab_custom_kernel))
ALWAN_MAP_P_PLANAR_TILED_WHITE(ALWAN_MAP_LITCAT(alwan_hunter_lab_to_xyz_custom, _map_planar),  ALWAN_MAP_LIT(alwan__hunter_lab_to_xyz_custom_kernel))

/* XYZ <-> ProLab custom */
ALWAN_MAP_P_PLANAR_TILED_WHITE(ALWAN_MAP_LITCAT(alwan_xyz_to_prolab_custom, _map_planar),  ALWAN_MAP_LIT(alwan__xyz_to_prolab_custom_kernel))
ALWAN_MAP_P_PLANAR_TILED_WHITE(ALWAN_MAP_LITCAT(alwan_prolab_to_xyz_custom, _map_planar),  ALWAN_MAP_LIT(alwan__prolab_to_xyz_custom_kernel))

/* XYZ <-> UVW */
ALWAN_MAP_P_PLANAR_TILED_WHITE(ALWAN_MAP_LITCAT(alwan_xyz_to_uvw, _map_planar), ALWAN_MAP_LIT(alwan__xyz_to_uvw_kernel))
ALWAN_MAP_P_PLANAR_TILED_WHITE(ALWAN_MAP_LITCAT(alwan_uvw_to_xyz, _map_planar), ALWAN_MAP_LIT(alwan__uvw_to_xyz_kernel))

#undef ALWAN_MAP_P_PLANAR_TILED_WHITE

/* Lab <-> DIN99 (int variant param) */

#define ALWAN_MAP_P_PLANAR_TILED_INT(name, kernel) \
alwan_status name(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, \
         alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, \
         size_t count, \
         int variant) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    size_t off_ = 0; \
    while (off_ < count) { \
        size_t tile_ = count - off_; \
        if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS; \
        ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS]; \
        ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS]; \
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_); \
        kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, variant); \
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_); \
        off_ += tile_; \
    } \
    return ALWAN_OK; \
}

ALWAN_MAP_P_PLANAR_TILED_INT(ALWAN_MAP_LITCAT(alwan_lab_to_din99, _map_planar), ALWAN_MAP_LIT(alwan__lab_to_din99_kernel))
ALWAN_MAP_P_PLANAR_TILED_INT(ALWAN_MAP_LITCAT(alwan_din99_to_lab, _map_planar), ALWAN_MAP_LIT(alwan__din99_to_lab_kernel))

#undef ALWAN_MAP_P_PLANAR_TILED_INT
