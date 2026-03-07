/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map internal header - shared SIMD building blocks for map functions
 */

#ifndef ALWAN_MAP_INTERNAL_H
#define ALWAN_MAP_INTERNAL_H

#include "../simd/alwan_simd.h"
#include "../alwan_platform.h"
#include "../alwan.h"
#include <stdint.h>

/* ================================================================
 * Type-Generic SIMD Aliases
 *
 * Maps generic names to f32 or f64 based on ALWAN_SCALAR_IS_FLOAT.
 * All map functions use these instead of alwan_simd_f32_* directly,
 * so the same code works at full precision for both float and double.
 * ================================================================ */

#if ALWAN_SCALAR_IS_FLOAT
    #define ALWAN_SIMD_WIDTH        ALWAN_SIMD_F32_WIDTH
    typedef float                   alwan_simd_lane;
    typedef alwan_simd_f32          alwan_simd;
    typedef alwan_simd_f32_mask     alwan_simd_mask;
    #define alwan_simd_set1(v)      alwan_simd_f32_set1(v)
    #define alwan_simd_zero         alwan_simd_f32_zero
    #define alwan_simd_load         alwan_simd_f32_load
    #define alwan_simd_store        alwan_simd_f32_store
    #define alwan_simd_add          alwan_simd_f32_add
    #define alwan_simd_sub          alwan_simd_f32_sub
    #define alwan_simd_mul          alwan_simd_f32_mul
    #define alwan_simd_div          alwan_simd_f32_div
    #define alwan_simd_neg          alwan_simd_f32_neg
    #define alwan_simd_abs          alwan_simd_f32_abs
    #define alwan_simd_fmadd        alwan_simd_f32_fmadd
    #define alwan_simd_fmsub        alwan_simd_f32_fmsub
    #define alwan_simd_sqrt         alwan_simd_f32_sqrt
    #define alwan_simd_cbrt         alwan_simd_f32_cbrt
    #define alwan_simd_pow          alwan_simd_f32_pow
    #define alwan_simd_exp          alwan_simd_f32_exp
    #define alwan_simd_log          alwan_simd_f32_log
    #define alwan_simd_log2         alwan_simd_f32_log2
    #define alwan_simd_log10        alwan_simd_f32_log10
    #define alwan_simd_sin          alwan_simd_f32_sin
    #define alwan_simd_cos          alwan_simd_f32_cos
    #define alwan_simd_atan2        alwan_simd_f32_atan2
    #define alwan_simd_floor        alwan_simd_f32_floor
    #define alwan_simd_ceil         alwan_simd_f32_ceil
    #define alwan_simd_round        alwan_simd_f32_round
    #define alwan_simd_trunc        alwan_simd_f32_trunc
    #define alwan_simd_min          alwan_simd_f32_min
    #define alwan_simd_max          alwan_simd_f32_max
    #define alwan_simd_clamp        alwan_simd_f32_clamp
    #define alwan_simd_rcp          alwan_simd_f32_rcp
    #define alwan_simd_cmpeq        alwan_simd_f32_cmpeq
    #define alwan_simd_cmplt        alwan_simd_f32_cmplt
    #define alwan_simd_cmple        alwan_simd_f32_cmple
    #define alwan_simd_cmpgt        alwan_simd_f32_cmpgt
    #define alwan_simd_cmpge        alwan_simd_f32_cmpge
    #define alwan_simd_select       alwan_simd_f32_select
#else
    #define ALWAN_SIMD_WIDTH        ALWAN_SIMD_F64_WIDTH
    typedef double                  alwan_simd_lane;
    typedef alwan_simd_f64          alwan_simd;
    typedef alwan_simd_f64_mask     alwan_simd_mask;
    #define alwan_simd_set1(v)      alwan_simd_f64_set1(v)
    #define alwan_simd_zero         alwan_simd_f64_zero
    #define alwan_simd_load         alwan_simd_f64_load
    #define alwan_simd_store        alwan_simd_f64_store
    #define alwan_simd_add          alwan_simd_f64_add
    #define alwan_simd_sub          alwan_simd_f64_sub
    #define alwan_simd_mul          alwan_simd_f64_mul
    #define alwan_simd_div          alwan_simd_f64_div
    #define alwan_simd_neg          alwan_simd_f64_neg
    #define alwan_simd_abs          alwan_simd_f64_abs
    #define alwan_simd_fmadd        alwan_simd_f64_fmadd
    #define alwan_simd_fmsub        alwan_simd_f64_fmsub
    #define alwan_simd_sqrt         alwan_simd_f64_sqrt
    #define alwan_simd_cbrt         alwan_simd_f64_cbrt
    #define alwan_simd_pow          alwan_simd_f64_pow
    #define alwan_simd_exp          alwan_simd_f64_exp
    #define alwan_simd_log          alwan_simd_f64_log
    #define alwan_simd_log2         alwan_simd_f64_log2
    #define alwan_simd_log10        alwan_simd_f64_log10
    #define alwan_simd_sin          alwan_simd_f64_sin
    #define alwan_simd_cos          alwan_simd_f64_cos
    #define alwan_simd_atan2        alwan_simd_f64_atan2
    #define alwan_simd_floor        alwan_simd_f64_floor
    #define alwan_simd_ceil         alwan_simd_f64_ceil
    #define alwan_simd_round        alwan_simd_f64_round
    #define alwan_simd_trunc        alwan_simd_f64_trunc
    #define alwan_simd_min          alwan_simd_f64_min
    #define alwan_simd_max          alwan_simd_f64_max
    #define alwan_simd_clamp        alwan_simd_f64_clamp
    #define alwan_simd_rcp          alwan_simd_f64_rcp
    #define alwan_simd_cmpeq        alwan_simd_f64_cmpeq
    #define alwan_simd_cmplt        alwan_simd_f64_cmplt
    #define alwan_simd_cmple        alwan_simd_f64_cmple
    #define alwan_simd_cmpgt        alwan_simd_f64_cmpgt
    #define alwan_simd_cmpge        alwan_simd_f64_cmpge
    #define alwan_simd_select       alwan_simd_f64_select
