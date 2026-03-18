/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only CIE Lab, Luv, xyY, LCh, UCS, UVW, Delta E Color Spaces
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_COLORSPACE_CORE_H
#define ALWAN_COLORSPACE_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_core.h"  /* for alwan_lab_f and alwan_lab_f_inv */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_colorspace_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_colorspace_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_xyy alwan_xyz_to_xyy_v(alwan_xyz xyz) {
    alwan_xyy result;
    alwan_scalar sum = xyz.x + xyz.y + xyz.z;
    result.x = ALWAN_SELECT(sum < ALWAN_EPSILON, ALWAN_LITERAL(0.0), xyz.x / sum);
    result.y = ALWAN_SELECT(sum < ALWAN_EPSILON, ALWAN_LITERAL(0.0), xyz.y / sum);
    result.Y = ALWAN_SELECT(sum < ALWAN_EPSILON, ALWAN_LITERAL(0.0), xyz.y);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_xyy_to_xyz_v(alwan_xyy xyy) {
    alwan_xyz result;
    result.x = ALWAN_SELECT(xyy.y < ALWAN_EPSILON, ALWAN_LITERAL(0.0), (xyy.x * xyy.Y) / xyy.y);
    result.y = ALWAN_SELECT(xyy.y < ALWAN_EPSILON, ALWAN_LITERAL(0.0), xyy.Y);
    result.z = ALWAN_SELECT(xyy.y < ALWAN_EPSILON, ALWAN_LITERAL(0.0), ((ALWAN_LITERAL(1.0) - xyy.x - xyy.y) * xyy.Y) / xyy.y);
    return result;
}

ALWAN_INLINE alwan_lab alwan_xyz_to_lab_v(alwan_xyz xyz, alwan_xyz white) {
    alwan_lab result;
    alwan_scalar xr = xyz.x / white.x;
    alwan_scalar yr = xyz.y / white.y;
    alwan_scalar zr = xyz.z / white.z;
    alwan_scalar fx = alwan_lab_f(xr);
    alwan_scalar fy = alwan_lab_f(yr);
    alwan_scalar fz = alwan_lab_f(zr);
    result.L = ALWAN_LITERAL(116.0) * fy - ALWAN_LITERAL(16.0);
    result.a = ALWAN_LITERAL(500.0) * (fx - fy);
    result.b = ALWAN_LITERAL(200.0) * (fy - fz);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_lab_to_xyz_v(alwan_lab lab, alwan_xyz white) {
    alwan_xyz result;
    alwan_scalar fy = (lab.L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    alwan_scalar fx = lab.a / ALWAN_LITERAL(500.0) + fy;
    alwan_scalar fz = fy - lab.b / ALWAN_LITERAL(200.0);
    result.x = white.x * alwan_lab_f_inv(fx);
    result.y = white.y * alwan_lab_f_inv(fy);
    result.z = white.z * alwan_lab_f_inv(fz);
    return result;
}

ALWAN_INLINE alwan_luv alwan_xyz_to_luv_v(alwan_xyz xyz, alwan_xyz white) {
    alwan_luv result;
    alwan_scalar denom = xyz.x + ALWAN_LITERAL(15.0) * xyz.y + ALWAN_LITERAL(3.0) * xyz.z;
    alwan_scalar u_prime = ALWAN_SELECT(denom < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), (ALWAN_LITERAL(4.0) * xyz.x) / denom);
    alwan_scalar v_prime = ALWAN_SELECT(denom < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), (ALWAN_LITERAL(9.0) * xyz.y) / denom);
    alwan_scalar denom_n = white.x + ALWAN_LITERAL(15.0) * white.y + ALWAN_LITERAL(3.0) * white.z;
    alwan_scalar u_prime_n = ALWAN_SELECT(denom_n < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), (ALWAN_LITERAL(4.0) * white.x) / denom_n);
    alwan_scalar v_prime_n = ALWAN_SELECT(denom_n < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), (ALWAN_LITERAL(9.0) * white.y) / denom_n);
    alwan_scalar yr = xyz.y / white.y;
    alwan_scalar L = ALWAN_LITERAL(116.0) * alwan_lab_f(yr) - ALWAN_LITERAL(16.0);
    result.L = L;
    result.u = ALWAN_LITERAL(13.0) * L * (u_prime - u_prime_n);
    result.v = ALWAN_LITERAL(13.0) * L * (v_prime - v_prime_n);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_luv_to_xyz_v(alwan_luv luv, alwan_xyz white) {
    alwan_xyz result;
    alwan_scalar denom_n = white.x + ALWAN_LITERAL(15.0) * white.y + ALWAN_LITERAL(3.0) * white.z;
    alwan_scalar u_prime_n = ALWAN_SELECT(denom_n < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), (ALWAN_LITERAL(4.0) * white.x) / denom_n);
    alwan_scalar v_prime_n = ALWAN_SELECT(denom_n < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), (ALWAN_LITERAL(9.0) * white.y) / denom_n);
    alwan_scalar fy = (luv.L + ALWAN_LITERAL(16.0)) / ALWAN_LITERAL(116.0);
    alwan_scalar Y = white.y * alwan_lab_f_inv(fy);
    alwan_scalar L13 = ALWAN_LITERAL(13.0) * luv.L;
    alwan_scalar u_prime = ALWAN_SELECT(ALWAN_ABS(luv.L) < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), luv.u / L13 + u_prime_n);
    alwan_scalar v_prime = ALWAN_SELECT(ALWAN_ABS(luv.L) < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), luv.v / L13 + v_prime_n);
    result.x = ALWAN_SELECT(ALWAN_ABS(v_prime) < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), Y * (ALWAN_LITERAL(9.0) * u_prime) / (ALWAN_LITERAL(4.0) * v_prime));
    result.y = Y;
    result.z = ALWAN_SELECT(ALWAN_ABS(v_prime) < ALWAN_EPSILON,
        ALWAN_LITERAL(0.0), Y * (ALWAN_LITERAL(12.0) - ALWAN_LITERAL(3.0) * u_prime - ALWAN_LITERAL(20.0) * v_prime) / (ALWAN_LITERAL(4.0) * v_prime));
    return result;
}

