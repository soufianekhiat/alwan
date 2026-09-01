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
#include <string.h>

/* ================================================================
 * Type-Generic SIMD Aliases (always f64)
 * ================================================================ */

    #define ALWAN_SIMD_WIDTH        ALWAN_SIMD_F64_WIDTH
    typedef double                  alwan_simd_lane;
    typedef alwan_simd_f64          alwan_simd;
    typedef alwan_simd_f64_mask     alwan_simd_mask;
    #define alwan_simd_set1(v)      alwan_simd_f64_set1(v)
    #define alwan_simd_zero         alwan_simd_f64_zero
    #define alwan_simd_load         alwan_simd_f64_load
    #define alwan_simd_store        alwan_simd_f64_store
    #define alwan_simd_loadu        alwan_simd_f64_loadu
    #define alwan_simd_storeu       alwan_simd_f64_storeu
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
    #define alwan_simd_pow24        alwan_simd_f64_pow24
    #define alwan_simd_pow_inv24    alwan_simd_f64_pow_inv24
    #define alwan_simd_cbrt_fast    alwan_simd_f64_cbrt_fast
    #define alwan_simd_deinterleave3 alwan_simd_f64_deinterleave3
    #define alwan_simd_interleave3   alwan_simd_f64_interleave3
    #define alwan_simd_mask_all_set  alwan_simd_f64_mask_all_set

/* ----------------------------------------------------------------
 * Tile Constants
 *
 * Tile size: 128 x 32 = 4096 pixels
 * SoA scratch: 3 channels x 4096 x sizeof(alwan_simd_lane)
 *   f32: 48 KB -> fits in L1
 *   f64: 96 KB -> fits in L2, partially in L1
 * ---------------------------------------------------------------- */

/* Tile: 64 x 32 = 2048 pixels, 48 KB f64 scratch */
#define ALWAN_TILE_W       64
#define ALWAN_TILE_H       32
#define ALWAN_TILE_PIXELS (ALWAN_TILE_W * ALWAN_TILE_H)

/* Pixel format matching alwan_f64 (always double) */
#define ALWAN_NATIVE_PIXEL_FMT  ALWAN_PIXEL_F64

/* Near-zero guards for SIMD divide-by-zero protection */
#define ALWAN_MAP_DIV_GUARD    1e-10   /* For chromaticity / xyY denominators */
#define ALWAN_MAP_PQ_DIV_GUARD 1e-30   /* For PQ EOTF denominator (tiny, avoids denormals) */

/* ----------------------------------------------------------------
 * Tile Load: AoS strided input -> flat SoA arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load_tile_aos3(alwan_simd_lane *ch0, alwan_simd_lane *ch1, alwan_simd_lane *ch2,
                                         alwan_f64 const *base, size_t offset,
                                         size_t stride, size_t tile_count) {
    if (stride == 3 * sizeof(alwan_f64)) {
        /* Packed AoS: use SIMD deinterleave for contiguous RGB data */
        alwan_simd_lane const *src = (alwan_simd_lane const *)base + offset * 3;
        size_t j = 0;
#if ALWAN_SIMD_WIDTH > 1
        for (; j + ALWAN_SIMD_WIDTH <= tile_count; j += ALWAN_SIMD_WIDTH) {
            alwan_simd c0, c1, c2;
            alwan_simd_deinterleave3(src + j * 3, &c0, &c1, &c2);
            alwan_simd_store(&ch0[j], c0);
            alwan_simd_store(&ch1[j], c1);
            alwan_simd_store(&ch2[j], c2);
        }
#endif
        for (; j < tile_count; j++) {
            ch0[j] = src[j * 3 + 0];
            ch1[j] = src[j * 3 + 1];
            ch2[j] = src[j * 3 + 2];
        }
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            alwan_f64 const *p = (alwan_f64 const *)((char const *)base + (offset + j) * stride);
            ch0[j] = (alwan_simd_lane)p[0];
            ch1[j] = (alwan_simd_lane)p[1];
            ch2[j] = (alwan_simd_lane)p[2];
        }
    }
}

/* ----------------------------------------------------------------
 * Tile Store: flat SoA arrays -> AoS strided output
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__store_tile_aos3(alwan_f64 *base, size_t offset, size_t stride,
                                          alwan_simd_lane const *ch0, alwan_simd_lane const *ch1, alwan_simd_lane const *ch2,
                                          size_t tile_count) {
    if (stride == 3 * sizeof(alwan_f64)) {
        /* Packed AoS: use SIMD interleave for contiguous RGB data */
        alwan_simd_lane *dst = (alwan_simd_lane *)base + offset * 3;
        size_t j = 0;
#if ALWAN_SIMD_WIDTH > 1
        for (; j + ALWAN_SIMD_WIDTH <= tile_count; j += ALWAN_SIMD_WIDTH) {
            alwan_simd c0 = alwan_simd_load(&ch0[j]);
            alwan_simd c1 = alwan_simd_load(&ch1[j]);
            alwan_simd c2 = alwan_simd_load(&ch2[j]);
            alwan_simd_interleave3(dst + j * 3, c0, c1, c2);
        }
#endif
        for (; j < tile_count; j++) {
            dst[j * 3 + 0] = ch0[j];
            dst[j * 3 + 1] = ch1[j];
            dst[j * 3 + 2] = ch2[j];
        }
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            alwan_f64 *p = (alwan_f64 *)((char *)base + (offset + j) * stride);
            p[0] = (alwan_f64)ch0[j];
            p[1] = (alwan_f64)ch1[j];
            p[2] = (alwan_f64)ch2[j];
        }
    }
}

/* ----------------------------------------------------------------
 * IEEE 754 half-float (binary16) scalar conversion
 * ---------------------------------------------------------------- */

