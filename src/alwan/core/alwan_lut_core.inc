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
    const ALWAN_CORE_T inv = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_T)(size - 1);
    const int r = (int)(index % (size_t)size);
    const int g = (int)((index / (size_t)size) % (size_t)size);
    const int b = (int)(index / ((size_t)size * (size_t)size));
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

ALWAN_INLINE void ALWAN_CORE_FNVLIT(alwan_lut2d_dimensions)(int size, ALWAN_PARAM_INT_OUT width, ALWAN_PARAM_INT_OUT height) {
    ALWAN_REF(width)  = size * size;
    ALWAN_REF(height) = size;
}

/* Convert a 3D index (r, g, b each in [0, size-1]) to a 2D pixel coordinate */
ALWAN_INLINE void ALWAN_CORE_FNVLIT(alwan_lut3d_to_2d)(int r, int g, int b, int size,
                                        ALWAN_PARAM_INT_OUT px, ALWAN_PARAM_INT_OUT py) {
    ALWAN_REF(px) = b * size + r;
    ALWAN_REF(py) = g;
}

/* Convert a 2D pixel coordinate back to 3D index */
ALWAN_INLINE void ALWAN_CORE_FNVLIT(alwan_lut2d_to_3d)(int px, int py, int size,
                                        ALWAN_PARAM_INT_OUT r, ALWAN_PARAM_INT_OUT g, ALWAN_PARAM_INT_OUT b) {
    ALWAN_REF(b) = px / size;
    ALWAN_REF(r) = px % size;
    ALWAN_REF(g) = py;
}

/* ================================================================
 * LUT sampling through a pointer
 *
 * Thin names over the table readers, kept for callers. HLSL and GLSL have no
 * pointer to pass, so these are compiled out there: a shader binds its LUT
 * with alwan_table_reader.inc and calls alwan_table1d_sample_linear_<name>,
 * alwan_table2d_sample_trilinear_<name> or alwan_table3d_sample_trilinear_<name>,
 * which are the same three readers.
 * ================================================================ */
#if ALWAN_HAS_POINTERS

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

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_lut1d_sample)(const ALWAN_CORE_T *lut,
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

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lut2d_sample)(const ALWAN_CORE_T *lut2d,
                                               ALWAN_CORE_VEC3 rgb,
                                               int size) {
    return ALWAN_CORE_FNVLIT(alwan_table2d_sample_trilinear)(lut2d, size, rgb);
}

/* ================================================================
 * 3D LUT trilinear interpolation
 *
 * Addressing lives in alwan_table_core.
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(const ALWAN_CORE_T *lut,
                                               ALWAN_CORE_VEC3 rgb,
                                               int size) {
    return ALWAN_CORE_FNVLIT(alwan_table3d_sample_trilinear)(lut, size, rgb);
}

#endif /* ALWAN_HAS_POINTERS */
