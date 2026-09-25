/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Color Vision Deficiency (CVD) Simulation
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Brettel, Vienot & Mollon (1997) -- confusion-line projection
 * Machado, Oliveira & Fernandes (2009) -- cone spectral shift
 */

#ifndef ALWAN_VISION_CORE_H
#define ALWAN_VISION_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"
#include "alwan_table_core.h"

/* Machado 2009 publishes 11 severity steps, 0.0 to 1.0 in 0.1. */
#define ALWAN_MACHADO_SEVERITY_STEPS 11

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_vision_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_vision_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR alwan_mat3x3 CVD_RGB_TO_LMS = {{
#include "../data/matrices/cvd_rgb_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_LMS_TO_RGB = {{
#include "../data/matrices/cvd_lms_to_rgb.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_BRETTEL_PROTAN_H1 = {{
#include "../data/matrices/cvd_brettel_protan_h1.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_BRETTEL_PROTAN_H2 = {{
#include "../data/matrices/cvd_brettel_protan_h2.csv"
}};
ALWAN_CONSTEXPR alwan_vec3 CVD_BRETTEL_PROTAN_N = {{
#include "../data/matrices/cvd_brettel_protan_n.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_BRETTEL_DEUTAN_H1 = {{
#include "../data/matrices/cvd_brettel_deutan_h1.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_BRETTEL_DEUTAN_H2 = {{
#include "../data/matrices/cvd_brettel_deutan_h2.csv"
}};
ALWAN_CONSTEXPR alwan_vec3 CVD_BRETTEL_DEUTAN_N = {{
#include "../data/matrices/cvd_brettel_deutan_n.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_BRETTEL_TRITAN_H1 = {{
#include "../data/matrices/cvd_brettel_tritan_h1.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_BRETTEL_TRITAN_H2 = {{
#include "../data/matrices/cvd_brettel_tritan_h2.csv"
}};
ALWAN_CONSTEXPR alwan_vec3 CVD_BRETTEL_TRITAN_N = {{
#include "../data/matrices/cvd_brettel_tritan_n.csv"
}};

ALWAN_DIAG_POP

/* Brettel 1997, two half-planes; see the .inc. */
ALWAN_INLINE alwan_rgb alwan_simulate_cvd_brettel_v(alwan_rgb rgb, alwan_mat3x3 H1, alwan_mat3x3 H2,
                                                    alwan_vec3 n, alwan_scalar severity) {
    alwan_rgb result;
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 lms = alwan_mat3_mulv_v(CVD_RGB_TO_LMS, rgb_v);
    alwan_vec3 p1 = alwan_mat3_mulv_v(H1, lms);
    alwan_vec3 p2 = alwan_mat3_mulv_v(H2, lms);
    alwan_scalar side = lms.v[0] * n.v[0] + lms.v[1] * n.v[1] + lms.v[2] * n.v[2];
    alwan_vec3 p;
    p.v[0] = ALWAN_SELECT(side < ALWAN_ZERO, p2.v[0], p1.v[0]);
    p.v[1] = ALWAN_SELECT(side < ALWAN_ZERO, p2.v[1], p1.v[1]);
    p.v[2] = ALWAN_SELECT(side < ALWAN_ZERO, p2.v[2], p1.v[2]);
    alwan_vec3 cvd = alwan_mat3_mulv_v(CVD_LMS_TO_RGB, p);
    severity = alwan_clamp(severity, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    result.r = cvd.v[0] * severity + rgb.r * (ALWAN_ONE - severity);
    result.g = cvd.v[1] * severity + rgb.g * (ALWAN_ONE - severity);
    result.b = cvd.v[2] * severity + rgb.b * (ALWAN_ONE - severity);
    return result;
}

ALWAN_INLINE alwan_rgb alwan_simulate_protanopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_brettel_v(rgb, CVD_BRETTEL_PROTAN_H1, CVD_BRETTEL_PROTAN_H2, CVD_BRETTEL_PROTAN_N, severity);
}
ALWAN_INLINE alwan_rgb alwan_simulate_deuteranopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_brettel_v(rgb, CVD_BRETTEL_DEUTAN_H1, CVD_BRETTEL_DEUTAN_H2, CVD_BRETTEL_DEUTAN_N, severity);
}
ALWAN_INLINE alwan_rgb alwan_simulate_tritanopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_brettel_v(rgb, CVD_BRETTEL_TRITAN_H1, CVD_BRETTEL_TRITAN_H2, CVD_BRETTEL_TRITAN_N, severity);
}

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR alwan_mat3x3 MACHADO_PROTAN[11] = {
    {{
#include "../data/matrices/machado2009_protan_00.csv"
    }},{{
#include "../data/matrices/machado2009_protan_01.csv"
    }},{{
#include "../data/matrices/machado2009_protan_02.csv"
    }},{{
#include "../data/matrices/machado2009_protan_03.csv"
    }},{{
#include "../data/matrices/machado2009_protan_04.csv"
    }},{{
#include "../data/matrices/machado2009_protan_05.csv"
    }},{{
#include "../data/matrices/machado2009_protan_06.csv"
    }},{{
#include "../data/matrices/machado2009_protan_07.csv"
    }},{{
#include "../data/matrices/machado2009_protan_08.csv"
    }},{{
#include "../data/matrices/machado2009_protan_09.csv"
    }},{{
#include "../data/matrices/machado2009_protan_10.csv"
    }},
};