ALWAN_INLINE float alwan__f16_to_f32(uint16_t h) {
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp  = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x03FFu;
    uint32_t f;
    if (exp == 0) {
        if (mant == 0) { f = sign; }
        else {
            exp = 1;
            while (!(mant & 0x0400u)) { mant <<= 1; exp--; }
            mant &= 0x03FFu;
            f = sign | ((exp + 127u - 15u) << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        f = sign | 0x7F800000u | (mant << 13);
    } else {
        f = sign | ((exp + 127u - 15u) << 23) | (mant << 13);
    }
    float result;
    memcpy(&result, &f, sizeof(float));
    return result;
}

ALWAN_INLINE uint16_t alwan__f32_to_f16(float val) {
    uint32_t f;
    memcpy(&f, &val, sizeof(float));
    uint32_t sign = (f >> 16) & 0x8000u;
    int32_t exp = (int32_t)((f >> 23) & 0xFFu) - 127 + 15;
    uint32_t mant = f & 0x007FFFFFu;
    if (exp <= 0) {
        if (exp < -10) return (uint16_t)sign;
        mant |= 0x00800000u;
        return (uint16_t)(sign | (mant >> (14 - exp)));
    }
    if (exp >= 31) return (uint16_t)(sign | 0x7C00u);
    return (uint16_t)(sign | ((uint32_t)exp << 10) | (mant >> 13));
}

/* ----------------------------------------------------------------
 * Typed per-element load/store (for _ex functions scalar fallback)
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load3_typed(alwan_f64 out[3],
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
        out[0] = (alwan_f64)p[0];
        out[1] = (alwan_f64)p[1];
        out[2] = (alwan_f64)p[2];
    } break;
    case ALWAN_PIXEL_F64: {
        double const *p = (double const *)ptr;
        out[0] = (alwan_f64)p[0];
        out[1] = (alwan_f64)p[1];
        out[2] = (alwan_f64)p[2];
    } break;
    case ALWAN_PIXEL_F16: {
        uint16_t const *p = (uint16_t const *)ptr;
        out[0] = (alwan_f64)alwan__f16_to_f32(p[0]);
        out[1] = (alwan_f64)alwan__f16_to_f32(p[1]);
        out[2] = (alwan_f64)alwan__f16_to_f32(p[2]);
    } break;
    }
}

ALWAN_INLINE void alwan__store3_typed(void *ptr,
                                       alwan_f64 const in[3],
                                       alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        uint8_t *p = (uint8_t *)ptr;
        for (int c = 0; c < 3; c++) {
            alwan_f64 v = in[c] * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
            if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
            if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
            p[c] = (uint8_t)v;
        }
    } break;
    case ALWAN_PIXEL_U16: {
        uint16_t *p = (uint16_t *)ptr;
        for (int c = 0; c < 3; c++) {
            alwan_f64 v = in[c] * ALWAN_LITERAL(65535.0) + ALWAN_LITERAL(0.5);
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
    case ALWAN_PIXEL_F16: {
        uint16_t *p = (uint16_t *)ptr;
        p[0] = alwan__f32_to_f16((float)in[0]);
        p[1] = alwan__f32_to_f16((float)in[1]);
        p[2] = alwan__f32_to_f16((float)in[2]);
    } break;
    }
}

ALWAN_INLINE void alwan__store1_typed(void *ptr,
                                       alwan_f64 val,
                                       alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        alwan_f64 v = val * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
        if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
        if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
        *(uint8_t *)ptr = (uint8_t)v;
    } break;
    case ALWAN_PIXEL_U16: {
        alwan_f64 v = val * ALWAN_LITERAL(65535.0) + ALWAN_LITERAL(0.5);
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
    case ALWAN_PIXEL_F16:
        *(uint16_t *)ptr = alwan__f32_to_f16((float)val);
        break;
    }
}

/* ----------------------------------------------------------------
 * Typed tile load/store: void* typed buffer <-> alwan_simd_lane SoA
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load_tile_typed_3(alwan_simd_lane *ch0, alwan_simd_lane *ch1, alwan_simd_lane *ch2,
                                            void const *base, alwan_pixel_format fmt,
                                            size_t offset, size_t stride, size_t n) {
    /* Fast path: native scalar format -> use SIMD deinterleave */
    if (fmt == ALWAN_PIXEL_F64) {
        alwan__load_tile_aos3(ch0, ch1, ch2, (alwan_f64 const *)base, offset, stride, n);
        return;
    }
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        alwan_simd_lane const inv = (alwan_simd_lane)(1.0 / 255.0);
        if (stride == 3) {
            uint8_t const *src = (uint8_t const *)base + offset * 3;
            j = 0;
#if defined(__AVX2__)
            {
                __m128i const shuf_r = _mm_setr_epi8(0,3,6,9, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1);
                __m128i const shuf_g = _mm_setr_epi8(1,4,7,10, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1);
                __m128i const shuf_b = _mm_setr_epi8(2,5,8,11, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1);
                __m256d const inv256d = _mm256_set1_pd(1.0 / 255.0);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw = _mm_loadu_si128((const __m128i *)(src + j * 3));
                    __m128i ri32 = _mm_cvtepu8_epi32(_mm_shuffle_epi8(raw, shuf_r));
                    __m128i gi32 = _mm_cvtepu8_epi32(_mm_shuffle_epi8(raw, shuf_g));
                    __m128i bi32 = _mm_cvtepu8_epi32(_mm_shuffle_epi8(raw, shuf_b));
                    _mm256_storeu_pd(&ch0[j], _mm256_mul_pd(_mm256_cvtepi32_pd(ri32), inv256d));
                    _mm256_storeu_pd(&ch1[j], _mm256_mul_pd(_mm256_cvtepi32_pd(gi32), inv256d));
                    _mm256_storeu_pd(&ch2[j], _mm256_mul_pd(_mm256_cvtepi32_pd(bi32), inv256d));
                }
            }
#endif
            for (; j < n; j++) {
                ch0[j] = (alwan_simd_lane)src[j * 3 + 0] * inv;
                ch1[j] = (alwan_simd_lane)src[j * 3 + 1] * inv;
                ch2[j] = (alwan_simd_lane)src[j * 3 + 2] * inv;
            }
        } else {
            for (j = 0; j < n; j++) {
                uint8_t const *p = (uint8_t const *)((char const *)base + (offset + j) * stride);
                ch0[j] = (alwan_simd_lane)p[0] * inv;
                ch1[j] = (alwan_simd_lane)p[1] * inv;
                ch2[j] = (alwan_simd_lane)p[2] * inv;
            }
        }
        break;
    }
    case ALWAN_PIXEL_U16: {
        alwan_simd_lane const inv = (alwan_simd_lane)(1.0 / 65535.0);
        for (j = 0; j < n; j++) {
            uint16_t const *p = (uint16_t const *)((char const *)base + (offset + j) * stride);
            ch0[j] = (alwan_simd_lane)p[0] * inv;
            ch1[j] = (alwan_simd_lane)p[1] * inv;
            ch2[j] = (alwan_simd_lane)p[2] * inv;
        }
        break;
    }
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++) {
            float const *p = (float const *)((char const *)base + (offset + j) * stride);
            ch0[j] = (alwan_simd_lane)p[0];
            ch1[j] = (alwan_simd_lane)p[1];
            ch2[j] = (alwan_simd_lane)p[2];
        }
        break;
    }
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++) {
            double const *p = (double const *)((char const *)base + (offset + j) * stride);
            ch0[j] = (alwan_simd_lane)p[0];
            ch1[j] = (alwan_simd_lane)p[1];
            ch2[j] = (alwan_simd_lane)p[2];
        }
        break;
    }
    case ALWAN_PIXEL_F16: {
        if (stride == 6) {
            uint16_t const *src_u16 = (uint16_t const *)base + offset * 3;
            j = 0;
#if defined(__AVX2__)
            {
                for (; j + 4 <= n; j += 4) {
                    uint16_t const *sp = src_u16 + j * 3;
                    __m128i h0 = _mm_loadu_si128((const __m128i *)sp);
                    __m128i h1 = _mm_loadl_epi64((const __m128i *)(sp + 8));
                    __m128 f0 = _mm_cvtph_ps(h0);
                    __m128 f1 = _mm_cvtph_ps(_mm_srli_si128(h0, 8));
                    __m128 f2 = _mm_cvtph_ps(h1);
                    __m128 t0 = _mm_shuffle_ps(f0, f1, _MM_SHUFFLE(1, 0, 2, 1));
                    __m128 t1 = _mm_shuffle_ps(f1, f2, _MM_SHUFFLE(2, 1, 3, 2));
                    __m128 rf = _mm_shuffle_ps(f0, t1, _MM_SHUFFLE(2, 0, 3, 0));
                    __m128 gf = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(3, 1, 2, 0));
                    __m128 bf = _mm_shuffle_ps(t0, f2, _MM_SHUFFLE(3, 0, 3, 1));
                    _mm256_storeu_pd(&ch0[j], _mm256_cvtps_pd(rf));
                    _mm256_storeu_pd(&ch1[j], _mm256_cvtps_pd(gf));
                    _mm256_storeu_pd(&ch2[j], _mm256_cvtps_pd(bf));
                }
            }
#endif
            for (; j < n; j++) {
                ch0[j] = (alwan_simd_lane)alwan__f16_to_f32(src_u16[j * 3 + 0]);
                ch1[j] = (alwan_simd_lane)alwan__f16_to_f32(src_u16[j * 3 + 1]);
                ch2[j] = (alwan_simd_lane)alwan__f16_to_f32(src_u16[j * 3 + 2]);
            }
        } else {
            for (j = 0; j < n; j++) {
                uint16_t const *p = (uint16_t const *)((char const *)base + (offset + j) * stride);
                ch0[j] = (alwan_simd_lane)alwan__f16_to_f32(p[0]);
                ch1[j] = (alwan_simd_lane)alwan__f16_to_f32(p[1]);
                ch2[j] = (alwan_simd_lane)alwan__f16_to_f32(p[2]);
            }
        }
        break;
    }
    default: break;
    }
}

ALWAN_INLINE void alwan__store_tile_typed_3(void *base, alwan_pixel_format fmt,
                                             size_t offset, size_t stride,
                                             alwan_simd_lane const *ch0, alwan_simd_lane const *ch1, alwan_simd_lane const *ch2,
                                             size_t n) {
    /* Fast path: native scalar format -> use SIMD interleave */
    if (fmt == ALWAN_PIXEL_F64) {
        alwan__store_tile_aos3((alwan_f64 *)base, offset, stride, ch0, ch1, ch2, n);
        return;
    }
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        if (stride == 3) {
            uint8_t *dst_u8 = (uint8_t *)base + offset * 3;
            j = 0;
#if defined(__AVX2__)
            {
                __m128 const scale128 = _mm_set1_ps(255.0f);
                __m128 const half128 = _mm_set1_ps(0.5f);
                __m128i const shuf_out = _mm_setr_epi8(0,4,8, 1,5,9, 2,6,10, 3,7,11, -1,-1,-1,-1);
                for (; j + 4 <= n; j += 4) {
                    __m128 rf = _mm256_cvtpd_ps(_mm256_loadu_pd(&ch0[j]));
                    __m128 gf = _mm256_cvtpd_ps(_mm256_loadu_pd(&ch1[j]));
                    __m128 bf = _mm256_cvtpd_ps(_mm256_loadu_pd(&ch2[j]));
                    __m128i ri32 = _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(rf, scale128), half128));
                    __m128i gi32 = _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(gf, scale128), half128));
                    __m128i bi32 = _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(bf, scale128), half128));
                    __m128i rg16 = _mm_packs_epi32(ri32, gi32);
                    __m128i b_16 = _mm_packs_epi32(bi32, _mm_setzero_si128());
                    __m128i rgb8 = _mm_packus_epi16(rg16, b_16);
                    __m128i result = _mm_shuffle_epi8(rgb8, shuf_out);
                    _mm_storel_epi64((__m128i *)(dst_u8 + j * 3), result);
                    { uint32_t tmp = (uint32_t)_mm_extract_epi32(result, 2);
                      memcpy(dst_u8 + j * 3 + 8, &tmp, 4); }
                }
            }
