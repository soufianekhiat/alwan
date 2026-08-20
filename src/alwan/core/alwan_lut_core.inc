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
 *
 * Addressing, including the NaN guard that keeps (int)t out of undefined
 * behaviour, lives in alwan_table_core. Signature kept for callers.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_lut1d_sample)(ALWAN_CORE_T const *lut,
                                                 ALWAN_CORE_T t,
                                                 int size) {
    return ALWAN_CORE_FNVLIT(alwan_table1d_sample_linear)(lut, size, t);
}

/* ================================================================
 * 2D LUT sampling (flattened 3D strip)
 *
 * The strip is a flattened cube, so this is a TRILINEAR sample over r, g and
 * b, not a bilinear one. Addressing lives in alwan_table_core.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lut2d_sample)(ALWAN_CORE_T const *lut2d,
                                               ALWAN_CORE_VEC3 rgb,
                                               int size) {
    return ALWAN_CORE_FNVLIT(alwan_table2d_sample_trilinear)(lut2d, size, rgb);
}

/* ================================================================
 * 3D LUT trilinear interpolation
 *
 * Addressing lives in alwan_table_core.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(ALWAN_CORE_T const *lut,
                                               ALWAN_CORE_VEC3 rgb,
                                               int size) {
    return ALWAN_CORE_FNVLIT(alwan_table3d_sample_trilinear)(lut, size, rgb);
}