ALWAN_CONSTEXPR alwan_mat3x3 MACHADO_DEUTAN[11] = {
    {{
#include "../data/matrices/machado2009_deutan_00.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_01.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_02.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_03.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_04.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_05.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_06.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_07.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_08.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_09.csv"
    }},{{
#include "../data/matrices/machado2009_deutan_10.csv"
    }},
};

ALWAN_CONSTEXPR alwan_mat3x3 MACHADO_TRITAN[11] = {
    {{
#include "../data/matrices/machado2009_tritan_00.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_01.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_02.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_03.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_04.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_05.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_06.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_07.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_08.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_09.csv"
    }},{{
#include "../data/matrices/machado2009_tritan_10.csv"
    }},
};

ALWAN_DIAG_POP

/* See the .inc twin for why the old (int)(severity*10) was a crash on NaN. */
ALWAN_INLINE alwan_mat3x3 alwan_machado_interpolate_v(
    ALWAN_PARAM_ARRAY_IN(alwan_mat3x3, lut, ALWAN_MACHADO_SEVERITY_STEPS), alwan_scalar severity) {
    const alwan_table_cell c = alwan_table_cell_v(
        severity, ALWAN_MACHADO_SEVERITY_STEPS);
    return alwan_table_blend_mat3_v(lut[c.i0], lut[c.i1], c.frac);
}

ALWAN_INLINE alwan_rgb alwan_simulate_cvd_machado_v(
    alwan_rgb rgb, ALWAN_PARAM_ARRAY_IN(alwan_mat3x3, lut, ALWAN_MACHADO_SEVERITY_STEPS), alwan_scalar severity) {
    alwan_mat3x3 mat = alwan_machado_interpolate_v(lut, severity);
    alwan_vec3 v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 mapped = alwan_mat3_mulv_v(mat, v);
    /* Raw Machado 2009 matrix product -- no gamut clamp (matches the dual-precision core). */
    alwan_rgb result;
    result.r = mapped.v[0];
    result.g = mapped.v[1];
    result.b = mapped.v[2];
    return result;
}

ALWAN_INLINE alwan_rgb alwan_simulate_machado_protan_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_machado_v(rgb, MACHADO_PROTAN, severity);
}
ALWAN_INLINE alwan_rgb alwan_simulate_machado_deutan_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_machado_v(rgb, MACHADO_DEUTAN, severity);
}
ALWAN_INLINE alwan_rgb alwan_simulate_machado_tritan_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_machado_v(rgb, MACHADO_TRITAN, severity);
}

ALWAN_INLINE alwan_scalar alwan_pupil_diameter_barten1999_v(
    alwan_scalar L, alwan_scalar X_0, alwan_scalar Y_0) {
    alwan_scalar Y = ALWAN_SELECT(Y_0 < ALWAN_ZERO, X_0, Y_0);
    alwan_scalar arg = ALWAN_LITERAL(0.4) * ALWAN_LOG10(L * X_0 * Y / ALWAN_LITERAL(1600.0));
    return ALWAN_LITERAL(5.0) - ALWAN_LITERAL(3.0) * ALWAN_TANH(arg);
}

ALWAN_INLINE alwan_scalar alwan_retinal_illuminance_barten1999_v(
    alwan_scalar L, alwan_scalar d, alwan_scalar apply_stiles_crawford) {
    alwan_scalar E = (ALWAN_PI * d * d / ALWAN_LITERAL(4.0)) * L;
    alwan_scalar d_97 = d / ALWAN_LITERAL(9.7);
    alwan_scalar d_124 = d / ALWAN_LITERAL(12.4);
    alwan_scalar sc = ALWAN_ONE - d_97 * d_97 + d_124 * d_124 * d_124 * d_124;
    alwan_scalar correction = ALWAN_SELECT(apply_stiles_crawford > ALWAN_LITERAL(0.5), sc, ALWAN_ONE);
    return E * correction;
}