ALWAN_INLINE alwan_lch alwan_lab_to_lch_v(alwan_lab lab) {
    alwan_lch result;
    result.L = lab.L;
    result.C = ALWAN_SQRT(lab.a * lab.a + lab.b * lab.b);
    alwan_scalar h_rad = ALWAN_ATAN2(lab.b, lab.a);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(h_deg < ALWAN_LITERAL(0.0), h_deg + ALWAN_LITERAL(360.0), h_deg);
    return result;
}

ALWAN_INLINE alwan_lab alwan_lch_to_lab_v(alwan_lch lch) {
    alwan_lab result;
    alwan_scalar h_rad = lch.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    result.L = lch.L;
    result.a = lch.C * ALWAN_COS(h_rad);
    result.b = lch.C * ALWAN_SIN(h_rad);
    return result;
}

ALWAN_INLINE alwan_lchuv alwan_luv_to_lchuv_v(alwan_luv luv) {
    alwan_lchuv result;
    result.L = luv.L;
    result.C = ALWAN_SQRT(luv.u * luv.u + luv.v * luv.v);
    alwan_scalar h_rad = ALWAN_ATAN2(luv.v, luv.u);
    alwan_scalar h_deg = h_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    result.h = ALWAN_SELECT(h_deg < ALWAN_LITERAL(0.0), h_deg + ALWAN_LITERAL(360.0), h_deg);
    return result;
}

ALWAN_INLINE alwan_luv alwan_lchuv_to_luv_v(alwan_lchuv lchuv) {
    alwan_luv result;
    alwan_scalar h_rad = lchuv.h * ALWAN_PI / ALWAN_LITERAL(180.0);
    result.L = lchuv.L;
    result.u = lchuv.C * ALWAN_COS(h_rad);
    result.v = lchuv.C * ALWAN_SIN(h_rad);
    return result;
}

ALWAN_INLINE alwan_ucs alwan_xyz_to_ucs_v(alwan_xyz xyz) {
    alwan_ucs result;
    result.U = (ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) * xyz.x;
    result.V = xyz.y;
    result.W = (ALWAN_LITERAL(0.5)) * (-xyz.x + ALWAN_LITERAL(3.0) * xyz.y + xyz.z);
    return result;
}

