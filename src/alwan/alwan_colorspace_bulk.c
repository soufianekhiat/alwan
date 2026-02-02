/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Bulk Color Space Conversions
 */

#include "alwan.h"
#include "alwan_internal.h"

/* ----------------------------------------------------------------
 * Bulk XYZ <-> Lab Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_lab_bulk(alwan_scalar *lab_out,
                          alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyz_in || !lab_out || !white_xyz || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lab_out + i * out_stride);

        alwan_xyz xyz = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        alwan_lab lab;
        alwan_xyz_to_lab(&lab, &xyz, white_xyz);

        out_ptr[0] = lab.L;
        out_ptr[1] = lab.a;
        out_ptr[2] = lab.b;
    }

    return ALWAN_OK;
}

int alwan_lab_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *lab_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!lab_in || !xyz_out || !white_xyz || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_lab lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_lab_to_xyz(&xyz, &lab, white_xyz);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk XYZ <-> Luv Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_luv_bulk(alwan_scalar *luv_out,
                          alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyz_in || !luv_out || !white_xyz || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)luv_out + i * out_stride);

        alwan_xyz xyz = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        alwan_luv luv;
        alwan_xyz_to_luv(&luv, &xyz, white_xyz);

        out_ptr[0] = luv.L;
        out_ptr[1] = luv.u;
        out_ptr[2] = luv.v;
    }

    return ALWAN_OK;
}

int alwan_luv_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *luv_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!luv_in || !xyz_out || !white_xyz || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)luv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_luv luv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_luv_to_xyz(&xyz, &luv, white_xyz);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk Lab <-> LCh Conversions
 * ---------------------------------------------------------------- */

int alwan_lab_to_lch_bulk(alwan_scalar *lch_out,
                          alwan_scalar const *lab_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!lab_in || !lch_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lch_out + i * out_stride);

        alwan_lab lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lch lch;
        alwan_lab_to_lch(&lch, &lab);

        out_ptr[0] = lch.L;
        out_ptr[1] = lch.C;
        out_ptr[2] = lch.h;
    }

    return ALWAN_OK;
}

int alwan_lch_to_lab_bulk(alwan_scalar *lab_out,
                          alwan_scalar const *lch_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!lch_in || !lab_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lch_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lab_out + i * out_stride);

        alwan_lch lch = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lab lab;
        alwan_lch_to_lab(&lab, &lch);

        out_ptr[0] = lab.L;
        out_ptr[1] = lab.a;
        out_ptr[2] = lab.b;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk Luv <-> LCh(uv) Conversions
 * ---------------------------------------------------------------- */

int alwan_luv_to_lchuv_bulk(alwan_scalar *lchuv_out,
                            alwan_scalar const *luv_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!luv_in || !lchuv_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)luv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lchuv_out + i * out_stride);

        alwan_luv luv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lchuv lchuv;
        alwan_luv_to_lchuv(&lchuv, &luv);

        out_ptr[0] = lchuv.L;
        out_ptr[1] = lchuv.C;
        out_ptr[2] = lchuv.h;
    }

    return ALWAN_OK;
}

int alwan_lchuv_to_luv_bulk(alwan_scalar *luv_out,
                            alwan_scalar const *lchuv_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride) {
    if (!lchuv_in || !luv_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lchuv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)luv_out + i * out_stride);

        alwan_lchuv lchuv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_luv luv;
        alwan_lchuv_to_luv(&luv, &lchuv);

        out_ptr[0] = luv.L;
        out_ptr[1] = luv.u;
        out_ptr[2] = luv.v;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Bulk XYZ <-> xyY Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_xyy_bulk(alwan_scalar *xyy_out,
                          alwan_scalar const *xyz_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyz_in || !xyy_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyy_out + i * out_stride);

        alwan_xyz xyz = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        alwan_xyy xyy;
        alwan_xyz_to_xyy(&xyy, &xyz);

        out_ptr[0] = xyy.x;
        out_ptr[1] = xyy.y;
        out_ptr[2] = xyy.Y;
    }

    return ALWAN_OK;
}

int alwan_xyy_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *xyy_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride) {
    if (!xyy_in || !xyz_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyy_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);

        alwan_xyy xyy = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz xyz;
        alwan_xyy_to_xyz(&xyz, &xyy);

        out_ptr[0] = xyz.x;
        out_ptr[1] = xyz.y;
        out_ptr[2] = xyz.z;
    }

    return ALWAN_OK;
}