ALWAN_INLINE alwan_scalar alwan_optical_mtf_barten1999_v(alwan_scalar u, alwan_scalar sigma) {
    return ALWAN_EXP(ALWAN_LITERAL(-2.0) * ALWAN_PI * ALWAN_PI * sigma * sigma * u * u);
}

ALWAN_INLINE alwan_scalar alwan_sigma_barten1999_v(
    alwan_scalar sigma_0, alwan_scalar C_ab, alwan_scalar d) {
    alwan_scalar Cab_d = C_ab * d;
    return ALWAN_SQRT(sigma_0 * sigma_0 + Cab_d * Cab_d);
}

ALWAN_INLINE alwan_scalar alwan_maximum_angular_size_barten1999_v(
    alwan_scalar u, alwan_scalar X_0, alwan_scalar X_max, alwan_scalar N_max) {
    alwan_scalar term1 = ALWAN_ONE / (X_0 * X_0);
    alwan_scalar term2 = ALWAN_ONE / (X_max * X_max);
    alwan_scalar term3 = (u * u) / (N_max * N_max);
    return ALWAN_POW(term1 + term2 + term3, ALWAN_LITERAL(-0.5));
}

typedef struct {
    alwan_scalar sigma; alwan_scalar k; alwan_scalar T;
    alwan_scalar X_0; alwan_scalar Y_0; alwan_scalar X_max; alwan_scalar Y_max;
    alwan_scalar N_max; alwan_scalar n; alwan_scalar p; alwan_scalar E;
    alwan_scalar phi_0; alwan_scalar u_0;
} alwan_csf_barten1999_v_params;

ALWAN_INLINE alwan_scalar alwan_csf_barten1999_v(
    alwan_scalar u, alwan_csf_barten1999_v_params p) {
    alwan_scalar Y_0 = ALWAN_SELECT(p.Y_0 < ALWAN_ZERO, p.X_0, p.Y_0);
    alwan_scalar Y_max = ALWAN_SELECT(p.Y_max < ALWAN_ZERO, p.X_max, p.Y_max);
    alwan_scalar M_opt = alwan_optical_mtf_barten1999_v(u, p.sigma);
    alwan_scalar X = alwan_maximum_angular_size_barten1999_v(u, p.X_0, p.X_max, p.N_max);
    alwan_scalar Y = alwan_maximum_angular_size_barten1999_v(u, Y_0, Y_max, p.N_max);
    alwan_scalar M_as = ALWAN_ONE / (X * Y);
    alwan_scalar photon_term = ALWAN_ONE / (p.n * p.p * p.E);
    alwan_scalar u_ratio = u / p.u_0;
    alwan_scalar neural_term = p.phi_0 / (ALWAN_ONE - ALWAN_EXP(-(u_ratio * u_ratio)));
    alwan_scalar noise = (ALWAN_LITERAL(2.0) / p.T) * M_as * (photon_term + neural_term);
    return (M_opt / p.k) / ALWAN_SQRT(noise);
}

ALWAN_INLINE alwan_scalar alwan_csf_simple_v(
    alwan_scalar spatial_frequency, alwan_scalar luminance) {
    alwan_scalar f = spatial_frequency; alwan_scalar L = luminance;
    alwan_scalar log_L = ALWAN_LOG10(L);
    alwan_scalar d = ALWAN_LITERAL(5.0) - ALWAN_LITERAL(3.0) * ALWAN_TANH(ALWAN_LITERAL(0.4) * log_L);
    alwan_scalar pupil_area = ALWAN_PI * d * d / ALWAN_LITERAL(4.0);
    alwan_scalar E = L * pupil_area;
    alwan_scalar low_freq_atten = f / (f + ALWAN_LITERAL(0.5));
    alwan_scalar high_freq_atten = ALWAN_EXP(ALWAN_LITERAL(-0.005) * f * f);
    alwan_scalar M_opt = low_freq_atten * high_freq_atten;
    alwan_scalar phi_0 = ALWAN_LITERAL(3.0e-8); alwan_scalar k = ALWAN_LITERAL(3.0);
    alwan_scalar noise_photon = phi_0 / (E + ALWAN_LITERAL(1e-10));
    alwan_scalar noise_neural = ALWAN_ONE / k;
    alwan_scalar noise_total = ALWAN_SQRT(noise_photon * noise_photon + noise_neural * noise_neural);
    return (M_opt * E) / (noise_total + ALWAN_LITERAL(1e-10)) * ALWAN_LITERAL(10.0);
}

