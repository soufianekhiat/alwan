/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Jzazbz & JzCzhz Color Spaces (HDR Perceptual) (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Safdar et al. (2017)
 */

/* ----------------------------------------------------------------
 * Jzazbz Constants (Safdar 2017)
 * ---------------------------------------------------------------- */

/* Chromatic adaptation coefficients */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_B) = ALWAN_CORE_LITERAL(1.15);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_G) = ALWAN_CORE_LITERAL(0.66);

/* Perceptual quantizer (PQ) constants from ST 2084 */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_C1) = ALWAN_CORE_LITERAL(0.8359375);              /* 3424/4096 */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_C2) = ALWAN_CORE_LITERAL(18.8515625);             /* 2413/128 */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_C3) = ALWAN_CORE_LITERAL(18.6875);                /* 2392/128 */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_N)  = ALWAN_CORE_LITERAL(0.1593017578125);        /* 2610/16384 */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_P)  = ALWAN_CORE_LITERAL(134.034375);             /* 1.7 * 2523/32 */

/* Lightness transformation constants */
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_D)  = ALWAN_CORE_LITERAL(-0.56);
static ALWAN_CORE_T const ALWAN_CORE_FNLIT(JZAZBZ_V_D0) = ALWAN_CORE_LITERAL(1.6295499532821566e-11);

/* ----------------------------------------------------------------
 * Jzazbz Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(JZAZBZ_V_XYZ_TO_LMS) = {{
#include "../data/matrices/jzazbz_xyz_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(JZAZBZ_V_LMS_TO_XYZ) = {{
#include "../data/matrices/jzazbz_lms_to_xyz.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(JZAZBZ_V_LMS_P_TO_IZAZBZ) = {{
#include "../data/matrices/jzazbz_lms_p_to_izazbz.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(JZAZBZ_V_IZAZBZ_TO_LMS_P) = {{
#include "../data/matrices/jzazbz_izazbz_to_lms_p.csv"
}};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Helper Functions: PQ Transfer Functions
 * ---------------------------------------------------------------- */

/* PQ OETF: linear -> encoded (using PQ-like formula with Jzazbz constants) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(pq_jz_oetf)(ALWAN_CORE_T lin) {
    ALWAN_CORE_T linear_n = ALWAN_CORE_POW(ALWAN_CORE_SELECT(lin <= ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), lin) / ALWAN_CORE_LITERAL(10000.0), ALWAN_CORE_FNLIT(JZAZBZ_V_N));
    ALWAN_CORE_T numerator = ALWAN_CORE_FNLIT(JZAZBZ_V_C1) + ALWAN_CORE_FNLIT(JZAZBZ_V_C2) * linear_n;
    ALWAN_CORE_T denominator = ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNLIT(JZAZBZ_V_C3) * linear_n;
    return ALWAN_CORE_SELECT(lin <= ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0),
                        ALWAN_CORE_POW(numerator / denominator, ALWAN_CORE_FNLIT(JZAZBZ_V_P)));
}

/* PQ EOTF: encoded -> linear (inverse of PQ OETF) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(pq_jz_eotf)(ALWAN_CORE_T encoded) {
    ALWAN_CORE_T enc = ALWAN_CORE_SELECT(encoded <= ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), encoded);
    ALWAN_CORE_T encoded_p = ALWAN_CORE_POW(enc, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_FNLIT(JZAZBZ_V_P));
    ALWAN_CORE_T numerator = encoded_p - ALWAN_CORE_FNLIT(JZAZBZ_V_C1);
    ALWAN_CORE_T denominator = ALWAN_CORE_FNLIT(JZAZBZ_V_C2) - ALWAN_CORE_FNLIT(JZAZBZ_V_C3) * encoded_p;
    ALWAN_CORE_T ratio = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(denominator) < ALWAN_CORE_EPSILON, ALWAN_CORE_LITERAL(0.0), numerator / denominator);
    ratio = ALWAN_CORE_SELECT(ratio < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), ratio);
    return ALWAN_CORE_SELECT(encoded <= ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0),
                        ALWAN_CORE_LITERAL(10000.0) * ALWAN_CORE_POW(ratio, ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_FNLIT(JZAZBZ_V_N)));
}

/* ----------------------------------------------------------------
 * XYZ -> Jzazbz (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_JZAZBZ ALWAN_CORE_FNVLIT(alwan_xyz_to_jzazbz)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_JZAZBZ result;

    /* Step 1: Chromatic adaptation (D65) */
    ALWAN_CORE_T xa = ALWAN_CORE_FNLIT(JZAZBZ_V_B) * xyz.x - (ALWAN_CORE_FNLIT(JZAZBZ_V_B) - ALWAN_CORE_LITERAL(1.0)) * xyz.z;
    ALWAN_CORE_T ya = ALWAN_CORE_FNLIT(JZAZBZ_V_G) * xyz.y - (ALWAN_CORE_FNLIT(JZAZBZ_V_G) - ALWAN_CORE_LITERAL(1.0)) * xyz.x;
    ALWAN_CORE_T za = xyz.z;

    /* Step 2: XYZ (adapted) -> LMS via matrix */
    ALWAN_CORE_VEC3 xa_v = {{xa, ya, za}};
    ALWAN_CORE_VEC3 lms_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(JZAZBZ_V_XYZ_TO_LMS), xa_v);
    ALWAN_CORE_T l = lms_v.v[0];
    ALWAN_CORE_T m = lms_v.v[1];
    ALWAN_CORE_T s = lms_v.v[2];

    /* Step 3: PQ OETF: LMS -> LMS' */
    ALWAN_CORE_T lp = ALWAN_CORE_FNVLIT(pq_jz_oetf)(l);
    ALWAN_CORE_T mp = ALWAN_CORE_FNVLIT(pq_jz_oetf)(m);
    ALWAN_CORE_T sp = ALWAN_CORE_FNVLIT(pq_jz_oetf)(s);

    /* Step 4: LMS' -> Izazbz via matrix */
    ALWAN_CORE_VEC3 lms_p_v = {{lp, mp, sp}};
    ALWAN_CORE_VEC3 izazbz_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(JZAZBZ_V_LMS_P_TO_IZAZBZ), lms_p_v);
    ALWAN_CORE_T iz = izazbz_v.v[0];
    ALWAN_CORE_T az = izazbz_v.v[1];
    ALWAN_CORE_T bz = izazbz_v.v[2];

    /* Step 5: Calculate Jz from Iz (clamp to non-negative: lightness cannot be negative) */
    ALWAN_CORE_T jz_raw = ((ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNLIT(JZAZBZ_V_D)) * iz) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNLIT(JZAZBZ_V_D) * iz) - ALWAN_CORE_FNLIT(JZAZBZ_V_D0);
    result.Jz = ALWAN_CORE_SELECT(jz_raw < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, jz_raw);
    result.az = az;
    result.bz = bz;

    return result;
}

