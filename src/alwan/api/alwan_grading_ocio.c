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
#include <string.h>

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

static void alwan_gp_init(alwan_grading_primary *params, alwan_grading_style style) {
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

/* ================================================================
 * GRADING TONE: OCIO's GradingToneTransform (GradingTone.cpp for the pre-render,
 * GradingToneOpCPU.cpp for the pixel). Five zones, each a piecewise-quadratic curve
 * whose slopes the controls set, applied per channel and then by the master to all
 * three: midtones, highlights, whites, shadows, blacks, then an S-contrast about the
 * style's pivot. The lin style works in OCIO's log domain (its own lin-to-log with a
 * linear toe) and returns to linear at the end.
 *
 * OCIO evaluates a single channel and the master slightly differently: the channel
 * path compares with > and the master path with >= at the segment joins, and the
 * channel path of the inverse midtones extrapolates above the top from the BOTTOM
 * anchor (x0 + (t - y0) / m0, where the master path uses x5 and y5). The curve is built
 * so the two agree to rounding, and both are reproduced as written.
 * ================================================================ */

enum { ALWAN_GT_R = 0, ALWAN_GT_G = 1, ALWAN_GT_B = 2, ALWAN_GT_M = 3 };

typedef struct {
    int style, inverse, identity;
    double top, top_sc, bottom, pivot;
    double hl_start, hl_width, wh_start, wh_width, sh_start, sh_width, bl_start, bl_width;
    double mid_adj[4], mid_x[4][6], mid_y[4][6], mid_m[4][6];
    double hs_val[2][4], hs_x[2][4][3], hs_y[2][4][3], hs_m[2][4][2];
    double wb_val[2][4], wb_x[2][4][2], wb_y[2][4][2], wb_m[2][4][2], wb_gain[2][4];
    double sc, sc_x[2][4], sc_y[2][4], sc_m[2][2];
} alwan_gt_render;

static double alwan_gt_channel(alwan_grading_rgbmsw const *v, int ch) {
    return ch == ALWAN_GT_R ? v->red : ch == ALWAN_GT_G ? v->green : ch == ALWAN_GT_B ? v->blue : v->master;
}

static double alwan_gt_clampd(double a, double lo, double hi) {
    double const m = a < lo ? lo : a;
    return hi < m ? hi : m;
}

static void alwan_gt_init(alwan_grading_tone *tone, alwan_grading_style style) {
    static double const sw[3][5][2] = {   /* start, width: blacks, shadows, midtones, highlights, whites */
        { { 0.4, 0.4 }, { 0.5, 0.0 }, { 0.4, 0.6 }, { 0.3, 1.0 }, { 0.4, 0.5 } },
        { { 0.0, 4.0 }, { 2.0, -7.0 }, { 0.0, 8.0 }, { -2.0, 9.0 }, { 0.0, 8.0 } },
        { { 0.4, 0.4 }, { 0.6, 0.0 }, { 0.4, 0.7 }, { 0.2, 1.0 }, { 0.5, 0.5 } } };
    alwan_grading_rgbmsw *z[5];
    int s = style == ALWAN_GRADING_LIN ? 1 : style == ALWAN_GRADING_VIDEO ? 2 : 0, i;
    if (!tone) return;
    z[0] = &tone->blacks; z[1] = &tone->shadows; z[2] = &tone->midtones; z[3] = &tone->highlights; z[4] = &tone->whites;
    for (i = 0; i < 5; i++) {
        z[i]->red = z[i]->green = z[i]->blue = z[i]->master = 1.0;
        z[i]->start = sw[s][i][0];
        z[i]->width = sw[s][i][1];
    }
    tone->scontrast = 1.0;
}

static double alwan_gt_faux_fwd(double t, double x0, double x2, double y0, double y2, double m0, double m2, double x1) {
    double const y1 = (0.5 / ((x2 - x1) + (x1 - x0))) *
                      ((2. * y0 + m0 * (x1 - x0)) * (x2 - x1) + (2. * y2 - m2 * (x2 - x1)) * (x1 - x0));
    double const tL = (t - x0) / (x1 - x0);
    double const tR = (t - x1) / (x2 - x1);
    double const fL = y0 * (1. - tL * tL) + y1 * tL * tL + m0 * (1. - tL) * tL * (x1 - x0);
    double const fR = y1 * (1. - tR) * (1. - tR) + y2 * (2. - tR) * tR + m2 * (tR - 1.) * tR * (x2 - x1);
    double res = (t < x1) ? fL : fR;
    res = (t < x0) ? y0 + (t - x0) * m0 : res;
    res = (t > x2) ? y2 + (t - x2) * m2 : res;
    return res;
}

static double alwan_gt_faux_rev(double t, double x0, double x2, double y0, double y2, double m0, double m2, double x1) {
    double const y1 = (0.5 / ((x2 - x1) + (x1 - x0))) *
                      ((2. * y0 + m0 * (x1 - x0)) * (x2 - x1) + (2. * y2 - m2 * (x2 - x1)) * (x1 - x0));
    double const cL = y0 - t, bL = m0 * (x1 - x0), aL = y1 - y0 - m0 * (x1 - x0);
    double const outL = (2. * cL) / (-sqrt(bL * bL - 4. * aL * cL) - bL) * (x1 - x0) + x0;
    double const cR = y1 - t, bR = 2. * y2 - 2. * y1 - m2 * (x2 - x1), aR = y1 - y2 + m2 * (x2 - x1);
    double const outR = (2. * cR) / (-sqrt(bR * bR - 4. * aR * cR) - bR) * (x2 - x1) + x1;
    double res = (t < y1) ? outL : outR;
    res = (t < y0) ? x0 + (t - y0) / m0 : res;
    res = (t > y2) ? x2 + (t - y2) / m2 : res;
    return res;
}

static double alwan_gt_highlight_eval(double t, double start, double pivot, double val) {
    double const x0 = start, x2 = pivot, x1 = x0 + (x2 - x0) * 0.5;
    val = 2. - val;
    if (val <= 1.) return alwan_gt_faux_fwd(t, x0, x2, x0, x2, 1., val < 0.01 ? 0.01 : val, x1);
    return alwan_gt_faux_rev(t, x0, x2, x0, x2, 1., 2. - val < 0.01 ? 0.01 : 2. - val, x1);
}

static double alwan_gt_shadow_eval(double t, double start, double pivot, double val) {
    double const x0 = start, x2 = pivot, x1 = x0 + (x2 - x0) * 0.5;
    if (val <= 1.) return alwan_gt_faux_fwd(t, x0, x2, x0, x2, val < 0.01 ? 0.01 : val, 1., x1);
    return alwan_gt_faux_rev(t, x0, x2, x0, x2, 2. - val < 0.01 ? 0.01 : 2. - val, 1., x1);
}

static int alwan_gt_zone_ok(alwan_grading_rgbmsw const *z, double lo, double hi) {
    return alwan_gp_finite(z->red) && alwan_gp_finite(z->green) && alwan_gp_finite(z->blue)
        && alwan_gp_finite(z->master) && alwan_gp_finite(z->start) && alwan_gp_finite(z->width)
        && !(z->red < lo || z->green < lo || z->blue < lo || z->master < lo)
        && !(z->red > hi || z->green > hi || z->blue > hi || z->master > hi);
}

static int alwan_gt_is_identity(alwan_grading_rgbmsw const *z) {
    return z->red == 1. && z->green == 1. && z->blue == 1. && z->master == 1.;
}

/* OCIO's GradingTone::validate(), then GradingTonePreRender::update() in double. */
static alwan_status alwan_gt_prepare(alwan_gt_render *r, alwan_grading_style style,
                                     alwan_grading_tone const *v, int inverse) {
    double const err = 0.000001, min_bmw = 0.1 - err, max_bmw = 1.9 + err;
    double const min_sh = 0.2 - err, max_sh = 1.8 + err, min_wsc = 0.01 - err, max_sc = 1.99 - err;
    int ch, k;
    if (!v) return ALWAN_E_INVALID;
    if (style != ALWAN_GRADING_LOG && style != ALWAN_GRADING_LIN && style != ALWAN_GRADING_VIDEO) {
        return ALWAN_E_INVALID;
    }
    if (!alwan_gt_zone_ok(&v->blacks, min_bmw, max_bmw) || v->blacks.width < min_wsc) return ALWAN_E_INVALID;
    if (!alwan_gt_zone_ok(&v->midtones, min_bmw, max_bmw) || v->midtones.width < min_wsc) return ALWAN_E_INVALID;
    if (!alwan_gt_zone_ok(&v->whites, min_bmw, max_bmw) || v->whites.width < min_wsc) return ALWAN_E_INVALID;
    if (!alwan_gt_zone_ok(&v->shadows, min_sh, max_sh) || v->shadows.start < v->shadows.width + min_wsc) return ALWAN_E_INVALID;
    if (!alwan_gt_zone_ok(&v->highlights, min_sh, max_sh) || v->highlights.start > v->highlights.width - min_wsc) return ALWAN_E_INVALID;
    if (!alwan_gp_finite(v->scontrast) || v->scontrast < min_wsc || v->scontrast > max_sc) return ALWAN_E_INVALID;

    r->style = (int)style;
    r->inverse = inverse;
    r->identity = alwan_gt_is_identity(&v->blacks) && alwan_gt_is_identity(&v->shadows)
               && alwan_gt_is_identity(&v->midtones) && alwan_gt_is_identity(&v->highlights)
               && alwan_gt_is_identity(&v->whites) && v->scontrast == 1.;
    if (style == ALWAN_GRADING_LIN) { r->top = 7.5; r->top_sc = 6.5; r->bottom = -5.5; r->pivot = 0.0; }
    else { r->top = 1.0; r->top_sc = 1.0; r->bottom = 0.0; r->pivot = (double)0.4f; }
    if (r->identity) return ALWAN_OK;

    {   /* the whites follow the highlights, the blacks the shadows */
        double const pivot = v->highlights.width, start = v->highlights.start;
        double ns, ne;
        r->hl_start = (start > pivot - 0.01) ? pivot - 0.01 : start;
        r->hl_width = pivot;
        ns = alwan_gt_highlight_eval(v->whites.start, r->hl_start, r->hl_width, v->highlights.master);
        ne = alwan_gt_highlight_eval(v->whites.start + v->whites.width, r->hl_start, r->hl_width, v->highlights.master);
        r->wh_start = ns;
        r->wh_width = ne - ns;
    }
    {
        double const pivot = v->shadows.width, start = v->shadows.start;
        double ns, ne;
        r->sh_start = (start < pivot + 0.01) ? pivot + 0.01 : start;
        r->sh_width = pivot;
        ns = alwan_gt_shadow_eval(v->blacks.start, r->sh_width, r->sh_start, v->shadows.master);
        ne = alwan_gt_shadow_eval(v->blacks.start - v->blacks.width, r->sh_width, r->sh_start, v->shadows.master);
        r->bl_start = ns;
        r->bl_width = ns - ne;
    }

    for (ch = 0; ch < 4; ch++) {   /* midtones */
        double *x = r->mid_x[ch], *y = r->mid_y[ch], *m = r->mid_m[ch];
        double adj = alwan_gt_clampd(alwan_gt_channel(&v->midtones, ch), 0.01, 1.99);
        r->mid_adj[ch] = adj;
        if (adj != 1.) {
            double const halo = 0.4, min_slope = 0.1;
            double max_width, width, center;
            x[0] = r->bottom;
            x[5] = r->top;
            max_width = (x[5] - x[0]) * 0.95;
            width = alwan_gt_clampd(v->midtones.width, 0.01, max_width);
            center = alwan_gt_clampd(v->midtones.start, x[0] + width * 0.51, x[5] - width * 0.51);
            x[1] = center - width * 0.5;
            x[4] = x[1] + width;
            x[2] = x[1] + (x[4] - x[1]) * 0.25;
            x[3] = x[1] + (x[4] - x[1]) * 0.75;
            y[0] = x[0];
            m[0] = 1.;
            m[5] = 1.;
            adj = (adj - 1.) * (1. - min_slope);
            m[2] = 1. + adj;
            m[3] = 1. - adj;
            m[1] = 1. + adj * halo;
            m[4] = 1. - adj * halo;
            if (center <= (x[5] + x[0]) * 0.5) {
                double const area = (x[1] - x[0]) * (m[1] - m[0]) * 0.5
                                  + (x[2] - x[1]) * ((m[1] - m[0]) + (m[2] - m[1]) * 0.5)
                                  + (center - x[2]) * (m[2] - m[0]) * 0.5;
                m[4] = (-0.5 * (x[5] - x[4]) * m[5] + (x[4] - x[3]) * (0.5 * m[3] - m[5])
                        + (x[3] - center) * (m[3] - m[5]) * 0.5 + area) / (-0.5 * (x[5] - x[3]));
            } else {
                double const area = (x[5] - x[4]) * (m[4] - m[5]) * 0.5
                                  + (x[4] - x[3]) * ((m[4] - m[5]) + (m[3] - m[4]) * 0.5)
                                  + (x[3] - center) * (m[3] - m[5]) * 0.5;
                m[1] = (-0.5 * (x[1] - x[0]) * m[0] + (x[2] - x[1]) * (0.5 * m[2] - m[0])
                        + (center - x[2]) * (m[2] - m[0]) * 0.5 + area) / (-0.5 * (x[2] - x[0]));
            }
            for (k = 1; k < 6; k++) y[k] = y[k - 1] + (m[k - 1] + m[k]) * (x[k] - x[k - 1]) * 0.5;
        }
    }

    for (k = 0; k < 2; k++) {   /* 0 highlights, 1 shadows */
        for (ch = 0; ch < 4; ch++) {
            double *x = r->hs_x[k][ch], *y = r->hs_y[k][ch], *m = r->hs_m[k][ch];
            double val = k ? alwan_gt_channel(&v->shadows, ch) : 2. - alwan_gt_channel(&v->highlights, ch);
            r->hs_val[k][ch] = val;
            if (val != 1.) {
                double const start = k ? r->sh_start : r->hl_start, pivot = k ? r->sh_width : r->hl_width;
                double const s = val < 1. ? val : 2. - val;
                x[0] = k ? pivot : start;
                x[2] = k ? start : pivot;
                y[0] = x[0];
                y[2] = x[2];
                x[1] = x[0] + (x[2] - x[0]) * 0.5;
                m[0] = k ? (s < 0.01 ? 0.01 : s) : 1.;
                m[1] = k ? 1. : (s < 0.01 ? 0.01 : s);
                y[1] = (0.5 / ((x[2] - x[1]) + (x[1] - x[0]))) * ((2. * y[0] + m[0] * (x[1] - x[0])) * (x[2] - x[1])
                        + (2. * y[2] - m[1] * (x[2] - x[1])) * (x[1] - x[0]));
            }
        }
    }

    for (k = 0; k < 2; k++) {   /* 0 whites, 1 blacks */
        for (ch = 0; ch < 4; ch++) {
            double *x = r->wb_x[k][ch], *y = r->wb_y[k][ch], *m = r->wb_m[k][ch];
            double const start = k ? r->bl_start : r->wh_start, width = k ? r->bl_width : r->wh_width;
            double const val = k ? alwan_gt_channel(&v->blacks, ch) : alwan_gt_channel(&v->whites, ch);
            double const mtest = k ? 2. - val : val;
            r->wb_val[k][ch] = val;
            x[0] = k ? start - width : start;
            x[1] = k ? start : x[0] + width;
            r->wb_gain[k][ch] = 1.;
            if (mtest < 1.) {
                if (!k) { m[0] = 1.; m[1] = val < 0.01 ? 0.01 : val; y[0] = x[0]; y[1] = y[0] + (m[0] + m[1]) * (x[1] - x[0]) * 0.5; }
                else { m[0] = 2. - val < 0.01 ? 0.01 : 2. - val; m[1] = 1.; y[1] = x[1]; y[0] = y[1] - (m[0] + m[1]) * (x[1] - x[0]) * 0.5; }
            } else if (mtest > 1.) {
                if (!k) { m[0] = 1.; m[1] = 2. - val < 0.01 ? 0.01 : 2. - val; y[0] = x[0]; y[1] = 0.0; }
                else { m[0] = val < 0.01 ? 0.01 : val; m[1] = 1.; y[1] = x[1]; y[0] = y[1] - (m[0] + m[1]) * (x[1] - x[0]) * 0.5; }
                r->wb_gain[k][ch] = (m[0] + m[1]) * 0.5;
            }
        }
    }

    r->sc = v->scontrast;
    if (r->sc != 1.) {
        double const c = r->sc > 1. ? 1. / (1.8125 - 0.8125 * (r->sc < 1.99 ? r->sc : 1.99))
                                    : 0.28125 + 0.71875 * (r->sc > 0.01 ? r->sc : 0.01);
        double *x, *y, *m, min_width, center;
        r->sc = c;
        x = r->sc_x[0]; y = r->sc_y[0]; m = r->sc_m[0];   /* top end */
        x[3] = r->top_sc;
        y[3] = r->top_sc;
        y[0] = r->pivot + (y[3] - r->pivot) * 0.25;
        m[0] = c;
        x[0] = r->pivot + (y[0] - r->pivot) / m[0];
        min_width = (x[3] - x[0]) * 0.3;
        m[1] = 1. / m[0];
        center = (y[3] - y[0] - m[1] * x[3] + m[0] * x[0]) / (m[0] - m[1]);
        x[1] = x[0];
        x[2] = 2. * center - x[1];
        if (x[2] > x[3]) {
            x[2] = x[3];
            x[1] = 2. * center - x[2];
        } else if ((x[2] - x[1]) < min_width) {
            double nc;
            x[2] = x[1] + min_width;
            nc = (x[2] + x[1]) * 0.5;
            m[1] = (y[3] - y[0] + m[0] * x[0] - nc * m[0]) / (x[3] - nc);
        }
        y[1] = y[0];
        y[2] = y[1] + (m[0] + m[1]) * (x[2] - x[1]) * 0.5;

        x = r->sc_x[1]; y = r->sc_y[1]; m = r->sc_m[1];   /* bottom end */
        x[0] = r->bottom;
        y[0] = r->bottom;
        y[3] = r->pivot - (r->pivot - y[0]) * 0.25;
        m[1] = c;
        x[3] = r->pivot - (r->pivot - y[3]) / m[1];
        min_width = (x[3] - x[0]) * 0.3;
        m[0] = 1. / m[1];
        center = (y[3] - y[0] - m[1] * x[3] + m[0] * x[0]) / (m[0] - m[1]);
        x[2] = x[3];
        x[1] = 2. * center - x[2];
        if (x[1] < x[0]) {
            x[1] = x[0];
            x[2] = 2. * center - x[1];
        } else if ((x[2] - x[1]) < min_width) {
            double nc;
            x[1] = x[2] - min_width;
            nc = (x[2] + x[1]) * 0.5;
            m[0] = (y[3] - y[0] - m[1] * x[3] + nc * m[1]) / (nc - x[0]);
        }
        y[2] = y[3];
        y[1] = y[2] - (m[0] + m[1]) * (x[2] - x[1]) * 0.5;
    }
    return ALWAN_OK;
}

/* ---- per zone, per value: master = 1 takes OCIO's float3 path, 0 its channel path ---- */

static double alwan_gt_mid_fwd(alwan_gt_render const *r, int ch, double t, int master) {
    double const *x = r->mid_x[ch], *y = r->mid_y[ch], *m = r->mid_m[ch];
    double const tL = (t - x[0]) / (x[1] - x[0]), tM = (t - x[1]) / (x[2] - x[1]), tR = (t - x[2]) / (x[3] - x[2]);
    double const tR2 = (t - x[3]) / (x[4] - x[3]), tR3 = (t - x[4]) / (x[5] - x[4]);
    double const fL = tL * (x[1] - x[0]) * (tL * 0.5 * (m[1] - m[0]) + m[0]) + y[0];
    double const fM = tM * (x[2] - x[1]) * (tM * 0.5 * (m[2] - m[1]) + m[1]) + y[1];
    double const fR = tR * (x[3] - x[2]) * (tR * 0.5 * (m[3] - m[2]) + m[2]) + y[2];
    double const fR2 = tR2 * (x[4] - x[3]) * (tR2 * 0.5 * (m[4] - m[3]) + m[3]) + y[3];
    double const fR3 = tR3 * (x[5] - x[4]) * (tR3 * 0.5 * (m[5] - m[4]) + m[4]) + y[4];
    double res = t < x[1] ? fL : fM;
    if (!master) {
        if (t > x[2]) res = fR;
        if (t > x[3]) res = fR2;
        if (t > x[4]) res = fR3;
        if (t < x[0]) res = y[0] + (t - x[0]) * m[0];
        if (t > x[5]) res = y[5] + (t - x[5]) * m[5];
    } else {
        res = t < x[2] ? res : fR;
        res = t < x[3] ? res : fR2;
        res = t < x[4] ? res : fR3;
        res = t < x[0] ? (t - x[0]) * m[0] + y[0] : res;
        res = t < x[5] ? res : (t - x[5]) * m[5] + y[5];
    }
    return res;
}

static double alwan_gt_mid_seg_rev(double t, double xa, double xb, double ya, double ma, double mb) {
    double const c = ya - t, b = ma * (xb - xa), a = 0.5 * (mb - ma) * (xb - xa);
    return (2. * c) / (-sqrt(b * b - 4. * a * c) - b) * (xb - xa) + xa;
}

static double alwan_gt_mid_rev(alwan_gt_render const *r, int ch, double t, int master) {
    double const *x = r->mid_x[ch], *y = r->mid_y[ch], *m = r->mid_m[ch];
    if (!master) {
        if (t >= y[5]) return x[0] + (t - y[0]) / m[0];   /* OCIO's channel path, as written */
        if (t >= y[4]) return alwan_gt_mid_seg_rev(t, x[4], x[5], y[4], m[4], m[5]);
        if (t >= y[3]) return alwan_gt_mid_seg_rev(t, x[3], x[4], y[3], m[3], m[4]);
        if (t >= y[2]) return alwan_gt_mid_seg_rev(t, x[2], x[3], y[2], m[2], m[3]);
        if (t >= y[1]) return alwan_gt_mid_seg_rev(t, x[1], x[2], y[1], m[1], m[2]);
        if (t >= y[0]) return alwan_gt_mid_seg_rev(t, x[0], x[1], y[0], m[0], m[1]);
        return x[0] + (t - y[0]) / m[0];
    } else {
        double const outR4 = x[5] + (t - y[5]) / m[5];
        double const outR3 = alwan_gt_mid_seg_rev(t, x[4], x[5], y[4], m[4], m[5]);
        double const outR2 = alwan_gt_mid_seg_rev(t, x[3], x[4], y[3], m[3], m[4]);
        double const outR = alwan_gt_mid_seg_rev(t, x[2], x[3], y[2], m[2], m[3]);
        double const outM = alwan_gt_mid_seg_rev(t, x[1], x[2], y[1], m[1], m[2]);
        double const outL = alwan_gt_mid_seg_rev(t, x[0], x[1], y[0], m[0], m[1]);
        double const outL0 = x[0] + (t - y[0]) / m[0];
        double res = t < y[1] ? outL : outM;
        res = t < y[2] ? res : outR;
        res = t < y[3] ? res : outR2;
        res = t < y[4] ? res : outR3;
        res = t < y[0] ? outL0 : res;
        res = t < y[5] ? res : outR4;
        return res;
    }
}

static double alwan_gt_hs_fwd(double const *x, double const *y, double const *m, double t) {
    double const tL = (t - x[0]) / (x[1] - x[0]), tR = (t - x[1]) / (x[2] - x[1]);
    double const fL = y[0] * (1. - tL * tL) + y[1] * tL * tL + m[0] * (1. - tL) * tL * (x[1] - x[0]);
    double const fR = y[1] * (1. - tR) * (1. - tR) + y[2] * (2. - tR) * tR + m[1] * (tR - 1.) * tR * (x[2] - x[1]);
    double res = t < x[1] ? fL : fR;
    res = t < x[0] ? (t - x[0]) * m[0] + y[0] : res;
    res = t < x[2] ? res : (t - x[2]) * m[1] + y[2];
    return res;
}

static double alwan_gt_hs_rev(double const *x, double const *y, double const *m, double t) {
    double const bL = m[0] * (x[1] - x[0]), aL = y[1] - y[0] - m[0] * (x[1] - x[0]), cL = y[0] - t;
    double const outL = (-2. * cL) / (sqrt(bL * bL - 4. * aL * cL) + bL) * (x[1] - x[0]) + x[0];
    double const bR = 2. * y[2] - 2. * y[1] - m[1] * (x[2] - x[1]), aR = y[1] - y[2] + m[1] * (x[2] - x[1]), cR = y[1] - t;
    double const outR = (-2. * cR) / (sqrt(bR * bR - 4. * aR * cR) + bR) * (x[2] - x[1]) + x[1];
    double res = t < y[1] ? outL : outR;
    res = t < y[0] ? (t - y[0]) / m[0] + x[0] : res;
    res = t < y[2] ? res : (t - y[2]) / m[1] + x[2];
    return res;
}

/* ComputeWBFwd / ComputeWBRev for one value; k = 0 whites, 1 blacks. */
static double alwan_gt_wb(alwan_gt_render const *r, int k, int ch, double t, int fwd) {
    double const *x = r->wb_x[k][ch], *y = r->wb_y[k][ch], *m = r->wb_m[k][ch];
    double const val = r->wb_val[k][ch], gain = r->wb_gain[k][ch];
    double const mtest = k ? 2. - val : val;
    double const a = 0.5 * (m[1] - m[0]) * (x[1] - x[0]), b = m[0] * (x[1] - x[0]);
    if (mtest == 1.) return t;
    if ((mtest < 1.) == (fwd != 0)) {   /* the quadratic, forward */
        double tl, res;
        if (mtest > 1.) t = !k ? (t - x[0]) * gain + x[0] : (t - x[1]) * gain + x[1];
        tl = (t - x[0]) / (x[1] - x[0]);
        res = tl * (x[1] - x[0]) * (tl * 0.5 * (m[1] - m[0]) + m[0]) + y[0];
        res = t < x[0] ? y[0] + (t - x[0]) * m[0] : res;
        if (mtest < 1.) return t < x[1] ? res : y[1] + (t - x[1]) * m[1];
        if (!k) {
            double const new_y1 = (x[1] - x[0]) / gain + x[0], xd = x[0] + (x[1] - x[0]) * 0.99;
            double md = 1. / (m[0] + (xd - x[0]) * (m[1] - m[0]) / (x[1] - x[0]));
            double const aa = 0.5 * (1. / m[1] - md) / (x[1] - xd), bb = 1. / m[1] - 2. * aa * x[1];
            double const cc = new_y1 - bb * x[1] - aa * x[1] * x[1];
            double const brk = (aa * x[1] + bb) * x[1] + cc;
            res = (res - x[0]) / gain + x[0];
            t = (t - x[0]) / gain + x[0];
            {
                double const c = cc - t;
                double const res1 = (-2. * c) / (sqrt(bb * bb - 4. * aa * c) + bb);
                return t < brk ? res : res1;
            }
        }
        res = t < x[1] ? res : y[1] + (t - x[1]) * m[1];
        return (res - x[1]) / gain + x[1];
    } else {                           /* the quadratic, inverted */
        double c, res;
        if (mtest > 1.) t = !k ? (t - x[0]) * gain + x[0] : (t - x[1]) * gain + x[1];
        c = y[0] - t;
        res = (-2. * c) / (sqrt(b * b - 4. * a * c) + b) * (x[1] - x[0]) + x[0];
        res = t < y[0] ? x[0] + (t - y[0]) / m[0] : res;
        if (mtest < 1.) return t < y[1] ? res : x[1] + (t - y[1]) / m[1];
        if (!k) {
            double const new_y1 = (x[1] - x[0]) / gain + x[0], xd = x[0] + (x[1] - x[0]) * 0.99;
            double md = 1. / (m[0] + (xd - x[0]) * (m[1] - m[0]) / (x[1] - x[0]));
            double const aa = 0.5 * (1. / m[1] - md) / (x[1] - xd), bb = 1. / m[1] - 2. * aa * x[1];
            double const cc = new_y1 - bb * x[1] - aa * x[1] * x[1];
            res = (res - x[0]) / gain + x[0];
            t = (t - x[0]) / gain + x[0];
            return t < x[1] ? res : (aa * t + bb) * t + cc;
        }
        res = t < y[1] ? res : x[1] + (t - y[1]) / m[1];
        return (res - x[1]) / gain + x[1];
    }
}

static double alwan_gt_sc(alwan_gt_render const *r, double t, int fwd) {
    double const *xt = r->sc_x[0], *yt = r->sc_y[0], *mt = r->sc_m[0];
    double const *xb = r->sc_x[1], *yb = r->sc_y[1], *mb = r->sc_m[1];
    double out;
    if (fwd) {
        double tR = (t - xt[1]) / (xt[2] - xt[1]);
        double res = tR * (xt[2] - xt[1]) * (tR * 0.5 * (mt[1] - mt[0]) + mt[0]) + yt[1];
        out = (t - r->pivot) * r->sc + r->pivot;
        out = t < xt[1] ? out : res;
        out = t < xt[2] ? out : yt[2] + (t - xt[2]) * mt[1];
        tR = (t - xb[1]) / (xb[2] - xb[1]);
        res = tR * (xb[2] - xb[1]) * (tR * 0.5 * (mb[1] - mb[0]) + mb[0]) + yb[1];
        out = t < xb[2] ? res : out;
        out = t < xb[1] ? yb[1] + (t - xb[1]) * mb[0] : out;
    } else {
        double b = mt[0] * (xt[2] - xt[1]), a = (mt[1] - mt[0]) * 0.5 * (xt[2] - xt[1]), c = yt[1] - t;
        double res = (xt[2] - xt[1]) * (-2. * c) / (sqrt(b * b - 4. * a * c) + b) + xt[1];
        out = (t - r->pivot) / r->sc + r->pivot;
        out = t < yt[1] ? out : res;
        out = t < yt[2] ? out : xt[2] + (t - yt[2]) / mt[1];
        b = mb[0] * (xb[2] - xb[1]); a = (mb[1] - mb[0]) * 0.5 * (xb[2] - xb[1]); c = yb[1] - t;
        res = (xb[2] - xb[1]) * (-2. * c) / (sqrt(b * b - 4. * a * c) + b) + xb[1];
        out = t < yb[2] ? res : out;
        out = t < yb[1] ? xb[1] + (t - yb[1]) / mb[0] : out;
    }
    return out;
}

/* One zone on a pixel: the channel's own curve for ch < 3, the master on all three. */
static void alwan_gt_mids(alwan_gt_render const *r, int ch, double *p, int fwd) {
    int i;
    if (r->mid_adj[ch] == 1.) return;
    if (ch < 3) p[ch] = fwd ? alwan_gt_mid_fwd(r, ch, p[ch], 0) : alwan_gt_mid_rev(r, ch, p[ch], 0);
    else for (i = 0; i < 3; i++) p[i] = fwd ? alwan_gt_mid_fwd(r, ch, p[i], 1) : alwan_gt_mid_rev(r, ch, p[i], 1);
}

static void alwan_gt_hs(alwan_gt_render const *r, int k, int ch, double *p, int fwd) {
    double const val = r->hs_val[k][ch];
    int const use_fwd = (val < 1.) == (fwd != 0);
    int i;
    if (val == 1.) return;
    for (i = 0; i < 3; i++) {
        if (ch < 3 && i != ch) continue;
        p[i] = use_fwd ? alwan_gt_hs_fwd(r->hs_x[k][ch], r->hs_y[k][ch], r->hs_m[k][ch], p[i])
                       : alwan_gt_hs_rev(r->hs_x[k][ch], r->hs_y[k][ch], r->hs_m[k][ch], p[i]);
    }
}

static void alwan_gt_wbz(alwan_gt_render const *r, int k, int ch, double *p, int fwd) {
    int i;
    for (i = 0; i < 3; i++) {
        if (ch < 3 && i != ch) continue;
        p[i] = alwan_gt_wb(r, k, ch, p[i], fwd);
    }
}

/* OCIO's LogLinConstants, float as OCIO has them. */
#define ALWAN_GT_XBRK ((double)0.0041318374739483946f)
#define ALWAN_GT_SHIFT ((double)-0.000157849851665374f)
#define ALWAN_GT_GAIN ((double)363.034608563f)

static double alwan_gt_linlog(double x) {
    double const m = (double)(1.f / (0.18f + -0.000157849851665374f));
    return x < ALWAN_GT_XBRK ? x * ALWAN_GT_GAIN + -7.0 : log2((x + ALWAN_GT_SHIFT) * m);
}

static double alwan_gt_loglin(double y) {
    return y < -5.5 ? (y - -7.0) / ALWAN_GT_GAIN
                    : pow(2.0, y) * (double)(0.18f + -0.000157849851665374f) - ALWAN_GT_SHIFT;
}

static void alwan_gt_pixel(double *p, alwan_gt_render const *r) {
    int ch, i;
    if (r->identity) return;
    if (r->style == ALWAN_GRADING_LIN) for (i = 0; i < 3; i++) p[i] = alwan_gt_linlog(p[i]);
    if (!r->inverse) {
        for (ch = 0; ch < 4; ch++) alwan_gt_mids(r, ch, p, 1);
        for (ch = 0; ch < 4; ch++) alwan_gt_hs(r, 0, ch, p, 1);
        for (ch = 0; ch < 4; ch++) alwan_gt_wbz(r, 0, ch, p, 1);
        for (ch = 0; ch < 4; ch++) alwan_gt_hs(r, 1, ch, p, 1);
        for (ch = 0; ch < 4; ch++) alwan_gt_wbz(r, 1, ch, p, 1);
        if (r->sc != 1.) for (i = 0; i < 3; i++) p[i] = alwan_gt_sc(r, p[i], 1);
    } else {
        static int const order[4] = { ALWAN_GT_M, ALWAN_GT_R, ALWAN_GT_G, ALWAN_GT_B };
        if (r->sc != 1.) for (i = 0; i < 3; i++) p[i] = alwan_gt_sc(r, p[i], 0);
        for (ch = 0; ch < 4; ch++) alwan_gt_wbz(r, 1, order[ch], p, 0);
        for (ch = 0; ch < 4; ch++) alwan_gt_hs(r, 1, order[ch], p, 0);
        for (ch = 0; ch < 4; ch++) alwan_gt_wbz(r, 0, order[ch], p, 0);
        for (ch = 0; ch < 4; ch++) alwan_gt_hs(r, 0, order[ch], p, 0);
        for (ch = 0; ch < 4; ch++) alwan_gt_mids(r, order[ch], p, 0);
    }
    if (r->style == ALWAN_GRADING_LIN) for (i = 0; i < 3; i++) p[i] = alwan_gt_loglin(p[i]);
    for (i = 0; i < 3; i++) p[i] = 65504.0 < p[i] ? 65504.0 : p[i];   /* std::min: a NaN passes */
}

/* ================================================================
 * B-SPLINE CURVES: OCIO's GradingBSplineCurve (GradingBSplineCurve.cpp), the monotone
 * piecewise-quadratic spline its RGB and hue curves share. A curve is fitted from its
 * control points (and optional slopes) to knots and quadratic coefficients.
 *
 * THE FIT IS IN FLOAT, as OCIO's is. Its slope estimation and knot placement branch on
 * float thresholds (1e-6, 1e-5, 2e-3 of the span), so a fit in double could take the
 * other branch on the same control points and give a different curve. Evaluation of
 * the fitted pieces is in double.
 * ================================================================ */

#define ALWAN_GC_MAXK (2 * ALWAN_GRADING_CURVE_MAX_POINTS + 4)

typedef struct {
    int sets;                                 /* 0: identity */
    int nknots;
    float knots[ALWAN_GC_MAXK];
    float a[ALWAN_GC_MAXK], b[ALWAN_GC_MAXK], c[ALWAN_GC_MAXK];
} alwan_gc_fit;

static int alwan_gc_slopes_default(alwan_grading_curve const *cv) {
    int i;
    for (i = 0; i < cv->count; i++) if ((float)cv->slopes[i] != 0.f) return 0;
    return 1;
}

static void alwan_gc_estimate_rgb_slopes(float const *x, float const *y, int n, float *slopes) {
    float secant[ALWAN_GRADING_CURVE_MAX_POINTS], len[ALWAN_GRADING_CURVE_MAX_POINTS];
    int i, k;
    for (i = 0; i < n - 1; i++) {
        float const dx = x[i + 1] - x[i], dy = y[i + 1] - y[i];
        secant[i] = dy / dx;
        len[i] = sqrtf(dx * dx + dy * dy);
    }
    if (n == 2) { slopes[0] = slopes[1] = secant[0]; return; }
    i = 0;
    for (;;) {
        int j = i;
        float dl = len[i];
        while (j < n - 2 && fabsf(secant[j + 1] - secant[j]) < 1e-6f) { dl += len[j + 1]; j++; }
        for (k = i; k <= j; k++) len[k] = dl;
        if (j >= n - 3) break;
        i = j + 1;
    }
    slopes[0] = 0.f;
    for (k = 1; k < n - 1; k++) {
        slopes[k] = (len[k] * secant[k] + len[k - 1] * secant[k - 1]) / (len[k] + len[k - 1]);
    }
    {
        float const e = 0.5f * (3.f * secant[n - 2] - slopes[n - 2]);
        slopes[n - 1] = e > 0.01f ? e : 0.01f;
    }
    {
        float const s = 0.5f * (3.f * secant[0] - slopes[1]);
        slopes[0] = s > 0.01f ? s : 0.01f;
    }
}

static void alwan_gc_push(alwan_gc_fit *f, float a, float b, float c) {
    f->a[f->sets] = a; f->b[f->sets] = b; f->c[f->sets] = c; f->sets++;
}

static void alwan_gc_fit_rgb(float const *x, float const *y, int n, float const *slopes, alwan_gc_fit *f) {
    int i;
    f->sets = 0;
    f->nknots = 0;
    f->knots[f->nknots++] = x[0];
    for (i = 0; i < n - 1; i++) {
        float const xi = x[i], xi1 = x[i + 1], yi = y[i];
        float const dx = xi1 - xi, dy = y[i + 1] - yi, sec = dy / dx;
        if (fabsf((slopes[i] + slopes[i + 1]) - 2.f * sec) < 1e-6f) {
            alwan_gc_push(f, 0.5f * (slopes[i + 1] - slopes[i]) / dx, slopes[i], yi);
        } else {
            float ksi, s_bar, eta;
            float const aa = slopes[i] - sec, bb = slopes[i + 1] - sec;
            if (aa * bb >= 0.f) ksi = (xi + xi1) * 0.5f;
            else if (fabsf(aa) > fabsf(bb)) ksi = xi1 + aa * dx / (slopes[i + 1] - slopes[i]);
            else ksi = xi + bb * dx / (slopes[i + 1] - slopes[i]);
            s_bar = (2.f * sec - slopes[i + 1]) + (slopes[i + 1] - slopes[i]) * (ksi - xi) / dx;
            eta = (s_bar - slopes[i]) / (ksi - xi);
            alwan_gc_push(f, 0.5f * eta, slopes[i], yi);
            alwan_gc_push(f, 0.5f * (slopes[i + 1] - s_bar) / (xi1 - ksi), s_bar,
                          yi + slopes[i] * (ksi - xi) + 0.5f * eta * (ksi - xi) * (ksi - xi));
            f->knots[f->nknots++] = ksi;
        }
        f->knots[f->nknots++] = xi1;
    }
}

static int alwan_gc_adjust_rgb(float const *x, float const *y, float *slopes, alwan_gc_fit const *f) {
    int done = 0, i = 0, j;
    for (j = 0; j < f->nknots; j++) {
        if (x[i] != f->knots[j]) {
            float const ksi = f->knots[j], xi = x[i], xi1 = x[i + 1], yi = y[i], yi1 = y[i + 1];
            float const s_bar = (2.f * (yi1 - yi) - (ksi - xi) * slopes[i] - (xi1 - ksi) * slopes[i + 1]) / (xi1 - xi);
            if (s_bar < 0.f) {
                float const secant = (yi1 - yi) / (xi1 - xi);
                float const blend = ((ksi - xi) * slopes[i] + (xi1 - ksi) * slopes[i + 1]) / (xi1 - xi);
                float aim = 0.01f * 0.5f * (slopes[i] + slopes[i + 1]);
                float adjust;
                done = 1;
                if (aim > secant) aim = secant;
                adjust = (2.f * secant - aim) / blend;
                slopes[i] *= adjust;
                slopes[i + 1] *= adjust;
            }
            i++;
        }
    }
    return done;
}

/* A B_SPLINE curve's validate(): at least two points, x and y non-decreasing. */
static int alwan_gc_valid_rgb(alwan_grading_curve const *cv) {
    float lx = -FLT_MAX, ly = -FLT_MAX;
    int i;
    if (cv->count < 2 || cv->count > ALWAN_GRADING_CURVE_MAX_POINTS) return 0;
    for (i = 0; i < cv->count; i++) {
        float const x = (float)cv->points[i].x, y = (float)cv->points[i].y;
        if (!alwan_gp_finite(cv->points[i].x) || !alwan_gp_finite(cv->points[i].y) || !alwan_gp_finite(cv->slopes[i])) return 0;
        if (x < lx || y < ly) return 0;
        lx = x;
        ly = y;
    }
    return 1;
}

static void alwan_gc_prepare_rgb(alwan_grading_curve const *cv, alwan_gc_fit *f) {
    float x[ALWAN_GRADING_CURVE_MAX_POINTS], y[ALWAN_GRADING_CURVE_MAX_POINTS], s[ALWAN_GRADING_CURVE_MAX_POINTS];
    int const n = cv->count, dflt = alwan_gc_slopes_default(cv);
    int i, identity = dflt;
    for (i = 0; i < n; i++) {
        x[i] = (float)cv->points[i].x;
        y[i] = (float)cv->points[i].y;
        s[i] = (float)cv->slopes[i];
        if (x[i] != y[i]) identity = 0;
    }
    f->sets = 0;
    f->nknots = 0;
    if (identity) return;
    if (dflt) alwan_gc_estimate_rgb_slopes(x, y, n, s);
    alwan_gc_fit_rgb(x, y, n, s, f);
    if (alwan_gc_adjust_rgb(x, y, s, f)) alwan_gc_fit_rgb(x, y, n, s, f);
}

/* KnotsCoefs::evalCurve on one fitted curve. */
static double alwan_gc_eval(alwan_gc_fit const *f, double x, double identity_x) {
    int const sets = f->sets, nk = f->nknots;
    double kstart, kend;
    int i;
    if (sets == 0) return identity_x;
    kstart = f->knots[0];
    kend = f->knots[nk - 1];
    if (x <= kstart) return (x - kstart) * f->b[0] + f->c[0];
    if (x >= kend) {
        double const a = f->a[sets - 1], b = f->b[sets - 1], c = f->c[sets - 1];
        double const t = kend - (double)f->knots[nk - 2];
        return (x - kend) * (2. * a * t + b) + ((a * t + b) * t + c);
    }
    for (i = 0; i < nk - 2; i++) if (x < f->knots[i + 1]) break;
    {
        double const t = x - (double)f->knots[i];
        return ((double)f->a[i] * t + f->b[i]) * t + f->c[i];
    }
}

/* KnotsCoefs::evalCurveRev: the inverse of a monotone curve. */
static double alwan_gc_eval_rev(alwan_gc_fit const *f, double y) {
    int const sets = f->sets, nk = f->nknots;
    double kstart, kend, ystart, yend;
    int i;
    if (sets == 0) return y;
    kstart = f->knots[0];
    kend = f->knots[nk - 1];
    ystart = f->c[0];
    {
        double const a = f->a[sets - 1], b = f->b[sets - 1], c = f->c[sets - 1];
        double const t = kend - (double)f->knots[nk - 2];
        yend = (a * t + b) * t + c;
        if (y >= yend && !(y <= ystart)) {
            double const slope = 2. * a * t + b;
            return fabs(slope) < 1e-5 ? kend : (y - yend) / slope + kend;
        }
    }
    if (y <= ystart) {
        double const b = f->b[0];
        return fabs(b) < 1e-5 ? kstart : (y - ystart) / b + kstart;
    }
    for (i = 0; i < nk - 2; i++) if (y < f->c[i + 1]) break;
    {
        double const a = f->a[i], b = f->b[i], c0 = (double)f->c[i] - y;
        return (double)f->knots[i] + (-2. * c0) / (sqrt(b * b - 4. * a * c0) + b);
    }
}

/* ---- RGB curve ---- */

typedef struct {
    int style, inverse, identity;
    alwan_gc_fit curve[4];   /* red, green, blue, master */
} alwan_grc_render;

static void alwan_grc_init(alwan_grading_rgb_curve *curves, alwan_grading_style style) {
    alwan_grading_curve *c[4];
    int i, k;
    if (!curves) return;
    c[0] = &curves->red; c[1] = &curves->green; c[2] = &curves->blue; c[3] = &curves->master;
    for (i = 0; i < 4; i++) {
        memset(c[i], 0, sizeof *c[i]);
        c[i]->count = 3;
        for (k = 0; k < 3; k++) {
            double const v = style == ALWAN_GRADING_LIN ? -7.0 + 7.0 * k : 0.5 * k;
            c[i]->points[k].x = v;
            c[i]->points[k].y = v;
        }
    }
}

static alwan_status alwan_grc_prepare(alwan_grc_render *r, alwan_grading_style style,
                                      alwan_grading_rgb_curve const *v, int inverse) {
    alwan_grading_curve const *c[4];
    int i;
    if (!v) return ALWAN_E_INVALID;
    if (style != ALWAN_GRADING_LOG && style != ALWAN_GRADING_LIN && style != ALWAN_GRADING_VIDEO) {
        return ALWAN_E_INVALID;
    }
    c[0] = &v->red; c[1] = &v->green; c[2] = &v->blue; c[3] = &v->master;
    for (i = 0; i < 4; i++) if (!alwan_gc_valid_rgb(c[i])) return ALWAN_E_INVALID;
    r->style = (int)style;
    r->inverse = inverse;
    r->identity = 1;
    for (i = 0; i < 4; i++) {
        alwan_gc_prepare_rgb(c[i], &r->curve[i]);
        if (r->curve[i].sets) r->identity = 0;
    }
    return ALWAN_OK;
}

static void alwan_grc_pixel(double *p, alwan_grc_render const *r) {
    int i;
    if (r->identity) return;
    if (r->style == ALWAN_GRADING_LIN) for (i = 0; i < 3; i++) p[i] = alwan_gt_linlog(p[i]);
    if (!r->inverse) {
        for (i = 0; i < 3; i++) p[i] = alwan_gc_eval(&r->curve[i], p[i], p[i]);
        for (i = 0; i < 3; i++) p[i] = alwan_gc_eval(&r->curve[3], p[i], p[i]);
    } else {
        for (i = 0; i < 3; i++) p[i] = alwan_gc_eval_rev(&r->curve[3], p[i]);
        for (i = 0; i < 3; i++) p[i] = alwan_gc_eval_rev(&r->curve[i], p[i]);
    }
    if (r->style == ALWAN_GRADING_LIN) for (i = 0; i < 3; i++) p[i] = alwan_gt_loglin(p[i]);
}

/* ---- hue curves: GradingHueCurve.cpp, GradingHueCurveOpCPU.cpp, and the RGB_TO_HSY
 * fixed functions of FixedFunctionOpCPU.cpp ---- */

enum { ALWAN_GH_HUE_HUE = 0, ALWAN_GH_HUE_SAT, ALWAN_GH_HUE_LUM, ALWAN_GH_LUM_SAT, ALWAN_GH_SAT_SAT,
       ALWAN_GH_LUM_LUM, ALWAN_GH_SAT_LUM, ALWAN_GH_HUE_FX };
/* OCIO's BSplineType for each role */
enum { ALWAN_GS_DIAGONAL, ALWAN_GS_HUE_HUE, ALWAN_GS_PERIODIC_1, ALWAN_GS_PERIODIC_0, ALWAN_GS_HORIZONTAL_1 };

static int alwan_gh_type(int role) {
    static int const t[8] = { ALWAN_GS_HUE_HUE, ALWAN_GS_PERIODIC_1, ALWAN_GS_PERIODIC_1, ALWAN_GS_HORIZONTAL_1,
                              ALWAN_GS_DIAGONAL, ALWAN_GS_DIAGONAL, ALWAN_GS_HORIZONTAL_1, ALWAN_GS_PERIODIC_0 };
    return t[role];
}

/* GradingBSplineCurveImpl::validate() for the hue-curve types. */
static int alwan_gh_valid(alwan_grading_curve const *cv, int type) {
    int const n = cv->count;
    float lx = -FLT_MAX, ly;
    int i;
    if (n < 2 || n > ALWAN_GRADING_CURVE_MAX_POINTS) return 0;
    for (i = 0; i < n; i++) {
        float const x = (float)cv->points[i].x;
        if (!alwan_gp_finite(cv->points[i].x) || !alwan_gp_finite(cv->points[i].y) || !alwan_gp_finite(cv->slopes[i])) return 0;
        if (x < lx) return 0;
        lx = x;
    }
    if (type == ALWAN_GS_HUE_HUE && ((float)cv->points[0].x < 0.f || (float)cv->points[n - 1].x > 1.f)) return 0;
    if (type == ALWAN_GS_DIAGONAL || type == ALWAN_GS_HUE_HUE) {
        ly = type == ALWAN_GS_HUE_HUE ? (float)cv->points[n - 1].y - 1.f : -FLT_MAX;
        for (i = 0; i < n; i++) {
            float const y = (float)cv->points[i].y;
            if (y < ly) return 0;
            ly = y;
        }
    }
    if (n == 2 && (type == ALWAN_GS_PERIODIC_1 || type == ALWAN_GS_PERIODIC_0 || type == ALWAN_GS_HUE_HUE)) {
        float const dx = (float)cv->points[1].x - (float)cv->points[0].x;
        if (fabsf(1.f - dx) < 1e-3f) return 0;
    }
    return 1;
}

static int alwan_gh_identity(alwan_grading_curve const *cv, int type) {
    int i;
    if (!alwan_gc_slopes_default(cv)) return 0;
    for (i = 0; i < cv->count; i++) {
        float const x = (float)cv->points[i].x, y = (float)cv->points[i].y;
        if (type == ALWAN_GS_DIAGONAL || type == ALWAN_GS_HUE_HUE) { if (x != y) return 0; }
        else if (type == ALWAN_GS_PERIODIC_0) { if (y != 0.f) return 0; }
        else if (y != 1.f) return 0;
    }
    return 1;
}

/* PrepHueCurveData: wrap, sort by x, keep a minimum spacing, extend a periodic curve by
 * one point at each end. Returns the new count. */
static int alwan_gh_prep(alwan_grading_curve const *cv, int periodic, int horizontal, float *x, float *y) {
    int const n = cv->count;
    float const tol = 2e-3f;
    float xspan, yspan;
    int i, j;
    for (i = 0; i < n; i++) {
        float const xv = (float)cv->points[i].x, yv = (float)cv->points[i].y;
        if (periodic && xv < 0.f) { x[i] = xv + 1.f; y[i] = horizontal ? yv : yv + 1.f; }
        else if (periodic && xv >= 1.f) { x[i] = xv - 1.f; y[i] = horizontal ? yv : yv - 1.f; }
        else { x[i] = xv; y[i] = yv; }
    }
    for (i = 0; i < n; i++) {   /* selection sort, as OCIO's */
        int mi = i;
        float mv = x[i], t;
        for (j = i + 1; j < n; j++) if (x[j] < mv) { mv = x[j]; mi = j; }
        t = x[i]; x[i] = x[mi]; x[mi] = t;
        t = y[i]; y[i] = y[mi]; y[mi] = t;
    }
    xspan = x[n - 1] - x[0];
    for (i = 1; i < n; i++) if ((x[i] - x[i - 1]) < xspan * tol) x[i] = x[i - 1] + xspan * tol;
    if (!horizontal) {
        yspan = y[n - 1] - y[0];
        for (i = 1; i < n; i++) if ((y[i] - y[i - 1]) < yspan * tol) y[i] = y[i - 1] + yspan * tol;
    }
    if (periodic) {
        float const fx = x[n - 1] - 1.f, fy = horizontal ? y[n - 1] : y[n - 1] - 1.f;
        float lxv, lyv;
        for (i = n; i > 0; i--) { x[i] = x[i - 1]; y[i] = y[i - 1]; }
        x[0] = fx; y[0] = fy;
        lxv = x[1] + 1.f; lyv = horizontal ? y[1] : y[1] + 1.f;
        x[n + 1] = lxv; y[n + 1] = lyv;
        return n + 2;
    }
    return n;
}

static void alwan_gh_estimate_slopes(float const *x, float const *y, int n, float *slopes, int periodic, int horizontal) {
    float secant[ALWAN_GRADING_CURVE_MAX_POINTS + 2], len[ALWAN_GRADING_CURVE_MAX_POINTS + 2];
    int i, k;
    for (i = 0; i < n - 1; i++) {
        float const dx = x[i + 1] - x[i], dy = y[i + 1] - y[i];
        secant[i] = dy / dx;
        len[i] = sqrtf(dx * dx + dy * dy);
    }
    if (n == 2) { slopes[0] = slopes[1] = secant[0]; return; }
    slopes[0] = 0.f;
    if (horizontal) {
        for (i = 1; i < n - 1; i++) {
            float s, denom = secant[i] + secant[i - 1];
            if (fabsf(denom) < 1e-3f) s = 2.f * secant[i] * secant[i - 1] / (denom < 0.f ? -1e-3f : 1e-3f);
            else s = 2.f * secant[i] * secant[i - 1] / denom;
            if (secant[i] * secant[i - 1] <= 0.f) s = 0.f;
            slopes[i] = s;
        }
        slopes[n - 1] = 0.5f * (3.f * secant[n - 2] - slopes[n - 2]);
        slopes[0] = 0.5f * (3.f * secant[0] - slopes[1]);
    } else {
        i = 0;
        for (;;) {
            int j = i;
            float dl = len[i];
            while (j < n - 2 && fabsf(secant[j + 1] - secant[j]) < 1e-6f) { dl += len[j + 1]; j++; }
            for (k = i; k <= j; k++) len[k] = dl;
            if (j >= n - 3) break;
            i = j + 1;
        }
        for (k = 1; k < n - 1; k++) slopes[k] = (len[k] * secant[k] + len[k - 1] * secant[k - 1]) / (len[k] + len[k - 1]);
        {
            float const e = 0.5f * (3.f * secant[n - 2] - slopes[n - 2]);
            slopes[n - 1] = e > 0.01f ? e : 0.01f;
        }
        {
            float const s = 0.5f * (3.f * secant[0] - slopes[1]);
            slopes[0] = s > 0.01f ? s : 0.01f;
        }
    }
    for (i = 0; i < n - 1; i++) {   /* shape preservation */
        float kk = 0.2f, near_min, scale = 1.f;
        if (fabsf(slopes[i]) > fabsf(slopes[i + 1])) kk = 1.f - kk;
        near_min = slopes[i] + kk * (slopes[i + 1] - slopes[i]);
        if (near_min != 0.f) scale = 0.75f * 2.f * secant[i] / near_min;
        if (scale < 1.f) { slopes[i] *= scale; slopes[i + 1] *= scale; }
    }
    if (periodic) { slopes[0] = slopes[n - 2]; slopes[n - 1] = slopes[1]; }
}

static float alwan_gh_ksi(int i, float const *x, float const *y, float const *slopes) {
    float const k = 0.2f, dx = x[i + 1] - x[i];
    float secant = (y[i + 1] - y[i]) / dx, m0 = slopes[i], m1 = slopes[i + 1];
    float x_mid, left, right, top, bottom, m_min, m_max, dm, b, b_high, b_low, bbb, bb, rel, alpha;
    if (secant < 0.f) { m0 = -slopes[i]; m1 = -slopes[i + 1]; secant = -secant; }
    x_mid = x[i] + 0.5f * dx;
    left = x[i] + dx * k;
    right = x[i + 1] - dx * k;
    top = left; bottom = right; m_min = m0; m_max = m1;
    if (m0 > m1) { m_max = m0; m_min = m1; top = right; bottom = left; }
    dm = m_max - m_min;
    b = 1.f - 0.5f * k;
    b_high = m_min + b * dm;
    b_low = m_min + (1.f - b) * dm;
    bbb = m_max * 4.f;
    bb = m_max * 1.1f;
    rel = dm / (m_max > 0.01f ? m_max : 0.01f);
    alpha = (rel - 0.05f) / (0.75f - 0.05f);
    alpha = alpha < 1.f ? alpha : 1.f;
    alpha = alpha > 0.f ? alpha : 0.f;
    top = x_mid + alpha * (top - x_mid);
    bottom = x_mid + alpha * (bottom - x_mid);
    if (secant >= bbb) return x_mid;
    if (secant > bb) return top + (secant - bb) / (bbb - bb) * (x_mid - top);
    if (secant >= b_high) return top;
    if (secant > b_low && b_high != b_low) return bottom + (secant - b_low) / (b_high - b_low) * (top - bottom);
    return bottom;
}

static void alwan_gh_fit(float const *x, float const *y, int n, float const *slopes, alwan_gc_fit *f) {
    int i;
    f->sets = 0;
    f->nknots = 0;
    f->knots[f->nknots++] = x[0];
    for (i = 0; i < n - 1; i++) {
        float const dx = x[i + 1] - x[i], sec = (y[i + 1] - y[i]) / dx;
        if (fabsf((slopes[i] + slopes[i + 1]) - 2.f * sec) <= 1e-5f) {
            alwan_gc_push(f, 0.5f * (slopes[i + 1] - slopes[i]) / dx, slopes[i], y[i]);
        } else {
            float const ksi = alwan_gh_ksi(i, x, y, slopes);
            float const m_bar = (2.f * sec - slopes[i + 1]) + (slopes[i + 1] - slopes[i]) * (ksi - x[i]) / (x[i + 1] - x[i]);
            float const eta = (m_bar - slopes[i]) / (ksi - x[i]);
            alwan_gc_push(f, 0.5f * eta, slopes[i], y[i]);
            alwan_gc_push(f, 0.5f * (slopes[i + 1] - m_bar) / (x[i + 1] - ksi), m_bar,
                          y[i] + slopes[i] * (ksi - x[i]) + 0.5f * eta * (ksi - x[i]) * (ksi - x[i]));
            f->knots[f->nknots++] = ksi;
        }
        f->knots[f->nknots++] = x[i + 1];
    }
}

/* computeKnotsAndCoefsForHueCurve */
static void alwan_gh_prepare_curve(alwan_grading_curve const *cv, int type, alwan_gc_fit *f) {
    float x[ALWAN_GRADING_CURVE_MAX_POINTS + 2], y[ALWAN_GRADING_CURVE_MAX_POINTS + 2], s[ALWAN_GRADING_CURVE_MAX_POINTS + 2];
    int const periodic = type == ALWAN_GS_PERIODIC_1 || type == ALWAN_GS_PERIODIC_0 || type == ALWAN_GS_HUE_HUE;
    int horizontal = !(type == ALWAN_GS_DIAGONAL || type == ALWAN_GS_HUE_HUE);
    int n, i;
    f->sets = 0;
    f->nknots = 0;
    if (alwan_gh_identity(cv, type)) return;
    n = alwan_gh_prep(cv, periodic, horizontal, x, y);
    if (type == ALWAN_GS_HUE_HUE) horizontal = 1;
    if (!alwan_gc_slopes_default(cv)) {
        /* the caller's slopes, in the caller's order; a periodic curve takes the last
         * slope at both new ends, as OCIO does */
        int const m = cv->count;
        if (periodic) {
            s[0] = (float)cv->slopes[m - 1];
            for (i = 0; i < m; i++) s[i + 1] = (float)cv->slopes[i];
            s[m + 1] = s[0];
        } else {
            for (i = 0; i < m; i++) s[i] = (float)cv->slopes[i];
        }
    } else {
        alwan_gh_estimate_slopes(x, y, n, s, periodic, horizontal);
    }
    alwan_gh_fit(x, y, n, s, f);
}

/* KnotsCoefs::evalCurveRevHue: inverts HUE_FX (is_fx) and HUE_HUE. */
static double alwan_gh_eval_rev_hue(alwan_gc_fit const *f, double y, int is_fx) {
    int const sets = f->sets, nk = f->nknots;
    double kstart, kend, ystart, yend, a, b, c, kn, c0;
    int i;
    if (sets == 0) return y;
    kstart = f->knots[0];
    kend = f->knots[nk - 1];
    ystart = f->c[0];
    ystart = is_fx ? ystart + kstart : ystart;
    {
        double const aa = f->a[sets - 1], bb = f->b[sets - 1], cc = f->c[sets - 1];
        double const t = kend - (double)f->knots[nk - 2];
        yend = (aa * t + bb) * t + cc;
        yend = is_fx ? yend + kend : yend;
    }
    if (y < ystart) y += ceil(ystart - y);
    else if (y > yend) y -= ceil(y - yend);
    for (i = 0; i < nk - 2; i++) {
        double cy = f->c[i + 1];
        cy = is_fx ? cy + (double)f->knots[i + 1] : cy;
        if (y < cy) break;
    }
    a = f->a[i]; b = f->b[i]; c = f->c[i]; kn = f->knots[i];
    if (is_fx) { c += kn; b += 1.; }
    c0 = c - y;
    return kn + (-2. * c0) / (sqrt(b * b - 4. * a * c0) + b);
}

/* applyRGBToHSY; style 0 log, 1 lin, 2 video */
static void alwan_gh_rgb_to_hsy(double *p, int style) {
    double const r = p[0], g = p[1], b = p[2];
    double const mn = (r < g ? r : g) < b ? (r < g ? r : g) : b;
    double const mx = (r > g ? r : g) > b ? (r > g ? r : g) : b;
    double const luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    double const dist = fabs(r - luma) + fabs(g - luma) + fabs(b - luma);
    double sat, hue = 0.0;
    if (style == ALWAN_GRADING_LIN) {
        double const sum = r + g + b, k = 0.15;
        double const lo = 0.07 * dist + 1e-6, den = k + sum;
        double const hi = dist / (lo > den ? lo : den);
        double const sat_lo = dist * 5.0;
        double const alpha = alwan_gt_clampd((luma - 0.001) / (0.01 - 0.001), 0.0, 1.0);
        sat = (sat_lo + alpha * (hi - sat_lo)) * 1.4;
    } else {
        sat = dist * (style == ALWAN_GRADING_LOG ? 4.0 : 1.25);
    }
    if (mn != mx) {
        double const delta = mx - mn;
        if (r == mx) hue = 1.0 + (g - b) / delta;
        else if (g == mx) hue = 3.0 + (b - r) / delta;
        else hue = 5.0 + (r - g) / delta;
        hue *= 1.0 / 6.0;
    }
    p[0] = hue; p[1] = sat; p[2] = luma;
}

/* applyHSYToRGB */
static void alwan_gh_hsy_to_rgb(double *p, int style) {
    double hue = p[0] - 1.0 / 6.0, sat = p[1];
    double const luma = p[2];
    double red, grn, blu, cur, dist, gain = sat;
    hue = luma < 0.0 ? hue + 0.5 : hue;
    hue = (hue - floor(hue)) * 6.0;
    red = alwan_gt_clampd(fabs(hue - 3.0) - 1.0, 0.0, 1.0);
    grn = alwan_gt_clampd(2.0 - fabs(hue - 2.0), 0.0, 1.0);
    blu = alwan_gt_clampd(2.0 - fabs(hue - 4.0), 0.0, 1.0);
    cur = 0.2126 * red + 0.7152 * grn + 0.0722 * blu;
    red *= luma / cur;
    grn *= luma / cur;
    blu *= luma / cur;
    dist = fabs(red - luma) + fabs(grn - luma) + fabs(blu - luma);
    if (style == ALWAN_GRADING_LIN) {
        double const sum = red + grn + blu, k = 0.15, lo_gain = 5.0;
        double tmp, s1, s0, alpha;
        sat /= 1.4;
        tmp = -sat * sum + sat * 3.0 * luma + dist;
        tmp = tmp > 1e-6 ? tmp : 1e-6;
        s1 = sat * (k + 3.0 * luma) / tmp;
        s1 = s1 < 50.0 ? s1 : 50.0;
        s0 = sat / (dist * lo_gain > 1e-10 ? dist * lo_gain : 1e-10);
        alpha = alwan_gt_clampd((luma - 0.001) / (0.01 - 0.001), 0.0, 1.0);
        if (alpha == 1.0) gain = s1;
        else if (alpha == 0.0) gain = s0;
        else {
            double const a = dist * lo_gain * (1.0 - alpha) * (sum - 3.0 * luma);
            double const b = dist * lo_gain * (1.0 - alpha) * (k + 3.0 * luma) + dist * alpha - sat * (sum - 3.0 * luma);
            double const c = -sat * (k + 3.0 * luma);
            double const disc = sqrt(b * b - 4.0 * a * c);
            double const den = -disc - b;
            gain = (2.0 * c) / den;
            gain = gain >= 0.0 ? gain : (2.0 * c) / (den + disc * 2.0);
        }
    } else {
        double const cs = dist * (style == ALWAN_GRADING_LOG ? 4.0 : 1.25);
        gain = sat / (cs > 1e-10 ? cs : 1e-10);
    }
    p[0] = luma + gain * (red - luma);
    p[1] = luma + gain * (grn - luma);
    p[2] = luma + gain * (blu - luma);
}

typedef struct {
    int style, inverse, identity;
    alwan_gc_fit curve[8];
} alwan_ghc_render;

static alwan_grading_curve const *alwan_gh_role(alwan_grading_hue_curve const *v, int role) {
    alwan_grading_curve const *c[8];
    c[0] = &v->hue_hue; c[1] = &v->hue_sat; c[2] = &v->hue_lum; c[3] = &v->lum_sat;
    c[4] = &v->sat_sat; c[5] = &v->lum_lum; c[6] = &v->sat_lum; c[7] = &v->hue_fx;
    return c[role];
}

static void alwan_ghc_init(alwan_grading_hue_curve *curves, alwan_grading_style style) {
    int const lin = style == ALWAN_GRADING_LIN;
    int role, k;
    if (!curves) return;
    memset(curves, 0, sizeof *curves);
    for (role = 0; role < 8; role++) {
        alwan_grading_curve *c = (alwan_grading_curve *)alwan_gh_role(curves, role);
        int const type = alwan_gh_type(role);
        if (type == ALWAN_GS_HUE_HUE || type == ALWAN_GS_PERIODIC_1 || type == ALWAN_GS_PERIODIC_0) {
            c->count = 6;
            for (k = 0; k < 6; k++) {
                c->points[k].x = (double)((float)k / 6.f);
                c->points[k].y = type == ALWAN_GS_HUE_HUE ? c->points[k].x : type == ALWAN_GS_PERIODIC_1 ? 1.0 : 0.0;
            }
        } else {
            double const lo = (lin && (role == ALWAN_GH_LUM_SAT || role == ALWAN_GH_LUM_LUM)) ? -7.0 : 0.0;
            double const hi = (lin && (role == ALWAN_GH_LUM_SAT || role == ALWAN_GH_LUM_LUM)) ? 7.0 : 1.0;
            c->count = 3;
            for (k = 0; k < 3; k++) {
                c->points[k].x = lo + (hi - lo) * 0.5 * k;
                c->points[k].y = type == ALWAN_GS_DIAGONAL ? c->points[k].x : 1.0;
            }
        }
    }
}

static alwan_status alwan_ghc_prepare(alwan_ghc_render *r, alwan_grading_style style,
                                      alwan_grading_hue_curve const *v, int inverse) {
    int role;
    if (!v) return ALWAN_E_INVALID;
    if (style != ALWAN_GRADING_LOG && style != ALWAN_GRADING_LIN && style != ALWAN_GRADING_VIDEO) {
        return ALWAN_E_INVALID;
    }
    for (role = 0; role < 8; role++) if (!alwan_gh_valid(alwan_gh_role(v, role), alwan_gh_type(role))) return ALWAN_E_INVALID;
    r->style = (int)style;
    r->inverse = inverse;
    r->identity = 1;
    for (role = 0; role < 8; role++) {
        alwan_gh_prepare_curve(alwan_gh_role(v, role), alwan_gh_type(role), &r->curve[role]);
        if (r->curve[role].sets) r->identity = 0;
    }
    return ALWAN_OK;
}

static double alwan_gh_max0(double v) { return v > 0.0 ? v : 0.0; }

static void alwan_ghc_pixel(double *p, alwan_ghc_render const *r) {
    alwan_gc_fit const *cv = r->curve;
    int const lin = r->style == ALWAN_GRADING_LIN;
    if (r->identity) return;
    alwan_gh_rgb_to_hsy(p, r->style);
    if (!r->inverse) {
        double const hue_sat = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_HUE_SAT], p[0], 1.0));
        double hue_lum = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_HUE_LUM], p[0], 1.0));
        double lum_sat, sat_lum;
        if (lin) p[2] = alwan_gt_linlog(p[2]);
        p[0] = alwan_gc_eval(&cv[ALWAN_GH_HUE_HUE], p[0], p[0]);
        p[1] = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_SAT_SAT], p[1], p[1]));
        lum_sat = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_LUM_SAT], p[2], 1.0));
        p[1] *= lum_sat * hue_sat;
        sat_lum = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_SAT_LUM], p[1], 1.0));
        p[2] = alwan_gc_eval(&cv[ALWAN_GH_LUM_LUM], p[2], p[2]);
        if (lin) p[2] = alwan_gt_loglin(p[2]);
        hue_lum = 1.0 - (1.0 - hue_lum) * (p[1] < 1.0 ? p[1] : 1.0);
        p[2] = lin ? p[2] * hue_lum * sat_lum : p[2] + (hue_lum + sat_lum - 2.0) * 0.1;
        p[0] = p[0] - floor(p[0]);
        p[0] = p[0] + alwan_gc_eval(&cv[ALWAN_GH_HUE_FX], p[0], 0.0);
    } else {
        double hue_sat, hue_lum, sat_lum, lum_gain, lum_sat, sat_gain;
        p[0] = alwan_gh_eval_rev_hue(&cv[ALWAN_GH_HUE_FX], p[0], 1);
        p[0] = alwan_gh_eval_rev_hue(&cv[ALWAN_GH_HUE_HUE], p[0], 0);
        p[0] = p[0] - floor(p[0]);
        hue_sat = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_HUE_SAT], p[0], 1.0));
        hue_lum = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_HUE_LUM], p[0], 1.0));
        p[1] = alwan_gh_max0(p[1]);
        sat_lum = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_SAT_LUM], p[1], 1.0));
        hue_lum = 1.0 - (1.0 - hue_lum) * (p[1] < 1.0 ? p[1] : 1.0);
        lum_gain = hue_lum * sat_lum;
        p[2] = lin ? p[2] / (lum_gain > 0.01 ? lum_gain : 0.01) : p[2] - (hue_lum + sat_lum - 2.0) * 0.1;
        if (lin) p[2] = alwan_gt_linlog(p[2]);
        p[2] = alwan_gc_eval_rev(&cv[ALWAN_GH_LUM_LUM], p[2]);
        lum_sat = alwan_gh_max0(alwan_gc_eval(&cv[ALWAN_GH_LUM_SAT], p[2], 1.0));
        if (lin) p[2] = alwan_gt_loglin(p[2]);
        sat_gain = lum_sat * hue_sat;
        p[1] /= sat_gain > 0.01 ? sat_gain : 0.01;
        p[1] = alwan_gh_max0(alwan_gc_eval_rev(&cv[ALWAN_GH_SAT_SAT], p[1]));
    }
    alwan_gh_hsy_to_rgb(p, r->style);
}