#endif

/* ----------------------------------------------------------------
 * Tile Constants
 *
 * Tile size: 128 x 32 = 4096 pixels
 * SoA scratch: 3 channels x 4096 x sizeof(alwan_simd_lane)
 *   f32: 48 KB -> fits in L1
 *   f64: 96 KB -> fits in L2, partially in L1
 * ---------------------------------------------------------------- */

#define ALWAN_TILE_W      128
#define ALWAN_TILE_H       32
#define ALWAN_TILE_PIXELS (ALWAN_TILE_W * ALWAN_TILE_H)  /* 4096 */

/* Near-zero guards for SIMD divide-by-zero protection */
#define ALWAN_MAP_DIV_GUARD    1e-10   /* For chromaticity / xyY denominators */
#define ALWAN_MAP_PQ_DIV_GUARD 1e-30   /* For PQ EOTF denominator (tiny, avoids denormals) */

/* ----------------------------------------------------------------
 * Tile Load: AoS strided input -> flat SoA arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load_tile_aos3(alwan_simd_lane *ch0, alwan_simd_lane *ch1, alwan_simd_lane *ch2,
                                         alwan_scalar const *base, size_t offset,
                                         size_t stride, size_t tile_count) {
    for (size_t j = 0; j < tile_count; j++) {
        alwan_scalar const *p = (alwan_scalar const *)((char const *)base + (offset + j) * stride);
        ch0[j] = (alwan_simd_lane)p[0];
        ch1[j] = (alwan_simd_lane)p[1];
        ch2[j] = (alwan_simd_lane)p[2];
    }
}

/* ----------------------------------------------------------------
 * Tile Store: flat SoA arrays -> AoS strided output
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__store_tile_aos3(alwan_scalar *base, size_t offset, size_t stride,
                                          alwan_simd_lane const *ch0, alwan_simd_lane const *ch1, alwan_simd_lane const *ch2,
                                          size_t tile_count) {
    for (size_t j = 0; j < tile_count; j++) {
        alwan_scalar *p = (alwan_scalar *)((char *)base + (offset + j) * stride);
        p[0] = (alwan_scalar)ch0[j];
        p[1] = (alwan_scalar)ch1[j];
        p[2] = (alwan_scalar)ch2[j];
    }
}

/* ----------------------------------------------------------------
 * Typed per-element load/store (for _ex functions scalar fallback)
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load3_typed(alwan_scalar out[3],
                                      void const *ptr,
                                      alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        uint8_t const *p = (uint8_t const *)ptr;
        out[0] = p[0] * ALWAN_LITERAL(1.0 / 255.0);
        out[1] = p[1] * ALWAN_LITERAL(1.0 / 255.0);
        out[2] = p[2] * ALWAN_LITERAL(1.0 / 255.0);
    } break;
    case ALWAN_PIXEL_U16: {
        uint16_t const *p = (uint16_t const *)ptr;
        out[0] = p[0] * ALWAN_LITERAL(1.0 / 65535.0);
        out[1] = p[1] * ALWAN_LITERAL(1.0 / 65535.0);
        out[2] = p[2] * ALWAN_LITERAL(1.0 / 65535.0);
    } break;
    case ALWAN_PIXEL_F32: {
        float const *p = (float const *)ptr;
        out[0] = (alwan_scalar)p[0];
        out[1] = (alwan_scalar)p[1];
        out[2] = (alwan_scalar)p[2];
    } break;
    case ALWAN_PIXEL_F64: {
        double const *p = (double const *)ptr;
        out[0] = (alwan_scalar)p[0];
        out[1] = (alwan_scalar)p[1];
        out[2] = (alwan_scalar)p[2];
    } break;
    }
}

ALWAN_INLINE void alwan__store3_typed(void *ptr,
                                       alwan_scalar const in[3],
                                       alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        uint8_t *p = (uint8_t *)ptr;
        for (int c = 0; c < 3; c++) {
            alwan_scalar v = in[c] * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
            if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
            if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
            p[c] = (uint8_t)v;
        }
    } break;
    case ALWAN_PIXEL_U16: {
        uint16_t *p = (uint16_t *)ptr;
        for (int c = 0; c < 3; c++) {
            alwan_scalar v = in[c] * ALWAN_LITERAL(65535.0) + ALWAN_LITERAL(0.5);
            if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
            if (v > ALWAN_LITERAL(65535.0)) v = ALWAN_LITERAL(65535.0);
            p[c] = (uint16_t)v;
        }
    } break;
    case ALWAN_PIXEL_F32: {
        float *p = (float *)ptr;
        p[0] = (float)in[0];
        p[1] = (float)in[1];
        p[2] = (float)in[2];
    } break;
    case ALWAN_PIXEL_F64: {
        double *p = (double *)ptr;
        p[0] = (double)in[0];
        p[1] = (double)in[1];
        p[2] = (double)in[2];
    } break;
    }
}

ALWAN_INLINE void alwan__store1_typed(void *ptr,
                                       alwan_scalar val,
                                       alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        alwan_scalar v = val * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
        if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
        if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
        *(uint8_t *)ptr = (uint8_t)v;
    } break;
    case ALWAN_PIXEL_U16: {
        alwan_scalar v = val * ALWAN_LITERAL(65535.0) + ALWAN_LITERAL(0.5);
        if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
        if (v > ALWAN_LITERAL(65535.0)) v = ALWAN_LITERAL(65535.0);
        *(uint16_t *)ptr = (uint16_t)v;
    } break;
    case ALWAN_PIXEL_F32:
        *(float *)ptr = (float)val;
        break;
    case ALWAN_PIXEL_F64:
        *(double *)ptr = (double)val;
        break;
    }
}

/* ----------------------------------------------------------------
 * Typed tile load/store: void* typed buffer <-> alwan_simd_lane SoA
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load_tile_typed_3(alwan_simd_lane *ch0, alwan_simd_lane *ch1, alwan_simd_lane *ch2,
                                            void const *base, alwan_pixel_format fmt,
                                            size_t offset, size_t stride, size_t n) {
    for (size_t j = 0; j < n; j++) {
        alwan_scalar s[3];
        alwan__load3_typed(s, (char const *)base + (offset + j) * stride, fmt);
        ch0[j] = (alwan_simd_lane)s[0];
        ch1[j] = (alwan_simd_lane)s[1];
        ch2[j] = (alwan_simd_lane)s[2];
    }
}

ALWAN_INLINE void alwan__store_tile_typed_3(void *base, alwan_pixel_format fmt,
                                             size_t offset, size_t stride,
                                             alwan_simd_lane const *ch0, alwan_simd_lane const *ch1, alwan_simd_lane const *ch2,
                                             size_t n) {
    for (size_t j = 0; j < n; j++) {
        alwan_scalar d[3] = {(alwan_scalar)ch0[j], (alwan_scalar)ch1[j], (alwan_scalar)ch2[j]};
        alwan__store3_typed((char *)base + (offset + j) * stride, d, fmt);
    }
}

/* ----------------------------------------------------------------
 * Planar Tile Load: separate channel arrays -> flat SoA arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load_tile_planar3(alwan_simd_lane *ch0, alwan_simd_lane *ch1, alwan_simd_lane *ch2,
                                            alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                            size_t offset, size_t stride, size_t tile_count) {
    for (size_t j = 0; j < tile_count; j++) {
        ch0[j] = (alwan_simd_lane)*(alwan_scalar const *)((char const *)in0 + (offset + j) * stride);
        ch1[j] = (alwan_simd_lane)*(alwan_scalar const *)((char const *)in1 + (offset + j) * stride);
        ch2[j] = (alwan_simd_lane)*(alwan_scalar const *)((char const *)in2 + (offset + j) * stride);
    }
}

/* ----------------------------------------------------------------
 * Planar Tile Store: flat SoA arrays -> separate channel arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__store_tile_planar3(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                             size_t offset, size_t stride,
                                             alwan_simd_lane const *ch0, alwan_simd_lane const *ch1, alwan_simd_lane const *ch2,
                                             size_t tile_count) {
    for (size_t j = 0; j < tile_count; j++) {
        *(alwan_scalar *)((char *)out0 + (offset + j) * stride) = (alwan_scalar)ch0[j];
        *(alwan_scalar *)((char *)out1 + (offset + j) * stride) = (alwan_scalar)ch1[j];
        *(alwan_scalar *)((char *)out2 + (offset + j) * stride) = (alwan_scalar)ch2[j];
    }
}

/* ----------------------------------------------------------------
 * Typed planar load/store: void* typed channels <-> alwan_simd_lane SoA
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_scalar alwan__load1_typed(void const *ptr, alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return *(uint8_t  const *)ptr * ALWAN_LITERAL(1.0 / 255.0);
    case ALWAN_PIXEL_U16: return *(uint16_t const *)ptr * ALWAN_LITERAL(1.0 / 65535.0);
    case ALWAN_PIXEL_F32: return (alwan_scalar)*(float  const *)ptr;
    case ALWAN_PIXEL_F64: return (alwan_scalar)*(double const *)ptr;
    }
    return ALWAN_LITERAL(0.0);
}

ALWAN_INLINE size_t alwan__fmt_size(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 1;
    case ALWAN_PIXEL_U16: return 2;
    case ALWAN_PIXEL_F32: return 4;
    case ALWAN_PIXEL_F64: return 8;
    }
    return 0;
}

ALWAN_INLINE void alwan__load_tile_planar_typed_3(alwan_simd_lane *ch0, alwan_simd_lane *ch1, alwan_simd_lane *ch2,
                                                   void const *in0, void const *in1, void const *in2,
                                                   alwan_pixel_format fmt,
                                                   size_t offset, size_t stride, size_t n) {
    for (size_t j = 0; j < n; j++) {
        size_t byte_off = (offset + j) * stride;
        ch0[j] = (alwan_simd_lane)alwan__load1_typed((char const *)in0 + byte_off, fmt);
        ch1[j] = (alwan_simd_lane)alwan__load1_typed((char const *)in1 + byte_off, fmt);
        ch2[j] = (alwan_simd_lane)alwan__load1_typed((char const *)in2 + byte_off, fmt);
    }
}

ALWAN_INLINE void alwan__store_tile_planar_typed_3(void *out0, void *out1, void *out2,
                                                    alwan_pixel_format fmt,
                                                    size_t offset, size_t stride,
                                                    alwan_simd_lane const *ch0, alwan_simd_lane const *ch1, alwan_simd_lane const *ch2,
                                                    size_t n) {
    for (size_t j = 0; j < n; j++) {
        size_t byte_off = (offset + j) * stride;
        alwan__store1_typed((char *)out0 + byte_off, (alwan_scalar)ch0[j], fmt);
        alwan__store1_typed((char *)out1 + byte_off, (alwan_scalar)ch1[j], fmt);
        alwan__store1_typed((char *)out2 + byte_off, (alwan_scalar)ch2[j], fmt);
    }
}

/* ================================================================
 * SIMD Building Blocks
 *
 * Color-math helpers built from type-generic alwan_simd_* atomic ops.
 * Used by all map functions in their SIMD tile loops.
 * ================================================================ */

