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

/* ================================================================
 * Matrix Data (static const, CSV-embedded)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* hdr-IPT matrices */
static alwan_scalar const ALWAN_EXT_LMS_TO_IPT_HDR[9] = {
#include "../data/matrices/lms_to_ipt_hdr.csv"
};
static alwan_scalar const ALWAN_EXT_IPT_TO_LMS_HDR[9] = {
#include "../data/matrices/ipt_to_lms_hdr.csv"
};
static alwan_scalar const ALWAN_EXT_XYZ_TO_LMS_IPT[9] = {
#include "../data/matrices/xyz_to_lms_ipt.csv"
};
static alwan_scalar const ALWAN_EXT_LMS_TO_XYZ_IPT[9] = {
#include "../data/matrices/lms_to_xyz_ipt.csv"
};

/* IgPgTg matrices */
static alwan_scalar const ALWAN_EXT_LMS_TO_IGPGTG[9] = {
#include "../data/matrices/lms_to_igpgtg.csv"
};
static alwan_scalar const ALWAN_EXT_IGPGTG_TO_LMS[9] = {
#include "../data/matrices/igpgtg_to_lms.csv"
};
static alwan_scalar const ALWAN_EXT_XYZ_TO_LMS_IGPGTG[9] = {
#include "../data/matrices/xyz_to_lms_igpgtg.csv"
};
static alwan_scalar const ALWAN_EXT_LMS_TO_XYZ_IGPGTG[9] = {
#include "../data/matrices/lms_to_xyz_igpgtg.csv"
};

/* ICaCb matrices */
static alwan_scalar const ALWAN_EXT_LMS_TO_ICACB[9] = {
#include "../data/matrices/lms_to_icacb.csv"
};
static alwan_scalar const ALWAN_EXT_ICACB_TO_LMS[9] = {
#include "../data/matrices/icacb_to_lms.csv"
};
static alwan_scalar const ALWAN_EXT_XYZ_TO_LMS_ICACB[9] = {
#include "../data/matrices/xyz_to_lms_icacb.csv"
};
static alwan_scalar const ALWAN_EXT_LMS_TO_XYZ_ICACB[9] = {
#include "../data/matrices/lms_to_xyz_icacb.csv"
};

/* IgPgTg LMS scaling factors */
static alwan_scalar const ALWAN_EXT_IGPGTG_LMS_SCALE[3] = {
#include "../data/igpgtg_lms_scale.csv"
};

/* D65 white point for HDR calculations (Y=1 scale) */
static alwan_scalar const ALWAN_EXT_HDR_D65_WHITE[3] = {
#include "../data/hdr_d65_white.csv"
};

/* IHLS matrices */
static alwan_scalar const ALWAN_EXT_IHLS_RGB_TO_YC1C2[9] = {
#include "../data/ihls_rgb_to_yc1c2.csv"
};
static alwan_scalar const ALWAN_EXT_IHLS_YC1C2_TO_RGB[9] = {
#include "../data/ihls_yc1c2_to_rgb.csv"
};

ALWAN_DIAG_POP

