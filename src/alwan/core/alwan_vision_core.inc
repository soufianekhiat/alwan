/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Color Vision Deficiency (CVD) Simulation (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * Brettel, Vienot & Mollon (1997) -- confusion-line projection
 * Machado, Oliveira & Fernandes (2009) -- cone spectral shift
 */

/* ================================================================
 * CVD Transformation Matrices
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_RGB_TO_LMS) = {{
#include "../data/matrices/cvd_rgb_to_lms.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_LMS_TO_RGB) = {{
#include "../data/matrices/cvd_lms_to_rgb.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_PROTANOPIA) = {{
#include "../data/matrices/cvd_protanopia.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_DEUTERANOPIA) = {{
#include "../data/matrices/cvd_deuteranopia.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_TRITANOPIA) = {{
#include "../data/matrices/cvd_tritanopia.csv"
}};

ALWAN_DIAG_POP

/* ================================================================
 * Generic CVD Simulation (takes CVD matrix as parameter)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_cvd_matrix)(ALWAN_CORE_RGB rgb,
                                                    ALWAN_CORE_MAT3X3 cvd_matrix,
                                                    ALWAN_CORE_T severity) {
    ALWAN_CORE_RGB result;

    severity = ALWAN_CORE_CLAMP(severity, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CVD_RGB_TO_LMS), rgb_v);
    ALWAN_CORE_VEC3 lms_cvd = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(cvd_matrix, lms);
    ALWAN_CORE_VEC3 cvd_rgb = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CVD_LMS_TO_RGB), lms_cvd);
    /* Raw Brettel/Vienot simulation -- no gamut clamp. The LMS round-trip can
     * legitimately land outside [0,1] for saturated inputs; use
     * alwan_simulate_cvd_gamut_safe for a guaranteed in-gamut result. */
    ALWAN_CORE_T cr = cvd_rgb.v[0];
    ALWAN_CORE_T cg = cvd_rgb.v[1];
    ALWAN_CORE_T cb = cvd_rgb.v[2];

    result.r = ALWAN_CORE_LERP(rgb.r, cr, severity);
    result.g = ALWAN_CORE_LERP(rgb.g, cg, severity);
    result.b = ALWAN_CORE_LERP(rgb.b, cb, severity);

    return result;
}

/* ================================================================
 * Named CVD Simulation Functions
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_protanopia)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_matrix)(rgb, ALWAN_CORE_FNLIT(CVD_PROTANOPIA), severity);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_deuteranopia)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_matrix)(rgb, ALWAN_CORE_FNLIT(CVD_DEUTERANOPIA), severity);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_tritanopia)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_matrix)(rgb, ALWAN_CORE_FNLIT(CVD_TRITANOPIA), severity);
}

/* ================================================================
 * Machado, Oliveira & Fernandes (2009) CVD Simulation
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(MACHADO_PROTAN)[11] = {
    {{
#include "../data/matrices/machado2009_protan_00.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_01.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_02.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_03.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_04.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_05.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_06.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_07.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_08.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_09.csv"
    }},
    {{
#include "../data/matrices/machado2009_protan_10.csv"
    }},
};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(MACHADO_DEUTAN)[11] = {
    {{
#include "../data/matrices/machado2009_deutan_00.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_01.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_02.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_03.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_04.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_05.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_06.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_07.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_08.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_09.csv"
    }},
    {{
#include "../data/matrices/machado2009_deutan_10.csv"
    }},
};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(MACHADO_TRITAN)[11] = {
    {{
#include "../data/matrices/machado2009_tritan_00.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_01.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_02.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_03.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_04.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_05.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_06.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_07.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_08.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_09.csv"
    }},
    {{
#include "../data/matrices/machado2009_tritan_10.csv"
    }},
};

ALWAN_DIAG_POP

/* ALWAN_CORE_CLAMP is NaN-transparent -- clamp(NaN,0,1) is NaN -- so the old
 * `(int)(severity*10)` landed on INT_MIN for a NaN severity and `if (lo >= 10)`
 * did not catch it, dereferencing lut[INT_MIN]. The shared gate resolves NaN to
 * severity 0. Finite severities are bit-identical: the reader clamps the node
 * index where this clamped the segment, and the two agree everywhere including
 * severity exactly 1.0, where both evaluate to lut[10]. */
