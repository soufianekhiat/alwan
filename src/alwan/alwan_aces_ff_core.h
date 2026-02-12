/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ACES Fixed Functions (per-pixel math)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: OpenColorIO FixedFunctionOpCPU.cpp,
 *            ACES CTL Lib.Academy.OutputTransform.ctl
 */

#ifndef ALWAN_ACES_FF_CORE_H
#define ALWAN_ACES_FF_CORE_H

#include "alwan_platform.h"
#include "alwan_types.h"

/* ================================================================
 * Constants
 * ================================================================ */

#define ACES_CAM_NL_OFFSET  ALWAN_LITERAL(27.13)
#define ACES_J_SCALE        ALWAN_LITERAL(100.0)
#define ACES_GAMUT_COMPRESSION_THRESHOLD ALWAN_LITERAL(0.75)
#define ACES_GAMUT_SMOOTH_CUSPS ALWAN_LITERAL(0.12)
#define ACES_GAMUT_CUSP_MID_BLEND ALWAN_LITERAL(1.3)
#define ACES2_REACH_TABLE_SIZE 360
#define ACES2_CUSP_TABLE_SIZE 362

/* Fourier coefficient arrays for chroma_compress_norm */
ALWAN_CONSTEXPR alwan_scalar ACES2_CHROMA_NORM_COS_V[4] = {
    ALWAN_LITERAL(11.34072), ALWAN_LITERAL(16.46899),
    ALWAN_LITERAL(7.88380),  ALWAN_LITERAL(0.0)
};
ALWAN_CONSTEXPR alwan_scalar ACES2_CHROMA_NORM_SIN_V[4] = {
    ALWAN_LITERAL(14.66441), ALWAN_LITERAL(-6.37224),
    ALWAN_LITERAL(9.19364),  ALWAN_LITERAL(77.12896)
};

/* ================================================================
 * Struct Definitions
 * ================================================================ */

typedef struct {
    alwan_scalar MATRIX_RGB_to_CAM16_c[9];
    alwan_scalar MATRIX_cone_response_to_Aab[9];
    alwan_scalar MATRIX_CAM16_to_RGB[9];
    alwan_scalar MATRIX_Aab_to_cone[9];
    alwan_scalar cz;
    alwan_scalar inv_cz;
    alwan_scalar A_w_J;
    alwan_scalar inv_A_w_J;
    alwan_scalar F_L_n;
} aces2_JMhParams;

typedef struct {
    alwan_scalar n, n_r, g, t_1, s_2, u_2, m_2;
} aces2_TSParams;

typedef struct {
    alwan_scalar J, M, gamma_top_inv;
} aces2_GamutCuspEntry;

typedef struct {
    alwan_scalar limit_J_max, mid_J, focus_dist;
    alwan_scalar lower_hull_gamma_inv, model_gamma_inv, reach_max_M;
    aces2_GamutCuspEntry cusp_table[ACES2_CUSP_TABLE_SIZE];
    alwan_scalar reach_m_table[ACES2_REACH_TABLE_SIZE];
    alwan_scalar hue_table[ACES2_CUSP_TABLE_SIZE];
} aces2_GamutCompressParams;

typedef struct {
    alwan_scalar cusp_J, cusp_M, gamma_top_inv, gamma_bottom_inv;
    alwan_scalar focus_J, analytical_threshold;
} aces2_HueDependentGamutParams;

/* ================================================================
 * Transfer Functions
 * ================================================================ */

