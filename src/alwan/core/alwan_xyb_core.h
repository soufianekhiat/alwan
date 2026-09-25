/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only XYB (JPEG XL)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Linear sRGB goes to an LMS by the opsin matrix, takes a biased cube root, and
 * becomes X = (L' - M') / 2, Y = (L' + M') / 2, B = S' - Y, so that X = B = 0 on
 * the grey axis. Reference: JPEG XL white paper; constants as ColorAide carries them.
 */

#ifndef ALWAN_XYB_CORE_H
#define ALWAN_XYB_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_xyb_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_xyb_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 XYB_LRGB_TO_LMS = {{
#include "../data/matrices/xyb_lrgb_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 XYB_LMS_TO_LRGB = {{
#include "../data/matrices/xyb_lms_to_lrgb.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 XYB_XYZ_TO_LMS = {{
#include "../data/matrices/xyb_xyz_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 XYB_LMS_TO_XYZ = {{
#include "../data/matrices/xyb_lms_to_xyz.csv"
}};
/* the bias, and its cube root */
ALWAN_CONSTEXPR alwan_scalar XYB_BIAS[2] = {
#include "../data/xyb_bias.csv"
};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_xyb alwan_xyb_from_lms_v(alwan_vec3 lms) {
    alwan_xyb result;
    alwan_scalar l_ = ALWAN_CBRT(lms.v[0] + XYB_BIAS[0]) - XYB_BIAS[1];
    alwan_scalar m_ = ALWAN_CBRT(lms.v[1] + XYB_BIAS[0]) - XYB_BIAS[1];
    alwan_scalar s_ = ALWAN_CBRT(lms.v[2] + XYB_BIAS[0]) - XYB_BIAS[1];
    result.x = ALWAN_LITERAL(0.5) * l_ - ALWAN_LITERAL(0.5) * m_;
    result.y = ALWAN_LITERAL(0.5) * l_ + ALWAN_LITERAL(0.5) * m_;
    result.b = s_ - result.y;
    return result;
}

ALWAN_INLINE alwan_vec3 alwan_xyb_to_lms_v(alwan_xyb xyb) {
    alwan_vec3 lms;
    alwan_scalar l_ = xyb.x + xyb.y + XYB_BIAS[1];
    alwan_scalar m_ = xyb.y - xyb.x + XYB_BIAS[1];
    alwan_scalar s_ = xyb.b + xyb.y + XYB_BIAS[1];
    lms.v[0] = l_ * l_ * l_ - XYB_BIAS[0];
    lms.v[1] = m_ * m_ * m_ - XYB_BIAS[0];
    lms.v[2] = s_ * s_ * s_ - XYB_BIAS[0];
    return lms;
}

ALWAN_INLINE alwan_xyb alwan_linear_srgb_to_xyb_v(alwan_rgb rgb) {
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    return alwan_xyb_from_lms_v(alwan_mat3_mulv_v(XYB_LRGB_TO_LMS, rgb_v));
}

ALWAN_INLINE alwan_rgb alwan_xyb_to_linear_srgb_v(alwan_xyb xyb) {
    alwan_rgb result;
    alwan_vec3 rgb_v = alwan_mat3_mulv_v(XYB_LMS_TO_LRGB, alwan_xyb_to_lms_v(xyb));
    result.r = rgb_v.v[0];
    result.g = rgb_v.v[1];
    result.b = rgb_v.v[2];
    return result;
}

ALWAN_INLINE alwan_xyb alwan_xyz_to_xyb_v(alwan_xyz xyz) {
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    return alwan_xyb_from_lms_v(alwan_mat3_mulv_v(XYB_XYZ_TO_LMS, xyz_v));
}

ALWAN_INLINE alwan_xyz alwan_xyb_to_xyz_v(alwan_xyb xyb) {
    alwan_xyz result;
    alwan_vec3 xyz_v = alwan_mat3_mulv_v(XYB_LMS_TO_XYZ, alwan_xyb_to_lms_v(xyb));
    result.x = xyz_v.v[0];
    result.y = xyz_v.v[1];
    result.z = xyz_v.v[2];
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_XYB_CORE_H */
