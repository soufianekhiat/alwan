/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Thin-film iridescence table readers, written against an accessor instead of a pointer.
 * No include guard - included once per binding.
 *
 * SENSITIVITY TABLE. The Fourier transform in wavenumber of the three colour weights,
 * G_c(f) = integral of w_c(lambda) e^(i 2 pi f / lambda) d lambda over one spectral band,
 * f the optical path difference in nm, w_c the colour matching function c (or the RGB
 * primary c) times the illuminant, normalised so that over all bands G_Y(0) = 1 (a
 * reflectance of 1 is the white). Per band, count entries, entry j at f = j * opd_step,
 * six values each: Re, Im of channel 0, 1, 2; band b's entries start at b * count * 6.
 * Stored demodulated, D(f) = G(f) e^(-i 2 pi f nu_b) with nu_b the band's mean
 * wavenumber, so linear interpolation between entries is accurate; the reader turns it
 * back. G is 0 past the last entry. As alwan_iridescence_sensitivity writes it.
 *
 * IRIDESCENCE TABLE. Three values per texel, width texels of the incidence cosine by
 * height texels of film thickness, row-major, texel (i, j) at (j * width + i) * 3, centres
 * at ((i + 0.5) / width, (j + 0.5) / height) of [0, 1] x [0, 1] (the thickness axis
 * spans the table's thickness range), as alwan_iridescence_table writes it. Bilinear,
 * clamp to edge.
 *
 * ACCESSOR. The tables are read through ALWAN_IRIDESCENCE_READ(i). A shader defines
 *     StructuredBuffer<float> Sens : register(t7);
 *     #define ALWAN_IRIDESCENCE_NAME     film
 *     #define ALWAN_IRIDESCENCE_READ(i)  Sens[i]
 *     #include "core/alwan_iridescence_reader.inc"
 */

#if defined(ALWAN_IRIDESCENCE_READER_POINTER)

# define ALWAN_IRID_FN_(base)      ALWAN_CORE_FNVLIT(base)
# define ALWAN_IRID_PARAMS_        const ALWAN_CORE_T *tab,
# define ALWAN_IRID_ARGS_          tab,
# define ALWAN_IRIDESCENCE_READ(i) tab[i]

#else

# if !defined(ALWAN_IRIDESCENCE_NAME) || !defined(ALWAN_IRIDESCENCE_READ)
#  error "alwan_iridescence_reader.inc: define ALWAN_IRIDESCENCE_NAME and ALWAN_IRIDESCENCE_READ before including it"
# endif
# define ALWAN_IRID_PASTE2_(a, b)  a##_##b
# define ALWAN_IRID_PASTE_(a, b)   ALWAN_IRID_PASTE2_(a, b)
# define ALWAN_IRID_FN_(base)      ALWAN_IRID_PASTE_(base, ALWAN_IRIDESCENCE_NAME)
# define ALWAN_IRID_PARAMS_
# define ALWAN_IRID_ARGS_

#endif

/* Channel c of G at the optical path difference f (nm) in band `band`: v[0] = Re,
 * v[1] = Im. nu is the band's demodulation wavenumber. */
ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_IRID_FN_(alwan_iridescence_sensitivity_at)(ALWAN_IRID_PARAMS_ int count, ALWAN_CORE_T opd_step,
                                                                             ALWAN_CORE_T nu, int band, int c, ALWAN_CORE_T f) {
    ALWAN_CORE_VEC2 g;
    const int base = band * count * 6;
    ALWAN_CORE_T x = f / opd_step;
    ALWAN_CORE_T fr, dre, dim, ph, cs, sn;
    int i;
    g.v[0] = ALWAN_CORE_LITERAL(0.0);
    g.v[1] = ALWAN_CORE_LITERAL(0.0);
    if (!(x >= ALWAN_CORE_LITERAL(0.0)) || x > (ALWAN_CORE_T)(count - 1)) return g;
    i = ALWAN_CORE_FNVLIT(alwan_irid_lower)(x, count);
    fr = x - (ALWAN_CORE_T)i;
    dre = ALWAN_IRIDESCENCE_READ(base + i * 6 + c * 2) +
          (ALWAN_IRIDESCENCE_READ(base + (i + 1) * 6 + c * 2) - ALWAN_IRIDESCENCE_READ(base + i * 6 + c * 2)) * fr;
    dim = ALWAN_IRIDESCENCE_READ(base + i * 6 + c * 2 + 1) +
          (ALWAN_IRIDESCENCE_READ(base + (i + 1) * 6 + c * 2 + 1) - ALWAN_IRIDESCENCE_READ(base + i * 6 + c * 2 + 1)) * fr;
    ph = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * f * nu;
    cs = ALWAN_CORE_COS(ph);
    sn = ALWAN_CORE_SIN(ph);
    g.v[0] = dre * cs - dim * sn;
    g.v[1] = dre * sn + dim * cs;
    return g;
}

/* One band's share of the film's reflected colour: the Airy series of
 * alwan_iridescence_core.inc integrated against the band's three weights, averaged over s
 * and p, with the band's indices. Summed over the table's bands it is the iridescent
 * Fresnel term at the incidence cosine cos1 (n . l of a mirror, h . v of a microfacet).
 * Terms are added until one falls below tolerance, at most max_terms. Past total
 * reflection at the film's top the series does not apply (the film is evanescent and
 * nothing oscillates): the band's share is then the exact Airy reflectance at the band's
 * wavelength 1 / nu times the band's white. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_IRID_FN_(alwan_iridescence_fresnel_band)(ALWAN_IRID_PARAMS_ int count, ALWAN_CORE_T opd_step,
                                                                           ALWAN_CORE_T nu, int band, int max_terms,
                                                                           ALWAN_CORE_T tolerance, ALWAN_CORE_T eta1,
                                                                           ALWAN_CORE_T eta2, ALWAN_CORE_T d, ALWAN_CORE_T eta3,
                                                                           ALWAN_CORE_T kappa3, ALWAN_CORE_T cos1) {
    ALWAN_CORE_VEC3 acc, ifc;
    const int base = band * count * 6;
    ALWAN_CORE_T q2, opd, a, a2, rho2, den, kk, c0, pr, pim, am, t, coef, mag;
    int pol, m, c;
    acc.v[0] = ALWAN_CORE_LITERAL(0.0);
    acc.v[1] = ALWAN_CORE_LITERAL(0.0);
    acc.v[2] = ALWAN_CORE_LITERAL(0.0);
    if (cos1 > ALWAN_CORE_LITERAL(1.0)) cos1 = ALWAN_CORE_LITERAL(1.0);
    if (!(cos1 > ALWAN_CORE_LITERAL(0.0))) cos1 = ALWAN_CORE_LITERAL(0.0);
    q2 = ALWAN_CORE_FNVLIT(alwan_irid_q2)(eta1, eta2, cos1);
    if (!(q2 > ALWAN_CORE_LITERAL(0.0))) {
        const ALWAN_CORE_T r = ALWAN_CORE_FNVLIT(alwan_irid_reflectance)(eta1, eta2, d, eta3, kappa3, cos1,
                                                                         ALWAN_CORE_LITERAL(1.0) / nu, 0);
        for (c = 0; c < 3; c++) acc.v[c] = r * ALWAN_IRIDESCENCE_READ(base + c * 2);
        return acc;
    }
    opd = ALWAN_CORE_LITERAL(2.0) * d * q2;
    for (pol = 0; pol < 2; pol++) {
        ifc = ALWAN_CORE_FNVLIT(alwan_irid_interfaces)(eta1, eta2, eta3, kappa3, cos1, q2, pol);
        a = ifc.v[0];
        a2 = a * a;
        rho2 = ifc.v[1] * ifc.v[1] + ifc.v[2] * ifc.v[2];
        den = ALWAN_CORE_LITERAL(1.0) - a2 * rho2;
        if (!(den > ALWAN_CORE_LITERAL(0.0))) {
            /* a lossless mirror on either side: everything is reflected */
            for (c = 0; c < 3; c++) acc.v[c] = acc.v[c] + ALWAN_CORE_LITERAL(0.5) * ALWAN_IRIDESCENCE_READ(base + c * 2);
            continue;
        }
        kk = (ALWAN_CORE_LITERAL(1.0) - a2) * (ALWAN_CORE_LITERAL(1.0) - rho2) / den;
        c0 = (a2 + rho2 - ALWAN_CORE_LITERAL(2.0) * a2 * rho2) / den;
        for (c = 0; c < 3; c++) acc.v[c] = acc.v[c] + ALWAN_CORE_LITERAL(0.5) * c0 * ALWAN_IRIDESCENCE_READ(base + c * 2);
        pr = ALWAN_CORE_LITERAL(1.0);   /* r23^m */
        pim = ALWAN_CORE_LITERAL(0.0);
        am = ALWAN_CORE_LITERAL(1.0);   /* (-a)^m */
        for (m = 1; m <= max_terms; m++) {
            t = pr * ifc.v[1] - pim * ifc.v[2];
            pim = pr * ifc.v[2] + pim * ifc.v[1];
            pr = t;
            am = -am * a;
            coef = ALWAN_CORE_LITERAL(-2.0) * kk * am;
            mag = ALWAN_CORE_ABS(coef) * ALWAN_CORE_SQRT(pr * pr + pim * pim);
            if (!(mag >= tolerance)) break;
            for (c = 0; c < 3; c++) {
                const ALWAN_CORE_VEC2 g = ALWAN_IRID_FN_(alwan_iridescence_sensitivity_at)(ALWAN_IRID_ARGS_ count, opd_step, nu, band, c,
                                                                                         (ALWAN_CORE_T)m * opd);
                acc.v[c] = acc.v[c] + ALWAN_CORE_LITERAL(0.5) * coef * (pr * g.v[0] - pim * g.v[1]);
            }
        }
    }
    return acc;
}

