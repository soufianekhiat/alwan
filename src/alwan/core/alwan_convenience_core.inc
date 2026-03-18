/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Convenience Color Conversions (parameterized)
 * CMY, YCoCg, HSV, HSL, YCbCr, YcCbcCrc
 * No include guard - included twice via setup/teardown.
 */

/* ----------------------------------------------------------------
 * RGB <-> CMY (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_CMY ALWAN_CORE_FNVLIT(alwan_rgb_to_cmy)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_CMY result;
    result.c = ALWAN_CORE_LITERAL(1.0) - rgb.r;
    result.m = ALWAN_CORE_LITERAL(1.0) - rgb.g;
    result.y = ALWAN_CORE_LITERAL(1.0) - rgb.b;
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_cmy_to_rgb)(ALWAN_CORE_CMY cmy) {
    ALWAN_CORE_RGB result;
    result.r = ALWAN_CORE_LITERAL(1.0) - cmy.c;
    result.g = ALWAN_CORE_LITERAL(1.0) - cmy.m;
    result.b = ALWAN_CORE_LITERAL(1.0) - cmy.y;
    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_YCOCG ALWAN_CORE_FNVLIT(alwan_rgb_to_ycocg)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_YCOCG result;
    result.Y  = ALWAN_CORE_LITERAL(0.25) * rgb.r + ALWAN_CORE_LITERAL(0.5) * rgb.g + ALWAN_CORE_LITERAL(0.25) * rgb.b;
    result.Co = ALWAN_CORE_LITERAL(0.5) * rgb.r - ALWAN_CORE_LITERAL(0.5) * rgb.b;
    result.Cg = -ALWAN_CORE_LITERAL(0.25) * rgb.r + ALWAN_CORE_LITERAL(0.5) * rgb.g - ALWAN_CORE_LITERAL(0.25) * rgb.b;
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_ycocg_to_rgb)(ALWAN_CORE_YCOCG ycocg) {
    ALWAN_CORE_RGB result;
    ALWAN_CORE_T temp = ycocg.Y - ycocg.Cg;
    result.r = temp + ycocg.Co;
    result.g = ycocg.Y + ycocg.Cg;
    result.b = temp - ycocg.Co;
    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> HSV (value-returning)
 * Operates on encoded (display-referred) sRGB values in [0,1].
 * For linear input, apply alwan_srgb_oetf first.
 * Hue in [0, 1] (0 = 0 degrees, 1 = 360 degrees)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_HSV ALWAN_CORE_FNVLIT(alwan_rgb_to_hsv)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_HSV result;

    ALWAN_CORE_T max_val = ALWAN_CORE_MAX3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T min_val = ALWAN_CORE_MIN3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T delta = max_val - min_val;

    /* V */
    result.v = max_val;

    /* S */
    result.s = ALWAN_CORE_SELECT(max_val > ALWAN_CORE_LITERAL(0.0), delta / max_val, ALWAN_CORE_LITERAL(0.0));

    /* H (in degrees, then normalize to [0,1]) */
    /* Determine which channel is max via nested ALWAN_CORE_SELECT */
    ALWAN_CORE_T h_r = ALWAN_CORE_LITERAL(60.0) * (rgb.g - rgb.b) / delta;
    ALWAN_CORE_T h_r_adj = ALWAN_CORE_SELECT(rgb.g < rgb.b, h_r + ALWAN_CORE_LITERAL(360.0), h_r);
    ALWAN_CORE_T h_g = ALWAN_CORE_LITERAL(60.0) * ((rgb.b - rgb.r) / delta + ALWAN_CORE_LITERAL(2.0));
    ALWAN_CORE_T h_b = ALWAN_CORE_LITERAL(60.0) * ((rgb.r - rgb.g) / delta + ALWAN_CORE_LITERAL(4.0));

    /* Select hue based on which channel is max */
    /* max == r? use h_r. max == g? use h_g. else h_b */
    ALWAN_CORE_T h = ALWAN_CORE_SELECT(max_val == rgb.r, h_r_adj,
                      ALWAN_CORE_SELECT(max_val == rgb.g, h_g, h_b));
    h = ALWAN_CORE_SELECT(delta > ALWAN_CORE_LITERAL(0.0), h / ALWAN_CORE_LITERAL(360.0), ALWAN_CORE_LITERAL(0.0));

    result.h = h;
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hsv_to_rgb)(ALWAN_CORE_HSV hsv) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T h = hsv.h * ALWAN_CORE_LITERAL(360.0);
    ALWAN_CORE_T s = hsv.s;
    ALWAN_CORE_T v = hsv.v;

    /* Normalize hue to [0, 360) via modulo arithmetic */
    h = h - ALWAN_CORE_FLOOR(h / ALWAN_CORE_LITERAL(360.0)) * ALWAN_CORE_LITERAL(360.0);
    h = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(0.0), h + ALWAN_CORE_LITERAL(360.0), h);

    ALWAN_CORE_T h_sector = h / ALWAN_CORE_LITERAL(60.0);
    ALWAN_CORE_T sector_f = ALWAN_CORE_FLOOR(h_sector);
    ALWAN_CORE_T f = h_sector - sector_f;

    ALWAN_CORE_T p = v * (ALWAN_CORE_LITERAL(1.0) - s);
    ALWAN_CORE_T q = v * (ALWAN_CORE_LITERAL(1.0) - s * f);
    ALWAN_CORE_T t = v * (ALWAN_CORE_LITERAL(1.0) - s * (ALWAN_CORE_LITERAL(1.0) - f));

    /* 6-sector via nested ALWAN_CORE_SELECT */
    /* sector 0: v,t,p  sector 1: q,v,p  sector 2: p,v,t
     * sector 3: p,q,v  sector 4: t,p,v  sector 5: v,p,q */
    ALWAN_CORE_T r_01 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(1.0), v, q);
    ALWAN_CORE_T g_01 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(1.0), t, v);
    ALWAN_CORE_T b_01 = p;

    ALWAN_CORE_T r_23 = p;
    ALWAN_CORE_T g_23 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(3.0), v, q);
    ALWAN_CORE_T b_23 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(3.0), t, v);

    ALWAN_CORE_T r_45 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(5.0), t, v);
    ALWAN_CORE_T g_45 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(5.0), p, p);
    ALWAN_CORE_T b_45 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(5.0), v, q);

    ALWAN_CORE_T r_0123 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(2.0), r_01, r_23);
    ALWAN_CORE_T g_0123 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(2.0), g_01, g_23);
    ALWAN_CORE_T b_0123 = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(2.0), b_01, b_23);

    result.r = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(4.0), r_0123, r_45);
    result.g = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(4.0), g_0123, g_45);
    result.b = ALWAN_CORE_SELECT(sector_f < ALWAN_CORE_LITERAL(4.0), b_0123, b_45);

    /* Achromatic: when s <= 0, return (v,v,v) */
    result.r = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), v, result.r);
    result.g = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), v, result.g);
    result.b = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), v, result.b);

    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> HSL (value-returning)
 * Operates on encoded (display-referred) sRGB values in [0,1].
 * For linear input, apply alwan_srgb_oetf first.
 * Hue in [0, 1] (0 = 0 degrees, 1 = 360 degrees)
 * ---------------------------------------------------------------- */

