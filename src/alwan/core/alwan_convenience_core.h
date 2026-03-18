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

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_convenience_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_convenience_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV/HSL (emitted here, not in .inc, to avoid
 * macro name collisions with alwan_srgb_oetf/eotf aliases)
 * ---------------------------------------------------------------- */
#ifdef ALWAN_CORE_H

ALWAN_INLINE alwan_hsv_f32 alwan_linear_srgb_to_hsv_f32_v(alwan_rgb_f32 linear) {
    alwan_rgb_f32 encoded;
    encoded.r = alwan_srgb_oetf_f32(linear.r);
    encoded.g = alwan_srgb_oetf_f32(linear.g);
    encoded.b = alwan_srgb_oetf_f32(linear.b);
    return alwan_rgb_to_hsv_f32_v(encoded);
}

ALWAN_INLINE alwan_rgb_f32 alwan_hsv_to_linear_srgb_f32_v(alwan_hsv_f32 hsv) {
    alwan_rgb_f32 encoded = alwan_hsv_to_rgb_f32_v(hsv);
    alwan_rgb_f32 linear;
    linear.r = alwan_srgb_eotf_f32(encoded.r);
    linear.g = alwan_srgb_eotf_f32(encoded.g);
    linear.b = alwan_srgb_eotf_f32(encoded.b);
    return linear;
}

ALWAN_INLINE alwan_hsl_f32 alwan_linear_srgb_to_hsl_f32_v(alwan_rgb_f32 linear) {
    alwan_rgb_f32 encoded;
    encoded.r = alwan_srgb_oetf_f32(linear.r);
    encoded.g = alwan_srgb_oetf_f32(linear.g);
    encoded.b = alwan_srgb_oetf_f32(linear.b);
    return alwan_rgb_to_hsl_f32_v(encoded);
}

ALWAN_INLINE alwan_rgb_f32 alwan_hsl_to_linear_srgb_f32_v(alwan_hsl_f32 hsl) {
    alwan_rgb_f32 encoded = alwan_hsl_to_rgb_f32_v(hsl);
    alwan_rgb_f32 linear;
    linear.r = alwan_srgb_eotf_f32(encoded.r);
    linear.g = alwan_srgb_eotf_f32(encoded.g);
    linear.b = alwan_srgb_eotf_f32(encoded.b);
    return linear;
}

ALWAN_INLINE alwan_hsv_f64 alwan_linear_srgb_to_hsv_f64_v(alwan_rgb_f64 linear) {
    alwan_rgb_f64 encoded;
    encoded.r = alwan_srgb_oetf_f64(linear.r);
    encoded.g = alwan_srgb_oetf_f64(linear.g);
    encoded.b = alwan_srgb_oetf_f64(linear.b);
    return alwan_rgb_to_hsv_f64_v(encoded);
}

ALWAN_INLINE alwan_rgb_f64 alwan_hsv_to_linear_srgb_f64_v(alwan_hsv_f64 hsv) {
    alwan_rgb_f64 encoded = alwan_hsv_to_rgb_f64_v(hsv);
    alwan_rgb_f64 linear;
    linear.r = alwan_srgb_eotf_f64(encoded.r);
    linear.g = alwan_srgb_eotf_f64(encoded.g);
    linear.b = alwan_srgb_eotf_f64(encoded.b);
    return linear;
}

ALWAN_INLINE alwan_hsl_f64 alwan_linear_srgb_to_hsl_f64_v(alwan_rgb_f64 linear) {
    alwan_rgb_f64 encoded;
    encoded.r = alwan_srgb_oetf_f64(linear.r);
    encoded.g = alwan_srgb_oetf_f64(linear.g);
    encoded.b = alwan_srgb_oetf_f64(linear.b);
    return alwan_rgb_to_hsl_f64_v(encoded);
}

ALWAN_INLINE alwan_rgb_f64 alwan_hsl_to_linear_srgb_f64_v(alwan_hsl_f64 hsl) {
    alwan_rgb_f64 encoded = alwan_hsl_to_rgb_f64_v(hsl);
    alwan_rgb_f64 linear;
    linear.r = alwan_srgb_eotf_f64(encoded.r);
    linear.g = alwan_srgb_eotf_f64(encoded.g);
    linear.b = alwan_srgb_eotf_f64(encoded.b);
    return linear;
}

