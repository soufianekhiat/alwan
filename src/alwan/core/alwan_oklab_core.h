/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Oklab & Oklch Color Spaces
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Bjorn Ottosson (2020)
 */

#ifndef ALWAN_OKLAB_CORE_H
#define ALWAN_OKLAB_CORE_H

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
#include "alwan_oklab_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_oklab_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 OKLAB_M1 = {{
#include "../data/matrices/oklab_m1.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 OKLAB_M2 = {{
#include "../data/matrices/oklab_m2.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 OKLAB_M1_INV = {{
#include "../data/matrices/oklab_m1_inv.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 OKLAB_M2_INV = {{
#include "../data/matrices/oklab_m2_inv.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_oklab alwan_xyz_to_oklab_v(alwan_xyz xyz) {
    alwan_oklab result;

    /* Step 1: XYZ -> LMS via M1 matrix */
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms_v = alwan_mat3_mulv_v(OKLAB_M1, xyz_v);
    alwan_scalar l = lms_v.v[0];
    alwan_scalar m = lms_v.v[1];
    alwan_scalar s = lms_v.v[2];

    /* Step 2: LMS -> LMS' via cube root */
    alwan_scalar l_ = ALWAN_CBRT(l);
    alwan_scalar m_ = ALWAN_CBRT(m);
    alwan_scalar s_ = ALWAN_CBRT(s);

    /* Step 3: LMS' -> Lab via M2 matrix */
    alwan_vec3 lms_p_v = {{l_, m_, s_}};
    alwan_vec3 lab_v = alwan_mat3_mulv_v(OKLAB_M2, lms_p_v);
    result.L = lab_v.v[0];
    result.a = lab_v.v[1];
    result.b = lab_v.v[2];

    return result;
}

ALWAN_INLINE alwan_xyz alwan_oklab_to_xyz_v(alwan_oklab oklab) {
    alwan_xyz result;

    /* Step 1: Lab -> LMS' via M2_INV matrix */
    alwan_vec3 lab_v = {{oklab.L, oklab.a, oklab.b}};
    alwan_vec3 lms_p_v = alwan_mat3_mulv_v(OKLAB_M2_INV, lab_v);
    alwan_scalar l_ = lms_p_v.v[0];
    alwan_scalar m_ = lms_p_v.v[1];
    alwan_scalar s_ = lms_p_v.v[2];

    /* Step 2: LMS' -> LMS via cube */
    alwan_scalar l = l_ * l_ * l_;
    alwan_scalar m = m_ * m_ * m_;
    alwan_scalar s = s_ * s_ * s_;

    /* Step 3: LMS -> XYZ via M1_INV matrix */
    alwan_vec3 lms_v = {{l, m, s}};
    alwan_vec3 xyz_v = alwan_mat3_mulv_v(OKLAB_M1_INV, lms_v);
    result.x = xyz_v.v[0];
    result.y = xyz_v.v[1];
    result.z = xyz_v.v[2];

    return result;
}

ALWAN_INLINE alwan_oklch alwan_oklab_to_oklch_v(alwan_oklab oklab) {
    alwan_oklch result;
    result.L = oklab.L;
    result.C = ALWAN_SQRT(oklab.a * oklab.a + oklab.b * oklab.b);
    result.h = ALWAN_ATAN2(oklab.b, oklab.a);
    return result;
}

ALWAN_INLINE alwan_oklab alwan_oklch_to_oklab_v(alwan_oklch oklch) {
    alwan_oklab result;
    result.L = oklch.L;
    result.a = oklch.C * ALWAN_COS(oklch.h);
    result.b = oklch.C * ALWAN_SIN(oklch.h);
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_OKLAB_CORE_H */
