/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ZCAM HDR Color Appearance Model (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Reference: Safdar, M., Hardeberg, J. Y., and Luo, M. R. (2021). ZCAM, a
 * colour appearance model based on a high dynamic range uniform colour space.
 * Optics Express, 29(4), 6036. https://doi.org/10.1364/OE.413659
 *
 * Checked against colour-science's XYZ_to_ZCAM and ZCAM_to_XYZ in suite 28,
 * over three surrounds, a white that is not D65, a white that is not at
 * Y = 100, and a discounted illuminant.
 *
 * WHAT THIS FILE GOT WRONG UNTIL 2026-09-21, because the record matters more
 * than the embarrassment. It had never been compared with anything: suite 28
 * printed each mismatch and returned success. Against the reference, 9 of 12
 * colours disagreed, lightness by up to 18 of 100. Three causes in the forward
 * model, and an inverse that was not one:
 *
 *   - The surround factor F_s was read from the wrong column. The API passed
 *     CIECAM02's F (1.0, 0.9, 0.8) where ZCAM's F_s is 0.69, 0.59, 0.525. F_s
 *     is an EXPONENT on the achromatic response, so this alone moved J_z of a
 *     dark grey from 34.4 to 21.3.
 *   - The achromatic response was Jzazbz's, (L' + M') / 2. ZCAM defines its
 *     own, I_z = M' - epsilon, which is why Izazbz has a Safdar 2021 form with
 *     its own matrix. Neutrals barely notice, since L' and M' nearly agree
 *     there. A saturated red was off by 6.7 in J_z from this alone.
 *   - Step 0 of the model, chromatic adaptation of the stimulus to D65, was
 *     missing. What the old code labelled "chromatic adaptation" is Jzazbz's
 *     own X', Y' pre-transform, which is something else.
 *   - The inverse read J_z through Jzazbz's I_z to J_z curve, which ZCAM does
 *     not use, ignored every viewing condition, and inverted the colourfulness
 *     formula without its viewing-condition terms. It could not round-trip, and
 *     a note in suite 93 explained the failure as f32 conditioning.
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
 * ZCAM Constants (Safdar 2017 for the Jzazbz front end, Safdar 2021 for the rest)
 * ---------------------------------------------------------------- */

#define ZCAM_V_B     ALWAN_CORE_LITERAL(1.15)
#define ZCAM_V_G     ALWAN_CORE_LITERAL(0.66)

#define ZCAM_V_PQ_C1 ALWAN_CORE_LITERAL(0.8359375)
#define ZCAM_V_PQ_C2 ALWAN_CORE_LITERAL(18.8515625)
#define ZCAM_V_PQ_C3 ALWAN_CORE_LITERAL(18.6875)
#define ZCAM_V_PQ_N  ALWAN_CORE_LITERAL(0.1593017578125)
/* 1.7 * 2523 / 2^5, spelled as the reference computes it: 1.7 is not exact in
 * binary, and the product need not round to the literal 134.034375. */
#define ZCAM_V_PQ_P  (ALWAN_CORE_LITERAL(1.7) * ALWAN_CORE_LITERAL(2523.0) / ALWAN_CORE_LITERAL(32.0))

/* I_z = M' - epsilon. Safdar 2021, equation 9. */
#define ZCAM_V_EPSILON ALWAN_CORE_LITERAL(3.7035226210190005e-11)

/* D65 as the chromaticity (0.3127, 0.3290) at Y = 1, which is how the reference
 * builds the white it adapts to. Its scale is free: see alwan_cat_zhai2018. */
#define ZCAM_V_D65_X (ALWAN_CORE_LITERAL(0.3127) / ALWAN_CORE_LITERAL(0.3290))
#define ZCAM_V_D65_Z ((ALWAN_CORE_ONE - ALWAN_CORE_LITERAL(0.3127) - ALWAN_CORE_LITERAL(0.3290)) / ALWAN_CORE_LITERAL(0.3290))

/* ----------------------------------------------------------------
 * ZCAM Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_XYZ_TO_LMS) = {{
#include "../data/matrices/jzazbz_xyz_to_lms.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_LMS_TO_XYZ) = {{
#include "../data/matrices/jzazbz_lms_to_xyz.csv"
}};

/* The Safdar 2021 pair, whose first row is (0, 1, 0): I_z is M'. */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_LMS_P_TO_IZAZBZ) = {{
#include "../data/matrices/jzazbz_lms_p_to_izazbz_2021.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_IZAZBZ_TO_LMS_P) = {{
#include "../data/matrices/jzazbz_izazbz_to_lms_p_2021.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_CAT02) = {{
#include "../data/matrices/cat_cat02.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(ZCAM_V_CAT02_INV) = {{
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

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_pq_forward)(ALWAN_CORE_T L) {
    ALWAN_CORE_T L_pow_n = ALWAN_CORE_FNVLIT(alwan_cat_spow)(L / ALWAN_CORE_LITERAL(10000.0), ZCAM_V_PQ_N);
    ALWAN_CORE_T numerator = ZCAM_V_PQ_C1 + ZCAM_V_PQ_C2 * L_pow_n;
    ALWAN_CORE_T denominator = ALWAN_CORE_ONE + ZCAM_V_PQ_C3 * L_pow_n;
    return ALWAN_CORE_FNVLIT(alwan_cat_spow)(numerator / denominator, ZCAM_V_PQ_P);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_pq_inverse)(ALWAN_CORE_T E) {
    ALWAN_CORE_T E_pow = ALWAN_CORE_FNVLIT(alwan_cat_spow)(E, ALWAN_CORE_ONE / ZCAM_V_PQ_P);
    ALWAN_CORE_T numerator = E_pow - ZCAM_V_PQ_C1;
    ALWAN_CORE_T denominator = ZCAM_V_PQ_C2 - ZCAM_V_PQ_C3 * E_pow;
    /* Below the curve's floor there is no luminance to return: PQ(0) is C1^P,
     * and an encoded value under it would ask for the root of a negative. */
    numerator = ALWAN_CORE_SELECT(numerator < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, numerator);
    return ALWAN_CORE_LITERAL(10000.0) *
           ALWAN_CORE_FNVLIT(alwan_cat_spow)(numerator / denominator, ALWAN_CORE_ONE / ZCAM_V_PQ_N);
}

/* ----------------------------------------------------------------
 * Izazbz, Safdar 2021: absolute D65 XYZ in cd/m^2 <-> (I_z, a_z, b_z)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(zcam_xyz_to_izazbz)(ALWAN_CORE_XYZ xyz) {
    ALWAN_CORE_VEC3 in_v;
    in_v.v[0] = ZCAM_V_B * xyz.x - (ZCAM_V_B - ALWAN_CORE_ONE) * xyz.z;
    in_v.v[1] = ZCAM_V_G * xyz.y - (ZCAM_V_G - ALWAN_CORE_ONE) * xyz.x;
    in_v.v[2] = xyz.z;
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_XYZ_TO_LMS), in_v);

    ALWAN_CORE_VEC3 lms_p;
    lms_p.v[0] = ALWAN_CORE_FNVLIT(zcam_pq_forward)(lms.v[0]);
    lms_p.v[1] = ALWAN_CORE_FNVLIT(zcam_pq_forward)(lms.v[1]);
    lms_p.v[2] = ALWAN_CORE_FNVLIT(zcam_pq_forward)(lms.v[2]);

    ALWAN_CORE_VEC3 izazbz = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_LMS_P_TO_IZAZBZ), lms_p);
    izazbz.v[0] = izazbz.v[0] - ZCAM_V_EPSILON;
    return izazbz;
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(zcam_izazbz_to_xyz)(ALWAN_CORE_VEC3 izazbz) {
    ALWAN_CORE_VEC3 in_v;
    in_v.v[0] = izazbz.v[0] + ZCAM_V_EPSILON;
    in_v.v[1] = izazbz.v[1];
    in_v.v[2] = izazbz.v[2];
    ALWAN_CORE_VEC3 lms_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_IZAZBZ_TO_LMS_P), in_v);

    ALWAN_CORE_VEC3 lms;
    lms.v[0] = ALWAN_CORE_FNVLIT(zcam_pq_inverse)(lms_p.v[0]);
    lms.v[1] = ALWAN_CORE_FNVLIT(zcam_pq_inverse)(lms_p.v[1]);
    lms.v[2] = ALWAN_CORE_FNVLIT(zcam_pq_inverse)(lms_p.v[2]);

    ALWAN_CORE_VEC3 xp = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(ZCAM_V_LMS_TO_XYZ), lms);
    ALWAN_CORE_XYZ result;
    result.x = (xp.v[0] + (ZCAM_V_B - ALWAN_CORE_ONE) * xp.v[2]) / ZCAM_V_B;
    result.y = (xp.v[1] + (ZCAM_V_G - ALWAN_CORE_ONE) * result.x) / ZCAM_V_G;
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
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_zcam_degree_of_adaptation)(ALWAN_CORE_T F, ALWAN_CORE_T La) {
    return F * (ALWAN_CORE_ONE - (ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.6)) *
                    ALWAN_CORE_EXP((-La - ALWAN_CORE_LITERAL(42.0)) / ALWAN_CORE_LITERAL(92.0)));
}

/* Eccentricity factor, e_z = 1.015 + cos(89.038 + h_z) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(zcam_eccentricity)(ALWAN_CORE_T h_degrees) {
    ALWAN_CORE_T h_rad = (h_degrees + ALWAN_CORE_LITERAL(89.038)) * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    return ALWAN_CORE_LITERAL(1.015) + ALWAN_CORE_COS(h_rad);
}

/* ----------------------------------------------------------------
 * The per-white terms of the model, computed once
 *
 * Everything the forward and inverse transforms need that does not depend on
 * the stimulus: the two-step CAT's gains in each direction, the white's
 * achromatic response, and the viewing-condition powers. One colour computes
 * them and uses them once; a map computes them once for its buffer. The
 * arithmetic is the same in both, so a map is bit-identical to its scalar
 * twin, which the determinism dump checks.
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_VEC3 gains_to_d65;   /* two-step CAT, viewing white -> D65 */
    ALWAN_CORE_VEC3 gains_from_d65; /* and back */
    ALWAN_CORE_T Izw;               /* achromatic response of the white */
    ALWAN_CORE_T Qz_p;              /* exponent on I_z, 1.6 F_s / F_b^0.12 */
    ALWAN_CORE_T Qz_m;              /* F_s^2.2 F_b^0.5 F_L^0.2 */
    ALWAN_CORE_T Qzw;               /* brightness of the white */
    ALWAN_CORE_T FL_pow_02;         /* F_L^0.2 */
    ALWAN_CORE_T FL_pow_06;         /* F_L^0.6 */
    ALWAN_CORE_T Fb_pow_01;         /* F_b^0.1 */
    ALWAN_CORE_T Izw_pow_078;       /* I_z,w^0.78 */
    ALWAN_CORE_T Iz_p;              /* F_b^0.12 / (1.6 F_s), the inverse's exponent */
    ALWAN_CORE_T Iz_d;              /* 2700 * 100 * Qz_m */
} ALWAN_CORE_FNLIT(alwan_zcam_v_params);

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_zcam_v_params) ALWAN_CORE_FNVLIT(alwan_zcam_params)(
    ALWAN_CORE_XYZ xyz_w,
    ALWAN_CORE_T Fs,
    ALWAN_CORE_T D,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Y_b) {

    ALWAN_CORE_FNLIT(alwan_zcam_v_params) p;

    /* D65 and the equal-energy baseline of the two-step CAT, at Y = 1 */
    ALWAN_CORE_XYZ d65;
    d65.x = ZCAM_V_D65_X; d65.y = ALWAN_CORE_ONE; d65.z = ZCAM_V_D65_Z;
    ALWAN_CORE_XYZ ee;
    ee.x = ALWAN_CORE_ONE; ee.y = ALWAN_CORE_ONE; ee.z = ALWAN_CORE_ONE;

    p.gains_to_d65 = ALWAN_CORE_FNVLIT(alwan_cat_zhai2018_gains)(ALWAN_CORE_FNLIT(ZCAM_V_CAT02), xyz_w, d65, D, D, ee);
    p.gains_from_d65 = ALWAN_CORE_FNVLIT(alwan_cat_zhai2018_gains)(ALWAN_CORE_FNLIT(ZCAM_V_CAT02), d65, xyz_w, D, D, ee);

    /* Factors of the viewing conditions */
    ALWAN_CORE_T Fb = ALWAN_CORE_SQRT(Y_b / xyz_w.y);
    ALWAN_CORE_T FL = ALWAN_CORE_LITERAL(0.171) *
                      ALWAN_CORE_FNVLIT(alwan_cat_spow)(La, ALWAN_CORE_ONE / ALWAN_CORE_LITERAL(3.0)) *
                      (ALWAN_CORE_ONE - ALWAN_CORE_EXP(-ALWAN_CORE_LITERAL(48.0) / ALWAN_CORE_LITERAL(9.0) * La));

    ALWAN_CORE_VEC3 izazbz_w = ALWAN_CORE_FNVLIT(zcam_xyz_to_izazbz)(xyz_w);
    p.Izw = izazbz_w.v[0];

    ALWAN_CORE_T Fb_pow_012 = ALWAN_CORE_FNVLIT(alwan_cat_spow)(Fb, ALWAN_CORE_LITERAL(0.12));
    p.Qz_p = (ALWAN_CORE_LITERAL(1.6) * Fs) / Fb_pow_012;
    p.FL_pow_02 = ALWAN_CORE_FNVLIT(alwan_cat_spow)(FL, ALWAN_CORE_LITERAL(0.2));
    p.FL_pow_06 = ALWAN_CORE_FNVLIT(alwan_cat_spow)(FL, ALWAN_CORE_LITERAL(0.6));
    p.Qz_m = ALWAN_CORE_FNVLIT(alwan_cat_spow)(Fs, ALWAN_CORE_LITERAL(2.2)) *
             ALWAN_CORE_FNVLIT(alwan_cat_spow)(Fb, ALWAN_CORE_LITERAL(0.5)) * p.FL_pow_02;
    p.Qzw = ALWAN_CORE_LITERAL(2700.0) * ALWAN_CORE_FNVLIT(alwan_cat_spow)(p.Izw, p.Qz_p) * p.Qz_m;
    p.Fb_pow_01 = ALWAN_CORE_FNVLIT(alwan_cat_spow)(Fb, ALWAN_CORE_LITERAL(0.1));
    p.Izw_pow_078 = ALWAN_CORE_FNVLIT(alwan_cat_spow)(p.Izw, ALWAN_CORE_LITERAL(0.78));
    p.Iz_p = Fb_pow_012 / (ALWAN_CORE_LITERAL(1.6) * Fs);
    p.Iz_d = ALWAN_CORE_LITERAL(2700.0) * ALWAN_CORE_LITERAL(100.0) * p.Qz_m;
    return p;
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

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) ALWAN_CORE_FNVLIT(alwan_zcam_forward_params)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_FNLIT(alwan_zcam_v_params) p) {

    ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) result;

    /* Step 0: the stimulus to D65, by the two-step CAT over CAT02. The white is
     * NOT adapted: the model takes I_z,w from the white as given. */
    ALWAN_CORE_XYZ xyz_d65 = ALWAN_CORE_FNVLIT(alwan_cat_zhai2018_apply)(
        ALWAN_CORE_FNLIT(ZCAM_V_CAT02), ALWAN_CORE_FNLIT(ZCAM_V_CAT02_INV), xyz, p.gains_to_d65);

    /* Step 2: achromatic response and opponent signals */
    ALWAN_CORE_VEC3 izazbz = ALWAN_CORE_FNVLIT(zcam_xyz_to_izazbz)(xyz_d65);
    ALWAN_CORE_T Iz = izazbz.v[0];
    ALWAN_CORE_T az = izazbz.v[1];
    ALWAN_CORE_T bz = izazbz.v[2];

    /* Step 3: hue angle, degrees in [0, 360) */
    ALWAN_CORE_T hz_raw = ALWAN_CORE_ATAN2(bz, az) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    result.hz = ALWAN_CORE_SELECT(hz_raw < ALWAN_CORE_ZERO, hz_raw + ALWAN_CORE_LITERAL(360.0), hz_raw);

    /* Step 4: eccentricity */
    ALWAN_CORE_T ez = ALWAN_CORE_FNVLIT(zcam_eccentricity)(result.hz);

    /* Step 5: brightness Q_z */
    result.Qz = ALWAN_CORE_LITERAL(2700.0) * ALWAN_CORE_FNVLIT(alwan_cat_spow)(Iz, p.Qz_p) * p.Qz_m;

    /* Step 6: lightness J_z */
    result.Jz = ALWAN_CORE_LITERAL(100.0) * (result.Qz / p.Qzw);

    /* Step 7: colourfulness M_z */
    result.Mz = ALWAN_CORE_LITERAL(100.0) *
                ALWAN_CORE_FNVLIT(alwan_cat_spow)(az * az + bz * bz, ALWAN_CORE_LITERAL(0.37)) *
                ((ALWAN_CORE_FNVLIT(alwan_cat_spow)(ez, ALWAN_CORE_LITERAL(0.068)) * p.FL_pow_02) /
                 (p.Fb_pow_01 * p.Izw_pow_078));

    /* Step 8: chroma C_z */
    result.Cz = ALWAN_CORE_LITERAL(100.0) * (result.Mz / p.Qzw);

    /* Step 9: saturation S_z. A stimulus with no brightness has none. */
    ALWAN_CORE_T Mz_over_Qz = ALWAN_CORE_SELECT(result.Qz > ALWAN_CORE_LITERAL(1e-10),
                                            result.Mz / result.Qz, ALWAN_CORE_ZERO);
    result.Sz = ALWAN_CORE_LITERAL(100.0) * p.FL_pow_06 * ALWAN_CORE_SQRT(Mz_over_Qz);

    /* Step 10: vividness V_z, blackness K_z, whiteness W_z */
    ALWAN_CORE_T J_diff = result.Jz - ALWAN_CORE_LITERAL(58.0);
    result.Vz = ALWAN_CORE_SQRT(J_diff * J_diff + ALWAN_CORE_LITERAL(3.4) * result.Cz * result.Cz);

    result.Kz = ALWAN_CORE_LITERAL(100.0) -
                ALWAN_CORE_LITERAL(0.8) * ALWAN_CORE_SQRT(result.Jz * result.Jz +
                                                 ALWAN_CORE_LITERAL(8.0) * result.Cz * result.Cz);

    ALWAN_CORE_T J_diff_w = ALWAN_CORE_LITERAL(100.0) - result.Jz;
    result.Wz = ALWAN_CORE_LITERAL(100.0) - ALWAN_CORE_SQRT(J_diff_w * J_diff_w + result.Cz * result.Cz);

    return result;
}

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) ALWAN_CORE_FNVLIT(alwan_zcam_forward)(
    ALWAN_CORE_XYZ xyz,
    ALWAN_CORE_XYZ xyz_w,
    ALWAN_CORE_T Fs,
    ALWAN_CORE_T D,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Y_b) {
    return ALWAN_CORE_FNVLIT(alwan_zcam_forward_params)(xyz,
        ALWAN_CORE_FNVLIT(alwan_zcam_params)(xyz_w, Fs, D, La, Y_b));
}