/* ---- exposure and contrast ----
 *
 * OCIO's ExposureContrastTransform (ops/exposurecontrast/ExposureContrastOpCPU.cpp):
 *
 *   lin    out = pow(max(0, in 2^exposure / pivot), c) pivot,       c = contrast gamma
 *   video  the same with 2^exposure and pivot raised to 1 / 1.83 first
 *   log    out = in c + (exposure step - p) c + p,  p = log2(pivot / 0.18) step + mid_gray
 *
 * c and the pivot are floored at 0.001, and with c exactly 1 the lin and video styles
 * only scale (a negative value passes through), as OCIO does. The inverse runs each
 * backwards. OCIO's inverse log style reads the exposure step from a default of 0.088
 * instead of the step it was given, so with any other step it does not invert its own
 * forward transform (0.038 off at exposure 1.2 and step 0.12); this uses the step given.
 */
typedef struct {
    int style, inverse;
    double c, pivot, e;   /* e: 2^exposure (lin, video) or exposure step (log) */
} alwan_gec_render;

static void alwan_gec_init(alwan_grading_exposure_contrast *ec) {
    ec->exposure = 0.0;
    ec->contrast = 1.0;
    ec->gamma = 1.0;
    ec->pivot = 0.18;
    ec->log_exposure_step = 0.088;
    ec->log_mid_gray = 0.435;
}