#endif
            for (; j < n; j++) {
                dst_u8[j * 3 + 0] = (uint8_t)(ch0[j] * (alwan_simd_lane)255.0 + (alwan_simd_lane)0.5);
                dst_u8[j * 3 + 1] = (uint8_t)(ch1[j] * (alwan_simd_lane)255.0 + (alwan_simd_lane)0.5);
                dst_u8[j * 3 + 2] = (uint8_t)(ch2[j] * (alwan_simd_lane)255.0 + (alwan_simd_lane)0.5);
            }
        } else {
            for (j = 0; j < n; j++) {
                uint8_t *p = (uint8_t *)((char *)base + (offset + j) * stride);
                p[0] = (uint8_t)(ch0[j] * (alwan_simd_lane)255.0 + (alwan_simd_lane)0.5);
                p[1] = (uint8_t)(ch1[j] * (alwan_simd_lane)255.0 + (alwan_simd_lane)0.5);
                p[2] = (uint8_t)(ch2[j] * (alwan_simd_lane)255.0 + (alwan_simd_lane)0.5);
            }
        }
        break;
    }
    case ALWAN_PIXEL_U16: {
        for (j = 0; j < n; j++) {
            uint16_t *p = (uint16_t *)((char *)base + (offset + j) * stride);
            p[0] = (uint16_t)(ch0[j] * (alwan_simd_lane)65535.0 + (alwan_simd_lane)0.5);
            p[1] = (uint16_t)(ch1[j] * (alwan_simd_lane)65535.0 + (alwan_simd_lane)0.5);
            p[2] = (uint16_t)(ch2[j] * (alwan_simd_lane)65535.0 + (alwan_simd_lane)0.5);
        }
        break;
    }
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++) {
            float *p = (float *)((char *)base + (offset + j) * stride);
            p[0] = (float)ch0[j];
            p[1] = (float)ch1[j];
            p[2] = (float)ch2[j];
        }
        break;
    }
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++) {
            double *p = (double *)((char *)base + (offset + j) * stride);
            p[0] = (double)ch0[j];
            p[1] = (double)ch1[j];
            p[2] = (double)ch2[j];
        }
        break;
    }
    case ALWAN_PIXEL_F16: {
        if (stride == 6) {
            uint16_t *dst_u16 = (uint16_t *)base + offset * 3;
            j = 0;
#if defined(__AVX2__)
            {
                for (; j + 4 <= n; j += 4) {
                    __m128 rf = _mm256_cvtpd_ps(_mm256_loadu_pd(&ch0[j]));
                    __m128 gf = _mm256_cvtpd_ps(_mm256_loadu_pd(&ch1[j]));
                    __m128 bf = _mm256_cvtpd_ps(_mm256_loadu_pd(&ch2[j]));
                    /* SoA -> AoS interleave */
                    __m128 u0 = _mm_unpacklo_ps(rf, gf);
                    __m128 u1 = _mm_unpackhi_ps(rf, gf);
                    __m128 bc_hi = _mm_unpackhi_ps(gf, bf);
                    __m128 tc = _mm_shuffle_ps(bf, u0, _MM_SHUFFLE(2, 2, 0, 0));
                    __m128 v0 = _mm_shuffle_ps(u0, tc, _MM_SHUFFLE(2, 0, 1, 0));
                    __m128 tc2 = _mm_shuffle_ps(u0, bf, _MM_SHUFFLE(1, 1, 3, 3));
                    __m128 v1 = _mm_shuffle_ps(tc2, u1, _MM_SHUFFLE(1, 0, 2, 0));
                    __m128 tc3 = _mm_shuffle_ps(bc_hi, u1, _MM_SHUFFLE(2, 2, 1, 1));
                    __m128 v2 = _mm_shuffle_ps(tc3, bc_hi, _MM_SHUFFLE(3, 2, 2, 0));
                    /* F32 -> F16 */
                    __m128i h0 = _mm_cvtps_ph(v0, _MM_FROUND_TO_NEAREST_INT);
                    __m128i h1 = _mm_cvtps_ph(v1, _MM_FROUND_TO_NEAREST_INT);
                    __m128i h2 = _mm_cvtps_ph(v2, _MM_FROUND_TO_NEAREST_INT);
                    _mm_storeu_si128((__m128i *)(dst_u16 + j * 3), _mm_unpacklo_epi64(h0, h1));
                    _mm_storel_epi64((__m128i *)(dst_u16 + j * 3 + 8), h2);
                }
            }
#endif
            for (; j < n; j++) {
                dst_u16[j * 3 + 0] = alwan__f32_to_f16((float)ch0[j]);
                dst_u16[j * 3 + 1] = alwan__f32_to_f16((float)ch1[j]);
                dst_u16[j * 3 + 2] = alwan__f32_to_f16((float)ch2[j]);
            }
        } else {
            for (j = 0; j < n; j++) {
                uint16_t *p = (uint16_t *)((char *)base + (offset + j) * stride);
                p[0] = alwan__f32_to_f16((float)ch0[j]);
                p[1] = alwan__f32_to_f16((float)ch1[j]);
                p[2] = alwan__f32_to_f16((float)ch2[j]);
            }
        }
        break;
    }
    default: break;
    }
}

/* ----------------------------------------------------------------
 * Planar Tile Load: separate channel arrays -> flat SoA arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load_tile_planar3(alwan_simd_lane *ch0, alwan_simd_lane *ch1, alwan_simd_lane *ch2,
                                            alwan_f64 const *in0, alwan_f64 const *in1, alwan_f64 const *in2,
                                            size_t offset, size_t stride, size_t tile_count) {
    if (stride == sizeof(alwan_f64)) {
        /* Packed planar: contiguous channel data, use memcpy */
        memcpy(ch0, in0 + offset, tile_count * sizeof(alwan_simd_lane));
        memcpy(ch1, in1 + offset, tile_count * sizeof(alwan_simd_lane));
        memcpy(ch2, in2 + offset, tile_count * sizeof(alwan_simd_lane));
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            ch0[j] = (alwan_simd_lane)*(alwan_f64 const *)((char const *)in0 + (offset + j) * stride);
            ch1[j] = (alwan_simd_lane)*(alwan_f64 const *)((char const *)in1 + (offset + j) * stride);
            ch2[j] = (alwan_simd_lane)*(alwan_f64 const *)((char const *)in2 + (offset + j) * stride);
        }
    }
}

/* ----------------------------------------------------------------
 * Planar Tile Store: flat SoA arrays -> separate channel arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__store_tile_planar3(alwan_f64 *out0, alwan_f64 *out1, alwan_f64 *out2,
                                             size_t offset, size_t stride,
                                             alwan_simd_lane const *ch0, alwan_simd_lane const *ch1, alwan_simd_lane const *ch2,
                                             size_t tile_count) {
    if (stride == sizeof(alwan_f64)) {
        /* Packed planar: contiguous channel data, use memcpy */
        memcpy(out0 + offset, ch0, tile_count * sizeof(alwan_simd_lane));
        memcpy(out1 + offset, ch1, tile_count * sizeof(alwan_simd_lane));
        memcpy(out2 + offset, ch2, tile_count * sizeof(alwan_simd_lane));
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            *(alwan_f64 *)((char *)out0 + (offset + j) * stride) = (alwan_f64)ch0[j];
            *(alwan_f64 *)((char *)out1 + (offset + j) * stride) = (alwan_f64)ch1[j];
            *(alwan_f64 *)((char *)out2 + (offset + j) * stride) = (alwan_f64)ch2[j];
        }
    }
}