#endif /* ALWAN_CORE_H */

#else /* HLSL / GLSL / Halide */
/* ================================================================
 * GPU Backends: Single-precision only (original code)
 * ================================================================ */

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

ALWAN_INLINE alwan_hsv alwan_rgb_to_hsv_v(alwan_rgb rgb) {
    alwan_hsv result;
    alwan_scalar max_val = alwan_max3(rgb.r, rgb.g, rgb.b);
    alwan_scalar min_val = alwan_min3(rgb.r, rgb.g, rgb.b);
    alwan_scalar delta = max_val - min_val;
    result.v = max_val;
    result.s = ALWAN_SELECT(max_val > ALWAN_LITERAL(0.0), delta / max_val, ALWAN_LITERAL(0.0));
    alwan_scalar h_r = ALWAN_LITERAL(60.0) * (rgb.g - rgb.b) / delta;
    alwan_scalar h_r_adj = ALWAN_SELECT(rgb.g < rgb.b, h_r + ALWAN_LITERAL(360.0), h_r);
    alwan_scalar h_g = ALWAN_LITERAL(60.0) * ((rgb.b - rgb.r) / delta + ALWAN_LITERAL(2.0));
    alwan_scalar h_b = ALWAN_LITERAL(60.0) * ((rgb.r - rgb.g) / delta + ALWAN_LITERAL(4.0));
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
    h = h - ALWAN_FLOOR(h / ALWAN_LITERAL(360.0)) * ALWAN_LITERAL(360.0);
    h = ALWAN_SELECT(h < ALWAN_LITERAL(0.0), h + ALWAN_LITERAL(360.0), h);
    alwan_scalar h_sector = h / ALWAN_LITERAL(60.0);
    alwan_scalar sector_f = ALWAN_FLOOR(h_sector);
    alwan_scalar f = h_sector - sector_f;
    alwan_scalar p = v * (ALWAN_LITERAL(1.0) - s);
    alwan_scalar q = v * (ALWAN_LITERAL(1.0) - s * f);
    alwan_scalar t = v * (ALWAN_LITERAL(1.0) - s * (ALWAN_LITERAL(1.0) - f));
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
    result.r = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), v, result.r);
    result.g = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), v, result.g);
    result.b = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), v, result.b);
    return result;
}

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
    alwan_scalar l = (max_val + min_val) / ALWAN_LITERAL(2.0);
    alwan_scalar s = ALWAN_SELECT(delta > ALWAN_LITERAL(0.0),
        ALWAN_SELECT(l < ALWAN_LITERAL(0.5),
                     delta / (max_val + min_val),
                     delta / (ALWAN_LITERAL(2.0) - max_val - min_val)),
        ALWAN_LITERAL(0.0));
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
    result.r = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), l, r);
    result.g = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), l, g);
    result.b = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), l, b);
    return result;
}

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

typedef struct {
    alwan_scalar y_min, y_max, c_min, c_max;
} alwan_legal_range;

ALWAN_INLINE alwan_legal_range alwan_legal_range_from_bit_depth(int bit_depth) {
    alwan_legal_range r;
    alwan_scalar max_val, scale;
    switch (bit_depth) {
        case  8: max_val = ALWAN_LITERAL(255.0);   scale = ALWAN_LITERAL(1.0);   break;
        case 10: max_val = ALWAN_LITERAL(1023.0);  scale = ALWAN_LITERAL(4.0);   break;
        case 12: max_val = ALWAN_LITERAL(4095.0);  scale = ALWAN_LITERAL(16.0);  break;
        case 16: max_val = ALWAN_LITERAL(65535.0); scale = ALWAN_LITERAL(256.0); break;
        default: max_val = ALWAN_LITERAL(1023.0);  scale = ALWAN_LITERAL(4.0);   break;
    }
    r.y_min = ALWAN_LITERAL(16.0)  * scale / max_val;
    r.y_max = ALWAN_LITERAL(235.0) * scale / max_val;
    r.c_min = ALWAN_LITERAL(16.0)  * scale / max_val;
    r.c_max = ALWAN_LITERAL(240.0) * scale / max_val;
    return r;
}