/* hue sector interpolation (already branchless) */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_hue_to_rgb)(ALWAN_CORE_T p, ALWAN_CORE_T q, ALWAN_CORE_T t) {
    t = ALWAN_CORE_SELECT(t < ALWAN_CORE_LITERAL(0.0), t + ALWAN_CORE_LITERAL(1.0), t);
    t = ALWAN_CORE_SELECT(t > ALWAN_CORE_LITERAL(1.0), t - ALWAN_CORE_LITERAL(1.0), t);
    return ALWAN_CORE_SELECT(t < ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(6.0),
                        p + (q - p) * ALWAN_CORE_LITERAL(6.0) * t,
           ALWAN_CORE_SELECT(t < ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(2.0),
                        q,
           ALWAN_CORE_SELECT(t < ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0),
                        p + (q - p) * (ALWAN_CORE_LITERAL(2.0) / ALWAN_CORE_LITERAL(3.0) - t) * ALWAN_CORE_LITERAL(6.0),
                        p)));
}

ALWAN_INLINE ALWAN_CORE_HSL ALWAN_CORE_FNVLIT(alwan_rgb_to_hsl)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_HSL result;

    ALWAN_CORE_T max_val = ALWAN_CORE_MAX3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T min_val = ALWAN_CORE_MIN3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T delta = max_val - min_val;

    /* L */
    ALWAN_CORE_T l = (max_val + min_val) / ALWAN_CORE_LITERAL(2.0);

    /* S */
    ALWAN_CORE_T s = ALWAN_CORE_SELECT(delta > ALWAN_CORE_LITERAL(0.0),
        ALWAN_CORE_SELECT(l < ALWAN_CORE_LITERAL(0.5),
                     delta / (max_val + min_val),
                     delta / (ALWAN_CORE_LITERAL(2.0) - max_val - min_val)),
        ALWAN_CORE_LITERAL(0.0));

    /* H (same logic as HSV) */
    ALWAN_CORE_T h_r = ALWAN_CORE_LITERAL(60.0) * (rgb.g - rgb.b) / delta;
    ALWAN_CORE_T h_r_adj = ALWAN_CORE_SELECT(rgb.g < rgb.b, h_r + ALWAN_CORE_LITERAL(360.0), h_r);
    ALWAN_CORE_T h_g = ALWAN_CORE_LITERAL(60.0) * ((rgb.b - rgb.r) / delta + ALWAN_CORE_LITERAL(2.0));
    ALWAN_CORE_T h_b = ALWAN_CORE_LITERAL(60.0) * ((rgb.r - rgb.g) / delta + ALWAN_CORE_LITERAL(4.0));

    ALWAN_CORE_T h = ALWAN_CORE_SELECT(max_val == rgb.r, h_r_adj,
                      ALWAN_CORE_SELECT(max_val == rgb.g, h_g, h_b));
    h = ALWAN_CORE_SELECT(delta > ALWAN_CORE_LITERAL(0.0), h / ALWAN_CORE_LITERAL(360.0), ALWAN_CORE_LITERAL(0.0));

    result.h = h;
    result.s = s;
    result.l = l;
    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hsl_to_rgb)(ALWAN_CORE_HSL hsl) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T h = hsl.h;
    ALWAN_CORE_T s = hsl.s;
    ALWAN_CORE_T l = hsl.l;

    ALWAN_CORE_T q = ALWAN_CORE_SELECT(l < ALWAN_CORE_LITERAL(0.5),
                              l * (ALWAN_CORE_LITERAL(1.0) + s),
                              l + s - l * s);
    ALWAN_CORE_T p = ALWAN_CORE_LITERAL(2.0) * l - q;

    ALWAN_CORE_T r = ALWAN_CORE_FNVLIT(alwan_hue_to_rgb)(p, q, h + ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.0));
    ALWAN_CORE_T g = ALWAN_CORE_FNVLIT(alwan_hue_to_rgb)(p, q, h);
    ALWAN_CORE_T b = ALWAN_CORE_FNVLIT(alwan_hue_to_rgb)(p, q, h - ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(3.0));

    /* Achromatic: when s <= 0, return (l,l,l) */
    result.r = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), l, r);
    result.g = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), l, g);
    result.b = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), l, b);

    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr (value-returning, takes kr/kb directly)
 * The .c wrapper resolves the enum to kr/kb before calling.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_YCBCR ALWAN_CORE_FNVLIT(alwan_rgb_to_ycbcr_kr_kb)(ALWAN_CORE_RGB rgb, ALWAN_CORE_T kr, ALWAN_CORE_T kb) {
    ALWAN_CORE_YCBCR result;

    ALWAN_CORE_T y = kr * rgb.r + (ALWAN_CORE_LITERAL(1.0) - kr - kb) * rgb.g + kb * rgb.b;
    result.Y  = y;
    result.Cb = (rgb.b - y) / (ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_LITERAL(1.0) - kb)) + ALWAN_CORE_LITERAL(0.5);
    result.Cr = (rgb.r - y) / (ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_LITERAL(1.0) - kr)) + ALWAN_CORE_LITERAL(0.5);

    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_ycbcr_to_rgb_kr_kb)(ALWAN_CORE_YCBCR ycbcr, ALWAN_CORE_T kr, ALWAN_CORE_T kb) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T y  = ycbcr.Y;
    ALWAN_CORE_T cb = ycbcr.Cb - ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T cr = ycbcr.Cr - ALWAN_CORE_LITERAL(0.5);
    ALWAN_CORE_T kg = ALWAN_CORE_LITERAL(1.0) - kr - kb;

    ALWAN_CORE_T r = y + cr * ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_LITERAL(1.0) - kr);
    ALWAN_CORE_T b = y + cb * ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_LITERAL(1.0) - kb);
    ALWAN_CORE_T g = (y - kr * r - kb * b) / kg;

    result.r = ALWAN_CORE_CLAMP(r, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    result.g = ALWAN_CORE_CLAMP(g, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    result.b = ALWAN_CORE_CLAMP(b, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    return result;
}

/* ----------------------------------------------------------------
 * Legal range constants for a given bit depth (8, 10, 12, 16).
 * Y range:  [16*s, 235*s] / (2^N-1)
 * Cb/Cr range: [16*s, 240*s] / (2^N-1)
 * where s = 2^(N-8).
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T y_min, y_max, c_min, c_max;
} ALWAN_CORE_FNLIT(alwan_legal_range);

ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_legal_range) ALWAN_CORE_FNLIT(alwan_legal_range_from_bit_depth)(int bit_depth) {
    ALWAN_CORE_FNLIT(alwan_legal_range) r;
    ALWAN_CORE_T max_val, scale;
    switch (bit_depth) {
        case  8: max_val = ALWAN_CORE_LITERAL(255.0);   scale = ALWAN_CORE_LITERAL(1.0);   break;
        case 10: max_val = ALWAN_CORE_LITERAL(1023.0);  scale = ALWAN_CORE_LITERAL(4.0);   break;
        case 12: max_val = ALWAN_CORE_LITERAL(4095.0);  scale = ALWAN_CORE_LITERAL(16.0);  break;
        case 16: max_val = ALWAN_CORE_LITERAL(65535.0); scale = ALWAN_CORE_LITERAL(256.0); break;
        default: max_val = ALWAN_CORE_LITERAL(1023.0);  scale = ALWAN_CORE_LITERAL(4.0);   break;
    }
    r.y_min = ALWAN_CORE_LITERAL(16.0)  * scale / max_val;
    r.y_max = ALWAN_CORE_LITERAL(235.0) * scale / max_val;
    r.c_min = ALWAN_CORE_LITERAL(16.0)  * scale / max_val;
    r.c_max = ALWAN_CORE_LITERAL(240.0) * scale / max_val;
    return r;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc (BT.2020 constant luminance, value-returning)
 * bit_depth: 8, 10, 12, or 16 -- controls legal range scaling
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_YCCBCCRC ALWAN_CORE_FNVLIT(alwan_rgb_to_yccbccrc)(ALWAN_CORE_RGB rgb, int bit_depth) {
    ALWAN_CORE_YCCBCCRC result;

    ALWAN_CORE_T const kr = ALWAN_CORE_LITERAL(0.2627);
    ALWAN_CORE_T const kg = ALWAN_CORE_LITERAL(0.6780);
    ALWAN_CORE_T const kb = ALWAN_CORE_LITERAL(0.0593);

    /* Linear Yc */
    ALWAN_CORE_T yc_linear = kr * rgb.r + kg * rgb.g + kb * rgb.b;

    /* BT.2020 OETF */
    ALWAN_CORE_T const beta = ALWAN_CORE_LITERAL(0.018);
    ALWAN_CORE_T const alpha = ALWAN_CORE_LITERAL(1.099);

    ALWAN_CORE_T yc = ALWAN_CORE_SELECT(yc_linear < beta,
                                   ALWAN_CORE_LITERAL(4.5) * yc_linear,
                                   alpha * ALWAN_CORE_POW(yc_linear, ALWAN_CORE_LITERAL(0.45)) - (alpha - ALWAN_CORE_LITERAL(1.0)));
    ALWAN_CORE_T r_gamma = ALWAN_CORE_SELECT(rgb.r < beta,
                                        ALWAN_CORE_LITERAL(4.5) * rgb.r,
                                        alpha * ALWAN_CORE_POW(rgb.r, ALWAN_CORE_LITERAL(0.45)) - (alpha - ALWAN_CORE_LITERAL(1.0)));
    ALWAN_CORE_T b_gamma = ALWAN_CORE_SELECT(rgb.b < beta,
                                        ALWAN_CORE_LITERAL(4.5) * rgb.b,
                                        alpha * ALWAN_CORE_POW(rgb.b, ALWAN_CORE_LITERAL(0.45)) - (alpha - ALWAN_CORE_LITERAL(1.0)));

    /* Chroma differences */
    ALWAN_CORE_T diff_b = b_gamma - yc;
    ALWAN_CORE_T diff_r = r_gamma - yc;

    ALWAN_CORE_T cbc = ALWAN_CORE_SELECT(diff_b <= ALWAN_CORE_LITERAL(0.0),
                                    diff_b / ALWAN_CORE_LITERAL(1.9404),
                                    diff_b / ALWAN_CORE_LITERAL(1.5816));
    ALWAN_CORE_T crc = ALWAN_CORE_SELECT(diff_r <= ALWAN_CORE_LITERAL(0.0),
                                    diff_r / ALWAN_CORE_LITERAL(1.7184),
                                    diff_r / ALWAN_CORE_LITERAL(0.9936));

    /* Legal range scaling */
    ALWAN_CORE_FNLIT(alwan_legal_range) lr = ALWAN_CORE_FNLIT(alwan_legal_range_from_bit_depth)(bit_depth);

    result.Yc  = yc * (lr.y_max - lr.y_min) + lr.y_min;
    result.Cbc = cbc * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_CORE_LITERAL(2.0);
    result.Crc = crc * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_CORE_LITERAL(2.0);

    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_yccbccrc_to_rgb)(ALWAN_CORE_YCCBCCRC yccbccrc, int bit_depth) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T const kr = ALWAN_CORE_LITERAL(0.2627);
    ALWAN_CORE_T const kg = ALWAN_CORE_LITERAL(0.6780);
    ALWAN_CORE_T const kb = ALWAN_CORE_LITERAL(0.0593);

    /* Reverse legal range scaling */
    ALWAN_CORE_FNLIT(alwan_legal_range) lr = ALWAN_CORE_FNLIT(alwan_legal_range_from_bit_depth)(bit_depth);
    ALWAN_CORE_T c_center = (lr.c_max + lr.c_min) / ALWAN_CORE_LITERAL(2.0);

    ALWAN_CORE_T yc = (yccbccrc.Yc - lr.y_min) / (lr.y_max - lr.y_min);
    ALWAN_CORE_T cbc = (yccbccrc.Cbc - c_center) / (lr.c_max - lr.c_min);
    ALWAN_CORE_T crc = (yccbccrc.Crc - c_center) / (lr.c_max - lr.c_min);

    /* Reverse chroma divisors */
    ALWAN_CORE_T diff_b = ALWAN_CORE_SELECT(cbc <= ALWAN_CORE_LITERAL(0.0),
                                       cbc * ALWAN_CORE_LITERAL(1.9404),
                                       cbc * ALWAN_CORE_LITERAL(1.5816));
    ALWAN_CORE_T diff_r = ALWAN_CORE_SELECT(crc <= ALWAN_CORE_LITERAL(0.0),
                                       crc * ALWAN_CORE_LITERAL(1.7184),
                                       crc * ALWAN_CORE_LITERAL(0.9936));

    ALWAN_CORE_T r_gamma = yc + diff_r;
    ALWAN_CORE_T b_gamma = yc + diff_b;

    /* BT.2020 EOTF */
    ALWAN_CORE_T const beta = ALWAN_CORE_LITERAL(0.018);
    ALWAN_CORE_T const alpha = ALWAN_CORE_LITERAL(1.099);
    ALWAN_CORE_T const threshold = ALWAN_CORE_LITERAL(4.5) * beta;

    ALWAN_CORE_T yc_linear = ALWAN_CORE_SELECT(yc < threshold,
                                          yc / ALWAN_CORE_LITERAL(4.5),
                                          ALWAN_CORE_POW((yc + (alpha - ALWAN_CORE_LITERAL(1.0))) / alpha,
                                                    ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(0.45)));
    ALWAN_CORE_T r = ALWAN_CORE_SELECT(r_gamma < threshold,
                                  r_gamma / ALWAN_CORE_LITERAL(4.5),
                                  ALWAN_CORE_POW((r_gamma + (alpha - ALWAN_CORE_LITERAL(1.0))) / alpha,
                                            ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(0.45)));
    ALWAN_CORE_T b = ALWAN_CORE_SELECT(b_gamma < threshold,
                                  b_gamma / ALWAN_CORE_LITERAL(4.5),
                                  ALWAN_CORE_POW((b_gamma + (alpha - ALWAN_CORE_LITERAL(1.0))) / alpha,
                                            ALWAN_CORE_LITERAL(1.0) / ALWAN_CORE_LITERAL(0.45)));

    ALWAN_CORE_T g = (yc_linear - kr * r - kb * b) / kg;

    result.r = ALWAN_CORE_CLAMP(r, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    result.g = ALWAN_CORE_CLAMP(g, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));
    result.b = ALWAN_CORE_CLAMP(b, ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(1.0));

    return result;
}