/* ----------------------------------------------------------------
 * Typed planar load/store: void* typed channels <-> alwan_simd_lane SoA
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_f64 alwan__load1_typed(void const *ptr, alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return *(uint8_t  const *)ptr * ALWAN_LITERAL(1.0 / 255.0);
    case ALWAN_PIXEL_U16: return *(uint16_t const *)ptr * ALWAN_LITERAL(1.0 / 65535.0);
    case ALWAN_PIXEL_F32: return (alwan_f64)*(float  const *)ptr;
    case ALWAN_PIXEL_F64: return (alwan_f64)*(double const *)ptr;
    case ALWAN_PIXEL_F16: return (alwan_f64)alwan__f16_to_f32(*(uint16_t const *)ptr);
    }
    return ALWAN_LITERAL(0.0);
}

ALWAN_INLINE size_t alwan__fmt_size(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 1;
    case ALWAN_PIXEL_U16: return 2;
    case ALWAN_PIXEL_F32: return 4;
    case ALWAN_PIXEL_F64: return 8;
    case ALWAN_PIXEL_F16: return 2;
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
        alwan__store1_typed((char *)out0 + byte_off, (alwan_f64)ch0[j], fmt);
        alwan__store1_typed((char *)out1 + byte_off, (alwan_f64)ch1[j], fmt);
        alwan__store1_typed((char *)out2 + byte_off, (alwan_f64)ch2[j], fmt);
    }
}

/* ----------------------------------------------------------------
 * Optimized typed AoS tile load/store (format switch outside loop)
 * Used by delegation macros to convert typed AoS <-> alwan_f64 AoS
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan__load_tile_typed_aos(alwan_f64 *dst,
                                              void const *src, alwan_pixel_format fmt,
                                              size_t offset, size_t stride, size_t n, int ch) {
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        alwan_f64 const inv = ALWAN_LITERAL(1.0 / 255.0);
        if (ch == 3 && stride == 3) {
            uint8_t const *src_u8 = (uint8_t const *)src + offset * 3;
            j = 0;
#if defined(__AVX2__)
            {
                __m256d const inv256d = _mm256_set1_pd(1.0 / 255.0);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw = _mm_loadu_si128((const __m128i *)(src_u8 + j * 3));
                    _mm256_storeu_pd(&dst[j * 3],     _mm256_mul_pd(_mm256_cvtepi32_pd(_mm_cvtepu8_epi32(raw)), inv256d));
                    _mm256_storeu_pd(&dst[j * 3 + 4], _mm256_mul_pd(_mm256_cvtepi32_pd(_mm_cvtepu8_epi32(_mm_srli_si128(raw, 4))), inv256d));
                    _mm256_storeu_pd(&dst[j * 3 + 8], _mm256_mul_pd(_mm256_cvtepi32_pd(_mm_cvtepu8_epi32(_mm_srli_si128(raw, 8))), inv256d));
                }
            }
#endif
            for (; j < n; j++) {
                dst[j * 3 + 0] = (alwan_f64)src_u8[j * 3 + 0] * inv;
                dst[j * 3 + 1] = (alwan_f64)src_u8[j * 3 + 1] * inv;
                dst[j * 3 + 2] = (alwan_f64)src_u8[j * 3 + 2] * inv;
            }
        } else {
            for (j = 0; j < n; j++) {
                uint8_t const *p = (uint8_t const *)((char const *)src + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (alwan_f64)p[c] * inv;
            }
        }
    } break;
    case ALWAN_PIXEL_U16: {
        alwan_f64 const inv = ALWAN_LITERAL(1.0 / 65535.0);
        for (j = 0; j < n; j++) {
            uint16_t const *p = (uint16_t const *)((char const *)src + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (alwan_f64)p[c] * inv;
        }
    } break;
    case ALWAN_PIXEL_F16: {
        if (ch == 3 && stride == 6) {
            uint16_t const *src_u16 = (uint16_t const *)src + offset * 3;
            j = 0;
#if defined(__AVX2__)
            for (; j + 4 <= n; j += 4) {
                __m128i h0 = _mm_loadu_si128((const __m128i *)(src_u16 + j * 3));
                __m128i h1 = _mm_loadl_epi64((const __m128i *)(src_u16 + j * 3 + 8));
                __m128 f0 = _mm_cvtph_ps(h0);
                __m128 f1 = _mm_cvtph_ps(_mm_srli_si128(h0, 8));
                __m128 f2 = _mm_cvtph_ps(h1);
                _mm256_storeu_pd(&dst[j * 3],     _mm256_cvtps_pd(f0));
                _mm256_storeu_pd(&dst[j * 3 + 4], _mm256_cvtps_pd(f1));
                _mm256_storeu_pd(&dst[j * 3 + 8], _mm256_cvtps_pd(f2));
            }
#endif
            for (; j < n; j++) {
                dst[j * 3 + 0] = (alwan_f64)alwan__f16_to_f32(src_u16[j * 3 + 0]);
                dst[j * 3 + 1] = (alwan_f64)alwan__f16_to_f32(src_u16[j * 3 + 1]);
                dst[j * 3 + 2] = (alwan_f64)alwan__f16_to_f32(src_u16[j * 3 + 2]);
            }
        } else {
            for (j = 0; j < n; j++) {
                uint16_t const *p = (uint16_t const *)((char const *)src + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (alwan_f64)alwan__f16_to_f32(p[c]);
            }
        }
    } break;
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++) {
            float const *p = (float const *)((char const *)src + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (alwan_f64)p[c];
        }
    } break;
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++) {
            double const *p = (double const *)((char const *)src + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (alwan_f64)p[c];
        }
    } break;
    }
}

ALWAN_INLINE void alwan__store_tile_typed_aos(void *dst, alwan_pixel_format fmt,
                                               size_t offset, size_t stride,
                                               alwan_f64 const *src, size_t n, int ch) {
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        if (ch == 3 && stride == 3) {
            uint8_t *dst_u8 = (uint8_t *)dst + offset * 3;
            j = 0;
#if defined(__AVX2__)
            {
                __m128 const scale128 = _mm_set1_ps(255.0f);
                __m128 const half128 = _mm_set1_ps(0.5f);
                __m128 const zero128 = _mm_setzero_ps();
                __m128 const max128 = _mm_set1_ps(255.0f);
                for (; j + 4 <= n; j += 4) {
                    __m128 f0 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3]));
                    __m128 f1 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 4]));
                    __m128 f2 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 8]));
                    f0 = _mm_min_ps(_mm_max_ps(_mm_add_ps(_mm_mul_ps(f0, scale128), half128), zero128), max128);
                    f1 = _mm_min_ps(_mm_max_ps(_mm_add_ps(_mm_mul_ps(f1, scale128), half128), zero128), max128);
                    f2 = _mm_min_ps(_mm_max_ps(_mm_add_ps(_mm_mul_ps(f2, scale128), half128), zero128), max128);
                    __m128i i0 = _mm_cvttps_epi32(f0);
                    __m128i i1 = _mm_cvttps_epi32(f1);
                    __m128i i2 = _mm_cvttps_epi32(f2);
                    __m128i p01 = _mm_packus_epi16(_mm_packs_epi32(i0, i1), _mm_packs_epi32(i2, _mm_setzero_si128()));
                    _mm_storel_epi64((__m128i *)(dst_u8 + j * 3), p01);
                    { uint32_t tmp = (uint32_t)_mm_extract_epi32(p01, 2);
                      memcpy(dst_u8 + j * 3 + 8, &tmp, 4); }
                }
            }
#endif
            for (; j < n; j++) {
                int c; for (c = 0; c < 3; c++) {
                    alwan_f64 v = src[j * 3 + c] * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
                    if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
                    if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
                    dst_u8[j * 3 + c] = (uint8_t)v;
                }
            }
        } else {
            for (j = 0; j < n; j++) {
                uint8_t *p = (uint8_t *)((char *)dst + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) {
                    alwan_f64 v = src[j * ch + c] * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
                    if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
                    if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
                    p[c] = (uint8_t)v;
                }
            }
        }
    } break;
    case ALWAN_PIXEL_U16: {
        for (j = 0; j < n; j++) {
            uint16_t *p = (uint16_t *)((char *)dst + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) {
                alwan_f64 v = src[j * ch + c] * ALWAN_LITERAL(65535.0) + ALWAN_LITERAL(0.5);
                if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
                if (v > ALWAN_LITERAL(65535.0)) v = ALWAN_LITERAL(65535.0);
                p[c] = (uint16_t)v;
            }
        }
    } break;
    case ALWAN_PIXEL_F16: {
        if (ch == 3 && stride == 6) {
            uint16_t *dst_u16 = (uint16_t *)dst + offset * 3;
            j = 0;
#if defined(__AVX2__)
            for (; j + 4 <= n; j += 4) {
                __m128 f0 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3]));
                __m128 f1 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 4]));
                __m128 f2 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 8]));
                __m128i h0 = _mm_cvtps_ph(f0, _MM_FROUND_TO_NEAREST_INT);
                __m128i h1 = _mm_cvtps_ph(f1, _MM_FROUND_TO_NEAREST_INT);
                __m128i h2 = _mm_cvtps_ph(f2, _MM_FROUND_TO_NEAREST_INT);
                _mm_storeu_si128((__m128i *)(dst_u16 + j * 3), _mm_unpacklo_epi64(h0, h1));
                _mm_storel_epi64((__m128i *)(dst_u16 + j * 3 + 8), h2);
            }
#endif
            for (; j < n; j++) {
                dst_u16[j * 3 + 0] = alwan__f32_to_f16((float)src[j * 3 + 0]);
                dst_u16[j * 3 + 1] = alwan__f32_to_f16((float)src[j * 3 + 1]);
                dst_u16[j * 3 + 2] = alwan__f32_to_f16((float)src[j * 3 + 2]);
            }
        } else {
            for (j = 0; j < n; j++) {
                uint16_t *p = (uint16_t *)((char *)dst + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) p[c] = alwan__f32_to_f16((float)src[j * ch + c]);
            }
        }
    } break;
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++) {
            float *p = (float *)((char *)dst + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) p[c] = (float)src[j * ch + c];
        }
    } break;
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++) {
            double *p = (double *)((char *)dst + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) p[c] = (double)src[j * ch + c];
        }
    } break;
    }
}

/* Single-channel planar typed tile load/store (for planar delegation) */

