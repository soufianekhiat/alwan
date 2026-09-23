/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The grading operators of OpenColorIO, for pipelines that carry an OCIO grade and for
 * the controls a grading panel exposes. OpenColorIO is BSD-3-Clause; the arithmetic here
 * follows OCIO 2.5.0 (src/OpenColorIO/ops/gradingprimary/GradingPrimary.cpp for the
 * parameters, GradingPrimaryOpCPU.cpp for the pixel), and suite 184 holds it to the
 * installed PyOpenColorIO.
 *
 * GRADING PRIMARY has three styles, each a fixed chain:
 *
 *   log    out = in + brightness * 6.25 / 1023
 *          out = (out - pivot) * contrast + pivot,   pivot = 0.5 + 0.5 * p
 *          out = gamma curve about [pivot_black, pivot_white], exponent 1 / gamma
 *   lin    out = (in + offset) * 2^exposure
 *          out = sign(out) pivot |out / pivot|^contrast,   pivot = 0.18 * 2^p
 *   video  out = in + offset + lift
 *          out = (out - pivot_black) * slope + pivot_black,
 *                slope = (white - black) / (white / gain + lift - black)
 *          out = gamma curve as in log
 *
 * then saturation about Rec.709 luma and the clamp, in every style. Each control is an
 * RGBM quadruple: the channel's value combined with the master, added for brightness,
 * offset, exposure and lift and multiplied for contrast, gamma and gain. The inverse
 * runs the chain backwards with every step inverted. A zero gain, a zero log contrast or
 * a zero saturation is treated as 1 where it would be divided by, as OCIO does.
 *
 * OCIO renders in float32 from parameters rounded to float; this is double throughout, so
 * the two differ by float rounding and nothing else. OCIO's default linear pivot is 0.18,
 * and since that parameter is in stops from 0.18 the default grade pivots at
 * 0.18 * 2^0.18 = 0.2034; the init here gives the same defaults so a default grade means
 * what it means in OCIO.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <float.h>

#define ALWAN_GP_MIN (0.01 - 0.000001)   /* OCIO's lower bound on gamma and lin contrast */

typedef struct {
    int style, inverse;
    double add[3];          /* brightness (log) or offset [+ lift] (lin, video) */
    double scale[3];        /* contrast (log), exposure (lin), slope (video) */
    double power[3];        /* gamma exponent (log, video) or lin contrast */
    int power_identity;
    double pivot, pivot_black, pivot_white;
    double saturation, clamp_black, clamp_white;
} alwan_gp_render;

void alwan_grading_primary_init(alwan_grading_primary *params, alwan_grading_style style) {
    alwan_grading_rgbm const zero = { 0.0, 0.0, 0.0, 0.0 }, one = { 1.0, 1.0, 1.0, 1.0 };
    if (!params) return;
    params->brightness = zero;
    params->contrast = one;
    params->gamma = one;
    params->offset = zero;
    params->exposure = zero;
    params->lift = zero;
    params->gain = one;
    params->saturation = 1.0;
    params->pivot = style == ALWAN_GRADING_LOG ? -0.2 : 0.18;
    params->pivot_black = 0.0;
    params->pivot_white = 1.0;
    params->clamp_black = -DBL_MAX;
    params->clamp_white = DBL_MAX;
}

static int alwan_gp_finite(double v) {
    return v == v && v <= DBL_MAX && v >= -DBL_MAX;
}

static int alwan_gp_rgbm_finite(alwan_grading_rgbm const *v) {
    return alwan_gp_finite(v->red) && alwan_gp_finite(v->green) && alwan_gp_finite(v->blue)
        && alwan_gp_finite(v->master);
}

static int alwan_gp_rgbm_below(alwan_grading_rgbm const *v, double lo) {
    return v->red < lo || v->green < lo || v->blue < lo || v->master < lo;
}

static double alwan_gp_nz(double v) {
    return v == 0.0 ? 1.0 : v;
}