/* ----------------------------------------------------------------
 * ZCAM Inverse Transform: Correlates -> XYZ (value-returning)
 *
 * Reads J_z, M_z and h_z, the three that alwan_zcam_from_ucs fills, and
 * nothing else. Same viewing-condition arguments as the forward model, and the
 * exact inverse of it: every step is closed form.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_zcam_inverse_params)(
    ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) correlates,
    ALWAN_CORE_FNLIT(alwan_zcam_v_params) p) {

    /* Step 1: achromatic response I_z, from J_z through the white's brightness */
    ALWAN_CORE_T Iz = ALWAN_CORE_FNVLIT(alwan_cat_spow)((correlates.Jz * p.Qzw) / p.Iz_d, p.Iz_p);

    /* Step 2: opponent signals, from M_z and h_z. 50/37 is 1 / (2 * 0.37). */
    ALWAN_CORE_T ez = ALWAN_CORE_FNVLIT(zcam_eccentricity)(correlates.hz);
    ALWAN_CORE_T hz_rad = correlates.hz * ALWAN_CORE_PI / ALWAN_CORE_LITERAL(180.0);
    ALWAN_CORE_T Cz_p = ALWAN_CORE_FNVLIT(alwan_cat_spow)(
        (correlates.Mz * p.Izw_pow_078 * p.Fb_pow_01) /
        (ALWAN_CORE_LITERAL(100.0) * ALWAN_CORE_FNVLIT(alwan_cat_spow)(ez, ALWAN_CORE_LITERAL(0.068)) * p.FL_pow_02),
        ALWAN_CORE_LITERAL(50.0) / ALWAN_CORE_LITERAL(37.0));

    ALWAN_CORE_VEC3 izazbz;
    izazbz.v[0] = Iz;
    izazbz.v[1] = Cz_p * ALWAN_CORE_COS(hz_rad);
    izazbz.v[2] = Cz_p * ALWAN_CORE_SIN(hz_rad);

    /* Step 3: to D65 XYZ, then back under the viewing illuminant */
    ALWAN_CORE_XYZ xyz_d65 = ALWAN_CORE_FNVLIT(zcam_izazbz_to_xyz)(izazbz);
    return ALWAN_CORE_FNVLIT(alwan_cat_zhai2018_apply)(
        ALWAN_CORE_FNLIT(ZCAM_V_CAT02), ALWAN_CORE_FNLIT(ZCAM_V_CAT02_INV), xyz_d65, p.gains_from_d65);
}

