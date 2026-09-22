/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only OSA-UCS Color Space (Optical Society of America Uniform Color Scales)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: MacAdam, D. L. (1978), "Uniform color scales", Journal of the
 * Optical Society of America 68(1), 121-130, which defines the Y0 quadratic and
 * the Lambda expression used below, for the scales the OSA Uniform Color Scales
 * Committee published in 1977.
 *
 * The XYZ -> RGB_OSA matrix is not hand-entered: gendata/data/osa_ucs_matrices.py
 * takes it from colour-science's MATRIX_XYZ_TO_RGB_OSA_UCS. Suite 24 holds the
 * whole forward transform to colour.XYZ_to_OSA_UCS, so these constants are
 * checked against an implementation of the paper rather than only cited.
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
                        ALWAN_CBRT(ALWAN_SELECT(val < ALWAN_ZERO, ALWAN_ZERO, val)),
                        -ALWAN_CBRT(ALWAN_SELECT(-val < ALWAN_ZERO, ALWAN_ZERO, -val)));
}

ALWAN_INLINE alwan_osa_ucs alwan_xyz_to_osa_ucs_v(alwan_xyz xyz) {
    alwan_osa_ucs result;

    /* Step 1: Convert XYZ to xyY */
    alwan_scalar sum = xyz.x + xyz.y + xyz.z;
    alwan_scalar sum_safe = ALWAN_SELECT(sum < ALWAN_LITERAL(1e-10), ALWAN_ONE, sum);
    alwan_scalar cx = xyz.x / sum_safe;
    alwan_scalar cy = xyz.y / sum_safe;
    alwan_scalar Y = xyz.y;

    /* Guard: black point */
    alwan_scalar is_black = ALWAN_SELECT(sum < ALWAN_LITERAL(1e-10), ALWAN_ONE, ALWAN_ZERO);

    /* Step 2: Y0, the luminance factor.
     *
     * K is the published quadratic in the chromaticity coordinates from MacAdam
     * (1978), and the six coefficients below are reproduced from it rather than
     * fitted here:
     *
     *   K = 4.4934 x^2 + 4.3034 y^2 - 4.276 xy - 1.3744 x - 2.5643 y + 1.8103
     *
     * and Y0 = Y * K. The negative middle terms are the paper's, not a sign slip.
     */
    alwan_scalar k = ALWAN_LITERAL(4.4934) * cx * cx +
                     ALWAN_LITERAL(4.3034) * cy * cy -
                     ALWAN_LITERAL(4.276) * cx * cy -
                     ALWAN_LITERAL(1.3744) * cx -
                     ALWAN_LITERAL(2.5643) * cy +
                     ALWAN_LITERAL(1.8103);
    alwan_scalar Y0 = Y * k;
    Y0 = ALWAN_SELECT(Y0 < ALWAN_ZERO, ALWAN_ZERO, Y0);

    /* Step 3: Lambda, also MacAdam (1978):
     *
     *   Lambda = 5.9 [ Y0^(1/3) - 2/3 + 0.042 (Y0 - 30)^(1/3) ]
     *
     * The second cube root takes a NEGATIVE argument for any Y0 below 30, which
     * is most of the range, so it goes through the sign-preserving cube root
     * rather than a bare pow: pow(x, 1/3) is NaN there.
     */
    alwan_scalar Y0_cbrt = ALWAN_CBRT(Y0);
    alwan_scalar Y0_minus_30 = Y0 - ALWAN_LITERAL(30.0);
    alwan_scalar Y0_minus_30_cbrt = alwan_spow_cbrt_v(Y0_minus_30);
    alwan_scalar Y0_es = Y0_cbrt - ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0);
    alwan_scalar lambda = ALWAN_LITERAL(5.9) * (Y0_es + ALWAN_LITERAL(0.042) * Y0_minus_30_cbrt);

    /* Step 4: Transform XYZ to RGB */
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 rgb_v = alwan_mat3_mulv_v(XYZ_TO_RGB_OSA, xyz_v);
    alwan_scalar r = rgb_v.v[0]; alwan_scalar g = rgb_v.v[1]; alwan_scalar b = rgb_v.v[2];

    /* Step 5: Sign-preserving cube root for each RGB channel */
    alwan_scalar r_cbrt = alwan_spow_cbrt_v(r);
    alwan_scalar g_cbrt = alwan_spow_cbrt_v(g);
    alwan_scalar b_cbrt = alwan_spow_cbrt_v(b);

    /* Step 6: Calculate chroma coefficient C */
    alwan_scalar C = ALWAN_SELECT(ALWAN_ABS(Y0_es) > ALWAN_LITERAL(1e-10),
                                  lambda / (ALWAN_LITERAL(5.9) * Y0_es),
                                  ALWAN_ONE);

    /* Step 7: Calculate OSA-UCS coordinates */
    alwan_scalar L_val = (lambda - ALWAN_LITERAL(14.4)) / ALWAN_SQRT(ALWAN_LITERAL(2.0));
    alwan_scalar j_val = C * (ALWAN_LITERAL(1.7) * r_cbrt + ALWAN_LITERAL(8.0) * g_cbrt - ALWAN_LITERAL(9.7) * b_cbrt);
    alwan_scalar g_val = C * (ALWAN_LITERAL(-13.7) * r_cbrt + ALWAN_LITERAL(17.7) * g_cbrt - ALWAN_LITERAL(4.0) * b_cbrt);

    /* Apply black point guard */
    result.L = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, L_val);
    result.j = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, j_val);
    result.g = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, g_val);

    return result;
}

