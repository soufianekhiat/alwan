/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ACES Fixed Functions (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: OpenColorIO FixedFunctionOpCPU.cpp,
 *            ACES CTL Lib.Academy.OutputTransform.ctl
 */

/* ================================================================
 * Constants
 * ================================================================ */

#define ACES_CAM_NL_OFFSET_  ((ALWAN_CORE_T)(ACES_CAM_NL_OFFSET_VALUE))
#define ACES_J_SCALE_        ((ALWAN_CORE_T)(ACES_J_SCALE_VALUE))
#define ACES_GAMUT_COMPRESSION_THRESHOLD_ ((ALWAN_CORE_T)(ACES_GAMUT_COMPRESSION_THRESHOLD_VALUE))
#define ACES_GAMUT_SMOOTH_CUSPS_ ((ALWAN_CORE_T)(ACES_GAMUT_SMOOTH_CUSPS_VALUE))
#define ACES_GAMUT_CUSP_MID_BLEND_ ((ALWAN_CORE_T)(ACES_GAMUT_CUSP_MID_BLEND_VALUE))

/* ================================================================
 * Data Arrays
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_T ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_COS_V)[4] = {
#include "../data/aces2_chroma_norm_cos.csv"
};
ALWAN_CONSTEXPR ALWAN_CORE_T ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_SIN_V)[4] = {
#include "../data/aces2_chroma_norm_sin.csv"
};
ALWAN_DIAG_POP

/* ================================================================
 * Struct Definitions (suffixed per precision)
 * ================================================================ */

typedef struct {
    ALWAN_CORE_MAT3X3 MATRIX_RGB_to_CAM16_c;
    ALWAN_CORE_MAT3X3 MATRIX_cone_response_to_Aab;
    ALWAN_CORE_MAT3X3 MATRIX_CAM16_to_RGB;
    ALWAN_CORE_MAT3X3 MATRIX_Aab_to_cone;
    ALWAN_CORE_T cz;
    ALWAN_CORE_T inv_cz;
    ALWAN_CORE_T A_w_J;
    ALWAN_CORE_T inv_A_w_J;
    ALWAN_CORE_T F_L_n;
} ALWAN_CORE_FNLIT(aces2_JMhParams);

typedef struct {
    ALWAN_CORE_T n, n_r, g, t_1, s_2, u_2, m_2;
} ALWAN_CORE_FNLIT(aces2_TSParams);

typedef struct {
    ALWAN_CORE_T J, M, gamma_top_inv;
} ALWAN_CORE_FNLIT(aces2_GamutCuspEntry);

typedef struct {
    ALWAN_CORE_T limit_J_max, mid_J, focus_dist;
    ALWAN_CORE_T lower_hull_gamma_inv, model_gamma_inv, reach_max_M;
    ALWAN_CORE_FNLIT(aces2_GamutCuspEntry) cusp_table[ACES2_CUSP_TABLE_SIZE];
    ALWAN_CORE_T reach_m_table[ACES2_REACH_TABLE_SIZE];
    ALWAN_CORE_T hue_table[ACES2_CUSP_TABLE_SIZE];
} ALWAN_CORE_FNLIT(aces2_GamutCompressParams);

typedef struct {
    ALWAN_CORE_T cusp_J, cusp_M, gamma_top_inv, gamma_bottom_inv;
    ALWAN_CORE_T focus_J, analytical_threshold;
} ALWAN_CORE_FNLIT(aces2_HueDependentGamutParams);

/* ================================================================
 * Transfer Functions
 * ================================================================ */

/* BT.1886 OETF (gamma 2.4 inverse) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_bt1886_oetf)(ALWAN_CORE_T x) {
    if (x <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    return ALWAN_CORE_POW(x, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4));
}

/* sRGB OETF */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_srgb_oetf)(ALWAN_CORE_T x) {
    if (x <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    if (x <= ALWAN_CORE_LITERAL(0.0031308)) {
        return x * ALWAN_CORE_LITERAL(12.92);
    }
    return ALWAN_CORE_LITERAL(1.055) * ALWAN_CORE_POW(x, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.4)) - ALWAN_CORE_LITERAL(0.055);
}

