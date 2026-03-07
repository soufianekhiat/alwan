/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map JzAzBz Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_jzazbz_core.h"

int alwan_xyz_to_jzazbz_map_interleave(alwan_scalar *jzazbz_out, alwan_scalar const *xyz_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !jzazbz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const vB      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_B);
    alwan_simd const vBm1    = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_B - 1.0));
    alwan_simd const vG      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_G);
    alwan_simd const vGm1    = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_G - 1.0));
    alwan_simd const vD1     = alwan_simd_set1((alwan_simd_lane)(1.0 + JZAZBZ_V_D));
    alwan_simd const vD      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D);
    alwan_simd const vD0     = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D0);
    alwan_simd const one     = alwan_simd_set1((alwan_simd_lane)1.0);
    alwan_simd const zero    = alwan_simd_zero();

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

            /* Chromatic adaptation: xa = B*x - (B-1)*z, ya = G*y - (G-1)*x */
            alwan_simd xa = alwan_simd_fmsub(vB, vx, alwan_simd_mul(vBm1, vz));
            alwan_simd ya = alwan_simd_fmsub(vG, vy, alwan_simd_mul(vGm1, vx));

            /* XYZ(adapted) -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &JZAZBZ_V_XYZ_TO_LMS, xa, ya, vz);

            /* PQ OETF */
            vl = alwan__pq_jz_oetf_simd(vl);
            vm = alwan__pq_jz_oetf_simd(vm);
            vs = alwan__pq_jz_oetf_simd(vs);

            /* LMS' -> IzAzBz */
            alwan_simd iz, az, bz;
            alwan__mat3_mul_simd(&iz, &az, &bz, &JZAZBZ_V_LMS_P_TO_IZAZBZ, vl, vm, vs);

            /* Jz = (1+D)*Iz / (1+D*Iz) - D0, clamped to >= 0 */
            alwan_simd jz = alwan_simd_sub(
                alwan_simd_div(alwan_simd_mul(vD1, iz),
                               alwan_simd_add(one, alwan_simd_mul(vD, iz))),
                vD0);
            jz = alwan_simd_select(alwan_simd_cmplt(jz, zero), zero, jz);

            alwan_simd_store(&c0[i], jz);
            alwan_simd_store(&c1[i], az);
            alwan_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            alwan_xyz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_jzazbz r = alwan_xyz_to_jzazbz_v(v);
            c0[i] = (alwan_simd_lane)r.Jz; c1[i] = (alwan_simd_lane)r.az; c2[i] = (alwan_simd_lane)r.bz;
        }

        alwan__store_tile_aos3(jzazbz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)jzazbz_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_jzazbz jz;
        alwan_xyz_to_jzazbz(&jz, &xyz);
        out_ptr[0] = jz.Jz; out_ptr[1] = jz.az; out_ptr[2] = jz.bz;
    }
#endif
    return ALWAN_OK;
}

