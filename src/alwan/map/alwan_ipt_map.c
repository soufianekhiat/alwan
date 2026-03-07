/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map IPT Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_ipt_core.h"

int alwan_xyz_to_ipt_map_interleave(alwan_scalar *ipt_out, alwan_scalar const *xyz_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !ipt_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const expo = alwan_simd_set1((alwan_simd_lane)IPT_V_EXPONENT);
    alwan_simd const zero = alwan_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);

            /* XYZ -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &IPT_V_XYZ_TO_LMS, vx, vy, vz);

            /* Sign-preserving power: sign(x) * |x|^0.43 */
            alwan_simd_mask ml = alwan_simd_cmpge(vl, zero);
            alwan_simd_mask mm = alwan_simd_cmpge(vm, zero);
            alwan_simd_mask ms = alwan_simd_cmpge(vs, zero);
            alwan_simd lp = alwan_simd_pow(alwan_simd_abs(vl), expo);
            alwan_simd mp = alwan_simd_pow(alwan_simd_abs(vm), expo);
            alwan_simd sp = alwan_simd_pow(alwan_simd_abs(vs), expo);
            lp = alwan_simd_select(ml, lp, alwan_simd_neg(lp));
            mp = alwan_simd_select(mm, mp, alwan_simd_neg(mp));
            sp = alwan_simd_select(ms, sp, alwan_simd_neg(sp));

            /* LMS' -> IPT */
            alwan_simd oI, oP, oT;
            alwan__mat3_mul_simd(&oI, &oP, &oT, &IPT_V_LMS_P_TO_IPT, lp, mp, sp);

            alwan_simd_store(&c0[i], oI);
            alwan_simd_store(&c1[i], oP);
            alwan_simd_store(&c2[i], oT);
        }
        for (; i < tile; i++) {
            alwan_xyz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ipt r = alwan_xyz_to_ipt_v(v);
            c0[i] = (alwan_simd_lane)r.I; c1[i] = (alwan_simd_lane)r.P; c2[i] = (alwan_simd_lane)r.T;
        }

        alwan__store_tile_aos3(ipt_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ipt_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_ipt ipt;
        alwan_xyz_to_ipt(&ipt, &xyz);
        out_ptr[0] = ipt.I; out_ptr[1] = ipt.P; out_ptr[2] = ipt.T;
    }
#endif
    return ALWAN_OK;
}

