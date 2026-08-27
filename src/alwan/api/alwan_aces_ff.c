/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ACES Fixed Functions (RRT Components)
 * Per-pixel math in alwan_aces_ff_core.h
 *
 * Only init functions, table generation, enum dispatch, and public API live here.
 * Reference: OpenColorIO FixedFunctionOpCPU.cpp
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>  /* memcmp for the gamut-params cache */
#include "../core/alwan_aces_ff_core.h"

/* ----------------------------------------------------------------
 * ACES interpolation mode (global, thread-unsafe for simplicity)
 * ---------------------------------------------------------------- */
static alwan_aces_interp g_aces_interp = ALWAN_ACES_INTERP_BSPLINE;

void alwan_set_aces_interp(alwan_aces_interp method) { g_aces_interp = method; }
alwan_aces_interp alwan_get_aces_interp(void) { return g_aces_interp; }

/* ----------------------------------------------------------------
 * OCIO-matching piecewise quadratic Hermite tone curves
 * Control points and slopes extracted from OCIO builtin
 * ACES-OUTPUT ACES2065-1_to_CIE-XYZ-D65 SDR-VIDEO_1.0
 * ---------------------------------------------------------------- */

/* Evaluate piecewise quadratic Hermite: for each interval [x_i, x_{i+1}]
 * with slopes s_i, s_{i+1}, use simple quadratic A*t^2 + B*t + C
 * where t = x - x_i. Linear extrapolation outside range. */
static alwan_f64 hermite_quad_eval(alwan_f64 x,
                                    alwan_f64 const *px, alwan_f64 const *py,
                                    alwan_f64 const *ps, int n) {
    if (x <= px[0])
        return py[0] + ps[0] * (x - px[0]);
    if (x >= px[n-1]) {
        /* Linear extrapolation using last segment's end slope */
        alwan_f64 dx = px[n-1] - px[n-2];
        alwan_f64 A = (ps[n-1] - ps[n-2]) / (2.0 * dx);
        alwan_f64 end_slope = 2.0 * A * dx + ps[n-2];
        return py[n-1] + end_slope * (x - px[n-1]);
    }
    /* Find interval */
    int i = 0;
    while (i < n - 2 && x >= px[i + 1]) i++;
    alwan_f64 dx = px[i+1] - px[i];
    alwan_f64 t = x - px[i];
    /* Simple quadratic: f(t) = A*t^2 + B*t + C */
    alwan_f64 A = (ps[i+1] - ps[i]) / (2.0 * dx);
    alwan_f64 B = ps[i];
    alwan_f64 C = py[i];
    return (A * t + B) * t + C;
}

/* RRT tone curve (C5 equivalent) -- 7 control points from OCIO */
static alwan_f64 aces1_rrt_hermite(alwan_f64 x) {
    static const alwan_f64 px[7] = {
        -5.26017743, -3.75502745, -2.24987747,
        -0.74472749,  1.06145248,  2.86763245, 4.67381243 };
    static const alwan_f64 py[7] = {
        -4.0, -3.57868829, -1.82131329,
         0.68124124,  2.87457742,  3.83406206, 4.0 };
    static const alwan_f64 ps[7] = {
         0.0,  0.55982688,  1.77532247,
         1.55, 0.8787017,   0.18374463, 0.0 };
    alwan_f64 lx = ALWAN_LOG10_F64(fmax(x, 1e-10));
    alwan_f64 ly = hermite_quad_eval(lx, px, py, ps, 7);
    return ALWAN_POW_F64(10.0, ly);
}

/* SDR ODT tone curve (C9 equivalent, 48 nit) -- 15 control points from OCIO */
static alwan_f64 aces1_odt48_hermite(alwan_f64 x) {
    static const alwan_f64 px[15] = {
        -2.54062362, -2.08035721, -1.6200908,  -1.15982439, -0.69955799,
        -0.23929158,  0.22097483,  0.68124124,  1.01284632,  1.3444514,
         1.67605648,  2.00766156,  2.33926665,  2.67087173,  3.00247681 };
    static const alwan_f64 py[15] = {
        -1.69897,    -1.588435,   -1.3535,     -1.04695,    -0.6564,
        -0.22141,     0.22814402,  0.68124124,  0.99142189,  1.258,
         1.44995,     1.5591,      1.6226,      1.66065457,  1.68124124 };
    static const alwan_f64 ps[15] = {
         0.0,         0.4803088,   0.5405565,   0.79149813,  0.9055625,
         0.98460368,  0.96884766,  1.0,         0.87078346,  0.73702127,
         0.42068113,  0.23763206,  0.14535362,  0.08416378,  0.04 };
    static const alwan_f64 cinema_white = 48.0, cinema_black = 0.02;
    alwan_f64 lx = ALWAN_LOG10_F64(fmax(x, 1e-10));
    alwan_f64 ly = hermite_quad_eval(lx, px, py, ps, 15);
    alwan_f64 nits = ALWAN_POW_F64(10.0, ly);
    return (nits - cinema_black) / (cinema_white - cinema_black);
}

/* ----------------------------------------------------------------
 * Internal helper functions
 * ---------------------------------------------------------------- */

static alwan_f64 min3(alwan_f64 a, alwan_f64 b, alwan_f64 c) {
    return alwan_min3(a, b, c);
}

static alwan_f64 max3(alwan_f64 a, alwan_f64 b, alwan_f64 c) {
    return alwan_max3(a, b, c);
}

/* ----------------------------------------------------------------
 * RedMod helper functions (from OCIO)
 * ---------------------------------------------------------------- */

/* Saturation weight calculation */
static alwan_f64 calc_sat_weight(alwan_f64 red, alwan_f64 grn, alwan_f64 blu,
                                     alwan_f64 noise_limit) {
    return aces_calc_sat_weight_f64_v(red, grn, blu, noise_limit);
}

/* Hue weight calculation using B-spline */
static alwan_f64 calc_hue_weight(alwan_f64 red, alwan_f64 grn, alwan_f64 blu,
                                     alwan_f64 inv_width) {
    return aces_calc_hue_weight_f64_v(red, grn, blu, inv_width);
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

void alwan_aces_redmod03_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    alwan_f64 red = rgb_in->r;
    alwan_f64 grn = rgb_in->g;
    alwan_f64 blu = rgb_in->b;

    alwan_f64 f_H = calc_hue_weight(red, grn, blu, REDMOD03_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_f64 f_S = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
        alwan_f64 one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD03_SCALE;
        alwan_f64 new_red = red + f_H * f_S * (REDMOD03_PIVOT - red) * one_minus_scale;

        /* Preserve hue by adjusting green or blue.
         * Only adjust if the change to red is significant (> 1e-5) */
        alwan_f64 delta_red = new_red - red;
        if (ALWAN_ABS(delta_red) > ALWAN_LITERAL(1e-5)) {
            if (grn >= blu) {
                alwan_f64 denom = red - blu;
                if (denom > ALWAN_LITERAL(1e-10)) {
                    alwan_f64 hue_fac = (grn - blu) / denom;
                    grn = hue_fac * (new_red - blu) + blu;
                }
            } else {
                alwan_f64 denom = red - grn;
                if (denom > ALWAN_LITERAL(1e-10)) {
                    alwan_f64 hue_fac = (blu - grn) / denom;
                    blu = hue_fac * (new_red - grn) + grn;
                }
            }
        }

        red = new_red;
    }

    rgb_out->r = red;
    rgb_out->g = grn;
    rgb_out->b = blu;
}

void alwan_aces_redmod03_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    float red = rgb_in->r;
    float grn = rgb_in->g;
    float blu = rgb_in->b;

    float f_H = aces_calc_hue_weight_f32_v(red, grn, blu, (float)REDMOD03_INV_WIDTH);

    if (f_H > 0.0f) {
        float f_S = aces_calc_sat_weight_f32_v(red, grn, blu, (float)REDMOD_NOISE_LIMIT);
        float one_minus_scale = 1.0f - (float)REDMOD03_SCALE;
        float new_red = red + f_H * f_S * ((float)REDMOD03_PIVOT - red) * one_minus_scale;

        /* Preserve hue by adjusting green or blue.
         * Only adjust if the change to red is significant (> 1e-5) */
        float delta_red = new_red - red;
        if (ALWAN_ABS_F32(delta_red) > 1e-5f) {
            if (grn >= blu) {
                float denom = red - blu;
                if (denom > 1e-10f) {
                    float hue_fac = (grn - blu) / denom;
                    grn = hue_fac * (new_red - blu) + blu;
                }
            } else {
                float denom = red - grn;
                if (denom > 1e-10f) {
                    float hue_fac = (blu - grn) / denom;
                    blu = hue_fac * (new_red - grn) + grn;
                }
            }
        }

        red = new_red;
    }

    rgb_out->r = red;
    rgb_out->g = grn;
    rgb_out->b = blu;
}

/* ----------------------------------------------------------------
 * ACES RedMod10 - Red channel modification (RRT v1.0)
 * ---------------------------------------------------------------- */

#define REDMOD10_SCALE      ALWAN_LITERAL(0.82)
#define REDMOD10_PIVOT      ALWAN_LITERAL(0.03)
#define REDMOD10_INV_WIDTH  ALWAN_LITERAL(1.6976527263135504)

void alwan_aces_redmod10_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    alwan_f64 red = rgb_in->r;
    alwan_f64 grn = rgb_in->g;
    alwan_f64 blu = rgb_in->b;

    alwan_f64 f_H = calc_hue_weight(red, grn, blu, REDMOD10_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_f64 f_S = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
        alwan_f64 one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD10_SCALE;
        /* RedMod10 only modifies red - no hue preservation (unlike RedMod03) */
        red = red + f_H * f_S * (REDMOD10_PIVOT - red) * one_minus_scale;
    }

    rgb_out->r = red;
    rgb_out->g = grn;
    rgb_out->b = blu;
}

void alwan_aces_redmod10_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    float red = rgb_in->r;
    float grn = rgb_in->g;
    float blu = rgb_in->b;

    float f_H = aces_calc_hue_weight_f32_v(red, grn, blu, (float)REDMOD10_INV_WIDTH);

    if (f_H > 0.0f) {
        float f_S = aces_calc_sat_weight_f32_v(red, grn, blu, (float)REDMOD_NOISE_LIMIT);
        float one_minus_scale = 1.0f - (float)REDMOD10_SCALE;
        /* RedMod10 only modifies red - no hue preservation (unlike RedMod03) */
        red = red + f_H * f_S * ((float)REDMOD10_PIVOT - red) * one_minus_scale;
    }

    rgb_out->r = red;
    rgb_out->g = grn;
    rgb_out->b = blu;
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
static alwan_f64 rgb_to_yc(alwan_f64 red, alwan_f64 grn, alwan_f64 blu) {
    return aces_rgb_to_yc_f64_v(red, grn, blu);
}

/* Sigmoid shaper for saturation.
 * ACES CTL: sigmoid_shaper((sat - 0.4) / 0.2) -- pre-scales input here. */
static alwan_f64 sigmoid_shaper(alwan_f64 sat) {
    return aces_sigmoid_shaper_f64_v((sat - ALWAN_LITERAL(0.4)) / ALWAN_LITERAL(0.2));
}

/* Internal glow implementation with configurable parameters */
static int glow_impl(alwan_rgb_f64 const *rgb_in, alwan_f64 glow_gain_param,
                     alwan_f64 glow_mid_param, alwan_rgb_f64 *rgb_out) {
    alwan_f64 red = rgb_in->r;
    alwan_f64 grn = rgb_in->g;
    alwan_f64 blu = rgb_in->b;

    alwan_f64 YC = rgb_to_yc(red, grn, blu);
    alwan_f64 sat = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
    alwan_f64 s = sigmoid_shaper(sat);

    alwan_f64 glow_gain = glow_gain_param * s;
    alwan_f64 glow_mid = glow_mid_param;

    alwan_f64 glow_gain_out;
    if (YC >= glow_mid * ALWAN_LITERAL(2.0)) {
        glow_gain_out = ALWAN_LITERAL(0.0);
    } else if (YC <= glow_mid * ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
        glow_gain_out = glow_gain;
    } else {
        glow_gain_out = glow_gain * (glow_mid / YC - ALWAN_LITERAL(0.5));
    }

    alwan_f64 added_glow = ALWAN_LITERAL(1.0) + glow_gain_out;

    rgb_out->r = red * added_glow;
    rgb_out->g = grn * added_glow;
    rgb_out->b = blu * added_glow;

    return ALWAN_OK;
}

void alwan_aces_glow03_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    glow_impl(rgb_in, GLOW03_GAIN, GLOW03_MID, rgb_out);
}

/* Native f32 glow implementation, mirrors glow_impl exactly in single precision. */
static int glow_impl_f32(alwan_rgb_f32 const *rgb_in, float glow_gain_param,
                         float glow_mid_param, alwan_rgb_f32 *rgb_out) {
    float red = rgb_in->r;
    float grn = rgb_in->g;
    float blu = rgb_in->b;

    float YC = aces_rgb_to_yc_f32_v(red, grn, blu);
    float sat = aces_calc_sat_weight_f32_v(red, grn, blu, (float)REDMOD_NOISE_LIMIT);
    float s = aces_sigmoid_shaper_f32_v((sat - 0.4f) / 0.2f);

    float glow_gain = glow_gain_param * s;
    float glow_mid = glow_mid_param;

    float glow_gain_out;
    if (YC >= glow_mid * 2.0f) {
        glow_gain_out = 0.0f;
    } else if (YC <= glow_mid * 2.0f / 3.0f) {
        glow_gain_out = glow_gain;
    } else {
        glow_gain_out = glow_gain * (glow_mid / YC - 0.5f);
    }

    float added_glow = 1.0f + glow_gain_out;

    rgb_out->r = red * added_glow;
    rgb_out->g = grn * added_glow;
    rgb_out->b = blu * added_glow;

    return ALWAN_OK;
}

void alwan_aces_glow03_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    glow_impl_f32(rgb_in, (float)GLOW03_GAIN, (float)GLOW03_MID, rgb_out);
}

void alwan_aces_glow10_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    glow_impl(rgb_in, GLOW10_GAIN, GLOW10_MID, rgb_out);
}

void alwan_aces_glow10_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    glow_impl_f32(rgb_in, (float)GLOW10_GAIN, (float)GLOW10_MID, rgb_out);
}

/* ----------------------------------------------------------------
 * ACES DarkToDim10 - Surround compensation
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_ACES_DarkToDim10_Fwd
 * ---------------------------------------------------------------- */

void alwan_aces_dark_to_dim10_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_vec3_f64 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3_f64 out_v = aces_dark_to_dim10_f64_v(in_v);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
}

void alwan_aces_dark_to_dim10_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_vec3_f32 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3_f32 out_v = aces_dark_to_dim10_f32_v(in_v);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
}

/* ----------------------------------------------------------------
 * Rec.2100 Surround adjustment
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_REC2100_Surround
 * ---------------------------------------------------------------- */

void alwan_rec2100_surround_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_f64 gamma) {
    if (!rgb_out || !rgb_in) return;
    alwan_vec3_f64 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3_f64 out_v = aces_rec2100_surround_f64_v(in_v, gamma);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
}

void alwan_rec2100_surround_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_f32 gamma) {
    if (!rgb_out || !rgb_in) return;
    alwan_vec3_f32 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3_f32 out_v = aces_rec2100_surround_f32_v(in_v, gamma);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
}

/* ----------------------------------------------------------------
 * ACES GamutComp13 - Gamut compression (ACES 1.3)
 * Reference: OCIO FixedFunctionOpCPU.cpp - Renderer_ACES_GamutComp13_Fwd
 * ---------------------------------------------------------------- */

void alwan_aces_gamut_comp13_params_default_f32(alwan_aces_gamut_comp13_params_f32 *params) {
    if (!params) return;
    params->lim_cyan = 1.147f;
    params->lim_magenta = 1.264f;
    params->lim_yellow = 1.312f;
    params->thr_cyan = 0.815f;
    params->thr_magenta = 0.803f;
    params->thr_yellow = 0.880f;
    params->power = 1.2f;
}

void alwan_aces_gamut_comp13_params_default_f64(alwan_aces_gamut_comp13_params_f64 *params) {
    if (!params) return;
    params->lim_cyan = 1.147;
    params->lim_magenta = 1.264;
    params->lim_yellow = 1.312;
    params->thr_cyan = 0.815;
    params->thr_magenta = 0.803;
    params->thr_yellow = 0.880;
    params->power = 1.2;
}

/* Compression function (forward direction) */
static alwan_f64 compress_dist(alwan_f64 dist, alwan_f64 thr, alwan_f64 scale, alwan_f64 power) {
    return aces_compress_dist_f64_v(dist, thr, scale, power);
}

/* Per-channel gamut compression */
static alwan_f64 gamut_comp_channel(alwan_f64 val, alwan_f64 ach,
                                        alwan_f64 thr, alwan_f64 scale, alwan_f64 power) {
    return aces_gamut_comp_channel_f64_v(val, ach, thr, scale, power);
}

/* Compute scale from limit, threshold and power.
 * The scale ensures the compression function passes through (1, lim). */
static alwan_f64 calc_gamut_comp_scale(alwan_f64 lim, alwan_f64 thr, alwan_f64 power) {
    return aces_calc_gamut_comp_scale_f64_v(lim, thr, power);
}

void alwan_aces_gamut_comp13_f64(alwan_rgb_f64 *rgb_out,
                            alwan_rgb_f64 const *rgb_in,
                            alwan_aces_gamut_comp13_params_f64 const *params) {
    if (!rgb_out || !rgb_in || !params) return;

    alwan_f64 red = rgb_in->r;
    alwan_f64 grn = rgb_in->g;
    alwan_f64 blu = rgb_in->b;

    /* Compute scales from limits and thresholds using OCIO formula */
    alwan_f64 scale_cyan = calc_gamut_comp_scale(params->lim_cyan, params->thr_cyan, params->power);
    alwan_f64 scale_magenta = calc_gamut_comp_scale(params->lim_magenta, params->thr_magenta, params->power);
    alwan_f64 scale_yellow = calc_gamut_comp_scale(params->lim_yellow, params->thr_yellow, params->power);

    /* Achromatic = max(RGB) */
    alwan_f64 ach = max3(red, grn, blu);

    rgb_out->r = gamut_comp_channel(red, ach, params->thr_cyan, scale_cyan, params->power);
    rgb_out->g = gamut_comp_channel(grn, ach, params->thr_magenta, scale_magenta, params->power);
    rgb_out->b = gamut_comp_channel(blu, ach, params->thr_yellow, scale_yellow, params->power);
}

void alwan_aces_gamut_comp13_f32(alwan_rgb_f32 *rgb_out,
                            alwan_rgb_f32 const *rgb_in,
                            alwan_aces_gamut_comp13_params_f32 const *params) {
    if (!rgb_out || !rgb_in || !params) return;

    alwan_f32 red = rgb_in->r;
    alwan_f32 grn = rgb_in->g;
    alwan_f32 blu = rgb_in->b;

    /* Compute scales from limits and thresholds using OCIO formula */
    alwan_f32 scale_cyan = aces_calc_gamut_comp_scale_f32_v(params->lim_cyan, params->thr_cyan, params->power);
    alwan_f32 scale_magenta = aces_calc_gamut_comp_scale_f32_v(params->lim_magenta, params->thr_magenta, params->power);
    alwan_f32 scale_yellow = aces_calc_gamut_comp_scale_f32_v(params->lim_yellow, params->thr_yellow, params->power);

    /* Achromatic = max(RGB) */
    alwan_f32 ach = alwan_max3_f32(red, grn, blu);

    rgb_out->r = aces_gamut_comp_channel_f32_v(red, ach, params->thr_cyan, scale_cyan, params->power);
    rgb_out->g = aces_gamut_comp_channel_f32_v(grn, ach, params->thr_magenta, scale_magenta, params->power);
    rgb_out->b = aces_gamut_comp_channel_f32_v(blu, ach, params->thr_yellow, scale_yellow, params->power);
}

/* Decompression function (inverse direction)
 * Given compressed distance, recover original distance */
static alwan_f64 uncompress_dist(alwan_f64 compressed_dist, alwan_f64 thr, alwan_f64 scale, alwan_f64 power) {
    return aces_uncompress_dist_f64_v(compressed_dist, thr, scale, power);
}

/* Per-channel gamut decompression (inverse) */
static alwan_f64 gamut_decomp_channel(alwan_f64 val, alwan_f64 ach,
                                          alwan_f64 thr, alwan_f64 scale, alwan_f64 power) {
    return aces_gamut_decomp_channel_f64_v(val, ach, thr, scale, power);
}

void alwan_aces_gamut_comp13_inv_f64(alwan_rgb_f64 *rgb_out,
                                 alwan_rgb_f64 const *rgb_in,
                                 alwan_aces_gamut_comp13_params_f64 const *params) {
    if (!rgb_out || !rgb_in || !params) return;

    alwan_f64 red = rgb_in->r;
    alwan_f64 grn = rgb_in->g;
    alwan_f64 blu = rgb_in->b;

    /* Compute scales from limits and thresholds using OCIO formula */
    alwan_f64 scale_cyan = calc_gamut_comp_scale(params->lim_cyan, params->thr_cyan, params->power);
    alwan_f64 scale_magenta = calc_gamut_comp_scale(params->lim_magenta, params->thr_magenta, params->power);
    alwan_f64 scale_yellow = calc_gamut_comp_scale(params->lim_yellow, params->thr_yellow, params->power);

    /* Achromatic = max(RGB) - same as forward */
    alwan_f64 ach = max3(red, grn, blu);

    rgb_out->r = gamut_decomp_channel(red, ach, params->thr_cyan, scale_cyan, params->power);
    rgb_out->g = gamut_decomp_channel(grn, ach, params->thr_magenta, scale_magenta, params->power);
    rgb_out->b = gamut_decomp_channel(blu, ach, params->thr_yellow, scale_yellow, params->power);
}

void alwan_aces_gamut_comp13_inv_f32(alwan_rgb_f32 *rgb_out,
                                 alwan_rgb_f32 const *rgb_in,
                                 alwan_aces_gamut_comp13_params_f32 const *params) {
    if (!rgb_out || !rgb_in || !params) return;

    alwan_f32 red = rgb_in->r;
    alwan_f32 grn = rgb_in->g;
    alwan_f32 blu = rgb_in->b;

    /* Compute scales from limits and thresholds using OCIO formula */
    alwan_f32 scale_cyan = aces_calc_gamut_comp_scale_f32_v(params->lim_cyan, params->thr_cyan, params->power);
    alwan_f32 scale_magenta = aces_calc_gamut_comp_scale_f32_v(params->lim_magenta, params->thr_magenta, params->power);
    alwan_f32 scale_yellow = aces_calc_gamut_comp_scale_f32_v(params->lim_yellow, params->thr_yellow, params->power);

    /* Achromatic = max(RGB) - same as forward */
    alwan_f32 ach = alwan_max3_f32(red, grn, blu);

    rgb_out->r = aces_gamut_decomp_channel_f32_v(red, ach, params->thr_cyan, scale_cyan, params->power);
    rgb_out->g = aces_gamut_decomp_channel_f32_v(grn, ach, params->thr_magenta, scale_magenta, params->power);
    rgb_out->b = aces_gamut_decomp_channel_f32_v(blu, ach, params->thr_yellow, scale_yellow, params->power);
}

/* ----------------------------------------------------------------
 * ACES 1.x Output Transform (RRT + ODT)
 * Reference: Academy CTL, OpenColorIO
 * ---------------------------------------------------------------- */

/* ACES 1.x RRT tone scale constants -- shared by the native f32 impl
 * (alwan_aces1_impl.inc) and the f64 workers, so NOT precision-gated. */
static const alwan_f64 ACES1_MIN_STOP_SDR = ALWAN_LITERAL(-6.5);
static const alwan_f64 ACES1_MAX_STOP_SDR = ALWAN_LITERAL(6.5);
static const alwan_f64 ACES1_MIN_STOP_RRT = ALWAN_LITERAL(-15.0);
static const alwan_f64 ACES1_MAX_STOP_RRT = ALWAN_LITERAL(18.0);

/* ACES 1.x RRT parameters */
static const alwan_f64 ACES1_RRT_GLOW_GAIN = ALWAN_LITERAL(0.05);
static const alwan_f64 ACES1_RRT_GLOW_MID = ALWAN_LITERAL(0.08);

/* ACES 1.x helper functions (f64 workers) */
/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (RRT glow/red-modifier helpers shared by the aces1 inverse f32 facade). */
#if ALWAN_WITH_F64_FACADE

/* Calculate saturation as (max - min) / max */
static alwan_f64 aces1_saturation(alwan_f64 r, alwan_f64 g, alwan_f64 b) {
    return aces1_saturation_f64_v(r, g, b);
}

/* Convert RGB to hue in degrees [0, 360) */
static alwan_f64 aces1_rgb_to_hue(alwan_f64 r, alwan_f64 g, alwan_f64 b) {
    return aces1_rgb_to_hue_f64_v(r, g, b);
}

/* Center hue around a target hue */
static alwan_f64 aces1_center_hue(alwan_f64 hue, alwan_f64 center) {
    return aces1_center_hue_f64_v(hue, center);
}

/* Cubic basis shaper - smooth falloff from center */
static alwan_f64 aces1_cubic_basis_shaper(alwan_f64 x, alwan_f64 width) {
    return aces1_cubic_basis_shaper_f64_v(x, width);
}
#endif /* ALWAN_WITH_F64_FACADE */

/* AP0 to AP1 matrix */
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_AP0_TO_AP1_f64[9] = {
#include "../data/matrices/aces_ap0_to_ap1.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const float ACES1_AP0_TO_AP1_f32[9] = {
#include "../data/matrices/aces_ap0_to_ap1.csv"
};
ALWAN_DIAG_POP
#endif

/* AP1 to AP0 matrix */
/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_AP1_TO_AP0[9] = {
#include "../data/matrices/aces_ap1_to_ap0.csv"
};
ALWAN_DIAG_POP
#endif /* ALWAN_WITH_F64_FACADE */

/* D60 to D65 chromatic adaptation (Bradford) */
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_D60_TO_D65_f64[9] = {
#include "../data/matrices/aces_d60_to_d65_bradford.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const float ACES1_D60_TO_D65_f32[9] = {
#include "../data/matrices/aces_d60_to_d65_bradford.csv"
};
ALWAN_DIAG_POP
#endif

/* D65 to D60 chromatic adaptation (Bradford) */
/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_D65_TO_D60[9] = {
#include "../data/matrices/aces_d65_to_d60_bradford.csv"
};
ALWAN_DIAG_POP
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F64
/* Segmented spline function for RRT tone scale
 * This is a simplified version - full implementation would use LUTs */
static alwan_f64 aces1_segmented_spline_c5(alwan_f64 x) {
    return aces1_segmented_spline_c5_f64_v(x);
}
#endif

/* ---- C9 spline parameters per luminance target (from aces-dev CTL) ---- */

typedef struct {
    alwan_f64 coefsLow[10];
    alwan_f64 coefsHigh[10];
    alwan_f64 min_x, min_y;     /* breakpoint: C5(0.18*2^min_stops), display black nits */
    alwan_f64 mid_x, mid_y;     /* breakpoint: C5(0.18), midgray display nits */
    alwan_f64 max_x, max_y;     /* breakpoint: C5(0.18*2^max_stops), display white nits */
    alwan_f64 slope_low;
    alwan_f64 slope_high;
} aces1_c9_params_f64;

/* Native single-precision twin (same field layout, float storage). */
typedef struct {
    float coefsLow[10];
    float coefsHigh[10];
    float min_x, min_y;
    float mid_x, mid_y;
    float max_x, max_y;
    float slope_low;
    float slope_high;
} aces1_c9_params_f32;

/* C9 parameter data generated by alwan_gendata/data/aces1_c9_spline.py
 * Source: aces-dev CTL ACESlib.Tonescales.ctl segmented_spline_c9 params
 * Split into _coefs_low.csv, _coefs_high.csv, _breakpoints.csv per target */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* SDR 48 nit (cinema + video -- all SDR outputs use this) */