/* ----------------------------------------------------------------
 * YCbCr legal <-> full range (luma + chroma with centering)
 * bit_depth: 8, 10, 12, or 16
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_YCBCR ALWAN_CORE_FNVLIT(alwan_ycbcr_full_to_legal)(ALWAN_CORE_YCBCR ycbcr, int bit_depth) {
    ALWAN_CORE_FNLIT(alwan_legal_range) lr = ALWAN_CORE_FNLIT(alwan_legal_range_from_bit_depth)(bit_depth);
    ALWAN_CORE_YCBCR result;
    result.Y  = ycbcr.Y  * (lr.y_max - lr.y_min) + lr.y_min;
    result.Cb = ycbcr.Cb * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_CORE_LITERAL(2.0);
    result.Cr = ycbcr.Cr * (lr.c_max - lr.c_min) + (lr.c_max + lr.c_min) / ALWAN_CORE_LITERAL(2.0);
    return result;
}

ALWAN_INLINE ALWAN_CORE_YCBCR ALWAN_CORE_FNVLIT(alwan_ycbcr_legal_to_full)(ALWAN_CORE_YCBCR ycbcr, int bit_depth) {
    ALWAN_CORE_FNLIT(alwan_legal_range) lr = ALWAN_CORE_FNLIT(alwan_legal_range_from_bit_depth)(bit_depth);
    ALWAN_CORE_T c_center = (lr.c_max + lr.c_min) / ALWAN_CORE_LITERAL(2.0);
    ALWAN_CORE_YCBCR result;
    result.Y  = (ycbcr.Y  - lr.y_min) / (lr.y_max - lr.y_min);
    result.Cb = (ycbcr.Cb - c_center) / (lr.c_max - lr.c_min);
    result.Cr = (ycbcr.Cr - c_center) / (lr.c_max - lr.c_min);
    return result;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK (value-returning)
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_CMYK ALWAN_CORE_FNVLIT(alwan_cmy_to_cmyk)(ALWAN_CORE_CMY cmy) {
    ALWAN_CORE_CMYK result;
    ALWAN_CORE_T k = ALWAN_CORE_MIN3(cmy.c, cmy.m, cmy.y);
    ALWAN_CORE_T denom = ALWAN_CORE_LITERAL(1.0) - k;
    /* When k >= 1, CMY are all 1 -> pure black, c=m=y=0 */
    result.c = ALWAN_CORE_SELECT(k >= ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(0.0), (cmy.c - k) / denom);
    result.m = ALWAN_CORE_SELECT(k >= ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(0.0), (cmy.m - k) / denom);
    result.y = ALWAN_CORE_SELECT(k >= ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(0.0), (cmy.y - k) / denom);
    result.k = k;
    return result;
}