ALWAN_INLINE ALWAN_CORE_MAT3X3 ALWAN_CORE_FNVLIT(alwan_machado_interpolate)(
    ALWAN_CORE_MAT3X3 const *lut, ALWAN_CORE_T severity)
{
    return ALWAN_CORE_FNVLIT(alwan_table1d_mat3_sample_linear)(
        lut, ALWAN_MACHADO_SEVERITY_STEPS, severity);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_cvd_machado)(
    ALWAN_CORE_RGB rgb, ALWAN_CORE_MAT3X3 const *lut, ALWAN_CORE_T severity)
{
    ALWAN_CORE_MAT3X3 mat = ALWAN_CORE_FNVLIT(alwan_machado_interpolate)(lut, severity);
    ALWAN_CORE_VEC3 v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 mapped = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(mat, v);

    /* Raw Machado 2009 matrix product -- no gamut clamp (the reference model
     * defines only the matrix; colour-science does not clip either). Use
     * alwan_simulate_cvd_machado_gamut_safe for a guaranteed in-gamut result. */
    ALWAN_CORE_RGB result;
    result.r = mapped.v[0];
    result.g = mapped.v[1];
    result.b = mapped.v[2];
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_machado_protan)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_machado)(rgb, ALWAN_CORE_FNLIT(MACHADO_PROTAN), severity);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_machado_deutan)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_machado)(rgb, ALWAN_CORE_FNLIT(MACHADO_DEUTAN), severity);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_machado_tritan)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_machado)(rgb, ALWAN_CORE_FNLIT(MACHADO_TRITAN), severity);
}

/* ================================================================
 * Contrast Sensitivity Function (CSF) -- Barten 1999
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_pupil_diameter_barten1999)(
    ALWAN_CORE_T L, ALWAN_CORE_T X_0, ALWAN_CORE_T Y_0)
{
    ALWAN_CORE_T Y = ALWAN_CORE_SELECT(Y_0 < ALWAN_CORE_ZERO, X_0, Y_0);
    ALWAN_CORE_T arg = ALWAN_CORE_LITERAL(0.4) * ALWAN_CORE_LOG10(L * X_0 * Y / ALWAN_CORE_LITERAL(1600.0));
    return ALWAN_CORE_LITERAL(5.0) - ALWAN_CORE_LITERAL(3.0) * ALWAN_CORE_TANH(arg);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_retinal_illuminance_barten1999)(
    ALWAN_CORE_T L, ALWAN_CORE_T d, ALWAN_CORE_T apply_stiles_crawford)
{
    ALWAN_CORE_T E = (ALWAN_CORE_PI * d * d / ALWAN_CORE_LITERAL(4.0)) * L;

    ALWAN_CORE_T d_97 = d / ALWAN_CORE_LITERAL(9.7);
    ALWAN_CORE_T d_124 = d / ALWAN_CORE_LITERAL(12.4);
    ALWAN_CORE_T sc = ALWAN_CORE_ONE - d_97 * d_97 + d_124 * d_124 * d_124 * d_124;
    ALWAN_CORE_T correction = ALWAN_CORE_SELECT(apply_stiles_crawford > ALWAN_CORE_LITERAL(0.5), sc, ALWAN_CORE_ONE);

    return E * correction;
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_optical_mtf_barten1999)(ALWAN_CORE_T u, ALWAN_CORE_T sigma) {
    return ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-2.0) * ALWAN_CORE_PI * ALWAN_CORE_PI * sigma * sigma * u * u);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_sigma_barten1999)(
    ALWAN_CORE_T sigma_0, ALWAN_CORE_T C_ab, ALWAN_CORE_T d)
{
    ALWAN_CORE_T Cab_d = C_ab * d;
    return ALWAN_CORE_SQRT(sigma_0 * sigma_0 + Cab_d * Cab_d);
}

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_maximum_angular_size_barten1999)(
    ALWAN_CORE_T u, ALWAN_CORE_T X_0, ALWAN_CORE_T X_max, ALWAN_CORE_T N_max)
{
    ALWAN_CORE_T term1 = ALWAN_CORE_ONE / (X_0 * X_0);
    ALWAN_CORE_T term2 = ALWAN_CORE_ONE / (X_max * X_max);
    ALWAN_CORE_T term3 = (u * u) / (N_max * N_max);
    return ALWAN_CORE_POW(term1 + term2 + term3, ALWAN_CORE_LITERAL(-0.5));
}

/* Parameters for the full Barten 1999 CSF model */
typedef struct {
    ALWAN_CORE_T sigma;
    ALWAN_CORE_T k;
    ALWAN_CORE_T T;
    ALWAN_CORE_T X_0;
    ALWAN_CORE_T Y_0;
    ALWAN_CORE_T X_max;
    ALWAN_CORE_T Y_max;
    ALWAN_CORE_T N_max;
    ALWAN_CORE_T n;
    ALWAN_CORE_T p;
    ALWAN_CORE_T E;
    ALWAN_CORE_T phi_0;
    ALWAN_CORE_T u_0;
} ALWAN_CORE_FNLIT(alwan_csf_barten1999_v_params);

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_csf_barten1999)(
    ALWAN_CORE_T u, ALWAN_CORE_FNLIT(alwan_csf_barten1999_v_params) p)
{
    ALWAN_CORE_T Y_0   = ALWAN_CORE_SELECT(p.Y_0   < ALWAN_CORE_ZERO, p.X_0,   p.Y_0);
    ALWAN_CORE_T Y_max = ALWAN_CORE_SELECT(p.Y_max < ALWAN_CORE_ZERO, p.X_max, p.Y_max);

    ALWAN_CORE_T M_opt = ALWAN_CORE_FNVLIT(alwan_optical_mtf_barten1999)(u, p.sigma);

    ALWAN_CORE_T X = ALWAN_CORE_FNVLIT(alwan_maximum_angular_size_barten1999)(u, p.X_0, p.X_max, p.N_max);
    ALWAN_CORE_T Y = ALWAN_CORE_FNVLIT(alwan_maximum_angular_size_barten1999)(u, Y_0, Y_max, p.N_max);
    ALWAN_CORE_T M_as = ALWAN_CORE_ONE / (X * Y);

    ALWAN_CORE_T photon_term = ALWAN_CORE_ONE / (p.n * p.p * p.E);

    ALWAN_CORE_T u_ratio = u / p.u_0;
    ALWAN_CORE_T neural_term = p.phi_0 / (ALWAN_CORE_ONE - ALWAN_CORE_EXP(-(u_ratio * u_ratio)));

    ALWAN_CORE_T noise = (ALWAN_CORE_LITERAL(2.0) / p.T) * M_as * (photon_term + neural_term);

    return (M_opt / p.k) / ALWAN_CORE_SQRT(noise);
}

