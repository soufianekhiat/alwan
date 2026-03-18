/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Correction kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * LGG (Lift/Gamma/Gain) Apply Map - Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_lgg_apply, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    ALWAN_MAP_LIT(alwan_rgb) const *lift, ALWAN_MAP_LIT(alwan_rgb) const *gamma, ALWAN_MAP_LIT(alwan_rgb) const *gain,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !rgb_out || !lift || !gamma || !gain || count == 0)
        return ALWAN_E_INVALID;

    size_t processed = 0;
    /* Precompute per-channel constants */
    alwan_map_lane gs_r = gamma->r > (alwan_map_lane)0.0001 ? gamma->r : (alwan_map_lane)0.0001;
    alwan_map_lane gs_g = gamma->g > (alwan_map_lane)0.0001 ? gamma->g : (alwan_map_lane)0.0001;
    alwan_map_lane gs_b = gamma->b > (alwan_map_lane)0.0001 ? gamma->b : (alwan_map_lane)0.0001;
    alwan_map_lane inv_gamma_r = (alwan_map_lane)1.0 / gs_r;
    alwan_map_lane inv_gamma_g = (alwan_map_lane)1.0 / gs_g;
    alwan_map_lane inv_gamma_b = (alwan_map_lane)1.0 / gs_b;

    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd v_lift_r = alwan_map_simd_set1((alwan_map_lane)lift->r);
                alwan_map_simd v_lift_g = alwan_map_simd_set1((alwan_map_lane)lift->g);
                alwan_map_simd v_lift_b = alwan_map_simd_set1((alwan_map_lane)lift->b);
                alwan_map_simd v_inv_gr = alwan_map_simd_set1((alwan_map_lane)inv_gamma_r);
                alwan_map_simd v_inv_gg = alwan_map_simd_set1((alwan_map_lane)inv_gamma_g);
                alwan_map_simd v_inv_gb = alwan_map_simd_set1((alwan_map_lane)inv_gamma_b);
                alwan_map_simd v_gain_r = alwan_map_simd_set1((alwan_map_lane)gain->r);
                alwan_map_simd v_gain_g = alwan_map_simd_set1((alwan_map_lane)gain->g);
                alwan_map_simd v_gain_b = alwan_map_simd_set1((alwan_map_lane)gain->b);
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)0);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    /* lifted = max(0, rgb + lift) */
                    alwan_map_simd lr = alwan_map_simd_max(alwan_map_simd_add(r, v_lift_r), zero);
                    alwan_map_simd lg = alwan_map_simd_max(alwan_map_simd_add(g, v_lift_g), zero);
                    alwan_map_simd lb = alwan_map_simd_max(alwan_map_simd_add(b, v_lift_b), zero);
                    /* pow(lifted, 1/gamma) * gain */
                    alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_pow(lr, v_inv_gr), v_gain_r));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_mul(alwan_map_simd_pow(lg, v_inv_gg), v_gain_g));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_mul(alwan_map_simd_pow(lb, v_inv_gb), v_gain_b));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v_in = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) v_out = ALWAN_MAP_LITV(alwan_lgg_apply)(v_in, *lift, *gamma, *gain);
                d0[i] = (alwan_map_lane)v_out.r; d1[i] = (alwan_map_lane)v_out.g; d2[i] = (alwan_map_lane)v_out.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Color Matrix Apply Map - Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_color_matrix_apply, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    ALWAN_MAP_LIT(alwan_mat3x3) const *matrix,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !rgb_out || !matrix || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
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
            alwan_map_simd o0, o1, o2;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&o0, &o1, &o2, matrix, vr, vg, vb);
            alwan_map_simd_store(&c0[i], o0);
            alwan_map_simd_store(&c1[i], o1);
            alwan_map_simd_store(&c2[i], o2);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_color_matrix_apply)(v, *matrix);
            c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)rgb_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)rgb_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_rgb) rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_color_matrix_apply)(rgb, *matrix);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Printer Lights Apply Map - Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_printer_lights_apply, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    alwan_map_lane red_lights, alwan_map_lane green_lights,
    alwan_map_lane blue_lights,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    /* Precompute exposure multipliers (constant for all pixels) */
    alwan_map_lane ln10 = (alwan_map_lane)2.302585092994046;
    alwan_map_lane log_step = (alwan_map_lane)0.025;
    alwan_map_lane mr = alwan_map_scalar_exp(((alwan_map_lane)25.0 - red_lights)   * log_step * ln10);
    alwan_map_lane mg = alwan_map_scalar_exp(((alwan_map_lane)25.0 - green_lights) * log_step * ln10);
    alwan_map_lane mb = alwan_map_scalar_exp(((alwan_map_lane)25.0 - blue_lights)  * log_step * ln10);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd vmr = alwan_map_simd_set1((alwan_map_lane)mr);
                alwan_map_simd vmg = alwan_map_simd_set1((alwan_map_lane)mg);
                alwan_map_simd vmb = alwan_map_simd_set1((alwan_map_lane)mb);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd_store(&c0[i], alwan_map_simd_mul(alwan_map_simd_load(&c0[i]), vmr));
                    alwan_map_simd_store(&c1[i], alwan_map_simd_mul(alwan_map_simd_load(&c1[i]), vmg));
                    alwan_map_simd_store(&c2[i], alwan_map_simd_mul(alwan_map_simd_load(&c2[i]), vmb));
                }
            }