ALWAN_INLINE ALWAN_CORE_CMY ALWAN_CORE_FNVLIT(alwan_cmyk_to_cmy)(ALWAN_CORE_CMYK cmyk) {
    ALWAN_CORE_CMY result;
    ALWAN_CORE_T one_minus_k = ALWAN_CORE_LITERAL(1.0) - cmyk.k;
    result.c = cmyk.c * one_minus_k + cmyk.k;
    result.m = cmyk.m * one_minus_k + cmyk.k;
    result.y = cmyk.y * one_minus_k + cmyk.k;
    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> HWB (Hue, Whiteness, Blackness)
 * CSS Color Level 4 color model
 * H in [0, 1] (0 = 0 degrees, 1 = 360 degrees)
 * W = whiteness [0, 1], B = blackness [0, 1]
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_CORE_T h, w, b;
} ALWAN_CORE_FNLIT(alwan_hwb);

/* HSV -> HWB: W = (1-S)*V, B = 1-V, H = H */
ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_hwb) ALWAN_CORE_FNVLIT(alwan_hsv_to_hwb)(ALWAN_CORE_HSV hsv) {
    ALWAN_CORE_FNLIT(alwan_hwb) result;
    result.h = hsv.h;
    result.w = (ALWAN_CORE_ONE - hsv.s) * hsv.v;
    result.b = ALWAN_CORE_ONE - hsv.v;
    return result;
}

