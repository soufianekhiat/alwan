/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Jzazbz & JzCzhz Color Spaces (HDR Perceptual)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Safdar et al. (2017)
 */

#ifndef ALWAN_JZAZBZ_CORE_H
#define ALWAN_JZAZBZ_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_jzazbz_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_jzazbz_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

static const alwan_scalar JZAZBZ_V_B = ALWAN_LITERAL(1.15);
static const alwan_scalar JZAZBZ_V_G = ALWAN_LITERAL(0.66);
static const alwan_scalar JZAZBZ_V_C1 = ALWAN_LITERAL(0.8359375);
static const alwan_scalar JZAZBZ_V_C2 = ALWAN_LITERAL(18.8515625);
static const alwan_scalar JZAZBZ_V_C3 = ALWAN_LITERAL(18.6875);
static const alwan_scalar JZAZBZ_V_N  = ALWAN_LITERAL(0.1593017578125);
static const alwan_scalar JZAZBZ_V_P  = ALWAN_LITERAL(134.034375);
static const alwan_scalar JZAZBZ_V_D  = ALWAN_LITERAL(-0.56);
static const alwan_scalar JZAZBZ_V_D0 = ALWAN_LITERAL(1.6295499532821566e-11);

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 JZAZBZ_V_XYZ_TO_LMS = {{
#include "../data/matrices/jzazbz_xyz_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 JZAZBZ_V_LMS_TO_XYZ = {{
#include "../data/matrices/jzazbz_lms_to_xyz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 JZAZBZ_V_LMS_P_TO_IZAZBZ = {{
#include "../data/matrices/jzazbz_lms_p_to_izazbz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 JZAZBZ_V_IZAZBZ_TO_LMS_P = {{
#include "../data/matrices/jzazbz_izazbz_to_lms_p.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_scalar pq_jz_oetf_v(alwan_scalar lin) {
    alwan_scalar linear_n = ALWAN_POW(ALWAN_SELECT(lin <= ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), lin) / ALWAN_LITERAL(10000.0), JZAZBZ_V_N);
    alwan_scalar numerator = JZAZBZ_V_C1 + JZAZBZ_V_C2 * linear_n;
    alwan_scalar denominator = ALWAN_LITERAL(1.0) + JZAZBZ_V_C3 * linear_n;
    return ALWAN_SELECT(lin <= ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
                        ALWAN_POW(numerator / denominator, JZAZBZ_V_P));
}

ALWAN_INLINE alwan_scalar pq_jz_eotf_v(alwan_scalar encoded) {
    alwan_scalar enc = ALWAN_SELECT(encoded <= ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), encoded);
    alwan_scalar encoded_p = ALWAN_POW(enc, ALWAN_LITERAL(1.0) / JZAZBZ_V_P);
    alwan_scalar numerator = encoded_p - JZAZBZ_V_C1;
    alwan_scalar denominator = JZAZBZ_V_C2 - JZAZBZ_V_C3 * encoded_p;
    alwan_scalar ratio = ALWAN_SELECT(ALWAN_ABS(denominator) < ALWAN_EPSILON, ALWAN_LITERAL(0.0), numerator / denominator);
    ratio = ALWAN_SELECT(ratio < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ratio);
    return ALWAN_SELECT(encoded <= ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0),
                        ALWAN_LITERAL(10000.0) * ALWAN_POW(ratio, ALWAN_LITERAL(1.0) / JZAZBZ_V_N));
}

