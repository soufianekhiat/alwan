/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Gamut Mapping & Oklab utilities (parameterized)
 * No include guard - included twice via setup/teardown.
 *
 * References:
 *   - Bjorn Ottosson, "A perceptual color space for image processing" (2020)
 *   - Bjorn Ottosson, "Gamut clipping" (2021)
 */

/* ================================================================
 * RGB clamp to [0,1]
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(gamut_clip)(ALWAN_CORE_VEC3 rgb) {
    ALWAN_CORE_VEC3 r;
    r.v[0] = ALWAN_CORE_CLAMP(rgb.v[0], ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    r.v[1] = ALWAN_CORE_CLAMP(rgb.v[1], ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    r.v[2] = ALWAN_CORE_CLAMP(rgb.v[2], ALWAN_CORE_ZERO, ALWAN_CORE_ONE);
    return r;
}

/* ================================================================
 * CSS Oklab matrices: linear sRGB <-> Oklab (Ottosson 2020)
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CSS_SRGB_TO_LMS) = {{
#include "../data/matrices/css_srgb_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CSS_LMS_TO_LAB) = {{
#include "../data/matrices/css_lms_to_lab.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS) = {{
#include "../data/matrices/css_lab_to_lms.csv"
}};
ALWAN_CONSTEXPR ALWAN_CORE_MAT3X3 ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB) = {{
#include "../data/matrices/css_lms_to_srgb.csv"
}};
ALWAN_DIAG_POP

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(gamut_linear_srgb_to_oklab)(ALWAN_CORE_VEC3 rgb) {
    ALWAN_CORE_VEC3 lms = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CSS_SRGB_TO_LMS), rgb);
    ALWAN_CORE_VEC3 lms_p = {{ALWAN_CORE_CBRT(lms.v[0]), ALWAN_CORE_CBRT(lms.v[1]), ALWAN_CORE_CBRT(lms.v[2])}};
    return ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CSS_LMS_TO_LAB), lms_p);
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(gamut_oklab_to_linear_srgb)(ALWAN_CORE_VEC3 oklab) {
    ALWAN_CORE_VEC3 lms_p = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS), oklab);
    ALWAN_CORE_VEC3 lms = {{lms_p.v[0] * lms_p.v[0] * lms_p.v[0],
                       lms_p.v[1] * lms_p.v[1] * lms_p.v[1],
                       lms_p.v[2] * lms_p.v[2] * lms_p.v[2]}};
    return ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB), lms);
}

/* ================================================================
 * Maximum saturation for a given hue direction (a, b) in Oklab
 * ================================================================ */

