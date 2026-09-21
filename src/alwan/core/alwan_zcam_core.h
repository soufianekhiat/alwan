/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ZCAM HDR Color Appearance Model
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: Safdar et al. (2021) "ZCAM, a colour appearance model based on
 * a high dynamic range uniform colour space"
 * Reference: Optics Express 29(4), 6036-6052
 */

#ifndef ALWAN_ZCAM_CORE_H
#define ALWAN_ZCAM_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"
#include "alwan_cat_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_zcam_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_zcam_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only. The history and the reference are at
 * the top of alwan_zcam_core.inc.
 * ================================================================ */

/* ----------------------------------------------------------------
 * ZCAM Result Struct 
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_scalar Jz;     /* Lightness */
    alwan_scalar Cz;     /* Chroma */
    alwan_scalar hz;     /* Hue angle (degrees) */
    alwan_scalar Qz;     /* Brightness */
    alwan_scalar Mz;     /* Colorfulness */
    alwan_scalar Sz;     /* Saturation */
    alwan_scalar Vz;     /* Vividness */
    alwan_scalar Kz;     /* Blackness */
    alwan_scalar Wz;     /* Whiteness */
} alwan_zcam_v_correlates;

/* ----------------------------------------------------------------
 * ZCAM Constants (Safdar 2017 for the Jzazbz front end, Safdar 2021 for the rest)
 * ---------------------------------------------------------------- */

#define ZCAM_V_B     ALWAN_LITERAL(1.15)
#define ZCAM_V_G     ALWAN_LITERAL(0.66)

#define ZCAM_V_PQ_C1 ALWAN_LITERAL(0.8359375)
#define ZCAM_V_PQ_C2 ALWAN_LITERAL(18.8515625)
#define ZCAM_V_PQ_C3 ALWAN_LITERAL(18.6875)
#define ZCAM_V_PQ_N  ALWAN_LITERAL(0.1593017578125)
/* 1.7 * 2523 / 2^5, spelled as the reference computes it: 1.7 is not exact in
 * binary, and the product need not round to the literal 134.034375. */
#define ZCAM_V_PQ_P  (ALWAN_LITERAL(1.7) * ALWAN_LITERAL(2523.0) / ALWAN_LITERAL(32.0))

/* I_z = M' - epsilon. Safdar 2021, equation 9. */
#define ZCAM_V_EPSILON ALWAN_LITERAL(3.7035226210190005e-11)

/* D65 as the chromaticity (0.3127, 0.3290) at Y = 1, which is how the reference
 * builds the white it adapts to. Its scale is free: see alwan_cat_zhai2018. */
#define ZCAM_V_D65_X (ALWAN_LITERAL(0.3127) / ALWAN_LITERAL(0.3290))
#define ZCAM_V_D65_Z ((ALWAN_ONE - ALWAN_LITERAL(0.3127) - ALWAN_LITERAL(0.3290)) / ALWAN_LITERAL(0.3290))

/* ----------------------------------------------------------------
 * ZCAM Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_XYZ_TO_LMS = {{
#include "../data/matrices/jzazbz_xyz_to_lms.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_LMS_TO_XYZ = {{
#include "../data/matrices/jzazbz_lms_to_xyz.csv"
}};

/* The Safdar 2021 pair, whose first row is (0, 1, 0): I_z is M'. */
ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_LMS_P_TO_IZAZBZ = {{
#include "../data/matrices/jzazbz_lms_p_to_izazbz_2021.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_IZAZBZ_TO_LMS_P = {{
#include "../data/matrices/jzazbz_izazbz_to_lms_p_2021.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_CAT02 = {{
#include "../data/matrices/cat_cat02.csv"
}};

ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_CAT02_INV = {{
#include "../data/matrices/cat_cat02_inv.csv"
}};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * The PQ-shaped nonlinearity of Jzazbz, with its exponent of 134.03
 *
 * Signed powers, as the reference: a stimulus outside the spectral locus has a
 * negative cone response, and it keeps its sign through the curve instead of
 * being flattened to zero.
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar zcam_pq_forward_v(alwan_scalar L) {
    alwan_scalar L_pow_n = alwan_cat_spow_v(L / ALWAN_LITERAL(10000.0), ZCAM_V_PQ_N);
    alwan_scalar numerator = ZCAM_V_PQ_C1 + ZCAM_V_PQ_C2 * L_pow_n;
    alwan_scalar denominator = ALWAN_ONE + ZCAM_V_PQ_C3 * L_pow_n;
    return alwan_cat_spow_v(numerator / denominator, ZCAM_V_PQ_P);
}