static alwan_status alwan_gec_prepare(alwan_gec_render *r, alwan_grading_style style,
                                      alwan_grading_exposure_contrast const *ec, int inverse) {
    double const video_power = 0.54644808743169393; /* 1 / 1.83 */
    double pivot;
    if (!ec) return ALWAN_E_INVALID;
    if (style != ALWAN_GRADING_LOG && style != ALWAN_GRADING_LIN && style != ALWAN_GRADING_VIDEO) return ALWAN_E_INVALID;
    if (!alwan_gp_finite(ec->exposure) || !alwan_gp_finite(ec->contrast) || !alwan_gp_finite(ec->gamma) ||
        !alwan_gp_finite(ec->pivot) || !alwan_gp_finite(ec->log_exposure_step) || !alwan_gp_finite(ec->log_mid_gray)) {
        return ALWAN_E_INVALID;
    }
    r->style = (int)style;
    r->inverse = inverse;
    pivot = ec->pivot > 0.001 ? ec->pivot : 0.001;
    if (style == ALWAN_GRADING_LOG) {
        double const p = log2(pivot / 0.18) * ec->log_exposure_step + ec->log_mid_gray;
        double const cg = ec->contrast * ec->gamma;
        r->pivot = p > 0.0 ? p : 0.0;
        r->e = ec->exposure * ec->log_exposure_step;
        if (!inverse) r->c = cg > 0.001 ? cg : 0.001;
        else r->c = 1.0 / cg > 0.001 ? 1.0 / cg : 0.001; /* OCIO floors the reciprocal */
    } else {
        double const cg = ec->contrast * ec->gamma;
        r->c = cg > 0.001 ? cg : 0.001;
        r->e = pow(2.0, ec->exposure);
        r->pivot = pivot;
        if (style == ALWAN_GRADING_VIDEO) {
            r->e = pow(r->e, video_power);
            r->pivot = pow(pivot, video_power);
        }
    }
    return ALWAN_OK;
}

