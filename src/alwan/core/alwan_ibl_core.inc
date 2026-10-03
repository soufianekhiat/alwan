/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Image-based lighting math (parameterized). No include guard: included once per
 * precision pass. The table readers are in alwan_ibl_reader.inc.
 *
 * SPHERICAL HARMONICS. Real, orthonormal over the sphere, without the Condon-Shortley
 * phase (Ramamoorthi and Hanrahan 2001's basis: Y_1,1 = +0.488603 x). The polar axis is
 * alwan's +Y, the up of the equirectangular map, and the angles are the map's: a map
 * position (x, y) is theta = pi y from +Y and phi = 2 pi x, the direction
 * (sin theta cos phi, cos theta, sin theta sin phi). So
 *     Y_l,0  = N_l^0 P_l^0(cos theta)
 *     Y_l,m  = sqrt(2) N_l^m P_l^m(cos theta) cos(m phi)       m > 0
 *     Y_l,-m = sqrt(2) N_l^m P_l^m(cos theta) sin(m phi)       m > 0
 *     N_l^m  = sqrt((2l + 1) / (4 pi) (l - m)! / (l + m)!)
 * with P_l^m the associated Legendre function without the (-1)^m factor. In Cartesian
 * terms of an alwan direction d: Y_1,-1 = c d.z, Y_1,0 = c d.y, Y_1,1 = c d.x,
 * c = sqrt(3 / (4 pi)). Coefficient (l, m) is stored at index l^2 + l + m.
 *
 * P_l^m(cos theta) = sin^m(theta) Q_l^m(cos theta) with Q a polynomial, and
 * sin^m(theta) (cos m phi, sin m phi) = (Re, Im) of (d.x + i d.z)^m, so a basis value is
 * a polynomial in the direction's components: no angle is taken, and the poles need no
 * special case.
 */

/* The index of coefficient (l, m), -l <= m <= l. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_sh_index)(int l, int m) {
    return l * l + l + m;
}

/* N_l^am Q_l^am(ct), times sqrt(2) for am > 0: the theta part of Y_l,+-am without its
 * sin^am(theta). am = |m| <= l. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sh_theta_part)(int l, int am, ALWAN_CORE_T ct) {
    ALWAN_CORE_T qmm = ALWAN_CORE_LITERAL(1.0);
    ALWAN_CORE_T q0, q1, q2, ratio, n;
    int k;
    for (k = 1; k <= am; k++) qmm = qmm * (ALWAN_CORE_T)(2 * k - 1);
    q0 = qmm;
    q1 = ct * (ALWAN_CORE_T)(2 * am + 1) * qmm;
    for (k = am + 2; k <= l; k++) {
        q2 = ((ALWAN_CORE_T)(2 * k - 1) * ct * q1 - (ALWAN_CORE_T)(k + am - 1) * q0) / (ALWAN_CORE_T)(k - am);
        q0 = q1;
        q1 = q2;
    }
    if (l == am) q1 = qmm;
    ratio = ALWAN_CORE_LITERAL(1.0);
    for (k = l - am + 1; k <= l + am; k++) ratio = ratio / (ALWAN_CORE_T)k;
    n = ALWAN_CORE_SQRT((ALWAN_CORE_T)(2 * l + 1) / (ALWAN_CORE_LITERAL(4.0) * ALWAN_CORE_PI) * ratio);
    if (am > 0) n = n * ALWAN_CORE_LITERAL(1.4142135623730950488);
    return n * q1;
}

/* Y_l,m at the unit direction (dx, dy, dz) of alwan's frame (+Y up). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sh_basis)(int l, int m, ALWAN_CORE_T dx, ALWAN_CORE_T dy,
                                                         ALWAN_CORE_T dz) {
    const int am = m < 0 ? -m : m;
    ALWAN_CORE_T re = ALWAN_CORE_LITERAL(1.0), im = ALWAN_CORE_LITERAL(0.0), t;
    int k;
    for (k = 0; k < am; k++) {
        t = re * dx - im * dz;
        im = re * dz + im * dx;
        re = t;
    }
    t = ALWAN_CORE_FNVLIT(alwan_sh_theta_part)(l, am, dy);
    return m < 0 ? t * im : t * re;
}

/* The clamped cosine max(0, cos theta) in zonal harmonics, scaled as Ramamoorthi and
 * Hanrahan 2001 (eq. 8) use it: the irradiance coefficient is A_l L_l,m.
 *     A_0 = pi, A_1 = 2 pi / 3, A_l = 0 for odd l > 1,
 *     A_l = 2 pi (-1)^(l/2 - 1) / ((l + 2)(l - 1)) * l! / (2^l ((l/2)!)^2)   even l >= 2
 * so A_2 = pi / 4, A_4 = -pi / 24, A_6 = pi / 64, A_8 = -pi / 128. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sh_cosine_lobe)(int l) {
    ALWAN_CORE_T c;
    int k;
    if (l == 0) return ALWAN_CORE_PI;
    if (l == 1) return ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(3.0);
    if (l < 0 || (l & 1) != 0) return ALWAN_CORE_LITERAL(0.0);
    /* l! / (2^l ((l/2)!)^2), the central binomial over 2^l, built factor by factor */
    c = ALWAN_CORE_LITERAL(1.0);
    for (k = 1; k <= l / 2; k++) {
        c = c * (ALWAN_CORE_T)(l / 2 + k) / (ALWAN_CORE_T)k;
    }
    for (k = 0; k < l; k++) c = c * ALWAN_CORE_LITERAL(0.5);
    c = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * c / ((ALWAN_CORE_T)(l + 2) * (ALWAN_CORE_T)(l - 1));
    return ((l / 2) & 1) != 0 ? c : -c;
}

