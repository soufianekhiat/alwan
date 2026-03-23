/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Correction Functions
 * LGG, Color Matrix, Printer Lights, White Balance
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_color_correction_core.h"

/* ----------------------------------------------------------------
 * LGG (Lift/Gamma/Gain) Apply Map
 * ---------------------------------------------------------------- */

int alwan_lgg_apply_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                         alwan_rgb const *lift, alwan_rgb const *gamma, alwan_rgb const *gain,
                         size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || !lift || !gamma || !gain || count == 0)
        return ALWAN_E_INVALID;

    size_t processed = 0;
    /* Precompute per-channel constants */
    alwan_scalar gs_r = gamma->r > (alwan_scalar)0.0001 ? gamma->r : (alwan_scalar)0.0001;
    alwan_scalar gs_g = gamma->g > (alwan_scalar)0.0001 ? gamma->g : (alwan_scalar)0.0001;
    alwan_scalar gs_b = gamma->b > (alwan_scalar)0.0001 ? gamma->b : (alwan_scalar)0.0001;
    alwan_scalar inv_gamma_r = (alwan_scalar)1.0 / gs_r;
    alwan_scalar inv_gamma_g = (alwan_scalar)1.0 / gs_g;
    alwan_scalar inv_gamma_b = (alwan_scalar)1.0 / gs_b;

    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd v_lift_r = alwan_simd_set1((alwan_simd_lane)lift->r);
                alwan_simd v_lift_g = alwan_simd_set1((alwan_simd_lane)lift->g);
                alwan_simd v_lift_b = alwan_simd_set1((alwan_simd_lane)lift->b);
                alwan_simd v_inv_gr = alwan_simd_set1((alwan_simd_lane)inv_gamma_r);
                alwan_simd v_inv_gg = alwan_simd_set1((alwan_simd_lane)inv_gamma_g);
                alwan_simd v_inv_gb = alwan_simd_set1((alwan_simd_lane)inv_gamma_b);
                alwan_simd v_gain_r = alwan_simd_set1((alwan_simd_lane)gain->r);
                alwan_simd v_gain_g = alwan_simd_set1((alwan_simd_lane)gain->g);
                alwan_simd v_gain_b = alwan_simd_set1((alwan_simd_lane)gain->b);
                alwan_simd zero = alwan_simd_set1(0);
                for (; i + W <= tile; i += W) {
                    alwan_simd r = alwan_simd_load(&c0[i]);
                    alwan_simd g = alwan_simd_load(&c1[i]);
                    alwan_simd b = alwan_simd_load(&c2[i]);
                    /* lifted = max(0, rgb + lift) */
                    alwan_simd lr = alwan_simd_max(alwan_simd_add(r, v_lift_r), zero);
                    alwan_simd lg = alwan_simd_max(alwan_simd_add(g, v_lift_g), zero);
                    alwan_simd lb = alwan_simd_max(alwan_simd_add(b, v_lift_b), zero);
                    /* pow(lifted, 1/gamma) * gain */
                    alwan_simd_store(&d0[i], alwan_simd_mul(alwan_simd_pow(lr, v_inv_gr), v_gain_r));
                    alwan_simd_store(&d1[i], alwan_simd_mul(alwan_simd_pow(lg, v_inv_gg), v_gain_g));
                    alwan_simd_store(&d2[i], alwan_simd_mul(alwan_simd_pow(lb, v_inv_gb), v_gain_b));
                }
            }
#endif
            for (; i < tile; i++) {
                alwan_rgb v_in = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
                alwan_rgb v_out = alwan_lgg_apply_v(v_in, *lift, *gamma, *gain);
                d0[i] = (alwan_simd_lane)v_out.r; d1[i] = (alwan_simd_lane)v_out.g; d2[i] = (alwan_simd_lane)v_out.b;
            }
        }
        alwan__store_tile_aos3(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Color Matrix Apply Map (SIMD mat3 multiply)
 * ---------------------------------------------------------------- */

int alwan_color_matrix_apply_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                  alwan_mat3x3 const *matrix,
                                  size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || !matrix || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
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
            alwan_simd o0, o1, o2;
            alwan__mat3_mul_simd(&o0, &o1, &o2, matrix, vr, vg, vb);
            alwan_simd_store(&c0[i], o0);
            alwan_simd_store(&c1[i], o1);
            alwan_simd_store(&c2[i], o2);
        }
        for (; i < tile; i++) {
            alwan_rgb v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_color_matrix_apply_v(v, *matrix);
            c0[i] = (alwan_simd_lane)r.r; c1[i] = (alwan_simd_lane)r.g; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb r = alwan_color_matrix_apply_v(rgb, *matrix);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Printer Lights Apply Map
 * ---------------------------------------------------------------- */

int alwan_printer_lights_apply_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                    alwan_scalar red_lights, alwan_scalar green_lights,
                                    alwan_scalar blue_lights,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    /* Precompute exposure multipliers (constant for all pixels) */
    alwan_scalar ln10 = (alwan_scalar)2.302585092994046;
    alwan_scalar log_step = (alwan_scalar)0.025;
    alwan_scalar mr = ALWAN_EXP(((alwan_scalar)25.0 - red_lights)   * log_step * ln10);
    alwan_scalar mg = ALWAN_EXP(((alwan_scalar)25.0 - green_lights) * log_step * ln10);
    alwan_scalar mb = ALWAN_EXP(((alwan_scalar)25.0 - blue_lights)  * log_step * ln10);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd vmr = alwan_simd_set1((alwan_simd_lane)mr);
                alwan_simd vmg = alwan_simd_set1((alwan_simd_lane)mg);
                alwan_simd vmb = alwan_simd_set1((alwan_simd_lane)mb);
                for (; i + W <= tile; i += W) {
                    alwan_simd_store(&c0[i], alwan_simd_mul(alwan_simd_load(&c0[i]), vmr));
                    alwan_simd_store(&c1[i], alwan_simd_mul(alwan_simd_load(&c1[i]), vmg));
                    alwan_simd_store(&c2[i], alwan_simd_mul(alwan_simd_load(&c2[i]), vmb));
                }
            }
