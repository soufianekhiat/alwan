/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CAM18sl Color Appearance Model for self-luminous stimuli
 * Reference: Hermans et al. (2018)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_CAM18SL_CORE_H
#define ALWAN_CAM18SL_CORE_H

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
#include "alwan_cam18sl_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_cam18sl_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* GPU backends - original code */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

/* ================================================================
 * CAM18sl Correlates
 * ================================================================ */

typedef struct {
    alwan_scalar Q;   /* Brightness */
    alwan_scalar C;   /* Saturation s = M / Q */
    alwan_scalar h;   /* Hue angle (degrees) */
    alwan_scalar M;   /* Colourfulness */
    alwan_scalar a;   /* M cos h */
    alwan_scalar b;   /* M sin h */
} alwan_cam18sl_v_correlates;

/* CIE 170-2 10 degree cone fundamentals from XYZ: its published LMS to XYZ matrix,
 * X = 1.93986443 L - 1.34664359 M + 0.43044935 S, Y = 0.69283932 L + 0.34967567 M,
 * Z = 2.14687945 S, solved for LMS. */
ALWAN_INLINE alwan_vec3 alwan_cam18sl_xyz_to_lms_v(alwan_xyz xyz) {
    alwan_scalar a11 = ALWAN_LITERAL(1.93986443);
    alwan_scalar a12 = ALWAN_LITERAL(-1.34664359);
    alwan_scalar a13 = ALWAN_LITERAL(0.43044935);
    alwan_scalar a21 = ALWAN_LITERAL(0.69283932);
    alwan_scalar a22 = ALWAN_LITERAL(0.34967567);
    alwan_scalar a33 = ALWAN_LITERAL(2.14687945);
    alwan_vec3 lms;
    alwan_scalar s = xyz.z / a33;
    alwan_scalar xr = xyz.x - a13 * s;
    alwan_scalar det = a11 * a22 - a12 * a21;
    lms.v[0] = (a22 * xr - a12 * xyz.y) / det;
    lms.v[1] = (a11 * xyz.y - a21 * xr) / det;
    lms.v[2] = s;
    return lms;
}

ALWAN_INLINE alwan_xyz alwan_cam18sl_lms_to_xyz_v(alwan_vec3 lms) {
    alwan_xyz xyz;
    xyz.x = ALWAN_LITERAL(1.93986443) * lms.v[0] + ALWAN_LITERAL(-1.34664359) * lms.v[1]
          + ALWAN_LITERAL(0.43044935) * lms.v[2];
    xyz.y = ALWAN_LITERAL(0.69283932) * lms.v[0] + ALWAN_LITERAL(0.34967567) * lms.v[1];
    xyz.z = ALWAN_LITERAL(2.14687945) * lms.v[2];
    return xyz;
}

/* Naka-Rushton, n = 0.58, signed: sign(x) |x|^n / (|x|^n + sig^n). */
ALWAN_INLINE alwan_scalar alwan_cam18sl_nr_v(alwan_scalar x, alwan_scalar sig_n) {
    alwan_scalar ax = ALWAN_ABS(x);
    alwan_scalar p = ALWAN_POW(ax, ALWAN_LITERAL(0.58));
    alwan_scalar r = p / (p + sig_n);
    return ALWAN_SELECT(x < ALWAN_ZERO, -r, r);
}

/* Its inverse: sign(z) | |z| sig^n / (sign(z) - z) |^(1 / n), 0 at 0. */
ALWAN_INLINE alwan_scalar alwan_cam18sl_nr_inv_v(alwan_scalar z, alwan_scalar sig_n) {
    alwan_scalar sgn = ALWAN_SELECT(z < ALWAN_ZERO, -ALWAN_ONE, ALWAN_ONE);
    alwan_scalar az = ALWAN_ABS(z);
    alwan_scalar q = az * sig_n / (sgn - z);
    alwan_scalar aq = ALWAN_ABS(q);
    alwan_scalar r = ALWAN_POW(aq, ALWAN_ONE / ALWAN_LITERAL(0.58));
    r = ALWAN_SELECT(z < ALWAN_ZERO, -r, r);
    return ALWAN_SELECT(az > ALWAN_ZERO, r, ALWAN_ZERO);
}

