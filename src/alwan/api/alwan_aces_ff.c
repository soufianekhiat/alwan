/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ACES Fixed Functions (RRT Components)
 * Thin wrapper -- per-pixel math in alwan_aces_ff_core.h
 *
 * Only init functions, table generation, enum dispatch, and public API live here.
 * Reference: OpenColorIO FixedFunctionOpCPU.cpp
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include "../core/alwan_aces_ff_core.h"

/* Typedefs for backward compatibility with local struct names */
typedef aces2_JMhParams JMhParams;
typedef aces2_TSParams TSParams;
typedef aces2_GamutCuspEntry GamutCuspEntry;
typedef aces2_GamutCompressParams GamutCompressParams;
typedef aces2_HueDependentGamutParams HueDependentGamutParams;

/* ----------------------------------------------------------------
 * Internal helper functions
 * ---------------------------------------------------------------- */

static alwan_scalar min3(alwan_scalar a, alwan_scalar b, alwan_scalar c) {
    return alwan_min3(a, b, c);
}

static alwan_scalar max3(alwan_scalar a, alwan_scalar b, alwan_scalar c) {
    return alwan_max3(a, b, c);
}

/* ----------------------------------------------------------------
 * RedMod helper functions (from OCIO)
 * ---------------------------------------------------------------- */

/* Saturation weight calculation */
static alwan_scalar calc_sat_weight(alwan_scalar red, alwan_scalar grn, alwan_scalar blu,
                                     alwan_scalar noise_limit) {
    return aces_calc_sat_weight_v(red, grn, blu, noise_limit);
}

/* Hue weight calculation using B-spline */
static alwan_scalar calc_hue_weight(alwan_scalar red, alwan_scalar grn, alwan_scalar blu,
                                     alwan_scalar inv_width) {
    return aces_calc_hue_weight_v(red, grn, blu, inv_width);
}

/* ----------------------------------------------------------------
 * ACES RedMod03 - Red channel modification (RRT v0.3)
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_ACES_RedMod03_Fwd
 * ---------------------------------------------------------------- */

/* RedMod03 constants */
#define REDMOD03_SCALE      ALWAN_LITERAL(0.85)
#define REDMOD03_PIVOT      ALWAN_LITERAL(0.03)
#define REDMOD03_INV_WIDTH  ALWAN_LITERAL(1.9098593171027443)
#define REDMOD_NOISE_LIMIT  ALWAN_LITERAL(1e-2)

int alwan_aces_redmod03(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;

    alwan_scalar red = rgb_in->r;
    alwan_scalar grn = rgb_in->g;
    alwan_scalar blu = rgb_in->b;

    alwan_scalar f_H = calc_hue_weight(red, grn, blu, REDMOD03_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_scalar f_S = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
        alwan_scalar one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD03_SCALE;
        alwan_scalar new_red = red + f_H * f_S * (REDMOD03_PIVOT - red) * one_minus_scale;

        /* Preserve hue by adjusting green or blue.
         * Only adjust if the change to red is significant (> 1e-5) */
        alwan_scalar delta_red = new_red - red;
        if (ALWAN_ABS(delta_red) > ALWAN_LITERAL(1e-5)) {
            if (grn >= blu) {
                alwan_scalar denom = red - blu;
                if (denom > ALWAN_LITERAL(1e-10)) {
                    alwan_scalar hue_fac = (grn - blu) / denom;
                    grn = hue_fac * (new_red - blu) + blu;
                }
            } else {
                alwan_scalar denom = red - grn;
                if (denom > ALWAN_LITERAL(1e-10)) {
                    alwan_scalar hue_fac = (blu - grn) / denom;
                    blu = hue_fac * (new_red - grn) + grn;
                }
            }
        }

        red = new_red;
    }

    rgb_out->r = red;
    rgb_out->g = grn;
    rgb_out->b = blu;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES RedMod10 - Red channel modification (RRT v1.0)
 * ---------------------------------------------------------------- */

#define REDMOD10_SCALE      ALWAN_LITERAL(0.82)
#define REDMOD10_PIVOT      ALWAN_LITERAL(0.03)
#define REDMOD10_INV_WIDTH  ALWAN_LITERAL(1.6976527263135504)

int alwan_aces_redmod10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;

    alwan_scalar red = rgb_in->r;
    alwan_scalar grn = rgb_in->g;
    alwan_scalar blu = rgb_in->b;

    alwan_scalar f_H = calc_hue_weight(red, grn, blu, REDMOD10_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_scalar f_S = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
        alwan_scalar one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD10_SCALE;
        /* RedMod10 only modifies red - no hue preservation (unlike RedMod03) */
        red = red + f_H * f_S * (REDMOD10_PIVOT - red) * one_minus_scale;
    }

    rgb_out->r = red;
    rgb_out->g = grn;
    rgb_out->b = blu;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES Glow03/10 - Flare/glow effect
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_ACES_Glow03_Fwd
 * ---------------------------------------------------------------- */

/* Glow constants (from OCIO) - different for v03 and v10 */
#define GLOW03_GAIN ALWAN_LITERAL(0.075)
#define GLOW03_MID  ALWAN_LITERAL(0.1)
#define GLOW10_GAIN ALWAN_LITERAL(0.05)
#define GLOW10_MID  ALWAN_LITERAL(0.08)

/* YC (luminance with chroma weighting) calculation */
static alwan_scalar rgb_to_yc(alwan_scalar red, alwan_scalar grn, alwan_scalar blu) {
    return aces_rgb_to_yc_v(red, grn, blu);
}

/* Sigmoid shaper for saturation */
static alwan_scalar sigmoid_shaper(alwan_scalar sat) {
    return aces_sigmoid_shaper_v(sat);
}

/* Internal glow implementation with configurable parameters */
static int glow_impl(alwan_rgb const *rgb_in, alwan_scalar glow_gain_param,
                     alwan_scalar glow_mid_param, alwan_rgb *rgb_out) {
    alwan_scalar red = rgb_in->r;
    alwan_scalar grn = rgb_in->g;
    alwan_scalar blu = rgb_in->b;

    alwan_scalar YC = rgb_to_yc(red, grn, blu);
    alwan_scalar sat = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
    alwan_scalar s = sigmoid_shaper(sat);

    alwan_scalar glow_gain = glow_gain_param * s;
    alwan_scalar glow_mid = glow_mid_param;

    alwan_scalar glow_gain_out;
    if (YC >= glow_mid * ALWAN_LITERAL(2.0)) {
        glow_gain_out = ALWAN_LITERAL(0.0);
    } else if (YC <= glow_mid * ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
        glow_gain_out = glow_gain;
    } else {
        glow_gain_out = glow_gain * (glow_mid / YC - ALWAN_LITERAL(0.5));
    }

    alwan_scalar added_glow = ALWAN_LITERAL(1.0) + glow_gain_out;

    rgb_out->r = red * added_glow;
    rgb_out->g = grn * added_glow;
    rgb_out->b = blu * added_glow;

    return ALWAN_OK;
}

int alwan_aces_glow03(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    return glow_impl(rgb_in, GLOW03_GAIN, GLOW03_MID, rgb_out);
}

int alwan_aces_glow10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    return glow_impl(rgb_in, GLOW10_GAIN, GLOW10_MID, rgb_out);
}

/* ----------------------------------------------------------------
 * ACES DarkToDim10 - Surround compensation
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_ACES_DarkToDim10_Fwd
 * ---------------------------------------------------------------- */

int alwan_aces_dark_to_dim10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    alwan_vec3 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3 out_v = aces_dark_to_dim10_v(in_v);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Rec.2100 Surround adjustment
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_REC2100_Surround
 * ---------------------------------------------------------------- */

int alwan_rec2100_surround(alwan_rgb *rgb_out, alwan_rgb const *rgb_in, alwan_scalar gamma) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    alwan_vec3 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3 out_v = aces_rec2100_surround_v(in_v, gamma);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES GamutComp13 - Gamut compression (ACES 1.3)
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_ACES_GamutComp13_Fwd
 * ---------------------------------------------------------------- */

void alwan_aces_gamut_comp13_params_default(alwan_aces_gamut_comp13_params *params) {
    if (!params) return;
    params->lim_cyan = ALWAN_LITERAL(1.147);
    params->lim_magenta = ALWAN_LITERAL(1.264);
    params->lim_yellow = ALWAN_LITERAL(1.312);
    params->thr_cyan = ALWAN_LITERAL(0.815);
    params->thr_magenta = ALWAN_LITERAL(0.803);
    params->thr_yellow = ALWAN_LITERAL(0.880);
    params->power = ALWAN_LITERAL(1.2);
}

/* Compression function (forward direction) */
static alwan_scalar compress_dist(alwan_scalar dist, alwan_scalar thr, alwan_scalar scale, alwan_scalar power) {
    return aces_compress_dist_v(dist, thr, scale, power);
}

/* Per-channel gamut compression */
static alwan_scalar gamut_comp_channel(alwan_scalar val, alwan_scalar ach,
                                        alwan_scalar thr, alwan_scalar scale, alwan_scalar power) {
    return aces_gamut_comp_channel_v(val, ach, thr, scale, power);
}

/* Compute scale from limit, threshold and power.
 * The scale ensures the compression function passes through (1, lim). */
static alwan_scalar calc_gamut_comp_scale(alwan_scalar lim, alwan_scalar thr, alwan_scalar power) {
    return aces_calc_gamut_comp_scale_v(lim, thr, power);
}

int alwan_aces_gamut_comp13(alwan_rgb *rgb_out,
                            alwan_rgb const *rgb_in,
                            alwan_aces_gamut_comp13_params const *params) {
    if (!rgb_out || !rgb_in || !params) return ALWAN_E_INVALID;

    alwan_scalar red = rgb_in->r;
    alwan_scalar grn = rgb_in->g;
    alwan_scalar blu = rgb_in->b;

    /* Compute scales from limits and thresholds using OCIO formula */
    alwan_scalar scale_cyan = calc_gamut_comp_scale(params->lim_cyan, params->thr_cyan, params->power);
    alwan_scalar scale_magenta = calc_gamut_comp_scale(params->lim_magenta, params->thr_magenta, params->power);
    alwan_scalar scale_yellow = calc_gamut_comp_scale(params->lim_yellow, params->thr_yellow, params->power);

    /* Achromatic = max(RGB) */
    alwan_scalar ach = max3(red, grn, blu);

    rgb_out->r = gamut_comp_channel(red, ach, params->thr_cyan, scale_cyan, params->power);
    rgb_out->g = gamut_comp_channel(grn, ach, params->thr_magenta, scale_magenta, params->power);
    rgb_out->b = gamut_comp_channel(blu, ach, params->thr_yellow, scale_yellow, params->power);

    return ALWAN_OK;
}

/* Decompression function (inverse direction)
 * Given compressed distance, recover original distance */
static alwan_scalar uncompress_dist(alwan_scalar compressed_dist, alwan_scalar thr, alwan_scalar scale, alwan_scalar power) {
    return aces_uncompress_dist_v(compressed_dist, thr, scale, power);
}

/* Per-channel gamut decompression (inverse) */
static alwan_scalar gamut_decomp_channel(alwan_scalar val, alwan_scalar ach,
                                          alwan_scalar thr, alwan_scalar scale, alwan_scalar power) {
    return aces_gamut_decomp_channel_v(val, ach, thr, scale, power);
}

int alwan_aces_gamut_comp13_inv(alwan_rgb *rgb_out,
                                 alwan_rgb const *rgb_in,
                                 alwan_aces_gamut_comp13_params const *params) {
    if (!rgb_out || !rgb_in || !params) return ALWAN_E_INVALID;

    alwan_scalar red = rgb_in->r;
    alwan_scalar grn = rgb_in->g;
    alwan_scalar blu = rgb_in->b;

    /* Compute scales from limits and thresholds using OCIO formula */
    alwan_scalar scale_cyan = calc_gamut_comp_scale(params->lim_cyan, params->thr_cyan, params->power);
    alwan_scalar scale_magenta = calc_gamut_comp_scale(params->lim_magenta, params->thr_magenta, params->power);
    alwan_scalar scale_yellow = calc_gamut_comp_scale(params->lim_yellow, params->thr_yellow, params->power);

    /* Achromatic = max(RGB) - same as forward */
    alwan_scalar ach = max3(red, grn, blu);

    rgb_out->r = gamut_decomp_channel(red, ach, params->thr_cyan, scale_cyan, params->power);
    rgb_out->g = gamut_decomp_channel(grn, ach, params->thr_magenta, scale_magenta, params->power);
    rgb_out->b = gamut_decomp_channel(blu, ach, params->thr_yellow, scale_yellow, params->power);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES 1.x Output Transform (RRT + ODT)
 * Reference: Academy CTL, OpenColorIO
 * ---------------------------------------------------------------- */

/* ACES 1.x RRT tone scale constants */
static const alwan_scalar ACES1_MIN_STOP_SDR = ALWAN_LITERAL(-6.5);
static const alwan_scalar ACES1_MAX_STOP_SDR = ALWAN_LITERAL(6.5);
static const alwan_scalar ACES1_MIN_STOP_RRT = ALWAN_LITERAL(-15.0);
static const alwan_scalar ACES1_MAX_STOP_RRT = ALWAN_LITERAL(18.0);

/* ACES 1.x RRT parameters */
static const alwan_scalar ACES1_RRT_GLOW_GAIN = ALWAN_LITERAL(0.05);
static const alwan_scalar ACES1_RRT_GLOW_MID = ALWAN_LITERAL(0.08);

/* ACES 1.x helper functions */

/* Calculate saturation as (max - min) / max */
static alwan_scalar aces1_saturation(alwan_scalar r, alwan_scalar g, alwan_scalar b) {
    return aces1_saturation_v(r, g, b);
}

/* Convert RGB to hue in degrees [0, 360) */
static alwan_scalar aces1_rgb_to_hue(alwan_scalar r, alwan_scalar g, alwan_scalar b) {
    return aces1_rgb_to_hue_v(r, g, b);
}

/* Center hue around a target hue */
static alwan_scalar aces1_center_hue(alwan_scalar hue, alwan_scalar center) {
    return aces1_center_hue_v(hue, center);
}

/* Cubic basis shaper - smooth falloff from center */
static alwan_scalar aces1_cubic_basis_shaper(alwan_scalar x, alwan_scalar width) {
    return aces1_cubic_basis_shaper_v(x, width);
}

/* AP0 to AP1 matrix */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_AP0_TO_AP1[9] = {
#include "../data/matrices/aces_ap0_to_ap1.csv"
};
ALWAN_DIAG_POP

/* AP1 to AP0 matrix */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_AP1_TO_AP0[9] = {
#include "../data/matrices/aces_ap1_to_ap0.csv"
};
ALWAN_DIAG_POP

/* D60 to D65 chromatic adaptation (Bradford) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_D60_TO_D65[9] = {
#include "../data/matrices/aces_d60_to_d65_bradford.csv"
};
ALWAN_DIAG_POP

/* D65 to D60 chromatic adaptation (Bradford) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_D65_TO_D60[9] = {
#include "../data/matrices/aces_d65_to_d60_bradford.csv"
};
ALWAN_DIAG_POP

/* Segmented spline function for RRT tone scale
 * This is a simplified version - full implementation would use LUTs */
static alwan_scalar aces1_segmented_spline_c5(alwan_scalar x) {
    return aces1_segmented_spline_c5_v(x);
}

/* ACES 1.x RRT core (simplified) */
static void aces1_rrt_core(alwan_rgb const *ap1_in, alwan_rgb *rrt_out) {
    /* Apply glow module */
    alwan_scalar sat = aces1_saturation(ap1_in->r, ap1_in->g, ap1_in->b);
    alwan_scalar add_glow = ACES1_RRT_GLOW_GAIN * sigmoid_shaper(sat);

    alwan_scalar r = ap1_in->r * (ALWAN_LITERAL(1.0) + add_glow);
    alwan_scalar g = ap1_in->g * (ALWAN_LITERAL(1.0) + add_glow);
    alwan_scalar b = ap1_in->b * (ALWAN_LITERAL(1.0) + add_glow);

    /* Apply red modifier (RedMod10) inline */
    alwan_scalar hue = aces1_rgb_to_hue(r, g, b);
    alwan_scalar centeredHue = aces1_center_hue(hue, ALWAN_LITERAL(0.0));
    alwan_scalar hueWeight = aces1_cubic_basis_shaper(centeredHue, ALWAN_LITERAL(135.0));

    if (hueWeight > ALWAN_LITERAL(0.0)) {
        r = r + hueWeight * sat * (ALWAN_LITERAL(0.03) - ALWAN_LITERAL(0.03) * r);
    }

    /* Apply RRT tone scale per channel */
    r = aces1_segmented_spline_c5(r);
    g = aces1_segmented_spline_c5(g);
    b = aces1_segmented_spline_c5(b);

    /* Scale to display range (simplified: /48 for SDR, varies for HDR) */
    rrt_out->r = r / ALWAN_LITERAL(48.0);
    rrt_out->g = g / ALWAN_LITERAL(48.0);
    rrt_out->b = b / ALWAN_LITERAL(48.0);
}

/* Matrix multiply helper */
static void mat3_mul_vec3_aces1(alwan_scalar const *m, alwan_rgb const *v, alwan_rgb *out) {
    alwan_vec3 vec = {{v->r, v->g, v->b}};
    alwan_vec3 res = alwan_mat3_mulv_v(*(alwan_mat3x3 const *)m, vec);
    out->r = res.v[0]; out->g = res.v[1]; out->b = res.v[2];
}

/* Output primaries matrices */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_XYZ_TO_REC709[9] = {
#include "../data/matrices/aces_xyz_to_rec709.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_XYZ_TO_P3D65[9] = {
#include "../data/matrices/aces_xyz_to_p3d65.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_XYZ_TO_REC2020[9] = {
#include "../data/matrices/aces_xyz_to_rec2020.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_AP1_TO_XYZ_D60[9] = {
#include "../data/matrices/aces_ap1_to_xyz_d60.csv"
};
ALWAN_DIAG_POP

/* Apply BT.1886 EOTF inverse (gamma 2.4) */
static alwan_scalar bt1886_oetf(alwan_scalar x) {
    return aces_bt1886_oetf_v(x);
}

/* Apply sRGB OETF */
static alwan_scalar srgb_oetf(alwan_scalar x) {
    return aces_srgb_oetf_v(x);
}

/* Apply Gamma 2.6 OETF (cinema) */
static alwan_scalar gamma26_oetf(alwan_scalar x) {
    return aces_gamma26_oetf_v(x);
}

/* PQ (ST.2084) OETF */
static alwan_scalar pq_oetf(alwan_scalar Y, alwan_scalar peak_nits) {
    return aces_pq_oetf_v(Y, peak_nits);
}

int alwan_aces1_output_transform(alwan_rgb *rgb_out,
                                  alwan_rgb const *rgb_in,
                                  alwan_aces1_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES1_OUT_COUNT) return ALWAN_E_INVALID;

    alwan_rgb ap1, rrt, xyz, d65, display;

    /* Step 1: AP0 to AP1 */
    mat3_mul_vec3_aces1(ACES1_AP0_TO_AP1, rgb_in, &ap1);

    /* Clamp negatives */
    ap1.r = alwan_max(ap1.r, ALWAN_LITERAL(0.0));
    ap1.g = alwan_max(ap1.g, ALWAN_LITERAL(0.0));
    ap1.b = alwan_max(ap1.b, ALWAN_LITERAL(0.0));

    /* Step 2: Apply RRT */
    aces1_rrt_core(&ap1, &rrt);

    /* Step 3: AP1 to XYZ (D60) */
    mat3_mul_vec3_aces1(ACES1_AP1_TO_XYZ_D60, &rrt, &xyz);

    /* Step 4: D60 to D65 (for D65 white point outputs) */
    int needs_d65 = (output != ALWAN_ACES1_OUT_SRGB_D60_100NIT &&
                     output != ALWAN_ACES1_OUT_P3D60_48NIT);
    if (needs_d65) {
        mat3_mul_vec3_aces1(ACES1_D60_TO_D65, &xyz, &d65);
    } else {
        d65 = xyz;
    }

    /* Step 5: XYZ to output primaries */
    alwan_scalar const *xyz_to_display = NULL;
    alwan_scalar peak_nits = ALWAN_LITERAL(100.0);
    int use_pq = 0;
    int use_srgb = 0;
    int use_gamma26 = 0;

    switch (output) {
        case ALWAN_ACES1_OUT_REC709_100NIT:
            xyz_to_display = ACES1_XYZ_TO_REC709;
            break;
        case ALWAN_ACES1_OUT_SRGB_100NIT:
        case ALWAN_ACES1_OUT_SRGB_D60_100NIT:
            xyz_to_display = ACES1_XYZ_TO_REC709;
            use_srgb = 1;
            break;
        case ALWAN_ACES1_OUT_P3DCI_48NIT:
        case ALWAN_ACES1_OUT_P3D60_48NIT:
        case ALWAN_ACES1_OUT_P3D65_48NIT:
            xyz_to_display = ACES1_XYZ_TO_P3D65;
            use_gamma26 = 1;
            break;
        case ALWAN_ACES1_OUT_P3D65_100NIT:
            xyz_to_display = ACES1_XYZ_TO_P3D65;
            use_srgb = 1;
            break;
        case ALWAN_ACES1_OUT_REC2020_100NIT:
            xyz_to_display = ACES1_XYZ_TO_REC2020;
            break;
        case ALWAN_ACES1_OUT_REC2020_1000NIT_PQ:
            xyz_to_display = ACES1_XYZ_TO_REC2020;
            use_pq = 1;
            peak_nits = ALWAN_LITERAL(1000.0);
            break;
        case ALWAN_ACES1_OUT_REC2020_2000NIT_PQ:
            xyz_to_display = ACES1_XYZ_TO_REC2020;
            use_pq = 1;
            peak_nits = ALWAN_LITERAL(2000.0);
            break;
        case ALWAN_ACES1_OUT_REC2020_4000NIT_PQ:
            xyz_to_display = ACES1_XYZ_TO_REC2020;
            use_pq = 1;
            peak_nits = ALWAN_LITERAL(4000.0);
            break;
        case ALWAN_ACES1_OUT_DCDM_48NIT:
            /* DCDM stays in XYZ */
            display = d65;
            display.r = gamma26_oetf(display.r * ALWAN_LITERAL(48.0) / ALWAN_LITERAL(52.37));
            display.g = gamma26_oetf(display.g * ALWAN_LITERAL(48.0) / ALWAN_LITERAL(52.37));
            display.b = gamma26_oetf(display.b * ALWAN_LITERAL(48.0) / ALWAN_LITERAL(52.37));
            *rgb_out = display;
            return ALWAN_OK;
        default:
            return ALWAN_E_INVALID;
    }

    mat3_mul_vec3_aces1(xyz_to_display, &d65, &display);

    /* Clamp to [0, inf) */
    display.r = alwan_max(display.r, ALWAN_LITERAL(0.0));
    display.g = alwan_max(display.g, ALWAN_LITERAL(0.0));
    display.b = alwan_max(display.b, ALWAN_LITERAL(0.0));

    /* Step 6: Apply EOTF */
    if (use_pq) {
        rgb_out->r = pq_oetf(display.r, peak_nits);
        rgb_out->g = pq_oetf(display.g, peak_nits);
        rgb_out->b = pq_oetf(display.b, peak_nits);
    } else if (use_srgb) {
        rgb_out->r = srgb_oetf(display.r);
        rgb_out->g = srgb_oetf(display.g);
        rgb_out->b = srgb_oetf(display.b);
    } else if (use_gamma26) {
        rgb_out->r = gamma26_oetf(display.r);
        rgb_out->g = gamma26_oetf(display.g);
        rgb_out->b = gamma26_oetf(display.b);
    } else {
        /* BT.1886 */
        rgb_out->r = bt1886_oetf(display.r);
        rgb_out->g = bt1886_oetf(display.g);
        rgb_out->b = bt1886_oetf(display.b);
    }

    return ALWAN_OK;
}

