/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The DNG colour model: what a raw converter computes from a camera profile's
 * ColorMatrix, CameraCalibration, ForwardMatrix and AnalogBalance tags (DNG
 * specification 1.6, chapter 6). The tags are interpolated between the two
 * calibration illuminants linearly in inverse CCT, the CCT of the white read by
 * Robertson 1968.
 *
 * Reference: colour-hdri's colour_hdri.models.dng, which follows the white paper
 * where it and the DNG SDK differ. Two choices differ from colour-hdri, recorded
 * in docs/alwan_decisions.md: an all-zero matrix marks an absent tag (colour-hdri
 * uses the identity), and a single-illuminant profile uses its one set of tags
 * rather than interpolating it against identity, as the DNG SDK does.
 *
 * Also dcraw's highlight blend, as colour-hdri carries it.
 *
 * Everything computes in f64; the f32 entry points widen and narrow.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../data/alwan_data_tables_config.h"

/* The DNG profile connection space white, D50 as the specification writes it. */
#define ALWAN__DNG_PCS_x 0.3457
#define ALWAN__DNG_PCS_y 0.3585

typedef struct {
    alwan_f64 cct[2];
    alwan_mat3x3_f64 cm[2];
    alwan_mat3x3_f64 cc[2];
    alwan_mat3x3_f64 fm[2];
    alwan_f64 ab[3];
    int single;      /* one calibration illuminant */
    int has_forward; /* ForwardMatrix tags present */
} alwan__dng;

static int alwan__mat_is_zero(alwan_mat3x3_f64 const *m) {
    int i;
    for (i = 0; i < 9; i++) {
        if (m->m[i] != 0.0) return 0;
    }
    return 1;
}

static void alwan__mat_identity(alwan_mat3x3_f64 *m) {
    int i;
    for (i = 0; i < 9; i++) m->m[i] = (i % 4 == 0) ? 1.0 : 0.0;
}

static void alwan__xy_to_xyz(alwan_f64 out[3], alwan_f64 x, alwan_f64 y) {
    out[0] = x / y;
    out[1] = 1.0;
    out[2] = (1.0 - x - y) / y;
}

static void alwan__mulv(alwan_f64 out[3], alwan_mat3x3_f64 const *m, alwan_f64 const v[3]) {
    int r;
    for (r = 0; r < 3; r++) {
        out[r] = m->m[r * 3 + 0] * v[0] + m->m[r * 3 + 1] * v[1] + m->m[r * 3 + 2] * v[2];
    }
}

/* Read a profile into the working form: absent tags filled in, the two
 * calibration illuminants in ascending CCT. */
static alwan_status alwan__dng_load(alwan__dng *d, alwan_dng_profile_f64 const *p) {
    int fm1_zero, fm2_zero, i;
    if (!p || alwan__mat_is_zero(&p->color_matrix_1) || !(p->calibration_cct_1 > 0.0)) {
        return ALWAN_E_INVALID;
    }
    d->single = alwan__mat_is_zero(&p->color_matrix_2);
    if (!d->single && !(p->calibration_cct_2 > 0.0)) {
        return ALWAN_E_INVALID;
    }
    d->cct[0] = p->calibration_cct_1;
    d->cct[1] = d->single ? p->calibration_cct_1 : p->calibration_cct_2;
    d->cm[0] = p->color_matrix_1;
    d->cm[1] = d->single ? p->color_matrix_1 : p->color_matrix_2;

    if (alwan__mat_is_zero(&p->camera_calibration_1)) alwan__mat_identity(&d->cc[0]);
    else d->cc[0] = p->camera_calibration_1;
    if (d->single) d->cc[1] = d->cc[0];
    else if (alwan__mat_is_zero(&p->camera_calibration_2)) alwan__mat_identity(&d->cc[1]);
    else d->cc[1] = p->camera_calibration_2;

    fm1_zero = alwan__mat_is_zero(&p->forward_matrix_1);
    fm2_zero = alwan__mat_is_zero(&p->forward_matrix_2);
    d->has_forward = !(fm1_zero && (d->single || fm2_zero));
    if (d->has_forward) {
        d->fm[0] = fm1_zero ? p->forward_matrix_2 : p->forward_matrix_1;
        d->fm[1] = (d->single || fm2_zero) ? d->fm[0] : p->forward_matrix_2;
    }

    if (p->analog_balance.r == 0.0 && p->analog_balance.g == 0.0 && p->analog_balance.b == 0.0) {
        d->ab[0] = d->ab[1] = d->ab[2] = 1.0;
    } else {
        d->ab[0] = p->analog_balance.r;
        d->ab[1] = p->analog_balance.g;
        d->ab[2] = p->analog_balance.b;
        for (i = 0; i < 3; i++) {
            if (!(d->ab[i] > 0.0)) return ALWAN_E_INVALID;
        }
    }

    if (!d->single && d->cct[0] > d->cct[1]) {
        alwan_f64 t = d->cct[0];
        alwan_mat3x3_f64 m;
        d->cct[0] = d->cct[1]; d->cct[1] = t;
        m = d->cm[0]; d->cm[0] = d->cm[1]; d->cm[1] = m;
        m = d->cc[0]; d->cc[0] = d->cc[1]; d->cc[1] = m;
        m = d->fm[0]; d->fm[0] = d->fm[1]; d->fm[1] = m;
    }
    return ALWAN_OK;
}

