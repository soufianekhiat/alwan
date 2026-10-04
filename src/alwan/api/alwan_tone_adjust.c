/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Per-pixel tonal adjustments: levels, posterize, solarize, sigmoidal contrast and its
 * inverse, and modulate (brightness, saturation, hue). Every one maps a pixel's values
 * to new values with no reference to its neighbours or to the image's statistics; the
 * adjustments that need statistics (auto-level, auto-gamma, autocontrast with a cutoff)
 * are image processing and live in suwar.
 *
 * Formulas:
 *   LEVELS      v' = out_black + (out_white - out_black) * g((v - in_black) / (in_white - in_black)),
 *               g(x) = x^(1/gamma) for x >= 0 and x for x < 0 (ImageMagick's gamma_pow),
 *               no clamp unless asked.
 *   POSTERIZE   n levels: floor(v (n - 1) + 0.5) / (n - 1), ImageMagick's -posterize
 *               (MagickRound). pillow_bits keeps Pillow's ImageOps.posterize instead: the
 *               top bits of the 8-bit code, which is a floor, not n even levels.
 *   SOLARIZE    v >= threshold becomes 1 - v, as Pillow's ImageOps.solarize (ImageMagick's
 *               -solarize inverts only v > threshold).
 *   SIGMOIDAL   ImageMagick's -sigmoidal-contrast: Sig(x) = 1 / (1 + exp(a (b - x))),
 *               (Sig(v) - Sig(0)) / (Sig(1) - Sig(0)); the inverse is its exact inverse,
 *               with the logistic's argument limited to (eps, 1 - eps) as ImageMagick does.
 *   MODULATE    OKLCH (the default) scales Oklab L and chroma and turns the hue; HSL and
 *               HSV are ImageMagick's -modulate in those models.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

#define ALWAN__TA_PI 3.14159265358979323846

typedef struct {
    double in_black, in_scale, gamma_inv, out_black, out_span;
    int clamp;
    double post_n;
    int pillow_bits;
    double threshold;
    double sig_a, sig_b, sig0, sig1, sig_span;
    alwan_modulate_space space;
    double bright, sat, hue_turns;
    int input_linear;
} alwan__ta;

static double alwan__ta_sig(double a, double b, double x) {
    return 1.0 / (1.0 + ALWAN_EXP_F64(a * (b - x)));
}

