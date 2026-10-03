/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Highlight recovery of clipped camera RGB: dcraw's highlight blend, as colour-hdri carries
 * it, and colour-hdri's highlights_recovery_LCHab, ported from colour-hdri 0.2.6
 * (BSD-3-Clause, Copyright 2015 Colour Developers; redistributions keep this notice).
 * Split from alwan_dng.c (image processing, bound for suwar as a file).
 *
 * Everything computes in f64; the f32 entry points widen and narrow.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>

/* dcraw's opponent basis, with its rounded sqrt(3), as colour-hdri carries it. The
 * rows are orthogonal, so the inverse is the transpose over the squared row norms. */
#define ALWAN__BLEND_K 1.7320508

static void alwan__blend_px(alwan_f64 o[3], alwan_f64 const in[3], alwan_f64 clip) {
    alwan_f64 const k = ALWAN__BLEND_K;
    alwan_f64 c[3], lab[3], labc[3], s, sc, ratio;
    int i;
    for (i = 0; i < 3; i++) c[i] = in[i] < clip ? in[i] : clip;
    lab[0] = in[0] + in[1] + in[2];
    lab[1] = k * in[0] - k * in[1];
    lab[2] = -in[0] - in[1] + 2.0 * in[2];
    labc[0] = c[0] + c[1] + c[2];
    labc[1] = k * c[0] - k * c[1];
    labc[2] = -c[0] - c[1] + 2.0 * c[2];
    s = lab[1] * lab[1] + lab[2] * lab[2];
    sc = labc[1] * labc[1] + labc[2] * labc[2];
    ratio = ALWAN_SQRT(sc / s);
    if (!(ratio - ratio == 0.0)) {
        ratio = 1.0; /* NaN or infinite: an achromatic pixel keeps its chroma */
    }
    lab[1] *= ratio;
    lab[2] *= ratio;
    {
        alwan_f64 const n1 = 2.0 * k * k;
        o[0] = lab[0] / 3.0 + lab[1] * (k / n1) - lab[2] / 6.0;
        o[1] = lab[0] / 3.0 - lab[1] * (k / n1) - lab[2] / 6.0;
        o[2] = lab[0] / 3.0 + lab[2] * (2.0 / 6.0);
    }
}

static alwan_status alwan__blend_clip(alwan_f64 *clip, alwan_f64 const mult[3], alwan_f64 threshold) {
    alwan_f64 mn;
    if (!(mult[0] > 0.0) || !(mult[1] > 0.0) || !(mult[2] > 0.0) || !(threshold >= 0.0)) {
        return ALWAN_E_INVALID;
    }
    if (threshold == 0.0) threshold = 0.99;
    mn = mult[0];
    if (mult[1] < mn) mn = mult[1];
    if (mult[2] < mn) mn = mult[2];
    *clip = mn * threshold;
    return ALWAN_OK;
}

