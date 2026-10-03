/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Compositing and blending: the Porter-Duff operators (Porter and Duff, "Compositing
 * Digital Images", SIGGRAPH 1984) and the blend modes of W3C Compositing and Blending
 * Level 1 (Candidate Recommendation, 2015), written from those documents. A source pixel
 * is blended with the backdrop, Cs' = (1 - ab) Cs + ab B(Cb, Cs), then composited with the
 * operator's fractions Fa, Fb on premultiplied values: co = as Fa Cs' + ab Fb Cb,
 * ao = as Fa + ab Fb. Suite 301.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

/* ---- the separable modes: B(cb, cs), one channel ---- */

/* W3C soft-light's D(cb). */
static double cp_soft_d(double cb) {
    return cb <= 0.25 ? ((16.0 * cb - 12.0) * cb + 4.0) * cb : ALWAN_SQRT(cb);
}

static double cp_hard_light(double cb, double cs) {
    return cs <= 0.5 ? cb * (2.0 * cs) : (cb + (2.0 * cs - 1.0)) - cb * (2.0 * cs - 1.0);
}

static double cp_separable(alwan_blend_mode m, double cb, double cs) {
    switch (m) {
    case ALWAN_BLEND_NORMAL: return cs;
    case ALWAN_BLEND_MULTIPLY: return cb * cs;
    case ALWAN_BLEND_SCREEN: return cb + cs - cb * cs;
    case ALWAN_BLEND_OVERLAY: return cp_hard_light(cs, cb);
    case ALWAN_BLEND_DARKEN: return cb < cs ? cb : cs;
    case ALWAN_BLEND_LIGHTEN: return cb > cs ? cb : cs;
    case ALWAN_BLEND_COLOR_DODGE:
        if (cb == 0.0) return 0.0;
        if (cs >= 1.0) return 1.0;
        return cb / (1.0 - cs) < 1.0 ? cb / (1.0 - cs) : 1.0;
    case ALWAN_BLEND_COLOR_BURN:
        if (cb == 1.0) return 1.0;
        if (cs <= 0.0) return 0.0;
        return (1.0 - cb) / cs < 1.0 ? 1.0 - (1.0 - cb) / cs : 0.0;
    case ALWAN_BLEND_HARD_LIGHT: return cp_hard_light(cb, cs);
    case ALWAN_BLEND_SOFT_LIGHT:
        if (cs <= 0.5) return cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
        return cb + (2.0 * cs - 1.0) * (cp_soft_d(cb) - cb);
    case ALWAN_BLEND_DIFFERENCE: return ALWAN_ABS(cb - cs);
    case ALWAN_BLEND_EXCLUSION: return cb + cs - 2.0 * cb * cs;
    case ALWAN_BLEND_SOFT_LIGHT_PHOTOSHOP:
        if (cs < 0.5) return 2.0 * cb * cs + cb * cb * (1.0 - 2.0 * cs);
        return 2.0 * cb * (1.0 - cs) + ALWAN_SQRT(cb) * (2.0 * cs - 1.0);
    case ALWAN_BLEND_SOFT_LIGHT_PEGTOP: return (1.0 - 2.0 * cs) * cb * cb + 2.0 * cs * cb;
    case ALWAN_BLEND_SOFT_LIGHT_ILLUSIONS: return cb > 0.0 ? ALWAN_POW(cb, ALWAN_POW(2.0, 2.0 * (0.5 - cs))) : 0.0;
    default: return cs;
    }
}

/* ---- the non-separable modes (W3C section 10.3) ---- */

static double cp_lum(double const c[3]) { return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]; }

static void cp_clip_color(double c[3]) {
    double const l = cp_lum(c);
    double n = c[0], x = c[0];
    int i;
    for (i = 1; i < 3; i++) {
        if (c[i] < n) n = c[i];
        if (c[i] > x) x = c[i];
    }
    if (n < 0.0 && l - n > 0.0) {
        for (i = 0; i < 3; i++) c[i] = l + (c[i] - l) * l / (l - n);
    }
    if (x > 1.0 && x - l > 0.0) {
        for (i = 0; i < 3; i++) c[i] = l + (c[i] - l) * (1.0 - l) / (x - l);
    }
}

