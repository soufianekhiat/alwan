/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Extended Color Spaces & Models
 * hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb, Prismatic, HCL, IHLS
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_EXTENDED_CORE_H
#define ALWAN_EXTENDED_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"
#include "alwan_core.h"

/* ================================================================
 * Matrix Data (static const, CSV-embedded)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* hdr-IPT matrices */
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_LMS_TO_IPT_HDR = {{
#include "../data/matrices/lms_to_ipt_hdr.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_IPT_TO_LMS_HDR = {{
#include "../data/matrices/ipt_to_lms_hdr.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_XYZ_TO_LMS_IPT = {{
#include "../data/matrices/xyz_to_lms_ipt.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_LMS_TO_XYZ_IPT = {{
#include "../data/matrices/lms_to_xyz_ipt.csv"
}};

/* IgPgTg matrices */
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_LMS_TO_IGPGTG = {{
#include "../data/matrices/lms_to_igpgtg.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_IGPGTG_TO_LMS = {{
#include "../data/matrices/igpgtg_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_XYZ_TO_LMS_IGPGTG = {{
#include "../data/matrices/xyz_to_lms_igpgtg.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_LMS_TO_XYZ_IGPGTG = {{
#include "../data/matrices/lms_to_xyz_igpgtg.csv"
}};

/* ICaCb matrices */
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_LMS_TO_ICACB = {{
#include "../data/matrices/lms_to_icacb.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_ICACB_TO_LMS = {{
#include "../data/matrices/icacb_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_XYZ_TO_LMS_ICACB = {{
#include "../data/matrices/xyz_to_lms_icacb.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_LMS_TO_XYZ_ICACB = {{
#include "../data/matrices/lms_to_xyz_icacb.csv"
}};

/* IgPgTg LMS scaling factors */
static alwan_scalar const ALWAN_EXT_IGPGTG_LMS_SCALE[3] = {
#include "../data/igpgtg_lms_scale.csv"
};

/* D65 white point for HDR calculations (Y=1 scale) */
static alwan_scalar const ALWAN_EXT_HDR_D65_WHITE[3] = {
#include "../data/hdr_d65_white.csv"
};

/* IHLS matrices */
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_IHLS_RGB_TO_YC1C2 = {{
#include "../data/ihls_rgb_to_yc1c2.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ALWAN_EXT_IHLS_YC1C2_TO_RGB = {{
#include "../data/ihls_yc1c2_to_rgb.csv"
}};

ALWAN_DIAG_POP

/* ================================================================
 * Sign-preserving power (spow)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_spow_v(alwan_scalar x, alwan_scalar p) {
    return ALWAN_SELECT(x >= ALWAN_LITERAL(0.0),
                        ALWAN_POW(x, p),
                        -ALWAN_POW(-x, p));
}

/* ================================================================
 * Michaelis-Menten lightness (Fairchild 2011)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_lightness_fairchild2011_v(alwan_scalar Y, alwan_scalar epsilon) {
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);
    alwan_scalar Y_p = alwan_spow_v(Y, epsilon);
    return (V_max * Y_p) / (K_m + Y_p) + ALWAN_LITERAL(0.02);
}

/* ================================================================
 * Inverse Michaelis-Menten (Fairchild 2011)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_luminance_fairchild2011_v(alwan_scalar L_hdr, alwan_scalar epsilon) {
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);
    alwan_scalar v = L_hdr - ALWAN_LITERAL(0.02);
    alwan_scalar denom = V_max - v;
    /* Avoid division by zero */
    alwan_scalar S = ALWAN_SELECT(ALWAN_ABS(denom) < ALWAN_EPSILON,
                                  K_m,
                                  (v * K_m) / denom);
    return alwan_spow_v(S, ALWAN_LITERAL(1.0) / epsilon);
}

/* ================================================================
 * PQ (ST2084) inverse EOTF -- C -> N
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_eotf_inverse_st2084_v(alwan_scalar C) {
    alwan_scalar const L_p = ALWAN_LITERAL(10000.0);
    alwan_scalar const c1 = ALWAN_LITERAL(0.8359375);
    alwan_scalar const c2 = ALWAN_LITERAL(18.8515625);
    alwan_scalar const c3 = ALWAN_LITERAL(18.6875);
    alwan_scalar const m1 = ALWAN_LITERAL(0.1593017578125);
    alwan_scalar const m2 = ALWAN_LITERAL(78.84375);

    alwan_scalar Y_p = ALWAN_POW(C / L_p, m1);
    alwan_scalar numerator = c1 + c2 * Y_p;
    alwan_scalar denominator = c3 * Y_p + ALWAN_LITERAL(1.0);
    return ALWAN_POW(numerator / denominator, m2);
}

/* ================================================================
 * PQ (ST2084) EOTF -- N -> C
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_eotf_st2084_v(alwan_scalar N) {
    alwan_scalar const L_p = ALWAN_LITERAL(10000.0);
    alwan_scalar const c1 = ALWAN_LITERAL(0.8359375);
    alwan_scalar const c2 = ALWAN_LITERAL(18.8515625);
    alwan_scalar const c3 = ALWAN_LITERAL(18.6875);
    alwan_scalar const m1 = ALWAN_LITERAL(0.1593017578125);
    alwan_scalar const m2 = ALWAN_LITERAL(78.84375);
    alwan_scalar const inv_m2 = ALWAN_LITERAL(1.0) / m2;
    alwan_scalar const inv_m1 = ALWAN_LITERAL(1.0) / m1;

    alwan_scalar N_p = ALWAN_POW(N, inv_m2);
    alwan_scalar numerator = N_p - c1;
    alwan_scalar denominator = c2 - c3 * N_p;

    /* Guard: clamp negative values */
    numerator = ALWAN_SELECT(numerator < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), numerator);
    alwan_scalar Y_p = ALWAN_SELECT(denominator <= ALWAN_LITERAL(0.0),
                                    ALWAN_LITERAL(0.0),
                                    numerator / denominator);
    return L_p * ALWAN_POW(Y_p, inv_m1);
}

/* ================================================================
 * Prismatic (Pridmore 2021)
 * ================================================================ */

ALWAN_INLINE alwan_prismatic alwan_rgb_to_prismatic_v(alwan_rgb rgb) {
    alwan_prismatic result;
    alwan_scalar L = alwan_max3(rgb.r, rgb.g, rgb.b);
    alwan_scalar sum_rgb = rgb.r + rgb.g + rgb.b;
    result.L = L;
    result.s = ALWAN_SELECT(sum_rgb < ALWAN_EPSILON, ALWAN_LITERAL(0.0), rgb.r / sum_rgb);
    result.h = ALWAN_SELECT(sum_rgb < ALWAN_EPSILON, ALWAN_LITERAL(0.0), rgb.g / sum_rgb);
    return result;
}

ALWAN_INLINE alwan_rgb alwan_prismatic_to_rgb_v(alwan_prismatic prismatic) {
    alwan_rgb result;
    alwan_scalar R_comp = ALWAN_LITERAL(1.0) - prismatic.s - prismatic.h;
    alwan_scalar max_pqr = alwan_max3(prismatic.s, prismatic.h, R_comp);
    alwan_scalar sum_rgb = ALWAN_SELECT(max_pqr < ALWAN_EPSILON,
                                        ALWAN_LITERAL(0.0),
                                        prismatic.L / max_pqr);
    result.r = prismatic.s * sum_rgb;
    result.g = prismatic.h * sum_rgb;
    result.b = R_comp * sum_rgb;
    return result;
}

/* ================================================================
 * hdr-CIELAB (Fairchild & Wyble 2010)
 * ================================================================ */

ALWAN_INLINE alwan_lab alwan_xyz_to_hdr_cielab_v(alwan_xyz xyz) {
    alwan_lab result;

    /* Default parameters: Y_s=0.2, Y_abs=100 */
    alwan_scalar epsilon = ALWAN_LITERAL(0.58);
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (ALWAN_LITERAL(0.2) / ALWAN_LITERAL(0.184));
    alwan_scalar lf = ALWAN_LN(ALWAN_LITERAL(318.0)) / ALWAN_LN(ALWAN_LITERAL(100.0));
    epsilon /= sf * lf;

    alwan_scalar xr = xyz.x / ALWAN_EXT_HDR_D65_WHITE[0];
    alwan_scalar yr = xyz.y / ALWAN_EXT_HDR_D65_WHITE[1];
    alwan_scalar zr = xyz.z / ALWAN_EXT_HDR_D65_WHITE[2];

    /* Use Fairchild2011 lightness with POW (not spow, since these are ratios) */
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);

    alwan_scalar yr_eps = ALWAN_POW(yr, epsilon);
    alwan_scalar L_hdr = (V_max * yr_eps) / (K_m + yr_eps) + ALWAN_LITERAL(0.02);
    alwan_scalar xr_eps = ALWAN_POW(xr, epsilon);
    alwan_scalar fx = (V_max * xr_eps) / (K_m + xr_eps) + ALWAN_LITERAL(0.02);
    alwan_scalar zr_eps = ALWAN_POW(zr, epsilon);
    alwan_scalar fz = (V_max * zr_eps) / (K_m + zr_eps) + ALWAN_LITERAL(0.02);

    result.L = L_hdr;
    result.a = ALWAN_LITERAL(5.0) * (fx - L_hdr);
    result.b = ALWAN_LITERAL(2.0) * (L_hdr - fz);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_hdr_cielab_to_xyz_v(alwan_lab hdr_lab) {
    alwan_xyz result;

    alwan_scalar epsilon = ALWAN_LITERAL(0.58);
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (ALWAN_LITERAL(0.2) / ALWAN_LITERAL(0.184));
    alwan_scalar lf = ALWAN_LN(ALWAN_LITERAL(318.0)) / ALWAN_LN(ALWAN_LITERAL(100.0));
    epsilon /= sf * lf;

    alwan_scalar yr = alwan_luminance_fairchild2011_v(hdr_lab.L, epsilon);
    alwan_scalar xr = alwan_luminance_fairchild2011_v(
        (hdr_lab.a + ALWAN_LITERAL(5.0) * hdr_lab.L) / ALWAN_LITERAL(5.0), epsilon);
    alwan_scalar zr = alwan_luminance_fairchild2011_v(
        (-hdr_lab.b + ALWAN_LITERAL(2.0) * hdr_lab.L) / ALWAN_LITERAL(2.0), epsilon);

    result.x = xr * ALWAN_EXT_HDR_D65_WHITE[0];
    result.y = yr * ALWAN_EXT_HDR_D65_WHITE[1];
    result.z = zr * ALWAN_EXT_HDR_D65_WHITE[2];
    return result;
}