ALWAN_INLINE alwan_yccbccrc alwan_rgb_to_yccbccrc_v(alwan_rgb rgb, int bit_depth) {
    alwan_yccbccrc result;
    alwan_scalar const kr = ALWAN_LUMA_KR_BT2020;
    alwan_scalar const kg = ALWAN_LUMA_KG_BT2020;
    alwan_scalar const kb = ALWAN_LUMA_KB_BT2020;
    alwan_scalar yc_linear = kr * rgb.r + kg * rgb.g + kb * rgb.b;
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
    alwan_scalar diff_b = b_gamma - yc;
    alwan_scalar diff_r = r_gamma - yc;
    alwan_scalar cbc = ALWAN_SELECT(diff_b <= ALWAN_LITERAL(0.0),
                                    diff_b / ALWAN_LITERAL(1.9404),
                                    diff_b / ALWAN_LITERAL(1.5816));
    alwan_scalar crc = ALWAN_SELECT(diff_r <= ALWAN_LITERAL(0.0),
                                    diff_r / ALWAN_LITERAL(1.7184),
                                    diff_r / ALWAN_LITERAL(0.9936));
    alwan_legal_range lr = alwan_legal_range_from_bit_depth(bit_depth);
    result.Yc  = yc * (lr.y_max - lr.y_min) + lr.y_min;
    result.Cbc = cbc * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_LITERAL(2.0);
    result.Crc = crc * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_LITERAL(2.0);
    return result;
}