/* ================================================================
 * Helper: Sign-preserving power (spow)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_spow_v(alwan_scalar x, alwan_scalar p) {
    return ALWAN_SELECT(x >= ALWAN_LITERAL(0.0),
                        ALWAN_POW(x, p),
                        -ALWAN_POW(-x, p));
}

/* ================================================================
 * Helper: Michaelis-Menten lightness (Fairchild 2011)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_lightness_fairchild2011_v(alwan_scalar Y, alwan_scalar epsilon) {
    alwan_scalar const V_max = ALWAN_LITERAL(247.0);
    alwan_scalar K_m = ALWAN_POW(ALWAN_LITERAL(2.0), epsilon);
    alwan_scalar Y_p = alwan_spow_v(Y, epsilon);
    return (V_max * Y_p) / (K_m + Y_p) + ALWAN_LITERAL(0.02);
}

/* ================================================================
 * Helper: Inverse Michaelis-Menten (Fairchild 2011)
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
 * Helper: PQ (ST2084) inverse EOTF — C -> N
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
 * Helper: PQ (ST2084) EOTF — N -> C
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
    alwan_mat3x3 m;
    m.m[0] = ALWAN_EXT_XYZ_TO_LMS_IPT[0]; m.m[1] = ALWAN_EXT_XYZ_TO_LMS_IPT[1]; m.m[2] = ALWAN_EXT_XYZ_TO_LMS_IPT[2];
    m.m[3] = ALWAN_EXT_XYZ_TO_LMS_IPT[3]; m.m[4] = ALWAN_EXT_XYZ_TO_LMS_IPT[4]; m.m[5] = ALWAN_EXT_XYZ_TO_LMS_IPT[5];
    m.m[6] = ALWAN_EXT_XYZ_TO_LMS_IPT[6]; m.m[7] = ALWAN_EXT_XYZ_TO_LMS_IPT[7]; m.m[8] = ALWAN_EXT_XYZ_TO_LMS_IPT[8];

    alwan_vec3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    alwan_vec3 lms = alwan_mat3_mulv_v(m, vec_in);

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
    alwan_mat3x3 m2;
    m2.m[0] = ALWAN_EXT_LMS_TO_IPT_HDR[0]; m2.m[1] = ALWAN_EXT_LMS_TO_IPT_HDR[1]; m2.m[2] = ALWAN_EXT_LMS_TO_IPT_HDR[2];
    m2.m[3] = ALWAN_EXT_LMS_TO_IPT_HDR[3]; m2.m[4] = ALWAN_EXT_LMS_TO_IPT_HDR[4]; m2.m[5] = ALWAN_EXT_LMS_TO_IPT_HDR[5];
    m2.m[6] = ALWAN_EXT_LMS_TO_IPT_HDR[6]; m2.m[7] = ALWAN_EXT_LMS_TO_IPT_HDR[7]; m2.m[8] = ALWAN_EXT_LMS_TO_IPT_HDR[8];

    alwan_vec3 out = alwan_mat3_mulv_v(m2, lms);
    result.I = out.v[0]; result.P = out.v[1]; result.T = out.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_hdr_ipt_to_xyz_v(alwan_ipt hdr_ipt) {
    alwan_xyz result;

    alwan_scalar lf = ALWAN_LN(ALWAN_LITERAL(318.0)) / ALWAN_LN(ALWAN_LITERAL(100.0));
    alwan_scalar sf = ALWAN_LITERAL(1.25) - ALWAN_LITERAL(0.25) * (ALWAN_LITERAL(0.2) / ALWAN_LITERAL(0.184));
    alwan_scalar epsilon = ALWAN_LITERAL(0.59) / (sf * lf);

    /* IPT -> LMS */
    alwan_mat3x3 m;
    m.m[0] = ALWAN_EXT_IPT_TO_LMS_HDR[0]; m.m[1] = ALWAN_EXT_IPT_TO_LMS_HDR[1]; m.m[2] = ALWAN_EXT_IPT_TO_LMS_HDR[2];
    m.m[3] = ALWAN_EXT_IPT_TO_LMS_HDR[3]; m.m[4] = ALWAN_EXT_IPT_TO_LMS_HDR[4]; m.m[5] = ALWAN_EXT_IPT_TO_LMS_HDR[5];
    m.m[6] = ALWAN_EXT_IPT_TO_LMS_HDR[6]; m.m[7] = ALWAN_EXT_IPT_TO_LMS_HDR[7]; m.m[8] = ALWAN_EXT_IPT_TO_LMS_HDR[8];

    alwan_vec3 vec_in;
    vec_in.v[0] = hdr_ipt.I; vec_in.v[1] = hdr_ipt.P; vec_in.v[2] = hdr_ipt.T;
    alwan_vec3 lms = alwan_mat3_mulv_v(m, vec_in);

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
    alwan_mat3x3 m2;
    m2.m[0] = ALWAN_EXT_LMS_TO_XYZ_IPT[0]; m2.m[1] = ALWAN_EXT_LMS_TO_XYZ_IPT[1]; m2.m[2] = ALWAN_EXT_LMS_TO_XYZ_IPT[2];
    m2.m[3] = ALWAN_EXT_LMS_TO_XYZ_IPT[3]; m2.m[4] = ALWAN_EXT_LMS_TO_XYZ_IPT[4]; m2.m[5] = ALWAN_EXT_LMS_TO_XYZ_IPT[5];
    m2.m[6] = ALWAN_EXT_LMS_TO_XYZ_IPT[6]; m2.m[7] = ALWAN_EXT_LMS_TO_XYZ_IPT[7]; m2.m[8] = ALWAN_EXT_LMS_TO_XYZ_IPT[8];

    alwan_vec3 out = alwan_mat3_mulv_v(m2, lms);
    result.x = out.v[0]; result.y = out.v[1]; result.z = out.v[2];
    return result;
}