/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
static const aces1_c9_params_f64 c9_48nit_f64 = {
    { /* coefs_low[10] */
#include "../data/splines/aces1_c9_48nit_coefs_low.csv"
    },
    { /* coefs_high[10] */
#include "../data/splines/aces1_c9_48nit_coefs_high.csv"
    },
    /* min_x, min_y, mid_x, mid_y, max_x, max_y, slope_low, slope_high */
#include "../data/splines/aces1_c9_48nit_breakpoints.csv"
};

/* HDR 1000 nit (Rec.2020 PQ) */
static const aces1_c9_params_f64 c9_1000nit_f64 = {
    {
#include "../data/splines/aces1_c9_1000nit_coefs_low.csv"
    },
    {
#include "../data/splines/aces1_c9_1000nit_coefs_high.csv"
    },
#include "../data/splines/aces1_c9_1000nit_breakpoints.csv"
};

/* HDR 2000 nit */
static const aces1_c9_params_f64 c9_2000nit_f64 = {
    {
#include "../data/splines/aces1_c9_2000nit_coefs_low.csv"
    },
    {
#include "../data/splines/aces1_c9_2000nit_coefs_high.csv"
    },
#include "../data/splines/aces1_c9_2000nit_breakpoints.csv"
};

/* HDR 4000 nit */
static const aces1_c9_params_f64 c9_4000nit_f64 = {
    {
#include "../data/splines/aces1_c9_4000nit_coefs_low.csv"
    },
    {
#include "../data/splines/aces1_c9_4000nit_coefs_high.csv"
    },
#include "../data/splines/aces1_c9_4000nit_breakpoints.csv"
};
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
/* ---- Native single-precision twins (same CSV data, float storage) ---- */
static const aces1_c9_params_f32 c9_48nit_f32 = {
    {
#include "../data/splines/aces1_c9_48nit_coefs_low.csv"
    },
    {
#include "../data/splines/aces1_c9_48nit_coefs_high.csv"
    },
#include "../data/splines/aces1_c9_48nit_breakpoints.csv"
};

static const aces1_c9_params_f32 c9_1000nit_f32 = {
    {
#include "../data/splines/aces1_c9_1000nit_coefs_low.csv"
    },
    {
#include "../data/splines/aces1_c9_1000nit_coefs_high.csv"
    },
#include "../data/splines/aces1_c9_1000nit_breakpoints.csv"
};

static const aces1_c9_params_f32 c9_2000nit_f32 = {
    {
#include "../data/splines/aces1_c9_2000nit_coefs_low.csv"
    },
    {
#include "../data/splines/aces1_c9_2000nit_coefs_high.csv"
    },
#include "../data/splines/aces1_c9_2000nit_breakpoints.csv"
};

static const aces1_c9_params_f32 c9_4000nit_f32 = {
    {
#include "../data/splines/aces1_c9_4000nit_coefs_low.csv"
    },
    {
#include "../data/splines/aces1_c9_4000nit_coefs_high.csv"
    },
#include "../data/splines/aces1_c9_4000nit_breakpoints.csv"
};
#endif /* ALWAN_WITH_F32 */

ALWAN_DIAG_POP

/* Evaluate C9 spline raw: OCES nits -> display nits (no Y_to_linCV) */
/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
static alwan_f64 aces1_c9_raw(alwan_f64 oces, const aces1_c9_params_f64 *p) {
    alwan_f64 lx = ALWAN_LOG10_F64(fmax(oces, ALWAN_LITERAL(1e-10)));
    alwan_f64 log_min = ALWAN_LOG10_F64(p->min_x);
    alwan_f64 log_mid = ALWAN_LOG10_F64(p->mid_x);
    alwan_f64 log_max = ALWAN_LOG10_F64(p->max_x);
    alwan_f64 ly;

    if (lx <= log_min) {
        ly = ALWAN_LOG10_F64(p->min_y) + p->slope_low * (lx - log_min);
    } else if (lx >= log_max) {
        ly = ALWAN_LOG10_F64(p->max_y) + p->slope_high * (lx - log_max);
    } else {
        alwan_f64 const *co;
        alwan_f64 ks, ke;
        int nk = 8;
        if (lx < log_mid) { co = p->coefsLow; ks = log_min; ke = log_mid; }
        else               { co = p->coefsHigh; ks = log_mid; ke = log_max; }
        alwan_f64 kc = (alwan_f64)(nk - 1) * (lx - ks) / (ke - ks);
        int jj = (int)kc;
        if (jj < 0) jj = 0;
        if (jj > nk - 2) jj = nk - 2;
        alwan_f64 tt = kc - (alwan_f64)jj;
        alwan_f64 f0 = co[jj], f1 = co[jj + 1], f2 = co[jj + 2];
        ly = ALWAN_LITERAL(0.5) * tt * tt * (f0 - ALWAN_LITERAL(2.0) * f1 + f2)
           + tt * (-f0 + f1)
           + ALWAN_LITERAL(0.5) * (f0 + f1);
    }

    return ALWAN_POW_F64(ALWAN_LITERAL(10.0), ly);
}

/* Inverse C9 spline: display nits -> OCES nits (Newton-Raphson) */
static alwan_f64 aces1_c9_inv(alwan_f64 display_nits, const aces1_c9_params_f64 *p) {
    alwan_f64 x = fmax(display_nits, ALWAN_LITERAL(1e-10));
    for (int i = 0; i < 40; i++) {
        alwan_f64 fx = aces1_c9_raw(x, p) - display_nits;
        if (ALWAN_ABS_F64(fx) < ALWAN_LITERAL(1e-10)) break;
        alwan_f64 h = fmax(ALWAN_ABS_F64(x) * ALWAN_LITERAL(1e-6), ALWAN_LITERAL(1e-12));
        alwan_f64 dfx = (aces1_c9_raw(x + h, p) - aces1_c9_raw(x - h, p))
                       / (ALWAN_LITERAL(2.0) * h);
        if (ALWAN_ABS_F64(dfx) < ALWAN_LITERAL(1e-12)) break;
        x -= fx / dfx;
        if (x < ALWAN_LITERAL(1e-10)) x = ALWAN_LITERAL(1e-10);
    }
    return x;
}
#endif /* ALWAN_WITH_F64_FACADE */

/* ---- OCIO GradingRGBCurve data (monotone cubic Hermite in log10 space) ----
 * Generated by alwan_gendata/data/aces1_ocio_curves.py from OCIO built-in config.
 * Layout: x0,y0,slope0, x1,y1,slope1, ... (interleaved triplets) */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* SDR: C5 curve (7 knots) -- ALWAN_LOG10_F64(scene) -> ALWAN_LOG10_F64(OCES nits) */
#if ALWAN_WITH_F64
static const alwan_f64 ocio_sdr_c5_f64[7 * 3] = {
#include "../data/splines/aces1_ocio_sdr_c5.csv"
};

/* SDR: C9 curve (15 knots) -- ALWAN_LOG10_F64(OCES nits) -> ALWAN_LOG10_F64(display nits) */
static const alwan_f64 ocio_sdr_c9_f64[15 * 3] = {
#include "../data/splines/aces1_ocio_sdr_c9.csv"
};

/* HDR 1000 nit: combined curve (7 knots) -- ALWAN_LOG10_F64(scene) -> ALWAN_LOG10_F64(display nits) */
static const alwan_f64 ocio_hdr1000_f64[7 * 3] = {
#include "../data/splines/aces1_ocio_hdr1000.csv"
};
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
/* ---- Native single-precision twins (same CSV data, float storage) ---- */
static const float ocio_sdr_c5_f32[7 * 3] = {
#include "../data/splines/aces1_ocio_sdr_c5.csv"
};
static const float ocio_sdr_c9_f32[15 * 3] = {
#include "../data/splines/aces1_ocio_sdr_c9.csv"
};
static const float ocio_hdr1000_f32[7 * 3] = {
#include "../data/splines/aces1_ocio_hdr1000.csv"
};
#endif /* ALWAN_WITH_F32 */

ALWAN_DIAG_POP

/* Evaluate OCIO-style monotone cubic Hermite curve in log10 space.
 * knots: interleaved (x, y, slope) triplets, n_knots entries.
 * Input/output in log10 domain. */
#if ALWAN_WITH_F64
static alwan_f64 ocio_curve_eval(alwan_f64 log_in,
                                  const alwan_f64 *knots, int n_knots) {
    /* Clamp to curve endpoints */
    if (log_in <= knots[0]) return knots[1];
    if (log_in >= knots[(n_knots - 1) * 3]) return knots[(n_knots - 1) * 3 + 1];

    /* Find segment via linear search (small n) */
    int seg = 0;
    for (int i = 1; i < n_knots; i++) {
        if (log_in < knots[i * 3]) { seg = i - 1; break; }
    }

    /* Cubic Hermite interpolation (matches OCIO GradingBSplineCurve::evalCurve) */
    alwan_f64 x0 = knots[seg * 3],     y0 = knots[seg * 3 + 1],     s0 = knots[seg * 3 + 2];
    alwan_f64 x1 = knots[(seg+1) * 3], y1 = knots[(seg+1) * 3 + 1], s1 = knots[(seg+1) * 3 + 2];
    alwan_f64 dx = x1 - x0;
    alwan_f64 t = (log_in - x0) / dx;
    alwan_f64 t2 = t * t, t3 = t2 * t;

    alwan_f64 h00 = ALWAN_LITERAL(2.0)*t3 - ALWAN_LITERAL(3.0)*t2 + ALWAN_LITERAL(1.0);
    alwan_f64 h10 = t3 - ALWAN_LITERAL(2.0)*t2 + t;
    alwan_f64 h01 = ALWAN_LITERAL(-2.0)*t3 + ALWAN_LITERAL(3.0)*t2;
    alwan_f64 h11 = t3 - t2;

    /* Slopes scaled by segment width (standard Hermite convention) */
    return h00 * y0 + h10 * (s0 * dx) + h01 * y1 + h11 * (s1 * dx);
}

/* Combined OCIO-style C5+C9 evaluation for SDR:
 * scene-linear -> log10 -> C5 curve -> C9 curve -> 10^ -> Y_to_linCV */
static alwan_f64 aces1_ocio_sdr_eval(alwan_f64 scene_val) {
    alwan_f64 log_scene = ALWAN_LOG10_F64(fmax(scene_val, ALWAN_LITERAL(1e-10)));
    alwan_f64 log_oces = ocio_curve_eval(log_scene, ocio_sdr_c5_f64, 7);
    alwan_f64 log_display = ocio_curve_eval(log_oces, ocio_sdr_c9_f64, 15);
    alwan_f64 display_nits = ALWAN_POW_F64(ALWAN_LITERAL(10.0), log_display);
    return (display_nits - ALWAN_LITERAL(0.02)) / (ALWAN_LITERAL(48.0) - ALWAN_LITERAL(0.02));
}

/* Combined OCIO-style evaluation for HDR 1000 nit:
 * scene-linear -> log10 -> combined curve -> 10^ -> Y_to_linCV */
static alwan_f64 aces1_ocio_hdr1000_eval(alwan_f64 scene_val) {
    alwan_f64 log_scene = ALWAN_LOG10_F64(fmax(scene_val, ALWAN_LITERAL(1e-10)));
    alwan_f64 log_display = ocio_curve_eval(log_scene, ocio_hdr1000_f64, 7);
    alwan_f64 display_nits = ALWAN_POW_F64(ALWAN_LITERAL(10.0), log_display);
    return (display_nits - ALWAN_LITERAL(0.0001))
         / (ALWAN_LITERAL(1000.0) - ALWAN_LITERAL(0.0001));
}

/* Evaluate C9 spline + Y_to_linCV.  Input: OCES nits (from C5), output: [0,1] */
static alwan_f64 aces1_segmented_spline_c9(alwan_f64 oces, const aces1_c9_params_f64 *p) {
    alwan_f64 lx = ALWAN_LOG10_F64(fmax(oces, ALWAN_LITERAL(1e-10)));
    alwan_f64 log_min = ALWAN_LOG10_F64(p->min_x);
    alwan_f64 log_mid = ALWAN_LOG10_F64(p->mid_x);
    alwan_f64 log_max = ALWAN_LOG10_F64(p->max_x);
    alwan_f64 ly;

    if (lx <= log_min) {
        ly = ALWAN_LOG10_F64(p->min_y) + p->slope_low * (lx - log_min);
    } else if (lx >= log_max) {
        ly = ALWAN_LOG10_F64(p->max_y) + p->slope_high * (lx - log_max);
    } else {
        alwan_f64 const *co;
        alwan_f64 ks, ke;
        int nk = 8;  /* 10 coefficients -> 8 B-spline segments per half */
        if (lx < log_mid) { co = p->coefsLow; ks = log_min; ke = log_mid; }
        else               { co = p->coefsHigh; ks = log_mid; ke = log_max; }
        alwan_f64 kc = (alwan_f64)(nk - 1) * (lx - ks) / (ke - ks);
        int jj = (int)kc;
        if (jj < 0) jj = 0;
        if (jj > nk - 2) jj = nk - 2;
        alwan_f64 tt = kc - (alwan_f64)jj;
        alwan_f64 f0 = co[jj], f1 = co[jj + 1], f2 = co[jj + 2];
        ly = ALWAN_LITERAL(0.5) * tt * tt * (f0 - ALWAN_LITERAL(2.0) * f1 + f2)
           + tt * (-f0 + f1)
           + ALWAN_LITERAL(0.5) * (f0 + f1);
    }

    alwan_f64 display_nits = ALWAN_POW_F64(ALWAN_LITERAL(10.0), ly);
    return (display_nits - p->min_y) / (p->max_y - p->min_y);
}

#endif /* ALWAN_WITH_F64 */

/* Matrix multiply helper.
 * f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (used by the aces1/aces2 inverse f32 facades). */
#if ALWAN_WITH_F64_FACADE
static void mat3_mul_vec3_aces1(alwan_f64 const *m, alwan_rgb_f64 const *v, alwan_rgb_f64 *out) {
    alwan_vec3_f64 vec = {{v->r, v->g, v->b}};
    alwan_vec3_f64 res = alwan_mat3_mulv_f64_v(*(alwan_mat3x3_f64 const *)m, vec);
    out->r = res.v[0]; out->g = res.v[1]; out->b = res.v[2];
}
#endif /* ALWAN_WITH_F64_FACADE */

/* Output primaries matrices */
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_XYZ_TO_REC709_f64[9] = {
#include "../data/matrices/aces_xyz_to_rec709.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const float ACES1_XYZ_TO_REC709_f32[9] = {
#include "../data/matrices/aces_xyz_to_rec709.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_XYZ_TO_P3D65_f64[9] = {
#include "../data/matrices/aces_xyz_to_p3d65.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const float ACES1_XYZ_TO_P3D65_f32[9] = {
#include "../data/matrices/aces_xyz_to_p3d65.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_XYZ_TO_REC2020_f64[9] = {
#include "../data/matrices/aces_xyz_to_rec2020.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const float ACES1_XYZ_TO_REC2020_f32[9] = {
#include "../data/matrices/aces_xyz_to_rec2020.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_AP1_TO_XYZ_D60_f64[9] = {
#include "../data/matrices/aces_ap1_to_xyz_d60.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const float ACES1_AP1_TO_XYZ_D60_f32[9] = {
#include "../data/matrices/aces_ap1_to_xyz_d60.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
/* Apply BT.1886 EOTF inverse (gamma 2.4) */
static alwan_f64 bt1886_oetf(alwan_f64 x) {
    return aces_bt1886_oetf_f64_v(x);
}

/* Apply sRGB OETF */
static alwan_f64 srgb_oetf(alwan_f64 x) {
    return aces_srgb_oetf_f64_v(x);
}

/* Apply Gamma 2.6 OETF (cinema) */
static alwan_f64 gamma26_oetf(alwan_f64 x) {
    return aces_gamma26_oetf_f64_v(x);
}

/* PQ (ST.2084) OETF */
static alwan_f64 pq_oetf(alwan_f64 Y, alwan_f64 peak_nits) {
    return aces_pq_oetf_f64_v(Y, peak_nits);
}

alwan_status alwan_aces1_output_transform_f64(alwan_rgb_f64 *rgb_out,
                                      alwan_rgb_f64 const *rgb_in,
                                      alwan_aces1_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES1_OUT_COUNT) return ALWAN_E_INVALID;

    alwan_rgb_f64 ap0_mod, ap1, rrt, xyz, d65, display;

/* Step 0: Apply Glow and RedMod in AP0 space (OCIO order: ops 0-1 before AP0->AP1)
     * Source: OCIO BuiltinTransform ACES-OUTPUT SDR-VIDEO_1.0 */
    ap0_mod = *rgb_in;

    /* Glow10 -- matches OCIO ACES_GLOW_10_FWD exactly.
     * glowGain=0.05, glowMid=0.08. Glow is luminance-dependent:
     * full for dark pixels, zero above YC > 2*glowMid. */
    {
        alwan_f64 sat = aces1_saturation(ap0_mod.r, ap0_mod.g, ap0_mod.b);
        /* YC: custom luminance measure from ACES CTL rgb_2_yc */
        alwan_f64 chroma = ALWAN_SQRT_F64(ALWAN_ABS_F64(
            ap0_mod.b * (ap0_mod.b - ap0_mod.g) +
            ap0_mod.g * (ap0_mod.g - ap0_mod.r) +
            ap0_mod.r * (ap0_mod.r - ap0_mod.b)));
        alwan_f64 ycIn = (ap0_mod.r + ap0_mod.g + ap0_mod.b + ALWAN_LITERAL(1.75) * chroma) / ALWAN_LITERAL(3.0);
        alwan_f64 s = sigmoid_shaper(sat);
        alwan_f64 glowGainIn = ACES1_RRT_GLOW_GAIN * s;
        /* glow_fwd: luminance-dependent falloff */
        alwan_f64 glowMid = ALWAN_LITERAL(0.08);
        alwan_f64 glow;
        if (ycIn <= ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0) * glowMid)
            glow = glowGainIn;
        else if (ycIn >= ALWAN_LITERAL(2.0) * glowMid)
            glow = ALWAN_LITERAL(0.0);
        else
            glow = glowGainIn * (glowMid / ycIn - ALWAN_LITERAL(0.5));
        ap0_mod.r *= (ALWAN_LITERAL(1.0) + glow);
        ap0_mod.g *= (ALWAN_LITERAL(1.0) + glow);
        ap0_mod.b *= (ALWAN_LITERAL(1.0) + glow);
    }

    /* RedMod10 in AP0 */
    {
        alwan_f64 hueWeight = calc_hue_weight(ap0_mod.r, ap0_mod.g, ap0_mod.b, REDMOD10_INV_WIDTH);
        if (hueWeight > ALWAN_LITERAL(0.0)) {
            alwan_f64 f_S = calc_sat_weight(ap0_mod.r, ap0_mod.g, ap0_mod.b, REDMOD_NOISE_LIMIT);
            ap0_mod.r = ap0_mod.r + hueWeight * f_S * (REDMOD10_PIVOT - ap0_mod.r) * (ALWAN_LITERAL(1.0) - REDMOD10_SCALE);
        }
    }

    /* Clamp negatives (OCIO op 2) */
    ap0_mod.r = fmax(ap0_mod.r, ALWAN_LITERAL(0.0));
    ap0_mod.g = fmax(ap0_mod.g, ALWAN_LITERAL(0.0));
    ap0_mod.b = fmax(ap0_mod.b, ALWAN_LITERAL(0.0));

/* Step 1: AP0 to AP1 (OCIO op 3) */
    mat3_mul_vec3_aces1(ACES1_AP0_TO_AP1_f64, &ap0_mod, &ap1);

    /* Clamp negatives (OCIO op 4) */
    ap1.r = fmax(ap1.r, ALWAN_LITERAL(0.0));
    ap1.g = fmax(ap1.g, ALWAN_LITERAL(0.0));
    ap1.b = fmax(ap1.b, ALWAN_LITERAL(0.0));

    /* Step 2+3: RRT desaturation + tone curve (C5 + C9)
     * Three paths: B-spline (Academy CTL), legacy Hermite, or OCIO cubic Hermite. */
    {
        alwan_f64 rl = 0.2722287168, gl = 0.6740817658, bl = 0.0536895174;
        alwan_f64 luma = rl*ap1.r + gl*ap1.g + bl*ap1.b;
        rrt.r = luma + ALWAN_LITERAL(0.96) * (ap1.r - luma);
        rrt.g = luma + ALWAN_LITERAL(0.96) * (ap1.g - luma);
        rrt.b = luma + ALWAN_LITERAL(0.96) * (ap1.b - luma);

        int is_hdr_1000 = (output == ALWAN_ACES1_OUT_REC2020_1000NIT_PQ);

        if (g_aces_interp == ALWAN_ACES_INTERP_OCIO) {
            /* OCIO GradingRGBCurve path -- monotone cubic Hermite in log10 space.
             * Matches OCIO pixel-exactly. */
            if (is_hdr_1000) {
                /* HDR 1000: single combined curve replaces both C5+C9 */
                rrt.r = aces1_ocio_hdr1000_eval(rrt.r);
                rrt.g = aces1_ocio_hdr1000_eval(rrt.g);
                rrt.b = aces1_ocio_hdr1000_eval(rrt.b);
            } else {
                /* SDR: two cascaded curves (C5 then C9) */
                rrt.r = aces1_ocio_sdr_eval(rrt.r);
                rrt.g = aces1_ocio_sdr_eval(rrt.g);
                rrt.b = aces1_ocio_sdr_eval(rrt.b);
            }
        } else {
            /* Academy CTL path: separate C5 + C9 B-spline evaluation */
            if (g_aces_interp == ALWAN_ACES_INTERP_HERMITE) {
                rrt.r = aces1_rrt_hermite(rrt.r);
                rrt.g = aces1_rrt_hermite(rrt.g);
                rrt.b = aces1_rrt_hermite(rrt.b);
            } else {
                rrt.r = aces1_segmented_spline_c5(rrt.r);
                rrt.g = aces1_segmented_spline_c5(rrt.g);
                rrt.b = aces1_segmented_spline_c5(rrt.b);
            }

            /* C9 spline */
            const aces1_c9_params_f64 *c9p;
            switch (output) {
                case ALWAN_ACES1_OUT_REC2020_1000NIT_PQ: c9p = &c9_1000nit_f64; break;
                case ALWAN_ACES1_OUT_REC2020_2000NIT_PQ: c9p = &c9_2000nit_f64; break;
                case ALWAN_ACES1_OUT_REC2020_4000NIT_PQ: c9p = &c9_4000nit_f64; break;
                default: c9p = &c9_48nit_f64; break;
            }
            if (g_aces_interp == ALWAN_ACES_INTERP_HERMITE && c9p == &c9_48nit_f64) {
                rrt.r = aces1_odt48_hermite(rrt.r);
                rrt.g = aces1_odt48_hermite(rrt.g);
                rrt.b = aces1_odt48_hermite(rrt.b);
            } else {
                rrt.r = aces1_segmented_spline_c9(rrt.r, c9p);
                rrt.g = aces1_segmented_spline_c9(rrt.g, c9p);
                rrt.b = aces1_segmented_spline_c9(rrt.b, c9p);
            }
        }

    }

    /* dimSurround + ODT desaturation apply ONLY to 100-nit video outputs.
     * Cinema (dark surround) and HDR PQ outputs skip both -- per ACES ODT specs.
     * HDR PQ ODTs use a combined tone curve and no surround correction.
     * Cinema = 48 nit (P3DCI, P3D60, P3D65 48nit, DCDM)
     * HDR PQ = 1000/2000/4000 nit ST.2084 */
    {
        int is_cinema = (output == ALWAN_ACES1_OUT_P3DCI_48NIT ||
                         output == ALWAN_ACES1_OUT_P3D60_48NIT ||
                         output == ALWAN_ACES1_OUT_P3D65_48NIT ||
                         output == ALWAN_ACES1_OUT_DCDM_48NIT);
        int is_hdr_pq = (output == ALWAN_ACES1_OUT_REC2020_1000NIT_PQ ||
                         output == ALWAN_ACES1_OUT_REC2020_2000NIT_PQ ||
                         output == ALWAN_ACES1_OUT_REC2020_4000NIT_PQ);

        if (!is_cinema && !is_hdr_pq) {
            /* darkSurround_to_dimSurround (OCIO: ACES_DARK_TO_DIM_10_FWD)
             * scale RGB by Y^(gamma-1) where Y = AP1 luminance. */
            ALWAN_DIAG_PUSH
            ALWAN_DIAG_DISABLE_FLOAT_CONV
            static alwan_f64 const dim_rl = 0.2722287168, dim_gl = 0.6740817658, dim_bl = 0.0536895174;
            ALWAN_DIAG_POP
            alwan_f64 Y = dim_rl * rrt.r + dim_gl * rrt.g + dim_bl * rrt.b;
            if (Y > ALWAN_LITERAL(1e-10)) {
                alwan_f64 scale = ALWAN_POW_F64(Y, ALWAN_LITERAL(-0.0189));
                rrt.r *= scale;
                rrt.g *= scale;
                rrt.b *= scale;
            }

            /* ODT desaturation (ODT_SAT_FACTOR = 0.93) */
            ALWAN_DIAG_PUSH
            ALWAN_DIAG_DISABLE_FLOAT_CONV
            static alwan_f64 const odt_rl2 = 0.2722287168, odt_gl2 = 0.6740817658, odt_bl2 = 0.0536895174;
            static alwan_f64 const odt_s = 0.93;
            ALWAN_DIAG_POP
            alwan_f64 odt_luma = odt_rl2 * rrt.r + odt_gl2 * rrt.g + odt_bl2 * rrt.b;
            rrt.r = odt_luma + odt_s * (rrt.r - odt_luma);
            rrt.g = odt_luma + odt_s * (rrt.g - odt_luma);
            rrt.b = odt_luma + odt_s * (rrt.b - odt_luma);
        }
    }