/* HWB -> HSV: V = 1-B, S = 1-W/(1-B), H = H */
ALWAN_INLINE ALWAN_CORE_HSV ALWAN_CORE_FNVLIT(alwan_hwb_to_hsv)(ALWAN_CORE_FNLIT(alwan_hwb) hwb) {
    ALWAN_CORE_HSV result;
    result.h = hwb.h;
    result.v = ALWAN_CORE_ONE - hwb.b;

    /* When W+B >= 1, the color is a shade of gray: S=0, V=W/(W+B) */
    ALWAN_CORE_T wb_sum = hwb.w + hwb.b;
    ALWAN_CORE_T safe_v = ALWAN_CORE_SELECT(result.v < ALWAN_CORE_LITERAL(1e-10),
                                        ALWAN_CORE_LITERAL(1e-10), result.v);
    result.s = ALWAN_CORE_SELECT(wb_sum >= ALWAN_CORE_ONE,
                            ALWAN_CORE_ZERO,
                            ALWAN_CORE_ONE - hwb.w / safe_v);
    result.v = ALWAN_CORE_SELECT(wb_sum >= ALWAN_CORE_ONE,
                            hwb.w / wb_sum,
                            result.v);
    return result;
}

/* RGB -> HWB (via HSV intermediate) */
ALWAN_INLINE ALWAN_CORE_FNLIT(alwan_hwb) ALWAN_CORE_FNVLIT(alwan_rgb_to_hwb)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_HSV hsv = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsv)(rgb);
    return ALWAN_CORE_FNVLIT(alwan_hsv_to_hwb)(hsv);
}

/* HWB -> RGB (via HSV intermediate) */
ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hwb_to_rgb)(ALWAN_CORE_FNLIT(alwan_hwb) hwb) {
    ALWAN_CORE_HSV hsv = ALWAN_CORE_FNVLIT(alwan_hwb_to_hsv)(hwb);
    return ALWAN_CORE_FNVLIT(alwan_hsv_to_rgb)(hsv);
}