/* The semi-saturation constant raised to n: sig = 291.20 + 71.8 Y_b^0.78. */
ALWAN_INLINE alwan_scalar alwan_cam18sl_sig_n_v(alwan_scalar Y_b) {
    alwan_scalar yb = ALWAN_SELECT(Y_b > ALWAN_ZERO, Y_b, ALWAN_ZERO);
    alwan_scalar sig = ALWAN_LITERAL(291.20) + ALWAN_LITERAL(71.8) * ALWAN_POW(yb, ALWAN_LITERAL(0.78));
    return ALWAN_POW(sig, ALWAN_LITERAL(0.58));
}

/* Cone signals scaled to rho, gamma, beta: lms / K * k. */
ALWAN_INLINE alwan_vec3 alwan_cam18sl_rgb_v(alwan_vec3 lms) {
    alwan_vec3 r;
    alwan_scalar K = ALWAN_LITERAL(683.144);
    r.v[0] = lms.v[0] / K * ALWAN_LITERAL(676.7);
    r.v[1] = lms.v[1] / K * ALWAN_LITERAL(794.0);
    r.v[2] = lms.v[2] / K * ALWAN_LITERAL(1461.5);
    return r;
}

/* ================================================================
 * CAM18sl Forward Transform: XYZ (cd/m2) -> correlates
 * ================================================================ */

ALWAN_INLINE alwan_cam18sl_v_correlates alwan_cam18sl_forward_v(
    alwan_xyz xyz,
    alwan_scalar Y_b) {
    alwan_cam18sl_v_correlates result;
    alwan_xyz eew;
    eew.x = ALWAN_ONE; eew.y = ALWAN_ONE; eew.z = ALWAN_ONE;
    alwan_vec3 rgb = alwan_cam18sl_rgb_v(alwan_cam18sl_xyz_to_lms_v(xyz));
    alwan_vec3 w = alwan_cam18sl_rgb_v(alwan_cam18sl_xyz_to_lms_v(eew));

    /* von Kries to the equal-energy white of the background's luminance (D = 1); a zero
     * background leaves the signals as they are, as luxpy does. */
    alwan_scalar ra = ALWAN_SELECT(Y_b > ALWAN_ZERO, rgb.v[0] / w.v[0], rgb.v[0]);
    alwan_scalar ga = ALWAN_SELECT(Y_b > ALWAN_ZERO, rgb.v[1] / w.v[1], rgb.v[1]);
    alwan_scalar ba = ALWAN_SELECT(Y_b > ALWAN_ZERO, rgb.v[2] / w.v[2], rgb.v[2]);

    alwan_scalar sig_n = alwan_cam18sl_sig_n_v(Y_b);
    alwan_scalar rc = alwan_cam18sl_nr_v(ra, sig_n);
    alwan_scalar gc = alwan_cam18sl_nr_v(ga, sig_n);
    alwan_scalar bc = alwan_cam18sl_nr_v(ba, sig_n);

    alwan_scalar A = ALWAN_LITERAL(2.0) * rc + gc + bc / ALWAN_LITERAL(20.0);
    alwan_scalar a = ALWAN_LITERAL(0.63) * (rc - ALWAN_LITERAL(12.0) / ALWAN_LITERAL(11.0) * gc
                                                 + bc / ALWAN_LITERAL(11.0));
    alwan_scalar b = ALWAN_LITERAL(0.12) * (rc + gc - ALWAN_LITERAL(2.0) * bc);

    alwan_scalar M = ALWAN_LITERAL(3260.0) * ALWAN_SQRT(a * a + b * b);
    alwan_scalar Q = ALWAN_LITERAL(0.937) * (A + ALWAN_LITERAL(0.0024) * ALWAN_POW(M, ALWAN_LITERAL(1.09)));
    alwan_scalar h = ALWAN_ATAN2(b, a) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    h = ALWAN_SELECT(h < ALWAN_ZERO, h + ALWAN_LITERAL(360.0), h);

    result.Q = Q;
    result.M = M;
    result.C = ALWAN_SELECT(Q != ALWAN_ZERO, M / Q, ALWAN_ZERO);
    result.h = h;
    result.a = ALWAN_LITERAL(3260.0) * a;
    result.b = ALWAN_LITERAL(3260.0) * b;
    return result;
}