#endif
            for (; i < tile; i++) {
                c0[i] = (alwan_simd_lane)((alwan_scalar)c0[i] * mr);
                c1[i] = (alwan_simd_lane)((alwan_scalar)c1[i] * mg);
                c2[i] = (alwan_simd_lane)((alwan_scalar)c2[i] * mb);
            }
        }
        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * White Balance Apply Map
 * ---------------------------------------------------------------- */

int alwan_white_balance_apply_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                   alwan_rgb const *multipliers,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || !multipliers || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd mr = alwan_simd_set1((alwan_simd_lane)multipliers->r);
    alwan_simd mg = alwan_simd_set1((alwan_simd_lane)multipliers->g);
    alwan_simd mb = alwan_simd_set1((alwan_simd_lane)multipliers->b);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd_store(&c0[i], alwan_simd_mul(alwan_simd_load(&c0[i]), mr));
            alwan_simd_store(&c1[i], alwan_simd_mul(alwan_simd_load(&c1[i]), mg));
            alwan_simd_store(&c2[i], alwan_simd_mul(alwan_simd_load(&c2[i]), mb));
        }
        for (; i < tile; i++) {
            alwan_rgb v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_white_balance_apply_v(v, *multipliers);
            c0[i] = (alwan_simd_lane)r.r; c1[i] = (alwan_simd_lane)r.g; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb r = alwan_white_balance_apply_v(rgb, *multipliers);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int alwan_lgg_apply_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                 alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                 alwan_rgb const *lift, alwan_rgb const *gamma, alwan_rgb const *gain,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !lift || !gamma || !gain || count == 0)
        return ALWAN_E_INVALID;

    alwan_scalar gs_r = gamma->r > (alwan_scalar)0.0001 ? gamma->r : (alwan_scalar)0.0001;
    alwan_scalar gs_g = gamma->g > (alwan_scalar)0.0001 ? gamma->g : (alwan_scalar)0.0001;
    alwan_scalar gs_b = gamma->b > (alwan_scalar)0.0001 ? gamma->b : (alwan_scalar)0.0001;
    alwan_scalar inv_gamma_r = (alwan_scalar)1.0 / gs_r;
    alwan_scalar inv_gamma_g = (alwan_scalar)1.0 / gs_g;
    alwan_scalar inv_gamma_b = (alwan_scalar)1.0 / gs_b;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd v_lift_r = alwan_simd_set1((alwan_simd_lane)lift->r);
                alwan_simd v_lift_g = alwan_simd_set1((alwan_simd_lane)lift->g);
                alwan_simd v_lift_b = alwan_simd_set1((alwan_simd_lane)lift->b);
                alwan_simd v_inv_gr = alwan_simd_set1((alwan_simd_lane)inv_gamma_r);
                alwan_simd v_inv_gg = alwan_simd_set1((alwan_simd_lane)inv_gamma_g);
                alwan_simd v_inv_gb = alwan_simd_set1((alwan_simd_lane)inv_gamma_b);
                alwan_simd v_gain_r = alwan_simd_set1((alwan_simd_lane)gain->r);
                alwan_simd v_gain_g = alwan_simd_set1((alwan_simd_lane)gain->g);
                alwan_simd v_gain_b = alwan_simd_set1((alwan_simd_lane)gain->b);
                alwan_simd zero = alwan_simd_set1(0);
                for (; i + W <= tile; i += W) {
                    alwan_simd r = alwan_simd_load(&c0[i]);
                    alwan_simd g = alwan_simd_load(&c1[i]);
                    alwan_simd b = alwan_simd_load(&c2[i]);
                    alwan_simd lr = alwan_simd_max(alwan_simd_add(r, v_lift_r), zero);
                    alwan_simd lg = alwan_simd_max(alwan_simd_add(g, v_lift_g), zero);
                    alwan_simd lb = alwan_simd_max(alwan_simd_add(b, v_lift_b), zero);
                    alwan_simd_store(&d0[i], alwan_simd_mul(alwan_simd_pow(lr, v_inv_gr), v_gain_r));
                    alwan_simd_store(&d1[i], alwan_simd_mul(alwan_simd_pow(lg, v_inv_gg), v_gain_g));
                    alwan_simd_store(&d2[i], alwan_simd_mul(alwan_simd_pow(lb, v_inv_gb), v_gain_b));
                }
            }
