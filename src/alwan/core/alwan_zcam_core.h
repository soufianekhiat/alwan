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
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

typedef struct {
    alwan_scalar Jz;
    alwan_scalar Cz;
    alwan_scalar hz;
    alwan_scalar Qz;
    alwan_scalar Mz;
    alwan_scalar Sz;
    alwan_scalar Vz;
    alwan_scalar Kz;
    alwan_scalar Wz;
} alwan_zcam_v_correlates;

static alwan_scalar const ZCAM_V_B = ALWAN_LITERAL(1.15);
static alwan_scalar const ZCAM_V_G = ALWAN_LITERAL(0.66);
static alwan_scalar const ZCAM_V_PQ_C1 = ALWAN_LITERAL(0.8359375);
static alwan_scalar const ZCAM_V_PQ_C2 = ALWAN_LITERAL(18.8515625);
static alwan_scalar const ZCAM_V_PQ_C3 = ALWAN_LITERAL(18.6875);
static alwan_scalar const ZCAM_V_PQ_N  = ALWAN_LITERAL(0.1593017578125);
static alwan_scalar const ZCAM_V_PQ_P  = ALWAN_LITERAL(134.034375);
static alwan_scalar const ZCAM_V_PQ_D  = ALWAN_LITERAL(-0.56);
static alwan_scalar const ZCAM_V_PQ_D0 = ALWAN_LITERAL(1.6295499532821566e-11);

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_XYZ_TO_LMS = {{
#include "../data/matrices/jzazbz_xyz_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_LMS_P_TO_IZAZBZ = {{
#include "../data/matrices/jzazbz_lms_p_to_izazbz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_LMS_TO_XYZ = {{
#include "../data/matrices/jzazbz_lms_to_xyz.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ZCAM_V_IZAZBZ_TO_LMS_P = {{
#include "../data/matrices/jzazbz_izazbz_to_lms_p.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_scalar zcam_pq_forward_v(alwan_scalar L) {
    alwan_scalar L_safe = ALWAN_SELECT(L <= ALWAN_ZERO, ALWAN_ZERO, L);
    alwan_scalar L_norm = L_safe / ALWAN_LITERAL(10000.0);
    alwan_scalar L_pow_n = ALWAN_POW(L_norm, ZCAM_V_PQ_N);
    alwan_scalar numerator = ZCAM_V_PQ_C1 + ZCAM_V_PQ_C2 * L_pow_n;
    alwan_scalar denominator = ALWAN_ONE + ZCAM_V_PQ_C3 * L_pow_n;
    return ALWAN_SELECT(L <= ALWAN_ZERO, ALWAN_ZERO,
                        ALWAN_POW(numerator / denominator, ZCAM_V_PQ_P));
}

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

ALWAN_INLINE alwan_scalar zcam_iz_to_jz_v(alwan_scalar Iz) {
    alwan_scalar numerator = (ALWAN_ONE + ZCAM_V_PQ_D) * Iz;
    alwan_scalar denominator = ALWAN_ONE + ZCAM_V_PQ_D * Iz;
    return (numerator / denominator) - ZCAM_V_PQ_D0;
}

ALWAN_INLINE alwan_scalar zcam_jz_to_iz_v(alwan_scalar Jz) {
    alwan_scalar Jz_adj = Jz + ZCAM_V_PQ_D0;
    return Jz_adj / ((ALWAN_ONE + ZCAM_V_PQ_D) - ZCAM_V_PQ_D * Jz_adj);
}

ALWAN_INLINE alwan_scalar zcam_eccentricity_v(alwan_scalar h_degrees) {
    alwan_scalar h_rad = (h_degrees + ALWAN_LITERAL(89.038)) * ALWAN_PI / ALWAN_LITERAL(180.0);
    return ALWAN_LITERAL(1.015) + ALWAN_COS(h_rad);
}

ALWAN_INLINE alwan_zcam_v_correlates alwan_zcam_forward_v(
    alwan_xyz xyz, alwan_xyz xyz_w, alwan_scalar Fs, alwan_scalar c,
    alwan_scalar Nc, alwan_scalar F, alwan_scalar La, alwan_scalar Y_b, alwan_scalar Y_w) {
    alwan_zcam_v_correlates result;
    ALWAN_UNUSED(c); ALWAN_UNUSED(Nc); ALWAN_UNUSED(F);
    alwan_scalar Fb = ALWAN_SQRT(Y_b / Y_w);
    alwan_scalar FL = ALWAN_LITERAL(0.171) * ALWAN_POW(La, ALWAN_ONE / ALWAN_LITERAL(3.0)) *
                      (ALWAN_ONE - ALWAN_EXP(-ALWAN_LITERAL(48.0) / ALWAN_LITERAL(9.0) * La));
    alwan_scalar xa = ZCAM_V_B * xyz.x - (ZCAM_V_B - ALWAN_ONE) * xyz.z;
    alwan_scalar ya = ZCAM_V_G * xyz.y - (ZCAM_V_G - ALWAN_ONE) * xyz.x;
    alwan_scalar za = xyz.z;
    alwan_vec3 in_v = {{xa, ya, za}};
    alwan_vec3 out_v = alwan_mat3_mulv_v(ZCAM_V_XYZ_TO_LMS, in_v);
    alwan_scalar l = out_v.v[0]; alwan_scalar m = out_v.v[1]; alwan_scalar s = out_v.v[2];
    alwan_scalar lp = zcam_pq_forward_v(l); alwan_scalar mp = zcam_pq_forward_v(m); alwan_scalar sp = zcam_pq_forward_v(s);
    alwan_vec3 in_v2 = {{lp, mp, sp}};
    alwan_vec3 out_v2 = alwan_mat3_mulv_v(ZCAM_V_LMS_P_TO_IZAZBZ, in_v2);
    alwan_scalar Iz = out_v2.v[0]; alwan_scalar az = out_v2.v[1]; alwan_scalar bz = out_v2.v[2];
    alwan_scalar xaw = ZCAM_V_B * xyz_w.x - (ZCAM_V_B - ALWAN_ONE) * xyz_w.z;
    alwan_scalar yaw = ZCAM_V_G * xyz_w.y - (ZCAM_V_G - ALWAN_ONE) * xyz_w.x;
    alwan_scalar zaw = xyz_w.z;
    alwan_vec3 in_vw = {{xaw, yaw, zaw}};
    alwan_vec3 out_vw = alwan_mat3_mulv_v(ZCAM_V_XYZ_TO_LMS, in_vw);
    alwan_scalar lw = out_vw.v[0]; alwan_scalar mw = out_vw.v[1]; alwan_scalar sw = out_vw.v[2];
    alwan_scalar lwp = zcam_pq_forward_v(lw); alwan_scalar mwp = zcam_pq_forward_v(mw); alwan_scalar swp = zcam_pq_forward_v(sw);
    alwan_vec3 in_vwp = {{lwp, mwp, swp}};
    alwan_vec3 out_vwp = alwan_mat3_mulv_v(ZCAM_V_LMS_P_TO_IZAZBZ, in_vwp);
    alwan_scalar Izw = out_vwp.v[0];
    alwan_scalar hz_raw = ALWAN_ATAN2(bz, az) * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.hz = ALWAN_SELECT(hz_raw < ALWAN_ZERO, hz_raw + ALWAN_LITERAL(360.0), hz_raw);
    alwan_scalar ez = zcam_eccentricity_v(result.hz);
    alwan_scalar Fb_pow_012 = ALWAN_POW(Fb, ALWAN_LITERAL(0.12));
    alwan_scalar Iz_exp = ALWAN_LITERAL(1.6) * Fs / Fb_pow_012;
    alwan_scalar Iz_pow = ALWAN_POW(Iz, Iz_exp);
    alwan_scalar Fs_pow_22 = ALWAN_POW(Fs, ALWAN_LITERAL(2.2));
    alwan_scalar Fb_sqrt = ALWAN_SQRT(Fb);
    alwan_scalar FL_pow_02 = ALWAN_POW(FL, ALWAN_LITERAL(0.2));
    result.Qz = ALWAN_LITERAL(2700.0) * Iz_pow * Fs_pow_22 * Fb_sqrt * FL_pow_02;
    alwan_scalar Izw_pow = ALWAN_POW(Izw, Iz_exp);
    alwan_scalar Qzw = ALWAN_LITERAL(2700.0) * Izw_pow * Fs_pow_22 * Fb_sqrt * FL_pow_02;
    result.Jz = ALWAN_LITERAL(100.0) * (result.Qz / Qzw);
    alwan_scalar chroma_component = ALWAN_POW(az * az + bz * bz, ALWAN_LITERAL(0.37));
    alwan_scalar ez_pow = ALWAN_POW(ez, ALWAN_LITERAL(0.068));
    alwan_scalar Izw_pow_078 = ALWAN_POW(Izw, ALWAN_LITERAL(0.78));
    alwan_scalar Fb_pow_01 = ALWAN_POW(Fb, ALWAN_LITERAL(0.1));
    result.Mz = ALWAN_LITERAL(100.0) * chroma_component * ez_pow * FL_pow_02 / (Izw_pow_078 * Fb_pow_01);
    result.Cz = ALWAN_LITERAL(100.0) * result.Mz / Qzw;
    alwan_scalar Mz_over_Qz = ALWAN_SELECT(result.Qz > ALWAN_LITERAL(1e-10), result.Mz / result.Qz, ALWAN_ZERO);
    alwan_scalar FL_pow_06 = ALWAN_POW(FL, ALWAN_LITERAL(0.6));
    result.Sz = ALWAN_SELECT(result.Qz > ALWAN_LITERAL(1e-10),
                             ALWAN_LITERAL(100.0) * ALWAN_POW(Mz_over_Qz, ALWAN_LITERAL(0.5)) * FL_pow_06, ALWAN_ZERO);
    alwan_scalar J_diff = result.Jz - ALWAN_LITERAL(58.0);
    result.Vz = ALWAN_SQRT(J_diff * J_diff + ALWAN_LITERAL(3.4) * result.Cz * result.Cz);
    result.Kz = ALWAN_LITERAL(100.0) - ALWAN_LITERAL(0.8) * ALWAN_SQRT(result.Jz * result.Jz + ALWAN_LITERAL(8.0) * result.Cz * result.Cz);
    alwan_scalar J_diff_w = ALWAN_LITERAL(100.0) - result.Jz;
    result.Wz = ALWAN_LITERAL(100.0) - ALWAN_SQRT(J_diff_w * J_diff_w + result.Cz * result.Cz);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_zcam_inverse_v(
    alwan_zcam_v_correlates correlates, alwan_xyz xyz_w, alwan_scalar Fs, alwan_scalar c,
    alwan_scalar Nc, alwan_scalar F, alwan_scalar La, alwan_scalar Y_b, alwan_scalar Y_w) {
    alwan_xyz result;
    ALWAN_UNUSED(xyz_w); ALWAN_UNUSED(Fs); ALWAN_UNUSED(c); ALWAN_UNUSED(Nc); ALWAN_UNUSED(F);
    ALWAN_UNUSED(La); ALWAN_UNUSED(Y_b); ALWAN_UNUSED(Y_w);
    alwan_scalar Iz = zcam_jz_to_iz_v(correlates.Jz / ALWAN_LITERAL(100.0));
    alwan_scalar hz_rad = correlates.hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    alwan_scalar Mz_norm = correlates.Mz / ALWAN_LITERAL(100.0);
    alwan_scalar chroma_squared = ALWAN_POW(Mz_norm, ALWAN_ONE / ALWAN_LITERAL(0.37));
    alwan_scalar chroma_mag = ALWAN_SQRT(chroma_squared);
    alwan_scalar az = chroma_mag * ALWAN_COS(hz_rad);
    alwan_scalar bz = chroma_mag * ALWAN_SIN(hz_rad);
    alwan_vec3 in_v = {{Iz, az, bz}};
    alwan_vec3 out_v = alwan_mat3_mulv_v(ZCAM_V_IZAZBZ_TO_LMS_P, in_v);
    alwan_scalar lp = out_v.v[0]; alwan_scalar mp = out_v.v[1]; alwan_scalar sp = out_v.v[2];
    alwan_scalar l = zcam_pq_inverse_v(lp); alwan_scalar m = zcam_pq_inverse_v(mp); alwan_scalar s = zcam_pq_inverse_v(sp);
    alwan_vec3 in_v2 = {{l, m, s}};
    alwan_vec3 out_v2 = alwan_mat3_mulv_v(ZCAM_V_LMS_TO_XYZ, in_v2);
    alwan_scalar xa = out_v2.v[0]; alwan_scalar ya = out_v2.v[1]; alwan_scalar za = out_v2.v[2];
    result.z = za;
    result.x = (xa + (ZCAM_V_B - ALWAN_ONE) * result.z) / ZCAM_V_B;
    result.y = (ya + (ZCAM_V_G - ALWAN_ONE) * result.x) / ZCAM_V_G;
    return result;
}

ALWAN_INLINE alwan_jzazbz alwan_zcam_to_ucs_v(alwan_zcam_v_correlates correlates) {
    alwan_jzazbz result;
    alwan_scalar hz_rad = correlates.hz * ALWAN_PI / ALWAN_LITERAL(180.0);
    result.Jz = correlates.Jz;
    result.az = correlates.Mz * ALWAN_COS(hz_rad);
    result.bz = correlates.Mz * ALWAN_SIN(hz_rad);
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_ZCAM_CORE_H */