ALWAN_INLINE void alwan__load_tile_typed_ch(alwan_f64 *dst,
                                             void const *src, alwan_pixel_format fmt,
                                             size_t offset, size_t stride, size_t n) {
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        alwan_f64 const inv = ALWAN_LITERAL(1.0 / 255.0);
        if (stride == 1) {
            uint8_t const *src_u8 = (uint8_t const *)src + offset;
            j = 0;
#if defined(__AVX2__)
            {
                __m256d const inv256d = _mm256_set1_pd(1.0 / 255.0);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw = _mm_cvtsi32_si128(*(int const *)(src_u8 + j));
                    __m128i i32 = _mm_cvtepu8_epi32(raw);
                    _mm256_storeu_pd(&dst[j], _mm256_mul_pd(_mm256_cvtepi32_pd(i32), inv256d));
                }
            }
#endif
            for (; j < n; j++)
                dst[j] = (alwan_f64)src_u8[j] * inv;
        } else {
            for (j = 0; j < n; j++)
                dst[j] = (alwan_f64)*(uint8_t const *)((char const *)src + (offset + j) * stride) * inv;
        }
    } break;
    case ALWAN_PIXEL_U16: {
        alwan_f64 const inv = ALWAN_LITERAL(1.0 / 65535.0);
        for (j = 0; j < n; j++)
            dst[j] = (alwan_f64)*(uint16_t const *)((char const *)src + (offset + j) * stride) * inv;
    } break;
    case ALWAN_PIXEL_F16: {
        if (stride == 2) {
            uint16_t const *src_u16 = (uint16_t const *)src + offset;
            j = 0;
#if defined(__AVX2__)
            {
                for (; j + 4 <= n; j += 4) {
                    __m128i h = _mm_loadl_epi64((const __m128i *)(src_u16 + j));
                    __m128 f = _mm_cvtph_ps(h);
                    _mm256_storeu_pd(&dst[j], _mm256_cvtps_pd(f));
                }
            }
#endif
            for (; j < n; j++)
                dst[j] = (alwan_f64)alwan__f16_to_f32(src_u16[j]);
        } else {
            for (j = 0; j < n; j++)
                dst[j] = (alwan_f64)alwan__f16_to_f32(*(uint16_t const *)((char const *)src + (offset + j) * stride));
        }
    } break;
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++)
            dst[j] = (alwan_f64)*(float const *)((char const *)src + (offset + j) * stride);
    } break;
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++)
            dst[j] = (alwan_f64)*(double const *)((char const *)src + (offset + j) * stride);
    } break;
    }
}

ALWAN_INLINE void alwan__store_tile_typed_ch(void *dst, alwan_pixel_format fmt,
                                              size_t offset, size_t stride,
                                              alwan_f64 const *src, size_t n) {
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        if (stride == 1) {
            uint8_t *dst_u8 = (uint8_t *)dst + offset;
            j = 0;
#if defined(__AVX2__)
            {
                __m256d const scale256d = _mm256_set1_pd(255.0);
                __m256d const half256d = _mm256_set1_pd(0.5);
                __m256d const zero256d = _mm256_setzero_pd();
                __m256d const max256d = _mm256_set1_pd(255.0);
                for (; j + 4 <= n; j += 4) {
                    __m256d f = _mm256_loadu_pd(&src[j]);
                    f = _mm256_min_pd(_mm256_max_pd(_mm256_add_pd(_mm256_mul_pd(f, scale256d), half256d), zero256d), max256d);
                    __m128i i32 = _mm256_cvttpd_epi32(f);
                    __m128i i16 = _mm_packs_epi32(i32, _mm_setzero_si128());
                    __m128i u8 = _mm_packus_epi16(i16, _mm_setzero_si128());
                    *(uint32_t *)(dst_u8 + j) = (uint32_t)_mm_cvtsi128_si32(u8);
                }
            }
#endif
            for (; j < n; j++) {
                alwan_f64 v = src[j] * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
                if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
                if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
                dst_u8[j] = (uint8_t)v;
            }
        } else {
            for (j = 0; j < n; j++) {
                alwan_f64 v = src[j] * ALWAN_LITERAL(255.0) + ALWAN_LITERAL(0.5);
                if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
                if (v > ALWAN_LITERAL(255.0)) v = ALWAN_LITERAL(255.0);
                *(uint8_t *)((char *)dst + (offset + j) * stride) = (uint8_t)v;
            }
        }
    } break;
    case ALWAN_PIXEL_U16: {
        for (j = 0; j < n; j++) {
            alwan_f64 v = src[j] * ALWAN_LITERAL(65535.0) + ALWAN_LITERAL(0.5);
            if (v < ALWAN_LITERAL(0.0)) v = ALWAN_LITERAL(0.0);
            if (v > ALWAN_LITERAL(65535.0)) v = ALWAN_LITERAL(65535.0);
            *(uint16_t *)((char *)dst + (offset + j) * stride) = (uint16_t)v;
        }
    } break;
    case ALWAN_PIXEL_F16: {
        if (stride == 2) {
            uint16_t *dst_u16 = (uint16_t *)dst + offset;
            j = 0;
#if defined(__AVX2__)
            {
                for (; j + 4 <= n; j += 4) {
                    __m256d d = _mm256_loadu_pd(&src[j]);
                    __m128 f = _mm256_cvtpd_ps(d);
                    __m128i h = _mm_cvtps_ph(f, _MM_FROUND_TO_NEAREST_INT);
                    _mm_storel_epi64((__m128i *)(dst_u16 + j), h);
                }
            }
#endif
            for (; j < n; j++)
                dst_u16[j] = alwan__f32_to_f16((float)src[j]);
        } else {
            for (j = 0; j < n; j++)
                *(uint16_t *)((char *)dst + (offset + j) * stride) = alwan__f32_to_f16((float)src[j]);
        }
    } break;
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++)
            *(float *)((char *)dst + (offset + j) * stride) = (float)src[j];
    } break;
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++)
            *(double *)((char *)dst + (offset + j) * stride) = (double)src[j];
    } break;
    }
}

/* ================================================================
 * Precision-specific tile load/store (always available for dual dispatch)
 *
 * alwan__load_tile_typed_ch_f32  / _f64   -- planar single-channel
 * alwan__store_tile_typed_ch_f32 / _f64   -- planar single-channel
 * alwan__load_tile_typed_aos_f32 / _f64   -- AoS interleaved
 * alwan__store_tile_typed_aos_f32/ _f64   -- AoS interleaved
 * ================================================================ */

#define ALWAN_TILE_PIXELS_F32  4096
#define ALWAN_TILE_PIXELS_F64  2048

/* f32 tile helpers */
#define ALWAN_TILE_T       float
#define ALWAN_TILE_SUFFIX  _f32
#define ALWAN_TILE_IS_F32  1
#define ALWAN_TILE_LIT(x)  x##f
#include "alwan_tile_typed_gen.inc"
#undef ALWAN_TILE_T
#undef ALWAN_TILE_SUFFIX
#undef ALWAN_TILE_IS_F32
#undef ALWAN_TILE_LIT

/* f64 tile helpers */
#define ALWAN_TILE_T       double
#define ALWAN_TILE_SUFFIX  _f64
#define ALWAN_TILE_IS_F32  0
#define ALWAN_TILE_LIT(x)  x
#include "alwan_tile_typed_gen.inc"
#undef ALWAN_TILE_T
#undef ALWAN_TILE_SUFFIX
#undef ALWAN_TILE_IS_F32
#undef ALWAN_TILE_LIT

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
                                        alwan_mat3x3_f64 const *m,
                                        alwan_simd r, alwan_simd g, alwan_simd b) {
    /* Use mul+add (not FMA) to match scalar alwan_mat3_mulv_v rounding:
     * m[0]*r + m[1]*g + m[2]*b evaluated left-to-right with intermediate rounding. */
    *ox = alwan_simd_add(alwan_simd_add(
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[0]), r),
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[1]), g)),
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[2]), b));
    *oy = alwan_simd_add(alwan_simd_add(
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[3]), r),
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[4]), g)),
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[5]), b));
    *oz = alwan_simd_add(alwan_simd_add(
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[6]), r),
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[7]), g)),
              alwan_simd_mul(alwan_simd_set1((alwan_simd_lane)m->m[8]), b));
}

