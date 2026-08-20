/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_math_core.h"
#include <string.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_math_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (gamut volume/coverage f32 facades use alwan_mat3_det_f64 / _mulv_f64). */
#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#include "alwan_math_impl.inc"
#include "alwan_api_teardown.h"
#endif /* ALWAN_WITH_F64_FACADE */

/* ================================================================
 * Advanced Mathematical & Utility Functions
 *
 * alwan_interpolate_*, alwan_extrapolate_*, alwan_table_interp_1d_*,
 * alwan_cct_duv_optimize_* are templatized in alwan_math_impl.inc.
 * ================================================================ */


/* ----------------------------------------------------------------
 * Tristimulus Optimization
 *
 * Templatized in alwan_math_impl.inc.
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * Table Interpolation Utilities
 *
 * alwan_table_interp_1d_* is templatized in alwan_math_impl.inc.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
alwan_status alwan_table_interp_3d_trilinear_f64(alwan_rgb_f64 *rgb_out,
                                     alwan_f64 const *table, size_t const sizes[3],
                                     alwan_rgb_f64 const *rgb_in) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    /* Clamp input to [0, 1] */
    alwan_f64 r = rgb_in->r;
    alwan_f64 g = rgb_in->g;
    alwan_f64 b = rgb_in->b;

    if (r < 0.0) r = 0.0; if (r > 1.0) r = 1.0;
    if (g < 0.0) g = 0.0; if (g > 1.0) g = 1.0;
    if (b < 0.0) b = 0.0; if (b > 1.0) b = 1.0;

    /* Map to table indices */
    alwan_f64 r_pos = r * (sizes[0] - 1);
    alwan_f64 g_pos = g * (sizes[1] - 1);
    alwan_f64 b_pos = b * (sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_f64 r_frac = r_pos - r_idx;
    alwan_f64 g_frac = g_pos - g_idx;
    alwan_f64 b_frac = b_pos - b_idx;

    /* Clamp indices */
    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0; }

    /* Get 8 corner values */
    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_f64 c000[3], c001[3], c010[3], c011[3];
    alwan_f64 c100[3], c101[3], c110[3], c111[3];

    for (int c = 0; c < 3; c++) {
        c000[c] = GET_VALUE(r_idx, g_idx, b_idx, c);
        c001[c] = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
        c010[c] = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
        c011[c] = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
        c100[c] = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
        c101[c] = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
        c110[c] = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
        c111[c] = GET_VALUE(r_idx + 1, g_idx + 1, b_idx + 1, c);
    }

    #undef GET_VALUE

    /* Trilinear interpolation */
    for (int c = 0; c < 3; c++) {
        alwan_f64 c00 = c000[c] * (ALWAN_LITERAL(1.0) - r_frac) + c100[c] * r_frac;
        alwan_f64 c01 = c001[c] * (ALWAN_LITERAL(1.0) - r_frac) + c101[c] * r_frac;
        alwan_f64 c10 = c010[c] * (ALWAN_LITERAL(1.0) - r_frac) + c110[c] * r_frac;
        alwan_f64 c11 = c011[c] * (ALWAN_LITERAL(1.0) - r_frac) + c111[c] * r_frac;

        alwan_f64 c0 = c00 * (ALWAN_LITERAL(1.0) - g_frac) + c10 * g_frac;
        alwan_f64 c1 = c01 * (ALWAN_LITERAL(1.0) - g_frac) + c11 * g_frac;

        alwan_f64 result = c0 * (ALWAN_LITERAL(1.0) - b_frac) + c1 * b_frac;

        if (c == 0) rgb_out->r = result;
        else if (c == 1) rgb_out->g = result;
        else rgb_out->b = result;
    }

    return ALWAN_OK;
}