/* ================================================================
 * hdr-IPT (Fairchild 2011)
 * ================================================================ */

ALWAN_INLINE alwan_ipt alwan_xyz_to_hdr_ipt_v(alwan_xyz xyz) {
    alwan_ipt result;

    /* Epsilon formula: 0.59 / (sf * lf) */
    alwan_scalar lf = ALWAN_LN(ALWAN_LITERAL(318.0)) / ALWAN_LN(ALWAN_LITERAL(100.0));
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (ALWAN_LITERAL(0.2) / ALWAN_LITERAL(0.184));
    alwan_scalar epsilon = ALWAN_LITERAL(0.59) / (sf * lf);

    /* XYZ -> LMS via IPT matrix */
    alwan_vec3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    alwan_vec3 lms = alwan_mat3_mulv_v(ALWAN_EXT_XYZ_TO_LMS_IPT, vec_in);

    /* Apply sign(LMS) * |lightness(LMS, e)| */
    alwan_scalar sign0 = ALWAN_SELECT(lms.v[0] > ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
                          ALWAN_SELECT(lms.v[0] < ALWAN_LITERAL(0.0), ALWAN_LITERAL(-1.0), ALWAN_LITERAL(0.0)));
    alwan_scalar sign1 = ALWAN_SELECT(lms.v[1] > ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
                          ALWAN_SELECT(lms.v[1] < ALWAN_LITERAL(0.0), ALWAN_LITERAL(-1.0), ALWAN_LITERAL(0.0)));
    alwan_scalar sign2 = ALWAN_SELECT(lms.v[2] > ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
                          ALWAN_SELECT(lms.v[2] < ALWAN_LITERAL(0.0), ALWAN_LITERAL(-1.0), ALWAN_LITERAL(0.0)));

    lms.v[0] = sign0 * ALWAN_ABS(alwan_lightness_fairchild2011_v(lms.v[0], epsilon));
    lms.v[1] = sign1 * ALWAN_ABS(alwan_lightness_fairchild2011_v(lms.v[1], epsilon));
    lms.v[2] = sign2 * ALWAN_ABS(alwan_lightness_fairchild2011_v(lms.v[2], epsilon));

    /* LMS -> IPT */
    alwan_vec3 out = alwan_mat3_mulv_v(ALWAN_EXT_LMS_TO_IPT_HDR, lms);
    result.I = out.v[0]; result.P = out.v[1]; result.T = out.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_hdr_ipt_to_xyz_v(alwan_ipt hdr_ipt) {
    alwan_xyz result;

    alwan_scalar lf = ALWAN_LN(ALWAN_LITERAL(318.0)) / ALWAN_LN(ALWAN_LITERAL(100.0));
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (ALWAN_LITERAL(0.2) / ALWAN_LITERAL(0.184));
    alwan_scalar epsilon = ALWAN_LITERAL(0.59) / (sf * lf);

    /* IPT -> LMS */
    alwan_vec3 vec_in;
    vec_in.v[0] = hdr_ipt.I; vec_in.v[1] = hdr_ipt.P; vec_in.v[2] = hdr_ipt.T;
    alwan_vec3 lms = alwan_mat3_mulv_v(ALWAN_EXT_IPT_TO_LMS_HDR, vec_in);

    /* Apply sign(LMS) * |luminance(LMS, e)| */
    alwan_scalar sign0 = ALWAN_SELECT(lms.v[0] > ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
                          ALWAN_SELECT(lms.v[0] < ALWAN_LITERAL(0.0), ALWAN_LITERAL(-1.0), ALWAN_LITERAL(0.0)));
    alwan_scalar sign1 = ALWAN_SELECT(lms.v[1] > ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
                          ALWAN_SELECT(lms.v[1] < ALWAN_LITERAL(0.0), ALWAN_LITERAL(-1.0), ALWAN_LITERAL(0.0)));
    alwan_scalar sign2 = ALWAN_SELECT(lms.v[2] > ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0),
                          ALWAN_SELECT(lms.v[2] < ALWAN_LITERAL(0.0), ALWAN_LITERAL(-1.0), ALWAN_LITERAL(0.0)));

    lms.v[0] = sign0 * ALWAN_ABS(alwan_luminance_fairchild2011_v(lms.v[0], epsilon));
    lms.v[1] = sign1 * ALWAN_ABS(alwan_luminance_fairchild2011_v(lms.v[1], epsilon));
    lms.v[2] = sign2 * ALWAN_ABS(alwan_luminance_fairchild2011_v(lms.v[2], epsilon));

    /* LMS -> XYZ */
    alwan_vec3 out = alwan_mat3_mulv_v(ALWAN_EXT_LMS_TO_XYZ_IPT, lms);
    result.x = out.v[0]; result.y = out.v[1]; result.z = out.v[2];
    return result;
}

/* ================================================================
 * IgPgTg (Ebner & Fairchild 1998)
 * ================================================================ */

ALWAN_INLINE alwan_igpgtg alwan_xyz_to_igpgtg_v(alwan_xyz xyz) {
    alwan_igpgtg result;

    /* XYZ -> LMS */
    alwan_vec3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    alwan_vec3 lms = alwan_mat3_mulv_v(ALWAN_EXT_XYZ_TO_LMS_IGPGTG, vec_in);

    /* Scaled nonlinearity: spow(LMS / scale, 0.427) */
    alwan_scalar const exponent = ALWAN_LITERAL(0.427);
    lms.v[0] = alwan_spow_v(lms.v[0] / ALWAN_EXT_IGPGTG_LMS_SCALE[0], exponent);
    lms.v[1] = alwan_spow_v(lms.v[1] / ALWAN_EXT_IGPGTG_LMS_SCALE[1], exponent);
    lms.v[2] = alwan_spow_v(lms.v[2] / ALWAN_EXT_IGPGTG_LMS_SCALE[2], exponent);

    /* LMS -> IgPgTg */
    alwan_vec3 out = alwan_mat3_mulv_v(ALWAN_EXT_LMS_TO_IGPGTG, lms);
    result.Ig = out.v[0]; result.Pg = out.v[1]; result.Tg = out.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_igpgtg_to_xyz_v(alwan_igpgtg igpgtg) {
    alwan_xyz result;

    /* IgPgTg -> LMS */
    alwan_vec3 vec_in;
    vec_in.v[0] = igpgtg.Ig; vec_in.v[1] = igpgtg.Pg; vec_in.v[2] = igpgtg.Tg;
    alwan_vec3 lms = alwan_mat3_mulv_v(ALWAN_EXT_IGPGTG_TO_LMS, vec_in);

    /* Inverse scaled nonlinearity: scale * spow(LMS, 1/0.427) */
    alwan_scalar const inv_exponent = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.427);
    lms.v[0] = ALWAN_EXT_IGPGTG_LMS_SCALE[0] * alwan_spow_v(lms.v[0], inv_exponent);
    lms.v[1] = ALWAN_EXT_IGPGTG_LMS_SCALE[1] * alwan_spow_v(lms.v[1], inv_exponent);
    lms.v[2] = ALWAN_EXT_IGPGTG_LMS_SCALE[2] * alwan_spow_v(lms.v[2], inv_exponent);

    /* LMS -> XYZ */
    alwan_vec3 out = alwan_mat3_mulv_v(ALWAN_EXT_LMS_TO_XYZ_IGPGTG, lms);
    result.x = out.v[0]; result.y = out.v[1]; result.z = out.v[2];
    return result;
}

/* ================================================================
 * ICaCb (Zhang & Wandell 1996, 1997)
 * ================================================================ */