ALWAN_INLINE alwan_scalar zcam_pq_inverse_v(alwan_scalar E) {
    alwan_scalar E_pow = alwan_cat_spow_v(E, ALWAN_ONE / ZCAM_V_PQ_P);
    alwan_scalar numerator = E_pow - ZCAM_V_PQ_C1;
    alwan_scalar denominator = ZCAM_V_PQ_C2 - ZCAM_V_PQ_C3 * E_pow;
    /* Below the curve's floor there is no luminance to return: PQ(0) is C1^P,
     * and an encoded value under it would ask for the root of a negative. */
    numerator = ALWAN_SELECT(numerator < ALWAN_ZERO, ALWAN_ZERO, numerator);
    return ALWAN_LITERAL(10000.0) *
           alwan_cat_spow_v(numerator / denominator, ALWAN_ONE / ZCAM_V_PQ_N);
}

/* ----------------------------------------------------------------
 * Izazbz, Safdar 2021: absolute D65 XYZ in cd/m^2 <-> (I_z, a_z, b_z)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_vec3 zcam_xyz_to_izazbz_v(alwan_xyz xyz) {
    alwan_vec3 in_v;
    in_v.v[0] = ZCAM_V_B * xyz.x - (ZCAM_V_B - ALWAN_ONE) * xyz.z;
    in_v.v[1] = ZCAM_V_G * xyz.y - (ZCAM_V_G - ALWAN_ONE) * xyz.x;
    in_v.v[2] = xyz.z;
    alwan_vec3 lms = alwan_mat3_mulv_v(ZCAM_V_XYZ_TO_LMS, in_v);

    alwan_vec3 lms_p;
    lms_p.v[0] = zcam_pq_forward_v(lms.v[0]);
    lms_p.v[1] = zcam_pq_forward_v(lms.v[1]);
    lms_p.v[2] = zcam_pq_forward_v(lms.v[2]);

    alwan_vec3 izazbz = alwan_mat3_mulv_v(ZCAM_V_LMS_P_TO_IZAZBZ, lms_p);
    izazbz.v[0] = izazbz.v[0] - ZCAM_V_EPSILON;
    return izazbz;
}

ALWAN_INLINE alwan_xyz zcam_izazbz_to_xyz_v(alwan_vec3 izazbz) {
    alwan_vec3 in_v;
    in_v.v[0] = izazbz.v[0] + ZCAM_V_EPSILON;
    in_v.v[1] = izazbz.v[1];
    in_v.v[2] = izazbz.v[2];
    alwan_vec3 lms_p = alwan_mat3_mulv_v(ZCAM_V_IZAZBZ_TO_LMS_P, in_v);

    alwan_vec3 lms;
    lms.v[0] = zcam_pq_inverse_v(lms_p.v[0]);
    lms.v[1] = zcam_pq_inverse_v(lms_p.v[1]);
    lms.v[2] = zcam_pq_inverse_v(lms_p.v[2]);

    alwan_vec3 xp = alwan_mat3_mulv_v(ZCAM_V_LMS_TO_XYZ, lms);
    alwan_xyz result;
    result.x = (xp.v[0] + (ZCAM_V_B - ALWAN_ONE) * xp.v[2]) / ZCAM_V_B;
    result.y = (xp.v[1] + (ZCAM_V_G - ALWAN_ONE) * result.x) / ZCAM_V_G;
    result.z = xp.v[2];
    return result;
}

/* ----------------------------------------------------------------
 * Viewing-condition terms
 * ---------------------------------------------------------------- */

/* Degree of adaptation, the CIECAM02 form the model adopts:
 * D = F * (1 - (1/3.6) * exp((-L_A - 42) / 92)). F is the surround's F, which
 * is NOT F_s. Average, dim, dark: F = 1.0, 0.9, 0.8 and F_s = 0.69, 0.59, 0.525.
 * A discounted illuminant is D = 1, passed in place of this. */