/* Gamma 2.6 OETF (cinema) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_gamma26_oetf)(ALWAN_CORE_T x) {
    if (x <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    return ALWAN_CORE_POW(x, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.6));
}

/* PQ (ST.2084) OETF */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_pq_oetf)(ALWAN_CORE_T Y, ALWAN_CORE_T peak_nits) {
    ALWAN_CORE_T m1 = ALWAN_CORE_LITERAL(0.1593017578125);
    ALWAN_CORE_T m2 = ALWAN_CORE_LITERAL(78.84375);
    ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(0.8359375);
    ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(18.8515625);
    ALWAN_CORE_T c3 = ALWAN_CORE_LITERAL(18.6875);

    ALWAN_CORE_T L = ALWAN_CORE_MAX(Y * peak_nits / ALWAN_CORE_LITERAL(10000.0), ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T Lm1 = ALWAN_CORE_POW(L, m1);
    ALWAN_CORE_T num = c1 + c2 * Lm1;
    ALWAN_CORE_T den = ALWAN_CORE_LITERAL(1.0) + c3 * Lm1;
    return ALWAN_CORE_POW(num / den, m2);
}

/* BT.1886 EOTF (gamma 2.4) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_bt1886_eotf)(ALWAN_CORE_T x) {
    if (x <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    return ALWAN_CORE_POW(x, ALWAN_CORE_LITERAL(2.4));
}

/* sRGB EOTF */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_srgb_eotf)(ALWAN_CORE_T x) {
    if (x <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    if (x <= ALWAN_CORE_LITERAL(0.04045)) {
        return x / ALWAN_CORE_LITERAL(12.92);
    }
    return ALWAN_CORE_POW((x + ALWAN_CORE_LITERAL(0.055)) / ALWAN_CORE_LITERAL(1.055), ALWAN_CORE_LITERAL(2.4));
}

/* Gamma 2.6 EOTF */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_gamma26_eotf)(ALWAN_CORE_T x) {
    if (x <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    return ALWAN_CORE_POW(x, ALWAN_CORE_LITERAL(2.6));
}

/* PQ (ST.2084) EOTF */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_pq_eotf)(ALWAN_CORE_T E, ALWAN_CORE_T peak_nits) {
    ALWAN_CORE_T m1 = ALWAN_CORE_LITERAL(0.1593017578125);
    ALWAN_CORE_T m2 = ALWAN_CORE_LITERAL(78.84375);
    ALWAN_CORE_T c1 = ALWAN_CORE_LITERAL(0.8359375);
    ALWAN_CORE_T c2 = ALWAN_CORE_LITERAL(18.8515625);
    ALWAN_CORE_T c3 = ALWAN_CORE_LITERAL(18.6875);

    ALWAN_CORE_T Em2 = ALWAN_CORE_POW(ALWAN_CORE_MAX(E, ALWAN_CORE_LITERAL(0.0)), ALWAN_CORE_LITERAL(1.0) / m2);
    ALWAN_CORE_T num = ALWAN_CORE_MAX(Em2 - c1, ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T den = c2 - c3 * Em2;
    if (den <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T Y = ALWAN_CORE_POW(num / den, ALWAN_CORE_LITERAL(1.0) / m1);
    return Y * ALWAN_CORE_LITERAL(10000.0) / peak_nits;
}

/* ================================================================
 * RedMod / Glow Helpers
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_calc_sat_weight)(ALWAN_CORE_T red, ALWAN_CORE_T grn,
                                                  ALWAN_CORE_T blu, ALWAN_CORE_T noise_limit) {
    ALWAN_CORE_T min_val = ALWAN_CORE_MIN3(red, grn, blu);
    ALWAN_CORE_T max_val = ALWAN_CORE_MAX3(red, grn, blu);

    ALWAN_CORE_T clamped_max = max_val > ALWAN_CORE_LITERAL(1e-10) ? max_val : ALWAN_CORE_LITERAL(1e-10);
    ALWAN_CORE_T clamped_min = min_val > ALWAN_CORE_LITERAL(1e-10) ? min_val : ALWAN_CORE_LITERAL(1e-10);
    ALWAN_CORE_T denom = max_val > noise_limit ? max_val : noise_limit;

    ALWAN_CORE_T sat = (clamped_max - clamped_min) / denom;
    return sat;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_calc_hue_weight)(ALWAN_CORE_T red, ALWAN_CORE_T grn,
                                                  ALWAN_CORE_T blu, ALWAN_CORE_T inv_width) {
    ALWAN_CORE_T sqrt3 = ALWAN_CORE_LITERAL(1.7320508075688772);

    ALWAN_CORE_T a = ALWAN_CORE_LITERAL(2.0) * red - (grn + blu);
    ALWAN_CORE_T b = sqrt3 * (grn - blu);
    ALWAN_CORE_T hue = ALWAN_CORE_ATAN2(b, a);

    ALWAN_CORE_T knot_coord = hue * inv_width + ALWAN_CORE_LITERAL(2.0);
    int j = (int)knot_coord;

    /* B-spline matrix coefficients (row-major) */
    ALWAN_CORE_T M0[4] = { ALWAN_CORE_LITERAL(0.25),  ALWAN_CORE_LITERAL(0.00),  ALWAN_CORE_LITERAL(0.00),  ALWAN_CORE_LITERAL(0.00)};
    ALWAN_CORE_T M1[4] = {ALWAN_CORE_LITERAL(-0.75),  ALWAN_CORE_LITERAL(0.75),  ALWAN_CORE_LITERAL(0.75),  ALWAN_CORE_LITERAL(0.25)};
    ALWAN_CORE_T M2[4] = { ALWAN_CORE_LITERAL(0.75), ALWAN_CORE_LITERAL(-1.50),  ALWAN_CORE_LITERAL(0.00),  ALWAN_CORE_LITERAL(1.00)};
    ALWAN_CORE_T M3[4] = {ALWAN_CORE_LITERAL(-0.25),  ALWAN_CORE_LITERAL(0.75), ALWAN_CORE_LITERAL(-0.75),  ALWAN_CORE_LITERAL(0.25)};

    ALWAN_CORE_T f_H = ALWAN_CORE_LITERAL(0.0);
    if (j >= 0 && j < 4) {
        ALWAN_CORE_T t = knot_coord - (ALWAN_CORE_T)j;
        ALWAN_CORE_T c0, c1_v, c2_v, c3_v;
        if (j == 0)      { c0 = M0[0]; c1_v = M0[1]; c2_v = M0[2]; c3_v = M0[3]; }
        else if (j == 1)  { c0 = M1[0]; c1_v = M1[1]; c2_v = M1[2]; c3_v = M1[3]; }
        else if (j == 2)  { c0 = M2[0]; c1_v = M2[1]; c2_v = M2[2]; c3_v = M2[3]; }
        else              { c0 = M3[0]; c1_v = M3[1]; c2_v = M3[2]; c3_v = M3[3]; }
        f_H = c3_v + t * (c2_v + t * (c1_v + t * c0));
    }

    return f_H;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_rgb_to_yc)(ALWAN_CORE_T red, ALWAN_CORE_T grn, ALWAN_CORE_T blu) {
    ALWAN_CORE_T YC_RADIUS_WEIGHT = ALWAN_CORE_LITERAL(1.75);

    ALWAN_CORE_T chroma = ALWAN_CORE_SQRT(blu * (blu - grn) + grn * (grn - red) + red * (red - blu));
    ALWAN_CORE_T YC = (blu + grn + red + YC_RADIUS_WEIGHT * chroma) / ALWAN_CORE_LITERAL(3.0);

    return YC;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_sigmoid_shaper)(ALWAN_CORE_T sat) {
    ALWAN_CORE_T x = (sat - ALWAN_CORE_LITERAL(0.4)) * ALWAN_CORE_LITERAL(5.0);
    ALWAN_CORE_T sign = (x >= ALWAN_CORE_LITERAL(0.0)) ? ALWAN_CORE_LITERAL(1.0) : ALWAN_CORE_LITERAL(-1.0);
    ALWAN_CORE_T t = ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(0.5) * sign * x;
    if (t < ALWAN_CORE_LITERAL(0.0)) t = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T s = (ALWAN_CORE_LITERAL(1.0) + sign * (ALWAN_CORE_LITERAL(1.0) - t * t)) * ALWAN_CORE_LITERAL(0.5);
    return s;
}

/* ================================================================
 * GamutComp13 Helpers
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_compress_dist)(ALWAN_CORE_T dist, ALWAN_CORE_T thr,
                                                ALWAN_CORE_T scale, ALWAN_CORE_T power) {
    ALWAN_CORE_T nd = (dist - thr) / scale;
    ALWAN_CORE_T p = ALWAN_CORE_POW(nd, power);
    return thr + scale * nd / ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.0) + p, ALWAN_CORE_LITERAL(1.0) / power);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_uncompress_dist)(ALWAN_CORE_T compressed_dist, ALWAN_CORE_T thr,
                                                  ALWAN_CORE_T scale, ALWAN_CORE_T power) {
    ALWAN_CORE_T ndc = (compressed_dist - thr) / scale;
    if (ndc <= ALWAN_CORE_LITERAL(0.0)) {
        return compressed_dist;
    }
    ALWAN_CORE_T p = ALWAN_CORE_POW(ndc, power);
    if (p >= ALWAN_CORE_LITERAL(1.0)) {
        return thr + scale * ndc * ALWAN_CORE_LITERAL(100.0);
    }
    ALWAN_CORE_T nd = ndc / ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.0) - p, ALWAN_CORE_LITERAL(1.0) / power);
    return thr + scale * nd;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_gamut_comp_channel)(ALWAN_CORE_T val, ALWAN_CORE_T ach,
                                                     ALWAN_CORE_T thr, ALWAN_CORE_T scale,
                                                     ALWAN_CORE_T power) {
    if (ach == ALWAN_CORE_LITERAL(0.0)) {
        return ALWAN_CORE_LITERAL(0.0);
    }

    ALWAN_CORE_T dist = (ach - val) / ALWAN_CORE_ABS(ach);

    if (dist < thr) {
        return val;
    }

    ALWAN_CORE_T compr_dist = ALWAN_CORE_FNVLIT(aces_compress_dist)(dist, thr, scale, power);
    ALWAN_CORE_T compr = ach - compr_dist * ALWAN_CORE_ABS(ach);

    return compr;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_gamut_decomp_channel)(ALWAN_CORE_T val, ALWAN_CORE_T ach,
                                                       ALWAN_CORE_T thr, ALWAN_CORE_T scale,
                                                       ALWAN_CORE_T power) {
    if (ach == ALWAN_CORE_LITERAL(0.0)) {
        return ALWAN_CORE_LITERAL(0.0);
    }

    ALWAN_CORE_T dist = (ach - val) / ALWAN_CORE_ABS(ach);

    if (dist < thr) {
        return val;
    }

    ALWAN_CORE_T decompr_dist = ALWAN_CORE_FNVLIT(aces_uncompress_dist)(dist, thr, scale, power);
    ALWAN_CORE_T decompr = ach - decompr_dist * ALWAN_CORE_ABS(ach);

    return decompr;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_calc_gamut_comp_scale)(ALWAN_CORE_T lim, ALWAN_CORE_T thr,
                                                        ALWAN_CORE_T power) {
    ALWAN_CORE_T base = (ALWAN_CORE_LITERAL(1.0) - thr) / (lim - thr);
    ALWAN_CORE_T inner = ALWAN_CORE_POW(base, -power) - ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T denom = ALWAN_CORE_POW(inner, ALWAN_CORE_LITERAL(1.0) / power);
    return (lim - thr) / denom;
}

/* ================================================================
 * ACES1 Helpers
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces1_saturation)(ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T b) {
    ALWAN_CORE_T mx = ALWAN_CORE_MAX3(r, g, b);
    ALWAN_CORE_T mn = ALWAN_CORE_MIN3(r, g, b);
    if (mx <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);
    return (mx - mn) / mx;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces1_rgb_to_hue)(ALWAN_CORE_T r, ALWAN_CORE_T g, ALWAN_CORE_T b) {
    ALWAN_CORE_T mx = ALWAN_CORE_MAX3(r, g, b);
    ALWAN_CORE_T mn = ALWAN_CORE_MIN3(r, g, b);
    ALWAN_CORE_T chroma = mx - mn;

    if (chroma <= ALWAN_CORE_LITERAL(1e-10)) return ALWAN_CORE_LITERAL(0.0);

    ALWAN_CORE_T hue;
    if (mx == r) {
        hue = (g - b) / chroma;
        if (hue < ALWAN_CORE_LITERAL(0.0)) hue += ALWAN_CORE_LITERAL(6.0);
    } else if (mx == g) {
        hue = ALWAN_CORE_LITERAL(2.0) + (b - r) / chroma;
    } else {
        hue = ALWAN_CORE_LITERAL(4.0) + (r - g) / chroma;
    }
    return hue * ALWAN_CORE_LITERAL(60.0);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces1_center_hue)(ALWAN_CORE_T hue, ALWAN_CORE_T center) {
    ALWAN_CORE_T centered = hue - center;
    if (centered < ALWAN_CORE_LITERAL(-180.0)) centered += ALWAN_CORE_LITERAL(360.0);
    if (centered > ALWAN_CORE_LITERAL(180.0)) centered -= ALWAN_CORE_LITERAL(360.0);
    return centered;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces1_cubic_basis_shaper)(ALWAN_CORE_T x, ALWAN_CORE_T width) {
    ALWAN_CORE_T t = x / width;
    if (t < ALWAN_CORE_LITERAL(-1.0) || t > ALWAN_CORE_LITERAL(1.0)) return ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T t2 = t * t;
    return ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_LITERAL(3.0) * t2 + ALWAN_CORE_LITERAL(2.0) * t2 * ALWAN_CORE_ABS(t);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5)(ALWAN_CORE_T x) {
    ALWAN_CORE_T min_pt_x = ALWAN_CORE_LITERAL(5.4931640625e-6);
    ALWAN_CORE_T min_pt_y = ALWAN_CORE_LITERAL(0.0001);
    ALWAN_CORE_T mid_pt_x = ALWAN_CORE_LITERAL(0.18);
    ALWAN_CORE_T mid_pt_y = ALWAN_CORE_LITERAL(4.8);
    ALWAN_CORE_T max_pt_x = ALWAN_CORE_LITERAL(47185.92);
    ALWAN_CORE_T max_pt_y = ALWAN_CORE_LITERAL(48.0);

    ALWAN_CORE_T log_x = ALWAN_CORE_LOG10(ALWAN_CORE_MAX(x, ALWAN_CORE_LITERAL(1e-10)));
    ALWAN_CORE_T log_min = ALWAN_CORE_LOG10(min_pt_x);
    ALWAN_CORE_T log_mid = ALWAN_CORE_LOG10(mid_pt_x);
    ALWAN_CORE_T log_max = ALWAN_CORE_LOG10(max_pt_x);

    ALWAN_CORE_T t;
    if (log_x <= log_min) {
        return min_pt_y;
    } else if (log_x >= log_max) {
        return max_pt_y;
    } else if (log_x < log_mid) {
        t = (log_x - log_min) / (log_mid - log_min);
        ALWAN_CORE_T t2 = t * t;
        ALWAN_CORE_T t3 = t2 * t;
        return min_pt_y + (mid_pt_y - min_pt_y) * (ALWAN_CORE_LITERAL(3.0) * t2 - ALWAN_CORE_LITERAL(2.0) * t3);
    } else {
        t = (log_x - log_mid) / (log_max - log_mid);
        ALWAN_CORE_T t2 = t * t;
        ALWAN_CORE_T t3 = t2 * t;
        return mid_pt_y + (max_pt_y - mid_pt_y) * (ALWAN_CORE_LITERAL(3.0) * t2 - ALWAN_CORE_LITERAL(2.0) * t3);
    }
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5_inv)(ALWAN_CORE_T y) {
    ALWAN_CORE_T min_pt_x = ALWAN_CORE_LITERAL(5.4931640625e-6);
    ALWAN_CORE_T min_pt_y = ALWAN_CORE_LITERAL(0.0001);
    ALWAN_CORE_T mid_pt_x = ALWAN_CORE_LITERAL(0.18);
    ALWAN_CORE_T mid_pt_y = ALWAN_CORE_LITERAL(4.8);
    ALWAN_CORE_T max_pt_x = ALWAN_CORE_LITERAL(47185.92);
    ALWAN_CORE_T max_pt_y = ALWAN_CORE_LITERAL(48.0);

    ALWAN_CORE_T target_nits = y * ALWAN_CORE_LITERAL(48.0);

    if (target_nits <= min_pt_y) return min_pt_x;
    if (target_nits >= max_pt_y) return max_pt_x;

    ALWAN_CORE_T x;
    if (target_nits <= mid_pt_y) {
        ALWAN_CORE_T t = (target_nits - min_pt_y) / (mid_pt_y - min_pt_y);
        ALWAN_CORE_T log_min = ALWAN_CORE_LOG10(min_pt_x);
        ALWAN_CORE_T log_mid = ALWAN_CORE_LOG10(mid_pt_x);
        x = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), log_min + t * (log_mid - log_min));
    } else {
        ALWAN_CORE_T t = (target_nits - mid_pt_y) / (max_pt_y - mid_pt_y);
        ALWAN_CORE_T log_mid = ALWAN_CORE_LOG10(mid_pt_x);
        ALWAN_CORE_T log_max = ALWAN_CORE_LOG10(max_pt_x);
        x = ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), log_mid + t * (log_max - log_mid));
    }

    for (int i = 0; i < 30; i++) {
        ALWAN_CORE_T fx = ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5)(x) - target_nits;
        if (ALWAN_CORE_ABS(fx) < ALWAN_CORE_LITERAL(1e-10)) break;

        ALWAN_CORE_T h = ALWAN_CORE_MAX(ALWAN_CORE_ABS(x) * ALWAN_CORE_LITERAL(1e-6), ALWAN_CORE_LITERAL(1e-12));
        ALWAN_CORE_T dfx = (ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5)(x + h) - ALWAN_CORE_FNVLIT(aces1_segmented_spline_c5)(x - h)) / (ALWAN_CORE_LITERAL(2.0) * h);
        if (ALWAN_CORE_ABS(dfx) < ALWAN_CORE_LITERAL(1e-12)) break;

        ALWAN_CORE_T dx = fx / dfx;

        if (ALWAN_CORE_ABS(dx) > x * ALWAN_CORE_LITERAL(0.5)) {
            dx = (dx > ALWAN_CORE_LITERAL(0.0)) ? x * ALWAN_CORE_LITERAL(0.5) : -x * ALWAN_CORE_LITERAL(0.5);
        }

        x = x - dx;

        if (x < min_pt_x) x = min_pt_x;
        if (x > max_pt_x) x = max_pt_x;
    }

    return x;
}

/* ================================================================
 * Matrix Operations
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(aces_primaries_to_rgb_to_xyz)(ALWAN_CORE_T rx, ALWAN_CORE_T ry,
                                                          ALWAN_CORE_T gx, ALWAN_CORE_T gy,
                                                          ALWAN_CORE_T bx, ALWAN_CORE_T by,
                                                          ALWAN_CORE_T wx, ALWAN_CORE_T wy,
                                                          ALWAN_CORE_T Y) {
    ALWAN_CORE_MAT3X3 out;

    ALWAN_CORE_T rX = rx / ry;
    ALWAN_CORE_T rY = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T rZ = (ALWAN_CORE_LITERAL(1.0) - rx - ry) / ry;

    ALWAN_CORE_T gX = gx / gy;
    ALWAN_CORE_T gY = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T gZ = (ALWAN_CORE_LITERAL(1.0) - gx - gy) / gy;

    ALWAN_CORE_T bX = bx / by;
    ALWAN_CORE_T bY = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T bZ = (ALWAN_CORE_LITERAL(1.0) - bx - by) / by;

    ALWAN_CORE_T wX = wx * Y / wy;
    ALWAN_CORE_T wY = Y;
    ALWAN_CORE_T wZ = (ALWAN_CORE_LITERAL(1.0) - wx - wy) * Y / wy;

    ALWAN_CORE_MAT3X3 M;
    M.m[0] = rX; M.m[1] = gX; M.m[2] = bX;
    M.m[3] = rY; M.m[4] = gY; M.m[5] = bY;
    M.m[6] = rZ; M.m[7] = gZ; M.m[8] = bZ;

    ALWAN_CORE_VEC3 W = {{wX, wY, wZ}};
    ALWAN_CORE_VEC3 S = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNVLIT(alwan_mat3_inv)(M), W);

    out.m[0] = S.v[0] * rX; out.m[1] = S.v[1] * gX; out.m[2] = S.v[2] * bX;
    out.m[3] = S.v[0] * rY; out.m[4] = S.v[1] * gY; out.m[5] = S.v[2] * bY;
    out.m[6] = S.v[0] * rZ; out.m[7] = S.v[1] * gZ; out.m[8] = S.v[2] * bZ;

    return out;
}

/* ================================================================
 * ACES2 CAM Helpers
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_cone_response_fwd)(ALWAN_CORE_T v) {
    ALWAN_CORE_T abs_v = ALWAN_CORE_ABS(v);
    if (abs_v < ALWAN_CORE_LITERAL(1e-10)) return ALWAN_CORE_LITERAL(0.0);

    ALWAN_CORE_T F_L_Y = ALWAN_CORE_POW(abs_v, ALWAN_CORE_LITERAL(0.42));
    ALWAN_CORE_T Ra = F_L_Y / (ACES_CAM_NL_OFFSET_ + F_L_Y);

    return (v >= ALWAN_CORE_LITERAL(0.0)) ? Ra : -Ra;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_cone_response_inv)(ALWAN_CORE_T Ra) {
    ALWAN_CORE_T sign = (Ra >= ALWAN_CORE_LITERAL(0.0)) ? ALWAN_CORE_LITERAL(1.0) : ALWAN_CORE_LITERAL(-1.0);
    ALWAN_CORE_T Ra_abs = ALWAN_CORE_ABS(Ra);

    if (Ra_abs < ALWAN_CORE_LITERAL(1e-10)) return ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T Ra_lim = (Ra_abs < ALWAN_CORE_LITERAL(0.99)) ? Ra_abs : ALWAN_CORE_LITERAL(0.99);

    ALWAN_CORE_T F_L_Y = ACES_CAM_NL_OFFSET_ * Ra_lim / (ALWAN_CORE_LITERAL(1.0) - Ra_lim);
    ALWAN_CORE_T Rc = ALWAN_CORE_POW(F_L_Y, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(0.42));

    return sign * Rc;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_toe_fwd)(ALWAN_CORE_T x, ALWAN_CORE_T limit,
                                          ALWAN_CORE_T k1_in, ALWAN_CORE_T k2_in) {
    if (x > limit) return x;

    ALWAN_CORE_T k2 = (k2_in > ALWAN_CORE_LITERAL(0.001)) ? k2_in : ALWAN_CORE_LITERAL(0.001);
    ALWAN_CORE_T k1 = ALWAN_CORE_SQRT(k1_in * k1_in + k2 * k2);
    ALWAN_CORE_T k3 = (limit + k1) / (limit + k2);
    ALWAN_CORE_T minus_b = k3 * x - k1;
    ALWAN_CORE_T minus_ac = k2 * k3 * x;

    return ALWAN_CORE_LITERAL(0.5) * (minus_b + ALWAN_CORE_SQRT(minus_b * minus_b + ALWAN_CORE_LITERAL(4.0) * minus_ac));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_toe_inv)(ALWAN_CORE_T x, ALWAN_CORE_T limit,
                                          ALWAN_CORE_T k1_in, ALWAN_CORE_T k2_in) {
    if (x > limit) return x;

    ALWAN_CORE_T k2 = (k2_in > ALWAN_CORE_LITERAL(0.001)) ? k2_in : ALWAN_CORE_LITERAL(0.001);
    ALWAN_CORE_T k1 = ALWAN_CORE_SQRT(k1_in * k1_in + k2 * k2);
    ALWAN_CORE_T k3 = (limit + k1) / (limit + k2);

    return (x * x + k1 * x) / (k3 * (x + k2));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces_chroma_compress_norm)(ALWAN_CORE_T h_rad, ALWAN_CORE_T scale) {
    ALWAN_CORE_T cos_hr1 = ALWAN_CORE_COS(h_rad);
    ALWAN_CORE_T sin_hr1 = ALWAN_CORE_SIN(h_rad);

    ALWAN_CORE_T cos_hr2 = ALWAN_CORE_LITERAL(2.0) * cos_hr1 * cos_hr1 - ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T sin_hr2 = ALWAN_CORE_LITERAL(2.0) * cos_hr1 * sin_hr1;
    ALWAN_CORE_T cos_hr3 = ALWAN_CORE_LITERAL(4.0) * cos_hr1 * cos_hr1 * cos_hr1 - ALWAN_CORE_LITERAL(3.0) * cos_hr1;
    ALWAN_CORE_T sin_hr3 = ALWAN_CORE_LITERAL(3.0) * sin_hr1 - ALWAN_CORE_LITERAL(4.0) * sin_hr1 * sin_hr1 * sin_hr1;

    ALWAN_CORE_T M = ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_COS_V)[0] * cos_hr1
                   + ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_COS_V)[1] * cos_hr2
                   + ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_COS_V)[2] * cos_hr3
                   + ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_SIN_V)[0] * sin_hr1
                   + ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_SIN_V)[1] * sin_hr2
                   + ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_SIN_V)[2] * sin_hr3
                   + ALWAN_CORE_FNLIT(ACES2_CHROMA_NORM_SIN_V)[3];

    return M * scale;
}

/* ================================================================
 * ACES2 Per-pixel Conversions
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(aces2_rgb_to_aab)(ALWAN_CORE_VEC3 rgb, ALWAN_CORE_FNLIT(aces2_JMhParams) const *p) {
    ALWAN_CORE_VEC3 rgb_m = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(p->MATRIX_RGB_to_CAM16_c, rgb);

    ALWAN_CORE_VEC3 rgb_a;
    rgb_a.v[0] = ALWAN_CORE_FNVLIT(aces_cone_response_fwd)(rgb_m.v[0]);
    rgb_a.v[1] = ALWAN_CORE_FNVLIT(aces_cone_response_fwd)(rgb_m.v[1]);
    rgb_a.v[2] = ALWAN_CORE_FNVLIT(aces_cone_response_fwd)(rgb_m.v[2]);

    return ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(p->MATRIX_cone_response_to_Aab, rgb_a);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(aces2_aab_to_jmh)(ALWAN_CORE_VEC3 aab, ALWAN_CORE_FNLIT(aces2_JMhParams) const *p) {
    ALWAN_CORE_VEC3 jmh;

    if (aab.v[0] <= ALWAN_CORE_LITERAL(0.0)) {
        jmh.v[0] = ALWAN_CORE_LITERAL(0.0);
        jmh.v[1] = ALWAN_CORE_LITERAL(0.0);
        jmh.v[2] = ALWAN_CORE_LITERAL(0.0);
        return jmh;
    }

    jmh.v[0] = ACES_J_SCALE_ * ALWAN_CORE_POW(aab.v[0], p->cz);
    jmh.v[1] = ALWAN_CORE_SQRT(aab.v[1] * aab.v[1] + aab.v[2] * aab.v[2]);

    ALWAN_CORE_T h_rad = ALWAN_CORE_ATAN2(aab.v[2], aab.v[1]);
    ALWAN_CORE_T h_deg = h_rad * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;

    if (h_deg < ALWAN_CORE_LITERAL(0.0)) {
        h_deg += ALWAN_CORE_LITERAL(360.0);
    }
    jmh.v[2] = h_deg;

    return jmh;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(aces2_jmh_to_aab)(ALWAN_CORE_VEC3 jmh, ALWAN_CORE_FNLIT(aces2_JMhParams) const *p) {
    ALWAN_CORE_VEC3 aab;

    if (jmh.v[0] <= ALWAN_CORE_LITERAL(0.0)) {
        aab.v[0] = ALWAN_CORE_LITERAL(0.0);
        aab.v[1] = ALWAN_CORE_LITERAL(0.0);
        aab.v[2] = ALWAN_CORE_LITERAL(0.0);
        return aab;
    }

    aab.v[0] = ALWAN_CORE_POW(jmh.v[0] / ACES_J_SCALE_, p->inv_cz);

    ALWAN_CORE_T h_rad = jmh.v[2] * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);

    aab.v[1] = jmh.v[1] * ALWAN_CORE_COS(h_rad);
    aab.v[2] = jmh.v[1] * ALWAN_CORE_SIN(h_rad);

    return aab;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(aces2_aab_to_rgb)(ALWAN_CORE_VEC3 aab, ALWAN_CORE_FNLIT(aces2_JMhParams) const *p) {
    ALWAN_CORE_VEC3 rgb_a = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(p->MATRIX_Aab_to_cone, aab);

    ALWAN_CORE_VEC3 rgb_m;
    rgb_m.v[0] = ALWAN_CORE_FNVLIT(aces_cone_response_inv)(rgb_a.v[0]);
    rgb_m.v[1] = ALWAN_CORE_FNVLIT(aces_cone_response_inv)(rgb_a.v[1]);
    rgb_m.v[2] = ALWAN_CORE_FNVLIT(aces_cone_response_inv)(rgb_a.v[2]);

    return ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(p->MATRIX_CAM16_to_RGB, rgb_m);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_j_to_y)(ALWAN_CORE_T J, ALWAN_CORE_FNLIT(aces2_JMhParams) const *p) {
    if (J <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);

    ALWAN_CORE_T A = ALWAN_CORE_POW(J / ACES_J_SCALE_, p->inv_cz);
    ALWAN_CORE_T Ra = p->A_w_J * A;
    ALWAN_CORE_T Y = ALWAN_CORE_FNVLIT(aces_cone_response_inv)(Ra) / p->F_L_n;

    return Y;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_y_to_j)(ALWAN_CORE_T Y, ALWAN_CORE_FNLIT(aces2_JMhParams) const *p) {
    if (Y <= ALWAN_CORE_LITERAL(0.0)) return ALWAN_CORE_LITERAL(0.0);

    ALWAN_CORE_T Ra = ALWAN_CORE_FNVLIT(aces_cone_response_fwd)(Y * p->F_L_n);
    ALWAN_CORE_T J = ACES_J_SCALE_ * ALWAN_CORE_POW(Ra * p->inv_A_w_J, p->cz);

    return J;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_tonescale_fwd)(ALWAN_CORE_T x, ALWAN_CORE_FNLIT(aces2_TSParams) const *ts) {
    ALWAN_CORE_T x_clamped = (x > ALWAN_CORE_LITERAL(0.0)) ? x : ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T f = ts->m_2 * ALWAN_CORE_POW(x_clamped / (x_clamped + ts->s_2), ts->g);
    ALWAN_CORE_T h = (f > ALWAN_CORE_LITERAL(0.0)) ? (f * f / (f + ts->t_1)) : ALWAN_CORE_LITERAL(0.0);
    return h * ts->n_r;
}

/* ================================================================
 * Gamut Compression Math
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_smin_scaled)(ALWAN_CORE_T a, ALWAN_CORE_T b, ALWAN_CORE_T cusp_M) {
    ALWAN_CORE_T s = cusp_M * ACES_GAMUT_SMOOTH_CUSPS_;
    ALWAN_CORE_T x = a - b;
    ALWAN_CORE_T sign = (x >= ALWAN_CORE_LITERAL(0.0)) ? ALWAN_CORE_LITERAL(1.0) : ALWAN_CORE_LITERAL(-1.0);
    ALWAN_CORE_T abs_x = ALWAN_CORE_ABS(x);

    if (abs_x >= s) {
        return (a < b) ? a : b;
    }

    ALWAN_CORE_T t = (abs_x - s) / (ALWAN_CORE_LITERAL(2.0) * s);
    ALWAN_CORE_T blend = t * t * (abs_x + s) / (ALWAN_CORE_LITERAL(2.0) * s);
    ALWAN_CORE_T min_val = (a < b) ? a : b;

    return min_val + blend * sign;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_reinhard_fwd)(ALWAN_CORE_T x) {
    return x / (ALWAN_CORE_LITERAL(1.0) + x);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_reinhard_inv)(ALWAN_CORE_T x) {
    return x / (ALWAN_CORE_LITERAL(1.0) - x);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_remap_m_fwd)(ALWAN_CORE_T M, ALWAN_CORE_T gamut_boundary_M,
                                               ALWAN_CORE_T reach_boundary_M) {
    ALWAN_CORE_T boundary_ratio = gamut_boundary_M / reach_boundary_M;
    ALWAN_CORE_T proportion = (boundary_ratio > ACES_GAMUT_COMPRESSION_THRESHOLD_)
                             ? boundary_ratio : ACES_GAMUT_COMPRESSION_THRESHOLD_;
    ALWAN_CORE_T threshold = proportion * gamut_boundary_M;

    if (M <= threshold || proportion >= ALWAN_CORE_LITERAL(1.0)) {
        return M;
    }

    ALWAN_CORE_T m_offset = M - threshold;
    ALWAN_CORE_T gamut_offset = gamut_boundary_M - threshold;
    ALWAN_CORE_T reach_offset = reach_boundary_M - threshold;
    ALWAN_CORE_T scale = reach_offset / ((reach_offset / gamut_offset) - ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T nd = m_offset / scale;

    return threshold + scale * ALWAN_CORE_FNVLIT(aces2_reinhard_fwd)(nd);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_remap_m_inv)(ALWAN_CORE_T M, ALWAN_CORE_T gamut_boundary_M,
                                               ALWAN_CORE_T reach_boundary_M) {
    ALWAN_CORE_T boundary_ratio = gamut_boundary_M / reach_boundary_M;
    ALWAN_CORE_T proportion = (boundary_ratio > ACES_GAMUT_COMPRESSION_THRESHOLD_)
                             ? boundary_ratio : ACES_GAMUT_COMPRESSION_THRESHOLD_;
    ALWAN_CORE_T threshold = proportion * gamut_boundary_M;

    if (M <= threshold || proportion >= ALWAN_CORE_LITERAL(1.0)) {
        return M;
    }

    ALWAN_CORE_T m_offset = M - threshold;
    ALWAN_CORE_T gamut_offset = gamut_boundary_M - threshold;
    ALWAN_CORE_T reach_offset = reach_boundary_M - threshold;
    ALWAN_CORE_T scale = reach_offset / ((reach_offset / gamut_offset) - ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T nd = m_offset / scale;

    return threshold + scale * ALWAN_CORE_FNVLIT(aces2_reinhard_inv)(nd);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_compute_focus_j)(ALWAN_CORE_T cusp_J, ALWAN_CORE_T mid_J,
                                                   ALWAN_CORE_T limit_J_max) {
    ALWAN_CORE_T blend = ACES_GAMUT_CUSP_MID_BLEND_ - (cusp_J / limit_J_max);
    if (blend > ALWAN_CORE_LITERAL(1.0)) blend = ALWAN_CORE_LITERAL(1.0);
    return cusp_J + blend * (mid_J - cusp_J);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_get_focus_gain)(ALWAN_CORE_T J, ALWAN_CORE_T analytical_threshold,
                                                  ALWAN_CORE_T limit_J_max, ALWAN_CORE_T focus_dist) {
    ALWAN_CORE_T gain = limit_J_max * focus_dist;

    if (J > analytical_threshold) {
        ALWAN_CORE_T denom = limit_J_max - J;
        if (denom < ALWAN_CORE_LITERAL(0.0001)) denom = ALWAN_CORE_LITERAL(0.0001);
        ALWAN_CORE_T gain_adj = ALWAN_CORE_LN((limit_J_max - analytical_threshold) / denom)
                              / ALWAN_CORE_LN(ALWAN_CORE_LITERAL(10.0));
        gain_adj = gain_adj * gain_adj + ALWAN_CORE_LITERAL(1.0);
        gain = gain * gain_adj;
    }

    return gain;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_solve_j_intersect)(ALWAN_CORE_T J, ALWAN_CORE_T M,
                                                     ALWAN_CORE_T focus_J, ALWAN_CORE_T max_J,
                                                     ALWAN_CORE_T slope_gain) {
    ALWAN_CORE_T M_scaled = M / slope_gain;
    ALWAN_CORE_T a = M_scaled / focus_J;

    if (J < focus_J) {
        ALWAN_CORE_T b = ALWAN_CORE_LITERAL(1.0) - M_scaled;
        ALWAN_CORE_T c = -J;
        ALWAN_CORE_T det = b * b - ALWAN_CORE_LITERAL(4.0) * a * c;
        ALWAN_CORE_T root = ALWAN_CORE_SQRT(det);
        return -ALWAN_CORE_LITERAL(2.0) * c / (b + root);
    } else {
        ALWAN_CORE_T b = -(ALWAN_CORE_LITERAL(1.0) + M_scaled + max_J * a);
        ALWAN_CORE_T c = max_J * M_scaled + J;
        ALWAN_CORE_T det = b * b - ALWAN_CORE_LITERAL(4.0) * a * c;
        ALWAN_CORE_T root = ALWAN_CORE_SQRT(det);
        return -ALWAN_CORE_LITERAL(2.0) * c / (b - root);
    }
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_compression_vector_slope)(ALWAN_CORE_T J_intersect,
                                                            ALWAN_CORE_T focus_J,
                                                            ALWAN_CORE_T limit_J_max,
                                                            ALWAN_CORE_T slope_gain) {
    if (J_intersect < focus_J) {
        return slope_gain * (ALWAN_CORE_LITERAL(1.0) - J_intersect / focus_J);
    } else {
        return slope_gain * (J_intersect - focus_J) / (limit_J_max - focus_J);
    }
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_estimate_line_boundary_m)(ALWAN_CORE_T J_intersect_source,
                                                            ALWAN_CORE_T slope,
                                                            ALWAN_CORE_T gamma_inv,
                                                            ALWAN_CORE_T cusp_J,
                                                            ALWAN_CORE_T cusp_M,
                                                            ALWAN_CORE_T J_intersect_cusp) {
    ALWAN_CORE_T denom = J_intersect_cusp - cusp_J;
    if (ALWAN_CORE_ABS(denom) < ALWAN_CORE_LITERAL(1e-6)) {
        if (cusp_J > ALWAN_CORE_LITERAL(0.0)) {
            return cusp_M * J_intersect_source / cusp_J;
        }
        return cusp_M;
    }

    ALWAN_CORE_T t = (J_intersect_source - cusp_J) / denom;
    if (t < ALWAN_CORE_LITERAL(0.0)) t = ALWAN_CORE_LITERAL(0.0);
    if (t > ALWAN_CORE_LITERAL(1.0)) t = ALWAN_CORE_LITERAL(1.0);

    ALWAN_CORE_T M_boundary = cusp_M * ALWAN_CORE_POW(t, gamma_inv);

    ALWAN_CORE_T J_diff = ALWAN_CORE_ABS(J_intersect_source - cusp_J);
    if (slope != ALWAN_CORE_LITERAL(0.0)) {
        M_boundary = M_boundary + J_diff / ALWAN_CORE_ABS(slope);
    }

    return M_boundary;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(aces2_find_gamut_boundary)(ALWAN_CORE_T cusp_J,
                                                       ALWAN_CORE_T cusp_M,
                                                       ALWAN_CORE_T limit_J_max,
                                                       ALWAN_CORE_T gamma_top_inv,
                                                       ALWAN_CORE_T gamma_bottom_inv,
                                                       ALWAN_CORE_T J_intersect_source,
                                                       ALWAN_CORE_T slope,
                                                       ALWAN_CORE_T J_intersect_cusp) {
    ALWAN_CORE_T M_boundary_lower = ALWAN_CORE_FNVLIT(aces2_estimate_line_boundary_m)(
        J_intersect_source, slope, gamma_bottom_inv,
        cusp_J, cusp_M, J_intersect_cusp);

    ALWAN_CORE_T f_J_intersect_cusp = limit_J_max - J_intersect_cusp;
    ALWAN_CORE_T f_J_intersect_source = limit_J_max - J_intersect_source;
    ALWAN_CORE_T f_cusp_J = limit_J_max - cusp_J;

    ALWAN_CORE_T M_boundary_upper = ALWAN_CORE_FNVLIT(aces2_estimate_line_boundary_m)(
        f_J_intersect_source, -slope, gamma_top_inv,
        f_cusp_J, cusp_M, f_J_intersect_cusp);

    return ALWAN_CORE_FNVLIT(aces2_smin_scaled)(M_boundary_lower, M_boundary_upper, cusp_M);
}

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(aces2_compress_gamut_fwd)(ALWAN_CORE_T J, ALWAN_CORE_T M, ALWAN_CORE_T h,
                                                    ALWAN_CORE_FNLIT(aces2_GamutCompressParams) const *gcp,
                                                    ALWAN_CORE_FNLIT(aces2_HueDependentGamutParams) const *hdp) {
    ALWAN_CORE_VEC2 result;
    ALWAN_UNUSED(h);

    ALWAN_CORE_T slope_gain = ALWAN_CORE_FNVLIT(aces2_get_focus_gain)(J, hdp->analytical_threshold,
                                                      gcp->limit_J_max, gcp->focus_dist);

    ALWAN_CORE_T J_intersect = ALWAN_CORE_FNVLIT(aces2_solve_j_intersect)(J, M, hdp->focus_J,
                                                          gcp->limit_J_max, slope_gain);

    ALWAN_CORE_T gamut_slope = ALWAN_CORE_FNVLIT(aces2_compression_vector_slope)(
        J_intersect, hdp->focus_J, gcp->limit_J_max, slope_gain);

    ALWAN_CORE_T J_intersect_cusp = ALWAN_CORE_FNVLIT(aces2_solve_j_intersect)(
        hdp->cusp_J, hdp->cusp_M, hdp->focus_J, gcp->limit_J_max, slope_gain);

    ALWAN_CORE_T gamut_boundary_M = ALWAN_CORE_FNVLIT(aces2_find_gamut_boundary)(
        hdp->cusp_J, hdp->cusp_M, gcp->limit_J_max,
        hdp->gamma_top_inv, hdp->gamma_bottom_inv,
        J_intersect, gamut_slope, J_intersect_cusp);

    if (gamut_boundary_M <= ALWAN_CORE_LITERAL(0.0)) {
        result.v[0] = J;
        result.v[1] = ALWAN_CORE_LITERAL(0.0);
        return result;
    }

    ALWAN_CORE_T reach_boundary_M = ALWAN_CORE_FNVLIT(aces2_estimate_line_boundary_m)(
        J_intersect, gamut_slope, gcp->model_gamma_inv,
        gcp->limit_J_max, gcp->reach_max_M, gcp->limit_J_max);

    ALWAN_CORE_T remapped_M = ALWAN_CORE_FNVLIT(aces2_remap_m_fwd)(M, gamut_boundary_M, reach_boundary_M);

    result.v[0] = J_intersect + remapped_M * gamut_slope;
    result.v[1] = remapped_M;
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(aces2_compress_gamut_inv)(ALWAN_CORE_T J, ALWAN_CORE_T M, ALWAN_CORE_T h,
                                                    ALWAN_CORE_FNLIT(aces2_GamutCompressParams) const *gcp,
                                                    ALWAN_CORE_FNLIT(aces2_HueDependentGamutParams) const *hdp) {
    ALWAN_CORE_VEC2 result;
    ALWAN_UNUSED(h);

    ALWAN_CORE_T slope_gain = ALWAN_CORE_FNVLIT(aces2_get_focus_gain)(J, hdp->analytical_threshold,
                                                      gcp->limit_J_max, gcp->focus_dist);

    ALWAN_CORE_T J_intersect = ALWAN_CORE_FNVLIT(aces2_solve_j_intersect)(J, M, hdp->focus_J,
                                                          gcp->limit_J_max, slope_gain);

    ALWAN_CORE_T gamut_slope = ALWAN_CORE_FNVLIT(aces2_compression_vector_slope)(
        J_intersect, hdp->focus_J, gcp->limit_J_max, slope_gain);

    ALWAN_CORE_T J_intersect_cusp = ALWAN_CORE_FNVLIT(aces2_solve_j_intersect)(
        hdp->cusp_J, hdp->cusp_M, hdp->focus_J, gcp->limit_J_max, slope_gain);

    ALWAN_CORE_T gamut_boundary_M = ALWAN_CORE_FNVLIT(aces2_find_gamut_boundary)(
        hdp->cusp_J, hdp->cusp_M, gcp->limit_J_max,
        hdp->gamma_top_inv, hdp->gamma_bottom_inv,
        J_intersect, gamut_slope, J_intersect_cusp);

    if (gamut_boundary_M <= ALWAN_CORE_LITERAL(0.0)) {
        result.v[0] = J;
        result.v[1] = ALWAN_CORE_LITERAL(0.0);
        return result;
    }

    ALWAN_CORE_T reach_boundary_M = ALWAN_CORE_FNVLIT(aces2_estimate_line_boundary_m)(
        J_intersect, gamut_slope, gcp->model_gamma_inv,
        gcp->limit_J_max, gcp->reach_max_M, gcp->limit_J_max);

    ALWAN_CORE_T remapped_M = ALWAN_CORE_FNVLIT(aces2_remap_m_inv)(M, gamut_boundary_M, reach_boundary_M);

    result.v[0] = J_intersect + remapped_M * gamut_slope;
    result.v[1] = remapped_M;
    return result;
}

/* ================================================================
 * Surround Compensation
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(aces_dark_to_dim10)(ALWAN_CORE_VEC3 rgb) {
    ALWAN_CORE_T const MIN_LUM = ALWAN_CORE_LITERAL(1e-10);
    ALWAN_CORE_T const Y_r = ALWAN_CORE_LITERAL(0.27222871678091454);
    ALWAN_CORE_T const Y_g = ALWAN_CORE_LITERAL(0.67408176581114831);
    ALWAN_CORE_T const Y_b = ALWAN_CORE_LITERAL(0.053689517407937051);
    ALWAN_CORE_T const GAMMA = ALWAN_CORE_LITERAL(-0.0189);

    ALWAN_CORE_T Y = Y_r * rgb.v[0] + Y_g * rgb.v[1] + Y_b * rgb.v[2];
    if (Y < MIN_LUM) Y = MIN_LUM;
    ALWAN_CORE_T scale = ALWAN_CORE_POW(Y, GAMMA);

    ALWAN_CORE_VEC3 result;
    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(aces_rec2100_surround)(ALWAN_CORE_VEC3 rgb, ALWAN_CORE_T gamma) {
    ALWAN_CORE_T const MIN_LUM = ALWAN_CORE_LITERAL(1e-4);
    ALWAN_CORE_T const Y_r = ALWAN_CORE_LITERAL(0.2627);
    ALWAN_CORE_T const Y_g = ALWAN_CORE_LITERAL(0.6780);
    ALWAN_CORE_T const Y_b = ALWAN_CORE_LITERAL(0.0593);

    ALWAN_CORE_T Y = Y_r * rgb.v[0] + Y_g * rgb.v[1] + Y_b * rgb.v[2];
    Y = ALWAN_CORE_ABS(Y);
    if (Y < MIN_LUM) Y = MIN_LUM;
    ALWAN_CORE_T m_gamma = gamma - ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T scale = ALWAN_CORE_POW(Y, m_gamma);

    ALWAN_CORE_VEC3 result;
    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

/* ================================================================
 * Look Modification Transform (LMT)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(aces_lmt_apply)(ALWAN_CORE_VEC3 rgb,
                                          ALWAN_CORE_VEC3 slope, ALWAN_CORE_VEC3 offset, ALWAN_CORE_VEC3 power,
                                          ALWAN_CORE_T saturation) {
    ALWAN_CORE_T const LUM_R = ALWAN_CORE_LITERAL(0.27222871678091454);
    ALWAN_CORE_T const LUM_G = ALWAN_CORE_LITERAL(0.67408176581114831);
    ALWAN_CORE_T const LUM_B = ALWAN_CORE_LITERAL(0.053689517407937051);

    ALWAN_CORE_T r = rgb.v[0] * slope.v[0] + offset.v[0];
    ALWAN_CORE_T g = rgb.v[1] * slope.v[1] + offset.v[1];
    ALWAN_CORE_T b = rgb.v[2] * slope.v[2] + offset.v[2];

    if (r > ALWAN_CORE_LITERAL(0.0)) r = ALWAN_CORE_POW(r, power.v[0]);
    if (g > ALWAN_CORE_LITERAL(0.0)) g = ALWAN_CORE_POW(g, power.v[1]);
    if (b > ALWAN_CORE_LITERAL(0.0)) b = ALWAN_CORE_POW(b, power.v[2]);

    if (saturation != ALWAN_CORE_LITERAL(1.0)) {
        ALWAN_CORE_T lum = LUM_R * r + LUM_G * g + LUM_B * b;
        r = lum + saturation * (r - lum);
        g = lum + saturation * (g - lum);
        b = lum + saturation * (b - lum);
    }

    ALWAN_CORE_VEC3 result = {{r, g, b}};
    return result;
}

/* Clean up local constant macros */
#undef ACES_CAM_NL_OFFSET_
#undef ACES_J_SCALE_
#undef ACES_GAMUT_COMPRESSION_THRESHOLD_
#undef ACES_GAMUT_SMOOTH_CUSPS_
#undef ACES_GAMUT_CUSP_MID_BLEND_
