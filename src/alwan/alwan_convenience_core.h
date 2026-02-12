/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Convenience Color Conversions
 * CMY, YCoCg, HSV, HSL, YCbCr, YcCbcCrc
 * Value-returning variants for cross-platform (C/HLSL/Halide) use.
 */

#ifndef ALWAN_CONVENIENCE_CORE_H
#define ALWAN_CONVENIENCE_CORE_H

#include "alwan_platform.h"
#include "alwan_types.h"

/* ----------------------------------------------------------------
 * RGB <-> CMY (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_cmy alwan_rgb_to_cmy_v(alwan_rgb rgb) {
    alwan_cmy result;
    result.c = ALWAN_LITERAL(1.0) - rgb.r;
    result.m = ALWAN_LITERAL(1.0) - rgb.g;
    result.y = ALWAN_LITERAL(1.0) - rgb.b;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_cmy_to_rgb_v(alwan_cmy cmy) {
    alwan_rgb result;
    result.r = ALWAN_LITERAL(1.0) - cmy.c;
    result.g = ALWAN_LITERAL(1.0) - cmy.m;
    result.b = ALWAN_LITERAL(1.0) - cmy.y;
    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ycocg alwan_rgb_to_ycocg_v(alwan_rgb rgb) {
    alwan_ycocg result;
    result.Y  = ALWAN_LITERAL(0.25) * rgb.r + ALWAN_LITERAL(0.5) * rgb.g + ALWAN_LITERAL(0.25) * rgb.b;
    result.Co = ALWAN_LITERAL(0.5) * rgb.r - ALWAN_LITERAL(0.5) * rgb.b;
    result.Cg = -ALWAN_LITERAL(0.25) * rgb.r + ALWAN_LITERAL(0.5) * rgb.g - ALWAN_LITERAL(0.25) * rgb.b;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_ycocg_to_rgb_v(alwan_ycocg ycocg) {
    alwan_rgb result;
    alwan_scalar temp = ycocg.Y - ycocg.Cg;
    result.r = temp + ycocg.Co;
    result.g = ycocg.Y + ycocg.Cg;
    result.b = temp - ycocg.Co;
    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> HSV (value-returning)
 * Hue in [0, 1] (0 = 0 degrees, 1 = 360 degrees)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_hsv alwan_rgb_to_hsv_v(alwan_rgb rgb) {
    alwan_hsv result;

    alwan_scalar max_val = alwan_max3(rgb.r, rgb.g, rgb.b);
    alwan_scalar min_val = alwan_min3(rgb.r, rgb.g, rgb.b);
    alwan_scalar delta = max_val - min_val;

    /* V */
    result.v = max_val;

    /* S */
    result.s = ALWAN_SELECT(max_val > ALWAN_LITERAL(0.0), delta / max_val, ALWAN_LITERAL(0.0));

    /* H (in degrees, then normalize to [0,1]) */
    /* Determine which channel is max via nested ALWAN_SELECT */
    alwan_scalar h_r = ALWAN_LITERAL(60.0) * (rgb.g - rgb.b) / delta;
    alwan_scalar h_r_adj = ALWAN_SELECT(rgb.g < rgb.b, h_r + ALWAN_LITERAL(360.0), h_r);
    alwan_scalar h_g = ALWAN_LITERAL(60.0) * ((rgb.b - rgb.r) / delta + ALWAN_LITERAL(2.0));
    alwan_scalar h_b = ALWAN_LITERAL(60.0) * ((rgb.r - rgb.g) / delta + ALWAN_LITERAL(4.0));

    /* Select hue based on which channel is max */
    /* max == r? use h_r. max == g? use h_g. else h_b */
    alwan_scalar h = ALWAN_SELECT(max_val == rgb.r, h_r_adj,
                      ALWAN_SELECT(max_val == rgb.g, h_g, h_b));
    h = ALWAN_SELECT(delta > ALWAN_LITERAL(0.0), h / ALWAN_LITERAL(360.0), ALWAN_LITERAL(0.0));

    result.h = h;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_hsv_to_rgb_v(alwan_hsv hsv) {
    alwan_rgb result;

    alwan_scalar h = hsv.h * ALWAN_LITERAL(360.0);
    alwan_scalar s = hsv.s;
    alwan_scalar v = hsv.v;

    /* Normalize hue to [0, 360) via modulo arithmetic */
    h = h - ALWAN_FLOOR(h / ALWAN_LITERAL(360.0)) * ALWAN_LITERAL(360.0);
    h = ALWAN_SELECT(h < ALWAN_LITERAL(0.0), h + ALWAN_LITERAL(360.0), h);

    alwan_scalar h_sector = h / ALWAN_LITERAL(60.0);
    alwan_scalar sector_f = ALWAN_FLOOR(h_sector);
    alwan_scalar f = h_sector - sector_f;

    alwan_scalar p = v * (ALWAN_LITERAL(1.0) - s);
    alwan_scalar q = v * (ALWAN_LITERAL(1.0) - s * f);
    alwan_scalar t = v * (ALWAN_LITERAL(1.0) - s * (ALWAN_LITERAL(1.0) - f));

    /* 6-sector via nested ALWAN_SELECT */
    /* sector 0: v,t,p  sector 1: q,v,p  sector 2: p,v,t
     * sector 3: p,q,v  sector 4: t,p,v  sector 5: v,p,q */
    alwan_scalar r_01 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(1.0), v, q);
    alwan_scalar g_01 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(1.0), t, v);
    alwan_scalar b_01 = p;

    alwan_scalar r_23 = p;
    alwan_scalar g_23 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(3.0), v, q);
    alwan_scalar b_23 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(3.0), t, v);

    alwan_scalar r_45 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(5.0), t, v);
    alwan_scalar g_45 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(5.0), p, p);
    alwan_scalar b_45 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(5.0), v, q);

    alwan_scalar r_0123 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(2.0), r_01, r_23);
    alwan_scalar g_0123 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(2.0), g_01, g_23);
    alwan_scalar b_0123 = ALWAN_SELECT(sector_f < ALWAN_LITERAL(2.0), b_01, b_23);

    result.r = ALWAN_SELECT(sector_f < ALWAN_LITERAL(4.0), r_0123, r_45);
    result.g = ALWAN_SELECT(sector_f < ALWAN_LITERAL(4.0), g_0123, g_45);
    result.b = ALWAN_SELECT(sector_f < ALWAN_LITERAL(4.0), b_0123, b_45);

    /* Achromatic: when s <= 0, return (v,v,v) */
    result.r = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), v, result.r);
    result.g = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), v, result.g);
    result.b = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), v, result.b);

    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> HSL (value-returning)
 * Hue in [0, 1] (0 = 0 degrees, 1 = 360 degrees)
 * ---------------------------------------------------------------- */

