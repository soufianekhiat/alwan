/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only LUT baking and 2D flattening utilities
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_LUT_CORE_H
#define ALWAN_LUT_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_lut_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_lut_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_lut3d_index_to_rgb_v(size_t index, int size) {
    alwan_scalar const inv = ALWAN_LITERAL(1.0) / (alwan_scalar)(size - 1);
    int const r = (int)(index % (size_t)size);
    int const g = (int)((index / (size_t)size) % (size_t)size);
    int const b = (int)(index / ((size_t)size * (size_t)size));
    alwan_vec3 result;
    result.v[0] = (alwan_scalar)r * inv;
    result.v[1] = (alwan_scalar)g * inv;
    result.v[2] = (alwan_scalar)b * inv;
    return result;
}

ALWAN_INLINE alwan_scalar alwan_lut1d_index_to_val_v(size_t index, int size) {
    return (alwan_scalar)index / (alwan_scalar)(size - 1);
}

ALWAN_INLINE void alwan_lut2d_dimensions_v(int size, int *width, int *height) {
    *width  = size * size;
    *height = size;
}

ALWAN_INLINE void alwan_lut3d_to_2d_v(int r, int g, int b, int size,
                                        int *px, int *py) {
    *px = b * size + r;
    *py = g;
}

ALWAN_INLINE void alwan_lut2d_to_3d_v(int px, int py, int size,
                                        int *r, int *g, int *b) {
    *b = px / size;
    *r = px % size;
    *g = py;
}

ALWAN_INLINE alwan_scalar alwan_lut1d_sample_v(alwan_scalar const *lut,
                                                 alwan_scalar t,
                                                 int size) {
    alwan_scalar const max_idx = (alwan_scalar)(size - 1);

    alwan_scalar tc = ALWAN_SELECT(t < ALWAN_ZERO, ALWAN_ZERO, t);
    tc = ALWAN_SELECT(tc > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), tc) * max_idx;

    int i0 = (int)tc;
    int i1 = i0 + 1;
    if (i1 >= size) i1 = size - 1;

    alwan_scalar frac = tc - (alwan_scalar)i0;
    return lut[i0] * (ALWAN_LITERAL(1.0) - frac) + lut[i1] * frac;
}

ALWAN_INLINE alwan_vec3 alwan_lut2d_sample_v(alwan_scalar const *lut2d,
                                               alwan_vec3 rgb,
                                               int size) {
    alwan_scalar const max_idx = (alwan_scalar)(size - 1);
    int const w = size * size;

    alwan_scalar rf = ALWAN_SELECT(rgb.v[0] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[0]);
    alwan_scalar gf = ALWAN_SELECT(rgb.v[1] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[1]);
    alwan_scalar bf = ALWAN_SELECT(rgb.v[2] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[2]);
    rf = ALWAN_SELECT(rf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), rf) * max_idx;
    gf = ALWAN_SELECT(gf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), gf) * max_idx;
    bf = ALWAN_SELECT(bf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), bf) * max_idx;

    int r0 = (int)rf; int g0 = (int)gf; int b0 = (int)bf;
    int r1 = r0 + 1;  int g1 = g0 + 1;  int b1 = b0 + 1;
    if (r1 >= size) r1 = size - 1;
    if (g1 >= size) g1 = size - 1;
    if (b1 >= size) b1 = size - 1;

    alwan_scalar fr = rf - (alwan_scalar)r0;
    alwan_scalar fg = gf - (alwan_scalar)g0;
    alwan_scalar fb = bf - (alwan_scalar)b0;

    #define LUT2D_AT(rr, gg, bb) \
        (lut2d + ((size_t)(gg) * (size_t)w + (size_t)(bb) * (size_t)size + (size_t)(rr)) * 3)

    alwan_scalar const *c000 = LUT2D_AT(r0, g0, b0);
    alwan_scalar const *c100 = LUT2D_AT(r1, g0, b0);
    alwan_scalar const *c010 = LUT2D_AT(r0, g1, b0);
    alwan_scalar const *c110 = LUT2D_AT(r1, g1, b0);
    alwan_scalar const *c001 = LUT2D_AT(r0, g0, b1);
    alwan_scalar const *c101 = LUT2D_AT(r1, g0, b1);
    alwan_scalar const *c011 = LUT2D_AT(r0, g1, b1);
    alwan_scalar const *c111 = LUT2D_AT(r1, g1, b1);

    #undef LUT2D_AT

    alwan_vec3 result;
    for (int ch = 0; ch < 3; ch++) {
        alwan_scalar c00 = c000[ch] * (ALWAN_LITERAL(1.0) - fr) + c100[ch] * fr;
        alwan_scalar c01 = c001[ch] * (ALWAN_LITERAL(1.0) - fr) + c101[ch] * fr;
        alwan_scalar c10 = c010[ch] * (ALWAN_LITERAL(1.0) - fr) + c110[ch] * fr;
        alwan_scalar c11 = c011[ch] * (ALWAN_LITERAL(1.0) - fr) + c111[ch] * fr;

        alwan_scalar c0 = c00 * (ALWAN_LITERAL(1.0) - fg) + c10 * fg;
        alwan_scalar c1 = c01 * (ALWAN_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_LITERAL(1.0) - fb) + c1 * fb;
    }

    return result;
}

