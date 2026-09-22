/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Colour selection: soft masks that say how much each pixel belongs to a chosen
 * set of colours, by four approaches.
 *
 *   qualifier   ranges of hue, chroma and lightness with soft edges, the
 *               secondary-grading "HSL qualifier", evaluated in HSV, OkLCh or
 *               CIE LCh(ab)
 *   distance    nearness to one key colour under a colour-difference metric
 *               (Oklab Euclidean, CIE 1976, CIEDE2000)
 *   example     a Gaussian fitted to sample pixels in Oklab, keyed on the
 *               Mahalanobis distance: pick a few pixels, select their kind
 *   chroma key  the green / blue screen difference matte with despill
 *               (Vlahos; Smith and Blinn 1996)
 *
 * The work is done in double for both precisions; the f32 entry points widen
 * the pixels and narrow the mask, as the tonescale grade does.
 *
 * Every conversion runs through the core _v functions, which are in natural
 * units whatever ALWAN_NORMALIZE_RANGES says (CIE hue in degrees, Oklab hue in
 * radians, L* in 0..100), and the qualifier rescales them itself to one
 * convention in every build: hue in turns from the +a axis, lightness in [0, 1].
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_oklab_core.h"
#include "../core/alwan_convenience_core.h"
#include <float.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Shared: the pixel pipeline and the soft ramp
 * ---------------------------------------------------------------- */

static int sel_finite(double v) { return v == v && v <= DBL_MAX && v >= -DBL_MAX; }
static double sel_clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

/* 1 inside [lo, hi], falling linearly to 0 over `soft` outside it; soft = 0 is a
 * hard edge. hi may be DBL_MAX for an open range. */
static double sel_ramp_in(double x, double lo, double hi, double soft) {
    if (x >= lo && x <= hi) return 1.0;
    if (!(soft > 0.0)) return 0.0;
    if (x < lo) return sel_clamp01(1.0 - (lo - x) / soft);
    return sel_clamp01(1.0 - (x - hi) / soft);
}

/* 1 for d <= tol, 0 for d >= tol + soft, linear between; soft = 0 is a hard edge. */
static double sel_ramp_below(double d, double tol, double soft) {
    if (d <= tol) return 1.0;
    if (!(soft > 0.0)) return 0.0;
    return sel_clamp01(1.0 - (d - tol) / soft);
}

typedef struct {
    alwan_tf_fn_f64 decode;
    alwan_mat3x3_f64 rgb_to_xyz;
    alwan_xyz_f64 white;          /* the space's white, Y = 1 */
} sel_space;

static alwan_status sel_space_setup(sel_space *out, alwan_rgb_space_desc_f64 const *space) {
    alwan_mat3x3_f64 inv;
    alwan_xyy_f64 xyy;
    if (!space) return ALWAN_E_INVALID;
    out->decode = alwan__resolve_eotf_f64(space->eotf);
    if (!out->decode) return ALWAN_E_INVALID;
    if (space->has_matrices) {
        out->rgb_to_xyz = space->rgb_to_xyz;
    } else {
        alwan_status const st = alwan_rgb_derive_matrices_f64(&out->rgb_to_xyz, &inv, space);
        if (st != ALWAN_OK) return st;
    }
    xyy.x = space->white_xy[0]; xyy.y = space->white_xy[1]; xyy.Y = 1.0;
    if (!(xyy.y > 0.0)) return ALWAN_E_INVALID;
    alwan_xyy_to_xyz_f64(&out->white, &xyy);
    return ALWAN_OK;
}

static alwan_xyz_f64 sel_to_xyz(sel_space const *s, double r, double g, double b) {
    double const lr = s->decode(r), lg = s->decode(g), lb = s->decode(b);
    double const *m = s->rgb_to_xyz.m;
    alwan_xyz_f64 x;
    x.x = m[0] * lr + m[1] * lg + m[2] * lb;
    x.y = m[3] * lr + m[4] * lg + m[5] * lb;
    x.z = m[6] * lr + m[7] * lg + m[8] * lb;
    return x;
}

static alwan_lab_f64 sel_to_lab(sel_space const *s, double r, double g, double b) {
    return alwan_xyz_to_lab_f64_v(sel_to_xyz(s, r, g, b), s->white);
}