int alwan_jzazbz_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *jzazbz_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!jzazbz_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const vD      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D);
    alwan_simd const vD0     = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D0);
    alwan_simd const vD1     = alwan_simd_set1((alwan_simd_lane)(1.0 + JZAZBZ_V_D));
    alwan_simd const inv_B   = alwan_simd_set1((alwan_simd_lane)(1.0 / JZAZBZ_V_B));
    alwan_simd const inv_G   = alwan_simd_set1((alwan_simd_lane)(1.0 / JZAZBZ_V_G));
    alwan_simd const Bm1     = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_B - 1.0));
    alwan_simd const Gm1     = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_G - 1.0));

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, jzazbz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd jz = alwan_simd_load(&c0[i]);
            alwan_simd az = alwan_simd_load(&c1[i]);
            alwan_simd bz = alwan_simd_load(&c2[i]);

            /* Recover Iz from Jz: Iz = (Jz + D0) / (1 + D - D*(Jz + D0)) */
            alwan_simd jz_d0 = alwan_simd_add(jz, vD0);
            alwan_simd iz = alwan_simd_div(jz_d0,
                alwan_simd_sub(vD1, alwan_simd_mul(vD, jz_d0)));

            /* IzAzBz -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, &JZAZBZ_V_IZAZBZ_TO_LMS_P, iz, az, bz);

            /* PQ EOTF */
            alwan_simd vl = alwan__pq_jz_eotf_simd(lp);
            alwan_simd vm = alwan__pq_jz_eotf_simd(mp);
            alwan_simd vs = alwan__pq_jz_eotf_simd(sp);

            /* LMS -> XYZ(adapted) */
            alwan_simd xa, ya, za;
            alwan__mat3_mul_simd(&xa, &ya, &za, &JZAZBZ_V_LMS_TO_XYZ, vl, vm, vs);

            /* Inverse chromatic adaptation:
             * x = (xa + (B-1)*za) / B
             * y = (ya + (G-1)*x) / G
             * z = za */
            alwan_simd vx = alwan_simd_mul(alwan_simd_fmadd(Bm1, za, xa), inv_B);
            alwan_simd vy = alwan_simd_mul(alwan_simd_fmadd(Gm1, vx, ya), inv_G);

            alwan_simd_store(&c0[i], vx);
            alwan_simd_store(&c1[i], vy);
            alwan_simd_store(&c2[i], za);
        }
        for (; i < tile; i++) {
            alwan_jzazbz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz r = alwan_jzazbz_to_xyz_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.z;
        }

        alwan__store_tile_aos3(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)jzazbz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_jzazbz jz = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_xyz xyz;
        alwan_jzazbz_to_xyz(&xyz, &jz);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

int alwan_jzazbz_to_jzczhz_map_interleave(alwan_scalar *jzczhz_out, alwan_scalar const *jzazbz_in,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!jzazbz_in || !jzczhz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, jzazbz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd jz = alwan_simd_load(&c0[i]);
            alwan_simd az = alwan_simd_load(&c1[i]);
            alwan_simd bz = alwan_simd_load(&c2[i]);
            alwan_simd Cz = alwan_simd_sqrt(
                alwan_simd_fmadd(az, az, alwan_simd_mul(bz, bz)));
            alwan_simd hz = alwan_simd_atan2(bz, az);
            alwan_simd_store(&c0[i], jz);
            alwan_simd_store(&c1[i], Cz);
            alwan_simd_store(&c2[i], hz);
        }
        for (; i < tile; i++) {
            alwan_jzazbz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_jzczhz r = alwan_jzazbz_to_jzczhz_v(v);
            c0[i] = (alwan_simd_lane)r.Jz; c1[i] = (alwan_simd_lane)r.Cz; c2[i] = (alwan_simd_lane)r.hz;
        }

        alwan__store_tile_aos3(jzczhz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)jzazbz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)jzczhz_out + i * out_stride);
        alwan_jzazbz jz = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_jzczhz jzch;
        alwan_jzazbz_to_jzczhz(&jzch, &jz);
        out_ptr[0] = jzch.Jz; out_ptr[1] = jzch.Cz; out_ptr[2] = jzch.hz;
    }
#endif
    return ALWAN_OK;
}

