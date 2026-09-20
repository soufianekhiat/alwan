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
 * OSA-UCS -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_osa_ucs_to_xyz)(ALWAN_CORE_OSA_UCS osa) {
    ALWAN_CORE_XYZ result;

    /* Step 1: Calculate Lambda from L */
    ALWAN_CORE_T lambda = osa.L * ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(2.0)) + ALWAN_CORE_LITERAL(14.4);

    /* Step 2: Calculate Y0_cbrt from lightness */
    ALWAN_CORE_T Y0_cbrt = (lambda / ALWAN_CORE_LITERAL(5.9)) + ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0);

    /* Step 3: Calculate chroma coefficient C */
    ALWAN_CORE_T C = ALWAN_CORE_SELECT(Y0_cbrt > ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0),
                                  lambda / (ALWAN_CORE_LITERAL(5.9) * (Y0_cbrt - ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0))),
                                  ALWAN_CORE_ONE);

    /* Guard against division by zero */
    ALWAN_CORE_T c_too_small = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(C) < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_ONE, ALWAN_CORE_ZERO);

    /* Step 4: Recover RGB' (cube roots) from j and g */
    ALWAN_CORE_T C_safe = ALWAN_CORE_SELECT(c_too_small > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ONE, C);
    ALWAN_CORE_T j_norm = osa.j / C_safe;
    ALWAN_CORE_T g_norm = osa.g / C_safe;

    /* Approximate inverse RGB' from j_norm, g_norm */
    ALWAN_CORE_T r_cbrt = Y0_cbrt + ALWAN_CORE_LITERAL(0.01) * j_norm - ALWAN_CORE_LITERAL(0.02) * g_norm;
    ALWAN_CORE_T g_cbrt = Y0_cbrt + ALWAN_CORE_LITERAL(0.12) * j_norm + ALWAN_CORE_LITERAL(0.06) * g_norm;
    ALWAN_CORE_T b_cbrt = Y0_cbrt - ALWAN_CORE_LITERAL(0.10) * j_norm - ALWAN_CORE_LITERAL(0.05) * g_norm;

    /* Step 5: Cube each channel (sign-preserving) */
    ALWAN_CORE_T r_lin = ALWAN_CORE_SELECT(r_cbrt >= ALWAN_CORE_ZERO,
                                      r_cbrt * r_cbrt * r_cbrt,
                                      -((-r_cbrt) * (-r_cbrt) * (-r_cbrt)));
    ALWAN_CORE_T g_lin = ALWAN_CORE_SELECT(g_cbrt >= ALWAN_CORE_ZERO,
                                      g_cbrt * g_cbrt * g_cbrt,
                                      -((-g_cbrt) * (-g_cbrt) * (-g_cbrt)));
    ALWAN_CORE_T b_lin = ALWAN_CORE_SELECT(b_cbrt >= ALWAN_CORE_ZERO,
                                      b_cbrt * b_cbrt * b_cbrt,
                                      -((-b_cbrt) * (-b_cbrt) * (-b_cbrt)));

    /* Step 6: RGB to XYZ */
    ALWAN_CORE_VEC3 lin_v = {{r_lin, g_lin, b_lin}};
    ALWAN_CORE_VEC3 xyz_out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(RGB_TO_XYZ_OSA), lin_v);
    ALWAN_CORE_T out_x = xyz_out_v.v[0]; ALWAN_CORE_T out_y = xyz_out_v.v[1]; ALWAN_CORE_T out_z = xyz_out_v.v[2];

    /* Clamp negatives */
    out_x = ALWAN_CORE_SELECT(out_x < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, out_x);
    out_y = ALWAN_CORE_SELECT(out_y < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, out_y);
    out_z = ALWAN_CORE_SELECT(out_z < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, out_z);

    /* Apply zero guard for C too small */
    result.x = ALWAN_CORE_SELECT(c_too_small > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, out_x);
    result.y = ALWAN_CORE_SELECT(c_too_small > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, out_y);
    result.z = ALWAN_CORE_SELECT(c_too_small > ALWAN_CORE_LITERAL(0.5), ALWAN_CORE_ZERO, out_z);

    return result;
}
