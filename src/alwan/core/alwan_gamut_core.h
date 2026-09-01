/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Gamut Mapping & Oklab utilities
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 *
 * References:
 *   - Bjorn Ottosson, "A perceptual color space for image processing" (2020)
 *   - Bjorn Ottosson, "Gamut clipping" (2021)
 */

#ifndef ALWAN_GAMUT_CORE_H
#define ALWAN_GAMUT_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"
#include "alwan_oklab_core.h"
#include "alwan_colorspace_core.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_gamut_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_gamut_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

ALWAN_INLINE alwan_vec3 gamut_clip_v(alwan_vec3 rgb) {
    alwan_vec3 r;
    r.v[0] = alwan_clamp(rgb.v[0], ALWAN_ZERO, ALWAN_ONE);
    r.v[1] = alwan_clamp(rgb.v[1], ALWAN_ZERO, ALWAN_ONE);
    r.v[2] = alwan_clamp(rgb.v[2], ALWAN_ZERO, ALWAN_ONE);
    return r;
}

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR alwan_mat3x3 CSS_SRGB_TO_LMS = {{
#include "../data/matrices/css_srgb_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CSS_LMS_TO_LAB = {{
#include "../data/matrices/css_lms_to_lab.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CSS_LAB_TO_LMS = {{
#include "../data/matrices/css_lab_to_lms.csv"
}};
ALWAN_CONSTEXPR alwan_mat3x3 CSS_LMS_TO_SRGB = {{
#include "../data/matrices/css_lms_to_srgb.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE alwan_vec3 gamut_linear_srgb_to_oklab_v(alwan_vec3 rgb) {
    alwan_vec3 lms = alwan_mat3_mulv_v(CSS_SRGB_TO_LMS, rgb);
    alwan_vec3 lms_p = {{ALWAN_CBRT(lms.v[0]), ALWAN_CBRT(lms.v[1]), ALWAN_CBRT(lms.v[2])}};
    return alwan_mat3_mulv_v(CSS_LMS_TO_LAB, lms_p);
}

ALWAN_INLINE alwan_vec3 gamut_oklab_to_linear_srgb_v(alwan_vec3 oklab) {
    alwan_vec3 lms_p = alwan_mat3_mulv_v(CSS_LAB_TO_LMS, oklab);
    alwan_vec3 lms = {{lms_p.v[0] * lms_p.v[0] * lms_p.v[0],
                       lms_p.v[1] * lms_p.v[1] * lms_p.v[1],
                       lms_p.v[2] * lms_p.v[2] * lms_p.v[2]}};
    return alwan_mat3_mulv_v(CSS_LMS_TO_SRGB, lms);
}

ALWAN_INLINE alwan_scalar gamut_compute_max_saturation_v(alwan_scalar a, alwan_scalar b) {
    alwan_scalar k0, k1, k2, k3, k4, wl, wm, ws;
    if (-ALWAN_LITERAL(1.88170328) * a - ALWAN_LITERAL(0.80936493) * b > ALWAN_LITERAL(1.0)) {
        k0 = +ALWAN_LITERAL(1.19086277); k1 = +ALWAN_LITERAL(1.76576728);
        k2 = +ALWAN_LITERAL(0.59662641); k3 = +ALWAN_LITERAL(0.75515197);
        k4 = +ALWAN_LITERAL(0.56771245);
        wl = CSS_LMS_TO_SRGB.m[0]; wm = CSS_LMS_TO_SRGB.m[1]; ws = CSS_LMS_TO_SRGB.m[2];
    } else if (ALWAN_LITERAL(1.81444104) * a - ALWAN_LITERAL(1.19445276) * b > ALWAN_LITERAL(1.0)) {
        k0 = +ALWAN_LITERAL(0.73956515); k1 = -ALWAN_LITERAL(0.45954404);
        k2 = +ALWAN_LITERAL(0.08285427); k3 = +ALWAN_LITERAL(0.12541070);
        k4 = +ALWAN_LITERAL(0.14503204);
        wl = CSS_LMS_TO_SRGB.m[3]; wm = CSS_LMS_TO_SRGB.m[4]; ws = CSS_LMS_TO_SRGB.m[5];
    } else {
        k0 = +ALWAN_LITERAL(1.35733652); k1 = -ALWAN_LITERAL(0.00915799);
        k2 = -ALWAN_LITERAL(1.15130210); k3 = -ALWAN_LITERAL(0.50559606);
        k4 = +ALWAN_LITERAL(0.00692167);
        wl = CSS_LMS_TO_SRGB.m[6]; wm = CSS_LMS_TO_SRGB.m[7]; ws = CSS_LMS_TO_SRGB.m[8];
    }
    alwan_scalar S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;
    {
        alwan_scalar k_l = CSS_LAB_TO_LMS.m[1] * a + CSS_LAB_TO_LMS.m[2] * b;
        alwan_scalar k_m = CSS_LAB_TO_LMS.m[4] * a + CSS_LAB_TO_LMS.m[5] * b;
        alwan_scalar k_s = CSS_LAB_TO_LMS.m[7] * a + CSS_LAB_TO_LMS.m[8] * b;
        alwan_scalar l_ = ALWAN_LITERAL(1.0) + S * k_l;
        alwan_scalar m_ = ALWAN_LITERAL(1.0) + S * k_m;
        alwan_scalar s_ = ALWAN_LITERAL(1.0) + S * k_s;
        alwan_scalar l = l_ * l_ * l_; alwan_scalar m = m_ * m_ * m_; alwan_scalar s = s_ * s_ * s_;
        alwan_scalar l_dS = ALWAN_LITERAL(3.0) * k_l * l_ * l_;
        alwan_scalar m_dS = ALWAN_LITERAL(3.0) * k_m * m_ * m_;
        alwan_scalar s_dS = ALWAN_LITERAL(3.0) * k_s * s_ * s_;
        alwan_scalar f = wl * l + wm * m + ws * s;
        alwan_scalar f_dS = wl * l_dS + wm * m_dS + ws * s_dS;
        S = S - f / f_dS;
    }
    return S;
}

ALWAN_INLINE alwan_vec2 gamut_find_cusp_v(alwan_scalar a, alwan_scalar b) {
    alwan_vec2 result;
    alwan_scalar S_cusp = gamut_compute_max_saturation_v(a, b);
    alwan_vec3 oklab_cusp;
    oklab_cusp.v[0] = ALWAN_ONE; oklab_cusp.v[1] = S_cusp * a; oklab_cusp.v[2] = S_cusp * b;
    alwan_vec3 rgb_cusp = gamut_oklab_to_linear_srgb_v(oklab_cusp);
    alwan_scalar max_rgb = rgb_cusp.v[0];
    max_rgb = ALWAN_SELECT(rgb_cusp.v[1] > max_rgb, rgb_cusp.v[1], max_rgb);
    max_rgb = ALWAN_SELECT(rgb_cusp.v[2] > max_rgb, rgb_cusp.v[2], max_rgb);
    result.v[0] = ALWAN_CBRT(ALWAN_ONE / max_rgb);
    result.v[1] = result.v[0] * S_cusp;
    return result;
}

ALWAN_INLINE alwan_scalar gamut_find_intersection_v(alwan_scalar a, alwan_scalar b,
                                                     alwan_scalar L1, alwan_scalar C1,
                                                     alwan_scalar L0, alwan_scalar C0) {
    alwan_vec2 cusp = gamut_find_cusp_v(a, b);
    alwan_scalar L_cusp = cusp.v[0]; alwan_scalar C_cusp = cusp.v[1];
    alwan_scalar t;
    if (((L1 - L0) * C_cusp - (C1 - C0) * L_cusp) <= ALWAN_ZERO) {
        t = C_cusp * L0 / (C1 * L_cusp + C_cusp * (L0 - L1));
    } else {
        t = C_cusp * (L0 - ALWAN_ONE) / (C1 * (L_cusp - ALWAN_ONE) + C_cusp * (L0 - L1));
        {
            alwan_scalar dL = L1 - L0; alwan_scalar dC = C1 - C0;
            alwan_scalar L = L0 * (ALWAN_ONE - t) + t * L1; alwan_scalar C = t * C1;
            alwan_scalar k_l = +ALWAN_LITERAL(0.3963377774) * a + ALWAN_LITERAL(0.2158037573) * b;
            alwan_scalar k_m = -ALWAN_LITERAL(0.1055613458) * a - ALWAN_LITERAL(0.0638541728) * b;
            alwan_scalar k_s = -ALWAN_LITERAL(0.0894841775) * a - ALWAN_LITERAL(1.2914855480) * b;
            alwan_scalar l_ = L + C * k_l; alwan_scalar m_ = L + C * k_m; alwan_scalar s_ = L + C * k_s;
            alwan_scalar l = l_ * l_ * l_; alwan_scalar m = m_ * m_ * m_; alwan_scalar s = s_ * s_ * s_;
            alwan_scalar ldt = ALWAN_LITERAL(3.0) * dL * l_ * l_; alwan_scalar mdt = ALWAN_LITERAL(3.0) * dL * m_ * m_; alwan_scalar sdt = ALWAN_LITERAL(3.0) * dL * s_ * s_;
            alwan_scalar ldt2 = ALWAN_LITERAL(3.0) * dC * l_ * l_; alwan_scalar mdt2 = ALWAN_LITERAL(3.0) * dC * m_ * m_; alwan_scalar sdt2 = ALWAN_LITERAL(3.0) * dC * s_ * s_;
            alwan_scalar r = ALWAN_LITERAL(4.0767416621) * l - ALWAN_LITERAL(3.3077115913) * m + ALWAN_LITERAL(0.2309699292) * s - ALWAN_ONE;
            alwan_scalar r1 = ALWAN_LITERAL(4.0767416621) * ldt - ALWAN_LITERAL(3.3077115913) * mdt + ALWAN_LITERAL(0.2309699292) * sdt;
            alwan_scalar r2 = ALWAN_LITERAL(4.0767416621) * ldt2 - ALWAN_LITERAL(3.3077115913) * mdt2 + ALWAN_LITERAL(0.2309699292) * sdt2;
            alwan_scalar u_r = r1 / (r1 - r2); alwan_scalar t_r = -r / r1;
            alwan_scalar g = -ALWAN_LITERAL(1.2684380046) * l + ALWAN_LITERAL(2.6097574011) * m - ALWAN_LITERAL(0.3413193965) * s - ALWAN_ONE;
            alwan_scalar g1 = -ALWAN_LITERAL(1.2684380046) * ldt + ALWAN_LITERAL(2.6097574011) * mdt - ALWAN_LITERAL(0.3413193965) * sdt;
            alwan_scalar g2 = -ALWAN_LITERAL(1.2684380046) * ldt2 + ALWAN_LITERAL(2.6097574011) * mdt2 - ALWAN_LITERAL(0.3413193965) * sdt2;
            alwan_scalar u_g = g1 / (g1 - g2); alwan_scalar t_g = -g / g1;
            alwan_scalar b_val = -ALWAN_LITERAL(0.0041960863) * l - ALWAN_LITERAL(0.7034186147) * m + ALWAN_LITERAL(1.7076147010) * s - ALWAN_ONE;
            alwan_scalar b1 = -ALWAN_LITERAL(0.0041960863) * ldt - ALWAN_LITERAL(0.7034186147) * mdt + ALWAN_LITERAL(1.7076147010) * sdt;
            alwan_scalar b2 = -ALWAN_LITERAL(0.0041960863) * ldt2 - ALWAN_LITERAL(0.7034186147) * mdt2 + ALWAN_LITERAL(1.7076147010) * sdt2;
            alwan_scalar u_b = b1 / (b1 - b2); alwan_scalar t_b = -b_val / b1;
            t_r = ALWAN_SELECT(u_r >= ALWAN_ZERO, t_r, ALWAN_LITERAL(10000.0));
            t_g = ALWAN_SELECT(u_g >= ALWAN_ZERO, t_g, ALWAN_LITERAL(10000.0));
            t_b = ALWAN_SELECT(u_b >= ALWAN_ZERO, t_b, ALWAN_LITERAL(10000.0));
            alwan_scalar t_min = ALWAN_SELECT(t_r < t_g, t_r, t_g);
            t_min = ALWAN_SELECT(t_b < t_min, t_b, t_min);
            t += t_min;
        }
    }
    return t;
}

ALWAN_INLINE int gamut_css_in_gamut_v(alwan_vec3 rgb) {
    return rgb.v[0] >= -ALWAN_EPSILON && rgb.v[0] <= ALWAN_LITERAL(1.0) + ALWAN_EPSILON &&
           rgb.v[1] >= -ALWAN_EPSILON && rgb.v[1] <= ALWAN_LITERAL(1.0) + ALWAN_EPSILON &&
           rgb.v[2] >= -ALWAN_EPSILON && rgb.v[2] <= ALWAN_LITERAL(1.0) + ALWAN_EPSILON;
}

ALWAN_INLINE alwan_vec3 gamut_css_map_v(alwan_vec3 origin) {
    const alwan_scalar JND = ALWAN_LITERAL(0.02);
    const int MAX_ITER = 30;
    if (gamut_css_in_gamut_v(origin)) { return origin; }
    alwan_vec3 oklab = gamut_linear_srgb_to_oklab_v(origin);
    alwan_oklab ok; ok.L = oklab.v[0]; ok.a = oklab.v[1]; ok.b = oklab.v[2];
    alwan_oklch lch = alwan_oklab_to_oklch_v(ok);
    if (lch.L >= ALWAN_LITERAL(1.0)) { alwan_vec3 white = {{ALWAN_ONE, ALWAN_ONE, ALWAN_ONE}}; return white; }
    if (lch.L <= ALWAN_LITERAL(0.0)) { alwan_vec3 black = {{ALWAN_ZERO, ALWAN_ZERO, ALWAN_ZERO}}; return black; }
    {
        alwan_scalar lo = ALWAN_LITERAL(0.0); alwan_scalar hi = lch.C;
        alwan_oklch trial = lch; int i;
        for (i = 0; i < MAX_ITER; i++) {
            trial.C = (lo + hi) * ALWAN_LITERAL(0.5);
            alwan_oklab trial_oklab = alwan_oklch_to_oklab_v(trial);
            alwan_vec3 trial_oklab_v; trial_oklab_v.v[0] = trial_oklab.L; trial_oklab_v.v[1] = trial_oklab.a; trial_oklab_v.v[2] = trial_oklab.b;
            alwan_vec3 trial_rgb = gamut_oklab_to_linear_srgb_v(trial_oklab_v);
            alwan_vec3 clipped = gamut_clip_v(trial_rgb);
            alwan_vec3 clipped_oklab = gamut_linear_srgb_to_oklab_v(clipped);
            alwan_oklab clipped_ok; clipped_ok.L = clipped_oklab.v[0]; clipped_ok.a = clipped_oklab.v[1]; clipped_ok.b = clipped_oklab.v[2];
            alwan_scalar de = alwan_delta_e_ok_v(trial_oklab, clipped_ok);
            if (de - JND < ALWAN_EPSILON) {
                if (gamut_css_in_gamut_v(trial_rgb)) { break; }
                hi = trial.C;
            } else { hi = trial.C; }
            if (hi - lo < ALWAN_LITERAL(1e-12)) { break; }
        }
        {
            alwan_oklab final_oklab = alwan_oklch_to_oklab_v(trial);
            alwan_vec3 final_oklab_v; final_oklab_v.v[0] = final_oklab.L; final_oklab_v.v[1] = final_oklab.a; final_oklab_v.v[2] = final_oklab.b;
            alwan_vec3 final_rgb = gamut_oklab_to_linear_srgb_v(final_oklab_v);
            return gamut_clip_v(final_rgb);
        }
    }
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_GAMUT_CORE_H */
