/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Thin-film iridescence math (parameterized). No include guard: included once per
 * precision pass. The series reader is in alwan_iridescence_reader.inc.
 *
 * THE FILM. Light comes from an ambient of real index eta1, crosses a lossless film of
 * index eta2 and thickness d, and reflects off a base of index eta3 + i kappa3 (kappa3 = 0
 * for a dielectric), at the incidence cosine cos1 in the ambient. With s = eta1 sin(theta1)
 * the invariant of Snell's law, the normal components are q1 = eta1 cos(theta1),
 * q2 = sqrt(eta2^2 - s^2) and q3 = sqrt((eta3 + i kappa3)^2 - s^2), each the root with
 * Im >= 0 (Re >= 0 when real), the forward branch. q2 is imaginary past total internal
 * reflection at the film's top (eta1 > eta2 at a grazing angle): the wave in the film is
 * evanescent and a thin film still lets light tunnel to the base. Fresnel's amplitude
 * coefficients, in Byrnes's sign convention (alwan_fresnel, alwan_multilayer_tmm):
 *     r12_s = (q1 - q2) / (q1 + q2)        r12_p = (eta2^2 q1 - eta1^2 q2) / (eta2^2 q1 + eta1^2 q2)
 *     r23_s = (q2 - q3) / (q2 + q3)        r23_p = (n3^2 q2 - eta2^2 q3) / (n3^2 q2 + eta2^2 q3)
 * with n3^2 = (eta3 + i kappa3)^2. The film adds the round-trip factor e^(i delta),
 * delta = 4 pi d q2 / lambda (complex when q2 is), and the reflected amplitude is Airy's
 *     r = (r12 + r23 e^(i delta)) / (1 + r12 r23 e^(i delta)),   R = |r|^2.
 *
 * THE SERIES. Below total reflection r12 = a is real; with r23 = rho e^(i phi),
 *     R = C0 + sum_{m >= 1} c_m rho^m cos(m (delta + phi)),
 *     C0  = (a^2 + rho^2 - 2 a^2 rho^2) / (1 - a^2 rho^2),
 *     c_m = -2 K (-a)^m,    K = (1 - a^2)(1 - rho^2) / (1 - a^2 rho^2),
 * the Fourier expansion of R in delta (1 / (A + B cos x) expanded in powers of -a rho),
 * the form Belcour and Barla's equation 9 takes with their T121 and R23. Each term's colour
 * is rho^m cos(m (delta + phi)) integrated against a colour weight: Re(r23^m G(m OPD)),
 * OPD = 2 d q2, with G(f) the Fourier transform of the weight in wavenumber, which
 * alwan_iridescence_reader.inc reads from a table.
 */

