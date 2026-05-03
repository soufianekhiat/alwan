/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ACES Fixed Functions (per-pixel math)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: OpenColorIO FixedFunctionOpCPU.cpp,
 *            ACES CTL Lib.Academy.OutputTransform.ctl
 */

#ifndef ALWAN_ACES_FF_CORE_H
#define ALWAN_ACES_FF_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

#define ACES2_REACH_TABLE_SIZE 360
#define ACES2_CUSP_TABLE_SIZE 362

/* ACES2 numeric constants (precision-agnostic — use ALWAN_LITERAL in per-pass .inc) */
#define ACES_CAM_NL_OFFSET_VALUE             27.13
#define ACES_J_SCALE_VALUE                  100.0
#define ACES_GAMUT_COMPRESSION_THRESHOLD_VALUE  0.75
#define ACES_GAMUT_SMOOTH_CUSPS_VALUE           0.12
#define ACES_GAMUT_SMOOTH_M_VALUE               0.27
#define ACES_GAMUT_CUSP_MID_BLEND_VALUE          1.3

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_aces_ff_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_aces_ff_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

#define ACES_CAM_NL_OFFSET  ((alwan_scalar)(ACES_CAM_NL_OFFSET_VALUE))
#define ACES_J_SCALE        ((alwan_scalar)(ACES_J_SCALE_VALUE))
#define ACES_GAMUT_COMPRESSION_THRESHOLD ((alwan_scalar)(ACES_GAMUT_COMPRESSION_THRESHOLD_VALUE))
#define ACES_GAMUT_SMOOTH_CUSPS ((alwan_scalar)(ACES_GAMUT_SMOOTH_CUSPS_VALUE))
#define ACES_GAMUT_CUSP_MID_BLEND ((alwan_scalar)(ACES_GAMUT_CUSP_MID_BLEND_VALUE))

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_scalar ACES2_CHROMA_NORM_COS_V[4] = {
#include "../data/aces2_chroma_norm_cos.csv"
};
ALWAN_CONSTEXPR alwan_scalar ACES2_CHROMA_NORM_SIN_V[4] = {
#include "../data/aces2_chroma_norm_sin.csv"
};
ALWAN_DIAG_POP

typedef struct {
    alwan_mat3x3 MATRIX_RGB_to_CAM16_c;
    alwan_mat3x3 MATRIX_cone_response_to_Aab;
    alwan_mat3x3 MATRIX_CAM16_to_RGB;
    alwan_mat3x3 MATRIX_Aab_to_cone;
    alwan_scalar cz;
    alwan_scalar inv_cz;
    alwan_scalar A_w_J;
    alwan_scalar inv_A_w_J;
    alwan_scalar F_L_n;
} aces2_JMhParams;

typedef struct {
    alwan_scalar n, n_r, g, t_1, s_2, u_2, m_2;
} aces2_TSParams;

typedef struct {
    alwan_scalar J, M, gamma_top_inv;
} aces2_GamutCuspEntry;

typedef struct {
    alwan_scalar limit_J_max, mid_J, focus_dist;
    alwan_scalar lower_hull_gamma_inv, model_gamma_inv, reach_max_M;
    aces2_GamutCuspEntry cusp_table[ACES2_CUSP_TABLE_SIZE];
    alwan_scalar reach_m_table[ACES2_REACH_TABLE_SIZE];
    alwan_scalar hue_table[ACES2_CUSP_TABLE_SIZE];
} aces2_GamutCompressParams;

typedef struct {
    alwan_scalar cusp_J, cusp_M, gamma_top_inv, gamma_bottom_inv;
    alwan_scalar focus_J, analytical_threshold;
} aces2_HueDependentGamutParams;

ALWAN_INLINE alwan_scalar aces_bt1886_oetf_v(alwan_scalar x) {
    alwan_scalar const safe_x = alwan_max(x, ALWAN_LITERAL(0.0));
    return ALWAN_POW(safe_x, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.4));
}

ALWAN_INLINE alwan_scalar aces_srgb_oetf_v(alwan_scalar x) {
    alwan_scalar const safe_x = alwan_max(x, ALWAN_LITERAL(0.0));
    return ALWAN_SRGB_OETF(safe_x);
}

ALWAN_INLINE alwan_scalar aces_gamma26_oetf_v(alwan_scalar x) {
    alwan_scalar const safe_x = alwan_max(x, ALWAN_LITERAL(0.0));
    return ALWAN_POW(safe_x, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.6));
}

ALWAN_INLINE alwan_scalar aces_pq_oetf_v(alwan_scalar Y, alwan_scalar peak_nits) {
    alwan_scalar m1 = ALWAN_LITERAL(0.1593017578125);
    alwan_scalar m2 = ALWAN_LITERAL(78.84375);
    alwan_scalar c1 = ALWAN_LITERAL(0.8359375);
    alwan_scalar c2 = ALWAN_LITERAL(18.8515625);
    alwan_scalar c3 = ALWAN_LITERAL(18.6875);

    alwan_scalar L = alwan_max(Y * peak_nits / ALWAN_LITERAL(10000.0), ALWAN_LITERAL(0.0));
    alwan_scalar Lm1 = ALWAN_POW(L, m1);
    alwan_scalar num = c1 + c2 * Lm1;
    alwan_scalar den = ALWAN_LITERAL(1.0) + c3 * Lm1;
    return ALWAN_POW(num / den, m2);
}

ALWAN_INLINE alwan_scalar aces_bt1886_eotf_v(alwan_scalar x) {
    alwan_scalar const safe_x = alwan_max(x, ALWAN_LITERAL(0.0));
    return ALWAN_POW(safe_x, ALWAN_LITERAL(2.4));
}

