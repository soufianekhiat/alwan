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

/* ================================================================
 * RGB clamp to [0,1]
 * ================================================================ */

ALWAN_INLINE alwan_vec3 gamut_clip_v(alwan_vec3 rgb) {
    alwan_vec3 r;
    r.v[0] = alwan_clamp(rgb.v[0], ALWAN_ZERO, ALWAN_ONE);
    r.v[1] = alwan_clamp(rgb.v[1], ALWAN_ZERO, ALWAN_ONE);
    r.v[2] = alwan_clamp(rgb.v[2], ALWAN_ZERO, ALWAN_ONE);
    return r;
}

/* ================================================================
 * Linear sRGB <-> Oklab
 * Ottosson (2020)
 * ================================================================ */

ALWAN_INLINE alwan_vec3 gamut_linear_srgb_to_oklab_v(alwan_vec3 rgb) {
    alwan_vec3 oklab;

    alwan_scalar l = ALWAN_LITERAL(0.4122214708) * rgb.v[0] +
                     ALWAN_LITERAL(0.5363325363) * rgb.v[1] +
                     ALWAN_LITERAL(0.0514459929) * rgb.v[2];
    alwan_scalar m = ALWAN_LITERAL(0.2119034982) * rgb.v[0] +
                     ALWAN_LITERAL(0.6806995451) * rgb.v[1] +
                     ALWAN_LITERAL(0.1073969566) * rgb.v[2];
    alwan_scalar s = ALWAN_LITERAL(0.0883024619) * rgb.v[0] +
                     ALWAN_LITERAL(0.2817188376) * rgb.v[1] +
                     ALWAN_LITERAL(0.6299787005) * rgb.v[2];

    alwan_scalar l_ = ALWAN_CBRT(l);
    alwan_scalar m_ = ALWAN_CBRT(m);
    alwan_scalar s_ = ALWAN_CBRT(s);

    oklab.v[0] = ALWAN_LITERAL(0.2104542553) * l_ +
                 ALWAN_LITERAL(0.7936177850) * m_ -
                 ALWAN_LITERAL(0.0040720468) * s_;
    oklab.v[1] = ALWAN_LITERAL(1.9779984951) * l_ -
                 ALWAN_LITERAL(2.4285922050) * m_ +
                 ALWAN_LITERAL(0.4505937099) * s_;
    oklab.v[2] = ALWAN_LITERAL(0.0259040371) * l_ +
                 ALWAN_LITERAL(0.7827717662) * m_ -
                 ALWAN_LITERAL(0.8086757660) * s_;

    return oklab;
}

ALWAN_INLINE alwan_vec3 gamut_oklab_to_linear_srgb_v(alwan_vec3 oklab) {
    alwan_vec3 rgb;

    alwan_scalar l_ = oklab.v[0] + ALWAN_LITERAL(0.3963377774) * oklab.v[1] +
                      ALWAN_LITERAL(0.2158037573) * oklab.v[2];
    alwan_scalar m_ = oklab.v[0] - ALWAN_LITERAL(0.1055613458) * oklab.v[1] -
                      ALWAN_LITERAL(0.0638541728) * oklab.v[2];
    alwan_scalar s_ = oklab.v[0] - ALWAN_LITERAL(0.0894841775) * oklab.v[1] -
                      ALWAN_LITERAL(1.2914855480) * oklab.v[2];

    alwan_scalar l = l_ * l_ * l_;
    alwan_scalar m = m_ * m_ * m_;
    alwan_scalar s = s_ * s_ * s_;

    rgb.v[0] = +ALWAN_LITERAL(4.0767416621) * l -
                ALWAN_LITERAL(3.3077115913) * m +
                ALWAN_LITERAL(0.2309699292) * s;
    rgb.v[1] = -ALWAN_LITERAL(1.2684380046) * l +
                ALWAN_LITERAL(2.6097574011) * m -
                ALWAN_LITERAL(0.3413193965) * s;
    rgb.v[2] = -ALWAN_LITERAL(0.0041960863) * l -
                ALWAN_LITERAL(0.7034186147) * m +
                ALWAN_LITERAL(1.7076147010) * s;

    return rgb;
}

/* ================================================================
 * Maximum saturation for a given hue direction (a, b) in Oklab
 * Ottosson (2021): polynomial + one Newton refinement
 * ================================================================ */