/* BT.1886 OETF (gamma 2.4 inverse) */
ALWAN_INLINE alwan_scalar aces_bt1886_oetf_v(alwan_scalar x) {
    if (x <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    return ALWAN_POW(x, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
}

/* sRGB OETF */
ALWAN_INLINE alwan_scalar aces_srgb_oetf_v(alwan_scalar x) {
    if (x <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    if (x <= ALWAN_LITERAL(0.0031308)) {
        return x * ALWAN_LITERAL(12.92);
    }
    return ALWAN_LITERAL(1.055) * ALWAN_POW(x, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4)) - ALWAN_LITERAL(0.055);
}

/* Gamma 2.6 OETF (cinema) */
ALWAN_INLINE alwan_scalar aces_gamma26_oetf_v(alwan_scalar x) {
    if (x <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    return ALWAN_POW(x, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.6));
}

/* PQ (ST.2084) OETF */
ALWAN_INLINE alwan_scalar aces_pq_oetf_v(alwan_scalar Y, alwan_scalar peak_nits) {
    alwan_scalar m1 = ALWAN_LITERAL(0.1593017578125);
    alwan_scalar m2 = ALWAN_LITERAL(78.84375);
    alwan_scalar c1 = ALWAN_LITERAL(0.8359375);
    alwan_scalar c2 = ALWAN_LITERAL(18.8515625);
    alwan_scalar c3 = ALWAN_LITERAL(18.6875);

    alwan_scalar L = alwan_max(Y * peak_nits / ALWAN_LITERAL(10000.0), ALWAN_LITERAL(0.0));
    alwan_scalar Lm1 = ALWAN_POW(L, m1);
    alwan_scalar num = c1 + c2 * Lm1;
    alwan_scalar den = ALWAN_LITERAL(1.0) + c3 * Lm1;
    return ALWAN_POW(num / den, m2);
}

/* BT.1886 EOTF (gamma 2.4) */
ALWAN_INLINE alwan_scalar aces_bt1886_eotf_v(alwan_scalar x) {
    if (x <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    return ALWAN_POW(x, ALWAN_LITERAL(2.4));
}

/* sRGB EOTF */
ALWAN_INLINE alwan_scalar aces_srgb_eotf_v(alwan_scalar x) {
    if (x <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    if (x <= ALWAN_LITERAL(0.04045)) {
        return x / ALWAN_LITERAL(12.92);
    }
    return ALWAN_POW((x + ALWAN_LITERAL(0.055)) / ALWAN_LITERAL(1.055), ALWAN_LITERAL(2.4));
}

/* Gamma 2.6 EOTF */
ALWAN_INLINE alwan_scalar aces_gamma26_eotf_v(alwan_scalar x) {
    if (x <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    return ALWAN_POW(x, ALWAN_LITERAL(2.6));
}

/* PQ (ST.2084) EOTF */
ALWAN_INLINE alwan_scalar aces_pq_eotf_v(alwan_scalar E, alwan_scalar peak_nits) {
    alwan_scalar m1 = ALWAN_LITERAL(0.1593017578125);
    alwan_scalar m2 = ALWAN_LITERAL(78.84375);
    alwan_scalar c1 = ALWAN_LITERAL(0.8359375);
    alwan_scalar c2 = ALWAN_LITERAL(18.8515625);
    alwan_scalar c3 = ALWAN_LITERAL(18.6875);

    alwan_scalar Em2 = ALWAN_POW(alwan_max(E, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(1.0) / m2);
    alwan_scalar num = alwan_max(Em2 - c1, ALWAN_LITERAL(0.0));
    alwan_scalar den = c2 - c3 * Em2;
    if (den <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    alwan_scalar Y = ALWAN_POW(num / den, ALWAN_LITERAL(1.0) / m1);
    return Y * ALWAN_LITERAL(10000.0) / peak_nits;
}

/* ================================================================
 * RedMod / Glow Helpers
 * ================================================================ */

/* Saturation weight calculation */
ALWAN_INLINE alwan_scalar aces_calc_sat_weight_v(alwan_scalar red, alwan_scalar grn,
                                                  alwan_scalar blu, alwan_scalar noise_limit) {
    alwan_scalar min_val = alwan_min3(red, grn, blu);
    alwan_scalar max_val = alwan_max3(red, grn, blu);

    alwan_scalar clamped_max = max_val > ALWAN_LITERAL(1e-10) ? max_val : ALWAN_LITERAL(1e-10);
    alwan_scalar clamped_min = min_val > ALWAN_LITERAL(1e-10) ? min_val : ALWAN_LITERAL(1e-10);
    alwan_scalar denom = max_val > noise_limit ? max_val : noise_limit;

    alwan_scalar sat = (clamped_max - clamped_min) / denom;
    return sat;
}

/* Hue weight calculation using B-spline */
ALWAN_INLINE alwan_scalar aces_calc_hue_weight_v(alwan_scalar red, alwan_scalar grn,
                                                  alwan_scalar blu, alwan_scalar inv_width) {
    alwan_scalar sqrt3 = ALWAN_LITERAL(1.7320508075688772);

    alwan_scalar a = ALWAN_LITERAL(2.0) * red - (grn + blu);
    alwan_scalar b = sqrt3 * (grn - blu);
    alwan_scalar hue = ALWAN_ATAN2(b, a);

    alwan_scalar knot_coord = hue * inv_width + ALWAN_LITERAL(2.0);
    int j = (int)knot_coord;

    /* B-spline matrix coefficients (row-major) */
    alwan_scalar M0[4] = { ALWAN_LITERAL(0.25),  ALWAN_LITERAL(0.00),  ALWAN_LITERAL(0.00),  ALWAN_LITERAL(0.00)};
    alwan_scalar M1[4] = {ALWAN_LITERAL(-0.75),  ALWAN_LITERAL(0.75),  ALWAN_LITERAL(0.75),  ALWAN_LITERAL(0.25)};
    alwan_scalar M2[4] = { ALWAN_LITERAL(0.75), ALWAN_LITERAL(-1.50),  ALWAN_LITERAL(0.00),  ALWAN_LITERAL(1.00)};
    alwan_scalar M3[4] = {ALWAN_LITERAL(-0.25),  ALWAN_LITERAL(0.75), ALWAN_LITERAL(-0.75),  ALWAN_LITERAL(0.25)};

    alwan_scalar f_H = ALWAN_LITERAL(0.0);
    if (j >= 0 && j < 4) {
        alwan_scalar t = knot_coord - (alwan_scalar)j;
        alwan_scalar c0, c1_v, c2_v, c3_v;
        if (j == 0)      { c0 = M0[0]; c1_v = M0[1]; c2_v = M0[2]; c3_v = M0[3]; }
        else if (j == 1)  { c0 = M1[0]; c1_v = M1[1]; c2_v = M1[2]; c3_v = M1[3]; }
        else if (j == 2)  { c0 = M2[0]; c1_v = M2[1]; c2_v = M2[2]; c3_v = M2[3]; }
        else              { c0 = M3[0]; c1_v = M3[1]; c2_v = M3[2]; c3_v = M3[3]; }
        f_H = c3_v + t * (c2_v + t * (c1_v + t * c0));
    }

    return f_H;
}

/* YC (luminance with chroma weighting) calculation */
ALWAN_INLINE alwan_scalar aces_rgb_to_yc_v(alwan_scalar red, alwan_scalar grn, alwan_scalar blu) {
    alwan_scalar YC_RADIUS_WEIGHT = ALWAN_LITERAL(1.75);

    alwan_scalar chroma = ALWAN_SQRT(blu * (blu - grn) + grn * (grn - red) + red * (red - blu));
    alwan_scalar YC = (blu + grn + red + YC_RADIUS_WEIGHT * chroma) / ALWAN_LITERAL(3.0);

    return YC;
}

/* Sigmoid shaper for saturation */
ALWAN_INLINE alwan_scalar aces_sigmoid_shaper_v(alwan_scalar sat) {
    alwan_scalar x = (sat - ALWAN_LITERAL(0.4)) * ALWAN_LITERAL(5.0);
    alwan_scalar sign = (x >= ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(-1.0);
    alwan_scalar t = ALWAN_LITERAL(1.0) - ALWAN_LITERAL(0.5) * sign * x;
    if (t < ALWAN_LITERAL(0.0)) t = ALWAN_LITERAL(0.0);
    alwan_scalar s = (ALWAN_LITERAL(1.0) + sign * (ALWAN_LITERAL(1.0) - t * t)) * ALWAN_LITERAL(0.5);
    return s;
}

/* ================================================================
 * GamutComp13 Helpers
 * ================================================================ */

/* Compression function (forward direction) */
ALWAN_INLINE alwan_scalar aces_compress_dist_v(alwan_scalar dist, alwan_scalar thr,
                                                alwan_scalar scale, alwan_scalar power) {
    alwan_scalar nd = (dist - thr) / scale;
    alwan_scalar p = ALWAN_POW(nd, power);
    return thr + scale * nd / ALWAN_POW(ALWAN_LITERAL(1.0) + p, ALWAN_LITERAL(1.0) / power);
}

/* Decompression function (inverse direction) */
ALWAN_INLINE alwan_scalar aces_uncompress_dist_v(alwan_scalar compressed_dist, alwan_scalar thr,
                                                  alwan_scalar scale, alwan_scalar power) {
    alwan_scalar ndc = (compressed_dist - thr) / scale;
    if (ndc <= ALWAN_LITERAL(0.0)) {
        return compressed_dist;
    }
    alwan_scalar p = ALWAN_POW(ndc, power);
    if (p >= ALWAN_LITERAL(1.0)) {
        return thr + scale * ndc * ALWAN_LITERAL(100.0);
    }
    alwan_scalar nd = ndc / ALWAN_POW(ALWAN_LITERAL(1.0) - p, ALWAN_LITERAL(1.0) / power);
    return thr + scale * nd;
}

/* Per-channel gamut compression */
ALWAN_INLINE alwan_scalar aces_gamut_comp_channel_v(alwan_scalar val, alwan_scalar ach,
                                                     alwan_scalar thr, alwan_scalar scale,
                                                     alwan_scalar power) {
    if (ach == ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }

    alwan_scalar dist = (ach - val) / ALWAN_ABS(ach);

    if (dist < thr) {
        return val;
    }

    alwan_scalar compr_dist = aces_compress_dist_v(dist, thr, scale, power);
    alwan_scalar compr = ach - compr_dist * ALWAN_ABS(ach);

    return compr;
}

/* Per-channel gamut decompression (inverse) */
ALWAN_INLINE alwan_scalar aces_gamut_decomp_channel_v(alwan_scalar val, alwan_scalar ach,
                                                       alwan_scalar thr, alwan_scalar scale,
                                                       alwan_scalar power) {
    if (ach == ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }

    alwan_scalar dist = (ach - val) / ALWAN_ABS(ach);

    if (dist < thr) {
        return val;
    }

    alwan_scalar decompr_dist = aces_uncompress_dist_v(dist, thr, scale, power);
    alwan_scalar decompr = ach - decompr_dist * ALWAN_ABS(ach);

    return decompr;
}

/* Compute scale from limit, threshold and power */
ALWAN_INLINE alwan_scalar aces_calc_gamut_comp_scale_v(alwan_scalar lim, alwan_scalar thr,
                                                        alwan_scalar power) {
    alwan_scalar base = (ALWAN_LITERAL(1.0) - thr) / (lim - thr);
    alwan_scalar inner = ALWAN_POW(base, -power) - ALWAN_LITERAL(1.0);
    alwan_scalar denom = ALWAN_POW(inner, ALWAN_LITERAL(1.0) / power);
    return (lim - thr) / denom;
}

/* ================================================================
 * ACES1 Helpers
 * ================================================================ */

/* Calculate saturation as (max - min) / max */
ALWAN_INLINE alwan_scalar aces1_saturation_v(alwan_scalar r, alwan_scalar g, alwan_scalar b) {
    alwan_scalar mx = alwan_max3(r, g, b);
    alwan_scalar mn = alwan_min3(r, g, b);
    if (mx <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    return (mx - mn) / mx;
}

/* Convert RGB to hue in degrees [0, 360) */
ALWAN_INLINE alwan_scalar aces1_rgb_to_hue_v(alwan_scalar r, alwan_scalar g, alwan_scalar b) {
    alwan_scalar mx = alwan_max3(r, g, b);
    alwan_scalar mn = alwan_min3(r, g, b);
    alwan_scalar chroma = mx - mn;

    if (chroma <= ALWAN_LITERAL(1e-10)) return ALWAN_LITERAL(0.0);

    alwan_scalar hue;
    if (mx == r) {
        hue = (g - b) / chroma;
        if (hue < ALWAN_LITERAL(0.0)) hue += ALWAN_LITERAL(6.0);
    } else if (mx == g) {
        hue = ALWAN_LITERAL(2.0) + (b - r) / chroma;
    } else {
        hue = ALWAN_LITERAL(4.0) + (r - g) / chroma;
    }
    return hue * ALWAN_LITERAL(60.0);
}

/* Center hue around a target hue */
ALWAN_INLINE alwan_scalar aces1_center_hue_v(alwan_scalar hue, alwan_scalar center) {
    alwan_scalar centered = hue - center;
    if (centered < ALWAN_LITERAL(-180.0)) centered += ALWAN_LITERAL(360.0);
    if (centered > ALWAN_LITERAL(180.0)) centered -= ALWAN_LITERAL(360.0);
    return centered;
}

/* Cubic basis shaper - smooth falloff from center */
ALWAN_INLINE alwan_scalar aces1_cubic_basis_shaper_v(alwan_scalar x, alwan_scalar width) {
    alwan_scalar t = x / width;
    if (t < ALWAN_LITERAL(-1.0) || t > ALWAN_LITERAL(1.0)) return ALWAN_LITERAL(0.0);
    alwan_scalar t2 = t * t;
    return ALWAN_LITERAL(1.0) - ALWAN_LITERAL(3.0) * t2 + ALWAN_LITERAL(2.0) * t2 * ALWAN_ABS(t);
}

/* Segmented spline function for RRT tone scale (c5 variant) */
ALWAN_INLINE alwan_scalar aces1_segmented_spline_c5_v(alwan_scalar x) {
    alwan_scalar min_pt_x = ALWAN_LITERAL(5.4931640625e-6);
    alwan_scalar min_pt_y = ALWAN_LITERAL(0.0001);
    alwan_scalar mid_pt_x = ALWAN_LITERAL(0.18);
    alwan_scalar mid_pt_y = ALWAN_LITERAL(4.8);
    alwan_scalar max_pt_x = ALWAN_LITERAL(47185.92);
    alwan_scalar max_pt_y = ALWAN_LITERAL(48.0);

    alwan_scalar log_x = ALWAN_LOG10(alwan_max(x, ALWAN_LITERAL(1e-10)));
    alwan_scalar log_min = ALWAN_LOG10(min_pt_x);
    alwan_scalar log_mid = ALWAN_LOG10(mid_pt_x);
    alwan_scalar log_max = ALWAN_LOG10(max_pt_x);

    alwan_scalar t;
    if (log_x <= log_min) {
        return min_pt_y;
    } else if (log_x >= log_max) {
        return max_pt_y;
    } else if (log_x < log_mid) {
        t = (log_x - log_min) / (log_mid - log_min);
        alwan_scalar t2 = t * t;
        alwan_scalar t3 = t2 * t;
        return min_pt_y + (mid_pt_y - min_pt_y) * (ALWAN_LITERAL(3.0) * t2 - ALWAN_LITERAL(2.0) * t3);
    } else {
        t = (log_x - log_mid) / (log_max - log_mid);
        alwan_scalar t2 = t * t;
        alwan_scalar t3 = t2 * t;
        return mid_pt_y + (max_pt_y - mid_pt_y) * (ALWAN_LITERAL(3.0) * t2 - ALWAN_LITERAL(2.0) * t3);
    }
}

/* Inverse segmented spline c5 (Newton-Raphson) */
ALWAN_INLINE alwan_scalar aces1_segmented_spline_c5_inv_v(alwan_scalar y) {
    alwan_scalar min_pt_x = ALWAN_LITERAL(5.4931640625e-6);
    alwan_scalar min_pt_y = ALWAN_LITERAL(0.0001);
    alwan_scalar mid_pt_x = ALWAN_LITERAL(0.18);
    alwan_scalar mid_pt_y = ALWAN_LITERAL(4.8);
    alwan_scalar max_pt_x = ALWAN_LITERAL(47185.92);
    alwan_scalar max_pt_y = ALWAN_LITERAL(48.0);

    alwan_scalar target_nits = y * ALWAN_LITERAL(48.0);

    if (target_nits <= min_pt_y) return min_pt_x;
    if (target_nits >= max_pt_y) return max_pt_x;

    alwan_scalar x;
    if (target_nits <= mid_pt_y) {
        alwan_scalar t = (target_nits - min_pt_y) / (mid_pt_y - min_pt_y);
        alwan_scalar log_min = ALWAN_LOG10(min_pt_x);
        alwan_scalar log_mid = ALWAN_LOG10(mid_pt_x);
        x = ALWAN_POW(ALWAN_LITERAL(10.0), log_min + t * (log_mid - log_min));
    } else {
        alwan_scalar t = (target_nits - mid_pt_y) / (max_pt_y - mid_pt_y);
        alwan_scalar log_mid = ALWAN_LOG10(mid_pt_x);
        alwan_scalar log_max = ALWAN_LOG10(max_pt_x);
        x = ALWAN_POW(ALWAN_LITERAL(10.0), log_mid + t * (log_max - log_mid));
    }

    for (int i = 0; i < 30; i++) {
        alwan_scalar fx = aces1_segmented_spline_c5_v(x) - target_nits;
        if (ALWAN_ABS(fx) < ALWAN_LITERAL(1e-10)) break;

        alwan_scalar h = alwan_max(ALWAN_ABS(x) * ALWAN_LITERAL(1e-6), ALWAN_LITERAL(1e-12));
        alwan_scalar dfx = (aces1_segmented_spline_c5_v(x + h) - aces1_segmented_spline_c5_v(x - h)) / (ALWAN_LITERAL(2.0) * h);
        if (ALWAN_ABS(dfx) < ALWAN_LITERAL(1e-12)) break;

        alwan_scalar dx = fx / dfx;

        if (ALWAN_ABS(dx) > x * ALWAN_LITERAL(0.5)) {
            dx = (dx > ALWAN_LITERAL(0.0)) ? x * ALWAN_LITERAL(0.5) : -x * ALWAN_LITERAL(0.5);
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

/* Compute RGB to XYZ matrix from chromaticities */
ALWAN_INLINE alwan_mat3x3 aces_primaries_to_rgb_to_xyz_v(alwan_scalar rx, alwan_scalar ry,
                                                          alwan_scalar gx, alwan_scalar gy,
                                                          alwan_scalar bx, alwan_scalar by,
                                                          alwan_scalar wx, alwan_scalar wy,
                                                          alwan_scalar Y) {
    alwan_mat3x3 out;

    alwan_scalar rX = rx / ry;
    alwan_scalar rY = ALWAN_LITERAL(1.0);
    alwan_scalar rZ = (ALWAN_LITERAL(1.0) - rx - ry) / ry;

    alwan_scalar gX = gx / gy;
    alwan_scalar gY = ALWAN_LITERAL(1.0);
    alwan_scalar gZ = (ALWAN_LITERAL(1.0) - gx - gy) / gy;

    alwan_scalar bX = bx / by;
    alwan_scalar bY = ALWAN_LITERAL(1.0);
    alwan_scalar bZ = (ALWAN_LITERAL(1.0) - bx - by) / by;

    alwan_scalar wX = wx * Y / wy;
    alwan_scalar wY = Y;
    alwan_scalar wZ = (ALWAN_LITERAL(1.0) - wx - wy) * Y / wy;

    alwan_scalar d = rX * (bY * gZ - gY * bZ) - gX * (bY * rZ - rY * bZ) + bX * (gY * rZ - rY * gZ);

    alwan_scalar Sr = (wX * (bY * gZ - gY * bZ) - gX * (bY * wZ - wY * bZ) + bX * (gY * wZ - wY * gZ)) / d;
    alwan_scalar Sg = (rX * (bY * wZ - wY * bZ) - wX * (bY * rZ - rY * bZ) + bX * (wY * rZ - rY * wZ)) / d;
    alwan_scalar Sb = (rX * (wY * gZ - gY * wZ) - gX * (wY * rZ - rY * wZ) + wX * (gY * rZ - rY * gZ)) / d;

    out.m[0] = Sr * rX; out.m[1] = Sg * gX; out.m[2] = Sb * bX;
    out.m[3] = Sr * rY; out.m[4] = Sg * gY; out.m[5] = Sb * bY;
    out.m[6] = Sr * rZ; out.m[7] = Sg * gZ; out.m[8] = Sb * bZ;

    return out;
}

/* Invert 3x3 matrix (det==0 returns zero matrix) */
ALWAN_INLINE alwan_mat3x3 aces_invert_mat3_v(alwan_mat3x3 const *m) {
    alwan_mat3x3 out;

    alwan_scalar det = m->m[0] * (m->m[4] * m->m[8] - m->m[5] * m->m[7])
                     - m->m[1] * (m->m[3] * m->m[8] - m->m[5] * m->m[6])
                     + m->m[2] * (m->m[3] * m->m[7] - m->m[4] * m->m[6]);

    if (ALWAN_ABS(det) < ALWAN_LITERAL(1e-10)) {
        for (int i = 0; i < 9; i++) out.m[i] = ALWAN_LITERAL(0.0);
        return out;
    }

    alwan_scalar inv_det = ALWAN_LITERAL(1.0) / det;

    out.m[0] = (m->m[4] * m->m[8] - m->m[5] * m->m[7]) * inv_det;
    out.m[1] = (m->m[2] * m->m[7] - m->m[1] * m->m[8]) * inv_det;
    out.m[2] = (m->m[1] * m->m[5] - m->m[2] * m->m[4]) * inv_det;
    out.m[3] = (m->m[5] * m->m[6] - m->m[3] * m->m[8]) * inv_det;
    out.m[4] = (m->m[0] * m->m[8] - m->m[2] * m->m[6]) * inv_det;
    out.m[5] = (m->m[2] * m->m[3] - m->m[0] * m->m[5]) * inv_det;
    out.m[6] = (m->m[3] * m->m[7] - m->m[4] * m->m[6]) * inv_det;
    out.m[7] = (m->m[1] * m->m[6] - m->m[0] * m->m[7]) * inv_det;
    out.m[8] = (m->m[0] * m->m[4] - m->m[1] * m->m[3]) * inv_det;

    return out;
}

/* Multiply 3x3 matrices: result = a * b */
ALWAN_INLINE alwan_mat3x3 aces_mult_mat3_v(alwan_mat3x3 const *a, alwan_mat3x3 const *b) {
    alwan_mat3x3 out;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out.m[i * 3 + j] = a->m[i * 3 + 0] * b->m[0 * 3 + j]
                              + a->m[i * 3 + 1] * b->m[1 * 3 + j]
                              + a->m[i * 3 + 2] * b->m[2 * 3 + j];
        }
    }
    return out;
}

/* Multiply vector by matrix: result = v * m (row-vector convention) */
ALWAN_INLINE alwan_vec3 aces_mult_vec_mat3_v(alwan_vec3 v, alwan_mat3x3 const *m) {
    alwan_vec3 out;
    out.v[0] = v.v[0] * m->m[0] + v.v[1] * m->m[1] + v.v[2] * m->m[2];
    out.v[1] = v.v[0] * m->m[3] + v.v[1] * m->m[4] + v.v[2] * m->m[5];
    out.v[2] = v.v[0] * m->m[6] + v.v[1] * m->m[7] + v.v[2] * m->m[8];
    return out;
}

/* Multiply matrix by vector: result = m * v (column-vector convention) */
ALWAN_INLINE alwan_vec3 aces_mat3_mul_vec3_v(alwan_mat3x3 const *m, alwan_vec3 v) {
    alwan_vec3 out;
    out.v[0] = m->m[0] * v.v[0] + m->m[1] * v.v[1] + m->m[2] * v.v[2];
    out.v[1] = m->m[3] * v.v[0] + m->m[4] * v.v[1] + m->m[5] * v.v[2];
    out.v[2] = m->m[6] * v.v[0] + m->m[7] * v.v[1] + m->m[8] * v.v[2];
    return out;
}

/* ================================================================
 * ACES2 CAM Helpers
 * ================================================================ */

/* Post-adaptation cone response compression (forward) */
ALWAN_INLINE alwan_scalar aces_cone_response_fwd_v(alwan_scalar v) {
    alwan_scalar abs_v = ALWAN_ABS(v);
    if (abs_v < ALWAN_LITERAL(1e-10)) return ALWAN_LITERAL(0.0);

    alwan_scalar F_L_Y = ALWAN_POW(abs_v, ALWAN_LITERAL(0.42));
    alwan_scalar Ra = F_L_Y / (ACES_CAM_NL_OFFSET + F_L_Y);

    return (v >= ALWAN_LITERAL(0.0)) ? Ra : -Ra;
}

/* Post-adaptation cone response compression (inverse) */
ALWAN_INLINE alwan_scalar aces_cone_response_inv_v(alwan_scalar Ra) {
    alwan_scalar sign = (Ra >= ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(-1.0);
    alwan_scalar Ra_abs = ALWAN_ABS(Ra);

    if (Ra_abs < ALWAN_LITERAL(1e-10)) return ALWAN_LITERAL(0.0);
    alwan_scalar Ra_lim = (Ra_abs < ALWAN_LITERAL(0.99)) ? Ra_abs : ALWAN_LITERAL(0.99);

    alwan_scalar F_L_Y = ACES_CAM_NL_OFFSET * Ra_lim / (ALWAN_LITERAL(1.0) - Ra_lim);
    alwan_scalar Rc = ALWAN_POW(F_L_Y, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.42));

    return sign * Rc;
}

/* Toe compression function (forward) */
ALWAN_INLINE alwan_scalar aces_toe_fwd_v(alwan_scalar x, alwan_scalar limit,
                                          alwan_scalar k1_in, alwan_scalar k2_in) {
    if (x > limit) return x;

    alwan_scalar k2 = (k2_in > ALWAN_LITERAL(0.001)) ? k2_in : ALWAN_LITERAL(0.001);
    alwan_scalar k1 = ALWAN_SQRT(k1_in * k1_in + k2 * k2);
    alwan_scalar k3 = (limit + k1) / (limit + k2);
    alwan_scalar minus_b = k3 * x - k1;
    alwan_scalar minus_ac = k2 * k3 * x;

    return ALWAN_LITERAL(0.5) * (minus_b + ALWAN_SQRT(minus_b * minus_b + ALWAN_LITERAL(4.0) * minus_ac));
}

/* Toe compression function (inverse) */
ALWAN_INLINE alwan_scalar aces_toe_inv_v(alwan_scalar x, alwan_scalar limit,
                                          alwan_scalar k1_in, alwan_scalar k2_in) {
    if (x > limit) return x;

    alwan_scalar k2 = (k2_in > ALWAN_LITERAL(0.001)) ? k2_in : ALWAN_LITERAL(0.001);
    alwan_scalar k1 = ALWAN_SQRT(k1_in * k1_in + k2 * k2);
    alwan_scalar k3 = (limit + k1) / (limit + k2);

    return (x * x + k1 * x) / (k3 * (x + k2));
}

/* Chroma compression normalization (Fourier series) */
ALWAN_INLINE alwan_scalar aces_chroma_compress_norm_v(alwan_scalar h_rad, alwan_scalar scale) {
    alwan_scalar cos_hr1 = ALWAN_COS(h_rad);
    alwan_scalar sin_hr1 = ALWAN_SIN(h_rad);

    alwan_scalar cos_hr2 = ALWAN_LITERAL(2.0) * cos_hr1 * cos_hr1 - ALWAN_LITERAL(1.0);
    alwan_scalar sin_hr2 = ALWAN_LITERAL(2.0) * cos_hr1 * sin_hr1;
    alwan_scalar cos_hr3 = ALWAN_LITERAL(4.0) * cos_hr1 * cos_hr1 * cos_hr1 - ALWAN_LITERAL(3.0) * cos_hr1;
    alwan_scalar sin_hr3 = ALWAN_LITERAL(3.0) * sin_hr1 - ALWAN_LITERAL(4.0) * sin_hr1 * sin_hr1 * sin_hr1;

    alwan_scalar M = ACES2_CHROMA_NORM_COS_V[0] * cos_hr1
                   + ACES2_CHROMA_NORM_COS_V[1] * cos_hr2
                   + ACES2_CHROMA_NORM_COS_V[2] * cos_hr3
                   + ACES2_CHROMA_NORM_SIN_V[0] * sin_hr1
                   + ACES2_CHROMA_NORM_SIN_V[1] * sin_hr2
                   + ACES2_CHROMA_NORM_SIN_V[2] * sin_hr3
                   + ACES2_CHROMA_NORM_SIN_V[3];

    return M * scale;
}

/* ================================================================
 * ACES2 Per-pixel Conversions
 * ================================================================ */

/* RGB to Aab (adapted opponent color space) */
ALWAN_INLINE alwan_vec3 aces2_rgb_to_aab_v(alwan_vec3 rgb, aces2_JMhParams const *p) {
    alwan_mat3x3 mat_rgb_to_cam16;
    for (int i = 0; i < 9; i++) mat_rgb_to_cam16.m[i] = p->MATRIX_RGB_to_CAM16_c[i];

    alwan_vec3 rgb_m = aces_mult_vec_mat3_v(rgb, &mat_rgb_to_cam16);

    alwan_vec3 rgb_a;
    rgb_a.v[0] = aces_cone_response_fwd_v(rgb_m.v[0]);
    rgb_a.v[1] = aces_cone_response_fwd_v(rgb_m.v[1]);
    rgb_a.v[2] = aces_cone_response_fwd_v(rgb_m.v[2]);

    alwan_mat3x3 mat_cone_to_aab;
    for (int i = 0; i < 9; i++) mat_cone_to_aab.m[i] = p->MATRIX_cone_response_to_Aab[i];

    return aces_mult_vec_mat3_v(rgb_a, &mat_cone_to_aab);
}

/* Aab to JMh (lightness, colorfulness, hue) */
ALWAN_INLINE alwan_vec3 aces2_aab_to_jmh_v(alwan_vec3 aab, aces2_JMhParams const *p) {
    alwan_vec3 jmh;

    if (aab.v[0] <= ALWAN_LITERAL(0.0)) {
        jmh.v[0] = ALWAN_LITERAL(0.0);
        jmh.v[1] = ALWAN_LITERAL(0.0);
        jmh.v[2] = ALWAN_LITERAL(0.0);
        return jmh;
    }

    jmh.v[0] = ACES_J_SCALE * ALWAN_POW(aab.v[0], p->cz);
    jmh.v[1] = ALWAN_SQRT(aab.v[1] * aab.v[1] + aab.v[2] * aab.v[2]);

    alwan_scalar h_rad = ALWAN_ATAN2(aab.v[2], aab.v[1]);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;

    if (h_deg < ALWAN_LITERAL(0.0)) {
        h_deg += ALWAN_LITERAL(360.0);
    }
    jmh.v[2] = h_deg;

    return jmh;
}

/* JMh to Aab (inverse) */
ALWAN_INLINE alwan_vec3 aces2_jmh_to_aab_v(alwan_vec3 jmh, aces2_JMhParams const *p) {
    alwan_vec3 aab;

    if (jmh.v[0] <= ALWAN_LITERAL(0.0)) {
        aab.v[0] = ALWAN_LITERAL(0.0);
        aab.v[1] = ALWAN_LITERAL(0.0);
        aab.v[2] = ALWAN_LITERAL(0.0);
        return aab;
    }

    aab.v[0] = ALWAN_POW(jmh.v[0] / ACES_J_SCALE, p->inv_cz);

    alwan_scalar h_rad = jmh.v[2] * ALWAN_PI / ALWAN_LITERAL(180.0);

    aab.v[1] = jmh.v[1] * ALWAN_COS(h_rad);
    aab.v[2] = jmh.v[1] * ALWAN_SIN(h_rad);

    return aab;
}

/* Aab to RGB (inverse) */
ALWAN_INLINE alwan_vec3 aces2_aab_to_rgb_v(alwan_vec3 aab, aces2_JMhParams const *p) {
    alwan_mat3x3 mat_aab_to_cone;
    for (int i = 0; i < 9; i++) mat_aab_to_cone.m[i] = p->MATRIX_Aab_to_cone[i];

    alwan_vec3 rgb_a = aces_mult_vec_mat3_v(aab, &mat_aab_to_cone);

    alwan_vec3 rgb_m;
    rgb_m.v[0] = aces_cone_response_inv_v(rgb_a.v[0]);
    rgb_m.v[1] = aces_cone_response_inv_v(rgb_a.v[1]);
    rgb_m.v[2] = aces_cone_response_inv_v(rgb_a.v[2]);

    alwan_mat3x3 mat_cam16_to_rgb;
    for (int i = 0; i < 9; i++) mat_cam16_to_rgb.m[i] = p->MATRIX_CAM16_to_RGB[i];

    return aces_mult_vec_mat3_v(rgb_m, &mat_cam16_to_rgb);
}

/* J to Y conversion */
ALWAN_INLINE alwan_scalar aces2_j_to_y_v(alwan_scalar J, aces2_JMhParams const *p) {
    if (J <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);

    alwan_scalar A = ALWAN_POW(J / ACES_J_SCALE, p->inv_cz);
    alwan_scalar Ra = p->A_w_J * A;
    alwan_scalar Y = aces_cone_response_inv_v(Ra) / p->F_L_n;

    return Y;
}

/* Y to J conversion */
ALWAN_INLINE alwan_scalar aces2_y_to_j_v(alwan_scalar Y, aces2_JMhParams const *p) {
    if (Y <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);

    alwan_scalar Ra = aces_cone_response_fwd_v(Y * p->F_L_n);
    alwan_scalar J = ACES_J_SCALE * ALWAN_POW(Ra * p->inv_A_w_J, p->cz);

    return J;
}

/* Tonescale forward */
ALWAN_INLINE alwan_scalar aces2_tonescale_fwd_v(alwan_scalar x, aces2_TSParams const *ts) {
    alwan_scalar x_clamped = (x > ALWAN_LITERAL(0.0)) ? x : ALWAN_LITERAL(0.0);
    alwan_scalar f = ts->m_2 * ALWAN_POW(x_clamped / (x_clamped + ts->s_2), ts->g);
    alwan_scalar h = (f > ALWAN_LITERAL(0.0)) ? (f * f / (f + ts->t_1)) : ALWAN_LITERAL(0.0);
    return h * ts->n_r;
}

/* ================================================================
 * Gamut Compression Math
 * ================================================================ */

/* Smooth minimum function */
ALWAN_INLINE alwan_scalar aces2_smin_scaled_v(alwan_scalar a, alwan_scalar b, alwan_scalar cusp_M) {
    alwan_scalar s = cusp_M * ACES_GAMUT_SMOOTH_CUSPS;
    alwan_scalar x = a - b;
    alwan_scalar sign = (x >= ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(-1.0);
    alwan_scalar abs_x = ALWAN_ABS(x);

    if (abs_x >= s) {
        return (a < b) ? a : b;
    }

    alwan_scalar t = (abs_x - s) / (ALWAN_LITERAL(2.0) * s);
    alwan_scalar blend = t * t * (abs_x + s) / (ALWAN_LITERAL(2.0) * s);
    alwan_scalar min_val = (a < b) ? a : b;

    return min_val + blend * sign;
}

/* Reinhard forward */
ALWAN_INLINE alwan_scalar aces2_reinhard_fwd_v(alwan_scalar x) {
    return x / (ALWAN_LITERAL(1.0) + x);
}

/* Reinhard inverse */
ALWAN_INLINE alwan_scalar aces2_reinhard_inv_v(alwan_scalar x) {
    return x / (ALWAN_LITERAL(1.0) - x);
}

/* Remap M forward */
ALWAN_INLINE alwan_scalar aces2_remap_m_fwd_v(alwan_scalar M, alwan_scalar gamut_boundary_M,
                                               alwan_scalar reach_boundary_M) {
    alwan_scalar boundary_ratio = gamut_boundary_M / reach_boundary_M;
    alwan_scalar proportion = (boundary_ratio > ACES_GAMUT_COMPRESSION_THRESHOLD)
                             ? boundary_ratio : ACES_GAMUT_COMPRESSION_THRESHOLD;
    alwan_scalar threshold = proportion * gamut_boundary_M;

    if (M <= threshold || proportion >= ALWAN_LITERAL(1.0)) {
        return M;
    }

    alwan_scalar m_offset = M - threshold;
    alwan_scalar gamut_offset = gamut_boundary_M - threshold;
    alwan_scalar reach_offset = reach_boundary_M - threshold;
    alwan_scalar scale = reach_offset / ((reach_offset / gamut_offset) - ALWAN_LITERAL(1.0));
    alwan_scalar nd = m_offset / scale;

    return threshold + scale * aces2_reinhard_fwd_v(nd);
}

/* Remap M inverse */
ALWAN_INLINE alwan_scalar aces2_remap_m_inv_v(alwan_scalar M, alwan_scalar gamut_boundary_M,
                                               alwan_scalar reach_boundary_M) {
    alwan_scalar boundary_ratio = gamut_boundary_M / reach_boundary_M;
    alwan_scalar proportion = (boundary_ratio > ACES_GAMUT_COMPRESSION_THRESHOLD)
                             ? boundary_ratio : ACES_GAMUT_COMPRESSION_THRESHOLD;
    alwan_scalar threshold = proportion * gamut_boundary_M;

    if (M <= threshold || proportion >= ALWAN_LITERAL(1.0)) {
        return M;
    }

    alwan_scalar m_offset = M - threshold;
    alwan_scalar gamut_offset = gamut_boundary_M - threshold;
    alwan_scalar reach_offset = reach_boundary_M - threshold;
    alwan_scalar scale = reach_offset / ((reach_offset / gamut_offset) - ALWAN_LITERAL(1.0));
    alwan_scalar nd = m_offset / scale;

    return threshold + scale * aces2_reinhard_inv_v(nd);
}

/* Compute focus J */
ALWAN_INLINE alwan_scalar aces2_compute_focus_j_v(alwan_scalar cusp_J, alwan_scalar mid_J,
                                                   alwan_scalar limit_J_max) {
    alwan_scalar blend = ACES_GAMUT_CUSP_MID_BLEND - (cusp_J / limit_J_max);
    if (blend > ALWAN_LITERAL(1.0)) blend = ALWAN_LITERAL(1.0);
    return cusp_J + blend * (mid_J - cusp_J);
}

/* Get focus gain */
ALWAN_INLINE alwan_scalar aces2_get_focus_gain_v(alwan_scalar J, alwan_scalar analytical_threshold,
                                                  alwan_scalar limit_J_max, alwan_scalar focus_dist) {
    alwan_scalar gain = limit_J_max * focus_dist;

    if (J > analytical_threshold) {
        alwan_scalar denom = limit_J_max - J;
        if (denom < ALWAN_LITERAL(0.0001)) denom = ALWAN_LITERAL(0.0001);
        alwan_scalar gain_adj = ALWAN_LN((limit_J_max - analytical_threshold) / denom)
                              / ALWAN_LN(ALWAN_LITERAL(10.0));
        gain_adj = gain_adj * gain_adj + ALWAN_LITERAL(1.0);
        gain = gain * gain_adj;
    }

    return gain;
}

/* Solve J intersection for compression vector */
ALWAN_INLINE alwan_scalar aces2_solve_j_intersect_v(alwan_scalar J, alwan_scalar M,
                                                     alwan_scalar focus_J, alwan_scalar max_J,
                                                     alwan_scalar slope_gain) {
    alwan_scalar M_scaled = M / slope_gain;
    alwan_scalar a = M_scaled / focus_J;

    if (J < focus_J) {
        alwan_scalar b = ALWAN_LITERAL(1.0) - M_scaled;
        alwan_scalar c = -J;
        alwan_scalar det = b * b - ALWAN_LITERAL(4.0) * a * c;
        alwan_scalar root = ALWAN_SQRT(det);
        return -ALWAN_LITERAL(2.0) * c / (b + root);
    } else {
        alwan_scalar b = -(ALWAN_LITERAL(1.0) + M_scaled + max_J * a);
        alwan_scalar c = max_J * M_scaled + J;
        alwan_scalar det = b * b - ALWAN_LITERAL(4.0) * a * c;
        alwan_scalar root = ALWAN_SQRT(det);
        return -ALWAN_LITERAL(2.0) * c / (b - root);
    }
}

/* Compute compression vector slope */
ALWAN_INLINE alwan_scalar aces2_compression_vector_slope_v(alwan_scalar J_intersect,
                                                            alwan_scalar focus_J,
                                                            alwan_scalar limit_J_max,
                                                            alwan_scalar slope_gain) {
    if (J_intersect < focus_J) {
        return slope_gain * (ALWAN_LITERAL(1.0) - J_intersect / focus_J);
    } else {
        return slope_gain * (J_intersect - focus_J) / (limit_J_max - focus_J);
    }
}

/* Estimate line-boundary intersection M */
ALWAN_INLINE alwan_scalar aces2_estimate_line_boundary_m_v(alwan_scalar J_intersect_source,
                                                            alwan_scalar slope,
                                                            alwan_scalar gamma_inv,
                                                            alwan_scalar cusp_J,
                                                            alwan_scalar cusp_M,
                                                            alwan_scalar J_intersect_cusp) {
    alwan_scalar denom = J_intersect_cusp - cusp_J;
    if (ALWAN_ABS(denom) < ALWAN_LITERAL(1e-6)) {
        if (cusp_J > ALWAN_LITERAL(0.0)) {
            return cusp_M * J_intersect_source / cusp_J;
        }
        return cusp_M;
    }

    alwan_scalar t = (J_intersect_source - cusp_J) / denom;
    if (t < ALWAN_LITERAL(0.0)) t = ALWAN_LITERAL(0.0);
    if (t > ALWAN_LITERAL(1.0)) t = ALWAN_LITERAL(1.0);

    alwan_scalar M_boundary = cusp_M * ALWAN_POW(t, gamma_inv);

    alwan_scalar J_diff = ALWAN_ABS(J_intersect_source - cusp_J);
    if (slope != ALWAN_LITERAL(0.0)) {
        M_boundary = M_boundary + J_diff / ALWAN_ABS(slope);
    }

    return M_boundary;
}

/* Find gamut boundary intersection */
ALWAN_INLINE alwan_scalar aces2_find_gamut_boundary_v(alwan_scalar cusp_J,
                                                       alwan_scalar cusp_M,
                                                       alwan_scalar limit_J_max,
                                                       alwan_scalar gamma_top_inv,
                                                       alwan_scalar gamma_bottom_inv,
                                                       alwan_scalar J_intersect_source,
                                                       alwan_scalar slope,
                                                       alwan_scalar J_intersect_cusp) {
    alwan_scalar M_boundary_lower = aces2_estimate_line_boundary_m_v(
        J_intersect_source, slope, gamma_bottom_inv,
        cusp_J, cusp_M, J_intersect_cusp);

    alwan_scalar f_J_intersect_cusp = limit_J_max - J_intersect_cusp;
    alwan_scalar f_J_intersect_source = limit_J_max - J_intersect_source;
    alwan_scalar f_cusp_J = limit_J_max - cusp_J;

    alwan_scalar M_boundary_upper = aces2_estimate_line_boundary_m_v(
        f_J_intersect_source, -slope, gamma_top_inv,
        f_cusp_J, cusp_M, f_J_intersect_cusp);

    return aces2_smin_scaled_v(M_boundary_lower, M_boundary_upper, cusp_M);
}

/* Gamut compression forward */
ALWAN_INLINE alwan_vec2 aces2_compress_gamut_fwd_v(alwan_scalar J, alwan_scalar M, alwan_scalar h,
                                                    aces2_GamutCompressParams const *gcp,
                                                    aces2_HueDependentGamutParams const *hdp) {
    alwan_vec2 result;
    (void)h;

    alwan_scalar slope_gain = aces2_get_focus_gain_v(J, hdp->analytical_threshold,
                                                      gcp->limit_J_max, gcp->focus_dist);

    alwan_scalar J_intersect = aces2_solve_j_intersect_v(J, M, hdp->focus_J,
                                                          gcp->limit_J_max, slope_gain);

    alwan_scalar gamut_slope = aces2_compression_vector_slope_v(
        J_intersect, hdp->focus_J, gcp->limit_J_max, slope_gain);

    alwan_scalar J_intersect_cusp = aces2_solve_j_intersect_v(
        hdp->cusp_J, hdp->cusp_M, hdp->focus_J, gcp->limit_J_max, slope_gain);

    alwan_scalar gamut_boundary_M = aces2_find_gamut_boundary_v(
        hdp->cusp_J, hdp->cusp_M, gcp->limit_J_max,
        hdp->gamma_top_inv, hdp->gamma_bottom_inv,
        J_intersect, gamut_slope, J_intersect_cusp);

    if (gamut_boundary_M <= ALWAN_LITERAL(0.0)) {
        result.v[0] = J;
        result.v[1] = ALWAN_LITERAL(0.0);
        return result;
    }

    alwan_scalar reach_boundary_M = aces2_estimate_line_boundary_m_v(
        J_intersect, gamut_slope, gcp->model_gamma_inv,
        gcp->limit_J_max, gcp->reach_max_M, gcp->limit_J_max);

    alwan_scalar remapped_M = aces2_remap_m_fwd_v(M, gamut_boundary_M, reach_boundary_M);

    result.v[0] = J_intersect + remapped_M * gamut_slope;
    result.v[1] = remapped_M;
    return result;
}

/* Gamut compression inverse */
ALWAN_INLINE alwan_vec2 aces2_compress_gamut_inv_v(alwan_scalar J, alwan_scalar M, alwan_scalar h,
                                                    aces2_GamutCompressParams const *gcp,
                                                    aces2_HueDependentGamutParams const *hdp) {
    alwan_vec2 result;
    (void)h;

    alwan_scalar slope_gain = aces2_get_focus_gain_v(J, hdp->analytical_threshold,
                                                      gcp->limit_J_max, gcp->focus_dist);

    alwan_scalar J_intersect = aces2_solve_j_intersect_v(J, M, hdp->focus_J,
                                                          gcp->limit_J_max, slope_gain);

    alwan_scalar gamut_slope = aces2_compression_vector_slope_v(
        J_intersect, hdp->focus_J, gcp->limit_J_max, slope_gain);

    alwan_scalar J_intersect_cusp = aces2_solve_j_intersect_v(
        hdp->cusp_J, hdp->cusp_M, hdp->focus_J, gcp->limit_J_max, slope_gain);

    alwan_scalar gamut_boundary_M = aces2_find_gamut_boundary_v(
        hdp->cusp_J, hdp->cusp_M, gcp->limit_J_max,
        hdp->gamma_top_inv, hdp->gamma_bottom_inv,
        J_intersect, gamut_slope, J_intersect_cusp);

    if (gamut_boundary_M <= ALWAN_LITERAL(0.0)) {
        result.v[0] = J;
        result.v[1] = ALWAN_LITERAL(0.0);
        return result;
    }

    alwan_scalar reach_boundary_M = aces2_estimate_line_boundary_m_v(
        J_intersect, gamut_slope, gcp->model_gamma_inv,
        gcp->limit_J_max, gcp->reach_max_M, gcp->limit_J_max);

    alwan_scalar remapped_M = aces2_remap_m_inv_v(M, gamut_boundary_M, reach_boundary_M);

    result.v[0] = J_intersect + remapped_M * gamut_slope;
    result.v[1] = remapped_M;
    return result;
}

#endif /* ALWAN_ACES_FF_CORE_H */
