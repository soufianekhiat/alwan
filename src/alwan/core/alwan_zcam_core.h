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
 * ZCAM Constants
 * ---------------------------------------------------------------- */

/* Jzazbz/ZCAM chromatic adaptation parameters */
static alwan_scalar const ZCAM_V_B = ALWAN_LITERAL(1.15);
static alwan_scalar const ZCAM_V_G = ALWAN_LITERAL(0.66);

/* PQ (Perceptual Quantizer) constants from ST 2084 */
static alwan_scalar const ZCAM_V_PQ_C1 = ALWAN_LITERAL(0.8359375);              /* 3424/4096 */
static alwan_scalar const ZCAM_V_PQ_C2 = ALWAN_LITERAL(18.8515625);             /* 2413/128 */
static alwan_scalar const ZCAM_V_PQ_C3 = ALWAN_LITERAL(18.6875);                /* 2392/128 */
static alwan_scalar const ZCAM_V_PQ_N  = ALWAN_LITERAL(0.1593017578125);        /* 2610/16384 */
static alwan_scalar const ZCAM_V_PQ_P  = ALWAN_LITERAL(134.034375);             /* 1.7*2523/32 */

/* Lightness transformation constants */
static alwan_scalar const ZCAM_V_PQ_D  = ALWAN_LITERAL(-0.56);
static alwan_scalar const ZCAM_V_PQ_D0 = ALWAN_LITERAL(1.6295499532821566e-11);

/* ----------------------------------------------------------------
 * ZCAM Transformation Matrices
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* XYZ to LMS matrix (from Jzazbz) */
static alwan_scalar const ZCAM_V_XYZ_TO_LMS[9] = {
    ALWAN_LITERAL( 0.41478972), ALWAN_LITERAL( 0.579999),  ALWAN_LITERAL( 0.014648),
    ALWAN_LITERAL(-0.20151),    ALWAN_LITERAL( 1.120649),  ALWAN_LITERAL( 0.0531008),
    ALWAN_LITERAL(-0.0166008),  ALWAN_LITERAL( 0.2648),    ALWAN_LITERAL( 0.6684799)
};

/* LMS' to Izazbz matrix */
static alwan_scalar const ZCAM_V_LMS_P_TO_IZAZBZ[9] = {
    ALWAN_LITERAL( 0.5),       ALWAN_LITERAL( 0.5),       ALWAN_LITERAL( 0.0),
    ALWAN_LITERAL( 3.524),     ALWAN_LITERAL(-4.066708),   ALWAN_LITERAL( 0.542708),
    ALWAN_LITERAL( 0.199076),  ALWAN_LITERAL( 1.096799),   ALWAN_LITERAL(-1.295875)
};

/* Inverse: LMS to XYZ */
static alwan_scalar const ZCAM_V_LMS_TO_XYZ[9] = {
    ALWAN_LITERAL( 1.9242264357876069),  ALWAN_LITERAL(-1.0047923125953657), ALWAN_LITERAL( 0.0376514040306180),
    ALWAN_LITERAL( 0.3503167620949991),  ALWAN_LITERAL( 0.7264811939316552), ALWAN_LITERAL(-0.0653844229480850),
    ALWAN_LITERAL(-0.0909828109828476),  ALWAN_LITERAL(-0.3127282905230740), ALWAN_LITERAL( 1.5227665613052603)
};