/* OCIO's validate() plus finiteness, then GradingPrimaryPreRender::update() in double. */
static alwan_status alwan_gp_prepare(alwan_gp_render *r, alwan_grading_style style,
                                     alwan_grading_primary const *v, int inverse) {
    alwan_grading_rgbm const *rgbm[7];
    double ch[3][7];
    int i, c;
    if (!v) return ALWAN_E_INVALID;
    if (style != ALWAN_GRADING_LOG && style != ALWAN_GRADING_LIN && style != ALWAN_GRADING_VIDEO) {
        return ALWAN_E_INVALID;
    }
    rgbm[0] = &v->brightness; rgbm[1] = &v->contrast; rgbm[2] = &v->gamma; rgbm[3] = &v->offset;
    rgbm[4] = &v->exposure; rgbm[5] = &v->lift; rgbm[6] = &v->gain;
    for (i = 0; i < 7; i++) if (!alwan_gp_rgbm_finite(rgbm[i])) return ALWAN_E_INVALID;
    if (!alwan_gp_finite(v->saturation) || !alwan_gp_finite(v->pivot)
        || !alwan_gp_finite(v->pivot_black) || !alwan_gp_finite(v->pivot_white)) {
        return ALWAN_E_INVALID;
    }
    if (v->clamp_black != v->clamp_black || v->clamp_white != v->clamp_white) return ALWAN_E_INVALID;
    if (style != ALWAN_GRADING_LIN && alwan_gp_rgbm_below(&v->gamma, ALWAN_GP_MIN)) return ALWAN_E_INVALID;
    if (style == ALWAN_GRADING_LIN && alwan_gp_rgbm_below(&v->contrast, ALWAN_GP_MIN)) return ALWAN_E_INVALID;
    if (v->pivot_white - v->pivot_black < ALWAN_GP_MIN) return ALWAN_E_INVALID;
    if (v->clamp_black > v->clamp_white) return ALWAN_E_INVALID;

    for (i = 0; i < 7; i++) {
        ch[0][i] = rgbm[i]->red;
        ch[1][i] = rgbm[i]->green;
        ch[2][i] = rgbm[i]->blue;
    }
    r->style = (int)style;
    r->inverse = inverse;
    r->pivot_black = v->pivot_black;
    r->pivot_white = v->pivot_white;
    r->saturation = inverse ? 1.0 / alwan_gp_nz(v->saturation) : v->saturation;
    r->clamp_black = v->clamp_black;
    r->clamp_white = v->clamp_white;
    for (c = 0; c < 3; c++) {
        if (style == ALWAN_GRADING_LOG) {
            double const b = (v->brightness.master + ch[c][0]) * 6.25 / 1023.0;
            double const k = v->contrast.master * ch[c][1];
            double const g = v->gamma.master * ch[c][2];
            r->add[c] = inverse ? -b : b;
            r->scale[c] = inverse ? 1.0 / alwan_gp_nz(k) : k;
            r->power[c] = inverse ? g : 1.0 / g;
        } else if (style == ALWAN_GRADING_LIN) {
            double const o = v->offset.master + ch[c][3];
            double const e = pow(2.0, v->exposure.master + ch[c][4]);
            double const k = v->contrast.master * ch[c][1];
            r->add[c] = inverse ? -o : o;
            r->scale[c] = inverse ? 1.0 / e : e;
            r->power[c] = inverse ? 1.0 / k : k;
        } else {
            double const lift = v->lift.master + ch[c][5];
            double const o = v->offset.master + ch[c][3] + lift;
            double const gain = alwan_gp_nz(v->gain.master * ch[c][6]);
            double const g = v->gamma.master * ch[c][2];
            double const range = v->pivot_white - v->pivot_black;
            r->add[c] = inverse ? -o : o;
            r->scale[c] = inverse ? (v->pivot_white / gain + (lift - v->pivot_black)) / range
                                  : range / alwan_gp_nz(v->pivot_white / gain + lift - v->pivot_black);
            r->power[c] = inverse ? g : 1.0 / g;
        }
    }
    r->power_identity = r->power[0] == 1.0 && r->power[1] == 1.0 && r->power[2] == 1.0;
    r->pivot = style == ALWAN_GRADING_LOG ? 0.5 + v->pivot * 0.5
             : style == ALWAN_GRADING_LIN ? 0.18 * pow(2.0, v->pivot) : 0.0;
    return ALWAN_OK;
}

static void alwan_gp_add(double *p, double const *a) {
    p[0] += a[0]; p[1] += a[1]; p[2] += a[2];
}

static void alwan_gp_scale(double *p, double const *s) {
    p[0] *= s[0]; p[1] *= s[1]; p[2] *= s[2];
}

static void alwan_gp_contrast(double *p, double const *k, double pivot) {
    int c;
    for (c = 0; c < 3; c++) p[c] = (p[c] - pivot) * k[c] + pivot;
}

static void alwan_gp_lin_contrast(double *p, double const *k, double pivot) {
    int c;
    for (c = 0; c < 3; c++) p[c] = pow(fabs(p[c] / pivot), k[c]) * copysign(pivot, p[c]);
}

static void alwan_gp_gamma(double *p, double const *g, double black, double white) {
    double const range = white - black;
    int c;
    for (c = 0; c < 3; c++) {
        double const d = p[c] - black;
        p[c] = pow(fabs(d) / range, g[c]) * copysign(1.0, d) * range + black;
    }
}

static void alwan_gp_saturation(double *p, double s) {
    if (s != 1.0) {
        double const luma = p[0] * 0.2126 + p[1] * 0.7152 + p[2] * 0.0722;
        p[0] = luma + s * (p[0] - luma);
        p[1] = luma + s * (p[1] - luma);
        p[2] = luma + s * (p[2] - luma);
    }
}

/* std::min(std::max(x, lo), hi): a NaN passes through. */
static void alwan_gp_clamp(double *p, double lo, double hi) {
    int c;
    for (c = 0; c < 3; c++) {
        double const m = p[c] < lo ? lo : p[c];
        p[c] = hi < m ? hi : m;
    }
}