int alwan_jzczhz_to_jzazbz_map_interleave(alwan_scalar *jzazbz_out, alwan_scalar const *jzczhz_in,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!jzczhz_in || !jzazbz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, jzczhz_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd jz = alwan_simd_load(&c0[i]);
            alwan_simd Cz = alwan_simd_load(&c1[i]);
            alwan_simd hz = alwan_simd_load(&c2[i]);
            alwan_simd az = alwan_simd_mul(Cz, alwan_simd_cos(hz));
            alwan_simd bz = alwan_simd_mul(Cz, alwan_simd_sin(hz));
            alwan_simd_store(&c0[i], jz);
            alwan_simd_store(&c1[i], az);
            alwan_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            alwan_jzczhz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_jzazbz r = alwan_jzczhz_to_jzazbz_v(v);
            c0[i] = (alwan_simd_lane)r.Jz; c1[i] = (alwan_simd_lane)r.az; c2[i] = (alwan_simd_lane)r.bz;
        }

        alwan__store_tile_aos3(jzazbz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)jzczhz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)jzazbz_out + i * out_stride);
        alwan_jzczhz jzch = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_jzazbz jz;
        alwan_jzczhz_to_jzazbz(&jz, &jzch);
        out_ptr[0] = jz.Jz; out_ptr[1] = jz.az; out_ptr[2] = jz.bz;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int alwan_xyz_to_jzazbz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                    alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const vB      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_B);
    alwan_simd const vBm1    = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_B - 1.0));
    alwan_simd const vG      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_G);
    alwan_simd const vGm1    = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_G - 1.0));
    alwan_simd const vD1     = alwan_simd_set1((alwan_simd_lane)(1.0 + JZAZBZ_V_D));
    alwan_simd const vD      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D);
    alwan_simd const vD0     = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D0);
    alwan_simd const one     = alwan_simd_set1((alwan_simd_lane)1.0);
    alwan_simd const zero    = alwan_simd_zero();

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

            /* Chromatic adaptation: xa = B*x - (B-1)*z, ya = G*y - (G-1)*x */
            alwan_simd xa = alwan_simd_fmsub(vB, vx, alwan_simd_mul(vBm1, vz));
            alwan_simd ya = alwan_simd_fmsub(vG, vy, alwan_simd_mul(vGm1, vx));

            /* XYZ(adapted) -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &JZAZBZ_V_XYZ_TO_LMS, xa, ya, vz);

            /* PQ OETF */
            vl = alwan__pq_jz_oetf_simd(vl);
            vm = alwan__pq_jz_oetf_simd(vm);
            vs = alwan__pq_jz_oetf_simd(vs);

            /* LMS' -> IzAzBz */
            alwan_simd iz, az, bz;
            alwan__mat3_mul_simd(&iz, &az, &bz, &JZAZBZ_V_LMS_P_TO_IZAZBZ, vl, vm, vs);

            /* Jz = (1+D)*Iz / (1+D*Iz) - D0, clamped to >= 0 */
            alwan_simd jz = alwan_simd_sub(
                alwan_simd_div(alwan_simd_mul(vD1, iz),
                               alwan_simd_add(one, alwan_simd_mul(vD, iz))),
                vD0);
            jz = alwan_simd_select(alwan_simd_cmplt(jz, zero), zero, jz);

            alwan_simd_store(&c0[i], jz);
            alwan_simd_store(&c1[i], az);
            alwan_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            alwan_xyz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_jzazbz r = alwan_xyz_to_jzazbz_v(v);
            c0[i] = (alwan_simd_lane)r.Jz; c1[i] = (alwan_simd_lane)r.az; c2[i] = (alwan_simd_lane)r.bz;
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
        alwan_jzazbz jz;
        alwan_xyz_to_jzazbz(&jz, &xyz);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = jz.Jz;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = jz.az;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = jz.bz;
    }
#endif
    return ALWAN_OK;
}

