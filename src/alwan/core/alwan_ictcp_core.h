/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only ICtCp Color Space (ITU-R BT.2100 HDR)
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * Reference: ITU-R Recommendation BT.2100-3 (02/2025)
 */

#ifndef ALWAN_ICTCP_CORE_H
#define ALWAN_ICTCP_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_core.h"
#include "alwan_math_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* Temporarily remove backward-compat aliases from alwan_core.h
 * so that ALWAN_CORE_FN(alwan_pq_oetf) etc. token-paste correctly */
#undef alwan_pq_oetf
#undef alwan_pq_eotf
#undef alwan_hlg_oetf

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_ictcp_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_ictcp_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_RGB_TO_LMS = {{
#include "../data/ictcp_rgb_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_LMS_TO_RGB = {{
#include "../data/ictcp_lms_to_rgb.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_LMS_P_TO_ICTCP_PQ = {{
#include "../data/ictcp_lms_p_to_ictcp_pq.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_ICTCP_TO_LMS_P_PQ = {{
#include "../data/ictcp_ictcp_to_lms_p_pq.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_LMS_P_TO_ICTCP_HLG = {{
#include "../data/ictcp_lms_p_to_ictcp_hlg.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_ICTCP_TO_LMS_P_HLG = {{
#include "../data/ictcp_ictcp_to_lms_p_hlg.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_XYZ_TO_BT2020 = {{
#include "../data/ictcp_xyz_to_bt2020.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 ICTCP_BT2020_TO_XYZ = {{
#include "../data/ictcp_bt2020_to_xyz.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_scalar alwan_hlg_inverse_oetf_v(alwan_scalar encoded) {
    alwan_scalar a = ALWAN_LITERAL(0.17883277);
    alwan_scalar b = ALWAN_ONE - ALWAN_LITERAL(4.0) * a;
    alwan_scalar c = ALWAN_LITERAL(0.5) - a * ALWAN_LN(ALWAN_LITERAL(4.0) * a);

    alwan_scalar E = ALWAN_SELECT(encoded < ALWAN_ZERO, ALWAN_ZERO, encoded);
    alwan_scalar linear_lo = (E * E) / ALWAN_LITERAL(3.0);
    alwan_scalar linear_hi = (ALWAN_EXP((E - c) / a) + b) / ALWAN_LITERAL(12.0);
    return ALWAN_SELECT(E <= ALWAN_LITERAL(0.5), linear_lo, linear_hi);
}

ALWAN_INLINE alwan_ictcp alwan_rgb_to_ictcp_pq_v(alwan_rgb rgb) {
    alwan_ictcp result;
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 lms = alwan_mat3_mulv_v(ICTCP_RGB_TO_LMS, rgb_v);
    alwan_scalar l = lms.v[0]; alwan_scalar m = lms.v[1]; alwan_scalar s = lms.v[2];
    alwan_scalar lp = alwan_pq_oetf(l);
    alwan_scalar mp = alwan_pq_oetf(m);
    alwan_scalar sp = alwan_pq_oetf(s);
    alwan_vec3 lms_p = {{lp, mp, sp}};
    alwan_vec3 ictcp = alwan_mat3_mulv_v(ICTCP_LMS_P_TO_ICTCP_PQ, lms_p);
    result.I = ictcp.v[0]; result.Ct = ictcp.v[1]; result.Cp = ictcp.v[2];
    return result;
}

ALWAN_INLINE alwan_rgb alwan_ictcp_pq_to_rgb_v(alwan_ictcp ictcp) {
    alwan_rgb result;
    alwan_vec3 ictcp_v = {{ictcp.I, ictcp.Ct, ictcp.Cp}};
    alwan_vec3 lms_p = alwan_mat3_mulv_v(ICTCP_ICTCP_TO_LMS_P_PQ, ictcp_v);
    alwan_scalar l = alwan_pq_eotf(lms_p.v[0]);
    alwan_scalar m = alwan_pq_eotf(lms_p.v[1]);
    alwan_scalar s = alwan_pq_eotf(lms_p.v[2]);
    alwan_vec3 lms_v = {{l, m, s}};
    alwan_vec3 rgb = alwan_mat3_mulv_v(ICTCP_LMS_TO_RGB, lms_v);
    result.r = rgb.v[0]; result.g = rgb.v[1]; result.b = rgb.v[2];
    return result;
}

ALWAN_INLINE alwan_ictcp alwan_rgb_to_ictcp_hlg_v(alwan_rgb rgb) {
    alwan_ictcp result;
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 lms = alwan_mat3_mulv_v(ICTCP_RGB_TO_LMS, rgb_v);
    alwan_scalar lp = alwan_hlg_oetf(lms.v[0]);
    alwan_scalar mp = alwan_hlg_oetf(lms.v[1]);
    alwan_scalar sp = alwan_hlg_oetf(lms.v[2]);
    alwan_vec3 lms_p = {{lp, mp, sp}};
    alwan_vec3 ictcp = alwan_mat3_mulv_v(ICTCP_LMS_P_TO_ICTCP_HLG, lms_p);
    result.I = ictcp.v[0]; result.Ct = ictcp.v[1]; result.Cp = ictcp.v[2];
    return result;
}

ALWAN_INLINE alwan_rgb alwan_ictcp_hlg_to_rgb_v(alwan_ictcp ictcp) {
    alwan_rgb result;
    alwan_vec3 ictcp_v = {{ictcp.I, ictcp.Ct, ictcp.Cp}};
    alwan_vec3 lms_p = alwan_mat3_mulv_v(ICTCP_ICTCP_TO_LMS_P_HLG, ictcp_v);
    alwan_scalar l = alwan_hlg_inverse_oetf_v(lms_p.v[0]);
    alwan_scalar m = alwan_hlg_inverse_oetf_v(lms_p.v[1]);
    alwan_scalar s = alwan_hlg_inverse_oetf_v(lms_p.v[2]);
    alwan_vec3 lms_v = {{l, m, s}};
    alwan_vec3 rgb = alwan_mat3_mulv_v(ICTCP_LMS_TO_RGB, lms_v);
    result.r = rgb.v[0]; result.g = rgb.v[1]; result.b = rgb.v[2];
    return result;
}

ALWAN_INLINE alwan_ictcp alwan_xyz_to_ictcp_pq_v(alwan_xyz xyz) {
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 rgb_v = alwan_mat3_mulv_v(ICTCP_XYZ_TO_BT2020, xyz_v);
    alwan_rgb rgb; rgb.r = rgb_v.v[0]; rgb.g = rgb_v.v[1]; rgb.b = rgb_v.v[2];
    return alwan_rgb_to_ictcp_pq_v(rgb);
}

ALWAN_INLINE alwan_xyz alwan_ictcp_pq_to_xyz_v(alwan_ictcp ictcp) {
    alwan_xyz result;
    alwan_rgb rgb = alwan_ictcp_pq_to_rgb_v(ictcp);
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 xyz = alwan_mat3_mulv_v(ICTCP_BT2020_TO_XYZ, rgb_v);
    result.x = xyz.v[0]; result.y = xyz.v[1]; result.z = xyz.v[2];
    return result;
}

ALWAN_INLINE alwan_ictcp alwan_xyz_to_ictcp_hlg_v(alwan_xyz xyz) {
    alwan_vec3 xyz_v = {{xyz.x, xyz.y, xyz.z}};
    alwan_vec3 rgb_v = alwan_mat3_mulv_v(ICTCP_XYZ_TO_BT2020, xyz_v);
    alwan_rgb rgb; rgb.r = rgb_v.v[0]; rgb.g = rgb_v.v[1]; rgb.b = rgb_v.v[2];
    return alwan_rgb_to_ictcp_hlg_v(rgb);
}

ALWAN_INLINE alwan_xyz alwan_ictcp_hlg_to_xyz_v(alwan_ictcp ictcp) {
    alwan_xyz result;
    alwan_rgb rgb = alwan_ictcp_hlg_to_rgb_v(ictcp);
    alwan_vec3 rgb_v = {{rgb.r, rgb.g, rgb.b}};
    alwan_vec3 xyz = alwan_mat3_mulv_v(ICTCP_BT2020_TO_XYZ, rgb_v);
    result.x = xyz.v[0]; result.y = xyz.v[1]; result.z = xyz.v[2];
    return result;
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_ICTCP_CORE_H */