ALWAN_INLINE alwan_rgb alwan_yccbccrc_to_rgb_v(alwan_yccbccrc yccbccrc, int bit_depth) {
    alwan_rgb result;
    alwan_scalar const kr = ALWAN_LUMA_KR_BT2020;
    alwan_scalar const kg = ALWAN_LUMA_KG_BT2020;
    alwan_scalar const kb = ALWAN_LUMA_KB_BT2020;
    alwan_legal_range lr = alwan_legal_range_from_bit_depth(bit_depth);
    alwan_scalar c_center = (lr.c_max + lr.c_min) / ALWAN_LITERAL(2.0);
    alwan_scalar yc = (yccbccrc.Yc - lr.y_min) / (lr.y_max - lr.y_min);
    alwan_scalar cbc = (yccbccrc.Cbc - c_center) / (lr.c_max - lr.c_min);
    alwan_scalar crc = (yccbccrc.Crc - c_center) / (lr.c_max - lr.c_min);
    alwan_scalar diff_b = ALWAN_SELECT(cbc <= ALWAN_LITERAL(0.0),
                                       cbc * ALWAN_LITERAL(1.9404),
                                       cbc * ALWAN_LITERAL(1.5816));
    alwan_scalar diff_r = ALWAN_SELECT(crc <= ALWAN_LITERAL(0.0),
                                       crc * ALWAN_LITERAL(1.7184),
                                       crc * ALWAN_LITERAL(0.9936));
    alwan_scalar r_gamma = yc + diff_r;
    alwan_scalar b_gamma = yc + diff_b;
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

ALWAN_INLINE alwan_ycbcr alwan_ycbcr_full_to_legal_v(alwan_ycbcr ycbcr, int bit_depth) {
    alwan_legal_range lr = alwan_legal_range_from_bit_depth(bit_depth);
    alwan_ycbcr result;
    result.Y  = ycbcr.Y  * (lr.y_max - lr.y_min) + lr.y_min;
    result.Cb = ycbcr.Cb * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_LITERAL(2.0);
    result.Cr = ycbcr.Cr * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_LITERAL(2.0);
    return result;
}

ALWAN_INLINE alwan_ycbcr alwan_ycbcr_legal_to_full_v(alwan_ycbcr ycbcr, int bit_depth) {
    alwan_legal_range lr = alwan_legal_range_from_bit_depth(bit_depth);
    alwan_scalar c_center = (lr.c_max + lr.c_min) / ALWAN_LITERAL(2.0);
    alwan_ycbcr result;
    result.Y  = (ycbcr.Y  - lr.y_min) / (lr.y_max - lr.y_min);
    result.Cb = (ycbcr.Cb - c_center) / (lr.c_max - lr.c_min);
    result.Cr = (ycbcr.Cr - c_center) / (lr.c_max - lr.c_min);
    return result;
}

ALWAN_INLINE alwan_cmyk alwan_cmy_to_cmyk_v(alwan_cmy cmy) {
    alwan_cmyk result;
    alwan_scalar k = alwan_min3(cmy.c, cmy.m, cmy.y);
    alwan_scalar denom = ALWAN_LITERAL(1.0) - k;
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

typedef struct {
    alwan_scalar h, w, b;
} alwan_hwb;

ALWAN_INLINE alwan_hwb alwan_hsv_to_hwb_v(alwan_hsv hsv) {
    alwan_hwb result;
    result.h = hsv.h;
    result.w = (ALWAN_ONE - hsv.s) * hsv.v;
    result.b = ALWAN_ONE - hsv.v;
    return result;
}

ALWAN_INLINE alwan_hsv alwan_hwb_to_hsv_v(alwan_hwb hwb) {
    alwan_hsv result;
    result.h = hwb.h;
    result.v = ALWAN_ONE - hwb.b;
    alwan_scalar wb_sum = hwb.w + hwb.b;
    alwan_scalar safe_v = ALWAN_SELECT(result.v < ALWAN_LITERAL(1e-10),
                                        ALWAN_LITERAL(1e-10), result.v);
    result.s = ALWAN_SELECT(wb_sum >= ALWAN_ONE,
                            ALWAN_ZERO,
                            ALWAN_ONE - hwb.w / safe_v);
    result.v = ALWAN_SELECT(wb_sum >= ALWAN_ONE,
                            hwb.w / wb_sum,
                            result.v);
    return result;
}

ALWAN_INLINE alwan_hwb alwan_rgb_to_hwb_v(alwan_rgb rgb) {
    alwan_hsv hsv = alwan_rgb_to_hsv_v(rgb);
    return alwan_hsv_to_hwb_v(hsv);
}

ALWAN_INLINE alwan_rgb alwan_hwb_to_rgb_v(alwan_hwb hwb) {
    alwan_hsv hsv = alwan_hwb_to_hsv_v(hwb);
    return alwan_hsv_to_rgb_v(hsv);
}

ALWAN_INLINE alwan_scalar alwan_relative_luminance_v(alwan_rgb rgb,
                                                      alwan_scalar kr,
                                                      alwan_scalar kg,
                                                      alwan_scalar kb) {
    return kr * rgb.r + kg * rgb.g + kb * rgb.b;
}

ALWAN_INLINE alwan_hsp alwan_rgb_to_hsp_v(alwan_rgb rgb) {
    alwan_hsp result;
    alwan_hsv hsv = alwan_rgb_to_hsv_v(rgb);
    result.h = hsv.h;
    result.s = hsv.s;
    alwan_scalar const Pr = ALWAN_LUMA_KR_BT601;
    alwan_scalar const Pg = ALWAN_LUMA_KG_BT601;
    alwan_scalar const Pb = ALWAN_LUMA_KB_BT601;
    result.p = ALWAN_SQRT(Pr * rgb.r * rgb.r + Pg * rgb.g * rgb.g + Pb * rgb.b * rgb.b);
    return result;
}

ALWAN_INLINE alwan_rgb alwan_hsp_to_rgb_v(alwan_hsp hsp) {
    alwan_rgb result;
    alwan_scalar const Pr = ALWAN_LUMA_KR_BT601;
    alwan_scalar const Pg = ALWAN_LUMA_KG_BT601;
    alwan_scalar const Pb = ALWAN_LUMA_KB_BT601;
    alwan_scalar h = hsp.h * ALWAN_LITERAL(360.0);
    alwan_scalar s = hsp.s;
    alwan_scalar p = hsp.p;
    h = h - ALWAN_FLOOR(h / ALWAN_LITERAL(360.0)) * ALWAN_LITERAL(360.0);
    h = ALWAN_SELECT(h < ALWAN_LITERAL(0.0), h + ALWAN_LITERAL(360.0), h);
    alwan_scalar min_part = ALWAN_LITERAL(1.0) - s;
    alwan_scalar r, g, b;
    alwan_scalar f0 = h / ALWAN_LITERAL(60.0);
    alwan_scalar mid_frac_0 = min_part + s * f0;
    alwan_scalar denom_0 = ALWAN_SQRT(Pr + Pg * mid_frac_0 * mid_frac_0 + Pb * min_part * min_part);
    alwan_scalar v_0 = ALWAN_SELECT(denom_0 > ALWAN_LITERAL(0.0), p / denom_0, ALWAN_LITERAL(0.0));
    alwan_scalar r_0 = v_0; alwan_scalar g_0 = v_0 * mid_frac_0; alwan_scalar b_0 = v_0 * min_part;
    alwan_scalar f1 = ALWAN_LITERAL(1.0) - (h - ALWAN_LITERAL(60.0)) / ALWAN_LITERAL(60.0);
    alwan_scalar mid_frac_1 = min_part + s * f1;
    alwan_scalar denom_1 = ALWAN_SQRT(Pg + Pr * mid_frac_1 * mid_frac_1 + Pb * min_part * min_part);
    alwan_scalar v_1 = ALWAN_SELECT(denom_1 > ALWAN_LITERAL(0.0), p / denom_1, ALWAN_LITERAL(0.0));
    alwan_scalar r_1 = v_1 * mid_frac_1; alwan_scalar g_1 = v_1; alwan_scalar b_1 = v_1 * min_part;
    alwan_scalar f2 = (h - ALWAN_LITERAL(120.0)) / ALWAN_LITERAL(60.0);
    alwan_scalar mid_frac_2 = min_part + s * f2;
    alwan_scalar denom_2 = ALWAN_SQRT(Pg + Pb * mid_frac_2 * mid_frac_2 + Pr * min_part * min_part);
    alwan_scalar v_2 = ALWAN_SELECT(denom_2 > ALWAN_LITERAL(0.0), p / denom_2, ALWAN_LITERAL(0.0));
    alwan_scalar r_2 = v_2 * min_part; alwan_scalar g_2 = v_2; alwan_scalar b_2 = v_2 * mid_frac_2;
    alwan_scalar f3 = ALWAN_LITERAL(1.0) - (h - ALWAN_LITERAL(180.0)) / ALWAN_LITERAL(60.0);
    alwan_scalar mid_frac_3 = min_part + s * f3;
    alwan_scalar denom_3 = ALWAN_SQRT(Pb + Pg * mid_frac_3 * mid_frac_3 + Pr * min_part * min_part);
    alwan_scalar v_3 = ALWAN_SELECT(denom_3 > ALWAN_LITERAL(0.0), p / denom_3, ALWAN_LITERAL(0.0));
    alwan_scalar r_3 = v_3 * min_part; alwan_scalar g_3 = v_3 * mid_frac_3; alwan_scalar b_3 = v_3;
    alwan_scalar f4 = (h - ALWAN_LITERAL(240.0)) / ALWAN_LITERAL(60.0);
    alwan_scalar mid_frac_4 = min_part + s * f4;
    alwan_scalar denom_4 = ALWAN_SQRT(Pb + Pr * mid_frac_4 * mid_frac_4 + Pg * min_part * min_part);
    alwan_scalar v_4 = ALWAN_SELECT(denom_4 > ALWAN_LITERAL(0.0), p / denom_4, ALWAN_LITERAL(0.0));
    alwan_scalar r_4 = v_4 * mid_frac_4; alwan_scalar g_4 = v_4 * min_part; alwan_scalar b_4 = v_4;
    alwan_scalar f5 = ALWAN_LITERAL(1.0) - (h - ALWAN_LITERAL(300.0)) / ALWAN_LITERAL(60.0);
    alwan_scalar mid_frac_5 = min_part + s * f5;
    alwan_scalar denom_5 = ALWAN_SQRT(Pr + Pb * mid_frac_5 * mid_frac_5 + Pg * min_part * min_part);
    alwan_scalar v_5 = ALWAN_SELECT(denom_5 > ALWAN_LITERAL(0.0), p / denom_5, ALWAN_LITERAL(0.0));
    alwan_scalar r_5 = v_5; alwan_scalar g_5 = v_5 * min_part; alwan_scalar b_5 = v_5 * mid_frac_5;
    alwan_scalar r_01 = ALWAN_SELECT(h < ALWAN_LITERAL(60.0), r_0, r_1);
    alwan_scalar g_01 = ALWAN_SELECT(h < ALWAN_LITERAL(60.0), g_0, g_1);
    alwan_scalar b_01 = ALWAN_SELECT(h < ALWAN_LITERAL(60.0), b_0, b_1);
    alwan_scalar r_23 = ALWAN_SELECT(h < ALWAN_LITERAL(180.0), r_2, r_3);
    alwan_scalar g_23 = ALWAN_SELECT(h < ALWAN_LITERAL(180.0), g_2, g_3);
    alwan_scalar b_23 = ALWAN_SELECT(h < ALWAN_LITERAL(180.0), b_2, b_3);
    alwan_scalar r_45 = ALWAN_SELECT(h < ALWAN_LITERAL(300.0), r_4, r_5);
    alwan_scalar g_45 = ALWAN_SELECT(h < ALWAN_LITERAL(300.0), g_4, g_5);
    alwan_scalar b_45 = ALWAN_SELECT(h < ALWAN_LITERAL(300.0), b_4, b_5);
    alwan_scalar r_0123 = ALWAN_SELECT(h < ALWAN_LITERAL(120.0), r_01, r_23);
    alwan_scalar g_0123 = ALWAN_SELECT(h < ALWAN_LITERAL(120.0), g_01, g_23);
    alwan_scalar b_0123 = ALWAN_SELECT(h < ALWAN_LITERAL(120.0), b_01, b_23);
    r = ALWAN_SELECT(h < ALWAN_LITERAL(240.0), r_0123, r_45);
    g = ALWAN_SELECT(h < ALWAN_LITERAL(240.0), g_0123, g_45);
    b = ALWAN_SELECT(h < ALWAN_LITERAL(240.0), b_0123, b_45);
    result.r = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), p, r);
    result.g = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), p, g);
    result.b = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), p, b);
    return result;
}