alwan_status alwan_table_interp_3d_tetrahedral_f64(alwan_rgb_f64 *rgb_out,
                                       alwan_f64 const *table, size_t const sizes[3],
                                       alwan_rgb_f64 const *rgb_in) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    /* Clamp input to [0, 1] */
    alwan_f64 r = rgb_in->r;
    alwan_f64 g = rgb_in->g;
    alwan_f64 b = rgb_in->b;

    if (r < 0.0) r = 0.0; if (r > 1.0) r = 1.0;
    if (g < 0.0) g = 0.0; if (g > 1.0) g = 1.0;
    if (b < 0.0) b = 0.0; if (b > 1.0) b = 1.0;

    /* Map to table indices */
    alwan_f64 r_pos = r * (sizes[0] - 1);
    alwan_f64 g_pos = g * (sizes[1] - 1);
    alwan_f64 b_pos = b * (sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_f64 r_frac = r_pos - r_idx;
    alwan_f64 g_frac = g_pos - g_idx;
    alwan_f64 b_frac = b_pos - b_idx;

    /* Clamp indices */
    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0; }

    /* Get cube corner values */
    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_f64 result[3] = {0.0, 0.0, 0.0};

    /* Tetrahedral interpolation - determine which tetrahedron we're in */
    for (int c = 0; c < 3; c++) {
        alwan_f64 c000 = GET_VALUE(r_idx, g_idx, b_idx, c);
        alwan_f64 c111 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx + 1, c);

        if (r_frac > g_frac) {
            if (g_frac > b_frac) {
                /* Tetrahedron 1: r > g > b */
                alwan_f64 c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_f64 c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c110 - c100) * g_frac + (c111 - c110) * b_frac;
            } else if (r_frac > b_frac) {
                /* Tetrahedron 2: r > b > g */
                alwan_f64 c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_f64 c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c101 - c100) * b_frac + (c111 - c101) * g_frac;
            } else {
                /* Tetrahedron 3: b > r > g */
                alwan_f64 c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_f64 c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c101 - c001) * r_frac + (c111 - c101) * g_frac;
            }
        } else {
            if (b_frac > g_frac) {
                /* Tetrahedron 4: b > g > r */
                alwan_f64 c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_f64 c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c011 - c001) * g_frac + (c111 - c011) * r_frac;
            } else if (b_frac > r_frac) {
                /* Tetrahedron 5: g > b > r */
                alwan_f64 c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_f64 c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c011 - c010) * b_frac + (c111 - c011) * r_frac;
            } else {
                /* Tetrahedron 6: g > r > b */
                alwan_f64 c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_f64 c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c110 - c010) * r_frac + (c111 - c110) * b_frac;
            }
        }
    }

    #undef GET_VALUE

    rgb_out->r = result[0];
    rgb_out->g = result[1];
    rgb_out->b = result[2];

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
alwan_status alwan_table_interp_3d_trilinear_f32(alwan_rgb_f32 *rgb_out,
                                     alwan_f32 const *table, size_t const sizes[3],
                                     alwan_rgb_f32 const *rgb_in) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    alwan_f32 r = rgb_in->r;
    alwan_f32 g = rgb_in->g;
    alwan_f32 b = rgb_in->b;

    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f; if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f; if (b > 1.0f) b = 1.0f;

    alwan_f32 r_pos = r * (alwan_f32)(sizes[0] - 1);
    alwan_f32 g_pos = g * (alwan_f32)(sizes[1] - 1);
    alwan_f32 b_pos = b * (alwan_f32)(sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_f32 r_frac = r_pos - (alwan_f32)r_idx;
    alwan_f32 g_frac = g_pos - (alwan_f32)g_idx;
    alwan_f32 b_frac = b_pos - (alwan_f32)b_idx;

    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0f; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0f; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0f; }

    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_f32 c000[3], c001[3], c010[3], c011[3];
    alwan_f32 c100[3], c101[3], c110[3], c111[3];

    for (int c = 0; c < 3; c++) {
        c000[c] = GET_VALUE(r_idx, g_idx, b_idx, c);
        c001[c] = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
        c010[c] = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
        c011[c] = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
        c100[c] = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
        c101[c] = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
        c110[c] = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
        c111[c] = GET_VALUE(r_idx + 1, g_idx + 1, b_idx + 1, c);
    }

    #undef GET_VALUE

    for (int c = 0; c < 3; c++) {
        alwan_f32 c00 = c000[c] * (1.0f - r_frac) + c100[c] * r_frac;
        alwan_f32 c01 = c001[c] * (1.0f - r_frac) + c101[c] * r_frac;
        alwan_f32 c10 = c010[c] * (1.0f - r_frac) + c110[c] * r_frac;
        alwan_f32 c11 = c011[c] * (1.0f - r_frac) + c111[c] * r_frac;

        alwan_f32 c0 = c00 * (1.0f - g_frac) + c10 * g_frac;
        alwan_f32 c1 = c01 * (1.0f - g_frac) + c11 * g_frac;

        alwan_f32 result = c0 * (1.0f - b_frac) + c1 * b_frac;

        if (c == 0) rgb_out->r = result;
        else if (c == 1) rgb_out->g = result;
        else rgb_out->b = result;
    }

    return ALWAN_OK;
}