/* Inverse: Izazbz to LMS' */
static alwan_scalar const ZCAM_V_IZAZBZ_TO_LMS_P[9] = {
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL( 0.1386050432715393), ALWAN_LITERAL( 0.0580473161561189),
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL(-0.1386050432715393), ALWAN_LITERAL(-0.0580473161561189),
    ALWAN_LITERAL( 1.0000000000000000),  ALWAN_LITERAL(-0.0960192420263190), ALWAN_LITERAL(-0.8118918960560390)
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Local PQ Transfer Functions
 * ---------------------------------------------------------------- */

/* PQ OETF: linear luminance -> encoded (forward PQ) */
ALWAN_INLINE alwan_scalar zcam_pq_forward_v(alwan_scalar L) {
    alwan_scalar L_safe = ALWAN_SELECT(L <= ALWAN_ZERO, ALWAN_ZERO, L);
    alwan_scalar L_norm = L_safe / ALWAN_LITERAL(10000.0);
    alwan_scalar L_pow_n = ALWAN_POW(L_norm, ZCAM_V_PQ_N);
    alwan_scalar numerator = ZCAM_V_PQ_C1 + ZCAM_V_PQ_C2 * L_pow_n;
    alwan_scalar denominator = ALWAN_ONE + ZCAM_V_PQ_C3 * L_pow_n;
    return ALWAN_SELECT(L <= ALWAN_ZERO, ALWAN_ZERO,
                        ALWAN_POW(numerator / denominator, ZCAM_V_PQ_P));
}

/* PQ EOTF: encoded -> linear luminance (inverse PQ) */
ALWAN_INLINE alwan_scalar zcam_pq_inverse_v(alwan_scalar E) {
    alwan_scalar enc = ALWAN_SELECT(E <= ALWAN_ZERO, ALWAN_ZERO, E);
    alwan_scalar E_pow = ALWAN_POW(enc, ALWAN_ONE / ZCAM_V_PQ_P);
    alwan_scalar numerator = E_pow - ZCAM_V_PQ_C1;
    alwan_scalar denominator = ZCAM_V_PQ_C2 - ZCAM_V_PQ_C3 * E_pow;
    alwan_scalar ratio = ALWAN_SELECT(ALWAN_ABS(denominator) < ALWAN_EPSILON,
                                      ALWAN_ZERO, numerator / denominator);
    ratio = ALWAN_SELECT(ratio < ALWAN_ZERO, ALWAN_ZERO, ratio);
    return ALWAN_SELECT(E <= ALWAN_ZERO, ALWAN_ZERO,
                        ALWAN_LITERAL(10000.0) * ALWAN_POW(ratio, ALWAN_ONE / ZCAM_V_PQ_N));
}

/* ----------------------------------------------------------------
 * Iz <-> Jz Helpers
 * ---------------------------------------------------------------- */

/* Convert Iz to Jz */
ALWAN_INLINE alwan_scalar zcam_iz_to_jz_v(alwan_scalar Iz) {
    alwan_scalar numerator = (ALWAN_ONE + ZCAM_V_PQ_D) * Iz;
    alwan_scalar denominator = ALWAN_ONE + ZCAM_V_PQ_D * Iz;
    return (numerator / denominator) - ZCAM_V_PQ_D0;
}

/* Convert Jz to Iz (inverse) */
ALWAN_INLINE alwan_scalar zcam_jz_to_iz_v(alwan_scalar Jz) {
    alwan_scalar Jz_adj = Jz + ZCAM_V_PQ_D0;
    return Jz_adj / ((ALWAN_ONE + ZCAM_V_PQ_D) - ZCAM_V_PQ_D * Jz_adj);
}

/* ----------------------------------------------------------------
 * Eccentricity Factor
 * ---------------------------------------------------------------- */

/* Compute eccentricity factor from hue angle in degrees */
ALWAN_INLINE alwan_scalar zcam_eccentricity_v(alwan_scalar h_degrees) {
    alwan_scalar h_rad = (h_degrees + ALWAN_LITERAL(89.038)) * ALWAN_PI / ALWAN_LITERAL(180.0);
    return ALWAN_LITERAL(1.015) + ALWAN_COS(h_rad);
}

/* ----------------------------------------------------------------
 * ZCAM Forward Transform: XYZ -> Correlates (value-returning)
 *
 * Parameters:
 *   xyz     - absolute XYZ tristimulus values (cd/m^2)
 *   xyz_w   - white point XYZ (absolute, cd/m^2)
 *   Fs      - surround factor (Average=1.0, Dim=0.9, Dark=0.8)
 *   c       - surround chroma induction (Average=0.69, Dim=0.59, Dark=0.525)
 *   Nc      - surround chromatic induction (Average=1.0, Dim=0.9, Dark=0.8)
 *   F       - surround adaptation factor (Average=1.0, Dim=0.9, Dark=0.8)
 *   La      - adapting luminance (cd/m^2)
 *   Y_b     - background luminance factor
 *   Y_w     - white point luminance (xyz_w.y)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_zcam_v_correlates alwan_zcam_forward_v(
    alwan_xyz xyz,
    alwan_xyz xyz_w,
    alwan_scalar Fs,
    alwan_scalar c,
    alwan_scalar Nc,
    alwan_scalar F,
    alwan_scalar La,
    alwan_scalar Y_b,
    alwan_scalar Y_w) {

    alwan_zcam_v_correlates result;

    /* c, Nc, F reserved for full ZCAM surround model */
    (void)c; (void)Nc; (void)F;

    /* Step 1: Viewing condition factors */
    alwan_scalar Fb = ALWAN_SQRT(Y_b / Y_w);
    alwan_scalar FL = ALWAN_LITERAL(0.171) *
                      ALWAN_POW(La, ALWAN_ONE / ALWAN_LITERAL(3.0)) *
                      (ALWAN_ONE - ALWAN_EXP(-ALWAN_LITERAL(48.0) / ALWAN_LITERAL(9.0) * La));

    /* Step 2: Chromatic adaptation */
    alwan_scalar xa = ZCAM_V_B * xyz.x - (ZCAM_V_B - ALWAN_ONE) * xyz.z;
    alwan_scalar ya = ZCAM_V_G * xyz.y - (ZCAM_V_G - ALWAN_ONE) * xyz.x;
    alwan_scalar za = xyz.z;

    /* Step 3: Adapted XYZ -> LMS */
    alwan_scalar l = ZCAM_V_XYZ_TO_LMS[0] * xa + ZCAM_V_XYZ_TO_LMS[1] * ya + ZCAM_V_XYZ_TO_LMS[2] * za;
    alwan_scalar m = ZCAM_V_XYZ_TO_LMS[3] * xa + ZCAM_V_XYZ_TO_LMS[4] * ya + ZCAM_V_XYZ_TO_LMS[5] * za;
    alwan_scalar s = ZCAM_V_XYZ_TO_LMS[6] * xa + ZCAM_V_XYZ_TO_LMS[7] * ya + ZCAM_V_XYZ_TO_LMS[8] * za;

    /* Step 4: PQ nonlinearity: LMS -> LMS' */
    alwan_scalar lp = zcam_pq_forward_v(l);
    alwan_scalar mp = zcam_pq_forward_v(m);
    alwan_scalar sp = zcam_pq_forward_v(s);

    /* Step 5: LMS' -> Izazbz */
    alwan_scalar Iz = ZCAM_V_LMS_P_TO_IZAZBZ[0] * lp + ZCAM_V_LMS_P_TO_IZAZBZ[1] * mp + ZCAM_V_LMS_P_TO_IZAZBZ[2] * sp;
    alwan_scalar az = ZCAM_V_LMS_P_TO_IZAZBZ[3] * lp + ZCAM_V_LMS_P_TO_IZAZBZ[4] * mp + ZCAM_V_LMS_P_TO_IZAZBZ[5] * sp;
    alwan_scalar bz = ZCAM_V_LMS_P_TO_IZAZBZ[6] * lp + ZCAM_V_LMS_P_TO_IZAZBZ[7] * mp + ZCAM_V_LMS_P_TO_IZAZBZ[8] * sp;

    /* Step 6: White point -> Izw */
    alwan_scalar xaw = ZCAM_V_B * xyz_w.x - (ZCAM_V_B - ALWAN_ONE) * xyz_w.z;
    alwan_scalar yaw = ZCAM_V_G * xyz_w.y - (ZCAM_V_G - ALWAN_ONE) * xyz_w.x;
    alwan_scalar zaw = xyz_w.z;

    alwan_scalar lw = ZCAM_V_XYZ_TO_LMS[0] * xaw + ZCAM_V_XYZ_TO_LMS[1] * yaw + ZCAM_V_XYZ_TO_LMS[2] * zaw;
    alwan_scalar mw = ZCAM_V_XYZ_TO_LMS[3] * xaw + ZCAM_V_XYZ_TO_LMS[4] * yaw + ZCAM_V_XYZ_TO_LMS[5] * zaw;
    alwan_scalar sw = ZCAM_V_XYZ_TO_LMS[6] * xaw + ZCAM_V_XYZ_TO_LMS[7] * yaw + ZCAM_V_XYZ_TO_LMS[8] * zaw;

    alwan_scalar lwp = zcam_pq_forward_v(lw);
    alwan_scalar mwp = zcam_pq_forward_v(mw);
    alwan_scalar swp = zcam_pq_forward_v(sw);

    alwan_scalar Izw = ZCAM_V_LMS_P_TO_IZAZBZ[0] * lwp + ZCAM_V_LMS_P_TO_IZAZBZ[1] * mwp + ZCAM_V_LMS_P_TO_IZAZBZ[2] * swp;

    /* Step 7: Hue angle (degrees, [0, 360)) */
    alwan_scalar hz_raw = ALWAN_ATAN2(bz, az) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.hz = ALWAN_SELECT(hz_raw < ALWAN_ZERO, hz_raw + ALWAN_LITERAL(360.0), hz_raw);

    /* Step 8: Eccentricity factor */
    alwan_scalar ez = zcam_eccentricity_v(result.hz);

    /* Step 9: Brightness Qz */
    alwan_scalar Fb_pow_012 = ALWAN_POW(Fb, ALWAN_LITERAL(0.12));
    alwan_scalar Iz_exp = ALWAN_LITERAL(1.6) * Fs / Fb_pow_012;
    alwan_scalar Iz_pow = ALWAN_POW(Iz, Iz_exp);
    alwan_scalar Fs_pow_22 = ALWAN_POW(Fs, ALWAN_LITERAL(2.2));
    alwan_scalar Fb_sqrt = ALWAN_SQRT(Fb);
    alwan_scalar FL_pow_02 = ALWAN_POW(FL, ALWAN_LITERAL(0.2));
    result.Qz = ALWAN_LITERAL(2700.0) * Iz_pow * Fs_pow_22 * Fb_sqrt * FL_pow_02;

    /* Step 10: White point brightness Qzw */
    alwan_scalar Izw_pow = ALWAN_POW(Izw, Iz_exp);
    alwan_scalar Qzw = ALWAN_LITERAL(2700.0) * Izw_pow * Fs_pow_22 * Fb_sqrt * FL_pow_02;

    /* Step 11: Lightness Jz */
    result.Jz = ALWAN_LITERAL(100.0) * (result.Qz / Qzw);

    /* Step 12: Colorfulness Mz */
    alwan_scalar chroma_component = ALWAN_POW(az * az + bz * bz, ALWAN_LITERAL(0.37));
    alwan_scalar ez_pow = ALWAN_POW(ez, ALWAN_LITERAL(0.068));
    alwan_scalar Izw_pow_078 = ALWAN_POW(Izw, ALWAN_LITERAL(0.78));
    alwan_scalar Fb_pow_01 = ALWAN_POW(Fb, ALWAN_LITERAL(0.1));
    result.Mz = ALWAN_LITERAL(100.0) * chroma_component * ez_pow * FL_pow_02 /
                (Izw_pow_078 * Fb_pow_01);

    /* Step 13: Chroma Cz */
    result.Cz = ALWAN_LITERAL(100.0) * result.Mz / Qzw;

    /* Step 14: Saturation Sz */
    alwan_scalar Mz_over_Qz = ALWAN_SELECT(result.Qz > ALWAN_LITERAL(1e-10),
                                            result.Mz / result.Qz, ALWAN_ZERO);
    alwan_scalar FL_pow_06 = ALWAN_POW(FL, ALWAN_LITERAL(0.6));
    result.Sz = ALWAN_SELECT(result.Qz > ALWAN_LITERAL(1e-10),
                             ALWAN_LITERAL(100.0) * ALWAN_POW(Mz_over_Qz, ALWAN_LITERAL(0.5)) * FL_pow_06,
                             ALWAN_ZERO);

    /* Step 15: Vividness Vz */
    alwan_scalar J_diff = result.Jz - ALWAN_LITERAL(58.0);
    result.Vz = ALWAN_SQRT(J_diff * J_diff + ALWAN_LITERAL(3.4) * result.Cz * result.Cz);

    /* Step 16: Blackness Kz */
    result.Kz = ALWAN_LITERAL(100.0) -
                ALWAN_LITERAL(0.8) * ALWAN_SQRT(result.Jz * result.Jz +
                                                 ALWAN_LITERAL(8.0) * result.Cz * result.Cz);

    /* Step 17: Whiteness Wz */
    alwan_scalar J_diff_w = ALWAN_LITERAL(100.0) - result.Jz;
    result.Wz = ALWAN_LITERAL(100.0) - ALWAN_SQRT(J_diff_w * J_diff_w + result.Cz * result.Cz);

    return result;
}

