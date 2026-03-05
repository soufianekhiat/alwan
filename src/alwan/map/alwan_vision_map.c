/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map CVD (Color Vision Deficiency) Simulation Functions
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_vision_core.h"

/* ----------------------------------------------------------------
 * CVD Simulation Map (dispatches by type)
 * ---------------------------------------------------------------- */

int alwan_simulate_cvd_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                            alwan_cvd_type cvd_type, alwan_scalar severity,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb r;
        switch (cvd_type) {
        case ALWAN_CVD_PROTANOPIA:
        case ALWAN_CVD_PROTANOMALY:
            r = alwan_simulate_protanopia_v(rgb, severity);
            break;
        case ALWAN_CVD_DEUTERANOPIA:
        case ALWAN_CVD_DEUTERANOMALY:
            r = alwan_simulate_deuteranopia_v(rgb, severity);
            break;
        case ALWAN_CVD_TRITANOPIA:
        case ALWAN_CVD_TRITANOMALY:
            r = alwan_simulate_tritanopia_v(rgb, severity);
            break;
        default:
            r = rgb;
            break;
        }
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Individual CVD Type Map Functions
 * ---------------------------------------------------------------- */

int alwan_simulate_protanopia_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                   alwan_scalar severity,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb r = alwan_simulate_protanopia_v(rgb, severity);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

int alwan_simulate_deuteranopia_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                     alwan_scalar severity,
                                     size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb r = alwan_simulate_deuteranopia_v(rgb, severity);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

int alwan_simulate_tritanopia_map(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                   alwan_scalar severity,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb r = alwan_simulate_tritanopia_v(rgb, severity);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}