alwan_status alwan_table_interp_3d_tetrahedral_f32(alwan_rgb_f32 *rgb_out,
                                       alwan_f32 const *table, size_t const sizes[3],
                                       alwan_rgb_f32 const *rgb_in) {
    if (!table || !sizes || !rgb_in || !rgb_out) {
        return ALWAN_E_RANGE;
    }

    alwan_f32 r = rgb_in->r;
    alwan_f32 g = rgb_in->g;
    alwan_f32 b = rgb_in->b;

    if (r < 0.0f) r = 0.0f; if (r > 1.0f) r = 1.0f;
    if (g < 0.0f) g = 0.0f; if (g > 1.0f) g = 1.0f;
    if (b < 0.0f) b = 0.0f; if (b > 1.0f) b = 1.0f;

    alwan_f32 r_pos = r * (alwan_f32)(sizes[0] - 1);
    alwan_f32 g_pos = g * (alwan_f32)(sizes[1] - 1);
    alwan_f32 b_pos = b * (alwan_f32)(sizes[2] - 1);

    size_t r_idx = (size_t)r_pos;
    size_t g_idx = (size_t)g_pos;
    size_t b_idx = (size_t)b_pos;

    alwan_f32 r_frac = r_pos - (alwan_f32)r_idx;
    alwan_f32 g_frac = g_pos - (alwan_f32)g_idx;
    alwan_f32 b_frac = b_pos - (alwan_f32)b_idx;

    if (r_idx >= sizes[0] - 1) { r_idx = sizes[0] - 2; r_frac = 1.0f; }
    if (g_idx >= sizes[1] - 1) { g_idx = sizes[1] - 2; g_frac = 1.0f; }
    if (b_idx >= sizes[2] - 1) { b_idx = sizes[2] - 2; b_frac = 1.0f; }

    size_t stride_g = sizes[0] * 3;
    size_t stride_b = sizes[0] * sizes[1] * 3;

    #define GET_VALUE(ri, gi, bi, c) table[((bi) * stride_b + (gi) * stride_g + (ri) * 3 + (c))]

    alwan_f32 result[3] = {0.0f, 0.0f, 0.0f};

    for (int c = 0; c < 3; c++) {
        alwan_f32 c000 = GET_VALUE(r_idx, g_idx, b_idx, c);
        alwan_f32 c111 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx + 1, c);

        if (r_frac > g_frac) {
            if (g_frac > b_frac) {
                alwan_f32 c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_f32 c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c110 - c100) * g_frac + (c111 - c110) * b_frac;
            } else if (r_frac > b_frac) {
                alwan_f32 c100 = GET_VALUE(r_idx + 1, g_idx, b_idx, c);
                alwan_f32 c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c100 - c000) * r_frac +
                           (c101 - c100) * b_frac + (c111 - c101) * g_frac;
            } else {
                alwan_f32 c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_f32 c101 = GET_VALUE(r_idx + 1, g_idx, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c101 - c001) * r_frac + (c111 - c101) * g_frac;
            }
        } else {
            if (b_frac > g_frac) {
                alwan_f32 c001 = GET_VALUE(r_idx, g_idx, b_idx + 1, c);
                alwan_f32 c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c001 - c000) * b_frac +
                           (c011 - c001) * g_frac + (c111 - c011) * r_frac;
            } else if (b_frac > r_frac) {
                alwan_f32 c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_f32 c011 = GET_VALUE(r_idx, g_idx + 1, b_idx + 1, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c011 - c010) * b_frac + (c111 - c011) * r_frac;
            } else {
                alwan_f32 c010 = GET_VALUE(r_idx, g_idx + 1, b_idx, c);
                alwan_f32 c110 = GET_VALUE(r_idx + 1, g_idx + 1, b_idx, c);
                result[c] = c000 + (c010 - c000) * g_frac +
                           (c110 - c010) * r_frac + (c111 - c110) * b_frac;
            }
        }
    }

    #undef GET_VALUE

    rgb_out->r = result[0];
    rgb_out->g = result[1];
    rgb_out->b = result[2];

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