/* ----------------------------------------------------------------
 * Jzazbz -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_jzazbz_to_xyz)(ALWAN_CORE_JZAZBZ jzazbz) {
    ALWAN_CORE_XYZ result;

    /* Step 1: Recover Iz from Jz */
    ALWAN_CORE_T jz = jzazbz.Jz;
    ALWAN_CORE_T iz = (jz + ALWAN_CORE_FNLIT(JZAZBZ_V_D0)) / (ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_FNLIT(JZAZBZ_V_D) - ALWAN_CORE_FNLIT(JZAZBZ_V_D) * (jz + ALWAN_CORE_FNLIT(JZAZBZ_V_D0)));

    /* Step 2: Construct Izazbz */
    ALWAN_CORE_T i = iz;
    ALWAN_CORE_T a = jzazbz.az;
    ALWAN_CORE_T b = jzazbz.bz;

    /* Step 3: Izazbz -> LMS' via matrix */
    ALWAN_CORE_VEC3 izazbz_v = {{i, a, b}};
    ALWAN_CORE_VEC3 lms_p_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(JZAZBZ_V_IZAZBZ_TO_LMS_P), izazbz_v);
    ALWAN_CORE_T lp = lms_p_v.v[0];
    ALWAN_CORE_T mp = lms_p_v.v[1];
    ALWAN_CORE_T sp = lms_p_v.v[2];

    /* Step 4: PQ EOTF: LMS' -> LMS */
    ALWAN_CORE_T l = ALWAN_CORE_FNVLIT(pq_jz_eotf)(lp);
    ALWAN_CORE_T m = ALWAN_CORE_FNVLIT(pq_jz_eotf)(mp);
    ALWAN_CORE_T s = ALWAN_CORE_FNVLIT(pq_jz_eotf)(sp);

    /* Step 5: LMS -> XYZ (adapted) via matrix */
    ALWAN_CORE_VEC3 lms_v = {{l, m, s}};
    ALWAN_CORE_VEC3 xa_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(JZAZBZ_V_LMS_TO_XYZ), lms_v);
    ALWAN_CORE_T xa = xa_v.v[0];
    ALWAN_CORE_T ya = xa_v.v[1];
    ALWAN_CORE_T za = xa_v.v[2];

    /* Step 6: Inverse chromatic adaptation */
    result.x = (xa + (ALWAN_CORE_FNLIT(JZAZBZ_V_B) - ALWAN_CORE_LITERAL(1.0)) * za) / ALWAN_CORE_FNLIT(JZAZBZ_V_B);
    result.y = (ya + (ALWAN_CORE_FNLIT(JZAZBZ_V_G) - ALWAN_CORE_LITERAL(1.0)) * result.x) / ALWAN_CORE_FNLIT(JZAZBZ_V_G);
    result.z = za;

    return result;
}

/* ----------------------------------------------------------------
 * Jzazbz -> JzCzhz (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_JZCZHZ ALWAN_CORE_FNVLIT(alwan_jzazbz_to_jzczhz)(ALWAN_CORE_JZAZBZ jzazbz) {
    ALWAN_CORE_JZCZHZ result;

    /* Jz stays same */
    result.Jz = jzazbz.Jz;

    /* Cz = sqrt(az*az + bz*bz) */
    result.Cz = ALWAN_CORE_SQRT(jzazbz.az * jzazbz.az + jzazbz.bz * jzazbz.bz);

    /* hz = atan2(bz, az) */
    result.hz = ALWAN_CORE_ATAN2(jzazbz.bz, jzazbz.az);

    return result;
}

/* ----------------------------------------------------------------
 * JzCzhz -> Jzazbz (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_JZAZBZ ALWAN_CORE_FNVLIT(alwan_jzczhz_to_jzazbz)(ALWAN_CORE_JZCZHZ jzczhz) {
    ALWAN_CORE_JZAZBZ result;

    /* Jz stays same */
    result.Jz = jzczhz.Jz;

    /* az = Cz * cos(hz) */
    result.az = jzczhz.Cz * ALWAN_CORE_COS(jzczhz.hz);

    /* bz = Cz * sin(hz) */
    result.bz = jzczhz.Cz * ALWAN_CORE_SIN(jzczhz.hz);

    return result;
}