static alwan_status alwan__ta_prepare(alwan__ta *t, alwan_tone_adjust_method method, alwan_tone_adjust_params const *pp) {
    alwan_tone_adjust_params p;
    if (pp) p = *pp;
    else memset(&p, 0, sizeof p);
    memset(t, 0, sizeof *t);
    switch (method) {
    case ALWAN_TONE_ADJUST_LEVELS: {
        double const iw = p.in_white == 0.0 ? 1.0 : p.in_white;
        double const g = p.gamma == 0.0 ? 1.0 : p.gamma;
        double const ow = p.out_white == 0.0 ? 1.0 : p.out_white;
        if (!(iw - iw == 0.0) || !(p.in_black - p.in_black == 0.0) || iw == p.in_black) return ALWAN_E_RANGE;
        if (!(g > 0.0) || !(g - g == 0.0)) return ALWAN_E_RANGE;
        if (!(ow - ow == 0.0) || !(p.out_black - p.out_black == 0.0)) return ALWAN_E_RANGE;
        t->in_black = p.in_black;
        t->in_scale = 1.0 / (iw - p.in_black);
        t->gamma_inv = 1.0 / g;
        t->out_black = p.out_black;
        t->out_span = ow - p.out_black;
        t->clamp = p.clamp != 0;
        return ALWAN_OK;
    }
    case ALWAN_TONE_ADJUST_POSTERIZE: {
        int const n = p.levels == 0 ? 4 : p.levels;
        if (p.pillow_bits) {
            if (p.pillow_bits < 1 || p.pillow_bits > 8) return ALWAN_E_RANGE;
            t->pillow_bits = p.pillow_bits;
            return ALWAN_OK;
        }
        if (n < 2 || n > 65536) return ALWAN_E_RANGE;
        t->post_n = (double)n;
        return ALWAN_OK;
    }
    case ALWAN_TONE_ADJUST_SOLARIZE:
        t->threshold = p.threshold == 0.0 ? 0.5 : p.threshold;
        if (!(t->threshold - t->threshold == 0.0)) return ALWAN_E_RANGE;
        return ALWAN_OK;
    case ALWAN_TONE_ADJUST_SIGMOIDAL_CONTRAST:
    case ALWAN_TONE_ADJUST_SIGMOIDAL_CONTRAST_INVERSE:
        if (!(p.contrast - p.contrast == 0.0) || p.contrast < 0.0 || p.contrast > 1000.0) return ALWAN_E_RANGE;
        t->sig_a = p.contrast;
        t->sig_b = p.midpoint == 0.0 ? 0.5 : p.midpoint;
        if (!(t->sig_b - t->sig_b == 0.0)) return ALWAN_E_RANGE;
        if (t->sig_a > 0.0) {
            t->sig0 = alwan__ta_sig(t->sig_a, t->sig_b, 0.0);
            t->sig1 = alwan__ta_sig(t->sig_a, t->sig_b, 1.0);
            t->sig_span = t->sig1 - t->sig0;
            if (!(t->sig_span > 0.0)) return ALWAN_E_RANGE;
        }
        return ALWAN_OK;
    case ALWAN_TONE_ADJUST_MODULATE:
        if ((unsigned)p.space > (unsigned)ALWAN_MODULATE_HSV) return ALWAN_E_INVALID;
        if (!(p.brightness - p.brightness == 0.0) || !(p.saturation - p.saturation == 0.0) || !(p.hue - p.hue == 0.0))
            return ALWAN_E_RANGE;
        if (p.brightness < -1.0 || p.saturation < -1.0) return ALWAN_E_RANGE;
        t->space = p.space;
        t->bright = 1.0 + p.brightness;
        t->sat = 1.0 + p.saturation;
        t->hue_turns = 0.5 * p.hue; /* signed-range rule: hue / pi in [-1, 1] */
        t->input_linear = p.input_linear != 0;
        return ALWAN_OK;
    default:
        return ALWAN_E_INVALID;
    }
}

static double alwan__ta_scalar(alwan__ta const *t, alwan_tone_adjust_method m, double v) {
    switch (m) {
    case ALWAN_TONE_ADJUST_LEVELS: {
        double x = (v - t->in_black) * t->in_scale;
        double y;
        if (t->clamp) x = x < 0.0 ? 0.0 : (x > 1.0 ? 1.0 : x);
        y = x < 0.0 ? x : (t->gamma_inv == 1.0 ? x : ALWAN_POW_F64(x, t->gamma_inv));
        return t->out_black + t->out_span * y;
    }
    case ALWAN_TONE_ADJUST_POSTERIZE:
        return ALWAN_FLOOR_F64(v * (t->post_n - 1.0) + 0.5) / (t->post_n - 1.0);
    case ALWAN_TONE_ADJUST_SOLARIZE:
        return v >= t->threshold ? 1.0 - v : v;
    case ALWAN_TONE_ADJUST_SIGMOIDAL_CONTRAST:
        if (t->sig_a == 0.0) return v;
        return (alwan__ta_sig(t->sig_a, t->sig_b, v) - t->sig0) / t->sig_span;
    case ALWAN_TONE_ADJUST_SIGMOIDAL_CONTRAST_INVERSE: {
        double u;
        if (t->sig_a == 0.0) return v;
        u = t->sig_span * v + t->sig0;
        if (u < 1e-12) u = 1e-12;
        if (u > 1.0 - 1e-12) u = 1.0 - 1e-12;
        return t->sig_b - ALWAN_LN_F64(1.0 / u - 1.0) / t->sig_a;
    }
    default:
        return v;
    }
}

static double alwan__ta_wrap(double h) {
    h = h - ALWAN_FLOOR_F64(h);
    return h >= 1.0 ? 0.0 : h;
}