/* ================================================================
 * IgPgTg (Ebner & Fairchild 1998)
 * ================================================================ */

ALWAN_INLINE alwan_igpgtg alwan_xyz_to_igpgtg_v(alwan_xyz xyz) {
    alwan_igpgtg result;

    /* XYZ -> LMS */
    alwan_mat3x3 m;
    m.m[0] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[0]; m.m[1] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[1]; m.m[2] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[2];
    m.m[3] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[3]; m.m[4] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[4]; m.m[5] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[5];
    m.m[6] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[6]; m.m[7] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[7]; m.m[8] = ALWAN_EXT_XYZ_TO_LMS_IGPGTG[8];

    alwan_vec3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    alwan_vec3 lms = alwan_mat3_mulv_v(m, vec_in);

    /* Scaled nonlinearity: spow(LMS / scale, 0.427) */
    alwan_scalar const exponent = ALWAN_LITERAL(0.427);
    lms.v[0] = alwan_spow_v(lms.v[0] / ALWAN_EXT_IGPGTG_LMS_SCALE[0], exponent);
    lms.v[1] = alwan_spow_v(lms.v[1] / ALWAN_EXT_IGPGTG_LMS_SCALE[1], exponent);
    lms.v[2] = alwan_spow_v(lms.v[2] / ALWAN_EXT_IGPGTG_LMS_SCALE[2], exponent);

    /* LMS -> IgPgTg */
    alwan_mat3x3 m2;
    m2.m[0] = ALWAN_EXT_LMS_TO_IGPGTG[0]; m2.m[1] = ALWAN_EXT_LMS_TO_IGPGTG[1]; m2.m[2] = ALWAN_EXT_LMS_TO_IGPGTG[2];
    m2.m[3] = ALWAN_EXT_LMS_TO_IGPGTG[3]; m2.m[4] = ALWAN_EXT_LMS_TO_IGPGTG[4]; m2.m[5] = ALWAN_EXT_LMS_TO_IGPGTG[5];
    m2.m[6] = ALWAN_EXT_LMS_TO_IGPGTG[6]; m2.m[7] = ALWAN_EXT_LMS_TO_IGPGTG[7]; m2.m[8] = ALWAN_EXT_LMS_TO_IGPGTG[8];

    alwan_vec3 out = alwan_mat3_mulv_v(m2, lms);
    result.Ig = out.v[0]; result.Pg = out.v[1]; result.Tg = out.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_igpgtg_to_xyz_v(alwan_igpgtg igpgtg) {
    alwan_xyz result;

    /* IgPgTg -> LMS */
    alwan_mat3x3 m;
    m.m[0] = ALWAN_EXT_IGPGTG_TO_LMS[0]; m.m[1] = ALWAN_EXT_IGPGTG_TO_LMS[1]; m.m[2] = ALWAN_EXT_IGPGTG_TO_LMS[2];
    m.m[3] = ALWAN_EXT_IGPGTG_TO_LMS[3]; m.m[4] = ALWAN_EXT_IGPGTG_TO_LMS[4]; m.m[5] = ALWAN_EXT_IGPGTG_TO_LMS[5];
    m.m[6] = ALWAN_EXT_IGPGTG_TO_LMS[6]; m.m[7] = ALWAN_EXT_IGPGTG_TO_LMS[7]; m.m[8] = ALWAN_EXT_IGPGTG_TO_LMS[8];

    alwan_vec3 vec_in;
    vec_in.v[0] = igpgtg.Ig; vec_in.v[1] = igpgtg.Pg; vec_in.v[2] = igpgtg.Tg;
    alwan_vec3 lms = alwan_mat3_mulv_v(m, vec_in);

    /* Inverse scaled nonlinearity: scale * spow(LMS, 1/0.427) */
    alwan_scalar const inv_exponent = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.427);
    lms.v[0] = ALWAN_EXT_IGPGTG_LMS_SCALE[0] * alwan_spow_v(lms.v[0], inv_exponent);
    lms.v[1] = ALWAN_EXT_IGPGTG_LMS_SCALE[1] * alwan_spow_v(lms.v[1], inv_exponent);
    lms.v[2] = ALWAN_EXT_IGPGTG_LMS_SCALE[2] * alwan_spow_v(lms.v[2], inv_exponent);

    /* LMS -> XYZ */
    alwan_mat3x3 m2;
    m2.m[0] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[0]; m2.m[1] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[1]; m2.m[2] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[2];
    m2.m[3] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[3]; m2.m[4] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[4]; m2.m[5] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[5];
    m2.m[6] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[6]; m2.m[7] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[7]; m2.m[8] = ALWAN_EXT_LMS_TO_XYZ_IGPGTG[8];

    alwan_vec3 out = alwan_mat3_mulv_v(m2, lms);
    result.x = out.v[0]; result.y = out.v[1]; result.z = out.v[2];
    return result;
}

