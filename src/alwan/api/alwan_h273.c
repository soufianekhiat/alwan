/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ITU-T H.273 (ISO/IEC 23091-2) code points: colour_primaries,
 * transfer_characteristics and matrix_coefficients, the colour signalling of video
 * streams and containers, to alwan's enums and back. The chromaticities and
 * coefficients are H.273's tables as published. The getters that return numbers are
 * templated in alwan_h273_impl.inc and instantiated once per precision.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stddef.h>
#include <string.h>

#define ALWAN__H273_UNSPECIFIED 2

/* colour_primaries: xy of R, G and B, then the white point, and the alwan space with
 * those chromaticities, the linear one where alwan has it. 6 and 7 share their
 * chromaticities. */
typedef struct {
    int code;
    double xy[8];
    alwan_rgb_space space;
} alwan__h273_primaries;

static alwan__h273_primaries const g_h273_primaries[] = {
    {  1, { 0.640, 0.330, 0.300, 0.600, 0.150, 0.060, 0.3127, 0.3290 }, ALWAN_RGB_SPACE_LINEAR_REC709 },
    {  4, { 0.67, 0.33, 0.21, 0.71, 0.14, 0.08, 0.310, 0.316 }, ALWAN_RGB_SPACE_LINEAR_BT470_525 },
    {  5, { 0.64, 0.33, 0.29, 0.60, 0.15, 0.06, 0.3127, 0.3290 }, ALWAN_RGB_SPACE_LINEAR_BT470_625 },
    {  6, { 0.630, 0.340, 0.310, 0.595, 0.155, 0.070, 0.3127, 0.3290 }, ALWAN_RGB_SPACE_LINEAR_SMPTE_240M },
    {  7, { 0.630, 0.340, 0.310, 0.595, 0.155, 0.070, 0.3127, 0.3290 }, ALWAN_RGB_SPACE_LINEAR_SMPTE_240M },
    {  8, { 0.681, 0.319, 0.243, 0.692, 0.145, 0.049, 0.310, 0.316 }, ALWAN_RGB_SPACE_ITU_T_H273_GENERIC_FILM },
    {  9, { 0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290 }, ALWAN_RGB_SPACE_LINEAR_REC2020 },
    { 10, { 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0 / 3.0, 1.0 / 3.0 }, ALWAN_RGB_SPACE_DCDM_XYZ },
    { 11, { 0.680, 0.320, 0.265, 0.690, 0.150, 0.060, 0.314, 0.351 }, ALWAN_RGB_SPACE_LINEAR_DCI_P3 },
    { 12, { 0.680, 0.320, 0.265, 0.690, 0.150, 0.060, 0.3127, 0.3290 }, ALWAN_RGB_SPACE_LINEAR_P3_D65 },
    { 22, { 0.630, 0.340, 0.295, 0.605, 0.155, 0.077, 0.3127, 0.3290 }, ALWAN_RGB_SPACE_ITU_T_H273_22_UNSPECIFIED },
};

/* transfer_characteristics. 1 and 6 are the same curve, as are 14 and BT.709's. */
typedef struct {
    int code;
    alwan_transfer_function tf;
} alwan__h273_transfer;

static alwan__h273_transfer const g_h273_transfer[] = {
    {  1, ALWAN_TF_BT709 },         {  4, ALWAN_TF_GAMMA22 },       {  5, ALWAN_TF_GAMMA28 },
    {  6, ALWAN_TF_BT709 },         {  7, ALWAN_TF_SMPTE240M },     {  8, ALWAN_TF_LINEAR },
    {  9, ALWAN_TF_H273_LOG },      { 10, ALWAN_TF_H273_LOG_SQRT }, { 11, ALWAN_TF_XVYCC },
    { 12, ALWAN_TF_BT1361 },        { 13, ALWAN_TF_SYCC },          { 14, ALWAN_TF_BT2020 },
    { 15, ALWAN_TF_BT2020_12BIT },  { 16, ALWAN_TF_PQ },            { 17, ALWAN_TF_DCDM },
    { 18, ALWAN_TF_HLG },
};

/* matrix_coefficients with Kr and Kb as numbers. 12 and 13 derive them from the
 * colour primaries; 0 (identity), 8 (YCgCo), 11 (Y'D'zD'x) and 14 (ICtCp) have none. */
typedef struct {
    int code;
    double kr, kb;
} alwan__h273_matrix;

static alwan__h273_matrix const g_h273_matrix[] = {
    { 1, 0.2126, 0.0722 }, { 4, 0.30, 0.11 },   { 5, 0.299, 0.114 },   { 6, 0.299, 0.114 },
    { 7, 0.212, 0.087 },   { 9, 0.2627, 0.0593 }, { 10, 0.2627, 0.0593 },
};

