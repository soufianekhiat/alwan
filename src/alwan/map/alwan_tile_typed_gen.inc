/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Parameterized typed tile load/store (included twice: f32 and f64).
 *
 * Before #include, define:
 *   ALWAN_TILE_T       -- float or double
 *   ALWAN_TILE_SUFFIX  -- _f32 or _f64
 *   ALWAN_TILE_IS_F32  -- 1 or 0  (selects AVX ps vs pd paths)
 *   ALWAN_TILE_LIT(x)  -- x##f or x
 */

/* Token pasting */
#define ALWAN_TILE_FN2_(a, b) a##b
#define ALWAN_TILE_FN_(a, b) ALWAN_TILE_FN2_(a, b)
#define ALWAN_TILE_FN(name) ALWAN_TILE_FN_(name, ALWAN_TILE_SUFFIX)

/* ----------------------------------------------------------------
 * Single-channel planar typed tile load
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_TILE_FN(alwan__load_tile_typed_ch)(ALWAN_TILE_T *dst,
                                             void const *src, alwan_pixel_format fmt,
                                             size_t offset, size_t stride, size_t n) {
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        ALWAN_TILE_T const inv = (ALWAN_TILE_T)(1.0 / 255.0);
        if (stride == 1) {
            uint8_t const *src_u8 = (uint8_t const *)src + offset;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                __m256 const inv256 = _mm256_set1_ps(1.0f / 255.0f);
                for (; j + 8 <= n; j += 8) {
                    __m128i raw = _mm_loadl_epi64((const __m128i *)(src_u8 + j));
                    __m256 f = _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepu8_epi32(raw)), inv256);
                    _mm256_storeu_ps(&dst[j], f);
                }
            }
#else
            {
                __m256d const inv256d = _mm256_set1_pd(1.0 / 255.0);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw = _mm_cvtsi32_si128(*(int const *)(src_u8 + j));
                    __m128i i32 = _mm_cvtepu8_epi32(raw);
                    _mm256_storeu_pd(&dst[j], _mm256_mul_pd(_mm256_cvtepi32_pd(i32), inv256d));
                }
            }
#endif
#endif
            for (; j < n; j++)
                dst[j] = (ALWAN_TILE_T)src_u8[j] * inv;
        } else {
            for (j = 0; j < n; j++)
                dst[j] = (ALWAN_TILE_T)*(uint8_t const *)((char const *)src + (offset + j) * stride) * inv;
        }
    } break;
    case ALWAN_PIXEL_U16: {
        ALWAN_TILE_T const inv = (ALWAN_TILE_T)(1.0 / 65535.0);
        if (stride == 2) {
            uint16_t const *src_u16 = (uint16_t const *)src + offset;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                __m256 const inv256 = _mm256_set1_ps(1.0f / 65535.0f);
                for (; j + 8 <= n; j += 8) {
                    __m128i raw = _mm_loadu_si128((const __m128i *)(src_u16 + j));
                    _mm256_storeu_ps(&dst[j], _mm256_mul_ps(_mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(raw)), inv256));
                }
            }
#else
            {
                __m256d const inv256d = _mm256_set1_pd(1.0 / 65535.0);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw = _mm_loadl_epi64((const __m128i *)(src_u16 + j));
                    __m128i i32 = _mm_cvtepu16_epi32(raw);
                    _mm256_storeu_pd(&dst[j], _mm256_mul_pd(_mm256_cvtepi32_pd(i32), inv256d));
                }
            }
#endif
#endif
            for (; j < n; j++)
                dst[j] = (ALWAN_TILE_T)src_u16[j] * inv;
        } else {
            for (j = 0; j < n; j++)
                dst[j] = (ALWAN_TILE_T)*(uint16_t const *)((char const *)src + (offset + j) * stride) * inv;
        }
    } break;
    case ALWAN_PIXEL_F16: {
        if (stride == 2) {
            uint16_t const *src_u16 = (uint16_t const *)src + offset;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                for (; j + 8 <= n; j += 8) {
                    __m128i h = _mm_loadu_si128((const __m128i *)(src_u16 + j));
                    _mm256_storeu_ps(&dst[j], _mm256_cvtph_ps(h));
                }
            }
#else
            {
                for (; j + 4 <= n; j += 4) {
                    __m128i h = _mm_loadl_epi64((const __m128i *)(src_u16 + j));
                    __m128 f = _mm_cvtph_ps(h);
                    _mm256_storeu_pd(&dst[j], _mm256_cvtps_pd(f));
                }
            }
#endif
#endif
            for (; j < n; j++)
                dst[j] = (ALWAN_TILE_T)alwan__f16_to_f32(src_u16[j]);
        } else {
            for (j = 0; j < n; j++)
                dst[j] = (ALWAN_TILE_T)alwan__f16_to_f32(*(uint16_t const *)((char const *)src + (offset + j) * stride));
        }
    } break;
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++)
            dst[j] = (ALWAN_TILE_T)*(float const *)((char const *)src + (offset + j) * stride);
    } break;
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++)
            dst[j] = (ALWAN_TILE_T)*(double const *)((char const *)src + (offset + j) * stride);
    } break;
    }
}

/* ----------------------------------------------------------------
 * Single-channel planar typed tile store
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_TILE_FN(alwan__store_tile_typed_ch)(void *dst, alwan_pixel_format fmt,
                                              size_t offset, size_t stride,
                                              ALWAN_TILE_T const *src, size_t n) {
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        if (stride == 1) {
            uint8_t *dst_u8 = (uint8_t *)dst + offset;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                __m256 const scale256 = _mm256_set1_ps(255.0f);
                __m256 const half256 = _mm256_set1_ps(0.5f);
                __m256 const zero256 = _mm256_setzero_ps();
                __m256 const max256 = _mm256_set1_ps(255.0f);
                for (; j + 8 <= n; j += 8) {
                    __m256 f = _mm256_loadu_ps(&src[j]);
                    f = _mm256_min_ps(_mm256_max_ps(_mm256_add_ps(_mm256_mul_ps(f, scale256), half256), zero256), max256);
                    __m256i i32 = _mm256_cvttps_epi32(f);
                    __m128i lo16 = _mm_packs_epi32(_mm256_castsi256_si128(i32), _mm256_extracti128_si256(i32, 1));
                    __m128i u8 = _mm_packus_epi16(lo16, _mm_setzero_si128());
                    _mm_storel_epi64((__m128i *)(dst_u8 + j), u8);
                }
            }
#else
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
#endif
            for (; j < n; j++) {
                ALWAN_TILE_T v = src[j] * ALWAN_TILE_LIT(255.0) + ALWAN_TILE_LIT(0.5);
                if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                if (v > ALWAN_TILE_LIT(255.0)) v = ALWAN_TILE_LIT(255.0);
                dst_u8[j] = (uint8_t)v;
            }
        } else {
            for (j = 0; j < n; j++) {
                ALWAN_TILE_T v = src[j] * ALWAN_TILE_LIT(255.0) + ALWAN_TILE_LIT(0.5);
                if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                if (v > ALWAN_TILE_LIT(255.0)) v = ALWAN_TILE_LIT(255.0);
                *(uint8_t *)((char *)dst + (offset + j) * stride) = (uint8_t)v;
            }
        }
    } break;
    case ALWAN_PIXEL_U16: {
        if (stride == 2) {
            uint16_t *dst_u16 = (uint16_t *)dst + offset;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                __m256 const scale256 = _mm256_set1_ps(65535.0f);
                __m256 const half256  = _mm256_set1_ps(0.5f);
                __m256 const zero256  = _mm256_setzero_ps();
                __m256 const max256   = _mm256_set1_ps(65535.0f);
                for (; j + 8 <= n; j += 8) {
                    __m256 f = _mm256_loadu_ps(&src[j]);
                    f = _mm256_min_ps(_mm256_max_ps(_mm256_add_ps(_mm256_mul_ps(f, scale256), half256), zero256), max256);
                    __m256i i32 = _mm256_cvttps_epi32(f);
                    __m128i u16 = _mm_packus_epi32(_mm256_castsi256_si128(i32), _mm256_extracti128_si256(i32, 1));
                    _mm_storeu_si128((__m128i *)(dst_u16 + j), u16);
                }
            }
#else
            {
                __m256d const scale256d = _mm256_set1_pd(65535.0);
                __m256d const half256d  = _mm256_set1_pd(0.5);
                __m256d const zero256d  = _mm256_setzero_pd();
                __m256d const max256d   = _mm256_set1_pd(65535.0);
                for (; j + 4 <= n; j += 4) {
                    __m256d f = _mm256_loadu_pd(&src[j]);
                    f = _mm256_min_pd(_mm256_max_pd(_mm256_add_pd(_mm256_mul_pd(f, scale256d), half256d), zero256d), max256d);
                    __m128i i32 = _mm256_cvttpd_epi32(f);
                    __m128i u16 = _mm_packus_epi32(i32, _mm_setzero_si128());
                    _mm_storel_epi64((__m128i *)(dst_u16 + j), u16);
                }
            }
#endif
#endif
            for (; j < n; j++) {
                ALWAN_TILE_T v = src[j] * ALWAN_TILE_LIT(65535.0) + ALWAN_TILE_LIT(0.5);
                if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                if (v > ALWAN_TILE_LIT(65535.0)) v = ALWAN_TILE_LIT(65535.0);
                dst_u16[j] = (uint16_t)v;
            }
        } else {
            for (j = 0; j < n; j++) {
                ALWAN_TILE_T v = src[j] * ALWAN_TILE_LIT(65535.0) + ALWAN_TILE_LIT(0.5);
                if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                if (v > ALWAN_TILE_LIT(65535.0)) v = ALWAN_TILE_LIT(65535.0);
                *(uint16_t *)((char *)dst + (offset + j) * stride) = (uint16_t)v;
            }
        }
    } break;
    case ALWAN_PIXEL_F16: {
        if (stride == 2) {
            uint16_t *dst_u16 = (uint16_t *)dst + offset;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                for (; j + 8 <= n; j += 8) {
                    __m256 f = _mm256_loadu_ps(&src[j]);
                    _mm_storeu_si128((__m128i *)(dst_u16 + j), _mm256_cvtps_ph(f, _MM_FROUND_TO_NEAREST_INT));
                }
            }
#else
            {
                for (; j + 4 <= n; j += 4) {
                    __m256d d = _mm256_loadu_pd(&src[j]);
                    __m128 f = _mm256_cvtpd_ps(d);
                    __m128i h = _mm_cvtps_ph(f, _MM_FROUND_TO_NEAREST_INT);
                    _mm_storel_epi64((__m128i *)(dst_u16 + j), h);
                }
            }
#endif
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

/* ----------------------------------------------------------------
 * AoS interleaved typed tile load
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_TILE_FN(alwan__load_tile_typed_aos)(ALWAN_TILE_T *dst,
                                              void const *src, alwan_pixel_format fmt,
                                              size_t offset, size_t stride, size_t n, int ch) {
    size_t j;
    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        ALWAN_TILE_T const inv = (ALWAN_TILE_T)(1.0 / 255.0);
        if (ch == 3 && stride == 3) {
            uint8_t const *src_u8 = (uint8_t const *)src + offset * 3;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                __m128 const inv128 = _mm_set1_ps(1.0f / 255.0f);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw = _mm_loadu_si128((const __m128i *)(src_u8 + j * 3));
                    __m128 f0 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(raw)), inv128);
                    __m128 f1 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(raw, 4))), inv128);
                    __m128 f2 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(raw, 8))), inv128);
                    _mm_storeu_ps(&dst[j * 3], f0);
                    _mm_storeu_ps(&dst[j * 3 + 4], f1);
                    _mm_storeu_ps(&dst[j * 3 + 8], f2);
                }
            }
#else
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
#endif
            for (; j < n; j++) {
                dst[j * 3 + 0] = (ALWAN_TILE_T)src_u8[j * 3 + 0] * inv;
                dst[j * 3 + 1] = (ALWAN_TILE_T)src_u8[j * 3 + 1] * inv;
                dst[j * 3 + 2] = (ALWAN_TILE_T)src_u8[j * 3 + 2] * inv;
            }
        } else {
            for (j = 0; j < n; j++) {
                uint8_t const *p = (uint8_t const *)((char const *)src + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (ALWAN_TILE_T)p[c] * inv;
            }
        }
    } break;
    case ALWAN_PIXEL_U16: {
        ALWAN_TILE_T const inv = (ALWAN_TILE_T)(1.0 / 65535.0);
        if (ch == 3 && stride == 6) {
            uint16_t const *src_u16 = (uint16_t const *)src + offset * 3;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            {
                __m128 const inv128 = _mm_set1_ps(1.0f / 65535.0f);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw0 = _mm_loadu_si128((const __m128i *)(src_u16 + j * 3));
                    __m128i raw1 = _mm_loadl_epi64((const __m128i *)(src_u16 + j * 3 + 8));
                    __m128 f0 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu16_epi32(raw0)), inv128);
                    __m128 f1 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu16_epi32(_mm_srli_si128(raw0, 8))), inv128);
                    __m128 f2 = _mm_mul_ps(_mm_cvtepi32_ps(_mm_cvtepu16_epi32(raw1)), inv128);
                    _mm_storeu_ps(&dst[j * 3],     f0);
                    _mm_storeu_ps(&dst[j * 3 + 4], f1);
                    _mm_storeu_ps(&dst[j * 3 + 8], f2);
                }
            }
#else
            {
                __m256d const inv256d = _mm256_set1_pd(1.0 / 65535.0);
                for (; j + 4 <= n; j += 4) {
                    __m128i raw0 = _mm_loadu_si128((const __m128i *)(src_u16 + j * 3));
                    __m128i raw1 = _mm_loadl_epi64((const __m128i *)(src_u16 + j * 3 + 8));
                    _mm256_storeu_pd(&dst[j * 3],     _mm256_mul_pd(_mm256_cvtepi32_pd(_mm_cvtepu16_epi32(raw0)), inv256d));
                    _mm256_storeu_pd(&dst[j * 3 + 4], _mm256_mul_pd(_mm256_cvtepi32_pd(_mm_cvtepu16_epi32(_mm_srli_si128(raw0, 8))), inv256d));
                    _mm256_storeu_pd(&dst[j * 3 + 8], _mm256_mul_pd(_mm256_cvtepi32_pd(_mm_cvtepu16_epi32(raw1)), inv256d));
                }
            }
#endif
#endif
            for (; j < n; j++) {
                dst[j * 3 + 0] = (ALWAN_TILE_T)src_u16[j * 3 + 0] * inv;
                dst[j * 3 + 1] = (ALWAN_TILE_T)src_u16[j * 3 + 1] * inv;
                dst[j * 3 + 2] = (ALWAN_TILE_T)src_u16[j * 3 + 2] * inv;
            }
        } else {
            for (j = 0; j < n; j++) {
                uint16_t const *p = (uint16_t const *)((char const *)src + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (ALWAN_TILE_T)p[c] * inv;
            }
        }
    } break;
    case ALWAN_PIXEL_F16: {
        if (ch == 3 && stride == 6) {
            uint16_t const *src_u16 = (uint16_t const *)src + offset * 3;
            j = 0;
#if defined(__AVX2__)
#if ALWAN_TILE_IS_F32
            for (; j + 4 <= n; j += 4) {
                __m128i h0 = _mm_loadu_si128((const __m128i *)(src_u16 + j * 3));
                __m128i h1 = _mm_loadl_epi64((const __m128i *)(src_u16 + j * 3 + 8));
                __m128 f0 = _mm_cvtph_ps(h0);
                __m128 f1 = _mm_cvtph_ps(_mm_srli_si128(h0, 8));
                __m128 f2 = _mm_cvtph_ps(h1);
                _mm_storeu_ps(&dst[j * 3], f0);
                _mm_storeu_ps(&dst[j * 3 + 4], f1);
                _mm_storeu_ps(&dst[j * 3 + 8], f2);
            }
#else
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
#endif
            for (; j < n; j++) {
                dst[j * 3 + 0] = (ALWAN_TILE_T)alwan__f16_to_f32(src_u16[j * 3 + 0]);
                dst[j * 3 + 1] = (ALWAN_TILE_T)alwan__f16_to_f32(src_u16[j * 3 + 1]);
                dst[j * 3 + 2] = (ALWAN_TILE_T)alwan__f16_to_f32(src_u16[j * 3 + 2]);
            }
        } else {
            for (j = 0; j < n; j++) {
                uint16_t const *p = (uint16_t const *)((char const *)src + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (ALWAN_TILE_T)alwan__f16_to_f32(p[c]);
            }
        }
    } break;
    case ALWAN_PIXEL_F32: {
        for (j = 0; j < n; j++) {
            float const *p = (float const *)((char const *)src + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (ALWAN_TILE_T)p[c];
        }
    } break;
    case ALWAN_PIXEL_F64: {
        for (j = 0; j < n; j++) {
            double const *p = (double const *)((char const *)src + (offset + j) * stride);
            int c; for (c = 0; c < ch; c++) dst[j * ch + c] = (ALWAN_TILE_T)p[c];
        }
    } break;
    }
}

/* ----------------------------------------------------------------
 * AoS interleaved typed tile store
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_TILE_FN(alwan__store_tile_typed_aos)(void *dst, alwan_pixel_format fmt,
                                               size_t offset, size_t stride,
                                               ALWAN_TILE_T const *src, size_t n, int ch) {
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
#if ALWAN_TILE_IS_F32
                    __m128 f0 = _mm_loadu_ps(&src[j * 3]);
                    __m128 f1 = _mm_loadu_ps(&src[j * 3 + 4]);
                    __m128 f2 = _mm_loadu_ps(&src[j * 3 + 8]);
#else
                    __m128 f0 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3]));
                    __m128 f1 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 4]));
                    __m128 f2 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 8]));
#endif
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
                    ALWAN_TILE_T v = src[j * 3 + c] * ALWAN_TILE_LIT(255.0) + ALWAN_TILE_LIT(0.5);
                    if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                    if (v > ALWAN_TILE_LIT(255.0)) v = ALWAN_TILE_LIT(255.0);
                    dst_u8[j * 3 + c] = (uint8_t)v;
                }
            }
        } else {
            for (j = 0; j < n; j++) {
                uint8_t *p = (uint8_t *)((char *)dst + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) {
                    ALWAN_TILE_T v = src[j * ch + c] * ALWAN_TILE_LIT(255.0) + ALWAN_TILE_LIT(0.5);
                    if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                    if (v > ALWAN_TILE_LIT(255.0)) v = ALWAN_TILE_LIT(255.0);
                    p[c] = (uint8_t)v;
                }
            }
        }
    } break;
    case ALWAN_PIXEL_U16: {
        if (ch == 3 && stride == 6) {
            uint16_t *dst_u16 = (uint16_t *)dst + offset * 3;
            j = 0;
#if defined(__AVX2__)
            {
                __m128 const scale128 = _mm_set1_ps(65535.0f);
                __m128 const half128  = _mm_set1_ps(0.5f);
                __m128 const zero128  = _mm_setzero_ps();
                __m128 const max128   = _mm_set1_ps(65535.0f);
                for (; j + 4 <= n; j += 4) {
#if ALWAN_TILE_IS_F32
                    __m128 f0 = _mm_loadu_ps(&src[j * 3]);
                    __m128 f1 = _mm_loadu_ps(&src[j * 3 + 4]);
                    __m128 f2 = _mm_loadu_ps(&src[j * 3 + 8]);
#else
                    __m128 f0 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3]));
                    __m128 f1 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 4]));
                    __m128 f2 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 8]));
#endif
                    f0 = _mm_min_ps(_mm_max_ps(_mm_add_ps(_mm_mul_ps(f0, scale128), half128), zero128), max128);
                    f1 = _mm_min_ps(_mm_max_ps(_mm_add_ps(_mm_mul_ps(f1, scale128), half128), zero128), max128);
                    f2 = _mm_min_ps(_mm_max_ps(_mm_add_ps(_mm_mul_ps(f2, scale128), half128), zero128), max128);
                    __m128i i0  = _mm_cvttps_epi32(f0);
                    __m128i i1  = _mm_cvttps_epi32(f1);
                    __m128i i2  = _mm_cvttps_epi32(f2);
                    __m128i u01 = _mm_packus_epi32(i0, i1);
                    __m128i u2z = _mm_packus_epi32(i2, _mm_setzero_si128());
                    _mm_storeu_si128((__m128i *)(dst_u16 + j * 3), u01);
                    _mm_storel_epi64((__m128i *)(dst_u16 + j * 3 + 8), u2z);
                }
            }
#endif
            for (; j < n; j++) {
                int c; for (c = 0; c < 3; c++) {
                    ALWAN_TILE_T v = src[j * 3 + c] * ALWAN_TILE_LIT(65535.0) + ALWAN_TILE_LIT(0.5);
                    if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                    if (v > ALWAN_TILE_LIT(65535.0)) v = ALWAN_TILE_LIT(65535.0);
                    dst_u16[j * 3 + c] = (uint16_t)v;
                }
            }
        } else {
            for (j = 0; j < n; j++) {
                uint16_t *p = (uint16_t *)((char *)dst + (offset + j) * stride);
                int c; for (c = 0; c < ch; c++) {
                    ALWAN_TILE_T v = src[j * ch + c] * ALWAN_TILE_LIT(65535.0) + ALWAN_TILE_LIT(0.5);
                    if (v < ALWAN_TILE_LIT(0.0)) v = ALWAN_TILE_LIT(0.0);
                    if (v > ALWAN_TILE_LIT(65535.0)) v = ALWAN_TILE_LIT(65535.0);
                    p[c] = (uint16_t)v;
                }
            }
        }
    } break;
    case ALWAN_PIXEL_F16: {
        if (ch == 3 && stride == 6) {
            uint16_t *dst_u16 = (uint16_t *)dst + offset * 3;
            j = 0;
#if defined(__AVX2__)
            for (; j + 4 <= n; j += 4) {
#if ALWAN_TILE_IS_F32
                __m128 f0 = _mm_loadu_ps(&src[j * 3]);
                __m128 f1 = _mm_loadu_ps(&src[j * 3 + 4]);
                __m128 f2 = _mm_loadu_ps(&src[j * 3 + 8]);
#else
                __m128 f0 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3]));
                __m128 f1 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 4]));
                __m128 f2 = _mm256_cvtpd_ps(_mm256_loadu_pd(&src[j * 3 + 8]));
#endif
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

#undef ALWAN_TILE_FN
#undef ALWAN_TILE_FN_
#undef ALWAN_TILE_FN2_