ALWAN_INLINE alwan_scalar aces_srgb_eotf_v(alwan_scalar x) {
    alwan_scalar const safe_x = alwan_max(x, ALWAN_LITERAL(0.0));
    return ALWAN_SRGB_EOTF(safe_x);
}

ALWAN_INLINE alwan_scalar aces_gamma26_eotf_v(alwan_scalar x) {
    alwan_scalar const safe_x = alwan_max(x, ALWAN_LITERAL(0.0));
    return ALWAN_POW(safe_x, ALWAN_LITERAL(2.6));
}

ALWAN_INLINE alwan_scalar aces_pq_eotf_v(alwan_scalar E, alwan_scalar peak_nits) {
    alwan_scalar m1 = ALWAN_LITERAL(0.1593017578125);
    alwan_scalar m2 = ALWAN_LITERAL(78.84375);
    alwan_scalar c1 = ALWAN_LITERAL(0.8359375);
    alwan_scalar c2 = ALWAN_LITERAL(18.8515625);
    alwan_scalar c3 = ALWAN_LITERAL(18.6875);

    alwan_scalar Em2 = ALWAN_POW(alwan_max(E, ALWAN_LITERAL(0.0)), ALWAN_LITERAL(1.0) / m2);
    alwan_scalar num = alwan_max(Em2 - c1, ALWAN_LITERAL(0.0));
    alwan_scalar den = c2 - c3 * Em2;
    if (den <= ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    alwan_scalar Y = ALWAN_POW(num / den, ALWAN_LITERAL(1.0) / m1);
    return Y * ALWAN_LITERAL(10000.0) / peak_nits;
}

/* Remaining GPU functions kept minimal - only those commonly used on GPU */
ALWAN_INLINE alwan_scalar aces_cone_response_fwd_v(alwan_scalar v) {
    alwan_scalar abs_v = ALWAN_ABS(v);
    if (abs_v < ALWAN_LITERAL(1e-10)) return ALWAN_LITERAL(0.0);

    alwan_scalar F_L_Y = ALWAN_POW(abs_v, ALWAN_LITERAL(0.42));
    /* Guard: Inf or NaN F_L_Y (from Inf/NaN input) → Ra = Inf/Inf = NaN.
     * Physically Ra → 1 as stimulus → ∞, so cap at 1. Also protects
     * downstream hue lookups from (int)NaN undefined behaviour. */
    if (!(F_L_Y < ALWAN_LITERAL(1e15))) {
        return (v >= ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(-1.0);
    }
    alwan_scalar Ra = F_L_Y / (ACES_CAM_NL_OFFSET + F_L_Y);

    return (v >= ALWAN_LITERAL(0.0)) ? Ra : -Ra;
}

ALWAN_INLINE alwan_scalar aces_cone_response_inv_v(alwan_scalar Ra) {
    alwan_scalar sign = (Ra >= ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(-1.0);
    alwan_scalar Ra_abs = ALWAN_ABS(Ra);
    if (Ra_abs < ALWAN_LITERAL(1e-10)) return ALWAN_LITERAL(0.0);
    alwan_scalar Ra_lim = (Ra_abs < ALWAN_LITERAL(0.99)) ? Ra_abs : ALWAN_LITERAL(0.99);
    alwan_scalar F_L_Y = ACES_CAM_NL_OFFSET * Ra_lim / (ALWAN_LITERAL(1.0) - Ra_lim);
    alwan_scalar Rc = ALWAN_POW(F_L_Y, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.42));
    return sign * Rc;
}

ALWAN_INLINE alwan_vec3 aces_dark_to_dim10_v(alwan_vec3 rgb) {
    alwan_scalar const MIN_LUM = ALWAN_LITERAL(1e-10);
    alwan_scalar const Y_r = ALWAN_LUMA_KR_AP1;
    alwan_scalar const Y_g = ALWAN_LUMA_KG_AP1;
    alwan_scalar const Y_b = ALWAN_LUMA_KB_AP1;
    alwan_scalar const GAMMA = ALWAN_LITERAL(-0.0189);

    alwan_scalar Y = Y_r * rgb.v[0] + Y_g * rgb.v[1] + Y_b * rgb.v[2];
    if (Y < MIN_LUM) Y = MIN_LUM;
    alwan_scalar scale = ALWAN_POW(Y, GAMMA);

    alwan_vec3 result;
    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

ALWAN_INLINE alwan_vec3 aces_rec2100_surround_v(alwan_vec3 rgb, alwan_scalar gamma) {
    alwan_scalar const MIN_LUM = ALWAN_LITERAL(1e-4);
    alwan_scalar const Y_r = ALWAN_LUMA_KR_BT2020;
    alwan_scalar const Y_g = ALWAN_LUMA_KG_BT2020;
    alwan_scalar const Y_b = ALWAN_LUMA_KB_BT2020;

    alwan_scalar Y = Y_r * rgb.v[0] + Y_g * rgb.v[1] + Y_b * rgb.v[2];
    Y = ALWAN_ABS(Y);
    if (Y < MIN_LUM) Y = MIN_LUM;
    alwan_scalar m_gamma = gamma - ALWAN_LITERAL(1.0);
    alwan_scalar scale = ALWAN_POW(Y, m_gamma);

    alwan_vec3 result;
    result.v[0] = rgb.v[0] * scale;
    result.v[1] = rgb.v[1] * scale;
    result.v[2] = rgb.v[2] * scale;
    return result;
}

#endif

#endif /* ALWAN_ACES_FF_CORE_H */