static void cp_set_lum(double c[3], double l) {
    double const d = l - cp_lum(c);
    c[0] += d;
    c[1] += d;
    c[2] += d;
    cp_clip_color(c);
}

static double cp_sat(double const c[3]) {
    double n = c[0], x = c[0];
    int i;
    for (i = 1; i < 3; i++) {
        if (c[i] < n) n = c[i];
        if (c[i] > x) x = c[i];
    }
    return x - n;
}

/* SetSat: the largest channel to s, the smallest to 0, the middle in proportion. Ties keep
 * the first index as max and the last as min, so each channel gets one role. */
static void cp_set_sat(double c[3], double s) {
    int imax = 0, imin = 0, imid, i;
    for (i = 1; i < 3; i++) {
        if (c[i] > c[imax]) imax = i;
        if (c[i] < c[imin]) imin = i;
    }
    if (imax == imin) {         /* all equal */
        c[0] = c[1] = c[2] = 0.0;
        return;
    }
    imid = 3 - imax - imin;
    if (c[imax] > c[imin]) {
        c[imid] = (c[imid] - c[imin]) * s / (c[imax] - c[imin]);
        c[imax] = s;
    } else {
        c[imid] = c[imax] = 0.0;
    }
    c[imin] = 0.0;
}

static void cp_blend(alwan_blend_mode m, double const cb[3], double const cs[3], double out[3]) {
    double t[3];
    int i;
    switch (m) {
    case ALWAN_BLEND_HUE:
        memcpy(t, cs, sizeof t);
        cp_set_sat(t, cp_sat(cb));
        cp_set_lum(t, cp_lum(cb));
        break;
    case ALWAN_BLEND_SATURATION:
        memcpy(t, cb, sizeof t);
        cp_set_sat(t, cp_sat(cs));
        cp_set_lum(t, cp_lum(cb));
        break;
    case ALWAN_BLEND_COLOR:
        memcpy(t, cs, sizeof t);
        cp_set_lum(t, cp_lum(cb));
        break;
    case ALWAN_BLEND_LUMINOSITY:
        memcpy(t, cb, sizeof t);
        cp_set_lum(t, cp_lum(cs));
        break;
    default:
        for (i = 0; i < 3; i++) t[i] = cp_separable(m, cb[i], cs[i]);
        break;
    }
    memcpy(out, t, sizeof t);
}

/* ---- Porter-Duff fractions ---- */

static void cp_fractions(alwan_composite_operator op, double as, double ab, double *fa, double *fb) {
    switch (op) {
    case ALWAN_COMPOSITE_CLEAR: *fa = 0.0; *fb = 0.0; break;
    case ALWAN_COMPOSITE_COPY: *fa = 1.0; *fb = 0.0; break;
    case ALWAN_COMPOSITE_DESTINATION: *fa = 0.0; *fb = 1.0; break;
    case ALWAN_COMPOSITE_DESTINATION_OVER: *fa = 1.0 - ab; *fb = 1.0; break;
    case ALWAN_COMPOSITE_SOURCE_IN: *fa = ab; *fb = 0.0; break;
    case ALWAN_COMPOSITE_DESTINATION_IN: *fa = 0.0; *fb = as; break;
    case ALWAN_COMPOSITE_SOURCE_OUT: *fa = 1.0 - ab; *fb = 0.0; break;
    case ALWAN_COMPOSITE_DESTINATION_OUT: *fa = 0.0; *fb = 1.0 - as; break;
    case ALWAN_COMPOSITE_SOURCE_ATOP: *fa = ab; *fb = 1.0 - as; break;
    case ALWAN_COMPOSITE_DESTINATION_ATOP: *fa = 1.0 - ab; *fb = as; break;
    case ALWAN_COMPOSITE_XOR: *fa = 1.0 - ab; *fb = 1.0 - as; break;
    case ALWAN_COMPOSITE_LIGHTER: *fa = 1.0; *fb = 1.0; break;
    default: *fa = 1.0; *fb = 1.0 - as; break;   /* SOURCE_OVER */
    }
}

