/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk Oklab Conversions
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk XYZ <-> Oklab Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_oklab_bulk(alwan_scalar *oklab_out,
                            alwan_scalar const *xyz_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!xyz_in || !oklab_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)oklab_out + i * out_stride);

        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_oklab oklab;
        alwan_xyz_to_oklab(&oklab, &xyz);

        out_ptr[0] = oklab.L;
        out_ptr[1] = oklab.a;
        out_ptr[2] = oklab.b;
    }

    return ALWAN_OK;
}

int alwan_oklab_to_xyz_bulk(alwan_scalar *xyz_out,
                            alwan_scalar const *oklab_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!oklab_in || !xyz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)oklab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_oklab oklab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_oklab_to_xyz(&xyz, &oklab);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk Oklab <-> Oklch Conversions
 * ---------------------------------------------------------------- */

int alwan_oklab_to_oklch_bulk(alwan_scalar *oklch_out,
                              alwan_scalar const *oklab_in,
                              size_t count,
                              size_t in_stride,
                              size_t out_stride) {
    if (!oklab_in || !oklch_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)oklab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)oklch_out + i * out_stride);

        alwan_oklab oklab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_oklch oklch;
        alwan_oklab_to_oklch(&oklch, &oklab);

        out_ptr[0] = oklch.L;
        out_ptr[1] = oklch.C;
        out_ptr[2] = oklch.h;
    }

    return ALWAN_OK;
}

int alwan_oklch_to_oklab_bulk(alwan_scalar *oklab_out,
                              alwan_scalar const *oklch_in,
                              size_t count,
                              size_t in_stride,
                              size_t out_stride) {
    if (!oklch_in || !oklab_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)oklch_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)oklab_out + i * out_stride);

        alwan_oklch oklch = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_oklab oklab;
        alwan_oklch_to_oklab(&oklab, &oklch);

        out_ptr[0] = oklab.L;
        out_ptr[1] = oklab.a;
        out_ptr[2] = oklab.b;
    }

    return ALWAN_OK;
}