/* ================================================================
 * CAM18sl Inverse Transform: Q, M, h -> XYZ (cd/m2)
 * ================================================================ */

ALWAN_INLINE alwan_xyz alwan_cam18sl_inverse_v(
    alwan_cam18sl_v_correlates correlates,
    alwan_scalar Y_b) {
    alwan_scalar M = correlates.M;
    alwan_scalar A = correlates.Q / ALWAN_LITERAL(0.937) - ALWAN_LITERAL(0.0024) * ALWAN_POW(M, ALWAN_LITERAL(1.09));
    alwan_scalar hr = correlates.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar a = M / ALWAN_LITERAL(3260.0) * ALWAN_COS(hr) / ALWAN_LITERAL(0.63);
    alwan_scalar b = M / ALWAN_LITERAL(3260.0) * ALWAN_SIN(hr) / ALWAN_LITERAL(0.12);

    /* The inverse of [2 1 1/20; 1 -12/11 1/11; 1 1 -2], exactly. */
    alwan_scalar rc = ALWAN_LITERAL(20.0) / ALWAN_LITERAL(61.0) * A
                    + ALWAN_LITERAL(451.0) / ALWAN_LITERAL(1403.0) * a
                    + ALWAN_LITERAL(32.0) / ALWAN_LITERAL(1403.0) * b;
    alwan_scalar gc = ALWAN_LITERAL(20.0) / ALWAN_LITERAL(61.0) * A
                    - ALWAN_LITERAL(891.0) / ALWAN_LITERAL(1403.0) * a
                    - ALWAN_LITERAL(29.0) / ALWAN_LITERAL(1403.0) * b;
    alwan_scalar bc = ALWAN_LITERAL(20.0) / ALWAN_LITERAL(61.0) * A
                    - ALWAN_LITERAL(220.0) / ALWAN_LITERAL(1403.0) * a
                    - ALWAN_LITERAL(700.0) / ALWAN_LITERAL(1403.0) * b;

    alwan_scalar sig_n = alwan_cam18sl_sig_n_v(Y_b);
    alwan_scalar ra = alwan_cam18sl_nr_inv_v(rc, sig_n);
    alwan_scalar ga = alwan_cam18sl_nr_inv_v(gc, sig_n);
    alwan_scalar ba = alwan_cam18sl_nr_inv_v(bc, sig_n);

    alwan_xyz eew;
    eew.x = ALWAN_ONE; eew.y = ALWAN_ONE; eew.z = ALWAN_ONE;
    alwan_vec3 w = alwan_cam18sl_rgb_v(alwan_cam18sl_xyz_to_lms_v(eew));
    alwan_scalar r = ALWAN_SELECT(Y_b > ALWAN_ZERO, ra * w.v[0], ra);
    alwan_scalar g = ALWAN_SELECT(Y_b > ALWAN_ZERO, ga * w.v[1], ga);
    alwan_scalar bb = ALWAN_SELECT(Y_b > ALWAN_ZERO, ba * w.v[2], ba);

    alwan_scalar K = ALWAN_LITERAL(683.144);
    alwan_vec3 lms;
    lms.v[0] = r / ALWAN_LITERAL(676.7) * K;
    lms.v[1] = g / ALWAN_LITERAL(794.0) * K;
    lms.v[2] = bb / ALWAN_LITERAL(1461.5) * K;
    return alwan_cam18sl_lms_to_xyz_v(lms);
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CAM18SL_CORE_H */