/* The film's colour from a one-band table (the paper's model, constant indices): band 0. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_IRID_FN_(alwan_iridescence_fresnel)(ALWAN_IRID_PARAMS_ int count, ALWAN_CORE_T opd_step,
                                                                      ALWAN_CORE_T nu0, int max_terms, ALWAN_CORE_T tolerance,
                                                                      ALWAN_CORE_T eta1, ALWAN_CORE_T eta2, ALWAN_CORE_T d,
                                                                      ALWAN_CORE_T eta3, ALWAN_CORE_T kappa3, ALWAN_CORE_T cos1) {
    return ALWAN_IRID_FN_(alwan_iridescence_fresnel_band)(ALWAN_IRID_ARGS_ count, opd_step, nu0, 0, max_terms, tolerance, eta1, eta2, d,
                                                          eta3, kappa3, cos1);
}

/* The iridescence table at (cosine, thickness position t in [0, 1]), bilinear. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_IRID_FN_(alwan_iridescence_table_lookup)(ALWAN_IRID_PARAMS_ int width, int height,
                                                                           ALWAN_CORE_T cos1, ALWAN_CORE_T t) {
    ALWAN_CORE_VEC3 r;
    const ALWAN_CORE_T x = ALWAN_CORE_FNVLIT(alwan_irid_texel)(cos1, width);
    const ALWAN_CORE_T y = ALWAN_CORE_FNVLIT(alwan_irid_texel)(t, height);
    const int i0 = width > 1 ? ALWAN_CORE_FNVLIT(alwan_irid_lower)(x, width) : 0;
    const int j0 = height > 1 ? ALWAN_CORE_FNVLIT(alwan_irid_lower)(y, height) : 0;
    const int i1 = width > 1 ? i0 + 1 : i0;
    const int j1 = height > 1 ? j0 + 1 : j0;
    const ALWAN_CORE_T fx = width > 1 ? x - (ALWAN_CORE_T)i0 : ALWAN_CORE_LITERAL(0.0);
    const ALWAN_CORE_T fy = height > 1 ? y - (ALWAN_CORE_T)j0 : ALWAN_CORE_LITERAL(0.0);
    int c;
    for (c = 0; c < 3; c++) {
        const ALWAN_CORE_T a = ALWAN_IRIDESCENCE_READ((j0 * width + i0) * 3 + c);
        const ALWAN_CORE_T b = ALWAN_IRIDESCENCE_READ((j0 * width + i1) * 3 + c);
        const ALWAN_CORE_T e = ALWAN_IRIDESCENCE_READ((j1 * width + i0) * 3 + c);
        const ALWAN_CORE_T g = ALWAN_IRIDESCENCE_READ((j1 * width + i1) * 3 + c);
        const ALWAN_CORE_T top = a + (b - a) * fx;
        const ALWAN_CORE_T bot = e + (g - e) * fx;
        r.v[c] = top + (bot - top) * fy;
    }
    return r;
}

#undef ALWAN_IRID_FN_
#undef ALWAN_IRID_PARAMS_
#undef ALWAN_IRID_ARGS_
#undef ALWAN_IRIDESCENCE_READ
#if !defined(ALWAN_IRIDESCENCE_READER_POINTER)
# undef ALWAN_IRID_PASTE2_
# undef ALWAN_IRID_PASTE_
# undef ALWAN_IRIDESCENCE_NAME
#endif
#undef ALWAN_IRIDESCENCE_READER_POINTER
