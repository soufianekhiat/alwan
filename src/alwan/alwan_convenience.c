/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M9: Convenience Color Model Conversions
 * HSV, HSL, CMY, CMYK, YCbCr, YcCbcCrc
 * Thin wrappers — core logic in alwan_convenience_core.h
 */

#include "alwan.h"
#include "alwan_internal.h"
#include "alwan_convenience_core.h"

/* ----------------------------------------------------------------
 * RGB <-> HSV — delegated to alwan_convenience_core.h
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb) {
    if (!rgb || !hsv_out) return ALWAN_E_INVALID;
    *hsv_out = alwan_rgb_to_hsv_v(*rgb);
    return ALWAN_OK;
}

int alwan_hsv_to_rgb(alwan_rgb *rgb_out, alwan_hsv const *hsv) {
    if (!hsv || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_hsv_to_rgb_v(*hsv);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HSL — delegated to alwan_convenience_core.h
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb) {
    if (!rgb || !hsl_out) return ALWAN_E_INVALID;
    *hsl_out = alwan_rgb_to_hsl_v(*rgb);
    return ALWAN_OK;
}

int alwan_hsl_to_rgb(alwan_rgb *rgb_out, alwan_hsl const *hsl) {
    if (!hsl || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_hsl_to_rgb_v(*hsl);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> CMY — delegated to alwan_convenience_core.h
 * ---------------------------------------------------------------- */

int alwan_rgb_to_cmy(alwan_cmy *cmy_out, alwan_rgb const *rgb) {
    if (!rgb || !cmy_out) return ALWAN_E_INVALID;
    *cmy_out = alwan_rgb_to_cmy_v(*rgb);
    return ALWAN_OK;
}

int alwan_cmy_to_rgb(alwan_rgb *rgb_out, alwan_cmy const *cmy) {
    if (!cmy || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_cmy_to_rgb_v(*cmy);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK — delegated to alwan_convenience_core.h
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk(alwan_scalar *c, alwan_scalar *m, alwan_scalar *y, alwan_scalar *k, alwan_cmy const *cmy) {
    if (!cmy || !c || !m || !y || !k) {
        return ALWAN_E_INVALID;
    }
    alwan_cmyk result = alwan_cmy_to_cmyk_v(*cmy);
    *c = result.c;
    *m = result.m;
    *y = result.y;
    *k = result.k;
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy(alwan_cmy *cmy_out, alwan_scalar c, alwan_scalar m, alwan_scalar y, alwan_scalar k) {
    if (!cmy_out) {
        return ALWAN_E_INVALID;
    }
    alwan_cmyk cmyk;
    cmyk.c = c; cmyk.m = m; cmyk.y = y; cmyk.k = k;
    *cmy_out = alwan_cmyk_to_cmy_v(cmyk);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr — delegated to alwan_convenience_core.h
 * The .c wrapper resolves the enum to kr/kb.
 * ---------------------------------------------------------------- */

static void get_ycbcr_coeffs(alwan_ycbcr_standard standard, alwan_scalar *kr, alwan_scalar *kb) {
    switch (standard) {
        case ALWAN_YCBCR_BT601:
            *kr = ALWAN_LITERAL(0.299);
            *kb = ALWAN_LITERAL(0.114);
            break;
        case ALWAN_YCBCR_BT709:
            *kr = ALWAN_LITERAL(0.2126);
            *kb = ALWAN_LITERAL(0.0722);
            break;
        case ALWAN_YCBCR_BT2020:
            *kr = ALWAN_LITERAL(0.2627);
            *kb = ALWAN_LITERAL(0.0593);
            break;
        default:
            *kr = ALWAN_LITERAL(0.2126);
            *kb = ALWAN_LITERAL(0.0722);
            break;
    }
}

int alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr_out, alwan_rgb const *rgb, alwan_ycbcr_standard standard) {
    if (!rgb || !ycbcr_out) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    get_ycbcr_coeffs(standard, &kr, &kb);
    *ycbcr_out = alwan_rgb_to_ycbcr_kr_kb_v(*rgb, kr, kb);
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb(alwan_rgb *rgb_out, alwan_ycbcr const *ycbcr, alwan_ycbcr_standard standard) {
    if (!ycbcr || !rgb_out) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    get_ycbcr_coeffs(standard, &kr, &kb);
    *rgb_out = alwan_ycbcr_to_rgb_kr_kb_v(*ycbcr, kr, kb);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc — delegated to alwan_convenience_core.h
 * ---------------------------------------------------------------- */

int alwan_rgb_to_yccbccrc(alwan_yccbccrc *yccbccrc_out, alwan_rgb const *rgb) {
    if (!rgb || !yccbccrc_out) return ALWAN_E_INVALID;
    *yccbccrc_out = alwan_rgb_to_yccbccrc_v(*rgb);
    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb(alwan_rgb *rgb_out, alwan_yccbccrc const *yccbccrc) {
    if (!yccbccrc || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_yccbccrc_to_rgb_v(*yccbccrc);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg — delegated to alwan_convenience_core.h
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ycocg(alwan_ycocg *ycocg_out, alwan_rgb const *rgb) {
    if (!rgb || !ycocg_out) return ALWAN_E_INVALID;
    *ycocg_out = alwan_rgb_to_ycocg_v(*rgb);
    return ALWAN_OK;
}

int alwan_ycocg_to_rgb(alwan_rgb *rgb_out, alwan_ycocg const *ycocg) {
    if (!ycocg || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_ycocg_to_rgb_v(*ycocg);
    return ALWAN_OK;
}
