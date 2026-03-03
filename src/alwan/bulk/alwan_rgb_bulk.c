/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk sRGB Convenience and Batch Delta E
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk sRGB Convenience Conversions
 * ---------------------------------------------------------------- */

int alwan_srgb_to_xyz_bulk(alwan_scalar *xyz_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!rgb_in || !xyz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_srgb_to_xyz(&xyz, &rgb);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}

int alwan_xyz_to_srgb_bulk(alwan_scalar *rgb_out,
                           alwan_scalar const *xyz_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!xyz_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb;
        alwan_xyz_to_srgb(&rgb, &xyz);

        out_ptr[0] = rgb.r;
        out_ptr[1] = rgb.g;
        out_ptr[2] = rgb.b;
    }

    return ALWAN_OK;
}

int alwan_srgb_to_lab_bulk(alwan_scalar *lab_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!rgb_in || !lab_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lab_out + i * out_stride);

        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lab lab;
        alwan_srgb_to_lab(&lab, &rgb);

        out_ptr[0] = lab.L;
        out_ptr[1] = lab.a;
        out_ptr[2] = lab.b;
    }

    return ALWAN_OK;
}

int alwan_lab_to_srgb_bulk(alwan_scalar *rgb_out,
                           alwan_scalar const *lab_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!lab_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        alwan_lab lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb;
        alwan_lab_to_srgb(&rgb, &lab);

        out_ptr[0] = rgb.r;
        out_ptr[1] = rgb.g;
        out_ptr[2] = rgb.b;
    }

    return ALWAN_OK;
}

int alwan_srgb_to_oklab_bulk(alwan_scalar *oklab_out,
                             alwan_scalar const *rgb_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!rgb_in || !oklab_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)oklab_out + i * out_stride);

        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_oklab oklab;
        alwan_srgb_to_oklab(&oklab, &rgb);

        out_ptr[0] = oklab.L;
        out_ptr[1] = oklab.a;
        out_ptr[2] = oklab.b;
    }

    return ALWAN_OK;
}

int alwan_oklab_to_srgb_bulk(alwan_scalar *rgb_out,
                             alwan_scalar const *oklab_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride) {
    if (!oklab_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)oklab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        alwan_oklab oklab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb;
        alwan_oklab_to_srgb(&rgb, &oklab);

        out_ptr[0] = rgb.r;
        out_ptr[1] = rgb.g;
        out_ptr[2] = rgb.b;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Batch Delta E Computations
 * ---------------------------------------------------------------- */

int alwan_delta_e_76_batch(alwan_scalar *delta_e_out,
                           alwan_scalar const *lab1_in,
                           alwan_scalar const *lab2_in,
                           size_t count,
                           size_t in1_stride,
                           size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);

        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};

        delta_e_out[i] = alwan_delta_e_76(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_2000_batch(alwan_scalar *delta_e_out,
                             alwan_scalar const *lab1_in,
                             alwan_scalar const *lab2_in,
                             size_t count,
                             size_t in1_stride,
                             size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);

        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};

        delta_e_out[i] = alwan_delta_e_2000(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_94_batch(alwan_scalar *delta_e_out,
                           alwan_scalar const *lab1_in,
                           alwan_scalar const *lab2_in,
                           size_t count,
                           size_t in1_stride,
                           size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);

        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};

        delta_e_out[i] = alwan_delta_e_94(&lab1, &lab2);
    }

    return ALWAN_OK;
}

int alwan_delta_e_cmc_batch(alwan_scalar *delta_e_out,
                            alwan_scalar const *lab1_in,
                            alwan_scalar const *lab2_in,
                            alwan_scalar l,
                            alwan_scalar c,
                            size_t count,
                            size_t in1_stride,
                            size_t in2_stride) {
    if (!lab1_in || !lab2_in || !delta_e_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in1_ptr = (alwan_scalar const *)((char const *)lab1_in + i * in1_stride);
        alwan_scalar const *in2_ptr = (alwan_scalar const *)((char const *)lab2_in + i * in2_stride);

        alwan_lab lab1 = {in1_ptr[0], in1_ptr[1], in1_ptr[2]};
        alwan_lab lab2 = {in2_ptr[0], in2_ptr[1], in2_ptr[2]};

        delta_e_out[i] = alwan_delta_e_cmc(&lab1, &lab2, l, c);
    }

    return ALWAN_OK;
}
