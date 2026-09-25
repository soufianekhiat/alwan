/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Thin films and multilayers: the colours of soap bubbles, oil on water, anti-reflection
 * coatings, oxidised metal and the scales of some insects are the interference of light
 * reflected between the faces of layers thinner than a few wavelengths.
 *
 * alwan_multilayer_tmm_{T}: the reflectance and transmittance of a stack of plane parallel
 * layers between an incident medium and a substrate, for s and p polarised light, by the
 * transfer-matrix method as Byrnes writes it ("Multilayer optical calculations", 2016,
 * arXiv:1603.02720, and his tmm package, MIT): the angle in every medium by Snell's law in
 * complex numbers, per wavelength, the forward-travelling branch chosen in the incident
 * medium and the substrate; Fresnel's coefficients at each interface; a phase
 * delta = 2 pi n cos(theta) d / lambda through each layer (its imaginary part held at 35,
 * as Byrnes holds it, so an opaque layer does not overflow); the product of the layer
 * matrices; r = M10 / M00, t = 1 / M00, R = |r|^2, and T = |t|^2 times
 * Re(n_s cos(theta_s)) / Re(n_0 cos(theta_0)) (the cosines conjugated for p).
 *
 * colour's multilayer_tmm computes the same for lossless media and at normal incidence; it
 * takes every angle from the real part of the index at the first wavelength, so for an
 * absorbing layer at an angle, for dispersion, and past the critical angle it differs
 * (NaN there). Suite 240 holds alwan to colour where colour is exact and to Byrnes's tmm
 * everywhere.
 *
 * alwan_water_refractive_index_{T}: the refractive index of water from the wavelength,
 * the temperature and the density, by Schiebener, Straub, Levelt Sengers and Gallagher's
 * molar refraction (J. Phys. Chem. Ref. Data 19(3), 1990), as colour computes it.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <math.h>
#include <string.h>

typedef struct {
    double re, im;
} alwan_cx;

static alwan_cx alwan_cx_make(double re, double im) {
    alwan_cx z;
    z.re = re, z.im = im;
    return z;
}
static alwan_cx alwan_cx_add(alwan_cx a, alwan_cx b) { return alwan_cx_make(a.re + b.re, a.im + b.im); }
static alwan_cx alwan_cx_sub(alwan_cx a, alwan_cx b) { return alwan_cx_make(a.re - b.re, a.im - b.im); }
static alwan_cx alwan_cx_mul(alwan_cx a, alwan_cx b) { return alwan_cx_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re); }
static alwan_cx alwan_cx_scale(alwan_cx a, double s) { return alwan_cx_make(a.re * s, a.im * s); }
static alwan_cx alwan_cx_div(alwan_cx a, alwan_cx b) {
    /* Smith's algorithm */
    if (ALWAN_ABS_F64(b.re) >= ALWAN_ABS_F64(b.im)) {
        double const r = b.im / b.re, d = b.re + b.im * r;
        return alwan_cx_make((a.re + a.im * r) / d, (a.im - a.re * r) / d);
    } else {
        double const r = b.re / b.im, d = b.re * r + b.im;
        return alwan_cx_make((a.re * r + a.im) / d, (a.im * r - a.re) / d);
    }
}
static double alwan_cx_abs2(alwan_cx a) { return a.re * a.re + a.im * a.im; }
static alwan_cx alwan_cx_conj(alwan_cx a) { return alwan_cx_make(a.re, -a.im); }
static alwan_cx alwan_cx_sqrt(alwan_cx z) {
    /* the principal root */
    double const m = ALWAN_HYPOT_F64(z.re, z.im);
    double re = ALWAN_SQRT_F64(0.5 * (m + z.re)), im = ALWAN_SQRT_F64(0.5 * (m - z.re));
    if (z.im < 0.0) im = -im;
    return alwan_cx_make(re, im);
}
static alwan_cx alwan_cx_exp(alwan_cx z) {
    double const e = ALWAN_EXP_F64(z.re);
    return alwan_cx_make(e * ALWAN_COS_F64(z.im), e * ALWAN_SIN_F64(z.im));
}

