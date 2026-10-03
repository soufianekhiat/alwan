/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Microfacet terms (parameterized): the GGX normal distribution, Smith's Lambda, the
 * height-correlated Smith masking-shadowing term and GGX half-vector sampling. No include
 * guard: included once per precision pass. Shared by iridescence (alwan) and image-based
 * lighting (suwar, through alwan_foundation.h's ALWAN_FOUNDATION_MICROFACET_CORE).
 */

/* ----------------------------------------------------------------
 * GGX (Trowbridge and Reitz 1975; Walter, Marschner, Li and Torrance 2007). alpha is the
 * width, the square of the perceptual roughness of Burley 2012 and Karis 2013.
 * ---------------------------------------------------------------- */

/* The normal distribution D(h) at n . h: alpha^2 / (pi ((n.h)^2 (alpha^2 - 1) + 1)^2).
 * At alpha 0 it is a delta, returned as 0 everywhere. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_microfacet_ggx_d)(ALWAN_CORE_T noh, ALWAN_CORE_T alpha) {
    const ALWAN_CORE_T a2 = alpha * alpha;
    const ALWAN_CORE_T d = noh * noh * (a2 - ALWAN_CORE_LITERAL(1.0)) + ALWAN_CORE_LITERAL(1.0);
    if (!(a2 > ALWAN_CORE_LITERAL(0.0)) || !(d > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    return a2 / (ALWAN_CORE_PI * d * d);
}

/* Smith's Lambda for GGX at a cosine mu in (0, 1]. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_microfacet_smith_lambda)(ALWAN_CORE_T mu, ALWAN_CORE_T alpha) {
    const ALWAN_CORE_T m2 = mu * mu;
    const ALWAN_CORE_T t2 = (ALWAN_CORE_LITERAL(1.0) - m2) / m2;
    return ALWAN_CORE_LITERAL(0.5) * (ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(1.0) + alpha * alpha * t2) -
                                      ALWAN_CORE_LITERAL(1.0));
}

/* The height-correlated Smith masking-shadowing term (Heitz 2014, eq. 99):
 * 1 / (1 + Lambda(n.v) + Lambda(n.l)); 0 when either cosine is not positive. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_microfacet_smith_g2)(ALWAN_CORE_T nov, ALWAN_CORE_T nol, ALWAN_CORE_T alpha) {
    if (!(nov > ALWAN_CORE_LITERAL(0.0)) || !(nol > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);
    return ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNVLIT(alwan_microfacet_smith_lambda)(nov, alpha) +
                                      ALWAN_CORE_FNVLIT(alwan_microfacet_smith_lambda)(nol, alpha));
}

/* A half vector with density D(h) (n . h), from u0, u1 in [0, 1): in the tangent frame,
 * the normal +z. cos theta_h = sqrt((1 - u0) / (1 + (alpha^2 - 1) u0)), phi_h = 2 pi u1. */
ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(alwan_microfacet_ggx_sample)(ALWAN_CORE_T u0, ALWAN_CORE_T u1, ALWAN_CORE_T alpha) {
    ALWAN_CORE_VEC3 h;
    const ALWAN_CORE_T a2 = alpha * alpha;
    ALWAN_CORE_T c2 = (ALWAN_CORE_LITERAL(1.0) - u0) / (ALWAN_CORE_LITERAL(1.0) + (a2 - ALWAN_CORE_LITERAL(1.0)) * u0);
    ALWAN_CORE_T st, phi;
    if (c2 > ALWAN_CORE_LITERAL(1.0)) c2 = ALWAN_CORE_LITERAL(1.0);
    if (c2 < ALWAN_CORE_LITERAL(0.0)) c2 = ALWAN_CORE_LITERAL(0.0);
    st = ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(1.0) - c2);
    phi = ALWAN_CORE_LITERAL(2.0) * ALWAN_CORE_PI * u1;
    h.v[0] = st * ALWAN_CORE_COS(phi);
    h.v[1] = st * ALWAN_CORE_SIN(phi);
    h.v[2] = ALWAN_CORE_SQRT(c2);
    return h;
}

