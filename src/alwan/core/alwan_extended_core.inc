/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Extended Color Spaces & Models (parameterized)
 * hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb, Prismatic, HCL, IHLS
 * No include guard - included twice via setup/teardown.
 */

/* ================================================================
 * Matrix Data (static const, CSV-embedded)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* hdr-IPT matrices */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_IPT_HDR) = {{
#include "../data/matrices/lms_to_ipt_hdr.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_IPT_TO_LMS_HDR) = {{
#include "../data/matrices/ipt_to_lms_hdr.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_XYZ_TO_LMS_IPT) = {{
#include "../data/matrices/xyz_to_lms_ipt.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_XYZ_IPT) = {{
#include "../data/matrices/lms_to_xyz_ipt.csv"
}};

/* IgPgTg matrices */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_IGPGTG) = {{
#include "../data/matrices/lms_to_igpgtg.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_TO_LMS) = {{
#include "../data/matrices/igpgtg_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_XYZ_TO_LMS_IGPGTG) = {{
#include "../data/matrices/xyz_to_lms_igpgtg.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_XYZ_IGPGTG) = {{
#include "../data/matrices/lms_to_xyz_igpgtg.csv"
}};

/* ICaCb matrices */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_ICACB) = {{
#include "../data/matrices/lms_to_icacb.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_ICACB_TO_LMS) = {{
#include "../data/matrices/icacb_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_XYZ_TO_LMS_ICACB) = {{
#include "../data/matrices/xyz_to_lms_icacb.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_XYZ_ICACB) = {{
#include "../data/matrices/lms_to_xyz_icacb.csv"
}};

/* IgPgTg LMS scaling factors */
static const ALWAN_CORE_T ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[3] = {
#include "../data/igpgtg_lms_scale.csv"
};

/* D65 white point for HDR calculations (Y=1 scale) */
static const ALWAN_CORE_T ALWAN_CORE_FNLIT(ALWAN_EXT_HDR_D65_WHITE)[3] = {
#include "../data/hdr_d65_white.csv"
};

/* IHLS matrices */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_IHLS_RGB_TO_YC1C2) = {{
#include "../data/ihls_rgb_to_yc1c2.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ALWAN_EXT_IHLS_YC1C2_TO_RGB) = {{
#include "../data/ihls_yc1c2_to_rgb.csv"
}};

ALWAN_DIAG_POP

/* ================================================================
 * Sign-preserving power (spow)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_spow)(ALWAN_CORE_T x, ALWAN_CORE_T p) {
    return ALWAN_CORE_SELECT(x >= ALWAN_CORE_LITERAL(0.0),
                        ALWAN_CORE_POW(x, p),
                        -ALWAN_CORE_POW(-x, p));
}

/* ================================================================
 * Michaelis-Menten lightness (Fairchild 2011)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_lightness_fairchild2011)(ALWAN_CORE_T Y, ALWAN_CORE_T epsilon) {
    const ALWAN_CORE_T V_max = ALWAN_CORE_LITERAL(247.0);
    ALWAN_CORE_T K_m = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), epsilon);
    ALWAN_CORE_T Y_p = ALWAN_CORE_FNVLIT(alwan_spow)(Y, epsilon);
    return (V_max * Y_p) / (K_m + Y_p) + ALWAN_CORE_LITERAL(0.02);
}

/* ================================================================
 * Inverse Michaelis-Menten (Fairchild 2011)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_luminance_fairchild2011)(ALWAN_CORE_T L_hdr, ALWAN_CORE_T epsilon) {
    const ALWAN_CORE_T V_max = ALWAN_CORE_LITERAL(247.0);
    ALWAN_CORE_T K_m = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), epsilon);
    ALWAN_CORE_T v = L_hdr - ALWAN_CORE_LITERAL(0.02);
    ALWAN_CORE_T denom = V_max - v;
    /* Avoid division by zero */
    ALWAN_CORE_T S = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(denom) < ALWAN_CORE_EPSILON,
                                  K_m,
                                  (v * K_m) / denom);
    return ALWAN_CORE_FNVLIT(alwan_spow)(S, ALWAN_CORE_LITERAL(1.0) / epsilon);
}

/* ================================================================
 * PQ (ST2084) inverse EOTF -- C -> N
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_eotf_inverse_st2084)(ALWAN_CORE_T C) {
    const ALWAN_CORE_T L_p = ALWAN_CORE_LITERAL(10000.0);
    const ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(0.8359375);
    const ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(18.8515625);
    const ALWAN_CORE_T c3 = ALWAN_CORE_LITERAL(18.6875);
    const ALWAN_CORE_T m1 = ALWAN_CORE_LITERAL(0.1593017578125);
    const ALWAN_CORE_T m2 = ALWAN_CORE_LITERAL(78.84375);

    ALWAN_CORE_T Y_p = ALWAN_CORE_POW(C / L_p, m1);
    ALWAN_CORE_T numerator = c1 + c2 * Y_p;
    ALWAN_CORE_T denominator = c3 * Y_p + ALWAN_CORE_LITERAL(1.0);
    return ALWAN_CORE_POW(numerator / denominator, m2);
}

/* ================================================================
 * PQ (ST2084) EOTF -- N -> C
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_eotf_st2084)(ALWAN_CORE_T N) {
    const ALWAN_CORE_T L_p = ALWAN_CORE_LITERAL(10000.0);
    const ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(0.8359375);
    const ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(18.8515625);
    const ALWAN_CORE_T c3 = ALWAN_CORE_LITERAL(18.6875);
    const ALWAN_CORE_T m1 = ALWAN_CORE_LITERAL(0.1593017578125);
    const ALWAN_CORE_T m2 = ALWAN_CORE_LITERAL(78.84375);
    const ALWAN_CORE_T inv_m2 = ALWAN_CORE_LITERAL(1.0) / m2;
    const ALWAN_CORE_T inv_m1 = ALWAN_CORE_LITERAL(1.0) / m1;

    ALWAN_CORE_T N_p = ALWAN_CORE_POW(N, inv_m2);
    ALWAN_CORE_T numerator = N_p - c1;
    ALWAN_CORE_T denominator = c2 - c3 * N_p;

    /* Guard: clamp negative values */
    numerator = ALWAN_CORE_SELECT(numerator < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), numerator);
    ALWAN_CORE_T Y_p = ALWAN_CORE_SELECT(denominator <= ALWAN_CORE_LITERAL(0.0),
                                    ALWAN_CORE_LITERAL(0.0),
                                    numerator / denominator);
    return L_p * ALWAN_CORE_POW(Y_p, inv_m1);
}

/* ================================================================
 * Prismatic (Pridmore 2021)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_PRISMATIC ALWAN_CORE_FNVLIT(alwan_rgb_to_prismatic)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_PRISMATIC result;
    ALWAN_CORE_T L = ALWAN_CORE_MAX3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T sum_rgb = rgb.r + rgb.g + rgb.b;
    result.L = L;
    result.s = ALWAN_CORE_SELECT(sum_rgb < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), rgb.r / sum_rgb);
    result.h = ALWAN_CORE_SELECT(sum_rgb < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), rgb.g / sum_rgb);
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_prismatic_to_rgb)(ALWAN_CORE_PRISMATIC prismatic) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T R_comp = ALWAN_CORE_LITERAL(1.0) - prismatic.s - prismatic.h;
    ALWAN_CORE_T max_pqr = ALWAN_CORE_MAX3(prismatic.s, prismatic.h, R_comp);
    ALWAN_CORE_T sum_rgb = ALWAN_CORE_SELECT(max_pqr < ALWAN_CORE_EPSILON,
                                        ALWAN_CORE_LITERAL(0.0),
                                        prismatic.L / max_pqr);
    result.r = prismatic.s * sum_rgb;
    result.g = prismatic.h * sum_rgb;
    result.b = R_comp * sum_rgb;
    return result;
}

/* ================================================================
 * hdr-CIELAB (Fairchild & Wyble 2010)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_LAB ALWAN_CORE_FNVLIT(alwan_xyz_to_hdr_cielab)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_LAB result;

    /* Default parameters: Y_s=0.2, Y_abs=100 cd/m^2.
     * Epsilon computed via Fairchild & Wyble (2010) CIC18 eq. 7:
     *   sf = 1.25 - 0.25*(Y_s/0.184), lf = ln(Y_abs)/ln(100), epsilon_base/sf/lf
     * Published constants: epsilon_base=0.58, Y_s=0.2, Y_abs=318 (adapting field). */
    ALWAN_CORE_T epsilon = ALWAN_CORE_LITERAL(0.58);
    ALWAN_CORE_T sf = ALWAN_CORE_LITERAL(1.25) - ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_LITERAL(0.2) / ALWAN_CORE_LITERAL(0.184));
    ALWAN_CORE_T lf = ALWAN_CORE_LN(ALWAN_CORE_LITERAL(318.0)) / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(100.0));
    epsilon /= sf * lf;

    ALWAN_CORE_T xr = xyz.x / ALWAN_CORE_FNLIT(ALWAN_EXT_HDR_D65_WHITE)[0];
    ALWAN_CORE_T yr = xyz.y / ALWAN_CORE_FNLIT(ALWAN_EXT_HDR_D65_WHITE)[1];
    ALWAN_CORE_T zr = xyz.z / ALWAN_CORE_FNLIT(ALWAN_EXT_HDR_D65_WHITE)[2];

    /* Use Fairchild2011 lightness with POW (not spow, since these are ratios) */
    const ALWAN_CORE_T V_max = ALWAN_CORE_LITERAL(247.0);
    ALWAN_CORE_T K_m = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(2.0), epsilon);

    ALWAN_CORE_T yr_eps = ALWAN_CORE_POW(yr, epsilon);
    ALWAN_CORE_T L_hdr = (V_max * yr_eps) / (K_m + yr_eps) + ALWAN_CORE_LITERAL(0.02);
    ALWAN_CORE_T xr_eps = ALWAN_CORE_POW(xr, epsilon);
    ALWAN_CORE_T fx = (V_max * xr_eps) / (K_m + xr_eps) + ALWAN_CORE_LITERAL(0.02);
    ALWAN_CORE_T zr_eps = ALWAN_CORE_POW(zr, epsilon);
    ALWAN_CORE_T fz = (V_max * zr_eps) / (K_m + zr_eps) + ALWAN_CORE_LITERAL(0.02);

    result.L = L_hdr;
    result.a = ALWAN_CORE_LITERAL(5.0) * (fx - L_hdr);
    result.b = ALWAN_CORE_LITERAL(2.0) * (L_hdr - fz);
    return result;
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_hdr_cielab_to_xyz)(ALWAN_CORE_LAB hdr_lab) {
    ALWAN_CORE_XYZ result;

    ALWAN_CORE_T epsilon = ALWAN_CORE_LITERAL(0.58);
    ALWAN_CORE_T sf = ALWAN_CORE_LITERAL(1.25) - ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_LITERAL(0.2) / ALWAN_CORE_LITERAL(0.184));
    ALWAN_CORE_T lf = ALWAN_CORE_LN(ALWAN_CORE_LITERAL(318.0)) / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(100.0));
    epsilon /= sf * lf;

    ALWAN_CORE_T yr = ALWAN_CORE_FNVLIT(alwan_luminance_fairchild2011)(hdr_lab.L, epsilon);
    ALWAN_CORE_T xr = ALWAN_CORE_FNVLIT(alwan_luminance_fairchild2011)(
        (hdr_lab.a + ALWAN_CORE_LITERAL(5.0) * hdr_lab.L) / ALWAN_CORE_LITERAL(5.0), epsilon);
    ALWAN_CORE_T zr = ALWAN_CORE_FNVLIT(alwan_luminance_fairchild2011)(
        (-hdr_lab.b + ALWAN_CORE_LITERAL(2.0) * hdr_lab.L) / ALWAN_CORE_LITERAL(2.0), epsilon);

    result.x = xr * ALWAN_CORE_FNLIT(ALWAN_EXT_HDR_D65_WHITE)[0];
    result.y = yr * ALWAN_CORE_FNLIT(ALWAN_EXT_HDR_D65_WHITE)[1];
    result.z = zr * ALWAN_CORE_FNLIT(ALWAN_EXT_HDR_D65_WHITE)[2];
    return result;
}