/* ----------------------------------------------------------------
 * OSA-UCS -> XYZ, the exact inverse
 *
 * Until 2026-09-22 this "approximate" inverse recovered the cube roots of RGB
 * from j and g with three made-up coefficients (0.01, 0.12, 0.10 and friends)
 * and dropped the 0.042 term of Lambda. It returned D65 white as (107, 111,
 * 123), 11 per cent off, and its test printed the miss and passed.
 *
 * The forward model has no closed inverse, but it has a cheap exact one, as
 * colour-science's OSA_UCS_to_XYZ does it:
 *
 *   1. Lambda = sqrt(2) L + 14.4, and Y0 from Lambda by solving the cubic
 *      Lambda/5.9 + 2/3 - t = 0.042 (t^3 - 30)^(1/3), t = Y0^(1/3), which
 *      cubes to a real cubic in t with one real root (Cardano).
 *   2. C = Lambda / (5.9 (t - 2/3)), then a = g / C and b = j / C are the two
 *      linear forms of the cube roots of RGB: two equations in three unknowns.
 *   3. The third is Y0 itself: Y K(x, y) = Y0. Take w = R^(1/3) as the free
 *      variable, solve the 3x3 for the cube roots, cube, go to XYZ, and drive
 *      Y K - Y0 to zero in w by Newton with a one-sided difference. The
 *      reference's start, w = (79.9 + 41.94)^(1/3), converges in a handful of
 *      steps from anywhere the forward model reaches.
 *
 * Fixed step count, no early exit: a core function on a shading language has
 * no data-dependent loop bound to hand a compiler. Twenty steps is the
 * reference's own ceiling; converged iterations are the identity to rounding.
 * Suite 24 pins this to colour-science to 3e-11 and round-trips to 2e-11.
 * ---------------------------------------------------------------- */

/* The Y0 relation of MacAdam 1978, shared by the forward model and the Newton
 * residual below. */
