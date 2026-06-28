/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Image colour-space conversion — templated over descriptor precision.
 * No include guard — included twice (f32 and f64 passes).
 *
 * Templating contract:
 *   - The RGB-space DESCRIPTOR and the combined-matrix construction are in
 *     ALWAN_CORE_T precision, built natively from the native-precision
 *     descriptor (no widening of the descriptor).
 *   - The per-pixel READ/WRITE/FORMAT-DISPATCH pipeline is UNCHANGED and is
 *     driven by alwan_pixel_format. Its arithmetic lanes are always f64
 *     (alwan_simd / alwan__*_typed helpers); the native-precision combined
 *     matrix is bridged into the f64 matrix those kernels expect. For the
 *     f64 pass this bridge is an exact double->double copy, so output is
 *     byte-identical to the hand-written f64 body.
 */

int ALWAN_CORE_FNLIT(alwan_image_convert)(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, ALWAN_CORE_FNLIT(alwan_rgb_space_desc) const *src_space, ALWAN_CORE_FNLIT(alwan_rgb_space_desc) const *dst_space, alwan_ctx *ctx) {

    if (!dst || !src || !src_space || !dst_space || width == 0 || height == 0) {
        return ALWAN_E_INVALID;
    }

    /* Resolve pixel strides from format */
    size_t const src_px = alwan__pixel_stride(src_fmt);
    size_t const dst_px = alwan__pixel_stride(dst_fmt);
    if (src_px == 0 || dst_px == 0) return ALWAN_E_INVALID;

    /* Derive conversion matrices (native descriptor precision) */
    ALWAN_CORE_MAT3X3 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = ALWAN_CORE_FNLIT(alwan_rgb_derive_matrices)(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    ALWAN_CORE_MAT3X3 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = ALWAN_CORE_FNLIT(alwan_rgb_derive_matrices)(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    ALWAN_CORE_T const tol = ALWAN_CORE_LITERAL(1e-6);
    ALWAN_CORE_T dx = src_space->white_xy[0] - dst_space->white_xy[0];
    ALWAN_CORE_T dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (ALWAN_CORE_ABS(dx) > tol || ALWAN_CORE_ABS(dy) > tol);

    /* Precompute a single combined matrix: xyz_to_dst * [cat *] src_to_xyz
     * This reduces per-pixel work to one mat3 multiply. */
    ALWAN_CORE_MAT3X3 combined;
    if (need_cat && ctx) {
        ALWAN_CORE_XYY src_xyy, dst_xyy;
        ALWAN_CORE_XYZ src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_FNLIT(alwan_xyy_to_xyz)(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_FNLIT(alwan_xyy_to_xyz)(&dst_wp, &dst_xyy);

        ALWAN_CORE_MAT3X3 cat;
        int status = ALWAN_CORE_FNLIT(alwan_cat_matrix)(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        /* combined = xyz_to_dst * cat * src_to_xyz */
        ALWAN_CORE_MAT3X3 tmp = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(cat, src_to_xyz);
        combined = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(xyz_to_dst, tmp);
    } else {
        /* combined = xyz_to_dst * src_to_xyz */
        combined = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(xyz_to_dst, src_to_xyz);
    }

    /* Bridge the native-precision combined matrix into the f64 matrix the
     * per-pixel kernels operate on. f64 pass: exact double->double copy. */
    alwan_mat3x3_f64 combined_px;
    for (int mi_ = 0; mi_ < 9; mi_++) combined_px.m[mi_] = (alwan_f64)combined.m[mi_];

    /* Resolve transfer function pointers (pixel pipeline is f64) */
    alwan_tf_fn_f64 eotf_fn = alwan__resolve_eotf_f64(src_space->eotf);
    alwan_tf_fn_f64 oetf_fn = alwan__resolve_oetf_f64(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    /* SIMD fast path: resolve SIMD transfer function pairs */
    {
        typedef alwan_simd (*simd_tf_fn)(alwan_simd);
        simd_tf_fn eotf_simd = NULL, oetf_simd = NULL;

        switch (src_space->eotf) {
        case ALWAN_TF_SRGB:   eotf_simd = alwan__srgb_eotf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: eotf_simd = alwan__pq_eotf_simd;    break;
        case ALWAN_TF_HLG:    eotf_simd = alwan__hlg_eotf_full_simd; break;
        case ALWAN_TF_LINEAR: eotf_simd = NULL;                      break; /* identity -- handle below */
        default: break;
        }

        switch (dst_space->oetf) {
        case ALWAN_TF_SRGB:   oetf_simd = alwan__srgb_oetf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: oetf_simd = alwan__pq_oetf_simd;    break;
        case ALWAN_TF_HLG:    oetf_simd = alwan__hlg_oetf_simd;   break;
        case ALWAN_TF_LINEAR: oetf_simd = NULL;                    break; /* identity -- handle below */
        default: break;
        }

        /* Use SIMD path if both TFs are known (including LINEAR=identity) */
        if (eotf_simd || src_space->eotf == ALWAN_TF_LINEAR) {
            if (oetf_simd || dst_space->oetf == ALWAN_TF_LINEAR) {
                int const eotf_is_linear = (src_space->eotf == ALWAN_TF_LINEAR);
                int const oetf_is_linear = (dst_space->oetf == ALWAN_TF_LINEAR);
                size_t const W = ALWAN_SIMD_WIDTH;

                for (size_t y = 0; y < height; y++) {
                    char const *src_row = (char const *)src + y * src_row_stride;
                    char       *dst_row = (char       *)dst + y * dst_row_stride;
                    size_t processed = 0;

                    while (processed < width) {
                        size_t tile = width - processed;
                        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
                        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];

                        alwan__load_tile_typed_3(c0, c1, c2, src_row, src_fmt, processed, src_px, tile);

                        /* SIMD: EOTF -> mat3 -> OETF */
                        {
                            size_t j = 0;
                            for (; j + W <= tile; j += W) {
                                alwan_simd vr = alwan_simd_load(&c0[j]);
                                alwan_simd vg = alwan_simd_load(&c1[j]);
                                alwan_simd vb = alwan_simd_load(&c2[j]);

                                /* EOTF: encoded -> linear */
                                if (!eotf_is_linear) {
                                    vr = eotf_simd(vr);
                                    vg = eotf_simd(vg);
                                    vb = eotf_simd(vb);
                                }

                                /* Combined matrix multiply */
                                alwan_simd dr, dg, db;
                                alwan__mat3_mul_simd(&dr, &dg, &db, &combined_px, vr, vg, vb);

                                /* OETF: linear -> encoded */
                                if (!oetf_is_linear) {
                                    dr = oetf_simd(dr);
                                    dg = oetf_simd(dg);
                                    db = oetf_simd(db);
                                }

                                alwan_simd_store(&c0[j], dr);
                                alwan_simd_store(&c1[j], dg);
                                alwan_simd_store(&c2[j], db);
                            }
                            /* Scalar tail within tile */
                            for (; j < tile; j++) {
                                alwan_f64 r = (alwan_f64)c0[j];
                                alwan_f64 g = (alwan_f64)c1[j];
                                alwan_f64 b = (alwan_f64)c2[j];
                                if (!eotf_is_linear) {
                                    r = eotf_fn(r); g = eotf_fn(g); b = eotf_fn(b);
                                }
                                alwan_vec3_f64 lin = {{r, g, b}};
                                alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined_px, lin);
                                if (!oetf_is_linear) {
                                    c0[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[0]);
                                    c1[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[1]);
                                    c2[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[2]);
                                } else {
                                    c0[j] = (alwan_simd_lane)dst_lin.v[0];
                                    c1[j] = (alwan_simd_lane)dst_lin.v[1];
                                    c2[j] = (alwan_simd_lane)dst_lin.v[2];
                                }
                            }
                        }

                        alwan__store_tile_typed_3(dst_row, dst_fmt, processed, dst_px, c0, c1, c2, tile);
                        processed += tile;
                    }
                }
                return ALWAN_OK;
            }
        }
    }
#endif /* ALWAN_SIMD_WIDTH > 1 */

    /* Scalar fallback */
    for (size_t y = 0; y < height; y++) {
        char const *src_row = (char const *)src + y * src_row_stride;
        char       *dst_row = (char       *)dst + y * dst_row_stride;

        for (size_t x = 0; x < width; x++) {
            /* Load 3-channel pixel with format conversion */
            alwan_f64 rgb[3];
            alwan__load3_typed(rgb, src_row + x * src_px, src_fmt);

            /* Source EOTF: encoded -> linear */
            alwan_vec3_f64 lin = {{eotf_fn(rgb[0]), eotf_fn(rgb[1]), eotf_fn(rgb[2])}};

            /* Combined matrix: src linear RGB -> dst linear RGB */
            alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined_px, lin);

            /* Destination OETF: linear -> encoded */
            alwan_f64 out[3] = {
                oetf_fn(dst_lin.v[0]),
                oetf_fn(dst_lin.v[1]),
                oetf_fn(dst_lin.v[2])
            };

            /* Store with format conversion */
            alwan__store3_typed(dst_row + x * dst_px, out, dst_fmt);
        }
    }

    return ALWAN_OK;
}

int ALWAN_CORE_FNLIT(alwan_image_convert_rgba)(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, ALWAN_CORE_FNLIT(alwan_rgb_space_desc) const *src_space, ALWAN_CORE_FNLIT(alwan_rgb_space_desc) const *dst_space, alwan_alpha_mode alpha_mode, alwan_ctx *ctx) {

    if (!dst || !src || !src_space || !dst_space || width == 0 || height == 0) {
        return ALWAN_E_INVALID;
    }

    /* Resolve 4-channel pixel strides */
    size_t const src_px = alwan__pixel_stride4(src_fmt);
    size_t const dst_px = alwan__pixel_stride4(dst_fmt);
    if (src_px == 0 || dst_px == 0) return ALWAN_E_INVALID;

    /* Element size for alpha channel offset (4th channel at index 3) */
    size_t const src_elem = alwan__fmt_size(src_fmt);
    size_t const dst_elem = alwan__fmt_size(dst_fmt);

    /* Derive conversion matrices (native descriptor precision) */
    ALWAN_CORE_MAT3X3 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = ALWAN_CORE_FNLIT(alwan_rgb_derive_matrices)(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }

    ALWAN_CORE_MAT3X3 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = ALWAN_CORE_FNLIT(alwan_rgb_derive_matrices)(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }

    /* Check if chromatic adaptation is needed */
    ALWAN_CORE_T const tol = ALWAN_CORE_LITERAL(1e-6);
    ALWAN_CORE_T dx = src_space->white_xy[0] - dst_space->white_xy[0];
    ALWAN_CORE_T dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (ALWAN_CORE_ABS(dx) > tol || ALWAN_CORE_ABS(dy) > tol);

    /* Precompute combined matrix: xyz_to_dst * [cat *] src_to_xyz */
    ALWAN_CORE_MAT3X3 combined;
    if (need_cat && ctx) {
        ALWAN_CORE_XYY src_xyy, dst_xyy;
        ALWAN_CORE_XYZ src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_FNLIT(alwan_xyy_to_xyz)(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_FNLIT(alwan_xyy_to_xyz)(&dst_wp, &dst_xyy);

        ALWAN_CORE_MAT3X3 cat;
        int status = ALWAN_CORE_FNLIT(alwan_cat_matrix)(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        ALWAN_CORE_MAT3X3 tmp = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(cat, src_to_xyz);
        combined = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(xyz_to_dst, tmp);
    } else {
        combined = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(xyz_to_dst, src_to_xyz);
    }

    /* Bridge the native-precision combined matrix into the f64 matrix the
     * per-pixel kernels operate on. f64 pass: exact double->double copy. */
    alwan_mat3x3_f64 combined_px;
    for (int mi_ = 0; mi_ < 9; mi_++) combined_px.m[mi_] = (alwan_f64)combined.m[mi_];

    /* Resolve transfer function pointers (pixel pipeline is f64) */
    alwan_tf_fn_f64 eotf_fn = alwan__resolve_eotf_f64(src_space->eotf);
    alwan_tf_fn_f64 oetf_fn = alwan__resolve_oetf_f64(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    int const premul = (alpha_mode == ALWAN_ALPHA_PREMULTIPLIED);

#if ALWAN_SIMD_WIDTH > 1
    /* SIMD fast path: resolve SIMD transfer function pairs */
    {
        typedef alwan_simd (*simd_tf_fn)(alwan_simd);
        simd_tf_fn eotf_simd = NULL, oetf_simd = NULL;

        switch (src_space->eotf) {
        case ALWAN_TF_SRGB:   eotf_simd = alwan__srgb_eotf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: eotf_simd = alwan__pq_eotf_simd;    break;
        case ALWAN_TF_HLG:    eotf_simd = alwan__hlg_eotf_full_simd; break;
        case ALWAN_TF_LINEAR: eotf_simd = NULL;                      break;
        default: break;
        }

        switch (dst_space->oetf) {
        case ALWAN_TF_SRGB:   oetf_simd = alwan__srgb_oetf_simd;  break;
        case ALWAN_TF_PQ:
        case ALWAN_TF_ST2084: oetf_simd = alwan__pq_oetf_simd;    break;
        case ALWAN_TF_HLG:    oetf_simd = alwan__hlg_oetf_simd;   break;
        case ALWAN_TF_LINEAR: oetf_simd = NULL;                    break;
        default: break;
        }

        if (eotf_simd || src_space->eotf == ALWAN_TF_LINEAR) {
            if (oetf_simd || dst_space->oetf == ALWAN_TF_LINEAR) {
                int const eotf_is_linear = (src_space->eotf == ALWAN_TF_LINEAR);
                int const oetf_is_linear = (dst_space->oetf == ALWAN_TF_LINEAR);
                size_t const W = ALWAN_SIMD_WIDTH;

                for (size_t y = 0; y < height; y++) {
                    char const *src_row = (char const *)src + y * src_row_stride;
                    char       *dst_row = (char       *)dst + y * dst_row_stride;
                    size_t processed = 0;

                    while (processed < width) {
                        size_t tile = width - processed;
                        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
                        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS];
                        ALWAN_ALIGN(32) alwan_simd_lane c2[ALWAN_TILE_PIXELS], c3[ALWAN_TILE_PIXELS];

                        alwan__load_tile_typed_3(c0, c1, c2, src_row, src_fmt, processed, src_px, tile);

                        /* Load alpha (channel 3) */
                        for (size_t j = 0; j < tile; j++) {
                            char const *ap = src_row + (processed + j) * src_px + 3 * src_elem;
                            c3[j] = (alwan_simd_lane)alwan__load1_typed(ap, src_fmt);
                        }

                        /* SIMD: [unpremul] EOTF -> mat3 -> OETF [repremul] */
                        {
                            size_t j = 0;
                            for (; j + W <= tile; j += W) {
                                alwan_simd vr = alwan_simd_load(&c0[j]);
                                alwan_simd vg = alwan_simd_load(&c1[j]);
                                alwan_simd vb = alwan_simd_load(&c2[j]);

                                if (premul) {
                                    alwan_simd va      = alwan_simd_load(&c3[j]);
                                    alwan_simd zero    = alwan_simd_zero();
                                    alwan_simd_mask nz = alwan_simd_cmpgt(va, zero);
                                    alwan_simd sa      = alwan_simd_select(nz, va, alwan_simd_set1((alwan_simd_lane)1.0));
                                    alwan_simd inv_a   = alwan_simd_div(alwan_simd_set1((alwan_simd_lane)1.0), sa);
                                    vr = alwan_simd_mul(vr, inv_a);
                                    vg = alwan_simd_mul(vg, inv_a);
                                    vb = alwan_simd_mul(vb, inv_a);
                                }

                                if (!eotf_is_linear) {
                                    vr = eotf_simd(vr);
                                    vg = eotf_simd(vg);
                                    vb = eotf_simd(vb);
                                }

                                alwan_simd dr, dg, db;
                                alwan__mat3_mul_simd(&dr, &dg, &db, &combined_px, vr, vg, vb);

                                if (!oetf_is_linear) {
                                    dr = oetf_simd(dr);
                                    dg = oetf_simd(dg);
                                    db = oetf_simd(db);
                                }

                                if (premul) {
                                    alwan_simd va = alwan_simd_load(&c3[j]);
                                    dr = alwan_simd_mul(dr, va);
                                    dg = alwan_simd_mul(dg, va);
                                    db = alwan_simd_mul(db, va);
                                }

                                alwan_simd_store(&c0[j], dr);
                                alwan_simd_store(&c1[j], dg);
                                alwan_simd_store(&c2[j], db);
                            }
                            /* Scalar tail within tile */
                            for (; j < tile; j++) {
                                alwan_f64 r = (alwan_f64)c0[j];
                                alwan_f64 g = (alwan_f64)c1[j];
                                alwan_f64 b = (alwan_f64)c2[j];
                                alwan_f64 a = (alwan_f64)c3[j];
                                if (premul && a > ALWAN_LITERAL(0.0)) {
                                    alwan_f64 inv_a = ALWAN_LITERAL(1.0) / a;
                                    r *= inv_a; g *= inv_a; b *= inv_a;
                                }
                                if (!eotf_is_linear) { r = eotf_fn(r); g = eotf_fn(g); b = eotf_fn(b); }
                                alwan_vec3_f64 lin = {{r, g, b}};
                                alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined_px, lin);
                                if (!oetf_is_linear) {
                                    c0[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[0]);
                                    c1[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[1]);
                                    c2[j] = (alwan_simd_lane)oetf_fn(dst_lin.v[2]);
                                } else {
                                    c0[j] = (alwan_simd_lane)dst_lin.v[0];
                                    c1[j] = (alwan_simd_lane)dst_lin.v[1];
                                    c2[j] = (alwan_simd_lane)dst_lin.v[2];
                                }
                                if (premul) {
                                    c0[j] *= (alwan_simd_lane)a;
                                    c1[j] *= (alwan_simd_lane)a;
                                    c2[j] *= (alwan_simd_lane)a;
                                }
                            }
                        }

                        alwan__store_tile_typed_3(dst_row, dst_fmt, processed, dst_px, c0, c1, c2, tile);

                        /* Store alpha (channel 3) */
                        for (size_t j = 0; j < tile; j++) {
                            char *ap = dst_row + (processed + j) * dst_px + 3 * dst_elem;
                            alwan__store1_typed(ap, (alwan_f64)c3[j], dst_fmt);
                        }

                        processed += tile;
                    }
                }
                return ALWAN_OK;
            }
        }
    }
#endif /* ALWAN_SIMD_WIDTH > 1 */

    /* Process image row by row */
    for (size_t y = 0; y < height; y++) {
        char const *src_row = (char const *)src + y * src_row_stride;
        char       *dst_row = (char       *)dst + y * dst_row_stride;

        for (size_t x = 0; x < width; x++) {
            char const *src_pixel = src_row + x * src_px;
            char       *dst_pixel = dst_row + x * dst_px;

            /* Load RGB channels (first 3 elements of 4-channel pixel) */
            alwan_f64 rgb[3];
            alwan__load3_typed(rgb, src_pixel, src_fmt);

            /* Load alpha channel (4th element) */
            alwan_f64 alpha = alwan__load1_typed(src_pixel + 3 * src_elem, src_fmt);

            /* Unpremultiply if needed */
            if (premul && alpha > ALWAN_LITERAL(0.0)) {
                alwan_f64 inv_a = ALWAN_LITERAL(1.0) / alpha;
                rgb[0] *= inv_a;
                rgb[1] *= inv_a;
                rgb[2] *= inv_a;
            }

            /* Source EOTF: encoded -> linear */
            alwan_vec3_f64 lin = {{eotf_fn(rgb[0]), eotf_fn(rgb[1]), eotf_fn(rgb[2])}};

            /* Combined matrix: src linear RGB -> dst linear RGB */
            alwan_vec3_f64 dst_lin = alwan_mat3_mulv_f64_v(combined_px, lin);

            /* Destination OETF: linear -> encoded */
            alwan_f64 out[3] = {
                oetf_fn(dst_lin.v[0]),
                oetf_fn(dst_lin.v[1]),
                oetf_fn(dst_lin.v[2])
            };

            /* Repremultiply if needed */
            if (premul) {
                out[0] *= alpha;
                out[1] *= alpha;
                out[2] *= alpha;
            }

            /* Store RGB + alpha with format conversion */
            alwan__store3_typed(dst_pixel, out, dst_fmt);
            alwan__store1_typed(dst_pixel + 3 * dst_elem, alpha, dst_fmt);
        }
    }

    return ALWAN_OK;
}