ALWAN_INLINE alwan_scalar alwan_zcam_degree_of_adaptation_v(alwan_scalar F, alwan_scalar La) {
    return F * (ALWAN_ONE - (ALWAN_ONE / ALWAN_LITERAL(3.6)) *
                    ALWAN_EXP((-La - ALWAN_LITERAL(42.0)) / ALWAN_LITERAL(92.0)));
}

/* Eccentricity factor, e_z = 1.015 + cos(89.038 + h_z) */
ALWAN_INLINE alwan_scalar zcam_eccentricity_v(alwan_scalar h_degrees) {
    alwan_scalar h_rad = (h_degrees + ALWAN_LITERAL(89.038)) * ALWAN_PI / ALWAN_LITERAL(180.0);
    return ALWAN_LITERAL(1.015) + ALWAN_COS(h_rad);
}

/* ----------------------------------------------------------------
 * ZCAM Forward Transform: XYZ -> Correlates (value-returning)
 *
 * xyz, xyz_w  absolute, cd/m^2, under the viewing illuminant
 * Fs          the surround's F_s: 0.69 average, 0.59 dim, 0.525 dark
 * D           degree of adaptation, from alwan_zcam_degree_of_adaptation, or 1
 * La          adapting luminance, cd/m^2
 * Y_b         luminance factor of the background, on the scale of xyz_w.y
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_zcam_v_correlates alwan_zcam_forward_v(
    alwan_xyz xyz,
    alwan_xyz xyz_w,
    alwan_scalar Fs,
    alwan_scalar D,
    alwan_scalar La,
    alwan_scalar Y_b) {

    alwan_zcam_v_correlates result;

    /* D65 and the equal-energy baseline of the two-step CAT, at Y = 1 */
    alwan_xyz d65;
    d65.x = ZCAM_V_D65_X; d65.y = ALWAN_ONE; d65.z = ZCAM_V_D65_Z;
    alwan_xyz ee;
    ee.x = ALWAN_ONE; ee.y = ALWAN_ONE; ee.z = ALWAN_ONE;

    /* Step 0: the stimulus to D65, by the two-step CAT over CAT02. The white is
     * NOT adapted: the model takes I_z,w from the white as given. */
    alwan_xyz xyz_d65 = alwan_cat_zhai2018_v(
        ZCAM_V_CAT02, ZCAM_V_CAT02_INV,
        xyz, xyz_w, d65, D, D, ee);

    /* Step 1: factors of the viewing conditions */
    alwan_scalar Fb = ALWAN_SQRT(Y_b / xyz_w.y);
    alwan_scalar FL = ALWAN_LITERAL(0.171) *
                      alwan_cat_spow_v(La, ALWAN_ONE / ALWAN_LITERAL(3.0)) *
                      (ALWAN_ONE - ALWAN_EXP(-ALWAN_LITERAL(48.0) / ALWAN_LITERAL(9.0) * La));

    /* Step 2: achromatic response and opponent signals, stimulus and white */
    alwan_vec3 izazbz = zcam_xyz_to_izazbz_v(xyz_d65);
    alwan_scalar Iz = izazbz.v[0];
    alwan_scalar az = izazbz.v[1];
    alwan_scalar bz = izazbz.v[2];
    alwan_vec3 izazbz_w = zcam_xyz_to_izazbz_v(xyz_w);
    alwan_scalar Izw = izazbz_w.v[0];

    /* Step 3: hue angle, degrees in [0, 360) */
    alwan_scalar hz_raw = ALWAN_ATAN2(bz, az) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.hz = ALWAN_SELECT(hz_raw < ALWAN_ZERO, hz_raw + ALWAN_LITERAL(360.0), hz_raw);

    /* Step 4: eccentricity */
    alwan_scalar ez = zcam_eccentricity_v(result.hz);

    /* Step 5: brightness Q_z, of the stimulus and of the white */
    alwan_scalar Qz_p = (ALWAN_LITERAL(1.6) * Fs) / alwan_cat_spow_v(Fb, ALWAN_LITERAL(0.12));
    alwan_scalar FL_pow_02 = alwan_cat_spow_v(FL, ALWAN_LITERAL(0.2));
    alwan_scalar Qz_m = alwan_cat_spow_v(Fs, ALWAN_LITERAL(2.2)) *
                        alwan_cat_spow_v(Fb, ALWAN_LITERAL(0.5)) * FL_pow_02;
    result.Qz = ALWAN_LITERAL(2700.0) * alwan_cat_spow_v(Iz, Qz_p) * Qz_m;
    alwan_scalar Qzw = ALWAN_LITERAL(2700.0) * alwan_cat_spow_v(Izw, Qz_p) * Qz_m;

    /* Step 6: lightness J_z */
    result.Jz = ALWAN_LITERAL(100.0) * (result.Qz / Qzw);

    /* Step 7: colourfulness M_z */
    result.Mz = ALWAN_LITERAL(100.0) *
                alwan_cat_spow_v(az * az + bz * bz, ALWAN_LITERAL(0.37)) *
                ((alwan_cat_spow_v(ez, ALWAN_LITERAL(0.068)) * FL_pow_02) /
                 (alwan_cat_spow_v(Fb, ALWAN_LITERAL(0.1)) *
                  alwan_cat_spow_v(Izw, ALWAN_LITERAL(0.78))));

    /* Step 8: chroma C_z */
    result.Cz = ALWAN_LITERAL(100.0) * (result.Mz / Qzw);

    /* Step 9: saturation S_z. A stimulus with no brightness has none. */
    alwan_scalar Mz_over_Qz = ALWAN_SELECT(result.Qz > ALWAN_LITERAL(1e-10),
                                            result.Mz / result.Qz, ALWAN_ZERO);
    result.Sz = ALWAN_LITERAL(100.0) * alwan_cat_spow_v(FL, ALWAN_LITERAL(0.6)) *
                ALWAN_SQRT(Mz_over_Qz);

    /* Step 10: vividness V_z, blackness K_z, whiteness W_z */
    alwan_scalar J_diff = result.Jz - ALWAN_LITERAL(58.0);
    result.Vz = ALWAN_SQRT(J_diff * J_diff + ALWAN_LITERAL(3.4) * result.Cz * result.Cz);

    result.Kz = ALWAN_LITERAL(100.0) -
                ALWAN_LITERAL(0.8) * ALWAN_SQRT(result.Jz * result.Jz +
                                                 ALWAN_LITERAL(8.0) * result.Cz * result.Cz);

    alwan_scalar J_diff_w = ALWAN_LITERAL(100.0) - result.Jz;
    result.Wz = ALWAN_LITERAL(100.0) - ALWAN_SQRT(J_diff_w * J_diff_w + result.Cz * result.Cz);

    return result;
}