int alwan_jzazbz_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                    alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd const vD      = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D);
    alwan_simd const vD0     = alwan_simd_set1((alwan_simd_lane)JZAZBZ_V_D0);
    alwan_simd const vD1     = alwan_simd_set1((alwan_simd_lane)(1.0 + JZAZBZ_V_D));
    alwan_simd const inv_B   = alwan_simd_set1((alwan_simd_lane)(1.0 / JZAZBZ_V_B));
    alwan_simd const inv_G   = alwan_simd_set1((alwan_simd_lane)(1.0 / JZAZBZ_V_G));
    alwan_simd const Bm1     = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_B - 1.0));
    alwan_simd const Gm1     = alwan_simd_set1((alwan_simd_lane)(JZAZBZ_V_G - 1.0));

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd jz = alwan_simd_load(&c0[i]);
            alwan_simd az = alwan_simd_load(&c1[i]);
            alwan_simd bz = alwan_simd_load(&c2[i]);

            /* Recover Iz from Jz: Iz = (Jz + D0) / (1 + D - D*(Jz + D0)) */
            alwan_simd jz_d0 = alwan_simd_add(jz, vD0);
            alwan_simd iz = alwan_simd_div(jz_d0,
                alwan_simd_sub(vD1, alwan_simd_mul(vD, jz_d0)));

            /* IzAzBz -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, &JZAZBZ_V_IZAZBZ_TO_LMS_P, iz, az, bz);

            /* PQ EOTF */
            alwan_simd vl = alwan__pq_jz_eotf_simd(lp);
            alwan_simd vm = alwan__pq_jz_eotf_simd(mp);
            alwan_simd vs = alwan__pq_jz_eotf_simd(sp);

            /* LMS -> XYZ(adapted) */
            alwan_simd xa, ya, za;
            alwan__mat3_mul_simd(&xa, &ya, &za, &JZAZBZ_V_LMS_TO_XYZ, vl, vm, vs);

            /* Inverse chromatic adaptation:
             * x = (xa + (B-1)*za) / B
             * y = (ya + (G-1)*x) / G
             * z = za */
            alwan_simd vx = alwan_simd_mul(alwan_simd_fmadd(Bm1, za, xa), inv_B);
            alwan_simd vy = alwan_simd_mul(alwan_simd_fmadd(Gm1, vx, ya), inv_G);

            alwan_simd_store(&c0[i], vx);
            alwan_simd_store(&c1[i], vy);
            alwan_simd_store(&c2[i], za);
        }
        for (; i < tile; i++) {
            alwan_jzazbz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz r = alwan_jzazbz_to_xyz_v(v);
            c0[i] = (alwan_simd_lane)r.x; c1[i] = (alwan_simd_lane)r.y; c2[i] = (alwan_simd_lane)r.z;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_jzazbz jz = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_xyz xyz;
        alwan_jzazbz_to_xyz(&xyz, &jz);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}

int alwan_jzazbz_to_jzczhz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                        alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                        size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd jz = alwan_simd_load(&c0[i]);
            alwan_simd az = alwan_simd_load(&c1[i]);
            alwan_simd bz = alwan_simd_load(&c2[i]);
            alwan_simd Cz = alwan_simd_sqrt(
                alwan_simd_fmadd(az, az, alwan_simd_mul(bz, bz)));
            alwan_simd hz = alwan_simd_atan2(bz, az);
            alwan_simd_store(&c0[i], jz);
            alwan_simd_store(&c1[i], Cz);
            alwan_simd_store(&c2[i], hz);
        }
        for (; i < tile; i++) {
            alwan_jzazbz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_jzczhz r = alwan_jzazbz_to_jzczhz_v(v);
            c0[i] = (alwan_simd_lane)r.Jz; c1[i] = (alwan_simd_lane)r.Cz; c2[i] = (alwan_simd_lane)r.hz;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_jzazbz jz = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_jzczhz jzch;
        alwan_jzazbz_to_jzczhz(&jzch, &jz);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = jzch.Jz;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = jzch.Cz;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = jzch.hz;
    }
#endif
    return ALWAN_OK;
}

int alwan_jzczhz_to_jzazbz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                        alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                        size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd jz = alwan_simd_load(&c0[i]);
            alwan_simd Cz = alwan_simd_load(&c1[i]);
            alwan_simd hz = alwan_simd_load(&c2[i]);
            alwan_simd az = alwan_simd_mul(Cz, alwan_simd_cos(hz));
            alwan_simd bz = alwan_simd_mul(Cz, alwan_simd_sin(hz));
            alwan_simd_store(&c0[i], jz);
            alwan_simd_store(&c1[i], az);
            alwan_simd_store(&c2[i], bz);
        }
        for (; i < tile; i++) {
            alwan_jzczhz v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_jzazbz r = alwan_jzczhz_to_jzazbz_v(v);
            c0[i] = (alwan_simd_lane)r.Jz; c1[i] = (alwan_simd_lane)r.az; c2[i] = (alwan_simd_lane)r.bz;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_jzczhz jzch = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_jzazbz jz;
        alwan_jzczhz_to_jzazbz(&jz, &jzch);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = jz.Jz;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = jz.az;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = jz.bz;
    }
#endif
    return ALWAN_OK;
}