#if ALWAN_SIMD_WIDTH > 1

/* ----------------------------------------------------------------
 * 3x3 matrix multiply: M * [r,g,b] for W pixels simultaneously
 * Matrix is row-major, scalars broadcast from matrix elements.
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__mat3_mul_simd(alwan_simd *ox, alwan_simd *oy, alwan_simd *oz,
                                        alwan_mat3x3 const *m,
                                        alwan_simd r, alwan_simd g, alwan_simd b) {
    *ox = alwan_simd_fmadd(alwan_simd_set1((alwan_simd_lane)m->m[0]), r,
          alwan_simd_fmadd(alwan_simd_set1((alwan_simd_lane)m->m[1]), g,
          alwan_simd_mul(  alwan_simd_set1((alwan_simd_lane)m->m[2]), b)));
    *oy = alwan_simd_fmadd(alwan_simd_set1((alwan_simd_lane)m->m[3]), r,
          alwan_simd_fmadd(alwan_simd_set1((alwan_simd_lane)m->m[4]), g,
          alwan_simd_mul(  alwan_simd_set1((alwan_simd_lane)m->m[5]), b)));
    *oz = alwan_simd_fmadd(alwan_simd_set1((alwan_simd_lane)m->m[6]), r,
          alwan_simd_fmadd(alwan_simd_set1((alwan_simd_lane)m->m[7]), g,
          alwan_simd_mul(  alwan_simd_set1((alwan_simd_lane)m->m[8]), b)));
}

/* ----------------------------------------------------------------
 * CIE Lab f(t): t > delta^3 ? t^(1/3) : kappa*t + offset
 * delta = 6/29, delta^3 = 216/24389, kappa = 24389/27/116, offset = 16/116
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__lab_f_simd(alwan_simd t) {
    alwan_simd delta3 = alwan_simd_set1((alwan_simd_lane)(216.0 / 24389.0));
    alwan_simd kappa  = alwan_simd_set1((alwan_simd_lane)(24389.0 / (27.0 * 116.0)));
    alwan_simd offset = alwan_simd_set1((alwan_simd_lane)(16.0 / 116.0));

    alwan_simd cbrt_result   = alwan_simd_cbrt(t);
    alwan_simd linear_result = alwan_simd_fmadd(kappa, t, offset);

    alwan_simd_mask mask = alwan_simd_cmpgt(t, delta3);
    return alwan_simd_select(mask, cbrt_result, linear_result);
}

/* ----------------------------------------------------------------
 * CIE Lab f_inv(t): t > delta ? t^3 : kappa*(t - offset)
 * delta = 6/29, kappa = 3*delta^2 = 108/841
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__lab_f_inv_simd(alwan_simd t) {
    alwan_simd delta  = alwan_simd_set1((alwan_simd_lane)(6.0 / 29.0));
    alwan_simd kappa  = alwan_simd_set1((alwan_simd_lane)(108.0 / 841.0));
    alwan_simd offset = alwan_simd_set1((alwan_simd_lane)(16.0 / 116.0));

    alwan_simd cube_result   = alwan_simd_mul(t, alwan_simd_mul(t, t));
    alwan_simd linear_result = alwan_simd_mul(kappa, alwan_simd_sub(t, offset));

    alwan_simd_mask mask = alwan_simd_cmpgt(t, delta);
    return alwan_simd_select(mask, cube_result, linear_result);
}

/* ----------------------------------------------------------------
 * sRGB EOTF: encoded -> linear
 * V <= 0.04045 ? V/12.92 : ((V+0.055)/1.055)^2.4
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__srgb_eotf_simd(alwan_simd v) {
    alwan_simd thresh = alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_EOTF_THRESH);
    alwan_simd lo     = alwan_simd_mul(v, alwan_simd_set1((alwan_simd_lane)(1.0 / ALWAN_SRGB_LINEAR_GAIN)));
    alwan_simd hi_base = alwan_simd_mul(
        alwan_simd_add(v, alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_B)),
        alwan_simd_set1((alwan_simd_lane)(1.0 / ALWAN_SRGB_A)));
    alwan_simd hi     = alwan_simd_pow(hi_base, alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_GAMMA));

    alwan_simd_mask mask = alwan_simd_cmple(v, thresh);
    return alwan_simd_select(mask, lo, hi);
}

/* ----------------------------------------------------------------
 * sRGB OETF: linear -> encoded
 * V <= 0.0031308 ? 12.92*V : 1.055*V^(1/2.4) - 0.055
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__srgb_oetf_simd(alwan_simd v) {
    alwan_simd thresh = alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_OETF_THRESH);
    alwan_simd lo     = alwan_simd_mul(v, alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_LINEAR_GAIN));
    alwan_simd hi     = alwan_simd_sub(
        alwan_simd_mul(
            alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_A),
            alwan_simd_pow(v, alwan_simd_set1((alwan_simd_lane)(1.0 / ALWAN_SRGB_GAMMA)))),
        alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_B));

    alwan_simd_mask mask = alwan_simd_cmple(v, thresh);
    return alwan_simd_select(mask, lo, hi);
}

/* ----------------------------------------------------------------
 * PQ OETF (Standard SMPTE ST 2084): linear (0-10000 cd/m²) -> encoded
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__pq_oetf_simd(alwan_simd v) {
    alwan_simd const m1   = alwan_simd_set1((alwan_simd_lane)(2610.0 / 16384.0));
    alwan_simd const m2   = alwan_simd_set1((alwan_simd_lane)(2523.0 / 32.0));
    alwan_simd const c1   = alwan_simd_set1((alwan_simd_lane)(3424.0 / 4096.0));
    alwan_simd const c2   = alwan_simd_set1((alwan_simd_lane)(2413.0 / 128.0));
    alwan_simd const c3   = alwan_simd_set1((alwan_simd_lane)(2392.0 / 128.0));
    alwan_simd const inv10k = alwan_simd_set1((alwan_simd_lane)(1.0 / 10000.0));
    alwan_simd const zero = alwan_simd_zero();

    alwan_simd Y = alwan_simd_select(alwan_simd_cmplt(v, zero), zero, alwan_simd_mul(v, inv10k));
    alwan_simd Ym1 = alwan_simd_pow(Y, m1);
    alwan_simd num = alwan_simd_add(c1, alwan_simd_mul(c2, Ym1));
    alwan_simd den = alwan_simd_add(alwan_simd_set1((alwan_simd_lane)1.0), alwan_simd_mul(c3, Ym1));
    return alwan_simd_pow(alwan_simd_div(num, den), m2);
}

/* ----------------------------------------------------------------
 * PQ EOTF (Standard SMPTE ST 2084): encoded -> linear (0-10000 cd/m²)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__pq_eotf_simd(alwan_simd v) {
    alwan_simd const m1_inv = alwan_simd_set1((alwan_simd_lane)(16384.0 / 2610.0));
    alwan_simd const m2_inv = alwan_simd_set1((alwan_simd_lane)(32.0 / 2523.0));
    alwan_simd const c1   = alwan_simd_set1((alwan_simd_lane)(3424.0 / 4096.0));
    alwan_simd const c2   = alwan_simd_set1((alwan_simd_lane)(2413.0 / 128.0));
    alwan_simd const c3   = alwan_simd_set1((alwan_simd_lane)(2392.0 / 128.0));
    alwan_simd const ten_k = alwan_simd_set1((alwan_simd_lane)10000.0);
    alwan_simd const zero = alwan_simd_zero();

    alwan_simd E = alwan_simd_select(alwan_simd_cmplt(v, zero), zero, v);
    alwan_simd Ep = alwan_simd_pow(E, m2_inv);
    alwan_simd num = alwan_simd_sub(Ep, c1);
    num = alwan_simd_select(alwan_simd_cmplt(num, zero), zero, num);
    alwan_simd den = alwan_simd_sub(c2, alwan_simd_mul(c3, Ep));
    /* Avoid div-by-zero: if |den| < eps, result is 0 */
    alwan_simd const eps = alwan_simd_set1((alwan_simd_lane)ALWAN_MAP_PQ_DIV_GUARD);
    alwan_simd_mask safe = alwan_simd_cmpgt(alwan_simd_abs(den), eps);
    alwan_simd ratio = alwan_simd_select(safe, alwan_simd_div(num, den), zero);
    ratio = alwan_simd_select(alwan_simd_cmplt(ratio, zero), zero, ratio);
    return alwan_simd_mul(ten_k, alwan_simd_pow(ratio, m1_inv));
}

