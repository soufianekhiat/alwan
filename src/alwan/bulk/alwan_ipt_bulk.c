/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk IPT Conversions
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk XYZ <-> IPT Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_ipt_bulk(alwan_scalar *ipt_out,
                          alwan_scalar const *xyz_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyz_in || !ipt_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ipt_out + i * out_stride);

        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ipt ipt;
        alwan_xyz_to_ipt(&ipt, &xyz);

        out_ptr[0] = ipt.I;
        out_ptr[1] = ipt.P;
        out_ptr[2] = ipt.T;
    }

    return ALWAN_OK;
}

int alwan_ipt_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *ipt_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!ipt_in || !xyz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ipt_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_ipt ipt = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_ipt_to_xyz(&xyz, &ipt);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}
