/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Extended Color Space Conversions
 * IgPgTg, ICaCb, hdr-CIELAB, hdr-IPT, UCS, UVW, Hunter Lab, ProLab,
 * OSA-UCS, Prismatic, HCL, IHLS, DIN99
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_extended_core.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_din99_core.h"
#include "../core/alwan_hunter_lab_core.h"
#include "../core/alwan_prolab_core.h"
#include "../core/alwan_osa_ucs_core.h"

/* ----------------------------------------------------------------
 * XYZ <-> IgPgTg
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_igpgtg_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                         alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M1 = ALWAN_EXT_XYZ_TO_LMS_IGPGTG.m;
        alwan_scalar const *M2 = ALWAN_EXT_LMS_TO_IGPGTG.m;
        alwan_simd a00 = alwan_simd_set1(M1[0]), a01 = alwan_simd_set1(M1[1]), a02 = alwan_simd_set1(M1[2]);
        alwan_simd a10 = alwan_simd_set1(M1[3]), a11 = alwan_simd_set1(M1[4]), a12 = alwan_simd_set1(M1[5]);
        alwan_simd a20 = alwan_simd_set1(M1[6]), a21 = alwan_simd_set1(M1[7]), a22 = alwan_simd_set1(M1[8]);
        alwan_simd b00 = alwan_simd_set1(M2[0]), b01 = alwan_simd_set1(M2[1]), b02 = alwan_simd_set1(M2[2]);
        alwan_simd b10 = alwan_simd_set1(M2[3]), b11 = alwan_simd_set1(M2[4]), b12 = alwan_simd_set1(M2[5]);
        alwan_simd b20 = alwan_simd_set1(M2[6]), b21 = alwan_simd_set1(M2[7]), b22 = alwan_simd_set1(M2[8]);
        alwan_simd inv_s0 = alwan_simd_set1(1.0 / ALWAN_EXT_IGPGTG_LMS_SCALE[0]);
        alwan_simd inv_s1 = alwan_simd_set1(1.0 / ALWAN_EXT_IGPGTG_LMS_SCALE[1]);
        alwan_simd inv_s2 = alwan_simd_set1(1.0 / ALWAN_EXT_IGPGTG_LMS_SCALE[2]);
        alwan_simd exp_v = alwan_simd_set1(0.427);
        alwan_simd zero = alwan_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            alwan_simd L = alwan_simd_fmadd(a00, vx, alwan_simd_fmadd(a01, vy, alwan_simd_mul(a02, vz)));
            alwan_simd M = alwan_simd_fmadd(a10, vx, alwan_simd_fmadd(a11, vy, alwan_simd_mul(a12, vz)));
            alwan_simd S = alwan_simd_fmadd(a20, vx, alwan_simd_fmadd(a21, vy, alwan_simd_mul(a22, vz)));
            alwan_simd Ls = alwan_simd_mul(L, inv_s0);
            alwan_simd Ms = alwan_simd_mul(M, inv_s1);
            alwan_simd Ss = alwan_simd_mul(S, inv_s2);
            alwan_simd absL = alwan_simd_abs(Ls), absM = alwan_simd_abs(Ms), absS = alwan_simd_abs(Ss);
            alwan_simd pL = alwan_simd_pow(absL, exp_v);
            alwan_simd pM = alwan_simd_pow(absM, exp_v);
            alwan_simd pS = alwan_simd_pow(absS, exp_v);
            alwan_simd_mask posL = alwan_simd_cmpge(Ls, zero);
            alwan_simd_mask posM = alwan_simd_cmpge(Ms, zero);
            alwan_simd_mask posS = alwan_simd_cmpge(Ss, zero);
            alwan_simd sL = alwan_simd_select(posL, pL, alwan_simd_neg(pL));
            alwan_simd sM = alwan_simd_select(posM, pM, alwan_simd_neg(pM));
            alwan_simd sS = alwan_simd_select(posS, pS, alwan_simd_neg(pS));
            alwan_simd_store(&d0[i], alwan_simd_fmadd(b00, sL, alwan_simd_fmadd(b01, sM, alwan_simd_mul(b02, sS))));
            alwan_simd_store(&d1[i], alwan_simd_fmadd(b10, sL, alwan_simd_fmadd(b11, sM, alwan_simd_mul(b12, sS))));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(b20, sL, alwan_simd_fmadd(b21, sM, alwan_simd_mul(b22, sS))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_igpgtg r = alwan_xyz_to_igpgtg_v(xyz);
        d0[i] = (alwan_simd_lane)r.Ig; d1[i] = (alwan_simd_lane)r.Pg; d2[i] = (alwan_simd_lane)r.Tg;
    }
}

int alwan_xyz_to_igpgtg_map_interleave(alwan_scalar *igpgtg_out, alwan_scalar const *xyz_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !igpgtg_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_igpgtg_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(igpgtg_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__igpgtg_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                         alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M1 = ALWAN_EXT_IGPGTG_TO_LMS.m;
        alwan_scalar const *M2 = ALWAN_EXT_LMS_TO_XYZ_IGPGTG.m;
        alwan_simd a00 = alwan_simd_set1(M1[0]), a01 = alwan_simd_set1(M1[1]), a02 = alwan_simd_set1(M1[2]);
        alwan_simd a10 = alwan_simd_set1(M1[3]), a11 = alwan_simd_set1(M1[4]), a12 = alwan_simd_set1(M1[5]);
        alwan_simd a20 = alwan_simd_set1(M1[6]), a21 = alwan_simd_set1(M1[7]), a22 = alwan_simd_set1(M1[8]);
        alwan_simd b00 = alwan_simd_set1(M2[0]), b01 = alwan_simd_set1(M2[1]), b02 = alwan_simd_set1(M2[2]);
        alwan_simd b10 = alwan_simd_set1(M2[3]), b11 = alwan_simd_set1(M2[4]), b12 = alwan_simd_set1(M2[5]);
        alwan_simd b20 = alwan_simd_set1(M2[6]), b21 = alwan_simd_set1(M2[7]), b22 = alwan_simd_set1(M2[8]);
        alwan_simd s0 = alwan_simd_set1(ALWAN_EXT_IGPGTG_LMS_SCALE[0]);
        alwan_simd s1 = alwan_simd_set1(ALWAN_EXT_IGPGTG_LMS_SCALE[1]);
        alwan_simd s2 = alwan_simd_set1(ALWAN_EXT_IGPGTG_LMS_SCALE[2]);
        alwan_simd inv_exp = alwan_simd_set1(1.0 / 0.427);
        alwan_simd zero = alwan_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_simd vIg = alwan_simd_load(&c0[i]);
            alwan_simd vPg = alwan_simd_load(&c1[i]);
            alwan_simd vTg = alwan_simd_load(&c2[i]);
            /* mat3: IgPgTg -> LMS */
            alwan_simd L = alwan_simd_fmadd(a00, vIg, alwan_simd_fmadd(a01, vPg, alwan_simd_mul(a02, vTg)));
            alwan_simd M = alwan_simd_fmadd(a10, vIg, alwan_simd_fmadd(a11, vPg, alwan_simd_mul(a12, vTg)));
            alwan_simd S = alwan_simd_fmadd(a20, vIg, alwan_simd_fmadd(a21, vPg, alwan_simd_mul(a22, vTg)));
            /* spow(LMS, 1/0.427) * scale */
            alwan_simd absL = alwan_simd_abs(L), absM = alwan_simd_abs(M), absS = alwan_simd_abs(S);
            alwan_simd pL = alwan_simd_pow(absL, inv_exp);
            alwan_simd pM = alwan_simd_pow(absM, inv_exp);
            alwan_simd pS = alwan_simd_pow(absS, inv_exp);
            alwan_simd_mask posL = alwan_simd_cmpge(L, zero);
            alwan_simd_mask posM = alwan_simd_cmpge(M, zero);
            alwan_simd_mask posS = alwan_simd_cmpge(S, zero);
            alwan_simd sL = alwan_simd_mul(s0, alwan_simd_select(posL, pL, alwan_simd_neg(pL)));
            alwan_simd sM = alwan_simd_mul(s1, alwan_simd_select(posM, pM, alwan_simd_neg(pM)));
            alwan_simd sS = alwan_simd_mul(s2, alwan_simd_select(posS, pS, alwan_simd_neg(pS)));
            /* mat3: LMS -> XYZ */
            alwan_simd_store(&d0[i], alwan_simd_fmadd(b00, sL, alwan_simd_fmadd(b01, sM, alwan_simd_mul(b02, sS))));
            alwan_simd_store(&d1[i], alwan_simd_fmadd(b10, sL, alwan_simd_fmadd(b11, sM, alwan_simd_mul(b12, sS))));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(b20, sL, alwan_simd_fmadd(b21, sM, alwan_simd_mul(b22, sS))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_igpgtg igpgtg = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_igpgtg_to_xyz_v(igpgtg);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_igpgtg_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *igpgtg_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!igpgtg_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, igpgtg_in, processed, in_stride, tile);
        alwan__igpgtg_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> ICaCb
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_icacb_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                        alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M1 = ALWAN_EXT_XYZ_TO_LMS_ICACB.m;
        alwan_scalar const *M2 = ALWAN_EXT_LMS_TO_ICACB.m;
        alwan_simd a00 = alwan_simd_set1(M1[0]), a01 = alwan_simd_set1(M1[1]), a02 = alwan_simd_set1(M1[2]);
        alwan_simd a10 = alwan_simd_set1(M1[3]), a11 = alwan_simd_set1(M1[4]), a12 = alwan_simd_set1(M1[5]);
        alwan_simd a20 = alwan_simd_set1(M1[6]), a21 = alwan_simd_set1(M1[7]), a22 = alwan_simd_set1(M1[8]);
        alwan_simd b00 = alwan_simd_set1(M2[0]), b01 = alwan_simd_set1(M2[1]), b02 = alwan_simd_set1(M2[2]);
        alwan_simd b10 = alwan_simd_set1(M2[3]), b11 = alwan_simd_set1(M2[4]), b12 = alwan_simd_set1(M2[5]);
        alwan_simd b20 = alwan_simd_set1(M2[6]), b21 = alwan_simd_set1(M2[7]), b22 = alwan_simd_set1(M2[8]);
        /* PQ constants */
        alwan_simd inv_Lp = alwan_simd_set1(1.0 / 10000.0);
        alwan_simd pq_m1 = alwan_simd_set1(0.1593017578125);
        alwan_simd pq_m2 = alwan_simd_set1(78.84375);
        alwan_simd pq_c1 = alwan_simd_set1(0.8359375);
        alwan_simd pq_c2 = alwan_simd_set1(18.8515625);
        alwan_simd pq_c3 = alwan_simd_set1(18.6875);
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            /* mat3: XYZ -> LMS */
            alwan_simd L = alwan_simd_fmadd(a00, vx, alwan_simd_fmadd(a01, vy, alwan_simd_mul(a02, vz)));
            alwan_simd M = alwan_simd_fmadd(a10, vx, alwan_simd_fmadd(a11, vy, alwan_simd_mul(a12, vz)));
            alwan_simd S = alwan_simd_fmadd(a20, vx, alwan_simd_fmadd(a21, vy, alwan_simd_mul(a22, vz)));
            /* PQ inverse EOTF per channel: N = pow((c1 + c2*Y_p)/(c3*Y_p + 1), m2) where Y_p = pow(C/Lp, m1) */
            #define PQ_INV(ch) { \
                alwan_simd Yp = alwan_simd_pow(alwan_simd_mul(ch, inv_Lp), pq_m1); \
                alwan_simd num = alwan_simd_fmadd(pq_c2, Yp, pq_c1); \
                alwan_simd den = alwan_simd_fmadd(pq_c3, Yp, one); \
                ch = alwan_simd_pow(alwan_simd_div(num, den), pq_m2); \
            }
            PQ_INV(L) PQ_INV(M) PQ_INV(S)
            #undef PQ_INV
            /* mat3: LMS -> ICaCb */
            alwan_simd_store(&d0[i], alwan_simd_fmadd(b00, L, alwan_simd_fmadd(b01, M, alwan_simd_mul(b02, S))));
            alwan_simd_store(&d1[i], alwan_simd_fmadd(b10, L, alwan_simd_fmadd(b11, M, alwan_simd_mul(b12, S))));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(b20, L, alwan_simd_fmadd(b21, M, alwan_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_icacb r = alwan_xyz_to_icacb_v(xyz);
        d0[i] = (alwan_simd_lane)r.I; d1[i] = (alwan_simd_lane)r.Ca; d2[i] = (alwan_simd_lane)r.Cb;
    }
}