/* Step 4: AP1 to XYZ (D60) */
    mat3_mul_vec3_aces1(ACES1_AP1_TO_XYZ_D60_f64, &rrt, &xyz);

    /* Step 4: D60 to D65 (for D65 white point outputs) */
    int needs_d65 = (output != ALWAN_ACES1_OUT_SRGB_D60_100NIT &&
                     output != ALWAN_ACES1_OUT_P3D60_48NIT);
    if (needs_d65) {
        mat3_mul_vec3_aces1(ACES1_D60_TO_D65_f64, &xyz, &d65);
    } else {
        d65 = xyz;
    }

    /* Step 5: XYZ to output primaries */
    alwan_f64 const *xyz_to_display = NULL;
    alwan_f64 peak_nits = ALWAN_LITERAL(100.0);
    int use_pq = 0;
    int use_srgb = 0;
    int use_gamma26 = 0;

    switch (output) {
        case ALWAN_ACES1_OUT_REC709_100NIT:
            xyz_to_display = ACES1_XYZ_TO_REC709_f64;
            break;
        case ALWAN_ACES1_OUT_SRGB_100NIT:
        case ALWAN_ACES1_OUT_SRGB_D60_100NIT:
            xyz_to_display = ACES1_XYZ_TO_REC709_f64;
            use_srgb = 1;
            break;
        case ALWAN_ACES1_OUT_P3DCI_48NIT:
        case ALWAN_ACES1_OUT_P3D60_48NIT:
        case ALWAN_ACES1_OUT_P3D65_48NIT:
            xyz_to_display = ACES1_XYZ_TO_P3D65_f64;
            use_gamma26 = 1;
            break;
        case ALWAN_ACES1_OUT_P3D65_100NIT:
            xyz_to_display = ACES1_XYZ_TO_P3D65_f64;
            use_srgb = 1;
            break;
        case ALWAN_ACES1_OUT_REC2020_100NIT:
            xyz_to_display = ACES1_XYZ_TO_REC2020_f64;
            break;
        case ALWAN_ACES1_OUT_REC2020_1000NIT_PQ:
            xyz_to_display = ACES1_XYZ_TO_REC2020_f64;
            use_pq = 1;
            peak_nits = ALWAN_LITERAL(1000.0);
            break;
        case ALWAN_ACES1_OUT_REC2020_2000NIT_PQ:
            xyz_to_display = ACES1_XYZ_TO_REC2020_f64;
            use_pq = 1;
            peak_nits = ALWAN_LITERAL(2000.0);
            break;
        case ALWAN_ACES1_OUT_REC2020_4000NIT_PQ:
            xyz_to_display = ACES1_XYZ_TO_REC2020_f64;
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

    /* Clamp to [0, 1] in display-linear space (matches OCIO RangeTransform before EOTF) */
    display.r = fmin(fmax(display.r, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(1.0));
    display.g = fmin(fmax(display.g, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(1.0));
    display.b = fmin(fmax(display.b, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(1.0));

    /* Step 6: Apply OETF */
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
#endif /* ALWAN_WITH_F64 */

/* Inverse EOTF functions.
 * f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (these inverse-direction helpers back the aces1 inverse f32 facade). */
#if ALWAN_WITH_F64_FACADE
static alwan_f64 bt1886_eotf(alwan_f64 x) {
    return aces_bt1886_eotf_f64_v(x);
}

static alwan_f64 srgb_eotf(alwan_f64 x) {
    return aces_srgb_eotf_f64_v(x);
}

static alwan_f64 gamma26_eotf(alwan_f64 x) {
    return aces_gamma26_eotf_f64_v(x);
}

static alwan_f64 pq_eotf(alwan_f64 E, alwan_f64 peak_nits) {
    return aces_pq_eotf_f64_v(E, peak_nits);
}

/* Inverse XYZ to output primaries matrices */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_REC709_TO_XYZ[9] = {
#include "../data/matrices/aces_rec709_to_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_P3D65_TO_XYZ[9] = {
#include "../data/matrices/aces_p3d65_to_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_REC2020_TO_XYZ[9] = {
#include "../data/matrices/aces_rec2020_to_xyz.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static const alwan_f64 ACES1_XYZ_D60_TO_AP1[9] = {
#include "../data/matrices/aces_xyz_d60_to_ap1.csv"
};
ALWAN_DIAG_POP

/* Inverse RRT (simplified) */
static alwan_f64 aces1_segmented_spline_c5_inv(alwan_f64 y) {
    return aces1_segmented_spline_c5_inv_f64_v(y);
}

#endif /* ALWAN_WITH_F64_FACADE */

/* Native single-precision ACES 1.x output transform + inverse, generated
 * from alwan_aces1_impl.inc with f32 setup. The f64 path above remains the
 * OCIO-validated reference. All required f64 data (matrices, C9 params, OCIO
 * curves), the aces1_c9_params typedef, the g_aces_interp global and the
 * REDMOD/GLOW macros are already defined above this point. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_aces1_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
/* ----------------------------------------------------------------
 * Inverse of the two AP0-space RRT steps (Glow10, then RedMod10)
 *
 * The forward order is Glow10 then RedMod10, so the inverse runs RedMod10
 * first. Both are exact here. OCIO's own RedMod10 inverse is not: it takes
 * the closed form below as its answer, which round-trips a saturated red to
 * about 3.7e-03. See docs/alwan_decisions.md.
 * ---------------------------------------------------------------- */

/* The forward RedMod10 map restricted to red, with green and blue fixed. */
static alwan_f64 aces1_redmod10_fwd_red(alwan_f64 red, alwan_f64 grn, alwan_f64 blu) {
    alwan_f64 f_H = calc_hue_weight(red, grn, blu, REDMOD10_INV_WIDTH);
    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_f64 f_S = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
        red = red + f_H * f_S * (REDMOD10_PIVOT - red) *
              (ALWAN_LITERAL(1.0) - REDMOD10_SCALE);
    }
    return red;
}

/* Solve aces1_redmod10_fwd_red(red) == red_out for red.
 *
 * Green and blue pass through the forward untouched, so this is a scalar root
 * find. Writing w = f_H * f_S and k = 1 - scale, the forward is
 *
 *     red_out = red * (1 - w*k) + w*k*pivot
 *
 * and w*k is confined to [0, k] because both weights are in [0, 1]. Solving
 * that relation at each end of the range brackets the root, so no search for a
 * bracket is needed and the solve cannot run away. The map is monotonically
 * increasing in red, verified over the full grid of green and blue, so the
 * bracket contains exactly one root.
 *
 * Regula falsi with the Illinois correction: bracketed like bisection, but
 * converges in single-digit iterations because the map is nearly linear. */
static alwan_f64 aces1_redmod10_inv_red(alwan_f64 red_out, alwan_f64 grn, alwan_f64 blu) {
    alwan_f64 const k = ALWAN_LITERAL(1.0) - REDMOD10_SCALE;
    alwan_f64 const other = (red_out - k * REDMOD10_PIVOT) / (ALWAN_LITERAL(1.0) - k);
    alwan_f64 lo = red_out < other ? red_out : other;
    alwan_f64 hi = red_out < other ? other : red_out;
    alwan_f64 f_lo = aces1_redmod10_fwd_red(lo, grn, blu) - red_out;
    alwan_f64 f_hi = aces1_redmod10_fwd_red(hi, grn, blu) - red_out;
    alwan_f64 const scale = ALWAN_ABS_F64(red_out) > ALWAN_LITERAL(1.0)
                            ? ALWAN_ABS_F64(red_out) : ALWAN_LITERAL(1.0);
    alwan_f64 const tol = ALWAN_LITERAL(1e-16) * scale;
    int i;

    if (f_lo == ALWAN_LITERAL(0.0)) return lo;
    if (f_hi == ALWAN_LITERAL(0.0)) return hi;
    /* Not bracketed: the only way here is a non-finite input, where there is
     * nothing to solve. Leave the value alone rather than invent one. */
    if (!(f_lo < ALWAN_LITERAL(0.0) && f_hi > ALWAN_LITERAL(0.0))) return red_out;

    for (i = 0; i < 40 && (hi - lo) > tol; i++) {
        alwan_f64 const denom = f_hi - f_lo;
        alwan_f64 mid = denom != ALWAN_LITERAL(0.0)
                        ? hi - f_hi * (hi - lo) / denom
                        : lo + (hi - lo) * ALWAN_LITERAL(0.5);
        alwan_f64 f_mid;

        if (!(mid > lo && mid < hi)) {
            mid = lo + (hi - lo) * ALWAN_LITERAL(0.5);
        }
        f_mid = aces1_redmod10_fwd_red(mid, grn, blu) - red_out;
        if (f_mid == ALWAN_LITERAL(0.0)) return mid;
        if (f_mid < ALWAN_LITERAL(0.0)) {
            lo = mid; f_lo = f_mid;
            f_hi *= ALWAN_LITERAL(0.5);   /* Illinois: stop the stalled end sticking */
        } else {
            hi = mid; f_hi = f_mid;
            f_lo *= ALWAN_LITERAL(0.5);
        }
    }
    return lo + (hi - lo) * ALWAN_LITERAL(0.5);
}

/* Exact inverse of the RRT Glow10 step.
 *
 * The forward multiplies all three channels by (1 + glow). Saturation is
 * (max - min) / max, which a uniform scale leaves unchanged, and YC is
 * homogeneous of degree one, so the gain and the branch are both recoverable
 * from the output alone and the inverse is closed form. */
static void aces1_glow10_inv(alwan_rgb_f64 *rgb) {
    alwan_f64 const glow_mid = ACES1_RRT_GLOW_MID;
    alwan_f64 const sat = aces1_saturation(rgb->r, rgb->g, rgb->b);
    alwan_f64 const chroma = ALWAN_SQRT_F64(ALWAN_ABS_F64(
        rgb->b * (rgb->b - rgb->g) +
        rgb->g * (rgb->g - rgb->r) +
        rgb->r * (rgb->r - rgb->b)));
    alwan_f64 const yc_out = (rgb->r + rgb->g + rgb->b +
                              ALWAN_LITERAL(1.75) * chroma) / ALWAN_LITERAL(3.0);
    alwan_f64 const gain = ACES1_RRT_GLOW_GAIN * sigmoid_shaper(sat);
    alwan_f64 const knee_hi = ALWAN_LITERAL(2.0) * glow_mid;
    alwan_f64 const knee_lo = ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0) * glow_mid *
                              (ALWAN_LITERAL(1.0) + gain);
    alwan_f64 yc_in;

    /* Above the upper knee the forward added nothing. */
    if (yc_out >= knee_hi) return;

    if (yc_out <= knee_lo) {
        /* Flat region: the forward applied the full gain. */
        yc_in = yc_out / (ALWAN_LITERAL(1.0) + gain);
    } else {
        /* Falloff region. The forward is
         *     yc_out = yc_in * (1 - gain/2) + gain * glow_mid
         * which is linear in yc_in, so this inverts directly. */
        yc_in = (yc_out - gain * glow_mid) /
                (ALWAN_LITERAL(1.0) - ALWAN_LITERAL(0.5) * gain);
    }

    if (yc_out > ALWAN_LITERAL(0.0) && yc_in > ALWAN_LITERAL(0.0)) {
        alwan_f64 const inv_scale = yc_in / yc_out;
        rgb->r *= inv_scale;
        rgb->g *= inv_scale;
        rgb->b *= inv_scale;
    }
}

alwan_status alwan_aces1_output_transform_inv_f64(alwan_rgb_f64 *rgb_out,
                                          alwan_rgb_f64 const *rgb_in,
                                          alwan_aces1_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES1_OUT_COUNT) return ALWAN_E_INVALID;

    /* Each switch arm sets either `display` (with a matrix to convert it) or
     * `xyz` directly. Zero-initialised because the compiler cannot see that
     * `display_to_xyz == NULL` is exactly the case where `xyz` is already set. */
    alwan_rgb_f64 display = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_rgb_f64 xyz = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_rgb_f64 d60, ap1;

    /* Step 1: Inverse EOTF */
    alwan_f64 const *display_to_xyz = NULL;
    alwan_f64 peak_nits = ALWAN_LITERAL(100.0);

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
            /* DCDM is encoded directly in XYZ, so there is no display-primaries
             * matrix to undo; display_to_xyz stays NULL and step 2 is skipped.
             *
             * This used to return here through a separate AP1-space RRT inverse,
             * which did not invert the forward: the forward runs the whole
             * pipeline and diverges only at this final encode. Falling through
             * means DCDM is inverted by the same steps as every other output. */
            xyz.r = gamma26_eotf(rgb_in->r) * ALWAN_LITERAL(52.37) / ALWAN_LITERAL(48.0);
            xyz.g = gamma26_eotf(rgb_in->g) * ALWAN_LITERAL(52.37) / ALWAN_LITERAL(48.0);
            xyz.b = gamma26_eotf(rgb_in->b) * ALWAN_LITERAL(52.37) / ALWAN_LITERAL(48.0);
            break;
        default:
            return ALWAN_E_INVALID;
    }

    /* Step 2: Display to XYZ. DCDM is already in XYZ and leaves this NULL. */
    if (display_to_xyz != NULL) {
        mat3_mul_vec3_aces1(display_to_xyz, &display, &xyz);
    }

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

    /* Step 5-6: Inverse ODT desaturation + dimSurround (100-nit video outputs only) */
    {
        int is_cinema = (output == ALWAN_ACES1_OUT_P3DCI_48NIT ||
                         output == ALWAN_ACES1_OUT_P3D60_48NIT ||
                         output == ALWAN_ACES1_OUT_P3D65_48NIT ||
                         output == ALWAN_ACES1_OUT_DCDM_48NIT);
        int is_hdr_pq = (output == ALWAN_ACES1_OUT_REC2020_1000NIT_PQ ||
                         output == ALWAN_ACES1_OUT_REC2020_2000NIT_PQ ||
                         output == ALWAN_ACES1_OUT_REC2020_4000NIT_PQ);
        if (!is_cinema && !is_hdr_pq) {
            /* Inverse ODT desaturation */
            alwan_f64 rl = 0.2722287168, gl = 0.6740817658, bl = 0.0536895174;
            alwan_f64 s = 0.93;
            alwan_f64 luma = rl * ap1.r + gl * ap1.g + bl * ap1.b;
            ap1.r = luma + (ap1.r - luma) / s;
            ap1.g = luma + (ap1.g - luma) / s;
            ap1.b = luma + (ap1.b - luma) / s;

            /* Inverse dimSurround.
             *
             * The forward is x' = x * Y^-g with Y = luma(x), so the luminance
             * it produces is luma(x') = Y * Y^-g = Y^(1-g), which means
             * Y = luma(x')^(1/(1-g)) and
             *
             *     x = x' * Y^g = x' * luma(x')^(g/(1-g)).
             *
             * The exponent is g/(1-g), not g: only the pre-scale luminance
             * raised to g undoes the forward, and all we have here is the
             * post-scale one. Using g inverted a slightly different transform
             * and left 9.4e-04 of relative round-trip error on every 100-nit
             * video output. The desaturation above preserves luma exactly, so
             * Y here is the forward's output luminance.
             *
             * Cinema and HDR PQ outputs skip this block and already round-trip
             * at 1.3e-11, which is what isolated this. */
            alwan_f64 const dim_g = ALWAN_LITERAL(0.0189);
            alwan_f64 Y = rl * ap1.r + gl * ap1.g + bl * ap1.b;
            if (Y > ALWAN_LITERAL(1e-10)) {
                alwan_f64 scale = ALWAN_POW_F64(Y, dim_g / (ALWAN_LITERAL(1.0) - dim_g));
                ap1.r *= scale;
                ap1.g *= scale;
                ap1.b *= scale;
            }
        }
    }

    /* Step 7: Inverse C9 + Y_to_linCV -> OCES nits */
    {
        const aces1_c9_params_f64 *c9p;
        switch (output) {
            case ALWAN_ACES1_OUT_REC2020_1000NIT_PQ: c9p = &c9_1000nit_f64; break;
            case ALWAN_ACES1_OUT_REC2020_2000NIT_PQ: c9p = &c9_2000nit_f64; break;
            case ALWAN_ACES1_OUT_REC2020_4000NIT_PQ: c9p = &c9_4000nit_f64; break;
            default: c9p = &c9_48nit_f64; break;
        }
        /* Inverse Y_to_linCV: [0,1] -> display nits */
        ap1.r = ap1.r * (c9p->max_y - c9p->min_y) + c9p->min_y;
        ap1.g = ap1.g * (c9p->max_y - c9p->min_y) + c9p->min_y;
        ap1.b = ap1.b * (c9p->max_y - c9p->min_y) + c9p->min_y;
        /* Inverse C9: display nits -> OCES nits */
        ap1.r = aces1_c9_inv(ap1.r, c9p);
        ap1.g = aces1_c9_inv(ap1.g, c9p);
        ap1.b = aces1_c9_inv(ap1.b, c9p);
    }

    /* Step 8: Inverse C5 spline */
    ap1.r = aces1_segmented_spline_c5_inv(ap1.r);
    ap1.g = aces1_segmented_spline_c5_inv(ap1.g);
    ap1.b = aces1_segmented_spline_c5_inv(ap1.b);

    /* Step 9: Inverse RRT desaturation (factor 0.96) */
    {
        alwan_f64 rl = 0.2722287168, gl = 0.6740817658, bl = 0.0536895174;
        alwan_f64 luma = rl * ap1.r + gl * ap1.g + bl * ap1.b;
        ap1.r = luma + (ap1.r - luma) / ALWAN_LITERAL(0.96);
        ap1.g = luma + (ap1.g - luma) / ALWAN_LITERAL(0.96);
        ap1.b = luma + (ap1.b - luma) / ALWAN_LITERAL(0.96);
    }

    /* Step 10: AP1 to AP0 */
    mat3_mul_vec3_aces1(ACES1_AP1_TO_AP0, &ap1, rgb_out);

    /* Step 11: Inverse RedMod10 then Inverse Glow10 in AP0.
     * Reverses step 0 of the forward, which applies Glow10 then RedMod10. */
    rgb_out->r = aces1_redmod10_inv_red(rgb_out->r, rgb_out->g, rgb_out->b);
    aces1_glow10_inv(rgb_out);

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
/* alwan_aces1_output_transform_inv_f32 stays f64-internal (widen -> f64 ->
 * narrow): the ACES 1.x inverse uses iterative solvers (C9 Newton-Raphson,
 * 5-iteration RRT inverse) whose f64-scale convergence thresholds (1e-10/
 * 1e-12) are below f32 epsilon, so a native-f32 inverse fails to converge.
 * The forward is native f32 (alwan_aces1_impl.inc); see test 90. */
alwan_status alwan_aces1_output_transform_inv_f32(alwan_rgb_f32 *rgb_out,
                                          alwan_rgb_f32 const *rgb_in,
                                          alwan_aces1_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    int s = alwan_aces1_output_transform_inv_f64(&out64, &in64, output);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
    return s;
}
#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * ACES Primaries
 * Reference: Academy S-2014-003, S-2016-001
 * ---------------------------------------------------------------- */

/* ACES AP1 (ACEScg) primaries -- D60 white */
static const alwan_f64 AP1_RED_x   = ALWAN_AP1_RED_x;
static const alwan_f64 AP1_RED_y   = ALWAN_AP1_RED_y;
static const alwan_f64 AP1_GREEN_x = ALWAN_AP1_GREEN_x;
static const alwan_f64 AP1_GREEN_y = ALWAN_AP1_GREEN_y;
static const alwan_f64 AP1_BLUE_x  = ALWAN_AP1_BLUE_x;
static const alwan_f64 AP1_BLUE_y  = ALWAN_AP1_BLUE_y;
static const alwan_f64 AP1_WHITE_x = ALWAN_ACES_WHITE_x;
static const alwan_f64 AP1_WHITE_y = ALWAN_ACES_WHITE_y;

/* ----------------------------------------------------------------
 * ACES 2.0: Constants and Viewing Conditions
 * Reference: ACES CTL Lib.Academy.OutputTransform.ctl
 * ---------------------------------------------------------------- */

/* CAM16 primaries - used for computing cone response matrices */
static const alwan_f64 CAM16_PRI_RED_X = ALWAN_LITERAL(0.8336);
static const alwan_f64 CAM16_PRI_RED_Y = ALWAN_LITERAL(0.1735);
static const alwan_f64 CAM16_PRI_GREEN_X = ALWAN_LITERAL(2.3854);
static const alwan_f64 CAM16_PRI_GREEN_Y = ALWAN_LITERAL(-1.4659);
static const alwan_f64 CAM16_PRI_BLUE_X = ALWAN_LITERAL(0.087);
static const alwan_f64 CAM16_PRI_BLUE_Y = ALWAN_LITERAL(-0.125);
static const alwan_f64 CAM16_WHITE_X = ALWAN_LITERAL(0.333333333333);
static const alwan_f64 CAM16_WHITE_Y = ALWAN_LITERAL(0.333333333333);

/* ACES viewing condition parameters */
static const alwan_f64 ACES2_REF_LUMINANCE = ALWAN_LITERAL(100.0);
static const alwan_f64 ACES2_L_A = ALWAN_LITERAL(100.0);  /* Adapting luminance */
static const alwan_f64 ACES2_Y_b = ALWAN_LITERAL(20.0);   /* Background luminance factor */

/* Surround parameters (Dim surround) */
static const alwan_f64 ACES2_SURROUND_F = ALWAN_LITERAL(0.9);
static const alwan_f64 ACES2_SURROUND_C = ALWAN_LITERAL(0.59);
static const alwan_f64 ACES2_SURROUND_N_c = ALWAN_LITERAL(0.9);

/* CAM16 nonlinearity constants (use core header for OFFSET) */
static const alwan_f64 CAM_NL_Y_REF = ALWAN_LITERAL(100.0);
static const alwan_f64 CAM_NL_OFFSET = ACES_CAM_NL_OFFSET_VALUE;
static const alwan_f64 CAM_NL_SCALE = ALWAN_LITERAL(400.0);   /* 4.0 * 100.0 */

/* Lightness scale factor (J_scale = 100) */
static const alwan_f64 J_SCALE = ACES_J_SCALE_VALUE;

/* ----------------------------------------------------------------
 * ACES 2.0: Chroma Compression Constants (from OCIO Common.h)
 * ---------------------------------------------------------------- */

/* Chroma compression parameters */
static const alwan_f64 ACES2_CHROMA_COMPRESS = ALWAN_LITERAL(2.4);
static const alwan_f64 ACES2_CHROMA_COMPRESS_FACT = ALWAN_LITERAL(3.3);
static const alwan_f64 ACES2_CHROMA_EXPAND = ALWAN_LITERAL(1.3);
static const alwan_f64 ACES2_CHROMA_EXPAND_FACT = ALWAN_LITERAL(0.69);
static const alwan_f64 ACES2_CHROMA_EXPAND_THR = ALWAN_LITERAL(0.5);

/* Fourier coefficients -- alias core CSV-loaded arrays */
#define ACES2_CHROMA_NORM_COS ACES2_CHROMA_NORM_COS_V
#define ACES2_CHROMA_NORM_SIN ACES2_CHROMA_NORM_SIN_V

/* Base cone response to Aab matrix (before scaling)
 * Row 0: [2, 1, 1/20]         - Achromatic channel
 * Row 1: [1, -12/11, 1/11]    - Red-green opponent
 * Row 2: [1/9, 1/9, -2/9]     - Yellow-blue opponent
 */
static const alwan_f64 CONE_TO_AAB_BASE[9] = {
    ALWAN_LITERAL(2.0),                  ALWAN_LITERAL(1.0),                  ALWAN_LITERAL(0.05),             /* 1/20 */
    ALWAN_LITERAL(1.0),                  ALWAN_LITERAL(-1.090909090909090909), ALWAN_LITERAL(0.090909090909090909), /* -12/11, 1/11 */
    ALWAN_LITERAL(0.111111111111111111), ALWAN_LITERAL(0.111111111111111111), ALWAN_LITERAL(-0.222222222222222222)  /* 1/9, 1/9, -2/9 */
};

/* ----------------------------------------------------------------
 * ACES 2.0: Matrix helper functions
 * ---------------------------------------------------------------- */

/* Compute RGB to XYZ matrix from chromaticities */
static void primaries_to_rgb_to_xyz_f64(alwan_f64 rx, alwan_f64 ry,
                                     alwan_f64 gx, alwan_f64 gy,
                                     alwan_f64 bx, alwan_f64 by,
                                     alwan_f64 wx, alwan_f64 wy,
                                     alwan_f64 Y,
                                     alwan_f64 out[9]) {
    alwan_mat3x3_f64 result = aces_primaries_to_rgb_to_xyz_f64_v(rx, ry, gx, gy, bx, by, wx, wy, Y);
    for (int i = 0; i < 9; i++) out[i] = result.m[i];
}

/* Invert 3x3 matrix */
static int invert_mat3(alwan_f64 const m[9], alwan_f64 out[9]) {
    alwan_mat3x3_f64 result = alwan_mat3_inv_f64_v(*(alwan_mat3x3_f64 const *)m);
    for (int i = 0; i < 9; i++) out[i] = result.m[i];
    return 0;
}


/* Multiply 3x3 matrices: out = a * b */
static void mult_mat3(alwan_f64 const a[9], alwan_f64 const b[9], alwan_f64 out[9]) {
    alwan_mat3x3_f64 result = alwan_mat3_mul_f64_v(*(alwan_mat3x3_f64 const *)a, *(alwan_mat3x3_f64 const *)b);
    for (int i = 0; i < 9; i++) out[i] = result.m[i];
}

/* Multiply vector by matrix: out = m * v */
static void mult_vec_mat3(alwan_f64 const v[3], alwan_f64 const m[9], alwan_f64 out[3]) {
    alwan_vec3_f64 vec = {{v[0], v[1], v[2]}};
    alwan_vec3_f64 result = alwan_mat3_mulv_f64_v(*(alwan_mat3x3_f64 const *)m, vec);
    out[0] = result.v[0]; out[1] = result.v[1]; out[2] = result.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Post-adaptation cone response compression
 * Reference: ACES CTL _post_adaptation_cone_response_compression_fwd
 * Formula: Ra = (Rc^0.42) / (27.13 + Rc^0.42)
 * ---------------------------------------------------------------- */

static alwan_f64 post_adaptation_cone_response_compression_fwd(alwan_f64 v) {
    return aces_cone_response_fwd_f64_v(v);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Toe compression function (from OCIO Transform.cpp)
 * Smooth nonlinear compression using quadratic formula
 * ---------------------------------------------------------------- */

static alwan_f64 toe_fwd(alwan_f64 x, alwan_f64 limit,
                            alwan_f64 k1_in, alwan_f64 k2_in) {
    return aces_toe_fwd_f64_v(x, limit, k1_in, k2_in);
}

static alwan_f64 toe_inv(alwan_f64 x, alwan_f64 limit,
                            alwan_f64 k1_in, alwan_f64 k2_in) {
    return aces_toe_inv_f64_v(x, limit, k1_in, k2_in);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Chroma compression normalization (Fourier series)
 * Computes hue-dependent normalization factor using harmonic terms
 * ---------------------------------------------------------------- */

static alwan_f64 chroma_compress_norm(alwan_f64 h_rad, alwan_f64 scale) {
    return aces_chroma_compress_norm_f64_v(h_rad, scale);
}

/* ----------------------------------------------------------------
 * ACES 2.0: aces2_JMhParams_f64 computation
 * Computes all matrices and parameters needed for RGB to JMh conversion
 * ---------------------------------------------------------------- */

/* Chroma compression parameters (computed from peak luminance) */
typedef struct {
    alwan_f64 sat;                /* Saturation toe parameter */
    alwan_f64 sat_thr;            /* Saturation threshold */
    alwan_f64 compr;              /* Compression parameter */
    alwan_f64 chroma_compress_scale;  /* Scale for chroma normalization */
    alwan_f64 limit_J_max;        /* Maximum J at peak luminance */
    alwan_f64 model_gamma_inv;    /* 1/cz */
    alwan_f64 reach_m_table[ACES2_REACH_TABLE_SIZE];  /* Max chroma per hue */
} ChromaCompressParams_f64;

typedef struct {
    float sat;
    float sat_thr;
    float compr;
    float chroma_compress_scale;
    float limit_J_max;
    float model_gamma_inv;
    float reach_m_table[ACES2_REACH_TABLE_SIZE];
} ChromaCompressParams_f32;

static void init_JMhParams_f64(alwan_aces_primaries_f64 const *primaries, aces2_JMhParams_f64 *p) {
    /* Compute F_L (luminance adaptation factor) */
    alwan_f64 k = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(5.0) * ACES2_L_A + ALWAN_LITERAL(1.0));
    alwan_f64 k4 = k * k * k * k;
    alwan_f64 F_L = ALWAN_LITERAL(0.2) * k4 * (ALWAN_LITERAL(5.0) * ACES2_L_A)
                     + ALWAN_LITERAL(0.1) * (ALWAN_LITERAL(1.0) - k4) * (ALWAN_LITERAL(1.0) - k4)
                       * ALWAN_POW(ALWAN_LITERAL(5.0) * ACES2_L_A, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    alwan_f64 F_L_n = F_L / ACES2_REF_LUMINANCE;

    /* Compute model gamma (cz) */
    p->cz = ACES2_SURROUND_C * (ALWAN_LITERAL(1.48) + ALWAN_SQRT(ACES2_Y_b / ACES2_REF_LUMINANCE));
    p->inv_cz = ALWAN_LITERAL(1.0) / p->cz;

    /* Build RGB to XYZ matrix for input primaries */
    alwan_f64 rgb_to_xyz[9];
    primaries_to_rgb_to_xyz_f64(primaries->red_x, primaries->red_y,
                            primaries->green_x, primaries->green_y,
                            primaries->blue_x, primaries->blue_y,
                            primaries->white_x, primaries->white_y,
                            ALWAN_LITERAL(1.0), rgb_to_xyz);

    /* Build XYZ to CAM16 RGB matrix (CAM16 primaries) */
    alwan_f64 cam16_rgb_to_xyz[9];
    primaries_to_rgb_to_xyz_f64(CAM16_PRI_RED_X, CAM16_PRI_RED_Y,
                            CAM16_PRI_GREEN_X, CAM16_PRI_GREEN_Y,
                            CAM16_PRI_BLUE_X, CAM16_PRI_BLUE_Y,
                            CAM16_WHITE_X, CAM16_WHITE_Y,
                            ALWAN_LITERAL(1.0), cam16_rgb_to_xyz);

    alwan_f64 xyz_to_cam16_rgb[9];
    invert_mat3(cam16_rgb_to_xyz, xyz_to_cam16_rgb);

    /* Compute XYZ_w for white point (1,1,1) in input primaries */
    alwan_f64 white_rgb[3] = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
    alwan_f64 white_xyz[3];
    mult_vec_mat3(white_rgb, rgb_to_xyz, white_xyz);
    alwan_f64 Y_W = white_xyz[1];

    /* Compute white point in CAM16 RGB */
    alwan_f64 RGB_w[3];
    mult_vec_mat3(white_xyz, xyz_to_cam16_rgb, RGB_w);

    /* Compute chromatic adaptation coefficients (D = 1 for complete adaptation) */
    /* D_RGB[i] = F_L_n * Y_W / RGB_w[i] */
    alwan_f64 D_RGB[3];
    D_RGB[0] = F_L_n * Y_W / RGB_w[0];
    D_RGB[1] = F_L_n * Y_W / RGB_w[1];
    D_RGB[2] = F_L_n * Y_W / RGB_w[2];

    /* Build RGB_to_CAM16 matrix (includes reference_luminance scaling)
     * Order: xyz_to_cam16_rgb @ rgb_to_xyz to get RGB -> CAM16 RGB */
    alwan_f64 rgb_to_cam16_base[9];
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
    alwan_f64 white_cam16[3];
    mult_vec_mat3(white_rgb, p->MATRIX_RGB_to_CAM16_c.m, white_cam16);

    /* Apply post-adaptation compression to white */
    alwan_f64 rgb_a_w[3];
    rgb_a_w[0] = post_adaptation_cone_response_compression_fwd(white_cam16[0]);
    rgb_a_w[1] = post_adaptation_cone_response_compression_fwd(white_cam16[1]);
    rgb_a_w[2] = post_adaptation_cone_response_compression_fwd(white_cam16[2]);

    /* Build cone_response_to_Aab with cam_nl_scale = 400.0 */
    alwan_f64 cone_to_aab[9];
    for (int i = 0; i < 9; i++) {
        cone_to_aab[i] = CAM_NL_SCALE * CONE_TO_AAB_BASE[i];
    }

    /* A_w = first row of cone_to_aab @ rgb_a_w */
    alwan_f64 A_w = cone_to_aab[0] * rgb_a_w[0] + cone_to_aab[1] * rgb_a_w[1] + cone_to_aab[2] * rgb_a_w[2];

    /* Build MATRIX_cone_response_to_Aab:
     * Row 0: divided by A_w (achromatic normalization)
     * Rows 1-2: multiplied by 43 * surround[2] (chromatic scaling)
     */
    alwan_f64 ab_scale = ALWAN_LITERAL(43.0) * ACES2_SURROUND_N_c;

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

    /* Compute and store inverse matrices for Aab_to_RGB_f64 conversion */
    p->MATRIX_CAM16_to_RGB = alwan_mat3_inv_f64_v(p->MATRIX_RGB_to_CAM16_c);
    p->MATRIX_Aab_to_cone = alwan_mat3_inv_f64_v(p->MATRIX_cone_response_to_Aab);
}

/* ----------------------------------------------------------------
 * ACES 2.0: RGB to Aab conversion
 * ---------------------------------------------------------------- */

static void RGB_to_Aab_f64(alwan_f64 const rgb[3], aces2_JMhParams_f64 const *p, alwan_f64 aab[3]) {
    alwan_vec3_f64 in = {{rgb[0], rgb[1], rgb[2]}};
    alwan_vec3_f64 out = aces2_rgb_to_aab_f64_v(in, p);
    aab[0] = out.v[0]; aab[1] = out.v[1]; aab[2] = out.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Aab to JMh conversion
 * J = J_scale * A^cz
 * M = sqrt(a^2 + b^2)
 * h = atan2(b, a) in degrees [0, 360)
 * ---------------------------------------------------------------- */

static void Aab_to_JMh_f64(alwan_f64 const aab[3], aces2_JMhParams_f64 const *p, alwan_f64 jmh[3]) {
    alwan_vec3_f64 in = {{aab[0], aab[1], aab[2]}};
    alwan_vec3_f64 out = aces2_aab_to_jmh_f64_v(in, p);
    jmh[0] = out.v[0]; jmh[1] = out.v[1]; jmh[2] = out.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Inverse cone response compression
 * Ra_lim = min(Ra, 0.99)
 * F_L_Y = cam_nl_offset * Ra_lim / (1 - Ra_lim)
 * Rc = F_L_Y^(1/0.42)
 * ---------------------------------------------------------------- */

static alwan_f64 post_adaptation_cone_response_compression_inv(alwan_f64 Ra) {
    return aces_cone_response_inv_f64_v(Ra);
}

/* ----------------------------------------------------------------
 * ACES 2.0: JMh to Aab conversion (inverse)
 * A = (J / J_scale)^(1/cz)
 * a = M * cos(h_rad)
 * b = M * sin(h_rad)
 * ---------------------------------------------------------------- */

static void JMh_to_Aab_f64(alwan_f64 const jmh[3], aces2_JMhParams_f64 const *p, alwan_f64 aab[3]) {
    alwan_vec3_f64 in = {{jmh[0], jmh[1], jmh[2]}};
    alwan_vec3_f64 out = aces2_jmh_to_aab_f64_v(in, p);
    aab[0] = out.v[0]; aab[1] = out.v[1]; aab[2] = out.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Aab to RGB conversion (inverse)
 * Uses precomputed inverse matrices from aces2_JMhParams_f64
 * ---------------------------------------------------------------- */

static void Aab_to_RGB_f64(alwan_f64 const aab[3], aces2_JMhParams_f64 const *p, alwan_f64 rgb[3]) {
    alwan_vec3_f64 in = {{aab[0], aab[1], aab[2]}};
    alwan_vec3_f64 out = aces2_aab_to_rgb_f64_v(in, p);
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

static alwan_f64 J_to_Y_f64(alwan_f64 J, aces2_JMhParams_f64 const *p) {
    return aces2_j_to_y_f64_v(J, p);
}

static alwan_f64 Y_to_J_f64(alwan_f64 Y, aces2_JMhParams_f64 const *p) {
    return aces2_y_to_j_f64_v(Y, p);
}

/* Forward declarations for use in TonescaleCompress20 */
void alwan_aces_primaries_ap1_default_f64(alwan_aces_primaries_f64 *primaries);

/* ----------------------------------------------------------------
 * ACES 2.0: TonescaleCompress20
 * Reference: ACES CTL Lib.Academy.Tonescale.ctl
 * Full pipeline: RGB -> JMh -> tonescale J -> JMh -> RGB
 * ---------------------------------------------------------------- */

static void init_TSParams_f64(alwan_f64 peak_luminance, aces2_TSParams_f64 *ts) {
    /* Constants from ACES CTL */
    static const alwan_f64 n_r = ALWAN_LITERAL(100.0);
    static const alwan_f64 g = ALWAN_LITERAL(1.15);
    static const alwan_f64 c = ALWAN_LITERAL(0.18);
    static const alwan_f64 c_d = ALWAN_LITERAL(10.013);
    static const alwan_f64 w_g = ALWAN_LITERAL(0.14);
    static const alwan_f64 t_1 = ALWAN_LITERAL(0.04);
    static const alwan_f64 r_hit_min = ALWAN_LITERAL(128.0);
    static const alwan_f64 r_hit_max = ALWAN_LITERAL(896.0);

    ts->n = peak_luminance;
    ts->n_r = n_r;
    ts->g = g;
    ts->t_1 = t_1;

    /* Computed parameters */
    alwan_f64 r_hit = r_hit_min + (r_hit_max - r_hit_min)
                       * (ALWAN_LN(peak_luminance / n_r) / ALWAN_LN(ALWAN_LITERAL(10000.0) / ALWAN_LITERAL(100.0)));

    alwan_f64 m_0 = peak_luminance / n_r;
    alwan_f64 m_1 = ALWAN_LITERAL(0.5) * (m_0 + ALWAN_SQRT(m_0 * (m_0 + ALWAN_LITERAL(4.0) * t_1)));

    alwan_f64 u = ALWAN_POW((r_hit / m_1) / ((r_hit / m_1) + ALWAN_LITERAL(1.0)), g);
    alwan_f64 m = m_1 / u;

    alwan_f64 w_i = ALWAN_LN(peak_luminance / ALWAN_LITERAL(100.0)) / ALWAN_LN(ALWAN_LITERAL(2.0));
    alwan_f64 c_t = c_d / n_r * (ALWAN_LITERAL(1.0) + w_i * w_g);

    alwan_f64 g_ip = ALWAN_LITERAL(0.5) * (c_t + ALWAN_SQRT(c_t * (c_t + ALWAN_LITERAL(4.0) * t_1)));
    alwan_f64 g_ipp2 = -(m_1 * ALWAN_POW(g_ip / m, ALWAN_LITERAL(1.0) / g))
                        / (ALWAN_POW(g_ip / m, ALWAN_LITERAL(1.0) / g) - ALWAN_LITERAL(1.0));

    alwan_f64 w_2 = c / g_ipp2;
    ts->s_2 = w_2 * m_1 * n_r;  /* OCIO: s_2 = w_2 * m_1 * reference_luminance */
    ts->u_2 = ALWAN_POW((r_hit / m_1) / ((r_hit / m_1) + w_2), g);
    ts->m_2 = m_1 / ts->u_2;
}

static alwan_f64 tonescale_fwd(alwan_f64 x, aces2_TSParams_f64 const *ts) {
    return aces2_tonescale_fwd_f64_v(x, ts);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Reach table generation via binary search
 * Finds maximum M (chroma) at limit_J_max for each hue before
 * any RGB channel goes negative
 * ---------------------------------------------------------------- */

/* use_conservative: if 1, store `low` (last valid M, inside AP1 gamut).
 *                   if 0, store `high` (first invalid M, slightly outside AP1 gamut).
 * Chroma compression uses use_conservative=0 (matches OCIO reach table semantics).
 * Gamut compression uses make_reach_m_table_gamut_f32_as_f64 (float32 binary search). */
static void make_reach_m_table_f64(aces2_JMhParams_f64 const *p, alwan_f64 limit_J_max,
                               alwan_f64 reach_table[ACES2_REACH_TABLE_SIZE],
                               int use_conservative) {
    static const alwan_f64 SEARCH_RANGE = ALWAN_LITERAL(50.0);
    static const alwan_f64 SEARCH_MAX = ALWAN_LITERAL(1300.0);
    static const alwan_f64 SEARCH_TOL = ALWAN_LITERAL(0.01);

    for (int i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
        alwan_f64 hue = (alwan_f64)i;  /* Hue in degrees [0, 359] */
        alwan_f64 low = ALWAN_LITERAL(0.0);
        alwan_f64 high = SEARCH_RANGE;

        /* Coarse search: find upper bound where RGB goes negative */
        while (high < SEARCH_MAX) {
            alwan_f64 jmh[3] = {limit_J_max, high, hue};
            alwan_f64 aab[3], rgb[3];
            JMh_to_Aab_f64(jmh, p, aab);
            Aab_to_RGB_f64(aab, p, rgb);

            /* Check if any channel goes negative */
            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) || rgb[2] < ALWAN_LITERAL(0.0)) {
                break;
            }
            low = high;
            high += SEARCH_RANGE;
        }

        /* Binary search refinement */
        while ((high - low) > SEARCH_TOL) {
            alwan_f64 mid = (high + low) * ALWAN_LITERAL(0.5);
            alwan_f64 jmh[3] = {limit_J_max, mid, hue};
            alwan_f64 aab[3], rgb[3];
            JMh_to_Aab_f64(jmh, p, aab);
            Aab_to_RGB_f64(aab, p, rgb);

            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) || rgb[2] < ALWAN_LITERAL(0.0)) {
                high = mid;
            } else {
                low = mid;
            }
        }

        reach_table[i] = use_conservative ? low : high;
    }
}

/* ----------------------------------------------------------------
 * ACES 2.0: Reach table for gamut compression, built with float32 arithmetic
 * OCIO runs the entire ACES 2.0 pipeline in float32. At near-degenerate hues
 * (h~268-270 deg, AP1 ~ Rec.2020/Rec.709 blue primary), float32 matrix rounding in
 * JMh->Aab->RGB causes the critical channel to go negative at a lower M than float64.
 * This gives reach_m_f32 <= gamut_boundary_M (computed from cusp table, also ~ the
 * AP1/Rec.2020 boundary), so proportion = gamut/reach >= 1.0 -> SKIP -- matching OCIO.
 * For non-degenerate hues, f32 and f64 reach values agree to sub-LSB in 8-bit output.
 *
 * The float32 JMhParams are built by truncating the float64 values, which closely
 * matches OCIO's direct float32 initialization (same published ACES constants). */
static void make_reach_m_table_gamut_f32_as_f64(
        aces2_JMhParams_f64 const *p_f64,
        alwan_f64 limit_J_max,
        alwan_f64 reach_table[ACES2_REACH_TABLE_SIZE]) {
    aces2_JMhParams_f32 p;
    int i;
    for (i = 0; i < 9; i++) {
        p.MATRIX_RGB_to_CAM16_c.m[i]       = (float)p_f64->MATRIX_RGB_to_CAM16_c.m[i];
        p.MATRIX_cone_response_to_Aab.m[i] = (float)p_f64->MATRIX_cone_response_to_Aab.m[i];
        p.MATRIX_CAM16_to_RGB.m[i]         = (float)p_f64->MATRIX_CAM16_to_RGB.m[i];
        p.MATRIX_Aab_to_cone.m[i]          = (float)p_f64->MATRIX_Aab_to_cone.m[i];
    }
    p.cz        = (float)p_f64->cz;
    p.inv_cz    = (float)p_f64->inv_cz;
    p.A_w_J     = (float)p_f64->A_w_J;
    p.inv_A_w_J = (float)p_f64->inv_A_w_J;
    p.F_L_n     = (float)p_f64->F_L_n;

    float limit_J = (float)limit_J_max;

    for (i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
        float hue = (float)i;
        float lo = 0.0f;
        float hi = 50.0f;

        while (hi < 1300.0f) {
            alwan_vec3_f32 jmh = {{limit_J, hi, hue}};
            alwan_vec3_f32 aab = aces2_jmh_to_aab_f32_v(jmh, &p);
            alwan_vec3_f32 rgb = aces2_aab_to_rgb_f32_v(aab, &p);
            if (rgb.v[0] < 0.0f || rgb.v[1] < 0.0f || rgb.v[2] < 0.0f) break;
            lo = hi;
            hi += 50.0f;
        }

        while ((hi - lo) > 0.01f) {
            float mid = (hi + lo) * 0.5f;
            alwan_vec3_f32 jmh = {{limit_J, mid, hue}};
            alwan_vec3_f32 aab = aces2_jmh_to_aab_f32_v(jmh, &p);
            alwan_vec3_f32 rgb = aces2_aab_to_rgb_f32_v(aab, &p);
            if (rgb.v[0] < 0.0f || rgb.v[1] < 0.0f || rgb.v[2] < 0.0f)
                hi = mid;
            else
                lo = mid;
        }

        reach_table[i] = (alwan_f64)hi;  /* Store hi (first-invalid in f32), matching OCIO */
    }
}

/* ----------------------------------------------------------------
 * ACES 2.0: Initialize chroma compression parameters
 * Reference: OCIO Transform.cpp init_ChromaCompressParams_f64
 * ---------------------------------------------------------------- */

static void init_ChromaCompressParams_f64(alwan_f64 peak_luminance,
                                       aces2_JMhParams_f64 const *jmh_params,
                                       ChromaCompressParams_f64 *cp) {
    static const alwan_f64 n_r = ALWAN_LITERAL(100.0);

    /* Compute log_peak = ALWAN_LOG10_F64(peak_luminance / n_r) */
    alwan_f64 log_peak = ALWAN_LN(peak_luminance / n_r) / ALWAN_LN(ALWAN_LITERAL(10.0));

    /* compr = chroma_compress + (chroma_compress * chroma_compress_fact) * log_peak */
    cp->compr = ACES2_CHROMA_COMPRESS
              + (ACES2_CHROMA_COMPRESS * ACES2_CHROMA_COMPRESS_FACT) * log_peak;

    /* sat = max(0.2, chroma_expand - (chroma_expand * chroma_expand_fact) * log_peak) */
    alwan_f64 sat_val = ACES2_CHROMA_EXPAND
                         - (ACES2_CHROMA_EXPAND * ACES2_CHROMA_EXPAND_FACT) * log_peak;
    cp->sat = (sat_val > ALWAN_LITERAL(0.2)) ? sat_val : ALWAN_LITERAL(0.2);

    /* sat_thr = chroma_expand_thr / peak_luminance */
    cp->sat_thr = ACES2_CHROMA_EXPAND_THR / peak_luminance;

    /* chroma_compress_scale = ALWAN_POW_F64(0.03379 * peak_luminance, 0.30596) - 0.45135 */
    cp->chroma_compress_scale = ALWAN_POW(ALWAN_LITERAL(0.03379) * peak_luminance,
                                           ALWAN_LITERAL(0.30596))
                              - ALWAN_LITERAL(0.45135);

    /* limit_J_max = Y_to_J_f64(peak_luminance) */
    cp->limit_J_max = Y_to_J_f64(peak_luminance, jmh_params);

    /* model_gamma_inv = 1/cz */
    cp->model_gamma_inv = jmh_params->inv_cz;

    /* Generate reach table via binary search (use_conservative=0 matches OCIO semantics) */
    make_reach_m_table_f64(jmh_params, cp->limit_J_max, cp->reach_m_table, 0);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Lookup reach_m for a given hue with linear interpolation
 * ---------------------------------------------------------------- */

static alwan_f64 lookup_reach_m_f64(alwan_f64 h_deg, ChromaCompressParams_f64 const *cp) {
    /* Wrap hue to [0, 360). Guard NaN/+-Inf/huge first: the naive while-subtract
     * never terminates for +Inf or for |h| so large that h - 360 == h. */
    if (!(h_deg > ALWAN_LITERAL(-1.0e6) && h_deg < ALWAN_LITERAL(1.0e6))) h_deg = ALWAN_LITERAL(0.0);
    while (h_deg < ALWAN_LITERAL(0.0)) h_deg += ALWAN_LITERAL(360.0);
    while (h_deg >= ALWAN_LITERAL(360.0)) h_deg -= ALWAN_LITERAL(360.0);

    /* Linear interpolation between table entries */
    int idx0 = (int)h_deg;
    int idx1 = (idx0 + 1) % ACES2_REACH_TABLE_SIZE;
    alwan_f64 t = h_deg - (alwan_f64)idx0;

    return cp->reach_m_table[idx0] * (ALWAN_LITERAL(1.0) - t)
         + cp->reach_m_table[idx1] * t;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Chroma compression forward
 * Reference: OCIO Transform.cpp chroma_compress_fwd_f64
 * ---------------------------------------------------------------- */

static alwan_f64 chroma_compress_fwd_f64(alwan_f64 J, alwan_f64 M, alwan_f64 h_deg,
                                         alwan_f64 J_ts,
                                         ChromaCompressParams_f64 const *cp) {
    /* Handle edge cases */
    if (J <= ALWAN_LITERAL(0.0) || M <= ALWAN_LITERAL(0.0)) {
        return ALWAN_LITERAL(0.0);
    }

    /* Convert hue to radians for Mnorm calculation */
    alwan_f64 h_rad = h_deg * ALWAN_LITERAL(3.14159265358979323846) / ALWAN_LITERAL(180.0);

    /* Compute Mnorm using Fourier series */
    alwan_f64 Mnorm = chroma_compress_norm(h_rad, cp->chroma_compress_scale);

    /* Scale M by (J_ts/J)^model_gamma_inv */
    alwan_f64 M_cp = M * ALWAN_POW(J_ts / J, cp->model_gamma_inv);

    /* Normalize by Mnorm */
    M_cp = M_cp / Mnorm;

    /* Normalized J */
    alwan_f64 nJ = J_ts / cp->limit_J_max;
    alwan_f64 snJ = (ALWAN_LITERAL(1.0) - nJ > ALWAN_LITERAL(0.0))
                     ? (ALWAN_LITERAL(1.0) - nJ) : ALWAN_LITERAL(0.0);

    /* Look up reach for this hue */
    alwan_f64 reachMaxM = lookup_reach_m_f64(h_deg, cp);

    /* Compute limit for toe functions */
    alwan_f64 limit = ALWAN_POW(nJ, cp->model_gamma_inv) * reachMaxM / Mnorm;

    /* Saturation toe (lower bound compression) */
    alwan_f64 sat_limit = limit - ALWAN_LITERAL(0.001);
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

/* ----------------------------------------------------------------
 * ACES 2.0: f32-precision initialization helpers
 * Each function computes in f64, then downcasts to f32 structs.
 * This matches OCIO's float32 pipeline while maintaining numerical
 * equivalence for setup math.
 * ---------------------------------------------------------------- */

static void init_JMhParams_f32(alwan_aces_primaries_f32 const *primaries, aces2_JMhParams_f32 *p) {
    aces2_JMhParams_f64 p64;
    int i;
    /* Upcast f32 primaries to f64 for internal computation */
    alwan_aces_primaries_f64 prim64;
    prim64.red_x = primaries->red_x; prim64.red_y = primaries->red_y;
    prim64.green_x = primaries->green_x; prim64.green_y = primaries->green_y;
    prim64.blue_x = primaries->blue_x; prim64.blue_y = primaries->blue_y;
    prim64.white_x = primaries->white_x; prim64.white_y = primaries->white_y;
    init_JMhParams_f64(&prim64, &p64);
    for (i = 0; i < 9; i++) {
        p->MATRIX_RGB_to_CAM16_c.m[i]       = (float)p64.MATRIX_RGB_to_CAM16_c.m[i];
        p->MATRIX_cone_response_to_Aab.m[i] = (float)p64.MATRIX_cone_response_to_Aab.m[i];
        p->MATRIX_CAM16_to_RGB.m[i]         = (float)p64.MATRIX_CAM16_to_RGB.m[i];
        p->MATRIX_Aab_to_cone.m[i]          = (float)p64.MATRIX_Aab_to_cone.m[i];
    }
    p->cz        = (float)p64.cz;
    p->inv_cz    = (float)p64.inv_cz;
    p->A_w_J     = (float)p64.A_w_J;
    p->inv_A_w_J = (float)p64.inv_A_w_J;
    p->F_L_n     = (float)p64.F_L_n;
}

static void init_TSParams_f32(float peak_luminance, aces2_TSParams_f32 *ts) {
    aces2_TSParams_f64 ts64;
    init_TSParams_f64((alwan_f64)peak_luminance, &ts64);
    ts->n   = (float)ts64.n;
    ts->n_r = (float)ts64.n_r;
    ts->g   = (float)ts64.g;
    ts->t_1 = (float)ts64.t_1;
    ts->s_2 = (float)ts64.s_2;
    ts->u_2 = (float)ts64.u_2;
    ts->m_2 = (float)ts64.m_2;
}

/* f32 binary search for chroma-compress reach table -- mirrors
 * make_reach_m_table_gamut_f32_as_f64 but stores float results. */
static void make_reach_m_table_chroma_f32(aces2_JMhParams_f32 const *p, float limit_J,
                                           float reach_table[ACES2_REACH_TABLE_SIZE]) {
    int i;
    for (i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
        float hue = (float)i;
        float lo = 0.0f, hi = 50.0f;
        while (hi < 1300.0f) {
            alwan_vec3_f32 jmh = {{limit_J, hi, hue}};
            alwan_vec3_f32 aab = aces2_jmh_to_aab_f32_v(jmh, p);
            alwan_vec3_f32 rgb = aces2_aab_to_rgb_f32_v(aab, p);
            if (rgb.v[0] < 0.0f || rgb.v[1] < 0.0f || rgb.v[2] < 0.0f) break;
            lo = hi; hi += 50.0f;
        }
        while ((hi - lo) > 0.01f) {
            float mid = (hi + lo) * 0.5f;
            alwan_vec3_f32 jmh = {{limit_J, mid, hue}};
            alwan_vec3_f32 aab = aces2_jmh_to_aab_f32_v(jmh, p);
            alwan_vec3_f32 rgb = aces2_aab_to_rgb_f32_v(aab, p);
            if (rgb.v[0] < 0.0f || rgb.v[1] < 0.0f || rgb.v[2] < 0.0f) hi = mid;
            else lo = mid;
        }
        reach_table[i] = hi;
    }
}

static void init_ChromaCompressParams_f32(float peak_luminance,
                                           aces2_JMhParams_f32 const *jmh_params,
                                           ChromaCompressParams_f32 *cp) {
    /* Scalar params: compute in f64, downcast for accuracy */
    alwan_f64 pl = (alwan_f64)peak_luminance;
    alwan_f64 n_r = ALWAN_LITERAL(100.0);
    alwan_f64 log_peak = ALWAN_LN(pl / n_r) / ALWAN_LN(ALWAN_LITERAL(10.0));

    cp->compr = (float)(ACES2_CHROMA_COMPRESS
              + (ACES2_CHROMA_COMPRESS * ACES2_CHROMA_COMPRESS_FACT) * log_peak);

    alwan_f64 sat_val = ACES2_CHROMA_EXPAND
                      - (ACES2_CHROMA_EXPAND * ACES2_CHROMA_EXPAND_FACT) * log_peak;
    cp->sat = (float)((sat_val > ALWAN_LITERAL(0.2)) ? sat_val : ALWAN_LITERAL(0.2));

    cp->sat_thr = (float)(ACES2_CHROMA_EXPAND_THR / pl);
    cp->chroma_compress_scale = (float)(ALWAN_POW(ALWAN_LITERAL(0.03379) * pl,
                                                   ALWAN_LITERAL(0.30596))
                                      - ALWAN_LITERAL(0.45135));

    /* limit_J_max and model_gamma_inv: derive from f32 JMhParams for consistency */
    cp->limit_J_max    = aces2_y_to_j_f32_v(peak_luminance, jmh_params);
    cp->model_gamma_inv = jmh_params->inv_cz;

    /* Reach table: computed in f32 precision to match OCIO's float32 pipeline */
    make_reach_m_table_chroma_f32(jmh_params, cp->limit_J_max, cp->reach_m_table);
}

void alwan_aces_tonescale_compress20_f64(alwan_rgb_f64 *rgb_out,
                                     alwan_rgb_f64 const *rgb_in,
                                     alwan_f64 peak_luminance) {
    if (!rgb_out || !rgb_in) return;
    if (peak_luminance <= ALWAN_LITERAL(0.0)) return;

    /* Initialize JMh parameters (using AP1 primaries) */
    alwan_aces_primaries_f64 primaries;
    alwan_aces_primaries_ap1_default_f64(&primaries);
    aces2_JMhParams_f64 jmh_params;
    init_JMhParams_f64(&primaries, &jmh_params);

    /* Initialize tonescale parameters */
    aces2_TSParams_f64 ts;
    init_TSParams_f64(peak_luminance, &ts);

    /* Initialize chroma compression parameters */
    ChromaCompressParams_f64 chroma_params;
    init_ChromaCompressParams_f64(peak_luminance, &jmh_params, &chroma_params);

    /* Step 1: Convert RGB to Aab */
    alwan_f64 rgb[3] = {rgb_in->r, rgb_in->g, rgb_in->b};
    alwan_f64 aab[3];
    RGB_to_Aab_f64(rgb, &jmh_params, aab);

    /* Step 2: Convert Aab to JMh */
    alwan_f64 jmh[3];
    Aab_to_JMh_f64(aab, &jmh_params, jmh);

    /* Step 3: Convert J to Y, apply tonescale, convert back to J */
    alwan_f64 Y_in = J_to_Y_f64(jmh[0], &jmh_params);
    alwan_f64 Y_out = tonescale_fwd(Y_in, &ts);
    alwan_f64 J_ts = Y_to_J_f64(Y_out, &jmh_params);

    /* Step 4: Apply chroma compression */
    alwan_f64 M_out = chroma_compress_fwd_f64(jmh[0], jmh[1], jmh[2],
                                              J_ts, &chroma_params);

    /* Step 5: Build output JMh */
    alwan_f64 jmh_out[3] = {J_ts, M_out, jmh[2]};

    /* Step 6: Convert JMh back to Aab */
    alwan_f64 aab_out[3];
    JMh_to_Aab_f64(jmh_out, &jmh_params, aab_out);

    /* Step 7: Convert Aab back to RGB */
    alwan_f64 rgb_result[3];
    Aab_to_RGB_f64(aab_out, &jmh_params, rgb_result);

    rgb_out->r = rgb_result[0];
    rgb_out->g = rgb_result[1];
    rgb_out->b = rgb_result[2];
}

void alwan_aces_tonescale_compress20_f32(alwan_rgb_f32 *rgb_out,
                                     alwan_rgb_f32 const *rgb_in,
                                     alwan_f32 peak_luminance) {
    if (!rgb_out || !rgb_in) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_tonescale_compress20_f64(&out64, &in64, (double)peak_luminance);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
}

/* ----------------------------------------------------------------
 * ACES 2.0: RGB to JMh
 * Reference: ACES CTL Lib.Academy.OutputTransform.ctl
 * ---------------------------------------------------------------- */

void alwan_aces_primaries_ap1_default_f32(alwan_aces_primaries_f32 *primaries) {
    if (!primaries) return;
    primaries->red_x   = (float)AP1_RED_x;
    primaries->red_y   = (float)AP1_RED_y;
    primaries->green_x = (float)AP1_GREEN_x;
    primaries->green_y = (float)AP1_GREEN_y;
    primaries->blue_x  = (float)AP1_BLUE_x;
    primaries->blue_y  = (float)AP1_BLUE_y;
    primaries->white_x = (float)AP1_WHITE_x;
    primaries->white_y = (float)AP1_WHITE_y;
}

void alwan_aces_primaries_ap1_default_f64(alwan_aces_primaries_f64 *primaries) {
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

void alwan_aces_rgb_to_jmh20_f64(alwan_vec3_f64 *jmh_out,
                            alwan_rgb_f64 const *rgb_in,
                            alwan_aces_primaries_f64 const *primaries) {
    if (!jmh_out || !rgb_in || !primaries) return;

    /* Initialize JMh conversion parameters */
    aces2_JMhParams_f64 p;
    init_JMhParams_f64(primaries, &p);

    /* Convert RGB to Aab */
    alwan_f64 rgb[3] = {rgb_in->r, rgb_in->g, rgb_in->b};
    alwan_f64 aab[3];
    RGB_to_Aab_f64(rgb, &p, aab);

    /* Convert Aab to JMh */
    alwan_f64 jmh[3];
    Aab_to_JMh_f64(aab, &p, jmh);

    jmh_out->v[0] = jmh[0];
    jmh_out->v[1] = jmh[1];
    jmh_out->v[2] = jmh[2];
}

void alwan_aces_rgb_to_jmh20_f32(alwan_vec3_f32 *jmh_out,
                            alwan_rgb_f32 const *rgb_in,
                            alwan_aces_primaries_f32 const *primaries) {
    if (!jmh_out || !rgb_in || !primaries) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_aces_primaries_f64 p64;
    p64.red_x = primaries->red_x; p64.red_y = primaries->red_y;
    p64.green_x = primaries->green_x; p64.green_y = primaries->green_y;
    p64.blue_x = primaries->blue_x; p64.blue_y = primaries->blue_y;
    p64.white_x = primaries->white_x; p64.white_y = primaries->white_y;
    alwan_vec3_f64 out64;
    alwan_aces_rgb_to_jmh20_f64(&out64, &in64, &p64);
    jmh_out->v[0] = (float)out64.v[0]; jmh_out->v[1] = (float)out64.v[1]; jmh_out->v[2] = (float)out64.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: JMh to RGB (inverse of RGB to JMh)
 * ---------------------------------------------------------------- */

void alwan_aces_jmh_to_rgb20_f64(alwan_rgb_f64 *rgb_out,
                            alwan_vec3_f64 const *jmh_in,
                            alwan_aces_primaries_f64 const *primaries) {
    if (!rgb_out || !jmh_in || !primaries) return;

    /* Initialize JMh conversion parameters */
    aces2_JMhParams_f64 p;
    init_JMhParams_f64(primaries, &p);

    /* Convert JMh to Aab */
    alwan_f64 jmh[3] = {jmh_in->v[0], jmh_in->v[1], jmh_in->v[2]};
    alwan_f64 aab[3];
    JMh_to_Aab_f64(jmh, &p, aab);

    /* Convert Aab to RGB */
    alwan_f64 rgb[3];
    Aab_to_RGB_f64(aab, &p, rgb);

    rgb_out->r = rgb[0];
    rgb_out->g = rgb[1];
    rgb_out->b = rgb[2];
}

void alwan_aces_jmh_to_rgb20_f32(alwan_rgb_f32 *rgb_out,
                            alwan_vec3_f32 const *jmh_in,
                            alwan_aces_primaries_f32 const *primaries) {
    if (!rgb_out || !jmh_in || !primaries) return;
    alwan_vec3_f64 in64 = {{(alwan_f64)jmh_in->v[0], (alwan_f64)jmh_in->v[1], (alwan_f64)jmh_in->v[2]}};
    alwan_aces_primaries_f64 p64;
    p64.red_x = primaries->red_x; p64.red_y = primaries->red_y;
    p64.green_x = primaries->green_x; p64.green_y = primaries->green_y;
    p64.blue_x = primaries->blue_x; p64.blue_y = primaries->blue_y;
    p64.white_x = primaries->white_x; p64.white_y = primaries->white_y;
    alwan_rgb_f64 out64;
    alwan_aces_jmh_to_rgb20_f64(&out64, &in64, &p64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression Constants (from OCIO Common.h)
 * Reference: OpenColorIO/src/OpenColorIO/ops/fixedfunction/ACES2/Common.h
 * ---------------------------------------------------------------- */

static const alwan_f64 GAMUT_COMPRESSION_THRESHOLD = ACES_GAMUT_COMPRESSION_THRESHOLD_VALUE;
static const alwan_f64 GAMUT_SMOOTH_CUSPS = ACES_GAMUT_SMOOTH_CUSPS_VALUE;
static const alwan_f64 GAMUT_FOCUS_GAIN_BLEND = ALWAN_LITERAL(0.3);
static const alwan_f64 GAMUT_CUSP_MID_BLEND = ACES_GAMUT_CUSP_MID_BLEND_VALUE;
static const alwan_f64 GAMUT_FOCUS_DISTANCE = ALWAN_LITERAL(1.35);
static const alwan_f64 GAMUT_FOCUS_ADJUST_GAIN_INV = ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.55);

/* Number of gamut corners (R, Y, G, C, B, M) */
#define ACES2_CUSP_CORNER_COUNT 6

/* Lower hull gamma (constant across all hues) */
static const alwan_f64 GAMUT_LOWER_HULL_GAMMA = ALWAN_LITERAL(1.14);

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression Parameter Structures
 * ---------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * ACES 2.0: Smooth minimum function (smin)
 * Creates smooth transition between two boundaries
 * ---------------------------------------------------------------- */

static alwan_f64 smin_scaled(alwan_f64 a, alwan_f64 b, alwan_f64 cusp_M) {
    return aces2_smin_scaled_f64_v(a, b, cusp_M);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Reinhard remapping for M compression
 * ---------------------------------------------------------------- */

static alwan_f64 reinhard_fwd(alwan_f64 x) {
    return aces2_reinhard_fwd_f64_v(x);
}

static alwan_f64 reinhard_inv(alwan_f64 x) {
    return aces2_reinhard_inv_f64_v(x);
}

static alwan_f64 remap_M_fwd(alwan_f64 M, alwan_f64 gamut_boundary_M,
                                 alwan_f64 reach_boundary_M) {
    return aces2_remap_m_fwd_f64_v(M, gamut_boundary_M, reach_boundary_M);
}

static alwan_f64 remap_M_inv(alwan_f64 M, alwan_f64 gamut_boundary_M,
                                 alwan_f64 reach_boundary_M) {
    return aces2_remap_m_inv_f64_v(M, gamut_boundary_M, reach_boundary_M);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Focus geometry computations
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

static alwan_f64 compute_focus_J(alwan_f64 cusp_J, alwan_f64 mid_J,
                                     alwan_f64 limit_J_max) {
    return aces2_compute_focus_j_f64_v(cusp_J, mid_J, limit_J_max);
}

static alwan_f64 get_focus_gain(alwan_f64 J, alwan_f64 analytical_threshold,
                                    alwan_f64 limit_J_max, alwan_f64 focus_dist) {
    return aces2_get_focus_gain_f64_v(J, analytical_threshold, limit_J_max, focus_dist);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Solve J intersection for compression vector
 * ---------------------------------------------------------------- */

static alwan_f64 solve_J_intersect(alwan_f64 J, alwan_f64 M,
                                       alwan_f64 focus_J, alwan_f64 max_J,
                                       alwan_f64 slope_gain) {
    return aces2_solve_j_intersect_f64_v(J, M, focus_J, max_J, slope_gain);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Compute compression vector slope
 * ---------------------------------------------------------------- */

static alwan_f64 compute_compression_vector_slope(alwan_f64 J_intersect,
                                                      alwan_f64 focus_J,
                                                      alwan_f64 limit_J_max,
                                                      alwan_f64 slope_gain) {
    return aces2_compression_vector_slope_f64_v(J_intersect, focus_J, limit_J_max, slope_gain);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Estimate line-boundary intersection M
 * Uses gamma parameterization of gamut hull
 * ---------------------------------------------------------------- */

static alwan_f64 estimate_line_boundary_M(alwan_f64 J_intersect_source,
                                              alwan_f64 slope,
                                              alwan_f64 gamma_inv,
                                              alwan_f64 cusp_J,
                                              alwan_f64 cusp_M,
                                              alwan_f64 J_intersect_cusp) {
    return aces2_estimate_line_boundary_m_f64_v(J_intersect_source, slope, gamma_inv, cusp_J, cusp_M, J_intersect_cusp);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Find gamut boundary intersection
 * Combines lower and upper hull boundaries with smooth blending
 * ---------------------------------------------------------------- */

static alwan_f64 find_gamut_boundary_intersection(alwan_f64 cusp_J,
                                                      alwan_f64 cusp_M,
                                                      alwan_f64 limit_J_max,
                                                      alwan_f64 gamma_top_inv,
                                                      alwan_f64 gamma_bottom_inv,
                                                      alwan_f64 J_intersect_source,
                                                      alwan_f64 slope,
                                                      alwan_f64 J_intersect_cusp) {
    return aces2_find_gamut_boundary_f64_v(cusp_J, cusp_M, limit_J_max, gamma_top_inv, gamma_bottom_inv, J_intersect_source, slope, J_intersect_cusp);
}

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * ACES 2.0: Build cusp table for limit primaries
 * Finds the gamut boundary (max M) at each hue degree
 * ---------------------------------------------------------------- */

static void build_cusp_table_for_hue_f64(alwan_f64 hue_deg, aces2_JMhParams_f64 const *p,
                                      alwan_f64 limit_J_max, alwan_f64 lum_limit,
                                      aces2_GamutCuspEntry_f64 *cusp) {
    /* Binary search for maximum M at this hue while staying in-gamut.
     * lum_limit = peak_luminance / 100.0: the max valid display channel value
     * (1.0 for SDR 100 nit, 10.0 for HDR 1000 nit). */
    static const alwan_f64 SEARCH_RANGE = ALWAN_LITERAL(50.0);
    static const alwan_f64 SEARCH_MAX = ALWAN_LITERAL(500.0);
    static const alwan_f64 SEARCH_TOL = ALWAN_LITERAL(0.001);

    /* Find J at cusp by searching for max M */
    alwan_f64 best_M = ALWAN_LITERAL(0.0);
    alwan_f64 best_J = ALWAN_LITERAL(0.0);

    /* Sample J values to find approximate cusp location (step=1 for precision) */
    for (alwan_f64 J_sample = ALWAN_LITERAL(5.0);
         J_sample < limit_J_max - ALWAN_LITERAL(0.5);
         J_sample += ALWAN_LITERAL(1.0)) {

        /* Binary search for max M at this J */
        alwan_f64 low = ALWAN_LITERAL(0.0);
        alwan_f64 high = SEARCH_RANGE;

        while (high < SEARCH_MAX) {
            alwan_f64 jmh[3] = {J_sample, high, hue_deg};
            alwan_f64 aab[3], rgb[3];
            JMh_to_Aab_f64(jmh, p, aab);
            Aab_to_RGB_f64(aab, p, rgb);

            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) ||
                rgb[2] < ALWAN_LITERAL(0.0) || rgb[0] > lum_limit ||
                rgb[1] > lum_limit || rgb[2] > lum_limit) {
                break;
            }
            low = high;
            high += SEARCH_RANGE;
        }

        /* Refine with binary search */
        while ((high - low) > SEARCH_TOL) {
            alwan_f64 mid = (high + low) * ALWAN_LITERAL(0.5);
            alwan_f64 jmh[3] = {J_sample, mid, hue_deg};
            alwan_f64 aab[3], rgb[3];
            JMh_to_Aab_f64(jmh, p, aab);
            Aab_to_RGB_f64(aab, p, rgb);

            if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) ||
                rgb[2] < ALWAN_LITERAL(0.0) || rgb[0] > lum_limit ||
                rgb[1] > lum_limit || rgb[2] > lum_limit) {
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
    alwan_f64 J_low = best_J - ALWAN_LITERAL(2.0);
    alwan_f64 J_high = best_J + ALWAN_LITERAL(2.0);
    if (J_low < ALWAN_LITERAL(1.0)) J_low = ALWAN_LITERAL(1.0);
    if (J_high > limit_J_max - ALWAN_LITERAL(0.5)) J_high = limit_J_max - ALWAN_LITERAL(0.5);

    while ((J_high - J_low) > ALWAN_LITERAL(0.01)) {
        alwan_f64 J_mid1 = J_low + (J_high - J_low) * ALWAN_LITERAL(0.33);
        alwan_f64 J_mid2 = J_low + (J_high - J_low) * ALWAN_LITERAL(0.67);

        /* Get M at each J */
        alwan_f64 M1 = ALWAN_LITERAL(0.0), M2 = ALWAN_LITERAL(0.0);
        for (int iter = 0; iter < 2; iter++) {
            alwan_f64 J_test = (iter == 0) ? J_mid1 : J_mid2;
            alwan_f64 low_m = ALWAN_LITERAL(0.0);
            alwan_f64 high_m = best_M + ALWAN_LITERAL(50.0);

            while ((high_m - low_m) > SEARCH_TOL) {
                alwan_f64 mid_m = (high_m + low_m) * ALWAN_LITERAL(0.5);
                alwan_f64 jmh[3] = {J_test, mid_m, hue_deg};
                alwan_f64 aab[3], rgb[3];
                JMh_to_Aab_f64(jmh, p, aab);
                Aab_to_RGB_f64(aab, p, rgb);

                if (rgb[0] < ALWAN_LITERAL(0.0) || rgb[1] < ALWAN_LITERAL(0.0) ||
                    rgb[2] < ALWAN_LITERAL(0.0) || rgb[0] > lum_limit ||
                    rgb[1] > lum_limit || rgb[2] > lum_limit) {
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
    /* OCIO stores M * (1 + smooth_m * smooth_cusps) in the cusp table, not raw M */
    cusp->M = best_M * (1.0 + ACES_GAMUT_SMOOTH_M_VALUE * ACES_GAMUT_SMOOTH_CUSPS_VALUE);
    /* gamma_top_inv: overwritten by make_upper_hull_gamma binary search if available */
    alwan_f64 J_ratio = best_J / limit_J_max;
    cusp->gamma_top_inv = ALWAN_LITERAL(1.0) / (ALWAN_LITERAL(1.0) + J_ratio);
}

/* ----------------------------------------------------------------
 * ACES 2.0: Compute upper hull gamma per hue (binary search, matches OCIO)
 * Source: OCIO Transform.cpp make_upper_hull_gamma
 * ---------------------------------------------------------------- */

static alwan_f64 compute_focus_J(alwan_f64 cusp_J, alwan_f64 mid_J, alwan_f64 limit_J_max);

static void make_upper_hull_gamma_f64(aces2_GamutCompressParams_f64 *gcp,
                                       alwan_f64 peak_luminance,
                                       alwan_f64 mid_J,
                                       alwan_f64 focus_dist,
                                       aces2_JMhParams_f64 const *limit_params) {
    static const alwan_f64 GAMMA_MIN = ALWAN_LITERAL(0.0);
    static const alwan_f64 GAMMA_MAX = ALWAN_LITERAL(5.0);
    static const alwan_f64 GAMMA_STEP = ALWAN_LITERAL(0.4);
    static const alwan_f64 GAMMA_ACC = ALWAN_LITERAL(1e-5);
    static const int NTEST = 5;
    static const alwan_f64 test_pos[5] = {0.01, 0.1, 0.5, 0.8, 0.99};

    alwan_f64 lum_limit = peak_luminance / ALWAN_LITERAL(100.0);

    for (int i = 0; i < 360; i++) {
        alwan_f64 hue = (alwan_f64)i;
        alwan_f64 cusp_J = gcp->cusp_table[i + 1].J;
        alwan_f64 cusp_M = gcp->cusp_table[i + 1].M;

        alwan_f64 focus_J = compute_focus_J(cusp_J, mid_J, gcp->limit_J_max);
        alwan_f64 analytical_threshold = cusp_J +
            ALWAN_LITERAL(0.3) * (gcp->limit_J_max - cusp_J);

        /* Generate test data: 5 positions along the upper boundary */
        alwan_f64 test_J_int[5], test_slope[5], test_J_int_cusp[5], test_h[5];
        for (int t = 0; t < NTEST; t++) {
            alwan_f64 testJ = cusp_J + test_pos[t] * (gcp->limit_J_max - cusp_J);
            alwan_f64 sg = aces2_get_focus_gain_f64_v(testJ, analytical_threshold,
                                                       gcp->limit_J_max, focus_dist);
            test_J_int[t] = aces2_solve_j_intersect_f64_v(testJ, cusp_M, focus_J,
                                                           gcp->limit_J_max, sg);
            test_slope[t] = aces2_compression_vector_slope_f64_v(
                test_J_int[t], focus_J, gcp->limit_J_max, sg);
            test_J_int_cusp[t] = aces2_solve_j_intersect_f64_v(
                cusp_J, cusp_M, focus_J, gcp->limit_J_max, sg);
            test_h[t] = hue;
        }

        /* Binary search for gamma */
        alwan_f64 low = GAMMA_MIN;
        alwan_f64 high = low + GAMMA_STEP;
        int outside = 0;

        while (!outside && high < GAMMA_MAX) {
            alwan_f64 gamma_inv = ALWAN_LITERAL(1.0) / high;
            int all_outside = 1;
            for (int t = 0; t < NTEST && all_outside; t++) {
                alwan_f64 approxM = aces2_find_gamut_boundary_f64_v(
                    cusp_J, cusp_M, gcp->limit_J_max,
                    gamma_inv, gcp->lower_hull_gamma_inv,
                    test_J_int[t], test_slope[t], test_J_int_cusp[t]);
                alwan_f64 approxJ = test_J_int[t] + test_slope[t] * approxM;
                alwan_f64 jmh[3] = {approxJ, approxM, hue};
                alwan_f64 aab[3], rgb[3];
                JMh_to_Aab_f64(jmh, limit_params, aab);
                Aab_to_RGB_f64(aab, limit_params, rgb);
                /* outside_hull: any component >= lum_limit (>= matches OCIO for HDR) */
                if (!(rgb[0] >= lum_limit || rgb[1] >= lum_limit || rgb[2] >= lum_limit)) {
                    all_outside = 0;
                }
            }
            if (all_outside) {
                outside = 1;
            } else {
                low = high;
                high += GAMMA_STEP;
            }
        }

        /* Refine with binary search */
        while ((high - low) > GAMMA_ACC) {
            alwan_f64 mid = (high + low) * ALWAN_LITERAL(0.5);
            alwan_f64 gamma_inv = ALWAN_LITERAL(1.0) / mid;
            int all_outside = 1;
            for (int t = 0; t < NTEST && all_outside; t++) {
                alwan_f64 approxM = aces2_find_gamut_boundary_f64_v(
                    cusp_J, cusp_M, gcp->limit_J_max,
                    gamma_inv, gcp->lower_hull_gamma_inv,
                    test_J_int[t], test_slope[t], test_J_int_cusp[t]);
                alwan_f64 approxJ = test_J_int[t] + test_slope[t] * approxM;
                alwan_f64 jmh[3] = {approxJ, approxM, hue};
                alwan_f64 aab[3], rgb[3];
                JMh_to_Aab_f64(jmh, limit_params, aab);
                Aab_to_RGB_f64(aab, limit_params, rgb);
                if (!(rgb[0] >= lum_limit || rgb[1] >= lum_limit || rgb[2] >= lum_limit)) {
                    all_outside = 0;
                }
            }
            if (all_outside) high = mid;
            else low = mid;
        }

        gcp->cusp_table[i + 1].gamma_top_inv = ALWAN_LITERAL(1.0) / high;
    }

    /* Wrap-around */
    gcp->cusp_table[0].gamma_top_inv = gcp->cusp_table[360].gamma_top_inv;
    gcp->cusp_table[361].gamma_top_inv = gcp->cusp_table[1].gamma_top_inv;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Initialize gamut compression parameters
 * ---------------------------------------------------------------- */

/* Building the gamut-compression parameters is expensive: it constructs the
 * 360-hue cusp and upper-hull-gamma tables via per-hue binary search (~1.26M
 * JMh<->RGB conversions). The result depends ONLY on (peak_luminance,
 * limit_primaries, jmh_params), so a single-entry cache lets consecutive calls
 * with the same configuration -- the common case: a whole image / many pixels
 * through one output preset, and the back-to-back scalar+map per-pixel paths --
 * reuse the tables instead of rebuilding them every call. Both the f32 and f64
 * APIs route through this builder, so one cache speeds up all of them.
 * Single-threaded optimisation: a race on first build for the same config only
 * causes a benign redundant rebuild (the cached value is deterministic and
 * identical); callers needing concurrent first use should serialise it. */
static struct {
    int valid;
    alwan_f64 peak;
    alwan_aces_primaries_f64 prim;
    aces2_JMhParams_f64 jmh;
    aces2_GamutCompressParams_f64 gcp;
} g_aces2_gcp_cache_f64;

#if ALWAN_EMBED_DATA
/* ----------------------------------------------------------------
 * Precomputed gamut-compression tables for the standard ACES 2.0 output
 * presets. The 360-hue cusp / upper-hull-gamma / reach tables depend only on
 * (peak_luminance, limit_primaries), so they are constant per preset and are
 * embedded here instead of being rebuilt (via per-hue binary search) at runtime.
 *
 * Generated by gendata/aces2_gamut_tables.py, which captures the tables from
 * alwan's own gamut-boundary builder -- the builder is validated bit-exactly
 * against OCIO's full ACES 2.0 output transform by test suite 55 (OCIO and
 * colour-science do not expose these internal tables, so the validated builder
 * is the reference). Layout per file: 362 cusp entries x {J, M, gamma_top_inv}
 * (= ACES2_CUSP_TABLE_SIZE) followed by 360 reach_m values (ACES2_REACH_TABLE_SIZE). */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#define ALWAN_ACES2_GCP_LEN (ACES2_CUSP_TABLE_SIZE * 3 + ACES2_REACH_TABLE_SIZE)
static const alwan_f64 g_aces2_gamut_rec709_100[ALWAN_ACES2_GCP_LEN]  = {
#include "../data/aces2/gamut_rec709_100.csv"
};
static const alwan_f64 g_aces2_gamut_p3d65_100[ALWAN_ACES2_GCP_LEN]   = {
#include "../data/aces2/gamut_p3d65_100.csv"
};
static const alwan_f64 g_aces2_gamut_p3d65_1000[ALWAN_ACES2_GCP_LEN]  = {
#include "../data/aces2/gamut_p3d65_1000.csv"
};
static const alwan_f64 g_aces2_gamut_p3d65_48[ALWAN_ACES2_GCP_LEN]    = {
#include "../data/aces2/gamut_p3d65_48.csv"
};
static const alwan_f64 g_aces2_gamut_rec2020_500[ALWAN_ACES2_GCP_LEN] = {
#include "../data/aces2/gamut_rec2020_500.csv"
};
static const alwan_f64 g_aces2_gamut_rec2020_1000[ALWAN_ACES2_GCP_LEN]= {
#include "../data/aces2/gamut_rec2020_1000.csv"
};
static const alwan_f64 g_aces2_gamut_rec2020_2000[ALWAN_ACES2_GCP_LEN]= {
#include "../data/aces2/gamut_rec2020_2000.csv"
};
static const alwan_f64 g_aces2_gamut_rec2020_4000[ALWAN_ACES2_GCP_LEN]= {
#include "../data/aces2/gamut_rec2020_4000.csv"
};
ALWAN_DIAG_POP

static void primaries_rec709(alwan_aces_primaries_f64 *p);
static void primaries_p3_d65(alwan_aces_primaries_f64 *p);
static void primaries_rec2020(alwan_aces_primaries_f64 *p);

/* Return the embedded table for (peak, primaries) if it is one of the standard
 * presets, else NULL (the caller then builds the tables at runtime). Matches the
 * full primaries via memcmp so a custom gamut never aliases a preset. */
static const alwan_f64 *aces2_find_embedded_gamut_tables(
        alwan_f64 peak, const alwan_aces_primaries_f64 *p) {
    alwan_aces_primaries_f64 ref;
    if (peak == ALWAN_LITERAL(100.0)) {
        primaries_rec709(&ref); if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_rec709_100;
        primaries_p3_d65(&ref); if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_p3d65_100;
    } else if (peak == ALWAN_LITERAL(1000.0)) {
        primaries_p3_d65(&ref);  if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_p3d65_1000;
        primaries_rec2020(&ref); if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_rec2020_1000;
    } else if (peak == ALWAN_LITERAL(48.0)) {
        primaries_p3_d65(&ref);  if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_p3d65_48;
    } else if (peak == ALWAN_LITERAL(500.0)) {
        primaries_rec2020(&ref); if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_rec2020_500;
    } else if (peak == ALWAN_LITERAL(2000.0)) {
        primaries_rec2020(&ref); if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_rec2020_2000;
    } else if (peak == ALWAN_LITERAL(4000.0)) {
        primaries_rec2020(&ref); if (memcmp(&ref, p, sizeof(ref)) == 0) return g_aces2_gamut_rec2020_4000;
    }
    return NULL;
}
#endif /* ALWAN_EMBED_DATA */

#ifdef ALWAN_GENDATA_DUMP_ACES2
#include <stdio.h>
#include <stdlib.h>
/* Dump each unique (peak, primaries) config's tables once to
 * $ALWAN_ACES2_DUMP_DIR (in the embedded layout). gendata-only. */
static void alwan__gendata_dump_aces2_gamut(alwan_f64 peak,
        const alwan_aces_primaries_f64 *p, const aces2_GamutCompressParams_f64 *gcp) {
    static double seen_peak[32]; static alwan_aces_primaries_f64 seen_prim[32]; static int nseen = 0;
    int k;
    for (k = 0; k < nseen; k++)
        if (seen_peak[k] == peak && memcmp(&seen_prim[k], p, sizeof(*p)) == 0) return;
    {
        const char *dir = getenv("ALWAN_ACES2_DUMP_DIR"); char fn[512]; FILE *f; int i;
        if (!dir) dir = ".";
        snprintf(fn, sizeof(fn), "%s/gcp_peak%g_rx%.4f_gx%.4f_bx%.4f.csv",
                 dir, peak, p->red_x, p->green_x, p->blue_x);
        f = fopen(fn, "w");
        if (f) {
            for (i = 0; i < ACES2_CUSP_TABLE_SIZE; i++)
                fprintf(f, "%.17g,%.17g,%.17g,\n", gcp->cusp_table[i].J, gcp->cusp_table[i].M, gcp->cusp_table[i].gamma_top_inv);
            for (i = 0; i < ACES2_REACH_TABLE_SIZE; i++)
                fprintf(f, "%.17g,\n", gcp->reach_m_table[i]);
            fclose(f);
        }
    }
    if (nseen < 32) { seen_peak[nseen] = peak; seen_prim[nseen] = *p; nseen++; }
}
#endif /* ALWAN_GENDATA_DUMP_ACES2 */

static void init_GamutCompressParams_f64(alwan_f64 peak_luminance,
                                      alwan_aces_primaries_f64 const *limit_primaries,
                                      aces2_JMhParams_f64 const *jmh_params,
                                      aces2_GamutCompressParams_f64 *gcp) {
    if (g_aces2_gcp_cache_f64.valid
        && g_aces2_gcp_cache_f64.peak == peak_luminance
        && memcmp(&g_aces2_gcp_cache_f64.prim, limit_primaries, sizeof(*limit_primaries)) == 0
        && memcmp(&g_aces2_gcp_cache_f64.jmh, jmh_params, sizeof(*jmh_params)) == 0) {
        *gcp = g_aces2_gcp_cache_f64.gcp;
        return;
    }

    /* Basic parameters -- matches OCIO resolve_CompressionParams */
    gcp->limit_J_max = Y_to_J_f64(peak_luminance, jmh_params);
    gcp->model_gamma_inv = jmh_params->inv_cz;

    /* Bug 3 fix: focus_dist scales with log_peak (OCIO: focus_distance + focus_distance * 1.75 * log_peak) */
    alwan_f64 log_peak = ALWAN_LOG10_F64(peak_luminance / ALWAN_LITERAL(100.0));
    gcp->focus_dist = GAMUT_FOCUS_DISTANCE + GAMUT_FOCUS_DISTANCE * ALWAN_LITERAL(1.75) * log_peak;

    /* Bug 4 fix: lower_hull_gamma scales with log_peak (OCIO: 1/(1.14 + 0.07*log_peak)) */
    gcp->lower_hull_gamma_inv = ALWAN_LITERAL(1.0) / (GAMUT_LOWER_HULL_GAMMA + ALWAN_LITERAL(0.07) * log_peak);

    /* Bug 5 fix: mid_J uses OCIO's c_t formula
     * OCIO: c_t = (c_d/n_r) * (1 + w_g*log2(peak/100)), c_d=10.013, n_r=100, w_g=0.14 */
    {
        alwan_f64 c_d = ALWAN_LITERAL(10.013);
        alwan_f64 n_r = ALWAN_LITERAL(100.0);
        alwan_f64 w_g = ALWAN_LITERAL(0.14);
        alwan_f64 log2_peak = ALWAN_LN_F64(peak_luminance / ALWAN_LITERAL(100.0)) / ALWAN_LN_F64(ALWAN_LITERAL(2.0));
        alwan_f64 c_t = (c_d / n_r) * (ALWAN_LITERAL(1.0) + w_g * log2_peak);
        gcp->mid_J = Y_to_J_f64(c_t * ACES2_REF_LUMINANCE, jmh_params);
    }

#if ALWAN_EMBED_DATA && !defined(ALWAN_GENDATA_DUMP_ACES2)
    /* Standard preset? Load the precomputed cusp / gamma / reach tables instead
     * of rebuilding them by per-hue binary search (the expensive part). The
     * cheap scalars above are still computed at runtime. Byte-identical to the
     * runtime build, since the embedded tables are that build's output. */
    {
        const alwan_f64 *emb = aces2_find_embedded_gamut_tables(peak_luminance, limit_primaries);
        if (emb) {
            int i;
            for (i = 0; i < ACES2_CUSP_TABLE_SIZE; i++) {
                gcp->cusp_table[i].J             = emb[i * 3 + 0];
                gcp->cusp_table[i].M             = emb[i * 3 + 1];
                gcp->cusp_table[i].gamma_top_inv = emb[i * 3 + 2];
            }
            {
                const alwan_f64 *reach = emb + ACES2_CUSP_TABLE_SIZE * 3;
                gcp->reach_max_M = ALWAN_LITERAL(0.0);
                for (i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
                    gcp->reach_m_table[i] = reach[i];
                    if (reach[i] > gcp->reach_max_M) gcp->reach_max_M = reach[i];
                }
            }
            for (i = 0; i < 360; i++) gcp->hue_table[i + 1] = (alwan_f64)i;
            gcp->hue_table[0]   = gcp->hue_table[360] - ALWAN_LITERAL(360.0);
            gcp->hue_table[361] = gcp->hue_table[1]   + ALWAN_LITERAL(360.0);

            g_aces2_gcp_cache_f64.peak  = peak_luminance;
            g_aces2_gcp_cache_f64.prim  = *limit_primaries;
            g_aces2_gcp_cache_f64.jmh   = *jmh_params;
            g_aces2_gcp_cache_f64.gcp   = *gcp;
            g_aces2_gcp_cache_f64.valid = 1;
            return;
        }
    }
#endif /* ALWAN_EMBED_DATA */

    /* Initialize aces2_JMhParams_f64 for limit primaries */
    aces2_JMhParams_f64 limit_params;
    init_JMhParams_f64(limit_primaries, &limit_params);

    /* Build reach table using float32 binary search to match OCIO's float32 precision.
     * At near-degenerate hues (h~268-270 deg where AP1 ~ limit gamut), float32 rounding
     * gives a lower reach boundary so proportion close to 1.0 for degenerate pixels. */
    make_reach_m_table_gamut_f32_as_f64(jmh_params, gcp->limit_J_max, gcp->reach_m_table);

    /* Find max reach M across all hues */
    gcp->reach_max_M = ALWAN_LITERAL(0.0);
    for (int i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
        if (gcp->reach_m_table[i] > gcp->reach_max_M) {
            gcp->reach_max_M = gcp->reach_m_table[i];
        }
    }

    /* Build cusp table for limit primaries */
    /* First and last entries are duplicates for wrap-around interpolation */
    alwan_f64 lum_limit = peak_luminance / ALWAN_LITERAL(100.0);
    for (int i = 0; i < 360; i++) {
        build_cusp_table_for_hue_f64((alwan_f64)i, &limit_params,
                                  gcp->limit_J_max, lum_limit, &gcp->cusp_table[i + 1]);
        gcp->hue_table[i + 1] = (alwan_f64)i;
    }

    /* Wrap-around entries (J, M only -- gamma_top_inv set by make_upper_hull_gamma) */
    gcp->cusp_table[0] = gcp->cusp_table[360];
    gcp->hue_table[0] = gcp->hue_table[360] - ALWAN_LITERAL(360.0);
    gcp->cusp_table[361] = gcp->cusp_table[1];
    gcp->hue_table[361] = gcp->hue_table[1] + ALWAN_LITERAL(360.0);

    /* Compute upper hull gamma per hue via binary search (matches OCIO) */
    make_upper_hull_gamma_f64(gcp, peak_luminance, gcp->mid_J, gcp->focus_dist, &limit_params);

#ifdef ALWAN_GENDATA_DUMP_ACES2
    /* Regeneration hook for gendata/aces2_gamut_tables.py: dump each unique
     * (peak, primaries) config's freshly-built tables once, in the embed layout
     * (362 cusp {J,M,gamma} then 360 reach). Never compiled into normal builds. */
    alwan__gendata_dump_aces2_gamut(peak_luminance, limit_primaries, gcp);
#endif

    /* Populate the single-entry cache (store key + the fully-built tables last). */
    g_aces2_gcp_cache_f64.peak = peak_luminance;
    g_aces2_gcp_cache_f64.prim = *limit_primaries;
    g_aces2_gcp_cache_f64.jmh  = *jmh_params;
    g_aces2_gcp_cache_f64.gcp  = *gcp;
    g_aces2_gcp_cache_f64.valid = 1;
}

/* f32 version: compute in f64, downcast all fields to float */
static void init_GamutCompressParams_f32(float peak_luminance,
                                          alwan_aces_primaries_f32 const *limit_primaries,
                                          aces2_JMhParams_f32 const *jmh_params,
                                          aces2_GamutCompressParams_f32 *gcp) {
    /* Build a surrogate f64 JMhParams for the f64 init function.
     * The f64 init needs f64 primaries-based JMhParams; the f32 caller already
     * has those values as f32.  Upcasting preserves numerical equivalence. */
    aces2_JMhParams_f64 jmh64;
    int i;
    for (i = 0; i < 9; i++) {
        jmh64.MATRIX_RGB_to_CAM16_c.m[i]       = (alwan_f64)jmh_params->MATRIX_RGB_to_CAM16_c.m[i];
        jmh64.MATRIX_cone_response_to_Aab.m[i] = (alwan_f64)jmh_params->MATRIX_cone_response_to_Aab.m[i];
        jmh64.MATRIX_CAM16_to_RGB.m[i]         = (alwan_f64)jmh_params->MATRIX_CAM16_to_RGB.m[i];
        jmh64.MATRIX_Aab_to_cone.m[i]          = (alwan_f64)jmh_params->MATRIX_Aab_to_cone.m[i];
    }
    jmh64.cz        = (alwan_f64)jmh_params->cz;
    jmh64.inv_cz    = (alwan_f64)jmh_params->inv_cz;
    jmh64.A_w_J     = (alwan_f64)jmh_params->A_w_J;
    jmh64.inv_A_w_J = (alwan_f64)jmh_params->inv_A_w_J;
    jmh64.F_L_n     = (alwan_f64)jmh_params->F_L_n;

    /* Upcast f32 limit primaries to f64 */
    alwan_aces_primaries_f64 lp64;
    lp64.red_x = limit_primaries->red_x; lp64.red_y = limit_primaries->red_y;
    lp64.green_x = limit_primaries->green_x; lp64.green_y = limit_primaries->green_y;
    lp64.blue_x = limit_primaries->blue_x; lp64.blue_y = limit_primaries->blue_y;
    lp64.white_x = limit_primaries->white_x; lp64.white_y = limit_primaries->white_y;

    aces2_GamutCompressParams_f64 gcp64;
    init_GamutCompressParams_f64((alwan_f64)peak_luminance, &lp64, &jmh64, &gcp64);

    /* Downcast all scalar fields */
    gcp->limit_J_max        = (float)gcp64.limit_J_max;
    gcp->mid_J              = (float)gcp64.mid_J;
    gcp->focus_dist         = (float)gcp64.focus_dist;
    gcp->lower_hull_gamma_inv = (float)gcp64.lower_hull_gamma_inv;
    gcp->model_gamma_inv    = (float)gcp64.model_gamma_inv;
    gcp->reach_max_M        = (float)gcp64.reach_max_M;

    /* Downcast table arrays */
    for (i = 0; i < ACES2_CUSP_TABLE_SIZE; i++) {
        gcp->cusp_table[i].J             = (float)gcp64.cusp_table[i].J;
        gcp->cusp_table[i].M             = (float)gcp64.cusp_table[i].M;
        gcp->cusp_table[i].gamma_top_inv = (float)gcp64.cusp_table[i].gamma_top_inv;
        gcp->hue_table[i]                = (float)gcp64.hue_table[i];
    }
    for (i = 0; i < ACES2_REACH_TABLE_SIZE; i++) {
        gcp->reach_m_table[i] = (float)gcp64.reach_m_table[i];
    }
}

/* ----------------------------------------------------------------
 * ACES 2.0: Lookup cusp with interpolation
 * ---------------------------------------------------------------- */

static void lookup_cusp_f64(alwan_f64 h_deg, aces2_GamutCompressParams_f64 const *gcp,
                         alwan_f64 *cusp_J, alwan_f64 *cusp_M,
                         alwan_f64 *gamma_top_inv) {
    /* Wrap hue to [0, 360) -- NaN-safe (NaN fails both while conditions) */
    /* Guard NaN/+-Inf/huge first so the wrap terminates (the naive while-subtract
     * never ends for +Inf or for |h| where h - 360 == h). */
    if (!(h_deg > ALWAN_LITERAL(-1.0e6) && h_deg < ALWAN_LITERAL(1.0e6))) h_deg = ALWAN_LITERAL(0.0);
    while (h_deg < ALWAN_LITERAL(0.0)) h_deg += ALWAN_LITERAL(360.0);
    while (h_deg >= ALWAN_LITERAL(360.0)) h_deg -= ALWAN_LITERAL(360.0);

    /* Linear interpolation */
    int idx0 = (int)h_deg + 1;  /* +1 for offset due to wrap entry */
    int idx1 = (idx0 + 1 < ACES2_CUSP_TABLE_SIZE) ? idx0 + 1 : 1;
    alwan_f64 t = h_deg - (alwan_f64)(idx0 - 1);

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

/* Lookup per-hue reach_m from the GamutCompressParams table */
static alwan_f64 lookup_gamut_reach_m_f64(alwan_f64 h_deg, aces2_GamutCompressParams_f64 const *gcp) {
    /* Guard NaN/+-Inf/huge first so the wrap terminates (the naive while-subtract
     * never ends for +Inf or for |h| where h - 360 == h). */
    if (!(h_deg > ALWAN_LITERAL(-1.0e6) && h_deg < ALWAN_LITERAL(1.0e6))) h_deg = ALWAN_LITERAL(0.0);
    while (h_deg < ALWAN_LITERAL(0.0)) h_deg += ALWAN_LITERAL(360.0);
    while (h_deg >= ALWAN_LITERAL(360.0)) h_deg -= ALWAN_LITERAL(360.0);
    int idx0 = (int)h_deg;
    int idx1 = (idx0 + 1) % ACES2_REACH_TABLE_SIZE;
    alwan_f64 t = h_deg - (alwan_f64)idx0;
    return gcp->reach_m_table[idx0] * (ALWAN_LITERAL(1.0) - t)
         + gcp->reach_m_table[idx1] * t;
}

static void init_hue_dependent_params_f64(alwan_f64 h_deg,
                                       aces2_GamutCompressParams_f64 const *gcp,
                                       aces2_HueDependentGamutParams_f64 *hdp) {
    /* Perform cusp lookup and derived computations in f32 to match OCIO's
     * float32 pipeline.  At near-degenerate hues (h~267 deg) the f64-vs-f32
     * difference in interpolated cusp values cascades through the gamut
     * compression and produces wildly different Reinhard remap outputs. */

    /* Cusp lookup -- f32 interpolation */
    float fh = (float)h_deg;
    /* Guard NaN/+-Inf/huge first so the wrap terminates (the naive while-subtract
     * never ends for +Inf or for |h| where h - 360 == h). */
    if (!(fh > -1.0e6f && fh < 1.0e6f)) fh = 0.0f;
    while (fh < 0.0f) fh += 360.0f;
    while (fh >= 360.0f) fh -= 360.0f;
    int idx0 = (int)fh + 1;
    int idx1 = (idx0 + 1 < ACES2_CUSP_TABLE_SIZE) ? idx0 + 1 : 1;
    float ft = fh - (float)(idx0 - 1);
    float fcJ = (float)gcp->cusp_table[idx0].J * (1.0f - ft)
              + (float)gcp->cusp_table[idx1].J * ft;
    float fcM = (float)gcp->cusp_table[idx0].M * (1.0f - ft)
              + (float)gcp->cusp_table[idx1].M * ft;
    float fgti = (float)gcp->cusp_table[idx0].gamma_top_inv * (1.0f - ft)
               + (float)gcp->cusp_table[idx1].gamma_top_inv * ft;

    hdp->cusp_J = (alwan_f64)fcJ;
    hdp->cusp_M = (alwan_f64)fcM;
    hdp->gamma_top_inv = (alwan_f64)fgti;
    hdp->gamma_bottom_inv = gcp->lower_hull_gamma_inv;

    /* Focus J -- f32 */
    float f_focus_J = aces2_compute_focus_j_f32_v(fcJ, (float)gcp->mid_J, (float)gcp->limit_J_max);
    hdp->focus_J = (alwan_f64)f_focus_J;

    /* Analytical threshold -- f32 */
    float f_at = fcJ + (float)GAMUT_FOCUS_GAIN_BLEND * ((float)gcp->limit_J_max - fcJ);
    hdp->analytical_threshold = (alwan_f64)f_at;

    /* Reach M -- f32 interpolation */
    float frh = fh;
    int ridx0 = (int)frh;
    int ridx1 = (ridx0 + 1) % ACES2_REACH_TABLE_SIZE;
    float frt = frh - (float)ridx0;
    float f_reach = (float)gcp->reach_m_table[ridx0] * (1.0f - frt)
                  + (float)gcp->reach_m_table[ridx1] * frt;
    hdp->reach_m = (alwan_f64)f_reach;
}

/* f32 version: same interpolation as above but inputs/outputs are already float */
static void init_hue_dependent_params_f32(float h_deg,
                                           aces2_GamutCompressParams_f32 const *gcp,
                                           aces2_HueDependentGamutParams_f32 *hdp) {
    float fh = h_deg;
    /* Guard NaN/+-Inf/huge first so the wrap terminates (the naive while-subtract
     * never ends for +Inf or for |h| where h - 360 == h). */
    if (!(fh > -1.0e6f && fh < 1.0e6f)) fh = 0.0f;
    while (fh < 0.0f) fh += 360.0f;
    while (fh >= 360.0f) fh -= 360.0f;
    int idx0 = (int)fh + 1;
    int idx1 = (idx0 + 1 < ACES2_CUSP_TABLE_SIZE) ? idx0 + 1 : 1;
    float ft  = fh - (float)(idx0 - 1);
    float fcJ = gcp->cusp_table[idx0].J * (1.0f - ft) + gcp->cusp_table[idx1].J * ft;
    float fcM = gcp->cusp_table[idx0].M * (1.0f - ft) + gcp->cusp_table[idx1].M * ft;
    float fgti = gcp->cusp_table[idx0].gamma_top_inv * (1.0f - ft)
               + gcp->cusp_table[idx1].gamma_top_inv * ft;

    hdp->cusp_J          = fcJ;
    hdp->cusp_M          = fcM;
    hdp->gamma_top_inv   = fgti;
    hdp->gamma_bottom_inv = gcp->lower_hull_gamma_inv;

    hdp->focus_J = aces2_compute_focus_j_f32_v(fcJ, gcp->mid_J, gcp->limit_J_max);

    hdp->analytical_threshold = fcJ + (float)GAMUT_FOCUS_GAIN_BLEND * (gcp->limit_J_max - fcJ);

    /* Reach M */
    float frh = fh;
    int ridx0 = (int)frh;
    int ridx1 = (ridx0 + 1) % ACES2_REACH_TABLE_SIZE;
    float frt = frh - (float)ridx0;
    hdp->reach_m = gcp->reach_m_table[ridx0] * (1.0f - frt)
                 + gcp->reach_m_table[ridx1] * frt;
}

/* ----------------------------------------------------------------
 * ACES 2.0: Core gamut compression algorithm (forward)
 * ---------------------------------------------------------------- */

static void compress_gamut_fwd_f64(alwan_f64 J, alwan_f64 M, alwan_f64 h,
                                aces2_GamutCompressParams_f64 const *gcp,
                                aces2_HueDependentGamutParams_f64 const *hdp,
                                alwan_f64 *J_out, alwan_f64 *M_out) {
    alwan_vec2_f64 result = aces2_compress_gamut_fwd_f64_v(J, M, h, gcp, hdp);
    *J_out = result.v[0]; *M_out = result.v[1];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Core gamut compression algorithm (inverse)
 * ---------------------------------------------------------------- */

static void compress_gamut_inv_f64(alwan_f64 J, alwan_f64 M, alwan_f64 h,
                                aces2_GamutCompressParams_f64 const *gcp,
                                aces2_HueDependentGamutParams_f64 const *hdp,
                                alwan_f64 *J_out, alwan_f64 *M_out) {
    alwan_vec2_f64 result = aces2_compress_gamut_inv_f64_v(J, M, h, gcp, hdp);
    *J_out = result.v[0]; *M_out = result.v[1];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression in JMh space (public API)
 * Reference: OCIO Transform.cpp gamut_compress_fwd
 * ---------------------------------------------------------------- */

/* Check if primaries are approximately equal to AP1 */
static int primaries_are_ap1(alwan_aces_primaries_f64 const *p) {
    static const alwan_f64 tol = ALWAN_LITERAL(0.001);
    return ALWAN_ABS(p->red_x - AP1_RED_x) < tol &&
           ALWAN_ABS(p->red_y - AP1_RED_y) < tol &&
           ALWAN_ABS(p->green_x - AP1_GREEN_x) < tol &&
           ALWAN_ABS(p->green_y - AP1_GREEN_y) < tol &&
           ALWAN_ABS(p->blue_x - AP1_BLUE_x) < tol &&
           ALWAN_ABS(p->blue_y - AP1_BLUE_y) < tol;
}

void alwan_aces_gamut_compress20_f64(alwan_vec3_f64 *jmh_out,
                                 alwan_vec3_f64 const *jmh_in,
                                 alwan_f64 peak_luminance,
                                 alwan_aces_primaries_f64 const *limit_primaries) {
    if (!jmh_out || !jmh_in || !limit_primaries) return;
    if (peak_luminance < ALWAN_LITERAL(1.0) || peak_luminance > ALWAN_LITERAL(10000.0)) {
        return;
    }

    alwan_f64 J = jmh_in->v[0];
    alwan_f64 M = jmh_in->v[1];
    alwan_f64 h = jmh_in->v[2];

    /* Edge cases */
    if (J <= ALWAN_LITERAL(0.0)) {
        jmh_out->v[0] = ALWAN_LITERAL(0.0);
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return;
    }

    /* When limit primaries equal reach primaries (AP1), no compression needed.
     * This matches OCIO behavior: colors within AP1 gamut pass through unchanged.
     */
    if (primaries_are_ap1(limit_primaries)) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = M;
        jmh_out->v[2] = h;
        return;
    }

    /* Initialize JMh parameters for reach gamut (AP1) */
    alwan_aces_primaries_f64 reach_primaries;
    alwan_aces_primaries_ap1_default_f64(&reach_primaries);
    aces2_JMhParams_f64 jmh_params;
    init_JMhParams_f64(&reach_primaries, &jmh_params);

    /* Compute limit_J_max */
    alwan_f64 limit_J_max = Y_to_J_f64(peak_luminance, &jmh_params);

    if (M <= ALWAN_LITERAL(0.0) || J > limit_J_max) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return;
    }

    /* Initialize gamut compression parameters */
    aces2_GamutCompressParams_f64 gcp;
    init_GamutCompressParams_f64(peak_luminance, limit_primaries, &jmh_params, &gcp);

    /* Initialize hue-dependent parameters */
    aces2_HueDependentGamutParams_f64 hdp;
    init_hue_dependent_params_f64(h, &gcp, &hdp);

    /* Apply gamut compression */
    alwan_f64 J_out, M_out;
    compress_gamut_fwd_f64(J, M, h, &gcp, &hdp, &J_out, &M_out);

    jmh_out->v[0] = J_out;
    jmh_out->v[1] = M_out;
    jmh_out->v[2] = h;
}

void alwan_aces_gamut_compress20_f32(alwan_vec3_f32 *jmh_out,
                                 alwan_vec3_f32 const *jmh_in,
                                 alwan_f32 peak_luminance,
                                 alwan_aces_primaries_f32 const *limit_primaries) {
    if (!jmh_out || !jmh_in || !limit_primaries) return;
    alwan_vec3_f64 in64 = {{(alwan_f64)jmh_in->v[0], (alwan_f64)jmh_in->v[1], (alwan_f64)jmh_in->v[2]}};
    alwan_aces_primaries_f64 lp64;
    lp64.red_x = limit_primaries->red_x; lp64.red_y = limit_primaries->red_y;
    lp64.green_x = limit_primaries->green_x; lp64.green_y = limit_primaries->green_y;
    lp64.blue_x = limit_primaries->blue_x; lp64.blue_y = limit_primaries->blue_y;
    lp64.white_x = limit_primaries->white_x; lp64.white_y = limit_primaries->white_y;
    alwan_vec3_f64 out64;
    alwan_aces_gamut_compress20_f64(&out64, &in64, (double)peak_luminance, &lp64);
    jmh_out->v[0] = (float)out64.v[0]; jmh_out->v[1] = (float)out64.v[1]; jmh_out->v[2] = (float)out64.v[2];
}

/* ----------------------------------------------------------------
 * ACES 2.0: Gamut Compression Inverse (public API)
 * Reference: OCIO Transform.cpp gamut_compress_inv
 * ---------------------------------------------------------------- */

void alwan_aces_gamut_compress20_inv_f64(alwan_vec3_f64 *jmh_out,
                                     alwan_vec3_f64 const *jmh_in,
                                     alwan_f64 peak_luminance,
                                     alwan_aces_primaries_f64 const *limit_primaries) {
    if (!jmh_out || !jmh_in || !limit_primaries) return;
    if (peak_luminance < ALWAN_LITERAL(1.0) || peak_luminance > ALWAN_LITERAL(10000.0)) {
        return;
    }

    alwan_f64 J = jmh_in->v[0];
    alwan_f64 M = jmh_in->v[1];
    alwan_f64 h = jmh_in->v[2];

    /* Edge cases */
    if (J <= ALWAN_LITERAL(0.0)) {
        jmh_out->v[0] = ALWAN_LITERAL(0.0);
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return;
    }

    /* When limit primaries equal reach primaries (AP1), identity */
    if (primaries_are_ap1(limit_primaries)) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = M;
        jmh_out->v[2] = h;
        return;
    }

    /* Initialize JMh parameters for reach gamut (AP1) */
    alwan_aces_primaries_f64 reach_primaries;
    alwan_aces_primaries_ap1_default_f64(&reach_primaries);
    aces2_JMhParams_f64 jmh_params;
    init_JMhParams_f64(&reach_primaries, &jmh_params);

    /* Compute limit_J_max */
    alwan_f64 limit_J_max = Y_to_J_f64(peak_luminance, &jmh_params);

    if (M <= ALWAN_LITERAL(0.0) || J > limit_J_max) {
        jmh_out->v[0] = J;
        jmh_out->v[1] = ALWAN_LITERAL(0.0);
        jmh_out->v[2] = h;
        return;
    }

    /* Initialize gamut compression parameters */
    aces2_GamutCompressParams_f64 gcp;
    init_GamutCompressParams_f64(peak_luminance, limit_primaries, &jmh_params, &gcp);

    /* Initialize hue-dependent parameters */
    aces2_HueDependentGamutParams_f64 hdp;
    init_hue_dependent_params_f64(h, &gcp, &hdp);

    /* Apply inverse gamut compression */
    alwan_f64 J_out, M_out;
    compress_gamut_inv_f64(J, M, h, &gcp, &hdp, &J_out, &M_out);

    jmh_out->v[0] = J_out;
    jmh_out->v[1] = M_out;
    jmh_out->v[2] = h;
}

void alwan_aces_gamut_compress20_inv_f32(alwan_vec3_f32 *jmh_out,
                                     alwan_vec3_f32 const *jmh_in,
                                     alwan_f32 peak_luminance,
                                     alwan_aces_primaries_f32 const *limit_primaries) {
    if (!jmh_out || !jmh_in || !limit_primaries) return;
    alwan_vec3_f64 in64 = {{(alwan_f64)jmh_in->v[0], (alwan_f64)jmh_in->v[1], (alwan_f64)jmh_in->v[2]}};
    alwan_aces_primaries_f64 lp64;
    lp64.red_x = limit_primaries->red_x; lp64.red_y = limit_primaries->red_y;
    lp64.green_x = limit_primaries->green_x; lp64.green_y = limit_primaries->green_y;
    lp64.blue_x = limit_primaries->blue_x; lp64.blue_y = limit_primaries->blue_y;
    lp64.white_x = limit_primaries->white_x; lp64.white_y = limit_primaries->white_y;
    alwan_vec3_f64 out64;
    alwan_aces_gamut_compress20_inv_f64(&out64, &in64, (double)peak_luminance, &lp64);
    jmh_out->v[0] = (float)out64.v[0]; jmh_out->v[1] = (float)out64.v[1]; jmh_out->v[2] = (float)out64.v[2];
}

/* ================================================================
 * ACES 2.0 Output Transform (Unified API)
 * ================================================================ */

/* Output transform preset configuration */
typedef struct {
    alwan_aces_primaries_f64 primaries;
    alwan_f64 peak_luminance;
    alwan_transfer_function eotf;
    int needs_d60_to_d65;  /* 1 if chromatic adaptation needed */
} aces2_output_config;

/* Standard primaries definitions */
static void primaries_rec709(alwan_aces_primaries_f64 *p) {
    p->red_x = ALWAN_BT709_RED_x;     p->red_y = ALWAN_BT709_RED_y;
    p->green_x = ALWAN_BT709_GREEN_x; p->green_y = ALWAN_BT709_GREEN_y;
    p->blue_x = ALWAN_BT709_BLUE_x;   p->blue_y = ALWAN_BT709_BLUE_y;
    p->white_x = ALWAN_D65_x; p->white_y = ALWAN_D65_y;
}

static void primaries_p3_d65(alwan_aces_primaries_f64 *p) {
    p->red_x = ALWAN_P3_RED_x;     p->red_y = ALWAN_P3_RED_y;
    p->green_x = ALWAN_P3_GREEN_x; p->green_y = ALWAN_P3_GREEN_y;
    p->blue_x = ALWAN_P3_BLUE_x;   p->blue_y = ALWAN_P3_BLUE_y;
    p->white_x = ALWAN_D65_x; p->white_y = ALWAN_D65_y;
}

static void primaries_rec2020(alwan_aces_primaries_f64 *p) {
    p->red_x = ALWAN_BT2020_RED_x;     p->red_y = ALWAN_BT2020_RED_y;
    p->green_x = ALWAN_BT2020_GREEN_x; p->green_y = ALWAN_BT2020_GREEN_y;
    p->blue_x = ALWAN_BT2020_BLUE_x;   p->blue_y = ALWAN_BT2020_BLUE_y;
    p->white_x = ALWAN_D65_x; p->white_y = ALWAN_D65_y;
}

static void primaries_p3_dci(alwan_aces_primaries_f64 *p) {
    p->red_x = ALWAN_P3_RED_x;     p->red_y = ALWAN_P3_RED_y;
    p->green_x = ALWAN_P3_GREEN_x; p->green_y = ALWAN_P3_GREEN_y;
    p->blue_x = ALWAN_P3_BLUE_x;   p->blue_y = ALWAN_P3_BLUE_y;
    /* DCI white point */
    p->white_x = ALWAN_LITERAL(0.314); p->white_y = ALWAN_LITERAL(0.351);
}

/* DCDM uses CIE XYZ primaries */
static void primaries_xyz(alwan_aces_primaries_f64 *p) {
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
static const alwan_f64 g_d60_to_d65_bradford[9] = {
    ALWAN_LITERAL( 0.98722400870301763), ALWAN_LITERAL(-0.00611322860685689), ALWAN_LITERAL( 0.01595328833591263),
    ALWAN_LITERAL(-0.00759837181166235), ALWAN_LITERAL( 1.00186148473965364), ALWAN_LITERAL( 0.00533003579138894),
    ALWAN_LITERAL( 0.00307257705853153), ALWAN_LITERAL(-0.00509596151113058), ALWAN_LITERAL( 1.08168060306579528)
};

/* D65 to D60 chromatic adaptation matrix (Bradford, inverse) */
static const alwan_f64 g_d65_to_d60_bradford[9] = {
    ALWAN_LITERAL( 1.01303000), ALWAN_LITERAL( 0.00610531), ALWAN_LITERAL(-0.01497100),
    ALWAN_LITERAL( 0.00769823), ALWAN_LITERAL( 0.99816500), ALWAN_LITERAL(-0.00503203),
    ALWAN_LITERAL(-0.00284131), ALWAN_LITERAL( 0.00468516), ALWAN_LITERAL( 0.92450700)
};

/* Apply 3x3 matrix to RGB */
static void apply_matrix_rgb(alwan_f64 const m[9], alwan_rgb_f64 const *in, alwan_rgb_f64 *out) {
    alwan_f64 r = in->r, g = in->g, b = in->b;
    out->r = m[0] * r + m[1] * g + m[2] * b;
    out->g = m[3] * r + m[4] * g + m[5] * b;
    out->b = m[6] * r + m[7] * g + m[8] * b;
}

/* Compute AP1 to limiting primaries matrix */
static void compute_ap1_to_limit_matrix_f64(alwan_aces_primaries_f64 const *limit,
                                         alwan_f64 out[9]) {
    /* AP1 (ACEScg) primaries - ACES white point D60 */
    alwan_f64 ap1_to_xyz[9];
    primaries_to_rgb_to_xyz_f64(
        AP1_RED_x,   AP1_RED_y,
        AP1_GREEN_x, AP1_GREEN_y,
        AP1_BLUE_x,  AP1_BLUE_y,
        AP1_WHITE_x, AP1_WHITE_y,
        ALWAN_LITERAL(1.0),
        ap1_to_xyz
    );

    /* Limit primaries to XYZ */
    alwan_f64 limit_to_xyz[9];
    primaries_to_rgb_to_xyz_f64(
        limit->red_x, limit->red_y,
        limit->green_x, limit->green_y,
        limit->blue_x, limit->blue_y,
        limit->white_x, limit->white_y,
        ALWAN_LITERAL(1.0),
        limit_to_xyz
    );

    /* XYZ to limit primaries */
    alwan_f64 xyz_to_limit[9];
    invert_mat3(limit_to_xyz, xyz_to_limit);

    /* AP1 -> XYZ -> Limit = AP1 -> Limit */
    mult_mat3(xyz_to_limit, ap1_to_xyz, out);
}

/* Compute limit primaries to AP1 matrix (inverse) */
static void compute_limit_to_ap1_matrix(alwan_aces_primaries_f64 const *limit,
                                         alwan_f64 out[9]) {
    alwan_f64 ap1_to_limit[9];
    compute_ap1_to_limit_matrix_f64(limit, ap1_to_limit);
    invert_mat3(ap1_to_limit, out);
}

/* ----------------------------------------------------------------
 * ACES 2.0 Output Transform Implementation
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
alwan_status alwan_aces2_output_transform_f64(alwan_rgb_f64 *rgb_out,
                                      alwan_rgb_f64 const *rgb_in,
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
        alwan_aces_primaries_f64 ap1;
        alwan_aces_primaries_ap1_default_f64(&ap1);

        /* Use P3-D65 as the limiting primaries for gamut compression */
        alwan_aces_primaries_f64 p3_d65;
        primaries_p3_d65(&p3_d65);

        /* Step 1: Apply tonescale compression */
        alwan_rgb_f64 rgb_ts;
        alwan_aces_tonescale_compress20_f64(&rgb_ts, rgb_in, config.peak_luminance);

        /* Step 2: Convert tonescale output to JMh for gamut compression */
        alwan_vec3_f64 jmh_ts;
        alwan_aces_rgb_to_jmh20_f64(&jmh_ts, &rgb_ts, &ap1);

        /* Step 3: Apply gamut compression using P3-D65 as limiting primaries */
        alwan_vec3_f64 jmh_gc;
        alwan_aces_gamut_compress20_f64(&jmh_gc, &jmh_ts, config.peak_luminance, &p3_d65);

        /* Step 4: Convert JMh back to RGB in AP1 space */
        alwan_rgb_f64 rgb_ap1;
        alwan_aces_jmh_to_rgb20_f64(&rgb_ap1, &jmh_gc, &ap1);

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
        alwan_f64 xyz[3];
        xyz[0] = ACES1_AP1_TO_XYZ_D60_f64[0] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60_f64[1] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60_f64[2] * rgb_ap1.b;
        xyz[1] = ACES1_AP1_TO_XYZ_D60_f64[3] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60_f64[4] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60_f64[5] * rgb_ap1.b;
        xyz[2] = ACES1_AP1_TO_XYZ_D60_f64[6] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60_f64[7] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60_f64[8] * rgb_ap1.b;

        /* Step 6: Normalize XYZ to equal-energy white for DCDM
         * D60 white point in XYZ is approximately (0.9526, 1.0, 1.0089)
         * We scale each component so that neutral colors have X=Y=Z */
        static const alwan_f64 D60_WHITE_X = ALWAN_LITERAL(0.952646074569846);
        static const alwan_f64 D60_WHITE_Z = ALWAN_LITERAL(1.008825184351586);
        xyz[0] /= D60_WHITE_X;
        xyz[2] /= D60_WHITE_Z;

        /* Step 7: Clamp negative values */
        alwan_f64 x_clamped = xyz[0] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : xyz[0];
        alwan_f64 y_clamped = xyz[1] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : xyz[1];
        alwan_f64 z_clamped = xyz[2] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : xyz[2];

        /* Step 8: Apply gamma 2.6 encoding */
        rgb_out->r = aces_gamma26_oetf_f64_v(x_clamped);
        rgb_out->g = aces_gamma26_oetf_f64_v(y_clamped);
        rgb_out->b = aces_gamma26_oetf_f64_v(z_clamped);

        return ALWAN_OK;
    }

    if (output == ALWAN_ACES2_OUT_P3DCI_48NIT) {
        /* P3-DCI: P3 primaries with DCI white point and gamma 2.6 encoding */

        /* Initialize AP1 primaries for JMh conversion */
        alwan_aces_primaries_f64 ap1;
        alwan_aces_primaries_ap1_default_f64(&ap1);

        /* Use P3-D65 as the limiting primaries for gamut compression
         * (P3-DCI has same primaries, just different white point) */
        alwan_aces_primaries_f64 p3_d65;
        primaries_p3_d65(&p3_d65);

        /* Step 1: Apply tonescale compression */
        alwan_rgb_f64 rgb_ts;
        alwan_aces_tonescale_compress20_f64(&rgb_ts, rgb_in, config.peak_luminance);

        /* Step 2: Convert tonescale output to JMh for gamut compression */
        alwan_vec3_f64 jmh_ts;
        alwan_aces_rgb_to_jmh20_f64(&jmh_ts, &rgb_ts, &ap1);

        /* Step 3: Apply gamut compression using P3-D65 as limiting primaries */
        alwan_vec3_f64 jmh_gc;
        alwan_aces_gamut_compress20_f64(&jmh_gc, &jmh_ts, config.peak_luminance, &p3_d65);

        /* Step 4: Convert JMh back to RGB in AP1 space */
        alwan_rgb_f64 rgb_ap1;
        alwan_aces_jmh_to_rgb20_f64(&rgb_ap1, &jmh_gc, &ap1);

        /* Step 5: Convert AP1 to XYZ (D60) */
        alwan_f64 xyz_d60[3];
        xyz_d60[0] = ACES1_AP1_TO_XYZ_D60_f64[0] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60_f64[1] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60_f64[2] * rgb_ap1.b;
        xyz_d60[1] = ACES1_AP1_TO_XYZ_D60_f64[3] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60_f64[4] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60_f64[5] * rgb_ap1.b;
        xyz_d60[2] = ACES1_AP1_TO_XYZ_D60_f64[6] * rgb_ap1.r + ACES1_AP1_TO_XYZ_D60_f64[7] * rgb_ap1.g + ACES1_AP1_TO_XYZ_D60_f64[8] * rgb_ap1.b;

        /* Step 6: Convert XYZ (D60) to P3-DCI
         * P3-DCI uses DCI white point (0.314, 0.351)
         * For simplicity, we use the same conversion as ACES 1.x which applies D60->D65
         * then converts to P3-D65 (same primaries as P3-DCI) */
        alwan_f64 xyz_d65[3];
        xyz_d65[0] = ACES1_D60_TO_D65_f64[0] * xyz_d60[0] + ACES1_D60_TO_D65_f64[1] * xyz_d60[1] + ACES1_D60_TO_D65_f64[2] * xyz_d60[2];
        xyz_d65[1] = ACES1_D60_TO_D65_f64[3] * xyz_d60[0] + ACES1_D60_TO_D65_f64[4] * xyz_d60[1] + ACES1_D60_TO_D65_f64[5] * xyz_d60[2];
        xyz_d65[2] = ACES1_D60_TO_D65_f64[6] * xyz_d60[0] + ACES1_D60_TO_D65_f64[7] * xyz_d60[1] + ACES1_D60_TO_D65_f64[8] * xyz_d60[2];

        /* XYZ (D65) to P3-D65 matrix (same primaries as P3-DCI) */
        static const alwan_f64 XYZ_D65_TO_P3[9] = {
            ALWAN_LITERAL( 2.4934969119), ALWAN_LITERAL(-0.9313836179), ALWAN_LITERAL(-0.4027107845),
            ALWAN_LITERAL(-0.8294889696), ALWAN_LITERAL( 1.7626640603), ALWAN_LITERAL( 0.0236246858),
            ALWAN_LITERAL( 0.0358458302), ALWAN_LITERAL(-0.0761723893), ALWAN_LITERAL( 0.9568845240)
        };

        alwan_f64 p3[3];
        p3[0] = XYZ_D65_TO_P3[0] * xyz_d65[0] + XYZ_D65_TO_P3[1] * xyz_d65[1] + XYZ_D65_TO_P3[2] * xyz_d65[2];
        p3[1] = XYZ_D65_TO_P3[3] * xyz_d65[0] + XYZ_D65_TO_P3[4] * xyz_d65[1] + XYZ_D65_TO_P3[5] * xyz_d65[2];
        p3[2] = XYZ_D65_TO_P3[6] * xyz_d65[0] + XYZ_D65_TO_P3[7] * xyz_d65[1] + XYZ_D65_TO_P3[8] * xyz_d65[2];

        /* Step 7: Clamp to [0, 1] for cinema */
        alwan_f64 r_clamped = p3[0] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (p3[0] > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : p3[0]);
        alwan_f64 g_clamped = p3[1] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (p3[1] > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : p3[1]);
        alwan_f64 b_clamped = p3[2] < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (p3[2] > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : p3[2]);

        /* Step 8: Apply gamma 2.6 encoding */
        rgb_out->r = aces_gamma26_oetf_f64_v(r_clamped);
        rgb_out->g = aces_gamma26_oetf_f64_v(g_clamped);
        rgb_out->b = aces_gamma26_oetf_f64_v(b_clamped);

        return ALWAN_OK;
    }

    return alwan_aces2_output_transform_custom_f64(rgb_out,
                                                    rgb_in,
                                                    config.peak_luminance,
                                                    &config.primaries,
                                                    config.eotf);
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
/* Native single-precision batch pipeline (generated from
 * alwan_aces2_map_impl.inc below). Forward-declared so the scalar entry can
 * reuse it for count == 1 now that the batch path matches the scalar/OCIO. */
static int aces2_ot_map_impl_f32(alwan_f32 *out, alwan_f32 const *in,
                                 alwan_aces2_output output, size_t count,
                                 size_t in_stride, size_t out_stride);

alwan_status alwan_aces2_output_transform_f32(alwan_rgb_f32 *rgb_out,
                                      alwan_rgb_f32 const *rgb_in,
                                      alwan_aces2_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    if (output < 0 || output >= ALWAN_ACES2_OUT_COUNT) return ALWAN_E_INVALID;

    /* DCDM / P3-DCI cinema presets use a bespoke XYZ-domain scalar path that
     * the batch impl does not replicate; keep them on the f64 reference. */
    if (output == ALWAN_ACES2_OUT_DCDM_48NIT || output == ALWAN_ACES2_OUT_P3DCI_48NIT) {
#if ALWAN_WITH_F64
        alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
        alwan_rgb_f64 out64;
        int s = alwan_aces2_output_transform_f64(&out64, &in64, output);
        rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
        return s;
#else
        return ALWAN_E_INVALID;
#endif
    }

    /* General presets: native single-precision via the templated batch
     * pipeline (one pixel). alwan_rgb_f32 is three contiguous floats. */
    return aces2_ot_map_impl_f32((alwan_f32 *)rgb_out, (alwan_f32 const *)rgb_in,
                                 output, 1, 3 * sizeof(alwan_f32), 3 * sizeof(alwan_f32));
}
#endif /* ALWAN_WITH_F32 */

/* Display-linear front half of the ACES 2.0 output transform: tonescale,
 * chroma compression and gamut compression, decoded into the LIMIT primaries,
 * WITHOUT the display encode (no [0,peak] clamp, no OETF). Out-of-gamut /
 * over-range residuals that the encode clamp would discard are preserved --
 * this is the raw rendering result the encode tail consumes. */
alwan_status alwan_aces2_output_transform_custom_display_linear_f64(alwan_rgb_f64 *rgb_out,
                                             alwan_rgb_f64 const *rgb_in,
                                             alwan_f64 peak_luminance,
                                             alwan_aces_primaries_f64 const *limit_primaries) {
    if (!rgb_out || !rgb_in || !limit_primaries) return ALWAN_E_INVALID;
    if (peak_luminance < ALWAN_LITERAL(1.0) || peak_luminance > ALWAN_LITERAL(10000.0)) {
        return ALWAN_E_INVALID;
    }

    /* Initialize AP1 primaries for JMh conversion */
    alwan_aces_primaries_f64 ap1;
    alwan_aces_primaries_ap1_default_f64(&ap1);

    /* Step 1: Convert input RGB (AP1) to JMh */
    alwan_vec3_f64 jmh;
    alwan_aces_rgb_to_jmh20_f64(&jmh, rgb_in, &ap1);

    /* Step 2: Apply tonescale compression */
    alwan_rgb_f64 rgb_ts;
    alwan_aces_tonescale_compress20_f64(&rgb_ts, rgb_in, peak_luminance);

    /* Step 3: Convert tonescale output to JMh for gamut compression */
    alwan_vec3_f64 jmh_ts;
    alwan_aces_rgb_to_jmh20_f64(&jmh_ts, &rgb_ts, &ap1);

    /* Step 4: Apply gamut compression to limiting primaries */
    alwan_vec3_f64 jmh_gc;
    alwan_aces_gamut_compress20_f64(&jmh_gc, &jmh_ts, peak_luminance, limit_primaries);

    /* Step 5: Convert the gamut-compressed JMh back to RGB directly in the LIMIT
     * primaries. OCIO decodes with the limit-primary JMhParams (matching the
     * gamut compression, which already targets the limit gamut). The old path
     * decoded in AP1 then applied an AP1->limit matrix + D60->D65 CAT -- but
     * JMh->RGB is a nonlinear CAM inverse, so that detour is not equivalent and
     * diverges most at the blue cusp (up to dE_ITP ~8 at hue ~276). Decoding in
     * the limit primaries is linear-exact vs OCIO across all hues (verified). */
    alwan_aces_jmh_to_rgb20_f64(rgb_out, &jmh_gc, limit_primaries);
    return ALWAN_OK;
}

alwan_status alwan_aces2_output_transform_custom_f64(alwan_rgb_f64 *rgb_out,
                                             alwan_rgb_f64 const *rgb_in,
                                             alwan_f64 peak_luminance,
                                             alwan_aces_primaries_f64 const *limit_primaries,
                                             alwan_transfer_function eotf) {
    int status;
    alwan_rgb_f64 rgb_adapted;
    status = alwan_aces2_output_transform_custom_display_linear_f64(&rgb_adapted, rgb_in, peak_luminance, limit_primaries);
    if (status != ALWAN_OK) return status;

    /* Step 8: Clamp and scale for display encoding */
    alwan_rgb_f64 rgb_clamped;
    if (eotf == ALWAN_TF_PQ) {
        /* PQ: rgb_adapted is display-linear normalised to the ACES2 reference
         * luminance n_r = 100 nits (NOT to peak). Convert to absolute nits with
         * n_r, then clamp to the display peak; the PQ OETF normalises by 10000.
         * (Scaling by peak over-brightened every non-100-nit output by
         * peak/100 -- e.g. 10x for the 1000-nit PQ outputs.) */
        alwan_f64 const nr = ALWAN_LITERAL(100.0);
        alwan_f64 nits_r = rgb_adapted.r * nr, nits_g = rgb_adapted.g * nr, nits_b = rgb_adapted.b * nr;
        rgb_clamped.r = nits_r < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (nits_r > peak_luminance ? peak_luminance : nits_r);
        rgb_clamped.g = nits_g < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (nits_g > peak_luminance ? peak_luminance : nits_g);
        rgb_clamped.b = nits_b < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) :
                        (nits_b > peak_luminance ? peak_luminance : nits_b);
    } else if (eotf == ALWAN_TF_HLG) {
        /* HLG: rgb_adapted is display-linear in n_r=100 units. Normalise to
         * [0,1] by the display peak, then apply the inverse OOTF (display->scene)
         * -- E = Yd^((1-g)/g) * Fd with Lw=1 -- so the basic HLG OETF applied by
         * alwan_oetf_apply below yields the signal. System gamma per BT.2100:
         * 1.2 + 0.42*log10(peak/1000) (= 1.2 at 1000 nits). Clamping to [0,1]
         * directly (the old code) treated display-linear as scene signal and
         * skipped both the peak normalisation and the OOTF. */
        alwan_f64 sc = ALWAN_LITERAL(100.0) / peak_luminance;
        alwan_f64 fr = rgb_adapted.r * sc, fg = rgb_adapted.g * sc, fb = rgb_adapted.b * sc;
        fr = fr < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (fr > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : fr);
        fg = fg < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (fg > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : fg);
        fb = fb < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (fb > ALWAN_LITERAL(1.0) ? ALWAN_LITERAL(1.0) : fb);
        alwan_f64 gamma = ALWAN_LITERAL(1.2)
            + ALWAN_LITERAL(0.42) * (ALWAN_LN(peak_luminance / ALWAN_LITERAL(1000.0)) / ALWAN_LN(ALWAN_LITERAL(10.0)));
        alwan_f64 Yd = ALWAN_LITERAL(0.2627) * fr + ALWAN_LITERAL(0.6780) * fg + ALWAN_LITERAL(0.0593) * fb;
        if (Yd < ALWAN_LITERAL(1e-12)) Yd = ALWAN_LITERAL(1e-12);
        alwan_f64 factor = ALWAN_POW(Yd, (ALWAN_LITERAL(1.0) - gamma) / gamma);
        rgb_clamped.r = factor * fr;
        rgb_clamped.g = factor * fg;
        rgb_clamped.b = factor * fb;
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
    alwan_f64 linear[3] = {rgb_clamped.r, rgb_clamped.g, rgb_clamped.b};
    alwan_f64 encoded[3];

    status = alwan_oetf_apply_f64(encoded, sizeof(alwan_f64), linear, sizeof(alwan_f64), 3, eotf);
    if (status != ALWAN_OK) {
        /* The caller asked for display-encoded output. Returning the scene-linear
         * values with ALWAN_OK would hand back a picture that is wrong by a whole
         * transfer function and say it succeeded. */
        return status;
    }

    rgb_out->r = encoded[0];
    rgb_out->g = encoded[1];
    rgb_out->b = encoded[2];

    return ALWAN_OK;
}

alwan_status alwan_aces2_output_transform_custom_display_linear_f32(alwan_rgb_f32 *rgb_out,
                                             alwan_rgb_f32 const *rgb_in,
                                             alwan_f32 peak_luminance,
                                             alwan_aces_primaries_f32 const *limit_primaries) {
    if (!rgb_out || !rgb_in || !limit_primaries) return ALWAN_E_INVALID;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_primaries_f64 lp64;
    lp64.red_x = limit_primaries->red_x; lp64.red_y = limit_primaries->red_y;
    lp64.green_x = limit_primaries->green_x; lp64.green_y = limit_primaries->green_y;
    lp64.blue_x = limit_primaries->blue_x; lp64.blue_y = limit_primaries->blue_y;
    lp64.white_x = limit_primaries->white_x; lp64.white_y = limit_primaries->white_y;
    int s = alwan_aces2_output_transform_custom_display_linear_f64(&out64, &in64, (double)peak_luminance, &lp64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
    return s;
}

alwan_status alwan_aces2_output_transform_custom_f32(alwan_rgb_f32 *rgb_out,
                                             alwan_rgb_f32 const *rgb_in,
                                             alwan_f32 peak_luminance,
                                             alwan_aces_primaries_f32 const *limit_primaries,
                                             alwan_transfer_function eotf) {
    if (!rgb_out || !rgb_in || !limit_primaries) return ALWAN_E_INVALID;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_primaries_f64 lp64;
    lp64.red_x = limit_primaries->red_x; lp64.red_y = limit_primaries->red_y;
    lp64.green_x = limit_primaries->green_x; lp64.green_y = limit_primaries->green_y;
    lp64.blue_x = limit_primaries->blue_x; lp64.blue_y = limit_primaries->blue_y;
    lp64.white_x = limit_primaries->white_x; lp64.white_y = limit_primaries->white_y;
    int s = alwan_aces2_output_transform_custom_f64(&out64, &in64, (double)peak_luminance, &lp64, eotf);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
    return s;
}

/* ----------------------------------------------------------------
 * ACES 2.0 Batch Output Transform -- native f32 and f64 via .inc
 * Pre-initializes all parameters ONCE, then processes N pixels.
 * ~100x faster than calling alwan_aces2_output_transform per pixel.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_aces2_map_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE
 * (the aces2 inverse f32 facade calls the f64 core generated here). */
#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#include "alwan_aces2_map_impl.inc"
#include "alwan_api_teardown.h"
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F64
alwan_status alwan_aces2_output_transform_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_aces2_output output) {
    return aces2_ot_map_impl_f64(out, in, output, count, in_stride, out_stride);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_aces2_output_transform_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_aces2_output output) {
    return aces2_ot_map_impl_f32(out, in, output, count, in_stride, out_stride);
}
#endif

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
alwan_status alwan_aces2_output_transform_inv_f64(alwan_rgb_f64 *rgb_out,
                                          alwan_rgb_f64 const *rgb_in,
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
        alwan_f64 xyz[3];
        xyz[0] = aces_gamma26_eotf_f64_v(rgb_in->r);
        xyz[1] = aces_gamma26_eotf_f64_v(rgb_in->g);
        xyz[2] = aces_gamma26_eotf_f64_v(rgb_in->b);

        /* Step 2: De-normalize from equal-energy white to D60
         * This is the inverse of the forward transform's normalization */
        static const alwan_f64 D60_WHITE_X = ALWAN_LITERAL(0.952646074569846);
        static const alwan_f64 D60_WHITE_Z = ALWAN_LITERAL(1.008825184351586);
        xyz[0] *= D60_WHITE_X;
        xyz[2] *= D60_WHITE_Z;

        /* Step 3: Convert XYZ (D60) to AP1 */
        static const alwan_f64 XYZ_D60_TO_AP1[9] = {
            ALWAN_LITERAL( 1.6410233797), ALWAN_LITERAL(-0.3248032942), ALWAN_LITERAL(-0.2364246952),
            ALWAN_LITERAL(-0.6636628587), ALWAN_LITERAL( 1.6153315917), ALWAN_LITERAL( 0.0167563477),
            ALWAN_LITERAL( 0.0030476112), ALWAN_LITERAL(-0.0164295295), ALWAN_LITERAL( 0.9888322028)
        };

        alwan_rgb_f64 rgb_ap1;
        rgb_ap1.r = XYZ_D60_TO_AP1[0] * xyz[0] + XYZ_D60_TO_AP1[1] * xyz[1] + XYZ_D60_TO_AP1[2] * xyz[2];
        rgb_ap1.g = XYZ_D60_TO_AP1[3] * xyz[0] + XYZ_D60_TO_AP1[4] * xyz[1] + XYZ_D60_TO_AP1[5] * xyz[2];
        rgb_ap1.b = XYZ_D60_TO_AP1[6] * xyz[0] + XYZ_D60_TO_AP1[7] * xyz[1] + XYZ_D60_TO_AP1[8] * xyz[2];

        /* Step 5: Convert to JMh for inverse gamut compression */
        alwan_aces_primaries_f64 ap1;
        alwan_aces_primaries_ap1_default_f64(&ap1);

        alwan_aces_primaries_f64 p3_d65;
        primaries_p3_d65(&p3_d65);

        alwan_vec3_f64 jmh;
        alwan_aces_rgb_to_jmh20_f64(&jmh, &rgb_ap1, &ap1);

        /* Step 6: Inverse gamut compression using P3-D65 */
        alwan_vec3_f64 jmh_exp;
        alwan_aces_gamut_compress20_inv_f64(&jmh_exp, &jmh, config.peak_luminance, &p3_d65);

        /* Step 7: Convert JMh back to AP1 RGB */
        alwan_aces_jmh_to_rgb20_f64(rgb_out, &jmh_exp, &ap1);

        return ALWAN_OK;
    }

    if (output == ALWAN_ACES2_OUT_P3DCI_48NIT) {
        /* P3-DCI inverse: decode gamma 2.6, P3 to AP1 */

        /* Step 1: Decode gamma 2.6 */
        alwan_f64 p3_linear[3];
        p3_linear[0] = aces_gamma26_eotf_f64_v(rgb_in->r);
        p3_linear[1] = aces_gamma26_eotf_f64_v(rgb_in->g);
        p3_linear[2] = aces_gamma26_eotf_f64_v(rgb_in->b);

        /* Step 2: P3 to XYZ (D65) */
        static const alwan_f64 P3_D65_TO_XYZ[9] = {
            ALWAN_LITERAL(0.4865709486), ALWAN_LITERAL(0.2656676932), ALWAN_LITERAL(0.1982172852),
            ALWAN_LITERAL(0.2289745641), ALWAN_LITERAL(0.6917385218), ALWAN_LITERAL(0.0792869141),
            ALWAN_LITERAL(0.0000000000), ALWAN_LITERAL(0.0451133819), ALWAN_LITERAL(1.0439443689)
        };

        alwan_f64 xyz_d65[3];
        xyz_d65[0] = P3_D65_TO_XYZ[0] * p3_linear[0] + P3_D65_TO_XYZ[1] * p3_linear[1] + P3_D65_TO_XYZ[2] * p3_linear[2];
        xyz_d65[1] = P3_D65_TO_XYZ[3] * p3_linear[0] + P3_D65_TO_XYZ[4] * p3_linear[1] + P3_D65_TO_XYZ[5] * p3_linear[2];
        xyz_d65[2] = P3_D65_TO_XYZ[6] * p3_linear[0] + P3_D65_TO_XYZ[7] * p3_linear[1] + P3_D65_TO_XYZ[8] * p3_linear[2];

        /* Step 3: Apply D65 to D60 chromatic adaptation */
        alwan_f64 xyz_d60[3];
        xyz_d60[0] = ACES1_D65_TO_D60[0] * xyz_d65[0] + ACES1_D65_TO_D60[1] * xyz_d65[1] + ACES1_D65_TO_D60[2] * xyz_d65[2];
        xyz_d60[1] = ACES1_D65_TO_D60[3] * xyz_d65[0] + ACES1_D65_TO_D60[4] * xyz_d65[1] + ACES1_D65_TO_D60[5] * xyz_d65[2];
        xyz_d60[2] = ACES1_D65_TO_D60[6] * xyz_d65[0] + ACES1_D65_TO_D60[7] * xyz_d65[1] + ACES1_D65_TO_D60[8] * xyz_d65[2];

        /* Step 4: Convert XYZ (D60) to AP1 */
        static const alwan_f64 XYZ_D60_TO_AP1[9] = {
            ALWAN_LITERAL( 1.6410233797), ALWAN_LITERAL(-0.3248032942), ALWAN_LITERAL(-0.2364246952),
            ALWAN_LITERAL(-0.6636628587), ALWAN_LITERAL( 1.6153315917), ALWAN_LITERAL( 0.0167563477),
            ALWAN_LITERAL( 0.0030476112), ALWAN_LITERAL(-0.0164295295), ALWAN_LITERAL( 0.9888322028)
        };

        alwan_rgb_f64 rgb_ap1;
        rgb_ap1.r = XYZ_D60_TO_AP1[0] * xyz_d60[0] + XYZ_D60_TO_AP1[1] * xyz_d60[1] + XYZ_D60_TO_AP1[2] * xyz_d60[2];
        rgb_ap1.g = XYZ_D60_TO_AP1[3] * xyz_d60[0] + XYZ_D60_TO_AP1[4] * xyz_d60[1] + XYZ_D60_TO_AP1[5] * xyz_d60[2];
        rgb_ap1.b = XYZ_D60_TO_AP1[6] * xyz_d60[0] + XYZ_D60_TO_AP1[7] * xyz_d60[1] + XYZ_D60_TO_AP1[8] * xyz_d60[2];

        /* Step 5: Convert to JMh for inverse gamut compression */
        alwan_aces_primaries_f64 ap1;
        alwan_aces_primaries_ap1_default_f64(&ap1);

        alwan_aces_primaries_f64 p3_d65;
        primaries_p3_d65(&p3_d65);

        alwan_vec3_f64 jmh;
        alwan_aces_rgb_to_jmh20_f64(&jmh, &rgb_ap1, &ap1);

        /* Step 6: Inverse gamut compression using P3-D65 */
        alwan_vec3_f64 jmh_exp;
        alwan_aces_gamut_compress20_inv_f64(&jmh_exp, &jmh, config.peak_luminance, &p3_d65);

        /* Step 7: Convert JMh back to AP1 RGB */
        alwan_aces_jmh_to_rgb20_f64(rgb_out, &jmh_exp, &ap1);

        return ALWAN_OK;
    }

    /* Step 1: Decode display encoding (EOTF) */
    alwan_f64 encoded[3] = {rgb_in->r, rgb_in->g, rgb_in->b};
    alwan_f64 linear[3];

    status = alwan_eotf_apply_f64(linear, sizeof(alwan_f64), encoded, sizeof(alwan_f64), 3, config.eotf);
    if (status != ALWAN_OK) {
        /* Input is display-encoded by construction; treating it as already linear
         * silently skips a transfer function and inverts the wrong signal. */
        return status;
    }

    alwan_rgb_f64 rgb_linear = {linear[0], linear[1], linear[2]};

    /* Step 2: Apply D65 to D60 chromatic adaptation if needed */
    int needs_cat = (ALWAN_ABS(config.primaries.white_x - ALWAN_D65_x) < ALWAN_LITERAL(0.01) &&
                     ALWAN_ABS(config.primaries.white_y - ALWAN_D65_y) < ALWAN_LITERAL(0.01));

    alwan_rgb_f64 rgb_d60;
    if (needs_cat) {
        /* Convert to XYZ */
        alwan_f64 limit_to_xyz[9];
        primaries_to_rgb_to_xyz_f64(
            config.primaries.red_x, config.primaries.red_y,
            config.primaries.green_x, config.primaries.green_y,
            config.primaries.blue_x, config.primaries.blue_y,
            config.primaries.white_x, config.primaries.white_y,
            ALWAN_LITERAL(1.0),
            limit_to_xyz
        );

        alwan_f64 xyz[3];
        xyz[0] = limit_to_xyz[0] * rgb_linear.r + limit_to_xyz[1] * rgb_linear.g + limit_to_xyz[2] * rgb_linear.b;
        xyz[1] = limit_to_xyz[3] * rgb_linear.r + limit_to_xyz[4] * rgb_linear.g + limit_to_xyz[5] * rgb_linear.b;
        xyz[2] = limit_to_xyz[6] * rgb_linear.r + limit_to_xyz[7] * rgb_linear.g + limit_to_xyz[8] * rgb_linear.b;

        /* Apply D65 to D60 CAT */
        alwan_f64 xyz_d60[3];
        xyz_d60[0] = g_d65_to_d60_bradford[0] * xyz[0] + g_d65_to_d60_bradford[1] * xyz[1] + g_d65_to_d60_bradford[2] * xyz[2];
        xyz_d60[1] = g_d65_to_d60_bradford[3] * xyz[0] + g_d65_to_d60_bradford[4] * xyz[1] + g_d65_to_d60_bradford[5] * xyz[2];
        xyz_d60[2] = g_d65_to_d60_bradford[6] * xyz[0] + g_d65_to_d60_bradford[7] * xyz[1] + g_d65_to_d60_bradford[8] * xyz[2];

        /* Convert back to limit RGB */
        alwan_f64 xyz_to_limit[9];
        invert_mat3(limit_to_xyz, xyz_to_limit);

        rgb_d60.r = xyz_to_limit[0] * xyz_d60[0] + xyz_to_limit[1] * xyz_d60[1] + xyz_to_limit[2] * xyz_d60[2];
        rgb_d60.g = xyz_to_limit[3] * xyz_d60[0] + xyz_to_limit[4] * xyz_d60[1] + xyz_to_limit[5] * xyz_d60[2];
        rgb_d60.b = xyz_to_limit[6] * xyz_d60[0] + xyz_to_limit[7] * xyz_d60[1] + xyz_to_limit[8] * xyz_d60[2];
    } else {
        rgb_d60 = rgb_linear;
    }

    /* Step 3: Convert from limit primaries to AP1 */
    alwan_f64 limit_to_ap1[9];
    compute_limit_to_ap1_matrix(&config.primaries, limit_to_ap1);

    alwan_rgb_f64 rgb_ap1;
    apply_matrix_rgb(limit_to_ap1, &rgb_d60, &rgb_ap1);

    /* Step 4: Convert to JMh */
    alwan_aces_primaries_f64 ap1;
    alwan_aces_primaries_ap1_default_f64(&ap1);

    alwan_vec3_f64 jmh;
    alwan_aces_rgb_to_jmh20_f64(&jmh, &rgb_ap1, &ap1);

    /* Step 5: Inverse gamut compression */
    alwan_vec3_f64 jmh_exp;
    alwan_aces_gamut_compress20_inv_f64(&jmh_exp, &jmh, config.peak_luminance, &config.primaries);

    /* Step 6: Convert expanded JMh back to AP1 RGB.
     * Inverse tonescale is not applied (gamut expansion only). */
    alwan_aces_jmh_to_rgb20_f64(rgb_out, &jmh_exp, &ap1);

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
alwan_status alwan_aces2_output_transform_inv_f32(alwan_rgb_f32 *rgb_out,
                                          alwan_rgb_f32 const *rgb_in,
                                          alwan_aces2_output output) {
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    int s = alwan_aces2_output_transform_inv_f64(&out64, &in64, output);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
    return s;
}
#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * Blue Light Artifact Fix (Neon Suppression) LMT
 * Reference: ACES CLF (urn:ampas:aces:transformId:v1.5:LMT.Academy.BlueLightArtifactFix.a1.1.0)
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
static alwan_f64 const BLUE_FIX_MATRIX[9] = {
    ALWAN_LITERAL(0.9404372683),  ALWAN_LITERAL(-0.0183068787), ALWAN_LITERAL(0.0778696104),
    ALWAN_LITERAL(0.0083786969),  ALWAN_LITERAL(0.8286599939),  ALWAN_LITERAL(0.1629613092),
    ALWAN_LITERAL(0.0005471261),  ALWAN_LITERAL(-0.0008833746), ALWAN_LITERAL(1.000336248)
};

static alwan_f64 const BLUE_FIX_MATRIX_INV[9] = {
    ALWAN_LITERAL(1.0631770724326068),  ALWAN_LITERAL(0.0233955757076458),  ALWAN_LITERAL(-0.0865726481835390),
    ALWAN_LITERAL(-0.0106337301402402), ALWAN_LITERAL(1.2063240280839118),  ALWAN_LITERAL(-0.1956902980415168),
    ALWAN_LITERAL(-0.0005908868078512), ALWAN_LITERAL(0.0010524817807910),  ALWAN_LITERAL(0.9995384055268296)
};
#endif /* ALWAN_WITH_F64 */

/* Native f32 twins for the single-precision path. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static float const BLUE_FIX_MATRIX_f32[9] = {
    0.9404372683f,  -0.0183068787f, 0.0778696104f,
    0.0083786969f,  0.8286599939f,  0.1629613092f,
    0.0005471261f,  -0.0008833746f, 1.000336248f
};

static float const BLUE_FIX_MATRIX_INV_f32[9] = {
    1.0631770724326068f,  0.0233955757076458f,  -0.0865726481835390f,
    -0.0106337301402402f, 1.2063240280839118f,  -0.1956902980415168f,
    -0.0005908868078512f, 0.0010524817807910f,  0.9995384055268296f
};
ALWAN_DIAG_POP
#endif /* ALWAN_WITH_F32 */

#if ALWAN_WITH_F64
void alwan_aces_blue_light_fix_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_f64 r = rgb_in->r, g = rgb_in->g, b = rgb_in->b;
    rgb_out->r = BLUE_FIX_MATRIX[0] * r + BLUE_FIX_MATRIX[1] * g + BLUE_FIX_MATRIX[2] * b;
    rgb_out->g = BLUE_FIX_MATRIX[3] * r + BLUE_FIX_MATRIX[4] * g + BLUE_FIX_MATRIX[5] * b;
    rgb_out->b = BLUE_FIX_MATRIX[6] * r + BLUE_FIX_MATRIX[7] * g + BLUE_FIX_MATRIX[8] * b;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
void alwan_aces_blue_light_fix_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    float r = rgb_in->r, g = rgb_in->g, b = rgb_in->b;
    rgb_out->r = BLUE_FIX_MATRIX_f32[0] * r + BLUE_FIX_MATRIX_f32[1] * g + BLUE_FIX_MATRIX_f32[2] * b;
    rgb_out->g = BLUE_FIX_MATRIX_f32[3] * r + BLUE_FIX_MATRIX_f32[4] * g + BLUE_FIX_MATRIX_f32[5] * b;
    rgb_out->b = BLUE_FIX_MATRIX_f32[6] * r + BLUE_FIX_MATRIX_f32[7] * g + BLUE_FIX_MATRIX_f32[8] * b;
}
#endif /* ALWAN_WITH_F32 */

#if ALWAN_WITH_F64
void alwan_aces_blue_light_fix_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_f64 r = rgb_in->r, g = rgb_in->g, b = rgb_in->b;
    rgb_out->r = BLUE_FIX_MATRIX_INV[0] * r + BLUE_FIX_MATRIX_INV[1] * g + BLUE_FIX_MATRIX_INV[2] * b;
    rgb_out->g = BLUE_FIX_MATRIX_INV[3] * r + BLUE_FIX_MATRIX_INV[4] * g + BLUE_FIX_MATRIX_INV[5] * b;
    rgb_out->b = BLUE_FIX_MATRIX_INV[6] * r + BLUE_FIX_MATRIX_INV[7] * g + BLUE_FIX_MATRIX_INV[8] * b;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
void alwan_aces_blue_light_fix_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    float r = rgb_in->r, g = rgb_in->g, b = rgb_in->b;
    rgb_out->r = BLUE_FIX_MATRIX_INV_f32[0] * r + BLUE_FIX_MATRIX_INV_f32[1] * g + BLUE_FIX_MATRIX_INV_f32[2] * b;
    rgb_out->g = BLUE_FIX_MATRIX_INV_f32[3] * r + BLUE_FIX_MATRIX_INV_f32[4] * g + BLUE_FIX_MATRIX_INV_f32[5] * b;
    rgb_out->b = BLUE_FIX_MATRIX_INV_f32[6] * r + BLUE_FIX_MATRIX_INV_f32[7] * g + BLUE_FIX_MATRIX_INV_f32[8] * b;
}
#endif /* ALWAN_WITH_F32 */

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
static int glow_inv_impl(alwan_rgb_f64 const *rgb_in, alwan_f64 glow_gain_param,
                         alwan_f64 glow_mid_param, alwan_rgb_f64 *rgb_out) {
    alwan_f64 red_target = rgb_in->r;
    alwan_f64 grn_target = rgb_in->g;
    alwan_f64 blu_target = rgb_in->b;

    alwan_f64 GlowMid = glow_mid_param;

    /* Initial estimate using OCIO analytical formula */
    alwan_f64 YC = rgb_to_yc(red_target, grn_target, blu_target);
    alwan_f64 sat = calc_sat_weight(red_target, grn_target, blu_target, REDMOD_NOISE_LIMIT);
    alwan_f64 s = sigmoid_shaper(sat);
    alwan_f64 GlowGain = glow_gain_param * s;

    /* OCIO piecewise analytical formula for initial estimate */
    alwan_f64 glowGainOut;
    if (YC >= GlowMid * ALWAN_LITERAL(2.0)) {
        glowGainOut = ALWAN_LITERAL(0.0);
    } else if (YC <= (ALWAN_LITERAL(1.0) + GlowGain) * GlowMid * ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
        glowGainOut = -GlowGain / (ALWAN_LITERAL(1.0) + GlowGain);
    } else {
        glowGainOut = GlowGain * (GlowMid / YC - ALWAN_LITERAL(0.5)) / (GlowGain * ALWAN_LITERAL(0.5) - ALWAN_LITERAL(1.0));
    }

    alwan_f64 scale = ALWAN_LITERAL(1.0) + glowGainOut;
    alwan_f64 red = red_target * scale;
    alwan_f64 grn = grn_target * scale;
    alwan_f64 blu = blu_target * scale;

    /* Iterative refinement: apply forward and correct by dividing by scale.
     * This converges quickly because rgb_out = rgb_in * scale, so rgb_in = rgb_out / scale */
    for (int iter = 0; iter < 16; ++iter) {
        alwan_f64 YC_iter = rgb_to_yc(red, grn, blu);
        alwan_f64 sat_iter = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
        alwan_f64 s_iter = sigmoid_shaper(sat_iter);
        alwan_f64 glow_iter = glow_gain_param * s_iter;

        alwan_f64 glow_out_iter;
        if (YC_iter >= GlowMid * ALWAN_LITERAL(2.0)) {
            glow_out_iter = ALWAN_LITERAL(0.0);
        } else if (YC_iter <= GlowMid * ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) {
            glow_out_iter = glow_iter;
        } else {
            glow_out_iter = glow_iter * (GlowMid / YC_iter - ALWAN_LITERAL(0.5));
        }

        alwan_f64 fwd_scale = ALWAN_LITERAL(1.0) + glow_out_iter;
        alwan_f64 red_fwd = red * fwd_scale;
        alwan_f64 grn_fwd = grn * fwd_scale;
        alwan_f64 blu_fwd = blu * fwd_scale;

        alwan_f64 err_r = red_fwd - red_target;
        alwan_f64 err_g = grn_fwd - grn_target;
        alwan_f64 err_b = blu_fwd - blu_target;

        alwan_f64 max_err = ALWAN_ABS(err_r);
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

void alwan_aces_glow03_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    glow_inv_impl(rgb_in, GLOW03_GAIN, GLOW03_MID, rgb_out);
}

void alwan_aces_glow03_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_glow03_inv_f64(&out64, &in64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
}

void alwan_aces_glow10_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    glow_inv_impl(rgb_in, GLOW10_GAIN, GLOW10_MID, rgb_out);
}

void alwan_aces_glow10_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_glow10_inv_f64(&out64, &in64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
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
void alwan_aces_redmod10_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    alwan_f64 red_target = rgb_in->r;
    alwan_f64 grn = rgb_in->g;
    alwan_f64 blu = rgb_in->b;

    alwan_f64 f_H = calc_hue_weight(red_target, grn, blu, REDMOD10_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_f64 minChan = (grn < blu) ? grn : blu;
        alwan_f64 one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD10_SCALE;

        /* Initial estimate using quadratic formula (OCIO approach) */
        alwan_f64 a = f_H * one_minus_scale - ALWAN_LITERAL(1.0);
        alwan_f64 b = red_target - f_H * (REDMOD10_PIVOT + minChan) * one_minus_scale;
        alwan_f64 c = f_H * REDMOD10_PIVOT * minChan * one_minus_scale;
        alwan_f64 red = (-b - ALWAN_SQRT(b * b - ALWAN_LITERAL(4.0) * a * c)) / (ALWAN_LITERAL(2.0) * a);

        /* Iterative refinement using Newton's method.
         * Forward: r' = r + f_H * f_S * (P - r) * k = r * (1 - f_H * f_S * k) + f_H * f_S * P * k
         * Derivative: dr'/dr ~= 1 - f_H * f_S * k (ignoring d(f_H)/dr and d(f_S)/dr)
         * Newton: r_new = r - (f(r) - target) / f'(r) */
        for (int iter = 0; iter < 16; ++iter) {
            alwan_f64 f_H_iter = calc_hue_weight(red, grn, blu, REDMOD10_INV_WIDTH);
            alwan_f64 f_S_iter = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
            alwan_f64 mod = f_H_iter * f_S_iter * one_minus_scale;
            alwan_f64 red_fwd = red + mod * (REDMOD10_PIVOT - red);
            alwan_f64 error = red_fwd - red_target;
            if (ALWAN_ABS(error) < ALWAN_LITERAL(1e-15)) break;
            /* Approximate derivative: 1 - f_H * f_S * k */
            alwan_f64 deriv = ALWAN_LITERAL(1.0) - mod;
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
}

void alwan_aces_redmod10_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_redmod10_inv_f64(&out64, &in64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
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
void alwan_aces_redmod03_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    alwan_f64 red_target = rgb_in->r;
    alwan_f64 grn_target = rgb_in->g;
    alwan_f64 blu_target = rgb_in->b;

    alwan_f64 f_H = calc_hue_weight(red_target, grn_target, blu_target, REDMOD03_INV_WIDTH);

    if (f_H > ALWAN_LITERAL(0.0)) {
        alwan_f64 one_minus_scale = ALWAN_LITERAL(1.0) - REDMOD03_SCALE;

        /* Determine which channel was modified (grn or blu) and compute hue factor */
        int grn_modified = (grn_target >= blu_target);
        alwan_f64 hue_fac;
        if (grn_modified) {
            hue_fac = (grn_target - blu_target) / fmax(ALWAN_LITERAL(1e-10), red_target - blu_target);
        } else {
            hue_fac = (blu_target - grn_target) / fmax(ALWAN_LITERAL(1e-10), red_target - grn_target);
        }

        /* Initial estimate using quadratic formula */
        alwan_f64 minChan = grn_modified ? blu_target : grn_target;
        alwan_f64 a = f_H * one_minus_scale - ALWAN_LITERAL(1.0);
        alwan_f64 b = red_target - f_H * (REDMOD03_PIVOT + minChan) * one_minus_scale;
        alwan_f64 c = f_H * REDMOD03_PIVOT * minChan * one_minus_scale;
        alwan_f64 red = (-b - ALWAN_SQRT(b * b - ALWAN_LITERAL(4.0) * a * c)) / (ALWAN_LITERAL(2.0) * a);

        /* Compute initial grn/blu from hue restoration */
        alwan_f64 grn, blu;
        if (grn_modified) {
            blu = blu_target;
            grn = hue_fac * (red - blu) + blu;
        } else {
            grn = grn_target;
            blu = hue_fac * (red - grn) + grn;
        }

        /* Iterative refinement using Newton's method */
        for (int iter = 0; iter < 16; ++iter) {
            alwan_f64 f_H_iter = calc_hue_weight(red, grn, blu, REDMOD03_INV_WIDTH);
            alwan_f64 f_S_iter = calc_sat_weight(red, grn, blu, REDMOD_NOISE_LIMIT);
            alwan_f64 mod = f_H_iter * f_S_iter * one_minus_scale;
            alwan_f64 red_fwd = red + mod * (REDMOD03_PIVOT - red);
            alwan_f64 error = red_fwd - red_target;
            if (ALWAN_ABS(error) < ALWAN_LITERAL(1e-15)) break;
            /* Approximate derivative: 1 - f_H * f_S * k */
            alwan_f64 deriv = ALWAN_LITERAL(1.0) - mod;
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
}

void alwan_aces_redmod03_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_redmod03_inv_f64(&out64, &in64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
}

/* ----------------------------------------------------------------
 * ACES 1.0 Look LMT
 * Emulates ACES 1.0 look when used with ACES 1.0.3+ RRT
 * Applies: Glow10_inv -> Glow03 -> RedMod10_inv -> RedMod03
 * ---------------------------------------------------------------- */

void alwan_aces_look_1_0_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    alwan_rgb_f64 temp1, temp2, temp3;

    /* Step 1: Undo Glow10 */
    alwan_aces_glow10_inv_f64(&temp1, rgb_in);

    /* Step 2: Apply Glow03 */
    alwan_aces_glow03_f64(&temp2, &temp1);

    /* Step 3: Undo RedMod10 */
    alwan_aces_redmod10_inv_f64(&temp3, &temp2);

    /* Step 4: Apply RedMod03 */
    alwan_aces_redmod03_f64(rgb_out, &temp3);
}

void alwan_aces_look_1_0_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_look_1_0_f64(&out64, &in64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
}

/* Inverse of ACES 1.0 Look LMT */
void alwan_aces_look_1_0_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;

    alwan_rgb_f64 temp1, temp2, temp3;

    /* Reverse order: RedMod03_inv -> RedMod10 -> Glow03_inv -> Glow10 */

    /* Step 1: Undo RedMod03 */
    alwan_aces_redmod03_inv_f64(&temp1, rgb_in);

    /* Step 2: Apply RedMod10 */
    alwan_aces_redmod10_f64(&temp2, &temp1);

    /* Step 3: Undo Glow03 */
    alwan_aces_glow03_inv_f64(&temp3, &temp2);

    /* Step 4: Apply Glow10 */
    alwan_aces_glow10_f64(rgb_out, &temp3);
}

void alwan_aces_look_1_0_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in) {
    if (!rgb_out || !rgb_in) return;
    alwan_rgb_f64 in64 = {(alwan_f64)rgb_in->r, (alwan_f64)rgb_in->g, (alwan_f64)rgb_in->b};
    alwan_rgb_f64 out64;
    alwan_aces_look_1_0_inv_f64(&out64, &in64);
    rgb_out->r = (float)out64.r; rgb_out->g = (float)out64.g; rgb_out->b = (float)out64.b;
}

/* ----------------------------------------------------------------
 * Parametric LMT (CDL-style color grading)
 * ---------------------------------------------------------------- */

void alwan_aces_lmt_params_init_f32(alwan_aces_lmt_params_f32 *params) {
    if (!params) return;
    params->slope[0] = params->slope[1] = params->slope[2] = 1.0f;
    params->offset[0] = params->offset[1] = params->offset[2] = 0.0f;
    params->power[0] = params->power[1] = params->power[2] = 1.0f;
    params->saturation = 1.0f;
}

void alwan_aces_lmt_params_init_f64(alwan_aces_lmt_params_f64 *params) {
    if (!params) return;
    params->slope[0] = params->slope[1] = params->slope[2] = 1.0;
    params->offset[0] = params->offset[1] = params->offset[2] = 0.0;
    params->power[0] = params->power[1] = params->power[2] = 1.0;
    params->saturation = 1.0;
}

void alwan_aces_lmt_apply_f64(alwan_rgb_f64 *rgb_out,
                         alwan_rgb_f64 const *rgb_in,
                         alwan_aces_lmt_params_f64 const *params) {
    if (!rgb_out || !rgb_in || !params) return;

    alwan_vec3_f64 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3_f64 slope_v = {{params->slope[0], params->slope[1], params->slope[2]}};
    alwan_vec3_f64 offset_v = {{params->offset[0], params->offset[1], params->offset[2]}};
    alwan_vec3_f64 power_v = {{params->power[0], params->power[1], params->power[2]}};
    alwan_vec3_f64 out_v = aces_lmt_apply_f64_v(in_v, slope_v, offset_v, power_v, params->saturation);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
}

void alwan_aces_lmt_apply_f32(alwan_rgb_f32 *rgb_out,
                         alwan_rgb_f32 const *rgb_in,
                         alwan_aces_lmt_params_f32 const *params) {
    if (!rgb_out || !rgb_in || !params) return;

    alwan_vec3_f32 in_v = {{rgb_in->r, rgb_in->g, rgb_in->b}};
    alwan_vec3_f32 slope_v = {{params->slope[0], params->slope[1], params->slope[2]}};
    alwan_vec3_f32 offset_v = {{params->offset[0], params->offset[1], params->offset[2]}};
    alwan_vec3_f32 power_v = {{params->power[0], params->power[1], params->power[2]}};
    alwan_vec3_f32 out_v = aces_lmt_apply_f32_v(in_v, slope_v, offset_v, power_v, params->saturation);
    rgb_out->r = out_v.v[0];
    rgb_out->g = out_v.v[1];
    rgb_out->b = out_v.v[2];
}
