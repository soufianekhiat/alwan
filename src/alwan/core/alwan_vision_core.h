/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Color Vision Deficiency (CVD) Simulation
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Based on Brettel, Vienot & Mollon (1997)
 */

#ifndef ALWAN_VISION_CORE_H
#define ALWAN_VISION_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_math_core.h"

/* ================================================================
 * CVD Transformation Matrices
 * Based on Brettel, Vienot & Mollon (1997)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* sRGB to LMS matrix */
ALWAN_CONSTEXPR alwan_mat3x3 CVD_RGB_TO_LMS = {{
#include "../data/matrices/cvd_rgb_to_lms.csv"
}};

/* LMS to sRGB matrix */
ALWAN_CONSTEXPR alwan_mat3x3 CVD_LMS_TO_RGB = {{
#include "../data/matrices/cvd_lms_to_rgb.csv"
}};

/* Protanopia (L-cone absent) - red-blind */
ALWAN_CONSTEXPR alwan_mat3x3 CVD_PROTANOPIA = {{
#include "../data/matrices/cvd_protanopia.csv"
}};

/* Deuteranopia (M-cone absent) - green-blind */
ALWAN_CONSTEXPR alwan_mat3x3 CVD_DEUTERANOPIA = {{
#include "../data/matrices/cvd_deuteranopia.csv"
}};

/* Tritanopia (S-cone absent) - blue-blind */
ALWAN_CONSTEXPR alwan_mat3x3 CVD_TRITANOPIA = {{
#include "../data/matrices/cvd_tritanopia.csv"
}};

ALWAN_DIAG_POP

/* ================================================================
 * Generic CVD Simulation (takes CVD matrix as parameter)
 * ================================================================
 *
 * Pipeline: RGB -> LMS -> CVD(LMS) -> RGB -> lerp(original, cvd, severity)
 */

ALWAN_INLINE alwan_rgb alwan_simulate_cvd_matrix_v(alwan_rgb rgb,
                                                    alwan_mat3x3 cvd_matrix,
                                                    alwan_scalar severity) {
    alwan_rgb result;

    /* Clamp severity to [0, 1] */
    severity = alwan_clamp(severity, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    /* RGB -> LMS */
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 lms = alwan_mat3_mulv_v(CVD_RGB_TO_LMS, rgb_v);

    /* Apply CVD transformation in LMS space */
    alwan_vec3 lms_cvd = alwan_mat3_mulv_v(cvd_matrix, lms);

    /* LMS -> RGB (unrolled matrix multiply) */
    alwan_vec3 cvd_rgb = alwan_mat3_mulv_v(CVD_LMS_TO_RGB, lms_cvd);
    alwan_scalar cr = alwan_saturate(cvd_rgb.v[0]);
    alwan_scalar cg = alwan_saturate(cvd_rgb.v[1]);
    alwan_scalar cb = alwan_saturate(cvd_rgb.v[2]);

    /* Interpolate with original: out = lerp(rgb_in, cvd_rgb, severity) */
    result.r = alwan_lerp(rgb.r, cr, severity);
    result.g = alwan_lerp(rgb.g, cg, severity);
    result.b = alwan_lerp(rgb.b, cb, severity);

    return result;
}

/* ================================================================
 * Named CVD Simulation Functions
 * ================================================================ */

/* Protanopia simulation (L-cone deficiency, red-blind) */
ALWAN_INLINE alwan_rgb alwan_simulate_protanopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_matrix_v(rgb, CVD_PROTANOPIA, severity);
}

/* Deuteranopia simulation (M-cone deficiency, green-blind) */
ALWAN_INLINE alwan_rgb alwan_simulate_deuteranopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_matrix_v(rgb, CVD_DEUTERANOPIA, severity);
}

/* Tritanopia simulation (S-cone deficiency, blue-blind) */
ALWAN_INLINE alwan_rgb alwan_simulate_tritanopia_v(alwan_rgb rgb, alwan_scalar severity) {
    return alwan_simulate_cvd_matrix_v(rgb, CVD_TRITANOPIA, severity);
}

/* ================================================================
 * Contrast Sensitivity Function (CSF) -- Barten 1999
 * Reference: Barten, P. G. J. (1999). Contrast sensitivity of the
 *            human eye and its effects on image quality. SPIE Press.
 * ================================================================ */

