/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Hero Wavelength Sampling for spectral rendering
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Wilkie et al. (2014), using Wyman (2013) analytic CMF fit
 */

#ifndef ALWAN_HERO_WAVELENGTH_CORE_H
#define ALWAN_HERO_WAVELENGTH_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_hero_wavelength_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_hero_wavelength_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_wyman_xbar_v(alwan_scalar lambda) {
    alwan_scalar t1 = (lambda - ALWAN_LITERAL(442.0)) *
        ALWAN_SELECT(lambda < ALWAN_LITERAL(442.0),
                     ALWAN_LITERAL(0.0624), ALWAN_LITERAL(0.0374));
    alwan_scalar t2 = (lambda - ALWAN_LITERAL(599.8)) *
        ALWAN_SELECT(lambda < ALWAN_LITERAL(599.8),
                     ALWAN_LITERAL(0.0264), ALWAN_LITERAL(0.0323));
    alwan_scalar t3 = (lambda - ALWAN_LITERAL(501.1)) *
        ALWAN_SELECT(lambda < ALWAN_LITERAL(501.1),
                     ALWAN_LITERAL(0.0490), ALWAN_LITERAL(0.0382));

    return ALWAN_LITERAL(0.362) * ALWAN_EXP(ALWAN_LITERAL(-0.5) * t1 * t1)
         + ALWAN_LITERAL(1.056) * ALWAN_EXP(ALWAN_LITERAL(-0.5) * t2 * t2)
         - ALWAN_LITERAL(0.065) * ALWAN_EXP(ALWAN_LITERAL(-0.5) * t3 * t3);
}

ALWAN_INLINE alwan_scalar alwan_wyman_ybar_v(alwan_scalar lambda) {
    alwan_scalar t1 = (lambda - ALWAN_LITERAL(568.8)) *
        ALWAN_SELECT(lambda < ALWAN_LITERAL(568.8),
                     ALWAN_LITERAL(0.0213), ALWAN_LITERAL(0.0247));
    alwan_scalar t2 = (lambda - ALWAN_LITERAL(530.9)) *
        ALWAN_SELECT(lambda < ALWAN_LITERAL(530.9),
                     ALWAN_LITERAL(0.0613), ALWAN_LITERAL(0.0322));

    return ALWAN_LITERAL(0.821) * ALWAN_EXP(ALWAN_LITERAL(-0.5) * t1 * t1)
         + ALWAN_LITERAL(0.286) * ALWAN_EXP(ALWAN_LITERAL(-0.5) * t2 * t2);
}

ALWAN_INLINE alwan_scalar alwan_wyman_zbar_v(alwan_scalar lambda) {
    alwan_scalar t1 = (lambda - ALWAN_LITERAL(437.0)) *
        ALWAN_SELECT(lambda < ALWAN_LITERAL(437.0),
                     ALWAN_LITERAL(0.0845), ALWAN_LITERAL(0.0278));
    alwan_scalar t2 = (lambda - ALWAN_LITERAL(459.0)) *
        ALWAN_SELECT(lambda < ALWAN_LITERAL(459.0),
                     ALWAN_LITERAL(0.0385), ALWAN_LITERAL(0.0725));

    return ALWAN_LITERAL(1.217) * ALWAN_EXP(ALWAN_LITERAL(-0.5) * t1 * t1)
         + ALWAN_LITERAL(0.681) * ALWAN_EXP(ALWAN_LITERAL(-0.5) * t2 * t2);
}

ALWAN_INLINE alwan_xyz alwan_hero_wavelength_to_xyz_v(alwan_scalar lambda) {
    alwan_xyz result;
    result.x = alwan_wyman_xbar_v(lambda);
    result.y = alwan_wyman_ybar_v(lambda);
    result.z = alwan_wyman_zbar_v(lambda);
    return result;
}

ALWAN_INLINE alwan_scalar alwan_hero_wavelength_pdf_v(alwan_scalar lambda) {
    ALWAN_UNUSED(lambda);
    return ALWAN_ONE / ALWAN_LITERAL(400.0);
}

ALWAN_INLINE alwan_scalar alwan_hero_wavelength_sample_v(alwan_scalar u) {
    return ALWAN_LITERAL(380.0) + u * ALWAN_LITERAL(400.0);
}

ALWAN_INLINE alwan_scalar alwan_hero_wavelength_stratified_v(
    alwan_scalar hero,
    int i,
    int N) {
    alwan_scalar range = ALWAN_LITERAL(400.0);
    alwan_scalar offset = (alwan_scalar)i / (alwan_scalar)N * range;
    alwan_scalar lambda = hero + offset;
    lambda = ALWAN_SELECT(lambda > ALWAN_LITERAL(780.0),
                           lambda - range, lambda);
    return lambda;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_HERO_WAVELENGTH_CORE_H */