int alwan_xyz_to_icacb_map_interleave(alwan_scalar *icacb_out, alwan_scalar const *xyz_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !icacb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_icacb_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(icacb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__icacb_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                        alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M1 = ALWAN_EXT_ICACB_TO_LMS.m;
        alwan_scalar const *M2 = ALWAN_EXT_LMS_TO_XYZ_ICACB.m;
        alwan_simd a00 = alwan_simd_set1(M1[0]), a01 = alwan_simd_set1(M1[1]), a02 = alwan_simd_set1(M1[2]);
        alwan_simd a10 = alwan_simd_set1(M1[3]), a11 = alwan_simd_set1(M1[4]), a12 = alwan_simd_set1(M1[5]);
        alwan_simd a20 = alwan_simd_set1(M1[6]), a21 = alwan_simd_set1(M1[7]), a22 = alwan_simd_set1(M1[8]);
        alwan_simd b00 = alwan_simd_set1(M2[0]), b01 = alwan_simd_set1(M2[1]), b02 = alwan_simd_set1(M2[2]);
        alwan_simd b10 = alwan_simd_set1(M2[3]), b11 = alwan_simd_set1(M2[4]), b12 = alwan_simd_set1(M2[5]);
        alwan_simd b20 = alwan_simd_set1(M2[6]), b21 = alwan_simd_set1(M2[7]), b22 = alwan_simd_set1(M2[8]);
        /* PQ EOTF constants */
        alwan_simd pq_Lp = alwan_simd_set1(10000.0);
        alwan_simd pq_c1 = alwan_simd_set1(0.8359375);
        alwan_simd pq_c2 = alwan_simd_set1(18.8515625);
        alwan_simd pq_c3 = alwan_simd_set1(18.6875);
        alwan_simd pq_inv_m2 = alwan_simd_set1(1.0 / 78.84375);
        alwan_simd pq_inv_m1 = alwan_simd_set1(1.0 / 0.1593017578125);
        alwan_simd zero = alwan_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_simd vI = alwan_simd_load(&c0[i]);
            alwan_simd vCa = alwan_simd_load(&c1[i]);
            alwan_simd vCb = alwan_simd_load(&c2[i]);
            /* mat3: ICaCb -> LMS */
            alwan_simd L = alwan_simd_fmadd(a00, vI, alwan_simd_fmadd(a01, vCa, alwan_simd_mul(a02, vCb)));
            alwan_simd M = alwan_simd_fmadd(a10, vI, alwan_simd_fmadd(a11, vCa, alwan_simd_mul(a12, vCb)));
            alwan_simd S = alwan_simd_fmadd(a20, vI, alwan_simd_fmadd(a21, vCa, alwan_simd_mul(a22, vCb)));
            /* PQ EOTF per channel: C = Lp * pow(max(0, (N^(1/m2) - c1)) / (c2 - c3*N^(1/m2)), 1/m1) */
            #define PQ_FWD(ch) { \
                alwan_simd Np = alwan_simd_pow(ch, pq_inv_m2); \
                alwan_simd num = alwan_simd_sub(Np, pq_c1); \
                alwan_simd den = alwan_simd_fmsub(pq_c2, alwan_simd_set1(1.0), alwan_simd_mul(pq_c3, Np)); \
                num = alwan_simd_select(alwan_simd_cmplt(num, zero), zero, num); \
                alwan_simd Yp = alwan_simd_select(alwan_simd_cmple(den, zero), zero, alwan_simd_div(num, den)); \
                ch = alwan_simd_mul(pq_Lp, alwan_simd_pow(Yp, pq_inv_m1)); \
            }
            PQ_FWD(L) PQ_FWD(M) PQ_FWD(S)
            #undef PQ_FWD
            /* mat3: LMS -> XYZ */
            alwan_simd_store(&d0[i], alwan_simd_fmadd(b00, L, alwan_simd_fmadd(b01, M, alwan_simd_mul(b02, S))));
            alwan_simd_store(&d1[i], alwan_simd_fmadd(b10, L, alwan_simd_fmadd(b11, M, alwan_simd_mul(b12, S))));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(b20, L, alwan_simd_fmadd(b21, M, alwan_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_icacb icacb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_icacb_to_xyz_v(icacb);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_icacb_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *icacb_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!icacb_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, icacb_in, processed, in_stride, tile);
        alwan__icacb_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> hdr-CIELAB
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_hdr_cielab_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                             alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    /* Precompute epsilon and Km */
    alwan_scalar sf = 1.25 - 0.25 * (0.2 / 0.184);
    alwan_scalar lf = ALWAN_LN(318.0) / ALWAN_LN(100.0);
    alwan_scalar epsilon_s = 0.58 / (sf * lf);
    alwan_scalar Km_s = ALWAN_POW(2.0, epsilon_s);

    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd inv_wx = alwan_simd_set1(1.0 / ALWAN_EXT_HDR_D65_WHITE[0]);
        alwan_simd inv_wy = alwan_simd_set1(1.0 / ALWAN_EXT_HDR_D65_WHITE[1]);
        alwan_simd inv_wz = alwan_simd_set1(1.0 / ALWAN_EXT_HDR_D65_WHITE[2]);
        alwan_simd v_eps = alwan_simd_set1(epsilon_s);
        alwan_simd v_Km = alwan_simd_set1(Km_s);
        alwan_simd v_Vmax = alwan_simd_set1(247.0);
        alwan_simd v_off = alwan_simd_set1(0.02);
        alwan_simd k5 = alwan_simd_set1(5.0);
        alwan_simd k2 = alwan_simd_set1(2.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            /* Normalize by D65 white */
            alwan_simd xr = alwan_simd_mul(vx, inv_wx);
            alwan_simd yr = alwan_simd_mul(vy, inv_wy);
            alwan_simd zr = alwan_simd_mul(vz, inv_wz);
            /* Fairchild lightness: f(x) = Vmax * x^eps / (Km + x^eps) + 0.02 */
            alwan_simd xr_e = alwan_simd_pow(xr, v_eps);
            alwan_simd yr_e = alwan_simd_pow(yr, v_eps);
            alwan_simd zr_e = alwan_simd_pow(zr, v_eps);
            alwan_simd fx = alwan_simd_fmadd(v_Vmax, alwan_simd_div(xr_e, alwan_simd_add(v_Km, xr_e)), v_off);
            alwan_simd fy = alwan_simd_fmadd(v_Vmax, alwan_simd_div(yr_e, alwan_simd_add(v_Km, yr_e)), v_off);
            alwan_simd fz = alwan_simd_fmadd(v_Vmax, alwan_simd_div(zr_e, alwan_simd_add(v_Km, zr_e)), v_off);
            /* L = fy, a = 5*(fx-fy), b = 2*(fy-fz) */
            alwan_simd_store(&d0[i], fy);
            alwan_simd_store(&d1[i], alwan_simd_mul(k5, alwan_simd_sub(fx, fy)));
            alwan_simd_store(&d2[i], alwan_simd_mul(k2, alwan_simd_sub(fy, fz)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_lab r = alwan_xyz_to_hdr_cielab_v(xyz);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.a; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_xyz_to_hdr_cielab_map_interleave(alwan_scalar *hdr_lab_out, alwan_scalar const *xyz_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hdr_lab_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_hdr_cielab_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(hdr_lab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__hdr_cielab_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                             alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    /* Precompute epsilon and inverse */
    alwan_scalar sf = 1.25 - 0.25 * (0.2 / 0.184);
    alwan_scalar lf = ALWAN_LN(318.0) / ALWAN_LN(100.0);
    alwan_scalar epsilon_s = 0.58 / (sf * lf);
    alwan_scalar Km_s = ALWAN_POW(2.0, epsilon_s);
    alwan_scalar inv_eps = 1.0 / epsilon_s;

    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd wx = alwan_simd_set1(ALWAN_EXT_HDR_D65_WHITE[0]);
        alwan_simd wy = alwan_simd_set1(ALWAN_EXT_HDR_D65_WHITE[1]);
        alwan_simd wz = alwan_simd_set1(ALWAN_EXT_HDR_D65_WHITE[2]);
        alwan_simd v_Km = alwan_simd_set1(Km_s);
        alwan_simd v_Vmax = alwan_simd_set1(247.0);
        alwan_simd v_off = alwan_simd_set1(0.02);
        alwan_simd v_inv_eps = alwan_simd_set1(inv_eps);
        alwan_simd k5 = alwan_simd_set1(5.0);
        alwan_simd k2 = alwan_simd_set1(2.0);
        alwan_simd eps_guard = alwan_simd_set1(ALWAN_EPSILON);
        for (; i + W <= n; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            /* Recover fx, fz from L,a,b: fx = (a + 5L)/5, fz = (-b + 2L)/2 */
            alwan_simd fy = vL;
            alwan_simd fx = alwan_simd_div(alwan_simd_fmadd(k5, vL, va), k5);
            alwan_simd fz = alwan_simd_div(alwan_simd_sub(alwan_simd_mul(k2, vL), vb), k2);
            /* Inverse Fairchild: v = f-0.02; S = (v*Km)/(Vmax-v); result = spow(S, 1/eps) */
            #define INV_FAIR(f, w) { \
                alwan_simd v_val = alwan_simd_sub(f, v_off); \
                alwan_simd denom = alwan_simd_sub(v_Vmax, v_val); \
                alwan_simd abs_denom = alwan_simd_abs(denom); \
                alwan_simd_mask denom_tiny = alwan_simd_cmplt(abs_denom, eps_guard); \
                alwan_simd S = alwan_simd_select(denom_tiny, v_Km, alwan_simd_div(alwan_simd_mul(v_val, v_Km), denom)); \
                alwan_simd abs_S = alwan_simd_abs(S); \
                alwan_simd_mask pos = alwan_simd_cmpge(S, alwan_simd_zero()); \
                alwan_simd pw = alwan_simd_pow(abs_S, v_inv_eps); \
                f = alwan_simd_mul(alwan_simd_select(pos, pw, alwan_simd_neg(pw)), w); \
            }
            INV_FAIR(fx, wx) INV_FAIR(fy, wy) INV_FAIR(fz, wz)
            #undef INV_FAIR
            alwan_simd_store(&d0[i], fx);
            alwan_simd_store(&d1[i], fy);
            alwan_simd_store(&d2[i], fz);
        }
    }
#endif
    for (; i < n; i++) {
        alwan_lab hdr_lab = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_hdr_cielab_to_xyz_v(hdr_lab);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_hdr_cielab_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hdr_lab_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!hdr_lab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hdr_lab_in, processed, in_stride, tile);
        alwan__hdr_cielab_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> hdr-IPT
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_hdr_ipt_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                          alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    /* Precompute epsilon and Km */
    alwan_scalar lf = ALWAN_LN(318.0) / ALWAN_LN(100.0);
    alwan_scalar sf = 1.25 - 0.25 * (0.2 / 0.184);
    alwan_scalar epsilon_s = 0.59 / (sf * lf);
    alwan_scalar Km_s = ALWAN_POW(2.0, epsilon_s);

    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M1 = ALWAN_EXT_XYZ_TO_LMS_IPT.m;
        alwan_scalar const *M2 = ALWAN_EXT_LMS_TO_IPT_HDR.m;
        alwan_simd a00 = alwan_simd_set1(M1[0]), a01 = alwan_simd_set1(M1[1]), a02 = alwan_simd_set1(M1[2]);
        alwan_simd a10 = alwan_simd_set1(M1[3]), a11 = alwan_simd_set1(M1[4]), a12 = alwan_simd_set1(M1[5]);
        alwan_simd a20 = alwan_simd_set1(M1[6]), a21 = alwan_simd_set1(M1[7]), a22 = alwan_simd_set1(M1[8]);
        alwan_simd b00 = alwan_simd_set1(M2[0]), b01 = alwan_simd_set1(M2[1]), b02 = alwan_simd_set1(M2[2]);
        alwan_simd b10 = alwan_simd_set1(M2[3]), b11 = alwan_simd_set1(M2[4]), b12 = alwan_simd_set1(M2[5]);
        alwan_simd b20 = alwan_simd_set1(M2[6]), b21 = alwan_simd_set1(M2[7]), b22 = alwan_simd_set1(M2[8]);
        alwan_simd v_eps = alwan_simd_set1(epsilon_s);
        alwan_simd v_Km = alwan_simd_set1(Km_s);
        alwan_simd v_Vmax = alwan_simd_set1(247.0);
        alwan_simd v_off = alwan_simd_set1(0.02);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd one = alwan_simd_set1(1.0);
        alwan_simd neg_one = alwan_simd_set1(-1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            /* mat3: XYZ -> LMS */
            alwan_simd L = alwan_simd_fmadd(a00, vx, alwan_simd_fmadd(a01, vy, alwan_simd_mul(a02, vz)));
            alwan_simd M = alwan_simd_fmadd(a10, vx, alwan_simd_fmadd(a11, vy, alwan_simd_mul(a12, vz)));
            alwan_simd S = alwan_simd_fmadd(a20, vx, alwan_simd_fmadd(a21, vy, alwan_simd_mul(a22, vz)));
            /* sign-preserving Fairchild lightness: sign(x) * |lightness(x, eps)| */
            /* sign(x) = x>0 ? 1 : x<0 ? -1 : 0 (three-way, zero at origin) */
            /* lightness(x,e) = Vmax * spow(x,e) / (Km + spow(x,e)) + 0.02 */
            #define SPOW_LIGHT(ch) { \
                alwan_simd_mask gt = alwan_simd_cmpgt(ch, zero); \
                alwan_simd_mask lt = alwan_simd_cmplt(ch, zero); \
                alwan_simd sign_v = alwan_simd_select(gt, one, alwan_simd_select(lt, neg_one, zero)); \
                alwan_simd abs_ch = alwan_simd_abs(ch); \
                alwan_simd pow_abs = alwan_simd_pow(abs_ch, v_eps); \
                alwan_simd Y_p = alwan_simd_select(gt, pow_abs, alwan_simd_select(lt, alwan_simd_neg(pow_abs), zero)); \
                alwan_simd light = alwan_simd_fmadd(v_Vmax, alwan_simd_div(Y_p, alwan_simd_add(v_Km, Y_p)), v_off); \
                ch = alwan_simd_mul(sign_v, alwan_simd_abs(light)); \
            }
            SPOW_LIGHT(L) SPOW_LIGHT(M) SPOW_LIGHT(S)
            #undef SPOW_LIGHT
            /* mat3: LMS -> IPT */
            alwan_simd_store(&d0[i], alwan_simd_fmadd(b00, L, alwan_simd_fmadd(b01, M, alwan_simd_mul(b02, S))));
            alwan_simd_store(&d1[i], alwan_simd_fmadd(b10, L, alwan_simd_fmadd(b11, M, alwan_simd_mul(b12, S))));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(b20, L, alwan_simd_fmadd(b21, M, alwan_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_ipt r = alwan_xyz_to_hdr_ipt_v(xyz);
        d0[i] = (alwan_simd_lane)r.I; d1[i] = (alwan_simd_lane)r.P; d2[i] = (alwan_simd_lane)r.T;
    }
}

int alwan_xyz_to_hdr_ipt_map_interleave(alwan_scalar *hdr_ipt_out, alwan_scalar const *xyz_in,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hdr_ipt_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_hdr_ipt_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(hdr_ipt_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__hdr_ipt_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                          alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2, size_t n) {
    /* Precompute epsilon and inverse */
    alwan_scalar lf = ALWAN_LN(318.0) / ALWAN_LN(100.0);
    alwan_scalar sf = 1.25 - 0.25 * (0.2 / 0.184);
    alwan_scalar epsilon_s = 0.59 / (sf * lf);
    alwan_scalar Km_s = ALWAN_POW(2.0, epsilon_s);
    alwan_scalar inv_eps = 1.0 / epsilon_s;

    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M1 = ALWAN_EXT_IPT_TO_LMS_HDR.m;
        alwan_scalar const *M2 = ALWAN_EXT_LMS_TO_XYZ_IPT.m;
        alwan_simd a00 = alwan_simd_set1(M1[0]), a01 = alwan_simd_set1(M1[1]), a02 = alwan_simd_set1(M1[2]);
        alwan_simd a10 = alwan_simd_set1(M1[3]), a11 = alwan_simd_set1(M1[4]), a12 = alwan_simd_set1(M1[5]);
        alwan_simd a20 = alwan_simd_set1(M1[6]), a21 = alwan_simd_set1(M1[7]), a22 = alwan_simd_set1(M1[8]);
        alwan_simd b00 = alwan_simd_set1(M2[0]), b01 = alwan_simd_set1(M2[1]), b02 = alwan_simd_set1(M2[2]);
        alwan_simd b10 = alwan_simd_set1(M2[3]), b11 = alwan_simd_set1(M2[4]), b12 = alwan_simd_set1(M2[5]);
        alwan_simd b20 = alwan_simd_set1(M2[6]), b21 = alwan_simd_set1(M2[7]), b22 = alwan_simd_set1(M2[8]);
        alwan_simd v_Km = alwan_simd_set1(Km_s);
        alwan_simd v_Vmax = alwan_simd_set1(247.0);
        alwan_simd v_off = alwan_simd_set1(0.02);
        alwan_simd v_inv_eps = alwan_simd_set1(inv_eps);
        alwan_simd eps_guard = alwan_simd_set1(ALWAN_EPSILON);
        alwan_simd zero = alwan_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_simd vI = alwan_simd_load(&c0[i]);
            alwan_simd vP = alwan_simd_load(&c1[i]);
            alwan_simd vT = alwan_simd_load(&c2[i]);
            /* mat3: IPT -> LMS */
            alwan_simd L = alwan_simd_fmadd(a00, vI, alwan_simd_fmadd(a01, vP, alwan_simd_mul(a02, vT)));
            alwan_simd M = alwan_simd_fmadd(a10, vI, alwan_simd_fmadd(a11, vP, alwan_simd_mul(a12, vT)));
            alwan_simd S = alwan_simd_fmadd(a20, vI, alwan_simd_fmadd(a21, vP, alwan_simd_mul(a22, vT)));
            /* sign-preserving inverse Fairchild: sign(ch)*|spow(S,1/e)|
             * where S = (v*Km)/(Vmax-v), v = ch - 0.02 (on the SIGNED value,
             * matching the scalar luminance_fairchild2011_v path). */
            #define SPOW_LUMINANCE(ch) { \
                alwan_simd_mask orig_pos = alwan_simd_cmpge(ch, zero); \
                alwan_simd v_val = alwan_simd_sub(ch, v_off); \
                alwan_simd denom = alwan_simd_sub(v_Vmax, v_val); \
                alwan_simd abs_denom = alwan_simd_abs(denom); \
                alwan_simd_mask denom_tiny = alwan_simd_cmplt(abs_denom, eps_guard); \
                alwan_simd Sv = alwan_simd_select(denom_tiny, v_Km, alwan_simd_div(alwan_simd_mul(v_val, v_Km), denom)); \
                alwan_simd abs_Sv = alwan_simd_abs(Sv); \
                alwan_simd pw = alwan_simd_pow(abs_Sv, v_inv_eps); \
                alwan_simd_mask Sv_pos = alwan_simd_cmpge(Sv, zero); \
                alwan_simd spow_result = alwan_simd_select(Sv_pos, pw, alwan_simd_neg(pw)); \
                alwan_simd abs_result = alwan_simd_abs(spow_result); \
                ch = alwan_simd_select(orig_pos, abs_result, alwan_simd_neg(abs_result)); \
            }
            SPOW_LUMINANCE(L) SPOW_LUMINANCE(M) SPOW_LUMINANCE(S)
            #undef SPOW_LUMINANCE
            /* mat3: LMS -> XYZ */
            alwan_simd_store(&d0[i], alwan_simd_fmadd(b00, L, alwan_simd_fmadd(b01, M, alwan_simd_mul(b02, S))));
            alwan_simd_store(&d1[i], alwan_simd_fmadd(b10, L, alwan_simd_fmadd(b11, M, alwan_simd_mul(b12, S))));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(b20, L, alwan_simd_fmadd(b21, M, alwan_simd_mul(b22, S))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_ipt hdr_ipt = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_hdr_ipt_to_xyz_v(hdr_ipt);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_hdr_ipt_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hdr_ipt_in,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!hdr_ipt_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hdr_ipt_in, processed, in_stride, tile);
        alwan__hdr_ipt_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> CIE 1960 UCS
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_ucs_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                      alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd k23 = alwan_simd_set1(2.0 / 3.0);
        alwan_simd kn05 = alwan_simd_set1(-0.5);
        alwan_simd k15 = alwan_simd_set1(1.5);
        alwan_simd k05 = alwan_simd_set1(0.5);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            /* U = (2/3)*x, V = y, W = -0.5*x + 1.5*y + 0.5*z */
            alwan_simd_store(&d0[i], alwan_simd_mul(k23, vx));
            alwan_simd_store(&d1[i], vy);
            alwan_simd_store(&d2[i], alwan_simd_fmadd(kn05, vx, alwan_simd_fmadd(k15, vy, alwan_simd_mul(k05, vz))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_ucs r = alwan_xyz_to_ucs_v(xyz);
        d0[i] = (alwan_simd_lane)r.U; d1[i] = (alwan_simd_lane)r.V; d2[i] = (alwan_simd_lane)r.W;
    }
}

int alwan_xyz_to_ucs_map_interleave(alwan_scalar *ucs_out, alwan_scalar const *xyz_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !ucs_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_ucs_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(ucs_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__ucs_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                      alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd k15 = alwan_simd_set1(1.5);
        alwan_simd k2 = alwan_simd_set1(2.0);
        alwan_simd kn3 = alwan_simd_set1(-3.0);
        for (; i + W <= n; i += W) {
            alwan_simd vU = alwan_simd_load(&c0[i]);
            alwan_simd vV = alwan_simd_load(&c1[i]);
            alwan_simd vW = alwan_simd_load(&c2[i]);
            /* x = 1.5*U, y = V, z = 2*W + 1.5*U - 3*V */
            alwan_simd vx = alwan_simd_mul(k15, vU);
            alwan_simd_store(&d0[i], vx);
            alwan_simd_store(&d1[i], vV);
            alwan_simd_store(&d2[i], alwan_simd_fmadd(k2, vW, alwan_simd_fmadd(kn3, vV, vx)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_ucs ucs = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_ucs_to_xyz_v(ucs);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_ucs_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *ucs_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!ucs_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, ucs_in, processed, in_stride, tile);
        alwan__ucs_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> OSA-UCS
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_osa_ucs_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                          alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                          size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *Mo = XYZ_TO_RGB_OSA.m;
        alwan_simd m00 = alwan_simd_set1(Mo[0]), m01 = alwan_simd_set1(Mo[1]), m02 = alwan_simd_set1(Mo[2]);
        alwan_simd m10 = alwan_simd_set1(Mo[3]), m11 = alwan_simd_set1(Mo[4]), m12 = alwan_simd_set1(Mo[5]);
        alwan_simd m20 = alwan_simd_set1(Mo[6]), m21 = alwan_simd_set1(Mo[7]), m22 = alwan_simd_set1(Mo[8]);
        alwan_simd guard = alwan_simd_set1(1e-10);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd one = alwan_simd_set1(1.0);
        alwan_simd third = alwan_simd_set1(1.0 / 3.0);
        alwan_simd two_thirds = alwan_simd_set1(2.0 / 3.0);
        alwan_simd inv_sqrt2 = alwan_simd_set1(1.0 / ALWAN_SQRT(2.0));
        alwan_simd k_144 = alwan_simd_set1(14.4);
        alwan_simd k_59 = alwan_simd_set1(5.9);
        alwan_simd k_042 = alwan_simd_set1(0.042);
        /* Polynomial k coefficients */
        alwan_simd ka = alwan_simd_set1(4.4934), kb = alwan_simd_set1(4.3034);
        alwan_simd kc = alwan_simd_set1(-4.276), kd = alwan_simd_set1(-1.3744);
        alwan_simd ke = alwan_simd_set1(-2.5643), kf = alwan_simd_set1(1.8103);
        /* j,g coefficients */
        alwan_simd j0 = alwan_simd_set1(1.7), j1 = alwan_simd_set1(8.0), j2 = alwan_simd_set1(-9.7);
        alwan_simd g0 = alwan_simd_set1(-13.7), g1 = alwan_simd_set1(17.7), g2 = alwan_simd_set1(-4.0);
        alwan_simd k30 = alwan_simd_set1(30.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            /* xyY chromaticity */
            alwan_simd sum = alwan_simd_add(alwan_simd_add(vx, vy), vz);
            alwan_simd_mask is_black = alwan_simd_cmplt(sum, guard);
            alwan_simd sum_safe = alwan_simd_select(is_black, one, sum);
            alwan_simd cx = alwan_simd_div(vx, sum_safe);
            alwan_simd cy_v = alwan_simd_div(vy, sum_safe);
            /* Polynomial k: 4.4934*cx^2 + 4.3034*cy^2 - 4.276*cx*cy - 1.3744*cx - 2.5643*cy + 1.8103 */
            alwan_simd cx2 = alwan_simd_mul(cx, cx);
            alwan_simd cy2 = alwan_simd_mul(cy_v, cy_v);
            alwan_simd cxy = alwan_simd_mul(cx, cy_v);
            alwan_simd k_val = alwan_simd_fmadd(ka, cx2, alwan_simd_fmadd(kb, cy2, alwan_simd_fmadd(kc, cxy, alwan_simd_fmadd(kd, cx, alwan_simd_fmadd(ke, cy_v, kf)))));
            alwan_simd Y0 = alwan_simd_max(zero, alwan_simd_mul(vy, k_val));
            /* Lambda = 5.9*(Y0^(1/3) - 2/3 + 0.042*(Y0-30)^(1/3)) */
            alwan_simd Y0_cbrt = alwan_simd_pow(Y0, third);
            alwan_simd Y0m30 = alwan_simd_sub(Y0, k30);
            alwan_simd abs_Y0m30 = alwan_simd_abs(Y0m30);
            alwan_simd cbrt_abs = alwan_simd_pow(abs_Y0m30, third);
            alwan_simd_mask pos_y0m30 = alwan_simd_cmpge(Y0m30, zero);
            alwan_simd Y0m30_cbrt = alwan_simd_select(pos_y0m30, cbrt_abs, alwan_simd_neg(cbrt_abs));
            alwan_simd Y0_es = alwan_simd_sub(Y0_cbrt, two_thirds);
            alwan_simd lambda = alwan_simd_mul(k_59, alwan_simd_fmadd(k_042, Y0m30_cbrt, Y0_es));
            /* mat3: XYZ -> RGB_osa */
            alwan_simd ro = alwan_simd_fmadd(m00, vx, alwan_simd_fmadd(m01, vy, alwan_simd_mul(m02, vz)));
            alwan_simd go = alwan_simd_fmadd(m10, vx, alwan_simd_fmadd(m11, vy, alwan_simd_mul(m12, vz)));
            alwan_simd bo = alwan_simd_fmadd(m20, vx, alwan_simd_fmadd(m21, vy, alwan_simd_mul(m22, vz)));
            /* Sign-preserving cube root */
            alwan_simd abs_r = alwan_simd_abs(ro), abs_g = alwan_simd_abs(go), abs_b = alwan_simd_abs(bo);
            alwan_simd r_cbrt = alwan_simd_pow(abs_r, third);
            alwan_simd g_cbrt = alwan_simd_pow(abs_g, third);
            alwan_simd b_cbrt = alwan_simd_pow(abs_b, third);
            alwan_simd_mask r_pos = alwan_simd_cmpge(ro, zero);
            alwan_simd_mask g_pos = alwan_simd_cmpge(go, zero);
            alwan_simd_mask b_pos = alwan_simd_cmpge(bo, zero);
            r_cbrt = alwan_simd_select(r_pos, r_cbrt, alwan_simd_neg(r_cbrt));
            g_cbrt = alwan_simd_select(g_pos, g_cbrt, alwan_simd_neg(g_cbrt));
            b_cbrt = alwan_simd_select(b_pos, b_cbrt, alwan_simd_neg(b_cbrt));
            /* Chroma coefficient C */
            alwan_simd abs_Y0es = alwan_simd_abs(Y0_es);
            alwan_simd_mask y0es_tiny = alwan_simd_cmplt(abs_Y0es, guard);
            alwan_simd C = alwan_simd_select(y0es_tiny, one, alwan_simd_div(lambda, alwan_simd_mul(k_59, Y0_es)));
            /* L, j, g */
            alwan_simd L_val = alwan_simd_mul(alwan_simd_sub(lambda, k_144), inv_sqrt2);
            alwan_simd j_val = alwan_simd_mul(C, alwan_simd_fmadd(j0, r_cbrt, alwan_simd_fmadd(j1, g_cbrt, alwan_simd_mul(j2, b_cbrt))));
            alwan_simd g_val = alwan_simd_mul(C, alwan_simd_fmadd(g0, r_cbrt, alwan_simd_fmadd(g1, g_cbrt, alwan_simd_mul(g2, b_cbrt))));
            /* Black guard */
            alwan_simd_store(&d0[i], alwan_simd_select(is_black, zero, L_val));
            alwan_simd_store(&d1[i], alwan_simd_select(is_black, zero, j_val));
            alwan_simd_store(&d2[i], alwan_simd_select(is_black, zero, g_val));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_osa_ucs r = alwan_xyz_to_osa_ucs_v(xyz);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.j; d2[i] = (alwan_simd_lane)r.g;
    }
}

int alwan_xyz_to_osa_ucs_map_interleave(alwan_scalar *osa_out, alwan_scalar const *xyz_in,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !osa_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_osa_ucs_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(osa_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__osa_ucs_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                          alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                          size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *Mi = RGB_TO_XYZ_OSA.m;
        alwan_simd m00 = alwan_simd_set1(Mi[0]), m01 = alwan_simd_set1(Mi[1]), m02 = alwan_simd_set1(Mi[2]);
        alwan_simd m10 = alwan_simd_set1(Mi[3]), m11 = alwan_simd_set1(Mi[4]), m12 = alwan_simd_set1(Mi[5]);
        alwan_simd m20 = alwan_simd_set1(Mi[6]), m21 = alwan_simd_set1(Mi[7]), m22 = alwan_simd_set1(Mi[8]);
        alwan_simd sqrt2 = alwan_simd_set1(ALWAN_SQRT(2.0));
        alwan_simd k_144 = alwan_simd_set1(14.4);
        alwan_simd inv_59 = alwan_simd_set1(1.0 / 5.9);
        alwan_simd two_thirds = alwan_simd_set1(2.0 / 3.0);
        alwan_simd k_59 = alwan_simd_set1(5.9);
        alwan_simd guard = alwan_simd_set1(1e-10);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd one = alwan_simd_set1(1.0);
        /* Approximate inverse RGB' coefficients */
        alwan_simd jr0 = alwan_simd_set1(0.01), jr1 = alwan_simd_set1(-0.02);
        alwan_simd jg0 = alwan_simd_set1(0.12), jg1 = alwan_simd_set1(0.06);
        alwan_simd jb0 = alwan_simd_set1(-0.10), jb1 = alwan_simd_set1(-0.05);
        for (; i + W <= n; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vj = alwan_simd_load(&c1[i]);
            alwan_simd vg = alwan_simd_load(&c2[i]);
            /* lambda = L*sqrt(2) + 14.4 */
            alwan_simd lambda = alwan_simd_fmadd(vL, sqrt2, k_144);
            /* Y0_cbrt = lambda/5.9 + 2/3 */
            alwan_simd Y0_cbrt = alwan_simd_fmadd(lambda, inv_59, two_thirds);
            /* C = lambda / (5.9 * (Y0_cbrt - 2/3)) */
            alwan_simd Y0_es = alwan_simd_sub(Y0_cbrt, two_thirds);
            alwan_simd_mask y0_gt = alwan_simd_cmpgt(Y0_cbrt, two_thirds);
            alwan_simd C = alwan_simd_select(y0_gt, alwan_simd_div(lambda, alwan_simd_mul(k_59, Y0_es)), one);
            /* Guard C too small */
            alwan_simd abs_C = alwan_simd_abs(C);
            alwan_simd_mask c_tiny = alwan_simd_cmplt(abs_C, guard);
            alwan_simd C_safe = alwan_simd_select(c_tiny, one, C);
            alwan_simd j_norm = alwan_simd_div(vj, C_safe);
            alwan_simd g_norm = alwan_simd_div(vg, C_safe);
            /* Recover cube roots */
            alwan_simd r_cbrt = alwan_simd_fmadd(jr0, j_norm, alwan_simd_fmadd(jr1, g_norm, Y0_cbrt));
            alwan_simd g_cbrt = alwan_simd_fmadd(jg0, j_norm, alwan_simd_fmadd(jg1, g_norm, Y0_cbrt));
            alwan_simd b_cbrt = alwan_simd_fmadd(jb0, j_norm, alwan_simd_fmadd(jb1, g_norm, Y0_cbrt));
            /* Cube (sign-preserving): x^3 */
            alwan_simd r2 = alwan_simd_mul(r_cbrt, r_cbrt), r3 = alwan_simd_mul(r2, r_cbrt);
            alwan_simd g2 = alwan_simd_mul(g_cbrt, g_cbrt), g3 = alwan_simd_mul(g2, g_cbrt);
            alwan_simd b2 = alwan_simd_mul(b_cbrt, b_cbrt), b3 = alwan_simd_mul(b2, b_cbrt);
            /* mat3: RGB -> XYZ */
            alwan_simd ox = alwan_simd_fmadd(m00, r3, alwan_simd_fmadd(m01, g3, alwan_simd_mul(m02, b3)));
            alwan_simd oy = alwan_simd_fmadd(m10, r3, alwan_simd_fmadd(m11, g3, alwan_simd_mul(m12, b3)));
            alwan_simd oz = alwan_simd_fmadd(m20, r3, alwan_simd_fmadd(m21, g3, alwan_simd_mul(m22, b3)));
            /* Clamp negatives + C_tiny guard */
            ox = alwan_simd_max(zero, ox); oy = alwan_simd_max(zero, oy); oz = alwan_simd_max(zero, oz);
            alwan_simd_store(&d0[i], alwan_simd_select(c_tiny, zero, ox));
            alwan_simd_store(&d1[i], alwan_simd_select(c_tiny, zero, oy));
            alwan_simd_store(&d2[i], alwan_simd_select(c_tiny, zero, oz));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_osa_ucs osa = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_osa_ucs_to_xyz_v(osa);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_osa_ucs_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *osa_in,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!osa_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, osa_in, processed, in_stride, tile);
        alwan__osa_ucs_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> Hunter Lab
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_hunter_lab_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                             alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                             size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd inv_xn = alwan_simd_set1(1.0 / ALWAN_HUNTER_D65_XN);
        alwan_simd inv_yn = alwan_simd_set1(1.0 / ALWAN_HUNTER_D65_YN);
        alwan_simd inv_zn = alwan_simd_set1(1.0 / ALWAN_HUNTER_D65_ZN);
        alwan_simd k100 = alwan_simd_set1(100.0);
        alwan_simd ka = alwan_simd_set1(ALWAN_HUNTER_KA_D65);
        alwan_simd kb = alwan_simd_set1(ALWAN_HUNTER_KB_D65);
        alwan_simd guard = alwan_simd_set1(1e-10);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            alwan_simd y_ratio = alwan_simd_mul(vy, inv_yn);
            alwan_simd sqrt_yr = alwan_simd_sqrt(y_ratio);
            alwan_simd_mask tiny = alwan_simd_cmplt(sqrt_yr, guard);
            alwan_simd safe = alwan_simd_select(tiny, zero, one);
            alwan_simd inv_sqrt = alwan_simd_select(tiny, zero, alwan_simd_div(one, sqrt_yr));
            alwan_simd x_ratio = alwan_simd_mul(vx, inv_xn);
            alwan_simd z_ratio = alwan_simd_mul(vz, inv_zn);
            alwan_simd_store(&d0[i], alwan_simd_mul(alwan_simd_mul(k100, sqrt_yr), safe));
            alwan_simd_store(&d1[i], alwan_simd_mul(alwan_simd_mul(ka, alwan_simd_sub(x_ratio, y_ratio)), alwan_simd_mul(inv_sqrt, safe)));
            alwan_simd_store(&d2[i], alwan_simd_mul(alwan_simd_mul(kb, alwan_simd_sub(y_ratio, z_ratio)), alwan_simd_mul(inv_sqrt, safe)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_hunter_lab r = alwan_xyz_to_hunter_lab_v(xyz);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.a; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_xyz_to_hunter_lab_map_interleave(alwan_scalar *hl_out, alwan_scalar const *xyz_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hl_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_hunter_lab_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(hl_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__hunter_lab_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                             alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                             size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd inv100 = alwan_simd_set1(1.0 / 100.0);
        alwan_simd yn = alwan_simd_set1(ALWAN_HUNTER_D65_YN);
        alwan_simd xn = alwan_simd_set1(ALWAN_HUNTER_D65_XN);
        alwan_simd zn = alwan_simd_set1(ALWAN_HUNTER_D65_ZN);
        alwan_simd inv_ka = alwan_simd_set1(1.0 / ALWAN_HUNTER_KA_D65);
        alwan_simd inv_kb = alwan_simd_set1(1.0 / ALWAN_HUNTER_KB_D65);
        for (; i + W <= n; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd l_norm = alwan_simd_mul(vL, inv100);
            alwan_simd l2 = alwan_simd_mul(l_norm, l_norm);
            /* y = l_norm^2 * YN */
            alwan_simd_store(&d1[i], alwan_simd_mul(l2, yn));
            /* a_term = (a/KA) * l_norm; x = (a_term + l_norm^2) * XN */
            alwan_simd a_term = alwan_simd_mul(alwan_simd_mul(va, inv_ka), l_norm);
            alwan_simd_store(&d0[i], alwan_simd_mul(alwan_simd_add(a_term, l2), xn));
            /* b_term = (b/KB) * l_norm; z = -(b_term - l_norm^2) * ZN */
            alwan_simd b_term = alwan_simd_mul(alwan_simd_mul(vb, inv_kb), l_norm);
            alwan_simd_store(&d2[i], alwan_simd_mul(alwan_simd_sub(l2, b_term), zn));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_hunter_lab hl = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_hunter_lab_to_xyz_v(hl);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_hunter_lab_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hl_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!hl_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hl_in, processed, in_stride, tile);
        alwan__hunter_lab_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__xyz_to_hunter_lab_custom_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                                    alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                                    size_t n, alwan_xyz const *white_xyz) {
    alwan_vec2 coeffs = alwan_hunter_coefficients_v(*white_xyz);
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd inv_xn = alwan_simd_set1(1.0 / white_xyz->x);
        alwan_simd inv_yn = alwan_simd_set1(1.0 / white_xyz->y);
        alwan_simd inv_zn = alwan_simd_set1(1.0 / white_xyz->z);
        alwan_simd k10 = alwan_simd_set1(10.0);
        alwan_simd ka = alwan_simd_set1(coeffs.v[0]);
        alwan_simd kb = alwan_simd_set1(coeffs.v[1]);
        alwan_simd guard = alwan_simd_set1(1e-10);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            alwan_simd y_ratio = alwan_simd_mul(vy, inv_yn);
            alwan_simd sqrt_yr = alwan_simd_sqrt(y_ratio);
            alwan_simd_mask tiny = alwan_simd_cmplt(sqrt_yr, guard);
            alwan_simd safe = alwan_simd_select(tiny, zero, one);
            alwan_simd inv_sqrt = alwan_simd_select(tiny, zero, alwan_simd_div(one, sqrt_yr));
            alwan_simd x_ratio = alwan_simd_mul(vx, inv_xn);
            alwan_simd z_ratio = alwan_simd_mul(vz, inv_zn);
            alwan_simd_store(&d0[i], alwan_simd_mul(alwan_simd_mul(k10, sqrt_yr), safe));
            alwan_simd_store(&d1[i], alwan_simd_mul(alwan_simd_mul(ka, alwan_simd_sub(x_ratio, y_ratio)), alwan_simd_mul(inv_sqrt, safe)));
            alwan_simd_store(&d2[i], alwan_simd_mul(alwan_simd_mul(kb, alwan_simd_sub(y_ratio, z_ratio)), alwan_simd_mul(inv_sqrt, safe)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_hunter_lab r = alwan_xyz_to_hunter_lab_custom_v(xyz, *white_xyz);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.a; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_xyz_to_hunter_lab_custom_map_interleave(alwan_scalar *hl_out, alwan_scalar const *xyz_in,
                                        alwan_xyz const *white_xyz,
                                        size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hl_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_hunter_lab_custom_kernel(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(hl_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__hunter_lab_to_xyz_custom_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                                    alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                                    size_t n, alwan_xyz const *white_xyz) {
    alwan_vec2 coeffs = alwan_hunter_coefficients_v(*white_xyz);
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd inv10 = alwan_simd_set1(1.0 / 10.0);
        alwan_simd v_yn = alwan_simd_set1(white_xyz->y);
        alwan_simd v_xn = alwan_simd_set1(white_xyz->x);
        alwan_simd v_zn = alwan_simd_set1(white_xyz->z);
        alwan_simd inv_ka = alwan_simd_set1(1.0 / coeffs.v[0]);
        alwan_simd inv_kb = alwan_simd_set1(1.0 / coeffs.v[1]);
        for (; i + W <= n; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd l_norm = alwan_simd_mul(vL, inv10);
            alwan_simd l2 = alwan_simd_mul(l_norm, l_norm);
            alwan_simd_store(&d1[i], alwan_simd_mul(l2, v_yn));
            alwan_simd a_term = alwan_simd_mul(alwan_simd_mul(va, inv_ka), l_norm);
            alwan_simd_store(&d0[i], alwan_simd_mul(alwan_simd_add(a_term, l2), v_xn));
            alwan_simd b_term = alwan_simd_mul(alwan_simd_mul(vb, inv_kb), l_norm);
            alwan_simd_store(&d2[i], alwan_simd_mul(alwan_simd_sub(l2, b_term), v_zn));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_hunter_lab hl = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_hunter_lab_to_xyz_custom_v(hl, *white_xyz);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_hunter_lab_to_xyz_custom_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hl_in,
                                        alwan_xyz const *white_xyz,
                                        size_t count, size_t in_stride, size_t out_stride) {
    if (!hl_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hl_in, processed, in_stride, tile);
        alwan__hunter_lab_to_xyz_custom_kernel(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> ProLab
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_prolab_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                         alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                         size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        /* Precompute: normalize by D65 white then apply Q */
        alwan_scalar const *Q = ALWAN_PROLAB_MATRIX_Q.m;
        alwan_scalar inv_wx = 1.0 / ALWAN_PROLAB_D65_WHITE[0];
        alwan_scalar inv_wy = 1.0 / ALWAN_PROLAB_D65_WHITE[1];
        alwan_scalar inv_wz = 1.0 / ALWAN_PROLAB_D65_WHITE[2];
        /* Fused coefficients: Q * diag(1/wx, 1/wy, 1/wz, 1) */
        alwan_simd m00 = alwan_simd_set1(Q[0]*inv_wx), m01 = alwan_simd_set1(Q[1]*inv_wy), m02 = alwan_simd_set1(Q[2]*inv_wz), m03 = alwan_simd_set1(Q[3]);
        alwan_simd m10 = alwan_simd_set1(Q[4]*inv_wx), m11 = alwan_simd_set1(Q[5]*inv_wy), m12 = alwan_simd_set1(Q[6]*inv_wz), m13 = alwan_simd_set1(Q[7]);
        alwan_simd m20 = alwan_simd_set1(Q[8]*inv_wx), m21 = alwan_simd_set1(Q[9]*inv_wy), m22 = alwan_simd_set1(Q[10]*inv_wz), m23 = alwan_simd_set1(Q[11]);
        alwan_simd m30 = alwan_simd_set1(Q[12]*inv_wx), m31 = alwan_simd_set1(Q[13]*inv_wy), m32 = alwan_simd_set1(Q[14]*inv_wz), m33 = alwan_simd_set1(Q[15]);
        alwan_simd guard = alwan_simd_set1(1e-10);
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            alwan_simd r0 = alwan_simd_fmadd(m00, vx, alwan_simd_fmadd(m01, vy, alwan_simd_fmadd(m02, vz, m03)));
            alwan_simd r1 = alwan_simd_fmadd(m10, vx, alwan_simd_fmadd(m11, vy, alwan_simd_fmadd(m12, vz, m13)));
            alwan_simd r2 = alwan_simd_fmadd(m20, vx, alwan_simd_fmadd(m21, vy, alwan_simd_fmadd(m22, vz, m23)));
            alwan_simd w  = alwan_simd_fmadd(m30, vx, alwan_simd_fmadd(m31, vy, alwan_simd_fmadd(m32, vz, m33)));
            alwan_simd abs_w = alwan_simd_abs(w);
            alwan_simd_mask tiny = alwan_simd_cmplt(abs_w, guard);
            alwan_simd inv_w = alwan_simd_select(tiny, one, alwan_simd_div(one, w));
            /* Fallback to normalized input on degenerate w */
            alwan_simd nx = alwan_simd_mul(vx, alwan_simd_set1(inv_wx));
            alwan_simd ny = alwan_simd_mul(vy, alwan_simd_set1(inv_wy));
            alwan_simd nz = alwan_simd_mul(vz, alwan_simd_set1(inv_wz));
            alwan_simd_store(&d0[i], alwan_simd_select(tiny, nx, alwan_simd_mul(r0, inv_w)));
            alwan_simd_store(&d1[i], alwan_simd_select(tiny, ny, alwan_simd_mul(r1, inv_w)));
            alwan_simd_store(&d2[i], alwan_simd_select(tiny, nz, alwan_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_prolab r = alwan_xyz_to_prolab_v(xyz);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.a; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_xyz_to_prolab_map_interleave(alwan_scalar *prolab_out, alwan_scalar const *xyz_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !prolab_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_prolab_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(prolab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__prolab_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                         alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                         size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *Qi = ALWAN_PROLAB_MATRIX_Q_INV.m;
        alwan_scalar wx = ALWAN_PROLAB_D65_WHITE[0];
        alwan_scalar wy = ALWAN_PROLAB_D65_WHITE[1];
        alwan_scalar wz = ALWAN_PROLAB_D65_WHITE[2];
        /* Fused coefficients: diag(wx,wy,wz) * Q_inv */
        alwan_simd m00 = alwan_simd_set1(Qi[0]*wx), m01 = alwan_simd_set1(Qi[1]*wx), m02 = alwan_simd_set1(Qi[2]*wx), m03 = alwan_simd_set1(Qi[3]*wx);
        alwan_simd m10 = alwan_simd_set1(Qi[4]*wy), m11 = alwan_simd_set1(Qi[5]*wy), m12 = alwan_simd_set1(Qi[6]*wy), m13 = alwan_simd_set1(Qi[7]*wy);
        alwan_simd m20 = alwan_simd_set1(Qi[8]*wz), m21 = alwan_simd_set1(Qi[9]*wz), m22 = alwan_simd_set1(Qi[10]*wz), m23 = alwan_simd_set1(Qi[11]*wz);
        alwan_simd m30 = alwan_simd_set1(Qi[12]), m31 = alwan_simd_set1(Qi[13]), m32 = alwan_simd_set1(Qi[14]), m33 = alwan_simd_set1(Qi[15]);
        alwan_simd guard = alwan_simd_set1(1e-10);
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            /* Compute w first for perspective divide */
            alwan_simd w  = alwan_simd_fmadd(m30, vL, alwan_simd_fmadd(m31, va, alwan_simd_fmadd(m32, vb, m33)));
            alwan_simd abs_w = alwan_simd_abs(w);
            alwan_simd_mask tiny = alwan_simd_cmplt(abs_w, guard);
            alwan_simd inv_w = alwan_simd_select(tiny, one, alwan_simd_div(one, w));
            /* Rows with fused white multiplication */
            alwan_simd r0 = alwan_simd_fmadd(m00, vL, alwan_simd_fmadd(m01, va, alwan_simd_fmadd(m02, vb, m03)));
            alwan_simd r1 = alwan_simd_fmadd(m10, vL, alwan_simd_fmadd(m11, va, alwan_simd_fmadd(m12, vb, m13)));
            alwan_simd r2 = alwan_simd_fmadd(m20, vL, alwan_simd_fmadd(m21, va, alwan_simd_fmadd(m22, vb, m23)));
            alwan_simd_store(&d0[i], alwan_simd_select(tiny, alwan_simd_mul(vL, alwan_simd_set1(wx)), alwan_simd_mul(r0, inv_w)));
            alwan_simd_store(&d1[i], alwan_simd_select(tiny, alwan_simd_mul(va, alwan_simd_set1(wy)), alwan_simd_mul(r1, inv_w)));
            alwan_simd_store(&d2[i], alwan_simd_select(tiny, alwan_simd_mul(vb, alwan_simd_set1(wz)), alwan_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_prolab prolab = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_prolab_to_xyz_v(prolab);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_prolab_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *prolab_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!prolab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, prolab_in, processed, in_stride, tile);
        alwan__prolab_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__xyz_to_prolab_custom_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                                alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                                size_t n, alwan_xyz const *white_xyz) {
    /* Precompute safe white point reciprocals */
    alwan_scalar const eps = 1e-10;
    alwan_scalar inv_wx = (ALWAN_ABS(white_xyz->x) > eps) ? 1.0 / white_xyz->x : 1.0 / eps;
    alwan_scalar inv_wy = (ALWAN_ABS(white_xyz->y) > eps) ? 1.0 / white_xyz->y : 1.0 / eps;
    alwan_scalar inv_wz = (ALWAN_ABS(white_xyz->z) > eps) ? 1.0 / white_xyz->z : 1.0 / eps;
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *Q = ALWAN_PROLAB_MATRIX_Q.m;
        alwan_simd m00 = alwan_simd_set1(Q[0]*inv_wx), m01 = alwan_simd_set1(Q[1]*inv_wy), m02 = alwan_simd_set1(Q[2]*inv_wz), m03 = alwan_simd_set1(Q[3]);
        alwan_simd m10 = alwan_simd_set1(Q[4]*inv_wx), m11 = alwan_simd_set1(Q[5]*inv_wy), m12 = alwan_simd_set1(Q[6]*inv_wz), m13 = alwan_simd_set1(Q[7]);
        alwan_simd m20 = alwan_simd_set1(Q[8]*inv_wx), m21 = alwan_simd_set1(Q[9]*inv_wy), m22 = alwan_simd_set1(Q[10]*inv_wz), m23 = alwan_simd_set1(Q[11]);
        alwan_simd m30 = alwan_simd_set1(Q[12]*inv_wx), m31 = alwan_simd_set1(Q[13]*inv_wy), m32 = alwan_simd_set1(Q[14]*inv_wz), m33 = alwan_simd_set1(Q[15]);
        alwan_simd vguard = alwan_simd_set1(1e-10);
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            alwan_simd r0 = alwan_simd_fmadd(m00, vx, alwan_simd_fmadd(m01, vy, alwan_simd_fmadd(m02, vz, m03)));
            alwan_simd r1 = alwan_simd_fmadd(m10, vx, alwan_simd_fmadd(m11, vy, alwan_simd_fmadd(m12, vz, m13)));
            alwan_simd r2 = alwan_simd_fmadd(m20, vx, alwan_simd_fmadd(m21, vy, alwan_simd_fmadd(m22, vz, m23)));
            alwan_simd w  = alwan_simd_fmadd(m30, vx, alwan_simd_fmadd(m31, vy, alwan_simd_fmadd(m32, vz, m33)));
            alwan_simd abs_w = alwan_simd_abs(w);
            alwan_simd_mask tiny = alwan_simd_cmplt(abs_w, vguard);
            alwan_simd inv_w = alwan_simd_select(tiny, one, alwan_simd_div(one, w));
            alwan_simd nx = alwan_simd_mul(vx, alwan_simd_set1(inv_wx));
            alwan_simd ny = alwan_simd_mul(vy, alwan_simd_set1(inv_wy));
            alwan_simd nz = alwan_simd_mul(vz, alwan_simd_set1(inv_wz));
            alwan_simd_store(&d0[i], alwan_simd_select(tiny, nx, alwan_simd_mul(r0, inv_w)));
            alwan_simd_store(&d1[i], alwan_simd_select(tiny, ny, alwan_simd_mul(r1, inv_w)));
            alwan_simd_store(&d2[i], alwan_simd_select(tiny, nz, alwan_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_prolab r = alwan_xyz_to_prolab_custom_v(xyz, *white_xyz);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.a; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_xyz_to_prolab_custom_map_interleave(alwan_scalar *prolab_out, alwan_scalar const *xyz_in,
                                    alwan_xyz const *white_xyz,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !prolab_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_prolab_custom_kernel(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(prolab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__prolab_to_xyz_custom_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                                alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                                size_t n, alwan_xyz const *white_xyz) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *Qi = ALWAN_PROLAB_MATRIX_Q_INV.m;
        alwan_scalar wx = white_xyz->x, wy = white_xyz->y, wz = white_xyz->z;
        alwan_simd m00 = alwan_simd_set1(Qi[0]*wx), m01 = alwan_simd_set1(Qi[1]*wx), m02 = alwan_simd_set1(Qi[2]*wx), m03 = alwan_simd_set1(Qi[3]*wx);
        alwan_simd m10 = alwan_simd_set1(Qi[4]*wy), m11 = alwan_simd_set1(Qi[5]*wy), m12 = alwan_simd_set1(Qi[6]*wy), m13 = alwan_simd_set1(Qi[7]*wy);
        alwan_simd m20 = alwan_simd_set1(Qi[8]*wz), m21 = alwan_simd_set1(Qi[9]*wz), m22 = alwan_simd_set1(Qi[10]*wz), m23 = alwan_simd_set1(Qi[11]*wz);
        alwan_simd m30 = alwan_simd_set1(Qi[12]), m31 = alwan_simd_set1(Qi[13]), m32 = alwan_simd_set1(Qi[14]), m33 = alwan_simd_set1(Qi[15]);
        alwan_simd guard = alwan_simd_set1(1e-10);
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W <= n; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd w  = alwan_simd_fmadd(m30, vL, alwan_simd_fmadd(m31, va, alwan_simd_fmadd(m32, vb, m33)));
            alwan_simd abs_w = alwan_simd_abs(w);
            alwan_simd_mask tiny = alwan_simd_cmplt(abs_w, guard);
            alwan_simd inv_w = alwan_simd_select(tiny, one, alwan_simd_div(one, w));
            alwan_simd r0 = alwan_simd_fmadd(m00, vL, alwan_simd_fmadd(m01, va, alwan_simd_fmadd(m02, vb, m03)));
            alwan_simd r1 = alwan_simd_fmadd(m10, vL, alwan_simd_fmadd(m11, va, alwan_simd_fmadd(m12, vb, m13)));
            alwan_simd r2 = alwan_simd_fmadd(m20, vL, alwan_simd_fmadd(m21, va, alwan_simd_fmadd(m22, vb, m23)));
            alwan_simd_store(&d0[i], alwan_simd_select(tiny, alwan_simd_mul(vL, alwan_simd_set1(wx)), alwan_simd_mul(r0, inv_w)));
            alwan_simd_store(&d1[i], alwan_simd_select(tiny, alwan_simd_mul(va, alwan_simd_set1(wy)), alwan_simd_mul(r1, inv_w)));
            alwan_simd_store(&d2[i], alwan_simd_select(tiny, alwan_simd_mul(vb, alwan_simd_set1(wz)), alwan_simd_mul(r2, inv_w)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_prolab prolab = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_prolab_to_xyz_custom_v(prolab, *white_xyz);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_prolab_to_xyz_custom_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *prolab_in,
                                    alwan_xyz const *white_xyz,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!prolab_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, prolab_in, processed, in_stride, tile);
        alwan__prolab_to_xyz_custom_kernel(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> UVW (with white point)
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_uvw_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                      alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                      size_t n, alwan_xyz const *white_xyz) {
    /* Precompute white point chromaticity */
    alwan_scalar sum_n = white_xyz->x + 15.0 * white_xyz->y + 3.0 * white_xyz->z;
    alwan_scalar un_s = (ALWAN_ABS(sum_n) < ALWAN_EPSILON) ? 0.0 : (4.0 * white_xyz->x) / sum_n;
    alwan_scalar vn_s = (ALWAN_ABS(sum_n) < ALWAN_EPSILON) ? 0.0 : (6.0 * white_xyz->y) / sum_n;
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd eps = alwan_simd_set1(ALWAN_EPSILON);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd k4 = alwan_simd_set1(4.0);
        alwan_simd k6 = alwan_simd_set1(6.0);
        alwan_simd k15 = alwan_simd_set1(15.0);
        alwan_simd k3 = alwan_simd_set1(3.0);
        alwan_simd k13 = alwan_simd_set1(13.0);
        alwan_simd k25 = alwan_simd_set1(25.0);
        alwan_simd kn17 = alwan_simd_set1(-17.0);
        alwan_simd v_un = alwan_simd_set1(un_s);
        alwan_simd v_vn = alwan_simd_set1(vn_s);
        alwan_simd inv_wy = alwan_simd_set1(white_xyz->y > ALWAN_EPSILON ? 1.0 / white_xyz->y : 0.0);
        for (; i + W <= n; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);
            /* sum = x + 15*y + 3*z */
            alwan_simd sum = alwan_simd_fmadd(k15, vy, alwan_simd_fmadd(k3, vz, vx));
            alwan_simd abs_sum = alwan_simd_abs(sum);
            alwan_simd_mask sum_tiny = alwan_simd_cmplt(abs_sum, eps);
            alwan_simd inv_sum = alwan_simd_div(alwan_simd_set1(1.0), sum);
            alwan_simd u = alwan_simd_select(sum_tiny, zero, alwan_simd_mul(alwan_simd_mul(k4, vx), inv_sum));
            alwan_simd v = alwan_simd_select(sum_tiny, zero, alwan_simd_mul(alwan_simd_mul(k6, vy), inv_sum));
            /* Y_ratio = y / white.y; W = 25*cbrt(Y_ratio) - 17 */
            alwan_simd Y_ratio = alwan_simd_mul(vy, inv_wy);
            alwan_simd_mask y_tiny = alwan_simd_cmplt(vy, eps);
            alwan_simd Wstar = alwan_simd_select(y_tiny, kn17,
                                alwan_simd_fmadd(k25, alwan_simd_cbrt(Y_ratio), kn17));
            /* U* = 13*W*(u-un), V* = 13*W*(v-vn) */
            alwan_simd k13W = alwan_simd_mul(k13, Wstar);
            alwan_simd_store(&d0[i], alwan_simd_mul(k13W, alwan_simd_sub(u, v_un)));
            alwan_simd_store(&d1[i], alwan_simd_mul(k13W, alwan_simd_sub(v, v_vn)));
            alwan_simd_store(&d2[i], Wstar);
        }
    }
#endif
    for (; i < n; i++) {
        alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_uvw r = alwan_xyz_to_uvw_v(xyz, *white_xyz);
        d0[i] = (alwan_simd_lane)r.U; d1[i] = (alwan_simd_lane)r.V; d2[i] = (alwan_simd_lane)r.W;
    }
}

int alwan_xyz_to_uvw_map_interleave(alwan_scalar *uvw_out, alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !uvw_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_uvw_kernel(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(uvw_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__uvw_to_xyz_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                      alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                      size_t n, alwan_xyz const *white_xyz) {
    /* Precompute white point chromaticity */
    alwan_scalar sum_n = white_xyz->x + 15.0 * white_xyz->y + 3.0 * white_xyz->z;
    alwan_scalar un_s = (ALWAN_ABS(sum_n) < ALWAN_EPSILON) ? 0.0 : (4.0 * white_xyz->x) / sum_n;
    alwan_scalar vn_s = (ALWAN_ABS(sum_n) < ALWAN_EPSILON) ? 0.0 : (6.0 * white_xyz->y) / sum_n;
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd eps = alwan_simd_set1(ALWAN_EPSILON);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd k13 = alwan_simd_set1(13.0);
        alwan_simd k17 = alwan_simd_set1(17.0);
        alwan_simd inv25 = alwan_simd_set1(1.0 / 25.0);
        alwan_simd k9 = alwan_simd_set1(9.0);
        alwan_simd k4 = alwan_simd_set1(4.0);
        alwan_simd k12 = alwan_simd_set1(12.0);
        alwan_simd kn3 = alwan_simd_set1(-3.0);
        alwan_simd kn20 = alwan_simd_set1(-20.0);
        alwan_simd v_un = alwan_simd_set1(un_s);
        alwan_simd v_vn = alwan_simd_set1(vn_s);
        for (; i + W <= n; i += W) {
            alwan_simd vU = alwan_simd_load(&c0[i]);
            alwan_simd vV = alwan_simd_load(&c1[i]);
            alwan_simd vW = alwan_simd_load(&c2[i]);
            /* Y from W*: Y_cbrt = (W+17)/25; Y = Y_cbrt^3 */
            alwan_simd W17 = alwan_simd_add(vW, k17);
            alwan_simd_mask w17_tiny = alwan_simd_cmplt(W17, eps);
            alwan_simd Y_cbrt = alwan_simd_select(w17_tiny, zero, alwan_simd_mul(W17, inv25));
            alwan_simd Y = alwan_simd_mul(alwan_simd_mul(Y_cbrt, Y_cbrt), Y_cbrt);
            /* Recover u, v from U*, V*, W* */
            alwan_simd W13 = alwan_simd_mul(k13, vW);
            alwan_simd abs_W = alwan_simd_abs(vW);
            alwan_simd_mask w_tiny = alwan_simd_cmplt(abs_W, eps);
            alwan_simd u = alwan_simd_select(w_tiny, v_un,
                            alwan_simd_fmadd(alwan_simd_div(vU, W13), alwan_simd_set1(1.0), v_un));
            alwan_simd v = alwan_simd_select(w_tiny, v_vn,
                            alwan_simd_fmadd(alwan_simd_div(vV, W13), alwan_simd_set1(1.0), v_vn));
            /* X = 9*u*Y / (4*v), Z = (12 - 3*u - 20*v)*Y / (4*v) */
            alwan_simd abs_v = alwan_simd_abs(v);
            alwan_simd_mask v_tiny = alwan_simd_cmplt(abs_v, eps);
            alwan_simd inv_4v = alwan_simd_div(alwan_simd_set1(1.0), alwan_simd_mul(k4, v));
            alwan_simd_store(&d0[i], alwan_simd_select(v_tiny, zero,
                                alwan_simd_mul(alwan_simd_mul(k9, alwan_simd_mul(u, Y)), inv_4v)));
            alwan_simd_store(&d1[i], Y);
            /* (12 - 3*u - 20*v) */
            alwan_simd numer = alwan_simd_fmadd(kn3, u, alwan_simd_fmadd(kn20, v, k12));
            alwan_simd_store(&d2[i], alwan_simd_select(v_tiny, zero,
                                alwan_simd_mul(alwan_simd_mul(numer, Y), inv_4v)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_uvw uvw = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_xyz r = alwan_uvw_to_xyz_v(uvw, *white_xyz);
        d0[i] = (alwan_simd_lane)r.x; d1[i] = (alwan_simd_lane)r.y; d2[i] = (alwan_simd_lane)r.z;
    }
}

int alwan_uvw_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *uvw_in,
                          alwan_xyz const *white_xyz,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!uvw_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, uvw_in, processed, in_stride, tile);
        alwan__uvw_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile, white_xyz);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> Prismatic
 * ---------------------------------------------------------------- */

static void alwan__rgb_to_prismatic_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                            alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                            size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd eps = alwan_simd_set1(ALWAN_EPSILON);
        alwan_simd zero = alwan_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd L = alwan__simd_max3(vr, vg, vb);
            alwan_simd sum_rgb = alwan_simd_add(alwan_simd_add(vr, vg), vb);
            alwan_simd_mask tiny = alwan_simd_cmplt(sum_rgb, eps);
            alwan_simd inv_sum = alwan_simd_div(alwan_simd_set1(1.0), sum_rgb);
            alwan_simd_store(&d0[i], L);
            alwan_simd_store(&d1[i], alwan_simd_select(tiny, zero, alwan_simd_mul(vr, inv_sum)));
            alwan_simd_store(&d2[i], alwan_simd_select(tiny, zero, alwan_simd_mul(vg, inv_sum)));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_prismatic r = alwan_rgb_to_prismatic_v(rgb);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.s; d2[i] = (alwan_simd_lane)r.h;
    }
}

int alwan_rgb_to_prismatic_map_interleave(alwan_scalar *prismatic_out, alwan_scalar const *rgb_in,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !prismatic_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        alwan__rgb_to_prismatic_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(prismatic_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__prismatic_to_rgb_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                            alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                            size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd eps = alwan_simd_set1(ALWAN_EPSILON);
        alwan_simd one = alwan_simd_set1(1.0);
        alwan_simd zero = alwan_simd_zero();
        for (; i + W <= n; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vs = alwan_simd_load(&c1[i]);
            alwan_simd vh = alwan_simd_load(&c2[i]);
            /* R_comp = 1 - s - h */
            alwan_simd R_comp = alwan_simd_sub(alwan_simd_sub(one, vs), vh);
            alwan_simd max_pqr = alwan__simd_max3(vs, vh, R_comp);
            alwan_simd_mask tiny = alwan_simd_cmplt(max_pqr, eps);
            alwan_simd sum_rgb = alwan_simd_select(tiny, zero, alwan_simd_div(vL, max_pqr));
            alwan_simd_store(&d0[i], alwan_simd_mul(vs, sum_rgb));
            alwan_simd_store(&d1[i], alwan_simd_mul(vh, sum_rgb));
            alwan_simd_store(&d2[i], alwan_simd_mul(R_comp, sum_rgb));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_prismatic prismatic = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_rgb r = alwan_prismatic_to_rgb_v(prismatic);
        d0[i] = (alwan_simd_lane)r.r; d1[i] = (alwan_simd_lane)r.g; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_prismatic_to_rgb_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *prismatic_in,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!prismatic_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, prismatic_in, processed, in_stride, tile);
        alwan__prismatic_to_rgb_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HCL  (SIMD kernels)
 * ---------------------------------------------------------------- */

static void alwan__rgb_to_hcl_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                      alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd const gamma_inv_Y0 = alwan_simd_set1((alwan_simd_lane)(3.0 / 100.0));
        alwan_simd const eps   = alwan_simd_set1((alwan_simd_lane)ALWAN_EPSILON);
        alwan_simd const zero  = alwan_simd_zero();
        alwan_simd const one   = alwan_simd_set1(1.0);
        alwan_simd const half  = alwan_simd_set1(0.5);
        alwan_simd const third = alwan_simd_set1((alwan_simd_lane)(1.0 / 3.0));
        alwan_simd const two_third  = alwan_simd_set1((alwan_simd_lane)(2.0 / 3.0));
        alwan_simd const four_third = alwan_simd_set1((alwan_simd_lane)(4.0 / 3.0));
        alwan_simd const pi    = alwan_simd_set1((alwan_simd_lane)ALWAN_PI);
        alwan_simd const half_pi     = alwan_simd_set1((alwan_simd_lane)(ALWAN_PI * 0.5));
        alwan_simd const neg_half_pi = alwan_simd_set1((alwan_simd_lane)(-ALWAN_PI * 0.5));
        for (; i + W <= n; i += W) {
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd mx = alwan__simd_max3(vr, vg, vb);
            alwan_simd mn = alwan__simd_min3(vr, vg, vb);

            /* Q = exp(min * gamma / (max * Y0)) when max > eps, else 1 */
            alwan_simd_mask mx_ok = alwan_simd_cmpgt(mx, eps);
            alwan_simd safe_mx = alwan_simd_select(mx_ok, mx, one);
            alwan_simd Q = alwan_simd_select(mx_ok,
                alwan_simd_exp(alwan_simd_mul(
                    alwan_simd_div(mn, safe_mx), gamma_inv_Y0)),
                one);

            /* L = (Q*max + (Q-1)*min) / 2 */
            alwan_simd L = alwan_simd_mul(half,
                alwan_simd_add(alwan_simd_mul(Q, mx),
                    alwan_simd_mul(alwan_simd_sub(Q, one), mn)));

            /* C = Q * (|r-g| + |g-b| + |b-r|) / 3 */
            alwan_simd r_g = alwan_simd_sub(vr, vg);
            alwan_simd g_b = alwan_simd_sub(vg, vb);
            alwan_simd b_r = alwan_simd_sub(vb, vr);
            alwan_simd C = alwan_simd_mul(Q, alwan_simd_mul(third,
                alwan_simd_add(alwan_simd_abs(r_g),
                    alwan_simd_add(alwan_simd_abs(g_b), alwan_simd_abs(b_r)))));

            /* h_temp = atan(g_b / r_g); fallback ±pi/2 when r_g≈0.
             * Must use atan (not atan2) — the 4-way H correction below
             * assumes atan output range [-pi/2, pi/2]. */
            alwan_simd_mask rg_small = alwan_simd_cmplt(alwan_simd_abs(r_g), eps);
            alwan_simd ratio = alwan_simd_div(g_b, alwan_simd_select(rg_small, one, r_g));
            alwan_simd h_temp = alwan_simd_select(rg_small,
                alwan_simd_select(alwan_simd_cmpge(g_b, zero), half_pi, neg_half_pi),
                alwan_simd_atan2(ratio, one));

            alwan_simd two_h_3  = alwan_simd_mul(two_third,  h_temp);
            alwan_simd four_h_3 = alwan_simd_mul(four_third, h_temp);

            /* H based on signs of r_g and g_b (branchless 4-way select) */
            alwan_simd_mask rg_pos = alwan_simd_cmpge(r_g, zero);
            alwan_simd_mask gb_pos = alwan_simd_cmpge(g_b, zero);
            alwan_simd H_rg_pos = alwan_simd_select(gb_pos, two_h_3, four_h_3);
            alwan_simd H_rg_neg = alwan_simd_select(gb_pos,
                alwan_simd_add(pi, four_h_3),
                alwan_simd_sub(two_h_3, pi));
            alwan_simd H = alwan_simd_select(alwan_simd_cmpgt(C, eps),
                alwan_simd_select(rg_pos, H_rg_pos, H_rg_neg),
                zero);

            alwan_simd_store(&d0[i], H);
            alwan_simd_store(&d1[i], C);
            alwan_simd_store(&d2[i], L);
        }
    }
#endif
    for (; i < n; i++) {
        alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_hcl r = alwan_rgb_to_hcl_v(rgb);
        d0[i] = (alwan_simd_lane)r.H; d1[i] = (alwan_simd_lane)r.C; d2[i] = (alwan_simd_lane)r.L;
    }
}

static void alwan__hcl_to_rgb_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                      alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                      size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_simd const eps  = alwan_simd_set1((alwan_simd_lane)ALWAN_EPSILON);
        alwan_simd const zero = alwan_simd_zero();
        alwan_simd const one  = alwan_simd_set1(1.0);
        alwan_simd const two  = alwan_simd_set1(2.0);
        alwan_simd const three = alwan_simd_set1(3.0);
        alwan_simd const four  = alwan_simd_set1(4.0);
        alwan_simd const gamma_inv_Y0 = alwan_simd_set1((alwan_simd_lane)(3.0 / 100.0));
        alwan_simd const pi    = alwan_simd_set1((alwan_simd_lane)ALWAN_PI);
        alwan_simd const r_p60  = alwan_simd_set1((alwan_simd_lane)(ALWAN_PI / 3.0));
        alwan_simd const r_p120 = alwan_simd_set1((alwan_simd_lane)(2.0 * ALWAN_PI / 3.0));
        alwan_simd const r_n60  = alwan_simd_set1((alwan_simd_lane)(-ALWAN_PI / 3.0));
        alwan_simd const r_n120 = alwan_simd_set1((alwan_simd_lane)(-2.0 * ALWAN_PI / 3.0));
        alwan_simd const three_half    = alwan_simd_set1(1.5);
        alwan_simd const three_quarter = alwan_simd_set1(0.75);
        for (; i + W <= n; i += W) {
            alwan_simd H = alwan_simd_load(&c0[i]);
            alwan_simd C = alwan_simd_load(&c1[i]);
            alwan_simd L = alwan_simd_load(&c2[i]);

            /* Q = exp((1 - 3C/(4L)) * gamma/Y0) when L > eps */
            alwan_simd_mask L_ok = alwan_simd_cmpgt(L, eps);
            alwan_simd safe_L = alwan_simd_select(L_ok, L, one);
            alwan_simd Q = alwan_simd_select(L_ok,
                alwan_simd_exp(alwan_simd_mul(
                    alwan_simd_sub(one, alwan_simd_div(
                        alwan_simd_mul(three, C),
                        alwan_simd_mul(four, safe_L))),
                    gamma_inv_Y0)),
                one);

            /* Min = (4L - 3C) / (4Q - 2) */
            alwan_simd denom = alwan_simd_sub(alwan_simd_mul(four, Q), two);
            alwan_simd_mask denom_ok = alwan_simd_cmpgt(alwan_simd_abs(denom), eps);
            alwan_simd Min = alwan_simd_select(L_ok,
                alwan_simd_select(denom_ok,
                    alwan_simd_div(
                        alwan_simd_sub(alwan_simd_mul(four, L), alwan_simd_mul(three, C)),
                        alwan_simd_select(denom_ok, denom, one)),
                    zero),
                zero);

            /* Max = Min + 3C/(2Q) */
            alwan_simd_mask Q_ok = alwan_simd_cmpgt(Q, eps);
            alwan_simd Max = alwan_simd_select(L_ok,
                alwan_simd_select(Q_ok,
                    alwan_simd_add(Min, alwan_simd_div(
                        alwan_simd_mul(three_half, C),
                        alwan_simd_select(Q_ok, Q, one))),
                    zero),
                zero);

            /* Sector 0 [0,60): tan(3H/2) */
            alwan_simd arg0 = alwan_simd_mul(three_half, H);
            alwan_simd s0 = alwan_simd_sin(arg0), c0_ = alwan_simd_cos(arg0);
            alwan_simd safe_c0 = alwan_simd_select(alwan_simd_cmpgt(alwan_simd_abs(c0_), eps), c0_, one);
            alwan_simd t0 = alwan_simd_div(s0, safe_c0);
            alwan_simd r0 = Max;
            alwan_simd g0 = alwan_simd_div(alwan_simd_add(alwan_simd_mul(Max, t0), Min),
                                            alwan_simd_add(one, t0));
            alwan_simd b0 = Min;

            /* Sectors 1,2 [60,180): tan(3(H-pi)/4) */
            alwan_simd arg1 = alwan_simd_mul(three_quarter, alwan_simd_sub(H, pi));
            alwan_simd s1 = alwan_simd_sin(arg1), c1_ = alwan_simd_cos(arg1);
            alwan_simd safe_c1 = alwan_simd_select(alwan_simd_cmpgt(alwan_simd_abs(c1_), eps), c1_, one);
            alwan_simd t1 = alwan_simd_div(s1, safe_c1);
            alwan_simd_mask t1_ok = alwan_simd_cmpgt(alwan_simd_abs(t1), eps);
            alwan_simd safe_t1 = alwan_simd_select(t1_ok, t1, one);

            alwan_simd r1 = alwan_simd_select(t1_ok,
                alwan_simd_div(alwan_simd_sub(
                    alwan_simd_mul(Max, alwan_simd_add(one, t1)), Min), safe_t1),
                Max);
            alwan_simd g1 = Max;
            alwan_simd b1 = Min;

            alwan_simd r2 = Min;
            alwan_simd g2 = Max;
            alwan_simd b2 = alwan_simd_sub(
                alwan_simd_mul(Max, alwan_simd_add(one, t1)),
                alwan_simd_mul(Min, t1));

            /* Sectors 3,4 [-120,0): tan(3H/4) */
            alwan_simd arg3 = alwan_simd_mul(three_quarter, H);
            alwan_simd s3 = alwan_simd_sin(arg3), c3_ = alwan_simd_cos(arg3);
            alwan_simd safe_c3 = alwan_simd_select(alwan_simd_cmpgt(alwan_simd_abs(c3_), eps), c3_, one);
            alwan_simd t3 = alwan_simd_div(s3, safe_c3);
            alwan_simd_mask t3_ok = alwan_simd_cmpgt(alwan_simd_abs(t3), eps);
            alwan_simd safe_t3 = alwan_simd_select(t3_ok, t3, one);

            alwan_simd r3 = Max;
            alwan_simd g3 = Min;
            alwan_simd b3 = alwan_simd_sub(
                alwan_simd_mul(Min, alwan_simd_add(one, t3)),
                alwan_simd_mul(Max, t3));

            alwan_simd r4 = alwan_simd_select(t3_ok,
                alwan_simd_div(alwan_simd_sub(
                    alwan_simd_mul(Min, alwan_simd_add(one, t3)), Max), safe_t3),
                Min);
            alwan_simd g4 = Min;
            alwan_simd b4 = Max;

            /* Sector 5 [-180,-120): tan(3(H+pi)/2) */
            alwan_simd arg5 = alwan_simd_mul(three_half, alwan_simd_add(H, pi));
            alwan_simd s5 = alwan_simd_sin(arg5), c5_ = alwan_simd_cos(arg5);
            alwan_simd safe_c5 = alwan_simd_select(alwan_simd_cmpgt(alwan_simd_abs(c5_), eps), c5_, one);
            alwan_simd t5 = alwan_simd_div(s5, safe_c5);
            alwan_simd r5 = Min;
            alwan_simd g5 = alwan_simd_div(alwan_simd_add(alwan_simd_mul(Min, t5), Max),
                                            alwan_simd_add(one, t5));
            alwan_simd b5 = Max;

            /* Branchless 6-way sector selection via nested selects */
            alwan_simd_mask h_ge_n120 = alwan_simd_cmpge(H, r_n120);
            alwan_simd r_45 = alwan_simd_select(h_ge_n120, r4, r5);
            alwan_simd g_45 = alwan_simd_select(h_ge_n120, g4, g5);
            alwan_simd b_45 = alwan_simd_select(h_ge_n120, b4, b5);

            alwan_simd_mask h_ge_n60 = alwan_simd_cmpge(H, r_n60);
            alwan_simd r_345 = alwan_simd_select(h_ge_n60, r3, r_45);
            alwan_simd g_345 = alwan_simd_select(h_ge_n60, g3, g_45);
            alwan_simd b_345 = alwan_simd_select(h_ge_n60, b3, b_45);

            alwan_simd_mask h_lt_120 = alwan_simd_cmplt(H, r_p120);
            alwan_simd r_12 = alwan_simd_select(h_lt_120, r1, r2);
            alwan_simd g_12 = alwan_simd_select(h_lt_120, g1, g2);
            alwan_simd b_12 = alwan_simd_select(h_lt_120, b1, b2);

            alwan_simd_mask h_lt_60 = alwan_simd_cmplt(H, r_p60);
            alwan_simd r_012 = alwan_simd_select(h_lt_60, r0, r_12);
            alwan_simd g_012 = alwan_simd_select(h_lt_60, g0, g_12);
            alwan_simd b_012 = alwan_simd_select(h_lt_60, b0, b_12);

            alwan_simd_mask h_pos = alwan_simd_cmpge(H, zero);
            alwan_simd_store(&d0[i], alwan_simd_select(h_pos, r_012, r_345));
            alwan_simd_store(&d1[i], alwan_simd_select(h_pos, g_012, g_345));
            alwan_simd_store(&d2[i], alwan_simd_select(h_pos, b_012, b_345));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_hcl hcl = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_rgb r = alwan_hcl_to_rgb_v(hcl);
        d0[i] = (alwan_simd_lane)r.r; d1[i] = (alwan_simd_lane)r.g; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_rgb_to_hcl_map_interleave(alwan_scalar *hcl_out, alwan_scalar const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !hcl_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        alwan__rgb_to_hcl_kernel(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NORM_AFFINE(d0, tile, 0.15915494309189533577, 0.5);
        alwan__store_tile_aos3(hcl_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_hcl_to_rgb_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *hcl_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hcl_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hcl_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_AFFINE(c0, tile, 6.28318530717958647692, -3.14159265358979323846);
        alwan__hcl_to_rgb_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> IHLS  (SIMD kernels)
 * ---------------------------------------------------------------- */

static void alwan__rgb_to_ihls_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                       alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M = ALWAN_EXT_IHLS_RGB_TO_YC1C2.m;
        alwan_simd a00 = alwan_simd_set1((alwan_simd_lane)M[0]);
        alwan_simd a01 = alwan_simd_set1((alwan_simd_lane)M[1]);
        alwan_simd a02 = alwan_simd_set1((alwan_simd_lane)M[2]);
        alwan_simd a10 = alwan_simd_set1((alwan_simd_lane)M[3]);
        alwan_simd a11 = alwan_simd_set1((alwan_simd_lane)M[4]);
        alwan_simd a12 = alwan_simd_set1((alwan_simd_lane)M[5]);
        alwan_simd a20 = alwan_simd_set1((alwan_simd_lane)M[6]);
        alwan_simd a21 = alwan_simd_set1((alwan_simd_lane)M[7]);
        alwan_simd a22 = alwan_simd_set1((alwan_simd_lane)M[8]);
        alwan_simd const eps    = alwan_simd_set1((alwan_simd_lane)ALWAN_EPSILON);
        alwan_simd const zero   = alwan_simd_zero();
        alwan_simd const one    = alwan_simd_set1(1.0);
        alwan_simd const neg_one = alwan_simd_set1(-1.0);
        alwan_simd const two_pi = alwan_simd_set1((alwan_simd_lane)(2.0 * ALWAN_PI));
        for (; i + W <= n; i += W) {
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd mx = alwan__simd_max3(vr, vg, vb);
            alwan_simd mn = alwan__simd_min3(vr, vg, vb);
            alwan_simd delta = alwan_simd_sub(mx, mn);

            /* Y, C1, C2 = M * [r,g,b] */
            alwan_simd Y  = alwan_simd_fmadd(a00, vr, alwan_simd_fmadd(a01, vg, alwan_simd_mul(a02, vb)));
            alwan_simd C1 = alwan_simd_fmadd(a10, vr, alwan_simd_fmadd(a11, vg, alwan_simd_mul(a12, vb)));
            alwan_simd C2 = alwan_simd_fmadd(a20, vr, alwan_simd_fmadd(a21, vg, alwan_simd_mul(a22, vb)));

            /* C_mag = sqrt(C1^2 + C2^2) */
            alwan_simd C_mag = alwan_simd_sqrt(alwan_simd_fmadd(C1, C1, alwan_simd_mul(C2, C2)));

            /* cosH = clamp(C1 / C_mag, -1, 1) */
            alwan_simd_mask mag_ok = alwan_simd_cmpgt(C_mag, eps);
            alwan_simd cosH = alwan_simd_select(mag_ok,
                alwan_simd_div(C1, alwan_simd_select(mag_ok, C_mag, one)), one);
            cosH = alwan_simd_clamp(cosH, neg_one, one);

            /* acos(cosH) = atan2(sqrt(1-cosH^2), cosH) */
            alwan_simd sinH = alwan_simd_sqrt(
                alwan_simd_sub(one, alwan_simd_mul(cosH, cosH)));
            alwan_simd H_temp = alwan_simd_atan2(sinH, cosH);

            /* If C2 > 0: H = 2pi - H_temp; else H = H_temp; achromatic: 0 */
            alwan_simd H = alwan_simd_select(mag_ok,
                alwan_simd_select(alwan_simd_cmple(C2, zero),
                    H_temp, alwan_simd_sub(two_pi, H_temp)),
                zero);

            alwan_simd_store(&d0[i], H);
            alwan_simd_store(&d1[i], Y);
            alwan_simd_store(&d2[i], delta);
        }
    }
#endif
    for (; i < n; i++) {
        alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_ihls r = alwan_rgb_to_ihls_v(rgb);
        d0[i] = (alwan_simd_lane)r.H; d1[i] = (alwan_simd_lane)r.L; d2[i] = (alwan_simd_lane)r.S;
    }
}

static void alwan__ihls_to_rgb_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                       alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                       size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W = ALWAN_SIMD_WIDTH;
        alwan_scalar const *M = ALWAN_EXT_IHLS_YC1C2_TO_RGB.m;
        alwan_simd a00 = alwan_simd_set1((alwan_simd_lane)M[0]);
        alwan_simd a01 = alwan_simd_set1((alwan_simd_lane)M[1]);
        alwan_simd a02 = alwan_simd_set1((alwan_simd_lane)M[2]);
        alwan_simd a10 = alwan_simd_set1((alwan_simd_lane)M[3]);
        alwan_simd a11 = alwan_simd_set1((alwan_simd_lane)M[4]);
        alwan_simd a12 = alwan_simd_set1((alwan_simd_lane)M[5]);
        alwan_simd a20 = alwan_simd_set1((alwan_simd_lane)M[6]);
        alwan_simd a21 = alwan_simd_set1((alwan_simd_lane)M[7]);
        alwan_simd a22 = alwan_simd_set1((alwan_simd_lane)M[8]);
        alwan_simd const eps = alwan_simd_set1((alwan_simd_lane)ALWAN_EPSILON);
        alwan_simd const zero = alwan_simd_zero();
        alwan_simd const pi_3     = alwan_simd_set1((alwan_simd_lane)(ALWAN_PI / 3.0));
        alwan_simd const two_pi_3 = alwan_simd_set1((alwan_simd_lane)(2.0 * ALWAN_PI / 3.0));
        alwan_simd const sqrt3_half = alwan_simd_set1((alwan_simd_lane)0.86602540378443864676);
        for (; i + W <= n; i += W) {
            alwan_simd H = alwan_simd_load(&c0[i]);
            alwan_simd Y = alwan_simd_load(&c1[i]);
            alwan_simd S = alwan_simd_load(&c2[i]);

            /* k = floor(H / (pi/3)), H_s = H - k*(pi/3) */
            alwan_simd k = alwan_simd_floor(alwan_simd_div(H, pi_3));
            alwan_simd H_s = alwan_simd_sub(H, alwan_simd_mul(k, pi_3));

            /* sin_val = sin(2pi/3 - H_s) */
            alwan_simd sin_val = alwan_simd_sin(alwan_simd_sub(two_pi_3, H_s));

            /* C_mag = sqrt(3)*S / (2*sin_val) when |sin_val| >= eps */
            alwan_simd_mask sin_ok = alwan_simd_cmpge(alwan_simd_abs(sin_val), eps);
            alwan_simd C_mag = alwan_simd_select(sin_ok,
                alwan_simd_div(alwan_simd_mul(sqrt3_half, S),
                    alwan_simd_select(sin_ok, sin_val, alwan_simd_set1(1.0))),
                zero);

            /* C1 = C_mag*cos(H), C2 = -C_mag*sin(H) */
            alwan_simd C1 = alwan_simd_mul(C_mag, alwan_simd_cos(H));
            alwan_simd C2 = alwan_simd_neg(alwan_simd_mul(C_mag, alwan_simd_sin(H)));

            /* RGB = M_inv * [Y, C1, C2] */
            alwan_simd_store(&d0[i], alwan_simd_fmadd(a00, Y, alwan_simd_fmadd(a01, C1, alwan_simd_mul(a02, C2))));
            alwan_simd_store(&d1[i], alwan_simd_fmadd(a10, Y, alwan_simd_fmadd(a11, C1, alwan_simd_mul(a12, C2))));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(a20, Y, alwan_simd_fmadd(a21, C1, alwan_simd_mul(a22, C2))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_ihls ihls = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_rgb r = alwan_ihls_to_rgb_v(ihls);
        d0[i] = (alwan_simd_lane)r.r; d1[i] = (alwan_simd_lane)r.g; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_rgb_to_ihls_map_interleave(alwan_scalar *ihls_out, alwan_scalar const *rgb_in,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !ihls_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        alwan__rgb_to_ihls_kernel(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NORM_MUL(d0, tile, 0.15915494309189533577);
        alwan__store_tile_aos3(ihls_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_ihls_to_rgb_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *ihls_in,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!ihls_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, ihls_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 6.28318530717958647692);
        alwan__ihls_to_rgb_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Lab <-> DIN99 (with int variant)
 * ---------------------------------------------------------------- */

static void alwan__lab_to_din99_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                        alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                        size_t n, int variant) {
    /* Precompute variant-dependent constants */
    int v = (variant < 0) ? 0 : (variant > 3) ? 3 : variant;
    alwan_scalar c3_rad = ALWAN_DIN99_COEFFS[v][2] * ALWAN_PI / 180.0;
    alwan_scalar c7_rad = ALWAN_DIN99_COEFFS[v][6] * ALWAN_PI / 180.0;
    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W_s = ALWAN_SIMD_WIDTH;
        alwan_simd v_c0 = alwan_simd_set1(ALWAN_DIN99_COEFFS[v][0]);
        alwan_simd v_c1 = alwan_simd_set1(ALWAN_DIN99_COEFFS[v][1]);
        alwan_simd v_c3f = alwan_simd_set1(ALWAN_DIN99_COEFFS[v][3]);
        alwan_simd v_c4 = alwan_simd_set1(ALWAN_DIN99_COEFFS[v][4]);
        alwan_simd v_c5 = alwan_simd_set1(ALWAN_DIN99_COEFFS[v][5]);
        alwan_simd v_c7_scale = alwan_simd_set1(ALWAN_DIN99_COEFFS[v][7]);
        alwan_simd v_cos = alwan_simd_set1(cos_c3);
        alwan_simd v_sin = alwan_simd_set1(sin_c3);
        alwan_simd v_nsin = alwan_simd_set1(-sin_c3);
        alwan_simd v_c7r = alwan_simd_set1(c7_rad);
        alwan_simd one = alwan_simd_set1(1.0);
        alwan_simd zero = alwan_simd_zero();
        alwan_simd chroma_guard = alwan_simd_set1(1e-12);
        for (; i + W_s <= n; i += W_s) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            /* L99 = c0 * ln(1 + c1*L) */
            alwan_simd L99 = alwan_simd_mul(v_c0, alwan_simd_log(alwan_simd_fmadd(v_c1, vL, one)));
            /* Achromatic guard */
            alwan_simd chroma_sq = alwan_simd_fmadd(va, va, alwan_simd_mul(vb, vb));
            alwan_simd_mask achromatic = alwan_simd_cmplt(chroma_sq, chroma_guard);
            /* e = cos*a + sin*b; f = c3f * (-sin*a + cos*b) */
            alwan_simd e = alwan_simd_fmadd(v_cos, va, alwan_simd_mul(v_sin, vb));
            alwan_simd f = alwan_simd_mul(v_c3f, alwan_simd_fmadd(v_nsin, va, alwan_simd_mul(v_cos, vb)));
            /* G = sqrt(e*e + f*f) */
            alwan_simd G = alwan_simd_sqrt(alwan_simd_fmadd(e, e, alwan_simd_mul(f, f)));
            /* h_ef = atan2(f, e) + c7_rad */
            alwan_simd h_ef = alwan_simd_add(alwan_simd_atan2(f, e), v_c7r);
            /* C99 = c4 * ln(1 + c5*G) / c7_scale */
            alwan_simd C99 = alwan_simd_div(alwan_simd_mul(v_c4, alwan_simd_log(alwan_simd_fmadd(v_c5, G, one))), v_c7_scale);
            /* a99, b99 with achromatic guard */
            alwan_simd_store(&d0[i], L99);
            alwan_simd_store(&d1[i], alwan_simd_select(achromatic, zero, alwan_simd_mul(C99, alwan_simd_cos(h_ef))));
            alwan_simd_store(&d2[i], alwan_simd_select(achromatic, zero, alwan_simd_mul(C99, alwan_simd_sin(h_ef))));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_lab lab = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_din99 r = alwan_lab_to_din99_v(lab, variant);
        d0[i] = (alwan_simd_lane)r.L99; d1[i] = (alwan_simd_lane)r.a99; d2[i] = (alwan_simd_lane)r.b99;
    }
}

int alwan_lab_to_din99_map_interleave(alwan_scalar *din99_out, alwan_scalar const *lab_in,
                            int variant,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!lab_in || !din99_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, lab_in, processed, in_stride, tile);
        alwan__lab_to_din99_kernel(d0, d1, d2, c0, c1, c2, tile, variant);
        alwan__store_tile_aos3(din99_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

static void alwan__din99_to_lab_kernel(alwan_simd_lane *d0, alwan_simd_lane *d1, alwan_simd_lane *d2,
                                        alwan_simd_lane const *c0, alwan_simd_lane const *c1, alwan_simd_lane const *c2,
                                        size_t n, int variant) {
    /* Precompute variant-dependent constants */
    int v = (variant < 0) ? 0 : (variant > 3) ? 3 : variant;
    alwan_scalar c3_rad = ALWAN_DIN99_COEFFS[v][2] * ALWAN_PI / 180.0;
    alwan_scalar c7_rad = ALWAN_DIN99_COEFFS[v][6] * ALWAN_PI / 180.0;
    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);
    alwan_scalar inv_c3f = 1.0 / ALWAN_DIN99_COEFFS[v][3];
    alwan_scalar c7_over_c4 = ALWAN_DIN99_COEFFS[v][7] / ALWAN_DIN99_COEFFS[v][4];
    alwan_scalar inv_c5 = 1.0 / ALWAN_DIN99_COEFFS[v][5];
    alwan_scalar inv_c0 = 1.0 / ALWAN_DIN99_COEFFS[v][0];
    alwan_scalar inv_c1 = 1.0 / ALWAN_DIN99_COEFFS[v][1];
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    {
        size_t const W_s = ALWAN_SIMD_WIDTH;
        alwan_simd v_c7r = alwan_simd_set1(c7_rad);
        alwan_simd v_c7_c4 = alwan_simd_set1(c7_over_c4);
        alwan_simd v_inv_c5 = alwan_simd_set1(inv_c5);
        alwan_simd v_cos = alwan_simd_set1(cos_c3);
        alwan_simd v_sin = alwan_simd_set1(sin_c3);
        alwan_simd v_nsin = alwan_simd_set1(-sin_c3);
        alwan_simd v_inv_c3f = alwan_simd_set1(inv_c3f);
        alwan_simd v_inv_c0 = alwan_simd_set1(inv_c0);
        alwan_simd v_inv_c1 = alwan_simd_set1(inv_c1);
        alwan_simd one = alwan_simd_set1(1.0);
        for (; i + W_s <= n; i += W_s) {
            alwan_simd vL99 = alwan_simd_load(&c0[i]);
            alwan_simd va99 = alwan_simd_load(&c1[i]);
            alwan_simd vb99 = alwan_simd_load(&c2[i]);
            /* h99 = atan2(b99, a99) - c7_rad */
            alwan_simd h99 = alwan_simd_sub(alwan_simd_atan2(vb99, va99), v_c7r);
            /* C99 = sqrt(a99^2 + b99^2) */
            alwan_simd C99 = alwan_simd_sqrt(alwan_simd_fmadd(va99, va99, alwan_simd_mul(vb99, vb99)));
            /* G = (exp(c7/c4 * C99) - 1) / c5 */
            alwan_simd G = alwan_simd_mul(alwan_simd_sub(alwan_simd_exp(alwan_simd_mul(v_c7_c4, C99)), one), v_inv_c5);
            /* e = G*cos(h99), f = G*sin(h99) */
            alwan_simd e = alwan_simd_mul(G, alwan_simd_cos(h99));
            alwan_simd f = alwan_simd_mul(G, alwan_simd_sin(h99));
            /* a = e*cos_c3 - (f/c3f)*sin_c3; b = e*sin_c3 + (f/c3f)*cos_c3 */
            alwan_simd f_scaled = alwan_simd_mul(f, v_inv_c3f);
            alwan_simd_store(&d1[i], alwan_simd_fmadd(e, v_cos, alwan_simd_mul(f_scaled, v_nsin)));
            alwan_simd_store(&d2[i], alwan_simd_fmadd(e, v_sin, alwan_simd_mul(f_scaled, v_cos)));
            /* L = (exp(L99/c0) - 1) / c1 */
            alwan_simd_store(&d0[i], alwan_simd_mul(alwan_simd_sub(alwan_simd_exp(alwan_simd_mul(vL99, v_inv_c0)), one), v_inv_c1));
        }
    }
#endif
    for (; i < n; i++) {
        alwan_din99 din99 = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
        alwan_lab r = alwan_din99_to_lab_v(din99, variant);
        d0[i] = (alwan_simd_lane)r.L; d1[i] = (alwan_simd_lane)r.a; d2[i] = (alwan_simd_lane)r.b;
    }
}

int alwan_din99_to_lab_map_interleave(alwan_scalar *lab_out, alwan_scalar const *din99_in,
                            int variant,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!din99_in || !lab_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, din99_in, processed, in_stride, tile);
        alwan__din99_to_lab_kernel(d0, d1, d2, c0, c1, c2, tile, variant);
        alwan__store_tile_aos3(lab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants (tiled, reusing SIMD kernels)
 * ================================================================ */

/* --- Simple kernel planar functions (no extra params) --- */

#define ALWAN_PLANAR_FROM_KERNEL(name, kernel) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    ALWAN_MAP3_TILED_PLANAR(in0, in1, in2, in_stride, out0, out1, out2, out_stride, count, kernel); \
    return ALWAN_OK; \
}

/* XYZ <-> IgPgTg */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_igpgtg_map_planar,  alwan__xyz_to_igpgtg_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_igpgtg_to_xyz_map_planar,  alwan__igpgtg_to_xyz_kernel)

/* XYZ <-> ICaCb */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_icacb_map_planar,   alwan__xyz_to_icacb_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_icacb_to_xyz_map_planar,   alwan__icacb_to_xyz_kernel)

/* XYZ <-> hdr-CIELAB */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_hdr_cielab_map_planar, alwan__xyz_to_hdr_cielab_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_hdr_cielab_to_xyz_map_planar, alwan__hdr_cielab_to_xyz_kernel)

/* XYZ <-> hdr-IPT */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_hdr_ipt_map_planar, alwan__xyz_to_hdr_ipt_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_hdr_ipt_to_xyz_map_planar, alwan__hdr_ipt_to_xyz_kernel)

/* XYZ <-> UCS */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_ucs_map_planar,     alwan__xyz_to_ucs_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_ucs_to_xyz_map_planar,     alwan__ucs_to_xyz_kernel)

/* XYZ <-> OSA-UCS */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_osa_ucs_map_planar, alwan__xyz_to_osa_ucs_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_osa_ucs_to_xyz_map_planar, alwan__osa_ucs_to_xyz_kernel)

/* XYZ <-> Hunter Lab */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_hunter_lab_map_planar,  alwan__xyz_to_hunter_lab_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_hunter_lab_to_xyz_map_planar,  alwan__hunter_lab_to_xyz_kernel)

/* XYZ <-> ProLab */
ALWAN_PLANAR_FROM_KERNEL(alwan_xyz_to_prolab_map_planar,  alwan__xyz_to_prolab_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_prolab_to_xyz_map_planar,  alwan__prolab_to_xyz_kernel)

/* RGB <-> Prismatic */
ALWAN_PLANAR_FROM_KERNEL(alwan_rgb_to_prismatic_map_planar,  alwan__rgb_to_prismatic_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_prismatic_to_rgb_map_planar,  alwan__prismatic_to_rgb_kernel)

/* RGB <-> HCL */
ALWAN_PLANAR_FROM_KERNEL(alwan_rgb_to_hcl_map_planar,  alwan__rgb_to_hcl_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_hcl_to_rgb_map_planar,  alwan__hcl_to_rgb_kernel)

/* RGB <-> IHLS */
ALWAN_PLANAR_FROM_KERNEL(alwan_rgb_to_ihls_map_planar, alwan__rgb_to_ihls_kernel)
ALWAN_PLANAR_FROM_KERNEL(alwan_ihls_to_rgb_map_planar, alwan__ihls_to_rgb_kernel)

#undef ALWAN_PLANAR_FROM_KERNEL

/* --- Extra-param planar functions (white_xyz / variant) --- */

#define ALWAN_PLANAR_TILED_WHITE(name, kernel) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         alwan_xyz const *white_xyz, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    size_t off_ = 0; \
    while (off_ < count) { \
        size_t tile_ = count - off_; \
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
        ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
        ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
        alwan__load_tile_planar3(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_); \
        kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, white_xyz); \
        alwan__store_tile_planar3(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_); \
        off_ += tile_; \
    } \
    return ALWAN_OK; \
}

/* XYZ <-> Hunter Lab custom */
ALWAN_PLANAR_TILED_WHITE(alwan_xyz_to_hunter_lab_custom_map_planar,  alwan__xyz_to_hunter_lab_custom_kernel)
ALWAN_PLANAR_TILED_WHITE(alwan_hunter_lab_to_xyz_custom_map_planar,  alwan__hunter_lab_to_xyz_custom_kernel)

/* XYZ <-> ProLab custom */
ALWAN_PLANAR_TILED_WHITE(alwan_xyz_to_prolab_custom_map_planar,  alwan__xyz_to_prolab_custom_kernel)
ALWAN_PLANAR_TILED_WHITE(alwan_prolab_to_xyz_custom_map_planar,  alwan__prolab_to_xyz_custom_kernel)

/* XYZ <-> UVW */
ALWAN_PLANAR_TILED_WHITE(alwan_xyz_to_uvw_map_planar, alwan__xyz_to_uvw_kernel)
ALWAN_PLANAR_TILED_WHITE(alwan_uvw_to_xyz_map_planar, alwan__uvw_to_xyz_kernel)

#undef ALWAN_PLANAR_TILED_WHITE

/* Lab <-> DIN99 (int variant param) */

#define ALWAN_PLANAR_TILED_INT(name, kernel) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         int variant, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    size_t off_ = 0; \
    while (off_ < count) { \
        size_t tile_ = count - off_; \
        if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
        ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
        ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
        alwan__load_tile_planar3(ci0_, ci1_, ci2_, in0, in1, in2, off_, in_stride, tile_); \
        kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_, variant); \
        alwan__store_tile_planar3(out0, out1, out2, off_, out_stride, co0_, co1_, co2_, tile_); \
        off_ += tile_; \
    } \
    return ALWAN_OK; \
}

ALWAN_PLANAR_TILED_INT(alwan_lab_to_din99_map_planar, alwan__lab_to_din99_kernel)
ALWAN_PLANAR_TILED_INT(alwan_din99_to_lab_map_planar, alwan__din99_to_lab_kernel)

#undef ALWAN_PLANAR_TILED_INT