/* ---- transfer function round trip, in whichever precision the build has ---- */

static void cp_tf(double v[3], alwan_transfer_function tf, int decode) {
#if ALWAN_WITH_F64
    alwan_f64 t[3];
    if (decode) alwan_eotf_apply_f64(t, sizeof(alwan_f64), v, sizeof(alwan_f64), 3, tf);
    else alwan_oetf_apply_f64(t, sizeof(alwan_f64), v, sizeof(alwan_f64), 3, tf);
    v[0] = t[0]; v[1] = t[1]; v[2] = t[2];
#else
    alwan_f32 a[3], t[3];
    a[0] = (alwan_f32)v[0]; a[1] = (alwan_f32)v[1]; a[2] = (alwan_f32)v[2];
    if (decode) alwan_eotf_apply_f32(t, sizeof(alwan_f32), a, sizeof(alwan_f32), 3, tf);
    else alwan_oetf_apply_f32(t, sizeof(alwan_f32), a, sizeof(alwan_f32), 3, tf);
    v[0] = t[0]; v[1] = t[1]; v[2] = t[2];
#endif
}

/* One pixel: s and d hold straight or premultiplied RGBA (alpha 1 when channels == 3),
 * o receives the same layout. */
static void cp_pixel(double o[4], double const s[4], double const d[4], alwan_composite_params const *p) {
    double cs[3], cb[3], b[3], csp[3], co[3], fa, fb, ao;
    double const as = s[3], ab = d[3];
    int i;
    for (i = 0; i < 3; i++) {
        cs[i] = p->premultiplied ? (as > 0.0 ? s[i] / as : 0.0) : s[i];
        cb[i] = p->premultiplied ? (ab > 0.0 ? d[i] / ab : 0.0) : d[i];
    }
    if (p->transfer != ALWAN_TF_LINEAR) {
        cp_tf(cs, p->transfer, 1);
        cp_tf(cb, p->transfer, 1);
    }
    cp_blend(p->blend, cb, cs, b);
    for (i = 0; i < 3; i++) csp[i] = (1.0 - ab) * cs[i] + ab * b[i];
    cp_fractions(p->op, as, ab, &fa, &fb);
    ao = as * fa + ab * fb;
    for (i = 0; i < 3; i++) co[i] = as * fa * csp[i] + ab * fb * cb[i];
    if (p->op == ALWAN_COMPOSITE_LIGHTER) {   /* plus-lighter: each clamped to 1 */
        if (ao > 1.0) ao = 1.0;
        for (i = 0; i < 3; i++) if (co[i] > 1.0) co[i] = 1.0;
    }
    for (i = 0; i < 3; i++) co[i] = ao > 0.0 ? co[i] / ao : 0.0;   /* straight */
    if (p->transfer != ALWAN_TF_LINEAR) cp_tf(co, p->transfer, 0);
    for (i = 0; i < 3; i++) o[i] = p->premultiplied ? co[i] * ao : co[i];
    o[3] = ao;
}

/* ---- buffer driver over the four element types ---- */

static double cp_load(void const *row, alwan_pixel_format f, size_t i) {
    switch (f) {
    case ALWAN_PIXEL_U8: return ((uint8_t const *)row)[i] * (1.0 / 255.0);
    case ALWAN_PIXEL_U16: return ((uint16_t const *)row)[i] * (1.0 / 65535.0);
    case ALWAN_PIXEL_F32: return (double)((alwan_f32 const *)row)[i];
    default: return ((alwan_f64 const *)row)[i];
    }
}

static void cp_store(void *row, alwan_pixel_format f, size_t i, double v) {
    double q;
    switch (f) {
    case ALWAN_PIXEL_U8:
        q = ALWAN_FLOOR(v * 255.0 + 0.5);
        ((uint8_t *)row)[i] = (uint8_t)(q < 0.0 ? 0.0 : (q > 255.0 ? 255.0 : q));
        break;
    case ALWAN_PIXEL_U16:
        q = ALWAN_FLOOR(v * 65535.0 + 0.5);
        ((uint16_t *)row)[i] = (uint16_t)(q < 0.0 ? 0.0 : (q > 65535.0 ? 65535.0 : q));
        break;
    case ALWAN_PIXEL_F32: ((alwan_f32 *)row)[i] = (alwan_f32)v; break;
    default: ((alwan_f64 *)row)[i] = v; break;
    }
}