/* ================================================================
 * hdr-IPT (Fairchild 2011)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_IPT ALWAN_CORE_FNVLIT(alwan_xyz_to_hdr_ipt)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_IPT result;

    /* Epsilon formula per Fairchild (2011) "Thinking in Three Dimensions" eq. 4:
     *   epsilon = 0.59 / (sf * lf), with sf = 1.25 - 0.25*(Y_s/0.184),
     *   lf = ln(Y_abs)/ln(100), Y_s=0.2 (surround), Y_abs=318 cd/m^2 */
    ALWAN_CORE_T lf = ALWAN_CORE_LN(ALWAN_CORE_LITERAL(318.0)) / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T sf = ALWAN_CORE_LITERAL(1.25) - ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_LITERAL(0.2) / ALWAN_CORE_LITERAL(0.184));
    ALWAN_CORE_T epsilon = ALWAN_CORE_LITERAL(0.59) / (sf * lf);

    /* XYZ -> LMS via IPT matrix */
    ALWAN_CORE_VEC3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_XYZ_TO_LMS_IPT), vec_in);

    /* Apply sign(LMS) * |lightness(LMS, e)| */
    ALWAN_CORE_T sign0 = ALWAN_CORE_SELECT(lms.v[0] > ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0),
                          ALWAN_CORE_SELECT(lms.v[0] < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(-1.0), ALWAN_CORE_LITERAL(0.0)));
    ALWAN_CORE_T sign1 = ALWAN_CORE_SELECT(lms.v[1] > ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0),
                          ALWAN_CORE_SELECT(lms.v[1] < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(-1.0), ALWAN_CORE_LITERAL(0.0)));
    ALWAN_CORE_T sign2 = ALWAN_CORE_SELECT(lms.v[2] > ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0),
                          ALWAN_CORE_SELECT(lms.v[2] < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(-1.0), ALWAN_CORE_LITERAL(0.0)));

    lms.v[0] = sign0 * ALWAN_CORE_ABS(ALWAN_CORE_FNVLIT(alwan_lightness_fairchild2011)(lms.v[0], epsilon));
    lms.v[1] = sign1 * ALWAN_CORE_ABS(ALWAN_CORE_FNVLIT(alwan_lightness_fairchild2011)(lms.v[1], epsilon));
    lms.v[2] = sign2 * ALWAN_CORE_ABS(ALWAN_CORE_FNVLIT(alwan_lightness_fairchild2011)(lms.v[2], epsilon));

    /* LMS -> IPT */
    ALWAN_CORE_VEC3 mapped = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_IPT_HDR), lms);
    result.I = mapped.v[0]; result.P = mapped.v[1]; result.T = mapped.v[2];
    return result;
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_hdr_ipt_to_xyz)(ALWAN_CORE_IPT hdr_ipt) {
    ALWAN_CORE_XYZ result;

    ALWAN_CORE_T lf = ALWAN_CORE_LN(ALWAN_CORE_LITERAL(318.0)) / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(100.0));
    ALWAN_CORE_T sf = ALWAN_CORE_LITERAL(1.25) - ALWAN_CORE_LITERAL(0.25) * (ALWAN_CORE_LITERAL(0.2) / ALWAN_CORE_LITERAL(0.184));
    ALWAN_CORE_T epsilon = ALWAN_CORE_LITERAL(0.59) / (sf * lf);

    /* IPT -> LMS */
    ALWAN_CORE_VEC3 vec_in;
    vec_in.v[0] = hdr_ipt.I; vec_in.v[1] = hdr_ipt.P; vec_in.v[2] = hdr_ipt.T;
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_IPT_TO_LMS_HDR), vec_in);

    /* Apply sign(LMS) * |luminance(LMS, e)| */
    ALWAN_CORE_T sign0 = ALWAN_CORE_SELECT(lms.v[0] > ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0),
                          ALWAN_CORE_SELECT(lms.v[0] < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(-1.0), ALWAN_CORE_LITERAL(0.0)));
    ALWAN_CORE_T sign1 = ALWAN_CORE_SELECT(lms.v[1] > ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0),
                          ALWAN_CORE_SELECT(lms.v[1] < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(-1.0), ALWAN_CORE_LITERAL(0.0)));
    ALWAN_CORE_T sign2 = ALWAN_CORE_SELECT(lms.v[2] > ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0),
                          ALWAN_CORE_SELECT(lms.v[2] < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(-1.0), ALWAN_CORE_LITERAL(0.0)));

    lms.v[0] = sign0 * ALWAN_CORE_ABS(ALWAN_CORE_FNVLIT(alwan_luminance_fairchild2011)(lms.v[0], epsilon));
    lms.v[1] = sign1 * ALWAN_CORE_ABS(ALWAN_CORE_FNVLIT(alwan_luminance_fairchild2011)(lms.v[1], epsilon));
    lms.v[2] = sign2 * ALWAN_CORE_ABS(ALWAN_CORE_FNVLIT(alwan_luminance_fairchild2011)(lms.v[2], epsilon));

    /* LMS -> XYZ */
    ALWAN_CORE_VEC3 mapped = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_XYZ_IPT), lms);
    result.x = mapped.v[0]; result.y = mapped.v[1]; result.z = mapped.v[2];
    return result;
}

/* ================================================================
 * IgPgTg (Ebner & Fairchild 1998)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_IGPGTG ALWAN_CORE_FNVLIT(alwan_xyz_to_igpgtg)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_IGPGTG result;

    /* XYZ -> LMS */
    ALWAN_CORE_VEC3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_XYZ_TO_LMS_IGPGTG), vec_in);

    /* Scaled nonlinearity: spow(LMS / scale, 0.427)
     * Exponent 0.427 from Ebner & Fairchild (1998) "Development and testing
     * of a color space (IPT) with improved hue uniformity" IS&T 6th CIC, Table 3. */
    const ALWAN_CORE_T exponent = ALWAN_CORE_LITERAL(0.427);
    lms.v[0] = ALWAN_CORE_FNVLIT(alwan_spow)(lms.v[0] / ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[0], exponent);
    lms.v[1] = ALWAN_CORE_FNVLIT(alwan_spow)(lms.v[1] / ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[1], exponent);
    lms.v[2] = ALWAN_CORE_FNVLIT(alwan_spow)(lms.v[2] / ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[2], exponent);

    /* LMS -> IgPgTg */
    ALWAN_CORE_VEC3 mapped = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_IGPGTG), lms);
    result.Ig = mapped.v[0]; result.Pg = mapped.v[1]; result.Tg = mapped.v[2];
    return result;
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_igpgtg_to_xyz)(ALWAN_CORE_IGPGTG igpgtg) {
    ALWAN_CORE_XYZ result;

    /* IgPgTg -> LMS */
    ALWAN_CORE_VEC3 vec_in;
    vec_in.v[0] = igpgtg.Ig; vec_in.v[1] = igpgtg.Pg; vec_in.v[2] = igpgtg.Tg;
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_TO_LMS), vec_in);

    /* Inverse scaled nonlinearity: scale * spow(LMS, 1/0.427) */
    const ALWAN_CORE_T inv_exponent = ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(0.427);
    lms.v[0] = ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[0] * ALWAN_CORE_FNVLIT(alwan_spow)(lms.v[0], inv_exponent);
    lms.v[1] = ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[1] * ALWAN_CORE_FNVLIT(alwan_spow)(lms.v[1], inv_exponent);
    lms.v[2] = ALWAN_CORE_FNLIT(ALWAN_EXT_IGPGTG_LMS_SCALE)[2] * ALWAN_CORE_FNVLIT(alwan_spow)(lms.v[2], inv_exponent);

    /* LMS -> XYZ */
    ALWAN_CORE_VEC3 mapped = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_XYZ_IGPGTG), lms);
    result.x = mapped.v[0]; result.y = mapped.v[1]; result.z = mapped.v[2];
    return result;
}

/* ================================================================
 * ICaCb (Zhang & Wandell 1996, 1997)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_ICACB ALWAN_CORE_FNVLIT(alwan_xyz_to_icacb)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_ICACB result;

    /* XYZ -> LMS */
    ALWAN_CORE_VEC3 vec_in;
    vec_in.v[0] = xyz.x; vec_in.v[1] = xyz.y; vec_in.v[2] = xyz.z;
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_XYZ_TO_LMS_ICACB), vec_in);

    /* PQ inverse EOTF */
    lms.v[0] = ALWAN_CORE_FNVLIT(alwan_eotf_inverse_st2084)(lms.v[0]);
    lms.v[1] = ALWAN_CORE_FNVLIT(alwan_eotf_inverse_st2084)(lms.v[1]);
    lms.v[2] = ALWAN_CORE_FNVLIT(alwan_eotf_inverse_st2084)(lms.v[2]);

    /* LMS -> ICaCb */
    ALWAN_CORE_VEC3 mapped = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_ICACB), lms);
    result.I = mapped.v[0]; result.Ca = mapped.v[1]; result.Cb = mapped.v[2];
    return result;
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_icacb_to_xyz)(ALWAN_CORE_ICACB icacb) {
    ALWAN_CORE_XYZ result;

    /* ICaCb -> LMS */
    ALWAN_CORE_VEC3 vec_in;
    vec_in.v[0] = icacb.I; vec_in.v[1] = icacb.Ca; vec_in.v[2] = icacb.Cb;
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_ICACB_TO_LMS), vec_in);

    /* PQ EOTF */
    lms.v[0] = ALWAN_CORE_FNVLIT(alwan_eotf_st2084)(lms.v[0]);
    lms.v[1] = ALWAN_CORE_FNVLIT(alwan_eotf_st2084)(lms.v[1]);
    lms.v[2] = ALWAN_CORE_FNVLIT(alwan_eotf_st2084)(lms.v[2]);

    /* LMS -> XYZ */
    ALWAN_CORE_VEC3 mapped = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_LMS_TO_XYZ_ICACB), lms);
    result.x = mapped.v[0]; result.y = mapped.v[1]; result.z = mapped.v[2];
    return result;
}

