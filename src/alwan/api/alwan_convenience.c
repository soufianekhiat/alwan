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

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_convenience_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_convenience_impl.inc"
#include "alwan_api_teardown.h"

/* ----------------------------------------------------------------
 * CMY <-> CMYK
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk(alwan_f64 *c, alwan_f64 *m, alwan_f64 *y, alwan_f64 *k, alwan_cmy const *cmy) {
    if (!cmy || !c || !m || !y || !k) {
        return ALWAN_E_INVALID;
    }
    alwan_cmyk result = alwan_cmy_to_cmyk_f64_v(*cmy);
    *c = result.c;
    *m = result.m;
    *y = result.y;
    *k = result.k;
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy(alwan_cmy *cmy_out, alwan_f64 c, alwan_f64 m, alwan_f64 y, alwan_f64 k) {
    if (!cmy_out) {
        return ALWAN_E_INVALID;
    }
    alwan_cmyk cmyk;
    cmyk.c = c; cmyk.m = m; cmyk.y = y; cmyk.k = k;
    *cmy_out = alwan_cmyk_to_cmy_f64_v(cmyk);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr
 * The .c wrapper resolves the enum to kr/kb.
 * ---------------------------------------------------------------- */

/* YCbCr coefficients resolved via alwan__get_ycbcr_coeffs() in alwan_internal.h */

int alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr_out, alwan_rgb const *rgb, alwan_ycbcr_standard standard) {
    if (!rgb || !ycbcr_out) return ALWAN_E_INVALID;
    alwan_f64 kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    *ycbcr_out = alwan_rgb_to_ycbcr_kr_kb_f64_v(*rgb, kr, kb);
    ALWAN_NORM_YCBCR(ycbcr_out);
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb(alwan_rgb *rgb_out, alwan_ycbcr const *ycbcr, alwan_ycbcr_standard standard) {
    if (!ycbcr || !rgb_out) return ALWAN_E_INVALID;
    alwan_ycbcr tmp = *ycbcr;
    ALWAN_DENORM_YCBCR(&tmp);
    alwan_f64 kr, kb;
    alwan__get_ycbcr_coeffs(standard, &kr, &kb);
    *rgb_out = alwan_ycbcr_to_rgb_kr_kb_f64_v(tmp, kr, kb);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HWB
 * ---------------------------------------------------------------- */

void alwan_rgb_to_hwb_f32(float *hwb_out, alwan_rgb_f32 const *rgb) {
    if (!rgb || !hwb_out) return;
    alwan_hwb_f32 result = alwan_rgb_to_hwb_f32_v(*rgb);
    hwb_out[0] = result.h;
    hwb_out[1] = result.w;
    hwb_out[2] = result.b;
}

void alwan_rgb_to_hwb_f64(double *hwb_out, alwan_rgb_f64 const *rgb) {
    if (!rgb || !hwb_out) return;
    alwan_hwb_f64 result = alwan_rgb_to_hwb_f64_v(*rgb);
    hwb_out[0] = result.h;
    hwb_out[1] = result.w;
    hwb_out[2] = result.b;
}

void alwan_hwb_to_rgb_f32(alwan_rgb_f32 *rgb_out, float const *hwb_in) {
    if (!rgb_out || !hwb_in) return;
    alwan_hwb_f32 hwb;
    hwb.h = hwb_in[0];
    hwb.w = hwb_in[1];
    hwb.b = hwb_in[2];
    *rgb_out = alwan_hwb_to_rgb_f32_v(hwb);
}

void alwan_hwb_to_rgb_f64(alwan_rgb_f64 *rgb_out, double const *hwb_in) {
    if (!rgb_out || !hwb_in) return;
    alwan_hwb_f64 hwb;
    hwb.h = hwb_in[0];
    hwb.w = hwb_in[1];
    hwb.b = hwb_in[2];
    *rgb_out = alwan_hwb_to_rgb_f64_v(hwb);
}

/* ----------------------------------------------------------------
 * Relative Luminance
 * ---------------------------------------------------------------- */

int alwan_relative_luminance(alwan_f64 *Y_out,
                             alwan_rgb const *rgb,
                             alwan_luma_standard standard) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_f64 kr, kg, kb;
    alwan__get_luma_coeffs((int)standard, &kr, &kg, &kb);
    *Y_out = alwan_relative_luminance_f64_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_kr_kb(alwan_f64 *Y_out,
                                   alwan_rgb const *rgb,
                                   alwan_f64 kr, alwan_f64 kb) {
    if (!rgb || !Y_out) return ALWAN_E_INVALID;
    alwan_f64 kg = ALWAN_LITERAL(1.0) - kr - kb;
    *Y_out = alwan_relative_luminance_f64_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}

int alwan_relative_luminance_space(alwan_f64 *Y_out,
                                   alwan_rgb const *rgb,
                                   alwan_rgb_space_desc const *space) {
    if (!rgb || !Y_out || !space) return ALWAN_E_INVALID;
    if (!space->has_matrices) return ALWAN_E_INVALID;
    /* Y row of the RGB-to-XYZ NPM (row-major: m[3], m[4], m[5]) */
    alwan_f64 kr = space->rgb_to_xyz.m[3];
    alwan_f64 kg = space->rgb_to_xyz.m[4];
    alwan_f64 kb = space->rgb_to_xyz.m[5];
    *Y_out = alwan_relative_luminance_f64_v(*rgb, kr, kg, kb);
    return ALWAN_OK;
}