/* Pupil diameter using Barten (1999) method
 * Formula: d = 5 - 3 * tanh(0.4 * log10(L * X_0 * Y_0 / 40^2))
 * When Y_0 < 0, uses X_0 in its place. */
ALWAN_INLINE alwan_scalar alwan_pupil_diameter_barten1999_v(
    alwan_scalar L, alwan_scalar X_0, alwan_scalar Y_0)
{
    alwan_scalar Y = ALWAN_SELECT(Y_0 < ALWAN_ZERO, X_0, Y_0);
    alwan_scalar arg = ALWAN_LITERAL(0.4) * ALWAN_LOG10(L * X_0 * Y / ALWAN_LITERAL(1600.0));
    return ALWAN_LITERAL(5.0) - ALWAN_LITERAL(3.0) * ALWAN_TANH(arg);
}

/* Retinal illuminance using Barten (1999) method
 * Formula: E = (pi * d^2 / 4) * L * [Stiles-Crawford correction]
 * apply_stiles_crawford: 1.0 to apply, 0.0 to skip */
ALWAN_INLINE alwan_scalar alwan_retinal_illuminance_barten1999_v(
    alwan_scalar L, alwan_scalar d, alwan_scalar apply_stiles_crawford)
{
    alwan_scalar E = (ALWAN_PI * d * d / ALWAN_LITERAL(4.0)) * L;

    alwan_scalar d_97 = d / ALWAN_LITERAL(9.7);
    alwan_scalar d_124 = d / ALWAN_LITERAL(12.4);
    alwan_scalar sc = ALWAN_ONE - d_97 * d_97 + d_124 * d_124 * d_124 * d_124;
    alwan_scalar correction = ALWAN_SELECT(apply_stiles_crawford > ALWAN_LITERAL(0.5), sc, ALWAN_ONE);

    return E * correction;
}

/* Optical MTF: M_opt = exp(-2 * pi^2 * sigma^2 * u^2) */
ALWAN_INLINE alwan_scalar alwan_optical_mtf_barten1999_v(alwan_scalar u, alwan_scalar sigma) {
    return ALWAN_EXP(ALWAN_LITERAL(-2.0) * ALWAN_PI * ALWAN_PI * sigma * sigma * u * u);
}

/* Standard deviation of line-spread function:
 * sigma = sqrt(sigma_0^2 + (C_ab * d)^2) */
ALWAN_INLINE alwan_scalar alwan_sigma_barten1999_v(
    alwan_scalar sigma_0, alwan_scalar C_ab, alwan_scalar d)
{
    alwan_scalar Cab_d = C_ab * d;
    return ALWAN_SQRT(sigma_0 * sigma_0 + Cab_d * Cab_d);
}

/* Maximum angular size:
 * X = (1/X_0^2 + 1/X_max^2 + u^2/N_max^2)^(-0.5) */
ALWAN_INLINE alwan_scalar alwan_maximum_angular_size_barten1999_v(
    alwan_scalar u, alwan_scalar X_0, alwan_scalar X_max, alwan_scalar N_max)
{
    alwan_scalar term1 = ALWAN_ONE / (X_0 * X_0);
    alwan_scalar term2 = ALWAN_ONE / (X_max * X_max);
    alwan_scalar term3 = (u * u) / (N_max * N_max);
    return ALWAN_POW(term1 + term2 + term3, ALWAN_LITERAL(-0.5));
}

/* Parameters for the full Barten 1999 CSF model */
typedef struct {
    alwan_scalar sigma;   /* Std. dev. of line-spread function (deg) */
    alwan_scalar k;       /* Signal-to-noise ratio */
    alwan_scalar T;       /* Integration time (seconds) */
    alwan_scalar X_0;     /* Angular size of object (deg) */
    alwan_scalar Y_0;     /* Vertical angular size (<0 means use X_0) */
    alwan_scalar X_max;   /* Maximum integration area (deg) */
    alwan_scalar Y_max;   /* Vertical max area (<0 means use X_max) */
    alwan_scalar N_max;   /* Maximum number of cycles */
    alwan_scalar n;       /* Quantum efficiency of the eye */
    alwan_scalar p;       /* Photon conversion factor */
    alwan_scalar E;       /* Retinal illuminance (Trolands) */
    alwan_scalar phi_0;   /* Neural noise spectral density */
    alwan_scalar u_0;     /* Lateral inhibition cutoff frequency */
} alwan_csf_barten1999_v_params;