ALWAN_INLINE alwan_scalar alwan_wcag_contrast_ratio_v(alwan_scalar Y1, alwan_scalar Y2) {
    alwan_scalar L1 = ALWAN_SELECT(Y1 > Y2, Y1, Y2);
    alwan_scalar L2 = ALWAN_SELECT(Y1 > Y2, Y2, Y1);
    return (L1 + ALWAN_LITERAL(0.05)) / (L2 + ALWAN_LITERAL(0.05));
}

ALWAN_INLINE alwan_scalar alwan_apca_contrast_v(alwan_rgb srgb_text, alwan_rgb srgb_bg) {
    /* APCA-W3 0.1.9 step for step; see the .inc. */
    const alwan_scalar mainTRC = ALWAN_LITERAL(2.4);
    const alwan_scalar Rco = ALWAN_LITERAL(0.2126729); const alwan_scalar Gco = ALWAN_LITERAL(0.7151522); const alwan_scalar Bco = ALWAN_LITERAL(0.0721750);
    const alwan_scalar blkThrs = ALWAN_LITERAL(0.022); const alwan_scalar blkClmp = ALWAN_LITERAL(1.414);
    const alwan_scalar deltaYmin = ALWAN_LITERAL(0.0005);
    const alwan_scalar normBG = ALWAN_LITERAL(0.56); const alwan_scalar normTXT = ALWAN_LITERAL(0.57);
    const alwan_scalar revBG = ALWAN_LITERAL(0.65); const alwan_scalar revTXT = ALWAN_LITERAL(0.62);
    const alwan_scalar scaleBoW = ALWAN_LITERAL(1.14); const alwan_scalar scaleWoB = ALWAN_LITERAL(1.14);
    const alwan_scalar loClip = ALWAN_LITERAL(0.1);
    const alwan_scalar loBoWoffset = ALWAN_LITERAL(0.027); const alwan_scalar loWoBoffset = ALWAN_LITERAL(0.027);
    const alwan_scalar yMax = ALWAN_LITERAL(1.1);
    int valid = srgb_text.r >= ALWAN_ZERO && srgb_text.g >= ALWAN_ZERO && srgb_text.b >= ALWAN_ZERO &&
                srgb_bg.r >= ALWAN_ZERO && srgb_bg.g >= ALWAN_ZERO && srgb_bg.b >= ALWAN_ZERO;
    alwan_scalar tr = ALWAN_SELECT(srgb_text.r > ALWAN_ZERO, srgb_text.r, ALWAN_ZERO);
    alwan_scalar tg = ALWAN_SELECT(srgb_text.g > ALWAN_ZERO, srgb_text.g, ALWAN_ZERO);
    alwan_scalar tb = ALWAN_SELECT(srgb_text.b > ALWAN_ZERO, srgb_text.b, ALWAN_ZERO);
    alwan_scalar br = ALWAN_SELECT(srgb_bg.r > ALWAN_ZERO, srgb_bg.r, ALWAN_ZERO);
    alwan_scalar bgc = ALWAN_SELECT(srgb_bg.g > ALWAN_ZERO, srgb_bg.g, ALWAN_ZERO);
    alwan_scalar bb = ALWAN_SELECT(srgb_bg.b > ALWAN_ZERO, srgb_bg.b, ALWAN_ZERO);
    alwan_scalar Ytxt = Rco * ALWAN_POW(tr, mainTRC) + Gco * ALWAN_POW(tg, mainTRC) + Bco * ALWAN_POW(tb, mainTRC);
    alwan_scalar Ybg = Rco * ALWAN_POW(br, mainTRC) + Gco * ALWAN_POW(bgc, mainTRC) + Bco * ALWAN_POW(bb, mainTRC);
    valid = valid && Ytxt <= yMax && Ybg <= yMax;
    alwan_scalar dTxt = ALWAN_SELECT(Ytxt > blkThrs, ALWAN_ZERO, blkThrs - Ytxt);
    alwan_scalar dBg = ALWAN_SELECT(Ybg > blkThrs, ALWAN_ZERO, blkThrs - Ybg);
    Ytxt = ALWAN_SELECT(Ytxt > blkThrs, Ytxt, Ytxt + ALWAN_POW(dTxt, blkClmp));
    Ybg = ALWAN_SELECT(Ybg > blkThrs, Ybg, Ybg + ALWAN_POW(dBg, blkClmp));
    alwan_scalar dY = Ybg - Ytxt;
    alwan_scalar adY = ALWAN_SELECT(dY < ALWAN_ZERO, -dY, dY);
    alwan_scalar s_bow = (ALWAN_POW(Ybg, normBG) - ALWAN_POW(Ytxt, normTXT)) * scaleBoW;
    alwan_scalar s_wob = (ALWAN_POW(Ybg, revBG) - ALWAN_POW(Ytxt, revTXT)) * scaleWoB;
    alwan_scalar o_bow = ALWAN_SELECT(s_bow < loClip, ALWAN_ZERO, s_bow - loBoWoffset);
    alwan_scalar o_wob = ALWAN_SELECT(s_wob > -loClip, ALWAN_ZERO, s_wob + loWoBoffset);
    alwan_scalar res = ALWAN_SELECT(Ybg > Ytxt, o_bow, o_wob);
    res = ALWAN_SELECT(adY < deltaYmin, ALWAN_ZERO, res);
    res = ALWAN_SELECT(valid, res, ALWAN_ZERO);
    return res * ALWAN_LITERAL(100.0);
}