/* Polynomial approximation for maximum saturation at a given Oklab (a, b) hue.
 * Three zones (R/G/B gamut faces); k0-k4 are fitted polynomial coefficients,
 * wl/wm/ws are the corresponding LMS-to-sRGB row elements for that face.
 * Coefficients from Ottosson (2021) "Gamut clipping"
 *   https://bottosson.github.io/posts/gamutclipping/ reference implementation. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(gamut_compute_max_saturation)(ALWAN_CORE_T a, ALWAN_CORE_T b) {
    ALWAN_CORE_T k0, k1, k2, k3, k4, wl, wm, ws;

    if (-ALWAN_CORE_LITERAL(1.88170328) * a - ALWAN_CORE_LITERAL(0.80936493) * b > ALWAN_CORE_LITERAL(1.0)) {
        k0 = +ALWAN_CORE_LITERAL(1.19086277); k1 = +ALWAN_CORE_LITERAL(1.76576728);
        k2 = +ALWAN_CORE_LITERAL(0.59662641); k3 = +ALWAN_CORE_LITERAL(0.75515197);
        k4 = +ALWAN_CORE_LITERAL(0.56771245);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[0]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[1];
        ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[2];
    } else if (ALWAN_CORE_LITERAL(1.81444104) * a - ALWAN_CORE_LITERAL(1.19445276) * b > ALWAN_CORE_LITERAL(1.0)) {
        k0 = +ALWAN_CORE_LITERAL(0.73956515); k1 = -ALWAN_CORE_LITERAL(0.45954404);
        k2 = +ALWAN_CORE_LITERAL(0.08285427); k3 = +ALWAN_CORE_LITERAL(0.12541070);
        k4 = +ALWAN_CORE_LITERAL(0.14503204);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[3]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[4];
        ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[5];
    } else {
        k0 = +ALWAN_CORE_LITERAL(1.35733652); k1 = -ALWAN_CORE_LITERAL(0.00915799);
        k2 = -ALWAN_CORE_LITERAL(1.15130210); k3 = -ALWAN_CORE_LITERAL(0.50559606);
        k4 = +ALWAN_CORE_LITERAL(0.00692167);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[6]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[7];
        ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[8];
    }

    ALWAN_CORE_T S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;

    {
        ALWAN_CORE_T k_l = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[1] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[2] * b;
        ALWAN_CORE_T k_m = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[4] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[5] * b;
        ALWAN_CORE_T k_s = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[7] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[8] * b;

        ALWAN_CORE_T l_ = ALWAN_CORE_LITERAL(1.0) + S * k_l;
        ALWAN_CORE_T m_ = ALWAN_CORE_LITERAL(1.0) + S * k_m;
        ALWAN_CORE_T s_ = ALWAN_CORE_LITERAL(1.0) + S * k_s;

        ALWAN_CORE_T l = l_ * l_ * l_;
        ALWAN_CORE_T m = m_ * m_ * m_;
        ALWAN_CORE_T s = s_ * s_ * s_;

        ALWAN_CORE_T l_dS = ALWAN_CORE_LITERAL(3.0) * k_l * l_ * l_;
        ALWAN_CORE_T m_dS = ALWAN_CORE_LITERAL(3.0) * k_m * m_ * m_;
        ALWAN_CORE_T s_dS = ALWAN_CORE_LITERAL(3.0) * k_s * s_ * s_;

        ALWAN_CORE_T f   = wl * l   + wm * m   + ws * s;
        ALWAN_CORE_T f_dS = wl * l_dS + wm * m_dS + ws * s_dS;

        S = S - f / f_dS;
    }

    return S;
}

/* ================================================================
 * Find gamut cusp (L, C) for a given hue direction (a, b)
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_VEC2 ALWAN_CORE_FNVLIT(gamut_find_cusp)(ALWAN_CORE_T a, ALWAN_CORE_T b) {
    ALWAN_CORE_VEC2 result;
    ALWAN_CORE_T S_cusp = ALWAN_CORE_FNVLIT(gamut_compute_max_saturation)(a, b);

    ALWAN_CORE_VEC3 oklab_cusp;
    oklab_cusp.v[0] = ALWAN_CORE_ONE;
    oklab_cusp.v[1] = S_cusp * a;
    oklab_cusp.v[2] = S_cusp * b;

    ALWAN_CORE_VEC3 rgb_cusp = ALWAN_CORE_FNVLIT(gamut_oklab_to_linear_srgb)(oklab_cusp);

    ALWAN_CORE_T max_rgb = rgb_cusp.v[0];
    max_rgb = ALWAN_CORE_SELECT(rgb_cusp.v[1] > max_rgb, rgb_cusp.v[1], max_rgb);
    max_rgb = ALWAN_CORE_SELECT(rgb_cusp.v[2] > max_rgb, rgb_cusp.v[2], max_rgb);

    result.v[0] = ALWAN_CORE_CBRT(ALWAN_CORE_ONE / max_rgb);
    result.v[1] = result.v[0] * S_cusp;
    return result;
}

/* ================================================================
 * Find gamut boundary intersection
 * ================================================================ */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(gamut_find_intersection)(ALWAN_CORE_T a, ALWAN_CORE_T b,
                                                     ALWAN_CORE_T L1, ALWAN_CORE_T C1,
                                                     ALWAN_CORE_T L0, ALWAN_CORE_T C0) {
    ALWAN_CORE_VEC2 cusp = ALWAN_CORE_FNVLIT(gamut_find_cusp)(a, b);
    ALWAN_CORE_T L_cusp = cusp.v[0];
    ALWAN_CORE_T C_cusp = cusp.v[1];

    ALWAN_CORE_T t;
    if (((L1 - L0) * C_cusp - (C1 - C0) * L_cusp) <= ALWAN_CORE_ZERO) {
        t = C_cusp * L0 / (C1 * L_cusp + C_cusp * (L0 - L1));
    } else {
        t = C_cusp * (L0 - ALWAN_CORE_ONE) / (C1 * (L_cusp - ALWAN_CORE_ONE) +
                                           C_cusp * (L0 - L1));

        {
            ALWAN_CORE_T dL = L1 - L0;
            ALWAN_CORE_T dC = C1 - C0;

            ALWAN_CORE_T L = L0 * (ALWAN_CORE_ONE - t) + t * L1;
            ALWAN_CORE_T C = t * C1;

            ALWAN_CORE_T k_l = +ALWAN_CORE_LITERAL(0.3963377774) * a + ALWAN_CORE_LITERAL(0.2158037573) * b;
            ALWAN_CORE_T k_m = -ALWAN_CORE_LITERAL(0.1055613458) * a - ALWAN_CORE_LITERAL(0.0638541728) * b;
            ALWAN_CORE_T k_s = -ALWAN_CORE_LITERAL(0.0894841775) * a - ALWAN_CORE_LITERAL(1.2914855480) * b;

            ALWAN_CORE_T l_ = L + C * k_l;
            ALWAN_CORE_T m_ = L + C * k_m;
            ALWAN_CORE_T s_ = L + C * k_s;

            ALWAN_CORE_T l = l_ * l_ * l_;
            ALWAN_CORE_T m = m_ * m_ * m_;
            ALWAN_CORE_T s = s_ * s_ * s_;

            ALWAN_CORE_T ldt  = ALWAN_CORE_LITERAL(3.0) * dL * l_ * l_;
            ALWAN_CORE_T mdt  = ALWAN_CORE_LITERAL(3.0) * dL * m_ * m_;
            ALWAN_CORE_T sdt  = ALWAN_CORE_LITERAL(3.0) * dL * s_ * s_;

            ALWAN_CORE_T ldt2 = ALWAN_CORE_LITERAL(3.0) * dC * l_ * l_;
            ALWAN_CORE_T mdt2 = ALWAN_CORE_LITERAL(3.0) * dC * m_ * m_;
            ALWAN_CORE_T sdt2 = ALWAN_CORE_LITERAL(3.0) * dC * s_ * s_;

            ALWAN_CORE_T r  = ALWAN_CORE_LITERAL(4.0767416621) * l  - ALWAN_CORE_LITERAL(3.3077115913) * m  +
                              ALWAN_CORE_LITERAL(0.2309699292) * s  - ALWAN_CORE_ONE;
            ALWAN_CORE_T r1 = ALWAN_CORE_LITERAL(4.0767416621) * ldt - ALWAN_CORE_LITERAL(3.3077115913) * mdt +
                              ALWAN_CORE_LITERAL(0.2309699292) * sdt;
            ALWAN_CORE_T r2 = ALWAN_CORE_LITERAL(4.0767416621) * ldt2 - ALWAN_CORE_LITERAL(3.3077115913) * mdt2 +
                              ALWAN_CORE_LITERAL(0.2309699292) * sdt2;

            ALWAN_CORE_T u_r = r1 / (r1 - r2);
            ALWAN_CORE_T t_r = -r / r1;

            ALWAN_CORE_T g  = -ALWAN_CORE_LITERAL(1.2684380046) * l  + ALWAN_CORE_LITERAL(2.6097574011) * m  -
                               ALWAN_CORE_LITERAL(0.3413193965) * s  - ALWAN_CORE_ONE;
            ALWAN_CORE_T g1 = -ALWAN_CORE_LITERAL(1.2684380046) * ldt + ALWAN_CORE_LITERAL(2.6097574011) * mdt -
                               ALWAN_CORE_LITERAL(0.3413193965) * sdt;
            ALWAN_CORE_T g2 = -ALWAN_CORE_LITERAL(1.2684380046) * ldt2 + ALWAN_CORE_LITERAL(2.6097574011) * mdt2 -
                               ALWAN_CORE_LITERAL(0.3413193965) * sdt2;

            ALWAN_CORE_T u_g = g1 / (g1 - g2);
            ALWAN_CORE_T t_g = -g / g1;

            ALWAN_CORE_T b_val = -ALWAN_CORE_LITERAL(0.0041960863) * l  - ALWAN_CORE_LITERAL(0.7034186147) * m  +
                                  ALWAN_CORE_LITERAL(1.7076147010) * s  - ALWAN_CORE_ONE;
            ALWAN_CORE_T b1    = -ALWAN_CORE_LITERAL(0.0041960863) * ldt - ALWAN_CORE_LITERAL(0.7034186147) * mdt +
                                  ALWAN_CORE_LITERAL(1.7076147010) * sdt;
            ALWAN_CORE_T b2    = -ALWAN_CORE_LITERAL(0.0041960863) * ldt2 - ALWAN_CORE_LITERAL(0.7034186147) * mdt2 +
                                  ALWAN_CORE_LITERAL(1.7076147010) * sdt2;

            ALWAN_CORE_T u_b = b1 / (b1 - b2);
            ALWAN_CORE_T t_b = -b_val / b1;

            t_r = ALWAN_CORE_SELECT(u_r >= ALWAN_CORE_ZERO, t_r, ALWAN_CORE_LITERAL(10000.0));
            t_g = ALWAN_CORE_SELECT(u_g >= ALWAN_CORE_ZERO, t_g, ALWAN_CORE_LITERAL(10000.0));
            t_b = ALWAN_CORE_SELECT(u_b >= ALWAN_CORE_ZERO, t_b, ALWAN_CORE_LITERAL(10000.0));

            ALWAN_CORE_T t_min = ALWAN_CORE_SELECT(t_r < t_g, t_r, t_g);
            t_min = ALWAN_CORE_SELECT(t_b < t_min, t_b, t_min);

            t += t_min;
        }
    }

    return t;
}