static alwan_oklab_f64 sel_to_oklab(sel_space const *s, double r, double g, double b) {
    return alwan_xyz_to_oklab_f64_v(sel_to_xyz(s, r, g, b));
}

/* Stride check shared by every entry point: explicit byte strides, at least one
 * element wide, because a stride of 0 means three different things elsewhere in
 * the library and none of them is wanted here. */
static int sel_strides_ok(size_t mask_stride, size_t mask_elem, size_t rgb_stride, size_t rgb_elem) {
    return mask_stride >= mask_elem && rgb_stride >= 3 * rgb_elem;
}

void alwan_select_qualifier_params_init(alwan_select_qualifier_params *params) {
    if (!params) return;
    memset(params, 0, sizeof *params);
    params->model = ALWAN_SELECT_OKLCH;
    params->hue_width = 1.0;
    params->chroma_max = DBL_MAX;
    params->light_max = DBL_MAX;
}

#if ALWAN_WITH_F64_FACADE

/* ----------------------------------------------------------------
 * Qualifier
 * ---------------------------------------------------------------- */

static int sel_qualifier_ok(alwan_select_qualifier_params const *p) {
    if (!p) return 0;
    if ((int)p->model < (int)ALWAN_SELECT_HSV || (int)p->model > (int)ALWAN_SELECT_CIELCH) return 0;
    if (!sel_finite(p->hue_center) || !sel_finite(p->hue_width) || !(p->hue_width >= 0.0)) return 0;
    if (!sel_finite(p->hue_softness) || !(p->hue_softness >= 0.0)) return 0;
    if (!sel_finite(p->chroma_min) || !(p->chroma_min <= p->chroma_max)) return 0;
    if (!sel_finite(p->chroma_softness) || !(p->chroma_softness >= 0.0)) return 0;
    if (!sel_finite(p->light_min) || !(p->light_min <= p->light_max)) return 0;
    if (!sel_finite(p->light_softness) || !(p->light_softness >= 0.0)) return 0;
    return 1;
}

/* Circular distance between two hues in turns, in [0, 0.5]. */
static double sel_hue_distance(double h, double c) {
    double d = h - c;
    d -= (double)(long long)d;
    if (d < 0.0) d += 1.0;
    return d > 0.5 ? 1.0 - d : d;
}

/* An angle in radians as a fraction of a turn in [0, 1). */
static double sel_turns_from_radians(double h) {
    double t = h / 6.283185307179586476925286766559;
    return t < 0.0 ? t + 1.0 : t;
}

static double sel_qualify(sel_space const *s, alwan_select_qualifier_params const *p, double r, double g, double b) {
    double hue, chroma, light, w;
    if (p->model == ALWAN_SELECT_HSV) {
        alwan_rgb_f64 v;
        alwan_hsv_f64 hsv;
        v.r = r; v.g = g; v.b = b;
        hsv = alwan_rgb_to_hsv_f64_v(v);
        hue = hsv.h; chroma = hsv.s; light = hsv.v;
    } else if (p->model == ALWAN_SELECT_OKLCH) {
        alwan_oklch_f64 const lch = alwan_oklab_to_oklch_f64_v(sel_to_oklab(s, r, g, b));
        hue = sel_turns_from_radians(lch.h); chroma = lch.C; light = lch.L;
    } else {
        alwan_lch_f64 const lch = alwan_lab_to_lch_f64_v(sel_to_lab(s, r, g, b));
        hue = lch.h / 360.0; chroma = lch.C; light = lch.L * 0.01;
    }
    {
        double const wl = sel_ramp_in(light, p->light_min, p->light_max, p->light_softness);
        double const wc = sel_ramp_in(chroma, p->chroma_min, p->chroma_max, p->chroma_softness);
        double const wh = p->hue_width < 1.0
            ? sel_ramp_below(sel_hue_distance(hue, p->hue_center), 0.5 * p->hue_width, p->hue_softness) : 1.0;
        w = wl < wc ? wl : wc;
        w = w < wh ? w : wh;
    }
    return p->invert ? 1.0 - w : w;
}

