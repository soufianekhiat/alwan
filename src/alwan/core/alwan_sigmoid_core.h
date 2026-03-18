/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Generic Sigmoid Utility
 * Parameterizable piece-wise power sigmoid with smooth join at pivot.
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_SIGMOID_CORE_H
#define ALWAN_SIGMOID_CORE_H

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
#include "alwan_sigmoid_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_sigmoid_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_scalar alwan_sigmoid_v(alwan_scalar x,
                                           alwan_scalar pivot,
                                           alwan_scalar slope,
                                           alwan_scalar toe_power,
                                           alwan_scalar shoulder_power) {
    /* Scale input around pivot */
    alwan_scalar scaled = pivot + (x - pivot) * slope;
    alwan_scalar clamped = alwan_clamp(scaled, ALWAN_ZERO, ALWAN_ONE);

    /* Safe division: avoid divide-by-zero when pivot is 0 or 1 */
    alwan_scalar safe_pivot = alwan_clamp(pivot, ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1.0) - ALWAN_LITERAL(1e-10));

    /* Toe region: x < pivot */
    alwan_scalar toe_ratio = clamped / safe_pivot;
    alwan_scalar toe_result = safe_pivot * ALWAN_POW(toe_ratio, toe_power);

    /* Shoulder region: x >= pivot */
    alwan_scalar shoulder_ratio = (ALWAN_ONE - clamped) / (ALWAN_ONE - safe_pivot);
    alwan_scalar shoulder_result = ALWAN_ONE - (ALWAN_ONE - safe_pivot) * ALWAN_POW(shoulder_ratio, shoulder_power);

    return ALWAN_SELECT(clamped < safe_pivot, toe_result, shoulder_result);
}

ALWAN_INLINE alwan_scalar alwan_sigmoid_inv_v(alwan_scalar y,
                                               alwan_scalar pivot,
                                               alwan_scalar slope,
                                               alwan_scalar toe_power,
                                               alwan_scalar shoulder_power) {
    alwan_scalar clamped = alwan_clamp(y, ALWAN_ZERO, ALWAN_ONE);

    alwan_scalar safe_pivot = alwan_clamp(pivot, ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1.0) - ALWAN_LITERAL(1e-10));
    alwan_scalar safe_slope = ALWAN_SELECT(ALWAN_ABS(slope) < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), slope);

    /* Inverse toe: y = pivot * (x/pivot)^p  =>  x = pivot * (y/pivot)^(1/p) */
    alwan_scalar inv_toe_power = ALWAN_ONE / toe_power;
    alwan_scalar toe_x = safe_pivot * ALWAN_POW(clamped / safe_pivot, inv_toe_power);

    /* Inverse shoulder: y = 1-(1-pivot)*((1-x)/(1-pivot))^p  =>  x = 1-(1-pivot)*((1-y)/(1-pivot))^(1/p) */
    alwan_scalar inv_shoulder_power = ALWAN_ONE / shoulder_power;
    alwan_scalar shoulder_x = ALWAN_ONE - (ALWAN_ONE - safe_pivot) *
        ALWAN_POW((ALWAN_ONE - clamped) / (ALWAN_ONE - safe_pivot), inv_shoulder_power);

    alwan_scalar x_scaled = ALWAN_SELECT(clamped < safe_pivot, toe_x, shoulder_x);

    /* Undo the slope scaling: scaled = pivot + (x - pivot) * slope => x = pivot + (scaled - pivot) / slope */
    return pivot + (x_scaled - pivot) / safe_slope;
}

#endif

#endif /* ALWAN_SIGMOID_CORE_H */