/* Full Barten (1999) CSF
 * Formula: S = (M_opt / k) / sqrt(2/T * M_as * (1/(n*p*E) + phi_0/(1 - exp(-(u/u_0)^2)))) */
ALWAN_INLINE alwan_scalar alwan_csf_barten1999_v(
    alwan_scalar u, alwan_csf_barten1999_v_params p)
{
    /* Get Y values (use X if Y is negative) */
    alwan_scalar Y_0   = ALWAN_SELECT(p.Y_0   < ALWAN_ZERO, p.X_0,   p.Y_0);
    alwan_scalar Y_max = ALWAN_SELECT(p.Y_max < ALWAN_ZERO, p.X_max, p.Y_max);

    /* Optical MTF */
    alwan_scalar M_opt = alwan_optical_mtf_barten1999_v(u, p.sigma);

    /* Maximum angular size product M_as = 1/(X*Y) */
    alwan_scalar X = alwan_maximum_angular_size_barten1999_v(u, p.X_0, p.X_max, p.N_max);
    alwan_scalar Y = alwan_maximum_angular_size_barten1999_v(u, Y_0, Y_max, p.N_max);
    alwan_scalar M_as = ALWAN_ONE / (X * Y);

    /* Photon noise term */
    alwan_scalar photon_term = ALWAN_ONE / (p.n * p.p * p.E);

    /* Neural noise term with lateral inhibition cutoff */
    alwan_scalar u_ratio = u / p.u_0;
    alwan_scalar neural_term = p.phi_0 / (ALWAN_ONE - ALWAN_EXP(-(u_ratio * u_ratio)));

    /* Total noise under square root */
    alwan_scalar noise = (ALWAN_LITERAL(2.0) / p.T) * M_as * (photon_term + neural_term);

    /* Contrast sensitivity */
    return (M_opt / p.k) / ALWAN_SQRT(noise);
}

/* Simplified CSF model (value-returning, branchless)
 * Barten-inspired model with pupil area and band-pass filtering. */
ALWAN_INLINE alwan_scalar alwan_csf_simple_v(
    alwan_scalar spatial_frequency, alwan_scalar luminance)
{
    alwan_scalar f = spatial_frequency;
    alwan_scalar L = luminance;

    /* Pupil diameter: d ≈ 5 - 3*tanh(0.4*log10(L)) */
    alwan_scalar log_L = ALWAN_LOG10(L);
    alwan_scalar d = ALWAN_LITERAL(5.0) - ALWAN_LITERAL(3.0) * ALWAN_TANH(ALWAN_LITERAL(0.4) * log_L);
    alwan_scalar pupil_area = ALWAN_PI * d * d / ALWAN_LITERAL(4.0);
    alwan_scalar E = L * pupil_area;

    /* Band-pass filter: low-frequency roll-off * high-frequency roll-off */
    alwan_scalar low_freq_atten  = f / (f + ALWAN_LITERAL(0.5));
    alwan_scalar high_freq_atten = ALWAN_EXP(ALWAN_LITERAL(-0.005) * f * f);
    alwan_scalar M_opt = low_freq_atten * high_freq_atten;

    /* Noise: photon + neural */
    alwan_scalar phi_0 = ALWAN_LITERAL(3.0e-8);
    alwan_scalar k = ALWAN_LITERAL(3.0);
    alwan_scalar noise_photon = phi_0 / (E + ALWAN_LITERAL(1e-10));
    alwan_scalar noise_neural = ALWAN_ONE / k;
    alwan_scalar noise_total = ALWAN_SQRT(noise_photon * noise_photon + noise_neural * noise_neural);

    /* Contrast sensitivity, scaled */
    return (M_opt * E) / (noise_total + ALWAN_LITERAL(1e-10)) * ALWAN_LITERAL(10.0);
}

#endif /* ALWAN_VISION_CORE_H */
