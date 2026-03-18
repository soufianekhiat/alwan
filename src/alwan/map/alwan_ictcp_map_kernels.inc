/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map ICtCp kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * RGB -> ICtCp Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_rgb_to_ictcp, _map_interleave)(
    alwan_map_lane *ictcp_out, alwan_map_lane const *rgb_in,
    int use_pq, size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !ictcp_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *tf_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_PQ) : &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_HLG);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

            /* RGB -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(ICTCP_RGB_TO_LMS), vr, vg, vb);

            /* LMS -> LMS' via PQ or HLG OETF */
            if (use_pq) {
                vl = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vs);
            } else {
                vl = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vs);
            }

            /* LMS' -> ICtCp */
            alwan_map_simd oI, oCt, oCp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_map_simd_store(&c0[i], oI);
            alwan_map_simd_store(&c1[i], oCt);
            alwan_map_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = use_pq ? ALWAN_MAP_LITV(alwan_rgb_to_ictcp_pq)(rgb) : ALWAN_MAP_LITV(alwan_rgb_to_ictcp_hlg)(rgb);
            c0[i] = (alwan_map_lane)ictcp.I; c1[i] = (alwan_map_lane)ictcp.Ct; c2[i] = (alwan_map_lane)ictcp.Cp;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(ictcp_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)rgb_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)ictcp_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_rgb) rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_ictcp) ictcp;
        ALWAN_MAP_LIT(alwan_rgb_to_ictcp)(&ictcp, &rgb, use_pq);
        out_ptr[0] = ictcp.I; out_ptr[1] = ictcp.Ct; out_ptr[2] = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ICtCp -> RGB Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_ictcp_to_rgb, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *ictcp_in,
    int use_pq, size_t count, size_t in_stride, size_t out_stride)
{
    if (!ictcp_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *inv_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_PQ) : &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_HLG);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, ictcp_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vI  = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vCt = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vCp = alwan_map_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via PQ EOTF or HLG inverse OETF */
            if (use_pq) {
                lp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(sp);
            } else {
                lp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(sp);
            }

            /* LMS -> RGB */
            alwan_map_simd or_, og, ob;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&or_, &og, &ob, &ALWAN_MAP_LIT(ICTCP_LMS_TO_RGB), lp, mp, sp);

            alwan_map_simd_store(&c0[i], or_);
            alwan_map_simd_store(&c1[i], og);
            alwan_map_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) rgb = use_pq ? ALWAN_MAP_LITV(alwan_ictcp_pq_to_rgb)(ictcp) : ALWAN_MAP_LITV(alwan_ictcp_hlg_to_rgb)(ictcp);
            c0[i] = (alwan_map_lane)rgb.r; c1[i] = (alwan_map_lane)rgb.g; c2[i] = (alwan_map_lane)rgb.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)ictcp_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)rgb_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_ictcp) ictcp = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_rgb) rgb;
        ALWAN_MAP_LIT(alwan_ictcp_to_rgb)(&rgb, &ictcp, use_pq);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ -> ICtCp Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_xyz_to_ictcp, _map_interleave)(
    alwan_map_lane *ictcp_out, alwan_map_lane const *xyz_in,
    int use_pq, size_t count, size_t in_stride, size_t out_stride)
{
    if (!xyz_in || !ictcp_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *tf_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_PQ) : &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_HLG);

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

            /* XYZ -> BT.2020 RGB */
            alwan_map_simd vr, vg, vb;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vr, &vg, &vb, &ALWAN_MAP_LIT(ICTCP_XYZ_TO_BT2020), vx, vy, vz);

            /* RGB -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(ICTCP_RGB_TO_LMS), vr, vg, vb);

            /* LMS -> LMS' via OETF */
            if (use_pq) {
                vl = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vs);
            } else {
                vl = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vs);
            }

            /* LMS' -> ICtCp */
            alwan_map_simd oI, oCt, oCp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_map_simd_store(&c0[i], oI);
            alwan_map_simd_store(&c1[i], oCt);
            alwan_map_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = use_pq ? ALWAN_MAP_LITV(alwan_xyz_to_ictcp_pq)(xyz) : ALWAN_MAP_LITV(alwan_xyz_to_ictcp_hlg)(xyz);
            c0[i] = (alwan_map_lane)ictcp.I; c1[i] = (alwan_map_lane)ictcp.Ct; c2[i] = (alwan_map_lane)ictcp.Cp;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(ictcp_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)xyz_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)ictcp_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_xyz) xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_ictcp) ictcp;
        ALWAN_MAP_LIT(alwan_xyz_to_ictcp)(&ictcp, &xyz, use_pq);
        out_ptr[0] = ictcp.I; out_ptr[1] = ictcp.Ct; out_ptr[2] = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ICtCp -> XYZ Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_ictcp_to_xyz, _map_interleave)(
    alwan_map_lane *xyz_out, alwan_map_lane const *ictcp_in,
    int use_pq, size_t count, size_t in_stride, size_t out_stride)
{
    if (!ictcp_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *inv_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_PQ) : &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_HLG);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, ictcp_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vI  = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vCt = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vCp = alwan_map_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via EOTF / inverse OETF */
            if (use_pq) {
                lp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(sp);
            } else {
                lp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(sp);
            }

            /* LMS -> RGB */
            alwan_map_simd vr, vg, vb;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vr, &vg, &vb, &ALWAN_MAP_LIT(ICTCP_LMS_TO_RGB), lp, mp, sp);

            /* BT.2020 RGB -> XYZ */
            alwan_map_simd ox, oy, oz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(ICTCP_BT2020_TO_XYZ), vr, vg, vb);

            alwan_map_simd_store(&c0[i], ox);
            alwan_map_simd_store(&c1[i], oy);
            alwan_map_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) xyz = use_pq ? ALWAN_MAP_LITV(alwan_ictcp_pq_to_xyz)(ictcp) : ALWAN_MAP_LITV(alwan_ictcp_hlg_to_xyz)(ictcp);
            c0[i] = (alwan_map_lane)xyz.x; c1[i] = (alwan_map_lane)xyz.y; c2[i] = (alwan_map_lane)xyz.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)ictcp_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)xyz_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_ictcp) ictcp = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_xyz) xyz;
        ALWAN_MAP_LIT(alwan_ictcp_to_xyz)(&xyz, &ictcp, use_pq);
        out_ptr[0] = xyz.x; out_ptr[1] = xyz.y; out_ptr[2] = xyz.z;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int ALWAN_MAP_LITCAT(alwan_rgb_to_ictcp, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    int use_pq,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *tf_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_PQ) : &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_HLG);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);

            /* RGB -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(ICTCP_RGB_TO_LMS), vr, vg, vb);

            /* LMS -> LMS' via PQ or HLG OETF */
            if (use_pq) {
                vl = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vs);
            } else {
                vl = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vs);
            }

            /* LMS' -> ICtCp */
            alwan_map_simd oI, oCt, oCp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_map_simd_store(&c0[i], oI);
            alwan_map_simd_store(&c1[i], oCt);
            alwan_map_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) rgb = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = use_pq ? ALWAN_MAP_LITV(alwan_rgb_to_ictcp_pq)(rgb) : ALWAN_MAP_LITV(alwan_rgb_to_ictcp_hlg)(rgb);
            c0[i] = (alwan_map_lane)ictcp.I; c1[i] = (alwan_map_lane)ictcp.Ct; c2[i] = (alwan_map_lane)ictcp.Cp;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_ictcp) ictcp;
        ALWAN_MAP_LIT(alwan_rgb_to_ictcp)(&ictcp, &rgb, use_pq);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = ictcp.I;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = ictcp.Ct;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ictcp_to_rgb, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    int use_pq,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *inv_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_PQ) : &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_HLG);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vI  = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vCt = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vCp = alwan_map_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via PQ EOTF or HLG inverse OETF */
            if (use_pq) {
                lp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(sp);
            } else {
                lp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(sp);
            }

            /* LMS -> RGB */
            alwan_map_simd or_, og, ob;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&or_, &og, &ob, &ALWAN_MAP_LIT(ICTCP_LMS_TO_RGB), lp, mp, sp);

            alwan_map_simd_store(&c0[i], or_);
            alwan_map_simd_store(&c1[i], og);
            alwan_map_simd_store(&c2[i], ob);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) rgb = use_pq ? ALWAN_MAP_LITV(alwan_ictcp_pq_to_rgb)(ictcp) : ALWAN_MAP_LITV(alwan_ictcp_hlg_to_rgb)(ictcp);
            c0[i] = (alwan_map_lane)rgb.r; c1[i] = (alwan_map_lane)rgb.g; c2[i] = (alwan_map_lane)rgb.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_ictcp) ictcp = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_rgb) rgb;
        ALWAN_MAP_LIT(alwan_ictcp_to_rgb)(&rgb, &ictcp, use_pq);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = rgb.r;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = rgb.g;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = rgb.b;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_xyz_to_ictcp, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    int use_pq,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *tf_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_PQ) : &ALWAN_MAP_LIT(ICTCP_LMS_P_TO_ICTCP_HLG);

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

            /* XYZ -> BT.2020 RGB */
            alwan_map_simd vr, vg, vb;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vr, &vg, &vb, &ALWAN_MAP_LIT(ICTCP_XYZ_TO_BT2020), vx, vy, vz);

            /* RGB -> LMS */
            alwan_map_simd vl, vm, vs;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(ICTCP_RGB_TO_LMS), vr, vg, vb);

            /* LMS -> LMS' via OETF */
            if (use_pq) {
                vl = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__pq_oetf_simd)(vs);
            } else {
                vl = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vl);
                vm = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vm);
                vs = ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(vs);
            }

            /* LMS' -> ICtCp */
            alwan_map_simd oI, oCt, oCp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oI, &oCt, &oCp, tf_mat, vl, vm, vs);

            alwan_map_simd_store(&c0[i], oI);
            alwan_map_simd_store(&c1[i], oCt);
            alwan_map_simd_store(&c2[i], oCp);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_xyz) xyz = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = use_pq ? ALWAN_MAP_LITV(alwan_xyz_to_ictcp_pq)(xyz) : ALWAN_MAP_LITV(alwan_xyz_to_ictcp_hlg)(xyz);
            c0[i] = (alwan_map_lane)ictcp.I; c1[i] = (alwan_map_lane)ictcp.Ct; c2[i] = (alwan_map_lane)ictcp.Cp;
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
        ALWAN_MAP_LIT(alwan_ictcp) ictcp;
        ALWAN_MAP_LIT(alwan_xyz_to_ictcp)(&ictcp, &xyz, use_pq);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = ictcp.I;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = ictcp.Ct;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = ictcp.Cp;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_ictcp_to_xyz, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    int use_pq,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    ALWAN_MAP_LIT(alwan_mat3x3) const *inv_mat = use_pq ? &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_PQ) : &ALWAN_MAP_LIT(ICTCP_ICTCP_TO_LMS_P_HLG);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vI  = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vCt = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vCp = alwan_map_simd_load(&c2[i]);

            /* ICtCp -> LMS' */
            alwan_map_simd lp, mp, sp;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, inv_mat, vI, vCt, vCp);

            /* LMS' -> LMS via EOTF / inverse OETF */
            if (use_pq) {
                lp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__pq_eotf_simd)(sp);
            } else {
                lp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(lp);
                mp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(mp);
                sp = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(sp);
            }

            /* LMS -> RGB */
            alwan_map_simd vr, vg, vb;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vr, &vg, &vb, &ALWAN_MAP_LIT(ICTCP_LMS_TO_RGB), lp, mp, sp);

            /* BT.2020 RGB -> XYZ */
            alwan_map_simd ox, oy, oz;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(ICTCP_BT2020_TO_XYZ), vr, vg, vb);

            alwan_map_simd_store(&c0[i], ox);
            alwan_map_simd_store(&c1[i], oy);
            alwan_map_simd_store(&c2[i], oz);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_ictcp) ictcp = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_xyz) xyz = use_pq ? ALWAN_MAP_LITV(alwan_ictcp_pq_to_xyz)(ictcp) : ALWAN_MAP_LITV(alwan_ictcp_hlg_to_xyz)(ictcp);
            c0[i] = (alwan_map_lane)xyz.x; c1[i] = (alwan_map_lane)xyz.y; c2[i] = (alwan_map_lane)xyz.z;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_ictcp) ictcp = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_xyz) xyz;
        ALWAN_MAP_LIT(alwan_ictcp_to_xyz)(&xyz, &ictcp, use_pq);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = xyz.x;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = xyz.y;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = xyz.z;
    }
#endif
    return ALWAN_OK;
}