/* Byrnes's is_forward_angle, on n cos(theta): the wave decays forward, or travels forward. */
static int alwan_tf_forward(alwan_cx ncos) {
    return ALWAN_ABS_F64(ncos.im) > 100.0 * 2.220446049250313e-16 ? ncos.im > 0.0 : ncos.re > 0.0;
}

/* The angle's cosine in a medium of index n from the invariant n_0 sin(theta_0). */
static alwan_cx alwan_tf_cos(alwan_cx n, alwan_cx nsin0, int fix_forward) {
    alwan_cx const s = alwan_cx_div(nsin0, n);
    alwan_cx c = alwan_cx_sqrt(alwan_cx_sub(alwan_cx_make(1.0, 0.0), alwan_cx_mul(s, s)));
    if (fix_forward && !alwan_tf_forward(alwan_cx_mul(n, c))) c = alwan_cx_scale(c, -1.0);
    return c;
}

/* Fresnel's amplitudes from medium i to f (Byrnes's interface_r and interface_t). */
static void alwan_tf_interface(alwan_cx *r, alwan_cx *t, int p, alwan_cx ni, alwan_cx nf, alwan_cx ci, alwan_cx cf) {
    alwan_cx const a = alwan_cx_mul(ni, ci), b = alwan_cx_mul(nf, cf);
    alwan_cx const two_a = alwan_cx_scale(a, 2.0);
    if (!p) {
        alwan_cx const den = alwan_cx_add(a, b);
        *r = alwan_cx_div(alwan_cx_sub(a, b), den);
        *t = alwan_cx_div(two_a, den);
    } else {
        alwan_cx const c = alwan_cx_mul(nf, ci), d = alwan_cx_mul(ni, cf);
        alwan_cx const den = alwan_cx_add(c, d);
        *r = alwan_cx_div(alwan_cx_sub(c, d), den);
        *t = alwan_cx_div(two_a, den);
    }
}

static alwan_cx alwan_tf_index(alwan_multilayer const *st, size_t m, size_t w) {
    size_t const at = st->row_stride ? m * st->row_stride + w : m;
    return alwan_cx_make(st->n[at], st->k ? st->k[at] : 0.0);
}