/* colour-hdri's matrix_interpolated: linear in 1/CCT, the nearer tag outside the
 * two CCTs, written in its operation order. */
static void alwan__interp(alwan_mat3x3_f64 *out, alwan_f64 cct, alwan_f64 cct_1, alwan_f64 cct_2,
                          alwan_mat3x3_f64 const *m1, alwan_mat3x3_f64 const *m2) {
    int i;
    if (cct <= cct_1) { *out = *m1; return; }
    if (cct >= cct_2) { *out = *m2; return; }
    {
        alwan_f64 const a = 1e6 / cct;
        alwan_f64 const r1 = 1e6 / cct_1;
        alwan_f64 const r2 = 1e6 / cct_2;
        for (i = 0; i < 9; i++) {
            out->m[i] = ((a - r1) * (m2->m[i] - m1->m[i])) / (r2 - r1) + m1->m[i];
        }
    }
}

static void alwan__dng_interp(alwan_mat3x3_f64 *out, alwan__dng const *d, alwan_mat3x3_f64 const m[2],
                              alwan_f64 cct) {
    if (d->single) { *out = m[0]; return; }
    alwan__interp(out, cct, d->cct[0], d->cct[1], &m[0], &m[1]);
}

static alwan_status alwan__dng_cct(alwan_f64 *cct, alwan_vec2_f64 const *xy) {
    if (!xy || !(xy->v[1] > 0.0)) {
        return ALWAN_E_INVALID;
    }
#if !ALWAN_TABLE_ROBERTSON_LOCUS
    (void)cct;
    return ALWAN_E_NODATA;
#else
    *cct = alwan_cct_robertson_xy_f64(xy);
    return (*cct > 0.0) ? ALWAN_OK : ALWAN_E_RANGE;
#endif
}

/* AnalogBalance x CameraCalibration, interpolated. */
static void alwan__ab_cc(alwan_mat3x3_f64 *out, alwan__dng const *d, alwan_f64 cct) {
    alwan_mat3x3_f64 cc;
    int r, c;
    alwan__dng_interp(&cc, d, d->cc, cct);
    for (r = 0; r < 3; r++) {
        for (c = 0; c < 3; c++) out->m[r * 3 + c] = d->ab[r] * cc.m[r * 3 + c];
    }
}

static alwan_status alwan__xyz_to_camera(alwan_mat3x3_f64 *out, alwan__dng const *d, alwan_vec2_f64 const *xy) {
    alwan_mat3x3_f64 cm, ab_cc;
    alwan_f64 cct = 0.0;
    alwan_status const st = alwan__dng_cct(&cct, xy);
    if (st != ALWAN_OK) return st;
    alwan__dng_interp(&cm, d, d->cm, cct);
    alwan__ab_cc(&ab_cc, d, cct);
    alwan_mat3_mul_f64(out, &ab_cc, &cm);
    return ALWAN_OK;
}

static alwan_status alwan__neutral(alwan_f64 n[3], alwan__dng const *d, alwan_vec2_f64 const *xy) {
    alwan_mat3x3_f64 m;
    alwan_f64 w[3], g;
    alwan_status const st = alwan__xyz_to_camera(&m, d, xy);
    if (st != ALWAN_OK) return st;
    alwan__xy_to_xyz(w, xy->v[0], xy->v[1]);
    alwan__mulv(n, &m, w);
    g = n[1];
    if (!(g != 0.0)) return ALWAN_E_RANGE;
    n[0] /= g; n[1] /= g; n[2] /= g;
    return ALWAN_OK;
}

