/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M9: Convenience Color Model Conversions
 * HSV, HSL, CMY, CMYK, YCbCr, YcCbcCrc
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * RGB <-> HSV
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb) {
    if (!rgb || !hsv_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar r = rgb->r;
    alwan_scalar g = rgb->g;
    alwan_scalar b = rgb->b;

    alwan_scalar max_val = alwan_max3(r, g, b);
    alwan_scalar min_val = alwan_min3(r, g, b);
    alwan_scalar delta = max_val - min_val;

    /* V (value) */
    alwan_scalar v = max_val;

    /* S (saturation) */
    alwan_scalar s = (max_val > ALWAN_LITERAL(0.0)) ? (delta / max_val) : ALWAN_LITERAL(0.0);

    /* H (hue) */
    alwan_scalar h = ALWAN_LITERAL(0.0);
    if (delta > ALWAN_LITERAL(0.0)) {
        if (max_val == r) {
            h = ALWAN_LITERAL(60.0) * (g - b) / delta;
            if (g < b) h += ALWAN_LITERAL(360.0);
        } else if (max_val == g) {
            h = ALWAN_LITERAL(60.0) * ((b - r) / delta + ALWAN_LITERAL(2.0));
        } else {
            h = ALWAN_LITERAL(60.0) * ((r - g) / delta + ALWAN_LITERAL(4.0));
        }
    }

    /* Normalize H to [0, 1] */
    h /= ALWAN_LITERAL(360.0);

    hsv_out->h = h;
    hsv_out->s = s;
    hsv_out->v = v;

    return ALWAN_OK;
}

int alwan_hsv_to_rgb(alwan_rgb *rgb_out, alwan_hsv const *hsv) {
    if (!hsv || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar h = hsv->h * ALWAN_LITERAL(360.0);  /* Convert to [0, 360] */
    alwan_scalar s = hsv->s;
    alwan_scalar v = hsv->v;

    if (s <= ALWAN_LITERAL(0.0)) {
        /* Achromatic (gray) */
        rgb_out->r = v;
        rgb_out->g = v;
        rgb_out->b = v;
        return ALWAN_OK;
    }

    /* Normalize hue to [0, 360) */
    while (h < ALWAN_LITERAL(0.0)) h += ALWAN_LITERAL(360.0);
    while (h >= ALWAN_LITERAL(360.0)) h -= ALWAN_LITERAL(360.0);

    alwan_scalar h_sector = h / ALWAN_LITERAL(60.0);
    int sector = (int)ALWAN_FLOOR(h_sector);
    alwan_scalar f = h_sector - (alwan_scalar)sector;

    alwan_scalar p = v * (ALWAN_LITERAL(1.0) - s);
    alwan_scalar q = v * (ALWAN_LITERAL(1.0) - s * f);
    alwan_scalar t = v * (ALWAN_LITERAL(1.0) - s * (ALWAN_LITERAL(1.0) - f));

    switch (sector) {
        case 0:  rgb_out->r = v; rgb_out->g = t; rgb_out->b = p; break;
        case 1:  rgb_out->r = q; rgb_out->g = v; rgb_out->b = p; break;
        case 2:  rgb_out->r = p; rgb_out->g = v; rgb_out->b = t; break;
        case 3:  rgb_out->r = p; rgb_out->g = q; rgb_out->b = v; break;
        case 4:  rgb_out->r = t; rgb_out->g = p; rgb_out->b = v; break;
        default: rgb_out->r = v; rgb_out->g = p; rgb_out->b = q; break;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HSL
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb) {
    if (!rgb || !hsl_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar r = rgb->r;
    alwan_scalar g = rgb->g;
    alwan_scalar b = rgb->b;

    alwan_scalar max_val = alwan_max3(r, g, b);
    alwan_scalar min_val = alwan_min3(r, g, b);
    alwan_scalar delta = max_val - min_val;

    /* L (lightness) */
    alwan_scalar l = (max_val + min_val) / ALWAN_LITERAL(2.0);

    /* S (saturation) */
    alwan_scalar s = ALWAN_LITERAL(0.0);
    if (delta > ALWAN_LITERAL(0.0)) {
        if (l < ALWAN_LITERAL(0.5)) {
            s = delta / (max_val + min_val);
        } else {
            s = delta / (ALWAN_LITERAL(2.0) - max_val - min_val);
        }
    }

    /* H (hue) */
    alwan_scalar h = ALWAN_LITERAL(0.0);
    if (delta > ALWAN_LITERAL(0.0)) {
        if (max_val == r) {
            h = ALWAN_LITERAL(60.0) * (g - b) / delta;
            if (g < b) h += ALWAN_LITERAL(360.0);
        } else if (max_val == g) {
            h = ALWAN_LITERAL(60.0) * ((b - r) / delta + ALWAN_LITERAL(2.0));
        } else {
            h = ALWAN_LITERAL(60.0) * ((r - g) / delta + ALWAN_LITERAL(4.0));
        }
    }

    /* Normalize H to [0, 1] */
    h /= ALWAN_LITERAL(360.0);

    hsl_out->h = h;
    hsl_out->s = s;
    hsl_out->l = l;

    return ALWAN_OK;
}

/* Helper for HSL to RGB conversion */
static alwan_scalar hue_to_rgb(alwan_scalar p, alwan_scalar q, alwan_scalar t) {
    if (t < ALWAN_LITERAL(0.0)) t += ALWAN_LITERAL(1.0);
    if (t > ALWAN_LITERAL(1.0)) t -= ALWAN_LITERAL(1.0);
    if (t < ALWAN_LITERAL(1.0) / ALWAN_LITERAL(6.0)) return p + (q - p) * ALWAN_LITERAL(6.0) * t;
    if (t < ALWAN_LITERAL(1.0) / ALWAN_LITERAL(2.0)) return q;
    if (t < ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0)) return p + (q - p) * (ALWAN_LITERAL(2.0) / ALWAN_LITERAL(3.0) - t) * ALWAN_LITERAL(6.0);
    return p;
}

int alwan_hsl_to_rgb(alwan_rgb *rgb_out, alwan_hsl const *hsl) {
    if (!hsl || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar h = hsl->h;
    alwan_scalar s = hsl->s;
    alwan_scalar l = hsl->l;

    if (s <= ALWAN_LITERAL(0.0)) {
        /* Achromatic */
        rgb_out->r = l;
        rgb_out->g = l;
        rgb_out->b = l;
        return ALWAN_OK;
    }

    alwan_scalar q = (l < ALWAN_LITERAL(0.5)) ?
                     l * (ALWAN_LITERAL(1.0) + s) :
                     l + s - l * s;
    alwan_scalar p = ALWAN_LITERAL(2.0) * l - q;

    rgb_out->r = hue_to_rgb(p, q, h + ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    rgb_out->g = hue_to_rgb(p, q, h);
    rgb_out->b = hue_to_rgb(p, q, h - ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> CMY
 * ---------------------------------------------------------------- */

int alwan_rgb_to_cmy(alwan_cmy *cmy_out, alwan_rgb const *rgb) {
    if (!rgb || !cmy_out) {
        return ALWAN_E_INVALID;
    }

    cmy_out->c = ALWAN_LITERAL(1.0) - rgb->r;
    cmy_out->m = ALWAN_LITERAL(1.0) - rgb->g;
    cmy_out->y = ALWAN_LITERAL(1.0) - rgb->b;

    return ALWAN_OK;
}

int alwan_cmy_to_rgb(alwan_rgb *rgb_out, alwan_cmy const *cmy) {
    if (!cmy || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    rgb_out->r = ALWAN_LITERAL(1.0) - cmy->c;
    rgb_out->g = ALWAN_LITERAL(1.0) - cmy->m;
    rgb_out->b = ALWAN_LITERAL(1.0) - cmy->y;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk(alwan_scalar *c, alwan_scalar *m, alwan_scalar *y, alwan_scalar *k, alwan_cmy const *cmy) {
    if (!cmy || !c || !m || !y || !k) {
        return ALWAN_E_INVALID;
    }

    /* K is the minimum of CMY */
    *k = alwan_min3(cmy->c, cmy->m, cmy->y);

    /* If K = 1, then C = M = Y = 0 */
    if (*k >= ALWAN_LITERAL(1.0)) {
        *c = ALWAN_LITERAL(0.0);
        *m = ALWAN_LITERAL(0.0);
        *y = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    /* Otherwise, calculate C, M, Y */
    alwan_scalar denom = ALWAN_LITERAL(1.0) - *k;
    *c = (cmy->c - *k) / denom;
    *m = (cmy->m - *k) / denom;
    *y = (cmy->y - *k) / denom;

    return ALWAN_OK;
}

int alwan_cmyk_to_cmy(alwan_cmy *cmy_out, alwan_scalar c, alwan_scalar m, alwan_scalar y, alwan_scalar k) {
    if (!cmy_out) {
        return ALWAN_E_INVALID;
    }

    cmy_out->c = c * (ALWAN_LITERAL(1.0) - k) + k;
    cmy_out->m = m * (ALWAN_LITERAL(1.0) - k) + k;
    cmy_out->y = y * (ALWAN_LITERAL(1.0) - k) + k;

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr
 * ---------------------------------------------------------------- */

/* YCbCr coefficients for different standards */
static void get_ycbcr_coeffs(alwan_ycbcr_standard standard, alwan_scalar *kr, alwan_scalar *kg, alwan_scalar *kb) {
    switch (standard) {
        case ALWAN_YCBCR_BT601:
            *kr = ALWAN_LITERAL(0.299);
            *kg = ALWAN_LITERAL(0.587);
            *kb = ALWAN_LITERAL(0.114);
            break;
        case ALWAN_YCBCR_BT709:
            *kr = ALWAN_LITERAL(0.2126);
            *kg = ALWAN_LITERAL(0.7152);
            *kb = ALWAN_LITERAL(0.0722);
            break;
        case ALWAN_YCBCR_BT2020:
            *kr = ALWAN_LITERAL(0.2627);
            *kg = ALWAN_LITERAL(0.6780);
            *kb = ALWAN_LITERAL(0.0593);
            break;
        default:
            /* Default to BT.709 */
            *kr = ALWAN_LITERAL(0.2126);
            *kg = ALWAN_LITERAL(0.7152);
            *kb = ALWAN_LITERAL(0.0722);
            break;
    }
}

int alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr_out, alwan_rgb const *rgb, alwan_ycbcr_standard standard) {
    if (!rgb || !ycbcr_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar kr, kg, kb;
    get_ycbcr_coeffs(standard, &kr, &kg, &kb);

    alwan_scalar r = rgb->r;
    alwan_scalar g = rgb->g;
    alwan_scalar b = rgb->b;

    /* Y' = Kr*R + Kg*G + Kb*B */
    alwan_scalar y = kr * r + kg * g + kb * b;

    /* Cb = (B - Y') / (2 * (1 - Kb)) + 0.5 */
    alwan_scalar cb = (b - y) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kb)) + ALWAN_LITERAL(0.5);

    /* Cr = (R - Y') / (2 * (1 - Kr)) + 0.5 */
    alwan_scalar cr = (r - y) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kr)) + ALWAN_LITERAL(0.5);

    ycbcr_out->Y = y;
    ycbcr_out->Cb = cb;
    ycbcr_out->Cr = cr;

    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb(alwan_rgb *rgb_out, alwan_ycbcr const *ycbcr, alwan_ycbcr_standard standard) {
    if (!ycbcr || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar kr, kg, kb;
    get_ycbcr_coeffs(standard, &kr, &kg, &kb);

    alwan_scalar y  = ycbcr->Y;
    alwan_scalar cb = ycbcr->Cb - ALWAN_LITERAL(0.5);
    alwan_scalar cr = ycbcr->Cr - ALWAN_LITERAL(0.5);

    /* R = Y' + Cr * 2 * (1 - Kr) */
    alwan_scalar r = y + cr * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kr);

    /* B = Y' + Cb * 2 * (1 - Kb) */
    alwan_scalar b = y + cb * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kb);

    /* G = (Y' - Kr*R - Kb*B) / Kg */
    alwan_scalar g = (y - kr * r - kb * b) / kg;

    rgb_out->r = alwan_clamp(r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->g = alwan_clamp(g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->b = alwan_clamp(b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc (constant luminance, BT.2020)
 * ---------------------------------------------------------------- */

int alwan_rgb_to_yccbccrc(alwan_yccbccrc *yccbccrc_out, alwan_rgb const *rgb) {
    if (!rgb || !yccbccrc_out) {
        return ALWAN_E_INVALID;
    }

    /* BT.2020 constant luminance coefficients */
    alwan_scalar const kr = ALWAN_LITERAL(0.2627);
    alwan_scalar const kg = ALWAN_LITERAL(0.6780);
    alwan_scalar const kb = ALWAN_LITERAL(0.0593);

    alwan_scalar r = rgb->r;
    alwan_scalar g = rgb->g;
    alwan_scalar b = rgb->b;

    /* Step 1: Compute linear Yc */
    alwan_scalar yc_linear = kr * r + kg * g + kb * b;

    /* Step 2: Apply BT.2020 OETF to Yc, R, and B */
    /* BT.2020 OETF: E' = 4.5 * E (E < 0.018) or 1.099 * E^0.45 - 0.099 */
    alwan_scalar const beta = ALWAN_LITERAL(0.018);
    alwan_scalar const alpha = ALWAN_LITERAL(1.099);

    alwan_scalar yc, r_gamma, b_gamma;

    /* Apply OETF to Yc */
    if (yc_linear < beta) {
        yc = ALWAN_LITERAL(4.5) * yc_linear;
    } else {
        yc = alpha * ALWAN_POW(yc_linear, ALWAN_LITERAL(0.45)) - (alpha - ALWAN_LITERAL(1.0));
    }

    /* Apply OETF to R */
    if (r < beta) {
        r_gamma = ALWAN_LITERAL(4.5) * r;
    } else {
        r_gamma = alpha * ALWAN_POW(r, ALWAN_LITERAL(0.45)) - (alpha - ALWAN_LITERAL(1.0));
    }

    /* Apply OETF to B */
    if (b < beta) {
        b_gamma = ALWAN_LITERAL(4.5) * b;
    } else {
        b_gamma = alpha * ALWAN_POW(b, ALWAN_LITERAL(0.45)) - (alpha - ALWAN_LITERAL(1.0));
    }

    /* Step 3: Compute chroma differences and apply divisors */
    alwan_scalar diff_b = b_gamma - yc;
    alwan_scalar diff_r = r_gamma - yc;

    /* Chroma divisors from ITU-R BT.2020 */
    alwan_scalar cbc, crc;
    if (diff_b <= ALWAN_LITERAL(0.0)) {
        cbc = diff_b / ALWAN_LITERAL(1.9404);
    } else {
        cbc = diff_b / ALWAN_LITERAL(1.5816);
    }

    if (diff_r <= ALWAN_LITERAL(0.0)) {
        crc = diff_r / ALWAN_LITERAL(1.7184);
    } else {
        crc = diff_r / ALWAN_LITERAL(0.9936);
    }

    /* Step 4: Apply legal range scaling (10-bit: Y: 64-940, C: 64-960) */
    alwan_scalar const y_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const y_max = ALWAN_LITERAL(940.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_max = ALWAN_LITERAL(960.0) / ALWAN_LITERAL(1023.0);

    yccbccrc_out->Yc = yc * (y_max - y_min) + y_min;
    yccbccrc_out->Cbc = cbc * (c_max - c_min) + (c_max + c_min) / ALWAN_LITERAL(2.0);
    yccbccrc_out->Crc = crc * (c_max - c_min) + (c_max + c_min) / ALWAN_LITERAL(2.0);

    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb(alwan_rgb *rgb_out, alwan_yccbccrc const *yccbccrc) {
    if (!yccbccrc || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* BT.2020 constant luminance coefficients */
    alwan_scalar const kr = ALWAN_LITERAL(0.2627);
    alwan_scalar const kg = ALWAN_LITERAL(0.6780);
    alwan_scalar const kb = ALWAN_LITERAL(0.0593);

    /* Step 1: Reverse legal range scaling (10-bit: Y: 64-940, C: 64-960) */
    alwan_scalar const y_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const y_max = ALWAN_LITERAL(940.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_min = ALWAN_LITERAL(64.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_max = ALWAN_LITERAL(960.0) / ALWAN_LITERAL(1023.0);
    alwan_scalar const c_center = (c_max + c_min) / ALWAN_LITERAL(2.0);

    alwan_scalar yc = (yccbccrc->Yc - y_min) / (y_max - y_min);
    alwan_scalar cbc = (yccbccrc->Cbc - c_center) / (c_max - c_min);
    alwan_scalar crc = (yccbccrc->Crc - c_center) / (c_max - c_min);

    /* Step 2: Reverse chroma divisors */
    alwan_scalar diff_b, diff_r;
    if (cbc <= ALWAN_LITERAL(0.0)) {
        diff_b = cbc * ALWAN_LITERAL(1.9404);
    } else {
        diff_b = cbc * ALWAN_LITERAL(1.5816);
    }

    if (crc <= ALWAN_LITERAL(0.0)) {
        diff_r = crc * ALWAN_LITERAL(1.7184);
    } else {
        diff_r = crc * ALWAN_LITERAL(0.9936);
    }

    /* Step 3: Reconstruct gamma-encoded R and B */
    alwan_scalar r_gamma = yc + diff_r;
    alwan_scalar b_gamma = yc + diff_b;

    /* Step 4: Apply BT.2020 EOTF to convert back to linear */
    alwan_scalar const beta = ALWAN_LITERAL(0.018);
    alwan_scalar const alpha = ALWAN_LITERAL(1.099);
    alwan_scalar const threshold = ALWAN_LITERAL(4.5) * beta;  /* 0.081 */

    alwan_scalar yc_linear, r, b;

    /* Apply EOTF to Yc */
    if (yc < threshold) {
        yc_linear = yc / ALWAN_LITERAL(4.5);
    } else {
        yc_linear = ALWAN_POW((yc + (alpha - ALWAN_LITERAL(1.0))) / alpha,
                              ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.45));
    }

    /* Apply EOTF to R */
    if (r_gamma < threshold) {
        r = r_gamma / ALWAN_LITERAL(4.5);
    } else {
        r = ALWAN_POW((r_gamma + (alpha - ALWAN_LITERAL(1.0))) / alpha,
                      ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.45));
    }

    /* Apply EOTF to B */
    if (b_gamma < threshold) {
        b = b_gamma / ALWAN_LITERAL(4.5);
    } else {
        b = ALWAN_POW((b_gamma + (alpha - ALWAN_LITERAL(1.0))) / alpha,
                      ALWAN_LITERAL(1.0) / ALWAN_LITERAL(0.45));
    }

    /* Step 5: Reconstruct G from linear Yc = Kr*R + Kg*G + Kb*B */
    alwan_scalar g = (yc_linear - kr * r - kb * b) / kg;

    /* Clamp to valid range - constant luminance can produce out-of-gamut values */
    rgb_out->r = alwan_clamp(r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->g = alwan_clamp(g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->b = alwan_clamp(b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg (video compression color space)
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ycocg(alwan_ycocg *ycocg_out, alwan_rgb const *rgb) {
    if (!rgb || !ycocg_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar r = rgb->r;
    alwan_scalar g = rgb->g;
    alwan_scalar b = rgb->b;

    /* YCoCg transform (H.264/AVC)
     * Y  =  0.25*R + 0.5*G + 0.25*B
     * Co =  0.5*R + 0*G - 0.5*B
     * Cg = -0.25*R + 0.5*G - 0.25*B
     */
    alwan_scalar y  = ALWAN_LITERAL(0.25) * r + ALWAN_LITERAL(0.5) * g + ALWAN_LITERAL(0.25) * b;
    alwan_scalar co = ALWAN_LITERAL(0.5) * r - ALWAN_LITERAL(0.5) * b;
    alwan_scalar cg = -ALWAN_LITERAL(0.25) * r + ALWAN_LITERAL(0.5) * g - ALWAN_LITERAL(0.25) * b;

    ycocg_out->Y = y;
    ycocg_out->Co = co;
    ycocg_out->Cg = cg;

    return ALWAN_OK;
}

int alwan_ycocg_to_rgb(alwan_rgb *rgb_out, alwan_ycocg const *ycocg) {
    if (!ycocg || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar y  = ycocg->Y;
    alwan_scalar co = ycocg->Co;
    alwan_scalar cg = ycocg->Cg;

    /* Inverse YCoCg transform
     * temp = Y - Cg
     * R = temp + Co
     * G = Y + Cg
     * B = temp - Co
     */
    alwan_scalar temp = y - cg;
    alwan_scalar r = temp + co;
    alwan_scalar g = y + cg;
    alwan_scalar b = temp - co;

    rgb_out->r = r;
    rgb_out->g = g;
    rgb_out->b = b;

    return ALWAN_OK;
}