ALWAN_INLINE alwan_scalar gamut_compute_max_saturation_v(alwan_scalar a, alwan_scalar b) {
    alwan_scalar k0, k1, k2, k3, k4, wl, wm, ws;

    if (-ALWAN_LITERAL(1.88170328) * a - ALWAN_LITERAL(0.80936493) * b > ALWAN_LITERAL(1.0)) {
        /* Red zone */
        k0 = +ALWAN_LITERAL(1.19086277); k1 = +ALWAN_LITERAL(1.76576728);
        k2 = +ALWAN_LITERAL(0.59662641); k3 = +ALWAN_LITERAL(0.75515197);
        k4 = +ALWAN_LITERAL(0.56771245);
        wl = +ALWAN_LITERAL(4.0767416621); wm = -ALWAN_LITERAL(3.3077115913);
        ws = +ALWAN_LITERAL(0.2309699292);
    } else if (ALWAN_LITERAL(1.81444104) * a - ALWAN_LITERAL(1.19445276) * b > ALWAN_LITERAL(1.0)) {
        /* Green zone */
        k0 = +ALWAN_LITERAL(0.73956515); k1 = -ALWAN_LITERAL(0.45954404);
        k2 = +ALWAN_LITERAL(0.08285427); k3 = +ALWAN_LITERAL(0.12541070);
        k4 = +ALWAN_LITERAL(0.14503204);
        wl = -ALWAN_LITERAL(1.2684380046); wm = +ALWAN_LITERAL(2.6097574011);
        ws = -ALWAN_LITERAL(0.3413193965);
    } else {
        /* Blue zone */
        k0 = +ALWAN_LITERAL(1.35733652); k1 = -ALWAN_LITERAL(0.00915799);
        k2 = -ALWAN_LITERAL(1.15130210); k3 = -ALWAN_LITERAL(0.50559606);
        k4 = +ALWAN_LITERAL(0.00692167);
        wl = -ALWAN_LITERAL(0.0041960863); wm = -ALWAN_LITERAL(0.7034186147);
        ws = +ALWAN_LITERAL(1.7076147010);
    }

    alwan_scalar S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;

    /* One Newton refinement iteration */
    {
        alwan_scalar k_l = +ALWAN_LITERAL(0.3963377774) * a + ALWAN_LITERAL(0.2158037573) * b;
        alwan_scalar k_m = -ALWAN_LITERAL(0.1055613458) * a - ALWAN_LITERAL(0.0638541728) * b;
        alwan_scalar k_s = -ALWAN_LITERAL(0.0894841775) * a - ALWAN_LITERAL(1.2914855480) * b;

        alwan_scalar l_ = ALWAN_LITERAL(1.0) + S * k_l;
        alwan_scalar m_ = ALWAN_LITERAL(1.0) + S * k_m;
        alwan_scalar s_ = ALWAN_LITERAL(1.0) + S * k_s;

        alwan_scalar l = l_ * l_ * l_;
        alwan_scalar m = m_ * m_ * m_;
        alwan_scalar s = s_ * s_ * s_;

        alwan_scalar l_dS = ALWAN_LITERAL(3.0) * k_l * l_ * l_;
        alwan_scalar m_dS = ALWAN_LITERAL(3.0) * k_m * m_ * m_;
        alwan_scalar s_dS = ALWAN_LITERAL(3.0) * k_s * s_ * s_;

        alwan_scalar f   = wl * l   + wm * m   + ws * s;
        alwan_scalar f_dS = wl * l_dS + wm * m_dS + ws * s_dS;

        S = S - f / f_dS;
    }

    return S;
}

/* ================================================================
 * Find gamut cusp (L, C) for a given hue direction (a, b)
 * Returns vec2: v[0] = L_cusp, v[1] = C_cusp
 * ================================================================ */

ALWAN_INLINE alwan_vec2 gamut_find_cusp_v(alwan_scalar a, alwan_scalar b) {
    alwan_vec2 result;
    alwan_scalar S_cusp = gamut_compute_max_saturation_v(a, b);

    alwan_vec3 oklab_cusp;
    oklab_cusp.v[0] = ALWAN_ONE;
    oklab_cusp.v[1] = S_cusp * a;
    oklab_cusp.v[2] = S_cusp * b;

    alwan_vec3 rgb_cusp = gamut_oklab_to_linear_srgb_v(oklab_cusp);

    alwan_scalar max_rgb = rgb_cusp.v[0];
    max_rgb = ALWAN_SELECT(rgb_cusp.v[1] > max_rgb, rgb_cusp.v[1], max_rgb);
    max_rgb = ALWAN_SELECT(rgb_cusp.v[2] > max_rgb, rgb_cusp.v[2], max_rgb);

    result.v[0] = ALWAN_CBRT(ALWAN_ONE / max_rgb);        /* L_cusp */
    result.v[1] = result.v[0] * S_cusp;                    /* C_cusp */
    return result;
}

/* ================================================================
 * Find gamut boundary intersection
 * Line from (L0,C0) toward (L1,C1), returns parameter t
 * ================================================================ */

