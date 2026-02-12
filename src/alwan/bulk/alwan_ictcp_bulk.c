/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk ICtCp Conversions
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk RGB <-> ICtCp Conversions
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ictcp_bulk(alwan_scalar *ictcp_out,
                            alwan_scalar const *rgb_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!rgb_in || !ictcp_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ictcp_out + i * out_stride);

        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ictcp ictcp;
        alwan_rgb_to_ictcp(&ictcp, &rgb, use_pq);

        out_ptr[0] = ictcp.I;
        out_ptr[1] = ictcp.Ct;
        out_ptr[2] = ictcp.Cp;
    }

    return ALWAN_OK;
}

int alwan_ictcp_to_rgb_bulk(alwan_scalar *rgb_out,
                            alwan_scalar const *ictcp_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!ictcp_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ictcp_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        alwan_ictcp ictcp = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb;
        alwan_ictcp_to_rgb(&rgb, &ictcp, use_pq);

        out_ptr[0] = rgb.r;
        out_ptr[1] = rgb.g;
        out_ptr[2] = rgb.b;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk XYZ <-> ICtCp Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_ictcp_bulk(alwan_scalar *ictcp_out,
                            alwan_scalar const *xyz_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!xyz_in || !ictcp_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ictcp_out + i * out_stride);

        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ictcp ictcp;
        alwan_xyz_to_ictcp(&ictcp, &xyz, use_pq);

        out_ptr[0] = ictcp.I;
        out_ptr[1] = ictcp.Ct;
        out_ptr[2] = ictcp.Cp;
    }

    return ALWAN_OK;
}

int alwan_ictcp_to_xyz_bulk(alwan_scalar *xyz_out,
                            alwan_scalar const *ictcp_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!ictcp_in || !xyz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ictcp_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_ictcp ictcp = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_ictcp_to_xyz(&xyz, &ictcp, use_pq);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}