static void alwan_gec_pixel(double *p, alwan_gec_render const *r) {
    int k;
    if (r->style == ALWAN_GRADING_LOG) {
        if (!r->inverse) {
            double const off = (r->e - r->pivot) * r->c + r->pivot;
            for (k = 0; k < 3; k++) p[k] = p[k] * r->c + off;
        } else {
            double const off = r->pivot - r->pivot * r->c - r->e;
            for (k = 0; k < 3; k++) p[k] = p[k] * r->c + off;
        }
        return;
    }
    if (r->c == 1.0) {
        for (k = 0; k < 3; k++) p[k] = r->inverse ? p[k] / r->e : p[k] * r->e;
        return;
    }
    for (k = 0; k < 3; k++) {
        if (!r->inverse) {
            double const v = p[k] * (r->e / r->pivot);
            p[k] = pow(v > 0.0 ? v : 0.0, r->c) * r->pivot;
        } else {
            double const v = p[k] / r->pivot;
            p[k] = pow(v > 0.0 ? v : 0.0, 1.0 / r->c) * (r->pivot / r->e);
        }
    }
}

/* ---- the family ---- */

void alwan_grading_params_init(alwan_grading_params *params, alwan_grading_style style) {
    if (!params) return;
    alwan_gp_init(&params->primary, style);
    alwan_gt_init(&params->tone, style);
    alwan_grc_init(&params->rgb_curve, style);
    alwan_ghc_init(&params->hue_curve, style);
    alwan_gec_init(&params->exposure_contrast);
}