/* Inverse EOTF functions */
static alwan_scalar bt1886_eotf(alwan_scalar x) {
    return aces_bt1886_eotf_v(x);
}

static alwan_scalar srgb_eotf(alwan_scalar x) {
    return aces_srgb_eotf_v(x);
}

static alwan_scalar gamma26_eotf(alwan_scalar x) {
    return aces_gamma26_eotf_v(x);
}

static alwan_scalar pq_eotf(alwan_scalar E, alwan_scalar peak_nits) {
    return aces_pq_eotf_v(E, peak_nits);
}

/* Inverse XYZ to output primaries matrices */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_REC709_TO_XYZ[9] = {
#include "../data/matrices/aces_rec709_to_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_P3D65_TO_XYZ[9] = {
#include "../data/matrices/aces_p3d65_to_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_REC2020_TO_XYZ[9] = {
#include "../data/matrices/aces_rec2020_to_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_scalar ACES1_XYZ_D60_TO_AP1[9] = {
#include "../data/matrices/aces_xyz_d60_to_ap1.csv"
};
ALWAN_DIAG_POP

/* Inverse RRT (simplified) */
static alwan_scalar aces1_segmented_spline_c5_inv(alwan_scalar y) {
    return aces1_segmented_spline_c5_inv_v(y);
}

static void aces1_rrt_core_inv(alwan_rgb const *rrt_in, alwan_rgb *ap1_out) {
    /* Scale from display range */
    alwan_scalar r_ts = rrt_in->r;
    alwan_scalar g_ts = rrt_in->g;
    alwan_scalar b_ts = rrt_in->b;

    /* Invert RRT tone scale */
    r_ts = aces1_segmented_spline_c5_inv(r_ts);
    g_ts = aces1_segmented_spline_c5_inv(g_ts);
    b_ts = aces1_segmented_spline_c5_inv(b_ts);

    /* Start with tone-scale-inverted values as initial estimate */
    alwan_scalar r = r_ts;
    alwan_scalar g = g_ts;
    alwan_scalar b = b_ts;

    /* Iterative inverse of glow and red modifier for better accuracy
     * We iterate because glow affects saturation which affects red mod */
    for (int iter = 0; iter < 5; iter++) {
        /* Compute what the forward transform would produce from current estimate */
        alwan_scalar sat = aces1_saturation(r, g, b);
        alwan_scalar add_glow = ACES1_RRT_GLOW_GAIN * sigmoid_shaper(sat);
        alwan_scalar glow_scale = ALWAN_LITERAL(1.0) + add_glow;

        /* Apply forward glow */
        alwan_scalar r_glow = r * glow_scale;
        alwan_scalar g_glow = g * glow_scale;
        alwan_scalar b_glow = b * glow_scale;

        /* Apply forward red modifier */
        alwan_scalar hue = aces1_rgb_to_hue(r_glow, g_glow, b_glow);
        alwan_scalar centeredHue = aces1_center_hue(hue, ALWAN_LITERAL(0.0));
        alwan_scalar hueWeight = aces1_cubic_basis_shaper(centeredHue, ALWAN_LITERAL(135.0));

        alwan_scalar r_mod = r_glow;
        if (hueWeight > ALWAN_LITERAL(0.0)) {
            r_mod = r_glow + hueWeight * sat * (ALWAN_LITERAL(0.03) - ALWAN_LITERAL(0.03) * r_glow);
        }

        /* Compute error: (forward_result - target) */
        alwan_scalar err_r = r_mod - r_ts;
        alwan_scalar err_g = g_glow - g_ts;
        alwan_scalar err_b = b_glow - b_ts;

        /* Correct estimate */
        r = r - err_r;
        g = g - err_g;
        b = b - err_b;
    }

    ap1_out->r = r;
    ap1_out->g = g;
    ap1_out->b = b;
}

int alwan_aces1_output_transform_inv(alwan_rgb *rgb_out,
                                      alwan_rgb const *rgb_in,
                                      alwan_aces1_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES1_OUT_COUNT) return ALWAN_E_INVALID;

    alwan_rgb display, xyz, d60, ap1;

    /* Step 1: Inverse EOTF */
    alwan_scalar const *display_to_xyz = NULL;
    alwan_scalar peak_nits = ALWAN_LITERAL(100.0);

    switch (output) {
        case ALWAN_ACES1_OUT_REC709_100NIT:
            display_to_xyz = ACES1_REC709_TO_XYZ;
            display.r = bt1886_eotf(rgb_in->r);
            display.g = bt1886_eotf(rgb_in->g);
            display.b = bt1886_eotf(rgb_in->b);
            break;
        case ALWAN_ACES1_OUT_SRGB_100NIT:
        case ALWAN_ACES1_OUT_SRGB_D60_100NIT:
            display_to_xyz = ACES1_REC709_TO_XYZ;
            display.r = srgb_eotf(rgb_in->r);
            display.g = srgb_eotf(rgb_in->g);
            display.b = srgb_eotf(rgb_in->b);
            break;
        case ALWAN_ACES1_OUT_P3DCI_48NIT:
        case ALWAN_ACES1_OUT_P3D60_48NIT:
        case ALWAN_ACES1_OUT_P3D65_48NIT:
            display_to_xyz = ACES1_P3D65_TO_XYZ;
            display.r = gamma26_eotf(rgb_in->r);
            display.g = gamma26_eotf(rgb_in->g);
            display.b = gamma26_eotf(rgb_in->b);
            break;
        case ALWAN_ACES1_OUT_P3D65_100NIT:
            display_to_xyz = ACES1_P3D65_TO_XYZ;
            display.r = srgb_eotf(rgb_in->r);
            display.g = srgb_eotf(rgb_in->g);
            display.b = srgb_eotf(rgb_in->b);
            break;
        case ALWAN_ACES1_OUT_REC2020_100NIT:
            display_to_xyz = ACES1_REC2020_TO_XYZ;
            display.r = bt1886_eotf(rgb_in->r);
            display.g = bt1886_eotf(rgb_in->g);
            display.b = bt1886_eotf(rgb_in->b);
            break;
        case ALWAN_ACES1_OUT_REC2020_1000NIT_PQ:
            display_to_xyz = ACES1_REC2020_TO_XYZ;
            peak_nits = ALWAN_LITERAL(1000.0);
            display.r = pq_eotf(rgb_in->r, peak_nits);
            display.g = pq_eotf(rgb_in->g, peak_nits);
            display.b = pq_eotf(rgb_in->b, peak_nits);
            break;
        case ALWAN_ACES1_OUT_REC2020_2000NIT_PQ:
            display_to_xyz = ACES1_REC2020_TO_XYZ;
            peak_nits = ALWAN_LITERAL(2000.0);
            display.r = pq_eotf(rgb_in->r, peak_nits);
            display.g = pq_eotf(rgb_in->g, peak_nits);
            display.b = pq_eotf(rgb_in->b, peak_nits);
            break;
        case ALWAN_ACES1_OUT_REC2020_4000NIT_PQ:
            display_to_xyz = ACES1_REC2020_TO_XYZ;
            peak_nits = ALWAN_LITERAL(4000.0);
            display.r = pq_eotf(rgb_in->r, peak_nits);
            display.g = pq_eotf(rgb_in->g, peak_nits);
            display.b = pq_eotf(rgb_in->b, peak_nits);
            break;
        case ALWAN_ACES1_OUT_DCDM_48NIT:
            /* DCDM is in XYZ */
            xyz.r = gamma26_eotf(rgb_in->r) * ALWAN_LITERAL(52.37) / ALWAN_LITERAL(48.0);
            xyz.g = gamma26_eotf(rgb_in->g) * ALWAN_LITERAL(52.37) / ALWAN_LITERAL(48.0);
            xyz.b = gamma26_eotf(rgb_in->b) * ALWAN_LITERAL(52.37) / ALWAN_LITERAL(48.0);
            mat3_mul_vec3_aces1(ACES1_D65_TO_D60, &xyz, &d60);
            mat3_mul_vec3_aces1(ACES1_XYZ_D60_TO_AP1, &d60, &ap1);
            aces1_rrt_core_inv(&ap1, &ap1);
            mat3_mul_vec3_aces1(ACES1_AP1_TO_AP0, &ap1, rgb_out);
            return ALWAN_OK;
        default:
            return ALWAN_E_INVALID;
    }

    /* Step 2: Display to XYZ */
    mat3_mul_vec3_aces1(display_to_xyz, &display, &xyz);

    /* Step 3: D65 to D60 (for D65 white point outputs) */
    int needs_d65 = (output != ALWAN_ACES1_OUT_SRGB_D60_100NIT &&
                     output != ALWAN_ACES1_OUT_P3D60_48NIT);
    if (needs_d65) {
        mat3_mul_vec3_aces1(ACES1_D65_TO_D60, &xyz, &d60);
    } else {
        d60 = xyz;
    }

    /* Step 4: XYZ (D60) to AP1 */
    mat3_mul_vec3_aces1(ACES1_XYZ_D60_TO_AP1, &d60, &ap1);

    /* Step 5: Inverse RRT */
    aces1_rrt_core_inv(&ap1, &ap1);

    /* Step 6: AP1 to AP0 */
    mat3_mul_vec3_aces1(ACES1_AP1_TO_AP0, &ap1, rgb_out);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES Primaries (single source of truth)
 * Reference: Academy S-2014-003, S-2016-001
 * ---------------------------------------------------------------- */

/* ACES AP1 (ACEScg) primaries -- D60 white */
static const alwan_scalar AP1_RED_x   = ALWAN_LITERAL(0.713);
static const alwan_scalar AP1_RED_y   = ALWAN_LITERAL(0.293);
static const alwan_scalar AP1_GREEN_x = ALWAN_LITERAL(0.165);
static const alwan_scalar AP1_GREEN_y = ALWAN_LITERAL(0.830);
static const alwan_scalar AP1_BLUE_x  = ALWAN_LITERAL(0.128);
static const alwan_scalar AP1_BLUE_y  = ALWAN_LITERAL(0.044);
static const alwan_scalar AP1_WHITE_x = ALWAN_LITERAL(0.32168);
static const alwan_scalar AP1_WHITE_y = ALWAN_LITERAL(0.33767);

/* ----------------------------------------------------------------
 * ACES 2.0: Constants and Viewing Conditions
 * Reference: ACES CTL Lib.Academy.OutputTransform.ctl
 * ---------------------------------------------------------------- */

/* CAM16 primaries - used for computing cone response matrices */
static const alwan_scalar CAM16_PRI_RED_X = ALWAN_LITERAL(0.8336);
static const alwan_scalar CAM16_PRI_RED_Y = ALWAN_LITERAL(0.1735);
static const alwan_scalar CAM16_PRI_GREEN_X = ALWAN_LITERAL(2.3854);
static const alwan_scalar CAM16_PRI_GREEN_Y = ALWAN_LITERAL(-1.4659);
static const alwan_scalar CAM16_PRI_BLUE_X = ALWAN_LITERAL(0.087);
static const alwan_scalar CAM16_PRI_BLUE_Y = ALWAN_LITERAL(-0.125);
static const alwan_scalar CAM16_WHITE_X = ALWAN_LITERAL(0.333333333333);
static const alwan_scalar CAM16_WHITE_Y = ALWAN_LITERAL(0.333333333333);

/* ACES viewing condition parameters */
static const alwan_scalar ACES2_REF_LUMINANCE = ALWAN_LITERAL(100.0);
static const alwan_scalar ACES2_L_A = ALWAN_LITERAL(100.0);  /* Adapting luminance */
static const alwan_scalar ACES2_Y_b = ALWAN_LITERAL(20.0);   /* Background luminance factor */

/* Surround parameters (Dim surround) */
static const alwan_scalar ACES2_SURROUND_F = ALWAN_LITERAL(0.9);
static const alwan_scalar ACES2_SURROUND_C = ALWAN_LITERAL(0.59);
static const alwan_scalar ACES2_SURROUND_N_c = ALWAN_LITERAL(0.9);

/* CAM16 nonlinearity constants (use core header for OFFSET) */
static const alwan_scalar CAM_NL_Y_REF = ALWAN_LITERAL(100.0);
static const alwan_scalar CAM_NL_OFFSET = ACES_CAM_NL_OFFSET;
static const alwan_scalar CAM_NL_SCALE = ALWAN_LITERAL(400.0);   /* 4.0 * 100.0 */

/* Lightness scale factor (J_scale = 100) */
static const alwan_scalar J_SCALE = ALWAN_LITERAL(100.0);