ALWAN_INLINE alwan_jzazbz alwan_xyz_to_jzazbz_v(alwan_xyz xyz) {
    alwan_jzazbz result;

    /* Step 1: Chromatic adaptation (D65) */
    alwan_scalar xa = JZAZBZ_V_B * xyz.x - (JZAZBZ_V_B - ALWAN_LITERAL(1.0)) * xyz.z;
    alwan_scalar ya = JZAZBZ_V_G * xyz.y - (JZAZBZ_V_G - ALWAN_LITERAL(1.0)) * xyz.x;
    alwan_scalar za = xyz.z;

    /* Step 2: XYZ (adapted) -> LMS via matrix */
    alwan_vec3 xa_v = {{xa, ya, za}};
    alwan_vec3 lms_v = alwan_mat3_mulv_v(JZAZBZ_V_XYZ_TO_LMS, xa_v);
    alwan_scalar l = lms_v.v[0];
    alwan_scalar m = lms_v.v[1];
    alwan_scalar s = lms_v.v[2];

    /* Step 3: PQ OETF: LMS -> LMS' */
    alwan_scalar lp = pq_jz_oetf_v(l);
    alwan_scalar mp = pq_jz_oetf_v(m);
    alwan_scalar sp = pq_jz_oetf_v(s);

    /* Step 4: LMS' -> Izazbz via matrix */
    alwan_vec3 lms_p_v = {{lp, mp, sp}};
    alwan_vec3 izazbz_v = alwan_mat3_mulv_v(JZAZBZ_V_LMS_P_TO_IZAZBZ, lms_p_v);
    alwan_scalar iz = izazbz_v.v[0];
    alwan_scalar az = izazbz_v.v[1];
    alwan_scalar bz = izazbz_v.v[2];

    /* Step 5: Calculate Jz from Iz (clamp to non-negative: lightness cannot be negative) */
    alwan_scalar jz_raw = ((ALWAN_LITERAL(1.0) + JZAZBZ_V_D) * iz) / (ALWAN_LITERAL(1.0) + JZAZBZ_V_D * iz) - JZAZBZ_V_D0;
    result.Jz = ALWAN_SELECT(jz_raw < ALWAN_ZERO, ALWAN_ZERO, jz_raw);
    result.az = az;
    result.bz = bz;

    return result;
}

ALWAN_INLINE alwan_xyz alwan_jzazbz_to_xyz_v(alwan_jzazbz jzazbz) {
    alwan_xyz result;

    /* Step 1: Recover Iz from Jz */
    alwan_scalar jz = jzazbz.Jz;
    alwan_scalar iz = (jz + JZAZBZ_V_D0) / (ALWAN_LITERAL(1.0) + JZAZBZ_V_D - JZAZBZ_V_D * (jz + JZAZBZ_V_D0));

    /* Step 2: Construct Izazbz */
    alwan_scalar i = iz;
    alwan_scalar a = jzazbz.az;
    alwan_scalar b = jzazbz.bz;

    /* Step 3: Izazbz -> LMS' via matrix */
    alwan_vec3 izazbz_v = {{i, a, b}};
    alwan_vec3 lms_p_v = alwan_mat3_mulv_v(JZAZBZ_V_IZAZBZ_TO_LMS_P, izazbz_v);
    alwan_scalar lp = lms_p_v.v[0];
    alwan_scalar mp = lms_p_v.v[1];
    alwan_scalar sp = lms_p_v.v[2];

    /* Step 4: PQ EOTF: LMS' -> LMS */
    alwan_scalar l = pq_jz_eotf_v(lp);
    alwan_scalar m = pq_jz_eotf_v(mp);
    alwan_scalar s = pq_jz_eotf_v(sp);

    /* Step 5: LMS -> XYZ (adapted) via matrix */
    alwan_vec3 lms_v = {{l, m, s}};
    alwan_vec3 xa_v = alwan_mat3_mulv_v(JZAZBZ_V_LMS_TO_XYZ, lms_v);
    alwan_scalar xa = xa_v.v[0];
    alwan_scalar ya = xa_v.v[1];
    alwan_scalar za = xa_v.v[2];

    /* Step 6: Inverse chromatic adaptation */
    result.x = (xa + (JZAZBZ_V_B - ALWAN_LITERAL(1.0)) * za) / JZAZBZ_V_B;
    result.y = (ya + (JZAZBZ_V_G - ALWAN_LITERAL(1.0)) * result.x) / JZAZBZ_V_G;
    result.z = za;

    return result;
}

ALWAN_INLINE alwan_jzczhz alwan_jzazbz_to_jzczhz_v(alwan_jzazbz jzazbz) {
    alwan_jzczhz result;
    result.Jz = jzazbz.Jz;
    result.Cz = ALWAN_SQRT(jzazbz.az * jzazbz.az + jzazbz.bz * jzazbz.bz);
    result.hz = ALWAN_ATAN2(jzazbz.bz, jzazbz.az);
    return result;
}

ALWAN_INLINE alwan_jzazbz alwan_jzczhz_to_jzazbz_v(alwan_jzczhz jzczhz) {
    alwan_jzazbz result;
    result.Jz = jzczhz.Jz;
    result.az = jzczhz.Cz * ALWAN_COS(jzczhz.hz);
    result.bz = jzczhz.Cz * ALWAN_SIN(jzczhz.hz);
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_JZAZBZ_CORE_H */