#endif
            for (; i < tile; i++) {
                alwan_rgb v_in = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
                alwan_rgb v_out = alwan_lgg_apply_v(v_in, *lift, *gamma, *gain);
                d0[i] = (alwan_simd_lane)v_out.r; d1[i] = (alwan_simd_lane)v_out.g; d2[i] = (alwan_simd_lane)v_out.b;
            }
        }
        alwan__store_tile_planar3(out0, out1, out2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_color_matrix_apply_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                          alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                          alwan_mat3x3 const *matrix,
                                          size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !matrix || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vr = alwan_simd_load(&c0[i]);
            alwan_simd vg = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd o0, o1, o2;
            alwan__mat3_mul_simd(&o0, &o1, &o2, matrix, vr, vg, vb);
            alwan_simd_store(&c0[i], o0);
            alwan_simd_store(&c1[i], o1);
            alwan_simd_store(&c2[i], o2);
        }
        for (; i < tile; i++) {
            alwan_rgb v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_color_matrix_apply_v(v, *matrix);
            c0[i] = (alwan_simd_lane)r.r; c1[i] = (alwan_simd_lane)r.g; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_planar3(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_rgb rgb = {
            *(alwan_scalar const *)((char const *)in0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in2 + i * in_stride)
        };
        alwan_rgb r = alwan_color_matrix_apply_v(rgb, *matrix);
        *(alwan_scalar *)((char *)out0 + i * out_stride) = r.r;
        *(alwan_scalar *)((char *)out1 + i * out_stride) = r.g;
        *(alwan_scalar *)((char *)out2 + i * out_stride) = r.b;
    }
#endif
    return ALWAN_OK;
}

int alwan_printer_lights_apply_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                            alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                            alwan_scalar red_lights, alwan_scalar green_lights, alwan_scalar blue_lights,
                                            size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;

    alwan_scalar ln10 = (alwan_scalar)2.302585092994046;
    alwan_scalar log_step = (alwan_scalar)0.025;
    alwan_scalar mr = ALWAN_EXP(((alwan_scalar)25.0 - red_lights)   * log_step * ln10);
    alwan_scalar mg = ALWAN_EXP(((alwan_scalar)25.0 - green_lights) * log_step * ln10);
    alwan_scalar mb = ALWAN_EXP(((alwan_scalar)25.0 - blue_lights)  * log_step * ln10);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd vmr = alwan_simd_set1((alwan_simd_lane)mr);
                alwan_simd vmg = alwan_simd_set1((alwan_simd_lane)mg);
                alwan_simd vmb = alwan_simd_set1((alwan_simd_lane)mb);
                for (; i + W <= tile; i += W) {
                    alwan_simd_store(&c0[i], alwan_simd_mul(alwan_simd_load(&c0[i]), vmr));
                    alwan_simd_store(&c1[i], alwan_simd_mul(alwan_simd_load(&c1[i]), vmg));
                    alwan_simd_store(&c2[i], alwan_simd_mul(alwan_simd_load(&c2[i]), vmb));
                }
            }
#endif
            for (; i < tile; i++) {
                c0[i] = (alwan_simd_lane)((alwan_scalar)c0[i] * mr);
                c1[i] = (alwan_simd_lane)((alwan_scalar)c1[i] * mg);
                c2[i] = (alwan_simd_lane)((alwan_scalar)c2[i] * mb);
            }
        }
        alwan__store_tile_planar3(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_white_balance_apply_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                           alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                           alwan_rgb const *multipliers,
                                           size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !multipliers || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd mr = alwan_simd_set1((alwan_simd_lane)multipliers->r);
    alwan_simd mg = alwan_simd_set1((alwan_simd_lane)multipliers->g);
    alwan_simd mb = alwan_simd_set1((alwan_simd_lane)multipliers->b);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd_store(&c0[i], alwan_simd_mul(alwan_simd_load(&c0[i]), mr));
            alwan_simd_store(&c1[i], alwan_simd_mul(alwan_simd_load(&c1[i]), mg));
            alwan_simd_store(&c2[i], alwan_simd_mul(alwan_simd_load(&c2[i]), mb));
        }
        for (; i < tile; i++) {
            alwan_rgb v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_white_balance_apply_v(v, *multipliers);
            c0[i] = (alwan_simd_lane)r.r; c1[i] = (alwan_simd_lane)r.g; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_planar3(out0, out1, out2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_rgb rgb = {
            *(alwan_scalar const *)((char const *)in0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in2 + i * in_stride)
        };
        alwan_rgb r = alwan_white_balance_apply_v(rgb, *multipliers);
        *(alwan_scalar *)((char *)out0 + i * out_stride) = r.r;
        *(alwan_scalar *)((char *)out1 + i * out_stride) = r.g;
        *(alwan_scalar *)((char *)out2 + i * out_stride) = r.b;
    }
#endif
    return ALWAN_OK;
}