/* ================================================================
 * ICaCb (Zhang & Wandell 1996, 1997)
 * ================================================================ */

ALWAN_INLINE alwan_icacb alwan_xyz_to_icacb_v(alwan_xyz xyz) {
    alwan_icacb result;

    /* XYZ -> LMS */
    alwan_mat3x3 m;
    m.m[0] = ALWAN_EXT_XYZ_TO_LMS_ICACB[0]; m.m[1] = ALWAN_EXT_XYZ_TO_LMS_ICACB[1]; m.m[2] = ALWAN_EXT_XYZ_TO_LMS_ICACB[2];
    m.m[3] = ALWAN_EXT_XYZ_TO_LMS_ICACB[3]; m.m[4] = ALWAN_EXT_XYZ_TO_LMS_ICACB[4]; m.m[5] = ALWAN_EXT_XYZ_TO_LMS_ICACB[5];
    m.m[6] = ALWAN_EXT_XYZ_TO_LMS_ICACB[6]; m.m[7] = ALWAN_EXT_XYZ_TO_LMS_ICACB[7]; m.m[8] = ALWAN_EXT_XYZ_TO_LMS_ICACB[8];

    alwan_vec3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    alwan_vec3 lms = alwan_mat3_mulv_v(m, vec_in);

    /* PQ inverse EOTF */
    lms.v[0] = alwan_eotf_inverse_st2084_v(lms.v[0]);
    lms.v[1] = alwan_eotf_inverse_st2084_v(lms.v[1]);
    lms.v[2] = alwan_eotf_inverse_st2084_v(lms.v[2]);

    /* LMS -> ICaCb */
    alwan_mat3x3 m2;
    m2.m[0] = ALWAN_EXT_LMS_TO_ICACB[0]; m2.m[1] = ALWAN_EXT_LMS_TO_ICACB[1]; m2.m[2] = ALWAN_EXT_LMS_TO_ICACB[2];
    m2.m[3] = ALWAN_EXT_LMS_TO_ICACB[3]; m2.m[4] = ALWAN_EXT_LMS_TO_ICACB[4]; m2.m[5] = ALWAN_EXT_LMS_TO_ICACB[5];
    m2.m[6] = ALWAN_EXT_LMS_TO_ICACB[6]; m2.m[7] = ALWAN_EXT_LMS_TO_ICACB[7]; m2.m[8] = ALWAN_EXT_LMS_TO_ICACB[8];

    alwan_vec3 out = alwan_mat3_mulv_v(m2, lms);
    result.I = out.v[0]; result.Ca = out.v[1]; result.Cb = out.v[2];
    return result;
}

