/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hunt Color Appearance Model
 * Value-returning variant for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Hunt (1991, 1995)
 * "Revised colour-appearance model for related and unrelated colours"
 */

#ifndef ALWAN_HUNT_CORE_H
#define ALWAN_HUNT_CORE_H

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
#include "alwan_hunt_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_hunt_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* GPU backends - original code */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

typedef struct {
    alwan_scalar J;
    alwan_scalar C;
    alwan_scalar h;
    alwan_scalar s;
    alwan_scalar Q;
    alwan_scalar M;
} alwan_hunt_v_correlates;

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 HUNT_V_M_HPE = {{
#include "../data/matrices/hpe.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_scalar hunt_fn_v(alwan_scalar x) {
    alwan_scalar ax = ALWAN_ABS(x);
    alwan_scalar ax_pow = ALWAN_POW(ax, ALWAN_LITERAL(0.73));
    alwan_scalar magnitude = ALWAN_LITERAL(40.0) * ax_pow / (ax_pow + ALWAN_LITERAL(2.0));
    alwan_scalar sign = ALWAN_SELECT(x >= ALWAN_ZERO, ALWAN_ONE, -ALWAN_ONE);
    return sign * magnitude;
}

ALWAN_INLINE alwan_hunt_v_correlates alwan_hunt_forward_v(
    alwan_xyz xyz,
    alwan_scalar xyz_w_x, alwan_scalar xyz_w_y, alwan_scalar xyz_w_z,
    alwan_scalar La, alwan_scalar Yb, alwan_scalar Nc, alwan_scalar Nb,
    alwan_scalar D) {
    alwan_hunt_v_correlates result;
    ALWAN_UNUSED(Nc); ALWAN_UNUSED(Nb);
    alwan_scalar k = ALWAN_ONE / (ALWAN_LITERAL(5.0) * La + ALWAN_ONE);
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar FL = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * La) +
                      ALWAN_LITERAL(0.1) * (ALWAN_ONE - k4) * (ALWAN_ONE - k4) *
                      ALWAN_POW(ALWAN_LITERAL(5.0) * La, ALWAN_ONE / ALWAN_LITERAL(3.0));
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 lms = alwan_mat3_mulv_v(HUNT_V_M_HPE, xyz_v);
    alwan_scalar lms_l = lms.v[0]; alwan_scalar lms_m = lms.v[1]; alwan_scalar lms_s = lms.v[2];
    alwan_vec3 xyz_w_v = {{xyz_w_x, xyz_w_y, xyz_w_z}};
    alwan_vec3 lms_w = alwan_mat3_mulv_v(HUNT_V_M_HPE, xyz_w_v);
    alwan_scalar lms_w_l = lms_w.v[0]; alwan_scalar lms_w_m = lms_w.v[1]; alwan_scalar lms_w_s = lms_w.v[2];
    ALWAN_UNUSED(lms_w_l); ALWAN_UNUSED(lms_w_m); ALWAN_UNUSED(lms_w_s);
    alwan_scalar adapt_l = (D + ALWAN_ONE - D) * lms_l;
    alwan_scalar adapt_m = (D + ALWAN_ONE - D) * lms_m;
    alwan_scalar adapt_s = (D + ALWAN_ONE - D) * lms_s;
    alwan_scalar lms_n_l = hunt_fn_v(FL * adapt_l);
    alwan_scalar lms_n_m = hunt_fn_v(FL * adapt_m);
    alwan_scalar lms_n_s = hunt_fn_v(FL * adapt_s);
    alwan_scalar C1 = lms_n_l - lms_n_m;
    alwan_scalar C2 = lms_n_m - lms_n_s;
    alwan_scalar C3 = lms_n_s - lms_n_l;
    alwan_scalar h_num = ALWAN_LITERAL(0.5) * (C2 - C3) / ALWAN_LITERAL(4.5);
    alwan_scalar h_den = C1 - C2 / ALWAN_LITERAL(11.0);
    alwan_scalar h_rad = ALWAN_ATAN2(h_num, h_den);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(h_deg < ALWAN_ZERO, h_deg + ALWAN_LITERAL(360.0), h_deg);
    alwan_scalar A = ALWAN_LITERAL(2.0) * lms_n_l + lms_n_m +
                     ALWAN_LITERAL(0.05) * lms_n_s - ALWAN_LITERAL(3.05) + ALWAN_ONE;
    alwan_scalar N1 = ALWAN_POW(ALWAN_LITERAL(7.0), ALWAN_LITERAL(0.6));
    alwan_scalar M_total = ALWAN_SQRT(C1 * C1 + C2 * C2);
    result.Q = N1 * (A + M_total / ALWAN_LITERAL(100.0));
    alwan_scalar Qw = N1 * ALWAN_LITERAL(2.0);
    alwan_scalar Qw_safe = ALWAN_SELECT(Qw > ALWAN_LITERAL(1e-10), Qw, ALWAN_ONE);
    result.J = ALWAN_SELECT(Qw > ALWAN_LITERAL(1e-10),
                            ALWAN_LITERAL(100.0) * ALWAN_POW(result.Q / Qw_safe, ALWAN_LITERAL(0.67)),
                            ALWAN_ZERO);
    alwan_scalar A_safe = ALWAN_SELECT(A > ALWAN_LITERAL(1e-10), A, ALWAN_ONE);
    result.s = ALWAN_SELECT(A > ALWAN_LITERAL(1e-10),
                            ALWAN_LITERAL(50.0) * M_total / A_safe, ALWAN_ZERO);
    alwan_scalar Q_safe = ALWAN_SELECT(result.Q > ALWAN_LITERAL(1e-10), result.Q, ALWAN_ONE);
    alwan_scalar Yw = xyz_w_y;
    alwan_scalar Yb_Yw = ALWAN_SELECT(Yw > ALWAN_LITERAL(1e-10), Yb / Yw, ALWAN_ZERO);
    result.C = ALWAN_SELECT(result.Q > ALWAN_LITERAL(1e-10),
                            ALWAN_LITERAL(2.44) * ALWAN_POW(result.s, ALWAN_LITERAL(0.69)) *
                            ALWAN_POW(Q_safe / Qw_safe, Yb_Yw), ALWAN_ZERO);
    result.M = ALWAN_POW(FL, ALWAN_LITERAL(0.15)) * result.C;
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_HUNT_CORE_H */
