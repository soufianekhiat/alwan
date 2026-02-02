/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk Color Appearance Model Conversions
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk CIECAM02 Conversions
 * ---------------------------------------------------------------- */

int alwan_ciecam02_forward_bulk(alwan_ciecam02_correlates *correlates_out,
                                alwan_scalar const *xyz_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count,
                                size_t in_stride) {
    if (!xyz_in || !correlates_out || !vc || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);

        alwan_xyz xyz = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        int status = alwan_ciecam02_forward(&correlates_out[i], &xyz, vc);
        if (status != ALWAN_OK) {
            return status;
        }
    }

    return ALWAN_OK;
}

int alwan_ciecam02_inverse_bulk(alwan_scalar *xyz_out,
                                alwan_ciecam02_correlates const *correlates_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count,
                                size_t out_stride) {
    if (!correlates_in || !xyz_out || !vc || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_xyz xyz;
        int status = alwan_ciecam02_inverse(&xyz, &correlates_in[i], vc);
        if (status != ALWAN_OK) {
            return status;
        }

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk CAM16 Conversions
 * ---------------------------------------------------------------- */

int alwan_cam16_forward_bulk(alwan_cam16_correlates *correlates_out,
                             alwan_scalar const *xyz_in,
                             alwan_cam16_viewing_conditions const *vc,
                             size_t count,
                             size_t in_stride) {
    if (!xyz_in || !correlates_out || !vc || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);

        alwan_xyz xyz = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        int status = alwan_cam16_forward(&correlates_out[i], &xyz, vc);
        if (status != ALWAN_OK) {
            return status;
        }
    }

    return ALWAN_OK;
}

int alwan_cam16_inverse_bulk(alwan_scalar *xyz_out,
                             alwan_cam16_correlates const *correlates_in,
                             alwan_cam16_viewing_conditions const *vc,
                             size_t count,
                             size_t out_stride) {
    if (!correlates_in || !xyz_out || !vc || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_xyz xyz;
        int status = alwan_cam16_inverse(&xyz, &correlates_in[i], vc);
        if (status != ALWAN_OK) {
            return status;
        }

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}