/* Barten-inspired simplified CSF model for fast approximation.
 * Not a direct implementation of Barten 1999; constants are fitted to
 * qualitatively match the full model without integration overhead.
 * For accurate CSF evaluation use alwan_csf_barten1999() instead.
 * Rolloff constants: low-freq (-3dB at 0.5 cpd), noise floor phi_0=3e-8,
 * detection threshold multiplier k=3. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_csf_simple)(
    ALWAN_CORE_T spatial_frequency, ALWAN_CORE_T luminance)
{
    ALWAN_CORE_T f = spatial_frequency;
    ALWAN_CORE_T L = luminance;

    ALWAN_CORE_T log_L = ALWAN_CORE_LOG10(L);
    ALWAN_CORE_T d = ALWAN_CORE_LITERAL(5.0) - ALWAN_CORE_LITERAL(3.0) * ALWAN_CORE_TANH(ALWAN_CORE_LITERAL(0.4) * log_L);
    ALWAN_CORE_T pupil_area = ALWAN_CORE_PI * d * d / ALWAN_CORE_LITERAL(4.0);
    ALWAN_CORE_T E = L * pupil_area;

    ALWAN_CORE_T low_freq_atten  = f / (f + ALWAN_CORE_LITERAL(0.5));
    ALWAN_CORE_T high_freq_atten = ALWAN_CORE_EXP(ALWAN_CORE_LITERAL(-0.005) * f * f);
    ALWAN_CORE_T M_opt = low_freq_atten * high_freq_atten;

    ALWAN_CORE_T phi_0 = ALWAN_CORE_LITERAL(3.0e-8);
    ALWAN_CORE_T k = ALWAN_CORE_LITERAL(3.0);
    ALWAN_CORE_T noise_photon = phi_0 / (E + ALWAN_CORE_LITERAL(1e-10));
    ALWAN_CORE_T noise_neural = ALWAN_CORE_ONE / k;
    ALWAN_CORE_T noise_total = ALWAN_CORE_SQRT(noise_photon * noise_photon + noise_neural * noise_neural);

    return (M_opt * E) / (noise_total + ALWAN_CORE_LITERAL(1e-10)) * ALWAN_CORE_LITERAL(10.0);
}

/* ================================================================
 * WCAG 2.x Contrast Ratio
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_wcag_contrast_ratio)(ALWAN_CORE_T Y1,
                                                       ALWAN_CORE_T Y2) {
    ALWAN_CORE_T L1 = ALWAN_CORE_SELECT(Y1 > Y2, Y1, Y2);
    ALWAN_CORE_T L2 = ALWAN_CORE_SELECT(Y1 > Y2, Y2, Y1);
    return (L1 + ALWAN_CORE_LITERAL(0.05)) / (L2 + ALWAN_CORE_LITERAL(0.05));
}

/* ================================================================
 * APCA (Advanced Perceptual Contrast Algorithm)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_apca_contrast)(ALWAN_CORE_RGB srgb_text,
                                                  ALWAN_CORE_RGB srgb_bg) {
    const ALWAN_CORE_T mainTRC = ALWAN_CORE_LITERAL(2.4);

    ALWAN_CORE_T Rtxt = ALWAN_CORE_POW(srgb_text.r, mainTRC);
    ALWAN_CORE_T Gtxt = ALWAN_CORE_POW(srgb_text.g, mainTRC);
    ALWAN_CORE_T Btxt = ALWAN_CORE_POW(srgb_text.b, mainTRC);

    ALWAN_CORE_T Rbg = ALWAN_CORE_POW(srgb_bg.r, mainTRC);
    ALWAN_CORE_T Gbg = ALWAN_CORE_POW(srgb_bg.g, mainTRC);
    ALWAN_CORE_T Bbg = ALWAN_CORE_POW(srgb_bg.b, mainTRC);

    const ALWAN_CORE_T Rco = ALWAN_CORE_LITERAL(0.2126729);
    const ALWAN_CORE_T Gco = ALWAN_CORE_LITERAL(0.7151522);
    const ALWAN_CORE_T Bco = ALWAN_CORE_LITERAL(0.0721750);

    ALWAN_CORE_T Ytxt = Rco * Rtxt + Gco * Gtxt + Bco * Btxt;
    ALWAN_CORE_T Ybg  = Rco * Rbg  + Gco * Gbg  + Bco * Bbg;

    const ALWAN_CORE_T blkThrs = ALWAN_CORE_LITERAL(0.022);
    const ALWAN_CORE_T blkClmp = ALWAN_CORE_LITERAL(1.414);

    ALWAN_CORE_T diffTxt = ALWAN_CORE_SELECT(Ytxt < blkThrs, blkThrs - Ytxt, ALWAN_CORE_ZERO);
    ALWAN_CORE_T diffBg  = ALWAN_CORE_SELECT(Ybg  < blkThrs, blkThrs - Ybg,  ALWAN_CORE_ZERO);
    Ytxt = ALWAN_CORE_SELECT(Ytxt < blkThrs, Ytxt + ALWAN_CORE_POW(diffTxt, blkClmp), Ytxt);
    Ybg  = ALWAN_CORE_SELECT(Ybg  < blkThrs, Ybg  + ALWAN_CORE_POW(diffBg,  blkClmp), Ybg);

    const ALWAN_CORE_T normBG  = ALWAN_CORE_LITERAL(0.56);
    const ALWAN_CORE_T normTXT = ALWAN_CORE_LITERAL(0.57);
    const ALWAN_CORE_T revBG   = ALWAN_CORE_LITERAL(0.65);
    const ALWAN_CORE_T revTXT  = ALWAN_CORE_LITERAL(0.62);
    const ALWAN_CORE_T scaleBO = ALWAN_CORE_LITERAL(1.14);
    const ALWAN_CORE_T loClip  = ALWAN_CORE_LITERAL(0.1);
    const ALWAN_CORE_T loBoWoffset = ALWAN_CORE_LITERAL(0.027);

    ALWAN_CORE_T Sapc_normal = (ALWAN_CORE_POW(Ybg, normBG) - ALWAN_CORE_POW(Ytxt, normTXT)) * scaleBO;
    ALWAN_CORE_T Sapc_reverse = (ALWAN_CORE_POW(Ybg, revBG) - ALWAN_CORE_POW(Ytxt, revTXT)) * scaleBO;

    ALWAN_CORE_T SAPC = ALWAN_CORE_SELECT(Ybg > Ytxt, Sapc_normal, Sapc_reverse);

    ALWAN_CORE_T abs_SAPC = ALWAN_CORE_SELECT(SAPC < ALWAN_CORE_ZERO, -SAPC, SAPC);

    ALWAN_CORE_T Lc_pos = SAPC - loBoWoffset;
    ALWAN_CORE_T Lc_neg = SAPC + loBoWoffset;
    ALWAN_CORE_T Lc = ALWAN_CORE_SELECT(SAPC > ALWAN_CORE_ZERO, Lc_pos, Lc_neg);

    Lc = ALWAN_CORE_SELECT(abs_SAPC < loClip, ALWAN_CORE_ZERO, Lc);

    return Lc * ALWAN_CORE_LITERAL(100.0);
}