/* ----------------------------------------------------------------
 * ZCAM Inverse Transform: Correlates -> XYZ (value-returning)
 *
 * Simplified inverse using Jz and Mz+hz as input correlates.
 *
 * Parameters:
 *   correlates - ZCAM appearance correlates (uses Jz, Mz, hz)
 *   xyz_w      - white point XYZ (absolute, cd/m^2)
 *   Fs         - surround factor
 *   c          - surround chroma induction
 *   Nc         - surround chromatic induction
 *   F          - surround adaptation factor
 *   La         - adapting luminance (cd/m^2)
 *   Y_b        - background luminance factor
 *   Y_w        - white point luminance (xyz_w.y)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_xyz alwan_zcam_inverse_v(
    alwan_zcam_v_correlates correlates,
    alwan_xyz xyz_w,
    alwan_scalar Fs,
    alwan_scalar c,
    alwan_scalar Nc,
    alwan_scalar F,
    alwan_scalar La,
    alwan_scalar Y_b,
    alwan_scalar Y_w) {

    alwan_xyz result;

    /* Viewing condition params reserved for full ZCAM inverse model */
    (void)xyz_w; (void)Fs; (void)c; (void)Nc; (void)F;
    (void)La; (void)Y_b; (void)Y_w;

    /* Step 1: Convert Jz to Iz (Jz is on 0-100 scale, normalize first) */
    alwan_scalar Iz = zcam_jz_to_iz_v(correlates.Jz / ALWAN_LITERAL(100.0));

    /* Step 2: Recover az, bz from Mz and hz */
    alwan_scalar hz_rad = correlates.hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar Mz_norm = correlates.Mz / ALWAN_LITERAL(100.0);

    /* Approximate inverse of colorfulness formula */
    alwan_scalar chroma_squared = ALWAN_POW(Mz_norm, ALWAN_ONE / ALWAN_LITERAL(0.37));
    alwan_scalar chroma_mag = ALWAN_SQRT(chroma_squared);

    alwan_scalar az = chroma_mag * ALWAN_COS(hz_rad);
    alwan_scalar bz = chroma_mag * ALWAN_SIN(hz_rad);

    /* Step 3: Izazbz -> LMS' */
    alwan_scalar lp = ZCAM_V_IZAZBZ_TO_LMS_P[0] * Iz + ZCAM_V_IZAZBZ_TO_LMS_P[1] * az + ZCAM_V_IZAZBZ_TO_LMS_P[2] * bz;
    alwan_scalar mp = ZCAM_V_IZAZBZ_TO_LMS_P[3] * Iz + ZCAM_V_IZAZBZ_TO_LMS_P[4] * az + ZCAM_V_IZAZBZ_TO_LMS_P[5] * bz;
    alwan_scalar sp = ZCAM_V_IZAZBZ_TO_LMS_P[6] * Iz + ZCAM_V_IZAZBZ_TO_LMS_P[7] * az + ZCAM_V_IZAZBZ_TO_LMS_P[8] * bz;

    /* Step 4: Inverse PQ: LMS' -> LMS */
    alwan_scalar l = zcam_pq_inverse_v(lp);
    alwan_scalar m = zcam_pq_inverse_v(mp);
    alwan_scalar s = zcam_pq_inverse_v(sp);

    /* Step 5: LMS -> adapted XYZ */
    alwan_scalar xa = ZCAM_V_LMS_TO_XYZ[0] * l + ZCAM_V_LMS_TO_XYZ[1] * m + ZCAM_V_LMS_TO_XYZ[2] * s;
    alwan_scalar ya = ZCAM_V_LMS_TO_XYZ[3] * l + ZCAM_V_LMS_TO_XYZ[4] * m + ZCAM_V_LMS_TO_XYZ[5] * s;
    alwan_scalar za = ZCAM_V_LMS_TO_XYZ[6] * l + ZCAM_V_LMS_TO_XYZ[7] * m + ZCAM_V_LMS_TO_XYZ[8] * s;

    /* Step 6: Inverse chromatic adaptation */
    result.z = za;
    result.x = (xa + (ZCAM_V_B - ALWAN_ONE) * result.z) / ZCAM_V_B;
    result.y = (ya + (ZCAM_V_G - ALWAN_ONE) * result.x) / ZCAM_V_G;

    return result;
}

/* ----------------------------------------------------------------
 * ZCAM to UCS (Uniform Color Space) for color difference
 * (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_jzazbz alwan_zcam_to_ucs_v(alwan_zcam_v_correlates correlates) {
    alwan_jzazbz result;

    alwan_scalar hz_rad = correlates.hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    result.Jz = correlates.Jz;
    result.az = correlates.Mz * ALWAN_COS(hz_rad);
    result.bz = correlates.Mz * ALWAN_SIN(hz_rad);

    return result;
}

#endif /* ALWAN_ZCAM_CORE_H */