/* ================================================================
 * HCL (Sarifuddin 2005)
 * Reference: Sarifuddin & Missaoui (2005) "A New Perceptually Uniform
 *   Color Space with Associated Color Similarity Measure for Content-Based
 *   Image and Video Retrieval" MMDB '05, ACM.
 * gamma=3.0 and Y_0=100.0 are the fixed model constants from eq. (9-11).
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_HCL ALWAN_CORE_FNVLIT(alwan_rgb_to_hcl)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_HCL result;

    /* gamma=3.0, Y_0=100.0 per Sarifuddin & Missaoui (2005) eqs. 9-11 */
    const ALWAN_CORE_T gamma = ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T Y_0 = ALWAN_CORE_LITERAL(100.0);

    ALWAN_CORE_T max_val = ALWAN_CORE_MAX3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T min_val = ALWAN_CORE_MIN3(rgb.r, rgb.g, rgb.b);

    /* Q factor */
    ALWAN_CORE_T Q = ALWAN_CORE_SELECT(max_val > ALWAN_CORE_EPSILON,
                                  ALWAN_CORE_EXP((min_val * gamma) / (max_val * Y_0)),
                                  ALWAN_CORE_LITERAL(1.0));

    /* Luminance */
    ALWAN_CORE_T L = (Q * max_val + (Q - ALWAN_CORE_LITERAL(1.0)) * min_val) / ALWAN_CORE_LITERAL(2.0);

    /* Chroma */
    ALWAN_CORE_T r_g = rgb.r - rgb.g;
    ALWAN_CORE_T g_b = rgb.g - rgb.b;
    ALWAN_CORE_T b_r = rgb.b - rgb.r;
    ALWAN_CORE_T C = Q * (ALWAN_CORE_ABS(r_g) + ALWAN_CORE_ABS(g_b) + ALWAN_CORE_ABS(b_r)) / ALWAN_CORE_LITERAL(3.0);

    /* Hue with 4-way sector branching via nested ALWAN_CORE_SELECT */
    /* atan(g_b / r_g) as r_g -> 0. The limit is +-pi/2 by the sign of g_b when
     * r_g approaches from above, and the opposite when it approaches from
     * below, because the quotient changes sign with the denominator. The sector
     * select further down keys off r_g >= 0, so substituting the r_g > 0 limit
     * for a small negative r_g put H at 5pi/3, outside the model's [-pi, pi],
     * and the inverse diverged from there. */
    const ALWAN_CORE_T half_pi = ALWAN_CORE_PI / ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T atan_limit = ALWAN_CORE_SELECT(g_b >= ALWAN_CORE_LITERAL(0.0), half_pi, -half_pi);
    atan_limit = ALWAN_CORE_SELECT(r_g >= ALWAN_CORE_LITERAL(0.0), atan_limit, -atan_limit);
    ALWAN_CORE_T h_temp = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(r_g) < ALWAN_CORE_EPSILON,
                                       atan_limit,
                                       ALWAN_CORE_ATAN(g_b / r_g));
    ALWAN_CORE_T two_h_3 = ALWAN_CORE_LITERAL(2.0) * h_temp / ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T four_h_3 = ALWAN_CORE_LITERAL(4.0) * h_temp / ALWAN_CORE_LITERAL(3.0);

    /* H based on signs of r_g and g_b */
    ALWAN_CORE_T H_rg_pos_gb_pos = two_h_3;
    ALWAN_CORE_T H_rg_pos_gb_neg = four_h_3;
    ALWAN_CORE_T H_rg_neg_gb_pos = ALWAN_CORE_PI + four_h_3;
    ALWAN_CORE_T H_rg_neg_gb_neg = two_h_3 - ALWAN_CORE_PI;

    ALWAN_CORE_T H_rg_pos = ALWAN_CORE_SELECT(g_b >= ALWAN_CORE_LITERAL(0.0), H_rg_pos_gb_pos, H_rg_pos_gb_neg);
    ALWAN_CORE_T H_rg_neg = ALWAN_CORE_SELECT(g_b >= ALWAN_CORE_LITERAL(0.0), H_rg_neg_gb_pos, H_rg_neg_gb_neg);
    ALWAN_CORE_T H = ALWAN_CORE_SELECT(C > ALWAN_CORE_EPSILON,
                                  ALWAN_CORE_SELECT(r_g >= ALWAN_CORE_LITERAL(0.0), H_rg_pos, H_rg_neg),
                                  ALWAN_CORE_LITERAL(0.0));

    result.H = H;
    result.C = C;
    result.L = L;
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hcl_to_rgb)(ALWAN_CORE_HCL hcl) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T H = hcl.H;
    ALWAN_CORE_T C = hcl.C;
    ALWAN_CORE_T L = hcl.L;

    const ALWAN_CORE_T gamma = ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T Y_0 = ALWAN_CORE_LITERAL(100.0);

    /* Compute Q, Min, Max */
    ALWAN_CORE_T Q = ALWAN_CORE_SELECT(L > ALWAN_CORE_EPSILON,
        ALWAN_CORE_EXP((ALWAN_CORE_LITERAL(1.0) - (ALWAN_CORE_LITERAL(3.0) * C) / (ALWAN_CORE_LITERAL(4.0) * L)) * gamma / Y_0),
        ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T denom = ALWAN_CORE_LITERAL(4.0) * Q - ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_T Min = ALWAN_CORE_SELECT(L > ALWAN_CORE_EPSILON,
        ALWAN_CORE_SELECT(ALWAN_CORE_ABS(denom) > ALWAN_CORE_EPSILON,
            (ALWAN_CORE_LITERAL(4.0) * L - ALWAN_CORE_LITERAL(3.0) * C) / denom,
            ALWAN_CORE_LITERAL(0.0)),
        ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T Max = ALWAN_CORE_SELECT(L > ALWAN_CORE_EPSILON,
        ALWAN_CORE_SELECT(Q > ALWAN_CORE_EPSILON,
            Min + (ALWAN_CORE_LITERAL(3.0) * C) / (ALWAN_CORE_LITERAL(2.0) * Q),
            ALWAN_CORE_LITERAL(0.0)),
        ALWAN_CORE_LITERAL(0.0));

    /* Sector boundaries */
    const ALWAN_CORE_T r_p60 = ALWAN_CORE_PI / ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T r_p120 = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T r_n60 = -ALWAN_CORE_PI / ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T r_n120 = -ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(3.0);

    /* Sector 0: [0, 60) */
    ALWAN_CORE_T tan0 = ALWAN_CORE_TAN(ALWAN_CORE_LITERAL(3.0) * H / ALWAN_CORE_LITERAL(2.0));
    ALWAN_CORE_T r0 = Max;
    ALWAN_CORE_T g0 = (Max * tan0 + Min) / (ALWAN_CORE_LITERAL(1.0) + tan0);
    ALWAN_CORE_T b0 = Min;

    /* Sector 1: [60, 120) */
    ALWAN_CORE_T tan1 = ALWAN_CORE_TAN(ALWAN_CORE_LITERAL(3.0) * (H - ALWAN_CORE_PI) / ALWAN_CORE_LITERAL(4.0));
    ALWAN_CORE_T r1 = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(tan1) > ALWAN_CORE_EPSILON,
        (Max * (ALWAN_CORE_LITERAL(1.0) + tan1) - Min) / tan1, Max);
    ALWAN_CORE_T g1 = Max;
    ALWAN_CORE_T b1 = Min;

    /* Sector 2: [120, 180] */
    ALWAN_CORE_T tan2 = ALWAN_CORE_TAN(ALWAN_CORE_LITERAL(3.0) * (H - ALWAN_CORE_PI) / ALWAN_CORE_LITERAL(4.0));
    ALWAN_CORE_T r2 = Min;
    ALWAN_CORE_T g2 = Max;
    ALWAN_CORE_T b2 = Max * (ALWAN_CORE_LITERAL(1.0) + tan2) - Min * tan2;

    /* Sector 3: [-60, 0) */
    ALWAN_CORE_T tan3 = ALWAN_CORE_TAN(ALWAN_CORE_LITERAL(3.0) * H / ALWAN_CORE_LITERAL(4.0));
    ALWAN_CORE_T r3 = Max;
    ALWAN_CORE_T g3 = Min;
    ALWAN_CORE_T b3 = Min * (ALWAN_CORE_LITERAL(1.0) + tan3) - Max * tan3;

    /* Sector 4: [-120, -60) */
    ALWAN_CORE_T tan4 = ALWAN_CORE_TAN(ALWAN_CORE_LITERAL(3.0) * H / ALWAN_CORE_LITERAL(4.0));
    ALWAN_CORE_T r4 = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(tan4) > ALWAN_CORE_EPSILON,
        (Min * (ALWAN_CORE_LITERAL(1.0) + tan4) - Max) / tan4, Min);
    ALWAN_CORE_T g4 = Min;
    ALWAN_CORE_T b4 = Max;

    /* Sector 5: [-180, -120) */
    ALWAN_CORE_T tan5 = ALWAN_CORE_TAN(ALWAN_CORE_LITERAL(3.0) * (H + ALWAN_CORE_PI) / ALWAN_CORE_LITERAL(2.0));
    ALWAN_CORE_T r5 = Min;
    ALWAN_CORE_T g5 = (Min * tan5 + Max) / (ALWAN_CORE_LITERAL(1.0) + tan5);
    ALWAN_CORE_T b5 = Max;

    /* Select sector via nested ALWAN_CORE_SELECT */
    ALWAN_CORE_T r_45 = ALWAN_CORE_SELECT(H >= r_n120, r4, r5);
    ALWAN_CORE_T g_45 = ALWAN_CORE_SELECT(H >= r_n120, g4, g5);
    ALWAN_CORE_T b_45 = ALWAN_CORE_SELECT(H >= r_n120, b4, b5);

    ALWAN_CORE_T r_345 = ALWAN_CORE_SELECT(H >= r_n60, r3, r_45);
    ALWAN_CORE_T g_345 = ALWAN_CORE_SELECT(H >= r_n60, g3, g_45);
    ALWAN_CORE_T b_345 = ALWAN_CORE_SELECT(H >= r_n60, b3, b_45);

    ALWAN_CORE_T r_12 = ALWAN_CORE_SELECT(H < r_p120, r1, r2);
    ALWAN_CORE_T g_12 = ALWAN_CORE_SELECT(H < r_p120, g1, g2);
    ALWAN_CORE_T b_12 = ALWAN_CORE_SELECT(H < r_p120, b1, b2);

    ALWAN_CORE_T r_012 = ALWAN_CORE_SELECT(H < r_p60, r0, r_12);
    ALWAN_CORE_T g_012 = ALWAN_CORE_SELECT(H < r_p60, g0, g_12);
    ALWAN_CORE_T b_012 = ALWAN_CORE_SELECT(H < r_p60, b0, b_12);

    result.r = ALWAN_CORE_SELECT(H >= ALWAN_CORE_LITERAL(0.0), r_012, r_345);
    result.g = ALWAN_CORE_SELECT(H >= ALWAN_CORE_LITERAL(0.0), g_012, g_345);
    result.b = ALWAN_CORE_SELECT(H >= ALWAN_CORE_LITERAL(0.0), b_012, b_345);

    return result;
}