ALWAN_INLINE alwan_xyz alwan_ucs_to_xyz_v(alwan_ucs ucs) {
    alwan_xyz result;
    result.x = (ALWAN_LITERAL(3.0) / ALWAN_LITERAL(2.0)) * ucs.U;
    result.y = ucs.V;
    result.z = ALWAN_LITERAL(2.0) * ucs.W + result.x - ALWAN_LITERAL(3.0) * ucs.V;
    return result;
}

ALWAN_INLINE alwan_uvw alwan_xyz_to_uvw_v(alwan_xyz xyz, alwan_xyz white) {
    alwan_uvw result;
    alwan_scalar sum = xyz.x + ALWAN_LITERAL(15.0) * xyz.y + ALWAN_LITERAL(3.0) * xyz.z;
    alwan_scalar u = ALWAN_SELECT(ALWAN_ABS(sum) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                                  (ALWAN_LITERAL(4.0) * xyz.x) / sum);
    alwan_scalar v = ALWAN_SELECT(ALWAN_ABS(sum) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                                  (ALWAN_LITERAL(6.0) * xyz.y) / sum);
    alwan_scalar sum_n = white.x + ALWAN_LITERAL(15.0) * white.y + ALWAN_LITERAL(3.0) * white.z;
    alwan_scalar un = ALWAN_SELECT(ALWAN_ABS(sum_n) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                                   (ALWAN_LITERAL(4.0) * white.x) / sum_n);
    alwan_scalar vn = ALWAN_SELECT(ALWAN_ABS(sum_n) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                                   (ALWAN_LITERAL(6.0) * white.y) / sum_n);
    alwan_scalar Y_ratio = ALWAN_SELECT(white.y < ALWAN_EPSILON, ALWAN_LITERAL(0.0), xyz.y / white.y);
    alwan_scalar W = ALWAN_SELECT(xyz.y < ALWAN_EPSILON,
                                  ALWAN_LITERAL(-17.0),
                                  ALWAN_LITERAL(25.0) * ALWAN_CBRT(Y_ratio) - ALWAN_LITERAL(17.0));
    result.U = ALWAN_LITERAL(13.0) * W * (u - un);
    result.V = ALWAN_LITERAL(13.0) * W * (v - vn);
    result.W = W;
    return result;
}

ALWAN_INLINE alwan_xyz alwan_uvw_to_xyz_v(alwan_uvw uvw, alwan_xyz white) {
    alwan_xyz result;
    alwan_scalar W_plus_17 = uvw.W + ALWAN_LITERAL(17.0);
    alwan_scalar Y_cbrt = ALWAN_SELECT(W_plus_17 < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                                       W_plus_17 / ALWAN_LITERAL(25.0));
    alwan_scalar Y = Y_cbrt * Y_cbrt * Y_cbrt;
    alwan_scalar sum_n = white.x + ALWAN_LITERAL(15.0) * white.y + ALWAN_LITERAL(3.0) * white.z;
    alwan_scalar un = ALWAN_SELECT(ALWAN_ABS(sum_n) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                                   (ALWAN_LITERAL(4.0) * white.x) / sum_n);
    alwan_scalar vn = ALWAN_SELECT(ALWAN_ABS(sum_n) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                                   (ALWAN_LITERAL(6.0) * white.y) / sum_n);
    alwan_scalar W13 = ALWAN_LITERAL(13.0) * uvw.W;
    alwan_scalar u = ALWAN_SELECT(ALWAN_ABS(uvw.W) < ALWAN_EPSILON, un, uvw.U / W13 + un);
    alwan_scalar v = ALWAN_SELECT(ALWAN_ABS(uvw.W) < ALWAN_EPSILON, vn, uvw.V / W13 + vn);
    result.x = ALWAN_SELECT(ALWAN_ABS(v) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                            (ALWAN_LITERAL(9.0) * u * Y) / (ALWAN_LITERAL(4.0) * v));
    result.y = Y;
    result.z = ALWAN_SELECT(ALWAN_ABS(v) < ALWAN_EPSILON, ALWAN_LITERAL(0.0),
                            ((ALWAN_LITERAL(12.0) - ALWAN_LITERAL(3.0) * u - ALWAN_LITERAL(20.0) * v) * Y) / (ALWAN_LITERAL(4.0) * v));
    return result;
}