/* ================================================================
 * Helmholtz-Kohlrausch effect (Nayatani 1997)
 *
 * How much brighter a chromatic stimulus looks than an achromatic one of the
 * same luminance. Everything works in CIE 1960 UCS chromaticities, the stimulus
 * against the adapting field.
 * ================================================================ */

/* Brightness coefficient of the adapting luminance. */
ALWAN_INLINE alwan_scalar alwan_hke_coefficient_K_Br_v(alwan_scalar L_a) {
    alwan_scalar p = ALWAN_POW(L_a, ALWAN_LITERAL(0.4495));
    return (p * ALWAN_LITERAL(6.362) + ALWAN_LITERAL(6.469)) *
           ALWAN_LITERAL(0.2717) / (p + ALWAN_LITERAL(6.469));
}

/* Hue-dependent coefficient: four harmonics of the chromaticity angle. */
ALWAN_INLINE alwan_scalar alwan_hke_coefficient_q_v(alwan_scalar theta) {
    alwan_scalar t2 = theta + theta;
    alwan_scalar t3 = t2 + theta;
    alwan_scalar t4 = t2 + t2;
    return ALWAN_LITERAL(-0.01585)
         - ALWAN_LITERAL(0.03017) * ALWAN_COS(theta)
         - ALWAN_LITERAL(0.04556) * ALWAN_COS(t2)
         - ALWAN_LITERAL(0.02667) * ALWAN_COS(t3)
         - ALWAN_LITERAL(0.00295) * ALWAN_COS(t4)
         + ALWAN_LITERAL(0.14592) * ALWAN_SIN(theta)
         + ALWAN_LITERAL(0.05084) * ALWAN_SIN(t2)
         - ALWAN_LITERAL(0.01900) * ALWAN_SIN(t3)
         - ALWAN_LITERAL(0.00764) * ALWAN_SIN(t4);
}

/* The object variant. method_coefficient is the caller's VCC or VAC weight.
 *
 * A stimulus sitting on the adapting field gives S_uv = 0 and therefore exactly
 * 1, which is the quantity's whole meaning: no chromatic content, no effect. */
ALWAN_INLINE alwan_scalar alwan_hke_object_nayatani1997_v(
    alwan_scalar u,
    alwan_scalar v,
    alwan_scalar u_c,
    alwan_scalar v_c,
    alwan_scalar L_a,
    alwan_scalar method_coefficient)
{
    alwan_scalar du = u - u_c;
    alwan_scalar dv = v - v_c;
    alwan_scalar K_Br = alwan_hke_coefficient_K_Br_v(L_a);
    alwan_scalar q = alwan_hke_coefficient_q_v(ALWAN_ATAN2(dv, du));
    alwan_scalar S_uv = ALWAN_LITERAL(13.0) * ALWAN_SQRT(du * du + dv * dv);
    return ALWAN_ONE + (method_coefficient * q + ALWAN_LITERAL(0.0872) * K_Br) * S_uv;
}

/* The luminous variant, a cube of the object one. */
ALWAN_INLINE alwan_scalar alwan_hke_luminous_nayatani1997_v(
    alwan_scalar u,
    alwan_scalar v,
    alwan_scalar u_c,
    alwan_scalar v_c,
    alwan_scalar L_a,
    alwan_scalar method_coefficient)
{
    alwan_scalar g = alwan_hke_object_nayatani1997_v(u, v, u_c, v_c, L_a,
                                                                      method_coefficient);
    alwan_scalar s = g + ALWAN_LITERAL(0.3086);
    return ALWAN_LITERAL(0.4462) * s * s * s;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_VISION_CORE_H */
