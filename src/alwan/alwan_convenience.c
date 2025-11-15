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

int alwan_rgb_to_hsv(alwan_vec3 const *rgb, alwan_vec3 *hsv_out) {
    if (!rgb || !hsv_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

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

    hsv_out->v[0] = h;
    hsv_out->v[1] = s;
    hsv_out->v[2] = v;

    return ALWAN_OK;
}

int alwan_hsv_to_rgb(alwan_vec3 const *hsv, alwan_vec3 *rgb_out) {
    if (!hsv || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar h = hsv->v[0] * ALWAN_LITERAL(360.0);  /* Convert to [0, 360] */
    alwan_scalar s = hsv->v[1];
    alwan_scalar v = hsv->v[2];

    if (s <= ALWAN_LITERAL(0.0)) {
        /* Achromatic (gray) */
        rgb_out->v[0] = v;
        rgb_out->v[1] = v;
        rgb_out->v[2] = v;
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
        case 0:  rgb_out->v[0] = v; rgb_out->v[1] = t; rgb_out->v[2] = p; break;
        case 1:  rgb_out->v[0] = q; rgb_out->v[1] = v; rgb_out->v[2] = p; break;
        case 2:  rgb_out->v[0] = p; rgb_out->v[1] = v; rgb_out->v[2] = t; break;
        case 3:  rgb_out->v[0] = p; rgb_out->v[1] = q; rgb_out->v[2] = v; break;
        case 4:  rgb_out->v[0] = t; rgb_out->v[1] = p; rgb_out->v[2] = v; break;
        default: rgb_out->v[0] = v; rgb_out->v[1] = p; rgb_out->v[2] = q; break;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HSL
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsl(alwan_vec3 const *rgb, alwan_vec3 *hsl_out) {
    if (!rgb || !hsl_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

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

    hsl_out->v[0] = h;
    hsl_out->v[1] = s;
    hsl_out->v[2] = l;

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

int alwan_hsl_to_rgb(alwan_vec3 const *hsl, alwan_vec3 *rgb_out) {
    if (!hsl || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar h = hsl->v[0];
    alwan_scalar s = hsl->v[1];
    alwan_scalar l = hsl->v[2];

    if (s <= ALWAN_LITERAL(0.0)) {
        /* Achromatic */
        rgb_out->v[0] = l;
        rgb_out->v[1] = l;
        rgb_out->v[2] = l;
        return ALWAN_OK;
    }

    alwan_scalar q = (l < ALWAN_LITERAL(0.5)) ?
                     l * (ALWAN_LITERAL(1.0) + s) :
                     l + s - l * s;
    alwan_scalar p = ALWAN_LITERAL(2.0) * l - q;

    rgb_out->v[0] = hue_to_rgb(p, q, h + ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));
    rgb_out->v[1] = hue_to_rgb(p, q, h);
    rgb_out->v[2] = hue_to_rgb(p, q, h - ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> CMY
 * ---------------------------------------------------------------- */

int alwan_rgb_to_cmy(alwan_vec3 const *rgb, alwan_vec3 *cmy_out) {
    if (!rgb || !cmy_out) {
        return ALWAN_E_INVALID;
    }

    cmy_out->v[0] = ALWAN_LITERAL(1.0) - rgb->v[0];
    cmy_out->v[1] = ALWAN_LITERAL(1.0) - rgb->v[1];
    cmy_out->v[2] = ALWAN_LITERAL(1.0) - rgb->v[2];

    return ALWAN_OK;
}

int alwan_cmy_to_rgb(alwan_vec3 const *cmy, alwan_vec3 *rgb_out) {
    if (!cmy || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    rgb_out->v[0] = ALWAN_LITERAL(1.0) - cmy->v[0];
    rgb_out->v[1] = ALWAN_LITERAL(1.0) - cmy->v[1];
    rgb_out->v[2] = ALWAN_LITERAL(1.0) - cmy->v[2];

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk(alwan_vec3 const *cmy, alwan_scalar *c, alwan_scalar *m, alwan_scalar *y, alwan_scalar *k) {
    if (!cmy || !c || !m || !y || !k) {
        return ALWAN_E_INVALID;
    }

    /* K is the minimum of CMY */
    *k = alwan_min3(cmy->v[0], cmy->v[1], cmy->v[2]);

    /* If K = 1, then C = M = Y = 0 */
    if (*k >= ALWAN_LITERAL(1.0)) {
        *c = ALWAN_LITERAL(0.0);
        *m = ALWAN_LITERAL(0.0);
        *y = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    /* Otherwise, calculate C, M, Y */
    alwan_scalar denom = ALWAN_LITERAL(1.0) - *k;
    *c = (cmy->v[0] - *k) / denom;
    *m = (cmy->v[1] - *k) / denom;
    *y = (cmy->v[2] - *k) / denom;

    return ALWAN_OK;
}

int alwan_cmyk_to_cmy(alwan_scalar c, alwan_scalar m, alwan_scalar y, alwan_scalar k, alwan_vec3 *cmy_out) {
    if (!cmy_out) {
        return ALWAN_E_INVALID;
    }

    cmy_out->v[0] = c * (ALWAN_LITERAL(1.0) - k) + k;
    cmy_out->v[1] = m * (ALWAN_LITERAL(1.0) - k) + k;
    cmy_out->v[2] = y * (ALWAN_LITERAL(1.0) - k) + k;

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

int alwan_rgb_to_ycbcr(alwan_vec3 const *rgb, alwan_ycbcr_standard standard, alwan_vec3 *ycbcr_out) {
    if (!rgb || !ycbcr_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar kr, kg, kb;
    get_ycbcr_coeffs(standard, &kr, &kg, &kb);

    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

    /* Y' = Kr*R + Kg*G + Kb*B */
    alwan_scalar y = kr * r + kg * g + kb * b;

    /* Cb = (B - Y') / (2 * (1 - Kb)) + 0.5 */
    alwan_scalar cb = (b - y) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kb)) + ALWAN_LITERAL(0.5);

    /* Cr = (R - Y') / (2 * (1 - Kr)) + 0.5 */
    alwan_scalar cr = (r - y) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kr)) + ALWAN_LITERAL(0.5);

    ycbcr_out->v[0] = y;
    ycbcr_out->v[1] = cb;
    ycbcr_out->v[2] = cr;

    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb(alwan_vec3 const *ycbcr, alwan_ycbcr_standard standard, alwan_vec3 *rgb_out) {
    if (!ycbcr || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_scalar kr, kg, kb;
    get_ycbcr_coeffs(standard, &kr, &kg, &kb);

    alwan_scalar y  = ycbcr->v[0];
    alwan_scalar cb = ycbcr->v[1] - ALWAN_LITERAL(0.5);
    alwan_scalar cr = ycbcr->v[2] - ALWAN_LITERAL(0.5);

    /* R = Y' + Cr * 2 * (1 - Kr) */
    alwan_scalar r = y + cr * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kr);

    /* B = Y' + Cb * 2 * (1 - Kb) */
    alwan_scalar b = y + cb * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - kb);

    /* G = (Y' - Kr*R - Kb*B) / Kg */
    alwan_scalar g = (y - kr * r - kb * b) / kg;

    rgb_out->v[0] = alwan_clamp(r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->v[1] = alwan_clamp(g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->v[2] = alwan_clamp(b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc (constant luminance, BT.2020)
 * ---------------------------------------------------------------- */

int alwan_rgb_to_yccbccrc(alwan_vec3 const *rgb, alwan_vec3 *yccbccrc_out) {
    if (!rgb || !yccbccrc_out) {
        return ALWAN_E_INVALID;
    }

    /* BT.2020 constant luminance coefficients */
    alwan_scalar const kr = ALWAN_LITERAL(0.2627);
    alwan_scalar const kg = ALWAN_LITERAL(0.6780);
    alwan_scalar const kb = ALWAN_LITERAL(0.0593);

    alwan_scalar r = rgb->v[0];
    alwan_scalar g = rgb->v[1];
    alwan_scalar b = rgb->v[2];

    /* Yc = Kr*R + Kg*G + Kb*B */
    alwan_scalar yc = kr * r + kg * g + kb * b;

    /* Cbc and Crc use different formulas depending on whether B > Yc or R > Yc */
    /* Handle edge cases (black/white) to avoid division by zero */
    /* Use tolerance for white/black checks to handle f32 precision issues */
    alwan_scalar const edge_threshold = ALWAN_LITERAL(0.001);
    alwan_scalar cbc, crc;

    if (b <= edge_threshold || yc <= edge_threshold) {
        cbc = ALWAN_LITERAL(0.5);
    } else if (yc >= (ALWAN_LITERAL(1.0) - edge_threshold)) {
        cbc = ALWAN_LITERAL(0.5);  /* White: no chroma */
    } else if (b < yc) {
        cbc = (b - yc) / (ALWAN_LITERAL(2.0) * yc * (ALWAN_LITERAL(1.0) - kb)) + ALWAN_LITERAL(0.5);
    } else {
        cbc = (b - yc) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - yc) * (ALWAN_LITERAL(1.0) - kb)) + ALWAN_LITERAL(0.5);
    }

    if (r <= edge_threshold || yc <= edge_threshold) {
        crc = ALWAN_LITERAL(0.5);
    } else if (yc >= (ALWAN_LITERAL(1.0) - edge_threshold)) {
        crc = ALWAN_LITERAL(0.5);  /* White: no chroma */
    } else if (r < yc) {
        crc = (r - yc) / (ALWAN_LITERAL(2.0) * yc * (ALWAN_LITERAL(1.0) - kr)) + ALWAN_LITERAL(0.5);
    } else {
        crc = (r - yc) / (ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - yc) * (ALWAN_LITERAL(1.0) - kr)) + ALWAN_LITERAL(0.5);
    }

    yccbccrc_out->v[0] = yc;
    yccbccrc_out->v[1] = cbc;
    yccbccrc_out->v[2] = crc;

    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb(alwan_vec3 const *yccbccrc, alwan_vec3 *rgb_out) {
    if (!yccbccrc || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* BT.2020 constant luminance coefficients */
    alwan_scalar const kr = ALWAN_LITERAL(0.2627);
    alwan_scalar const kg = ALWAN_LITERAL(0.6780);
    alwan_scalar const kb = ALWAN_LITERAL(0.0593);

    alwan_scalar yc  = yccbccrc->v[0];
    alwan_scalar cbc = yccbccrc->v[1] - ALWAN_LITERAL(0.5);
    alwan_scalar crc = yccbccrc->v[2] - ALWAN_LITERAL(0.5);

    /* Reconstruct R and B using standard formulas */
    alwan_scalar r, b;

    if (crc <= ALWAN_LITERAL(0.0)) {
        r = yc + crc * ALWAN_LITERAL(2.0) * yc * (ALWAN_LITERAL(1.0) - kr);
    } else {
        r = yc + crc * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - yc) * (ALWAN_LITERAL(1.0) - kr);
    }

    if (cbc <= ALWAN_LITERAL(0.0)) {
        b = yc + cbc * ALWAN_LITERAL(2.0) * yc * (ALWAN_LITERAL(1.0) - kb);
    } else {
        b = yc + cbc * ALWAN_LITERAL(2.0) * (ALWAN_LITERAL(1.0) - yc) * (ALWAN_LITERAL(1.0) - kb);
    }

    /* G = (Yc - Kr*R - Kb*B) / Kg */
    alwan_scalar g = (yc - kr * r - kb * b) / kg;

    /* Clamp to valid range - constant luminance can produce out-of-gamut values */
    rgb_out->v[0] = alwan_clamp(r, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->v[1] = alwan_clamp(g, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));
    rgb_out->v[2] = alwan_clamp(b, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

    return ALWAN_OK;
}