/* Helper: hue sector interpolation (already branchless) */
ALWAN_INLINE alwan_scalar alwan_hue_to_rgb_v(alwan_scalar p, alwan_scalar q, alwan_scalar t) {
    t = ALWAN_SELECT(t < ALWAN_LITERAL(0.0), t + ALWAN_LITERAL(1.0), t);
    t = ALWAN_SELECT(t > ALWAN_LITERAL(1.0), t - ALWAN_LITERAL(1.0), t);
    return ALWAN_SELECT(t < ALWAN_LITERAL(1.0) / ALWAN_LITERAL(6.0),
                        p + (q - p) * ALWAN_LITERAL(6.0) * t,
           ALWAN_SELECT(t < ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.0),
                        q,
           ALWAN_SELECT(t < ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0),
                        p + (q - p) * (ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0) - t) * ALWAN_LITERAL(6.0),
                        p)));
}

ALWAN_INLINE alwan_hsl alwan_rgb_to_hsl_v(alwan_rgb rgb) {
    alwan_hsl result;

    alwan_scalar max_val = alwan_max3(rgb.r, rgb.g, rgb.b);
    alwan_scalar min_val = alwan_min3(rgb.r, rgb.g, rgb.b);
    alwan_scalar delta = max_val - min_val;

    /* L */
    alwan_scalar l = (max_val + min_val) / ALWAN_LITERAL(2.0);

    /* S */
    alwan_scalar s = ALWAN_SELECT(delta > ALWAN_LITERAL(0.0),
        ALWAN_SELECT(l < ALWAN_LITERAL(0.5),
                     delta / (max_val + min_val),
                     delta / (ALWAN_LITERAL(2.0) - max_val - min_val)),
        ALWAN_LITERAL(0.0));

    /* H (same logic as HSV) */
    alwan_scalar h_r = ALWAN_LITERAL(60.0) * (rgb.g - rgb.b) / delta;
    alwan_scalar h_r_adj = ALWAN_SELECT(rgb.g < rgb.b, h_r + ALWAN_LITERAL(360.0), h_r);
    alwan_scalar h_g = ALWAN_LITERAL(60.0) * ((rgb.b - rgb.r) / delta + ALWAN_LITERAL(2.0));
    alwan_scalar h_b = ALWAN_LITERAL(60.0) * ((rgb.r - rgb.g) / delta + ALWAN_LITERAL(4.0));

    alwan_scalar h = ALWAN_SELECT(max_val == rgb.r, h_r_adj,
                      ALWAN_SELECT(max_val == rgb.g, h_g, h_b));
    h = ALWAN_SELECT(delta > ALWAN_LITERAL(0.0), h / ALWAN_LITERAL(360.0), ALWAN_LITERAL(0.0));

    result.h = h;
    result.s = s;
    result.l = l;
    return result;
}

