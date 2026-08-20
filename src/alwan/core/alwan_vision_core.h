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
ALWAN_CONSTEXPR alwan_mat3x3 CVD_PROTANOPIA = {{
#include "../data/matrices/cvd_protanopia.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_DEUTERANOPIA = {{
#include "../data/matrices/cvd_deuteranopia.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CVD_TRITANOPIA = {{
#include "../data/matrices/cvd_tritanopia.csv"
}};

ALWAN_DIAG_POP

ALWAN_INLINE alwan_rgb alwan_simulate_cvd_matrix_v(alwan_rgb rgb,
                                                    alwan_mat3x3 cvd_matrix,
                                                    alwan_scalar severity) {
    alwan_rgb result;
    severity = alwan_clamp(severity, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 lms = alwan_mat3_mulv_v(CVD_RGB_TO_LMS, rgb_v);
    alwan_vec3 lms_cvd = alwan_mat3_mulv_v(cvd_matrix, lms);
    alwan_vec3 cvd_rgb = alwan_mat3_mulv_v(CVD_LMS_TO_RGB, lms_cvd);
    /* Raw Brettel/Vienot simulation -- no gamut clamp (matches the dual-precision core). */
    alwan_scalar cr = cvd_rgb.v[0];
    alwan_scalar cg = cvd_rgb.v[1];
    alwan_scalar cb = cvd_rgb.v[2];
    result.r = alwan_lerp(rgb.r, cr, severity);
    result.g = alwan_lerp(rgb.g, cg, severity);
    result.b = alwan_lerp(rgb.b, cb, severity);
    return result;
}

ALWAN_INLINE alwan_rgb alwan_simulate_protanopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_matrix_v(rgb, CVD_PROTANOPIA, severity);
}
ALWAN_INLINE alwan_rgb alwan_simulate_deuteranopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_matrix_v(rgb, CVD_DEUTERANOPIA, severity);
}
ALWAN_INLINE alwan_rgb alwan_simulate_tritanopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_matrix_v(rgb, CVD_TRITANOPIA, severity);
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
    alwan_mat3x3 const *lut, alwan_scalar severity) {
    return alwan_table1d_mat3_sample_linear_v(
        lut, ALWAN_MACHADO_SEVERITY_STEPS, severity);
}

ALWAN_INLINE alwan_rgb alwan_simulate_cvd_machado_v(
    alwan_rgb rgb, alwan_mat3x3 const *lut, alwan_scalar severity) {
    alwan_mat3x3 mat = alwan_machado_interpolate_v(lut, severity);
    alwan_vec3 v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 out = alwan_mat3_mulv_v(mat, v);
    /* Raw Machado 2009 matrix product -- no gamut clamp (matches the dual-precision core). */
    alwan_rgb result;
    result.r = out.v[0];
    result.g = out.v[1];
    result.b = out.v[2];
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
    alwan_scalar const mainTRC = ALWAN_LITERAL(2.4);
    alwan_scalar Rtxt = ALWAN_POW(srgb_text.r, mainTRC); alwan_scalar Gtxt = ALWAN_POW(srgb_text.g, mainTRC); alwan_scalar Btxt = ALWAN_POW(srgb_text.b, mainTRC);
    alwan_scalar Rbg = ALWAN_POW(srgb_bg.r, mainTRC); alwan_scalar Gbg = ALWAN_POW(srgb_bg.g, mainTRC); alwan_scalar Bbg = ALWAN_POW(srgb_bg.b, mainTRC);
    alwan_scalar const Rco = ALWAN_LITERAL(0.2126729); alwan_scalar const Gco = ALWAN_LITERAL(0.7151522); alwan_scalar const Bco = ALWAN_LITERAL(0.0721750);
    alwan_scalar Ytxt = Rco * Rtxt + Gco * Gtxt + Bco * Btxt;
    alwan_scalar Ybg = Rco * Rbg + Gco * Gbg + Bco * Bbg;
    alwan_scalar const blkThrs = ALWAN_LITERAL(0.022); alwan_scalar const blkClmp = ALWAN_LITERAL(1.414);
    alwan_scalar diffTxt = ALWAN_SELECT(Ytxt < blkThrs, blkThrs - Ytxt, ALWAN_ZERO);
    alwan_scalar diffBg = ALWAN_SELECT(Ybg < blkThrs, blkThrs - Ybg, ALWAN_ZERO);
    Ytxt = ALWAN_SELECT(Ytxt < blkThrs, Ytxt + ALWAN_POW(diffTxt, blkClmp), Ytxt);
    Ybg = ALWAN_SELECT(Ybg < blkThrs, Ybg + ALWAN_POW(diffBg, blkClmp), Ybg);
    alwan_scalar const normBG = ALWAN_LITERAL(0.56); alwan_scalar const normTXT = ALWAN_LITERAL(0.57);
    alwan_scalar const revBG = ALWAN_LITERAL(0.65); alwan_scalar const revTXT = ALWAN_LITERAL(0.62);
    alwan_scalar const scaleBO = ALWAN_LITERAL(1.14); alwan_scalar const loClip = ALWAN_LITERAL(0.1);
    alwan_scalar const loBoWoffset = ALWAN_LITERAL(0.027);
    alwan_scalar Sapc_normal = (ALWAN_POW(Ybg, normBG) - ALWAN_POW(Ytxt, normTXT)) * scaleBO;
    alwan_scalar Sapc_reverse = (ALWAN_POW(Ybg, revBG) - ALWAN_POW(Ytxt, revTXT)) * scaleBO;
    alwan_scalar SAPC = ALWAN_SELECT(Ybg > Ytxt, Sapc_normal, Sapc_reverse);
    alwan_scalar abs_SAPC = ALWAN_SELECT(SAPC < ALWAN_ZERO, -SAPC, SAPC);
    alwan_scalar Lc_pos = SAPC - loBoWoffset; alwan_scalar Lc_neg = SAPC + loBoWoffset;
    alwan_scalar Lc = ALWAN_SELECT(SAPC > ALWAN_ZERO, Lc_pos, Lc_neg);
    Lc = ALWAN_SELECT(abs_SAPC < loClip, ALWAN_ZERO, Lc);
    return Lc * ALWAN_LITERAL(100.0);
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_VISION_CORE_H */