static alwan_status alwan__neutral_to_xy(alwan_vec2_f64 *out, alwan__dng const *d, alwan_f64 const n[3]) {
    alwan_vec2_f64 xy;
    int it;
    xy.v[0] = 1.0 / 3.0;
    xy.v[1] = 1.0 / 3.0;
    for (it = 0; it < 200; it++) {
        alwan_mat3x3_f64 m, inv;
        alwan_f64 xyz[3], s, dx, dy;
        alwan_vec2_f64 const prev = xy;
        alwan_status st = alwan__xyz_to_camera(&m, d, &xy);
        if (st != ALWAN_OK) return st;
        if (alwan_mat3_inv_f64(&inv, &m) != ALWAN_OK) return ALWAN_E_RANGE;
        alwan__mulv(xyz, &inv, n);
        s = xyz[0] + xyz[1] + xyz[2];
        if (!(s != 0.0)) return ALWAN_E_RANGE;
        xy.v[0] = xyz[0] / s;
        xy.v[1] = xyz[1] / s;
        if (!(xy.v[1] > 0.0)) return ALWAN_E_RANGE;
        dx = xy.v[0] - prev.v[0];
        dy = xy.v[1] - prev.v[1];
        if (dx < 0.0) dx = -dx;
        if (dy < 0.0) dy = -dy;
        if (dx <= 4.0 * 2.220446049250313e-16 && dy <= 4.0 * 2.220446049250313e-16) {
            *out = xy;
            return ALWAN_OK;
        }
    }
    return ALWAN_E_RANGE;
}

static alwan_status alwan__camera_to_xyz(alwan_mat3x3_f64 *out, alwan__dng const *d, alwan_vec2_f64 const *xy,
                                         alwan_cat_method cat) {
    alwan_status st;
    if (!d->has_forward) {
        /* The inverse of XYZ to camera, then adapted from the white to D50. */
        alwan_mat3x3_f64 m, inv, adapt;
        alwan_xyz_f64 src, dst;
        alwan_f64 w[3];
        st = alwan__xyz_to_camera(&m, d, xy);
        if (st != ALWAN_OK) return st;
        if (alwan_mat3_inv_f64(&inv, &m) != ALWAN_OK) return ALWAN_E_RANGE;
        alwan__xy_to_xyz(w, xy->v[0], xy->v[1]);
        src.x = w[0]; src.y = w[1]; src.z = w[2];
        alwan__xy_to_xyz(w, ALWAN__DNG_PCS_x, ALWAN__DNG_PCS_y);
        dst.x = w[0]; dst.y = w[1]; dst.z = w[2];
        st = alwan_cat_matrix_f64(&adapt, &src, &dst, cat);
        if (st != ALWAN_OK) return st;
        alwan_mat3_mul_f64(out, &adapt, &inv);
        return ALWAN_OK;
    } else {
        /* ForwardMatrix x D x (AB CC)^-1, D mapping the white's reference neutral to 1. */
        alwan_mat3x3_f64 ab_cc, ab_cc_inv, fm, fm_d;
        alwan_f64 n[3], ref[3], cct = 0.0;
        int r, c;
        st = alwan__dng_cct(&cct, xy);
        if (st != ALWAN_OK) return st;
        st = alwan__neutral(n, d, xy);
        if (st != ALWAN_OK) return st;
        alwan__ab_cc(&ab_cc, d, cct);
        if (alwan_mat3_inv_f64(&ab_cc_inv, &ab_cc) != ALWAN_OK) return ALWAN_E_RANGE;
        alwan__mulv(ref, &ab_cc_inv, n);
        for (c = 0; c < 3; c++) {
            if (!(ref[c] != 0.0)) return ALWAN_E_RANGE;
        }
        alwan__dng_interp(&fm, d, d->fm, cct);
        for (r = 0; r < 3; r++) {
            for (c = 0; c < 3; c++) fm_d.m[r * 3 + c] = fm.m[r * 3 + c] * (1.0 / ref[c]);
        }
        alwan_mat3_mul_f64(out, &fm_d, &ab_cc_inv);
        return ALWAN_OK;
    }
}

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