/* ----------------------------------------------------------------
 * CIE Lab f(t): t > delta^3 ? t^(1/3) : kappa*t + offset
 * delta = 6/29, delta^3 = 216/24389, kappa = 24389/27/116, offset = 16/116
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__lab_f_simd(alwan_simd t) {
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
    /* Det mode: lane-unpack so the cube root branch matches the scalar
     * deterministic polynomial bit-exactly across SIMD widths. */
    alwan_simd_lane const delta3   = (alwan_simd_lane)(216.0 / 24389.0);
    alwan_simd_lane const kappa_s  = (alwan_simd_lane)(24389.0 / (27.0 * 116.0));
    alwan_simd_lane const offset_s = (alwan_simd_lane)(16.0 / 116.0);
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, t);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        alwan_simd_lane const x = lanes[i];
        lanes[i] = (x > delta3) ? alwan_det_cbrt_f64(x) : (kappa_s * x + offset_s);
    }
    return alwan_simd_load(lanes);
#else
    alwan_simd delta3 = alwan_simd_set1((alwan_simd_lane)(216.0 / 24389.0));
    alwan_simd kappa  = alwan_simd_set1((alwan_simd_lane)(24389.0 / (27.0 * 116.0)));
    alwan_simd offset = alwan_simd_set1((alwan_simd_lane)(16.0 / 116.0));

    alwan_simd cbrt_result   = alwan_simd_cbrt_fast(t);
    alwan_simd linear_result = alwan_simd_fmadd(kappa, t, offset);

    alwan_simd_mask mask = alwan_simd_cmpgt(t, delta3);
    return alwan_simd_select(mask, cbrt_result, linear_result);
#endif
}

/* ----------------------------------------------------------------
 * CIE Lab f_inv(t): t > delta ? t^3 : kappa*(t - offset)
 * delta = 6/29, kappa = 3*delta^2 = 108/841
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__lab_f_inv_simd(alwan_simd t) {
    /* Match scalar alwan_lab_f_inv constant computation (float arithmetic) */
    alwan_simd_lane delta_s = (alwan_simd_lane)6.0 / (alwan_simd_lane)29.0;
    alwan_simd_lane kappa_s = (alwan_simd_lane)3.0 * delta_s * delta_s;
    alwan_simd_lane offset_s = (alwan_simd_lane)16.0 / (alwan_simd_lane)116.0;
    alwan_simd delta  = alwan_simd_set1(delta_s);
    alwan_simd kappa  = alwan_simd_set1(kappa_s);
    alwan_simd offset = alwan_simd_set1(offset_s);

    alwan_simd cube_result   = alwan_simd_mul(t, alwan_simd_mul(t, t));
    alwan_simd linear_result = alwan_simd_mul(kappa, alwan_simd_sub(t, offset));

    alwan_simd_mask mask = alwan_simd_cmpgt(t, delta);
    return alwan_simd_select(mask, cube_result, linear_result);
}

/* ----------------------------------------------------------------
 * sRGB EOTF: encoded -> linear
 * V <= 0.04045 ? V/12.92 : ((V+0.055)/1.055)^2.4
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

/* Det mode: lane-unpack to the canonical scalar polynomial. The vector
 * pow24/pow_inv24 paths use approximation polynomials whose lane order
 * and FMA usage differ across SSE/AVX/NEON; routing through the scalar
 * polynomial keeps SIMD and scalar paths bit-identical.
 * docs/determinism.md */
ALWAN_INLINE alwan_simd alwan__srgb_eotf_simd(alwan_simd v) {
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        lanes[i] = alwan_det_srgb_eotf_f64(lanes[i]);
    }
    return alwan_simd_load(lanes);
}

ALWAN_INLINE alwan_simd alwan__srgb_oetf_simd(alwan_simd v) {
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        lanes[i] = alwan_det_srgb_oetf_f64(lanes[i]);
    }
    return alwan_simd_load(lanes);
}

#else /* fast mode -- vectorised approximations */

ALWAN_INLINE alwan_simd alwan__srgb_eotf_simd(alwan_simd v) {
    alwan_simd thresh = alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_EOTF_THRESH);
    alwan_simd_mask mask = alwan_simd_cmple(v, thresh);
    alwan_simd lo     = alwan_simd_mul(v, alwan_simd_set1((alwan_simd_lane)(1.0 / ALWAN_SRGB_LINEAR_GAIN)));

    /* Early-out: skip pow24 when all lanes are in the linear region */
    if (alwan_simd_mask_all_set(mask))
        return lo;

    alwan_simd hi_base = alwan_simd_mul(
        alwan_simd_add(v, alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_B)),
        alwan_simd_set1((alwan_simd_lane)(1.0 / ALWAN_SRGB_A)));
    alwan_simd hi     = alwan_simd_pow24(hi_base);
    return alwan_simd_select(mask, lo, hi);
}

/* ----------------------------------------------------------------
 * sRGB OETF: linear -> encoded
 * V <= 0.0031308 ? 12.92*V : 1.055*V^(1/2.4) - 0.055
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd alwan__srgb_oetf_simd(alwan_simd v) {
    alwan_simd thresh = alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_OETF_THRESH);
    alwan_simd_mask mask = alwan_simd_cmple(v, thresh);
    alwan_simd lo = alwan_simd_mul(v, alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_LINEAR_GAIN));
    /* Early-out: skip pow_inv24 when all lanes are in the linear region */
    if (alwan_simd_mask_all_set(mask))
        return lo;
    /* Clamp to zero before pow_inv24 to avoid undefined pow(negative, non-integer) */
    alwan_simd hi_base = alwan_simd_select(
        alwan_simd_cmpgt(v, alwan_simd_zero()),
        v,
        alwan_simd_zero());
    alwan_simd hi = alwan_simd_sub(
        alwan_simd_mul(
            alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_A),
            alwan_simd_pow_inv24(hi_base)),
        alwan_simd_set1((alwan_simd_lane)ALWAN_SRGB_B));
    return alwan_simd_select(mask, lo, hi);
}

#endif /* ALWAN_DETERMINISTIC */

/* ----------------------------------------------------------------
 * PQ OETF (Standard SMPTE ST 2084): linear (0-10000 cd/m^2) -> encoded
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
ALWAN_INLINE alwan_simd alwan__pq_oetf_simd(alwan_simd v) {
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        lanes[i] = alwan_pq_oetf_f64(lanes[i]);
    }
    return alwan_simd_load(lanes);
}
#else
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
#endif /* ALWAN_DETERMINISTIC */

/* ----------------------------------------------------------------
 * PQ EOTF (Standard SMPTE ST 2084): encoded -> linear (0-10000 cd/m^2)
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
ALWAN_INLINE alwan_simd alwan__pq_eotf_simd(alwan_simd v) {
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        lanes[i] = alwan_pq_eotf_f64(lanes[i]);
    }
    return alwan_simd_load(lanes);
}
#else
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
#endif /* ALWAN_DETERMINISTIC */

/* ----------------------------------------------------------------
 * JzAzBz-specific PQ OETF: uses JzAzBz constants (N, P, C1-C3)
 * linear -> encoded
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
/* Det mode: lane-unpack with the scalar formula inlined directly to
 * avoid the inv10k vs / 10000.0 1-ULP divergence (see alwan__pq_oetf_simd
 * det branch above). The formula matches alwan/core/alwan_jzazbz_core.inc
 * `pq_jz_oetf_v` exactly when ALWAN_POW_F64 routes to alwan_det_pow_pos. */
ALWAN_INLINE alwan_simd alwan__pq_jz_oetf_simd(alwan_simd v) {
    alwan_simd_lane const n  = (alwan_simd_lane)0.1593017578125;
    alwan_simd_lane const p  = (alwan_simd_lane)134.034375;
    alwan_simd_lane const c1 = (alwan_simd_lane)0.8359375;
    alwan_simd_lane const c2 = (alwan_simd_lane)18.8515625;
    alwan_simd_lane const c3 = (alwan_simd_lane)18.6875;
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        alwan_simd_lane const lin = lanes[i];
        if (lin <= (alwan_simd_lane)0) {
            lanes[i] = (alwan_simd_lane)0;
        } else {
            alwan_simd_lane const Yn = ALWAN_POW_F64(lin / (alwan_simd_lane)10000.0, n);
            alwan_simd_lane const num = c1 + c2 * Yn;
            alwan_simd_lane const den = (alwan_simd_lane)1.0 + c3 * Yn;
            lanes[i] = ALWAN_POW_F64(num / den, p);
        }
    }
    return alwan_simd_load(lanes);
}
#else
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
#endif /* ALWAN_DETERMINISTIC */

