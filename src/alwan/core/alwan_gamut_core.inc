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
    /* Ottosson (2021) "sRGB gamut clipping", compute_max_saturation, MIT, Copyright (c) 2021
     * Bjorn Ottosson: a polynomial first guess, then one Halley step on the gamut face. */
    ALWAN_CORE_T k0, k1, k2, k3, k4, wl, wm, ws;
    if (-ALWAN_CORE_LITERAL(1.88170328) * a - ALWAN_CORE_LITERAL(0.80936493) * b > ALWAN_CORE_LITERAL(1.0)) {
        k0 = +ALWAN_CORE_LITERAL(1.19086277); k1 = +ALWAN_CORE_LITERAL(1.76576728); k2 = +ALWAN_CORE_LITERAL(0.59662641);
        k3 = +ALWAN_CORE_LITERAL(0.75515197); k4 = +ALWAN_CORE_LITERAL(0.56771245);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[0]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[1]; ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[2];
    } else if (ALWAN_CORE_LITERAL(1.81444104) * a - ALWAN_CORE_LITERAL(1.19445276) * b > ALWAN_CORE_LITERAL(1.0)) {
        k0 = +ALWAN_CORE_LITERAL(0.73956515); k1 = -ALWAN_CORE_LITERAL(0.45954404); k2 = +ALWAN_CORE_LITERAL(0.08285427);
        k3 = +ALWAN_CORE_LITERAL(0.12541070); k4 = +ALWAN_CORE_LITERAL(0.14503204);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[3]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[4]; ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[5];
    } else {
        k0 = +ALWAN_CORE_LITERAL(1.35733652); k1 = -ALWAN_CORE_LITERAL(0.00915799); k2 = -ALWAN_CORE_LITERAL(1.15130210);
        k3 = -ALWAN_CORE_LITERAL(0.50559606); k4 = +ALWAN_CORE_LITERAL(0.00692167);
        wl = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[6]; wm = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[7]; ws = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[8];
    }
    ALWAN_CORE_T S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;
    {
        ALWAN_CORE_T k_l = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[1] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[2] * b;
        ALWAN_CORE_T k_m = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[4] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[5] * b;
        ALWAN_CORE_T k_s = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[7] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[8] * b;
        ALWAN_CORE_T l_ = ALWAN_CORE_ONE + S * k_l;
        ALWAN_CORE_T m_ = ALWAN_CORE_ONE + S * k_m;
        ALWAN_CORE_T s_ = ALWAN_CORE_ONE + S * k_s;
        ALWAN_CORE_T l = l_ * l_ * l_;
        ALWAN_CORE_T m = m_ * m_ * m_;
        ALWAN_CORE_T s = s_ * s_ * s_;
        ALWAN_CORE_T l_dS = ALWAN_CORE_LITERAL(3.0) * k_l * l_ * l_;
        ALWAN_CORE_T m_dS = ALWAN_CORE_LITERAL(3.0) * k_m * m_ * m_;
        ALWAN_CORE_T s_dS = ALWAN_CORE_LITERAL(3.0) * k_s * s_ * s_;
        ALWAN_CORE_T l_dS2 = ALWAN_CORE_LITERAL(6.0) * k_l * k_l * l_;
        ALWAN_CORE_T m_dS2 = ALWAN_CORE_LITERAL(6.0) * k_m * k_m * m_;
        ALWAN_CORE_T s_dS2 = ALWAN_CORE_LITERAL(6.0) * k_s * k_s * s_;
        ALWAN_CORE_T f = wl * l + wm * m + ws * s;
        ALWAN_CORE_T f1 = wl * l_dS + wm * m_dS + ws * s_dS;
        ALWAN_CORE_T f2 = wl * l_dS2 + wm * m_dS2 + ws * s_dS2;
        S = S - f * f1 / (f1 * f1 - ALWAN_CORE_LITERAL(0.5) * f * f2);
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
                                                     ALWAN_CORE_T L0) {
    /* Ottosson (2021) "sRGB gamut clipping", find_gamut_intersection, MIT, Copyright (c) 2021
     * Bjorn Ottosson: where the line from (L0, 0) to (L1, C1) in the hue plane (a, b) leaves
     * the gamut, as the fraction t of the way to (L1, C1). Below the cusp's line the lower edge
     * is exact; above it the triangle approximation is refined by one Halley step on each RGB
     * face. */
    ALWAN_CORE_VEC2 cusp = ALWAN_CORE_FNVLIT(gamut_find_cusp)(a, b);
    ALWAN_CORE_T L_cusp = cusp.v[0];
    ALWAN_CORE_T C_cusp = cusp.v[1];
    ALWAN_CORE_T t;
    if (((L1 - L0) * C_cusp - (L_cusp - L0) * C1) <= ALWAN_CORE_ZERO) {
        t = C_cusp * L0 / (C1 * L_cusp + C_cusp * (L0 - L1));
    } else {
        ALWAN_CORE_T dL = L1 - L0;
        ALWAN_CORE_T dC = C1;
        ALWAN_CORE_T k_l = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[1] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[2] * b;
        ALWAN_CORE_T k_m = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[4] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[5] * b;
        ALWAN_CORE_T k_s = ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[7] * a + ALWAN_CORE_FNLIT(CSS_LAB_TO_LMS).m[8] * b;
        ALWAN_CORE_T l_dt = dL + dC * k_l;
        ALWAN_CORE_T m_dt = dL + dC * k_m;
        ALWAN_CORE_T s_dt = dL + dC * k_s;
        ALWAN_CORE_T Lt, Ct, l_, m_, s_, l, m, s, ldt, mdt, sdt, ldt2, mdt2, sdt2;
        ALWAN_CORE_T t_r, t_g, t_b, t_min;
        t = C_cusp * (L0 - ALWAN_CORE_ONE) / (C1 * (L_cusp - ALWAN_CORE_ONE) + C_cusp * (L0 - L1));
        Lt = L0 * (ALWAN_CORE_ONE - t) + t * L1;
        Ct = t * C1;
        l_ = Lt + Ct * k_l;
        m_ = Lt + Ct * k_m;
        s_ = Lt + Ct * k_s;
        l = l_ * l_ * l_;
        m = m_ * m_ * m_;
        s = s_ * s_ * s_;
        ldt = ALWAN_CORE_LITERAL(3.0) * l_dt * l_ * l_;
        mdt = ALWAN_CORE_LITERAL(3.0) * m_dt * m_ * m_;
        sdt = ALWAN_CORE_LITERAL(3.0) * s_dt * s_ * s_;
        ldt2 = ALWAN_CORE_LITERAL(6.0) * l_dt * l_dt * l_;
        mdt2 = ALWAN_CORE_LITERAL(6.0) * m_dt * m_dt * m_;
        sdt2 = ALWAN_CORE_LITERAL(6.0) * s_dt * s_dt * s_;
        {
            ALWAN_CORE_T f = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[0] * l + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[1] * m + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[2] * s - ALWAN_CORE_ONE;
            ALWAN_CORE_T f1 = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[0] * ldt + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[1] * mdt + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[2] * sdt;
            ALWAN_CORE_T f2 = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[0] * ldt2 + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[1] * mdt2 + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[2] * sdt2;
            ALWAN_CORE_T u = f1 / (f1 * f1 - ALWAN_CORE_LITERAL(0.5) * f * f2);
            t_r = ALWAN_CORE_SELECT(u >= ALWAN_CORE_ZERO, -f * u, ALWAN_CORE_LITERAL(1e30));
        }
        {
            ALWAN_CORE_T f = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[3] * l + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[4] * m + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[5] * s - ALWAN_CORE_ONE;
            ALWAN_CORE_T f1 = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[3] * ldt + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[4] * mdt + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[5] * sdt;
            ALWAN_CORE_T f2 = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[3] * ldt2 + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[4] * mdt2 + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[5] * sdt2;
            ALWAN_CORE_T u = f1 / (f1 * f1 - ALWAN_CORE_LITERAL(0.5) * f * f2);
            t_g = ALWAN_CORE_SELECT(u >= ALWAN_CORE_ZERO, -f * u, ALWAN_CORE_LITERAL(1e30));
        }
        {
            ALWAN_CORE_T f = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[6] * l + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[7] * m + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[8] * s - ALWAN_CORE_ONE;
            ALWAN_CORE_T f1 = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[6] * ldt + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[7] * mdt + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[8] * sdt;
            ALWAN_CORE_T f2 = ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[6] * ldt2 + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[7] * mdt2 + ALWAN_CORE_FNLIT(CSS_LMS_TO_SRGB).m[8] * sdt2;
            ALWAN_CORE_T u = f1 / (f1 * f1 - ALWAN_CORE_LITERAL(0.5) * f * f2);
            t_b = ALWAN_CORE_SELECT(u >= ALWAN_CORE_ZERO, -f * u, ALWAN_CORE_LITERAL(1e30));
        }
        t_min = ALWAN_CORE_SELECT(t_r < t_g, t_r, t_g);
        t_min = ALWAN_CORE_SELECT(t_b < t_min, t_b, t_min);
        t += t_min;
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
    /* CSS Color 4, section 13.2: binary search on Oklch chroma at constant lightness
     * and hue. A candidate inside the cube raises the lower bound while no clipped
     * candidate has been accepted yet; a candidate whose clip is within the JND
     * raises it too, and ends the search once it is within epsilon of the JND. */
    const ALWAN_CORE_T JND = ALWAN_CORE_LITERAL(0.02);
    const ALWAN_CORE_T EPS = ALWAN_CORE_LITERAL(0.0001);
    const int MAX_ITER = 64;

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
        ALWAN_CORE_VEC3 clipped = ALWAN_CORE_FNVLIT(gamut_clip)(origin);
        ALWAN_CORE_VEC3 clipped_oklab = ALWAN_CORE_FNVLIT(gamut_linear_srgb_to_oklab)(clipped);
        ALWAN_CORE_OKLAB clipped_ok;
        clipped_ok.L = clipped_oklab.v[0];
        clipped_ok.a = clipped_oklab.v[1];
        clipped_ok.b = clipped_oklab.v[2];
        ALWAN_CORE_T de = ALWAN_CORE_FNVLIT(alwan_delta_e_ok)(ok, clipped_ok);
        ALWAN_CORE_T lo = ALWAN_CORE_LITERAL(0.0);
        ALWAN_CORE_T hi = lch.C;
        ALWAN_CORE_OKLCH trial = lch;
        int lo_in_gamut = 1;
        int done = de < JND;
        int i;

        for (i = 0; i < MAX_ITER && !done && hi - lo > EPS; i++) {
            trial.C = (lo + hi) * ALWAN_CORE_LITERAL(0.5);

            ALWAN_CORE_OKLAB trial_oklab = ALWAN_CORE_FNVLIT(alwan_oklch_to_oklab)(trial);
            ALWAN_CORE_VEC3 trial_oklab_v;
            trial_oklab_v.v[0] = trial_oklab.L;
            trial_oklab_v.v[1] = trial_oklab.a;
            trial_oklab_v.v[2] = trial_oklab.b;
            ALWAN_CORE_VEC3 trial_rgb = ALWAN_CORE_FNVLIT(gamut_oklab_to_linear_srgb)(trial_oklab_v);

            if (lo_in_gamut && ALWAN_CORE_FNVLIT(gamut_css_in_gamut)(trial_rgb)) {
                lo = trial.C;
            } else {
                clipped = ALWAN_CORE_FNVLIT(gamut_clip)(trial_rgb);
                clipped_oklab = ALWAN_CORE_FNVLIT(gamut_linear_srgb_to_oklab)(clipped);
                clipped_ok.L = clipped_oklab.v[0];
                clipped_ok.a = clipped_oklab.v[1];
                clipped_ok.b = clipped_oklab.v[2];
                de = ALWAN_CORE_FNVLIT(alwan_delta_e_ok)(trial_oklab, clipped_ok);
                if (de < JND) {
                    done = JND - de < EPS;
                    lo_in_gamut = 0;
                    lo = trial.C;
                } else {
                    hi = trial.C;
                }
            }
        }
        return clipped;
    }
}
