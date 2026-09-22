/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only OSA-UCS Color Space (parameterized)
 * No include guard - included twice via setup/teardown.
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

/* ----------------------------------------------------------------
 * OSA-UCS Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(XYZ_TO_RGB_OSA) = {{
#include "../data/matrices/osa_ucs_xyz_to_rgb.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(RGB_TO_XYZ_OSA) = {{
#include "../data/matrices/osa_ucs_rgb_to_xyz.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Sign-preserving cube root
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_spow_cbrt)(ALWAN_CORE_T val) {
    return ALWAN_CORE_SELECT(val >= ALWAN_CORE_ZERO,
                        ALWAN_CORE_CBRT(ALWAN_CORE_SELECT(val < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, val)),
                        -ALWAN_CORE_CBRT(ALWAN_CORE_SELECT(-val < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, -val)));
}

/* ----------------------------------------------------------------
 * XYZ -> OSA-UCS (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_OSA_UCS ALWAN_CORE_FNVLIT(alwan_xyz_to_osa_ucs)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_OSA_UCS result;

    /* Step 1: Convert XYZ to xyY */
    ALWAN_CORE_T sum = xyz.x + xyz.y + xyz.z;
    ALWAN_CORE_T sum_safe = ALWAN_CORE_SELECT(sum < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ONE, sum);
    ALWAN_CORE_T cx = xyz.x / sum_safe;
    ALWAN_CORE_T cy = xyz.y / sum_safe;
    ALWAN_CORE_T Y = xyz.y;

    /* Guard: black point */
    ALWAN_CORE_T is_black = ALWAN_CORE_SELECT(sum < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ONE, ALWAN_CORE_ZERO);

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
    ALWAN_CORE_T k = ALWAN_CORE_LITERAL(4.4934) * cx * cx +
                     ALWAN_CORE_LITERAL(4.3034) * cy * cy -
                     ALWAN_CORE_LITERAL(4.276) * cx * cy -
                     ALWAN_CORE_LITERAL(1.3744) * cx -
                     ALWAN_CORE_LITERAL(2.5643) * cy +
                     ALWAN_CORE_LITERAL(1.8103);
    ALWAN_CORE_T Y0 = Y * k;
    Y0 = ALWAN_CORE_SELECT(Y0 < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, Y0);

    /* Step 3: Lambda, also MacAdam (1978):
     *
     *   Lambda = 5.9 [ Y0^(1/3) - 2/3 + 0.042 (Y0 - 30)^(1/3) ]
     *
     * The second cube root takes a NEGATIVE argument for any Y0 below 30, which
     * is most of the range, so it goes through the sign-preserving cube root
     * rather than a bare pow: pow(x, 1/3) is NaN there.
     */
    ALWAN_CORE_T Y0_cbrt = ALWAN_CORE_CBRT(Y0);
    ALWAN_CORE_T Y0_minus_30 = Y0 - ALWAN_CORE_LITERAL(30.0);
    ALWAN_CORE_T Y0_minus_30_cbrt = ALWAN_CORE_FNVLIT(alwan_spow_cbrt)(Y0_minus_30);
    ALWAN_CORE_T Y0_es = Y0_cbrt - ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T lambda = ALWAN_CORE_LITERAL(5.9) * (Y0_es + ALWAN_CORE_LITERAL(0.042) * Y0_minus_30_cbrt);

    /* Step 4: Transform XYZ to RGB */
    ALWAN_CORE_VEC3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    ALWAN_CORE_VEC3 rgb_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(XYZ_TO_RGB_OSA), xyz_v);
    ALWAN_CORE_T r = rgb_v.v[0]; ALWAN_CORE_T g = rgb_v.v[1]; ALWAN_CORE_T b = rgb_v.v[2];

    /* Step 5: Sign-preserving cube root for each RGB channel */
    ALWAN_CORE_T r_cbrt = ALWAN_CORE_FNVLIT(alwan_spow_cbrt)(r);
    ALWAN_CORE_T g_cbrt = ALWAN_CORE_FNVLIT(alwan_spow_cbrt)(g);
    ALWAN_CORE_T b_cbrt = ALWAN_CORE_FNVLIT(alwan_spow_cbrt)(b);

    /* Step 6: Calculate chroma coefficient C */
    ALWAN_CORE_T C = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(Y0_es) > ALWAN_CORE_LITERAL(1e-10),
                                  lambda / (ALWAN_CORE_LITERAL(5.9) * Y0_es),
                                  ALWAN_CORE_ONE);

    /* Step 7: Calculate OSA-UCS coordinates */
    ALWAN_CORE_T L_val = (lambda - ALWAN_CORE_LITERAL(14.4)) / ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(2.0));
    ALWAN_CORE_T j_val = C * (ALWAN_CORE_LITERAL(1.7) * r_cbrt + ALWAN_CORE_LITERAL(8.0) * g_cbrt - ALWAN_CORE_LITERAL(9.7) * b_cbrt);
    ALWAN_CORE_T g_val = C * (ALWAN_CORE_LITERAL(-13.7) * r_cbrt + ALWAN_CORE_LITERAL(17.7) * g_cbrt - ALWAN_CORE_LITERAL(4.0) * b_cbrt);

    /* Apply black point guard */
    result.L = ALWAN_CORE_SELECT(is_black > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, L_val);
    result.j = ALWAN_CORE_SELECT(is_black > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, j_val);
    result.g = ALWAN_CORE_SELECT(is_black > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, g_val);

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
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(osa_ucs_y0_of_xyz)(ALWAN_CORE_T X, ALWAN_CORE_T Y, ALWAN_CORE_T Z) {
    ALWAN_CORE_T sum = X + Y + Z;
    ALWAN_CORE_T sum_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sum) < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ONE, sum);
    ALWAN_CORE_T cx = X / sum_safe;
    ALWAN_CORE_T cy = Y / sum_safe;
    ALWAN_CORE_T k = ALWAN_CORE_LITERAL(4.4934) * cx * cx +
                     ALWAN_CORE_LITERAL(4.3034) * cy * cy -
                     ALWAN_CORE_LITERAL(4.276) * cx * cy -
                     ALWAN_CORE_LITERAL(1.3744) * cx -
                     ALWAN_CORE_LITERAL(2.5643) * cy +
                     ALWAN_CORE_LITERAL(1.8103);
    return Y * k;
}

