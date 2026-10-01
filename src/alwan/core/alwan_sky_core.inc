/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Physical sky models, per direction (parameterized). No include guard: included
 * once per precision pass. The Bruneton table lookups live in
 * alwan_sky_atmosphere_reader.inc; this file binds them to pointers.
 */

/* ----------------------------------------------------------------
 * Perez et al. 1993 all-weather distribution, as Preetham, Shirley and Smits 1999
 * write it (their Equation 3):
 *     F(theta, gamma) = (1 + A exp(B / cos theta)) (1 + C exp(D gamma) + E cos^2 gamma)
 * theta the view's angle from the zenith, gamma its angle from the sun.
 * ---------------------------------------------------------------- */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sky_perez)(ALWAN_CORE_T a, ALWAN_CORE_T b, ALWAN_CORE_T c,
                                                            ALWAN_CORE_T d, ALWAN_CORE_T e,
                                                            ALWAN_CORE_T cos_theta, ALWAN_CORE_T gamma) {
    const ALWAN_CORE_T cg = ALWAN_CORE_COS(gamma);
    return (ALWAN_CORE_LITERAL(1.0) + a * ALWAN_CORE_EXP(b / cos_theta))
         * (ALWAN_CORE_LITERAL(1.0) + c * ALWAN_CORE_EXP(d * gamma) + e * cg * cg);
}

/* Preetham 1999, Appendix A.2: the Perez coefficients of channel ch (0 = Y, 1 = x,
 * 2 = y) at turbidity T, as five values k = 0..4 for A..E. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sky_preetham_coef)(int ch, int k, ALWAN_CORE_T t) {
    /* rows A..E: (slope in T, constant) */
    ALWAN_CORE_T s = ALWAN_CORE_LITERAL(0.0), c0 = ALWAN_CORE_LITERAL(0.0);
    if (ch == 0) {
        if (k == 0) { s = ALWAN_CORE_LITERAL(0.1787);  c0 = ALWAN_CORE_LITERAL(-1.4630); }
        if (k == 1) { s = ALWAN_CORE_LITERAL(-0.3554); c0 = ALWAN_CORE_LITERAL(0.4275); }
        if (k == 2) { s = ALWAN_CORE_LITERAL(-0.0227); c0 = ALWAN_CORE_LITERAL(5.3251); }
        if (k == 3) { s = ALWAN_CORE_LITERAL(0.1206);  c0 = ALWAN_CORE_LITERAL(-2.5771); }
        if (k == 4) { s = ALWAN_CORE_LITERAL(-0.0670); c0 = ALWAN_CORE_LITERAL(0.3703); }
    } else if (ch == 1) {
        if (k == 0) { s = ALWAN_CORE_LITERAL(-0.0193); c0 = ALWAN_CORE_LITERAL(-0.2592); }
        if (k == 1) { s = ALWAN_CORE_LITERAL(-0.0665); c0 = ALWAN_CORE_LITERAL(0.0008); }
        if (k == 2) { s = ALWAN_CORE_LITERAL(-0.0004); c0 = ALWAN_CORE_LITERAL(0.2125); }
        if (k == 3) { s = ALWAN_CORE_LITERAL(-0.0641); c0 = ALWAN_CORE_LITERAL(-0.8989); }
        if (k == 4) { s = ALWAN_CORE_LITERAL(-0.0033); c0 = ALWAN_CORE_LITERAL(0.0452); }
    } else {
        if (k == 0) { s = ALWAN_CORE_LITERAL(-0.0167); c0 = ALWAN_CORE_LITERAL(-0.2608); }
        if (k == 1) { s = ALWAN_CORE_LITERAL(-0.0950); c0 = ALWAN_CORE_LITERAL(0.0092); }
        if (k == 2) { s = ALWAN_CORE_LITERAL(-0.0079); c0 = ALWAN_CORE_LITERAL(0.2102); }
        if (k == 3) { s = ALWAN_CORE_LITERAL(-0.0441); c0 = ALWAN_CORE_LITERAL(-1.6537); }
        if (k == 4) { s = ALWAN_CORE_LITERAL(-0.0109); c0 = ALWAN_CORE_LITERAL(0.0529); }
    }
    return s * t + c0;
}