/* ----------------------------------------------------------------
 * ZCAM Inverse Transform: Correlates -> XYZ (value-returning)
 *
 * Reads J_z, M_z and h_z, the three that alwan_zcam_from_ucs fills, and
 * nothing else. Same viewing-condition arguments as the forward model, and the
 * exact inverse of it: every step is closed form.
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_zcam_inverse_v(
    alwan_zcam_v_correlates correlates,
    alwan_xyz xyz_w,
    alwan_scalar Fs,
    alwan_scalar D,
    alwan_scalar La,
    alwan_scalar Y_b) {

    /* D65 and the equal-energy baseline of the two-step CAT, at Y = 1 */
    alwan_xyz d65;
    d65.x = ZCAM_V_D65_X; d65.y = ALWAN_ONE; d65.z = ZCAM_V_D65_Z;
    alwan_xyz ee;
    ee.x = ALWAN_ONE; ee.y = ALWAN_ONE; ee.z = ALWAN_ONE;

    /* Factors of the viewing conditions, as the forward model */
    alwan_scalar Fb = ALWAN_SQRT(Y_b / xyz_w.y);
    alwan_scalar FL = ALWAN_LITERAL(0.171) *
                      alwan_cat_spow_v(La, ALWAN_ONE / ALWAN_LITERAL(3.0)) *
                      (ALWAN_ONE - ALWAN_EXP(-ALWAN_LITERAL(48.0) / ALWAN_LITERAL(9.0) * La));
    alwan_vec3 izazbz_w = zcam_xyz_to_izazbz_v(xyz_w);
    alwan_scalar Izw = izazbz_w.v[0];

    /* Step 1: achromatic response I_z, from J_z through the white's brightness */
    alwan_scalar Fb_pow_012 = alwan_cat_spow_v(Fb, ALWAN_LITERAL(0.12));
    alwan_scalar FL_pow_02 = alwan_cat_spow_v(FL, ALWAN_LITERAL(0.2));
    alwan_scalar Qz_p = (ALWAN_LITERAL(1.6) * Fs) / Fb_pow_012;
    alwan_scalar Qz_m = alwan_cat_spow_v(Fs, ALWAN_LITERAL(2.2)) *
                        alwan_cat_spow_v(Fb, ALWAN_LITERAL(0.5)) * FL_pow_02;
    alwan_scalar Qzw = ALWAN_LITERAL(2700.0) * alwan_cat_spow_v(Izw, Qz_p) * Qz_m;

    alwan_scalar Iz_p = Fb_pow_012 / (ALWAN_LITERAL(1.6) * Fs);
    alwan_scalar Iz_d = ALWAN_LITERAL(2700.0) * ALWAN_LITERAL(100.0) * Qz_m;
    alwan_scalar Iz = alwan_cat_spow_v((correlates.Jz * Qzw) / Iz_d, Iz_p);

    /* Step 2: opponent signals, from M_z and h_z. 50/37 is 1 / (2 * 0.37). */
    alwan_scalar ez = zcam_eccentricity_v(correlates.hz);
    alwan_scalar hz_rad = correlates.hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar Cz_p = alwan_cat_spow_v(
        (correlates.Mz * alwan_cat_spow_v(Izw, ALWAN_LITERAL(0.78)) *
         alwan_cat_spow_v(Fb, ALWAN_LITERAL(0.1))) /
        (ALWAN_LITERAL(100.0) * alwan_cat_spow_v(ez, ALWAN_LITERAL(0.068)) * FL_pow_02),
        ALWAN_LITERAL(50.0) / ALWAN_LITERAL(37.0));

    alwan_vec3 izazbz;
    izazbz.v[0] = Iz;
    izazbz.v[1] = Cz_p * ALWAN_COS(hz_rad);
    izazbz.v[2] = Cz_p * ALWAN_SIN(hz_rad);

    /* Step 3: to D65 XYZ, then back under the viewing illuminant */
    alwan_xyz xyz_d65 = zcam_izazbz_to_xyz_v(izazbz);
    return alwan_cat_zhai2018_v(
        ZCAM_V_CAT02, ZCAM_V_CAT02_INV,
        xyz_d65, d65, xyz_w, D, D, ee);
}