#endif
            for (; i < tile; i++) {
                c0[i] = (alwan_map_lane)((alwan_map_lane)c0[i] * mr);
                c1[i] = (alwan_map_lane)((alwan_map_lane)c1[i] * mg);
                c2[i] = (alwan_map_lane)((alwan_map_lane)c2[i] * mb);
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * White Balance Apply Map - Interleave
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_white_balance_apply, _map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    ALWAN_MAP_LIT(alwan_rgb) const *multipliers,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!rgb_in || !rgb_out || !multipliers || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd mr = alwan_map_simd_set1((alwan_map_lane)multipliers->r);
    alwan_map_simd mg = alwan_map_simd_set1((alwan_map_lane)multipliers->g);
    alwan_map_simd mb = alwan_map_simd_set1((alwan_map_lane)multipliers->b);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd_store(&c0[i], alwan_map_simd_mul(alwan_map_simd_load(&c0[i]), mr));
            alwan_map_simd_store(&c1[i], alwan_map_simd_mul(alwan_map_simd_load(&c1[i]), mg));
            alwan_map_simd_store(&c2[i], alwan_map_simd_mul(alwan_map_simd_load(&c2[i]), mb));
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_white_balance_apply)(v, *multipliers);
            c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)rgb_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)rgb_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_rgb) rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_white_balance_apply)(rgb, *multipliers);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int ALWAN_MAP_LITCAT(alwan_lgg_apply, _map_planar)(
    alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
    ALWAN_MAP_LIT(alwan_rgb) const *lift, ALWAN_MAP_LIT(alwan_rgb) const *gamma, ALWAN_MAP_LIT(alwan_rgb) const *gain,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !lift || !gamma || !gain || count == 0)
        return ALWAN_E_INVALID;

    alwan_map_lane gs_r = gamma->r > (alwan_map_lane)0.0001 ? gamma->r : (alwan_map_lane)0.0001;
    alwan_map_lane gs_g = gamma->g > (alwan_map_lane)0.0001 ? gamma->g : (alwan_map_lane)0.0001;
    alwan_map_lane gs_b = gamma->b > (alwan_map_lane)0.0001 ? gamma->b : (alwan_map_lane)0.0001;
    alwan_map_lane inv_gamma_r = (alwan_map_lane)1.0 / gs_r;
    alwan_map_lane inv_gamma_g = (alwan_map_lane)1.0 / gs_g;
    alwan_map_lane inv_gamma_b = (alwan_map_lane)1.0 / gs_b;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd v_lift_r = alwan_map_simd_set1((alwan_map_lane)lift->r);
                alwan_map_simd v_lift_g = alwan_map_simd_set1((alwan_map_lane)lift->g);
                alwan_map_simd v_lift_b = alwan_map_simd_set1((alwan_map_lane)lift->b);
                alwan_map_simd v_inv_gr = alwan_map_simd_set1((alwan_map_lane)inv_gamma_r);
                alwan_map_simd v_inv_gg = alwan_map_simd_set1((alwan_map_lane)inv_gamma_g);
                alwan_map_simd v_inv_gb = alwan_map_simd_set1((alwan_map_lane)inv_gamma_b);
                alwan_map_simd v_gain_r = alwan_map_simd_set1((alwan_map_lane)gain->r);
                alwan_map_simd v_gain_g = alwan_map_simd_set1((alwan_map_lane)gain->g);
                alwan_map_simd v_gain_b = alwan_map_simd_set1((alwan_map_lane)gain->b);
                alwan_map_simd zero = alwan_map_simd_set1((alwan_map_lane)0);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd lr = alwan_map_simd_max(alwan_map_simd_add(r, v_lift_r), zero);
                    alwan_map_simd lg = alwan_map_simd_max(alwan_map_simd_add(g, v_lift_g), zero);
                    alwan_map_simd lb = alwan_map_simd_max(alwan_map_simd_add(b, v_lift_b), zero);
                    alwan_map_simd_store(&d0[i], alwan_map_simd_mul(alwan_map_simd_pow(lr, v_inv_gr), v_gain_r));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_mul(alwan_map_simd_pow(lg, v_inv_gg), v_gain_g));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_mul(alwan_map_simd_pow(lb, v_inv_gb), v_gain_b));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v_in = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) v_out = ALWAN_MAP_LITV(alwan_lgg_apply)(v_in, *lift, *gamma, *gain);
                d0[i] = (alwan_map_lane)v_out.r; d1[i] = (alwan_map_lane)v_out.g; d2[i] = (alwan_map_lane)v_out.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_color_matrix_apply, _map_planar)(
    alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
    ALWAN_MAP_LIT(alwan_mat3x3) const *matrix,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !matrix || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vr = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vg = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd o0, o1, o2;
            ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&o0, &o1, &o2, matrix, vr, vg, vb);
            alwan_map_simd_store(&c0[i], o0);
            alwan_map_simd_store(&c1[i], o1);
            alwan_map_simd_store(&c2[i], o2);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_color_matrix_apply)(v, *matrix);
            c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {
            *(alwan_map_lane const *)((char const *)in0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_color_matrix_apply)(rgb, *matrix);
        *(alwan_map_lane *)((char *)out0 + i * out_stride) = r.r;
        *(alwan_map_lane *)((char *)out1 + i * out_stride) = r.g;
        *(alwan_map_lane *)((char *)out2 + i * out_stride) = r.b;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_printer_lights_apply, _map_planar)(
    alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
    alwan_map_lane red_lights, alwan_map_lane green_lights, alwan_map_lane blue_lights,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;

    alwan_map_lane ln10 = (alwan_map_lane)2.302585092994046;
    alwan_map_lane log_step = (alwan_map_lane)0.025;
    alwan_map_lane mr = alwan_map_scalar_exp(((alwan_map_lane)25.0 - red_lights)   * log_step * ln10);
    alwan_map_lane mg = alwan_map_scalar_exp(((alwan_map_lane)25.0 - green_lights) * log_step * ln10);
    alwan_map_lane mb = alwan_map_scalar_exp(((alwan_map_lane)25.0 - blue_lights)  * log_step * ln10);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd vmr = alwan_map_simd_set1((alwan_map_lane)mr);
                alwan_map_simd vmg = alwan_map_simd_set1((alwan_map_lane)mg);
                alwan_map_simd vmb = alwan_map_simd_set1((alwan_map_lane)mb);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd_store(&c0[i], alwan_map_simd_mul(alwan_map_simd_load(&c0[i]), vmr));
                    alwan_map_simd_store(&c1[i], alwan_map_simd_mul(alwan_map_simd_load(&c1[i]), vmg));
                    alwan_map_simd_store(&c2[i], alwan_map_simd_mul(alwan_map_simd_load(&c2[i]), vmb));
                }
            }