/* ================================================================
 * IHLS (Improved HLS - Hanbury 2003)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_IHLS ALWAN_CORE_FNVLIT(alwan_rgb_to_ihls)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_IHLS result;

    ALWAN_CORE_T max_val = ALWAN_CORE_MAX3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T min_val = ALWAN_CORE_MIN3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T delta = max_val - min_val;

    /* Y, C1, C2 via matrix multiply */
    ALWAN_CORE_VEC3 in_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_IHLS_RGB_TO_YC1C2), in_v);
    ALWAN_CORE_T Y  = out_v.v[0];
    ALWAN_CORE_T C_1 = out_v.v[1];
    ALWAN_CORE_T C_2 = out_v.v[2];

    ALWAN_CORE_T C_mag = ALWAN_CORE_SQRT(C_1 * C_1 + C_2 * C_2);

    /* Hue via ACOS with clamp */
    ALWAN_CORE_T C_1_C = ALWAN_CORE_SELECT(C_mag > ALWAN_CORE_EPSILON, C_1 / C_mag, ALWAN_CORE_LITERAL(1.0));
    C_1_C = ALWAN_CORE_SELECT(C_1_C > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0),
             ALWAN_CORE_SELECT(C_1_C < ALWAN_CORE_LITERAL(-1.0), ALWAN_CORE_LITERAL(-1.0), C_1_C));
    ALWAN_CORE_T H_temp = ALWAN_CORE_ACOS(C_1_C);

    /* Adjust based on C_2 sign */
    ALWAN_CORE_T H = ALWAN_CORE_SELECT(C_mag > ALWAN_CORE_EPSILON,
                      ALWAN_CORE_SELECT(C_2 <= ALWAN_CORE_LITERAL(0.0), H_temp, ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI - H_temp),
                      ALWAN_CORE_LITERAL(0.0));

    result.H = H;
    result.L = Y;
    result.S = delta;
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_ihls_to_rgb)(ALWAN_CORE_IHLS ihls) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T H = ihls.H;
    ALWAN_CORE_T Y = ihls.L;
    ALWAN_CORE_T S = ihls.S;

    const ALWAN_CORE_T pi_3 = ALWAN_CORE_PI / ALWAN_CORE_LITERAL(3.0);
    const ALWAN_CORE_T two_pi_3 = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(3.0);

    ALWAN_CORE_T k = ALWAN_CORE_FLOOR(H / pi_3);
    ALWAN_CORE_T H_s = H - k * pi_3;
    ALWAN_CORE_T sin_val = ALWAN_CORE_SIN(two_pi_3 - H_s);
    ALWAN_CORE_T C_mag = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(sin_val) < ALWAN_CORE_EPSILON,
                                      ALWAN_CORE_LITERAL(0.0),
                                      (ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(3.0)) * S) / (ALWAN_CORE_LITERAL(2.0) * sin_val));

    ALWAN_CORE_T C_1 = C_mag * ALWAN_CORE_COS(H);
    ALWAN_CORE_T C_2 = -C_mag * ALWAN_CORE_SIN(H);

    ALWAN_CORE_VEC3 in_v = {{Y, C_1, C_2}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ALWAN_EXT_IHLS_YC1C2_TO_RGB), in_v);
    result.r = out_v.v[0];
    result.g = out_v.v[1];
    result.b = out_v.v[2];

    return result;
}

/* ================================================================
 * HLC -- Cylindrical CIELAB with (H, L, C) ordering
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_HLC ALWAN_CORE_FNVLIT(alwan_lch_to_hlc)(ALWAN_CORE_LCH lch) {
    ALWAN_CORE_HLC result;
    result.H = lch.h;
    result.L = lch.L;
    result.C = lch.C;
    return result;
}

ALWAN_INLINE ALWAN_CORE_LCH ALWAN_CORE_FNVLIT(alwan_hlc_to_lch)(ALWAN_CORE_HLC hlc) {
    ALWAN_CORE_LCH result;
    result.L = hlc.L;
    result.C = hlc.C;
    result.h = hlc.H;
    return result;
}

/* ================================================================
 * Cubehelix Color Space (Green 2011)
 * ================================================================ */

/* Cubehelix rotation matrix coefficients (Rec.601 luminance-neutral) */
#define ALWAN_CUBEHELIX_A  ALWAN_CORE_LITERAL(-0.14861)
#define ALWAN_CUBEHELIX_B  ALWAN_CORE_LITERAL( 1.78277)
#define ALWAN_CUBEHELIX_C  ALWAN_CORE_LITERAL(-0.29227)
#define ALWAN_CUBEHELIX_D  ALWAN_CORE_LITERAL(-0.90649)
#define ALWAN_CUBEHELIX_E  ALWAN_CORE_LITERAL( 1.97294)

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_cubehelix_to_rgb)(ALWAN_CORE_CUBEHELIX ch) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T angle = (ch.h + ALWAN_CORE_LITERAL(120.0)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T amp = ch.s * ch.l * (ALWAN_CORE_ONE - ch.l);
    ALWAN_CORE_T cos_a = ALWAN_CORE_COS(angle);
    ALWAN_CORE_T sin_a = ALWAN_CORE_SIN(angle);

    result.r = ch.l + amp * (ALWAN_CUBEHELIX_A * cos_a + ALWAN_CUBEHELIX_B * sin_a);
    result.g = ch.l + amp * (ALWAN_CUBEHELIX_C * cos_a + ALWAN_CUBEHELIX_D * sin_a);
    result.b = ch.l + amp * (ALWAN_CUBEHELIX_E * cos_a);
    return result;
}

ALWAN_INLINE ALWAN_CORE_CUBEHELIX ALWAN_CORE_FNVLIT(alwan_rgb_to_cubehelix)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_CUBEHELIX result;

    /* Derived inverse constants */
    const ALWAN_CORE_T ED    = ALWAN_CUBEHELIX_E * ALWAN_CUBEHELIX_D;
    const ALWAN_CORE_T EB    = ALWAN_CUBEHELIX_E * ALWAN_CUBEHELIX_B;
    const ALWAN_CORE_T BC_DA = ALWAN_CUBEHELIX_B * ALWAN_CUBEHELIX_C
                             - ALWAN_CUBEHELIX_D * ALWAN_CUBEHELIX_A;

    /* Recover lightness (luminance under Rec.601 via the matrix inverse) */
    result.l = (BC_DA * rgb.b + ED * rgb.r - EB * rgb.g)
             / (BC_DA + ED - EB);

    /* Recover cosine and sine projections */
    ALWAN_CORE_T bl = rgb.b - result.l;
    ALWAN_CORE_T k  = (ALWAN_CUBEHELIX_E * (rgb.g - result.l)
                      - ALWAN_CUBEHELIX_C * bl) / ALWAN_CUBEHELIX_D;

    /* Saturation and hue */
    ALWAN_CORE_T chroma = ALWAN_CORE_SQRT(k * k + bl * bl);
    ALWAN_CORE_T denom  = ALWAN_CUBEHELIX_E * result.l * (ALWAN_CORE_ONE - result.l);

    result.s = ALWAN_CORE_SELECT(denom > ALWAN_CORE_LITERAL(1e-10),
                            chroma / denom, ALWAN_CORE_ZERO);

    result.h = ALWAN_CORE_ATAN2(k, bl) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI
             - ALWAN_CORE_LITERAL(120.0);
    result.h = ALWAN_CORE_SELECT(result.h < ALWAN_CORE_ZERO,
                            result.h + ALWAN_CORE_LITERAL(360.0), result.h);

    return result;
}

#undef ALWAN_CUBEHELIX_A
#undef ALWAN_CUBEHELIX_B
#undef ALWAN_CUBEHELIX_C
#undef ALWAN_CUBEHELIX_D
#undef ALWAN_CUBEHELIX_E

/* ================================================================
 * HSLuv / HPLuv / Okhsl / Okhsv -- Perceptual HSL/HSV spaces
 * ================================================================ */

/* sRGB D65 XYZ-to-linear-RGB matrix (IEC 61966-2-1) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const ALWAN_CORE_T ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[3][3] = {
    { 3.24096994190452134377, -1.53738317757009345794, -0.49861076029300328366},
    {-0.96924363628087982613,  1.87596750150772066772,  0.04155505740717561247},
    { 0.05563007969699360846, -0.20397695888897656435,  1.05697151424287856072}
};
/* sRGB D65 linear-RGB-to-XYZ matrix */
static const ALWAN_CORE_T ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[3][3] = {
    { 0.41239079926595948129,  0.35758433938387796373,  0.18048078840183428751},
    { 0.21263900587151035754,  0.71516867876775592746,  0.07219231536073371500},
    { 0.01933081871559185069,  0.11919477979462598791,  0.95053215224966058086}
};
ALWAN_DIAG_POP

/* L* -> Y */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hsluv_l2y)(ALWAN_CORE_T L) {
    /* Exact inverse of alwan_hsluv_y2l: Y = ((L+16)/116)^3 above the linear
     * segment, and 116^3 = 1560896. The upper branch used to return a literal
     * 1.0 for every L > 8, so every non-dark lightness decoded to white.
     * alwan_hsluv_get_bounds already computed (L+16)^3 / 1560896 inline. */
    const ALWAN_CORE_T tl = L + ALWAN_CORE_LITERAL(16.0);
    return ALWAN_CORE_SELECT(L <= ALWAN_CORE_LITERAL(8.0),
        L / ALWAN_CORE_LITERAL(903.29629629629629629630),
        (tl * tl * tl) / ALWAN_CORE_LITERAL(1560896.0));
}

/* Y -> L* */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hsluv_y2l)(ALWAN_CORE_T Y) {
    return ALWAN_CORE_SELECT(Y <= ALWAN_CORE_LITERAL(0.00885645167903563082),
        Y * ALWAN_CORE_LITERAL(903.29629629629629629630),
        ALWAN_CORE_LITERAL(116.0) * ALWAN_CORE_CBRT(Y) - ALWAN_CORE_LITERAL(16.0));
}

typedef struct { ALWAN_CORE_T a, b; } ALWAN_CORE_FNLIT(alwan_hsluv_bound);