#if ALWAN_WITH_F64
alwan_status alwan_highlights_recovery_blend_f64_map_interleave(alwan_f64 *out, size_t out_stride,
                                                               alwan_f64 const *in, size_t in_stride, size_t count,
                                                               alwan_rgb_f64 const *multipliers, alwan_f64 threshold) {
    alwan_f64 mult[3], clip = 0.0;
    size_t p;
    alwan_status st;
    if (!out || !in || !multipliers) return ALWAN_E_INVALID;
    mult[0] = multipliers->r; mult[1] = multipliers->g; mult[2] = multipliers->b;
    st = alwan__blend_clip(&clip, mult, threshold);
    if (st != ALWAN_OK) return st;
    for (p = 0; p < count; p++) {
        alwan_f64 const *s = (alwan_f64 const *)((char const *)in + p * in_stride);
        alwan_f64 *o = (alwan_f64 *)((char *)out + p * out_stride);
        alwan_f64 v[3], r[3];
        v[0] = s[0]; v[1] = s[1]; v[2] = s[2];
        alwan__blend_px(r, v, clip);
        o[0] = r[0]; o[1] = r[1]; o[2] = r[2];
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
alwan_status alwan_highlights_recovery_blend_f32_map_interleave(alwan_f32 *out, size_t out_stride,
                                                               alwan_f32 const *in, size_t in_stride, size_t count,
                                                               alwan_rgb_f32 const *multipliers, alwan_f32 threshold) {
    alwan_f64 mult[3], clip = 0.0;
    size_t p;
    alwan_status st;
    if (!out || !in || !multipliers) return ALWAN_E_INVALID;
    mult[0] = (alwan_f64)multipliers->r; mult[1] = (alwan_f64)multipliers->g; mult[2] = (alwan_f64)multipliers->b;
    st = alwan__blend_clip(&clip, mult, (alwan_f64)threshold);
    if (st != ALWAN_OK) return st;
    for (p = 0; p < count; p++) {
        alwan_f32 const *s = (alwan_f32 const *)((char const *)in + p * in_stride);
        alwan_f32 *o = (alwan_f32 *)((char *)out + p * out_stride);
        alwan_f64 v[3], r[3];
        v[0] = (alwan_f64)s[0]; v[1] = (alwan_f64)s[1]; v[2] = (alwan_f64)s[2];
        alwan__blend_px(r, v, clip);
        o[0] = (alwan_f32)r[0]; o[1] = (alwan_f32)r[1]; o[2] = (alwan_f32)r[2];
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * Highlights recovery in CIE LCHab: colour-hdri's highlights_recovery_LCHab. Each pixel
 * keeps its own lightness and hue and takes the chroma of its version clipped to
 * [0, threshold]. RGB to XYZ by the space's matrix (linear RGB, no decoding), XYZ to Lab
 * against the space's white with Y = 1, LCHab, and back.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_f64 m[9], mi[9], wn[3], threshold;
    int clip_high;
} alwan__lch_render;

static alwan_status alwan__lch_prepare(alwan__lch_render *r, alwan_rgb_space_desc_f64 const *space,
                                       alwan_f64 threshold) {
    int i;
    if (!(threshold == threshold) || threshold < 0.0 || threshold > 1e300) return ALWAN_E_INVALID;
    r->threshold = threshold;
    r->clip_high = threshold > 0.0;
    if (!space) {
        /* colour-hdri's default, colour's sRGB: IEC 61966-2-1's four-decimal matrices
         * both ways (not the ones the primaries derive, 3.9e-5 away), white D65 */
        static alwan_f64 const m[9] = { 0.4124, 0.3576, 0.1805, 0.2126, 0.7152, 0.0722, 0.0193, 0.1192, 0.9505 };
        static alwan_f64 const mi[9] = { 3.2406, -1.5372, -0.4986, -0.9689, 1.8758, 0.0415, 0.0557, -0.2040, 1.0570 };
        for (i = 0; i < 9; i++) { r->m[i] = m[i]; r->mi[i] = mi[i]; }
        r->wn[0] = 0.3127 / 0.3290; r->wn[1] = 1.0; r->wn[2] = (1.0 - 0.3127 - 0.3290) / 0.3290;
        return ALWAN_OK;
    }
    if (!(space->white_xy[1] > 0.0)) return ALWAN_E_INVALID;
    if (space->has_matrices) {
        for (i = 0; i < 9; i++) { r->m[i] = space->rgb_to_xyz.m[i]; r->mi[i] = space->xyz_to_rgb.m[i]; }
    } else {
        alwan_mat3x3_f64 a, b;
        alwan_status st = alwan_rgb_derive_matrices_f64(&a, &b, space);
        if (st != ALWAN_OK) return st;
        for (i = 0; i < 9; i++) { r->m[i] = a.m[i]; r->mi[i] = b.m[i]; }
    }
    r->wn[0] = space->white_xy[0] / space->white_xy[1];
    r->wn[1] = 1.0;
    r->wn[2] = (1.0 - space->white_xy[0] - space->white_xy[1]) / space->white_xy[1];
    return ALWAN_OK;
}

/* CIE 1976 f and its inverse, as colour's intermediate lightness and luminance functions */
static alwan_f64 alwan__lab_f(alwan_f64 t) {
    return t > 216.0 / 24389.0 ? ALWAN_CBRT_F64(t) : (24389.0 / 27.0 * t + 16.0) / 116.0;
}

static alwan_f64 alwan__lab_finv(alwan_f64 f) {
    return f > 24.0 / 116.0 ? f * f * f : (f - 16.0 / 116.0) * (108.0 / 841.0);
}

static void alwan__rgb_to_lab(alwan_f64 lab[3], alwan_f64 const rgb[3], alwan__lch_render const *r) {
    alwan_f64 f[3];
    int k;
    for (k = 0; k < 3; k++)
        f[k] = alwan__lab_f((r->m[3 * k] * rgb[0] + r->m[3 * k + 1] * rgb[1] + r->m[3 * k + 2] * rgb[2]) / r->wn[k]);
    lab[0] = 116.0 * f[1] - 16.0;
    lab[1] = 500.0 * (f[0] - f[1]);
    lab[2] = 200.0 * (f[1] - f[2]);
}

static void alwan__lch_px(alwan_f64 o[3], alwan_f64 const in[3], alwan__lch_render const *r) {
    alwan_f64 lab[3], labc[3], clipped[3], chroma, hue, fx, fy, fz, xyz[3];
    int k;
    for (k = 0; k < 3; k++) {
        alwan_f64 v = in[k] > 0.0 ? in[k] : 0.0;   /* np.clip(RGB, 0, threshold) */
        clipped[k] = r->clip_high && v > r->threshold ? r->threshold : v;
    }
    alwan__rgb_to_lab(lab, in, r);
    alwan__rgb_to_lab(labc, clipped, r);
    chroma = ALWAN_HYPOT_F64(labc[1], labc[2]);
    hue = ALWAN_ATAN2_F64(lab[2], lab[1]);         /* atan2(0, 0) = 0, as colour reads a grey */
    fy = (lab[0] + 16.0) / 116.0;
    fx = fy + chroma * ALWAN_COS_F64(hue) / 500.0;
    fz = fy - chroma * ALWAN_SIN_F64(hue) / 200.0;
    xyz[0] = alwan__lab_finv(fx) * r->wn[0];
    xyz[1] = alwan__lab_finv(fy) * r->wn[1];
    xyz[2] = alwan__lab_finv(fz) * r->wn[2];
    for (k = 0; k < 3; k++)
        o[k] = r->mi[3 * k] * xyz[0] + r->mi[3 * k + 1] * xyz[1] + r->mi[3 * k + 2] * xyz[2];
}

#if ALWAN_WITH_F64
alwan_status alwan_highlights_recovery_lchab_f64_map_interleave(alwan_f64 *out, size_t out_stride,
                                                               alwan_f64 const *in, size_t in_stride, size_t count,
                                                               alwan_rgb_space_desc_f64 const *space,
                                                               alwan_f64 threshold) {
    alwan__lch_render r;
    alwan_status st;
    size_t p;
    if (!out || !in || out_stride < 3 * sizeof(alwan_f64) || in_stride < 3 * sizeof(alwan_f64)) return ALWAN_E_INVALID;
    st = alwan__lch_prepare(&r, space, threshold);
    if (st != ALWAN_OK) return st;
    for (p = 0; p < count; p++) {
        alwan_f64 const *s = (alwan_f64 const *)((char const *)in + p * in_stride);
        alwan_f64 *o = (alwan_f64 *)((char *)out + p * out_stride);
        alwan_f64 v[3], res[3];
        v[0] = s[0]; v[1] = s[1]; v[2] = s[2];
        alwan__lch_px(res, v, &r);
        o[0] = res[0]; o[1] = res[1]; o[2] = res[2];
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
alwan_status alwan_highlights_recovery_lchab_f32_map_interleave(alwan_f32 *out, size_t out_stride,
                                                               alwan_f32 const *in, size_t in_stride, size_t count,
                                                               alwan_rgb_space_desc_f32 const *space,
                                                               alwan_f32 threshold) {
    alwan__lch_render r;
    alwan_status st;
    size_t p;
    if (!out || !in || out_stride < 3 * sizeof(alwan_f32) || in_stride < 3 * sizeof(alwan_f32)) return ALWAN_E_INVALID;
    if (space) {
        alwan_rgb_space_desc_f64 d;
        int i;
        memset(&d, 0, sizeof d);
        for (i = 0; i < 6; i++) d.primaries_xy[i] = (alwan_f64)space->primaries_xy[i];
        d.white_xy[0] = (alwan_f64)space->white_xy[0];
        d.white_xy[1] = (alwan_f64)space->white_xy[1];
        d.oetf = space->oetf;
        d.eotf = space->eotf;
        d.has_matrices = space->has_matrices;
        for (i = 0; i < 9; i++) {
            d.rgb_to_xyz.m[i] = (alwan_f64)space->rgb_to_xyz.m[i];
            d.xyz_to_rgb.m[i] = (alwan_f64)space->xyz_to_rgb.m[i];
        }
        st = alwan__lch_prepare(&r, &d, (alwan_f64)threshold);
    } else {
        st = alwan__lch_prepare(&r, NULL, (alwan_f64)threshold);
    }
    if (st != ALWAN_OK) return st;
    for (p = 0; p < count; p++) {
        alwan_f32 const *s = (alwan_f32 const *)((char const *)in + p * in_stride);
        alwan_f32 *o = (alwan_f32 *)((char *)out + p * out_stride);
        alwan_f64 v[3], res[3];
        v[0] = (alwan_f64)s[0]; v[1] = (alwan_f64)s[1]; v[2] = (alwan_f64)s[2];
        alwan__lch_px(res, v, &r);
        o[0] = (alwan_f32)res[0]; o[1] = (alwan_f32)res[1]; o[2] = (alwan_f32)res[2];
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