alwan_status alwan_multilayer_tmm_f64(double *R, double *T, double const *wavelengths, size_t count, alwan_multilayer const *stack,
                                      double theta_degrees) {
    size_t w, m;
    double const th = theta_degrees * (3.14159265358979323846 / 180.0);
    if (!R || !T || !wavelengths || !stack || !stack->n || count == 0 || stack->media_count < 2 || stack->media_count > 4096) return ALWAN_E_INVALID;
    if (stack->media_count > 2 && !stack->thickness) return ALWAN_E_INVALID;
    if (!(theta_degrees >= 0.0 && theta_degrees < 90.0)) return ALWAN_E_RANGE;
    for (m = 0; m + 2 < stack->media_count; m++)
        if (!(stack->thickness[m] >= 0.0) || stack->thickness[m] - stack->thickness[m] != 0.0) return ALWAN_E_RANGE;
    for (w = 0; w < count; w++) {
        double const lambda = wavelengths[w];
        alwan_cx const n0 = alwan_tf_index(stack, 0, w);
        alwan_cx const nsin0 = alwan_cx_scale(n0, ALWAN_SIN_F64(th));
        alwan_cx c_prev, n_prev;
        int pol;
        if (!(lambda > 0.0) || lambda - lambda != 0.0) return ALWAN_E_RANGE;
        if (n0.im != 0.0 || !(n0.re > 0.0)) return ALWAN_E_RANGE;   /* the incident medium is lossless */
        for (m = 0; m < stack->media_count; m++) {
            alwan_cx const nm = alwan_tf_index(stack, m, w);
            if (nm.re - nm.re != 0.0 || nm.im - nm.im != 0.0) return ALWAN_E_INVALID;
        }
        for (pol = 0; pol < 2; pol++) {
            /* M = I_01 * prod over layers of diag(e^-i delta, e^i delta) I_(j, j+1), each I over t */
            alwan_cx M[2][2], r, t;
            n_prev = n0;
            c_prev = alwan_tf_cos(n0, nsin0, 1);
            {
                alwan_cx const n1 = alwan_tf_index(stack, 1, w);
                alwan_cx const c1 = alwan_tf_cos(n1, nsin0, stack->media_count == 2);
                alwan_cx const inv_t = alwan_cx_make(1.0, 0.0);
                alwan_tf_interface(&r, &t, pol, n0, n1, c_prev, c1);
                M[0][0] = alwan_cx_div(inv_t, t), M[0][1] = alwan_cx_div(r, t);
                M[1][0] = M[0][1], M[1][1] = M[0][0];
                n_prev = n1, c_prev = c1;
            }
            for (m = 1; m + 1 < stack->media_count; m++) {
                /* layer m, then the interface to medium m + 1 */
                alwan_cx const nn = alwan_tf_index(stack, m + 1, w);
                alwan_cx const cn = alwan_tf_cos(nn, nsin0, m + 2 == stack->media_count);
                alwan_cx delta = alwan_cx_scale(alwan_cx_mul(n_prev, c_prev), 2.0 * 3.14159265358979323846 * stack->thickness[m - 1] / lambda);
                alwan_cx em, ep, L[2][2], P[2][2];
                if (delta.im > 35.0) delta.im = 35.0;   /* Byrnes: an opaque layer, held finite */
                em = alwan_cx_exp(alwan_cx_make(delta.im, -delta.re));   /* e^(-i delta) */
                ep = alwan_cx_exp(alwan_cx_make(-delta.im, delta.re));   /* e^(i delta) */
                alwan_tf_interface(&r, &t, pol, n_prev, nn, c_prev, cn);
                L[0][0] = alwan_cx_div(em, t), L[0][1] = alwan_cx_div(alwan_cx_mul(em, r), t);
                L[1][0] = alwan_cx_div(alwan_cx_mul(ep, r), t), L[1][1] = alwan_cx_div(ep, t);
                P[0][0] = alwan_cx_add(alwan_cx_mul(M[0][0], L[0][0]), alwan_cx_mul(M[0][1], L[1][0]));
                P[0][1] = alwan_cx_add(alwan_cx_mul(M[0][0], L[0][1]), alwan_cx_mul(M[0][1], L[1][1]));
                P[1][0] = alwan_cx_add(alwan_cx_mul(M[1][0], L[0][0]), alwan_cx_mul(M[1][1], L[1][0]));
                P[1][1] = alwan_cx_add(alwan_cx_mul(M[1][0], L[0][1]), alwan_cx_mul(M[1][1], L[1][1]));
                memcpy(M, P, sizeof(M));
                n_prev = nn, c_prev = cn;
            }
            {
                alwan_cx const rr = alwan_cx_div(M[1][0], M[0][0]), tt = alwan_cx_div(alwan_cx_make(1.0, 0.0), M[0][0]);
                alwan_cx const c0 = alwan_tf_cos(n0, nsin0, 1);
                alwan_cx num, den;
                if (!pol) {
                    num = alwan_cx_mul(n_prev, c_prev), den = alwan_cx_mul(n0, c0);
                } else {
                    num = alwan_cx_mul(n_prev, alwan_cx_conj(c_prev)), den = alwan_cx_mul(n0, alwan_cx_conj(c0));
                }
                R[2 * w + pol] = alwan_cx_abs2(rr);
                T[2 * w + pol] = alwan_cx_abs2(tt) * num.re / den.re;
            }
        }
    }
    return ALWAN_OK;
}