ALWAN_INLINE alwan_hsplog alwan_rgb_to_hsplog_v(alwan_rgb rgb) {
    alwan_hsplog result;
    alwan_hsp hsp = alwan_rgb_to_hsp_v(rgb);
    result.h = hsp.h;
    result.p = hsp.p;
    result.s = ALWAN_LOG10(ALWAN_LITERAL(1.0) + ALWAN_LITERAL(9.0) * hsp.s);
    return result;
}

ALWAN_INLINE alwan_rgb alwan_hsplog_to_rgb_v(alwan_hsplog hsplog) {
    alwan_hsp hsp;
    hsp.h = hsplog.h;
    hsp.p = hsplog.p;
    hsp.s = (ALWAN_POW(ALWAN_LITERAL(10.0), hsplog.s) - ALWAN_LITERAL(1.0)) / ALWAN_LITERAL(9.0);
    return alwan_hsp_to_rgb_v(hsp);
}

ALWAN_INLINE alwan_scalar alwan__hsy_max_chroma(alwan_scalar h,
                                                  alwan_scalar kr,
                                                  alwan_scalar kg,
                                                  alwan_scalar kb) {
    alwan_scalar h6 = h * ALWAN_LITERAL(6.0);
    alwan_scalar sector = ALWAN_FLOOR(h6);
    alwan_scalar f = h6 - sector;
    alwan_scalar r_01 = ALWAN_SELECT(sector < ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) - f);
    alwan_scalar g_01 = ALWAN_SELECT(sector < ALWAN_LITERAL(1.0), f, ALWAN_LITERAL(1.0));
    alwan_scalar b_01 = ALWAN_LITERAL(0.0);
    alwan_scalar r_23 = ALWAN_LITERAL(0.0);
    alwan_scalar g_23 = ALWAN_SELECT(sector < ALWAN_LITERAL(3.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) - f);
    alwan_scalar b_23 = ALWAN_SELECT(sector < ALWAN_LITERAL(3.0), f, ALWAN_LITERAL(1.0));
    alwan_scalar r_45 = ALWAN_SELECT(sector < ALWAN_LITERAL(5.0), f, ALWAN_LITERAL(1.0));
    alwan_scalar g_45 = ALWAN_LITERAL(0.0);
    alwan_scalar b_45 = ALWAN_SELECT(sector < ALWAN_LITERAL(5.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) - f);
    alwan_scalar r_0123 = ALWAN_SELECT(sector < ALWAN_LITERAL(2.0), r_01, r_23);
    alwan_scalar g_0123 = ALWAN_SELECT(sector < ALWAN_LITERAL(2.0), g_01, g_23);
    alwan_scalar b_0123 = ALWAN_SELECT(sector < ALWAN_LITERAL(2.0), b_01, b_23);
    alwan_scalar pr = ALWAN_SELECT(sector < ALWAN_LITERAL(4.0), r_0123, r_45);
    alwan_scalar pg = ALWAN_SELECT(sector < ALWAN_LITERAL(4.0), g_0123, g_45);
    alwan_scalar pb = ALWAN_SELECT(sector < ALWAN_LITERAL(4.0), b_0123, b_45);
    return kr * pr + kg * pg + kb * pb;
}