/* ----------------------------------------------------------------
 * JzAzBz-specific PQ OETF: uses JzAzBz constants (N, P, C1-C3)
 * linear -> encoded
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__pq_jz_oetf_simd(alwan_simd v) {
    alwan_simd const n_exp = alwan_simd_set1((alwan_simd_lane)0.1593017578125);    /* 2610/16384 */
    alwan_simd const p_exp = alwan_simd_set1((alwan_simd_lane)134.034375);          /* 1.7*2523/32 */
    alwan_simd const c1    = alwan_simd_set1((alwan_simd_lane)0.8359375);           /* 3424/4096 */
    alwan_simd const c2    = alwan_simd_set1((alwan_simd_lane)18.8515625);          /* 2413/128 */
    alwan_simd const c3    = alwan_simd_set1((alwan_simd_lane)18.6875);             /* 2392/128 */
    alwan_simd const inv10k = alwan_simd_set1((alwan_simd_lane)(1.0 / 10000.0));
    alwan_simd const zero  = alwan_simd_zero();

    alwan_simd_mask pos = alwan_simd_cmpgt(v, zero);
    alwan_simd clamped = alwan_simd_select(pos, v, zero);
    alwan_simd Yn = alwan_simd_pow(alwan_simd_mul(clamped, inv10k), n_exp);
    alwan_simd num = alwan_simd_add(c1, alwan_simd_mul(c2, Yn));
    alwan_simd den = alwan_simd_add(alwan_simd_set1((alwan_simd_lane)1.0), alwan_simd_mul(c3, Yn));
    alwan_simd result = alwan_simd_pow(alwan_simd_div(num, den), p_exp);
    return alwan_simd_select(pos, result, zero);
}

