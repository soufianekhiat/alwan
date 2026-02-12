/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk Convenience Color Model Conversions
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk RGB <-> HSV Conversions
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsv_bulk(alwan_scalar *hsv_out,
                          alwan_scalar const *rgb_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!rgb_in || !hsv_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hsv_out + i * out_stride);

        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hsv hsv;
        int status = alwan_rgb_to_hsv(&hsv, &rgb);
        if (status != ALWAN_OK) {
            return status;
        }

        out_ptr[0] = hsv.h;
        out_ptr[1] = hsv.s;
        out_ptr[2] = hsv.v;
    }

    return ALWAN_OK;
}

int alwan_hsv_to_rgb_bulk(alwan_scalar *rgb_out,
                          alwan_scalar const *hsv_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!hsv_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hsv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        alwan_hsv hsv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb;
        int status = alwan_hsv_to_rgb(&rgb, &hsv);
        if (status != ALWAN_OK) {
            return status;
        }

        out_ptr[0] = rgb.r;
        out_ptr[1] = rgb.g;
        out_ptr[2] = rgb.b;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk RGB <-> HSL Conversions
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hsl_bulk(alwan_scalar *hsl_out,
                          alwan_scalar const *rgb_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!rgb_in || !hsl_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hsl_out + i * out_stride);

        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hsl hsl;
        int status = alwan_rgb_to_hsl(&hsl, &rgb);
        if (status != ALWAN_OK) {
            return status;
        }

        out_ptr[0] = hsl.h;
        out_ptr[1] = hsl.s;
        out_ptr[2] = hsl.l;
    }

    return ALWAN_OK;
}

int alwan_hsl_to_rgb_bulk(alwan_scalar *rgb_out,
                          alwan_scalar const *hsl_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!hsl_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hsl_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        alwan_hsl hsl = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb;
        int status = alwan_hsl_to_rgb(&rgb, &hsl);
        if (status != ALWAN_OK) {
            return status;
        }

        out_ptr[0] = rgb.r;
        out_ptr[1] = rgb.g;
        out_ptr[2] = rgb.b;
    }

    return ALWAN_OK;
}