int alwan_ipt_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *ipt_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!ipt_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const inv_expo = alwan_simd_set1((alwan_simd_lane)(1.0 / IPT_V_EXPONENT));
    alwan_simd const zero = alwan_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, ipt_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vI = alwan_simd_load(&c0[i]);
            alwan_simd vP = alwan_simd_load(&c1[i]);
            alwan_simd vT = alwan_simd_load(&c2[i]);

            /* IPT -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, &IPT_V_IPT_TO_LMS_P, vI, vP, vT);

            /* Inverse sign-preserving power: sign(x) * |x|^(1/0.43) */
            alwan_simd_mask ml = alwan_simd_cmpge(lp, zero);
            alwan_simd_mask mm = alwan_simd_cmpge(mp, zero);
            alwan_simd_mask ms = alwan_simd_cmpge(sp, zero);
            alwan_simd vl = alwan_simd_pow(alwan_simd_abs(lp), inv_expo);
            alwan_simd vm = alwan_simd_pow(alwan_simd_abs(mp), inv_expo);
            alwan_simd vs = alwan_simd_pow(alwan_simd_abs(sp), inv_expo);
            vl = alwan_simd_select(ml, vl, alwan_simd_neg(vl));
            vm = alwan_simd_select(mm, vm, alwan_simd_neg(vm));
            vs = alwan_simd_select(ms, vs, alwan_simd_neg(vs));

            /* LMS -> XYZ */
            alwan_simd ox, oy, oz;
            alwan__mat3_mul_simd(&ox, &oy, &oz, &IPT_V_LMS_TO_XYZ, vl, vm, vs);

            alwan_simd_store(&c0[i], ox);
            alwan_simd_store(&c1[i], oy);
            alwan_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            alwan_ipt v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz r = alwan_ipt_to_xyz_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.z;
        }

        alwan__store_tile_aos3(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ipt_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_ipt ipt = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_xyz xyz;
        alwan_ipt_to_xyz(&xyz, &ipt);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int alwan_xyz_to_ipt_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const expo = alwan_simd_set1((alwan_simd_lane)IPT_V_EXPONENT);
    alwan_simd const zero = alwan_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vx = alwan_simd_load(&c0[i]);
            alwan_simd vy = alwan_simd_load(&c1[i]);
            alwan_simd vz = alwan_simd_load(&c2[i]);

            /* XYZ -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &IPT_V_XYZ_TO_LMS, vx, vy, vz);

            /* Sign-preserving power: sign(x) * |x|^0.43 */
            alwan_simd_mask ml = alwan_simd_cmpge(vl, zero);
            alwan_simd_mask mm = alwan_simd_cmpge(vm, zero);
            alwan_simd_mask ms = alwan_simd_cmpge(vs, zero);
            alwan_simd lp = alwan_simd_pow(alwan_simd_abs(vl), expo);
            alwan_simd mp = alwan_simd_pow(alwan_simd_abs(vm), expo);
            alwan_simd sp = alwan_simd_pow(alwan_simd_abs(vs), expo);
            lp = alwan_simd_select(ml, lp, alwan_simd_neg(lp));
            mp = alwan_simd_select(mm, mp, alwan_simd_neg(mp));
            sp = alwan_simd_select(ms, sp, alwan_simd_neg(sp));

            /* LMS' -> IPT */
            alwan_simd oI, oP, oT;
            alwan__mat3_mul_simd(&oI, &oP, &oT, &IPT_V_LMS_P_TO_IPT, lp, mp, sp);

            alwan_simd_store(&c0[i], oI);
            alwan_simd_store(&c1[i], oP);
            alwan_simd_store(&c2[i], oT);
        }
        for (; i < tile; i++) {
            alwan_xyz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ipt r = alwan_xyz_to_ipt_v(v);
            c0[i] = (alwan_simd_lane)r.I; c1[i] = (alwan_simd_lane)r.P; c2[i] = (alwan_simd_lane)r.T;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_xyz xyz = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_ipt ipt;
        alwan_xyz_to_ipt(&ipt, &xyz);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = ipt.I;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = ipt.P;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = ipt.T;
    }
#endif
    return ALWAN_OK;
}

int alwan_ipt_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                 alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const inv_expo = alwan_simd_set1((alwan_simd_lane)(1.0 / IPT_V_EXPONENT));
    alwan_simd const zero = alwan_simd_zero();

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vI = alwan_simd_load(&c0[i]);
            alwan_simd vP = alwan_simd_load(&c1[i]);
            alwan_simd vT = alwan_simd_load(&c2[i]);

            /* IPT -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, &IPT_V_IPT_TO_LMS_P, vI, vP, vT);

            /* Inverse sign-preserving power: sign(x) * |x|^(1/0.43) */
            alwan_simd_mask ml = alwan_simd_cmpge(lp, zero);
            alwan_simd_mask mm = alwan_simd_cmpge(mp, zero);
            alwan_simd_mask ms = alwan_simd_cmpge(sp, zero);
            alwan_simd vl = alwan_simd_pow(alwan_simd_abs(lp), inv_expo);
            alwan_simd vm = alwan_simd_pow(alwan_simd_abs(mp), inv_expo);
            alwan_simd vs = alwan_simd_pow(alwan_simd_abs(sp), inv_expo);
            vl = alwan_simd_select(ml, vl, alwan_simd_neg(vl));
            vm = alwan_simd_select(mm, vm, alwan_simd_neg(vm));
            vs = alwan_simd_select(ms, vs, alwan_simd_neg(vs));

            /* LMS -> XYZ */
            alwan_simd ox, oy, oz;
            alwan__mat3_mul_simd(&ox, &oy, &oz, &IPT_V_LMS_TO_XYZ, vl, vm, vs);

            alwan_simd_store(&c0[i], ox);
            alwan_simd_store(&c1[i], oy);
            alwan_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            alwan_ipt v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz r = alwan_ipt_to_xyz_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.z;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_ipt ipt = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_xyz xyz;
        alwan_ipt_to_xyz(&xyz, &ipt);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}