ALWAN_INLINE void ALWAN_CORE_FNLIT(alwan_hsluv_get_bounds)(ALWAN_CORE_T L, ALWAN_CORE_FNLIT(alwan_hsluv_bound) bounds[6]) {
    ALWAN_CORE_T tl = L + ALWAN_CORE_LITERAL(16.0);
    ALWAN_CORE_T sub1 = (tl * tl * tl) / ALWAN_CORE_LITERAL(1560896.0);
    ALWAN_CORE_T sub2 = ALWAN_CORE_SELECT(sub1 > ALWAN_CORE_LITERAL(0.00885645167903563082), sub1,
                                      L / ALWAN_CORE_LITERAL(903.29629629629629629630));
    int ch, t;
    for (ch = 0; ch < 3; ch++) {
        ALWAN_CORE_T m1 = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[ch][0];
        ALWAN_CORE_T m2 = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[ch][1];
        ALWAN_CORE_T m3 = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[ch][2];
        for (t = 0; t < 2; t++) {
            ALWAN_CORE_T top1 = (ALWAN_CORE_LITERAL(284517.0) * m1
                               - ALWAN_CORE_LITERAL(94839.0) * m3) * sub2;
            ALWAN_CORE_T top2 = (ALWAN_CORE_LITERAL(838422.0) * m3
                               + ALWAN_CORE_LITERAL(769860.0) * m2
                               + ALWAN_CORE_LITERAL(731718.0) * m1) * L * sub2
                               - ALWAN_CORE_LITERAL(769860.0) * (ALWAN_CORE_T)t * L;
            ALWAN_CORE_T bottom = (ALWAN_CORE_LITERAL(632260.0) * m3
                                 - ALWAN_CORE_LITERAL(126452.0) * m2) * sub2
                                 + ALWAN_CORE_LITERAL(126452.0) * (ALWAN_CORE_T)t;
            bounds[ch * 2 + t].a = top1 / bottom;
            bounds[ch * 2 + t].b = top2 / bottom;
        }
    }
}

/* Maximum chroma for a given (L, H) in sRGB gamut */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hsluv_max_chroma_lh)(ALWAN_CORE_T L, ALWAN_CORE_T H) {
    ALWAN_CORE_T hrad = H * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T sin_h = ALWAN_CORE_SIN(hrad);
    ALWAN_CORE_T cos_h = ALWAN_CORE_COS(hrad);
    ALWAN_CORE_T min_len = ALWAN_CORE_LITERAL(1e30);
    ALWAN_CORE_FNLIT(alwan_hsluv_bound) bounds[6];
    int i;
    ALWAN_CORE_FNLIT(alwan_hsluv_get_bounds)(L, bounds);
    for (i = 0; i < 6; i++) {
        ALWAN_CORE_T denom = sin_h - bounds[i].a * cos_h;
        ALWAN_CORE_T len = bounds[i].b / denom;
        if (len >= ALWAN_CORE_ZERO && len < min_len) min_len = len;
    }
    return min_len;
}

/* Minimum chroma across all hues for a given L (HPLuv) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_hsluv_max_safe_chroma)(ALWAN_CORE_T L) {
    ALWAN_CORE_T min_dist_sq = ALWAN_CORE_LITERAL(1e30);
    ALWAN_CORE_FNLIT(alwan_hsluv_bound) bounds[6];
    int i;
    ALWAN_CORE_FNLIT(alwan_hsluv_get_bounds)(L, bounds);
    for (i = 0; i < 6; i++) {
        ALWAN_CORE_T m1 = bounds[i].a;
        ALWAN_CORE_T b1 = bounds[i].b;
        ALWAN_CORE_T x = b1 / (-ALWAN_CORE_ONE / m1 - m1);
        ALWAN_CORE_T dist = x * x + (b1 + x * m1) * (b1 + x * m1);
        if (dist < min_dist_sq) min_dist_sq = dist;
    }
    return ALWAN_CORE_SQRT(min_dist_sq);
}

/* Internal: LCHuv -> linear sRGB (no clamping) */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNLIT(alwan_hsluv_lchuv_to_rgb)(ALWAN_CORE_T L, ALWAN_CORE_T C, ALWAN_CORE_T H) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T h_rad, u, v, var_u, var_v, Y, X, Z;

    h_rad = H * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    u = C * ALWAN_CORE_COS(h_rad);
    v = C * ALWAN_CORE_SIN(h_rad);

    if (L < ALWAN_CORE_LITERAL(1e-8)) {
        result.r = ALWAN_CORE_ZERO; result.g = ALWAN_CORE_ZERO; result.b = ALWAN_CORE_ZERO;
        return result;
    }
    var_u = u / (ALWAN_CORE_LITERAL(13.0) * L) + ALWAN_CORE_LITERAL(0.19783000664283680764);
    var_v = v / (ALWAN_CORE_LITERAL(13.0) * L) + ALWAN_CORE_LITERAL(0.46831999493879100370);

    if (L <= ALWAN_CORE_LITERAL(8.0))
        Y = L / ALWAN_CORE_LITERAL(903.29629629629629629630);
    else {
        ALWAN_CORE_T fy = (L + ALWAN_CORE_LITERAL(16.0)) / ALWAN_CORE_LITERAL(116.0);
        Y = fy * fy * fy;
    }

    X = ALWAN_CORE_LITERAL(-9.0) * Y * var_u
      / ((var_u - ALWAN_CORE_LITERAL(4.0)) * var_v - var_u * var_v);
    Z = (ALWAN_CORE_LITERAL(9.0) * Y - ALWAN_CORE_LITERAL(15.0) * var_v * Y - var_v * X)
      / (ALWAN_CORE_LITERAL(3.0) * var_v);

    result.r = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[0][0] * X + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[0][1] * Y + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[0][2] * Z;
    result.g = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[1][0] * X + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[1][1] * Y + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[1][2] * Z;
    result.b = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[2][0] * X + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[2][1] * Y + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M)[2][2] * Z;
    return result;
}

/* Internal: sRGB -> LCHuv */
ALWAN_INLINE void ALWAN_CORE_FNLIT(alwan_hsluv_rgb_to_lchuv)(ALWAN_CORE_T *L_out, ALWAN_CORE_T *C_out,
                                             ALWAN_CORE_T *H_out, ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_T X = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[0][0] * rgb.r + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[0][1] * rgb.g + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[0][2] * rgb.b;
    ALWAN_CORE_T Y = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[1][0] * rgb.r + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[1][1] * rgb.g + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[1][2] * rgb.b;
    ALWAN_CORE_T Z = ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[2][0] * rgb.r + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[2][1] * rgb.g + ALWAN_CORE_FNLIT(ALWAN_HSLUV_M_INV)[2][2] * rgb.b;

    ALWAN_CORE_T denom = X + ALWAN_CORE_LITERAL(15.0) * Y + ALWAN_CORE_LITERAL(3.0) * Z;
    ALWAN_CORE_T u_prime, v_prime, L, u, v;

    if (denom < ALWAN_CORE_LITERAL(1e-10)) {
        *L_out = ALWAN_CORE_ZERO; *C_out = ALWAN_CORE_ZERO; *H_out = ALWAN_CORE_ZERO;
        return;
    }
    u_prime = ALWAN_CORE_LITERAL(4.0) * X / denom;
    v_prime = ALWAN_CORE_LITERAL(9.0) * Y / denom;

    L = ALWAN_CORE_FNLIT(alwan_hsluv_y2l)(Y);
    u = ALWAN_CORE_LITERAL(13.0) * L * (u_prime - ALWAN_CORE_LITERAL(0.19783000664283680764));
    v = ALWAN_CORE_LITERAL(13.0) * L * (v_prime - ALWAN_CORE_LITERAL(0.46831999493879100370));

    *L_out = L;
    *C_out = ALWAN_CORE_SQRT(u * u + v * v);
    *H_out = ALWAN_CORE_ATAN2(v, u) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    *H_out += ALWAN_CORE_SELECT(*H_out < ALWAN_CORE_ZERO, ALWAN_CORE_LITERAL(360.0), ALWAN_CORE_ZERO);
}

/* HSLuv -> sRGB (encoded) */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hsluv_to_srgb)(ALWAN_CORE_HSLUV hsluv) {
    ALWAN_CORE_T H = hsluv.h;
    ALWAN_CORE_T S = hsluv.s;
    ALWAN_CORE_T L = hsluv.l;
    ALWAN_CORE_T C;
    ALWAN_CORE_RGB lin;

    if (L > ALWAN_CORE_LITERAL(99.9999999)) {
        ALWAN_CORE_RGB result = {ALWAN_CORE_ONE, ALWAN_CORE_ONE, ALWAN_CORE_ONE};
        return result;
    }
    if (L < ALWAN_CORE_LITERAL(0.00000001)) {
        ALWAN_CORE_RGB result = {ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO};
        return result;
    }

    C = ALWAN_CORE_FNLIT(alwan_hsluv_max_chroma_lh)(L, H) / ALWAN_CORE_LITERAL(100.0) * S;
    lin = ALWAN_CORE_FNLIT(alwan_hsluv_lchuv_to_rgb)(L, C, H);

    {
        ALWAN_CORE_RGB result;
        result.r = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(lin.r);
        result.g = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(lin.g);
        result.b = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(lin.b);
        return result;
    }
}

/* sRGB (encoded) -> HSLuv */
ALWAN_INLINE ALWAN_CORE_HSLUV ALWAN_CORE_FNVLIT(alwan_srgb_to_hsluv)(ALWAN_CORE_RGB srgb) {
    ALWAN_CORE_HSLUV result;
    ALWAN_CORE_RGB lin;
    ALWAN_CORE_T L, C, H;

    lin.r = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.r);
    lin.g = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.g);
    lin.b = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.b);

    ALWAN_CORE_FNLIT(alwan_hsluv_rgb_to_lchuv)(&L, &C, &H, lin);

    if (L > ALWAN_CORE_LITERAL(99.9999999)) {
        result.h = H; result.s = ALWAN_CORE_ZERO; result.l = ALWAN_CORE_LITERAL(100.0);
        return result;
    }
    if (L < ALWAN_CORE_LITERAL(0.00000001)) {
        result.h = H; result.s = ALWAN_CORE_ZERO; result.l = ALWAN_CORE_ZERO;
        return result;
    }

    result.h = H;
    result.s = C / ALWAN_CORE_FNLIT(alwan_hsluv_max_chroma_lh)(L, H) * ALWAN_CORE_LITERAL(100.0);
    result.l = L;
    return result;
}

/* HPLuv -> sRGB (encoded) */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hpluv_to_srgb)(ALWAN_CORE_HPLUV hpluv) {
    ALWAN_CORE_T H = hpluv.h;
    ALWAN_CORE_T S = hpluv.s;
    ALWAN_CORE_T L = hpluv.l;
    ALWAN_CORE_T C;
    ALWAN_CORE_RGB lin;

    if (L > ALWAN_CORE_LITERAL(99.9999999)) {
        ALWAN_CORE_RGB result = {ALWAN_CORE_ONE, ALWAN_CORE_ONE, ALWAN_CORE_ONE};
        return result;
    }
    if (L < ALWAN_CORE_LITERAL(0.00000001)) {
        ALWAN_CORE_RGB result = {ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO};
        return result;
    }

    C = ALWAN_CORE_FNLIT(alwan_hsluv_max_safe_chroma)(L) / ALWAN_CORE_LITERAL(100.0) * S;
    lin = ALWAN_CORE_FNLIT(alwan_hsluv_lchuv_to_rgb)(L, C, H);

    {
        ALWAN_CORE_RGB result;
        result.r = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(lin.r);
        result.g = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(lin.g);
        result.b = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(lin.b);
        return result;
    }
}

