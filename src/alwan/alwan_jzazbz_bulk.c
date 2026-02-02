/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk JzAzBz Conversions
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk XYZ <-> JzAzBz Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_jzazbz_bulk(alwan_scalar *jzazbz_out,
                             alwan_scalar const *xyz_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!xyz_in || !jzazbz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)jzazbz_out + i * out_stride);

        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_jzazbz jzazbz;
        alwan_xyz_to_jzazbz(&jzazbz, &xyz);

        out_ptr[0] = jzazbz.Jz;
        out_ptr[1] = jzazbz.az;
        out_ptr[2] = jzazbz.bz;
    }

    return ALWAN_OK;
}

int alwan_jzazbz_to_xyz_bulk(alwan_scalar *xyz_out,
                             alwan_scalar const *jzazbz_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!jzazbz_in || !xyz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)jzazbz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_jzazbz jzazbz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_jzazbz_to_xyz(&xyz, &jzazbz);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk JzAzBz <-> JzCzhz Conversions
 * ---------------------------------------------------------------- */

int alwan_jzazbz_to_jzczhz_bulk(alwan_scalar *jzczhz_out,
                                alwan_scalar const *jzazbz_in,
                                size_t count,
                                size_t in_stride,
                                size_t out_stride) {
    if (!jzazbz_in || !jzczhz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)jzazbz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)jzczhz_out + i * out_stride);

        alwan_jzazbz jzazbz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_jzczhz jzczhz;
        alwan_jzazbz_to_jzczhz(&jzczhz, &jzazbz);

        out_ptr[0] = jzczhz.Jz;
        out_ptr[1] = jzczhz.Cz;
        out_ptr[2] = jzczhz.hz;
    }

    return ALWAN_OK;
}

int alwan_jzczhz_to_jzazbz_bulk(alwan_scalar *jzazbz_out,
                                alwan_scalar const *jzczhz_in,
                                size_t count,
                                size_t in_stride,
                                size_t out_stride) {
    if (!jzczhz_in || !jzazbz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)jzczhz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)jzazbz_out + i * out_stride);

        alwan_jzczhz jzczhz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_jzazbz jzazbz;
        alwan_jzczhz_to_jzazbz(&jzazbz, &jzczhz);

        out_ptr[0] = jzazbz.Jz;
        out_ptr[1] = jzazbz.az;
        out_ptr[2] = jzazbz.bz;
    }

    return ALWAN_OK;
}