/* ----------------------------------------------------------------
 * JzAzBz-specific PQ EOTF: encoded -> linear
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__pq_jz_eotf_simd(alwan_simd v) {
    alwan_simd const n_inv = alwan_simd_set1((alwan_simd_lane)(1.0 / 0.1593017578125));
    alwan_simd const p_inv = alwan_simd_set1((alwan_simd_lane)(1.0 / 134.034375));
    alwan_simd const c1    = alwan_simd_set1((alwan_simd_lane)0.8359375);
    alwan_simd const c2    = alwan_simd_set1((alwan_simd_lane)18.8515625);
    alwan_simd const c3    = alwan_simd_set1((alwan_simd_lane)18.6875);
    alwan_simd const ten_k = alwan_simd_set1((alwan_simd_lane)10000.0);
    alwan_simd const zero  = alwan_simd_zero();
    alwan_simd const eps   = alwan_simd_set1((alwan_simd_lane)ALWAN_MAP_PQ_DIV_GUARD);

    alwan_simd_mask pos = alwan_simd_cmpgt(v, zero);
    alwan_simd enc = alwan_simd_select(pos, v, zero);
    alwan_simd Ep = alwan_simd_pow(enc, p_inv);
    alwan_simd num = alwan_simd_sub(Ep, c1);
    alwan_simd den = alwan_simd_sub(c2, alwan_simd_mul(c3, Ep));
    alwan_simd_mask safe = alwan_simd_cmpgt(alwan_simd_abs(den), eps);
    alwan_simd ratio = alwan_simd_select(safe, alwan_simd_div(num, den), zero);
    ratio = alwan_simd_select(alwan_simd_cmplt(ratio, zero), zero, ratio);
    alwan_simd result = alwan_simd_mul(ten_k, alwan_simd_pow(ratio, n_inv));
    return alwan_simd_select(pos, result, zero);
}

/* ----------------------------------------------------------------
 * HLG OETF: linear -> encoded
 * L <= 1/12: sqrt(3*L)
 * L > 1/12:  a*ln(12*L - b) + c
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__hlg_oetf_simd(alwan_simd v) {
    alwan_simd const a_     = alwan_simd_set1((alwan_simd_lane)0.17883277);
    alwan_simd const b_     = alwan_simd_set1((alwan_simd_lane)(1.0 - 4.0 * 0.17883277));
    alwan_simd const c_     = alwan_simd_set1((alwan_simd_lane)0.55991072952956202); /* 0.5 - a*ln(4a) */
    alwan_simd const three  = alwan_simd_set1((alwan_simd_lane)3.0);
    alwan_simd const twelve = alwan_simd_set1((alwan_simd_lane)12.0);
    alwan_simd const thresh = alwan_simd_set1((alwan_simd_lane)(1.0 / 12.0));
    alwan_simd const zero   = alwan_simd_zero();

    alwan_simd L = alwan_simd_select(alwan_simd_cmplt(v, zero), zero, v);
    alwan_simd lo = alwan_simd_sqrt(alwan_simd_mul(three, L));
    alwan_simd hi = alwan_simd_fmadd(a_, alwan_simd_log(alwan_simd_sub(alwan_simd_mul(twelve, L), b_)), c_);
    return alwan_simd_select(alwan_simd_cmple(L, thresh), lo, hi);
}