/* ----------------------------------------------------------------
 * Relative Luminance (value-returning)
 * Y = kr*R + kg*G + kb*B
 * Input RGB must be linear (scene-referred).
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_relative_luminance)(ALWAN_CORE_RGB rgb,
                                                      ALWAN_CORE_T kr,
                                                      ALWAN_CORE_T kg,
                                                      ALWAN_CORE_T kb) {
    return kr * rgb.r + kg * rgb.g + kb * rgb.b;
}

/* ----------------------------------------------------------------
 * RGB <-> HSP (value-returning)
 * HSP: Hue, Saturation, Perceived brightness
 * Reference: Darel Rex Finley (2006), "HSP Color Model - Alternative to
 * HSV (HSB) and HSL", http://alienryderflex.com/hsp.html
 *
 * P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights.
 * H and S are identical to HSV.
 * Inverse uses per-sector quadratic solve from Finley's algorithm.
 * Operates on encoded (display-referred) RGB in [0,1].
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_HSP ALWAN_CORE_FNVLIT(alwan_rgb_to_hsp)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_HSP result;

    /* H and S are identical to HSV */
    ALWAN_CORE_HSV hsv = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsv)(rgb);
    result.h = hsv.h;
    result.s = hsv.s;

    /* P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) */
    ALWAN_CORE_T const Pr = ALWAN_CORE_LITERAL(0.299);
    ALWAN_CORE_T const Pg = ALWAN_CORE_LITERAL(0.587);
    ALWAN_CORE_T const Pb = ALWAN_CORE_LITERAL(0.114);
    result.p = ALWAN_CORE_SQRT(Pr * rgb.r * rgb.r + Pg * rgb.g * rgb.g + Pb * rgb.b * rgb.b);

    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hsp_to_rgb)(ALWAN_CORE_HSP hsp) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T const Pr = ALWAN_CORE_LITERAL(0.299);
    ALWAN_CORE_T const Pg = ALWAN_CORE_LITERAL(0.587);
    ALWAN_CORE_T const Pb = ALWAN_CORE_LITERAL(0.114);

    ALWAN_CORE_T h = hsp.h * ALWAN_CORE_LITERAL(360.0);
    ALWAN_CORE_T s = hsp.s;
    ALWAN_CORE_T p = hsp.p;

    /* Normalize hue to [0, 360) */
    h = h - ALWAN_CORE_FLOOR(h / ALWAN_CORE_LITERAL(360.0)) * ALWAN_CORE_LITERAL(360.0);
    h = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(0.0), h + ALWAN_CORE_LITERAL(360.0), h);

    /*
     * Finley's HSP inverse algorithm.
     * For each 60-degree sector, one channel is max (v), one is mid (m),
     * one is min (min_c = v*(1-s)). We solve for v from:
     *   P^2 = Pw_max * v^2 + Pw_mid * m^2 + Pw_min * min_c^2
     * where m interpolates linearly between min_c and v based on hue position.
     */
    ALWAN_CORE_T min_part = ALWAN_CORE_LITERAL(1.0) - s;

    /* Achromatic: s <= 0 -> (p, p, p) */
    ALWAN_CORE_T r, g, b;

    /* Sector 0: [0, 60) -> R=max, G=mid, B=min */
    ALWAN_CORE_T f0 = h / ALWAN_CORE_LITERAL(60.0);
    /* mid = min + (max-min)*f = v*(1-s) + v*s*f = v*((1-s) + s*f) */
    ALWAN_CORE_T mid_frac_0 = min_part + s * f0;
    /* P^2 = Pr*v^2 + Pg*(v*mid_frac)^2 + Pb*(v*min_part)^2 */
    /* v = P / sqrt(Pr + Pg*mid_frac^2 + Pb*min_part^2) */
    ALWAN_CORE_T denom_0 = ALWAN_CORE_SQRT(Pr + Pg * mid_frac_0 * mid_frac_0 + Pb * min_part * min_part);
    ALWAN_CORE_T v_0 = ALWAN_CORE_SELECT(denom_0 > ALWAN_CORE_LITERAL(0.0), p / denom_0, ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T r_0 = v_0;
    ALWAN_CORE_T g_0 = v_0 * mid_frac_0;
    ALWAN_CORE_T b_0 = v_0 * min_part;

    /* Sector 1: [60, 120) -> G=max, R=mid, B=min */
    ALWAN_CORE_T f1 = ALWAN_CORE_LITERAL(1.0) - (h - ALWAN_CORE_LITERAL(60.0)) / ALWAN_CORE_LITERAL(60.0);
    ALWAN_CORE_T mid_frac_1 = min_part + s * f1;
    ALWAN_CORE_T denom_1 = ALWAN_CORE_SQRT(Pg + Pr * mid_frac_1 * mid_frac_1 + Pb * min_part * min_part);
    ALWAN_CORE_T v_1 = ALWAN_CORE_SELECT(denom_1 > ALWAN_CORE_LITERAL(0.0), p / denom_1, ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T r_1 = v_1 * mid_frac_1;
    ALWAN_CORE_T g_1 = v_1;
    ALWAN_CORE_T b_1 = v_1 * min_part;

    /* Sector 2: [120, 180) -> G=max, B=mid, R=min */
    ALWAN_CORE_T f2 = (h - ALWAN_CORE_LITERAL(120.0)) / ALWAN_CORE_LITERAL(60.0);
    ALWAN_CORE_T mid_frac_2 = min_part + s * f2;
    ALWAN_CORE_T denom_2 = ALWAN_CORE_SQRT(Pg + Pb * mid_frac_2 * mid_frac_2 + Pr * min_part * min_part);
    ALWAN_CORE_T v_2 = ALWAN_CORE_SELECT(denom_2 > ALWAN_CORE_LITERAL(0.0), p / denom_2, ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T r_2 = v_2 * min_part;
    ALWAN_CORE_T g_2 = v_2;
    ALWAN_CORE_T b_2 = v_2 * mid_frac_2;

    /* Sector 3: [180, 240) -> B=max, G=mid, R=min */
    ALWAN_CORE_T f3 = ALWAN_CORE_LITERAL(1.0) - (h - ALWAN_CORE_LITERAL(180.0)) / ALWAN_CORE_LITERAL(60.0);
    ALWAN_CORE_T mid_frac_3 = min_part + s * f3;
    ALWAN_CORE_T denom_3 = ALWAN_CORE_SQRT(Pb + Pg * mid_frac_3 * mid_frac_3 + Pr * min_part * min_part);
    ALWAN_CORE_T v_3 = ALWAN_CORE_SELECT(denom_3 > ALWAN_CORE_LITERAL(0.0), p / denom_3, ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T r_3 = v_3 * min_part;
    ALWAN_CORE_T g_3 = v_3 * mid_frac_3;
    ALWAN_CORE_T b_3 = v_3;

    /* Sector 4: [240, 300) -> B=max, R=mid, G=min */
    ALWAN_CORE_T f4 = (h - ALWAN_CORE_LITERAL(240.0)) / ALWAN_CORE_LITERAL(60.0);
    ALWAN_CORE_T mid_frac_4 = min_part + s * f4;
    ALWAN_CORE_T denom_4 = ALWAN_CORE_SQRT(Pb + Pr * mid_frac_4 * mid_frac_4 + Pg * min_part * min_part);
    ALWAN_CORE_T v_4 = ALWAN_CORE_SELECT(denom_4 > ALWAN_CORE_LITERAL(0.0), p / denom_4, ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T r_4 = v_4 * mid_frac_4;
    ALWAN_CORE_T g_4 = v_4 * min_part;
    ALWAN_CORE_T b_4 = v_4;

    /* Sector 5: [300, 360) -> R=max, B=mid, G=min */
    ALWAN_CORE_T f5 = ALWAN_CORE_LITERAL(1.0) - (h - ALWAN_CORE_LITERAL(300.0)) / ALWAN_CORE_LITERAL(60.0);
    ALWAN_CORE_T mid_frac_5 = min_part + s * f5;
    ALWAN_CORE_T denom_5 = ALWAN_CORE_SQRT(Pr + Pb * mid_frac_5 * mid_frac_5 + Pg * min_part * min_part);
    ALWAN_CORE_T v_5 = ALWAN_CORE_SELECT(denom_5 > ALWAN_CORE_LITERAL(0.0), p / denom_5, ALWAN_CORE_LITERAL(0.0));
    ALWAN_CORE_T r_5 = v_5;
    ALWAN_CORE_T g_5 = v_5 * min_part;
    ALWAN_CORE_T b_5 = v_5 * mid_frac_5;

    /* Select sector via nested ALWAN_CORE_SELECT (same tree as HSV inverse) */
    ALWAN_CORE_T r_01 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(60.0), r_0, r_1);
    ALWAN_CORE_T g_01 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(60.0), g_0, g_1);
    ALWAN_CORE_T b_01 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(60.0), b_0, b_1);

    ALWAN_CORE_T r_23 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(180.0), r_2, r_3);
    ALWAN_CORE_T g_23 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(180.0), g_2, g_3);
    ALWAN_CORE_T b_23 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(180.0), b_2, b_3);

    ALWAN_CORE_T r_45 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(300.0), r_4, r_5);
    ALWAN_CORE_T g_45 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(300.0), g_4, g_5);
    ALWAN_CORE_T b_45 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(300.0), b_4, b_5);

    ALWAN_CORE_T r_0123 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(120.0), r_01, r_23);
    ALWAN_CORE_T g_0123 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(120.0), g_01, g_23);
    ALWAN_CORE_T b_0123 = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(120.0), b_01, b_23);

    r = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(240.0), r_0123, r_45);
    g = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(240.0), g_0123, g_45);
    b = ALWAN_CORE_SELECT(h < ALWAN_CORE_LITERAL(240.0), b_0123, b_45);

    /* Achromatic override */
    result.r = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), p, r);
    result.g = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), p, g);
    result.b = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), p, b);

    return result;
}