/* XYZ from (a, b, w): the cube roots of RGB solved from
 *   a = -13.7 r' + 17.7 g' - 4.0 b'
 *   b =   1.7 r' +  8.0 g' - 9.7 b'
 *   w =       r'
 * which is the inverse of the augmented 3x3, written out. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(osa_ucs_xyz_of_abw)(ALWAN_CORE_T a, ALWAN_CORE_T b, ALWAN_CORE_T w) {
    /* From the second row: 8.0 g' = b - 1.7 w + 9.7 b'. From the first:
     * 17.7 g' = a + 13.7 w + 4.0 b'. Eliminate g':
     *   17.7 (b - 1.7 w + 9.7 b') = 8.0 (a + 13.7 w + 4.0 b')
     *   b' (17.7 * 9.7 - 8.0 * 4.0) = 8.0 a + (8.0 * 13.7 + 17.7 * 1.7) w - 17.7 b */
    ALWAN_CORE_T b_cbrt = (ALWAN_CORE_LITERAL(8.0) * a +
                           (ALWAN_CORE_LITERAL(8.0) * ALWAN_CORE_LITERAL(13.7) + ALWAN_CORE_LITERAL(17.7) * ALWAN_CORE_LITERAL(1.7)) * w -
                           ALWAN_CORE_LITERAL(17.7) * b) /
                          (ALWAN_CORE_LITERAL(17.7) * ALWAN_CORE_LITERAL(9.7) - ALWAN_CORE_LITERAL(8.0) * ALWAN_CORE_LITERAL(4.0));
    ALWAN_CORE_T g_cbrt = (b - ALWAN_CORE_LITERAL(1.7) * w + ALWAN_CORE_LITERAL(9.7) * b_cbrt) / ALWAN_CORE_LITERAL(8.0);
    ALWAN_CORE_T r_cbrt = w;
    ALWAN_CORE_VEC3 rgb;
    rgb.v[0] = r_cbrt * r_cbrt * r_cbrt;
    rgb.v[1] = g_cbrt * g_cbrt * g_cbrt;
    rgb.v[2] = b_cbrt * b_cbrt * b_cbrt;
    return ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RGB_TO_XYZ_OSA), rgb);
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_osa_ucs_to_xyz)(ALWAN_CORE_OSA_UCS osa) {
    ALWAN_CORE_XYZ result;
    const ALWAN_CORE_T two_thirds = ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T third = ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0);

    /* Step 1: Lambda, then Y0^(1/3) by Cardano on
     *   -(v + 1) t^3 + 3u t^2 - 3u^2 t + (u^3 + 30 v) = 0,  u = Lambda/5.9 + 2/3, v = 0.042^3 */
    ALWAN_CORE_T lambda = osa.L * ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(14.4);
    ALWAN_CORE_T u = lambda / ALWAN_CORE_LITERAL(5.9) + two_thirds;
    ALWAN_CORE_T v = ALWAN_CORE_LITERAL(0.042) * ALWAN_CORE_LITERAL(0.042) * ALWAN_CORE_LITERAL(0.042);
    ALWAN_CORE_T ca = -(v + ALWAN_CORE_ONE);
    ALWAN_CORE_T cb = ALWAN_CORE_LITERAL(3.0) * u;
    ALWAN_CORE_T cc = -ALWAN_CORE_LITERAL(3.0) * u * u;
    ALWAN_CORE_T cd = u * u * u + ALWAN_CORE_LITERAL(30.0) * v;
    ALWAN_CORE_T p = (ALWAN_CORE_LITERAL(3.0) * ca * cc - cb * cb) / (ALWAN_CORE_LITERAL(3.0) * ca * ca);
    ALWAN_CORE_T q = (ALWAN_CORE_LITERAL(2.0) * cb * cb * cb - ALWAN_CORE_LITERAL(9.0) * ca * cb * cc +
                      ALWAN_CORE_LITERAL(27.0) * ca * ca * cd) / (ALWAN_CORE_LITERAL(27.0) * ca * ca * ca);
    ALWAN_CORE_T half_q = q / ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T disc = half_q * half_q + (p * third) * (p * third) * (p * third);
    /* One real root: the discriminant is positive over the model's range. Below
     * zero, which only a Lambda outside the model reaches, its root is held at 0. */
    ALWAN_CORE_T sq = ALWAN_CORE_SQRT(ALWAN_CORE_SELECT(disc < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, disc));
    ALWAN_CORE_T t = -cb / (ALWAN_CORE_LITERAL(3.0) * ca) +
                     ALWAN_CORE_FNVLIT(alwan_spow_cbrt)(-half_q + sq) +
                     ALWAN_CORE_FNVLIT(alwan_spow_cbrt)(-half_q - sq);
    ALWAN_CORE_T Y0 = t * t * t;

    /* Step 2: C, and the two linear forms of the cube roots */
    ALWAN_CORE_T denom = ALWAN_CORE_LITERAL(5.9) * (t - two_thirds);
    ALWAN_CORE_T C = lambda / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(denom) < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ONE, denom);
    ALWAN_CORE_T C_safe = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(C) < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ONE, C);
    ALWAN_CORE_T a = osa.g / C_safe;
    ALWAN_CORE_T b = osa.j / C_safe;

    /* Step 3: Newton on w = R^(1/3), residual Y K(x, y) - Y0 */
    ALWAN_CORE_T w = ALWAN_CORE_CBRT(ALWAN_CORE_LITERAL(79.9) + ALWAN_CORE_LITERAL(41.94));
    const ALWAN_CORE_T eps = ALWAN_CORE_LITERAL(1e-8);
    ALWAN_CORE_VEC3 xyz = ALWAN_CORE_FNVLIT(osa_ucs_xyz_of_abw)(a, b, w);
    int it;
    for (it = 0; it < 20; it++) {
        ALWAN_CORE_T err = ALWAN_CORE_FNVLIT(osa_ucs_y0_of_xyz)(xyz.v[0], xyz.v[1], xyz.v[2]) - Y0;
        ALWAN_CORE_VEC3 xyz_p = ALWAN_CORE_FNVLIT(osa_ucs_xyz_of_abw)(a, b, w + eps);
        ALWAN_CORE_T err_p = ALWAN_CORE_FNVLIT(osa_ucs_y0_of_xyz)(xyz_p.v[0], xyz_p.v[1], xyz_p.v[2]) - Y0;
        ALWAN_CORE_T deriv = (err_p - err) / eps;
        ALWAN_CORE_T step = err / ALWAN_CORE_SELECT(ALWAN_CORE_ABS(deriv) < ALWAN_CORE_LITERAL(1e-30), ALWAN_CORE_ONE, deriv);
        /* A converged iterate is left alone, so the step count cannot walk a
         * solved point off by rounding. */
        w = w - ALWAN_CORE_SELECT(ALWAN_CORE_ABS(err) < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ZERO, step);
        xyz = ALWAN_CORE_FNVLIT(osa_ucs_xyz_of_abw)(a, b, w);
    }

    /* Black is the forward model's own guard, (0, 0, 0) for L = j = g = 0. */
    ALWAN_CORE_T is_black = ALWAN_CORE_SELECT(osa.L == ALWAN_CORE_ZERO && osa.j == ALWAN_CORE_ZERO && osa.g == ALWAN_CORE_ZERO,
                                              ALWAN_CORE_ONE, ALWAN_CORE_ZERO);
    result.x = ALWAN_CORE_SELECT(is_black > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, xyz.v[0]);
    result.y = ALWAN_CORE_SELECT(is_black > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, xyz.v[1]);
    result.z = ALWAN_CORE_SELECT(is_black > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, xyz.v[2]);
    return result;
}
