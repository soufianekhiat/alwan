/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * HDR Pipeline Utilities
 * Thin wrapper -- per-pixel math in alwan_hdr_core.h
 *
 * HLG OOTF, MaxCLL, MaxFALL, BT.2408 reference white.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_hdr_core.h"

/* ----------------------------------------------------------------
 * HLG OOTF
 * ---------------------------------------------------------------- */

int alwan_hlg_ootf(alwan_rgb *out, alwan_rgb const *in,
                    alwan_scalar Lw, alwan_scalar gamma_sys) {
    if (!out || !in) return ALWAN_E_INVALID;
    *out = alwan_hlg_ootf_v(*in, Lw, gamma_sys);
    return ALWAN_OK;
}

int alwan_hlg_ootf_inv(alwan_rgb *out, alwan_rgb const *in,
                        alwan_scalar Lw, alwan_scalar gamma_sys) {
    if (!out || !in) return ALWAN_E_INVALID;
    *out = alwan_hlg_ootf_inv_v(*in, Lw, gamma_sys);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * MaxCLL - Maximum Content Light Level
 * ---------------------------------------------------------------- */

int alwan_maxcll(alwan_scalar *maxcll_out,
                  alwan_scalar const *rgb_data,
                  size_t count,
                  size_t stride) {
    if (!maxcll_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxcll_out = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_scalar max_val = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *ptr = (alwan_scalar const *)((char const *)rgb_data + i * stride);
        /* MaxCLL is max of R, G, B across all pixels */
        alwan_scalar pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        if (pixel_max > max_val) max_val = pixel_max;
    }
    *maxcll_out = max_val;
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * MaxFALL - Maximum Frame Average Light Level
 * ---------------------------------------------------------------- */

int alwan_maxfall(alwan_scalar *maxfall_out,
                   alwan_scalar const *rgb_data,
                   size_t count,
                   size_t stride) {
    if (!maxfall_out || !rgb_data) return ALWAN_E_INVALID;
    if (count == 0) {
        *maxfall_out = ALWAN_LITERAL(0.0);
        return ALWAN_OK;
    }

    alwan_scalar sum = ALWAN_LITERAL(0.0);
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *ptr = (alwan_scalar const *)((char const *)rgb_data + i * stride);
        /* Frame average: mean of max(R,G,B) per pixel */
        alwan_scalar pixel_max = ptr[0];
        if (ptr[1] > pixel_max) pixel_max = ptr[1];
        if (ptr[2] > pixel_max) pixel_max = ptr[2];
        sum += pixel_max;
    }
    *maxfall_out = sum / (alwan_scalar)count;
    return ALWAN_OK;
}