/* ----------------------------------------------------------------
 * HLG inverse OETF (NOT EOTF): encoded -> linear (no system gamma)
 * E <= 0.5: E^2/3
 * E > 0.5:  (exp((E-c)/a) + b) / 12
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__hlg_eotf_simd(alwan_simd v) {
    alwan_simd const b_     = alwan_simd_set1((alwan_simd_lane)(1.0 - 4.0 * 0.17883277));
    alwan_simd const c_     = alwan_simd_set1((alwan_simd_lane)0.55991072952956202); /* 0.5 - a*ln(4a) */
    alwan_simd const inv_a  = alwan_simd_set1((alwan_simd_lane)(1.0 / 0.17883277));
    alwan_simd const three  = alwan_simd_set1((alwan_simd_lane)3.0);
    alwan_simd const twelve = alwan_simd_set1((alwan_simd_lane)12.0);
    alwan_simd const half   = alwan_simd_set1((alwan_simd_lane)0.5);
    alwan_simd const zero   = alwan_simd_zero();

    alwan_simd E = alwan_simd_select(alwan_simd_cmplt(v, zero), zero, v);
    alwan_simd lo = alwan_simd_div(alwan_simd_mul(E, E), three);
    alwan_simd hi = alwan_simd_div(
        alwan_simd_add(alwan_simd_exp(alwan_simd_mul(alwan_simd_sub(E, c_), inv_a)), b_),
        twelve);
    return alwan_simd_select(alwan_simd_cmple(E, half), lo, hi);
}

/* ----------------------------------------------------------------
 * SIMD min3 / max3 helpers (used by HSV, HSL, HWB kernels)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__simd_min3(alwan_simd a, alwan_simd b, alwan_simd c) {
    return alwan_simd_min(a, alwan_simd_min(b, c));
}

ALWAN_INLINE alwan_simd alwan__simd_max3(alwan_simd a, alwan_simd b, alwan_simd c) {
    return alwan_simd_max(a, alwan_simd_max(b, c));
}

#endif /* ALWAN_SIMD_WIDTH > 1 */

/* ----------------------------------------------------------------
 * Tile kernel generation macros
 * ---------------------------------------------------------------- */

#define ALWAN_TILE_KERNEL_3TO3(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
static void name(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2, \
                 alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2, size_t n) { \
    for (size_t j = 0; j < n; j++) { \
        InT s_ = {(alwan_scalar)i0[j], (alwan_scalar)i1[j], (alwan_scalar)i2[j]}; \
        OutT d_; \
        core(&d_, &s_); \
        o0[j] = (alwan_simd_lane)d_.fo0; o1[j] = (alwan_simd_lane)d_.fo1; o2[j] = (alwan_simd_lane)d_.fo2; \
    } \
}