/* The ringing window of band l: kind 0 none (1), 1 Hanning, 0.5 (1 + cos(pi l / width))
 * and 0 from l = width on, 2 Lanczos sigma, sinc(l / width) = sin(pi l / width) /
 * (pi l / width) and 0 from l = width on. width > 0. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sh_window)(int l, int kind, ALWAN_CORE_T width) {
    ALWAN_CORE_T s;
    if (kind == 0 || l == 0) return ALWAN_CORE_LITERAL(1.0);
    if ((ALWAN_CORE_T)l >= width) return ALWAN_CORE_LITERAL(0.0);
    s = ALWAN_CORE_PI * (ALWAN_CORE_T)l / width;
    if (kind == 1) return ALWAN_CORE_LITERAL(0.5) * (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_COS(s));
    return ALWAN_CORE_SIN(s) / s;
}

/* ----------------------------------------------------------------
 * GGX and Smith: the shared microfacet terms of alwan_microfacet_core.inc, under the names
 * image-based lighting has always used.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_ibl_ggx_d)(ALWAN_CORE_T noh, ALWAN_CORE_T alpha) {
    return ALWAN_CORE_FNVLIT(alwan_microfacet_ggx_d)(noh, alpha);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_ibl_smith_lambda)(ALWAN_CORE_T mu, ALWAN_CORE_T alpha) {
    return ALWAN_CORE_FNVLIT(alwan_microfacet_smith_lambda)(mu, alpha);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_ibl_smith_g2)(ALWAN_CORE_T nov, ALWAN_CORE_T nol, ALWAN_CORE_T alpha) {
    return ALWAN_CORE_FNVLIT(alwan_microfacet_smith_g2)(nov, nol, alpha);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_ibl_ggx_sample)(ALWAN_CORE_T u0, ALWAN_CORE_T u1, ALWAN_CORE_T alpha) {
    return ALWAN_CORE_FNVLIT(alwan_microfacet_ggx_sample)(u0, u1, alpha);
}

/* The split-sum table's continuous texel coordinate of a value t in [0, 1] over n
 * texels: texel centres at (i + 0.5) / n, held to [0, n - 1] (clamp to edge). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_ibl_lut_texel)(ALWAN_CORE_T t, int n) {
    ALWAN_CORE_T x = t * (ALWAN_CORE_T)n - ALWAN_CORE_LITERAL(0.5);
    if (!(x > ALWAN_CORE_LITERAL(0.0))) x = ALWAN_CORE_LITERAL(0.0);
    if (x > (ALWAN_CORE_T)(n - 1)) x = (ALWAN_CORE_T)(n - 1);
    return x;
}

/* The lower of the two texels a continuous coordinate x in [0, n - 1] blends. */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_ibl_lut_lower)(ALWAN_CORE_T x, int n) {
    int i = (int)ALWAN_CORE_FLOOR(x);
    if (i > n - 2) i = n - 2;
    if (i < 0) i = 0;
    return i;
}

#if ALWAN_HAS_POINTERS
#define ALWAN_IBL_READER_POINTER 1
#include "alwan_ibl_reader.inc"
#endif
