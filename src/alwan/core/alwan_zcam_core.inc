/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ZCAM HDR Color Appearance Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Safdar et al. (2021) "ZCAM, a colour appearance model based on
 * a high dynamic range uniform colour space"
 */

/* ----------------------------------------------------------------
 * ZCAM Result Struct (suffixed per precision)
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T Jz;     /* Lightness */
    ALWAN_CORE_T Cz;     /* Chroma */
    ALWAN_CORE_T hz;     /* Hue angle (degrees) */
    ALWAN_CORE_T Qz;     /* Brightness */
    ALWAN_CORE_T Mz;     /* Colorfulness */
    ALWAN_CORE_T Sz;     /* Saturation */
    ALWAN_CORE_T Vz;     /* Vividness */
    ALWAN_CORE_T Kz;     /* Blackness */
    ALWAN_CORE_T Wz;     /* Whiteness */
} ALWAN_CORE_FNLIT(alwan_zcam_v_correlates);

/* ----------------------------------------------------------------
 * ZCAM Constants
 * ---------------------------------------------------------------- */

#define ZCAM_V_B     ALWAN_CORE_LITERAL(1.15)
#define ZCAM_V_G     ALWAN_CORE_LITERAL(0.66)

#define ZCAM_V_PQ_C1 ALWAN_CORE_LITERAL(0.8359375)
#define ZCAM_V_PQ_C2 ALWAN_CORE_LITERAL(18.8515625)
#define ZCAM_V_PQ_C3 ALWAN_CORE_LITERAL(18.6875)
#define ZCAM_V_PQ_N  ALWAN_CORE_LITERAL(0.1593017578125)
#define ZCAM_V_PQ_P  ALWAN_CORE_LITERAL(134.034375)

#define ZCAM_V_PQ_D  ALWAN_CORE_LITERAL(-0.56)
#define ZCAM_V_PQ_D0 ALWAN_CORE_LITERAL(1.6295499532821566e-11)