/* ----------------------------------------------------------------
 * RGB <-> HSPLog (value-returning)
 * HSP with logarithmic saturation stretching.
 * Forward: S_log = log10(1 + 9*S), maps [0,1] -> [0,1].
 * Inverse: S = (10^S_log - 1) / 9.
 * Expands low saturation values -- designed for log/flat footage.
 * Inspired by Nobe Color Remap (Time in Pixels) / DaVinci Resolve.
 *
 * NOTE: No published specification exists. See alwan_types.h.
 * ---------------------------------------------------------------- */

ALWAN_INLINE ALWAN_CORE_HSPLOG ALWAN_CORE_FNVLIT(alwan_rgb_to_hsplog)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_HSPLOG result;
    ALWAN_CORE_HSP hsp = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsp)(rgb);
    result.h = hsp.h;
    result.p = hsp.p;

    /* Logarithmic saturation stretch: S_log = log10(1 + 9*S) */
    /* At S=0: log10(1) = 0.  At S=1: log10(10) = 1. */
    result.s = ALWAN_CORE_LOG10(ALWAN_CORE_LITERAL(1.0) + ALWAN_CORE_LITERAL(9.0) * hsp.s);

    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hsplog_to_rgb)(ALWAN_CORE_HSPLOG hsplog) {
    /* Invert log saturation: S = (10^S_log - 1) / 9 */
    ALWAN_CORE_HSP hsp;
    hsp.h = hsplog.h;
    hsp.p = hsplog.p;
    hsp.s = (ALWAN_CORE_POW(ALWAN_CORE_LITERAL(10.0), hsplog.s) - ALWAN_CORE_LITERAL(1.0)) / ALWAN_CORE_LITERAL(9.0);
    return ALWAN_CORE_FNVLIT(alwan_hsp_to_rgb)(hsp);
}

/* ----------------------------------------------------------------
 * RGB <-> HSY (value-returning)
 * HSY: Hue, Saturation, Luma (weighted linear luma)
 * Reference: Kuzma Shapran, "HCY color space" (chilliant.com);
 * Krita KoColorConversions.cpp (KDE, LGPL-2.1+).
 *
 * Y = kr*R + kg*G + kb*B (BT.601 weights).
 * H identical to HSV. S uses luma-aware max_sat remapping.
 * Operates on encoded (display-referred) RGB in [0,1].
 * ---------------------------------------------------------------- */

/*
 * Helper: compute maximum achievable chroma (max_sat) for a given
 * hue, used to normalize HSY saturation. The pure-hue color at a
 * given hue angle has luma = kr*R + kg*G + kb*B where R,G,B are
 * determined by the hue sector (chilliant HCY formulation).
 */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan__hsy_max_chroma)(ALWAN_CORE_T h,
                                                  ALWAN_CORE_T kr,
                                                  ALWAN_CORE_T kg,
                                                  ALWAN_CORE_T kb) {
    /* h in [0,1], convert to [0,6) for sector */
    ALWAN_CORE_T h6 = h * ALWAN_CORE_LITERAL(6.0);
    ALWAN_CORE_T sector = ALWAN_CORE_FLOOR(h6);
    ALWAN_CORE_T f = h6 - sector;

    /* Pure-hue RGB at this hue (same as hue_to_rgb from HSV with S=1,V=1) */
    /* Sector 0: (1, f, 0)   Sector 1: (1-f, 1, 0)
     * Sector 2: (0, 1, f)   Sector 3: (0, 1-f, 1)
     * Sector 4: (f, 0, 1)   Sector 5: (1, 0, 1-f) */
    ALWAN_CORE_T r_01 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0) - f);
    ALWAN_CORE_T g_01 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(1.0), f, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T b_01 = ALWAN_CORE_LITERAL(0.0);

    ALWAN_CORE_T r_23 = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T g_23 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(3.0), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0) - f);
    ALWAN_CORE_T b_23 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(3.0), f, ALWAN_CORE_LITERAL(1.0));

    ALWAN_CORE_T r_45 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(5.0), f, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T g_45 = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T b_45 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(5.0), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0) - f);

    ALWAN_CORE_T r_0123 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(2.0), r_01, r_23);
    ALWAN_CORE_T g_0123 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(2.0), g_01, g_23);
    ALWAN_CORE_T b_0123 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(2.0), b_01, b_23);

    ALWAN_CORE_T pr = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(4.0), r_0123, r_45);
    ALWAN_CORE_T pg = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(4.0), g_0123, g_45);
    ALWAN_CORE_T pb = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(4.0), b_0123, b_45);

    /* Luma of this pure-hue color */
    return kr * pr + kg * pg + kb * pb;
}