/* ----------------------------------------------------------------
 * JzAzBz-specific PQ EOTF: encoded -> linear
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
ALWAN_INLINE alwan_simd alwan__pq_jz_eotf_simd(alwan_simd v) {
    alwan_simd_lane const n_inv = (alwan_simd_lane)(1.0 / 0.1593017578125);
    alwan_simd_lane const p_inv = (alwan_simd_lane)(1.0 / 134.034375);
    alwan_simd_lane const c1 = (alwan_simd_lane)0.8359375;
    alwan_simd_lane const c2 = (alwan_simd_lane)18.8515625;
    alwan_simd_lane const c3 = (alwan_simd_lane)18.6875;
    alwan_simd_lane const eps = (alwan_simd_lane)ALWAN_MAP_PQ_DIV_GUARD;
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        alwan_simd_lane const enc = lanes[i];
        if (enc <= (alwan_simd_lane)0) { lanes[i] = (alwan_simd_lane)0; continue; }
        alwan_simd_lane const Ep = ALWAN_POW_F64(enc, p_inv);
        alwan_simd_lane const num = Ep - c1;
        alwan_simd_lane const den = c2 - c3 * Ep;
        alwan_simd_lane const abs_den = (den < (alwan_simd_lane)0) ? -den : den;
        alwan_simd_lane ratio = (abs_den > eps) ? (num / den) : (alwan_simd_lane)0;
        if (ratio < (alwan_simd_lane)0) ratio = (alwan_simd_lane)0;
        lanes[i] = (alwan_simd_lane)10000.0 * ALWAN_POW_F64(ratio, n_inv);
    }
    return alwan_simd_load(lanes);
}
#else
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
#endif /* ALWAN_DETERMINISTIC */

/* ----------------------------------------------------------------
 * HLG OETF: linear -> encoded
 * L <= 1/12: sqrt(3*L)
 * L > 1/12:  a*ln(12*L - b) + c
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
ALWAN_INLINE alwan_simd alwan__hlg_oetf_simd(alwan_simd v) {
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        lanes[i] = alwan_hlg_oetf_f64(lanes[i]);
    }
    return alwan_simd_load(lanes);
}
#else
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
    alwan_simd hi = alwan_simd_add(alwan_simd_mul(a_, alwan_simd_log(alwan_simd_sub(alwan_simd_mul(twelve, L), b_))), c_);
    return alwan_simd_select(alwan_simd_cmple(L, thresh), lo, hi);
}
#endif /* ALWAN_DETERMINISTIC */

/* ----------------------------------------------------------------
 * HLG inverse OETF (NOT full EOTF): encoded -> scene-linear (no OOTF)
 * Used internally by ICtCp-HLG which needs pure OETF^-1 per LMS channel.
 * E <= 0.5: E^2/3
 * E > 0.5:  (exp((E-c)/a) + b) / 12
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
/* Det mode no-OOTF inverse: replicate the scalar building blocks lane-wise.
 * No `alwan_hlg_inv_oetf_*` exists in alwan_core.inc (the scalar HLG EOTF
 * applies the OOTF), so re-derive the no-OOTF formula here using the
 * deterministic exp primitive. */
ALWAN_INLINE alwan_simd alwan__hlg_eotf_simd(alwan_simd v) {
    alwan_simd_lane const a = (alwan_simd_lane)0.17883277;
    alwan_simd_lane const b = (alwan_simd_lane)(1.0 - 4.0 * 0.17883277);
    alwan_simd_lane const c = (alwan_simd_lane)0.55991072952956202;
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        alwan_simd_lane const E = lanes[i] < (alwan_simd_lane)0 ? (alwan_simd_lane)0 : lanes[i];
        if (E <= (alwan_simd_lane)0.5) {
            lanes[i] = (E * E) / (alwan_simd_lane)3.0;
        } else {
            lanes[i] = (alwan_det_exp_f64((E - c) / a) + b) / (alwan_simd_lane)12.0;
        }
    }
    return alwan_simd_load(lanes);
}
#else
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
#endif /* ALWAN_DETERMINISTIC */

/* ----------------------------------------------------------------
 * HLG full EOTF: encoded -> display-linear (includes OOTF, system gamma 1.2)
 * Matches the scalar alwan_hlg_eotf_f64.
 * Used by RGB image conversion; ICtCp-HLG uses alwan__hlg_eotf_simd instead.
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
ALWAN_INLINE alwan_simd alwan__hlg_eotf_full_simd(alwan_simd v) {
    ALWAN_ALIGN(64) alwan_simd_lane lanes[ALWAN_SIMD_WIDTH];
    alwan_simd_store(lanes, v);
    for (size_t i = 0; i < ALWAN_SIMD_WIDTH; i++) {
        lanes[i] = alwan_hlg_eotf_f64(lanes[i]);
    }
    return alwan_simd_load(lanes);
}
#else
ALWAN_INLINE alwan_simd alwan__hlg_eotf_full_simd(alwan_simd v) {
    alwan_simd scene = alwan__hlg_eotf_simd(v);
    return alwan_simd_pow(scene, alwan_simd_set1((alwan_simd_lane)1.2));
}
#endif /* ALWAN_DETERMINISTIC */

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
        InT s_ = {(alwan_f64)i0[j], (alwan_f64)i1[j], (alwan_f64)i2[j]}; \
        OutT d_; \
        core(&d_, &s_); \
        o0[j] = (alwan_simd_lane)d_.fo0; o1[j] = (alwan_simd_lane)d_.fo1; o2[j] = (alwan_simd_lane)d_.fo2; \
    } \
}