/* Complex helpers: a value is (re, im) in a VEC2. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_irid_cmul)(ALWAN_CORE_VEC2 a, ALWAN_CORE_VEC2 b) {
    ALWAN_CORE_VEC2 r;
    r.v[0] = a.v[0] * b.v[0] - a.v[1] * b.v[1];
    r.v[1] = a.v[0] * b.v[1] + a.v[1] * b.v[0];
    return r;
}

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_irid_cdiv)(ALWAN_CORE_VEC2 a, ALWAN_CORE_VEC2 b) {
    ALWAN_CORE_VEC2 r;
    const ALWAN_CORE_T den = b.v[0] * b.v[0] + b.v[1] * b.v[1];
    r.v[0] = (a.v[0] * b.v[0] + a.v[1] * b.v[1]) / den;
    r.v[1] = (a.v[1] * b.v[0] - a.v[0] * b.v[1]) / den;
    return r;
}

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_irid_cplx)(ALWAN_CORE_T re, ALWAN_CORE_T im) {
    ALWAN_CORE_VEC2 r;
    r.v[0] = re;
    r.v[1] = im;
    return r;
}

/* The forward square root of re + i im: Im >= 0, and Re >= 0 when the root is real. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(alwan_irid_csqrt)(ALWAN_CORE_T re, ALWAN_CORE_T im) {
    ALWAN_CORE_VEC2 r;
    const ALWAN_CORE_T mag = ALWAN_CORE_SQRT(re * re + im * im);
    ALWAN_CORE_T t;
    r.v[0] = ALWAN_CORE_LITERAL(0.0);
    r.v[1] = ALWAN_CORE_LITERAL(0.0);
    if (!(mag > ALWAN_CORE_LITERAL(0.0))) return r;
    if (re >= ALWAN_CORE_LITERAL(0.0)) {
        t = ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(0.5) * (mag + re));
        r.v[0] = t;
        r.v[1] = im / (ALWAN_CORE_LITERAL(2.0) * t);
    } else {
        t = ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(0.5) * (mag - re));
        r.v[0] = ALWAN_CORE_ABS(im) / (ALWAN_CORE_LITERAL(2.0) * t);
        r.v[1] = im < ALWAN_CORE_LITERAL(0.0) ? -t : t;
    }
    if (r.v[1] < ALWAN_CORE_LITERAL(0.0)) {
        r.v[0] = -r.v[0];
        r.v[1] = -r.v[1];
    }
    return r;
}

/* The film's normal component q2 = sqrt(eta2^2 - s^2) at the incidence cosine cos1, or a
 * negative value past total reflection at the film's top (q2 then imaginary). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_irid_q2)(ALWAN_CORE_T eta1, ALWAN_CORE_T eta2, ALWAN_CORE_T cos1) {
    const ALWAN_CORE_T s2 = eta1 * eta1 * (ALWAN_CORE_LITERAL(1.0) - cos1 * cos1);
    const ALWAN_CORE_T d = eta2 * eta2 - s2;
    if (!(d > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(-1.0);
    return ALWAN_CORE_SQRT(d);
}

/* The two interfaces of one polarisation (pol 0 = s, 1 = p) below total reflection at the
 * top: v[0] = r12 (real), v[1], v[2] = Re, Im of r23. cos1 in [0, 1], q2 > 0. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_irid_interfaces)(ALWAN_CORE_T eta1, ALWAN_CORE_T eta2, ALWAN_CORE_T eta3,
                                                                     ALWAN_CORE_T kappa3, ALWAN_CORE_T cos1, ALWAN_CORE_T q2,
                                                                     int pol) {
    ALWAN_CORE_VEC3 r;
    const ALWAN_CORE_T s2 = eta1 * eta1 * (ALWAN_CORE_LITERAL(1.0) - cos1 * cos1);
    const ALWAN_CORE_T q1 = eta1 * cos1;
    const ALWAN_CORE_T n3re = eta3 * eta3 - kappa3 * kappa3;   /* n3^2 */
    const ALWAN_CORE_T n3im = ALWAN_CORE_LITERAL(2.0) * eta3 * kappa3;
    const ALWAN_CORE_VEC2 q3 = ALWAN_CORE_FNVLIT(alwan_irid_csqrt)(n3re - s2, n3im);
    ALWAN_CORE_VEC2 num, den, r23;
    ALWAN_CORE_T a;
    if (pol == 0) {
        a = (q1 - q2) / (q1 + q2);
        num = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(q2 - q3.v[0], -q3.v[1]);
        den = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(q2 + q3.v[0], q3.v[1]);
    } else {
        const ALWAN_CORE_T e1 = eta1 * eta1, e2 = eta2 * eta2;
        a = (e2 * q1 - e1 * q2) / (e2 * q1 + e1 * q2);
        num = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(n3re * q2 - e2 * q3.v[0], n3im * q2 - e2 * q3.v[1]);
        den = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(n3re * q2 + e2 * q3.v[0], n3im * q2 + e2 * q3.v[1]);
    }
    r23 = ALWAN_CORE_FNVLIT(alwan_irid_cdiv)(num, den);
    r.v[0] = a;
    r.v[1] = r23.v[0];
    r.v[2] = r23.v[1];
    return r;
}