ALWAN_INLINE alwan_rgb alwan_hsl_to_rgb_v(alwan_hsl hsl) {
    alwan_rgb result;

    alwan_scalar h = hsl.h;
    alwan_scalar s = hsl.s;
    alwan_scalar l = hsl.l;

    alwan_scalar q = ALWAN_SELECT(l < ALWAN_LITERAL(0.5),
                              l * (ALWAN_LITERAL(1.0) + s),
                              l + s - l * s);
    alwan_scalar p = ALWAN_LITERAL(2.0) * l - q;

    alwan_scalar r = alwan_hue_to_rgb_v(p, q, h + ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    alwan_scalar g = alwan_hue_to_rgb_v(p, q, h);
    alwan_scalar b = alwan_hue_to_rgb_v(p, q, h - ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    /* Achromatic: when s <= 0, return (l,l,l) */
    result.r = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), l, r);
    result.g = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), l, g);
    result.b = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), l, b);

    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr (value-returning, takes kr/kb directly)
 * The .c wrapper resolves the enum to kr/kb before calling.
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_ycbcr alwan_rgb_to_ycbcr_kr_kb_v(alwan_rgb rgb, alwan_scalar kr, alwan_scalar kb) {
    alwan_ycbcr result;

    alwan_scalar y = kr * rgb.r + (ALWAN_LITERAL(1.0) - kr - kb) * rgb.g + kb * rgb.b;
    result.Y  = y;
    result.Cb = (rgb.b - y) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kb)) + ALWAN_LITERAL(0.5);
    result.Cr = (rgb.r - y) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kr)) + ALWAN_LITERAL(0.5);

    return result;
}