#define ALWAN_TILE_KERNEL_3TO3_STATUS(name, InT, OutT, core, fi0, fi1, fi2, fo0, fo1, fo2) \
static int name(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2, \
                alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2, size_t n) { \
    for (size_t j = 0; j < n; j++) { \
        InT s_ = {(alwan_f64)i0[j], (alwan_f64)i1[j], (alwan_f64)i2[j]}; \
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
            ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
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
            ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_aos3(ci0_, ci1_, ci2_, (in_base), off_, (in_s), tile_); \
            { int st_ = kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
              if (st_ != ALWAN_OK) return st_; } \
            alwan__store_tile_aos3((out_base), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

/* Typed (_ex) tiled loop: typed void* in/out with format-aware tile load/store.
 * The kernel operates on alwan_simd_lane SoA buffers in-place.
 * Fast path: when both formats match native scalar, use ALWAN_MAP3_TILED directly. */
#define ALWAN_MAP3_TILED_EX(in_ptr, in_fmt, in_s, out_ptr, out_fmt, out_s, cnt, kernel) \
    do { \
        if ((in_fmt) == ALWAN_NATIVE_PIXEL_FMT && (out_fmt) == ALWAN_NATIVE_PIXEL_FMT) { \
            ALWAN_MAP3_TILED((alwan_f64 const *)(in_ptr), (in_s), \
                             (alwan_f64 *)(out_ptr), (out_s), (cnt), kernel); \
        } else { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
            ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_typed_3(ci0_, ci1_, ci2_, (in_ptr), (in_fmt), off_, (in_s), tile_); \
            kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
            alwan__store_tile_typed_3((out_ptr), (out_fmt), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } } \
    } while (0)

/* Planar tiled loop */
#define ALWAN_MAP3_TILED_PLANAR(in0, in1, in2, in_s, out0, out1, out2, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
            ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_planar3(ci0_, ci1_, ci2_, (in0), (in1), (in2), off_, (in_s), tile_); \
            kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
            alwan__store_tile_planar3((out0), (out1), (out2), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

/* ----------------------------------------------------------------
 * Planar tiled loop macros
 * ---------------------------------------------------------------- */

#define ALWAN_MAP3_TILED_PLANAR(in0, in1, in2, in_s, out0, out1, out2, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_TILE_PIXELS) tile_ = ALWAN_TILE_PIXELS; \
            ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
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
            ALWAN_ALIGN(32) alwan_simd_lane ci0_[ALWAN_TILE_PIXELS], ci1_[ALWAN_TILE_PIXELS], ci2_[ALWAN_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_simd_lane co0_[ALWAN_TILE_PIXELS], co1_[ALWAN_TILE_PIXELS], co2_[ALWAN_TILE_PIXELS]; \
            alwan__load_tile_planar3(ci0_, ci1_, ci2_, (in0), (in1), (in2), off_, (in_s), tile_); \
            { int st_ = kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
              if (st_ != ALWAN_OK) return st_; } \
            alwan__store_tile_planar3((out0), (out1), (out2), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

/* ================================================================
 * Normalization helpers for planar channel data
 *
 * Applied between kernel and store (forward) or between load and
 * kernel (inverse) when ALWAN_NORMALIZE_RANGES is enabled.
 * Compiled away entirely when normalization is off (default).
 * ================================================================ */

/* Always available -- used for unconditional offsets (e.g. YCbCr +/-0.5) */
ALWAN_INLINE void alwan__norm_lane_add(alwan_simd_lane *d, size_t n, alwan_simd_lane offset) {
    for (size_t i = 0; i < n; i++) d[i] += offset;
}

#if ALWAN_NORMALIZE_RANGES
ALWAN_INLINE void alwan__norm_lane_mul(alwan_simd_lane *d, size_t n, alwan_simd_lane factor) {
    for (size_t i = 0; i < n; i++) d[i] *= factor;
}
ALWAN_INLINE void alwan__norm_lane_affine(alwan_simd_lane *d, size_t n, alwan_simd_lane scale, alwan_simd_lane offset) {
    for (size_t i = 0; i < n; i++) d[i] = d[i] * scale + offset;
}
#define ALWAN_MAP_NORM_MUL(d, n, f)    alwan__norm_lane_mul((d), (n), (alwan_simd_lane)(f))
#define ALWAN_MAP_NORM_ADD(d, n, o)    alwan__norm_lane_add((d), (n), (alwan_simd_lane)(o))
#define ALWAN_MAP_NORM_AFFINE(d, n, s, o) alwan__norm_lane_affine((d), (n), (alwan_simd_lane)(s), (alwan_simd_lane)(o))
#else
#define ALWAN_MAP_NORM_MUL(d, n, f)    ((void)0)
#define ALWAN_MAP_NORM_ADD(d, n, o)    ((void)0)
#define ALWAN_MAP_NORM_AFFINE(d, n, s, o) ((void)0)
#endif

/* ================================================================
 * Delegation macros: _ex functions that delegate to native SIMD functions
 *
 * Instead of per-pixel processing, these:
 * 1. Load a tile of typed data -> intermediate alwan_f64 buffer
 * 2. Call the native SIMD interleave/planar function on the buffer
 * 3. Store the result back to typed output
 *
 * This reuses all existing SIMD kernels with minimal overhead.
 * The typed load/store loops have format-switch outside the loop
 * so the compiler can auto-vectorize the conversion.
 * ================================================================ */

/* ================================================================
 * Dual-dispatch delegation macros (Phase 4)
 *
 * Route _ex functions to f32 or f64 native pipeline:
 *   F32+F32 -> direct call to fn_f32 (zero overhead)
 *   F64+F64 -> direct call to fn_f64 (zero overhead)
 *   F64 input or output -> f64 tiled pipeline
 *   Everything else (U8/U16/F16/F32) -> f32 tiled pipeline
 * ================================================================ */

/* -- Planar dual-dispatch: basic 3-channel ---------------------- */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_PLANAR_EX_DELEGATE_DUAL(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count); \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count); \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } else { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_PLANAR_EX_DELEGATE_DUAL(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_PLANAR_EX_DELEGATE_DUAL(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* -- Planar dual-dispatch: with white_xyz ----------------------- */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_xyz_f64 const *white_xyz) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count, &w_); \
    } \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count, &w_); \
    } \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &w_); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } else { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &w_); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_xyz_f64 const *white_xyz) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count, &w_); \
    } \
    { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, &w_); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_WHITE(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_xyz_f64 const *white_xyz) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count, &w_); \
    } \
    { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, &w_); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* -- Planar dual-dispatch: with extra typed param (int/enum) ---- */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(name, fn_f32, fn_f64, extra_type, extra_name) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         extra_type extra_name) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count, extra_name); \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count, extra_name); \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, extra_name); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } else { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, extra_name); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(name, fn_f32, fn_f64, extra_type, extra_name) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         extra_type extra_name) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count, extra_name); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, extra_name); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_INT(name, fn_f32, fn_f64, extra_type, extra_name) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         extra_type extra_name) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count, extra_name); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, extra_name); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* -- Planar dual-dispatch: with alwan_f64 param -------------- */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_SCALAR(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_f64 param) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count, (float)param); \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count, (double)param); \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, (double)param); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } else { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, (float)param); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_SCALAR(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_f64 param) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out0, out_stride, (float *)out1, (float *)out2, \
                      (float const *)in0, in_stride, (float const *)in1, (float const *)in2, \
                      count, (float)param); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32]; \
            ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32]; \
            alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f32(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_, (float)param); \
            alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_PLANAR_EX_DELEGATE_DUAL_SCALAR(name, fn_f32, fn_f64) \
alwan_status name(void *out0, size_t out_stride, void *out1, void *out2, \
         void const *in0, size_t in_stride, void const *in1, void const *in2, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_f64 param) { \
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out0, out_stride, (double *)out1, (double *)out2, \
                      (double const *)in0, in_stride, (double const *)in1, (double const *)in2, \
                      count, (double)param); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64]; \
            ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64]; \
            alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_); \
            alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_); \
            fn_f64(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_, (double)param); \
            alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_); \
            alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_); \
            alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* -- Interleave dual-dispatch: basic 3-channel ------------------ */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_EX_DELEGATE_DUAL(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count); \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count); \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } else { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_EX_DELEGATE_DUAL(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_EX_DELEGATE_DUAL(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* -- Interleave dual-dispatch: with white_xyz ------------------- */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_EX_DELEGATE_DUAL_WHITE(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_xyz_f64 const *white_xyz) { \
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, &w_); \
    } \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, &w_); \
    } \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, &w_); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } else { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, &w_); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_EX_DELEGATE_DUAL_WHITE(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_xyz_f64 const *white_xyz) { \
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, &w_); \
    } \
    { \
        alwan_xyz_f32 w_ = {(float)white_xyz->x, (float)white_xyz->y, (float)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, &w_); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_EX_DELEGATE_DUAL_WHITE(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_xyz_f64 const *white_xyz) { \
    if (!in || !out || !white_xyz || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, &w_); \
    } \
    { \
        alwan_xyz_f64 w_ = {(double)white_xyz->x, (double)white_xyz->y, (double)white_xyz->z}; \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, &w_); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* -- Interleave dual-dispatch: with extra typed param (int/enum) */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_EX_DELEGATE_DUAL_INT(name, fn_f32, fn_f64, extra_type, extra_name) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         extra_type extra_name) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, extra_name); \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, extra_name); \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, extra_name); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } else { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, extra_name); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_EX_DELEGATE_DUAL_INT(name, fn_f32, fn_f64, extra_type, extra_name) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         extra_type extra_name) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, extra_name); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, extra_name); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_EX_DELEGATE_DUAL_INT(name, fn_f32, fn_f64, extra_type, extra_name) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         extra_type extra_name) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, extra_name); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, extra_name); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* -- Interleave dual-dispatch: with alwan_f64 param ---------- */

/* One definition per precision selection. A single-precision build must not
 * name the other precision's worker at all: the declaration exists in every
 * build but the definition is gated, so an unguarded reference resolves in a
 * static archive and fails to link in a shared one. Every pixel format still
 * works, because the typed tile loaders read and write all of them, the
 * excluded precision's buffers included, through whichever worker exists. */
#if ALWAN_WITH_BOTH
#define ALWAN_EX_DELEGATE_DUAL_SCALAR(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_f64 param) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, (float)param); \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, (double)param); \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, (double)param); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } else { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, (float)param); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#elif ALWAN_WITH_F32
#define ALWAN_EX_DELEGATE_DUAL_SCALAR(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_f64 param) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, (float)param); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32; \
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3]; \
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f32(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_, (float)param); \
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#else /* ALWAN_WITH_F64 */
#define ALWAN_EX_DELEGATE_DUAL_SCALAR(name, fn_f32, fn_f64) \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, \
         size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, \
         alwan_f64 param) { \
    if (!in || !out || count == 0) return ALWAN_E_INVALID; \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, (double)param); \
    { \
        size_t off_ = 0; \
        while (off_ < count) { \
            size_t tile_ = count - off_; \
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64; \
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3]; \
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3); \
            fn_f64(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_, (double)param); \
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3); \
            off_ += tile_; \
        } \
    } \
    return ALWAN_OK; \
}
#endif

/* ================================================================
 * Gamut map kernels (alwan_gamut_map.c) - dual precision
 * ================================================================ */

void alwan__gamut_clip_kernel_f32(float *c0, float *c1, float *c2, size_t n);
void alwan__gamut_clip_kernel_f64(double *c0, double *c1, double *c2, size_t n);
void alwan__css_gamut_map_kernel_f32(float *o0, float *o1, float *o2,
                                      float const *i0, float const *i1, float const *i2, size_t n);
void alwan__css_gamut_map_kernel_f64(double *o0, double *o1, double *o2,
                                      double const *i0, double const *i1, double const *i2, size_t n);

/* Backward-compatible aliases (compile-time selected) */
#define alwan__gamut_clip_kernel      alwan__gamut_clip_kernel_f64
#define alwan__css_gamut_map_kernel   alwan__css_gamut_map_kernel_f64

/* Forward declarations for all _f32/_f64 map_planar / map_interleave variants */
#include "alwan_map_fwd.h"

#endif /* ALWAN_MAP_INTERNAL_H */
