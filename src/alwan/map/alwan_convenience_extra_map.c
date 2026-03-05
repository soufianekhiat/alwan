/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Convenience Extra Color Model Conversions
 * CMY, YCoCg, HWB, YCbCr, YcCbcCrc, CMYK
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_convenience_core.h"

/* ----------------------------------------------------------------
 * YCbCr coefficient resolution (same as alwan_convenience.c)
 * ---------------------------------------------------------------- */

static void get_ycbcr_coeffs(alwan_ycbcr_standard standard, alwan_scalar *kr, alwan_scalar *kb) {
    switch (standard) {
        case ALWAN_YCBCR_BT601:
            *kr = ALWAN_LITERAL(0.299);
            *kb = ALWAN_LITERAL(0.114);
            break;
        case ALWAN_YCBCR_BT709:
            *kr = ALWAN_LITERAL(0.2126);
            *kb = ALWAN_LITERAL(0.0722);
            break;
        case ALWAN_YCBCR_BT2020:
            *kr = ALWAN_LITERAL(0.2627);
            *kb = ALWAN_LITERAL(0.0593);
            break;
        default:
            *kr = ALWAN_LITERAL(0.2126);
            *kb = ALWAN_LITERAL(0.0722);
            break;
    }
}

/* ----------------------------------------------------------------
 * RGB <-> CMY
 * ---------------------------------------------------------------- */

int alwan_rgb_to_cmy_map(alwan_scalar *cmy_out, alwan_scalar const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !cmy_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd one = alwan_simd_set1(1.0);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd_store(&c0[i], alwan_simd_sub(one, alwan_simd_load(&c0[i])));
            alwan_simd_store(&c1[i], alwan_simd_sub(one, alwan_simd_load(&c1[i])));
            alwan_simd_store(&c2[i], alwan_simd_sub(one, alwan_simd_load(&c2[i])));
        }
        for (; i < tile; i++) {
            alwan_rgb v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_cmy r = alwan_rgb_to_cmy_v(v);
            c0[i] = (alwan_simd_lane)r.c; c1[i] = (alwan_simd_lane)r.m; c2[i] = (alwan_simd_lane)r.y;
        }

        alwan__store_tile_aos3(cmy_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)cmy_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_cmy cmy = alwan_rgb_to_cmy_v(rgb);
        out_ptr[0] = cmy.c; out_ptr[1] = cmy.m; out_ptr[2] = cmy.y;
    }
#endif
    return ALWAN_OK;
}

int alwan_cmy_to_rgb_map(alwan_scalar *rgb_out, alwan_scalar const *cmy_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!cmy_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    alwan_simd one = alwan_simd_set1(1.0);
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, cmy_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd_store(&c0[i], alwan_simd_sub(one, alwan_simd_load(&c0[i])));
            alwan_simd_store(&c1[i], alwan_simd_sub(one, alwan_simd_load(&c1[i])));
            alwan_simd_store(&c2[i], alwan_simd_sub(one, alwan_simd_load(&c2[i])));
        }
        for (; i < tile; i++) {
            alwan_cmy v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_cmy_to_rgb_v(v);
            c0[i] = (alwan_simd_lane)r.r; c1[i] = (alwan_simd_lane)r.g; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)cmy_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_cmy cmy = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb = alwan_cmy_to_rgb_v(cmy);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCoCg
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ycocg_map(alwan_scalar *ycocg_out, alwan_scalar const *rgb_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !ycocg_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ycocg_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ycocg ycocg = alwan_rgb_to_ycocg_v(rgb);
        out_ptr[0] = ycocg.Y; out_ptr[1] = ycocg.Co; out_ptr[2] = ycocg.Cg;
    }
    return ALWAN_OK;
}

int alwan_ycocg_to_rgb_map(alwan_scalar *rgb_out, alwan_scalar const *ycocg_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!ycocg_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ycocg_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_ycocg ycocg = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb = alwan_ycocg_to_rgb_v(ycocg);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HWB, HSV <-> HWB
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hwb_map(alwan_scalar *hwb_out, alwan_scalar const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !hwb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hwb_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hwb hwb = alwan_rgb_to_hwb_v(rgb);
        out_ptr[0] = hwb.h; out_ptr[1] = hwb.w; out_ptr[2] = hwb.b;
    }
    return ALWAN_OK;
}

int alwan_hwb_to_rgb_map(alwan_scalar *rgb_out, alwan_scalar const *hwb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hwb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hwb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_hwb hwb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb = alwan_hwb_to_rgb_v(hwb);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
    return ALWAN_OK;
}

int alwan_hsv_to_hwb_map(alwan_scalar *hwb_out, alwan_scalar const *hsv_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hsv_in || !hwb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hsv_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hwb_out + i * out_stride);
        alwan_hsv hsv = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hwb hwb = alwan_hsv_to_hwb_v(hsv);
        out_ptr[0] = hwb.h; out_ptr[1] = hwb.w; out_ptr[2] = hwb.b;
    }
    return ALWAN_OK;
}