ALWAN_INLINE alwan_rgb alwan_ycbcr_to_rgb_kr_kb_v(alwan_ycbcr ycbcr, alwan_scalar kr, alwan_scalar kb) {
    alwan_rgb result;

    alwan_scalar y  = ycbcr.Y;
    alwan_scalar cb = ycbcr.Cb - ALWAN_LITERAL(0.5);
    alwan_scalar cr = ycbcr.Cr - ALWAN_LITERAL(0.5);
    alwan_scalar kg = ALWAN_LITERAL(1.0) - kr - kb;

    alwan_scalar r = y + cr * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kr);
    alwan_scalar b = y + cb * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kb);
    alwan_scalar g = (y - kr * r - kb * b) / kg;

    result.r = alwan_clamp(r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    result.g = alwan_clamp(g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    result.b = alwan_clamp(b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc (BT.2020 constant luminance, value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_yccbccrc alwan_rgb_to_yccbccrc_v(alwan_rgb rgb) {
    alwan_yccbccrc result;

    alwan_scalar const kr = ALWAN_LITERAL(0.2627);
    alwan_scalar const kg = ALWAN_LITERAL(0.6780);
    alwan_scalar const kb = ALWAN_LITERAL(0.0593);

    /* Linear Yc */
    alwan_scalar yc_linear = kr * rgb.r + kg * rgb.g + kb * rgb.b;

    /* BT.2020 OETF */
    alwan_scalar const beta = ALWAN_LITERAL(0.018);
    alwan_scalar const alpha = ALWAN_LITERAL(1.099);

    alwan_scalar yc = ALWAN_SELECT(yc_linear < beta,
                                   ALWAN_LITERAL(4.5) * yc_linear,
                                   alpha * ALWAN_POW(yc_linear, ALWAN_LITERAL(0.45)) - (alpha - ALWAN_LITERAL(1.0)));
    alwan_scalar r_gamma = ALWAN_SELECT(rgb.r < beta,
                                        ALWAN_LITERAL(4.5) * rgb.r,
                                        alpha * ALWAN_POW(rgb.r, ALWAN_LITERAL(0.45)) - (alpha - ALWAN_LITERAL(1.0)));
    alwan_scalar b_gamma = ALWAN_SELECT(rgb.b < beta,
                                        ALWAN_LITERAL(4.5) * rgb.b,
                                        alpha * ALWAN_POW(rgb.b, ALWAN_LITERAL(0.45)) - (alpha - ALWAN_LITERAL(1.0)));

    /* Chroma differences */
    alwan_scalar diff_b = b_gamma - yc;
    alwan_scalar diff_r = r_gamma - yc;

    alwan_scalar cbc = ALWAN_SELECT(diff_b <= ALWAN_LITERAL(0.0),
                                    diff_b / ALWAN_LITERAL(1.9404),
                                    diff_b / ALWAN_LITERAL(1.5816));
    alwan_scalar crc = ALWAN_SELECT(diff_r <= ALWAN_LITERAL(0.0),
                                    diff_r / ALWAN_LITERAL(1.7184),
                                    diff_r / ALWAN_LITERAL(0.9936));

    /* Legal range scaling (10-bit) */
    alwan_scalar const y_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const y_max = ALWAN_LITERAL(940.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_max = ALWAN_LITERAL(960.0) / ALWAN_LITERAL(1023.0);

    result.Yc  = yc * (y_max - y_min) + y_min;
    result.Cbc = cbc * (c_max - c_min) + (c_max + c_min) / ALWAN_LITERAL(2.0);
    result.Crc = crc * (c_max - c_min) + (c_max + c_min) / ALWAN_LITERAL(2.0);

    return result;
}

ALWAN_INLINE alwan_rgb alwan_yccbccrc_to_rgb_v(alwan_yccbccrc yccbccrc) {
    alwan_rgb result;

    alwan_scalar const kr = ALWAN_LITERAL(0.2627);
    alwan_scalar const kg = ALWAN_LITERAL(0.6780);
    alwan_scalar const kb = ALWAN_LITERAL(0.0593);

    /* Reverse legal range scaling */
    alwan_scalar const y_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const y_max = ALWAN_LITERAL(940.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_max = ALWAN_LITERAL(960.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_center = (c_max + c_min) / ALWAN_LITERAL(2.0);

    alwan_scalar yc = (yccbccrc.Yc - y_min) / (y_max - y_min);
    alwan_scalar cbc = (yccbccrc.Cbc - c_center) / (c_max - c_min);
    alwan_scalar crc = (yccbccrc.Crc - c_center) / (c_max - c_min);

    /* Reverse chroma divisors */
    alwan_scalar diff_b = ALWAN_SELECT(cbc <= ALWAN_LITERAL(0.0),
                                       cbc * ALWAN_LITERAL(1.9404),
                                       cbc * ALWAN_LITERAL(1.5816));
    alwan_scalar diff_r = ALWAN_SELECT(crc <= ALWAN_LITERAL(0.0),
                                       crc * ALWAN_LITERAL(1.7184),
                                       crc * ALWAN_LITERAL(0.9936));

    alwan_scalar r_gamma = yc + diff_r;
    alwan_scalar b_gamma = yc + diff_b;

    /* BT.2020 EOTF */
    alwan_scalar const beta = ALWAN_LITERAL(0.018);
    alwan_scalar const alpha = ALWAN_LITERAL(1.099);
    alwan_scalar const threshold = ALWAN_LITERAL(4.5) * beta;

    alwan_scalar yc_linear = ALWAN_SELECT(yc < threshold,
                                          yc / ALWAN_LITERAL(4.5),
                                          ALWAN_POW((yc + (alpha - ALWAN_LITERAL(1.0))) / alpha,
                                                    ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.45)));
    alwan_scalar r = ALWAN_SELECT(r_gamma < threshold,
                                  r_gamma / ALWAN_LITERAL(4.5),
                                  ALWAN_POW((r_gamma + (alpha - ALWAN_LITERAL(1.0))) / alpha,
                                            ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.45)));
    alwan_scalar b = ALWAN_SELECT(b_gamma < threshold,
                                  b_gamma / ALWAN_LITERAL(4.5),
                                  ALWAN_POW((b_gamma + (alpha - ALWAN_LITERAL(1.0))) / alpha,
                                            ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.45)));

    alwan_scalar g = (yc_linear - kr * r - kb * b) / kg;

    result.r = alwan_clamp(r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    result.g = alwan_clamp(g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    result.b = alwan_clamp(b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    return result;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_cmyk alwan_cmy_to_cmyk_v(alwan_cmy cmy) {
    alwan_cmyk result;
    alwan_scalar k = alwan_min3(cmy.c, cmy.m, cmy.y);
    alwan_scalar denom = ALWAN_LITERAL(1.0) - k;
    /* When k >= 1, CMY are all 1 → pure black, c=m=y=0 */
    result.c = ALWAN_SELECT(k >= ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), (cmy.c - k) / denom);
    result.m = ALWAN_SELECT(k >= ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), (cmy.m - k) / denom);
    result.y = ALWAN_SELECT(k >= ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), (cmy.y - k) / denom);
    result.k = k;
    return result;
}

ALWAN_INLINE alwan_cmy alwan_cmyk_to_cmy_v(alwan_cmyk cmyk) {
    alwan_cmy result;
    alwan_scalar one_minus_k = ALWAN_LITERAL(1.0) - cmyk.k;
    result.c = cmyk.c * one_minus_k + cmyk.k;
    result.m = cmyk.m * one_minus_k + cmyk.k;
    result.y = cmyk.y * one_minus_k + cmyk.k;
    return result;
}

#endif /* ALWAN_CONVENIENCE_CORE_H */
