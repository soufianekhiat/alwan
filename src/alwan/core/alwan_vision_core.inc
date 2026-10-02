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
 * Brettel, Vienot & Mollon (1997), as DaltonLens-Python's Simulator_Brettel1997 at its
 * defaults (gendata/data/cvd_matrices.py): linear sRGB to LMS by Smith and Pokorny 1975,
 * then the missing cone's coordinate replaced by its projection onto one of two
 * half-planes through the neutral axis (RGB white), H2 where the LMS falls on the negative
 * side of the separation plane (normal n), H1 elsewhere; back to linear sRGB. The anchors
 * are 475 / 575 nm for protanopia and deuteranopia, 485 / 660 nm for tritanopia.
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_RGB_TO_LMS) = {{
#include "../data/matrices/cvd_rgb_to_lms.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_LMS_TO_RGB) = {{
#include "../data/matrices/cvd_lms_to_rgb.csv"
}};

ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_BRETTEL_PROTAN_H1) = {{
#include "../data/matrices/cvd_brettel_protan_h1.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_BRETTEL_PROTAN_H2) = {{
#include "../data/matrices/cvd_brettel_protan_h2.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_VEC3 ALWAN_CORE_FNLIT(CVD_BRETTEL_PROTAN_N) = {{
#include "../data/matrices/cvd_brettel_protan_n.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_BRETTEL_DEUTAN_H1) = {{
#include "../data/matrices/cvd_brettel_deutan_h1.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_BRETTEL_DEUTAN_H2) = {{
#include "../data/matrices/cvd_brettel_deutan_h2.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_VEC3 ALWAN_CORE_FNLIT(CVD_BRETTEL_DEUTAN_N) = {{
#include "../data/matrices/cvd_brettel_deutan_n.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_BRETTEL_TRITAN_H1) = {{
#include "../data/matrices/cvd_brettel_tritan_h1.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_BRETTEL_TRITAN_H2) = {{
#include "../data/matrices/cvd_brettel_tritan_h2.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_VEC3 ALWAN_CORE_FNLIT(CVD_BRETTEL_TRITAN_N) = {{
#include "../data/matrices/cvd_brettel_tritan_n.csv"
}};
/* Vienot, Brettel and Mollon 1999: one plane, so one linear-sRGB matrix per deficiency
 * (DaltonLens's Simulator_Vienot1999 on the same LMS model). The tritan matrix is the
 * red-cyan plane DaltonLens builds; the paper does not cover tritanopia. */
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_VIENOT_PROTAN) = {{
#include "../data/matrices/cvd_vienot_protan.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_VIENOT_DEUTAN) = {{
#include "../data/matrices/cvd_vienot_deutan.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CVD_VIENOT_TRITAN) = {{
#include "../data/matrices/cvd_vienot_tritan.csv"
}};

ALWAN_DIAG_POP

/* Vienot 1999: the one matrix, then mixed back by severity as Brettel is. */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_cvd_vienot)(ALWAN_CORE_RGB rgb, ALWAN_CORE_MAT3X3 M,
                                                                       ALWAN_CORE_T severity) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 cvd = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(M, rgb_v);
    severity = ALWAN_CORE_CLAMP(severity, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    result.r = cvd.v[0] * severity + rgb.r * (ALWAN_CORE_ONE - severity);
    result.g = cvd.v[1] * severity + rgb.g * (ALWAN_CORE_ONE - severity);
    result.b = cvd.v[2] * severity + rgb.b * (ALWAN_CORE_ONE - severity);
    return result;
}

/* Brettel's two-plane dichromat simulation of linear sRGB, then mixed back with the input
 * by severity (DaltonLens's anomalous-trichromacy rule, linear in linear sRGB). Raw: a
 * saturated input can land outside [0, 1]; alwan_simulate_cvd_gamut_safe clamps. */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_cvd_brettel)(ALWAN_CORE_RGB rgb,
                                                    ALWAN_CORE_MAT3X3 H1, ALWAN_CORE_MAT3X3 H2,
                                                    ALWAN_CORE_VEC3 n, ALWAN_CORE_T severity) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_VEC3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CVD_RGB_TO_LMS), rgb_v);
    ALWAN_CORE_VEC3 p1 = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(H1, lms);
    ALWAN_CORE_VEC3 p2 = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(H2, lms);
    ALWAN_CORE_T side = lms.v[0] * n.v[0] + lms.v[1] * n.v[1] + lms.v[2] * n.v[2];
    ALWAN_CORE_VEC3 p;
    p.v[0] = ALWAN_CORE_SELECT(side < ALWAN_CORE_ZERO, p2.v[0], p1.v[0]);
    p.v[1] = ALWAN_CORE_SELECT(side < ALWAN_CORE_ZERO, p2.v[1], p1.v[1]);
    p.v[2] = ALWAN_CORE_SELECT(side < ALWAN_CORE_ZERO, p2.v[2], p1.v[2]);
    ALWAN_CORE_VEC3 cvd = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CVD_LMS_TO_RGB), p);

    severity = ALWAN_CORE_CLAMP(severity, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    result.r = cvd.v[0] * severity + rgb.r * (ALWAN_CORE_ONE - severity);
    result.g = cvd.v[1] * severity + rgb.g * (ALWAN_CORE_ONE - severity);
    result.b = cvd.v[2] * severity + rgb.b * (ALWAN_CORE_ONE - severity);
    return result;
}