#define ALWAN_TILE_KERNEL_3TO3_STATUS(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
static int name(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2, \
                alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2, size_t n) { \
    for (size_t j = 0; j < n; j++) { \
        InT s_ = {(alwan_scalar)i0[j], (alwan_scalar)i1[j], (alwan_scalar)i2[j]}; \
        OutT d_; \
        int st_ = core(&d_, &s_); \
        if (st_ != ALWAN_OK) return st_; \
        o0[j] = (alwan_simd_lane)d_.fo0; o1[j] = (alwan_simd_lane)d_.fo1; o2[j] = (alwan_simd_lane)d_.fo2; \
    } \
    return ALWAN_OK; \
}

/* ----------------------------------------------------------------
 * Tiled loop body macro (for simple 3->3 kernels)
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_TILED(in_base, in_s, out_base, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
            alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_aos3(ci0_, ci1_, ci2_, (in_base), off_, (in_s), tile_); \
            kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
            alwan__store_tile_aos3((out_base), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

/* Same but kernel returns status */
#define ALWAN_MAP3_TILED_STATUS(in_base, in_s, out_base, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
            alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_aos3(ci0_, ci1_, ci2_, (in_base), off_, (in_s), tile_); \
            { int st_ = kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
              if (st_ != ALWAN_OK) return st_; } \
            alwan__store_tile_aos3((out_base), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

/* _ex macro patterns for typed map functions */

#define ALWAN_MAP3_EX(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        core(&dst_, &src_); \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_EX_WHITE(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         alwan_xyz const *white_xyz, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        core(&dst_, &src_, white_xyz); \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_EX_PQ(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         int use_pq, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        core(&dst_, &src_, use_pq); \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_EX_STATUS(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        int st_ = core(&dst_, &src_); \
        if (st_ != ALWAN_OK) return st_; \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

/* ----------------------------------------------------------------
 * _ex macro: core returns value (not pointer-based), no extra params
 * core signature: OutT core(InT src)
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_EX_V(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_); \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

/* ----------------------------------------------------------------
 * _ex macro: core returns value, with white point param
 * core signature: OutT core(InT src, InT2 white)
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_EX_V_WHITE(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         alwan_xyz const *white_xyz, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_, *white_xyz); \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

/* ----------------------------------------------------------------
 * _ex macro: core returns value, with int param
 * core signature: OutT core(InT src, int param)
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_EX_V_INT(name, InT, OutT, core, extra_type, extra_name, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         extra_type extra_name, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_, extra_name); \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

/* ----------------------------------------------------------------
 * _ex macro: core returns value, with alwan_scalar param
 * core signature: OutT core(InT src, alwan_scalar param)
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_EX_V_SCALAR(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out, alwan_pixel_format out_fmt, \
         void const *in, alwan_pixel_format in_fmt, \
         alwan_scalar param, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        alwan__load3_typed(sv_, (char const *)in + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_, param); \
        alwan_scalar dv_[3] = {dst_.fo0, dst_.fo1, dst_.fo2}; \
        alwan__store3_typed((char *)out + ii_ * out_stride, dv_, out_fmt); \
    } \
    return ALWAN_OK; \
}

/* ----------------------------------------------------------------
 * Scalar planar _map_interleave generation macros
 *
 * These generate _map_planar function bodies from core _v functions.
 * For SIMD-vectorized functions, use ALWAN_MAP3_TILED_PLANAR instead
 * with an extracted tile kernel.
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_PLANAR_V(name, InT, OutT, core, fi0,fi1,fi2, fo0,fo1,fo2) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t i_ = 0; i_ < count; i_++) { \
        InT src_ = { \
            *(alwan_scalar const *)((char const *)in0 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in1 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in2 + i_ * in_stride) \
        }; \
        OutT dst_ = core(src_); \
        *(alwan_scalar *)((char *)out0 + i_ * out_stride) = dst_.fo0; \
        *(alwan_scalar *)((char *)out1 + i_ * out_stride) = dst_.fo1; \
        *(alwan_scalar *)((char *)out2 + i_ * out_stride) = dst_.fo2; \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_V_WHITE(name, InT, OutT, core, fi0,fi1,fi2, fo0,fo1,fo2) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         alwan_xyz const *white_xyz, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    for (size_t i_ = 0; i_ < count; i_++) { \
        InT src_ = { \
            *(alwan_scalar const *)((char const *)in0 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in1 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in2 + i_ * in_stride) \
        }; \
        OutT dst_ = core(src_, *white_xyz); \
        *(alwan_scalar *)((char *)out0 + i_ * out_stride) = dst_.fo0; \
        *(alwan_scalar *)((char *)out1 + i_ * out_stride) = dst_.fo1; \
        *(alwan_scalar *)((char *)out2 + i_ * out_stride) = dst_.fo2; \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_V_INT(name, InT, OutT, core, extra_type, extra_name, fi0,fi1,fi2, fo0,fo1,fo2) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         extra_type extra_name, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t i_ = 0; i_ < count; i_++) { \
        InT src_ = { \
            *(alwan_scalar const *)((char const *)in0 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in1 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in2 + i_ * in_stride) \
        }; \
        OutT dst_ = core(src_, extra_name); \
        *(alwan_scalar *)((char *)out0 + i_ * out_stride) = dst_.fo0; \
        *(alwan_scalar *)((char *)out1 + i_ * out_stride) = dst_.fo1; \
        *(alwan_scalar *)((char *)out2 + i_ * out_stride) = dst_.fo2; \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_V_SCALAR(name, InT, OutT, core, fi0,fi1,fi2, fo0,fo1,fo2) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         alwan_scalar param, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t i_ = 0; i_ < count; i_++) { \
        InT src_ = { \
            *(alwan_scalar const *)((char const *)in0 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in1 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in2 + i_ * in_stride) \
        }; \
        OutT dst_ = core(src_, param); \
        *(alwan_scalar *)((char *)out0 + i_ * out_stride) = dst_.fo0; \
        *(alwan_scalar *)((char *)out1 + i_ * out_stride) = dst_.fo1; \
        *(alwan_scalar *)((char *)out2 + i_ * out_stride) = dst_.fo2; \
    } \
    return ALWAN_OK; \
}

/* Pointer-based core with status return */
#define ALWAN_MAP3_PLANAR_STATUS(name, InT, OutT, core, fi0,fi1,fi2, fo0,fo1,fo2) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t i_ = 0; i_ < count; i_++) { \
        InT src_ = { \
            *(alwan_scalar const *)((char const *)in0 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in1 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in2 + i_ * in_stride) \
        }; \
        OutT dst_; \
        int st_ = core(&dst_, &src_); \
        if (st_ != ALWAN_OK) return st_; \
        *(alwan_scalar *)((char *)out0 + i_ * out_stride) = dst_.fo0; \
        *(alwan_scalar *)((char *)out1 + i_ * out_stride) = dst_.fo1; \
        *(alwan_scalar *)((char *)out2 + i_ * out_stride) = dst_.fo2; \
    } \
    return ALWAN_OK; \
}