/* Preetham 1999, Appendix A.2: the zenith luminance Y_z in kcd/m^2 at turbidity t and
 * sun zenith angle theta_s (radians):
 *     chi = (4/9 - T/120)(pi - 2 theta_s)
 *     Y_z = (4.0453 T - 4.9710) tan chi - 0.2155 T + 2.4192 */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sky_preetham_zenith_y)(ALWAN_CORE_T t, ALWAN_CORE_T theta_s) {
    const ALWAN_CORE_T chi = (ALWAN_CORE_LITERAL(4.0) / ALWAN_CORE_LITERAL(9.0) - t / ALWAN_CORE_LITERAL(120.0))
                           * (ALWAN_CORE_PI - ALWAN_CORE_LITERAL(2.0) * theta_s);
    return (ALWAN_CORE_LITERAL(4.0453) * t - ALWAN_CORE_LITERAL(4.9710)) * ALWAN_CORE_TAN(chi)
         - ALWAN_CORE_LITERAL(0.2155) * t + ALWAN_CORE_LITERAL(2.4192);
}

/* Preetham 1999, Appendix A.2: the zenith chromaticity x_z (ch 1) or y_z (ch 2),
 *     [T^2 T 1] M [theta_s^3 theta_s^2 theta_s 1]^T */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sky_preetham_zenith_xy)(int ch, ALWAN_CORE_T t, ALWAN_CORE_T theta_s) {
    const ALWAN_CORE_T t2 = t * t;
    const ALWAN_CORE_T s1 = theta_s, s2 = theta_s * theta_s, s3 = s2 * theta_s;
    if (ch == 1) {
        return t2 * (ALWAN_CORE_LITERAL(0.0017) * s3 - ALWAN_CORE_LITERAL(0.0037) * s2 + ALWAN_CORE_LITERAL(0.0021) * s1)
             + t * (ALWAN_CORE_LITERAL(-0.0290) * s3 + ALWAN_CORE_LITERAL(0.0638) * s2 - ALWAN_CORE_LITERAL(0.0320) * s1
                    + ALWAN_CORE_LITERAL(0.0039))
             + (ALWAN_CORE_LITERAL(0.1169) * s3 - ALWAN_CORE_LITERAL(0.2120) * s2 + ALWAN_CORE_LITERAL(0.0605) * s1
                + ALWAN_CORE_LITERAL(0.2589));
    }
    return t2 * (ALWAN_CORE_LITERAL(0.0028) * s3 - ALWAN_CORE_LITERAL(0.0061) * s2 + ALWAN_CORE_LITERAL(0.0032) * s1)
         + t * (ALWAN_CORE_LITERAL(-0.0421) * s3 + ALWAN_CORE_LITERAL(0.0897) * s2 - ALWAN_CORE_LITERAL(0.0415) * s1
                + ALWAN_CORE_LITERAL(0.0052))
         + (ALWAN_CORE_LITERAL(0.1535) * s3 - ALWAN_CORE_LITERAL(0.2676) * s2 + ALWAN_CORE_LITERAL(0.0667) * s1
            + ALWAN_CORE_LITERAL(0.2669));
}

