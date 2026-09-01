/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only DIN99 Family (DIN99, DIN99b, DIN99c, DIN99d)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: DIN 6176:2001-03, ASTM D2244-07
 */

#ifndef ALWAN_DIN99_CORE_H
#define ALWAN_DIN99_CORE_H

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
#include "alwan_din99_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_din99_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

static const alwan_scalar ALWAN_DIN99_COEFFS[4][8] = {
    /* DIN99 / ASTM D2244-07 */
    {
        ALWAN_LITERAL(105.509),
        ALWAN_LITERAL(0.0158),
        ALWAN_LITERAL(16.0),
        ALWAN_LITERAL(0.7),
        ALWAN_LITERAL(1.0),
        ALWAN_LITERAL(0.045),
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.045)
    },
    /* DIN99b */
    {
        ALWAN_LITERAL(303.67),
        ALWAN_LITERAL(0.0039),
        ALWAN_LITERAL(26.0),
        ALWAN_LITERAL(0.83),
        ALWAN_LITERAL(23.0),
        ALWAN_LITERAL(0.075),
        ALWAN_LITERAL(26.0),
        ALWAN_LITERAL(1.0)
    },
    /* DIN99c */
    {
        ALWAN_LITERAL(317.65),
        ALWAN_LITERAL(0.0037),
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(0.94),
        ALWAN_LITERAL(23.0),
        ALWAN_LITERAL(0.066),
        ALWAN_LITERAL(0.0),
        ALWAN_LITERAL(1.0)
    },
    /* DIN99d */
    {
        ALWAN_LITERAL(325.22),
        ALWAN_LITERAL(0.0036),
        ALWAN_LITERAL(50.0),
        ALWAN_LITERAL(1.14),
        ALWAN_LITERAL(22.5),
        ALWAN_LITERAL(0.06),
        ALWAN_LITERAL(50.0),
        ALWAN_LITERAL(1.0)
    }
};

ALWAN_DIAG_POP

ALWAN_INLINE alwan_din99 alwan_lab_to_din99_v(alwan_lab lab, int variant) {
    alwan_din99 result;
    int v = ALWAN_SELECT(variant < 0, 0, ALWAN_SELECT(variant > 3, 3, variant));
    const alwan_scalar k_E = ALWAN_LITERAL(1.0);
    const alwan_scalar k_CH = ALWAN_LITERAL(1.0);

    result.L99 = ALWAN_DIN99_COEFFS[v][0] * ALWAN_LN(ALWAN_LITERAL(1.0) + ALWAN_DIN99_COEFFS[v][1] * lab.L) * k_E;
    alwan_scalar lab_chroma_sq = lab.a * lab.a + lab.b * lab.b;
    alwan_scalar c3_rad = ALWAN_DIN99_COEFFS[v][2] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar c7_rad = ALWAN_DIN99_COEFFS[v][6] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);
    alwan_scalar e = cos_c3 * lab.a + sin_c3 * lab.b;
    alwan_scalar f = ALWAN_DIN99_COEFFS[v][3] * (-sin_c3 * lab.a + cos_c3 * lab.b);
    alwan_scalar G = ALWAN_SQRT(e * e + f * f);
    alwan_scalar h_ef = ALWAN_ATAN2(f, e) + c7_rad;
    alwan_scalar C99 = (ALWAN_DIN99_COEFFS[v][4] * ALWAN_LN(ALWAN_LITERAL(1.0) + ALWAN_DIN99_COEFFS[v][5] * G)) / (ALWAN_DIN99_COEFFS[v][7] * k_CH * k_E);
    result.a99 = ALWAN_SELECT(lab_chroma_sq < ALWAN_LITERAL(1e-12),
                              ALWAN_LITERAL(0.0), C99 * ALWAN_COS(h_ef));
    result.b99 = ALWAN_SELECT(lab_chroma_sq < ALWAN_LITERAL(1e-12),
                              ALWAN_LITERAL(0.0), C99 * ALWAN_SIN(h_ef));
    return result;
}

ALWAN_INLINE alwan_lab alwan_din99_to_lab_v(alwan_din99 din99, int variant) {
    alwan_lab result;
    int v = ALWAN_SELECT(variant < 0, 0, ALWAN_SELECT(variant > 3, 3, variant));
    const alwan_scalar k_E = ALWAN_LITERAL(1.0);
    const alwan_scalar k_CH = ALWAN_LITERAL(1.0);
    alwan_scalar c3_rad = ALWAN_DIN99_COEFFS[v][2] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar c7_rad = ALWAN_DIN99_COEFFS[v][6] * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar cos_c3 = ALWAN_COS(c3_rad);
    alwan_scalar sin_c3 = ALWAN_SIN(c3_rad);
    alwan_scalar h99 = ALWAN_ATAN2(din99.b99, din99.a99) - c7_rad;
    alwan_scalar C99 = ALWAN_SQRT(din99.a99 * din99.a99 + din99.b99 * din99.b99);
    alwan_scalar G = (ALWAN_EXP((ALWAN_DIN99_COEFFS[v][7] / ALWAN_DIN99_COEFFS[v][4]) * C99 * k_CH * k_E) - ALWAN_LITERAL(1.0)) / ALWAN_DIN99_COEFFS[v][5];
    alwan_scalar e = G * ALWAN_COS(h99);
    alwan_scalar f = G * ALWAN_SIN(h99);
    result.a = e * cos_c3 - (f / ALWAN_DIN99_COEFFS[v][3]) * sin_c3;
    result.b = e * sin_c3 + (f / ALWAN_DIN99_COEFFS[v][3]) * cos_c3;
    result.L = (ALWAN_EXP(din99.L99 * k_E / ALWAN_DIN99_COEFFS[v][0]) - ALWAN_LITERAL(1.0)) / ALWAN_DIN99_COEFFS[v][1];
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_DIN99_CORE_H */
