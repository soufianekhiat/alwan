/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M9: Convenience Color Model Conversions
 * HSV, HSL, CMY, CMYK, YCbCr, YcCbcCrc
 * See alwan_convenience_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_core.h"
#include "../core/alwan_convenience_core.h"

/* ----------------------------------------------------------------
 * RGB <-> HSV
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
 * RGB <-> HSL
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
 * Linear sRGB <-> HSV
 * ---------------------------------------------------------------- */

int alwan_linear_srgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb) {
    if (!rgb || !hsv_out) return ALWAN_E_INVALID;
    *hsv_out = alwan_linear_srgb_to_hsv_v(*rgb);
    return ALWAN_OK;
}

int alwan_hsv_to_linear_srgb(alwan_rgb *rgb_out, alwan_hsv const *hsv) {
    if (!hsv || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_hsv_to_linear_srgb_v(*hsv);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Linear sRGB <-> HSL
 * ---------------------------------------------------------------- */

int alwan_linear_srgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb) {
    if (!rgb || !hsl_out) return ALWAN_E_INVALID;
    *hsl_out = alwan_linear_srgb_to_hsl_v(*rgb);
    return ALWAN_OK;
}

int alwan_hsl_to_linear_srgb(alwan_rgb *rgb_out, alwan_hsl const *hsl) {
    if (!hsl || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_hsl_to_linear_srgb_v(*hsl);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> CMY
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
 * CMY <-> CMYK
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
 * RGB <-> YCbCr
 * The .c wrapper resolves the enum to kr/kb.
 * ---------------------------------------------------------------- */

/* YCbCr coefficients resolved via alwan__get_ycbcr_coeffs() in alwan_internal.h */

int alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr_out, alwan_rgb const *rgb, alwan_ycbcr_standard standard) {
    if (!rgb || !ycbcr_out) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    *ycbcr_out = alwan_rgb_to_ycbcr_kr_kb_v(*rgb, kr, kb);
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb(alwan_rgb *rgb_out, alwan_ycbcr const *ycbcr, alwan_ycbcr_standard standard) {
    if (!ycbcr || !rgb_out) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    *rgb_out = alwan_ycbcr_to_rgb_kr_kb_v(*ycbcr, kr, kb);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc
 * ---------------------------------------------------------------- */

int alwan_rgb_to_yccbccrc(alwan_yccbccrc *yccbccrc_out, alwan_rgb const *rgb, int bit_depth) {
    if (!rgb || !yccbccrc_out) return ALWAN_E_INVALID;
    *yccbccrc_out = alwan_rgb_to_yccbccrc_v(*rgb, bit_depth);
    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb(alwan_rgb *rgb_out, alwan_yccbccrc const *yccbccrc, int bit_depth) {
    if (!yccbccrc || !rgb_out) return ALWAN_E_INVALID;
    *rgb_out = alwan_yccbccrc_to_rgb_v(*yccbccrc, bit_depth);
    return ALWAN_OK;
}

int alwan_ycbcr_full_to_legal(alwan_ycbcr *out, alwan_ycbcr const *in, int bit_depth) {
    if (!in || !out) return ALWAN_E_INVALID;
    *out = alwan_ycbcr_full_to_legal_v(*in, bit_depth);
    return ALWAN_OK;
}

int alwan_ycbcr_legal_to_full(alwan_ycbcr *out, alwan_ycbcr const *in, int bit_depth) {
    if (!in || !out) return ALWAN_E_INVALID;
    *out = alwan_ycbcr_legal_to_full_v(*in, bit_depth);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg
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

/* ----------------------------------------------------------------
 * RGB <-> HWB
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hwb(alwan_scalar *hwb_out, alwan_rgb const *rgb) {
    if (!rgb || !hwb_out) return ALWAN_E_INVALID;
    alwan_hwb result = alwan_rgb_to_hwb_v(*rgb);
    hwb_out[0] = result.h;
    hwb_out[1] = result.w;
    hwb_out[2] = result.b;
    return ALWAN_OK;
}

int alwan_hwb_to_rgb(alwan_rgb *rgb_out, alwan_scalar const *hwb_in) {
    if (!rgb_out || !hwb_in) return ALWAN_E_INVALID;
    alwan_hwb hwb;
    hwb.h = hwb_in[0];
    hwb.w = hwb_in[1];
    hwb.b = hwb_in[2];
    *rgb_out = alwan_hwb_to_rgb_v(hwb);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Relative Luminance
 * ---------------------------------------------------------------- */

int alwan_relative_luminance(alwan_scalar *Y_out,
                             alwan_rgb const *rgb,
                             alwan_luma_standard standard) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_scalar kr, kg, kb;
    alwan__get_luma_coeffs((int)standard, &kr, &kg, &kb);
    *Y_out = alwan_relative_luminance_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_kr_kb(alwan_scalar *Y_out,
                                   alwan_rgb const *rgb,
                                   alwan_scalar kr, alwan_scalar kb) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_scalar kg = ALWAN_LITERAL(1.0) - kr - kb;
    *Y_out = alwan_relative_luminance_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_space(alwan_scalar *Y_out,
                                   alwan_rgb const *rgb,
                                   alwan_rgb_space_desc const *space) {
    if (!rgb || !Y_out || !space) return ALWAN_E_INVALID;
    if (!space->has_matrices) return ALWAN_E_INVALID;
    /* Y row of the RGB-to-XYZ NPM (row-major: m[3], m[4], m[5]) */
    alwan_scalar kr = space->rgb_to_xyz.m[3];
    alwan_scalar kg = space->rgb_to_xyz.m[4];
    alwan_scalar kb = space->rgb_to_xyz.m[5];
    *Y_out = alwan_relative_luminance_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}