/* ================================================================
 * CSS Color Level 4 Section 13.2 Gamut Mapping
 * ================================================================ */

ALWAN_INLINE int ALWAN_CORE_FNVLIT(gamut_css_in_gamut)(ALWAN_CORE_VEC3 rgb) {
    return rgb.v[0] >= -ALWAN_CORE_EPSILON && rgb.v[0] <= ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EPSILON &&
           rgb.v[1] >= -ALWAN_CORE_EPSILON && rgb.v[1] <= ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EPSILON &&
           rgb.v[2] >= -ALWAN_CORE_EPSILON && rgb.v[2] <= ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_EPSILON;
}

ALWAN_INLINE ALWAN_CORE_VEC3 ALWAN_CORE_FNVLIT(gamut_css_map)(ALWAN_CORE_VEC3 origin) {
    const ALWAN_CORE_T JND = ALWAN_CORE_LITERAL(0.02);
    const int MAX_ITER = 30;

    if (ALWAN_CORE_FNVLIT(gamut_css_in_gamut)(origin)) {
        return origin;
    }

    ALWAN_CORE_VEC3 oklab = ALWAN_CORE_FNVLIT(gamut_linear_srgb_to_oklab)(origin);
    ALWAN_CORE_OKLAB ok;
    ok.L = oklab.v[0]; ok.a = oklab.v[1]; ok.b = oklab.v[2];
    ALWAN_CORE_OKLCH lch = ALWAN_CORE_FNVLIT(alwan_oklab_to_oklch)(ok);

    if (lch.L >= ALWAN_CORE_LITERAL(1.0)) {
        ALWAN_CORE_VEC3 white = {{ALWAN_CORE_ONE, ALWAN_CORE_ONE, ALWAN_CORE_ONE}};
        return white;
    }
    if (lch.L <= ALWAN_CORE_LITERAL(0.0)) {
        ALWAN_CORE_VEC3 black = {{ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, ALWAN_CORE_ZERO}};
        return black;
    }

    {
        ALWAN_CORE_T lo = ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T hi = lch.C;
        ALWAN_CORE_OKLCH trial = lch;
        int i;

        for (i = 0; i < MAX_ITER; i++) {
            trial.C = (lo + hi) * ALWAN_CORE_LITERAL(0.5);

            ALWAN_CORE_OKLAB trial_oklab = ALWAN_CORE_FNVLIT(alwan_oklch_to_oklab)(trial);
            ALWAN_CORE_VEC3 trial_oklab_v;
            trial_oklab_v.v[0] = trial_oklab.L;
            trial_oklab_v.v[1] = trial_oklab.a;
            trial_oklab_v.v[2] = trial_oklab.b;
            ALWAN_CORE_VEC3 trial_rgb = ALWAN_CORE_FNVLIT(gamut_oklab_to_linear_srgb)(trial_oklab_v);

            ALWAN_CORE_VEC3 clipped = ALWAN_CORE_FNVLIT(gamut_clip)(trial_rgb);

            ALWAN_CORE_VEC3 clipped_oklab = ALWAN_CORE_FNVLIT(gamut_linear_srgb_to_oklab)(clipped);
            ALWAN_CORE_OKLAB clipped_ok;
            clipped_ok.L = clipped_oklab.v[0];
            clipped_ok.a = clipped_oklab.v[1];
            clipped_ok.b = clipped_oklab.v[2];

            ALWAN_CORE_T de = ALWAN_CORE_FNVLIT(alwan_delta_e_ok)(trial_oklab, clipped_ok);

            if (de - JND < ALWAN_CORE_EPSILON) {
                if (ALWAN_CORE_FNVLIT(gamut_css_in_gamut)(trial_rgb)) {
                    break;
                }
                hi = trial.C;
            } else {
                hi = trial.C;
            }

            if (hi - lo < ALWAN_CORE_LITERAL(1e-12)) {
                break;
            }
        }

        {
            ALWAN_CORE_OKLAB final_oklab = ALWAN_CORE_FNVLIT(alwan_oklch_to_oklab)(trial);
            ALWAN_CORE_VEC3 final_oklab_v;
            final_oklab_v.v[0] = final_oklab.L;
            final_oklab_v.v[1] = final_oklab.a;
            final_oklab_v.v[2] = final_oklab.b;
            ALWAN_CORE_VEC3 final_rgb = ALWAN_CORE_FNVLIT(gamut_oklab_to_linear_srgb)(final_oklab_v);
            return ALWAN_CORE_FNVLIT(gamut_clip)(final_rgb);
        }
    }
}