static size_t cp_size(alwan_pixel_format f) {
    return f == ALWAN_PIXEL_U8 ? 1u : f == ALWAN_PIXEL_U16 ? 2u : f == ALWAN_PIXEL_F32 ? 4u : 8u;
}

static alwan_status cp_run(void *out, size_t out_stride, void const *src, size_t src_stride, void const *dst,
                           size_t dst_stride, size_t channels, size_t width, size_t height, alwan_pixel_format f,
                           alwan_composite_params const *params) {
    alwan_composite_params p;
    size_t const row_bytes = width * channels * cp_size(f);
    size_t x, y, c;
    if (!out || !src || !dst || width == 0 || height == 0 || (channels != 3 && channels != 4)) return ALWAN_E_INVALID;
    if (out_stride < row_bytes || src_stride < row_bytes || dst_stride < row_bytes) return ALWAN_E_INVALID;
    if (params) p = *params;
    else memset(&p, 0, sizeof p);
    if ((unsigned)p.op > (unsigned)ALWAN_COMPOSITE_LIGHTER) return ALWAN_E_INVALID;
    if ((unsigned)p.blend > (unsigned)ALWAN_BLEND_SOFT_LIGHT_ILLUSIONS) return ALWAN_E_INVALID;
    for (y = 0; y < height; y++) {
        void const *sr = (char const *)src + y * src_stride;
        void const *dr = (char const *)dst + y * dst_stride;
        void *orow = (char *)out + y * out_stride;
        for (x = 0; x < width; x++) {
            double s[4], d[4], o[4];
            size_t const base = x * channels;
            for (c = 0; c < 3; c++) {
                s[c] = cp_load(sr, f, base + c);
                d[c] = cp_load(dr, f, base + c);
            }
            s[3] = channels == 4 ? cp_load(sr, f, base + 3) : 1.0;
            d[3] = channels == 4 ? cp_load(dr, f, base + 3) : 1.0;
            cp_pixel(o, s, d, &p);
            for (c = 0; c < channels; c++) cp_store(orow, f, base + c, o[c]);
        }
    }
    return ALWAN_OK;
}

#if ALWAN_WITH_F64
alwan_status alwan_composite_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride,
                                 alwan_f64 const *dst, size_t dst_row_stride, size_t channels, size_t width,
                                 size_t height, alwan_composite_params const *params) {
    return cp_run(out, out_row_stride, src, src_row_stride, dst, dst_row_stride, channels, width, height,
                  ALWAN_PIXEL_F64, params);
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_composite_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride,
                                 alwan_f32 const *dst, size_t dst_row_stride, size_t channels, size_t width,
                                 size_t height, alwan_composite_params const *params) {
    return cp_run(out, out_row_stride, src, src_row_stride, dst, dst_row_stride, channels, width, height,
                  ALWAN_PIXEL_F32, params);
}
#endif

alwan_status alwan_composite_u8(uint8_t *out, size_t out_row_stride, uint8_t const *src, size_t src_row_stride,
                                uint8_t const *dst, size_t dst_row_stride, size_t channels, size_t width,
                                size_t height, alwan_composite_params const *params) {
    return cp_run(out, out_row_stride, src, src_row_stride, dst, dst_row_stride, channels, width, height,
                  ALWAN_PIXEL_U8, params);
}

alwan_status alwan_composite_u16(uint16_t *out, size_t out_row_stride, uint16_t const *src, size_t src_row_stride,
                                 uint16_t const *dst, size_t dst_row_stride, size_t channels, size_t width,
                                 size_t height, alwan_composite_params const *params) {
    return cp_run(out, out_row_stride, src, src_row_stride, dst, dst_row_stride, channels, width, height,
                  ALWAN_PIXEL_U16, params);
}