/* ================================================================
 * Public entry points
 * ================================================================ */

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_dng_interpolate_matrix_f64(alwan_mat3x3_f64 *matrix_out, alwan_f64 cct, alwan_f64 cct_1,
                                              alwan_f64 cct_2, alwan_mat3x3_f64 const *m1,
                                              alwan_mat3x3_f64 const *m2) {
    if (!matrix_out || !m1 || !m2 || !(cct > 0.0) || !(cct_1 > 0.0) || !(cct_2 > 0.0)) {
        return ALWAN_E_INVALID;
    }
    if (cct_1 <= cct_2) alwan__interp(matrix_out, cct, cct_1, cct_2, m1, m2);
    else alwan__interp(matrix_out, cct, cct_2, cct_1, m2, m1);
    return ALWAN_OK;
}

alwan_status alwan_dng_xyz_to_camera_matrix_f64(alwan_mat3x3_f64 *matrix_out, alwan_dng_profile_f64 const *profile,
                                                alwan_vec2_f64 const *white_xy) {
    alwan__dng d;
    alwan_status st;
    if (!matrix_out) return ALWAN_E_INVALID;
    st = alwan__dng_load(&d, profile);
    return st != ALWAN_OK ? st : alwan__xyz_to_camera(matrix_out, &d, white_xy);
}

alwan_status alwan_dng_xy_to_camera_neutral_f64(alwan_rgb_f64 *neutral_out, alwan_dng_profile_f64 const *profile,
                                                alwan_vec2_f64 const *white_xy) {
    alwan__dng d;
    alwan_f64 n[3];
    alwan_status st;
    if (!neutral_out) return ALWAN_E_INVALID;
    st = alwan__dng_load(&d, profile);
    if (st == ALWAN_OK) st = alwan__neutral(n, &d, white_xy);
    if (st == ALWAN_OK) { neutral_out->r = n[0]; neutral_out->g = n[1]; neutral_out->b = n[2]; }
    return st;
}

alwan_status alwan_dng_camera_neutral_to_xy_f64(alwan_vec2_f64 *white_xy_out, alwan_dng_profile_f64 const *profile,
                                                alwan_rgb_f64 const *camera_neutral) {
    alwan__dng d;
    alwan_f64 n[3];
    alwan_status st;
    if (!white_xy_out || !camera_neutral) return ALWAN_E_INVALID;
    st = alwan__dng_load(&d, profile);
    if (st != ALWAN_OK) return st;
    n[0] = camera_neutral->r; n[1] = camera_neutral->g; n[2] = camera_neutral->b;
    return alwan__neutral_to_xy(white_xy_out, &d, n);
}

alwan_status alwan_dng_camera_to_xyz_matrix_f64(alwan_mat3x3_f64 *matrix_out, alwan_dng_profile_f64 const *profile,
                                                alwan_vec2_f64 const *white_xy, alwan_cat_method cat) {
    alwan__dng d;
    alwan_status st;
    if (!matrix_out) return ALWAN_E_INVALID;
    st = alwan__dng_load(&d, profile);
    return st != ALWAN_OK ? st : alwan__camera_to_xyz(matrix_out, &d, white_xy, cat);
}
#endif /* ALWAN_WITH_F64_FACADE */

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
static void alwan__profile_widen(alwan_dng_profile_f64 *o, alwan_dng_profile_f32 const *p) {
    int i;
    o->calibration_cct_1 = (alwan_f64)p->calibration_cct_1;
    o->calibration_cct_2 = (alwan_f64)p->calibration_cct_2;
    for (i = 0; i < 9; i++) {
        o->color_matrix_1.m[i] = (alwan_f64)p->color_matrix_1.m[i];
        o->color_matrix_2.m[i] = (alwan_f64)p->color_matrix_2.m[i];
        o->camera_calibration_1.m[i] = (alwan_f64)p->camera_calibration_1.m[i];
        o->camera_calibration_2.m[i] = (alwan_f64)p->camera_calibration_2.m[i];
        o->forward_matrix_1.m[i] = (alwan_f64)p->forward_matrix_1.m[i];
        o->forward_matrix_2.m[i] = (alwan_f64)p->forward_matrix_2.m[i];
    }
    o->analog_balance.r = (alwan_f64)p->analog_balance.r;
    o->analog_balance.g = (alwan_f64)p->analog_balance.g;
    o->analog_balance.b = (alwan_f64)p->analog_balance.b;
}

static void alwan__mat_narrow(alwan_mat3x3_f32 *o, alwan_mat3x3_f64 const *m) {
    int i;
    for (i = 0; i < 9; i++) o->m[i] = (alwan_f32)m->m[i];
}