alwan_status alwan_select_qualifier_f64(alwan_f64 *mask_out, size_t mask_stride, alwan_f64 const *rgb, size_t rgb_stride,
                                        size_t count, alwan_rgb_space_desc_f64 const *space,
                                        alwan_select_qualifier_params const *params) {
    sel_space s;
    alwan_status st;
    size_t i;
    if (!mask_out || !rgb || !sel_qualifier_ok(params)) return ALWAN_E_INVALID;
    if (!sel_strides_ok(mask_stride, sizeof(alwan_f64), rgb_stride, sizeof(alwan_f64))) return ALWAN_E_INVALID;
    st = sel_space_setup(&s, space);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        alwan_f64 const *p = (alwan_f64 const *)((char const *)rgb + i * rgb_stride);
        *(alwan_f64 *)((char *)mask_out + i * mask_stride) = sel_qualify(&s, params, p[0], p[1], p[2]);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Distance to a key colour
 * ---------------------------------------------------------------- */

static double sel_distance(sel_space const *s, alwan_select_metric metric, double r, double g, double b,
                           alwan_lab_f64 const *key_lab, alwan_oklab_f64 const *key_ok) {
    if (metric == ALWAN_SELECT_METRIC_OKLAB) {
        alwan_oklab_f64 const o = sel_to_oklab(s, r, g, b);
        double const dl = o.L - key_ok->L, da = o.a - key_ok->a, db = o.b - key_ok->b;
        return ALWAN_SQRT_F64(dl * dl + da * da + db * db);
    }
    {
        alwan_lab_f64 const lab = sel_to_lab(s, r, g, b);
        return metric == ALWAN_SELECT_METRIC_DE76 ? alwan_delta_e_76_f64_v(lab, *key_lab)
                                                  : alwan_delta_e_2000_f64_v(lab, *key_lab);
    }
}

alwan_status alwan_select_distance_f64(alwan_f64 *mask_out, size_t mask_stride, alwan_f64 const *rgb, size_t rgb_stride,
                                       size_t count, alwan_rgb_space_desc_f64 const *space, alwan_f64 const key_rgb[3],
                                       alwan_select_metric metric, alwan_f64 tolerance, alwan_f64 softness) {
    sel_space s;
    alwan_lab_f64 key_lab;
    alwan_oklab_f64 key_ok;
    alwan_status st;
    size_t i;
    if (!mask_out || !rgb || !key_rgb) return ALWAN_E_INVALID;
    if ((int)metric < (int)ALWAN_SELECT_METRIC_OKLAB || (int)metric > (int)ALWAN_SELECT_METRIC_DE2000) return ALWAN_E_INVALID;
    if (!(tolerance >= 0.0) || !sel_finite(tolerance) || !(softness >= 0.0) || !sel_finite(softness)) return ALWAN_E_INVALID;
    if (!sel_finite(key_rgb[0]) || !sel_finite(key_rgb[1]) || !sel_finite(key_rgb[2])) return ALWAN_E_INVALID;
    if (!sel_strides_ok(mask_stride, sizeof(alwan_f64), rgb_stride, sizeof(alwan_f64))) return ALWAN_E_INVALID;
    st = sel_space_setup(&s, space);
    if (st != ALWAN_OK) return st;
    key_lab = sel_to_lab(&s, key_rgb[0], key_rgb[1], key_rgb[2]);
    key_ok = sel_to_oklab(&s, key_rgb[0], key_rgb[1], key_rgb[2]);
    for (i = 0; i < count; i++) {
        alwan_f64 const *p = (alwan_f64 const *)((char const *)rgb + i * rgb_stride);
        double const d = sel_distance(&s, metric, p[0], p[1], p[2], &key_lab, &key_ok);
        *(alwan_f64 *)((char *)mask_out + i * mask_stride) = sel_ramp_below(d, tolerance, softness);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Select by example: a Gaussian in Oklab
 * ---------------------------------------------------------------- */

/* Inverse of a symmetric 3x3 by cofactors; 0 when the determinant is not
 * comfortably away from zero relative to the matrix's own scale. */
static int sel_inv3(double const a[9], double out[9]) {
    double const c00 = a[4] * a[8] - a[5] * a[7];
    double const c01 = a[5] * a[6] - a[3] * a[8];
    double const c02 = a[3] * a[7] - a[4] * a[6];
    double const det = a[0] * c00 + a[1] * c01 + a[2] * c02;
    double const scale = (a[0] > 0.0 ? a[0] : -a[0]) + (a[4] > 0.0 ? a[4] : -a[4]) + (a[8] > 0.0 ? a[8] : -a[8]);
    if (!(scale > 0.0) || !((det > 0.0 ? det : -det) > 1e-12 * scale * scale * scale)) return 0;
    out[0] = c00 / det;
    out[1] = (a[2] * a[7] - a[1] * a[8]) / det;
    out[2] = (a[1] * a[5] - a[2] * a[4]) / det;
    out[3] = c01 / det;
    out[4] = (a[0] * a[8] - a[2] * a[6]) / det;
    out[5] = (a[2] * a[3] - a[0] * a[5]) / det;
    out[6] = c02 / det;
    out[7] = (a[1] * a[6] - a[0] * a[7]) / det;
    out[8] = (a[0] * a[4] - a[1] * a[3]) / det;
    return 1;
}

alwan_status alwan_select_example_fit_f64(alwan_select_example *model_out, alwan_f64 const *samples, size_t sample_stride,
                                          size_t count, alwan_rgb_space_desc_f64 const *space, alwan_f64 ridge) {
    sel_space s;
    double mean[3] = { 0.0, 0.0, 0.0 }, cov[9] = { 0.0 };
    alwan_status st;
    size_t i;
    int r, c;
    if (!model_out || !samples || count < 2 || sample_stride < 3 * sizeof(alwan_f64)) return ALWAN_E_INVALID;
    if (!(ridge >= 0.0) || !sel_finite(ridge)) return ALWAN_E_INVALID;
    st = sel_space_setup(&s, space);
    if (st != ALWAN_OK) return st;
    /* two passes: the mean, then the scatter about it (numerically kinder than
     * accumulating the raw second moment) */
    for (i = 0; i < count; i++) {
        alwan_f64 const *p = (alwan_f64 const *)((char const *)samples + i * sample_stride);
        alwan_oklab_f64 const o = sel_to_oklab(&s, p[0], p[1], p[2]);
        if (!sel_finite(o.L) || !sel_finite(o.a) || !sel_finite(o.b)) return ALWAN_E_INVALID;
        mean[0] += o.L; mean[1] += o.a; mean[2] += o.b;
    }
    for (c = 0; c < 3; c++) mean[c] /= (double)count;
    for (i = 0; i < count; i++) {
        alwan_f64 const *p = (alwan_f64 const *)((char const *)samples + i * sample_stride);
        alwan_oklab_f64 const o = sel_to_oklab(&s, p[0], p[1], p[2]);
        double const d[3] = { o.L - mean[0], o.a - mean[1], o.b - mean[2] };
        for (r = 0; r < 3; r++) for (c = 0; c < 3; c++) cov[3 * r + c] += d[r] * d[c];
    }
    for (r = 0; r < 9; r++) cov[r] /= (double)(count - 1);
    for (r = 0; r < 3; r++) cov[4 * r] += ridge;
    if (!sel_inv3(cov, model_out->inv_covariance)) return ALWAN_E_DIVZERO;
    for (c = 0; c < 3; c++) model_out->mean[c] = mean[c];
    for (r = 0; r < 9; r++) model_out->covariance[r] = cov[r];
    return ALWAN_OK;
}

alwan_status alwan_select_example_f64(alwan_f64 *mask_out, size_t mask_stride, alwan_f64 const *rgb, size_t rgb_stride,
                                      size_t count, alwan_rgb_space_desc_f64 const *space,
                                      alwan_select_example const *model, alwan_f64 tolerance, alwan_f64 softness) {
    sel_space s;
    alwan_status st;
    size_t i;
    if (!mask_out || !rgb || !model) return ALWAN_E_INVALID;
    if (!(tolerance >= 0.0) || !sel_finite(tolerance) || !(softness >= 0.0) || !sel_finite(softness)) return ALWAN_E_INVALID;
    if (!sel_strides_ok(mask_stride, sizeof(alwan_f64), rgb_stride, sizeof(alwan_f64))) return ALWAN_E_INVALID;
    st = sel_space_setup(&s, space);
    if (st != ALWAN_OK) return st;
    for (i = 0; i < count; i++) {
        alwan_f64 const *p = (alwan_f64 const *)((char const *)rgb + i * rgb_stride);
        alwan_oklab_f64 const o = sel_to_oklab(&s, p[0], p[1], p[2]);
        double const d[3] = { o.L - model->mean[0], o.a - model->mean[1], o.b - model->mean[2] };
        double const *m = model->inv_covariance;
        double q = 0.0;
        int r;
        for (r = 0; r < 3; r++) q += d[r] * (m[3 * r] * d[0] + m[3 * r + 1] * d[1] + m[3 * r + 2] * d[2]);
        q = q > 0.0 ? ALWAN_SQRT_F64(q) : 0.0;
        *(alwan_f64 *)((char *)mask_out + i * mask_stride) = sel_ramp_below(q, tolerance, softness);
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Chroma key: difference matte and despill
 * ---------------------------------------------------------------- */

alwan_status alwan_key_chroma_f64(alwan_f64 *alpha_out, size_t alpha_stride, alwan_f64 *fg_out, size_t fg_stride,
                                  alwan_f64 const *rgb, size_t rgb_stride, size_t count, alwan_key_screen screen,
                                  alwan_f64 balance, alwan_f64 gain, int despill) {
    int si, o1, o2;
    size_t i;
    if (!alpha_out || !rgb) return ALWAN_E_INVALID;
    if (screen != ALWAN_KEY_GREEN && screen != ALWAN_KEY_BLUE) return ALWAN_E_INVALID;
    if (!(balance >= 0.0 && balance <= 1.0) || !(gain >= 0.0) || !sel_finite(gain)) return ALWAN_E_INVALID;
    if (!sel_strides_ok(alpha_stride, sizeof(alwan_f64), rgb_stride, sizeof(alwan_f64))) return ALWAN_E_INVALID;
    if (fg_out && fg_stride < 3 * sizeof(alwan_f64)) return ALWAN_E_INVALID;
    si = screen == ALWAN_KEY_GREEN ? 1 : 2;
    o1 = 0;                                  /* red */
    o2 = screen == ALWAN_KEY_GREEN ? 2 : 1;  /* the other non-screen channel */
    for (i = 0; i < count; i++) {
        alwan_f64 const *p = (alwan_f64 const *)((char const *)rgb + i * rgb_stride);
        double const ref = balance * p[o1] + (1.0 - balance) * p[o2];
        double const key = p[si] - ref;
        *(alwan_f64 *)((char *)alpha_out + i * alpha_stride) = sel_clamp01(1.0 - gain * key);
        if (fg_out) {
            alwan_f64 *q = (alwan_f64 *)((char *)fg_out + i * fg_stride);
            double const s0 = p[0], s1 = p[1], s2 = p[2];
            q[0] = s0; q[1] = s1; q[2] = s2;
            if (despill && p[si] > ref) q[si] = ref;
        }
    }
    return ALWAN_OK;
}

#endif /* ALWAN_WITH_F64_FACADE */

#if ALWAN_WITH_F32

/* ----------------------------------------------------------------
 * f32 entry points: widen, run the f64 worker per tile, narrow
 * ---------------------------------------------------------------- */

/* The f32 entry points share one shape: widen a tile of pixels, run the f64
 * worker on it, narrow the mask. A fixed tile keeps the stack bounded. */
#define SEL_TILE 256

static void sel_widen_desc(alwan_rgb_space_desc_f64 *out, alwan_rgb_space_desc_f32 const *in) {
    int i;
    memset(out, 0, sizeof *out);
    for (i = 0; i < 6; i++) out->primaries_xy[i] = (alwan_f64)in->primaries_xy[i];
    out->white_xy[0] = (alwan_f64)in->white_xy[0];
    out->white_xy[1] = (alwan_f64)in->white_xy[1];
    out->oetf = in->oetf;
    out->eotf = in->eotf;
    out->has_matrices = 0;   /* derived in double from the primaries */
}

static void sel_widen_tile(alwan_f64 *dst, alwan_f32 const *rgb, size_t rgb_stride, size_t first, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        alwan_f32 const *p = (alwan_f32 const *)((char const *)rgb + (first + i) * rgb_stride);
        dst[3 * i] = p[0]; dst[3 * i + 1] = p[1]; dst[3 * i + 2] = p[2];
    }
}

static void sel_narrow_mask(alwan_f32 *mask, size_t mask_stride, alwan_f64 const *src, size_t first, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        *(alwan_f32 *)((char *)mask + (first + i) * mask_stride) = (alwan_f32)src[i];
    }
}


alwan_status alwan_select_qualifier_f32(alwan_f32 *mask_out, size_t mask_stride, alwan_f32 const *rgb, size_t rgb_stride,
                                        size_t count, alwan_rgb_space_desc_f32 const *space,
                                        alwan_select_qualifier_params const *params) {
    alwan_rgb_space_desc_f64 d;
    alwan_f64 in[3 * SEL_TILE], out[SEL_TILE];
    size_t done = 0;
    if (!mask_out || !rgb || !space) return ALWAN_E_INVALID;
    if (!sel_strides_ok(mask_stride, sizeof(alwan_f32), rgb_stride, sizeof(alwan_f32))) return ALWAN_E_INVALID;
    sel_widen_desc(&d, space);
    if (count == 0) return alwan_select_qualifier_f64(out, sizeof(alwan_f64), in, 3 * sizeof(alwan_f64), 0, &d, params);
    while (done < count) {
        size_t const n = count - done < SEL_TILE ? count - done : SEL_TILE;
        alwan_status st;
        sel_widen_tile(in, rgb, rgb_stride, done, n);
        st = alwan_select_qualifier_f64(out, sizeof(alwan_f64), in, 3 * sizeof(alwan_f64), n, &d, params);
        if (st != ALWAN_OK) return st;
        sel_narrow_mask(mask_out, mask_stride, out, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_select_distance_f32(alwan_f32 *mask_out, size_t mask_stride, alwan_f32 const *rgb, size_t rgb_stride,
                                       size_t count, alwan_rgb_space_desc_f32 const *space, alwan_f32 const key_rgb[3],
                                       alwan_select_metric metric, alwan_f32 tolerance, alwan_f32 softness) {
    alwan_rgb_space_desc_f64 d;
    alwan_f64 in[3 * SEL_TILE], out[SEL_TILE], key[3];
    size_t done = 0;
    if (!mask_out || !rgb || !space || !key_rgb) return ALWAN_E_INVALID;
    if (!sel_strides_ok(mask_stride, sizeof(alwan_f32), rgb_stride, sizeof(alwan_f32))) return ALWAN_E_INVALID;
    sel_widen_desc(&d, space);
    key[0] = key_rgb[0]; key[1] = key_rgb[1]; key[2] = key_rgb[2];
    if (count == 0) return alwan_select_distance_f64(out, sizeof(alwan_f64), in, 3 * sizeof(alwan_f64), 0, &d, key, metric, tolerance, softness);
    while (done < count) {
        size_t const n = count - done < SEL_TILE ? count - done : SEL_TILE;
        alwan_status st;
        sel_widen_tile(in, rgb, rgb_stride, done, n);
        st = alwan_select_distance_f64(out, sizeof(alwan_f64), in, 3 * sizeof(alwan_f64), n, &d, key, metric, tolerance, softness);
        if (st != ALWAN_OK) return st;
        sel_narrow_mask(mask_out, mask_stride, out, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_select_example_fit_f32(alwan_select_example *model_out, alwan_f32 const *samples, size_t sample_stride,
                                          size_t count, alwan_rgb_space_desc_f32 const *space, alwan_f64 ridge) {
    alwan_rgb_space_desc_f64 d;
    alwan_f64 *buf;
    alwan_status st;
    size_t bytes;
    if (!model_out || !samples || !space || count < 2 || sample_stride < 3 * sizeof(alwan_f32)) return ALWAN_E_INVALID;
    bytes = alwan_safe_array_size(count, 3 * sizeof(alwan_f64));
    if (bytes == 0) return ALWAN_E_NOMEM;
    buf = (alwan_f64 *)ALWAN_ALLOC(bytes, sizeof(alwan_f64));
    if (!buf) return ALWAN_E_NOMEM;
    sel_widen_desc(&d, space);
    sel_widen_tile(buf, samples, sample_stride, 0, count);
    st = alwan_select_example_fit_f64(model_out, buf, 3 * sizeof(alwan_f64), count, &d, ridge);
    ALWAN_FREE(buf);
    return st;
}

alwan_status alwan_select_example_f32(alwan_f32 *mask_out, size_t mask_stride, alwan_f32 const *rgb, size_t rgb_stride,
                                      size_t count, alwan_rgb_space_desc_f32 const *space,
                                      alwan_select_example const *model, alwan_f32 tolerance, alwan_f32 softness) {
    alwan_rgb_space_desc_f64 d;
    alwan_f64 in[3 * SEL_TILE], out[SEL_TILE];
    size_t done = 0;
    if (!mask_out || !rgb || !space) return ALWAN_E_INVALID;
    if (!sel_strides_ok(mask_stride, sizeof(alwan_f32), rgb_stride, sizeof(alwan_f32))) return ALWAN_E_INVALID;
    sel_widen_desc(&d, space);
    if (count == 0) return alwan_select_example_f64(out, sizeof(alwan_f64), in, 3 * sizeof(alwan_f64), 0, &d, model, tolerance, softness);
    while (done < count) {
        size_t const n = count - done < SEL_TILE ? count - done : SEL_TILE;
        alwan_status st;
        sel_widen_tile(in, rgb, rgb_stride, done, n);
        st = alwan_select_example_f64(out, sizeof(alwan_f64), in, 3 * sizeof(alwan_f64), n, &d, model, tolerance, softness);
        if (st != ALWAN_OK) return st;
        sel_narrow_mask(mask_out, mask_stride, out, done, n);
        done += n;
    }
    return ALWAN_OK;
}

alwan_status alwan_key_chroma_f32(alwan_f32 *alpha_out, size_t alpha_stride, alwan_f32 *fg_out, size_t fg_stride,
                                  alwan_f32 const *rgb, size_t rgb_stride, size_t count, alwan_key_screen screen,
                                  alwan_f32 balance, alwan_f32 gain, int despill) {
    int si, o1, o2;
    size_t i;
    if (!alpha_out || !rgb) return ALWAN_E_INVALID;
    if (screen != ALWAN_KEY_GREEN && screen != ALWAN_KEY_BLUE) return ALWAN_E_INVALID;
    if (!(balance >= 0.0f && balance <= 1.0f) || !(gain >= 0.0f) || !sel_finite(gain)) return ALWAN_E_INVALID;
    if (!sel_strides_ok(alpha_stride, sizeof(alwan_f32), rgb_stride, sizeof(alwan_f32))) return ALWAN_E_INVALID;
    if (fg_out && fg_stride < 3 * sizeof(alwan_f32)) return ALWAN_E_INVALID;
    si = screen == ALWAN_KEY_GREEN ? 1 : 2;
    o1 = 0;
    o2 = screen == ALWAN_KEY_GREEN ? 2 : 1;
    for (i = 0; i < count; i++) {
        alwan_f32 const *p = (alwan_f32 const *)((char const *)rgb + i * rgb_stride);
        alwan_f32 const ref = balance * p[o1] + (1.0f - balance) * p[o2];
        alwan_f32 const key = p[si] - ref;
        alwan_f32 a = 1.0f - gain * key;
        a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
        *(alwan_f32 *)((char *)alpha_out + i * alpha_stride) = a;
        if (fg_out) {
            alwan_f32 *q = (alwan_f32 *)((char *)fg_out + i * fg_stride);
            alwan_f32 const s0 = p[0], s1 = p[1], s2 = p[2];
            q[0] = s0; q[1] = s1; q[2] = s2;
            if (despill && p[si] > ref) q[si] = ref;
        }
    }
    return ALWAN_OK;
}

#endif /* ALWAN_WITH_F32 */