ALWAN_INLINE alwan_hsy alwan_rgb_to_hsy_v(alwan_rgb rgb) {
    alwan_hsy result;
    alwan_scalar const kr = ALWAN_LUMA_KR_BT601;
    alwan_scalar const kg = ALWAN_LUMA_KG_BT601;
    alwan_scalar const kb = ALWAN_LUMA_KB_BT601;
    alwan_hsv hsv = alwan_rgb_to_hsv_v(rgb);
    result.h = hsv.h;
    alwan_scalar y = kr * rgb.r + kg * rgb.g + kb * rgb.b;
    result.y = y;
    alwan_scalar max_val = alwan_max3(rgb.r, rgb.g, rgb.b);
    alwan_scalar min_val = alwan_min3(rgb.r, rgb.g, rgb.b);
    alwan_scalar chroma = max_val - min_val;
    alwan_scalar y_pure = alwan__hsy_max_chroma(result.h, kr, kg, kb);
    alwan_scalar max_c;
    alwan_scalar safe_ypure = ALWAN_SELECT(y_pure < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), y_pure);
    alwan_scalar safe_ypure_inv = ALWAN_SELECT((ALWAN_LITERAL(1.0) - y_pure) < ALWAN_LITERAL(1e-10),
                                                ALWAN_LITERAL(1e-10),
                                                ALWAN_LITERAL(1.0) - y_pure);
    max_c = ALWAN_SELECT(y <= y_pure,
                          y / safe_ypure,
                          (ALWAN_LITERAL(1.0) - y) / safe_ypure_inv);
    max_c = ALWAN_SELECT(max_c < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), max_c);
    result.s = ALWAN_SELECT(chroma > ALWAN_LITERAL(0.0), chroma / max_c, ALWAN_LITERAL(0.0));
    return result;
}