int alwan_hwb_to_hsv_map(alwan_scalar *hsv_out, alwan_scalar const *hwb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hwb_in || !hsv_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hwb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hsv_out + i * out_stride);
        alwan_hwb hwb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hsv hsv = alwan_hwb_to_hsv_v(hwb);
        out_ptr[0] = hsv.h; out_ptr[1] = hsv.s; out_ptr[2] = hsv.v;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YCbCr (with standard enum)
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ycbcr_map(alwan_scalar *ycbcr_out, alwan_scalar const *rgb_in,
                            alwan_ycbcr_standard standard,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !ycbcr_out || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    get_ycbcr_coeffs(standard, &kr, &kb);

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ycbcr_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ycbcr ycbcr = alwan_rgb_to_ycbcr_kr_kb_v(rgb, kr, kb);
        out_ptr[0] = ycbcr.Y; out_ptr[1] = ycbcr.Cb; out_ptr[2] = ycbcr.Cr;
    }
    return ALWAN_OK;
}

int alwan_ycbcr_to_rgb_map(alwan_scalar *rgb_out, alwan_scalar const *ycbcr_in,
                            alwan_ycbcr_standard standard,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!ycbcr_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    alwan_scalar kr, kb;
    get_ycbcr_coeffs(standard, &kr, &kb);

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ycbcr_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_ycbcr ycbcr = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb = alwan_ycbcr_to_rgb_kr_kb_v(ycbcr, kr, kb);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> YcCbcCrc (with bit_depth)
 * ---------------------------------------------------------------- */

int alwan_rgb_to_yccbccrc_map(alwan_scalar *yccbccrc_out, alwan_scalar const *rgb_in,
                               int bit_depth,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !yccbccrc_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)yccbccrc_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_yccbccrc ycc = alwan_rgb_to_yccbccrc_v(rgb, bit_depth);
        out_ptr[0] = ycc.Yc; out_ptr[1] = ycc.Cbc; out_ptr[2] = ycc.Crc;
    }
    return ALWAN_OK;
}

int alwan_yccbccrc_to_rgb_map(alwan_scalar *rgb_out, alwan_scalar const *yccbccrc_in,
                               int bit_depth,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!yccbccrc_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)yccbccrc_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_yccbccrc ycc = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb rgb = alwan_yccbccrc_to_rgb_v(ycc, bit_depth);
        out_ptr[0] = rgb.r; out_ptr[1] = rgb.g; out_ptr[2] = rgb.b;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * YCbCr legal <-> full range (with bit_depth)
 * ---------------------------------------------------------------- */

int alwan_ycbcr_full_to_legal_map(alwan_scalar *out, alwan_scalar const *in,
                                   int bit_depth,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)out + i * out_stride);
        alwan_ycbcr ycbcr = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ycbcr result = alwan_ycbcr_full_to_legal_v(ycbcr, bit_depth);
        out_ptr[0] = result.Y; out_ptr[1] = result.Cb; out_ptr[2] = result.Cr;
    }
    return ALWAN_OK;
}

int alwan_ycbcr_legal_to_full_map(alwan_scalar *out, alwan_scalar const *in,
                                   int bit_depth,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)out + i * out_stride);
        alwan_ycbcr ycbcr = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ycbcr result = alwan_ycbcr_legal_to_full_v(ycbcr, bit_depth);
        out_ptr[0] = result.Y; out_ptr[1] = result.Cb; out_ptr[2] = result.Cr;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CMY <-> CMYK (4-channel, hand-written)
 * ---------------------------------------------------------------- */

int alwan_cmy_to_cmyk_map(alwan_scalar *cmyk_out,
                           alwan_scalar const *cmy_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!cmy_in || !cmyk_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)cmy_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)cmyk_out + i * out_stride);
        alwan_cmy cmy = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_cmyk cmyk = alwan_cmy_to_cmyk_v(cmy);
        out_ptr[0] = cmyk.c; out_ptr[1] = cmyk.m; out_ptr[2] = cmyk.y; out_ptr[3] = cmyk.k;
    }
    return ALWAN_OK;
}

int alwan_cmyk_to_cmy_map(alwan_scalar *cmy_out,
                           alwan_scalar const *cmyk_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride) {
    if (!cmyk_in || !cmy_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)cmyk_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)cmy_out + i * out_stride);
        alwan_cmyk cmyk = {in_ptr[0], in_ptr[1], in_ptr[2], in_ptr[3]};
        alwan_cmy cmy = alwan_cmyk_to_cmy_v(cmyk);
        out_ptr[0] = cmy.c; out_ptr[1] = cmy.m; out_ptr[2] = cmy.y;
    }
    return ALWAN_OK;
}
