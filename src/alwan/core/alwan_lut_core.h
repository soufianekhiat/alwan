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
#include "alwan_table_core.h"

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
    const alwan_scalar inv = ALWAN_LITERAL(1.0) / (alwan_scalar)(size - 1);
    const int r = (int)(index % (size_t)size);
    const int g = (int)((index / (size_t)size) % (size_t)size);
    const int b = (int)(index / ((size_t)size * (size_t)size));
    alwan_vec3 result;
    result.v[0] = (alwan_scalar)r * inv;
    result.v[1] = (alwan_scalar)g * inv;
    result.v[2] = (alwan_scalar)b * inv;
    return result;
}

ALWAN_INLINE alwan_scalar alwan_lut1d_index_to_val_v(size_t index, int size) {
    return (alwan_scalar)index / (alwan_scalar)(size - 1);
}

ALWAN_INLINE void alwan_lut2d_dimensions_v(int size, ALWAN_PARAM_INT_OUT width, ALWAN_PARAM_INT_OUT height) {
    ALWAN_REF(width)  = size * size;
    ALWAN_REF(height) = size;
}

ALWAN_INLINE void alwan_lut3d_to_2d_v(int r, int g, int b, int size,
                                        ALWAN_PARAM_INT_OUT px, ALWAN_PARAM_INT_OUT py) {
    ALWAN_REF(px) = b * size + r;
    ALWAN_REF(py) = g;
}

ALWAN_INLINE void alwan_lut2d_to_3d_v(int px, int py, int size,
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

ALWAN_INLINE alwan_scalar alwan_lut1d_sample_v(const alwan_scalar *lut,
                                                 alwan_scalar t,
                                                 int size) {
    return alwan_table1d_sample_linear_v(lut, size, t);
}

ALWAN_INLINE alwan_vec3 alwan_lut2d_sample_v(const alwan_scalar *lut2d,
                                               alwan_vec3 rgb,
                                               int size) {
    return alwan_table2d_sample_trilinear_v(lut2d, size, rgb);
}

ALWAN_INLINE alwan_vec3 alwan_lut3d_sample_v(const alwan_scalar *lut,
                                               alwan_vec3 rgb,
                                               int size) {
    return alwan_table3d_sample_trilinear_v(lut, size, rgb);
}

#endif /* ALWAN_HAS_POINTERS */

#endif

#endif /* ALWAN_LUT_CORE_H */
