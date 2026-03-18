/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only OSA-UCS Color Space (Optical Society of America Uniform Color Scales)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: OSA Uniform Color Scales Committee (1977)
 */

#ifndef ALWAN_OSA_UCS_CORE_H
#define ALWAN_OSA_UCS_CORE_H

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
#include "alwan_osa_ucs_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_osa_ucs_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 XYZ_TO_RGB_OSA = {{
#include "../data/matrices/osa_ucs_xyz_to_rgb.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 RGB_TO_XYZ_OSA = {{
#include "../data/matrices/osa_ucs_rgb_to_xyz.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_scalar alwan_spow_cbrt_v(alwan_scalar val) {
    return ALWAN_SELECT(val >= ALWAN_ZERO,
                        ALWAN_POW(ALWAN_SELECT(val < ALWAN_ZERO, ALWAN_ZERO, val), ALWAN_ONE / ALWAN_LITERAL(3.0)),
                        -ALWAN_POW(ALWAN_SELECT(-val < ALWAN_ZERO, ALWAN_ZERO, -val), ALWAN_ONE / ALWAN_LITERAL(3.0)));
}

ALWAN_INLINE alwan_osa_ucs alwan_xyz_to_osa_ucs_v(alwan_xyz xyz) {
    alwan_osa_ucs result;
    alwan_scalar sum = xyz.x + xyz.y + xyz.z;
    alwan_scalar sum_safe = ALWAN_SELECT(sum < ALWAN_LITERAL(1e-10), ALWAN_ONE, sum);
    alwan_scalar cx = xyz.x / sum_safe;
    alwan_scalar cy = xyz.y / sum_safe;
    alwan_scalar Y = xyz.y;
    alwan_scalar is_black = ALWAN_SELECT(sum < ALWAN_LITERAL(1e-10), ALWAN_ONE, ALWAN_ZERO);
    alwan_scalar k = ALWAN_LITERAL(4.4934) * cx * cx + ALWAN_LITERAL(4.3034) * cy * cy
                   - ALWAN_LITERAL(4.276) * cx * cy - ALWAN_LITERAL(1.3744) * cx
                   - ALWAN_LITERAL(2.5643) * cy + ALWAN_LITERAL(1.8103);
    alwan_scalar Y0 = Y * k;
    Y0 = ALWAN_SELECT(Y0 < ALWAN_ZERO, ALWAN_ZERO, Y0);
    alwan_scalar Y0_cbrt = ALWAN_POW(Y0, ALWAN_ONE / ALWAN_LITERAL(3.0));
    alwan_scalar Y0_minus_30_cbrt = alwan_spow_cbrt_v(Y0 - ALWAN_LITERAL(30.0));
    alwan_scalar Y0_es = Y0_cbrt - ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0);
    alwan_scalar lambda = ALWAN_LITERAL(5.9) * (Y0_es + ALWAN_LITERAL(0.042) * Y0_minus_30_cbrt);
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 rgb_v = alwan_mat3_mulv_v(XYZ_TO_RGB_OSA, xyz_v);
    alwan_scalar r_cbrt = alwan_spow_cbrt_v(rgb_v.v[0]);
    alwan_scalar g_cbrt = alwan_spow_cbrt_v(rgb_v.v[1]);
    alwan_scalar b_cbrt = alwan_spow_cbrt_v(rgb_v.v[2]);
    alwan_scalar C = ALWAN_SELECT(ALWAN_ABS(Y0_es) > ALWAN_LITERAL(1e-10),
                                  lambda / (ALWAN_LITERAL(5.9) * Y0_es), ALWAN_ONE);
    alwan_scalar L_val = (lambda - ALWAN_LITERAL(14.4)) / ALWAN_SQRT(ALWAN_LITERAL(2.0));
    alwan_scalar j_val = C * (ALWAN_LITERAL(1.7) * r_cbrt + ALWAN_LITERAL(8.0) * g_cbrt - ALWAN_LITERAL(9.7) * b_cbrt);
    alwan_scalar g_val = C * (ALWAN_LITERAL(-13.7) * r_cbrt + ALWAN_LITERAL(17.7) * g_cbrt - ALWAN_LITERAL(4.0) * b_cbrt);
    result.L = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, L_val);
    result.j = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, j_val);
    result.g = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, g_val);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_osa_ucs_to_xyz_v(alwan_osa_ucs osa) {
    alwan_xyz result;
    alwan_scalar lambda = osa.L * ALWAN_SQRT(ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(14.4);
    alwan_scalar Y0_cbrt = (lambda / ALWAN_LITERAL(5.9)) + ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0);
    alwan_scalar C = ALWAN_SELECT(Y0_cbrt > ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0),
                                  lambda / (ALWAN_LITERAL(5.9) * (Y0_cbrt - ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0))),
                                  ALWAN_ONE);
    alwan_scalar c_too_small = ALWAN_SELECT(ALWAN_ABS(C) < ALWAN_LITERAL(1e-10), ALWAN_ONE, ALWAN_ZERO);
    alwan_scalar C_safe = ALWAN_SELECT(c_too_small > ALWAN_LITERAL(0.5), ALWAN_ONE, C);
    alwan_scalar j_norm = osa.j / C_safe;
    alwan_scalar g_norm = osa.g / C_safe;
    alwan_scalar r_cbrt = Y0_cbrt + ALWAN_LITERAL(0.01) * j_norm - ALWAN_LITERAL(0.02) * g_norm;
    alwan_scalar g_cbrt = Y0_cbrt + ALWAN_LITERAL(0.12) * j_norm + ALWAN_LITERAL(0.06) * g_norm;
    alwan_scalar b_cbrt = Y0_cbrt - ALWAN_LITERAL(0.10) * j_norm - ALWAN_LITERAL(0.05) * g_norm;
    alwan_scalar r_lin = ALWAN_SELECT(r_cbrt >= ALWAN_ZERO, r_cbrt * r_cbrt * r_cbrt, -((-r_cbrt) * (-r_cbrt) * (-r_cbrt)));
    alwan_scalar g_lin = ALWAN_SELECT(g_cbrt >= ALWAN_ZERO, g_cbrt * g_cbrt * g_cbrt, -((-g_cbrt) * (-g_cbrt) * (-g_cbrt)));
    alwan_scalar b_lin = ALWAN_SELECT(b_cbrt >= ALWAN_ZERO, b_cbrt * b_cbrt * b_cbrt, -((-b_cbrt) * (-b_cbrt) * (-b_cbrt)));
    alwan_vec3 lin_v = {{r_lin, g_lin, b_lin}};
    alwan_vec3 xyz_out_v = alwan_mat3_mulv_v(RGB_TO_XYZ_OSA, lin_v);
    alwan_scalar out_x = ALWAN_SELECT(xyz_out_v.v[0] < ALWAN_ZERO, ALWAN_ZERO, xyz_out_v.v[0]);
    alwan_scalar out_y = ALWAN_SELECT(xyz_out_v.v[1] < ALWAN_ZERO, ALWAN_ZERO, xyz_out_v.v[1]);
    alwan_scalar out_z = ALWAN_SELECT(xyz_out_v.v[2] < ALWAN_ZERO, ALWAN_ZERO, xyz_out_v.v[2]);
    result.x = ALWAN_SELECT(c_too_small > ALWAN_LITERAL(0.5), ALWAN_ZERO, out_x);
    result.y = ALWAN_SELECT(c_too_small > ALWAN_LITERAL(0.5), ALWAN_ZERO, out_y);
    result.z = ALWAN_SELECT(c_too_small > ALWAN_LITERAL(0.5), ALWAN_ZERO, out_z);
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_OSA_UCS_CORE_H */