/* ----------------------------------------------------------------
 * ACES 2.0: Chroma Compression Constants (from OCIO Common.h)
 * ---------------------------------------------------------------- */

/* Chroma compression parameters */
static const alwan_scalar ACES2_CHROMA_COMPRESS = ALWAN_LITERAL(2.4);
static const alwan_scalar ACES2_CHROMA_COMPRESS_FACT = ALWAN_LITERAL(3.3);
static const alwan_scalar ACES2_CHROMA_EXPAND = ALWAN_LITERAL(1.3);
static const alwan_scalar ACES2_CHROMA_EXPAND_FACT = ALWAN_LITERAL(0.69);
static const alwan_scalar ACES2_CHROMA_EXPAND_THR = ALWAN_LITERAL(0.5);

/* Fourier coefficients -- use single source of truth from core header (CSV-loaded) */
#define ACES2_CHROMA_NORM_COS ACES2_CHROMA_NORM_COS_V
#define ACES2_CHROMA_NORM_SIN ACES2_CHROMA_NORM_SIN_V

/* Base cone response to Aab matrix (before scaling)
 * Row 0: [2, 1, 1/20]         - Achromatic channel
 * Row 1: [1, -12/11, 1/11]    - Red-green opponent
 * Row 2: [1/9, 1/9, -2/9]     - Yellow-blue opponent
 */
static const alwan_scalar CONE_TO_AAB_BASE[9] = {
    ALWAN_LITERAL(2.0),                  ALWAN_LITERAL(1.0),                  ALWAN_LITERAL(0.05),             /* 1/20 */
    ALWAN_LITERAL(1.0),                  ALWAN_LITERAL(-1.090909090909090909), ALWAN_LITERAL(0.090909090909090909), /* -12/11, 1/11 */
    ALWAN_LITERAL(0.111111111111111111), ALWAN_LITERAL(0.111111111111111111), ALWAN_LITERAL(-0.222222222222222222)  /* 1/9, 1/9, -2/9 */
};

/* ----------------------------------------------------------------
 * ACES 2.0: Matrix helper functions
 * ---------------------------------------------------------------- */

/* Compute RGB to XYZ matrix from chromaticities */
static void primaries_to_rgb_to_xyz(alwan_scalar rx, alwan_scalar ry,
                                     alwan_scalar gx, alwan_scalar gy,
                                     alwan_scalar bx, alwan_scalar by,
                                     alwan_scalar wx, alwan_scalar wy,
                                     alwan_scalar Y,
                                     alwan_scalar out[9]) {
    alwan_mat3x3 result = aces_primaries_to_rgb_to_xyz_v(rx, ry, gx, gy, bx, by, wx, wy, Y);
    for (int i = 0; i < 9; i++) out[i] = result.m[i];
}

/* Invert 3x3 matrix */
static int invert_mat3(alwan_scalar const m[9], alwan_scalar out[9]) {
    alwan_mat3x3 result = alwan_mat3_inv_v(*(alwan_mat3x3 const *)m);
    for (int i = 0; i < 9; i++) out[i] = result.m[i];
    return 0;
}

/* Multiply 3x3 matrices: out = a * b */
static void mult_mat3(alwan_scalar const a[9], alwan_scalar const b[9], alwan_scalar out[9]) {
    alwan_mat3x3 result = alwan_mat3_mul_v(*(alwan_mat3x3 const *)a, *(alwan_mat3x3 const *)b);
    for (int i = 0; i < 9; i++) out[i] = result.m[i];
}

