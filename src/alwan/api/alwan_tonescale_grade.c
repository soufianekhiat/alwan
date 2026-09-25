/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Tonescale-region grading: Canham, Punnappurath and Brown, "Adaptive Color
 * Grading", arXiv:2609.21169. Four overlapping regions of the tonescale, each
 * with a CIELAB a*b* offset, applied one after another on display-encoded RGB.
 *
 * Per region, on the running value v (encoded RGB in `space`):
 *   x   = mean(v)                                    the intensity
 *   w   = clip(1 + slope (x - pivot), 0, 1)          the membership
 *   adj = encode(XYZ->RGB(Lab->XYZ(XYZ->Lab(RGB->XYZ(decode(v))) + (0, a, b))))
 *   v   = v + w (adj - v)
 * with the Lab white the space's own. That is the reference's arithmetic read
 * from its lookup tables, except that the reference evaluates each region on a
 * 17^3 table and interpolates; this evaluates the pixel. The paper's default
 * pivots are 0.1, 0.4, 0.5 and 0.9 with slopes -10, -5, 5 and 10, and its
 * demonstration offsets (0, -10), (0, 10), (0, -10), (0, 10).
 *
 * The weight and the blend are the core (core/alwan_tonescale_grade_core.h);
 * the conversions around them need the descriptor's transfer functions, so the
 * whole pixel lives here. Evaluation runs in double on both paths.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_tonescale_grade_core.h"
#include <float.h>
#include <string.h>

#define TG_REGIONS 4

void alwan_tonescale_grade_params_init(alwan_tonescale_grade_params *params) {
    static alwan_f64 const pivot[TG_REGIONS] = { 0.1, 0.4, 0.5, 0.9 };
    static alwan_f64 const slope[TG_REGIONS] = { -10.0, -5.0, 5.0, 10.0 };
    int r;
    if (!params) return;
    for (r = 0; r < TG_REGIONS; r++) {
        params->pivot[r] = pivot[r];
        params->slope[r] = slope[r];
        params->offset_a[r] = 0.0;
        params->offset_b[r] = 0.0;
    }
}

static int tg_finite(double v) { return v == v && v <= DBL_MAX && v >= -DBL_MAX; }
static double tg_clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

static int tg_params_ok(alwan_tonescale_grade_params const *p) {
    int r;
    if (!p) return 0;
    for (r = 0; r < TG_REGIONS; r++) {
        if (!tg_finite(p->pivot[r]) || !tg_finite(p->slope[r]) || !tg_finite(p->offset_a[r]) ||
            !tg_finite(p->offset_b[r])) {
            return 0;
        }
    }
    return 1;
}

/* The pixel, in double, on an f64 descriptor. */
static alwan_status tg_pixel(alwan_rgb_f64 *out, alwan_rgb_f64 const *in, alwan_rgb_space_desc_f64 const *space,
                             alwan_tonescale_grade_params const *p, alwan_tf_fn_f64 decode, alwan_tf_fn_f64 encode,
                             alwan_xyz_f64 const *white) {
    alwan_rgb_f64 v = *in;
    int r;
    for (r = 0; r < TG_REGIONS; r++) {
        double const x = (v.r + v.g + v.b) / 3.0;
        double const w = alwan_tonescale_region_weight_f64_v(x, p->pivot[r], p->slope[r]);
        alwan_rgb_f64 lin, adj_lin, adj;
        alwan_xyz_f64 xyz;
        alwan_lab_f64 lab;
        alwan_status st;
        if (w <= 0.0) continue;
        lin.r = decode(v.r); lin.g = decode(v.g); lin.b = decode(v.b);
        st = alwan_rgb_to_xyz_f64(&xyz, space, &lin);
        if (st != ALWAN_OK) return st;
        alwan_xyz_to_lab_f64(&lab, &xyz, white);
        lab.a += p->offset_a[r];
        lab.b += p->offset_b[r];
        alwan_lab_to_xyz_f64(&xyz, &lab, white);
        st = alwan_xyz_to_rgb_f64(&adj_lin, space, &xyz);
        if (st != ALWAN_OK) return st;
        adj.r = encode(adj_lin.r); adj.g = encode(adj_lin.g); adj.b = encode(adj_lin.b);
        v.r = alwan_tonescale_region_blend_f64_v(v.r, adj.r, w);
        v.g = alwan_tonescale_region_blend_f64_v(v.g, adj.g, w);
        v.b = alwan_tonescale_region_blend_f64_v(v.b, adj.b, w);
        /* Held in [0, 1] between regions, not after the last: the value is
         * display-encoded and the reference's tables take their input on that
         * domain, so a region that pushes a channel past it hands the next
         * region the clamped value, while the final table's output is returned
         * as it is. Without this a light pixel's -10 then +10 on b* cancel here
         * where they do not in the reference, 0.03 on blue near white. */
        if (r + 1 < TG_REGIONS) {
            v.r = tg_clamp01(v.r);
            v.g = tg_clamp01(v.g);
            v.b = tg_clamp01(v.b);
        }
    }
    *out = v;
    return ALWAN_OK;
}