/* sRGB (encoded) -> HPLuv */
ALWAN_INLINE ALWAN_CORE_HPLUV ALWAN_CORE_FNVLIT(alwan_srgb_to_hpluv)(ALWAN_CORE_RGB srgb) {
    ALWAN_CORE_HPLUV result;
    ALWAN_CORE_RGB lin;
    ALWAN_CORE_T L, C, H;

    lin.r = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.r);
    lin.g = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.g);
    lin.b = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.b);

    ALWAN_CORE_FNLIT(alwan_hsluv_rgb_to_lchuv)(&L, &C, &H, lin);

    if (L > ALWAN_CORE_LITERAL(99.9999999)) {
        result.h = H; result.s = ALWAN_CORE_ZERO; result.l = ALWAN_CORE_LITERAL(100.0);
        return result;
    }
    if (L < ALWAN_CORE_LITERAL(0.00000001)) {
        result.h = H; result.s = ALWAN_CORE_ZERO; result.l = ALWAN_CORE_ZERO;
        return result;
    }

    result.h = H;
    result.s = C / ALWAN_CORE_FNLIT(alwan_hsluv_max_safe_chroma)(L) * ALWAN_CORE_LITERAL(100.0);
    result.l = L;
    return result;
}

/* ================================================================
 * Okhsl / Okhsv -- Perceptual HSL/HSV in Oklab space
 * ================================================================ */

ALWAN_INLINE void ALWAN_CORE_FNLIT(alwan_ok_srgb_to_lab)(ALWAN_CORE_T *L, ALWAN_CORE_T *a, ALWAN_CORE_T *b,
                                         ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T bl_in) {
    ALWAN_CORE_VEC3 rgb; rgb.v[0] = r; rgb.v[1] = g; rgb.v[2] = bl_in;
    ALWAN_CORE_VEC3 oklab = ALWAN_CORE_FNVLIT(gamut_linear_srgb_to_oklab)(rgb);
    *L = oklab.v[0]; *a = oklab.v[1]; *b = oklab.v[2];
}

ALWAN_INLINE void ALWAN_CORE_FNLIT(alwan_ok_lab_to_srgb)(ALWAN_CORE_T *r, ALWAN_CORE_T *g, ALWAN_CORE_T *b,
                                         ALWAN_CORE_T L, ALWAN_CORE_T a, ALWAN_CORE_T bl_in) {
    ALWAN_CORE_VEC3 oklab; oklab.v[0] = L; oklab.v[1] = a; oklab.v[2] = bl_in;
    ALWAN_CORE_VEC3 rgb = ALWAN_CORE_FNVLIT(gamut_oklab_to_linear_srgb)(oklab);
    *r = rgb.v[0]; *g = rgb.v[1]; *b = rgb.v[2];
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_ok_toe)(ALWAN_CORE_T x) {
    const ALWAN_CORE_T k1 = ALWAN_CORE_LITERAL(0.206);
    const ALWAN_CORE_T k2 = ALWAN_CORE_LITERAL(0.03);
    const ALWAN_CORE_T k3 = (ALWAN_CORE_ONE + k1) / (ALWAN_CORE_ONE + k2);
    ALWAN_CORE_T t = k3 * x - k1;
    return ALWAN_CORE_LITERAL(0.5) * (t + ALWAN_CORE_SQRT(t * t
         + ALWAN_CORE_LITERAL(4.0) * k2 * k3 * x));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_ok_toe_inv)(ALWAN_CORE_T x) {
    const ALWAN_CORE_T k1 = ALWAN_CORE_LITERAL(0.206);
    const ALWAN_CORE_T k2 = ALWAN_CORE_LITERAL(0.03);
    const ALWAN_CORE_T k3 = (ALWAN_CORE_ONE + k1) / (ALWAN_CORE_ONE + k2);
    return (x * x + k1 * x) / (k3 * (x + k2));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_ok_compute_max_saturation)(ALWAN_CORE_T a_, ALWAN_CORE_T b_) {
    ALWAN_CORE_T k0, k1, k2, k3, k4;
    ALWAN_CORE_T wl, wm, ws;

    if (-ALWAN_CORE_LITERAL(1.88170328) * a_ - ALWAN_CORE_LITERAL(0.80936493) * b_ > ALWAN_CORE_ONE) {
        k0 = ALWAN_CORE_LITERAL( 1.19086277); k1 = ALWAN_CORE_LITERAL( 1.76576728);
        k2 = ALWAN_CORE_LITERAL( 0.59662641); k3 = ALWAN_CORE_LITERAL( 0.75515197);
        k4 = ALWAN_CORE_LITERAL( 0.56771245);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[0]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[1];
        ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[2];
    } else if (ALWAN_CORE_LITERAL(1.81444104) * a_ - ALWAN_CORE_LITERAL(1.19445276) * b_ > ALWAN_CORE_ONE) {
        k0 = ALWAN_CORE_LITERAL( 0.73956515); k1 = ALWAN_CORE_LITERAL(-0.45954404);
        k2 = ALWAN_CORE_LITERAL( 0.08285427); k3 = ALWAN_CORE_LITERAL( 0.12541070);
        k4 = ALWAN_CORE_LITERAL( 0.14503204);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[3]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[4];
        ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[5];
    } else {
        k0 = ALWAN_CORE_LITERAL( 1.35733652); k1 = ALWAN_CORE_LITERAL(-0.00915799);
        k2 = ALWAN_CORE_LITERAL(-1.15130210); k3 = ALWAN_CORE_LITERAL(-0.50559606);
        k4 = ALWAN_CORE_LITERAL( 0.00692167);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[6]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[7];
        ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[8];
    }

    ALWAN_CORE_T S = k0 + k1 * a_ + k2 * b_ + k3 * a_ * a_ + k4 * a_ * b_;

    {
        ALWAN_CORE_T k_l = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[1] * a_ + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[2] * b_;
        ALWAN_CORE_T k_m = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[4] * a_ + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[5] * b_;
        ALWAN_CORE_T k_s = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[7] * a_ + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[8] * b_;

        ALWAN_CORE_T l_ = ALWAN_CORE_ONE + S * k_l;
        ALWAN_CORE_T m_ = ALWAN_CORE_ONE + S * k_m;
        ALWAN_CORE_T s_ = ALWAN_CORE_ONE + S * k_s;
        ALWAN_CORE_T l = l_ * l_ * l_;
        ALWAN_CORE_T m = m_ * m_ * m_;
        ALWAN_CORE_T s = s_ * s_ * s_;

        ALWAN_CORE_T l_dS  = ALWAN_CORE_LITERAL(3.0) * k_l * l_ * l_;
        ALWAN_CORE_T m_dS  = ALWAN_CORE_LITERAL(3.0) * k_m * m_ * m_;
        ALWAN_CORE_T s_dS  = ALWAN_CORE_LITERAL(3.0) * k_s * s_ * s_;
        ALWAN_CORE_T l_dS2 = ALWAN_CORE_LITERAL(6.0) * k_l * k_l * l_;
        ALWAN_CORE_T m_dS2 = ALWAN_CORE_LITERAL(6.0) * k_m * k_m * m_;
        ALWAN_CORE_T s_dS2 = ALWAN_CORE_LITERAL(6.0) * k_s * k_s * s_;

        ALWAN_CORE_T f  = wl * l  + wm * m  + ws * s;
        ALWAN_CORE_T f1 = wl * l_dS  + wm * m_dS  + ws * s_dS;
        ALWAN_CORE_T f2 = wl * l_dS2 + wm * m_dS2 + ws * s_dS2;
        S = S - f * f1 / (f1 * f1 - ALWAN_CORE_LITERAL(0.5) * f * f2);
    }
    return S;
}