ALWAN_INLINE alwan_lch alwan_xyz_to_lch_v(alwan_xyz xyz, alwan_xyz white) {
    return alwan_lab_to_lch_v(alwan_xyz_to_lab_v(xyz, white));
}

ALWAN_INLINE alwan_xyz alwan_lch_to_xyz_v(alwan_lch lch, alwan_xyz white) {
    return alwan_lab_to_xyz_v(alwan_lch_to_lab_v(lch), white);
}

ALWAN_INLINE alwan_lchuv alwan_xyz_to_lchuv_v(alwan_xyz xyz, alwan_xyz white) {
    return alwan_luv_to_lchuv_v(alwan_xyz_to_luv_v(xyz, white));
}

ALWAN_INLINE alwan_xyz alwan_lchuv_to_xyz_v(alwan_lchuv lchuv, alwan_xyz white) {
    return alwan_luv_to_xyz_v(alwan_lchuv_to_luv_v(lchuv), white);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_76_v(alwan_lab lab1, alwan_lab lab2) {
    alwan_scalar dL = lab1.L - lab2.L;
    alwan_scalar da = lab1.a - lab2.a;
    alwan_scalar db = lab1.b - lab2.b;
    return ALWAN_SQRT(dL * dL + da * da + db * db);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_ok_v(alwan_oklab a, alwan_oklab b) {
    alwan_scalar dL = a.L - b.L;
    alwan_scalar da = a.a - b.a;
    alwan_scalar db = a.b - b.b;
    return ALWAN_SQRT(dL * dL + da * da + db * db);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_itp_v(alwan_ictcp ictcp1, alwan_ictcp ictcp2, alwan_scalar scalar_factor) {
    alwan_scalar dI  = ictcp1.I  - ictcp2.I;
    alwan_scalar dCT = ictcp1.Ct - ictcp2.Ct;
    alwan_scalar dCP = ictcp1.Cp - ictcp2.Cp;
    return scalar_factor * ALWAN_SQRT(dI * dI + ALWAN_LITERAL(0.25) * dCT * dCT + dCP * dCP);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_din99_v(alwan_din99 din99_1, alwan_din99 din99_2) {
    alwan_scalar dL = din99_1.L99 - din99_2.L99;
    alwan_scalar da = din99_1.a99 - din99_2.a99;
    alwan_scalar db = din99_1.b99 - din99_2.b99;
    return ALWAN_SQRT(dL * dL + da * da + db * db);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_zcam_v(alwan_jzazbz jab1, alwan_jzazbz jab2) {
    alwan_scalar dJ = jab1.Jz - jab2.Jz;
    alwan_scalar da = jab1.az - jab2.az;
    alwan_scalar db = jab1.bz - jab2.bz;
    return ALWAN_SQRT(dJ * dJ + da * da + db * db);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_94_v(alwan_lab lab1, alwan_lab lab2) {
    alwan_scalar const K1 = ALWAN_LITERAL(0.045);
    alwan_scalar const K2 = ALWAN_LITERAL(0.015);
    alwan_scalar dL = lab1.L - lab2.L;
    alwan_scalar C1 = ALWAN_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    alwan_scalar C2 = ALWAN_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    alwan_scalar dCab = C1 - C2;
    alwan_scalar da = lab1.a - lab2.a;
    alwan_scalar db = lab1.b - lab2.b;
    alwan_scalar dHab_sq = da * da + db * db - dCab * dCab;
    dHab_sq = ALWAN_SELECT(dHab_sq > ALWAN_LITERAL(0.0), dHab_sq, ALWAN_LITERAL(0.0));
    alwan_scalar SC = ALWAN_LITERAL(1.0) + K1 * C1;
    alwan_scalar SH = ALWAN_LITERAL(1.0) + K2 * C1;
    alwan_scalar term1 = dL;
    alwan_scalar term2 = dCab / SC;
    alwan_scalar term3 = ALWAN_SQRT(dHab_sq) / SH;
    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cmc_v(alwan_lab lab1, alwan_lab lab2, alwan_scalar l, alwan_scalar c) {
    alwan_scalar dL = lab1.L - lab2.L;
    alwan_scalar C1 = ALWAN_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    alwan_scalar C2 = ALWAN_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    alwan_scalar dCab = C1 - C2;
    alwan_scalar da = lab1.a - lab2.a;
    alwan_scalar db = lab1.b - lab2.b;
    alwan_scalar dHab_sq = da * da + db * db - dCab * dCab;
    dHab_sq = ALWAN_SELECT(dHab_sq > ALWAN_LITERAL(0.0), dHab_sq, ALWAN_LITERAL(0.0));
    alwan_scalar h1_rad = ALWAN_ATAN2(lab1.b, lab1.a);
    h1_rad = ALWAN_SELECT(h1_rad < ALWAN_LITERAL(0.0), h1_rad + ALWAN_LITERAL(2.0) * ALWAN_PI, h1_rad);
    alwan_scalar h1 = h1_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    alwan_scalar SL = ALWAN_SELECT(lab1.L < ALWAN_LITERAL(16.0),
                                   ALWAN_LITERAL(0.511),
                                   (ALWAN_LITERAL(0.040975) * lab1.L) / (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.01765) * lab1.L));
    alwan_scalar SC = (ALWAN_LITERAL(0.0638) * C1) / (ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.0131) * C1) + ALWAN_LITERAL(0.638);
    alwan_scalar C1_4 = C1 * C1 * C1 * C1;
    alwan_scalar F = ALWAN_SQRT(C1_4 / (C1_4 + ALWAN_LITERAL(1900.0)));
    alwan_scalar T = ALWAN_SELECT(h1 >= ALWAN_LITERAL(164.0),
                      ALWAN_SELECT(h1 <= ALWAN_LITERAL(345.0),
                        ALWAN_LITERAL(0.56) + ALWAN_ABS(ALWAN_LITERAL(0.2) * ALWAN_COS((h1 + ALWAN_LITERAL(168.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))),
                        ALWAN_LITERAL(0.36) + ALWAN_ABS(ALWAN_LITERAL(0.4) * ALWAN_COS((h1 + ALWAN_LITERAL(35.0)) * ALWAN_PI / ALWAN_LITERAL(180.0)))),
                      ALWAN_LITERAL(0.36) + ALWAN_ABS(ALWAN_LITERAL(0.4) * ALWAN_COS((h1 + ALWAN_LITERAL(35.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))));
    alwan_scalar SH = SC * (F * T + ALWAN_LITERAL(1.0) - F);
    alwan_scalar term1 = dL / (l * SL);
    alwan_scalar term2 = dCab / (c * SC);
    alwan_scalar term3 = ALWAN_SQRT(dHab_sq) / SH;
    return ALWAN_SQRT(term1 * term1 + term2 * term2 + term3 * term3);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_2000_v(alwan_lab lab1, alwan_lab lab2) {
    alwan_scalar C1 = ALWAN_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    alwan_scalar C2 = ALWAN_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    alwan_scalar Cab_mean = (C1 + C2) / ALWAN_LITERAL(2.0);
    alwan_scalar Cab7 = Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean * Cab_mean;
    alwan_scalar G = ALWAN_LITERAL(0.5) * (ALWAN_LITERAL(1.0) - ALWAN_SQRT(Cab7 / (Cab7 + ALWAN_LITERAL(6103515625.0))));
    alwan_scalar a1p = (ALWAN_LITERAL(1.0) + G) * lab1.a;
    alwan_scalar a2p = (ALWAN_LITERAL(1.0) + G) * lab2.a;
    alwan_scalar C1p = ALWAN_SQRT(a1p * a1p + lab1.b * lab1.b);
    alwan_scalar C2p = ALWAN_SQRT(a2p * a2p + lab2.b * lab2.b);
    alwan_scalar h1p_rad = ALWAN_ATAN2(lab1.b, a1p);
    h1p_rad = ALWAN_SELECT(h1p_rad < ALWAN_LITERAL(0.0), h1p_rad + ALWAN_LITERAL(2.0) * ALWAN_PI, h1p_rad);
    alwan_scalar h1p = h1p_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    alwan_scalar h2p_rad = ALWAN_ATAN2(lab2.b, a2p);
    h2p_rad = ALWAN_SELECT(h2p_rad < ALWAN_LITERAL(0.0), h2p_rad + ALWAN_LITERAL(2.0) * ALWAN_PI, h2p_rad);
    alwan_scalar h2p = h2p_rad * ALWAN_LITERAL(180.0) / ALWAN_PI;
    alwan_scalar dLp = lab2.L - lab1.L;
    alwan_scalar dCp = C2p - C1p;
    alwan_scalar abs_dh = ALWAN_ABS(h2p - h1p);
    alwan_scalar achromatic = ALWAN_SELECT(C1p * C2p < ALWAN_EPSILON, ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0));
    alwan_scalar dhp = ALWAN_SELECT(achromatic > ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.0),
                        ALWAN_SELECT(abs_dh <= ALWAN_LITERAL(180.0), h2p - h1p,
                          ALWAN_SELECT(h2p - h1p > ALWAN_LITERAL(180.0), h2p - h1p - ALWAN_LITERAL(360.0),
                                       h2p - h1p + ALWAN_LITERAL(360.0))));
    alwan_scalar dHp = ALWAN_LITERAL(2.0) * ALWAN_SQRT(C1p * C2p) * ALWAN_SIN(dhp * ALWAN_PI / ALWAN_LITERAL(360.0));
    alwan_scalar Lpm = (lab1.L + lab2.L) / ALWAN_LITERAL(2.0);
    alwan_scalar Cpm = (C1p + C2p) / ALWAN_LITERAL(2.0);
    alwan_scalar h_sum = h1p + h2p;
    alwan_scalar Hpm = ALWAN_SELECT(achromatic > ALWAN_LITERAL(0.5), h_sum,
                        ALWAN_SELECT(abs_dh <= ALWAN_LITERAL(180.0), h_sum / ALWAN_LITERAL(2.0),
                          ALWAN_SELECT(h_sum < ALWAN_LITERAL(360.0),
                                       (h_sum + ALWAN_LITERAL(360.0)) / ALWAN_LITERAL(2.0),
                                       (h_sum - ALWAN_LITERAL(360.0)) / ALWAN_LITERAL(2.0))));
    alwan_scalar T2k = ALWAN_LITERAL(1.0)
        - ALWAN_LITERAL(0.17) * ALWAN_COS((Hpm - ALWAN_LITERAL(30.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))
        + ALWAN_LITERAL(0.24) * ALWAN_COS((ALWAN_LITERAL(2.0) * Hpm) * ALWAN_PI / ALWAN_LITERAL(180.0))
        + ALWAN_LITERAL(0.32) * ALWAN_COS((ALWAN_LITERAL(3.0) * Hpm + ALWAN_LITERAL(6.0)) * ALWAN_PI / ALWAN_LITERAL(180.0))
        - ALWAN_LITERAL(0.20) * ALWAN_COS((ALWAN_LITERAL(4.0) * Hpm - ALWAN_LITERAL(63.0)) * ALWAN_PI / ALWAN_LITERAL(180.0));
    alwan_scalar dTheta = ALWAN_LITERAL(30.0) * ALWAN_EXP(-((Hpm - ALWAN_LITERAL(275.0)) / ALWAN_LITERAL(25.0)) * ((Hpm - ALWAN_LITERAL(275.0)) / ALWAN_LITERAL(25.0)));
    alwan_scalar Cpm7 = Cpm * Cpm * Cpm * Cpm * Cpm * Cpm * Cpm;
    alwan_scalar RC = ALWAN_LITERAL(2.0) * ALWAN_SQRT(Cpm7 / (Cpm7 + ALWAN_LITERAL(6103515625.0)));
    alwan_scalar Lpm50sq = (Lpm - ALWAN_LITERAL(50.0)) * (Lpm - ALWAN_LITERAL(50.0));
    alwan_scalar SL = ALWAN_LITERAL(1.0) + (ALWAN_LITERAL(0.015) * Lpm50sq) / ALWAN_SQRT(ALWAN_LITERAL(20.0) + Lpm50sq);
    alwan_scalar SC = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.045) * Cpm;
    alwan_scalar SH = ALWAN_LITERAL(1.0) + ALWAN_LITERAL(0.015) * Cpm * T2k;
    alwan_scalar RT = -ALWAN_SIN((ALWAN_LITERAL(2.0) * dTheta) * ALWAN_PI / ALWAN_LITERAL(180.0)) * RC;
    alwan_scalar t1 = dLp / SL;
    alwan_scalar t2 = dCp / SC;
    alwan_scalar t3 = dHp / SH;
    return ALWAN_SQRT(t1 * t1 + t2 * t2 + t3 * t3 + RT * t2 * t3);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_hyab_v(alwan_lab lab1, alwan_lab lab2) {
    alwan_scalar dL = lab1.L - lab2.L;
    alwan_scalar db = lab1.b - lab2.b;
    alwan_scalar C1 = ALWAN_SQRT(lab1.a * lab1.a + lab1.b * lab1.b);
    alwan_scalar C2 = ALWAN_SQRT(lab2.a * lab2.a + lab2.b * lab2.b);
    alwan_scalar Cab = (C1 + C2) / ALWAN_LITERAL(2.0);
    alwan_scalar Cab7 = Cab * Cab * Cab * Cab * Cab * Cab * Cab;
    alwan_scalar G = ALWAN_LITERAL(0.5) * (ALWAN_LITERAL(1.0) - ALWAN_SQRT(Cab7 / (Cab7 + ALWAN_LITERAL(6103515625.0))));
    alwan_scalar a1p = (ALWAN_LITERAL(1.0) + G) * lab1.a;
    alwan_scalar a2p = (ALWAN_LITERAL(1.0) + G) * lab2.a;
    alwan_scalar C1p = ALWAN_SQRT(a1p * a1p + lab1.b * lab1.b);
    alwan_scalar C2p = ALWAN_SQRT(a2p * a2p + lab2.b * lab2.b);
    alwan_scalar dCp = C1p - C2p;
    alwan_scalar dap = a1p - a2p;
    alwan_scalar dHp_sq = dap * dap + db * db - dCp * dCp;
    dHp_sq = ALWAN_SELECT(dHp_sq > ALWAN_LITERAL(0.0), dHp_sq, ALWAN_LITERAL(0.0));
    return ALWAN_SQRT(dL * dL + dCp * dCp + dHp_sq);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cam_jab_v(alwan_cam_jab jab1, alwan_cam_jab jab2, alwan_scalar KL) {
    alwan_scalar dJ = (jab1.J - jab2.J) / KL;
    alwan_scalar da = jab1.a - jab2.a;
    alwan_scalar db = jab1.b - jab2.b;
    return ALWAN_SQRT(dJ * dJ + da * da + db * db);
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cam02_lcd_v(alwan_cam_jab jab1, alwan_cam_jab jab2) {
    return alwan_delta_e_cam_jab_v(jab1, jab2, ALWAN_LITERAL(0.77));
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cam02_scd_v(alwan_cam_jab jab1, alwan_cam_jab jab2) {
    return alwan_delta_e_cam_jab_v(jab1, jab2, ALWAN_LITERAL(1.24));
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cam16_lcd_v(alwan_cam_jab jab1, alwan_cam_jab jab2) {
    return alwan_delta_e_cam_jab_v(jab1, jab2, ALWAN_LITERAL(0.77));
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cam16_scd_v(alwan_cam_jab jab1, alwan_cam_jab jab2) {
    return alwan_delta_e_cam_jab_v(jab1, jab2, ALWAN_LITERAL(1.24));
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cam02_ucs_v(alwan_cam_jab jab1, alwan_cam_jab jab2) {
    return alwan_delta_e_cam_jab_v(jab1, jab2, ALWAN_LITERAL(1.0));
}

ALWAN_INLINE alwan_scalar alwan_delta_e_cam16_ucs_v(alwan_cam_jab jab1, alwan_cam_jab jab2) {
    return alwan_delta_e_cam_jab_v(jab1, jab2, ALWAN_LITERAL(1.0));
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_COLORSPACE_CORE_H */