alwan_status alwan_fresnel_f64(double *amplitudes, double n1, double k1, double n2, double k2, double theta_degrees) {
    alwan_cx const a = alwan_cx_make(n1, k1), b = alwan_cx_make(n2, k2);
    alwan_cx nsin0, ca, cb, r, t;
    int pol;
    if (!amplitudes) return ALWAN_E_INVALID;
    if (!(theta_degrees >= 0.0 && theta_degrees < 90.0) || k1 != 0.0 || !(n1 > 0.0) || n2 - n2 != 0.0 || k2 - k2 != 0.0) return ALWAN_E_RANGE;
    nsin0 = alwan_cx_scale(a, ALWAN_SIN_F64(theta_degrees * (3.14159265358979323846 / 180.0)));
    ca = alwan_tf_cos(a, nsin0, 1);
    cb = alwan_tf_cos(b, nsin0, 1);
    for (pol = 0; pol < 2; pol++) {
        alwan_tf_interface(&r, &t, pol, a, b, ca, cb);
        amplitudes[2 * pol] = r.re, amplitudes[2 * pol + 1] = r.im;
        amplitudes[4 + 2 * pol] = t.re, amplitudes[4 + 2 * pol + 1] = t.im;
    }
    return ALWAN_OK;
}

alwan_status alwan_water_refractive_index_f64(double *n_out, double wavelength_nm, double temperature_k, double density_kg_m3) {
    /* the molar refraction LL of Schiebener et al., its coefficients the paper's */
    double const wl = wavelength_nm / 589.0, Tn = temperature_k / 273.15, p = density_kg_m3 / 1000.0, wl2 = wl * wl;
    double const wl_uv = 0.2292020, wl_ir = 5.432937;
    double LL;
    if (!n_out) return ALWAN_E_INVALID;
    if (!(wavelength_nm > 0.0) || !(temperature_k > 0.0) || !(density_kg_m3 > 0.0) || wavelength_nm - wavelength_nm != 0.0) return ALWAN_E_RANGE;
    LL = 0.243905091 + 9.53518094e-3 * p + -3.64358110e-3 * Tn + 2.65666426e-4 * wl2 * Tn + 1.59189325e-3 / wl2 +
         (2.45733798e-3 / (wl2 - wl_uv * wl_uv)) + (0.897478251 / (wl2 - wl_ir * wl_ir)) + -1.63066183e-2 * p * p;
    if (!(1.0 / p - LL > 0.0)) return ALWAN_E_RANGE;
    *n_out = ALWAN_SQRT_F64((2.0 * LL + 1.0 / p) / (1.0 / p - LL));
    return ALWAN_OK;
}

#if ALWAN_WITH_F32
alwan_status alwan_multilayer_tmm_f32(float *R, float *T, float const *wavelengths, size_t count, alwan_multilayer const *stack,
                                      float theta_degrees) {
    double *buf;
    size_t i;
    alwan_status st;
    if (!R || !T || !wavelengths || count == 0) return ALWAN_E_INVALID;
    buf = (double *)ALWAN_ALLOC(alwan_safe_array_size(5 * count, sizeof(double)), sizeof(double));
    if (!buf) return ALWAN_E_NOMEM;
    for (i = 0; i < count; i++) buf[4 * count + i] = (double)wavelengths[i];
    st = alwan_multilayer_tmm_f64(buf, buf + 2 * count, buf + 4 * count, count, stack, (double)theta_degrees);
    if (st == ALWAN_OK)
        for (i = 0; i < 2 * count; i++) R[i] = (float)buf[i], T[i] = (float)buf[2 * count + i];
    ALWAN_FREE(buf);
    return st;
}

alwan_status alwan_fresnel_f32(float *amplitudes, float n1, float k1, float n2, float k2, float theta_degrees) {
    double a[8];
    int i;
    alwan_status st;
    if (!amplitudes) return ALWAN_E_INVALID;
    st = alwan_fresnel_f64(a, n1, k1, n2, k2, theta_degrees);
    if (st == ALWAN_OK)
        for (i = 0; i < 8; i++) amplitudes[i] = (float)a[i];
    return st;
}

alwan_status alwan_water_refractive_index_f32(float *n_out, float wavelength_nm, float temperature_k, float density_kg_m3) {
    double n;
    alwan_status st;
    if (!n_out) return ALWAN_E_INVALID;
    st = alwan_water_refractive_index_f64(&n, wavelength_nm, temperature_k, density_kg_m3);
    if (st == ALWAN_OK) *n_out = (float)n;
    return st;
}
#endif
