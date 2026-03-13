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

int alwan_simulate_cvd_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
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

int alwan_simulate_protanopia_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
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

int alwan_simulate_deuteranopia_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
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

int alwan_simulate_tritanopia_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
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

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int alwan_simulate_cvd_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                    alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                    alwan_cvd_type cvd_type, alwan_scalar severity,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb rgb = {
            *(alwan_scalar const *)((char const *)in0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in2 + i * in_stride)
        };
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
        *(alwan_scalar *)((char *)out0 + i * out_stride) = r.r;
        *(alwan_scalar *)((char *)out1 + i * out_stride) = r.g;
        *(alwan_scalar *)((char *)out2 + i * out_stride) = r.b;
    }
    return ALWAN_OK;
}

ALWAN_MAP3_PLANAR_V_SCALAR(alwan_simulate_protanopia_map_planar,   alwan_rgb, alwan_rgb, alwan_simulate_protanopia_v,   r,g,b, r,g,b)
ALWAN_MAP3_PLANAR_V_SCALAR(alwan_simulate_deuteranopia_map_planar, alwan_rgb, alwan_rgb, alwan_simulate_deuteranopia_v, r,g,b, r,g,b)
ALWAN_MAP3_PLANAR_V_SCALAR(alwan_simulate_tritanopia_map_planar,   alwan_rgb, alwan_rgb, alwan_simulate_tritanopia_v,   r,g,b, r,g,b)

/* ----------------------------------------------------------------
 * Machado 2009 CVD Batch Map
 * ---------------------------------------------------------------- */

int alwan_simulate_cvd_machado_map_interleave(alwan_scalar *rgb_out,
                                               alwan_scalar const *rgb_in,
                                               alwan_cvd_type cvd_type,
                                               alwan_scalar severity,
                                               size_t count,
                                               size_t in_stride,
                                               size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    /* Select LUT once outside the loop */
    alwan_mat3x3 const *lut;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:
    case ALWAN_CVD_PROTANOMALY:
        lut = MACHADO_PROTAN;
        break;
    case ALWAN_CVD_DEUTERANOPIA:
    case ALWAN_CVD_DEUTERANOMALY:
        lut = MACHADO_DEUTAN;
        break;
    case ALWAN_CVD_TRITANOPIA:
    case ALWAN_CVD_TRITANOMALY:
        lut = MACHADO_TRITAN;
        break;
    default:
        return ALWAN_E_INVALID;
    }

    /* Interpolate matrix once (severity is constant across batch) */
    alwan_mat3x3 mat = alwan_machado_interpolate_v(lut, severity);

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_vec3 v = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        alwan_vec3 r = alwan_mat3_mulv_v(mat, v);
        out_ptr[0] = alwan_saturate(r.v[0]);
        out_ptr[1] = alwan_saturate(r.v[1]);
        out_ptr[2] = alwan_saturate(r.v[2]);
    }
    return ALWAN_OK;
}