ALWAN_INLINE alwan_scalar gamut_find_intersection_v(alwan_scalar a, alwan_scalar b,
                                                     alwan_scalar L1, alwan_scalar C1,
                                                     alwan_scalar L0, alwan_scalar C0) {
    alwan_vec2 cusp = gamut_find_cusp_v(a, b);
    alwan_scalar L_cusp = cusp.v[0];
    alwan_scalar C_cusp = cusp.v[1];

    alwan_scalar t;
    if (((L1 - L0) * C_cusp - (C1 - C0) * L_cusp) <= ALWAN_ZERO) {
        /* Lower part */
        t = C_cusp * L0 / (C1 * L_cusp + C_cusp * (L0 - L1));
    } else {
        /* Upper part */
        t = C_cusp * (L0 - ALWAN_ONE) / (C1 * (L_cusp - ALWAN_ONE) +
                                           C_cusp * (L0 - L1));

        /* One Newton refinement */
        {
            alwan_scalar dL = L1 - L0;
            alwan_scalar dC = C1 - C0;

            alwan_scalar L = L0 * (ALWAN_ONE - t) + t * L1;
            alwan_scalar C = t * C1;

            alwan_scalar k_l = +ALWAN_LITERAL(0.3963377774) * a + ALWAN_LITERAL(0.2158037573) * b;
            alwan_scalar k_m = -ALWAN_LITERAL(0.1055613458) * a - ALWAN_LITERAL(0.0638541728) * b;
            alwan_scalar k_s = -ALWAN_LITERAL(0.0894841775) * a - ALWAN_LITERAL(1.2914855480) * b;

            alwan_scalar l_ = L + C * k_l;
            alwan_scalar m_ = L + C * k_m;
            alwan_scalar s_ = L + C * k_s;

            alwan_scalar l = l_ * l_ * l_;
            alwan_scalar m = m_ * m_ * m_;
            alwan_scalar s = s_ * s_ * s_;

            alwan_scalar ldt  = ALWAN_LITERAL(3.0) * dL * l_ * l_;
            alwan_scalar mdt  = ALWAN_LITERAL(3.0) * dL * m_ * m_;
            alwan_scalar sdt  = ALWAN_LITERAL(3.0) * dL * s_ * s_;

            alwan_scalar ldt2 = ALWAN_LITERAL(3.0) * dC * l_ * l_;
            alwan_scalar mdt2 = ALWAN_LITERAL(3.0) * dC * m_ * m_;
            alwan_scalar sdt2 = ALWAN_LITERAL(3.0) * dC * s_ * s_;

            alwan_scalar r  = ALWAN_LITERAL(4.0767416621) * l  - ALWAN_LITERAL(3.3077115913) * m  +
                              ALWAN_LITERAL(0.2309699292) * s  - ALWAN_ONE;
            alwan_scalar r1 = ALWAN_LITERAL(4.0767416621) * ldt - ALWAN_LITERAL(3.3077115913) * mdt +
                              ALWAN_LITERAL(0.2309699292) * sdt;
            alwan_scalar r2 = ALWAN_LITERAL(4.0767416621) * ldt2 - ALWAN_LITERAL(3.3077115913) * mdt2 +
                              ALWAN_LITERAL(0.2309699292) * sdt2;

            alwan_scalar u_r = r1 / (r1 - r2);
            alwan_scalar t_r = -r / r1;

            alwan_scalar g  = -ALWAN_LITERAL(1.2684380046) * l  + ALWAN_LITERAL(2.6097574011) * m  -
                               ALWAN_LITERAL(0.3413193965) * s  - ALWAN_ONE;
            alwan_scalar g1 = -ALWAN_LITERAL(1.2684380046) * ldt + ALWAN_LITERAL(2.6097574011) * mdt -
                               ALWAN_LITERAL(0.3413193965) * sdt;
            alwan_scalar g2 = -ALWAN_LITERAL(1.2684380046) * ldt2 + ALWAN_LITERAL(2.6097574011) * mdt2 -
                               ALWAN_LITERAL(0.3413193965) * sdt2;

            alwan_scalar u_g = g1 / (g1 - g2);
            alwan_scalar t_g = -g / g1;

            alwan_scalar b_val = -ALWAN_LITERAL(0.0041960863) * l  - ALWAN_LITERAL(0.7034186147) * m  +
                                  ALWAN_LITERAL(1.7076147010) * s  - ALWAN_ONE;
            alwan_scalar b1    = -ALWAN_LITERAL(0.0041960863) * ldt - ALWAN_LITERAL(0.7034186147) * mdt +
                                  ALWAN_LITERAL(1.7076147010) * sdt;
            alwan_scalar b2    = -ALWAN_LITERAL(0.0041960863) * ldt2 - ALWAN_LITERAL(0.7034186147) * mdt2 +
                                  ALWAN_LITERAL(1.7076147010) * sdt2;

            alwan_scalar u_b = b1 / (b1 - b2);
            alwan_scalar t_b = -b_val / b1;

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

#endif /* ALWAN_GAMUT_CORE_H */
