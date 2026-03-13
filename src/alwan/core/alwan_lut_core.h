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

/* ================================================================
 * 3D LUT coordinate generation
 *
 * Given a 1D index into a flattened size^3 cube, produce the
 * normalized [0,1] RGB coordinate.
 * Layout: R varies fastest, then G, then B (standard .cube order).
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

/* ================================================================
 * 1D LUT coordinate generation
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_lut1d_index_to_val_v(size_t index, int size) {
    return (alwan_scalar)index / (alwan_scalar)(size - 1);
}

/* ================================================================
 * 2D LUT layout (flattened 3D LUT into 2D texture)
 *
 * A 3D LUT of size N is flattened into a 2D image where:
 * - N slices are arranged horizontally (strip layout)
 * - Image width  = N * N
 * - Image height = N
 *
 * Each slice is a Blue-plane at constant B index.
 * Within each slice: x = R (column), y = G (row from bottom).
 *
 * Game engines (Unreal, Unity) use this convention for
 * color grading LUT textures.
 * ================================================================ */

ALWAN_INLINE void alwan_lut2d_dimensions_v(int size, int *width, int *height) {
    *width  = size * size;
    *height = size;
}

/* Convert a 3D index (r, g, b each in [0, size-1]) to a 2D pixel coordinate */
ALWAN_INLINE void alwan_lut3d_to_2d_v(int r, int g, int b, int size,
                                        int *px, int *py) {
    *px = b * size + r;
    *py = g;
}

/* Convert a 2D pixel coordinate back to 3D index */
ALWAN_INLINE void alwan_lut2d_to_3d_v(int px, int py, int size,
                                        int *r, int *g, int *b) {
    *b = px / size;
    *r = px % size;
    *g = py;
}

/* ================================================================
 * 1D LUT linear interpolation
 *
 * Sample a 1D LUT with linear interpolation.
 * lut: N-element array
 * t: input [0,1] coordinate
 * size: number of entries
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_lut1d_sample_v(alwan_scalar const *lut,
                                                 alwan_scalar t,
                                                 int size) {
    alwan_scalar const max_idx = (alwan_scalar)(size - 1);

    /* Clamp to [0,1] */
    alwan_scalar tc = ALWAN_SELECT(t < ALWAN_ZERO, ALWAN_ZERO, t);
    tc = ALWAN_SELECT(tc > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), tc) * max_idx;

    int i0 = (int)tc;
    int i1 = i0 + 1;
    if (i1 >= size) i1 = size - 1;

    alwan_scalar frac = tc - (alwan_scalar)i0;
    return lut[i0] * (ALWAN_LITERAL(1.0) - frac) + lut[i1] * frac;
}

/* ================================================================
 * 2D LUT bilinear sampling (flattened 3D strip)
 *
 * Sample a 2D strip LUT (horizontal blue slices) using the same
 * trilinear logic as 3D, but reading from the 2D memory layout.
 * This matches how game engines (Unreal/Unity) sample color
 * grading LUT textures on the GPU.
 *
 * lut2d: (size*size) * size * 3 array (row-major, RGB interleaved)
 * rgb: input [0,1] RGB coordinate
 * size: cube edge length
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_lut2d_sample_v(alwan_scalar const *lut2d,
                                               alwan_vec3 rgb,
                                               int size) {
    alwan_scalar const max_idx = (alwan_scalar)(size - 1);
    int const w = size * size;

    /* Clamp to [0,1] and scale to LUT coordinates */
    alwan_scalar rf = ALWAN_SELECT(rgb.v[0] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[0]);
    alwan_scalar gf = ALWAN_SELECT(rgb.v[1] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[1]);
    alwan_scalar bf = ALWAN_SELECT(rgb.v[2] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[2]);
    rf = ALWAN_SELECT(rf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), rf) * max_idx;
    gf = ALWAN_SELECT(gf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), gf) * max_idx;
    bf = ALWAN_SELECT(bf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), bf) * max_idx;

    /* Integer indices */
    int r0 = (int)rf; int g0 = (int)gf; int b0 = (int)bf;
    int r1 = r0 + 1;  int g1 = g0 + 1;  int b1 = b0 + 1;
    if (r1 >= size) r1 = size - 1;
    if (g1 >= size) g1 = size - 1;
    if (b1 >= size) b1 = size - 1;

    /* Fractional parts */
    alwan_scalar fr = rf - (alwan_scalar)r0;
    alwan_scalar fg = gf - (alwan_scalar)g0;
    alwan_scalar fb = bf - (alwan_scalar)b0;

    /* 2D layout: px = B*size + R, py = G */
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

    /* Trilinear interpolation per channel */
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

/* ================================================================
 * 3D LUT trilinear interpolation
 *
 * Sample a 3D LUT with trilinear interpolation.
 * lut: N^3 * 3 array (R-fastest, G, B-slowest)
 * rgb: input [0,1] coordinate
 * size: cube edge length
 * ================================================================ */

ALWAN_INLINE alwan_vec3 alwan_lut3d_sample_v(alwan_scalar const *lut,
                                               alwan_vec3 rgb,
                                               int size) {
    alwan_scalar const max_idx = (alwan_scalar)(size - 1);

    /* Scale to LUT coordinates */
    alwan_scalar rf = ALWAN_SELECT(rgb.v[0] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[0]);
    alwan_scalar gf = ALWAN_SELECT(rgb.v[1] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[1]);
    alwan_scalar bf = ALWAN_SELECT(rgb.v[2] < ALWAN_ZERO, ALWAN_ZERO, rgb.v[2]);
    rf = ALWAN_SELECT(rf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), rf) * max_idx;
    gf = ALWAN_SELECT(gf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), gf) * max_idx;
    bf = ALWAN_SELECT(bf > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), bf) * max_idx;

    /* Integer indices */
    int r0 = (int)rf; int g0 = (int)gf; int b0 = (int)bf;
    int r1 = r0 + 1;  int g1 = g0 + 1;  int b1 = b0 + 1;
    if (r1 >= size) r1 = size - 1;
    if (g1 >= size) g1 = size - 1;
    if (b1 >= size) b1 = size - 1;

    /* Fractional parts */
    alwan_scalar fr = rf - (alwan_scalar)r0;
    alwan_scalar fg = gf - (alwan_scalar)g0;
    alwan_scalar fb = bf - (alwan_scalar)b0;

    /* 8 corner lookups (R-fastest layout) */
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

    /* Trilinear interpolation per channel */
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

#endif /* ALWAN_LUT_CORE_H */