/* ================================================================
 * Named CVD Simulation Functions
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_protanopia)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_brettel)(rgb, ALWAN_CORE_FNLIT(CVD_BRETTEL_PROTAN_H1),
                                                         ALWAN_CORE_FNLIT(CVD_BRETTEL_PROTAN_H2),
                                                         ALWAN_CORE_FNLIT(CVD_BRETTEL_PROTAN_N), severity);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_deuteranopia)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_brettel)(rgb, ALWAN_CORE_FNLIT(CVD_BRETTEL_DEUTAN_H1),
                                                         ALWAN_CORE_FNLIT(CVD_BRETTEL_DEUTAN_H2),
                                                         ALWAN_CORE_FNLIT(CVD_BRETTEL_DEUTAN_N), severity);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_tritanopia)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T severity) {
    return ALWAN_CORE_FNVLIT(alwan_simulate_cvd_brettel)(rgb, ALWAN_CORE_FNLIT(CVD_BRETTEL_TRITAN_H1),
                                                         ALWAN_CORE_FNLIT(CVD_BRETTEL_TRITAN_H2),
                                                         ALWAN_CORE_FNLIT(CVD_BRETTEL_TRITAN_N), severity);
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
    ALWAN_PARAM_ARRAY_IN(ALWAN_CORE_MAT3X3, lut, ALWAN_MACHADO_SEVERITY_STEPS), ALWAN_CORE_T severity)
{
    const ALWAN_CORE_TABLE_CELL c = ALWAN_CORE_FNVLIT(alwan_table_cell)(
        severity, ALWAN_MACHADO_SEVERITY_STEPS);
    return ALWAN_CORE_FNVLIT(alwan_table_blend_mat3)(lut[c.i0], lut[c.i1], c.frac);
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_simulate_cvd_machado)(
    ALWAN_CORE_RGB rgb, ALWAN_PARAM_ARRAY_IN(ALWAN_CORE_MAT3X3, lut, ALWAN_MACHADO_SEVERITY_STEPS), ALWAN_CORE_T severity)
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
 * Helmholtz-Kohlrausch effect (Nayatani 1997)
 *
 * How much brighter a chromatic stimulus looks than an achromatic one of the
 * same luminance. Everything works in CIE 1960 UCS chromaticities, the stimulus
 * against the adapting field.
 * ================================================================ */

/* Brightness coefficient of the adapting luminance. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hke_coefficient_K_Br)(ALWAN_CORE_T L_a) {
    ALWAN_CORE_T p = ALWAN_CORE_POW(L_a, ALWAN_CORE_LITERAL(0.4495));
    return (p * ALWAN_CORE_LITERAL(6.362) + ALWAN_CORE_LITERAL(6.469)) *
           ALWAN_CORE_LITERAL(0.2717) / (p + ALWAN_CORE_LITERAL(6.469));
}

/* Hue-dependent coefficient: four harmonics of the chromaticity angle. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hke_coefficient_q)(ALWAN_CORE_T theta) {
    ALWAN_CORE_T t2 = theta + theta;
    ALWAN_CORE_T t3 = t2 + theta;
    ALWAN_CORE_T t4 = t2 + t2;
    return ALWAN_CORE_LITERAL(-0.01585)
         - ALWAN_CORE_LITERAL(0.03017) * ALWAN_CORE_COS(theta)
         - ALWAN_CORE_LITERAL(0.04556) * ALWAN_CORE_COS(t2)
         - ALWAN_CORE_LITERAL(0.02667) * ALWAN_CORE_COS(t3)
         - ALWAN_CORE_LITERAL(0.00295) * ALWAN_CORE_COS(t4)
         + ALWAN_CORE_LITERAL(0.14592) * ALWAN_CORE_SIN(theta)
         + ALWAN_CORE_LITERAL(0.05084) * ALWAN_CORE_SIN(t2)
         - ALWAN_CORE_LITERAL(0.01900) * ALWAN_CORE_SIN(t3)
         - ALWAN_CORE_LITERAL(0.00764) * ALWAN_CORE_SIN(t4);
}

/* The object variant. method_coefficient is the caller's VCC or VAC weight.
 *
 * A stimulus sitting on the adapting field gives S_uv = 0 and therefore exactly
 * 1, which is the quantity's whole meaning: no chromatic content, no effect. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hke_object_nayatani1997)(
    ALWAN_CORE_T u,
    ALWAN_CORE_T v,
    ALWAN_CORE_T u_c,
    ALWAN_CORE_T v_c,
    ALWAN_CORE_T L_a,
    ALWAN_CORE_T method_coefficient)
{
    ALWAN_CORE_T du = u - u_c;
    ALWAN_CORE_T dv = v - v_c;
    ALWAN_CORE_T K_Br = ALWAN_CORE_FNVLIT(alwan_hke_coefficient_K_Br)(L_a);
    ALWAN_CORE_T q = ALWAN_CORE_FNVLIT(alwan_hke_coefficient_q)(ALWAN_CORE_ATAN2(dv, du));
    ALWAN_CORE_T S_uv = ALWAN_CORE_LITERAL(13.0) * ALWAN_CORE_SQRT(du * du + dv * dv);
    return ALWAN_CORE_ONE + (method_coefficient * q + ALWAN_CORE_LITERAL(0.0872) * K_Br) * S_uv;
}

/* The luminous variant, a cube of the object one. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hke_luminous_nayatani1997)(
    ALWAN_CORE_T u,
    ALWAN_CORE_T v,
    ALWAN_CORE_T u_c,
    ALWAN_CORE_T v_c,
    ALWAN_CORE_T L_a,
    ALWAN_CORE_T method_coefficient)
{
    ALWAN_CORE_T g = ALWAN_CORE_FNVLIT(alwan_hke_object_nayatani1997)(u, v, u_c, v_c, L_a,
                                                                      method_coefficient);
    ALWAN_CORE_T s = g + ALWAN_CORE_LITERAL(0.3086);
    return ALWAN_CORE_LITERAL(0.4462) * s * s * s;
}