ALWAN_INLINE alwan_xyz alwan_icacb_to_xyz_v(alwan_icacb icacb) {
    alwan_xyz result;

    /* ICaCb -> LMS */
    alwan_mat3x3 m;
    m.m[0] = ALWAN_EXT_ICACB_TO_LMS[0]; m.m[1] = ALWAN_EXT_ICACB_TO_LMS[1]; m.m[2] = ALWAN_EXT_ICACB_TO_LMS[2];
    m.m[3] = ALWAN_EXT_ICACB_TO_LMS[3]; m.m[4] = ALWAN_EXT_ICACB_TO_LMS[4]; m.m[5] = ALWAN_EXT_ICACB_TO_LMS[5];
    m.m[6] = ALWAN_EXT_ICACB_TO_LMS[6]; m.m[7] = ALWAN_EXT_ICACB_TO_LMS[7]; m.m[8] = ALWAN_EXT_ICACB_TO_LMS[8];

    alwan_vec3 vec_in;
    vec_in.v[0] = icacb.I; vec_in.v[1] = icacb.Ca; vec_in.v[2] = icacb.Cb;
    alwan_vec3 lms = alwan_mat3_mulv_v(m, vec_in);

    /* PQ EOTF */
    lms.v[0] = alwan_eotf_st2084_v(lms.v[0]);
    lms.v[1] = alwan_eotf_st2084_v(lms.v[1]);
    lms.v[2] = alwan_eotf_st2084_v(lms.v[2]);

    /* LMS -> XYZ */
    alwan_mat3x3 m2;
    m2.m[0] = ALWAN_EXT_LMS_TO_XYZ_ICACB[0]; m2.m[1] = ALWAN_EXT_LMS_TO_XYZ_ICACB[1]; m2.m[2] = ALWAN_EXT_LMS_TO_XYZ_ICACB[2];
    m2.m[3] = ALWAN_EXT_LMS_TO_XYZ_ICACB[3]; m2.m[4] = ALWAN_EXT_LMS_TO_XYZ_ICACB[4]; m2.m[5] = ALWAN_EXT_LMS_TO_XYZ_ICACB[5];
    m2.m[6] = ALWAN_EXT_LMS_TO_XYZ_ICACB[6]; m2.m[7] = ALWAN_EXT_LMS_TO_XYZ_ICACB[7]; m2.m[8] = ALWAN_EXT_LMS_TO_XYZ_ICACB[8];

    alwan_vec3 out = alwan_mat3_mulv_v(m2, lms);
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
    /* When r_g ≈ 0 but g_b != 0, atan(g_b/r_g) = ±π/2 */
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
    alwan_scalar Y  = ALWAN_EXT_IHLS_RGB_TO_YC1C2[0] * rgb.r + ALWAN_EXT_IHLS_RGB_TO_YC1C2[1] * rgb.g + ALWAN_EXT_IHLS_RGB_TO_YC1C2[2] * rgb.b;
    alwan_scalar C_1 = ALWAN_EXT_IHLS_RGB_TO_YC1C2[3] * rgb.r + ALWAN_EXT_IHLS_RGB_TO_YC1C2[4] * rgb.g + ALWAN_EXT_IHLS_RGB_TO_YC1C2[5] * rgb.b;
    alwan_scalar C_2 = ALWAN_EXT_IHLS_RGB_TO_YC1C2[6] * rgb.r + ALWAN_EXT_IHLS_RGB_TO_YC1C2[7] * rgb.g + ALWAN_EXT_IHLS_RGB_TO_YC1C2[8] * rgb.b;

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

    result.r = ALWAN_EXT_IHLS_YC1C2_TO_RGB[0] * Y + ALWAN_EXT_IHLS_YC1C2_TO_RGB[1] * C_1 + ALWAN_EXT_IHLS_YC1C2_TO_RGB[2] * C_2;
    result.g = ALWAN_EXT_IHLS_YC1C2_TO_RGB[3] * Y + ALWAN_EXT_IHLS_YC1C2_TO_RGB[4] * C_1 + ALWAN_EXT_IHLS_YC1C2_TO_RGB[5] * C_2;
    result.b = ALWAN_EXT_IHLS_YC1C2_TO_RGB[6] * Y + ALWAN_EXT_IHLS_YC1C2_TO_RGB[7] * C_1 + ALWAN_EXT_IHLS_YC1C2_TO_RGB[8] * C_2;

    return result;
}

#endif /* ALWAN_EXTENDED_CORE_H */