ALWAN_INLINE ALWAN_CORE_HSY ALWAN_CORE_FNVLIT(alwan_rgb_to_hsy)(ALWAN_CORE_RGB rgb) {
    ALWAN_CORE_HSY result;

    ALWAN_CORE_T const kr = ALWAN_CORE_LITERAL(0.299);
    ALWAN_CORE_T const kg = ALWAN_CORE_LITERAL(0.587);
    ALWAN_CORE_T const kb = ALWAN_CORE_LITERAL(0.114);

    /* H is identical to HSV */
    ALWAN_CORE_HSV hsv = ALWAN_CORE_FNVLIT(alwan_rgb_to_hsv)(rgb);
    result.h = hsv.h;

    /* Y = weighted luma */
    ALWAN_CORE_T y = kr * rgb.r + kg * rgb.g + kb * rgb.b;
    result.y = y;

    /* Chroma = max - min */
    ALWAN_CORE_T max_val = ALWAN_CORE_MAX3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T min_val = ALWAN_CORE_MIN3(rgb.r, rgb.g, rgb.b);
    ALWAN_CORE_T chroma = max_val - min_val;

    /* Saturation: S = C / max_achievable_chroma_at_this_luma
     * max_chroma = min(Y/Y_pure, (1-Y)/(1-Y_pure)) when chroma > 0
     * (Krita/chilliant formulation) */
    ALWAN_CORE_T y_pure = ALWAN_CORE_FNLIT(alwan__hsy_max_chroma)(result.h, kr, kg, kb);
    ALWAN_CORE_T max_c;

    /* If Y <= Y_pure, the limiting factor is the dark side: max_c = Y / Y_pure
     * If Y > Y_pure, the limiting factor is the bright side: max_c = (1-Y) / (1-Y_pure) */
    ALWAN_CORE_T safe_ypure = ALWAN_CORE_SELECT(y_pure < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), y_pure);
    ALWAN_CORE_T safe_ypure_inv = ALWAN_CORE_SELECT((ALWAN_CORE_LITERAL(1.0) - y_pure) < ALWAN_CORE_LITERAL(1e-10),
                                                ALWAN_CORE_LITERAL(1e-10),
                                                ALWAN_CORE_LITERAL(1.0) - y_pure);
    max_c = ALWAN_CORE_SELECT(y <= y_pure,
                          y / safe_ypure,
                          (ALWAN_CORE_LITERAL(1.0) - y) / safe_ypure_inv);
    max_c = ALWAN_CORE_SELECT(max_c < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), max_c);

    result.s = ALWAN_CORE_SELECT(chroma > ALWAN_CORE_LITERAL(0.0), chroma / max_c, ALWAN_CORE_LITERAL(0.0));

    return result;
}

ALWAN_INLINE ALWAN_CORE_RGB ALWAN_CORE_FNVLIT(alwan_hsy_to_rgb)(ALWAN_CORE_HSY hsy) {
    ALWAN_CORE_RGB result;

    ALWAN_CORE_T const kr = ALWAN_CORE_LITERAL(0.299);
    ALWAN_CORE_T const kg = ALWAN_CORE_LITERAL(0.587);
    ALWAN_CORE_T const kb = ALWAN_CORE_LITERAL(0.114);

    ALWAN_CORE_T h = hsy.h;
    ALWAN_CORE_T s = hsy.s;
    ALWAN_CORE_T y = hsy.y;

    /* Compute the luma of the pure-hue color at this hue */
    ALWAN_CORE_T y_pure = ALWAN_CORE_FNLIT(alwan__hsy_max_chroma)(h, kr, kg, kb);

    /* Max achievable chroma at this luma */
    ALWAN_CORE_T safe_ypure = ALWAN_CORE_SELECT(y_pure < ALWAN_CORE_LITERAL(1e-10), ALWAN_CORE_LITERAL(1e-10), y_pure);
    ALWAN_CORE_T safe_ypure_inv = ALWAN_CORE_SELECT((ALWAN_CORE_LITERAL(1.0) - y_pure) < ALWAN_CORE_LITERAL(1e-10),
                                                ALWAN_CORE_LITERAL(1e-10),
                                                ALWAN_CORE_LITERAL(1.0) - y_pure);
    ALWAN_CORE_T max_c = ALWAN_CORE_SELECT(y <= y_pure,
                                       y / safe_ypure,
                                       (ALWAN_CORE_LITERAL(1.0) - y) / safe_ypure_inv);

    /* Chroma = S * max_c */
    ALWAN_CORE_T chroma = s * max_c;

    /* Reconstruct RGB from H and chroma (same as HCY -> RGB) */
    ALWAN_CORE_T h6 = h * ALWAN_CORE_LITERAL(6.0);
    ALWAN_CORE_T sector = ALWAN_CORE_FLOOR(h6);
    ALWAN_CORE_T f = h6 - sector;

    /* Pure-hue RGB (max channel = 1, mid channel = fractional, min = 0) */
    ALWAN_CORE_T r_01 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0) - f);
    ALWAN_CORE_T g_01 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(1.0), f, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T b_01 = ALWAN_CORE_LITERAL(0.0);

    ALWAN_CORE_T r_23 = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T g_23 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(3.0), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0) - f);
    ALWAN_CORE_T b_23 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(3.0), f, ALWAN_CORE_LITERAL(1.0));

    ALWAN_CORE_T r_45 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(5.0), f, ALWAN_CORE_LITERAL(1.0));
    ALWAN_CORE_T g_45 = ALWAN_CORE_LITERAL(0.0);
    ALWAN_CORE_T b_45 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(5.0), ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0) - f);

    ALWAN_CORE_T r_0123 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(2.0), r_01, r_23);
    ALWAN_CORE_T g_0123 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(2.0), g_01, g_23);
    ALWAN_CORE_T b_0123 = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(2.0), b_01, b_23);

    ALWAN_CORE_T pr = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(4.0), r_0123, r_45);
    ALWAN_CORE_T pg = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(4.0), g_0123, g_45);
    ALWAN_CORE_T pb = ALWAN_CORE_SELECT(sector < ALWAN_CORE_LITERAL(4.0), b_0123, b_45);

    /* Scale pure-hue by chroma: R_hue = chroma * pure_r, etc. */
    ALWAN_CORE_T r_hue = chroma * pr;
    ALWAN_CORE_T g_hue = chroma * pg;
    ALWAN_CORE_T b_hue = chroma * pb;

    /* Offset to match target luma: y = kr*(r_hue+m) + kg*(g_hue+m) + kb*(b_hue+m)
     * y = kr*r_hue + kg*g_hue + kb*b_hue + m
     * m = y - (kr*r_hue + kg*g_hue + kb*b_hue) */
    ALWAN_CORE_T m = y - (kr * r_hue + kg * g_hue + kb * b_hue);

    ALWAN_CORE_T r = r_hue + m;
    ALWAN_CORE_T g = g_hue + m;
    ALWAN_CORE_T b = b_hue + m;

    /* Achromatic: s <= 0 -> (y, y, y) */
    result.r = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), y, r);
    result.g = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), y, g);
    result.b = ALWAN_CORE_SELECT(s <= ALWAN_CORE_LITERAL(0.0), y, b);

    return result;
}

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSV/HSL functions are emitted directly in the
 * wrapper .h file (not here) to avoid macro name collisions with
 * the backward-compat alwan_srgb_oetf/eotf aliases from alwan_core.h.
 * ---------------------------------------------------------------- */
