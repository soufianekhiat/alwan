/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only LUT baking and 2D flattening utilities (parameterized)
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * 3D LUT coordinate generation
 *
 * Given a 1D index into a flattened size^3 cube, produce the
 * normalized [0,1] RGB coordinate.
 * Layout: R varies fastest, then G, then B (standard .cube order).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lut3d_index_to_rgb)(size_t index, int size) {
    ALWAN_CORE_T const inv = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_T)(size - 1);
    int const r = (int)(index % (size_t)size);
    int const g = (int)((index / (size_t)size) % (size_t)size);
    int const b = (int)(index / ((size_t)size * (size_t)size));
    ALWAN_CORE_VEC3 result;
    result.v[0] = (ALWAN_CORE_T)r * inv;
    result.v[1] = (ALWAN_CORE_T)g * inv;
    result.v[2] = (ALWAN_CORE_T)b * inv;
    return result;
}

/* ================================================================
 * 1D LUT coordinate generation
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_lut1d_index_to_val)(size_t index, int size) {
    return (ALWAN_CORE_T)index / (ALWAN_CORE_T)(size - 1);
}

/* ================================================================
 * 2D LUT layout (flattened 3D LUT into 2D texture)
 * ================================================================ */

ALWAN_INLINE void ALWAN_CORE_FNVLIT(alwan_lut2d_dimensions)(int size, int *width, int *height) {
    *width  = size * size;
    *height = size;
}

/* Convert a 3D index (r, g, b each in [0, size-1]) to a 2D pixel coordinate */
ALWAN_INLINE void ALWAN_CORE_FNVLIT(alwan_lut3d_to_2d)(int r, int g, int b, int size,
                                        int *px, int *py) {
    *px = b * size + r;
    *py = g;
}

/* Convert a 2D pixel coordinate back to 3D index */
ALWAN_INLINE void ALWAN_CORE_FNVLIT(alwan_lut2d_to_3d)(int px, int py, int size,
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

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_lut1d_sample)(ALWAN_CORE_T const *lut,
                                                 ALWAN_CORE_T t,
                                                 int size) {
    ALWAN_CORE_T const max_idx = (ALWAN_CORE_T)(size - 1);

    /* Clamp to [0,1] */
    ALWAN_CORE_T tc = ALWAN_CORE_SELECT(t < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, t);
    tc = ALWAN_CORE_SELECT(tc > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), tc) * max_idx;

    int i0 = (int)tc;
    int i1 = i0 + 1;
    if (i1 >= size) i1 = size - 1;

    ALWAN_CORE_T frac = tc - (ALWAN_CORE_T)i0;
    return lut[i0] * (ALWAN_CORE_LITERAL(1.0) - frac) + lut[i1] * frac;
}