static alwan_status tg_setup(alwan_rgb_space_desc_f64 const *space, alwan_tonescale_grade_params const *p,
                             alwan_tf_fn_f64 *decode, alwan_tf_fn_f64 *encode, alwan_xyz_f64 *white) {
    alwan_xyy_f64 xyy;
    if (!space || !tg_params_ok(p)) return ALWAN_E_INVALID;
    *decode = alwan__resolve_eotf_f64(space->eotf);
    *encode = alwan__resolve_oetf_f64(space->oetf);
    if (!*decode || !*encode) return ALWAN_E_INVALID;
    xyy.x = space->white_xy[0]; xyy.y = space->white_xy[1]; xyy.Y = 1.0;
    if (!(xyy.y > 0.0)) return ALWAN_E_INVALID;
    alwan_xyy_to_xyz_f64(white, &xyy);
    return ALWAN_OK;
}

static void tg_widen_desc(alwan_rgb_space_desc_f64 *out, alwan_rgb_space_desc_f32 const *in) {
    int i;
    memset(out, 0, sizeof *out);
    for (i = 0; i < 6; i++) out->primaries_xy[i] = (alwan_f64)in->primaries_xy[i];
    out->white_xy[0] = (alwan_f64)in->white_xy[0];
    out->white_xy[1] = (alwan_f64)in->white_xy[1];
    out->oetf = in->oetf;
    out->eotf = in->eotf;
    out->has_matrices = 0;   /* derived in double from the primaries */
}

alwan_status alwan_tonescale_grade_weights_f64(alwan_f64 weights_out[4], alwan_f64 intensity,
                                               alwan_tonescale_grade_params const *params) {
    int r;
    if (!weights_out || !tg_params_ok(params) || !tg_finite(intensity)) return ALWAN_E_INVALID;
    for (r = 0; r < TG_REGIONS; r++) {
        weights_out[r] = alwan_tonescale_region_weight_f64_v(intensity, params->pivot[r], params->slope[r]);
    }
    return ALWAN_OK;
}

alwan_status alwan_tonescale_grade_weights_f32(alwan_f32 weights_out[4], alwan_f32 intensity,
                                               alwan_tonescale_grade_params const *params) {
    alwan_f64 w[TG_REGIONS];
    alwan_status st;
    int r;
    if (!weights_out) return ALWAN_E_INVALID;
    st = alwan_tonescale_grade_weights_f64(w, (alwan_f64)intensity, params);
    if (st != ALWAN_OK) return st;
    for (r = 0; r < TG_REGIONS; r++) weights_out[r] = (alwan_f32)w[r];
    return ALWAN_OK;
}

alwan_status alwan_tonescale_grade_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                       alwan_rgb_space_desc_f64 const *space,
                                       alwan_tonescale_grade_params const *params) {
    alwan_tf_fn_f64 decode, encode;
    alwan_xyz_f64 white;
    alwan_status st;
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    st = tg_setup(space, params, &decode, &encode, &white);
    if (st != ALWAN_OK) return st;
    return tg_pixel(rgb_out, rgb_in, space, params, decode, encode, &white);
}

alwan_status alwan_tonescale_grade_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                       alwan_rgb_space_desc_f32 const *space,
                                       alwan_tonescale_grade_params const *params) {
    alwan_rgb_space_desc_f64 d;
    alwan_rgb_f64 in64, out64;
    alwan_status st;
    if (!rgb_out || !rgb_in || !space) return ALWAN_E_INVALID;
    tg_widen_desc(&d, space);
    in64.r = rgb_in->r; in64.g = rgb_in->g; in64.b = rgb_in->b;
    st = alwan_tonescale_grade_f64(&out64, &in64, &d, params);
    if (st != ALWAN_OK) return st;
    rgb_out->r = (alwan_f32)out64.r; rgb_out->g = (alwan_f32)out64.g; rgb_out->b = (alwan_f32)out64.b;
    return ALWAN_OK;
}