#if ALWAN_WITH_F64
typedef alwan_rgb_f64 alwan__ta_rgb;
typedef alwan_oklab_f64 alwan__ta_oklab;
typedef alwan_hsl_f64 alwan__ta_hsl;
typedef alwan_hsv_f64 alwan__ta_hsv;
typedef alwan_f64 alwan__ta_s;
#define ALWAN__TA(fn) fn##_f64
#else
typedef alwan_rgb_f32 alwan__ta_rgb;
typedef alwan_oklab_f32 alwan__ta_oklab;
typedef alwan_hsl_f32 alwan__ta_hsl;
typedef alwan_hsv_f32 alwan__ta_hsv;
typedef alwan_f32 alwan__ta_s;
#define ALWAN__TA(fn) fn##_f32
#endif

/* The model conversions are alwan's own: the double ones in a build with f64, the
 * float ones in a single-precision build. */
static void alwan__ta_modulate(alwan__ta const *t, double *c) {
    alwan__ta_rgb rgb;
    rgb.r = (alwan__ta_s)c[0];
    rgb.g = (alwan__ta_s)c[1];
    rgb.b = (alwan__ta_s)c[2];
    if (t->space == ALWAN_MODULATE_OKLCH) {
        alwan__ta_oklab lab;
        double chroma, hue;
        if (t->input_linear) {
            alwan__ta_s tmp[3], enc[3];
            tmp[0] = rgb.r;
            tmp[1] = rgb.g;
            tmp[2] = rgb.b;
            ALWAN__TA(alwan_oetf_apply)(enc, sizeof(alwan__ta_s), tmp, sizeof(alwan__ta_s), 3, ALWAN_TF_SRGB);
            rgb.r = enc[0];
            rgb.g = enc[1];
            rgb.b = enc[2];
        }
        ALWAN__TA(alwan_srgb_to_oklab)(&lab, &rgb);
        chroma = ALWAN_SQRT_F64((double)lab.a * (double)lab.a + (double)lab.b * (double)lab.b) * t->sat;
        hue = ALWAN_ATAN2_F64((double)lab.b, (double)lab.a) + 2.0 * ALWAN__TA_PI * t->hue_turns;
        lab.L = (alwan__ta_s)((double)lab.L * t->bright);
        lab.a = (alwan__ta_s)(chroma * ALWAN_COS_F64(hue));
        lab.b = (alwan__ta_s)(chroma * ALWAN_SIN_F64(hue));
        ALWAN__TA(alwan_oklab_to_srgb)(&rgb, &lab);
        if (t->input_linear) {
            alwan__ta_s enc[3], lin[3];
            enc[0] = rgb.r;
            enc[1] = rgb.g;
            enc[2] = rgb.b;
            ALWAN__TA(alwan_eotf_apply)(lin, sizeof(alwan__ta_s), enc, sizeof(alwan__ta_s), 3, ALWAN_TF_SRGB);
            rgb.r = lin[0];
            rgb.g = lin[1];
            rgb.b = lin[2];
        }
    } else if (t->space == ALWAN_MODULATE_HSL) {
        alwan__ta_hsl hsl;
        ALWAN__TA(alwan_rgb_to_hsl)(&hsl, &rgb);
        hsl.h = (alwan__ta_s)alwan__ta_wrap((double)hsl.h + t->hue_turns);
        hsl.s = (alwan__ta_s)((double)hsl.s * t->sat);
        hsl.l = (alwan__ta_s)((double)hsl.l * t->bright);
        ALWAN__TA(alwan_hsl_to_rgb)(&rgb, &hsl);
    } else {
        alwan__ta_hsv hsv;
        ALWAN__TA(alwan_rgb_to_hsv)(&hsv, &rgb);
        hsv.h = (alwan__ta_s)alwan__ta_wrap((double)hsv.h + t->hue_turns);
        hsv.s = (alwan__ta_s)((double)hsv.s * t->sat);
        hsv.v = (alwan__ta_s)((double)hsv.v * t->bright);
        ALWAN__TA(alwan_hsv_to_rgb)(&rgb, &hsv);
    }
    c[0] = (double)rgb.r;
    c[1] = (double)rgb.g;
    c[2] = (double)rgb.b;
}

/* Colour channels of a pixel of 1 to 4 values: the alpha (the 2nd of 2, the 4th of 4) is
 * left as it is. */
static size_t alwan__ta_colour_channels(size_t channels) {
    return channels == 2 ? 1 : (channels == 4 ? 3 : channels);
}