/* ----------------------------------------------------------------
 * ZCAM Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_XYZ_TO_LMS) = {{
#include "../data/matrices/jzazbz_xyz_to_lms.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_LMS_P_TO_IZAZBZ) = {{
#include "../data/matrices/jzazbz_lms_p_to_izazbz.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_LMS_TO_XYZ) = {{
#include "../data/matrices/jzazbz_lms_to_xyz.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_IZAZBZ_TO_LMS_P) = {{
#include "../data/matrices/jzazbz_izazbz_to_lms_p.csv"
}};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Local PQ Transfer Functions
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_pq_forward)(ALWAN_CORE_T L) {
    ALWAN_CORE_T L_safe = ALWAN_CORE_SELECT(L <= ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, L);
    ALWAN_CORE_T L_norm = L_safe / ALWAN_CORE_LITERAL(10000.0);
    ALWAN_CORE_T L_pow_n = ALWAN_CORE_POW(L_norm, ZCAM_V_PQ_N);
    ALWAN_CORE_T numerator = ZCAM_V_PQ_C1 + ZCAM_V_PQ_C2 * L_pow_n;
    ALWAN_CORE_T denominator = ALWAN_CORE_ONE + ZCAM_V_PQ_C3 * L_pow_n;
    return ALWAN_CORE_SELECT(L <= ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,
                        ALWAN_CORE_POW(numerator / denominator, ZCAM_V_PQ_P));
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_pq_inverse)(ALWAN_CORE_T E) {
    ALWAN_CORE_T enc = ALWAN_CORE_SELECT(E <= ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, E);
    ALWAN_CORE_T E_pow = ALWAN_CORE_POW(enc, ALWAN_CORE_ONE / ZCAM_V_PQ_P);
    ALWAN_CORE_T numerator = E_pow - ZCAM_V_PQ_C1;
    ALWAN_CORE_T denominator = ZCAM_V_PQ_C2 - ZCAM_V_PQ_C3 * E_pow;
    ALWAN_CORE_T ratio = ALWAN_CORE_SELECT(ALWAN_CORE_ABS(denominator) < ALWAN_CORE_EPSILON,
                                      ALWAN_CORE_ZERO, numerator / denominator);
    ratio = ALWAN_CORE_SELECT(ratio < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ratio);
    return ALWAN_CORE_SELECT(E <= ALWAN_CORE_ZERO, ALWAN_CORE_ZERO,
                        ALWAN_CORE_LITERAL(10000.0) * ALWAN_CORE_POW(ratio, ALWAN_CORE_ONE / ZCAM_V_PQ_N));
}

/* ----------------------------------------------------------------
 * Iz <-> Jz Helpers
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_iz_to_jz)(ALWAN_CORE_T Iz) {
    ALWAN_CORE_T numerator = (ALWAN_CORE_ONE + ZCAM_V_PQ_D) * Iz;
    ALWAN_CORE_T denominator = ALWAN_CORE_ONE + ZCAM_V_PQ_D * Iz;
    return (numerator / denominator) - ZCAM_V_PQ_D0;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_jz_to_iz)(ALWAN_CORE_T Jz) {
    ALWAN_CORE_T Jz_adj = Jz + ZCAM_V_PQ_D0;
    return Jz_adj / ((ALWAN_CORE_ONE + ZCAM_V_PQ_D) - ZCAM_V_PQ_D * Jz_adj);
}

/* ----------------------------------------------------------------
 * Eccentricity Factor
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_eccentricity)(ALWAN_CORE_T h_degrees) {
    ALWAN_CORE_T h_rad = (h_degrees + ALWAN_CORE_LITERAL(89.038)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    return ALWAN_CORE_LITERAL(1.015) + ALWAN_CORE_COS(h_rad);
}

/* ----------------------------------------------------------------
 * ZCAM Forward Transform: XYZ -> Correlates (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) ALWAN_CORE_FNVLIT(alwan_zcam_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ xyz_w,
    ALWAN_CORE_T Fs,
    ALWAN_CORE_T c,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T F,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Y_b,
    ALWAN_CORE_T Y_w) {

    ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) result;

    ALWAN_UNUSED(c); ALWAN_UNUSED(Nc); ALWAN_UNUSED(F);

    /* Step 1: Viewing condition factors */
    ALWAN_CORE_T Fb = ALWAN_CORE_SQRT(Y_b / Y_w);
    ALWAN_CORE_T FL = ALWAN_CORE_LITERAL(0.171) *
                      ALWAN_CORE_POW(La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0)) *
                      (ALWAN_CORE_ONE - ALWAN_CORE_EXP(-ALWAN_CORE_LITERAL(48.0) / ALWAN_CORE_LITERAL(9.0) * La));

    /* Step 2: Chromatic adaptation */
    ALWAN_CORE_T xa = ZCAM_V_B * xyz.x - (ZCAM_V_B - ALWAN_CORE_ONE) * xyz.z;
    ALWAN_CORE_T ya = ZCAM_V_G * xyz.y - (ZCAM_V_G - ALWAN_CORE_ONE) * xyz.x;
    ALWAN_CORE_T za = xyz.z;

    /* Step 3: Adapted XYZ -> LMS */
    ALWAN_CORE_VEC3 in_v = {{xa, ya, za}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_XYZ_TO_LMS), in_v);
    ALWAN_CORE_T l = out_v.v[0];
    ALWAN_CORE_T m = out_v.v[1];
    ALWAN_CORE_T s = out_v.v[2];

    /* Step 4: PQ nonlinearity: LMS -> LMS' */
    ALWAN_CORE_T lp = ALWAN_CORE_FNVLIT(zcam_pq_forward)(l);
    ALWAN_CORE_T mp = ALWAN_CORE_FNVLIT(zcam_pq_forward)(m);
    ALWAN_CORE_T sp = ALWAN_CORE_FNVLIT(zcam_pq_forward)(s);

    /* Step 5: LMS' -> Izazbz */
    ALWAN_CORE_VEC3 in_v2 = {{lp, mp, sp}};
    ALWAN_CORE_VEC3 out_v2 = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_LMS_P_TO_IZAZBZ), in_v2);
    ALWAN_CORE_T Iz = out_v2.v[0];
    ALWAN_CORE_T az = out_v2.v[1];
    ALWAN_CORE_T bz = out_v2.v[2];

    /* Step 6: White point -> Izw */
    ALWAN_CORE_T xaw = ZCAM_V_B * xyz_w.x - (ZCAM_V_B - ALWAN_CORE_ONE) * xyz_w.z;
    ALWAN_CORE_T yaw = ZCAM_V_G * xyz_w.y - (ZCAM_V_G - ALWAN_CORE_ONE) * xyz_w.x;
    ALWAN_CORE_T zaw = xyz_w.z;

    ALWAN_CORE_VEC3 in_vw = {{xaw, yaw, zaw}};
    ALWAN_CORE_VEC3 out_vw = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_XYZ_TO_LMS), in_vw);
    ALWAN_CORE_T lw = out_vw.v[0];
    ALWAN_CORE_T mw = out_vw.v[1];
    ALWAN_CORE_T sw = out_vw.v[2];

    ALWAN_CORE_T lwp = ALWAN_CORE_FNVLIT(zcam_pq_forward)(lw);
    ALWAN_CORE_T mwp = ALWAN_CORE_FNVLIT(zcam_pq_forward)(mw);
    ALWAN_CORE_T swp = ALWAN_CORE_FNVLIT(zcam_pq_forward)(sw);

    ALWAN_CORE_VEC3 in_vwp = {{lwp, mwp, swp}};
    ALWAN_CORE_VEC3 out_vwp = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_LMS_P_TO_IZAZBZ), in_vwp);
    ALWAN_CORE_T Izw = out_vwp.v[0];

    /* Step 7: Hue angle (degrees, [0, 360)) */
    ALWAN_CORE_T hz_raw = ALWAN_CORE_ATAN2(bz, az) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.hz = ALWAN_CORE_SELECT(hz_raw < ALWAN_CORE_ZERO, hz_raw + ALWAN_CORE_LITERAL(360.0), hz_raw);

    /* Step 8: Eccentricity factor */
    ALWAN_CORE_T ez = ALWAN_CORE_FNVLIT(zcam_eccentricity)(result.hz);

    /* Step 9: Brightness Qz */
    ALWAN_CORE_T Fb_pow_012 = ALWAN_CORE_POW(Fb, ALWAN_CORE_LITERAL(0.12));
    ALWAN_CORE_T Iz_exp = ALWAN_CORE_LITERAL(1.6) * Fs / Fb_pow_012;
    ALWAN_CORE_T Iz_pow = ALWAN_CORE_POW(Iz, Iz_exp);
    ALWAN_CORE_T Fs_pow_22 = ALWAN_CORE_POW(Fs, ALWAN_CORE_LITERAL(2.2));
    ALWAN_CORE_T Fb_sqrt = ALWAN_CORE_SQRT(Fb);
    ALWAN_CORE_T FL_pow_02 = ALWAN_CORE_POW(FL, ALWAN_CORE_LITERAL(0.2));
    result.Qz = ALWAN_CORE_LITERAL(2700.0) * Iz_pow * Fs_pow_22 * Fb_sqrt * FL_pow_02;

    /* Step 10: White point brightness Qzw */
    ALWAN_CORE_T Izw_pow = ALWAN_CORE_POW(Izw, Iz_exp);
    ALWAN_CORE_T Qzw = ALWAN_CORE_LITERAL(2700.0) * Izw_pow * Fs_pow_22 * Fb_sqrt * FL_pow_02;

    /* Step 11: Lightness Jz */
    result.Jz = ALWAN_CORE_LITERAL(100.0) * (result.Qz / Qzw);

    /* Step 12: Colorfulness Mz */
    ALWAN_CORE_T chroma_component = ALWAN_CORE_POW(az * az + bz * bz, ALWAN_CORE_LITERAL(0.37));
    ALWAN_CORE_T ez_pow = ALWAN_CORE_POW(ez, ALWAN_CORE_LITERAL(0.068));
    ALWAN_CORE_T Izw_pow_078 = ALWAN_CORE_POW(Izw, ALWAN_CORE_LITERAL(0.78));
    ALWAN_CORE_T Fb_pow_01 = ALWAN_CORE_POW(Fb, ALWAN_CORE_LITERAL(0.1));
    result.Mz = ALWAN_CORE_LITERAL(100.0) * chroma_component * ez_pow * FL_pow_02 /
                (Izw_pow_078 * Fb_pow_01);

    /* Step 13: Chroma Cz */
    result.Cz = ALWAN_CORE_LITERAL(100.0) * result.Mz / Qzw;

    /* Step 14: Saturation Sz */
    ALWAN_CORE_T Mz_over_Qz = ALWAN_CORE_SELECT(result.Qz > ALWAN_CORE_LITERAL(1e-10),
                                            result.Mz / result.Qz, ALWAN_CORE_ZERO);
    ALWAN_CORE_T FL_pow_06 = ALWAN_CORE_POW(FL, ALWAN_CORE_LITERAL(0.6));
    result.Sz = ALWAN_CORE_SELECT(result.Qz > ALWAN_CORE_LITERAL(1e-10),
                             ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_POW(Mz_over_Qz, ALWAN_CORE_LITERAL(0.5)) * FL_pow_06,
                             ALWAN_CORE_ZERO);

    /* Step 15: Vividness Vz */
    ALWAN_CORE_T J_diff = result.Jz - ALWAN_CORE_LITERAL(58.0);
    result.Vz = ALWAN_CORE_SQRT(J_diff * J_diff + ALWAN_CORE_LITERAL(3.4) * result.Cz * result.Cz);

    /* Step 16: Blackness Kz */
    result.Kz = ALWAN_CORE_LITERAL(100.0) -
                ALWAN_CORE_LITERAL(0.8) * ALWAN_CORE_SQRT(result.Jz * result.Jz +
                                                 ALWAN_CORE_LITERAL(8.0) * result.Cz * result.Cz);

    /* Step 17: Whiteness Wz */
    ALWAN_CORE_T J_diff_w = ALWAN_CORE_LITERAL(100.0) - result.Jz;
    result.Wz = ALWAN_CORE_LITERAL(100.0) - ALWAN_CORE_SQRT(J_diff_w * J_diff_w + result.Cz * result.Cz);

    return result;
}