ALWAN_INLINE alwan_vec3 alwan_lut3d_sample_v(alwan_scalar const *lut,
                                               alwan_vec3 rgb,
                                               int size) {
    alwan_scalar const max_idx = (alwan_scalar)(size - 1);

    alwan_scalar rf = ALWAN_SELECT(rgb.v[0] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[0]);
    alwan_scalar gf = ALWAN_SELECT(rgb.v[1] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[1]);
    alwan_scalar bf = ALWAN_SELECT(rgb.v[2] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[2]);
    rf = ALWAN_SELECT(rf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), rf) * max_idx;
    gf = ALWAN_SELECT(gf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), gf) * max_idx;
    bf = ALWAN_SELECT(bf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), bf) * max_idx;

    int r0 = (int)rf; int g0 = (int)gf; int b0 = (int)bf;
    int r1 = r0 + 1;  int g1 = g0 + 1;  int b1 = b0 + 1;
    if (r1 >= size) r1 = size - 1;
    if (g1 >= size) g1 = size - 1;
    if (b1 >= size) b1 = size - 1;

    alwan_scalar fr = rf - (alwan_scalar)r0;
    alwan_scalar fg = gf - (alwan_scalar)g0;
    alwan_scalar fb = bf - (alwan_scalar)b0;

    #define LUT3D_AT(rr, gg, bb) \
        (lut + ((size_t)(bb) * (size_t)size * (size_t)size + \
                (size_t)(gg) * (size_t)size + (size_t)(rr)) * 3)

    alwan_scalar const *c000 = LUT3D_AT(r0, g0, b0);
    alwan_scalar const *c100 = LUT3D_AT(r1, g0, b0);
    alwan_scalar const *c010 = LUT3D_AT(r0, g1, b0);
    alwan_scalar const *c110 = LUT3D_AT(r1, g1, b0);
    alwan_scalar const *c001 = LUT3D_AT(r0, g0, b1);
    alwan_scalar const *c101 = LUT3D_AT(r1, g0, b1);
    alwan_scalar const *c011 = LUT3D_AT(r0, g1, b1);
    alwan_scalar const *c111 = LUT3D_AT(r1, g1, b1);

    #undef LUT3D_AT

    alwan_vec3 result;
    for (int ch = 0; ch < 3; ch++) {
        alwan_scalar c00 = c000[ch] * (ALWAN_LITERAL(1.0) - fr) + c100[ch] * fr;
        alwan_scalar c01 = c001[ch] * (ALWAN_LITERAL(1.0) - fr) + c101[ch] * fr;
        alwan_scalar c10 = c010[ch] * (ALWAN_LITERAL(1.0) - fr) + c110[ch] * fr;
        alwan_scalar c11 = c011[ch] * (ALWAN_LITERAL(1.0) - fr) + c111[ch] * fr;

        alwan_scalar c0 = c00 * (ALWAN_LITERAL(1.0) - fg) + c10 * fg;
        alwan_scalar c1 = c01 * (ALWAN_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_LITERAL(1.0) - fb) + c1 * fb;
    }

    return result;
}

#endif

#endif /* ALWAN_LUT_CORE_H */