typedef enum { ALWAN__TA_F32, ALWAN__TA_F64, ALWAN__TA_U8 } alwan__ta_type;

static alwan_status alwan__ta_run(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count,
                                  size_t channels, alwan_tone_adjust_method method, alwan_tone_adjust_params const *params,
                                  alwan__ta_type type) {
    alwan__ta t;
    alwan_status st;
    size_t i, c, cc, esz;
    if (!out || !in) return ALWAN_E_INVALID;
    if (channels < 1 || channels > 4) return ALWAN_E_INVALID;
    if ((st = alwan__ta_prepare(&t, method, params)) != ALWAN_OK) return st;
    if (method == ALWAN_TONE_ADJUST_MODULATE && alwan__ta_colour_channels(channels) != 3) return ALWAN_E_INVALID;
    if (t.pillow_bits && type != ALWAN__TA_U8) return ALWAN_E_INVALID;
    esz = type == ALWAN__TA_F64 ? sizeof(alwan_f64) : (type == ALWAN__TA_F32 ? sizeof(alwan_f32) : 1);
    if (out_stride == 0) out_stride = channels * esz;
    if (in_stride == 0) in_stride = channels * esz;
    cc = alwan__ta_colour_channels(channels);
    for (i = 0; i < count; i++) {
        char const *src = (char const *)in + i * in_stride;
        char *dst = (char *)out + i * out_stride;
        double v[4] = {0.0, 0.0, 0.0, 0.0};
        for (c = 0; c < channels; c++) {
            if (type == ALWAN__TA_F64) v[c] = ((alwan_f64 const *)src)[c];
            else if (type == ALWAN__TA_F32) v[c] = (double)((alwan_f32 const *)src)[c];
            else v[c] = (double)((unsigned char const *)src)[c] / 255.0;
        }
        if (t.pillow_bits) {
            unsigned const mask = ~((1u << (8 - t.pillow_bits)) - 1u) & 0xFFu;
            for (c = 0; c < channels; c++) {
                unsigned char const b = ((unsigned char const *)src)[c];
                ((unsigned char *)dst)[c] = c < cc ? (unsigned char)(b & mask) : b;
            }
            continue;
        }
        if (method == ALWAN_TONE_ADJUST_MODULATE) {
            alwan__ta_modulate(&t, v);
        } else {
            for (c = 0; c < cc; c++) v[c] = alwan__ta_scalar(&t, method, v[c]);
        }
        for (c = 0; c < channels; c++) {
            if (type == ALWAN__TA_F64) {
                ((alwan_f64 *)dst)[c] = v[c];
            } else if (type == ALWAN__TA_F32) {
                ((alwan_f32 *)dst)[c] = (alwan_f32)v[c];
            } else if (c >= cc) {
                ((unsigned char *)dst)[c] = ((unsigned char const *)src)[c];
            } else {
                double const q = ALWAN_FLOOR_F64(v[c] * 255.0 + 0.5);
                ((unsigned char *)dst)[c] = (unsigned char)(q < 0.0 ? 0.0 : (q > 255.0 ? 255.0 : q));
            }
        }
    }
    return ALWAN_OK;
}

#if ALWAN_WITH_F64
alwan_status alwan_tone_adjust_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count,
                                   size_t channels, alwan_tone_adjust_method method, alwan_tone_adjust_params const *params) {
    return alwan__ta_run(out, out_stride, in, in_stride, count, channels, method, params, ALWAN__TA_F64);
}

#endif

#if ALWAN_WITH_F32
alwan_status alwan_tone_adjust_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count,
                                   size_t channels, alwan_tone_adjust_method method, alwan_tone_adjust_params const *params) {
    return alwan__ta_run(out, out_stride, in, in_stride, count, channels, method, params, ALWAN__TA_F32);
}

#endif

alwan_status alwan_tone_adjust_u8(unsigned char *out, size_t out_stride, unsigned char const *in, size_t in_stride,
                                  size_t count, size_t channels, alwan_tone_adjust_method method,
                                  alwan_tone_adjust_params const *params) {
    return alwan__ta_run(out, out_stride, in, in_stride, count, channels, method, params, ALWAN__TA_U8);
}