/* The exact Airy reflectance of one polarisation (pol 0 = s, 1 = p) at the wavelength
 * lambda (nm), every quantity complex, so it holds past total reflection at the top too
 * (an evanescent film, frustrated total internal reflection). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_irid_airy_pol)(ALWAN_CORE_T eta1, ALWAN_CORE_T eta2, ALWAN_CORE_T d,
                                                                ALWAN_CORE_T eta3, ALWAN_CORE_T kappa3, ALWAN_CORE_T cos1,
                                                                ALWAN_CORE_T lambda, int pol) {
    const ALWAN_CORE_T s2 = eta1 * eta1 * (ALWAN_CORE_LITERAL(1.0) - cos1 * cos1);
    const ALWAN_CORE_VEC2 q1 = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(eta1 * cos1, ALWAN_CORE_LITERAL(0.0));
    const ALWAN_CORE_VEC2 q2 = ALWAN_CORE_FNVLIT(alwan_irid_csqrt)(eta2 * eta2 - s2, ALWAN_CORE_LITERAL(0.0));
    const ALWAN_CORE_T n3re = eta3 * eta3 - kappa3 * kappa3;
    const ALWAN_CORE_T n3im = ALWAN_CORE_LITERAL(2.0) * eta3 * kappa3;
    const ALWAN_CORE_VEC2 n3sq = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(n3re, n3im);
    const ALWAN_CORE_VEC2 q3 = ALWAN_CORE_FNVLIT(alwan_irid_csqrt)(n3re - s2, n3im);
    const ALWAN_CORE_T e1 = eta1 * eta1, e2 = eta2 * eta2;
    const ALWAN_CORE_T k = ALWAN_CORE_LITERAL(4.0) * ALWAN_CORE_PI * d / lambda;
    ALWAN_CORE_VEC2 r12, r23, ph, z, num, den, one, r;
    ALWAN_CORE_T decay;
    if (pol == 0) {
        r12 = ALWAN_CORE_FNVLIT(alwan_irid_cdiv)(ALWAN_CORE_FNVLIT(alwan_irid_cplx)(q1.v[0] - q2.v[0], q1.v[1] - q2.v[1]),
                                                 ALWAN_CORE_FNVLIT(alwan_irid_cplx)(q1.v[0] + q2.v[0], q1.v[1] + q2.v[1]));
        r23 = ALWAN_CORE_FNVLIT(alwan_irid_cdiv)(ALWAN_CORE_FNVLIT(alwan_irid_cplx)(q2.v[0] - q3.v[0], q2.v[1] - q3.v[1]),
                                                 ALWAN_CORE_FNVLIT(alwan_irid_cplx)(q2.v[0] + q3.v[0], q2.v[1] + q3.v[1]));
    } else {
        const ALWAN_CORE_VEC2 a1 = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(e2 * q1.v[0] - e1 * q2.v[0], e2 * q1.v[1] - e1 * q2.v[1]);
        const ALWAN_CORE_VEC2 b1 = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(e2 * q1.v[0] + e1 * q2.v[0], e2 * q1.v[1] + e1 * q2.v[1]);
        const ALWAN_CORE_VEC2 n3q2 = ALWAN_CORE_FNVLIT(alwan_irid_cmul)(n3sq, q2);
        r12 = ALWAN_CORE_FNVLIT(alwan_irid_cdiv)(a1, b1);
        r23 = ALWAN_CORE_FNVLIT(alwan_irid_cdiv)(
            ALWAN_CORE_FNVLIT(alwan_irid_cplx)(n3q2.v[0] - e2 * q3.v[0], n3q2.v[1] - e2 * q3.v[1]),
            ALWAN_CORE_FNVLIT(alwan_irid_cplx)(n3q2.v[0] + e2 * q3.v[0], n3q2.v[1] + e2 * q3.v[1]));
    }
    /* e^(i delta) with delta = k q2: e^(-k Im q2) (cos, sin)(k Re q2) */
    decay = ALWAN_CORE_EXP(-k * q2.v[1]);
    ph = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(decay * ALWAN_CORE_COS(k * q2.v[0]), decay * ALWAN_CORE_SIN(k * q2.v[0]));
    z = ALWAN_CORE_FNVLIT(alwan_irid_cmul)(r23, ph);
    num = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(r12.v[0] + z.v[0], r12.v[1] + z.v[1]);
    one = ALWAN_CORE_FNVLIT(alwan_irid_cmul)(r12, z);
    den = ALWAN_CORE_FNVLIT(alwan_irid_cplx)(ALWAN_CORE_LITERAL(1.0) + one.v[0], one.v[1]);
    if (!(den.v[0] * den.v[0] + den.v[1] * den.v[1] > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(1.0);
    r = ALWAN_CORE_FNVLIT(alwan_irid_cdiv)(num, den);
    return r.v[0] * r.v[0] + r.v[1] * r.v[1];
}

/* The Airy reflectance at lambda: pol 1 = s, 2 = p, anything else the mean of the two
 * (alwan_polarization's values). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_irid_reflectance)(ALWAN_CORE_T eta1, ALWAN_CORE_T eta2, ALWAN_CORE_T d,
                                                                   ALWAN_CORE_T eta3, ALWAN_CORE_T kappa3, ALWAN_CORE_T cos1,
                                                                   ALWAN_CORE_T lambda, int pol) {
    ALWAN_CORE_T rs, rp;
    if (pol == 1) return ALWAN_CORE_FNVLIT(alwan_irid_airy_pol)(eta1, eta2, d, eta3, kappa3, cos1, lambda, 0);
    if (pol == 2) return ALWAN_CORE_FNVLIT(alwan_irid_airy_pol)(eta1, eta2, d, eta3, kappa3, cos1, lambda, 1);
    rs = ALWAN_CORE_FNVLIT(alwan_irid_airy_pol)(eta1, eta2, d, eta3, kappa3, cos1, lambda, 0);
    rp = ALWAN_CORE_FNVLIT(alwan_irid_airy_pol)(eta1, eta2, d, eta3, kappa3, cos1, lambda, 1);
    return ALWAN_CORE_LITERAL(0.5) * (rs + rp);
}

/* A table's continuous texel coordinate of a value t in [0, 1] over n texels: texel
 * centres at (i + 0.5) / n, held to [0, n - 1] (clamp to edge). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_irid_texel)(ALWAN_CORE_T t, int n) {
    ALWAN_CORE_T x = t * (ALWAN_CORE_T)n - ALWAN_CORE_LITERAL(0.5);
    if (!(x > ALWAN_CORE_LITERAL(0.0))) x = ALWAN_CORE_LITERAL(0.0);
    if (x > (ALWAN_CORE_T)(n - 1)) x = (ALWAN_CORE_T)(n - 1);
    return x;
}

/* The lower of the two entries a continuous coordinate x in [0, n - 1] blends. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_irid_lower)(ALWAN_CORE_T x, int n) {
    int i = (int)ALWAN_CORE_FLOOR(x);
    if (i > n - 2) i = n - 2;
    if (i < 0) i = 0;
    return i;
}

#if ALWAN_HAS_POINTERS
#define ALWAN_IRIDESCENCE_READER_POINTER 1
#include "alwan_iridescence_reader.inc"
#endif