/* ----------------------------------------------------------------
 * ZCAM to UCS (Uniform Color Space) for color difference
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_jzazbz alwan_zcam_to_ucs_v(alwan_zcam_v_correlates correlates) {
    alwan_jzazbz result;

    alwan_scalar hz_rad = correlates.hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    result.Jz = correlates.Jz;
    result.az = correlates.Mz * ALWAN_COS(hz_rad);
    result.bz = correlates.Mz * ALWAN_SIN(hz_rad);

    return result;
}

/* ZCAM-UCS back to correlates: Jz, Mz and hz exactly, the rest zero. */
ALWAN_INLINE alwan_zcam_v_correlates alwan_zcam_from_ucs_v(alwan_jzazbz Jab) {
    alwan_zcam_v_correlates result;

    alwan_scalar hz = ALWAN_ATAN2(Jab.bz, Jab.az) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    hz = ALWAN_SELECT(hz < ALWAN_ZERO, hz + ALWAN_LITERAL(360.0), hz);
    result.Jz = Jab.Jz;
    result.Mz = ALWAN_SQRT(Jab.az * Jab.az + Jab.bz * Jab.bz);
    result.hz = hz;
    result.Cz = ALWAN_ZERO; result.Qz = ALWAN_ZERO; result.Sz = ALWAN_ZERO;
    result.Vz = ALWAN_ZERO; result.Kz = ALWAN_ZERO; result.Wz = ALWAN_ZERO;

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
#undef ZCAM_V_EPSILON
#undef ZCAM_V_D65_X
#undef ZCAM_V_D65_Z

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_ZCAM_CORE_H */