ALWAN_INLINE ALWAN_CORE_XYZ ALWAN_CORE_FNVLIT(alwan_zcam_inverse)(
    ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) correlates,
    ALWAN_CORE_XYZ xyz_w,
    ALWAN_CORE_T Fs,
    ALWAN_CORE_T D,
    ALWAN_CORE_T La,
    ALWAN_CORE_T Y_b) {
    return ALWAN_CORE_FNVLIT(alwan_zcam_inverse_params)(correlates,
        ALWAN_CORE_FNVLIT(alwan_zcam_params)(xyz_w, Fs, D, La, Y_b));
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

/* ZCAM-UCS back to correlates: Jz, Mz and hz exactly, the rest zero. */
ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) ALWAN_CORE_FNVLIT(alwan_zcam_from_ucs)(ALWAN_CORE_JZAZBZ Jab) {
    ALWAN_CORE_FNLIT(alwan_zcam_v_correlates) result;

    ALWAN_CORE_T hz = ALWAN_CORE_ATAN2(Jab.bz, Jab.az) * ALWAN_CORE_LITERAL(180.0) / ALWAN_CORE_PI;
    hz = ALWAN_CORE_SELECT(hz < ALWAN_CORE_ZERO, hz + ALWAN_CORE_LITERAL(360.0), hz);
    result.Jz = Jab.Jz;
    result.Mz = ALWAN_CORE_SQRT(Jab.az * Jab.az + Jab.bz * Jab.bz);
    result.hz = hz;
    result.Cz = ALWAN_CORE_ZERO; result.Qz = ALWAN_CORE_ZERO; result.Sz = ALWAN_CORE_ZERO;
    result.Vz = ALWAN_CORE_ZERO; result.Kz = ALWAN_CORE_ZERO; result.Wz = ALWAN_CORE_ZERO;

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