ALWAN_INLINE alwan_rgb alwan_hsy_to_rgb_v(alwan_hsy hsy) {
    alwan_rgb result;
    alwan_scalar const kr = ALWAN_LUMA_KR_BT601;
    alwan_scalar const kg = ALWAN_LUMA_KG_BT601;
    alwan_scalar const kb = ALWAN_LUMA_KB_BT601;
    alwan_scalar h = hsy.h;
    alwan_scalar s = hsy.s;
    alwan_scalar y = hsy.y;
    alwan_scalar y_pure = alwan__hsy_max_chroma(h, kr, kg, kb);
    alwan_scalar safe_ypure = ALWAN_SELECT(y_pure < ALWAN_LITERAL(1e-10), ALWAN_LITERAL(1e-10), y_pure);
    alwan_scalar safe_ypure_inv = ALWAN_SELECT((ALWAN_LITERAL(1.0) - y_pure) < ALWAN_LITERAL(1e-10),
                                                ALWAN_LITERAL(1e-10),
                                                ALWAN_LITERAL(1.0) - y_pure);
    alwan_scalar max_c = ALWAN_SELECT(y <= y_pure,
                                       y / safe_ypure,
                                       (ALWAN_LITERAL(1.0) - y) / safe_ypure_inv);
    alwan_scalar chroma = s * max_c;
    alwan_scalar h6 = h * ALWAN_LITERAL(6.0);
    alwan_scalar sector = ALWAN_FLOOR(h6);
    alwan_scalar f = h6 - sector;
    alwan_scalar r_01 = ALWAN_SELECT(sector < ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) - f);
    alwan_scalar g_01 = ALWAN_SELECT(sector < ALWAN_LITERAL(1.0), f, ALWAN_LITERAL(1.0));
    alwan_scalar b_01 = ALWAN_LITERAL(0.0);
    alwan_scalar r_23 = ALWAN_LITERAL(0.0);
    alwan_scalar g_23 = ALWAN_SELECT(sector < ALWAN_LITERAL(3.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) - f);
    alwan_scalar b_23 = ALWAN_SELECT(sector < ALWAN_LITERAL(3.0), f, ALWAN_LITERAL(1.0));
    alwan_scalar r_45 = ALWAN_SELECT(sector < ALWAN_LITERAL(5.0), f, ALWAN_LITERAL(1.0));
    alwan_scalar g_45 = ALWAN_LITERAL(0.0);
    alwan_scalar b_45 = ALWAN_SELECT(sector < ALWAN_LITERAL(5.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0) - f);
    alwan_scalar r_0123 = ALWAN_SELECT(sector < ALWAN_LITERAL(2.0), r_01, r_23);
    alwan_scalar g_0123 = ALWAN_SELECT(sector < ALWAN_LITERAL(2.0), g_01, g_23);
    alwan_scalar b_0123 = ALWAN_SELECT(sector < ALWAN_LITERAL(2.0), b_01, b_23);
    alwan_scalar pr = ALWAN_SELECT(sector < ALWAN_LITERAL(4.0), r_0123, r_45);
    alwan_scalar pg = ALWAN_SELECT(sector < ALWAN_LITERAL(4.0), g_0123, g_45);
    alwan_scalar pb = ALWAN_SELECT(sector < ALWAN_LITERAL(4.0), b_0123, b_45);
    alwan_scalar r_hue = chroma * pr;
    alwan_scalar g_hue = chroma * pg;
    alwan_scalar b_hue = chroma * pb;
    alwan_scalar m = y - (kr * r_hue + kg * g_hue + kb * b_hue);
    alwan_scalar r = r_hue + m;
    alwan_scalar g = g_hue + m;
    alwan_scalar b = b_hue + m;
    result.r = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), y, r);
    result.g = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), y, g);
    result.b = ALWAN_SELECT(s <= ALWAN_LITERAL(0.0), y, b);
    return result;
}