ALWAN_INLINE void ALWAN_CORE_FNLIT(alwan_ok_find_cusp)(ALWAN_CORE_T *L_cusp, ALWAN_CORE_T *C_cusp,
                                       ALWAN_CORE_T a_, ALWAN_CORE_T b_) {
    ALWAN_CORE_T S_cusp = ALWAN_CORE_FNLIT(alwan_ok_compute_max_saturation)(a_, b_);
    ALWAN_CORE_T r, g, b;
    ALWAN_CORE_FNLIT(alwan_ok_lab_to_srgb)(&r, &g, &b, ALWAN_CORE_ONE, S_cusp * a_, S_cusp * b_);
    ALWAN_CORE_T max_rgb = ALWAN_CORE_MAX3(r, g, b);
    *L_cusp = ALWAN_CORE_CBRT(ALWAN_CORE_ONE / max_rgb);
    *C_cusp = (*L_cusp) * S_cusp;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_ok_find_gamut_intersection)(ALWAN_CORE_T a_, ALWAN_CORE_T b_,
                                                            ALWAN_CORE_T L1, ALWAN_CORE_T C1,
                                                            ALWAN_CORE_T L0,
                                                            ALWAN_CORE_T cusp_L, ALWAN_CORE_T cusp_C) {
    ALWAN_CORE_T t;
    if (((L1 - L0) * cusp_C - (cusp_L - L0) * C1) <= ALWAN_CORE_ZERO) {
        t = cusp_C * L0 / (C1 * cusp_L + cusp_C * (L0 - L1));
    } else {
        t = cusp_C * (L0 - ALWAN_CORE_ONE)
          / (C1 * (cusp_L - ALWAN_CORE_ONE) + cusp_C * (L0 - L1));

        {
            ALWAN_CORE_T dL = L1 - L0;
            ALWAN_CORE_T dC = C1;
            ALWAN_CORE_T k_l = ALWAN_CORE_LITERAL( 0.3963377774) * a_ + ALWAN_CORE_LITERAL(0.2158037573) * b_;
            ALWAN_CORE_T k_m = ALWAN_CORE_LITERAL(-0.1055613458) * a_ - ALWAN_CORE_LITERAL(0.0638541728) * b_;
            ALWAN_CORE_T k_s = ALWAN_CORE_LITERAL(-0.0894841775) * a_ - ALWAN_CORE_LITERAL(1.2914855480) * b_;

            ALWAN_CORE_T l_dt = dL + dC * k_l;
            ALWAN_CORE_T m_dt = dL + dC * k_m;
            ALWAN_CORE_T s_dt = dL + dC * k_s;

            ALWAN_CORE_T L_t = L0 * (ALWAN_CORE_ONE - t) + t * L1;
            ALWAN_CORE_T C_t = t * C1;
            ALWAN_CORE_T l_ = L_t + C_t * k_l;
            ALWAN_CORE_T m_ = L_t + C_t * k_m;
            ALWAN_CORE_T s_ = L_t + C_t * k_s;
            ALWAN_CORE_T l = l_ * l_ * l_;
            ALWAN_CORE_T m = m_ * m_ * m_;
            ALWAN_CORE_T s = s_ * s_ * s_;
            ALWAN_CORE_T ldt = ALWAN_CORE_LITERAL(3.0) * l_dt * l_ * l_;
            ALWAN_CORE_T mdt = ALWAN_CORE_LITERAL(3.0) * m_dt * m_ * m_;
            ALWAN_CORE_T sdt = ALWAN_CORE_LITERAL(3.0) * s_dt * s_ * s_;
            ALWAN_CORE_T ldt2 = ALWAN_CORE_LITERAL(6.0) * l_dt * l_dt * l_;
            ALWAN_CORE_T mdt2 = ALWAN_CORE_LITERAL(6.0) * m_dt * m_dt * m_;
            ALWAN_CORE_T sdt2 = ALWAN_CORE_LITERAL(6.0) * s_dt * s_dt * s_;

            ALWAN_CORE_T rgb_r  =  ALWAN_CORE_LITERAL(4.0767416621) * l  - ALWAN_CORE_LITERAL(3.3077115913) * m  + ALWAN_CORE_LITERAL(0.2309699292) * s  - ALWAN_CORE_ONE;
            ALWAN_CORE_T rgb_r1 =  ALWAN_CORE_LITERAL(4.0767416621) * ldt - ALWAN_CORE_LITERAL(3.3077115913) * mdt + ALWAN_CORE_LITERAL(0.2309699292) * sdt;
            ALWAN_CORE_T rgb_r2 =  ALWAN_CORE_LITERAL(4.0767416621) * ldt2 - ALWAN_CORE_LITERAL(3.3077115913) * mdt2 + ALWAN_CORE_LITERAL(0.2309699292) * sdt2;
            ALWAN_CORE_T u_r = rgb_r1 / (rgb_r1 * rgb_r1 - ALWAN_CORE_LITERAL(0.5) * rgb_r * rgb_r2);
            ALWAN_CORE_T t_r = -rgb_r * u_r;

            ALWAN_CORE_T rgb_g  = -ALWAN_CORE_LITERAL(1.2684380046) * l  + ALWAN_CORE_LITERAL(2.6097574011) * m  - ALWAN_CORE_LITERAL(0.3413193965) * s  - ALWAN_CORE_ONE;
            ALWAN_CORE_T rgb_g1 = -ALWAN_CORE_LITERAL(1.2684380046) * ldt + ALWAN_CORE_LITERAL(2.6097574011) * mdt - ALWAN_CORE_LITERAL(0.3413193965) * sdt;
            ALWAN_CORE_T rgb_g2 = -ALWAN_CORE_LITERAL(1.2684380046) * ldt2 + ALWAN_CORE_LITERAL(2.6097574011) * mdt2 - ALWAN_CORE_LITERAL(0.3413193965) * sdt2;
            ALWAN_CORE_T u_g = rgb_g1 / (rgb_g1 * rgb_g1 - ALWAN_CORE_LITERAL(0.5) * rgb_g * rgb_g2);
            ALWAN_CORE_T t_g = -rgb_g * u_g;

            ALWAN_CORE_T rgb_b  = -ALWAN_CORE_LITERAL(0.0041960863) * l  - ALWAN_CORE_LITERAL(0.7034186147) * m  + ALWAN_CORE_LITERAL(1.7076147010) * s  - ALWAN_CORE_ONE;
            ALWAN_CORE_T rgb_b1 = -ALWAN_CORE_LITERAL(0.0041960863) * ldt - ALWAN_CORE_LITERAL(0.7034186147) * mdt + ALWAN_CORE_LITERAL(1.7076147010) * sdt;
            ALWAN_CORE_T rgb_b2 = -ALWAN_CORE_LITERAL(0.0041960863) * ldt2 - ALWAN_CORE_LITERAL(0.7034186147) * mdt2 + ALWAN_CORE_LITERAL(1.7076147010) * sdt2;
            ALWAN_CORE_T u_b = rgb_b1 / (rgb_b1 * rgb_b1 - ALWAN_CORE_LITERAL(0.5) * rgb_b * rgb_b2);
            ALWAN_CORE_T t_b = -rgb_b * u_b;

            t_r = ALWAN_CORE_SELECT(u_r >= ALWAN_CORE_ZERO, t_r, ALWAN_CORE_LITERAL(1e30));
            t_g = ALWAN_CORE_SELECT(u_g >= ALWAN_CORE_ZERO, t_g, ALWAN_CORE_LITERAL(1e30));
            t_b = ALWAN_CORE_SELECT(u_b >= ALWAN_CORE_ZERO, t_b, ALWAN_CORE_LITERAL(1e30));
            t += ALWAN_CORE_MIN3(t_r, t_g, t_b);
        }
    }
    return t;
}

ALWAN_INLINE void ALWAN_CORE_FNLIT(alwan_ok_get_ST_mid)(ALWAN_CORE_T a_, ALWAN_CORE_T b_,
                                        ALWAN_CORE_T *S_mid, ALWAN_CORE_T *T_mid) {
    *S_mid = ALWAN_CORE_LITERAL(0.11516993) + ALWAN_CORE_ONE / (
        ALWAN_CORE_LITERAL(7.44778970) + ALWAN_CORE_LITERAL(4.15901240) * b_
        + a_ * (ALWAN_CORE_LITERAL(-2.19557347) + ALWAN_CORE_LITERAL(1.75198401) * b_
        + a_ * (ALWAN_CORE_LITERAL(-2.13704948) - ALWAN_CORE_LITERAL(10.02301043) * b_
        + a_ * (ALWAN_CORE_LITERAL(-4.24894561) + ALWAN_CORE_LITERAL(5.38770819) * b_
        + ALWAN_CORE_LITERAL(4.69891013) * a_))));

    *T_mid = ALWAN_CORE_LITERAL(0.11239642) + ALWAN_CORE_ONE / (
        ALWAN_CORE_LITERAL(1.61320320) - ALWAN_CORE_LITERAL(0.68124379) * b_
        + a_ * (ALWAN_CORE_LITERAL(0.40370612) + ALWAN_CORE_LITERAL(0.90148123) * b_
        + a_ * (ALWAN_CORE_LITERAL(-0.27087943) + ALWAN_CORE_LITERAL(0.61223990) * b_
        + a_ * (ALWAN_CORE_LITERAL(0.00299215) - ALWAN_CORE_LITERAL(0.45399568) * b_
        - ALWAN_CORE_LITERAL(0.14661872) * a_))));
}

ALWAN_INLINE void ALWAN_CORE_FNLIT(alwan_ok_get_Cs)(ALWAN_CORE_T L, ALWAN_CORE_T a_, ALWAN_CORE_T b_,
                                    ALWAN_CORE_T *C_0, ALWAN_CORE_T *C_mid, ALWAN_CORE_T *C_max) {
    ALWAN_CORE_T cusp_L, cusp_C;
    ALWAN_CORE_T S_max, T_max, k;
    ALWAN_CORE_T S_mid, T_mid, C_a, C_b;
    ALWAN_CORE_T C_a0, C_b0;

    ALWAN_CORE_FNLIT(alwan_ok_find_cusp)(&cusp_L, &cusp_C, a_, b_);
    *C_max = ALWAN_CORE_FNLIT(alwan_ok_find_gamut_intersection)(a_, b_, L, ALWAN_CORE_ONE, L, cusp_L, cusp_C);
    S_max = ALWAN_CORE_SELECT(cusp_L > ALWAN_CORE_LITERAL(1e-10), cusp_C / cusp_L, ALWAN_CORE_ZERO);
    T_max = ALWAN_CORE_SELECT((ALWAN_CORE_ONE - cusp_L) > ALWAN_CORE_LITERAL(1e-10),
                          cusp_C / (ALWAN_CORE_ONE - cusp_L), ALWAN_CORE_ZERO);

    k = (*C_max) / ALWAN_CORE_MIN(L * S_max, (ALWAN_CORE_ONE - L) * T_max + ALWAN_CORE_LITERAL(1e-10));

    ALWAN_CORE_FNLIT(alwan_ok_get_ST_mid)(a_, b_, &S_mid, &T_mid);
    C_a = L * S_mid;
    C_b = (ALWAN_CORE_ONE - L) * T_mid;
    {
        ALWAN_CORE_T ca4 = C_a * C_a * C_a * C_a;
        ALWAN_CORE_T cb4 = C_b * C_b * C_b * C_b;
        ALWAN_CORE_T blend = ALWAN_CORE_SELECT((ca4 + cb4) > ALWAN_CORE_LITERAL(1e-30),
            ALWAN_CORE_POW(ALWAN_CORE_ONE / (ALWAN_CORE_ONE / (ca4 + ALWAN_CORE_LITERAL(1e-30))
                                  + ALWAN_CORE_ONE / (cb4 + ALWAN_CORE_LITERAL(1e-30))),
                      ALWAN_CORE_LITERAL(0.25)),
            ALWAN_CORE_ZERO);
        *C_mid = ALWAN_CORE_LITERAL(0.9) * k * blend;
    }

    C_a0 = L * ALWAN_CORE_LITERAL(0.4);
    C_b0 = (ALWAN_CORE_ONE - L) * ALWAN_CORE_LITERAL(0.8);
    *C_0 = ALWAN_CORE_SQRT(ALWAN_CORE_ONE / (ALWAN_CORE_ONE / (C_a0 * C_a0 + ALWAN_CORE_LITERAL(1e-30))
                                  + ALWAN_CORE_ONE / (C_b0 * C_b0 + ALWAN_CORE_LITERAL(1e-30))));
}

/* Okhsl -> sRGB (encoded) */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_okhsl_to_srgb)(ALWAN_CORE_OKHSL okhsl) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T h = okhsl.h, s = okhsl.s, l = okhsl.l;
    ALWAN_CORE_T a_, b_, L, C, r, g, b;
    ALWAN_CORE_T C_0, C_mid, C_max;

    if (l >= ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(1e-10)) {
        result.r = ALWAN_CORE_ONE; result.g = ALWAN_CORE_ONE; result.b = ALWAN_CORE_ONE;
        return result;
    }
    if (l <= ALWAN_CORE_LITERAL(1e-10)) {
        result.r = ALWAN_CORE_ZERO; result.g = ALWAN_CORE_ZERO; result.b = ALWAN_CORE_ZERO;
        return result;
    }

    a_ = ALWAN_CORE_COS(ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * h);
    b_ = ALWAN_CORE_SIN(ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * h);
    L = ALWAN_CORE_FNLIT(alwan_ok_toe_inv)(l);

    ALWAN_CORE_FNLIT(alwan_ok_get_Cs)(L, a_, b_, &C_0, &C_mid, &C_max);

    {
        ALWAN_CORE_T mid = ALWAN_CORE_LITERAL(0.8);
        ALWAN_CORE_T mid_inv = ALWAN_CORE_LITERAL(1.25);

        if (s < mid) {
            ALWAN_CORE_T t = mid_inv * s;
            ALWAN_CORE_T k_1 = mid * C_0;
            ALWAN_CORE_T k_2 = ALWAN_CORE_ONE - k_1 / (C_mid + ALWAN_CORE_LITERAL(1e-30));
            C = t * k_1 / (ALWAN_CORE_ONE - k_2 * t + ALWAN_CORE_LITERAL(1e-30));
        } else {
            ALWAN_CORE_T t = (s - mid) / (ALWAN_CORE_ONE - mid);
            ALWAN_CORE_T k_0 = C_mid;
            ALWAN_CORE_T k_1 = (ALWAN_CORE_ONE - mid) * C_mid * C_mid * mid_inv * mid_inv
                             / (C_0 + ALWAN_CORE_LITERAL(1e-30));
            ALWAN_CORE_T k_2 = ALWAN_CORE_ONE - k_1 / (C_max - C_mid + ALWAN_CORE_LITERAL(1e-30));
            C = k_0 + t * k_1 / (ALWAN_CORE_ONE - k_2 * t + ALWAN_CORE_LITERAL(1e-30));
        }
    }

    ALWAN_CORE_FNLIT(alwan_ok_lab_to_srgb)(&r, &g, &b, L, C * a_, C * b_);

    result.r = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(r);
    result.g = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(g);
    result.b = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(b);
    return result;
}