static void alwan_gp_pixel(double *p, alwan_gp_render const *r) {
    if (!r->inverse) {
        alwan_gp_add(p, r->add);
        if (r->style == ALWAN_GRADING_LOG) {
            alwan_gp_contrast(p, r->scale, r->pivot);
            if (!r->power_identity) alwan_gp_gamma(p, r->power, r->pivot_black, r->pivot_white);
        } else if (r->style == ALWAN_GRADING_LIN) {
            alwan_gp_scale(p, r->scale);
            if (!r->power_identity) alwan_gp_lin_contrast(p, r->power, r->pivot);
        } else {
            alwan_gp_contrast(p, r->scale, r->pivot_black);
            if (!r->power_identity) alwan_gp_gamma(p, r->power, r->pivot_black, r->pivot_white);
        }
        alwan_gp_saturation(p, r->saturation);
        alwan_gp_clamp(p, r->clamp_black, r->clamp_white);
    } else {
        alwan_gp_clamp(p, r->clamp_black, r->clamp_white);
        alwan_gp_saturation(p, r->saturation);
        if (r->style == ALWAN_GRADING_LOG) {
            if (!r->power_identity) alwan_gp_gamma(p, r->power, r->pivot_black, r->pivot_white);
            alwan_gp_contrast(p, r->scale, r->pivot);
        } else if (r->style == ALWAN_GRADING_LIN) {
            if (!r->power_identity) alwan_gp_lin_contrast(p, r->power, r->pivot);
            alwan_gp_scale(p, r->scale);
        } else {
            if (!r->power_identity) alwan_gp_gamma(p, r->power, r->pivot_black, r->pivot_white);
            alwan_gp_contrast(p, r->scale, r->pivot_black);
        }
        alwan_gp_add(p, r->add);   /* already negated for the inverse */
    }
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_grading_primary_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                             alwan_grading_style style, alwan_grading_primary const *params,
                                             int inverse) {
    alwan_gp_render r;
    double p[3];
    alwan_status st;
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    st = alwan_gp_prepare(&r, style, params, inverse != 0);
    if (st != ALWAN_OK) return st;
    p[0] = (double)rgb_in->r; p[1] = (double)rgb_in->g; p[2] = (double)rgb_in->b;
    alwan_gp_pixel(p, &r);
    rgb_out->r = (alwan_f64)p[0]; rgb_out->g = (alwan_f64)p[1]; rgb_out->b = (alwan_f64)p[2];
    return ALWAN_OK;
}

alwan_status alwan_grading_primary_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in,
                                                      size_t in_stride, size_t count, alwan_grading_style style,
                                                      alwan_grading_primary const *params, int inverse) {
    alwan_gp_render r;
    alwan_status st;
    size_t i;
    if (!out || !in) return ALWAN_E_INVALID;
    if (out_stride < 3 * sizeof(alwan_f64) || in_stride < 3 * sizeof(alwan_f64)) return ALWAN_E_INVALID;
    st = alwan_gp_prepare(&r, style, params, inverse != 0);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        alwan_f64 const *s = (alwan_f64 const *)((char const *)in + i * in_stride);
        alwan_f64 *d = (alwan_f64 *)((char *)out + i * out_stride);
        double p[3];
        p[0] = (double)s[0]; p[1] = (double)s[1]; p[2] = (double)s[2];
        alwan_gp_pixel(p, &r);
        d[0] = (alwan_f64)p[0]; d[1] = (alwan_f64)p[1]; d[2] = (alwan_f64)p[2];
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
alwan_status alwan_grading_primary_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                             alwan_grading_style style, alwan_grading_primary const *params,
                                             int inverse) {
    alwan_gp_render r;
    double p[3];
    alwan_status st;
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    st = alwan_gp_prepare(&r, style, params, inverse != 0);
    if (st != ALWAN_OK) return st;
    p[0] = (double)rgb_in->r; p[1] = (double)rgb_in->g; p[2] = (double)rgb_in->b;
    alwan_gp_pixel(p, &r);
    rgb_out->r = (alwan_f32)p[0]; rgb_out->g = (alwan_f32)p[1]; rgb_out->b = (alwan_f32)p[2];
    return ALWAN_OK;
}

alwan_status alwan_grading_primary_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in,
                                                      size_t in_stride, size_t count, alwan_grading_style style,
                                                      alwan_grading_primary const *params, int inverse) {
    alwan_gp_render r;
    alwan_status st;
    size_t i;
    if (!out || !in) return ALWAN_E_INVALID;
    if (out_stride < 3 * sizeof(alwan_f32) || in_stride < 3 * sizeof(alwan_f32)) return ALWAN_E_INVALID;
    st = alwan_gp_prepare(&r, style, params, inverse != 0);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        alwan_f32 const *s = (alwan_f32 const *)((char const *)in + i * in_stride);
        alwan_f32 *d = (alwan_f32 *)((char *)out + i * out_stride);
        double p[3];
        p[0] = (double)s[0]; p[1] = (double)s[1]; p[2] = (double)s[2];
        alwan_gp_pixel(p, &r);
        d[0] = (alwan_f32)p[0]; d[1] = (alwan_f32)p[1]; d[2] = (alwan_f32)p[2];
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