ALWAN_INLINE alwan_scalar osa_ucs_y0_of_xyz_v(alwan_scalar X, alwan_scalar Y, alwan_scalar Z) {
    alwan_scalar sum = X + Y + Z;
    alwan_scalar sum_safe = ALWAN_SELECT(ALWAN_ABS(sum) < ALWAN_LITERAL(1e-10), ALWAN_ONE, sum);
    alwan_scalar cx = X / sum_safe;
    alwan_scalar cy = Y / sum_safe;
    alwan_scalar k = ALWAN_LITERAL(4.4934) * cx * cx +
                     ALWAN_LITERAL(4.3034) * cy * cy -
                     ALWAN_LITERAL(4.276) * cx * cy -
                     ALWAN_LITERAL(1.3744) * cx -
                     ALWAN_LITERAL(2.5643) * cy +
                     ALWAN_LITERAL(1.8103);
    return Y * k;
}

/* XYZ from (a, b, w): the cube roots of RGB solved from
 *   a = -13.7 r' + 17.7 g' - 4.0 b'
 *   b =   1.7 r' +  8.0 g' - 9.7 b'
 *   w =       r'
 * which is the inverse of the augmented 3x3, written out. */
ALWAN_INLINE alwan_vec3 osa_ucs_xyz_of_abw_v(alwan_scalar a, alwan_scalar b, alwan_scalar w) {
    /* From the second row: 8.0 g' = b - 1.7 w + 9.7 b'. From the first:
     * 17.7 g' = a + 13.7 w + 4.0 b'. Eliminate g':
     *   17.7 (b - 1.7 w + 9.7 b') = 8.0 (a + 13.7 w + 4.0 b')
     *   b' (17.7 * 9.7 - 8.0 * 4.0) = 8.0 a + (8.0 * 13.7 + 17.7 * 1.7) w - 17.7 b */
    alwan_scalar b_cbrt = (ALWAN_LITERAL(8.0) * a +
                           (ALWAN_LITERAL(8.0) * ALWAN_LITERAL(13.7) + ALWAN_LITERAL(17.7) * ALWAN_LITERAL(1.7)) * w -
                           ALWAN_LITERAL(17.7) * b) /
                          (ALWAN_LITERAL(17.7) * ALWAN_LITERAL(9.7) - ALWAN_LITERAL(8.0) * ALWAN_LITERAL(4.0));
    alwan_scalar g_cbrt = (b - ALWAN_LITERAL(1.7) * w + ALWAN_LITERAL(9.7) * b_cbrt) / ALWAN_LITERAL(8.0);
    alwan_scalar r_cbrt = w;
    alwan_vec3 rgb;
    rgb.v[0] = r_cbrt * r_cbrt * r_cbrt;
    rgb.v[1] = g_cbrt * g_cbrt * g_cbrt;
    rgb.v[2] = b_cbrt * b_cbrt * b_cbrt;
    return alwan_mat3_mulv_v(RGB_TO_XYZ_OSA, rgb);
}