/* Multiply vector by matrix: out = m * v */
static void mult_vec_mat3(alwan_scalar const v[3], alwan_scalar const m[9], alwan_scalar out[3]) {
    alwan_vec3 vec = {{v[0], v[1], v[2]}};
    alwan_vec3 result = alwan_mat3_mulv_v(*(alwan_mat3x3 const *)m, vec);
    out[0] = result.v[0]; out[1] = result.v[1]; out[2] = result.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Post-adaptation cone response compression
 * Reference: ACES CTL _post_adaptation_cone_response_compression_fwd
 * Formula: Ra = (Rc^0.42) / (27.13 + Rc^0.42)
 * ---------------------------------------------------------------- */

static alwan_scalar post_adaptation_cone_response_compression_fwd(alwan_scalar v) {
    return aces_cone_response_fwd_v(v);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Toe compression function (from OCIO Transform.cpp)
 * Smooth nonlinear compression using quadratic formula
 * ---------------------------------------------------------------- */

static alwan_scalar toe_fwd(alwan_scalar x, alwan_scalar limit,
                            alwan_scalar k1_in, alwan_scalar k2_in) {
    return aces_toe_fwd_v(x, limit, k1_in, k2_in);
}

static alwan_scalar toe_inv(alwan_scalar x, alwan_scalar limit,
                            alwan_scalar k1_in, alwan_scalar k2_in) {
    return aces_toe_inv_v(x, limit, k1_in, k2_in);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Chroma compression normalization (Fourier series)
 * Computes hue-dependent normalization factor using harmonic terms
 * ---------------------------------------------------------------- */

static alwan_scalar chroma_compress_norm(alwan_scalar h_rad, alwan_scalar scale) {
    return aces_chroma_compress_norm_v(h_rad, scale);
}

/* ----------------------------------------------------------------
 * ACES 2.0: JMhParams computation
 * Computes all matrices and parameters needed for RGB to JMh conversion
 * ---------------------------------------------------------------- */

/* Chroma compression parameters (computed from peak luminance) */
typedef struct {
    alwan_scalar sat;                /* Saturation toe parameter */
    alwan_scalar sat_thr;            /* Saturation threshold */
    alwan_scalar compr;              /* Compression parameter */
    alwan_scalar chroma_compress_scale;  /* Scale for chroma normalization */
    alwan_scalar limit_J_max;        /* Maximum J at peak luminance */
    alwan_scalar model_gamma_inv;    /* 1/cz */
    alwan_scalar reach_m_table[ACES2_REACH_TABLE_SIZE];  /* Max chroma per hue */
} ChromaCompressParams;

static void init_JMhParams(alwan_aces_primaries const *primaries, JMhParams *p) {
    /* Compute F_L (luminance adaptation factor) */
    alwan_scalar k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * ACES2_L_A + ALWAN_LITERAL(1.0));
    alwan_scalar k4 = k * k * k * k;
    alwan_scalar F_L = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * ACES2_L_A)
                     + ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4)
                       * ALWAN_POW(ALWAN_LITERAL(5.0) * ACES2_L_A, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    alwan_scalar F_L_n = F_L / ACES2_REF_LUMINANCE;

    /* Compute model gamma (cz) */
    p->cz = ACES2_SURROUND_C * (ALWAN_LITERAL(1.48) + ALWAN_SQRT(ACES2_Y_b / ACES2_REF_LUMINANCE));
    p->inv_cz = ALWAN_LITERAL(1.0) / p->cz;

    /* Build RGB to XYZ matrix for input primaries */
    alwan_scalar rgb_to_xyz[9];
    primaries_to_rgb_to_xyz(primaries->red_x, primaries->red_y,
                            primaries->green_x, primaries->green_y,
                            primaries->blue_x, primaries->blue_y,
                            primaries->white_x, primaries->white_y,
                            ALWAN_LITERAL(1.0), rgb_to_xyz);

    /* Build XYZ to CAM16 RGB matrix (CAM16 primaries) */
    alwan_scalar cam16_rgb_to_xyz[9];
    primaries_to_rgb_to_xyz(CAM16_PRI_RED_X, CAM16_PRI_RED_Y,
                            CAM16_PRI_GREEN_X, CAM16_PRI_GREEN_Y,
                            CAM16_PRI_BLUE_X, CAM16_PRI_BLUE_Y,
                            CAM16_WHITE_X, CAM16_WHITE_Y,
                            ALWAN_LITERAL(1.0), cam16_rgb_to_xyz);

    alwan_scalar xyz_to_cam16_rgb[9];
    invert_mat3(cam16_rgb_to_xyz, xyz_to_cam16_rgb);

    /* Compute XYZ_w for white point (1,1,1) in input primaries */
    alwan_scalar white_rgb[3] = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
    alwan_scalar white_xyz[3];
    mult_vec_mat3(white_rgb, rgb_to_xyz, white_xyz);
    alwan_scalar Y_W = white_xyz[1];

    /* Compute white point in CAM16 RGB */
    alwan_scalar RGB_w[3];
    mult_vec_mat3(white_xyz, xyz_to_cam16_rgb, RGB_w);

    /* Compute chromatic adaptation coefficients (D = 1 for complete adaptation) */
    /* D_RGB[i] = F_L_n * Y_W / RGB_w[i] */
    alwan_scalar D_RGB[3];
    D_RGB[0] = F_L_n * Y_W / RGB_w[0];
    D_RGB[1] = F_L_n * Y_W / RGB_w[1];
    D_RGB[2] = F_L_n * Y_W / RGB_w[2];

    /* Build RGB_to_CAM16 matrix (includes reference_luminance scaling)
     * Order: xyz_to_cam16_rgb @ rgb_to_xyz to get RGB -> CAM16 RGB */
    alwan_scalar rgb_to_cam16_base[9];
    mult_mat3(xyz_to_cam16_rgb, rgb_to_xyz, rgb_to_cam16_base);

    /* Scale by reference_luminance (100) */
    for (int i = 0; i < 9; i++) {
        rgb_to_cam16_base[i] *= ACES2_REF_LUMINANCE;
    }

    /* Apply chromatic adaptation: diag(D_RGB) @ rgb_to_cam16_base */
    p->MATRIX_RGB_to_CAM16_c.m[0] = D_RGB[0] * rgb_to_cam16_base[0];
    p->MATRIX_RGB_to_CAM16_c.m[1] = D_RGB[0] * rgb_to_cam16_base[1];
    p->MATRIX_RGB_to_CAM16_c.m[2] = D_RGB[0] * rgb_to_cam16_base[2];
    p->MATRIX_RGB_to_CAM16_c.m[3] = D_RGB[1] * rgb_to_cam16_base[3];
    p->MATRIX_RGB_to_CAM16_c.m[4] = D_RGB[1] * rgb_to_cam16_base[4];
    p->MATRIX_RGB_to_CAM16_c.m[5] = D_RGB[1] * rgb_to_cam16_base[5];
    p->MATRIX_RGB_to_CAM16_c.m[6] = D_RGB[2] * rgb_to_cam16_base[6];
    p->MATRIX_RGB_to_CAM16_c.m[7] = D_RGB[2] * rgb_to_cam16_base[7];
    p->MATRIX_RGB_to_CAM16_c.m[8] = D_RGB[2] * rgb_to_cam16_base[8];

    /* Compute white in adapted CAM16 space */
    alwan_scalar white_cam16[3];
    mult_vec_mat3(white_rgb, p->MATRIX_RGB_to_CAM16_c.m, white_cam16);

    /* Apply post-adaptation compression to white */
    alwan_scalar rgb_a_w[3];
    rgb_a_w[0] = post_adaptation_cone_response_compression_fwd(white_cam16[0]);
    rgb_a_w[1] = post_adaptation_cone_response_compression_fwd(white_cam16[1]);
    rgb_a_w[2] = post_adaptation_cone_response_compression_fwd(white_cam16[2]);

    /* Build cone_response_to_Aab with cam_nl_scale = 400.0 */
    alwan_scalar cone_to_aab[9];
    for (int i = 0; i < 9; i++) {
        cone_to_aab[i] = CAM_NL_SCALE * CONE_TO_AAB_BASE[i];
    }

    /* A_w = first row of cone_to_aab @ rgb_a_w */
    alwan_scalar A_w = cone_to_aab[0] * rgb_a_w[0] + cone_to_aab[1] * rgb_a_w[1] + cone_to_aab[2] * rgb_a_w[2];

    /* Build MATRIX_cone_response_to_Aab:
     * Row 0: divided by A_w (achromatic normalization)
     * Rows 1-2: multiplied by 43 * surround[2] (chromatic scaling)
     */
    alwan_scalar ab_scale = ALWAN_LITERAL(43.0) * ACES2_SURROUND_N_c;

    p->MATRIX_cone_response_to_Aab.m[0] = cone_to_aab[0] / A_w;
    p->MATRIX_cone_response_to_Aab.m[1] = cone_to_aab[1] / A_w;
    p->MATRIX_cone_response_to_Aab.m[2] = cone_to_aab[2] / A_w;
    p->MATRIX_cone_response_to_Aab.m[3] = cone_to_aab[3] * ab_scale;
    p->MATRIX_cone_response_to_Aab.m[4] = cone_to_aab[4] * ab_scale;
    p->MATRIX_cone_response_to_Aab.m[5] = cone_to_aab[5] * ab_scale;
    p->MATRIX_cone_response_to_Aab.m[6] = cone_to_aab[6] * ab_scale;
    p->MATRIX_cone_response_to_Aab.m[7] = cone_to_aab[7] * ab_scale;
    p->MATRIX_cone_response_to_Aab.m[8] = cone_to_aab[8] * ab_scale;

    /* A_w_J is the achromatic response to F_L (for J normalization) */
    p->A_w_J = post_adaptation_cone_response_compression_fwd(F_L);
    p->inv_A_w_J = ALWAN_LITERAL(1.0) / p->A_w_J;

    /* Store F_L_n for J to Y conversion */
    p->F_L_n = F_L_n;

    /* Compute and store inverse matrices for Aab_to_RGB conversion */
    p->MATRIX_CAM16_to_RGB = alwan_mat3_inv_v(p->MATRIX_RGB_to_CAM16_c);
    p->MATRIX_Aab_to_cone = alwan_mat3_inv_v(p->MATRIX_cone_response_to_Aab);
}

/* ----------------------------------------------------------------
 * ACES 2.0: RGB to Aab conversion
 * ---------------------------------------------------------------- */

static void RGB_to_Aab(alwan_scalar const rgb[3], JMhParams const *p, alwan_scalar aab[3]) {
    alwan_vec3 in = {{rgb[0], rgb[1], rgb[2]}};
    alwan_vec3 out = aces2_rgb_to_aab_v(in, p);
    aab[0] = out.v[0]; aab[1] = out.v[1]; aab[2] = out.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Aab to JMh conversion
 * J = J_scale * A^cz
 * M = sqrt(a^2 + b^2)
 * h = atan2(b, a) in degrees [0, 360)
 * ---------------------------------------------------------------- */

static void Aab_to_JMh(alwan_scalar const aab[3], JMhParams const *p, alwan_scalar jmh[3]) {
    alwan_vec3 in = {{aab[0], aab[1], aab[2]}};
    alwan_vec3 out = aces2_aab_to_jmh_v(in, p);
    jmh[0] = out.v[0]; jmh[1] = out.v[1]; jmh[2] = out.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Inverse cone response compression
 * Ra_lim = min(Ra, 0.99)
 * F_L_Y = cam_nl_offset * Ra_lim / (1 - Ra_lim)
 * Rc = F_L_Y^(1/0.42)
 * ---------------------------------------------------------------- */

static alwan_scalar post_adaptation_cone_response_compression_inv(alwan_scalar Ra) {
    return aces_cone_response_inv_v(Ra);
}

/* ----------------------------------------------------------------
 * ACES 2.0: JMh to Aab conversion (inverse)
 * A = (J / J_scale)^(1/cz)
 * a = M * cos(h_rad)
 * b = M * sin(h_rad)
 * ---------------------------------------------------------------- */

static void JMh_to_Aab(alwan_scalar const jmh[3], JMhParams const *p, alwan_scalar aab[3]) {
    alwan_vec3 in = {{jmh[0], jmh[1], jmh[2]}};
    alwan_vec3 out = aces2_jmh_to_aab_v(in, p);
    aab[0] = out.v[0]; aab[1] = out.v[1]; aab[2] = out.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Aab to RGB conversion (inverse)
 * Uses precomputed inverse matrices from JMhParams
 * ---------------------------------------------------------------- */

static void Aab_to_RGB(alwan_scalar const aab[3], JMhParams const *p, alwan_scalar rgb[3]) {
    alwan_vec3 in = {{aab[0], aab[1], aab[2]}};
    alwan_vec3 out = aces2_aab_to_rgb_v(in, p);
    rgb[0] = out.v[0]; rgb[1] = out.v[1]; rgb[2] = out.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: J to Y and Y to J conversions for tonescale
 *
 * J to Y:
 *   1. A = (J / J_scale)^(1/cz)
 *   2. Y = inverse_cone_response(A_w_J * A) / F_L_n
 *
 * Y to J:
 *   1. Ra = compression(Y * F_L_n)
 *   2. J = J_scale * (Ra / A_w_J)^cz
 * ---------------------------------------------------------------- */

static alwan_scalar J_to_Y(alwan_scalar J, JMhParams const *p) {
    return aces2_j_to_y_v(J, p);
}

static alwan_scalar Y_to_J(alwan_scalar Y, JMhParams const *p) {
    return aces2_y_to_j_v(Y, p);
}

/* Forward declaration for use in TonescaleCompress20 */
void alwan_aces_primaries_ap1_default(alwan_aces_primaries *primaries);

/* ----------------------------------------------------------------
 * ACES 2.0: TonescaleCompress20
 * Reference: ACES CTL Lib.Academy.Tonescale.ctl
 * Full pipeline: RGB -> JMh -> tonescale J -> JMh -> RGB
 * ---------------------------------------------------------------- */

static void init_TSParams(alwan_scalar peak_luminance, TSParams *ts) {
    /* Constants from ACES CTL */
    static const alwan_scalar n_r = ALWAN_LITERAL(100.0);
    static const alwan_scalar g = ALWAN_LITERAL(1.15);
    static const alwan_scalar c = ALWAN_LITERAL(0.18);
    static const alwan_scalar c_d = ALWAN_LITERAL(10.013);
    static const alwan_scalar w_g = ALWAN_LITERAL(0.14);
    static const alwan_scalar t_1 = ALWAN_LITERAL(0.04);
    static const alwan_scalar r_hit_min = ALWAN_LITERAL(128.0);
    static const alwan_scalar r_hit_max = ALWAN_LITERAL(896.0);

    ts->n = peak_luminance;
    ts->n_r = n_r;
    ts->g = g;
    ts->t_1 = t_1;

    /* Computed parameters */
    alwan_scalar r_hit = r_hit_min + (r_hit_max - r_hit_min)
                       * (ALWAN_LN(peak_luminance / n_r) / ALWAN_LN(ALWAN_LITERAL(10000.0) / ALWAN_LITERAL(100.0)));

    alwan_scalar m_0 = peak_luminance / n_r;
    alwan_scalar m_1 = ALWAN_LITERAL(0.5) * (m_0 + ALWAN_SQRT(m_0 * (m_0 + ALWAN_LITERAL(4.0) * t_1)));

    alwan_scalar u = ALWAN_POW((r_hit / m_1) / ((r_hit / m_1) + ALWAN_LITERAL(1.0)), g);
    alwan_scalar m = m_1 / u;

    alwan_scalar w_i = ALWAN_LN(peak_luminance / ALWAN_LITERAL(100.0)) / ALWAN_LN(ALWAN_LITERAL(2.0));
    alwan_scalar c_t = c_d / n_r * (ALWAN_LITERAL(1.0) + w_i * w_g);

    alwan_scalar g_ip = ALWAN_LITERAL(0.5) * (c_t + ALWAN_SQRT(c_t * (c_t + ALWAN_LITERAL(4.0) * t_1)));
    alwan_scalar g_ipp2 = -(m_1 * ALWAN_POW(g_ip / m, ALWAN_LITERAL(1.0) / g))
                        / (ALWAN_POW(g_ip / m, ALWAN_LITERAL(1.0) / g) - ALWAN_LITERAL(1.0));

    alwan_scalar w_2 = c / g_ipp2;
    ts->s_2 = w_2 * m_1 * n_r;  /* OCIO: s_2 = w_2 * m_1 * reference_luminance */
    ts->u_2 = ALWAN_POW((r_hit / m_1) / ((r_hit / m_1) + w_2), g);
    ts->m_2 = m_1 / ts->u_2;
}

static alwan_scalar tonescale_fwd(alwan_scalar x, TSParams const *ts) {
    return aces2_tonescale_fwd_v(x, ts);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Reach table generation via binary search
 * Finds maximum M (chroma) at limit_J_max for each hue before
 * any RGB channel goes negative
 * ---------------------------------------------------------------- */

static void make_reach_m_table(JMhParams const *p, alwan_scalar limit_J_max,
                               alwan_scalar reach_table[ACES2_REACH_TABLE_SIZE]) {
    static const alwan_scalar SEARCH_RANGE = ALWAN_LITERAL(50.0);
    static const alwan_scalar SEARCH_MAX = ALWAN_LITERAL(1300.0);
    static const alwan_scalar SEARCH_TOL = ALWAN_LITERAL(0.01);

    for (int i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
        alwan_scalar hue = (alwan_scalar)i;  /* Hue in degrees [0, 359] */
        alwan_scalar low = ALWAN_LITERAL(0.0);
        alwan_scalar high = SEARCH_RANGE;

        /* Coarse search: find upper bound where RGB goes negative */
        while (high < SEARCH_MAX) {
            alwan_scalar jmh[3] = {limit_J_max, high, hue};
            alwan_scalar aab[3], rgb[3];
            JMh_to_Aab(jmh, p, aab);
            Aab_to_RGB(aab, p, rgb);

            /* Check if any channel goes negative */
            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) || rgb[2] < ALWAN_LITERAL(0.0)) {
                break;
            }
            low = high;
            high += SEARCH_RANGE;
        }

        /* Binary search refinement */
        while ((high - low) > SEARCH_TOL) {
            alwan_scalar mid = (high + low) * ALWAN_LITERAL(0.5);
            alwan_scalar jmh[3] = {limit_J_max, mid, hue};
            alwan_scalar aab[3], rgb[3];
            JMh_to_Aab(jmh, p, aab);
            Aab_to_RGB(aab, p, rgb);

            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) || rgb[2] < ALWAN_LITERAL(0.0)) {
                high = mid;
            } else {
                low = mid;
            }
        }

        reach_table[i] = high;
    }
}

/* ----------------------------------------------------------------
 * ACES 2.0: Initialize chroma compression parameters
 * Reference: OCIO Transform.cpp init_ChromaCompressParams
 * ---------------------------------------------------------------- */

static void init_ChromaCompressParams(alwan_scalar peak_luminance,
                                       JMhParams const *jmh_params,
                                       ChromaCompressParams *cp) {
    static const alwan_scalar n_r = ALWAN_LITERAL(100.0);

    /* Compute log_peak = log10(peak_luminance / n_r) */
    alwan_scalar log_peak = ALWAN_LN(peak_luminance / n_r) / ALWAN_LN(ALWAN_LITERAL(10.0));

    /* compr = chroma_compress + (chroma_compress * chroma_compress_fact) * log_peak */
    cp->compr = ACES2_CHROMA_COMPRESS
              + (ACES2_CHROMA_COMPRESS * ACES2_CHROMA_COMPRESS_FACT) * log_peak;

    /* sat = max(0.2, chroma_expand - (chroma_expand * chroma_expand_fact) * log_peak) */
    alwan_scalar sat_val = ACES2_CHROMA_EXPAND
                         - (ACES2_CHROMA_EXPAND * ACES2_CHROMA_EXPAND_FACT) * log_peak;
    cp->sat = (sat_val > ALWAN_LITERAL(0.2)) ? sat_val : ALWAN_LITERAL(0.2);

    /* sat_thr = chroma_expand_thr / peak_luminance */
    cp->sat_thr = ACES2_CHROMA_EXPAND_THR / peak_luminance;

    /* chroma_compress_scale = pow(0.03379 * peak_luminance, 0.30596) - 0.45135 */
    cp->chroma_compress_scale = ALWAN_POW(ALWAN_LITERAL(0.03379) * peak_luminance,
                                           ALWAN_LITERAL(0.30596))
                              - ALWAN_LITERAL(0.45135);

    /* limit_J_max = Y_to_J(peak_luminance) */
    cp->limit_J_max = Y_to_J(peak_luminance, jmh_params);

    /* model_gamma_inv = 1/cz */
    cp->model_gamma_inv = jmh_params->inv_cz;

    /* Generate reach table via binary search */
    make_reach_m_table(jmh_params, cp->limit_J_max, cp->reach_m_table);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Lookup reach_m for a given hue with linear interpolation
 * ---------------------------------------------------------------- */

static alwan_scalar lookup_reach_m(alwan_scalar h_deg, ChromaCompressParams const *cp) {
    /* Wrap hue to [0, 360) */
    while (h_deg < ALWAN_LITERAL(0.0)) h_deg += ALWAN_LITERAL(360.0);
    while (h_deg >= ALWAN_LITERAL(360.0)) h_deg -= ALWAN_LITERAL(360.0);

    /* Linear interpolation between table entries */
    int idx0 = (int)h_deg;
    int idx1 = (idx0 + 1) % ACES2_REACH_TABLE_SIZE;
    alwan_scalar t = h_deg - (alwan_scalar)idx0;

    return cp->reach_m_table[idx0] * (ALWAN_LITERAL(1.0) - t)
         + cp->reach_m_table[idx1] * t;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Chroma compression forward
 * Reference: OCIO Transform.cpp chroma_compress_fwd
 * ---------------------------------------------------------------- */

static alwan_scalar chroma_compress_fwd(alwan_scalar J, alwan_scalar M, alwan_scalar h_deg,
                                         alwan_scalar J_ts,
                                         ChromaCompressParams const *cp) {
    /* Handle edge cases */
    if (J <= ALWAN_LITERAL(0.0) || M <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }

    /* Convert hue to radians for Mnorm calculation */
    alwan_scalar h_rad = h_deg * ALWAN_LITERAL(3.14159265358979323846) / ALWAN_LITERAL(180.0);

    /* Compute Mnorm using Fourier series */
    alwan_scalar Mnorm = chroma_compress_norm(h_rad, cp->chroma_compress_scale);

    /* Scale M by (J_ts/J)^model_gamma_inv */
    alwan_scalar M_cp = M * ALWAN_POW(J_ts / J, cp->model_gamma_inv);

    /* Normalize by Mnorm */
    M_cp = M_cp / Mnorm;

    /* Normalized J */
    alwan_scalar nJ = J_ts / cp->limit_J_max;
    alwan_scalar snJ = (ALWAN_LITERAL(1.0) - nJ > ALWAN_LITERAL(0.0))
                     ? (ALWAN_LITERAL(1.0) - nJ) : ALWAN_LITERAL(0.0);

    /* Look up reach for this hue */
    alwan_scalar reachMaxM = lookup_reach_m(h_deg, cp);

    /* Compute limit for toe functions */
    alwan_scalar limit = ALWAN_POW(nJ, cp->model_gamma_inv) * reachMaxM / Mnorm;

    /* Saturation toe (lower bound compression) */
    alwan_scalar sat_limit = limit - ALWAN_LITERAL(0.001);
    if (sat_limit < ALWAN_LITERAL(0.001)) sat_limit = ALWAN_LITERAL(0.001);
    M_cp = limit - toe_fwd(limit - M_cp, sat_limit,
                           snJ * cp->sat,
                           ALWAN_SQRT(nJ * nJ + cp->sat_thr));

    /* Compression toe (upper bound compression) */
    M_cp = toe_fwd(M_cp, limit, nJ * cp->compr, snJ);

    /* Denormalize */
    M_cp = M_cp * Mnorm;

    return M_cp;
}

int alwan_aces_tonescale_compress20(alwan_rgb *rgb_out,
                                     alwan_rgb const *rgb_in,
                                     alwan_scalar peak_luminance) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (peak_luminance <= ALWAN_LITERAL(0.0)) return ALWAN_E_INVALID;

    /* Initialize JMh parameters (using AP1 primaries) */
    alwan_aces_primaries primaries;
    alwan_aces_primaries_ap1_default(&primaries);
    JMhParams jmh_params;
    init_JMhParams(&primaries, &jmh_params);

    /* Initialize tonescale parameters */
    TSParams ts;
    init_TSParams(peak_luminance, &ts);

    /* Initialize chroma compression parameters */
    ChromaCompressParams chroma_params;
    init_ChromaCompressParams(peak_luminance, &jmh_params, &chroma_params);

    /* Step 1: Convert RGB to Aab */
    alwan_scalar rgb[3] = {rgb_in->r, rgb_in->g, rgb_in->b};
    alwan_scalar aab[3];
    RGB_to_Aab(rgb, &jmh_params, aab);

    /* Step 2: Convert Aab to JMh */
    alwan_scalar jmh[3];
    Aab_to_JMh(aab, &jmh_params, jmh);

    /* Step 3: Convert J to Y, apply tonescale, convert back to J */
    alwan_scalar Y_in = J_to_Y(jmh[0], &jmh_params);
    alwan_scalar Y_out = tonescale_fwd(Y_in, &ts);
    alwan_scalar J_ts = Y_to_J(Y_out, &jmh_params);

    /* Step 4: Apply chroma compression */
    alwan_scalar M_out = chroma_compress_fwd(jmh[0], jmh[1], jmh[2],
                                              J_ts, &chroma_params);

    /* Step 5: Build output JMh */
    alwan_scalar jmh_out[3] = {J_ts, M_out, jmh[2]};

    /* Step 6: Convert JMh back to Aab */
    alwan_scalar aab_out[3];
    JMh_to_Aab(jmh_out, &jmh_params, aab_out);

    /* Step 7: Convert Aab back to RGB */
    alwan_scalar rgb_result[3];
    Aab_to_RGB(aab_out, &jmh_params, rgb_result);

    rgb_out->r = rgb_result[0];
    rgb_out->g = rgb_result[1];
    rgb_out->b = rgb_result[2];

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES 2.0: RGB to JMh
 * Reference: ACES CTL Lib.Academy.OutputTransform.ctl
 * ---------------------------------------------------------------- */

void alwan_aces_primaries_ap1_default(alwan_aces_primaries *primaries) {
    if (!primaries) return;
    primaries->red_x   = AP1_RED_x;
    primaries->red_y   = AP1_RED_y;
    primaries->green_x = AP1_GREEN_x;
    primaries->green_y = AP1_GREEN_y;
    primaries->blue_x  = AP1_BLUE_x;
    primaries->blue_y  = AP1_BLUE_y;
    primaries->white_x = AP1_WHITE_x;
    primaries->white_y = AP1_WHITE_y;
}

int alwan_aces_rgb_to_jmh20(alwan_vec3 *jmh_out,
                            alwan_rgb const *rgb_in,
                            alwan_aces_primaries const *primaries) {
    if (!jmh_out || !rgb_in || !primaries) return ALWAN_E_INVALID;

    /* Initialize JMh conversion parameters */
    JMhParams p;
    init_JMhParams(primaries, &p);

    /* Convert RGB to Aab */
    alwan_scalar rgb[3] = {rgb_in->r, rgb_in->g, rgb_in->b};
    alwan_scalar aab[3];
    RGB_to_Aab(rgb, &p, aab);

    /* Convert Aab to JMh */
    alwan_scalar jmh[3];
    Aab_to_JMh(aab, &p, jmh);

    jmh_out->v[0] = jmh[0];
    jmh_out->v[1] = jmh[1];
    jmh_out->v[2] = jmh[2];

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES 2.0: JMh to RGB (inverse of RGB to JMh)
 * ---------------------------------------------------------------- */

int alwan_aces_jmh_to_rgb20(alwan_rgb *rgb_out,
                            alwan_vec3 const *jmh_in,
                            alwan_aces_primaries const *primaries) {
    if (!rgb_out || !jmh_in || !primaries) return ALWAN_E_INVALID;

    /* Initialize JMh conversion parameters */
    JMhParams p;
    init_JMhParams(primaries, &p);

    /* Convert JMh to Aab */
    alwan_scalar jmh[3] = {jmh_in->v[0], jmh_in->v[1], jmh_in->v[2]};
    alwan_scalar aab[3];
    JMh_to_Aab(jmh, &p, aab);

    /* Convert Aab to RGB */
    alwan_scalar rgb[3];
    Aab_to_RGB(aab, &p, rgb);

    rgb_out->r = rgb[0];
    rgb_out->g = rgb[1];
    rgb_out->b = rgb[2];

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression Constants (from OCIO Common.h)
 * Reference: OpenColorIO/src/OpenColorIO/ops/fixedfunction/ACES2/Common.h
 * ---------------------------------------------------------------- */

static const alwan_scalar GAMUT_COMPRESSION_THRESHOLD = ACES_GAMUT_COMPRESSION_THRESHOLD;
static const alwan_scalar GAMUT_SMOOTH_CUSPS = ACES_GAMUT_SMOOTH_CUSPS;
static const alwan_scalar GAMUT_FOCUS_GAIN_BLEND = ALWAN_LITERAL(0.3);
static const alwan_scalar GAMUT_CUSP_MID_BLEND = ACES_GAMUT_CUSP_MID_BLEND;
static const alwan_scalar GAMUT_FOCUS_DISTANCE = ALWAN_LITERAL(1.35);
static const alwan_scalar GAMUT_FOCUS_ADJUST_GAIN_INV = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.55);

/* Number of gamut corners (R, Y, G, C, B, M) */
#define ACES2_CUSP_CORNER_COUNT 6

/* Lower hull gamma (constant across all hues) */
static const alwan_scalar GAMUT_LOWER_HULL_GAMMA = ALWAN_LITERAL(1.14);

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression Parameter Structures
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * ACES 2.0: Smooth minimum function (smin)
 * Creates smooth transition between two boundaries
 * ---------------------------------------------------------------- */

static alwan_scalar smin_scaled(alwan_scalar a, alwan_scalar b, alwan_scalar cusp_M) {
    return aces2_smin_scaled_v(a, b, cusp_M);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Reinhard remapping for M compression
 * ---------------------------------------------------------------- */

static alwan_scalar reinhard_fwd(alwan_scalar x) {
    return aces2_reinhard_fwd_v(x);
}

static alwan_scalar reinhard_inv(alwan_scalar x) {
    return aces2_reinhard_inv_v(x);
}

static alwan_scalar remap_M_fwd(alwan_scalar M, alwan_scalar gamut_boundary_M,
                                 alwan_scalar reach_boundary_M) {
    return aces2_remap_m_fwd_v(M, gamut_boundary_M, reach_boundary_M);
}

static alwan_scalar remap_M_inv(alwan_scalar M, alwan_scalar gamut_boundary_M,
                                 alwan_scalar reach_boundary_M) {
    return aces2_remap_m_inv_v(M, gamut_boundary_M, reach_boundary_M);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Focus geometry computations
 * ---------------------------------------------------------------- */

static alwan_scalar compute_focus_J(alwan_scalar cusp_J, alwan_scalar mid_J,
                                     alwan_scalar limit_J_max) {
    return aces2_compute_focus_j_v(cusp_J, mid_J, limit_J_max);
}

static alwan_scalar get_focus_gain(alwan_scalar J, alwan_scalar analytical_threshold,
                                    alwan_scalar limit_J_max, alwan_scalar focus_dist) {
    return aces2_get_focus_gain_v(J, analytical_threshold, limit_J_max, focus_dist);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Solve J intersection for compression vector
 * ---------------------------------------------------------------- */

static alwan_scalar solve_J_intersect(alwan_scalar J, alwan_scalar M,
                                       alwan_scalar focus_J, alwan_scalar max_J,
                                       alwan_scalar slope_gain) {
    return aces2_solve_j_intersect_v(J, M, focus_J, max_J, slope_gain);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Compute compression vector slope
 * ---------------------------------------------------------------- */

static alwan_scalar compute_compression_vector_slope(alwan_scalar J_intersect,
                                                      alwan_scalar focus_J,
                                                      alwan_scalar limit_J_max,
                                                      alwan_scalar slope_gain) {
    return aces2_compression_vector_slope_v(J_intersect, focus_J, limit_J_max, slope_gain);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Estimate line-boundary intersection M
 * Uses gamma parameterization of gamut hull
 * ---------------------------------------------------------------- */

static alwan_scalar estimate_line_boundary_M(alwan_scalar J_intersect_source,
                                              alwan_scalar slope,
                                              alwan_scalar gamma_inv,
                                              alwan_scalar cusp_J,
                                              alwan_scalar cusp_M,
                                              alwan_scalar J_intersect_cusp) {
    return aces2_estimate_line_boundary_m_v(J_intersect_source, slope, gamma_inv, cusp_J, cusp_M, J_intersect_cusp);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Find gamut boundary intersection
 * Combines lower and upper hull boundaries with smooth blending
 * ---------------------------------------------------------------- */

static alwan_scalar find_gamut_boundary_intersection(alwan_scalar cusp_J,
                                                      alwan_scalar cusp_M,
                                                      alwan_scalar limit_J_max,
                                                      alwan_scalar gamma_top_inv,
                                                      alwan_scalar gamma_bottom_inv,
                                                      alwan_scalar J_intersect_source,
                                                      alwan_scalar slope,
                                                      alwan_scalar J_intersect_cusp) {
    return aces2_find_gamut_boundary_v(cusp_J, cusp_M, limit_J_max, gamma_top_inv, gamma_bottom_inv, J_intersect_source, slope, J_intersect_cusp);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Build cusp table for limit primaries
 * Finds the gamut boundary (max M) at each hue degree
 * ---------------------------------------------------------------- */

static void build_cusp_table_for_hue(alwan_scalar hue_deg, JMhParams const *p,
                                      alwan_scalar limit_J_max,
                                      GamutCuspEntry *cusp) {
    /* Binary search for maximum M at this hue while staying in-gamut */
    static const alwan_scalar SEARCH_RANGE = ALWAN_LITERAL(50.0);
    static const alwan_scalar SEARCH_MAX = ALWAN_LITERAL(500.0);
    static const alwan_scalar SEARCH_TOL = ALWAN_LITERAL(0.001);

    /* Find J at cusp by searching for max M */
    alwan_scalar best_M = ALWAN_LITERAL(0.0);
    alwan_scalar best_J = ALWAN_LITERAL(0.0);

    /* Sample J values to find approximate cusp location */
    for (alwan_scalar J_sample = ALWAN_LITERAL(10.0);
         J_sample < limit_J_max - ALWAN_LITERAL(1.0);
         J_sample += ALWAN_LITERAL(5.0)) {

        /* Binary search for max M at this J */
        alwan_scalar low = ALWAN_LITERAL(0.0);
        alwan_scalar high = SEARCH_RANGE;

        while (high < SEARCH_MAX) {
            alwan_scalar jmh[3] = {J_sample, high, hue_deg};
            alwan_scalar aab[3], rgb[3];
            JMh_to_Aab(jmh, p, aab);
            Aab_to_RGB(aab, p, rgb);

            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) ||
                rgb[2] < ALWAN_LITERAL(0.0) || rgb[0] > ALWAN_LITERAL(1.0) ||
                rgb[1] > ALWAN_LITERAL(1.0) || rgb[2] > ALWAN_LITERAL(1.0)) {
                break;
            }
            low = high;
            high += SEARCH_RANGE;
        }

        /* Refine with binary search */
        while ((high - low) > SEARCH_TOL) {
            alwan_scalar mid = (high + low) * ALWAN_LITERAL(0.5);
            alwan_scalar jmh[3] = {J_sample, mid, hue_deg};
            alwan_scalar aab[3], rgb[3];
            JMh_to_Aab(jmh, p, aab);
            Aab_to_RGB(aab, p, rgb);

            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) ||
                rgb[2] < ALWAN_LITERAL(0.0) || rgb[0] > ALWAN_LITERAL(1.0) ||
                rgb[1] > ALWAN_LITERAL(1.0) || rgb[2] > ALWAN_LITERAL(1.0)) {
                high = mid;
            } else {
                low = mid;
            }
        }

        if (low > best_M) {
            best_M = low;
            best_J = J_sample;
        }
    }

    /* Refine J at cusp */
    alwan_scalar J_low = best_J - ALWAN_LITERAL(5.0);
    alwan_scalar J_high = best_J + ALWAN_LITERAL(5.0);
    if (J_low < ALWAN_LITERAL(1.0)) J_low = ALWAN_LITERAL(1.0);
    if (J_high > limit_J_max - ALWAN_LITERAL(1.0)) J_high = limit_J_max - ALWAN_LITERAL(1.0);

    while ((J_high - J_low) > ALWAN_LITERAL(0.1)) {
        alwan_scalar J_mid1 = J_low + (J_high - J_low) * ALWAN_LITERAL(0.33);
        alwan_scalar J_mid2 = J_low + (J_high - J_low) * ALWAN_LITERAL(0.67);

        /* Get M at each J */
        alwan_scalar M1 = ALWAN_LITERAL(0.0), M2 = ALWAN_LITERAL(0.0);
        for (int iter = 0; iter < 2; iter++) {
            alwan_scalar J_test = (iter == 0) ? J_mid1 : J_mid2;
            alwan_scalar low_m = ALWAN_LITERAL(0.0);
            alwan_scalar high_m = best_M + ALWAN_LITERAL(50.0);

            while ((high_m - low_m) > SEARCH_TOL) {
                alwan_scalar mid_m = (high_m + low_m) * ALWAN_LITERAL(0.5);
                alwan_scalar jmh[3] = {J_test, mid_m, hue_deg};
                alwan_scalar aab[3], rgb[3];
                JMh_to_Aab(jmh, p, aab);
                Aab_to_RGB(aab, p, rgb);

                if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) ||
                    rgb[2] < ALWAN_LITERAL(0.0) || rgb[0] > ALWAN_LITERAL(1.0) ||
                    rgb[1] > ALWAN_LITERAL(1.0) || rgb[2] > ALWAN_LITERAL(1.0)) {
                    high_m = mid_m;
                } else {
                    low_m = mid_m;
                }
            }
            if (iter == 0) M1 = low_m;
            else M2 = low_m;
        }

        if (M1 > M2) {
            J_high = J_mid2;
            if (M1 > best_M) {
                best_M = M1;
                best_J = J_mid1;
            }
        } else {
            J_low = J_mid1;
            if (M2 > best_M) {
                best_M = M2;
                best_J = J_mid2;
            }
        }
    }

    cusp->J = best_J;
    cusp->M = best_M;

    /* Compute gamma_top_inv for upper hull approximation */
    /* Use simple estimate based on J position */
    alwan_scalar J_ratio = best_J / limit_J_max;
    cusp->gamma_top_inv = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(1.0) + J_ratio);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Initialize gamut compression parameters
 * ---------------------------------------------------------------- */

static void init_GamutCompressParams(alwan_scalar peak_luminance,
                                      alwan_aces_primaries const *limit_primaries,
                                      JMhParams const *jmh_params,
                                      GamutCompressParams *gcp) {
    /* Basic parameters */
    gcp->limit_J_max = Y_to_J(peak_luminance, jmh_params);
    gcp->mid_J = Y_to_J(ALWAN_LITERAL(0.18) * ACES2_REF_LUMINANCE, jmh_params);
    gcp->focus_dist = GAMUT_FOCUS_DISTANCE;
    gcp->lower_hull_gamma_inv = ALWAN_LITERAL(1.0) / GAMUT_LOWER_HULL_GAMMA;
    gcp->model_gamma_inv = jmh_params->inv_cz;

    /* Initialize JMhParams for limit primaries */
    JMhParams limit_params;
    init_JMhParams(limit_primaries, &limit_params);

    /* Build reach table (max M for reach gamut at limit_J_max) */
    make_reach_m_table(jmh_params, gcp->limit_J_max, gcp->reach_m_table);

    /* Find max reach M across all hues */
    gcp->reach_max_M = ALWAN_LITERAL(0.0);
    for (int i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
        if (gcp->reach_m_table[i] > gcp->reach_max_M) {
            gcp->reach_max_M = gcp->reach_m_table[i];
        }
    }

    /* Build cusp table for limit primaries */
    /* First and last entries are duplicates for wrap-around interpolation */
    for (int i = 0; i < 360; i++) {
        build_cusp_table_for_hue((alwan_scalar)i, &limit_params,
                                  gcp->limit_J_max, &gcp->cusp_table[i + 1]);
        gcp->hue_table[i + 1] = (alwan_scalar)i;
    }

    /* Wrap-around entries */
    gcp->cusp_table[0] = gcp->cusp_table[360];
    gcp->hue_table[0] = gcp->hue_table[360] - ALWAN_LITERAL(360.0);
    gcp->cusp_table[361] = gcp->cusp_table[1];
    gcp->hue_table[361] = gcp->hue_table[1] + ALWAN_LITERAL(360.0);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Lookup cusp with interpolation
 * ---------------------------------------------------------------- */

static void lookup_cusp(alwan_scalar h_deg, GamutCompressParams const *gcp,
                         alwan_scalar *cusp_J, alwan_scalar *cusp_M,
                         alwan_scalar *gamma_top_inv) {
    /* Wrap hue to [0, 360) */
    while (h_deg < ALWAN_LITERAL(0.0)) h_deg += ALWAN_LITERAL(360.0);
    while (h_deg >= ALWAN_LITERAL(360.0)) h_deg -= ALWAN_LITERAL(360.0);

    /* Linear interpolation */
    int idx0 = (int)h_deg + 1;  /* +1 for offset due to wrap entry */
    int idx1 = idx0 + 1;
    alwan_scalar t = h_deg - (alwan_scalar)(idx0 - 1);

    *cusp_J = gcp->cusp_table[idx0].J * (ALWAN_LITERAL(1.0) - t)
            + gcp->cusp_table[idx1].J * t;
    *cusp_M = gcp->cusp_table[idx0].M * (ALWAN_LITERAL(1.0) - t)
            + gcp->cusp_table[idx1].M * t;
    *gamma_top_inv = gcp->cusp_table[idx0].gamma_top_inv * (ALWAN_LITERAL(1.0) - t)
                   + gcp->cusp_table[idx1].gamma_top_inv * t;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Initialize hue-dependent parameters
 * ---------------------------------------------------------------- */

static void init_hue_dependent_params(alwan_scalar h_deg,
                                       GamutCompressParams const *gcp,
                                       HueDependentGamutParams *hdp) {
    /* Look up cusp for this hue */
    lookup_cusp(h_deg, gcp, &hdp->cusp_J, &hdp->cusp_M, &hdp->gamma_top_inv);

    hdp->gamma_bottom_inv = gcp->lower_hull_gamma_inv;

    /* Compute focus J */
    hdp->focus_J = compute_focus_J(hdp->cusp_J, gcp->mid_J, gcp->limit_J_max);

    /* Compute analytical threshold (above which focus gain increases) */
    hdp->analytical_threshold = hdp->cusp_J +
        GAMUT_FOCUS_GAIN_BLEND * (gcp->limit_J_max - hdp->cusp_J);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Core gamut compression algorithm (forward)
 * ---------------------------------------------------------------- */

static void compress_gamut_fwd(alwan_scalar J, alwan_scalar M, alwan_scalar h,
                                GamutCompressParams const *gcp,
                                HueDependentGamutParams const *hdp,
                                alwan_scalar *J_out, alwan_scalar *M_out) {
    alwan_vec2 result = aces2_compress_gamut_fwd_v(J, M, h, gcp, hdp);
    *J_out = result.v[0]; *M_out = result.v[1];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Core gamut compression algorithm (inverse)
 * ---------------------------------------------------------------- */

static void compress_gamut_inv(alwan_scalar J, alwan_scalar M, alwan_scalar h,
                                GamutCompressParams const *gcp,
                                HueDependentGamutParams const *hdp,
                                alwan_scalar *J_out, alwan_scalar *M_out) {
    alwan_vec2 result = aces2_compress_gamut_inv_v(J, M, h, gcp, hdp);
    *J_out = result.v[0]; *M_out = result.v[1];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression in JMh space (public API)
 * Reference: OCIO Transform.cpp gamut_compress_fwd
 * ---------------------------------------------------------------- */

/* Check if primaries are approximately equal to AP1 */
static int primaries_are_ap1(alwan_aces_primaries const *p) {
    static const alwan_scalar tol = ALWAN_LITERAL(0.001);
    return ALWAN_ABS(p->red_x - AP1_RED_x) < tol &&
           ALWAN_ABS(p->red_y - AP1_RED_y) < tol &&
           ALWAN_ABS(p->green_x - AP1_GREEN_x) < tol &&
           ALWAN_ABS(p->green_y - AP1_GREEN_y) < tol &&
           ALWAN_ABS(p->blue_x - AP1_BLUE_x) < tol &&
           ALWAN_ABS(p->blue_y - AP1_BLUE_y) < tol;
}

int alwan_aces_gamut_compress20(alwan_vec3 *jmh_out,
                                 alwan_vec3 const *jmh_in,
                                 alwan_scalar peak_luminance,
                                 alwan_aces_primaries const *limit_primaries) {
    if (!jmh_out || !jmh_in || !limit_primaries) return ALWAN_E_INVALID;
    if (peak_luminance < ALWAN_LITERAL(1.0) || peak_luminance > ALWAN_LITERAL(10000.0)) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar J = jmh_in->v[0];
    alwan_scalar M = jmh_in->v[1];
    alwan_scalar h = jmh_in->v[2];

    /* Edge cases */
    if (J <= ALWAN_LITERAL(0.0)) {
        jmh_out->v[0] = ALWAN_LITERAL(0.0);
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return ALWAN_OK;
    }

    /* When limit primaries equal reach primaries (AP1), no compression needed.
     * This matches OCIO behavior: colors within AP1 gamut pass through unchanged.
     */
    if (primaries_are_ap1(limit_primaries)) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = M;
        jmh_out->v[2] = h;
        return ALWAN_OK;
    }

    /* Initialize JMh parameters for reach gamut (AP1) */
    alwan_aces_primaries reach_primaries;
    alwan_aces_primaries_ap1_default(&reach_primaries);
    JMhParams jmh_params;
    init_JMhParams(&reach_primaries, &jmh_params);

    /* Compute limit_J_max */
    alwan_scalar limit_J_max = Y_to_J(peak_luminance, &jmh_params);

    if (M <= ALWAN_LITERAL(0.0) || J > limit_J_max) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return ALWAN_OK;
    }

    /* Initialize gamut compression parameters */
    GamutCompressParams gcp;
    init_GamutCompressParams(peak_luminance, limit_primaries, &jmh_params, &gcp);

    /* Initialize hue-dependent parameters */
    HueDependentGamutParams hdp;
    init_hue_dependent_params(h, &gcp, &hdp);

    /* Apply gamut compression */
    alwan_scalar J_out, M_out;
    compress_gamut_fwd(J, M, h, &gcp, &hdp, &J_out, &M_out);

    jmh_out->v[0] = J_out;
    jmh_out->v[1] = M_out;
    jmh_out->v[2] = h;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression Inverse (public API)
 * Reference: OCIO Transform.cpp gamut_compress_inv
 * ---------------------------------------------------------------- */

int alwan_aces_gamut_compress20_inv(alwan_vec3 *jmh_out,
                                     alwan_vec3 const *jmh_in,
                                     alwan_scalar peak_luminance,
                                     alwan_aces_primaries const *limit_primaries) {
    if (!jmh_out || !jmh_in || !limit_primaries) return ALWAN_E_INVALID;
    if (peak_luminance < ALWAN_LITERAL(1.0) || peak_luminance > ALWAN_LITERAL(10000.0)) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar J = jmh_in->v[0];
    alwan_scalar M = jmh_in->v[1];
    alwan_scalar h = jmh_in->v[2];

    /* Edge cases */
    if (J <= ALWAN_LITERAL(0.0)) {
        jmh_out->v[0] = ALWAN_LITERAL(0.0);
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return ALWAN_OK;
    }

    /* When limit primaries equal reach primaries (AP1), identity */
    if (primaries_are_ap1(limit_primaries)) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = M;
        jmh_out->v[2] = h;
        return ALWAN_OK;
    }

    /* Initialize JMh parameters for reach gamut (AP1) */
    alwan_aces_primaries reach_primaries;
    alwan_aces_primaries_ap1_default(&reach_primaries);
    JMhParams jmh_params;
    init_JMhParams(&reach_primaries, &jmh_params);

    /* Compute limit_J_max */
    alwan_scalar limit_J_max = Y_to_J(peak_luminance, &jmh_params);

    if (M <= ALWAN_LITERAL(0.0) || J > limit_J_max) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return ALWAN_OK;
    }

    /* Initialize gamut compression parameters */
    GamutCompressParams gcp;
    init_GamutCompressParams(peak_luminance, limit_primaries, &jmh_params, &gcp);

    /* Initialize hue-dependent parameters */
    HueDependentGamutParams hdp;
    init_hue_dependent_params(h, &gcp, &hdp);

    /* Apply inverse gamut compression */
    alwan_scalar J_out, M_out;
    compress_gamut_inv(J, M, h, &gcp, &hdp, &J_out, &M_out);

    jmh_out->v[0] = J_out;
    jmh_out->v[1] = M_out;
    jmh_out->v[2] = h;

    return ALWAN_OK;
}

/* ================================================================
 * ACES 2.0 Output Transform (Unified API)
 * ================================================================ */

/* Output transform preset configuration */
typedef struct {
    alwan_aces_primaries primaries;
    alwan_scalar peak_luminance;
    alwan_transfer_function eotf;
    int needs_d60_to_d65;  /* 1 if chromatic adaptation needed */
} aces2_output_config;

/* Standard primaries definitions */
static void primaries_rec709(alwan_aces_primaries *p) {
    p->red_x = ALWAN_LITERAL(0.64);   p->red_y = ALWAN_LITERAL(0.33);
    p->green_x = ALWAN_LITERAL(0.30); p->green_y = ALWAN_LITERAL(0.60);
    p->blue_x = ALWAN_LITERAL(0.15);  p->blue_y = ALWAN_LITERAL(0.06);
    p->white_x = ALWAN_D65_x; p->white_y = ALWAN_D65_y;
}

static void primaries_p3_d65(alwan_aces_primaries *p) {
    p->red_x = ALWAN_LITERAL(0.680);  p->red_y = ALWAN_LITERAL(0.320);
    p->green_x = ALWAN_LITERAL(0.265); p->green_y = ALWAN_LITERAL(0.690);
    p->blue_x = ALWAN_LITERAL(0.150);  p->blue_y = ALWAN_LITERAL(0.060);
    p->white_x = ALWAN_D65_x; p->white_y = ALWAN_D65_y;
}

static void primaries_rec2020(alwan_aces_primaries *p) {
    p->red_x = ALWAN_LITERAL(0.708);  p->red_y = ALWAN_LITERAL(0.292);
    p->green_x = ALWAN_LITERAL(0.170); p->green_y = ALWAN_LITERAL(0.797);
    p->blue_x = ALWAN_LITERAL(0.131);  p->blue_y = ALWAN_LITERAL(0.046);
    p->white_x = ALWAN_D65_x; p->white_y = ALWAN_D65_y;
}

static void primaries_p3_dci(alwan_aces_primaries *p) {
    p->red_x = ALWAN_LITERAL(0.680);  p->red_y = ALWAN_LITERAL(0.320);
    p->green_x = ALWAN_LITERAL(0.265); p->green_y = ALWAN_LITERAL(0.690);
    p->blue_x = ALWAN_LITERAL(0.150);  p->blue_y = ALWAN_LITERAL(0.060);
    /* DCI white point */
    p->white_x = ALWAN_LITERAL(0.314); p->white_y = ALWAN_LITERAL(0.351);
}

/* DCDM uses CIE XYZ primaries */
static void primaries_xyz(alwan_aces_primaries *p) {
    p->red_x = ALWAN_LITERAL(1.0);    p->red_y = ALWAN_LITERAL(0.0);
    p->green_x = ALWAN_LITERAL(0.0);  p->green_y = ALWAN_LITERAL(1.0);
    p->blue_x = ALWAN_LITERAL(0.0);   p->blue_y = ALWAN_LITERAL(0.0);
    /* Equal energy white for XYZ */
    p->white_x = ALWAN_LITERAL(0.3333); p->white_y = ALWAN_LITERAL(0.3333);
}

/* Get preset configuration */
static int get_output_config(alwan_aces2_output output, aces2_output_config *config) {
    config->needs_d60_to_d65 = 1;  /* Most outputs need D60->D65 */

    switch (output) {
        /* SDR Displays (100 nits) */
        case ALWAN_ACES2_OUT_REC709_100NIT_BT1886:
            primaries_rec709(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(100.0);
            config->eotf = ALWAN_TF_BT1886;
            break;

        case ALWAN_ACES2_OUT_SRGB_100NIT:
            primaries_rec709(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(100.0);
            config->eotf = ALWAN_TF_SRGB;
            break;

        case ALWAN_ACES2_OUT_P3D65_100NIT_SRGB:
            primaries_p3_d65(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(100.0);
            config->eotf = ALWAN_TF_SRGB;
            break;

        case ALWAN_ACES2_OUT_P3D65_100NIT_G22:
            primaries_p3_d65(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(100.0);
            config->eotf = ALWAN_TF_GAMMA22;
            break;

        /* HDR Displays (PQ) */
        case ALWAN_ACES2_OUT_P3D65_1000NIT_PQ:
            primaries_p3_d65(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(1000.0);
            config->eotf = ALWAN_TF_PQ;
            break;

        case ALWAN_ACES2_OUT_REC2100_500NIT_PQ:
            primaries_rec2020(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(500.0);
            config->eotf = ALWAN_TF_PQ;
            break;

        case ALWAN_ACES2_OUT_REC2100_1000NIT_PQ:
            primaries_rec2020(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(1000.0);
            config->eotf = ALWAN_TF_PQ;
            break;

        case ALWAN_ACES2_OUT_REC2100_2000NIT_PQ:
            primaries_rec2020(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(2000.0);
            config->eotf = ALWAN_TF_PQ;
            break;

        case ALWAN_ACES2_OUT_REC2100_4000NIT_PQ:
            primaries_rec2020(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(4000.0);
            config->eotf = ALWAN_TF_PQ;
            break;

        /* HDR Displays (HLG) */
        case ALWAN_ACES2_OUT_REC2100_1000NIT_HLG:
            primaries_rec2020(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(1000.0);
            config->eotf = ALWAN_TF_HLG;
            break;

        /* Cinema */
        case ALWAN_ACES2_OUT_DCDM_48NIT:
            primaries_xyz(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(48.0);
            config->eotf = ALWAN_TF_GAMMA26;
            config->needs_d60_to_d65 = 0;  /* DCDM doesn't need CAT */
            break;

        case ALWAN_ACES2_OUT_P3DCI_48NIT:
            primaries_p3_dci(&config->primaries);
            config->peak_luminance = ALWAN_LITERAL(48.0);
            config->eotf = ALWAN_TF_GAMMA26;
            config->needs_d60_to_d65 = 0;  /* DCI uses its own white point */
            break;

        default:
            return ALWAN_E_INVALID;
    }

    return ALWAN_OK;
}

/* D60 to D65 chromatic adaptation matrix (Bradford) */
static const alwan_scalar g_d60_to_d65_bradford[9] = {
    ALWAN_LITERAL( 0.98722400870301763), ALWAN_LITERAL(-0.00611322860685689), ALWAN_LITERAL( 0.01595328833591263),
    ALWAN_LITERAL(-0.00759837181166235), ALWAN_LITERAL( 1.00186148473965364), ALWAN_LITERAL( 0.00533003579138894),
    ALWAN_LITERAL( 0.00307257705853153), ALWAN_LITERAL(-0.00509596151113058), ALWAN_LITERAL( 1.08168060306579528)
};

/* D65 to D60 chromatic adaptation matrix (Bradford, inverse) */
static const alwan_scalar g_d65_to_d60_bradford[9] = {
    ALWAN_LITERAL( 1.01303000), ALWAN_LITERAL( 0.00610531), ALWAN_LITERAL(-0.01497100),
    ALWAN_LITERAL( 0.00769823), ALWAN_LITERAL( 0.99816500), ALWAN_LITERAL(-0.00503203),
    ALWAN_LITERAL(-0.00284131), ALWAN_LITERAL( 0.00468516), ALWAN_LITERAL( 0.92450700)
};

/* Apply 3x3 matrix to RGB */
static void apply_matrix_rgb(alwan_scalar const m[9], alwan_rgb const *in, alwan_rgb *out) {
    alwan_scalar r = in->r, g = in->g, b = in->b;
    out->r = m[0] * r + m[1] * g + m[2] * b;
    out->g = m[3] * r + m[4] * g + m[5] * b;
    out->b = m[6] * r + m[7] * g + m[8] * b;
}

/* Compute AP1 to limiting primaries matrix */
static void compute_ap1_to_limit_matrix(alwan_aces_primaries const *limit,
                                         alwan_scalar out[9]) {
    /* AP1 (ACEScg) primaries - ACES white point D60 */
    alwan_scalar ap1_to_xyz[9];
    primaries_to_rgb_to_xyz(
        AP1_RED_x,   AP1_RED_y,
        AP1_GREEN_x, AP1_GREEN_y,
        AP1_BLUE_x,  AP1_BLUE_y,
        AP1_WHITE_x, AP1_WHITE_y,
        ALWAN_LITERAL(1.0),
        ap1_to_xyz
    );

    /* Limit primaries to XYZ */
    alwan_scalar limit_to_xyz[9];
    primaries_to_rgb_to_xyz(
        limit->red_x, limit->red_y,
        limit->green_x, limit->green_y,
        limit->blue_x, limit->blue_y,
        limit->white_x, limit->white_y,
        ALWAN_LITERAL(1.0),
        limit_to_xyz
    );

    /* XYZ to limit primaries */
    alwan_scalar xyz_to_limit[9];
    invert_mat3(limit_to_xyz, xyz_to_limit);

    /* AP1 -> XYZ -> Limit = AP1 -> Limit */
    mult_mat3(xyz_to_limit, ap1_to_xyz, out);
}

/* Compute limit primaries to AP1 matrix (inverse) */
static void compute_limit_to_ap1_matrix(alwan_aces_primaries const *limit,
                                         alwan_scalar out[9]) {
    alwan_scalar ap1_to_limit[9];
    compute_ap1_to_limit_matrix(limit, ap1_to_limit);
    invert_mat3(ap1_to_limit, out);
}

/* ----------------------------------------------------------------
 * ACES 2.0 Output Transform Implementation
 * ---------------------------------------------------------------- */

int alwan_aces2_output_transform(alwan_rgb *rgb_out,
                                  alwan_rgb const *rgb_in,
                                  alwan_aces2_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES2_OUT_COUNT) return ALWAN_E_INVALID;

    /* Get preset configuration */
    aces2_output_config config;
    int status = get_output_config(output, &config);
    if (status != ALWAN_OK) return status;

    /* Special handling for cinema presets that can't use generic matrix calculation */
    if (output == ALWAN_ACES2_OUT_DCDM_48NIT) {
        /* DCDM: output directly to XYZ (D65) with gamma 2.6 encoding */

        /* Initialize AP1 primaries for JMh conversion */
        alwan_aces_primaries ap1;
        alwan_aces_primaries_ap1_default(&ap1);

        /* Use P3-D65 as the limiting primaries for gamut compression */
        alwan_aces_primaries p3_d65;
        primaries_p3_d65(&p3_d65);

        /* Step 1: Apply tonescale compression */
        alwan_rgb rgb_ts;
        status = alwan_aces_tonescale_compress20(&rgb_ts, rgb_in, config.peak_luminance);
        if (status != ALWAN_OK) return status;

        /* Step 2: Convert tonescale output to JMh for gamut compression */
        alwan_vec3 jmh_ts;
        status = alwan_aces_rgb_to_jmh20(&jmh_ts, &rgb_ts, &ap1);
        if (status != ALWAN_OK) return status;

        /* Step 3: Apply gamut compression using P3-D65 as limiting primaries */
        alwan_vec3 jmh_gc;
        status = alwan_aces_gamut_compress20(&jmh_gc, &jmh_ts, config.peak_luminance, &p3_d65);
        if (status != ALWAN_OK) return status;

        /* Step 4: Convert JMh back to RGB in AP1 space */
        alwan_rgb rgb_ap1;
        status = alwan_aces_jmh_to_rgb20(&rgb_ap1, &jmh_gc, &ap1);
        if (status != ALWAN_OK) return status;

        /* Step 5: Convert AP1 to XYZ
         * ACES 2.0 DCDM uses equal-energy white point (E), not D60 or D65.
         * The JMh conversion with XYZ "primaries" effectively outputs raw XYZ values
         * normalized to equal-energy white. Since we did gamut compress with P3-D65,
         * we need to convert AP1 to XYZ and normalize to preserve neutrality.
         *
         * For DCDM, the tonescale already maps luminance appropriately for 48 nits,
         * so we don't apply additional scaling. The output is simply X'Y'Z' with
         * gamma 2.6 encoding.
         */
        alwan_scalar xyz[3];
        xyz[0] = ACES1_AP1_TO_XYZ_D60[0] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60[1] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60[2] * rgb_ap1.b;
        xyz[1] = ACES1_AP1_TO_XYZ_D60[3] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60[4] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60[5] * rgb_ap1.b;
        xyz[2] = ACES1_AP1_TO_XYZ_D60[6] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60[7] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60[8] * rgb_ap1.b;

        /* Step 6: Normalize XYZ to equal-energy white for DCDM
         * D60 white point in XYZ is approximately (0.9526, 1.0, 1.0089)
         * We scale each component so that neutral colors have X=Y=Z */
        static const alwan_scalar D60_WHITE_X = ALWAN_LITERAL(0.952646074569846);
        static const alwan_scalar D60_WHITE_Z = ALWAN_LITERAL(1.008825184351586);
        xyz[0] /= D60_WHITE_X;
        xyz[2] /= D60_WHITE_Z;

        /* Step 7: Clamp negative values */
        alwan_scalar x_clamped = xyz[0] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : xyz[0];
        alwan_scalar y_clamped = xyz[1] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : xyz[1];
        alwan_scalar z_clamped = xyz[2] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : xyz[2];

        /* Step 8: Apply gamma 2.6 encoding */
        rgb_out->r = aces_gamma26_oetf_v(x_clamped);
        rgb_out->g = aces_gamma26_oetf_v(y_clamped);
        rgb_out->b = aces_gamma26_oetf_v(z_clamped);

        return ALWAN_OK;
    }

    if (output == ALWAN_ACES2_OUT_P3DCI_48NIT) {
        /* P3-DCI: P3 primaries with DCI white point and gamma 2.6 encoding */

        /* Initialize AP1 primaries for JMh conversion */
        alwan_aces_primaries ap1;
        alwan_aces_primaries_ap1_default(&ap1);

        /* Use P3-D65 as the limiting primaries for gamut compression
         * (P3-DCI has same primaries, just different white point) */
        alwan_aces_primaries p3_d65;
        primaries_p3_d65(&p3_d65);

        /* Step 1: Apply tonescale compression */
        alwan_rgb rgb_ts;
        status = alwan_aces_tonescale_compress20(&rgb_ts, rgb_in, config.peak_luminance);
        if (status != ALWAN_OK) return status;

        /* Step 2: Convert tonescale output to JMh for gamut compression */
        alwan_vec3 jmh_ts;
        status = alwan_aces_rgb_to_jmh20(&jmh_ts, &rgb_ts, &ap1);
        if (status != ALWAN_OK) return status;

        /* Step 3: Apply gamut compression using P3-D65 as limiting primaries */
        alwan_vec3 jmh_gc;
        status = alwan_aces_gamut_compress20(&jmh_gc, &jmh_ts, config.peak_luminance, &p3_d65);
        if (status != ALWAN_OK) return status;

        /* Step 4: Convert JMh back to RGB in AP1 space */
        alwan_rgb rgb_ap1;
        status = alwan_aces_jmh_to_rgb20(&rgb_ap1, &jmh_gc, &ap1);
        if (status != ALWAN_OK) return status;

        /* Step 5: Convert AP1 to XYZ (D60) */
        alwan_scalar xyz_d60[3];
        xyz_d60[0] = ACES1_AP1_TO_XYZ_D60[0] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60[1] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60[2] * rgb_ap1.b;
        xyz_d60[1] = ACES1_AP1_TO_XYZ_D60[3] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60[4] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60[5] * rgb_ap1.b;
        xyz_d60[2] = ACES1_AP1_TO_XYZ_D60[6] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60[7] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60[8] * rgb_ap1.b;

        /* Step 6: Convert XYZ (D60) to P3-DCI
         * P3-DCI uses DCI white point (0.314, 0.351)
         * For simplicity, we use the same conversion as ACES 1.x which applies D60->D65
         * then converts to P3-D65 (same primaries as P3-DCI) */
        alwan_scalar xyz_d65[3];
        xyz_d65[0] = ACES1_D60_TO_D65[0] * xyz_d60[0] + ACES1_D60_TO_D65[1] * xyz_d60[1] + ACES1_D60_TO_D65[2] * xyz_d60[2];
        xyz_d65[1] = ACES1_D60_TO_D65[3] * xyz_d60[0] + ACES1_D60_TO_D65[4] * xyz_d60[1] + ACES1_D60_TO_D65[5] * xyz_d60[2];
        xyz_d65[2] = ACES1_D60_TO_D65[6] * xyz_d60[0] + ACES1_D60_TO_D65[7] * xyz_d60[1] + ACES1_D60_TO_D65[8] * xyz_d60[2];

        /* XYZ (D65) to P3-D65 matrix (same primaries as P3-DCI) */
        static const alwan_scalar XYZ_D65_TO_P3[9] = {
            ALWAN_LITERAL( 2.4934969119), ALWAN_LITERAL(-0.9313836179), ALWAN_LITERAL(-0.4027107845),
            ALWAN_LITERAL(-0.8294889696), ALWAN_LITERAL( 1.7626640603), ALWAN_LITERAL( 0.0236246858),
            ALWAN_LITERAL( 0.0358458302), ALWAN_LITERAL(-0.0761723893), ALWAN_LITERAL( 0.9568845240)
        };

        alwan_scalar p3[3];
        p3[0] = XYZ_D65_TO_P3[0] * xyz_d65[0] + XYZ_D65_TO_P3[1] * xyz_d65[1] + XYZ_D65_TO_P3[2] * xyz_d65[2];
        p3[1] = XYZ_D65_TO_P3[3] * xyz_d65[0] + XYZ_D65_TO_P3[4] * xyz_d65[1] + XYZ_D65_TO_P3[5] * xyz_d65[2];
        p3[2] = XYZ_D65_TO_P3[6] * xyz_d65[0] + XYZ_D65_TO_P3[7] * xyz_d65[1] + XYZ_D65_TO_P3[8] * xyz_d65[2];

        /* Step 7: Clamp to [0, 1] for cinema */
        alwan_scalar r_clamped = p3[0] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (p3[0] > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : p3[0]);
        alwan_scalar g_clamped = p3[1] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (p3[1] > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : p3[1]);
        alwan_scalar b_clamped = p3[2] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (p3[2] > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : p3[2]);

        /* Step 8: Apply gamma 2.6 encoding */
        rgb_out->r = aces_gamma26_oetf_v(r_clamped);
        rgb_out->g = aces_gamma26_oetf_v(g_clamped);
        rgb_out->b = aces_gamma26_oetf_v(b_clamped);

        return ALWAN_OK;
    }

    return alwan_aces2_output_transform_custom(rgb_out,
                                                rgb_in,
                                                config.peak_luminance,
                                                &config.primaries,
                                                config.eotf);
}

int alwan_aces2_output_transform_custom(alwan_rgb *rgb_out,
                                         alwan_rgb const *rgb_in,
                                         alwan_scalar peak_luminance,
                                         alwan_aces_primaries const *limit_primaries,
                                         alwan_transfer_function eotf) {
    if (!rgb_out || !rgb_in || !limit_primaries) return ALWAN_E_INVALID;
    if (peak_luminance < ALWAN_LITERAL(1.0) || peak_luminance > ALWAN_LITERAL(10000.0)) {
        return ALWAN_E_INVALID;
    }

    int status;

    /* Initialize AP1 primaries for JMh conversion */
    alwan_aces_primaries ap1;
    alwan_aces_primaries_ap1_default(&ap1);

    /* Step 1: Convert input RGB (AP1) to JMh */
    alwan_vec3 jmh;
    status = alwan_aces_rgb_to_jmh20(&jmh, rgb_in, &ap1);
    if (status != ALWAN_OK) return status;

    /* Step 2: Apply tonescale compression
     * This includes both the tonescale and chroma compression */
    alwan_rgb rgb_ts;
    status = alwan_aces_tonescale_compress20(&rgb_ts, rgb_in, peak_luminance);
    if (status != ALWAN_OK) return status;

    /* Step 3: Convert tonescale output to JMh for gamut compression */
    alwan_vec3 jmh_ts;
    status = alwan_aces_rgb_to_jmh20(&jmh_ts, &rgb_ts, &ap1);
    if (status != ALWAN_OK) return status;

    /* Step 4: Apply gamut compression to limiting primaries */
    alwan_vec3 jmh_gc;
    status = alwan_aces_gamut_compress20(&jmh_gc, &jmh_ts, peak_luminance, limit_primaries);
    if (status != ALWAN_OK) return status;

    /* Step 5: Convert JMh back to RGB in AP1 space */
    alwan_rgb rgb_ap1;
    status = alwan_aces_jmh_to_rgb20(&rgb_ap1, &jmh_gc, &ap1);
    if (status != ALWAN_OK) return status;

    /* Step 6: Convert from AP1 to limiting primaries */
    alwan_scalar ap1_to_limit[9];
    compute_ap1_to_limit_matrix(limit_primaries, ap1_to_limit);

    alwan_rgb rgb_limit;
    apply_matrix_rgb(ap1_to_limit, &rgb_ap1, &rgb_limit);

    /* Step 7: Apply D60 to D65 chromatic adaptation if needed
     * (for D65-based output transforms like Rec.709, P3-D65, Rec.2020) */
    int needs_cat = (ALWAN_ABS(limit_primaries->white_x - ALWAN_D65_x) < ALWAN_LITERAL(0.01) &&
                     ALWAN_ABS(limit_primaries->white_y - ALWAN_D65_y) < ALWAN_LITERAL(0.01));

    alwan_rgb rgb_adapted;
    if (needs_cat) {
        /* Apply D60 to D65 Bradford matrix in XYZ space */
        /* First convert to XYZ */
        alwan_scalar limit_to_xyz[9];
        primaries_to_rgb_to_xyz(
            limit_primaries->red_x, limit_primaries->red_y,
            limit_primaries->green_x, limit_primaries->green_y,
            limit_primaries->blue_x, limit_primaries->blue_y,
            limit_primaries->white_x, limit_primaries->white_y,
            ALWAN_LITERAL(1.0),
            limit_to_xyz
        );

        alwan_scalar xyz[3];
        xyz[0] = limit_to_xyz[0] * rgb_limit.r + limit_to_xyz[1] * rgb_limit.g + limit_to_xyz[2] * rgb_limit.b;
        xyz[1] = limit_to_xyz[3] * rgb_limit.r + limit_to_xyz[4] * rgb_limit.g + limit_to_xyz[5] * rgb_limit.b;
        xyz[2] = limit_to_xyz[6] * rgb_limit.r + limit_to_xyz[7] * rgb_limit.g + limit_to_xyz[8] * rgb_limit.b;

        /* Apply D60 to D65 CAT */
        alwan_scalar xyz_d65[3];
        xyz_d65[0] = g_d60_to_d65_bradford[0] * xyz[0] + g_d60_to_d65_bradford[1] * xyz[1] + g_d60_to_d65_bradford[2] * xyz[2];
        xyz_d65[1] = g_d60_to_d65_bradford[3] * xyz[0] + g_d60_to_d65_bradford[4] * xyz[1] + g_d60_to_d65_bradford[5] * xyz[2];
        xyz_d65[2] = g_d60_to_d65_bradford[6] * xyz[0] + g_d60_to_d65_bradford[7] * xyz[1] + g_d60_to_d65_bradford[8] * xyz[2];

        /* Convert back to RGB */
        alwan_scalar xyz_to_limit[9];
        invert_mat3(limit_to_xyz, xyz_to_limit);

        rgb_adapted.r = xyz_to_limit[0] * xyz_d65[0] + xyz_to_limit[1] * xyz_d65[1] + xyz_to_limit[2] * xyz_d65[2];
        rgb_adapted.g = xyz_to_limit[3] * xyz_d65[0] + xyz_to_limit[4] * xyz_d65[1] + xyz_to_limit[5] * xyz_d65[2];
        rgb_adapted.b = xyz_to_limit[6] * xyz_d65[0] + xyz_to_limit[7] * xyz_d65[1] + xyz_to_limit[8] * xyz_d65[2];
    } else {
        rgb_adapted = rgb_limit;
    }

    /* Step 8: Clamp and scale for display encoding */
    alwan_rgb rgb_clamped;
    if (eotf == ALWAN_TF_PQ) {
        /* PQ: scale to absolute nits [0, peak_luminance], PQ OETF expects nits and normalizes by 10000 */
        rgb_clamped.r = rgb_adapted.r < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.r > ALWAN_LITERAL(1.0) ? peak_luminance : rgb_adapted.r * peak_luminance);
        rgb_clamped.g = rgb_adapted.g < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.g > ALWAN_LITERAL(1.0) ? peak_luminance : rgb_adapted.g * peak_luminance);
        rgb_clamped.b = rgb_adapted.b < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.b > ALWAN_LITERAL(1.0) ? peak_luminance : rgb_adapted.b * peak_luminance);
    } else if (eotf == ALWAN_TF_HLG) {
        /* HLG: clamp to [0, 1], HLG OETF expects normalized signal */
        rgb_clamped.r = rgb_adapted.r < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.r > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : rgb_adapted.r);
        rgb_clamped.g = rgb_adapted.g < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.g > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : rgb_adapted.g);
        rgb_clamped.b = rgb_adapted.b < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.b > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : rgb_adapted.b);
    } else {
        /* SDR: clamp to [0, 1] */
        rgb_clamped.r = rgb_adapted.r < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.r > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : rgb_adapted.r);
        rgb_clamped.g = rgb_adapted.g < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.g > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : rgb_adapted.g);
        rgb_clamped.b = rgb_adapted.b < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (rgb_adapted.b > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : rgb_adapted.b);
    }

    /* Step 9: Apply display encoding (OETF) */
    alwan_scalar linear[3] = {rgb_clamped.r, rgb_clamped.g, rgb_clamped.b};
    alwan_scalar encoded[3];

    status = alwan_oetf_apply(encoded, eotf, linear, 3, sizeof(alwan_scalar), sizeof(alwan_scalar));
    if (status != ALWAN_OK) {
        /* Fallback: return linear values if OETF not supported */
        rgb_out->r = rgb_clamped.r;
        rgb_out->g = rgb_clamped.g;
        rgb_out->b = rgb_clamped.b;
        return ALWAN_OK;
    }

    rgb_out->r = encoded[0];
    rgb_out->g = encoded[1];
    rgb_out->b = encoded[2];

    return ALWAN_OK;
}

int alwan_aces2_output_transform_inv(alwan_rgb *rgb_out,
                                      alwan_rgb const *rgb_in,
                                      alwan_aces2_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES2_OUT_COUNT) return ALWAN_E_INVALID;

    /* Get preset configuration */
    aces2_output_config config;
    int status = get_output_config(output, &config);
    if (status != ALWAN_OK) return status;

    /* Special handling for cinema presets */
    if (output == ALWAN_ACES2_OUT_DCDM_48NIT) {
        /* DCDM inverse: decode gamma 2.6, XYZ (equal-energy) to AP1 */

        /* Step 1: Decode gamma 2.6 */
        alwan_scalar xyz[3];
        xyz[0] = aces_gamma26_eotf_v(rgb_in->r);
        xyz[1] = aces_gamma26_eotf_v(rgb_in->g);
        xyz[2] = aces_gamma26_eotf_v(rgb_in->b);

        /* Step 2: De-normalize from equal-energy white to D60
         * This is the inverse of the forward transform's normalization */
        static const alwan_scalar D60_WHITE_X = ALWAN_LITERAL(0.952646074569846);
        static const alwan_scalar D60_WHITE_Z = ALWAN_LITERAL(1.008825184351586);
        xyz[0] *= D60_WHITE_X;
        xyz[2] *= D60_WHITE_Z;

        /* Step 3: Convert XYZ (D60) to AP1 */
        static const alwan_scalar XYZ_D60_TO_AP1[9] = {
            ALWAN_LITERAL( 1.6410233797), ALWAN_LITERAL(-0.3248032942), ALWAN_LITERAL(-0.2364246952),
            ALWAN_LITERAL(-0.6636628587), ALWAN_LITERAL( 1.6153315917), ALWAN_LITERAL( 0.0167563477),
            ALWAN_LITERAL( 0.0030476112), ALWAN_LITERAL(-0.0164295295), ALWAN_LITERAL( 0.9888322028)
        };

        alwan_rgb rgb_ap1;
        rgb_ap1.r = XYZ_D60_TO_AP1[0] * xyz[0] + XYZ_D60_TO_AP1[1] * xyz[1] + XYZ_D60_TO_AP1[2] * xyz[2];
        rgb_ap1.g = XYZ_D60_TO_AP1[3] * xyz[0] + XYZ_D60_TO_AP1[4] * xyz[1] + XYZ_D60_TO_AP1[5] * xyz[2];
        rgb_ap1.b = XYZ_D60_TO_AP1[6] * xyz[0] + XYZ_D60_TO_AP1[7] * xyz[1] + XYZ_D60_TO_AP1[8] * xyz[2];

        /* Step 5: Convert to JMh for inverse gamut compression */
        alwan_aces_primaries ap1;
        alwan_aces_primaries_ap1_default(&ap1);

        alwan_aces_primaries p3_d65;
        primaries_p3_d65(&p3_d65);

        alwan_vec3 jmh;
        status = alwan_aces_rgb_to_jmh20(&jmh, &rgb_ap1, &ap1);
        if (status != ALWAN_OK) return status;

        /* Step 6: Inverse gamut compression using P3-D65 */
        alwan_vec3 jmh_exp;
        status = alwan_aces_gamut_compress20_inv(&jmh_exp, &jmh, config.peak_luminance, &p3_d65);
        if (status != ALWAN_OK) return status;

        /* Step 7: Convert JMh back to AP1 RGB */
        status = alwan_aces_jmh_to_rgb20(rgb_out, &jmh_exp, &ap1);
        if (status != ALWAN_OK) return status;

        return ALWAN_OK;
    }

    if (output == ALWAN_ACES2_OUT_P3DCI_48NIT) {
        /* P3-DCI inverse: decode gamma 2.6, P3 to AP1 */

        /* Step 1: Decode gamma 2.6 */
        alwan_scalar p3_linear[3];
        p3_linear[0] = aces_gamma26_eotf_v(rgb_in->r);
        p3_linear[1] = aces_gamma26_eotf_v(rgb_in->g);
        p3_linear[2] = aces_gamma26_eotf_v(rgb_in->b);

        /* Step 2: P3 to XYZ (D65) */
        static const alwan_scalar P3_D65_TO_XYZ[9] = {
            ALWAN_LITERAL(0.4865709486), ALWAN_LITERAL(0.2656676932), ALWAN_LITERAL(0.1982172852),
            ALWAN_LITERAL(0.2289745641), ALWAN_LITERAL(0.6917385218), ALWAN_LITERAL(0.0792869141),
            ALWAN_LITERAL(0.0000000000), ALWAN_LITERAL(0.0451133819), ALWAN_LITERAL(1.0439443689)
        };

        alwan_scalar xyz_d65[3];
        xyz_d65[0] = P3_D65_TO_XYZ[0] * p3_linear[0] + P3_D65_TO_XYZ[1] * p3_linear[1] + P3_D65_TO_XYZ[2] * p3_linear[2];
        xyz_d65[1] = P3_D65_TO_XYZ[3] * p3_linear[0] + P3_D65_TO_XYZ[4] * p3_linear[1] + P3_D65_TO_XYZ[5] * p3_linear[2];
        xyz_d65[2] = P3_D65_TO_XYZ[6] * p3_linear[0] + P3_D65_TO_XYZ[7] * p3_linear[1] + P3_D65_TO_XYZ[8] * p3_linear[2];

        /* Step 3: Apply D65 to D60 chromatic adaptation */
        alwan_scalar xyz_d60[3];
        xyz_d60[0] = ACES1_D65_TO_D60[0] * xyz_d65[0] + ACES1_D65_TO_D60[1] * xyz_d65[1] + ACES1_D65_TO_D60[2] * xyz_d65[2];
        xyz_d60[1] = ACES1_D65_TO_D60[3] * xyz_d65[0] + ACES1_D65_TO_D60[4] * xyz_d65[1] + ACES1_D65_TO_D60[5] * xyz_d65[2];
        xyz_d60[2] = ACES1_D65_TO_D60[6] * xyz_d65[0] + ACES1_D65_TO_D60[7] * xyz_d65[1] + ACES1_D65_TO_D60[8] * xyz_d65[2];

        /* Step 4: Convert XYZ (D60) to AP1 */
        static const alwan_scalar XYZ_D60_TO_AP1[9] = {
            ALWAN_LITERAL( 1.6410233797), ALWAN_LITERAL(-0.3248032942), ALWAN_LITERAL(-0.2364246952),
            ALWAN_LITERAL(-0.6636628587), ALWAN_LITERAL( 1.6153315917), ALWAN_LITERAL( 0.0167563477),
            ALWAN_LITERAL( 0.0030476112), ALWAN_LITERAL(-0.0164295295), ALWAN_LITERAL( 0.9888322028)
        };

        alwan_rgb rgb_ap1;
        rgb_ap1.r = XYZ_D60_TO_AP1[0] * xyz_d60[0] + XYZ_D60_TO_AP1[1] * xyz_d60[1] + XYZ_D60_TO_AP1[2] * xyz_d60[2];
        rgb_ap1.g = XYZ_D60_TO_AP1[3] * xyz_d60[0] + XYZ_D60_TO_AP1[4] * xyz_d60[1] + XYZ_D60_TO_AP1[5] * xyz_d60[2];
        rgb_ap1.b = XYZ_D60_TO_AP1[6] * xyz_d60[0] + XYZ_D60_TO_AP1[7] * xyz_d60[1] + XYZ_D60_TO_AP1[8] * xyz_d60[2];

        /* Step 5: Convert to JMh for inverse gamut compression */
        alwan_aces_primaries ap1;
        alwan_aces_primaries_ap1_default(&ap1);

        alwan_aces_primaries p3_d65;
        primaries_p3_d65(&p3_d65);

        alwan_vec3 jmh;
        status = alwan_aces_rgb_to_jmh20(&jmh, &rgb_ap1, &ap1);
        if (status != ALWAN_OK) return status;

        /* Step 6: Inverse gamut compression using P3-D65 */
        alwan_vec3 jmh_exp;
        status = alwan_aces_gamut_compress20_inv(&jmh_exp, &jmh, config.peak_luminance, &p3_d65);
        if (status != ALWAN_OK) return status;

        /* Step 7: Convert JMh back to AP1 RGB */
        status = alwan_aces_jmh_to_rgb20(rgb_out, &jmh_exp, &ap1);
        if (status != ALWAN_OK) return status;

        return ALWAN_OK;
    }

    /* Step 1: Decode display encoding (EOTF) */
    alwan_scalar encoded[3] = {rgb_in->r, rgb_in->g, rgb_in->b};
    alwan_scalar linear[3];

    status = alwan_eotf_apply(linear, config.eotf, encoded, 3, sizeof(alwan_scalar), sizeof(alwan_scalar));
    if (status != ALWAN_OK) {
        /* Fallback: assume already linear */
        linear[0] = rgb_in->r;
        linear[1] = rgb_in->g;
        linear[2] = rgb_in->b;
    }

    alwan_rgb rgb_linear = {linear[0], linear[1], linear[2]};

    /* Step 2: Apply D65 to D60 chromatic adaptation if needed */
    int needs_cat = (ALWAN_ABS(config.primaries.white_x - ALWAN_D65_x) < ALWAN_LITERAL(0.01) &&
                     ALWAN_ABS(config.primaries.white_y - ALWAN_D65_y) < ALWAN_LITERAL(0.01));

    alwan_rgb rgb_d60;
    if (needs_cat) {
        /* Convert to XYZ */
        alwan_scalar limit_to_xyz[9];
        primaries_to_rgb_to_xyz(
            config.primaries.red_x, config.primaries.red_y,
            config.primaries.green_x, config.primaries.green_y,
            config.primaries.blue_x, config.primaries.blue_y,
            config.primaries.white_x, config.primaries.white_y,
            ALWAN_LITERAL(1.0),
            limit_to_xyz
        );

        alwan_scalar xyz[3];
        xyz[0] = limit_to_xyz[0] * rgb_linear.r + limit_to_xyz[1] * rgb_linear.g + limit_to_xyz[2] * rgb_linear.b;
        xyz[1] = limit_to_xyz[3] * rgb_linear.r + limit_to_xyz[4] * rgb_linear.g + limit_to_xyz[5] * rgb_linear.b;
        xyz[2] = limit_to_xyz[6] * rgb_linear.r + limit_to_xyz[7] * rgb_linear.g + limit_to_xyz[8] * rgb_linear.b;

        /* Apply D65 to D60 CAT */
        alwan_scalar xyz_d60[3];
        xyz_d60[0] = g_d65_to_d60_bradford[0] * xyz[0] + g_d65_to_d60_bradford[1] * xyz[1] + g_d65_to_d60_bradford[2] * xyz[2];
        xyz_d60[1] = g_d65_to_d60_bradford[3] * xyz[0] + g_d65_to_d60_bradford[4] * xyz[1] + g_d65_to_d60_bradford[5] * xyz[2];
        xyz_d60[2] = g_d65_to_d60_bradford[6] * xyz[0] + g_d65_to_d60_bradford[7] * xyz[1] + g_d65_to_d60_bradford[8] * xyz[2];

        /* Convert back to limit RGB */
        alwan_scalar xyz_to_limit[9];
        invert_mat3(limit_to_xyz, xyz_to_limit);

        rgb_d60.r = xyz_to_limit[0] * xyz_d60[0] + xyz_to_limit[1] * xyz_d60[1] + xyz_to_limit[2] * xyz_d60[2];
        rgb_d60.g = xyz_to_limit[3] * xyz_d60[0] + xyz_to_limit[4] * xyz_d60[1] + xyz_to_limit[5] * xyz_d60[2];
        rgb_d60.b = xyz_to_limit[6] * xyz_d60[0] + xyz_to_limit[7] * xyz_d60[1] + xyz_to_limit[8] * xyz_d60[2];
    } else {
        rgb_d60 = rgb_linear;
    }

    /* Step 3: Convert from limit primaries to AP1 */
    alwan_scalar limit_to_ap1[9];
    compute_limit_to_ap1_matrix(&config.primaries, limit_to_ap1);

    alwan_rgb rgb_ap1;
    apply_matrix_rgb(limit_to_ap1, &rgb_d60, &rgb_ap1);

    /* Step 4: Convert to JMh */
    alwan_aces_primaries ap1;
    alwan_aces_primaries_ap1_default(&ap1);

    alwan_vec3 jmh;
    status = alwan_aces_rgb_to_jmh20(&jmh, &rgb_ap1, &ap1);
    if (status != ALWAN_OK) return status;

    /* Step 5: Inverse gamut compression */
    alwan_vec3 jmh_exp;
    status = alwan_aces_gamut_compress20_inv(&jmh_exp, &jmh, config.peak_luminance, &config.primaries);
    if (status != ALWAN_OK) return status;

    /* Step 6: Convert JMh back to AP1 RGB
     * Note: Full inverse would require inverse tonescale, which is complex.
     * For now, we return the expanded JMh converted to RGB. */
    status = alwan_aces_jmh_to_rgb20(rgb_out, &jmh_exp, &ap1);
    if (status != ALWAN_OK) return status;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Blue Light Artifact Fix (Neon Suppression) LMT
 * Reference: ACES CLF (urn:ampas:aces:transformId:v1.5:LMT.Academy.BlueLightArtifactFix.a1.1.0)
 * ---------------------------------------------------------------- */

static alwan_scalar const BLUE_FIX_MATRIX[9] = {
    ALWAN_LITERAL(0.9404372683),  ALWAN_LITERAL(-0.0183068787), ALWAN_LITERAL(0.0778696104),
    ALWAN_LITERAL(0.0083786969),  ALWAN_LITERAL(0.8286599939),  ALWAN_LITERAL(0.1629613092),
    ALWAN_LITERAL(0.0005471261),  ALWAN_LITERAL(-0.0008833746), ALWAN_LITERAL(1.000336248)
};

static alwan_scalar const BLUE_FIX_MATRIX_INV[9] = {
    ALWAN_LITERAL(1.0631770724326068),  ALWAN_LITERAL(0.0233955757076458),  ALWAN_LITERAL(-0.0865726481835390),
    ALWAN_LITERAL(-0.0106337301402402), ALWAN_LITERAL(1.2063240280839118),  ALWAN_LITERAL(-0.1956902980415168),
    ALWAN_LITERAL(-0.0005908868078512), ALWAN_LITERAL(0.0010524817807910),  ALWAN_LITERAL(0.9995384055268296)
};

int alwan_aces_blue_light_fix(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    alwan_scalar r = rgb_in->r, g = rgb_in->g, b = rgb_in->b;
    rgb_out->r = BLUE_FIX_MATRIX[0] * r + BLUE_FIX_MATRIX[1] * g + BLUE_FIX_MATRIX[2] * b;
    rgb_out->g = BLUE_FIX_MATRIX[3] * r + BLUE_FIX_MATRIX[4] * g + BLUE_FIX_MATRIX[5] * b;
    rgb_out->b = BLUE_FIX_MATRIX[6] * r + BLUE_FIX_MATRIX[7] * g + BLUE_FIX_MATRIX[8] * b;
    return ALWAN_OK;
}

int alwan_aces_blue_light_fix_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    alwan_scalar r = rgb_in->r, g = rgb_in->g, b = rgb_in->b;
    rgb_out->r = BLUE_FIX_MATRIX_INV[0] * r + BLUE_FIX_MATRIX_INV[1] * g + BLUE_FIX_MATRIX_INV[2] * b;
    rgb_out->g = BLUE_FIX_MATRIX_INV[3] * r + BLUE_FIX_MATRIX_INV[4] * g + BLUE_FIX_MATRIX_INV[5] * b;
    rgb_out->b = BLUE_FIX_MATRIX_INV[6] * r + BLUE_FIX_MATRIX_INV[7] * g + BLUE_FIX_MATRIX_INV[8] * b;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Inverse Fixed Functions (for LMT emulation)
 * ---------------------------------------------------------------- */

/* Internal glow inverse implementation - analytical solution with iterative refinement.
 *
 * The OCIO piecewise formula provides an initial estimate, but since YC and
 * saturation depend on the scaled RGB values, we need iterative refinement
 * to achieve exact round-trip within floating-point precision.
 *
 * Forward: rgb_out = rgb_in * (1 + glowGainOut(YC, sat))
 * We solve for rgb_in given rgb_out using iterative refinement. */
static int glow_inv_impl(alwan_rgb const *rgb_in, alwan_scalar glow_gain_param,
                         alwan_scalar glow_mid_param, alwan_rgb *rgb_out) {
    alwan_scalar red_target = rgb_in->r;
    alwan_scalar grn_target = rgb_in->g;
    alwan_scalar blu_target = rgb_in->b;

    alwan_scalar GlowMid = glow_mid_param;

    /* Initial estimate using OCIO analytical formula */
    alwan_scalar YC = rgb_to_yc(red_target, grn_target, blu_target);
    alwan_scalar sat = calc_sat_weight(red_target, grn_target, blu_target, REDMOD_NOISE_LIMIT);
    alwan_scalar s = sigmoid_shaper(sat);
    alwan_scalar GlowGain = glow_gain_param * s;

    /* OCIO piecewise analytical formula for initial estimate */
    alwan_scalar glowGainOut;
    if (YC >= GlowMid * ALWAN_LITERAL(2.0)) {
        glowGainOut = ALWAN_LITERAL(0.0);
    } else if (YC <= (ALWAN_LITERAL(1.0) + GlowGain) * GlowMid * ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
        glowGainOut = -GlowGain / (ALWAN_LITERAL(1.0) + GlowGain);
    } else {
        glowGainOut = GlowGain * (GlowMid / YC - ALWAN_LITERAL(0.5)) / (GlowGain * ALWAN_LITERAL(0.5) - ALWAN_LITERAL(1.0));
    }

    alwan_scalar scale = ALWAN_LITERAL(1.0) + glowGainOut;
    alwan_scalar red = red_target * scale;
    alwan_scalar grn = grn_target * scale;
    alwan_scalar blu = blu_target * scale;

    /* Iterative refinement: apply forward and correct by dividing by scale.
     * This converges quickly because rgb_out = rgb_in * scale, so rgb_in = rgb_out / scale */
    for (int iter = 0; iter < 16; ++iter) {
        alwan_scalar YC_iter = rgb_to_yc(red, grn, blu);
        alwan_scalar sat_iter = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
        alwan_scalar s_iter = sigmoid_shaper(sat_iter);
        alwan_scalar glow_iter = glow_gain_param * s_iter;

        alwan_scalar glow_out_iter;
        if (YC_iter >= GlowMid * ALWAN_LITERAL(2.0)) {
            glow_out_iter = ALWAN_LITERAL(0.0);
        } else if (YC_iter <= GlowMid * ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
            glow_out_iter = glow_iter;
        } else {
            glow_out_iter = glow_iter * (GlowMid / YC_iter - ALWAN_LITERAL(0.5));
        }

        alwan_scalar fwd_scale = ALWAN_LITERAL(1.0) + glow_out_iter;
        alwan_scalar red_fwd = red * fwd_scale;
        alwan_scalar grn_fwd = grn * fwd_scale;
        alwan_scalar blu_fwd = blu * fwd_scale;

        alwan_scalar err_r = red_fwd - red_target;
        alwan_scalar err_g = grn_fwd - grn_target;
        alwan_scalar err_b = blu_fwd - blu_target;

        alwan_scalar max_err = ALWAN_ABS(err_r);
        if (ALWAN_ABS(err_g) > max_err) max_err = ALWAN_ABS(err_g);
        if (ALWAN_ABS(err_b) > max_err) max_err = ALWAN_ABS(err_b);
        if (max_err < ALWAN_LITERAL(1e-15)) break;

        /* Correct using forward scale factor */
        if (ALWAN_ABS(fwd_scale) > ALWAN_LITERAL(1e-10)) {
            red = red_target / fwd_scale;
            grn = grn_target / fwd_scale;
            blu = blu_target / fwd_scale;
        }
    }

    rgb_out->r = red;
    rgb_out->g = grn;
    rgb_out->b = blu;

    return ALWAN_OK;
}

int alwan_aces_glow03_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    return glow_inv_impl(rgb_in, GLOW03_GAIN, GLOW03_MID, rgb_out);
}

int alwan_aces_glow10_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    return glow_inv_impl(rgb_in, GLOW10_GAIN, GLOW10_MID, rgb_out);
}

/* RedMod10 inverse - analytical solution with iterative refinement.
 *
 * The quadratic formula (per OCIO) provides an initial estimate, but since f_H
 * depends on the red channel value (which changes in forward), using f_H computed
 * from the modified color introduces error. We refine with Newton iteration to
 * achieve exact round-trip within floating-point precision.
 *
 * Forward: r_out = r + f_H(r,g,b) * f_S(r,g,b) * (P - r) * k
 * We solve for r given r_out using iterative refinement. */
int alwan_aces_redmod10_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;

    alwan_scalar red_target = rgb_in->r;
    alwan_scalar grn = rgb_in->g;
    alwan_scalar blu = rgb_in->b;

    alwan_scalar f_H = calc_hue_weight(red_target, grn, blu, REDMOD10_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_scalar minChan = (grn < blu) ? grn : blu;
        alwan_scalar one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD10_SCALE;

        /* Initial estimate using quadratic formula (OCIO approach) */
        alwan_scalar a = f_H * one_minus_scale - ALWAN_LITERAL(1.0);
        alwan_scalar b = red_target - f_H * (REDMOD10_PIVOT + minChan) * one_minus_scale;
        alwan_scalar c = f_H * REDMOD10_PIVOT * minChan * one_minus_scale;
        alwan_scalar red = (-b - ALWAN_SQRT(b * b - ALWAN_LITERAL(4.0) * a * c)) / (ALWAN_LITERAL(2.0) * a);

        /* Iterative refinement using Newton's method.
         * Forward: r' = r + f_H * f_S * (P - r) * k = r * (1 - f_H * f_S * k) + f_H * f_S * P * k
         * Derivative: dr'/dr ≈ 1 - f_H * f_S * k (ignoring d(f_H)/dr and d(f_S)/dr)
         * Newton: r_new = r - (f(r) - target) / f'(r) */
        for (int iter = 0; iter < 16; ++iter) {
            alwan_scalar f_H_iter = calc_hue_weight(red, grn, blu, REDMOD10_INV_WIDTH);
            alwan_scalar f_S_iter = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
            alwan_scalar mod = f_H_iter * f_S_iter * one_minus_scale;
            alwan_scalar red_fwd = red + mod * (REDMOD10_PIVOT - red);
            alwan_scalar error = red_fwd - red_target;
            if (ALWAN_ABS(error) < ALWAN_LITERAL(1e-15)) break;
            /* Approximate derivative: 1 - f_H * f_S * k */
            alwan_scalar deriv = ALWAN_LITERAL(1.0) - mod;
            if (ALWAN_ABS(deriv) > ALWAN_LITERAL(1e-10)) {
                red -= error / deriv;
            } else {
                red -= error;
            }
        }

        rgb_out->r = red;
        rgb_out->g = grn;
        rgb_out->b = blu;
    } else {
        rgb_out->r = red_target;
        rgb_out->g = grn;
        rgb_out->b = blu;
    }

    return ALWAN_OK;
}

/* RedMod03 inverse - analytical solution with iterative refinement.
 *
 * Like RedMod10, the quadratic formula provides an initial estimate but
 * f_H depends on the modified color. Additionally, RedMod03 preserves hue
 * by modifying green or blue, so we need to track both channels.
 *
 * Forward: r_out = r + f_H(r,g,b) * f_S(r,g,b) * (P - r) * k
 *          grn_out = hue_fac * (r_out - blu) + blu  (if grn >= blu)
 *          blu_out = hue_fac * (r_out - grn) + grn  (if blu > grn)
 */
int alwan_aces_redmod03_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;

    alwan_scalar red_target = rgb_in->r;
    alwan_scalar grn_target = rgb_in->g;
    alwan_scalar blu_target = rgb_in->b;

    alwan_scalar f_H = calc_hue_weight(red_target, grn_target, blu_target, REDMOD03_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_scalar one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD03_SCALE;

        /* Determine which channel was modified (grn or blu) and compute hue factor */
        int grn_modified = (grn_target >= blu_target);
        alwan_scalar hue_fac;
        if (grn_modified) {
            hue_fac = (grn_target - blu_target) / alwan_max(ALWAN_LITERAL(1e-10), red_target - blu_target);
        } else {
            hue_fac = (blu_target - grn_target) / alwan_max(ALWAN_LITERAL(1e-10), red_target - grn_target);
        }

        /* Initial estimate using quadratic formula */
        alwan_scalar minChan = grn_modified ? blu_target : grn_target;
        alwan_scalar a = f_H * one_minus_scale - ALWAN_LITERAL(1.0);
        alwan_scalar b = red_target - f_H * (REDMOD03_PIVOT + minChan) * one_minus_scale;
        alwan_scalar c = f_H * REDMOD03_PIVOT * minChan * one_minus_scale;
        alwan_scalar red = (-b - ALWAN_SQRT(b * b - ALWAN_LITERAL(4.0) * a * c)) / (ALWAN_LITERAL(2.0) * a);

        /* Compute initial grn/blu from hue restoration */
        alwan_scalar grn, blu;
        if (grn_modified) {
            blu = blu_target;
            grn = hue_fac * (red - blu) + blu;
        } else {
            grn = grn_target;
            blu = hue_fac * (red - grn) + grn;
        }

        /* Iterative refinement using Newton's method */
        for (int iter = 0; iter < 16; ++iter) {
            alwan_scalar f_H_iter = calc_hue_weight(red, grn, blu, REDMOD03_INV_WIDTH);
            alwan_scalar f_S_iter = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
            alwan_scalar mod = f_H_iter * f_S_iter * one_minus_scale;
            alwan_scalar red_fwd = red + mod * (REDMOD03_PIVOT - red);
            alwan_scalar error = red_fwd - red_target;
            if (ALWAN_ABS(error) < ALWAN_LITERAL(1e-15)) break;
            /* Approximate derivative: 1 - f_H * f_S * k */
            alwan_scalar deriv = ALWAN_LITERAL(1.0) - mod;
            if (ALWAN_ABS(deriv) > ALWAN_LITERAL(1e-10)) {
                red -= error / deriv;
            } else {
                red -= error;
            }
            /* Update grn or blu based on new red estimate */
            if (grn_modified) {
                grn = hue_fac * (red - blu) + blu;
            } else {
                blu = hue_fac * (red - grn) + grn;
            }
        }

        rgb_out->r = red;
        rgb_out->g = grn;
        rgb_out->b = blu;
    } else {
        rgb_out->r = red_target;
        rgb_out->g = grn_target;
        rgb_out->b = blu_target;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * ACES 1.0 Look LMT
 * Emulates ACES 1.0 look when used with ACES 1.0.3+ RRT
 * Applies: Glow10_inv -> Glow03 -> RedMod10_inv -> RedMod03
 * ---------------------------------------------------------------- */

int alwan_aces_look_1_0(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;

    alwan_rgb temp1, temp2, temp3;

    /* Step 1: Undo Glow10 */
    int status = alwan_aces_glow10_inv(&temp1, rgb_in);
    if (status != ALWAN_OK) return status;

    /* Step 2: Apply Glow03 */
    status = alwan_aces_glow03(&temp2, &temp1);
    if (status != ALWAN_OK) return status;

    /* Step 3: Undo RedMod10 */
    status = alwan_aces_redmod10_inv(&temp3, &temp2);
    if (status != ALWAN_OK) return status;

    /* Step 4: Apply RedMod03 */
    status = alwan_aces_redmod03(rgb_out, &temp3);
    if (status != ALWAN_OK) return status;

    return ALWAN_OK;
}

/* Inverse of ACES 1.0 Look LMT */
int alwan_aces_look_1_0_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;

    alwan_rgb temp1, temp2, temp3;

    /* Reverse order: RedMod03_inv -> RedMod10 -> Glow03_inv -> Glow10 */

    /* Step 1: Undo RedMod03 */
    int status = alwan_aces_redmod03_inv(&temp1, rgb_in);
    if (status != ALWAN_OK) return status;

    /* Step 2: Apply RedMod10 */
    status = alwan_aces_redmod10(&temp2, &temp1);
    if (status != ALWAN_OK) return status;

    /* Step 3: Undo Glow03 */
    status = alwan_aces_glow03_inv(&temp3, &temp2);
    if (status != ALWAN_OK) return status;

    /* Step 4: Apply Glow10 */
    status = alwan_aces_glow10(rgb_out, &temp3);
    if (status != ALWAN_OK) return status;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Parametric LMT (CDL-style color grading)
 * ---------------------------------------------------------------- */

void alwan_aces_lmt_params_init(alwan_aces_lmt_params *params) {
    if (!params) return;

    /* Initialize to neutral/identity values */
    params->slope[0] = params->slope[1] = params->slope[2] = ALWAN_LITERAL(1.0);
    params->offset[0] = params->offset[1] = params->offset[2] = ALWAN_LITERAL(0.0);
    params->power[0] = params->power[1] = params->power[2] = ALWAN_LITERAL(1.0);
    params->saturation = ALWAN_LITERAL(1.0);
}

int alwan_aces_lmt_apply(alwan_rgb *rgb_out,
                         alwan_rgb const *rgb_in,
                         alwan_aces_lmt_params const *params) {
    if (!rgb_out || !rgb_in || !params) return ALWAN_E_INVALID;

    alwan_vec3 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3 slope_v = {{params->slope[0], params->slope[1], params->slope[2]}};
    alwan_vec3 offset_v = {{params->offset[0], params->offset[1], params->offset[2]}};
    alwan_vec3 power_v = {{params->power[0], params->power[1], params->power[2]}};
    alwan_vec3 out_v = aces_lmt_apply_v(in_v, slope_v, offset_v, power_v, params->saturation);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];

    return ALWAN_OK;
}