typedef union {
    alwan_gp_render gp;
    alwan_gt_render gt;
    alwan_grc_render grc;
    alwan_ghc_render ghc;
    alwan_gec_render gec;
} alwan_grading_render;

static alwan_status alwan_grading_run(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count,
                                      alwan_grading_op op, alwan_grading_style style, alwan_grading_params const *params,
                                      int inverse, int is_f32) {
    size_t const elem = is_f32 ? sizeof(alwan_f32) : sizeof(alwan_f64);
    alwan_grading_params *defaults = NULL;
    alwan_grading_render r;
    alwan_status st;
    size_t i;
    if (!out || !in) return ALWAN_E_INVALID;
    if (out_stride < 3 * elem || in_stride < 3 * elem) return ALWAN_E_INVALID;
    if (!params) { /* an identity grade: the defaults of the style */
        defaults = (alwan_grading_params *)ALWAN_ALLOC(sizeof(alwan_grading_params), sizeof(double));
        if (!defaults) return ALWAN_E_NOMEM;
        alwan_grading_params_init(defaults, style);
        params = defaults;
    }
    switch (op) {
    case ALWAN_GRADING_PRIMARY: st = alwan_gp_prepare(&r.gp, style, &params->primary, inverse != 0); break;
    case ALWAN_GRADING_TONE: st = alwan_gt_prepare(&r.gt, style, &params->tone, inverse != 0); break;
    case ALWAN_GRADING_RGB_CURVE: st = alwan_grc_prepare(&r.grc, style, &params->rgb_curve, inverse != 0); break;
    case ALWAN_GRADING_HUE_CURVE: st = alwan_ghc_prepare(&r.ghc, style, &params->hue_curve, inverse != 0); break;
    case ALWAN_GRADING_EXPOSURE_CONTRAST: st = alwan_gec_prepare(&r.gec, style, &params->exposure_contrast, inverse != 0); break;
    default: st = ALWAN_E_INVALID; break;
    }
    if (defaults) ALWAN_FREE(defaults);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        char const *s = (char const *)in + i * in_stride;
        char *d = (char *)out + i * out_stride;
        double p[3];
        int k;
        for (k = 0; k < 3; k++) p[k] = is_f32 ? (double)((alwan_f32 const *)s)[k] : ((alwan_f64 const *)s)[k];
        switch (op) {
        case ALWAN_GRADING_PRIMARY: alwan_gp_pixel(p, &r.gp); break;
        case ALWAN_GRADING_TONE: alwan_gt_pixel(p, &r.gt); break;
        case ALWAN_GRADING_RGB_CURVE: alwan_grc_pixel(p, &r.grc); break;
        case ALWAN_GRADING_HUE_CURVE: alwan_ghc_pixel(p, &r.ghc); break;
        default: alwan_gec_pixel(p, &r.gec); break;
        }
        for (k = 0; k < 3; k++) {
            if (is_f32) ((alwan_f32 *)d)[k] = (alwan_f32)p[k];
            else ((alwan_f64 *)d)[k] = p[k];
        }
    }
    return ALWAN_OK;
}

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_grading_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_grading_op op,
                                     alwan_grading_style style, alwan_grading_params const *params, int inverse) {
    return alwan_grading_run(rgb_out, sizeof(alwan_rgb_f64), rgb_in, sizeof(alwan_rgb_f64), 1, op, style, params, inverse, 0);
}

alwan_status alwan_grading_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride,
                                              size_t count, alwan_grading_op op, alwan_grading_style style,
                                              alwan_grading_params const *params, int inverse) {
    return alwan_grading_run(out, out_stride, in, in_stride, count, op, style, params, inverse, 0);
}
#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32
alwan_status alwan_grading_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_grading_op op,
                                     alwan_grading_style style, alwan_grading_params const *params, int inverse) {
    return alwan_grading_run(rgb_out, sizeof(alwan_rgb_f32), rgb_in, sizeof(alwan_rgb_f32), 1, op, style, params, inverse, 1);
}

alwan_status alwan_grading_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride,
                                              size_t count, alwan_grading_op op, alwan_grading_style style,
                                              alwan_grading_params const *params, int inverse) {
    return alwan_grading_run(out, out_stride, in, in_stride, count, op, style, params, inverse, 1);
}
#endif /* ALWAN_WITH_F32 */