ALWAN_INLINE alwan_xyz alwan_osa_ucs_to_xyz_v(alwan_osa_ucs osa) {
    alwan_xyz result;
    const alwan_scalar two_thirds = ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0);
    const alwan_scalar third = ALWAN_ONE / ALWAN_LITERAL(3.0);

    /* Step 1: Lambda, then Y0^(1/3) by Cardano on
     *   -(v + 1) t^3 + 3u t^2 - 3u^2 t + (u^3 + 30 v) = 0,  u = Lambda/5.9 + 2/3, v = 0.042^3 */
    alwan_scalar lambda = osa.L * ALWAN_SQRT(ALWAN_LITERAL(2.0)) + ALWAN_LITERAL(14.4);
    alwan_scalar u = lambda / ALWAN_LITERAL(5.9) + two_thirds;
    alwan_scalar v = ALWAN_LITERAL(0.042) * ALWAN_LITERAL(0.042) * ALWAN_LITERAL(0.042);
    alwan_scalar ca = -(v + ALWAN_ONE);
    alwan_scalar cb = ALWAN_LITERAL(3.0) * u;
    alwan_scalar cc = -ALWAN_LITERAL(3.0) * u * u;
    alwan_scalar cd = u * u * u + ALWAN_LITERAL(30.0) * v;
    alwan_scalar p = (ALWAN_LITERAL(3.0) * ca * cc - cb * cb) / (ALWAN_LITERAL(3.0) * ca * ca);
    alwan_scalar q = (ALWAN_LITERAL(2.0) * cb * cb * cb - ALWAN_LITERAL(9.0) * ca * cb * cc +
                      ALWAN_LITERAL(27.0) * ca * ca * cd) / (ALWAN_LITERAL(27.0) * ca * ca * ca);
    alwan_scalar half_q = q / ALWAN_LITERAL(2.0);
    alwan_scalar disc = half_q * half_q + (p * third) * (p * third) * (p * third);
    /* One real root: the discriminant is positive over the model's range. Below
     * zero, which only a Lambda outside the model reaches, its root is held at 0. */
    alwan_scalar sq = ALWAN_SQRT(ALWAN_SELECT(disc < ALWAN_ZERO, ALWAN_ZERO, disc));
    alwan_scalar t = -cb / (ALWAN_LITERAL(3.0) * ca) +
                     alwan_spow_cbrt_v(-half_q + sq) +
                     alwan_spow_cbrt_v(-half_q - sq);
    alwan_scalar Y0 = t * t * t;

    /* Step 2: C, and the two linear forms of the cube roots */
    alwan_scalar denom = ALWAN_LITERAL(5.9) * (t - two_thirds);
    alwan_scalar C = lambda / ALWAN_SELECT(ALWAN_ABS(denom) < ALWAN_LITERAL(1e-10), ALWAN_ONE, denom);
    alwan_scalar C_safe = ALWAN_SELECT(ALWAN_ABS(C) < ALWAN_LITERAL(1e-10), ALWAN_ONE, C);
    alwan_scalar a = osa.g / C_safe;
    alwan_scalar b = osa.j / C_safe;

    /* Step 3: Newton on w = R^(1/3), residual Y K(x, y) - Y0 */
    alwan_scalar w = ALWAN_CBRT(ALWAN_LITERAL(79.9) + ALWAN_LITERAL(41.94));
    const alwan_scalar eps = ALWAN_LITERAL(1e-8);
    alwan_vec3 xyz = osa_ucs_xyz_of_abw_v(a, b, w);
    int it;
    for (it = 0; it < 20; it++) {
        alwan_scalar err = osa_ucs_y0_of_xyz_v(xyz.v[0], xyz.v[1], xyz.v[2]) - Y0;
        alwan_vec3 xyz_p = osa_ucs_xyz_of_abw_v(a, b, w + eps);
        alwan_scalar err_p = osa_ucs_y0_of_xyz_v(xyz_p.v[0], xyz_p.v[1], xyz_p.v[2]) - Y0;
        alwan_scalar deriv = (err_p - err) / eps;
        alwan_scalar step = err / ALWAN_SELECT(ALWAN_ABS(deriv) < ALWAN_LITERAL(1e-30), ALWAN_ONE, deriv);
        /* A converged iterate is left alone, so the step count cannot walk a
         * solved point off by rounding. */
        w = w - ALWAN_SELECT(ALWAN_ABS(err) < ALWAN_LITERAL(1e-10), ALWAN_ZERO, step);
        xyz = osa_ucs_xyz_of_abw_v(a, b, w);
    }

    /* Black is the forward model's own guard, (0, 0, 0) for L = j = g = 0. */
    alwan_scalar is_black = ALWAN_SELECT(osa.L == ALWAN_ZERO && osa.j == ALWAN_ZERO && osa.g == ALWAN_ZERO,
                                              ALWAN_ONE, ALWAN_ZERO);
    result.x = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, xyz.v[0]);
    result.y = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, xyz.v[1]);
    result.z = ALWAN_SELECT(is_black > ALWAN_LITERAL(0.5), ALWAN_ZERO, xyz.v[2]);
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_OSA_UCS_CORE_H */