#ifdef ALWAN_CORE_H

ALWAN_INLINE alwan_hsv alwan_linear_srgb_to_hsv_v(alwan_rgb linear) {
    alwan_rgb encoded;
    encoded.r = alwan_srgb_oetf(linear.r);
    encoded.g = alwan_srgb_oetf(linear.g);
    encoded.b = alwan_srgb_oetf(linear.b);
    return alwan_rgb_to_hsv_v(encoded);
}

ALWAN_INLINE alwan_rgb alwan_hsv_to_linear_srgb_v(alwan_hsv hsv) {
    alwan_rgb encoded = alwan_hsv_to_rgb_v(hsv);
    alwan_rgb linear;
    linear.r = alwan_srgb_eotf(encoded.r);
    linear.g = alwan_srgb_eotf(encoded.g);
    linear.b = alwan_srgb_eotf(encoded.b);
    return linear;
}

ALWAN_INLINE alwan_hsl alwan_linear_srgb_to_hsl_v(alwan_rgb linear) {
    alwan_rgb encoded;
    encoded.r = alwan_srgb_oetf(linear.r);
    encoded.g = alwan_srgb_oetf(linear.g);
    encoded.b = alwan_srgb_oetf(linear.b);
    return alwan_rgb_to_hsl_v(encoded);
}

ALWAN_INLINE alwan_rgb alwan_hsl_to_linear_srgb_v(alwan_hsl hsl) {
    alwan_rgb encoded = alwan_hsl_to_rgb_v(hsl);
    alwan_rgb linear;
    linear.r = alwan_srgb_eotf(encoded.r);
    linear.g = alwan_srgb_eotf(encoded.g);
    linear.b = alwan_srgb_eotf(encoded.b);
    return linear;
}

#endif /* ALWAN_CORE_H */

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_CONVENIENCE_CORE_H */