alwan_status alwan_dng_interpolate_matrix_f32(alwan_mat3x3_f32 *matrix_out, alwan_f32 cct, alwan_f32 cct_1,
                                              alwan_f32 cct_2, alwan_mat3x3_f32 const *m1,
                                              alwan_mat3x3_f32 const *m2) {
    alwan_mat3x3_f64 a, b, r;
    alwan_status st;
    int i;
    if (!matrix_out || !m1 || !m2) return ALWAN_E_INVALID;
    for (i = 0; i < 9; i++) { a.m[i] = (alwan_f64)m1->m[i]; b.m[i] = (alwan_f64)m2->m[i]; }
    st = alwan_dng_interpolate_matrix_f64(&r, (alwan_f64)cct, (alwan_f64)cct_1, (alwan_f64)cct_2, &a, &b);
    if (st == ALWAN_OK) alwan__mat_narrow(matrix_out, &r);
    return st;
}

alwan_status alwan_dng_xyz_to_camera_matrix_f32(alwan_mat3x3_f32 *matrix_out, alwan_dng_profile_f32 const *profile,
                                                alwan_vec2_f32 const *white_xy) {
    alwan_dng_profile_f64 p;
    alwan_vec2_f64 xy;
    alwan_mat3x3_f64 m;
    alwan_status st;
    if (!matrix_out || !profile || !white_xy) return ALWAN_E_INVALID;
    alwan__profile_widen(&p, profile);
    xy.v[0] = (alwan_f64)white_xy->v[0]; xy.v[1] = (alwan_f64)white_xy->v[1];
    st = alwan_dng_xyz_to_camera_matrix_f64(&m, &p, &xy);
    if (st == ALWAN_OK) alwan__mat_narrow(matrix_out, &m);
    return st;
}

alwan_status alwan_dng_xy_to_camera_neutral_f32(alwan_rgb_f32 *neutral_out, alwan_dng_profile_f32 const *profile,
                                                alwan_vec2_f32 const *white_xy) {
    alwan_dng_profile_f64 p;
    alwan_vec2_f64 xy;
    alwan_rgb_f64 n;
    alwan_status st;
    if (!neutral_out || !profile || !white_xy) return ALWAN_E_INVALID;
    alwan__profile_widen(&p, profile);
    xy.v[0] = (alwan_f64)white_xy->v[0]; xy.v[1] = (alwan_f64)white_xy->v[1];
    st = alwan_dng_xy_to_camera_neutral_f64(&n, &p, &xy);
    if (st == ALWAN_OK) {
        neutral_out->r = (alwan_f32)n.r; neutral_out->g = (alwan_f32)n.g; neutral_out->b = (alwan_f32)n.b;
    }
    return st;
}

alwan_status alwan_dng_camera_neutral_to_xy_f32(alwan_vec2_f32 *white_xy_out, alwan_dng_profile_f32 const *profile,
                                                alwan_rgb_f32 const *camera_neutral) {
    alwan_dng_profile_f64 p;
    alwan_rgb_f64 n;
    alwan_vec2_f64 xy;
    alwan_status st;
    if (!white_xy_out || !profile || !camera_neutral) return ALWAN_E_INVALID;
    alwan__profile_widen(&p, profile);
    n.r = (alwan_f64)camera_neutral->r; n.g = (alwan_f64)camera_neutral->g; n.b = (alwan_f64)camera_neutral->b;
    st = alwan_dng_camera_neutral_to_xy_f64(&xy, &p, &n);
    if (st == ALWAN_OK) {
        white_xy_out->v[0] = (alwan_f32)xy.v[0];
        white_xy_out->v[1] = (alwan_f32)xy.v[1];
    }
    return st;
}

alwan_status alwan_dng_camera_to_xyz_matrix_f32(alwan_mat3x3_f32 *matrix_out, alwan_dng_profile_f32 const *profile,
                                                alwan_vec2_f32 const *white_xy, alwan_cat_method cat) {
    alwan_dng_profile_f64 p;
    alwan_vec2_f64 xy;
    alwan_mat3x3_f64 m;
    alwan_status st;
    if (!matrix_out || !profile || !white_xy) return ALWAN_E_INVALID;
    alwan__profile_widen(&p, profile);
    xy.v[0] = (alwan_f64)white_xy->v[0]; xy.v[1] = (alwan_f64)white_xy->v[1];
    st = alwan_dng_camera_to_xyz_matrix_f64(&m, &p, &xy, cat);
    if (st == ALWAN_OK) alwan__mat_narrow(matrix_out, &m);
    return st;
}

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