/* ----------------------------------------------------------------
 * ZCAM Inverse Transform: Correlates -> XYZ (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_zcam_inverse)(
    ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) correlates,
    ALWAN_CORE_XYZ xyz_w,
    ALWAN_CORE_T Fs,
    ALWAN_CORE_T c,
    ALWAN_CORE_T Nc,
    ALWAN_CORE_T F,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Y_b,
    ALWAN_CORE_T Y_w) {

    ALWAN_CORE_XYZ result;

    ALWAN_UNUSED(xyz_w); ALWAN_UNUSED(Fs); ALWAN_UNUSED(c); ALWAN_UNUSED(Nc); ALWAN_UNUSED(F);
    ALWAN_UNUSED(La); ALWAN_UNUSED(Y_b); ALWAN_UNUSED(Y_w);

    /* Step 1: Convert Jz to Iz (Jz is on 0-100 scale, normalize first) */
    ALWAN_CORE_T Iz = ALWAN_CORE_FNVLIT(zcam_jz_to_iz)(correlates.Jz / ALWAN_CORE_LITERAL(100.0));

    /* Step 2: Recover az, bz from Mz and hz */
    ALWAN_CORE_T hz_rad = correlates.hz * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T Mz_norm = correlates.Mz / ALWAN_CORE_LITERAL(100.0);

    /* Approximate inverse of colorfulness formula */
    ALWAN_CORE_T chroma_squared = ALWAN_CORE_POW(Mz_norm, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(0.37));
    ALWAN_CORE_T chroma_mag = ALWAN_CORE_SQRT(chroma_squared);

    ALWAN_CORE_T az = chroma_mag * ALWAN_CORE_COS(hz_rad);
    ALWAN_CORE_T bz = chroma_mag * ALWAN_CORE_SIN(hz_rad);

    /* Step 3: Izazbz -> LMS' */
    ALWAN_CORE_VEC3 in_v = {{Iz, az, bz}};
    ALWAN_CORE_VEC3 out_v = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_IZAZBZ_TO_LMS_P), in_v);
    ALWAN_CORE_T lp = out_v.v[0];
    ALWAN_CORE_T mp = out_v.v[1];
    ALWAN_CORE_T sp = out_v.v[2];

    /* Step 4: Inverse PQ: LMS' -> LMS */
    ALWAN_CORE_T l = ALWAN_CORE_FNVLIT(zcam_pq_inverse)(lp);
    ALWAN_CORE_T m = ALWAN_CORE_FNVLIT(zcam_pq_inverse)(mp);
    ALWAN_CORE_T s = ALWAN_CORE_FNVLIT(zcam_pq_inverse)(sp);

    /* Step 5: LMS -> adapted XYZ */
    ALWAN_CORE_VEC3 in_v2 = {{l, m, s}};
    ALWAN_CORE_VEC3 out_v2 = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_LMS_TO_XYZ), in_v2);
    ALWAN_CORE_T xa = out_v2.v[0];
    ALWAN_CORE_T ya = out_v2.v[1];
    ALWAN_CORE_T za = out_v2.v[2];

    /* Step 6: Inverse chromatic adaptation */
    result.z = za;
    result.x = (xa + (ZCAM_V_B - ALWAN_CORE_ONE) * result.z) / ZCAM_V_B;
    result.y = (ya + (ZCAM_V_G - ALWAN_CORE_ONE) * result.x) / ZCAM_V_G;

    return result;
}

/* ----------------------------------------------------------------
 * ZCAM to UCS (Uniform Color Space) for color difference
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_JZAZBZ ALWAN_CORE_FNVLIT(alwan_zcam_to_ucs)(ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) correlates) {
    ALWAN_CORE_JZAZBZ result;

    ALWAN_CORE_T hz_rad = correlates.hz * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    result.Jz = correlates.Jz;
    result.az = correlates.Mz * ALWAN_CORE_COS(hz_rad);
    result.bz = correlates.Mz * ALWAN_CORE_SIN(hz_rad);

    return result;
}

/* Undefine local constants to avoid redefinition on second pass */
#undef ZCAM_V_B
#undef ZCAM_V_G
#undef ZCAM_V_PQ_C1
#undef ZCAM_V_PQ_C2
#undef ZCAM_V_PQ_C3
#undef ZCAM_V_PQ_N
#undef ZCAM_V_PQ_P
#undef ZCAM_V_PQ_D
#undef ZCAM_V_PQ_D0