/* Preetham's sky value of channel ch (Y in kcd/m^2, x or y) in a direction:
 *     value = value_z F(theta, gamma) / F(0, theta_s)
 * cos_theta > 0 (above the horizon), gamma the angle to the sun. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sky_preetham_value)(int ch, ALWAN_CORE_T t, ALWAN_CORE_T theta_s,
                                                                     ALWAN_CORE_T cos_theta, ALWAN_CORE_T gamma) {
    const ALWAN_CORE_T a = ALWAN_CORE_FNVLIT(alwan_sky_preetham_coef)(ch, 0, t);
    const ALWAN_CORE_T b = ALWAN_CORE_FNVLIT(alwan_sky_preetham_coef)(ch, 1, t);
    const ALWAN_CORE_T c = ALWAN_CORE_FNVLIT(alwan_sky_preetham_coef)(ch, 2, t);
    const ALWAN_CORE_T d = ALWAN_CORE_FNVLIT(alwan_sky_preetham_coef)(ch, 3, t);
    const ALWAN_CORE_T e = ALWAN_CORE_FNVLIT(alwan_sky_preetham_coef)(ch, 4, t);
    const ALWAN_CORE_T z = ch == 0 ? ALWAN_CORE_FNVLIT(alwan_sky_preetham_zenith_y)(t, theta_s)
                                   : ALWAN_CORE_FNVLIT(alwan_sky_preetham_zenith_xy)(ch, t, theta_s);
    const ALWAN_CORE_T f = ALWAN_CORE_FNVLIT(alwan_sky_perez)(a, b, c, d, e, cos_theta, gamma);
    const ALWAN_CORE_T f0 = ALWAN_CORE_FNVLIT(alwan_sky_perez)(a, b, c, d, e, ALWAN_CORE_LITERAL(1.0), theta_s);
    return z * f / f0;
}

/* ----------------------------------------------------------------
 * Hosek and Wilkie 2012, the radiance distribution of one band (ArHosekSkyModel.c 1.4a,
 * ArHosekSkyModel_GetRadianceInternal, BSD-3-Clause, Copyright (c) 2012 - 2013 Lukas
 * Hosek and Alexander Wilkie): with the nine cooked coefficients A..I,
 *     (1 + A exp(B / (cos theta + 0.01)))
 *   x (C + D exp(E gamma) + F cos^2 gamma + G chi(H, gamma) + I sqrt(cos theta))
 *     chi(g, gamma) = (1 + cos^2 gamma) / (1 + g^2 - 2 g cos gamma)^(3/2)
 * times the band's cooked radiance. Valid above the horizon (cos theta >= 0).
 * ---------------------------------------------------------------- */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sky_hosek)(ALWAN_CORE_T c0, ALWAN_CORE_T c1, ALWAN_CORE_T c2,
                                                            ALWAN_CORE_T c3, ALWAN_CORE_T c4, ALWAN_CORE_T c5,
                                                            ALWAN_CORE_T c6, ALWAN_CORE_T c7, ALWAN_CORE_T c8,
                                                            ALWAN_CORE_T cos_theta, ALWAN_CORE_T gamma) {
    const ALWAN_CORE_T cg = ALWAN_CORE_COS(gamma);
    const ALWAN_CORE_T exp_m = ALWAN_CORE_EXP(c4 * gamma);
    const ALWAN_CORE_T ray_m = cg * cg;
    const ALWAN_CORE_T mie_m = (ALWAN_CORE_LITERAL(1.0) + cg * cg)
        / ALWAN_CORE_POW(ALWAN_CORE_LITERAL(1.0) + c8 * c8 - ALWAN_CORE_LITERAL(2.0) * c8 * cg, ALWAN_CORE_LITERAL(1.5));
    const ALWAN_CORE_T zenith = ALWAN_CORE_SQRT(cos_theta);
    return (ALWAN_CORE_LITERAL(1.0) + c0 * ALWAN_CORE_EXP(c1 / (cos_theta + ALWAN_CORE_LITERAL(0.01))))
         * (c2 + c3 * exp_m + c5 * ray_m + c6 * mie_m + c7 * zenith);
}

/* ----------------------------------------------------------------
 * Bruneton 2017 table lookups, bound to pointers wherever pointers exist. A shading
 * language binds them itself: see the top of alwan_sky_atmosphere_reader.inc.
 * ---------------------------------------------------------------- */
#if ALWAN_HAS_POINTERS
#define ALWAN_ATM_READER_POINTER 1
#include "alwan_sky_atmosphere_reader.inc"
#undef ALWAN_ATM_READER_POINTER
#endif