ALWAN_INLINE alwan_icacb alwan_xyz_to_icacb_v(alwan_xyz xyz) {
    alwan_icacb result;

    /* XYZ -> LMS */
    alwan_vec3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    alwan_vec3 lms = alwan_mat3_mulv_v(ALWAN_EXT_XYZ_TO_LMS_ICACB, vec_in);

    /* PQ inverse EOTF */
    lms.v[0] = alwan_eotf_inverse_st2084_v(lms.v[0]);
    lms.v[1] = alwan_eotf_inverse_st2084_v(lms.v[1]);
    lms.v[2] = alwan_eotf_inverse_st2084_v(lms.v[2]);

    /* LMS -> ICaCb */
    alwan_vec3 out = alwan_mat3_mulv_v(ALWAN_EXT_LMS_TO_ICACB, lms);
    result.I = out.v[0]; result.Ca = out.v[1]; result.Cb = out.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_icacb_to_xyz_v(alwan_icacb icacb) {
    alwan_xyz result;

    /* ICaCb -> LMS */
    alwan_vec3 vec_in;
    vec_in.v[0] = icacb.I; vec_in.v[1] = icacb.Ca; vec_in.v[2] = icacb.Cb;
    alwan_vec3 lms = alwan_mat3_mulv_v(ALWAN_EXT_ICACB_TO_LMS, vec_in);

    /* PQ EOTF */
    lms.v[0] = alwan_eotf_st2084_v(lms.v[0]);
    lms.v[1] = alwan_eotf_st2084_v(lms.v[1]);
    lms.v[2] = alwan_eotf_st2084_v(lms.v[2]);

    /* LMS -> XYZ */
    alwan_vec3 out = alwan_mat3_mulv_v(ALWAN_EXT_LMS_TO_XYZ_ICACB, lms);
    result.x = out.v[0]; result.y = out.v[1]; result.z = out.v[2];
    return result;
}

/* ================================================================
 * HCL (Sarifuddin 2005)
 * ================================================================ */

ALWAN_INLINE alwan_hcl alwan_rgb_to_hcl_v(alwan_rgb rgb) {
    alwan_hcl result;

    alwan_scalar const gamma = ALWAN_LITERAL(3.0);
    alwan_scalar const Y_0 = ALWAN_LITERAL(100.0);

    alwan_scalar max_val = alwan_max3(rgb.r, rgb.g, rgb.b);
    alwan_scalar min_val = alwan_min3(rgb.r, rgb.g, rgb.b);

    /* Q factor */
    alwan_scalar Q = ALWAN_SELECT(max_val > ALWAN_EPSILON,
                                  ALWAN_EXP((min_val * gamma) / (max_val * Y_0)),
                                  ALWAN_LITERAL(1.0));

    /* Luminance */
    alwan_scalar L = (Q * max_val + (Q - ALWAN_LITERAL(1.0)) * min_val) / ALWAN_LITERAL(2.0);

    /* Chroma */
    alwan_scalar r_g = rgb.r - rgb.g;
    alwan_scalar g_b = rgb.g - rgb.b;
    alwan_scalar b_r = rgb.b - rgb.r;
    alwan_scalar C = Q * (ALWAN_ABS(r_g) + ALWAN_ABS(g_b) + ALWAN_ABS(b_r)) / ALWAN_LITERAL(3.0);

    /* Hue with 4-way sector branching via nested ALWAN_SELECT */
    /* When r_g ~= 0 but g_b != 0, atan(g_b/r_g) = +-pi/2 */
    alwan_scalar h_temp = ALWAN_SELECT(ALWAN_ABS(r_g) < ALWAN_EPSILON,
                                       ALWAN_SELECT(g_b >= ALWAN_LITERAL(0.0),
                                                    ALWAN_PI / ALWAN_LITERAL(2.0),
                                                    -ALWAN_PI / ALWAN_LITERAL(2.0)),
                                       ALWAN_ATAN(g_b / r_g));
    alwan_scalar two_h_3 = ALWAN_LITERAL(2.0) * h_temp / ALWAN_LITERAL(3.0);
    alwan_scalar four_h_3 = ALWAN_LITERAL(4.0) * h_temp / ALWAN_LITERAL(3.0);

    /* H based on signs of r_g and g_b */
    alwan_scalar H_rg_pos_gb_pos = two_h_3;
    alwan_scalar H_rg_pos_gb_neg = four_h_3;
    alwan_scalar H_rg_neg_gb_pos = ALWAN_PI + four_h_3;
    alwan_scalar H_rg_neg_gb_neg = two_h_3 - ALWAN_PI;

    alwan_scalar H_rg_pos = ALWAN_SELECT(g_b >= ALWAN_LITERAL(0.0), H_rg_pos_gb_pos, H_rg_pos_gb_neg);
    alwan_scalar H_rg_neg = ALWAN_SELECT(g_b >= ALWAN_LITERAL(0.0), H_rg_neg_gb_pos, H_rg_neg_gb_neg);
    alwan_scalar H = ALWAN_SELECT(C > ALWAN_EPSILON,
                                  ALWAN_SELECT(r_g >= ALWAN_LITERAL(0.0), H_rg_pos, H_rg_neg),
                                  ALWAN_LITERAL(0.0));

    result.H = H;
    result.C = C;
    result.L = L;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_hcl_to_rgb_v(alwan_hcl hcl) {
    alwan_rgb result;

    alwan_scalar H = hcl.H;
    alwan_scalar C = hcl.C;
    alwan_scalar L = hcl.L;

    alwan_scalar const gamma = ALWAN_LITERAL(3.0);
    alwan_scalar const Y_0 = ALWAN_LITERAL(100.0);

    /* Compute Q, Min, Max */
    alwan_scalar Q = ALWAN_SELECT(L > ALWAN_EPSILON,
        ALWAN_EXP((ALWAN_LITERAL(1.0) - (ALWAN_LITERAL(3.0) * C) / (ALWAN_LITERAL(4.0) * L)) * gamma / Y_0),
        ALWAN_LITERAL(1.0));
    alwan_scalar denom = ALWAN_LITERAL(4.0) * Q - ALWAN_LITERAL(2.0);
    alwan_scalar Min = ALWAN_SELECT(L > ALWAN_EPSILON,
        ALWAN_SELECT(ALWAN_ABS(denom) > ALWAN_EPSILON,
            (ALWAN_LITERAL(4.0) * L - ALWAN_LITERAL(3.0) * C) / denom,
            ALWAN_LITERAL(0.0)),
        ALWAN_LITERAL(0.0));
    alwan_scalar Max = ALWAN_SELECT(L > ALWAN_EPSILON,
        ALWAN_SELECT(Q > ALWAN_EPSILON,
            Min + (ALWAN_LITERAL(3.0) * C) / (ALWAN_LITERAL(2.0) * Q),
            ALWAN_LITERAL(0.0)),
        ALWAN_LITERAL(0.0));

    /* Sector boundaries */
    alwan_scalar const r_p60 = ALWAN_PI / ALWAN_LITERAL(3.0);
    alwan_scalar const r_p120 = ALWAN_LITERAL(2.0) * ALWAN_PI / ALWAN_LITERAL(3.0);
    alwan_scalar const r_n60 = -ALWAN_PI / ALWAN_LITERAL(3.0);
    alwan_scalar const r_n120 = -ALWAN_LITERAL(2.0) * ALWAN_PI / ALWAN_LITERAL(3.0);

    /* 6-way sector branching via nested ALWAN_SELECT
     * Compute all sector outputs, then select */

    /* Sector 0: [0, 60) */
    alwan_scalar tan0 = ALWAN_TAN(ALWAN_LITERAL(3.0) * H / ALWAN_LITERAL(2.0));
    alwan_scalar r0 = Max;
    alwan_scalar g0 = (Max * tan0 + Min) / (ALWAN_LITERAL(1.0) + tan0);
    alwan_scalar b0 = Min;

    /* Sector 1: [60, 120) */
    alwan_scalar tan1 = ALWAN_TAN(ALWAN_LITERAL(3.0) * (H - ALWAN_PI) / ALWAN_LITERAL(4.0));
    alwan_scalar r1 = ALWAN_SELECT(ALWAN_ABS(tan1) > ALWAN_EPSILON,
        (Max * (ALWAN_LITERAL(1.0) + tan1) - Min) / tan1, Max);
    alwan_scalar g1 = Max;
    alwan_scalar b1 = Min;

    /* Sector 2: [120, 180] */
    alwan_scalar tan2 = ALWAN_TAN(ALWAN_LITERAL(3.0) * (H - ALWAN_PI) / ALWAN_LITERAL(4.0));
    alwan_scalar r2 = Min;
    alwan_scalar g2 = Max;
    alwan_scalar b2 = Max * (ALWAN_LITERAL(1.0) + tan2) - Min * tan2;

    /* Sector 3: [-60, 0) */
    alwan_scalar tan3 = ALWAN_TAN(ALWAN_LITERAL(3.0) * H / ALWAN_LITERAL(4.0));
    alwan_scalar r3 = Max;
    alwan_scalar g3 = Min;
    alwan_scalar b3 = Min * (ALWAN_LITERAL(1.0) + tan3) - Max * tan3;

    /* Sector 4: [-120, -60) */
    alwan_scalar tan4 = ALWAN_TAN(ALWAN_LITERAL(3.0) * H / ALWAN_LITERAL(4.0));
    alwan_scalar r4 = ALWAN_SELECT(ALWAN_ABS(tan4) > ALWAN_EPSILON,
        (Min * (ALWAN_LITERAL(1.0) + tan4) - Max) / tan4, Min);
    alwan_scalar g4 = Min;
    alwan_scalar b4 = Max;

    /* Sector 5: [-180, -120) */
    alwan_scalar tan5 = ALWAN_TAN(ALWAN_LITERAL(3.0) * (H + ALWAN_PI) / ALWAN_LITERAL(2.0));
    alwan_scalar r5 = Min;
    alwan_scalar g5 = (Min * tan5 + Max) / (ALWAN_LITERAL(1.0) + tan5);
    alwan_scalar b5 = Max;

    /* Select sector via nested ALWAN_SELECT */
    /* Innermost first: sector 4 vs 5 */
    alwan_scalar r_45 = ALWAN_SELECT(H >= r_n120, r4, r5);
    alwan_scalar g_45 = ALWAN_SELECT(H >= r_n120, g4, g5);
    alwan_scalar b_45 = ALWAN_SELECT(H >= r_n120, b4, b5);

    /* sector 3 vs (4|5) */
    alwan_scalar r_345 = ALWAN_SELECT(H >= r_n60, r3, r_45);
    alwan_scalar g_345 = ALWAN_SELECT(H >= r_n60, g3, g_45);
    alwan_scalar b_345 = ALWAN_SELECT(H >= r_n60, b3, b_45);

    /* negative hue: sector (3|4|5) */
    /* sector 1 vs 2 */
    alwan_scalar r_12 = ALWAN_SELECT(H < r_p120, r1, r2);
    alwan_scalar g_12 = ALWAN_SELECT(H < r_p120, g1, g2);
    alwan_scalar b_12 = ALWAN_SELECT(H < r_p120, b1, b2);

    /* sector 0 vs (1|2) */
    alwan_scalar r_012 = ALWAN_SELECT(H < r_p60, r0, r_12);
    alwan_scalar g_012 = ALWAN_SELECT(H < r_p60, g0, g_12);
    alwan_scalar b_012 = ALWAN_SELECT(H < r_p60, b0, b_12);

    /* positive vs negative */
    result.r = ALWAN_SELECT(H >= ALWAN_LITERAL(0.0), r_012, r_345);
    result.g = ALWAN_SELECT(H >= ALWAN_LITERAL(0.0), g_012, g_345);
    result.b = ALWAN_SELECT(H >= ALWAN_LITERAL(0.0), b_012, b_345);

    return result;
}

/* ================================================================
 * IHLS (Improved HLS - Hanbury 2003)
 * ================================================================ */

ALWAN_INLINE alwan_ihls alwan_rgb_to_ihls_v(alwan_rgb rgb) {
    alwan_ihls result;

    alwan_scalar max_val = alwan_max3(rgb.r, rgb.g, rgb.b);
    alwan_scalar min_val = alwan_min3(rgb.r, rgb.g, rgb.b);
    alwan_scalar delta = max_val - min_val;

    /* Y, C1, C2 via matrix multiply */
    alwan_vec3 in_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 out_v = alwan_mat3_mulv_v(ALWAN_EXT_IHLS_RGB_TO_YC1C2, in_v);
    alwan_scalar Y  = out_v.v[0];
    alwan_scalar C_1 = out_v.v[1];
    alwan_scalar C_2 = out_v.v[2];

    alwan_scalar C_mag = ALWAN_SQRT(C_1 * C_1 + C_2 * C_2);

    /* Hue via ACOS with clamp */
    alwan_scalar C_1_C = ALWAN_SELECT(C_mag > ALWAN_EPSILON, C_1 / C_mag, ALWAN_LITERAL(1.0));
    C_1_C = ALWAN_SELECT(C_1_C > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0),
             ALWAN_SELECT(C_1_C < ALWAN_LITERAL(-1.0), ALWAN_LITERAL(-1.0), C_1_C));
    alwan_scalar H_temp = ALWAN_ACOS(C_1_C);

    /* Adjust based on C_2 sign */
    alwan_scalar H = ALWAN_SELECT(C_mag > ALWAN_EPSILON,
                      ALWAN_SELECT(C_2 <= ALWAN_LITERAL(0.0), H_temp, ALWAN_LITERAL(2.0) * ALWAN_PI - H_temp),
                      ALWAN_LITERAL(0.0));

    result.H = H;
    result.L = Y;
    result.S = delta;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_ihls_to_rgb_v(alwan_ihls ihls) {
    alwan_rgb result;

    alwan_scalar H = ihls.H;
    alwan_scalar Y = ihls.L;
    alwan_scalar S = ihls.S;

    alwan_scalar const pi_3 = ALWAN_PI / ALWAN_LITERAL(3.0);
    alwan_scalar const two_pi_3 = ALWAN_LITERAL(2.0) * ALWAN_PI / ALWAN_LITERAL(3.0);

    alwan_scalar k = ALWAN_FLOOR(H / pi_3);
    alwan_scalar H_s = H - k * pi_3;
    alwan_scalar sin_val = ALWAN_SIN(two_pi_3 - H_s);
    alwan_scalar C_mag = ALWAN_SELECT(ALWAN_ABS(sin_val) < ALWAN_EPSILON,
                                      ALWAN_LITERAL(0.0),
                                      (ALWAN_SQRT(ALWAN_LITERAL(3.0)) * S) / (ALWAN_LITERAL(2.0) * sin_val));

    alwan_scalar C_1 = C_mag * ALWAN_COS(H);
    alwan_scalar C_2 = -C_mag * ALWAN_SIN(H);

    alwan_vec3 in_v = {{Y, C_1, C_2}};
    alwan_vec3 out_v = alwan_mat3_mulv_v(ALWAN_EXT_IHLS_YC1C2_TO_RGB, in_v);
    result.r = out_v.v[0];
    result.g = out_v.v[1];
    result.b = out_v.v[2];

    return result;
}

/* ================================================================
 * HLC — Cylindrical CIELAB with (H, L, C) ordering
 *
 * Identical to CIE LCH(ab) mathematically, with component order
 * rearranged to (Hue, Lightness, Chroma). Used by some color
 * frameworks where hue is the primary selector.
 *
 * References:
 *   CIE 015:2004, Colorimetry (3rd edition)
 * ================================================================ */

ALWAN_INLINE alwan_hlc alwan_lch_to_hlc_v(alwan_lch lch) {
    alwan_hlc result;
    result.H = lch.h;
    result.L = lch.L;
    result.C = lch.C;
    return result;
}

ALWAN_INLINE alwan_lch alwan_hlc_to_lch_v(alwan_hlc hlc) {
    alwan_lch result;
    result.L = hlc.L;
    result.C = hlc.C;
    result.h = hlc.H;
    return result;
}

/* ================================================================
 * Cubehelix Color Space (Green 2011)
 *
 * A helical color scheme with monotonically increasing perceived
 * brightness. The helix traces a path through RGB space such that
 * the luminance (using Rec.601 weights) increases linearly.
 *
 * Forward: Cubehelix(h, s, l) -> sRGB
 *   angle = (h + 120) * pi/180
 *   amp = s * l * (1 - l)
 *   R = l + amp * (A*cos + B*sin)
 *   G = l + amp * (C*cos + D*sin)
 *   B = l + amp * (E*cos)
 *
 * The coefficients satisfy: 0.299*R + 0.587*G + 0.114*B = l
 * (luminance-preserving constraint, Rec.601 weights).
 *
 * References:
 *   Paper:  Green, D. A., 2011, BASI, 39, 289
 *   Source: https://people.phy.cam.ac.uk/dag9/CUBEHELIX/
 *   D3:     https://github.com/d3/d3-color (cubehelix.js)
 * ================================================================ */

/* Cubehelix rotation matrix coefficients (Rec.601 luminance-neutral) */
#define ALWAN_CUBEHELIX_A  ALWAN_LITERAL(-0.14861)
#define ALWAN_CUBEHELIX_B  ALWAN_LITERAL( 1.78277)
#define ALWAN_CUBEHELIX_C  ALWAN_LITERAL(-0.29227)
#define ALWAN_CUBEHELIX_D  ALWAN_LITERAL(-0.90649)
#define ALWAN_CUBEHELIX_E  ALWAN_LITERAL( 1.97294)

ALWAN_INLINE alwan_rgb alwan_cubehelix_to_rgb_v(alwan_cubehelix ch) {
    alwan_rgb result;
    alwan_scalar angle = (ch.h + ALWAN_LITERAL(120.0)) * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar amp = ch.s * ch.l * (ALWAN_ONE - ch.l);
    alwan_scalar cos_a = ALWAN_COS(angle);
    alwan_scalar sin_a = ALWAN_SIN(angle);

    result.r = ch.l + amp * (ALWAN_CUBEHELIX_A * cos_a + ALWAN_CUBEHELIX_B * sin_a);
    result.g = ch.l + amp * (ALWAN_CUBEHELIX_C * cos_a + ALWAN_CUBEHELIX_D * sin_a);
    result.b = ch.l + amp * (ALWAN_CUBEHELIX_E * cos_a);
    return result;
}

ALWAN_INLINE alwan_cubehelix alwan_rgb_to_cubehelix_v(alwan_rgb rgb) {
    alwan_cubehelix result;

    /* Derived inverse constants */
    alwan_scalar const ED    = ALWAN_CUBEHELIX_E * ALWAN_CUBEHELIX_D;
    alwan_scalar const EB    = ALWAN_CUBEHELIX_E * ALWAN_CUBEHELIX_B;
    alwan_scalar const BC_DA = ALWAN_CUBEHELIX_B * ALWAN_CUBEHELIX_C
                             - ALWAN_CUBEHELIX_D * ALWAN_CUBEHELIX_A;

    /* Recover lightness (luminance under Rec.601 via the matrix inverse) */
    result.l = (BC_DA * rgb.b + ED * rgb.r - EB * rgb.g)
             / (BC_DA + ED - EB);

    /* Recover cosine and sine projections */
    alwan_scalar bl = rgb.b - result.l;
    alwan_scalar k  = (ALWAN_CUBEHELIX_E * (rgb.g - result.l)
                      - ALWAN_CUBEHELIX_C * bl) / ALWAN_CUBEHELIX_D;

    /* Saturation and hue */
    alwan_scalar chroma = ALWAN_SQRT(k * k + bl * bl);
    alwan_scalar denom  = ALWAN_CUBEHELIX_E * result.l * (ALWAN_ONE - result.l);

    result.s = ALWAN_SELECT(denom > ALWAN_LITERAL(1e-10),
                            chroma / denom, ALWAN_ZERO);

    result.h = ALWAN_ATAN2(k, bl) * ALWAN_LITERAL(180.0) / ALWAN_PI
             - ALWAN_LITERAL(120.0);
    result.h = ALWAN_SELECT(result.h < ALWAN_ZERO,
                            result.h + ALWAN_LITERAL(360.0), result.h);

    return result;
}

/* ================================================================
 * HSLuv / HPLuv — Human-friendly HSL via CIE LCHuv
 *
 * HSLuv maps saturation [0-100] to the percentage of maximum
 * achievable chroma at a given (L, H) within the sRGB gamut.
 * HPLuv uses the minimum chroma across ALL hues, guaranteeing
 * every (H, S, L) triple is in-gamut (pastel colors only).
 *
 * Conversion chain:
 *   HSLuv(H,S,L) -> LCH(uv) -> LUV -> XYZ -> linear sRGB
 *
 * References:
 *   Website: https://www.hsluv.org/
 *   C impl:  https://github.com/hsluv/hsluv-c (MIT, Boronine/Mitas)
 *   Math:    Algebraic sRGB gamut boundary in LCHuv via CAS (Maxima)
 * ================================================================ */

/* sRGB D65 XYZ-to-linear-RGB matrix (IEC 61966-2-1) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const ALWAN_HSLUV_M[3][3] = {
    { 3.24096994190452134377, -1.53738317757009345794, -0.49861076029300328366},
    {-0.96924363628087982613,  1.87596750150772066772,  0.04155505740717561247},
    { 0.05563007969699360846, -0.20397695888897656435,  1.05697151424287856072}
};
/* sRGB D65 linear-RGB-to-XYZ matrix */
static alwan_scalar const ALWAN_HSLUV_M_INV[3][3] = {
    { 0.41239079926595948129,  0.35758433938387796373,  0.18048078840183428751},
    { 0.21263900587151035754,  0.71516867876775592746,  0.07219231536073371500},
    { 0.01933081871559185069,  0.11919477979462598791,  0.95053215224966058086}
};
ALWAN_DIAG_POP

/* D65 reference white chromaticity in CIE 1976 UCS */
#define ALWAN_HSLUV_REF_U  ALWAN_LITERAL(0.19783000664283680764)
#define ALWAN_HSLUV_REF_V  ALWAN_LITERAL(0.46831999493879100370)
#define ALWAN_HSLUV_KAPPA  ALWAN_LITERAL(903.29629629629629629630)
#define ALWAN_HSLUV_EPS    ALWAN_LITERAL(0.00885645167903563082)

/* L* -> Y */
ALWAN_INLINE alwan_scalar alwan_hsluv_l2y(alwan_scalar L) {
    return ALWAN_SELECT(L <= ALWAN_LITERAL(8.0),
        L / ALWAN_HSLUV_KAPPA,
        ALWAN_LITERAL(1.0));  /* placeholder — computed below for L > 8 */
}

/* Y -> L* */
ALWAN_INLINE alwan_scalar alwan_hsluv_y2l(alwan_scalar Y) {
    return ALWAN_SELECT(Y <= ALWAN_HSLUV_EPS,
        Y * ALWAN_HSLUV_KAPPA,
        ALWAN_LITERAL(116.0) * ALWAN_CBRT(Y) - ALWAN_LITERAL(16.0));
}

/* Gamut bounding line: chroma = slope * hue_cos + intercept / hue_sin
 * Stored as (slope, intercept) pairs — 6 lines total (2 per RGB channel) */
typedef struct { alwan_scalar a, b; } alwan_hsluv_bound;

ALWAN_INLINE void alwan_hsluv_get_bounds(alwan_scalar L, alwan_hsluv_bound bounds[6]) {
    alwan_scalar tl = L + ALWAN_LITERAL(16.0);
    alwan_scalar sub1 = (tl * tl * tl) / ALWAN_LITERAL(1560896.0);
    alwan_scalar sub2 = ALWAN_SELECT(sub1 > ALWAN_HSLUV_EPS, sub1,
                                      L / ALWAN_HSLUV_KAPPA);
    int ch, t;
    for (ch = 0; ch < 3; ch++) {
        alwan_scalar m1 = ALWAN_HSLUV_M[ch][0];
        alwan_scalar m2 = ALWAN_HSLUV_M[ch][1];
        alwan_scalar m3 = ALWAN_HSLUV_M[ch][2];
        for (t = 0; t < 2; t++) {
            alwan_scalar top1 = (ALWAN_LITERAL(284517.0) * m1
                               - ALWAN_LITERAL( 94839.0) * m3) * sub2;
            alwan_scalar top2 = (ALWAN_LITERAL(838422.0) * m3
                               + ALWAN_LITERAL(769860.0) * m2
                               + ALWAN_LITERAL(731718.0) * m1) * L * sub2
                               - ALWAN_LITERAL(769860.0) * (alwan_scalar)t * L;
            alwan_scalar bottom = (ALWAN_LITERAL(632260.0) * m3
                                 - ALWAN_LITERAL(126452.0) * m2) * sub2
                                 + ALWAN_LITERAL(126452.0) * (alwan_scalar)t;
            bounds[ch * 2 + t].a = top1 / bottom;
            bounds[ch * 2 + t].b = top2 / bottom;
        }
    }
}

/* Maximum chroma for a given (L, H) in sRGB gamut */
ALWAN_INLINE alwan_scalar alwan_hsluv_max_chroma_lh(alwan_scalar L, alwan_scalar H) {
    alwan_scalar hrad = H * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar sin_h = ALWAN_SIN(hrad);
    alwan_scalar cos_h = ALWAN_COS(hrad);
    alwan_scalar min_len = ALWAN_LITERAL(1e30);
    alwan_hsluv_bound bounds[6];
    int i;
    alwan_hsluv_get_bounds(L, bounds);
    for (i = 0; i < 6; i++) {
        alwan_scalar denom = sin_h - bounds[i].a * cos_h;
        alwan_scalar len = bounds[i].b / denom;
        if (len >= ALWAN_ZERO && len < min_len) min_len = len;
    }
    return min_len;
}

/* Minimum chroma across all hues for a given L (HPLuv) */
ALWAN_INLINE alwan_scalar alwan_hsluv_max_safe_chroma(alwan_scalar L) {
    alwan_scalar min_dist_sq = ALWAN_LITERAL(1e30);
    alwan_hsluv_bound bounds[6];
    int i;
    alwan_hsluv_get_bounds(L, bounds);
    for (i = 0; i < 6; i++) {
        alwan_scalar m1 = bounds[i].a;
        alwan_scalar b1 = bounds[i].b;
        /* Closest point on line y=m1*x+b1 to origin: perpendicular intersection */
        alwan_scalar x = b1 / (-ALWAN_ONE / m1 - m1);
        alwan_scalar dist = x * x + (b1 + x * m1) * (b1 + x * m1);
        if (dist < min_dist_sq) min_dist_sq = dist;
    }
    return ALWAN_SQRT(min_dist_sq);
}

/* Internal: LCHuv -> linear sRGB (no clamping) */
ALWAN_INLINE alwan_rgb alwan_hsluv_lchuv_to_rgb(alwan_scalar L, alwan_scalar C, alwan_scalar H) {
    alwan_rgb result;
    alwan_scalar h_rad, u, v, var_u, var_v, Y, X, Z;

    /* LCH -> Luv */
    h_rad = H * ALWAN_PI / ALWAN_LITERAL(180.0);
    u = C * ALWAN_COS(h_rad);
    v = C * ALWAN_SIN(h_rad);

    /* Luv -> XYZ */
    if (L < ALWAN_LITERAL(1e-8)) {
        result.r = ALWAN_ZERO; result.g = ALWAN_ZERO; result.b = ALWAN_ZERO;
        return result;
    }
    var_u = u / (ALWAN_LITERAL(13.0) * L) + ALWAN_HSLUV_REF_U;
    var_v = v / (ALWAN_LITERAL(13.0) * L) + ALWAN_HSLUV_REF_V;

    /* L -> Y */
    if (L <= ALWAN_LITERAL(8.0))
        Y = L / ALWAN_HSLUV_KAPPA;
    else {
        alwan_scalar fy = (L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
        Y = fy * fy * fy;
    }

    X = ALWAN_LITERAL(-9.0) * Y * var_u
      / ((var_u - ALWAN_LITERAL(4.0)) * var_v - var_u * var_v);
    Z = (ALWAN_LITERAL(9.0) * Y - ALWAN_LITERAL(15.0) * var_v * Y - var_v * X)
      / (ALWAN_LITERAL(3.0) * var_v);

    /* XYZ -> linear sRGB */
    result.r = ALWAN_HSLUV_M[0][0] * X + ALWAN_HSLUV_M[0][1] * Y + ALWAN_HSLUV_M[0][2] * Z;
    result.g = ALWAN_HSLUV_M[1][0] * X + ALWAN_HSLUV_M[1][1] * Y + ALWAN_HSLUV_M[1][2] * Z;
    result.b = ALWAN_HSLUV_M[2][0] * X + ALWAN_HSLUV_M[2][1] * Y + ALWAN_HSLUV_M[2][2] * Z;
    return result;
}

/* Internal: sRGB -> LCHuv */
ALWAN_INLINE void alwan_hsluv_rgb_to_lchuv(alwan_scalar *L_out, alwan_scalar *C_out,
                                             alwan_scalar *H_out, alwan_rgb rgb) {
    /* linear sRGB -> XYZ */
    alwan_scalar X = ALWAN_HSLUV_M_INV[0][0] * rgb.r + ALWAN_HSLUV_M_INV[0][1] * rgb.g + ALWAN_HSLUV_M_INV[0][2] * rgb.b;
    alwan_scalar Y = ALWAN_HSLUV_M_INV[1][0] * rgb.r + ALWAN_HSLUV_M_INV[1][1] * rgb.g + ALWAN_HSLUV_M_INV[1][2] * rgb.b;
    alwan_scalar Z = ALWAN_HSLUV_M_INV[2][0] * rgb.r + ALWAN_HSLUV_M_INV[2][1] * rgb.g + ALWAN_HSLUV_M_INV[2][2] * rgb.b;

    /* XYZ -> Luv */
    alwan_scalar denom = X + ALWAN_LITERAL(15.0) * Y + ALWAN_LITERAL(3.0) * Z;
    alwan_scalar u_prime, v_prime, L, u, v;

    if (denom < ALWAN_LITERAL(1e-10)) {
        *L_out = ALWAN_ZERO; *C_out = ALWAN_ZERO; *H_out = ALWAN_ZERO;
        return;
    }
    u_prime = ALWAN_LITERAL(4.0) * X / denom;
    v_prime = ALWAN_LITERAL(9.0) * Y / denom;

    L = alwan_hsluv_y2l(Y);
    u = ALWAN_LITERAL(13.0) * L * (u_prime - ALWAN_HSLUV_REF_U);
    v = ALWAN_LITERAL(13.0) * L * (v_prime - ALWAN_HSLUV_REF_V);

    *L_out = L;
    *C_out = ALWAN_SQRT(u * u + v * v);
    *H_out = ALWAN_ATAN2(v, u) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    if (*H_out < ALWAN_ZERO) *H_out += ALWAN_LITERAL(360.0);
}

/* HSLuv -> sRGB (encoded) */
ALWAN_INLINE alwan_rgb alwan_hsluv_to_srgb_v(alwan_hsluv hsluv) {
    alwan_scalar H = hsluv.h;
    alwan_scalar S = hsluv.s;
    alwan_scalar L = hsluv.l;
    alwan_scalar C;
    alwan_rgb linear;

    if (L > ALWAN_LITERAL(99.9999999)) {
        alwan_rgb result = {ALWAN_ONE, ALWAN_ONE, ALWAN_ONE};
        return result;
    }
    if (L < ALWAN_LITERAL(0.00000001)) {
        alwan_rgb result = {ALWAN_ZERO, ALWAN_ZERO, ALWAN_ZERO};
        return result;
    }

    C = alwan_hsluv_max_chroma_lh(L, H) / ALWAN_LITERAL(100.0) * S;
    linear = alwan_hsluv_lchuv_to_rgb(L, C, H);

    /* Apply sRGB OETF */
    {
        alwan_rgb result;
        result.r = alwan_srgb_oetf(linear.r);
        result.g = alwan_srgb_oetf(linear.g);
        result.b = alwan_srgb_oetf(linear.b);
        return result;
    }
}

/* sRGB (encoded) -> HSLuv */
ALWAN_INLINE alwan_hsluv alwan_srgb_to_hsluv_v(alwan_rgb srgb) {
    alwan_hsluv result;
    alwan_rgb linear;
    alwan_scalar L, C, H;

    /* Remove sRGB OETF */
    linear.r = alwan_srgb_eotf(srgb.r);
    linear.g = alwan_srgb_eotf(srgb.g);
    linear.b = alwan_srgb_eotf(srgb.b);

    alwan_hsluv_rgb_to_lchuv(&L, &C, &H, linear);

    if (L > ALWAN_LITERAL(99.9999999)) {
        result.h = H; result.s = ALWAN_ZERO; result.l = ALWAN_LITERAL(100.0);
        return result;
    }
    if (L < ALWAN_LITERAL(0.00000001)) {
        result.h = H; result.s = ALWAN_ZERO; result.l = ALWAN_ZERO;
        return result;
    }

    result.h = H;
    result.s = C / alwan_hsluv_max_chroma_lh(L, H) * ALWAN_LITERAL(100.0);
    result.l = L;
    return result;
}

/* HPLuv -> sRGB (encoded) */
ALWAN_INLINE alwan_rgb alwan_hpluv_to_srgb_v(alwan_hpluv hpluv) {
    alwan_scalar H = hpluv.h;
    alwan_scalar S = hpluv.s;
    alwan_scalar L = hpluv.l;
    alwan_scalar C;
    alwan_rgb linear;

    if (L > ALWAN_LITERAL(99.9999999)) {
        alwan_rgb result = {ALWAN_ONE, ALWAN_ONE, ALWAN_ONE};
        return result;
    }
    if (L < ALWAN_LITERAL(0.00000001)) {
        alwan_rgb result = {ALWAN_ZERO, ALWAN_ZERO, ALWAN_ZERO};
        return result;
    }

    C = alwan_hsluv_max_safe_chroma(L) / ALWAN_LITERAL(100.0) * S;
    linear = alwan_hsluv_lchuv_to_rgb(L, C, H);

    {
        alwan_rgb result;
        result.r = alwan_srgb_oetf(linear.r);
        result.g = alwan_srgb_oetf(linear.g);
        result.b = alwan_srgb_oetf(linear.b);
        return result;
    }
}

/* sRGB (encoded) -> HPLuv */
ALWAN_INLINE alwan_hpluv alwan_srgb_to_hpluv_v(alwan_rgb srgb) {
    alwan_hpluv result;
    alwan_rgb linear;
    alwan_scalar L, C, H;

    linear.r = alwan_srgb_eotf(srgb.r);
    linear.g = alwan_srgb_eotf(srgb.g);
    linear.b = alwan_srgb_eotf(srgb.b);

    alwan_hsluv_rgb_to_lchuv(&L, &C, &H, linear);

    if (L > ALWAN_LITERAL(99.9999999)) {
        result.h = H; result.s = ALWAN_ZERO; result.l = ALWAN_LITERAL(100.0);
        return result;
    }
    if (L < ALWAN_LITERAL(0.00000001)) {
        result.h = H; result.s = ALWAN_ZERO; result.l = ALWAN_ZERO;
        return result;
    }

    result.h = H;
    result.s = C / alwan_hsluv_max_safe_chroma(L) * ALWAN_LITERAL(100.0);
    result.l = L;
    return result;
}

/* ================================================================
 * Okhsl / Okhsv — Perceptual HSL/HSV in Oklab space
 *
 * These color spaces provide perceptually uniform hue, saturation,
 * and lightness/value controls by mapping through Oklab's gamut
 * boundary. Saturation is defined as a percentage of the maximum
 * achievable chroma at a given (hue, lightness/value) in sRGB.
 *
 * References:
 *   Blog:   https://bottosson.github.io/posts/colorpicker/
 *   Source: https://github.com/bottosson/bottosson.github.io
 *           misc/ok_color.h (MIT, Bjorn Ottosson 2021)
 * ================================================================ */

/* Oklab internal: linear sRGB -> Oklab */
ALWAN_INLINE void alwan_ok_srgb_to_lab(alwan_scalar *L, alwan_scalar *a, alwan_scalar *b,
                                         alwan_scalar r, alwan_scalar g, alwan_scalar bl_in) {
    alwan_scalar l_ = ALWAN_LITERAL(0.4122214708) * r + ALWAN_LITERAL(0.5363325363) * g + ALWAN_LITERAL(0.0514459929) * bl_in;
    alwan_scalar m_ = ALWAN_LITERAL(0.2119034982) * r + ALWAN_LITERAL(0.6806995451) * g + ALWAN_LITERAL(0.1073969566) * bl_in;
    alwan_scalar s_ = ALWAN_LITERAL(0.0883024619) * r + ALWAN_LITERAL(0.2817188376) * g + ALWAN_LITERAL(0.6299787005) * bl_in;
    alwan_scalar lp = ALWAN_CBRT(l_);
    alwan_scalar mp = ALWAN_CBRT(m_);
    alwan_scalar sp = ALWAN_CBRT(s_);
    *L = ALWAN_LITERAL(0.2104542553) * lp + ALWAN_LITERAL(0.7936177850) * mp - ALWAN_LITERAL(0.0040720468) * sp;
    *a = ALWAN_LITERAL(1.9779984951) * lp - ALWAN_LITERAL(2.4285922050) * mp + ALWAN_LITERAL(0.4505937099) * sp;
    *b = ALWAN_LITERAL(0.0259040371) * lp + ALWAN_LITERAL(0.7827717662) * mp - ALWAN_LITERAL(0.8086757660) * sp;
}

/* Oklab internal: Oklab -> linear sRGB */
ALWAN_INLINE void alwan_ok_lab_to_srgb(alwan_scalar *r, alwan_scalar *g, alwan_scalar *b,
                                         alwan_scalar L, alwan_scalar a, alwan_scalar bl_in) {
    alwan_scalar lp = L + ALWAN_LITERAL(0.3963377774) * a + ALWAN_LITERAL(0.2158037573) * bl_in;
    alwan_scalar mp = L - ALWAN_LITERAL(0.1055613458) * a - ALWAN_LITERAL(0.0638541728) * bl_in;
    alwan_scalar sp = L - ALWAN_LITERAL(0.0894841775) * a - ALWAN_LITERAL(1.2914855480) * bl_in;
    alwan_scalar l_ = lp * lp * lp;
    alwan_scalar m_ = mp * mp * mp;
    alwan_scalar s_ = sp * sp * sp;
    *r =  ALWAN_LITERAL(4.0767416621) * l_ - ALWAN_LITERAL(3.3077115913) * m_ + ALWAN_LITERAL(0.2309699292) * s_;
    *g = -ALWAN_LITERAL(1.2684380046) * l_ + ALWAN_LITERAL(2.6097574011) * m_ - ALWAN_LITERAL(0.3413193965) * s_;
    *b = -ALWAN_LITERAL(0.0041960863) * l_ - ALWAN_LITERAL(0.7034186147) * m_ + ALWAN_LITERAL(1.7076147010) * s_;
}

/* Toe function: softens the low-lightness region */
ALWAN_INLINE alwan_scalar alwan_ok_toe(alwan_scalar x) {
    alwan_scalar const k1 = ALWAN_LITERAL(0.206);
    alwan_scalar const k2 = ALWAN_LITERAL(0.03);
    alwan_scalar const k3 = (ALWAN_ONE + k1) / (ALWAN_ONE + k2);
    alwan_scalar t = k3 * x - k1;
    return ALWAN_LITERAL(0.5) * (t + ALWAN_SQRT(t * t
         + ALWAN_LITERAL(4.0) * k2 * k3 * x));
}

ALWAN_INLINE alwan_scalar alwan_ok_toe_inv(alwan_scalar x) {
    alwan_scalar const k1 = ALWAN_LITERAL(0.206);
    alwan_scalar const k2 = ALWAN_LITERAL(0.03);
    alwan_scalar const k3 = (ALWAN_ONE + k1) / (ALWAN_ONE + k2);
    return (x * x + k1 * x) / (k3 * (x + k2));
}

/* Maximum saturation for a unit-length (a_, b_) direction in Oklab */
ALWAN_INLINE alwan_scalar alwan_ok_compute_max_saturation(alwan_scalar a_, alwan_scalar b_) {
    alwan_scalar k0, k1, k2, k3, k4;
    alwan_scalar wl, wm, ws;

    if (-ALWAN_LITERAL(1.88170328) * a_ - ALWAN_LITERAL(0.80936493) * b_ > ALWAN_ONE) {
        k0 = ALWAN_LITERAL( 1.19086277); k1 = ALWAN_LITERAL( 1.76576728);
        k2 = ALWAN_LITERAL( 0.59662641); k3 = ALWAN_LITERAL( 0.75515197);
        k4 = ALWAN_LITERAL( 0.56771245);
        wl = ALWAN_LITERAL( 4.0767416621); wm = ALWAN_LITERAL(-3.3077115913);
        ws = ALWAN_LITERAL( 0.2309699292);
    } else if (ALWAN_LITERAL(1.81444104) * a_ - ALWAN_LITERAL(1.19445276) * b_ > ALWAN_ONE) {
        k0 = ALWAN_LITERAL( 0.73956515); k1 = ALWAN_LITERAL(-0.45954404);
        k2 = ALWAN_LITERAL( 0.08285427); k3 = ALWAN_LITERAL( 0.12541070);
        k4 = ALWAN_LITERAL( 0.14503204);
        wl = ALWAN_LITERAL(-1.2684380046); wm = ALWAN_LITERAL( 2.6097574011);
        ws = ALWAN_LITERAL(-0.3413193965);
    } else {
        k0 = ALWAN_LITERAL( 1.35733652); k1 = ALWAN_LITERAL(-0.00915799);
        k2 = ALWAN_LITERAL(-1.15130210); k3 = ALWAN_LITERAL(-0.50559606);
        k4 = ALWAN_LITERAL( 0.00692167);
        wl = ALWAN_LITERAL(-0.0041960863); wm = ALWAN_LITERAL(-0.7034186147);
        ws = ALWAN_LITERAL( 1.7076147010);
    }

    /* Initial polynomial approximation */
    alwan_scalar S = k0 + k1 * a_ + k2 * b_ + k3 * a_ * a_ + k4 * a_ * b_;

    /* One step of Halley's method */
    {
        alwan_scalar k_l = ALWAN_LITERAL( 0.3963377774) * a_ + ALWAN_LITERAL(0.2158037573) * b_;
        alwan_scalar k_m = ALWAN_LITERAL(-0.1055613458) * a_ - ALWAN_LITERAL(0.0638541728) * b_;
        alwan_scalar k_s = ALWAN_LITERAL(-0.0894841775) * a_ - ALWAN_LITERAL(1.2914855480) * b_;

        alwan_scalar l_ = ALWAN_ONE + S * k_l;
        alwan_scalar m_ = ALWAN_ONE + S * k_m;
        alwan_scalar s_ = ALWAN_ONE + S * k_s;
        alwan_scalar l = l_ * l_ * l_;
        alwan_scalar m = m_ * m_ * m_;
        alwan_scalar s = s_ * s_ * s_;

        alwan_scalar l_dS  = ALWAN_LITERAL(3.0) * k_l * l_ * l_;
        alwan_scalar m_dS  = ALWAN_LITERAL(3.0) * k_m * m_ * m_;
        alwan_scalar s_dS  = ALWAN_LITERAL(3.0) * k_s * s_ * s_;
        alwan_scalar l_dS2 = ALWAN_LITERAL(6.0) * k_l * k_l * l_;
        alwan_scalar m_dS2 = ALWAN_LITERAL(6.0) * k_m * k_m * m_;
        alwan_scalar s_dS2 = ALWAN_LITERAL(6.0) * k_s * k_s * s_;

        alwan_scalar f  = wl * l  + wm * m  + ws * s;
        alwan_scalar f1 = wl * l_dS  + wm * m_dS  + ws * s_dS;
        alwan_scalar f2 = wl * l_dS2 + wm * m_dS2 + ws * s_dS2;
        S = S - f * f1 / (f1 * f1 - ALWAN_LITERAL(0.5) * f * f2);
    }
    return S;
}

/* Find the gamut cusp (L, C) for a given hue direction */
ALWAN_INLINE void alwan_ok_find_cusp(alwan_scalar *L_cusp, alwan_scalar *C_cusp,
                                       alwan_scalar a_, alwan_scalar b_) {
    alwan_scalar S_cusp = alwan_ok_compute_max_saturation(a_, b_);
    alwan_scalar r, g, b;
    alwan_ok_lab_to_srgb(&r, &g, &b, ALWAN_ONE, S_cusp * a_, S_cusp * b_);
    alwan_scalar max_rgb = alwan_max3(r, g, b);
    *L_cusp = ALWAN_CBRT(ALWAN_ONE / max_rgb);
    *C_cusp = (*L_cusp) * S_cusp;
}

/* Find gamut intersection parameter t */
ALWAN_INLINE alwan_scalar alwan_ok_find_gamut_intersection(alwan_scalar a_, alwan_scalar b_,
                                                            alwan_scalar L1, alwan_scalar C1,
                                                            alwan_scalar L0,
                                                            alwan_scalar cusp_L, alwan_scalar cusp_C) {
    alwan_scalar t;
    if (((L1 - L0) * cusp_C - (cusp_L - L0) * C1) <= ALWAN_ZERO) {
        /* Lower half */
        t = cusp_C * L0 / (C1 * cusp_L + cusp_C * (L0 - L1));
    } else {
        /* Upper half — linear approximation + Halley refinement */
        t = cusp_C * (L0 - ALWAN_ONE)
          / (C1 * (cusp_L - ALWAN_ONE) + cusp_C * (L0 - L1));

        /* One Halley iteration */
        {
            alwan_scalar dL = L1 - L0;
            alwan_scalar dC = C1;
            alwan_scalar k_l = ALWAN_LITERAL( 0.3963377774) * a_ + ALWAN_LITERAL(0.2158037573) * b_;
            alwan_scalar k_m = ALWAN_LITERAL(-0.1055613458) * a_ - ALWAN_LITERAL(0.0638541728) * b_;
            alwan_scalar k_s = ALWAN_LITERAL(-0.0894841775) * a_ - ALWAN_LITERAL(1.2914855480) * b_;

            alwan_scalar l_dt = dL + dC * k_l;
            alwan_scalar m_dt = dL + dC * k_m;
            alwan_scalar s_dt = dL + dC * k_s;

            alwan_scalar L_t = L0 * (ALWAN_ONE - t) + t * L1;
            alwan_scalar C_t = t * C1;
            alwan_scalar l_ = L_t + C_t * k_l;
            alwan_scalar m_ = L_t + C_t * k_m;
            alwan_scalar s_ = L_t + C_t * k_s;
            alwan_scalar l = l_ * l_ * l_;
            alwan_scalar m = m_ * m_ * m_;
            alwan_scalar s = s_ * s_ * s_;
            alwan_scalar ldt = ALWAN_LITERAL(3.0) * l_dt * l_ * l_;
            alwan_scalar mdt = ALWAN_LITERAL(3.0) * m_dt * m_ * m_;
            alwan_scalar sdt = ALWAN_LITERAL(3.0) * s_dt * s_ * s_;
            alwan_scalar ldt2 = ALWAN_LITERAL(6.0) * l_dt * l_dt * l_;
            alwan_scalar mdt2 = ALWAN_LITERAL(6.0) * m_dt * m_dt * m_;
            alwan_scalar sdt2 = ALWAN_LITERAL(6.0) * s_dt * s_dt * s_;

            alwan_scalar rgb_r  =  ALWAN_LITERAL(4.0767416621) * l  - ALWAN_LITERAL(3.3077115913) * m  + ALWAN_LITERAL(0.2309699292) * s  - ALWAN_ONE;
            alwan_scalar rgb_r1 =  ALWAN_LITERAL(4.0767416621) * ldt - ALWAN_LITERAL(3.3077115913) * mdt + ALWAN_LITERAL(0.2309699292) * sdt;
            alwan_scalar rgb_r2 =  ALWAN_LITERAL(4.0767416621) * ldt2 - ALWAN_LITERAL(3.3077115913) * mdt2 + ALWAN_LITERAL(0.2309699292) * sdt2;
            alwan_scalar u_r = rgb_r1 / (rgb_r1 * rgb_r1 - ALWAN_LITERAL(0.5) * rgb_r * rgb_r2);
            alwan_scalar t_r = -rgb_r * u_r;

            alwan_scalar rgb_g  = -ALWAN_LITERAL(1.2684380046) * l  + ALWAN_LITERAL(2.6097574011) * m  - ALWAN_LITERAL(0.3413193965) * s  - ALWAN_ONE;
            alwan_scalar rgb_g1 = -ALWAN_LITERAL(1.2684380046) * ldt + ALWAN_LITERAL(2.6097574011) * mdt - ALWAN_LITERAL(0.3413193965) * sdt;
            alwan_scalar rgb_g2 = -ALWAN_LITERAL(1.2684380046) * ldt2 + ALWAN_LITERAL(2.6097574011) * mdt2 - ALWAN_LITERAL(0.3413193965) * sdt2;
            alwan_scalar u_g = rgb_g1 / (rgb_g1 * rgb_g1 - ALWAN_LITERAL(0.5) * rgb_g * rgb_g2);
            alwan_scalar t_g = -rgb_g * u_g;

            alwan_scalar rgb_b  = -ALWAN_LITERAL(0.0041960863) * l  - ALWAN_LITERAL(0.7034186147) * m  + ALWAN_LITERAL(1.7076147010) * s  - ALWAN_ONE;
            alwan_scalar rgb_b1 = -ALWAN_LITERAL(0.0041960863) * ldt - ALWAN_LITERAL(0.7034186147) * mdt + ALWAN_LITERAL(1.7076147010) * sdt;
            alwan_scalar rgb_b2 = -ALWAN_LITERAL(0.0041960863) * ldt2 - ALWAN_LITERAL(0.7034186147) * mdt2 + ALWAN_LITERAL(1.7076147010) * sdt2;
            alwan_scalar u_b = rgb_b1 / (rgb_b1 * rgb_b1 - ALWAN_LITERAL(0.5) * rgb_b * rgb_b2);
            alwan_scalar t_b = -rgb_b * u_b;

            /* Take minimum positive correction */
            t_r = ALWAN_SELECT(u_r >= ALWAN_ZERO, t_r, ALWAN_LITERAL(1e30));
            t_g = ALWAN_SELECT(u_g >= ALWAN_ZERO, t_g, ALWAN_LITERAL(1e30));
            t_b = ALWAN_SELECT(u_b >= ALWAN_ZERO, t_b, ALWAN_LITERAL(1e30));
            t += alwan_min3(t_r, t_g, t_b);
        }
    }
    return t;
}

/* ST mid-point polynomial approximation */
ALWAN_INLINE void alwan_ok_get_ST_mid(alwan_scalar a_, alwan_scalar b_,
                                        alwan_scalar *S_mid, alwan_scalar *T_mid) {
    *S_mid = ALWAN_LITERAL(0.11516993) + ALWAN_ONE / (
        ALWAN_LITERAL(7.44778970) + ALWAN_LITERAL(4.15901240) * b_
        + a_ * (ALWAN_LITERAL(-2.19557347) + ALWAN_LITERAL(1.75198401) * b_
        + a_ * (ALWAN_LITERAL(-2.13704948) - ALWAN_LITERAL(10.02301043) * b_
        + a_ * (ALWAN_LITERAL(-4.24894561) + ALWAN_LITERAL(5.38770819) * b_
        + ALWAN_LITERAL(4.69891013) * a_))));

    *T_mid = ALWAN_LITERAL(0.11239642) + ALWAN_ONE / (
        ALWAN_LITERAL(1.61320320) - ALWAN_LITERAL(0.68124379) * b_
        + a_ * (ALWAN_LITERAL(0.40370612) + ALWAN_LITERAL(0.90148123) * b_
        + a_ * (ALWAN_LITERAL(-0.27087943) + ALWAN_LITERAL(0.61223990) * b_
        + a_ * (ALWAN_LITERAL(0.00299215) - ALWAN_LITERAL(0.45399568) * b_
        - ALWAN_LITERAL(0.14661872) * a_))));
}

/* Get three chroma boundaries (C_0, C_mid, C_max) */
ALWAN_INLINE void alwan_ok_get_Cs(alwan_scalar L, alwan_scalar a_, alwan_scalar b_,
                                    alwan_scalar *C_0, alwan_scalar *C_mid, alwan_scalar *C_max) {
    alwan_scalar cusp_L, cusp_C;
    alwan_scalar S_max, T_max, k;
    alwan_scalar S_mid, T_mid, C_a, C_b;
    alwan_scalar C_a0, C_b0;

    alwan_ok_find_cusp(&cusp_L, &cusp_C, a_, b_);
    *C_max = alwan_ok_find_gamut_intersection(a_, b_, L, ALWAN_ONE, L, cusp_L, cusp_C);
    S_max = ALWAN_SELECT(cusp_L > ALWAN_LITERAL(1e-10), cusp_C / cusp_L, ALWAN_ZERO);
    T_max = ALWAN_SELECT((ALWAN_ONE - cusp_L) > ALWAN_LITERAL(1e-10),
                          cusp_C / (ALWAN_ONE - cusp_L), ALWAN_ZERO);

    k = (*C_max) / alwan_min(L * S_max, (ALWAN_ONE - L) * T_max + ALWAN_LITERAL(1e-10));

    alwan_ok_get_ST_mid(a_, b_, &S_mid, &T_mid);
    C_a = L * S_mid;
    C_b = (ALWAN_ONE - L) * T_mid;
    {
        alwan_scalar ca4 = C_a * C_a * C_a * C_a;
        alwan_scalar cb4 = C_b * C_b * C_b * C_b;
        alwan_scalar blend = ALWAN_SELECT((ca4 + cb4) > ALWAN_LITERAL(1e-30),
            ALWAN_POW(ALWAN_ONE / (ALWAN_ONE / (ca4 + ALWAN_LITERAL(1e-30))
                                  + ALWAN_ONE / (cb4 + ALWAN_LITERAL(1e-30))),
                      ALWAN_LITERAL(0.25)),
            ALWAN_ZERO);
        *C_mid = ALWAN_LITERAL(0.9) * k * blend;
    }

    C_a0 = L * ALWAN_LITERAL(0.4);
    C_b0 = (ALWAN_ONE - L) * ALWAN_LITERAL(0.8);
    *C_0 = ALWAN_SQRT(ALWAN_ONE / (ALWAN_ONE / (C_a0 * C_a0 + ALWAN_LITERAL(1e-30))
                                  + ALWAN_ONE / (C_b0 * C_b0 + ALWAN_LITERAL(1e-30))));
}

/* Okhsl -> sRGB (encoded) */
ALWAN_INLINE alwan_rgb alwan_okhsl_to_srgb_v(alwan_okhsl okhsl) {
    alwan_rgb result;
    alwan_scalar h = okhsl.h, s = okhsl.s, l = okhsl.l;
    alwan_scalar a_, b_, L, C, r, g, b;
    alwan_scalar C_0, C_mid, C_max;

    if (l >= ALWAN_ONE - ALWAN_LITERAL(1e-10)) {
        result.r = ALWAN_ONE; result.g = ALWAN_ONE; result.b = ALWAN_ONE;
        return result;
    }
    if (l <= ALWAN_LITERAL(1e-10)) {
        result.r = ALWAN_ZERO; result.g = ALWAN_ZERO; result.b = ALWAN_ZERO;
        return result;
    }

    a_ = ALWAN_COS(ALWAN_LITERAL(2.0) * ALWAN_PI * h);
    b_ = ALWAN_SIN(ALWAN_LITERAL(2.0) * ALWAN_PI * h);
    L = alwan_ok_toe_inv(l);

    alwan_ok_get_Cs(L, a_, b_, &C_0, &C_mid, &C_max);

    {
        alwan_scalar mid = ALWAN_LITERAL(0.8);
        alwan_scalar mid_inv = ALWAN_LITERAL(1.25);

        if (s < mid) {
            alwan_scalar t = mid_inv * s;
            alwan_scalar k_1 = mid * C_0;
            alwan_scalar k_2 = ALWAN_ONE - k_1 / (C_mid + ALWAN_LITERAL(1e-30));
            C = t * k_1 / (ALWAN_ONE - k_2 * t + ALWAN_LITERAL(1e-30));
        } else {
            alwan_scalar t = (s - mid) / (ALWAN_ONE - mid);
            alwan_scalar k_0 = C_mid;
            alwan_scalar k_1 = (ALWAN_ONE - mid) * C_mid * C_mid * mid_inv * mid_inv
                             / (C_0 + ALWAN_LITERAL(1e-30));
            alwan_scalar k_2 = ALWAN_ONE - k_1 / (C_max - C_mid + ALWAN_LITERAL(1e-30));
            C = k_0 + t * k_1 / (ALWAN_ONE - k_2 * t + ALWAN_LITERAL(1e-30));
        }
    }

    alwan_ok_lab_to_srgb(&r, &g, &b, L, C * a_, C * b_);

    result.r = alwan_srgb_oetf(r);
    result.g = alwan_srgb_oetf(g);
    result.b = alwan_srgb_oetf(b);
    return result;
}

/* sRGB (encoded) -> Okhsl */
ALWAN_INLINE alwan_okhsl alwan_srgb_to_okhsl_v(alwan_rgb srgb) {
    alwan_okhsl result;
    alwan_scalar r_lin = alwan_srgb_eotf(srgb.r);
    alwan_scalar g_lin = alwan_srgb_eotf(srgb.g);
    alwan_scalar b_lin = alwan_srgb_eotf(srgb.b);
    alwan_scalar Lab_L, Lab_a, Lab_b;
    alwan_scalar C, a_, b_;
    alwan_scalar C_0, C_mid, C_max;

    alwan_ok_srgb_to_lab(&Lab_L, &Lab_a, &Lab_b, r_lin, g_lin, b_lin);
    C = ALWAN_SQRT(Lab_a * Lab_a + Lab_b * Lab_b);

    if (C < ALWAN_LITERAL(1e-10)) {
        result.h = ALWAN_ZERO;
        result.s = ALWAN_ZERO;
        result.l = alwan_ok_toe(Lab_L);
        return result;
    }

    a_ = Lab_a / C;
    b_ = Lab_b / C;
    result.h = ALWAN_LITERAL(0.5) + ALWAN_LITERAL(0.5) * ALWAN_ATAN2(-Lab_b, -Lab_a) / ALWAN_PI;

    alwan_ok_get_Cs(Lab_L, a_, b_, &C_0, &C_mid, &C_max);

    {
        alwan_scalar mid = ALWAN_LITERAL(0.8);
        alwan_scalar mid_inv = ALWAN_LITERAL(1.25);

        if (C < C_mid) {
            alwan_scalar k_1 = mid * C_0;
            alwan_scalar k_2 = ALWAN_ONE - k_1 / (C_mid + ALWAN_LITERAL(1e-30));
            alwan_scalar t = C / (k_1 + k_2 * C + ALWAN_LITERAL(1e-30));
            result.s = t * mid;
        } else {
            alwan_scalar k_0 = C_mid;
            alwan_scalar k_1 = (ALWAN_ONE - mid) * C_mid * C_mid * mid_inv * mid_inv
                             / (C_0 + ALWAN_LITERAL(1e-30));
            alwan_scalar k_2 = ALWAN_ONE - k_1 / (C_max - C_mid + ALWAN_LITERAL(1e-30));
            alwan_scalar t = (C - k_0) / (k_1 + k_2 * (C - k_0) + ALWAN_LITERAL(1e-30));
            result.s = mid + (ALWAN_ONE - mid) * t;
        }
    }
    result.l = alwan_ok_toe(Lab_L);
    return result;
}

/* Okhsv -> sRGB (encoded) */
ALWAN_INLINE alwan_rgb alwan_okhsv_to_srgb_v(alwan_okhsv okhsv) {
    alwan_rgb result;
    alwan_scalar h = okhsv.h, s = okhsv.s, v = okhsv.v;
    alwan_scalar a_, b_;
    alwan_scalar cusp_L, cusp_C, S_max, T_max;
    alwan_scalar S_0, k, L_v, C_v, L, C;
    alwan_scalar L_vt, C_vt, r_s, g_s, b_s, scale_L;
    alwan_scalar r_out, g_out, b_out;

    if (v <= ALWAN_LITERAL(1e-10)) {
        result.r = ALWAN_ZERO; result.g = ALWAN_ZERO; result.b = ALWAN_ZERO;
        return result;
    }

    a_ = ALWAN_COS(ALWAN_LITERAL(2.0) * ALWAN_PI * h);
    b_ = ALWAN_SIN(ALWAN_LITERAL(2.0) * ALWAN_PI * h);

    alwan_ok_find_cusp(&cusp_L, &cusp_C, a_, b_);
    S_max = ALWAN_SELECT(cusp_L > ALWAN_LITERAL(1e-10), cusp_C / cusp_L, ALWAN_ZERO);
    T_max = ALWAN_SELECT((ALWAN_ONE - cusp_L) > ALWAN_LITERAL(1e-10),
                          cusp_C / (ALWAN_ONE - cusp_L), ALWAN_ZERO);

    S_0 = ALWAN_LITERAL(0.5);
    k = ALWAN_ONE - S_0 / (S_max + ALWAN_LITERAL(1e-10));

    L_v = ALWAN_ONE - s * S_0 / (S_0 + T_max - T_max * k * s + ALWAN_LITERAL(1e-10));
    C_v = s * T_max * S_0 / (S_0 + T_max - T_max * k * s + ALWAN_LITERAL(1e-10));

    L = v * L_v;
    C = v * C_v;

    L_vt = alwan_ok_toe_inv(L_v);
    C_vt = ALWAN_SELECT(L_v > ALWAN_LITERAL(1e-10), C_v * L_vt / L_v, ALWAN_ZERO);

    {
        alwan_scalar L_new = alwan_ok_toe_inv(L);
        C = ALWAN_SELECT(L > ALWAN_LITERAL(1e-10), C * L_new / L, ALWAN_ZERO);
        L = L_new;
    }

    alwan_ok_lab_to_srgb(&r_s, &g_s, &b_s, L_vt, a_ * C_vt, b_ * C_vt);
    scale_L = ALWAN_CBRT(ALWAN_ONE / (alwan_max3(r_s, g_s, b_s) + ALWAN_LITERAL(1e-10)));

    L *= scale_L;
    C *= scale_L;

    alwan_ok_lab_to_srgb(&r_out, &g_out, &b_out, L, C * a_, C * b_);

    result.r = alwan_srgb_oetf(r_out);
    result.g = alwan_srgb_oetf(g_out);
    result.b = alwan_srgb_oetf(b_out);
    return result;
}

/* sRGB (encoded) -> Okhsv */
ALWAN_INLINE alwan_okhsv alwan_srgb_to_okhsv_v(alwan_rgb srgb) {
    alwan_okhsv result;
    alwan_scalar r_lin = alwan_srgb_eotf(srgb.r);
    alwan_scalar g_lin = alwan_srgb_eotf(srgb.g);
    alwan_scalar b_lin = alwan_srgb_eotf(srgb.b);
    alwan_scalar Lab_L, Lab_a, Lab_b;
    alwan_scalar C, a_, b_;
    alwan_scalar cusp_L, cusp_C, S_max, T_max;
    alwan_scalar S_0, k;

    alwan_ok_srgb_to_lab(&Lab_L, &Lab_a, &Lab_b, r_lin, g_lin, b_lin);
    C = ALWAN_SQRT(Lab_a * Lab_a + Lab_b * Lab_b);

    if (C < ALWAN_LITERAL(1e-10)) {
        result.h = ALWAN_ZERO;
        result.s = ALWAN_ZERO;
        result.v = alwan_ok_toe(Lab_L);
        return result;
    }

    a_ = Lab_a / C;
    b_ = Lab_b / C;
    result.h = ALWAN_LITERAL(0.5) + ALWAN_LITERAL(0.5) * ALWAN_ATAN2(-Lab_b, -Lab_a) / ALWAN_PI;

    alwan_ok_find_cusp(&cusp_L, &cusp_C, a_, b_);
    S_max = ALWAN_SELECT(cusp_L > ALWAN_LITERAL(1e-10), cusp_C / cusp_L, ALWAN_ZERO);
    T_max = ALWAN_SELECT((ALWAN_ONE - cusp_L) > ALWAN_LITERAL(1e-10),
                          cusp_C / (ALWAN_ONE - cusp_L), ALWAN_ZERO);
    S_0 = ALWAN_LITERAL(0.5);
    k = ALWAN_ONE - S_0 / (S_max + ALWAN_LITERAL(1e-10));

    {
        alwan_scalar t = T_max / (C + Lab_L * T_max + ALWAN_LITERAL(1e-10));
        alwan_scalar L_v = t * Lab_L;
        alwan_scalar C_v = t * C;

        alwan_scalar L_vt = alwan_ok_toe_inv(L_v);
        alwan_scalar C_vt = ALWAN_SELECT(L_v > ALWAN_LITERAL(1e-10),
                                          C_v * L_vt / L_v, ALWAN_ZERO);

        alwan_scalar r_s, g_s, b_s;
        alwan_scalar scale_L;
        alwan_ok_lab_to_srgb(&r_s, &g_s, &b_s, L_vt, a_ * C_vt, b_ * C_vt);
        scale_L = ALWAN_CBRT(ALWAN_ONE / (alwan_max3(r_s, g_s, b_s) + ALWAN_LITERAL(1e-10)));

        Lab_L /= scale_L;
        C /= scale_L;

        C = C * alwan_ok_toe(Lab_L) / (Lab_L + ALWAN_LITERAL(1e-10));
        Lab_L = alwan_ok_toe(Lab_L);

        result.v = Lab_L / (L_v + ALWAN_LITERAL(1e-10));
        result.s = (S_0 + T_max) * C_v / (T_max * S_0 + T_max * k * C_v + ALWAN_LITERAL(1e-10));
    }

    return result;
}

#endif /* ALWAN_EXTENDED_CORE_H */