alwan_status alwan_tonescale_grade_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in,
                                                      size_t in_stride, size_t count,
                                                      alwan_rgb_space_desc_f64 const *space,
                                                      alwan_tonescale_grade_params const *params) {
    alwan_tf_fn_f64 decode, encode;
    alwan_xyz_f64 white;
    alwan_status st;
    size_t i;
    if (!rgb_out || !rgb_in) return ALWAN_E_INVALID;
    st = tg_setup(space, params, &decode, &encode, &white);
    if (st != ALWAN_OK) return st;
    if (out_stride == 0) out_stride = 3 * sizeof(alwan_f64);
    if (in_stride == 0) in_stride = 3 * sizeof(alwan_f64);
    for (i = 0; i < count; i++) {
        alwan_f64 const *src = (alwan_f64 const *)((char const *)rgb_in + i * in_stride);
        alwan_f64 *dst = (alwan_f64 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f64 in, out;
        in.r = src[0]; in.g = src[1]; in.b = src[2];
        st = tg_pixel(&out, &in, space, params, decode, encode, &white);
        if (st != ALWAN_OK) return st;
        dst[0] = out.r; dst[1] = out.g; dst[2] = out.b;
    }
    return ALWAN_OK;
}

alwan_status alwan_tonescale_grade_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in,
                                                      size_t in_stride, size_t count,
                                                      alwan_rgb_space_desc_f32 const *space,
                                                      alwan_tonescale_grade_params const *params) {
    alwan_rgb_space_desc_f64 d;
    alwan_tf_fn_f64 decode, encode;
    alwan_xyz_f64 white;
    alwan_status st;
    size_t i;
    if (!rgb_out || !rgb_in || !space) return ALWAN_E_INVALID;
    tg_widen_desc(&d, space);
    st = tg_setup(&d, params, &decode, &encode, &white);
    if (st != ALWAN_OK) return st;
    if (out_stride == 0) out_stride = 3 * sizeof(alwan_f32);
    if (in_stride == 0) in_stride = 3 * sizeof(alwan_f32);
    for (i = 0; i < count; i++) {
        alwan_f32 const *src = (alwan_f32 const *)((char const *)rgb_in + i * in_stride);
        alwan_f32 *dst = (alwan_f32 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f64 in, out;
        in.r = src[0]; in.g = src[1]; in.b = src[2];
        st = tg_pixel(&out, &in, &d, params, decode, encode, &white);
        if (st != ALWAN_OK) return st;
        dst[0] = (alwan_f32)out.r; dst[1] = (alwan_f32)out.g; dst[2] = (alwan_f32)out.b;
    }
    return ALWAN_OK;
}

/* Planar twins: the scalar per pixel over three planes under one stride. */
#define TG_PLANAR(T, TN, DESC)                                                                          \
alwan_status alwan_tonescale_grade_##TN##_map_planar(T *out_ch0, size_t out_stride, T *out_ch1, T *out_ch2, \
        T const *in_ch0, size_t in_stride, T const *in_ch1, T const *in_ch2, size_t count,             \
        DESC const *space, alwan_tonescale_grade_params const *params) {                                \
    size_t i;                                                                                           \
    if (!out_ch0 || !out_ch1 || !out_ch2 || !in_ch0 || !in_ch1 || !in_ch2) return ALWAN_E_INVALID;      \
    if (out_stride == 0) out_stride = sizeof(T);                                                        \
    if (in_stride == 0) in_stride = sizeof(T);                                                          \
    for (i = 0; i < count; i++) {                                                                       \
        alwan_rgb_##TN in, out;                                                                         \
        alwan_status st;                                                                                \
        in.r = *(T const *)((char const *)in_ch0 + i * in_stride);                                      \
        in.g = *(T const *)((char const *)in_ch1 + i * in_stride);                                      \
        in.b = *(T const *)((char const *)in_ch2 + i * in_stride);                                      \
        st = alwan_tonescale_grade_##TN(&out, &in, space, params);                                      \
        if (st != ALWAN_OK) return st;                                                                  \
        *(T *)((char *)out_ch0 + i * out_stride) = out.r;                                               \
        *(T *)((char *)out_ch1 + i * out_stride) = out.g;                                               \
        *(T *)((char *)out_ch2 + i * out_stride) = out.b;                                               \
    }                                                                                                   \
    return ALWAN_OK;                                                                                    \
}

TG_PLANAR(alwan_f64, f64, alwan_rgb_space_desc_f64)
TG_PLANAR(alwan_f32, f32, alwan_rgb_space_desc_f32)
