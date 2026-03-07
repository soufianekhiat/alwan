/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map ICtCp Conversions - True SIMD vectorized (PQ and HLG paths)
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_ictcp_core.h"

/* ----------------------------------------------------------------
 * Map RGB <-> ICtCp Conversions
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ictcp_map_interleave(alwan_scalar *ictcp_out, alwan_scalar const *rgb_in,
                            int use_pq, size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !ictcp_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *tf_mat = use_pq ? &ICTCP_LMS_P_TO_ICTCP_PQ : &ICTCP_LMS_P_TO_ICTCP_HLG;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);

            /* RGB -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &ICTCP_RGB_TO_LMS, vr, vg, vb);

            /* LMS -> LMS' via PQ or HLG OETF */
            if (use_pq) {
                vl = alwan__pq_oetf_simd(vl);
                vm = alwan__pq_oetf_simd(vm);
                vs = alwan__pq_oetf_simd(vs);
            } else {
                vl = alwan__hlg_oetf_simd(vl);
                vm = alwan__hlg_oetf_simd(vm);
                vs = alwan__hlg_oetf_simd(vs);
            }

            /* LMS' -> ICtCp */
            alwan_simd oI, oCt, oCp;
            alwan__mat3_mul_simd(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_simd_store(&c0[i], oI);
            alwan_simd_store(&c1[i], oCt);
            alwan_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ictcp ictcp = use_pq ? alwan_rgb_to_ictcp_pq_v(rgb) : alwan_rgb_to_ictcp_hlg_v(rgb);
            c0[i] = (alwan_simd_lane)ictcp.I; c1[i] = (alwan_simd_lane)ictcp.Ct; c2[i] = (alwan_simd_lane)ictcp.Cp;
        }

        alwan__store_tile_aos3(ictcp_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ictcp_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_ictcp ictcp;
        alwan_rgb_to_ictcp(&ictcp, &rgb, use_pq);
        out_ptr[0] = ictcp.I; out_ptr[1] = ictcp.Ct; out_ptr[2] = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

int alwan_ictcp_to_rgb_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *ictcp_in,
                            int use_pq, size_t count, size_t in_stride, size_t out_stride) {
    if (!ictcp_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *inv_mat = use_pq ? &ICTCP_ICTCP_TO_LMS_P_PQ : &ICTCP_ICTCP_TO_LMS_P_HLG;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, ictcp_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vI  = alwan_simd_load(&c0[i]);
            alwan_simd vCt = alwan_simd_load(&c1[i]);
            alwan_simd vCp = alwan_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via PQ EOTF or HLG inverse OETF */
            if (use_pq) {
                lp = alwan__pq_eotf_simd(lp);
                mp = alwan__pq_eotf_simd(mp);
                sp = alwan__pq_eotf_simd(sp);
            } else {
                lp = alwan__hlg_eotf_simd(lp);
                mp = alwan__hlg_eotf_simd(mp);
                sp = alwan__hlg_eotf_simd(sp);
            }

            /* LMS -> RGB */
            alwan_simd or_, og, ob;
            alwan__mat3_mul_simd(&or_, &og, &ob, &ICTCP_LMS_TO_RGB, lp, mp, sp);

            alwan_simd_store(&c0[i], or_);
            alwan_simd_store(&c1[i], og);
            alwan_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            alwan_ictcp ictcp = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb rgb = use_pq ? alwan_ictcp_pq_to_rgb_v(ictcp) : alwan_ictcp_hlg_to_rgb_v(ictcp);
            c0[i] = (alwan_simd_lane)rgb.r; c1[i] = (alwan_simd_lane)rgb.g; c2[i] = (alwan_simd_lane)rgb.b;
        }

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ictcp_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_ictcp ictcp = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_rgb rgb;
        alwan_ictcp_to_rgb(&rgb, &ictcp, use_pq);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Map XYZ <-> ICtCp Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_ictcp_map_interleave(alwan_scalar *ictcp_out, alwan_scalar const *xyz_in,
                            int use_pq, size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !ictcp_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *tf_mat = use_pq ? &ICTCP_LMS_P_TO_ICTCP_PQ : &ICTCP_LMS_P_TO_ICTCP_HLG;

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

            /* XYZ -> BT.2020 RGB */
            alwan_simd vr, vg, vb;
            alwan__mat3_mul_simd(&vr, &vg, &vb, &ICTCP_XYZ_TO_BT2020, vx, vy, vz);

            /* RGB -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &ICTCP_RGB_TO_LMS, vr, vg, vb);

            /* LMS -> LMS' via OETF */
            if (use_pq) {
                vl = alwan__pq_oetf_simd(vl);
                vm = alwan__pq_oetf_simd(vm);
                vs = alwan__pq_oetf_simd(vs);
            } else {
                vl = alwan__hlg_oetf_simd(vl);
                vm = alwan__hlg_oetf_simd(vm);
                vs = alwan__hlg_oetf_simd(vs);
            }

            /* LMS' -> ICtCp */
            alwan_simd oI, oCt, oCp;
            alwan__mat3_mul_simd(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_simd_store(&c0[i], oI);
            alwan_simd_store(&c1[i], oCt);
            alwan_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ictcp ictcp = use_pq ? alwan_xyz_to_ictcp_pq_v(xyz) : alwan_xyz_to_ictcp_hlg_v(xyz);
            c0[i] = (alwan_simd_lane)ictcp.I; c1[i] = (alwan_simd_lane)ictcp.Ct; c2[i] = (alwan_simd_lane)ictcp.Cp;
        }

        alwan__store_tile_aos3(ictcp_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ictcp_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_ictcp ictcp;
        alwan_xyz_to_ictcp(&ictcp, &xyz, use_pq);
        out_ptr[0] = ictcp.I; out_ptr[1] = ictcp.Ct; out_ptr[2] = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

int alwan_ictcp_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *ictcp_in,
                            int use_pq, size_t count, size_t in_stride, size_t out_stride) {
    if (!ictcp_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *inv_mat = use_pq ? &ICTCP_ICTCP_TO_LMS_P_PQ : &ICTCP_ICTCP_TO_LMS_P_HLG;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, ictcp_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vI  = alwan_simd_load(&c0[i]);
            alwan_simd vCt = alwan_simd_load(&c1[i]);
            alwan_simd vCp = alwan_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via EOTF / inverse OETF */
            if (use_pq) {
                lp = alwan__pq_eotf_simd(lp);
                mp = alwan__pq_eotf_simd(mp);
                sp = alwan__pq_eotf_simd(sp);
            } else {
                lp = alwan__hlg_eotf_simd(lp);
                mp = alwan__hlg_eotf_simd(mp);
                sp = alwan__hlg_eotf_simd(sp);
            }

            /* LMS -> RGB */
            alwan_simd vr, vg, vb;
            alwan__mat3_mul_simd(&vr, &vg, &vb, &ICTCP_LMS_TO_RGB, lp, mp, sp);

            /* BT.2020 RGB -> XYZ */
            alwan_simd ox, oy, oz;
            alwan__mat3_mul_simd(&ox, &oy, &oz, &ICTCP_BT2020_TO_XYZ, vr, vg, vb);

            alwan_simd_store(&c0[i], ox);
            alwan_simd_store(&c1[i], oy);
            alwan_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            alwan_ictcp ictcp = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz xyz = use_pq ? alwan_ictcp_pq_to_xyz_v(ictcp) : alwan_ictcp_hlg_to_xyz_v(ictcp);
            c0[i] = (alwan_simd_lane)xyz.x; c1[i] = (alwan_simd_lane)xyz.y; c2[i] = (alwan_simd_lane)xyz.z;
        }

        alwan__store_tile_aos3(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ictcp_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_ictcp ictcp = {in_ptr[0], in_ptr[1], in_ptr[2]}; alwan_xyz xyz;
        alwan_ictcp_to_xyz(&xyz, &ictcp, use_pq);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Planar Map Variants
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ictcp_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   int use_pq,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *tf_mat = use_pq ? &ICTCP_LMS_P_TO_ICTCP_PQ : &ICTCP_LMS_P_TO_ICTCP_HLG;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);

            /* RGB -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &ICTCP_RGB_TO_LMS, vr, vg, vb);

            /* LMS -> LMS' via PQ or HLG OETF */
            if (use_pq) {
                vl = alwan__pq_oetf_simd(vl);
                vm = alwan__pq_oetf_simd(vm);
                vs = alwan__pq_oetf_simd(vs);
            } else {
                vl = alwan__hlg_oetf_simd(vl);
                vm = alwan__hlg_oetf_simd(vm);
                vs = alwan__hlg_oetf_simd(vs);
            }

            /* LMS' -> ICtCp */
            alwan_simd oI, oCt, oCp;
            alwan__mat3_mul_simd(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_simd_store(&c0[i], oI);
            alwan_simd_store(&c1[i], oCt);
            alwan_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ictcp ictcp = use_pq ? alwan_rgb_to_ictcp_pq_v(rgb) : alwan_rgb_to_ictcp_hlg_v(rgb);
            c0[i] = (alwan_simd_lane)ictcp.I; c1[i] = (alwan_simd_lane)ictcp.Ct; c2[i] = (alwan_simd_lane)ictcp.Cp;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_rgb rgb = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_ictcp ictcp;
        alwan_rgb_to_ictcp(&ictcp, &rgb, use_pq);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = ictcp.I;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = ictcp.Ct;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

int alwan_ictcp_to_rgb_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   int use_pq,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *inv_mat = use_pq ? &ICTCP_ICTCP_TO_LMS_P_PQ : &ICTCP_ICTCP_TO_LMS_P_HLG;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vI  = alwan_simd_load(&c0[i]);
            alwan_simd vCt = alwan_simd_load(&c1[i]);
            alwan_simd vCp = alwan_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via PQ EOTF or HLG inverse OETF */
            if (use_pq) {
                lp = alwan__pq_eotf_simd(lp);
                mp = alwan__pq_eotf_simd(mp);
                sp = alwan__pq_eotf_simd(sp);
            } else {
                lp = alwan__hlg_eotf_simd(lp);
                mp = alwan__hlg_eotf_simd(mp);
                sp = alwan__hlg_eotf_simd(sp);
            }

            /* LMS -> RGB */
            alwan_simd or_, og, ob;
            alwan__mat3_mul_simd(&or_, &og, &ob, &ICTCP_LMS_TO_RGB, lp, mp, sp);

            alwan_simd_store(&c0[i], or_);
            alwan_simd_store(&c1[i], og);
            alwan_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            alwan_ictcp ictcp = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb rgb = use_pq ? alwan_ictcp_pq_to_rgb_v(ictcp) : alwan_ictcp_hlg_to_rgb_v(ictcp);
            c0[i] = (alwan_simd_lane)rgb.r; c1[i] = (alwan_simd_lane)rgb.g; c2[i] = (alwan_simd_lane)rgb.b;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_ictcp ictcp = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_rgb rgb;
        alwan_ictcp_to_rgb(&rgb, &ictcp, use_pq);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = rgb.r;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = rgb.g;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = rgb.b;
    }
#endif
    return ALWAN_OK;
}

int alwan_xyz_to_ictcp_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   int use_pq,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *tf_mat = use_pq ? &ICTCP_LMS_P_TO_ICTCP_PQ : &ICTCP_LMS_P_TO_ICTCP_HLG;

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

            /* XYZ -> BT.2020 RGB */
            alwan_simd vr, vg, vb;
            alwan__mat3_mul_simd(&vr, &vg, &vb, &ICTCP_XYZ_TO_BT2020, vx, vy, vz);

            /* RGB -> LMS */
            alwan_simd vl, vm, vs;
            alwan__mat3_mul_simd(&vl, &vm, &vs, &ICTCP_RGB_TO_LMS, vr, vg, vb);

            /* LMS -> LMS' via OETF */
            if (use_pq) {
                vl = alwan__pq_oetf_simd(vl);
                vm = alwan__pq_oetf_simd(vm);
                vs = alwan__pq_oetf_simd(vs);
            } else {
                vl = alwan__hlg_oetf_simd(vl);
                vm = alwan__hlg_oetf_simd(vm);
                vs = alwan__hlg_oetf_simd(vs);
            }

            /* LMS' -> ICtCp */
            alwan_simd oI, oCt, oCp;
            alwan__mat3_mul_simd(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_simd_store(&c0[i], oI);
            alwan_simd_store(&c1[i], oCt);
            alwan_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            alwan_xyz xyz = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ictcp ictcp = use_pq ? alwan_xyz_to_ictcp_pq_v(xyz) : alwan_xyz_to_ictcp_hlg_v(xyz);
            c0[i] = (alwan_simd_lane)ictcp.I; c1[i] = (alwan_simd_lane)ictcp.Ct; c2[i] = (alwan_simd_lane)ictcp.Cp;
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
        alwan_ictcp ictcp;
        alwan_xyz_to_ictcp(&ictcp, &xyz, use_pq);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = ictcp.I;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = ictcp.Ct;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

int alwan_ictcp_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   int use_pq,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_mat3x3 const *inv_mat = use_pq ? &ICTCP_ICTCP_TO_LMS_P_PQ : &ICTCP_ICTCP_TO_LMS_P_HLG;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vI  = alwan_simd_load(&c0[i]);
            alwan_simd vCt = alwan_simd_load(&c1[i]);
            alwan_simd vCp = alwan_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_simd lp, mp, sp;
            alwan__mat3_mul_simd(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via EOTF / inverse OETF */
            if (use_pq) {
                lp = alwan__pq_eotf_simd(lp);
                mp = alwan__pq_eotf_simd(mp);
                sp = alwan__pq_eotf_simd(sp);
            } else {
                lp = alwan__hlg_eotf_simd(lp);
                mp = alwan__hlg_eotf_simd(mp);
                sp = alwan__hlg_eotf_simd(sp);
            }

            /* LMS -> RGB */
            alwan_simd vr, vg, vb;
            alwan__mat3_mul_simd(&vr, &vg, &vb, &ICTCP_LMS_TO_RGB, lp, mp, sp);

            /* BT.2020 RGB -> XYZ */
            alwan_simd ox, oy, oz;
            alwan__mat3_mul_simd(&ox, &oy, &oz, &ICTCP_BT2020_TO_XYZ, vr, vg, vb);

            alwan_simd_store(&c0[i], ox);
            alwan_simd_store(&c1[i], oy);
            alwan_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            alwan_ictcp ictcp = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_xyz xyz = use_pq ? alwan_ictcp_pq_to_xyz_v(ictcp) : alwan_ictcp_hlg_to_xyz_v(ictcp);
            c0[i] = (alwan_simd_lane)xyz.x; c1[i] = (alwan_simd_lane)xyz.y; c2[i] = (alwan_simd_lane)xyz.z;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_ictcp ictcp = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_xyz xyz;
        alwan_ictcp_to_xyz(&xyz, &ictcp, use_pq);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}