/* H.273 gives white points to three or four decimals: alwan's System M spaces carry
 * Illuminant C as (0.31006, 0.31616), and EBU Tech 3213-E rounds D65 to (0.313, 0.329). */
#define ALWAN__H273_WHITE_TOL 5e-4

static alwan__h273_primaries const *alwan__h273_find_primaries(int code) {
    size_t i;
    for (i = 0; i < sizeof(g_h273_primaries) / sizeof(g_h273_primaries[0]); i++) {
        if (g_h273_primaries[i].code == code) return &g_h273_primaries[i];
    }
    return NULL;
}

/* A code with no entry: unspecified means the stream does not say, anything else is
 * reserved or out of range. */
static alwan_status alwan__h273_unlisted(int code) {
    return code == ALWAN__H273_UNSPECIFIED ? ALWAN_E_NODATA : ALWAN_E_INVALID;
}

alwan_status alwan_h273_color_primaries_to_space(alwan_rgb_space *space_out, int color_primaries) {
    alwan__h273_primaries const *p;
    if (!space_out) return ALWAN_E_INVALID;
    p = alwan__h273_find_primaries(color_primaries);
    if (!p) return alwan__h273_unlisted(color_primaries);
    *space_out = p->space;
    return ALWAN_OK;
}

alwan_status alwan_h273_color_primaries_from_space(int *color_primaries_out, alwan_rgb_space space) {
    double xy[8];
    size_t i;
    int k, same;
    alwan_status st;
    if (!color_primaries_out) return ALWAN_E_INVALID;
#if ALWAN_WITH_F64
    {
        alwan_rgb_space_desc_f64 desc;
        st = alwan_rgb_get_space_descriptor_f64(&desc, space, NULL);
        if (st != ALWAN_OK) return st;
        for (k = 0; k < 6; k++) xy[k] = desc.primaries_xy[k];
        xy[6] = desc.white_xy[0];
        xy[7] = desc.white_xy[1];
    }
#else
    {
        alwan_rgb_space_desc_f32 desc;
        st = alwan_rgb_get_space_descriptor_f32(&desc, space, NULL);
        if (st != ALWAN_OK) return st;
        for (k = 0; k < 6; k++) xy[k] = (double)desc.primaries_xy[k];
        xy[6] = (double)desc.white_xy[0];
        xy[7] = (double)desc.white_xy[1];
    }
#endif
    /* The lowest code whose primaries are the space's and whose white point agrees to
     * H.273's rounding. */
    for (i = 0; i < sizeof(g_h273_primaries) / sizeof(g_h273_primaries[0]); i++) {
        double const *t = g_h273_primaries[i].xy;
        same = 1;
        for (k = 0; k < 6; k++) {
#if ALWAN_WITH_F64
            same = same && xy[k] == t[k];
#else
            same = same && xy[k] == (double)(float)t[k];
#endif
        }
        for (k = 6; k < 8; k++) same = same && xy[k] - t[k] <= ALWAN__H273_WHITE_TOL && t[k] - xy[k] <= ALWAN__H273_WHITE_TOL;
        if (same) {
            *color_primaries_out = g_h273_primaries[i].code;
            return ALWAN_OK;
        }
    }
    return ALWAN_E_NODATA;
}

alwan_status alwan_h273_transfer_to_tf(alwan_transfer_function *tf_out, int transfer_characteristics) {
    size_t i;
    if (!tf_out) return ALWAN_E_INVALID;
    for (i = 0; i < sizeof(g_h273_transfer) / sizeof(g_h273_transfer[0]); i++) {
        if (g_h273_transfer[i].code == transfer_characteristics) {
            *tf_out = g_h273_transfer[i].tf;
            return ALWAN_OK;
        }
    }
    return alwan__h273_unlisted(transfer_characteristics);
}

alwan_status alwan_h273_transfer_from_tf(int *transfer_characteristics_out, alwan_transfer_function tf) {
    size_t i;
    if (!transfer_characteristics_out || (int)tf < (int)ALWAN_TF_LINEAR || (int)tf >= (int)ALWAN_TF_COUNT) {
        return ALWAN_E_INVALID;
    }
    /* sRGB is transfer 13 on [0, 1]; below 0 alwan's curve continues the linear
     * segment where sYCC mirrors. ST2084 is the PQ alias. */
    if (tf == ALWAN_TF_SRGB) tf = ALWAN_TF_SYCC;
    if (tf == ALWAN_TF_ST2084) tf = ALWAN_TF_PQ;
    for (i = 0; i < sizeof(g_h273_transfer) / sizeof(g_h273_transfer[0]); i++) {
        if (g_h273_transfer[i].tf == tf) {
            *transfer_characteristics_out = g_h273_transfer[i].code;
            return ALWAN_OK;
        }
    }
    return ALWAN_E_NODATA;
}

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_h273_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_h273_impl.inc"
#include "alwan_api_teardown.h"
#endif
