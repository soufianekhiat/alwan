/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only spectral primitives (per-element math)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_SPD_CORE_H
#define ALWAN_SPD_CORE_H

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
#include "alwan_spd_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_spd_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_scalar spd_planck_radiance_v(alwan_scalar wavelength_m,
                                                 alwan_scalar temperature_K) {
    /* c1 = 2*pi*h*c^2, so c1/(lambda^5 (e^x - 1)) is spectral radiant exitance.
     * This returns radiance, which for a Lambertian emitter is exitance / pi. */
    const alwan_scalar c1 = ALWAN_LITERAL(3.741771e-16);
    const alwan_scalar c2 = ALWAN_LITERAL(1.4388e-2);
    alwan_scalar lambda5 = wavelength_m * wavelength_m * wavelength_m
                         * wavelength_m * wavelength_m;
    alwan_scalar exponent = c2 / (wavelength_m * temperature_K);
    alwan_scalar denominator = lambda5 * (ALWAN_EXP(exponent) - ALWAN_LITERAL(1.0));
    /* 1e-37, not 1e-100. This template is instantiated at both precisions, and
     * 1e-100f is not a representable float: the f32 pass got an out-of-range
     * constant, which is undefined behaviour and in practice folds to zero, so
     * the guard was dead there and a zero denominator reached the division.
     * nvcc rejects the literal outright, which is how this surfaced.
     *
     * 1e-37 is representable in both and is not reachable by any real
     * temperature. The denominator is lambda^5 (exp(c2/(lambda T)) - 1), and
     * lambda^5 is at least 6e-33 over the visible range, so falling below 1e-37
     * needs exp(x) - 1 < 1.6e-5, which needs T above 2.5e9 K. */
    if (ALWAN_ABS(denominator) < ALWAN_LITERAL(1e-37)) return ALWAN_LITERAL(0.0);
    return c1 / (ALWAN_PI * denominator);
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_SPD_CORE_H */