/* ================================================================
 * 2D LUT bilinear sampling (flattened 3D strip)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lut2d_sample)(ALWAN_CORE_T const *lut2d,
                                               ALWAN_CORE_VEC3 rgb,
                                               int size) {
    ALWAN_CORE_T const max_idx = (ALWAN_CORE_T)(size - 1);
    int const w = size * size;

    /* Clamp to [0,1] and scale to LUT coordinates */
    ALWAN_CORE_T rf = ALWAN_CORE_SELECT(rgb.v[0] < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, rgb.v[0]);
    ALWAN_CORE_T gf = ALWAN_CORE_SELECT(rgb.v[1] < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, rgb.v[1]);
    ALWAN_CORE_T bf = ALWAN_CORE_SELECT(rgb.v[2] < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, rgb.v[2]);
    rf = ALWAN_CORE_SELECT(rf > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), rf) * max_idx;
    gf = ALWAN_CORE_SELECT(gf > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), gf) * max_idx;
    bf = ALWAN_CORE_SELECT(bf > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), bf) * max_idx;

    /* Integer indices */
    int r0 = (int)rf; int g0 = (int)gf; int b0 = (int)bf;
    int r1 = r0 + 1;  int g1 = g0 + 1;  int b1 = b0 + 1;
    if (r1 >= size) r1 = size - 1;
    if (g1 >= size) g1 = size - 1;
    if (b1 >= size) b1 = size - 1;

    /* Fractional parts */
    ALWAN_CORE_T fr = rf - (ALWAN_CORE_T)r0;
    ALWAN_CORE_T fg = gf - (ALWAN_CORE_T)g0;
    ALWAN_CORE_T fb = bf - (ALWAN_CORE_T)b0;

    /* 2D layout: px = B*size + R, py = G */
    #define LUT2D_AT(rr, gg, bb) \
        (lut2d + ((size_t)(gg) * (size_t)w + (size_t)(bb) * (size_t)size + (size_t)(rr)) * 3)

    ALWAN_CORE_T const *c000 = LUT2D_AT(r0, g0, b0);
    ALWAN_CORE_T const *c100 = LUT2D_AT(r1, g0, b0);
    ALWAN_CORE_T const *c010 = LUT2D_AT(r0, g1, b0);
    ALWAN_CORE_T const *c110 = LUT2D_AT(r1, g1, b0);
    ALWAN_CORE_T const *c001 = LUT2D_AT(r0, g0, b1);
    ALWAN_CORE_T const *c101 = LUT2D_AT(r1, g0, b1);
    ALWAN_CORE_T const *c011 = LUT2D_AT(r0, g1, b1);
    ALWAN_CORE_T const *c111 = LUT2D_AT(r1, g1, b1);

    #undef LUT2D_AT

    /* Trilinear interpolation per channel */
    ALWAN_CORE_VEC3 result;
    for (int ch = 0; ch < 3; ch++) {
        ALWAN_CORE_T c00 = c000[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c100[ch] * fr;
        ALWAN_CORE_T c01 = c001[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c101[ch] * fr;
        ALWAN_CORE_T c10 = c010[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c110[ch] * fr;
        ALWAN_CORE_T c11 = c011[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c111[ch] * fr;

        ALWAN_CORE_T c0 = c00 * (ALWAN_CORE_LITERAL(1.0) - fg) + c10 * fg;
        ALWAN_CORE_T c1 = c01 * (ALWAN_CORE_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_CORE_LITERAL(1.0) - fb) + c1 * fb;
    }

    return result;
}

/* ================================================================
 * 3D LUT trilinear interpolation
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(ALWAN_CORE_T const *lut,
                                               ALWAN_CORE_VEC3 rgb,
                                               int size) {
    ALWAN_CORE_T const max_idx = (ALWAN_CORE_T)(size - 1);

    /* Scale to LUT coordinates */
    ALWAN_CORE_T rf = ALWAN_CORE_SELECT(rgb.v[0] < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, rgb.v[0]);
    ALWAN_CORE_T gf = ALWAN_CORE_SELECT(rgb.v[1] < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, rgb.v[1]);
    ALWAN_CORE_T bf = ALWAN_CORE_SELECT(rgb.v[2] < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, rgb.v[2]);
    rf = ALWAN_CORE_SELECT(rf > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), rf) * max_idx;
    gf = ALWAN_CORE_SELECT(gf > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), gf) * max_idx;
    bf = ALWAN_CORE_SELECT(bf > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), bf) * max_idx;

    /* Integer indices */
    int r0 = (int)rf; int g0 = (int)gf; int b0 = (int)bf;
    int r1 = r0 + 1;  int g1 = g0 + 1;  int b1 = b0 + 1;
    if (r1 >= size) r1 = size - 1;
    if (g1 >= size) g1 = size - 1;
    if (b1 >= size) b1 = size - 1;

    /* Fractional parts */
    ALWAN_CORE_T fr = rf - (ALWAN_CORE_T)r0;
    ALWAN_CORE_T fg = gf - (ALWAN_CORE_T)g0;
    ALWAN_CORE_T fb = bf - (ALWAN_CORE_T)b0;

    /* 8 corner lookups (R-fastest layout) */
    #define LUT3D_AT(rr, gg, bb) \
        (lut + ((size_t)(bb) * (size_t)size * (size_t)size + \
                (size_t)(gg) * (size_t)size + (size_t)(rr)) * 3)

    ALWAN_CORE_T const *c000 = LUT3D_AT(r0, g0, b0);
    ALWAN_CORE_T const *c100 = LUT3D_AT(r1, g0, b0);
    ALWAN_CORE_T const *c010 = LUT3D_AT(r0, g1, b0);
    ALWAN_CORE_T const *c110 = LUT3D_AT(r1, g1, b0);
    ALWAN_CORE_T const *c001 = LUT3D_AT(r0, g0, b1);
    ALWAN_CORE_T const *c101 = LUT3D_AT(r1, g0, b1);
    ALWAN_CORE_T const *c011 = LUT3D_AT(r0, g1, b1);
    ALWAN_CORE_T const *c111 = LUT3D_AT(r1, g1, b1);

    #undef LUT3D_AT

    /* Trilinear interpolation per channel */
    ALWAN_CORE_VEC3 result;
    for (int ch = 0; ch < 3; ch++) {
        ALWAN_CORE_T c00 = c000[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c100[ch] * fr;
        ALWAN_CORE_T c01 = c001[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c101[ch] * fr;
        ALWAN_CORE_T c10 = c010[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c110[ch] * fr;
        ALWAN_CORE_T c11 = c011[ch] * (ALWAN_CORE_LITERAL(1.0) - fr) + c111[ch] * fr;

        ALWAN_CORE_T c0 = c00 * (ALWAN_CORE_LITERAL(1.0) - fg) + c10 * fg;
        ALWAN_CORE_T c1 = c01 * (ALWAN_CORE_LITERAL(1.0) - fg) + c11 * fg;

        result.v[ch] = c0 * (ALWAN_CORE_LITERAL(1.0) - fb) + c1 * fb;
    }

    return result;
}