#endif
            for (; i < tile; i++) {
                c0[i] = (alwan_map_lane)((alwan_map_lane)c0[i] * mr);
                c1[i] = (alwan_map_lane)((alwan_map_lane)c1[i] * mg);
                c2[i] = (alwan_map_lane)((alwan_map_lane)c2[i] * mb);
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_white_balance_apply, _map_planar)(
    alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
    ALWAN_MAP_LIT(alwan_rgb) const *multipliers,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !multipliers || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_simd mr = alwan_map_simd_set1((alwan_map_lane)multipliers->r);
    alwan_map_simd mg = alwan_map_simd_set1((alwan_map_lane)multipliers->g);
    alwan_map_simd mb = alwan_map_simd_set1((alwan_map_lane)multipliers->b);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd_store(&c0[i], alwan_map_simd_mul(alwan_map_simd_load(&c0[i]), mr));
            alwan_map_simd_store(&c1[i], alwan_map_simd_mul(alwan_map_simd_load(&c1[i]), mg));
            alwan_map_simd_store(&c2[i], alwan_map_simd_mul(alwan_map_simd_load(&c2[i]), mb));
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_rgb) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_white_balance_apply)(v, *multipliers);
            c0[i] = (alwan_map_lane)r.r; c1[i] = (alwan_map_lane)r.g; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_rgb) rgb = {
            *(alwan_map_lane const *)((char const *)in0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_rgb) r = ALWAN_MAP_LITV(alwan_white_balance_apply)(rgb, *multipliers);
        *(alwan_map_lane *)((char *)out0 + i * out_stride) = r.r;
        *(alwan_map_lane *)((char *)out1 + i * out_stride) = r.g;
        *(alwan_map_lane *)((char *)out2 + i * out_stride) = r.b;
    }
#endif
    return ALWAN_OK;
}