/* sRGB (encoded) -> Okhsl */
ALWAN_INLINE ALWAN_CORE_OKHSL ALWAN_CORE_FNVLIT(alwan_srgb_to_okhsl)(ALWAN_CORE_RGB srgb) {
    ALWAN_CORE_OKHSL result;
    ALWAN_CORE_T r_lin = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.r);
    ALWAN_CORE_T g_lin = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.g);
    ALWAN_CORE_T b_lin = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.b);
    ALWAN_CORE_T Lab_L, Lab_a, Lab_b;
    ALWAN_CORE_T C, a_, b_;
    ALWAN_CORE_T C_0, C_mid, C_max;

    ALWAN_CORE_FNLIT(alwan_ok_srgb_to_lab)(&Lab_L, &Lab_a, &Lab_b, r_lin, g_lin, b_lin);
    C = ALWAN_CORE_SQRT(Lab_a * Lab_a + Lab_b * Lab_b);

    if (C < ALWAN_CORE_LITERAL(1e-10)) {
        result.h = ALWAN_CORE_ZERO;
        result.s = ALWAN_CORE_ZERO;
        result.l = ALWAN_CORE_FNLIT(alwan_ok_toe)(Lab_L);
        return result;
    }

    a_ = Lab_a / C;
    b_ = Lab_b / C;
    result.h = ALWAN_CORE_LITERAL(0.5) + ALWAN_CORE_LITERAL(0.5) * ALWAN_CORE_ATAN2(-Lab_b, -Lab_a) / ALWAN_CORE_PI;

    ALWAN_CORE_FNLIT(alwan_ok_get_Cs)(Lab_L, a_, b_, &C_0, &C_mid, &C_max);

    {
        ALWAN_CORE_T mid = ALWAN_CORE_LITERAL(0.8);
        ALWAN_CORE_T mid_inv = ALWAN_CORE_LITERAL(1.25);

        if (C < C_mid) {
            ALWAN_CORE_T k_1 = mid * C_0;
            ALWAN_CORE_T k_2 = ALWAN_CORE_ONE - k_1 / (C_mid + ALWAN_CORE_LITERAL(1e-30));
            ALWAN_CORE_T t = C / (k_1 + k_2 * C + ALWAN_CORE_LITERAL(1e-30));
            result.s = t * mid;
        } else {
            ALWAN_CORE_T k_0 = C_mid;
            ALWAN_CORE_T k_1 = (ALWAN_CORE_ONE - mid) * C_mid * C_mid * mid_inv * mid_inv
                             / (C_0 + ALWAN_CORE_LITERAL(1e-30));
            ALWAN_CORE_T k_2 = ALWAN_CORE_ONE - k_1 / (C_max - C_mid + ALWAN_CORE_LITERAL(1e-30));
            ALWAN_CORE_T t = (C - k_0) / (k_1 + k_2 * (C - k_0) + ALWAN_CORE_LITERAL(1e-30));
            result.s = mid + (ALWAN_CORE_ONE - mid) * t;
        }
    }
    result.l = ALWAN_CORE_FNLIT(alwan_ok_toe)(Lab_L);
    return result;
}

/* Okhsv -> sRGB (encoded) */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_okhsv_to_srgb)(ALWAN_CORE_OKHSV okhsv) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T h = okhsv.h, s = okhsv.s, v = okhsv.v;
    ALWAN_CORE_T a_, b_;
    ALWAN_CORE_T cusp_L, cusp_C, S_max, T_max;
    ALWAN_CORE_T S_0, k, L_v, C_v, L, C;
    ALWAN_CORE_T L_vt, C_vt, r_s, g_s, b_s, scale_L;
    ALWAN_CORE_T r_out, g_out, b_out;

    if (v <= ALWAN_CORE_LITERAL(1e-10)) {
        result.r = ALWAN_CORE_ZERO; result.g = ALWAN_CORE_ZERO; result.b = ALWAN_CORE_ZERO;
        return result;
    }

    a_ = ALWAN_CORE_COS(ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * h);
    b_ = ALWAN_CORE_SIN(ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * h);

    ALWAN_CORE_FNLIT(alwan_ok_find_cusp)(&cusp_L, &cusp_C, a_, b_);
    S_max = ALWAN_CORE_SELECT(cusp_L > ALWAN_CORE_LITERAL(1e-10), cusp_C / cusp_L, ALWAN_CORE_ZERO);
    T_max = ALWAN_CORE_SELECT((ALWAN_CORE_ONE - cusp_L) > ALWAN_CORE_LITERAL(1e-10),
                          cusp_C / (ALWAN_CORE_ONE - cusp_L), ALWAN_CORE_ZERO);

    S_0 = ALWAN_CORE_LITERAL(0.5);
    k = ALWAN_CORE_ONE - S_0 / (S_max + ALWAN_CORE_LITERAL(1e-10));

    L_v = ALWAN_CORE_ONE - s * S_0 / (S_0 + T_max - T_max * k * s + ALWAN_CORE_LITERAL(1e-10));
    C_v = s * T_max * S_0 / (S_0 + T_max - T_max * k * s + ALWAN_CORE_LITERAL(1e-10));

    L = v * L_v;
    C = v * C_v;

    L_vt = ALWAN_CORE_FNLIT(alwan_ok_toe_inv)(L_v);
    C_vt = ALWAN_CORE_SELECT(L_v > ALWAN_CORE_LITERAL(1e-10), C_v * L_vt / L_v, ALWAN_CORE_ZERO);

    {
        ALWAN_CORE_T L_new = ALWAN_CORE_FNLIT(alwan_ok_toe_inv)(L);
        C = ALWAN_CORE_SELECT(L > ALWAN_CORE_LITERAL(1e-10), C * L_new / L, ALWAN_CORE_ZERO);
        L = L_new;
    }

    ALWAN_CORE_FNLIT(alwan_ok_lab_to_srgb)(&r_s, &g_s, &b_s, L_vt, a_ * C_vt, b_ * C_vt);
    scale_L = ALWAN_CORE_CBRT(ALWAN_CORE_ONE / (ALWAN_CORE_MAX3(r_s, g_s, b_s) + ALWAN_CORE_LITERAL(1e-10)));

    L *= scale_L;
    C *= scale_L;

    ALWAN_CORE_FNLIT(alwan_ok_lab_to_srgb)(&r_out, &g_out, &b_out, L, C * a_, C * b_);

    result.r = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(r_out);
    result.g = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(g_out);
    result.b = ALWAN_CORE_FNLIT(alwan_srgb_oetf)(b_out);
    return result;
}

/* sRGB (encoded) -> Okhsv */
ALWAN_INLINE ALWAN_CORE_OKHSV ALWAN_CORE_FNVLIT(alwan_srgb_to_okhsv)(ALWAN_CORE_RGB srgb) {
    ALWAN_CORE_OKHSV result;
    ALWAN_CORE_T r_lin = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.r);
    ALWAN_CORE_T g_lin = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.g);
    ALWAN_CORE_T b_lin = ALWAN_CORE_FNLIT(alwan_srgb_eotf)(srgb.b);
    ALWAN_CORE_T Lab_L, Lab_a, Lab_b;
    ALWAN_CORE_T C, a_, b_;
    ALWAN_CORE_T cusp_L, cusp_C, S_max, T_max;
    ALWAN_CORE_T S_0, k;

    ALWAN_CORE_FNLIT(alwan_ok_srgb_to_lab)(&Lab_L, &Lab_a, &Lab_b, r_lin, g_lin, b_lin);
    C = ALWAN_CORE_SQRT(Lab_a * Lab_a + Lab_b * Lab_b);

    if (C < ALWAN_CORE_LITERAL(1e-10)) {
        result.h = ALWAN_CORE_ZERO;
        result.s = ALWAN_CORE_ZERO;
        result.v = ALWAN_CORE_FNLIT(alwan_ok_toe)(Lab_L);
        return result;
    }

    a_ = Lab_a / C;
    b_ = Lab_b / C;
    result.h = ALWAN_CORE_LITERAL(0.5) + ALWAN_CORE_LITERAL(0.5) * ALWAN_CORE_ATAN2(-Lab_b, -Lab_a) / ALWAN_CORE_PI;

    ALWAN_CORE_FNLIT(alwan_ok_find_cusp)(&cusp_L, &cusp_C, a_, b_);
    S_max = ALWAN_CORE_SELECT(cusp_L > ALWAN_CORE_LITERAL(1e-10), cusp_C / cusp_L, ALWAN_CORE_ZERO);
    T_max = ALWAN_CORE_SELECT((ALWAN_CORE_ONE - cusp_L) > ALWAN_CORE_LITERAL(1e-10),
                          cusp_C / (ALWAN_CORE_ONE - cusp_L), ALWAN_CORE_ZERO);
    S_0 = ALWAN_CORE_LITERAL(0.5);
    k = ALWAN_CORE_ONE - S_0 / (S_max + ALWAN_CORE_LITERAL(1e-10));

    {
        ALWAN_CORE_T t = T_max / (C + Lab_L * T_max + ALWAN_CORE_LITERAL(1e-10));
        ALWAN_CORE_T L_v = t * Lab_L;
        ALWAN_CORE_T C_v = t * C;

        ALWAN_CORE_T L_vt = ALWAN_CORE_FNLIT(alwan_ok_toe_inv)(L_v);
        ALWAN_CORE_T C_vt = ALWAN_CORE_SELECT(L_v > ALWAN_CORE_LITERAL(1e-10),
                                          C_v * L_vt / L_v, ALWAN_CORE_ZERO);

        ALWAN_CORE_T r_s, g_s, b_s;
        ALWAN_CORE_T scale_L;
        ALWAN_CORE_FNLIT(alwan_ok_lab_to_srgb)(&r_s, &g_s, &b_s, L_vt, a_ * C_vt, b_ * C_vt);
        scale_L = ALWAN_CORE_CBRT(ALWAN_CORE_ONE / (ALWAN_CORE_MAX3(r_s, g_s, b_s) + ALWAN_CORE_LITERAL(1e-10)));

        Lab_L /= scale_L;
        C /= scale_L;

        C = C * ALWAN_CORE_FNLIT(alwan_ok_toe)(Lab_L) / (Lab_L + ALWAN_CORE_LITERAL(1e-10));
        Lab_L = ALWAN_CORE_FNLIT(alwan_ok_toe)(Lab_L);

        result.v = Lab_L / (L_v + ALWAN_CORE_LITERAL(1e-10));
        result.s = (S_0 + T_max) * C_v / (T_max * S_0 + T_max * k * C_v + ALWAN_CORE_LITERAL(1e-10));
    }

    return result;
}