/* Pointer-based core with white_xyz and status return */
#define ALWAN_MAP3_PLANAR_WHITE(name, InT, OutT, core, fi0,fi1,fi2, fo0,fo1,fo2) \
int name(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2, \
         alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2, \
         alwan_xyz const *white_xyz, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    for (size_t i_ = 0; i_ < count; i_++) { \
        InT src_ = { \
            *(alwan_scalar const *)((char const *)in0 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in1 + i_ * in_stride), \
            *(alwan_scalar const *)((char const *)in2 + i_ * in_stride) \
        }; \
        OutT dst_; \
        core(&dst_, &src_, white_xyz); \
        *(alwan_scalar *)((char *)out0 + i_ * out_stride) = dst_.fo0; \
        *(alwan_scalar *)((char *)out1 + i_ * out_stride) = dst_.fo1; \
        *(alwan_scalar *)((char *)out2 + i_ * out_stride) = dst_.fo2; \
    } \
    return ALWAN_OK; \
}

/* ----------------------------------------------------------------
 * Planar tiled loop macros
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_TILED_PLANAR(in0, in1, in2, in_s, out0, out1, out2, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
            alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_planar3(ci0_, ci1_, ci2_, (in0), (in1), (in2), off_, (in_s), tile_); \
            kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
            alwan__store_tile_planar3((out0), (out1), (out2), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

#define ALWAN_MAP3_TILED_PLANAR_STATUS(in0, in1, in2, in_s, out0, out1, out2, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
            alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_planar3(ci0_, ci1_, ci2_, (in0), (in1), (in2), off_, (in_s), tile_); \
            { int st_ = kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
              if (st_ != ALWAN_OK) return st_; } \
            alwan__store_tile_planar3((out0), (out1), (out2), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

/* ----------------------------------------------------------------
 * Planar _ex macro patterns
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_PLANAR_EX(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    size_t isz_ = alwan__fmt_size(in_fmt); \
    size_t osz_ = alwan__fmt_size(out_fmt); \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        core(&dst_, &src_); \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    (void)isz_; (void)osz_; \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_EX_WHITE(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         alwan_xyz const *white_xyz, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        core(&dst_, &src_, white_xyz); \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_EX_PQ(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         int use_pq, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        core(&dst_, &src_, use_pq); \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_EX_STATUS(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_; \
        int st_ = core(&dst_, &src_); \
        if (st_ != ALWAN_OK) return st_; \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_EX_V(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_); \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_EX_V_WHITE(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         alwan_xyz const *white_xyz, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_, *white_xyz); \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_EX_V_INT(name, InT, OutT, core, extra_type, extra_name, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         extra_type extra_name, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_, extra_name); \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    return ALWAN_OK; \
}

#define ALWAN_MAP3_PLANAR_EX_V_SCALAR(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
int name(void *out0, void *out1, void *out2, alwan_pixel_format out_fmt, \
         void const *in0, void const *in1, void const *in2, alwan_pixel_format in_fmt, \
         alwan_scalar param, \
         size_t count, size_t in_stride, size_t out_stride) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    for (size_t ii_ = 0; ii_ < count; ii_++) { \
        alwan_scalar sv_[3]; \
        sv_[0] = alwan__load1_typed((char const *)in0 + ii_ * in_stride, in_fmt); \
        sv_[1] = alwan__load1_typed((char const *)in1 + ii_ * in_stride, in_fmt); \
        sv_[2] = alwan__load1_typed((char const *)in2 + ii_ * in_stride, in_fmt); \
        InT src_ = {sv_[0], sv_[1], sv_[2]}; \
        OutT dst_ = core(src_, param); \
        alwan__store1_typed((char *)out0 + ii_ * out_stride, dst_.fo0, out_fmt); \
        alwan__store1_typed((char *)out1 + ii_ * out_stride, dst_.fo1, out_fmt); \
        alwan__store1_typed((char *)out2 + ii_ * out_stride, dst_.fo2, out_fmt); \
    } \
    return ALWAN_OK; \
}

#endif /* ALWAN_MAP_INTERNAL_H */
