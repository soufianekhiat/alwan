/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Wavelength sampling for spectral rendering (parameterized).
 * No include guard - included once per precision pass via setup/teardown.
 *
 * Two closed-form densities over [lo, hi] (nm) and the hero rotation:
 *
 *   uniform   p = 1 / (hi - lo)
 *   visible   Radziszewski, Boryczko and Alda 2009, "An improved technique for full
 *             spectral rendering", Journal of WSCG 17: p proportional to
 *             1 / cosh^2(a (lambda - b)), a = 0.0072 / nm and b = 538 nm in the paper,
 *             a smooth bump over the CMFs. Its CDF is a tanh, so the inverse is exact:
 *               t(l) = tanh(a (l - b)),  CDF(l) = (t(l) - t(lo)) / (t(hi) - t(lo))
 *               l(u) = b + atanh(t(lo) + u (t(hi) - t(lo))) / a
 *               p(l) = a (1 - t(l)^2) / (t(hi) - t(lo))
 *   hero      Wilkie, Nawaz, Droske, Weidlich and Hanika 2014, "Hero wavelength
 *             spectral sampling": wavelength j of n is the hero rotated by j / n of
 *             the range, wrapped into [lo, hi).
 *
 * The tabulated density (any weight) reads the 2D sampler's tables; see
 * core/alwan_importance_sampling_reader.inc.
 */

/* A uniform u in [0, 1] to a wavelength in [lo, hi], uniformly. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wavelength_uniform_sample)(ALWAN_CORE_T u, ALWAN_CORE_T lo, ALWAN_CORE_T hi) {
    return lo + u * (hi - lo);
}

/* The uniform density at lambda: 1 / (hi - lo) inside [lo, hi], 0 outside. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wavelength_uniform_pdf)(ALWAN_CORE_T lambda, ALWAN_CORE_T lo, ALWAN_CORE_T hi) {
    const ALWAN_CORE_T inside = ALWAN_CORE_SELECT(lambda >= lo, ALWAN_CORE_ONE, ALWAN_CORE_ZERO)
                              * ALWAN_CORE_SELECT(lambda <= hi, ALWAN_CORE_ONE, ALWAN_CORE_ZERO);
    return inside / (hi - lo);
}

/* The visible density's inverse CDF at u in [0, 1]; t0 = tanh(a (lo - b)) and
 * t1 = tanh(a (hi - b)) are the caller's (they depend only on the range). The result is
 * held to [lo, hi] against rounding at the ends. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wavelength_visible_sample)(ALWAN_CORE_T u, ALWAN_CORE_T lo, ALWAN_CORE_T hi,
                                                                           ALWAN_CORE_T a, ALWAN_CORE_T b,
                                                                           ALWAN_CORE_T t0, ALWAN_CORE_T t1) {
    const ALWAN_CORE_T x = t0 + u * (t1 - t0);
    /* atanh(x) = ln((1 + x) / (1 - x)) / 2; |x| <= |t0|, |t1| < 1 */
    const ALWAN_CORE_T l = b + ALWAN_CORE_LITERAL(0.5) * ALWAN_CORE_LN((ALWAN_CORE_ONE + x) / (ALWAN_CORE_ONE - x)) / a;
    const ALWAN_CORE_T l_lo = ALWAN_CORE_SELECT(l < lo, lo, l);
    return ALWAN_CORE_SELECT(l_lo > hi, hi, l_lo);
}

/* The visible density at lambda: a (1 - tanh^2(a (lambda - b))) / (t1 - t0) inside
 * [lo, hi], 0 outside. 1 - tanh^2 is sech^2. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wavelength_visible_pdf)(ALWAN_CORE_T lambda, ALWAN_CORE_T lo, ALWAN_CORE_T hi,
                                                                        ALWAN_CORE_T a, ALWAN_CORE_T b,
                                                                        ALWAN_CORE_T t0, ALWAN_CORE_T t1) {
    const ALWAN_CORE_T t = ALWAN_CORE_TANH(a * (lambda - b));
    const ALWAN_CORE_T inside = ALWAN_CORE_SELECT(lambda >= lo, ALWAN_CORE_ONE, ALWAN_CORE_ZERO)
                              * ALWAN_CORE_SELECT(lambda <= hi, ALWAN_CORE_ONE, ALWAN_CORE_ZERO);
    return inside * a * (ALWAN_CORE_ONE - t * t) / (t1 - t0);
}

/* The visible density's CDF at lambda, the u that sample turns into lambda; held to
 * [0, 1]. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wavelength_visible_cdf)(ALWAN_CORE_T lambda, ALWAN_CORE_T a, ALWAN_CORE_T b,
                                                                        ALWAN_CORE_T t0, ALWAN_CORE_T t1) {
    const ALWAN_CORE_T c = (ALWAN_CORE_TANH(a * (lambda - b)) - t0) / (t1 - t0);
    const ALWAN_CORE_T c_lo = ALWAN_CORE_SELECT(c < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, c);
    return ALWAN_CORE_SELECT(c_lo > ALWAN_CORE_ONE, ALWAN_CORE_ONE, c_lo);
}

/* Hero rotation: wavelength j of n, the hero moved by j / n of the range and wrapped into
 * [lo, hi). j = 0 is the hero itself (up to the wrap of a hero at hi). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wavelength_hero_rotate)(ALWAN_CORE_T hero, ALWAN_CORE_T lo, ALWAN_CORE_T hi,
                                                                        int j, int n) {
    const ALWAN_CORE_T range = hi - lo;
    ALWAN_CORE_T s = (hero - lo) / range + (ALWAN_CORE_T)j / (ALWAN_CORE_T)n;
    s = s - ALWAN_CORE_FLOOR(s);
    return lo + s * range;
}
