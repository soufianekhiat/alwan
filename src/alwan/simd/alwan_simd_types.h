/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * SIMD type aliases and width constants
 */

#ifndef ALWAN_SIMD_TYPES_H
#define ALWAN_SIMD_TYPES_H

#include <stdint.h>

/* ----------------------------------------------------------------
 * SIMD Width Constants
 * Width = number of lanes per SIMD register for each element type.
 * Width only changes at SSE2 -> AVX -> AVX2 boundaries.
 * SSE3/SSSE3/SSE4.x add instructions, not width.
 * ---------------------------------------------------------------- */

#if defined(__AVX2__)
#  define ALWAN_SIMD_UINT8_WIDTH   32
#  define ALWAN_SIMD_UINT16_WIDTH  16
#  define ALWAN_SIMD_F32_WIDTH      8
#  define ALWAN_SIMD_F64_WIDTH      4
#elif defined(__AVX__)
#  define ALWAN_SIMD_UINT8_WIDTH   16  /* AVX has no 256-bit integer ops */
#  define ALWAN_SIMD_UINT16_WIDTH   8
#  define ALWAN_SIMD_F32_WIDTH      8  /* 256-bit float */
#  define ALWAN_SIMD_F64_WIDTH      4  /* 256-bit double */
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#  define ALWAN_SIMD_UINT8_WIDTH   16
#  define ALWAN_SIMD_UINT16_WIDTH   8
#  define ALWAN_SIMD_F32_WIDTH      4
#  define ALWAN_SIMD_F64_WIDTH      2
#elif defined(__aarch64__) || defined(__ARM_NEON)
#  define ALWAN_SIMD_UINT8_WIDTH   16
#  define ALWAN_SIMD_UINT16_WIDTH   8
#  define ALWAN_SIMD_F32_WIDTH      4
#  define ALWAN_SIMD_F64_WIDTH      2
#else
#  define ALWAN_SIMD_UINT8_WIDTH    1
#  define ALWAN_SIMD_UINT16_WIDTH   1
#  define ALWAN_SIMD_F32_WIDTH      1
#  define ALWAN_SIMD_F64_WIDTH      1
#endif

/* ----------------------------------------------------------------
 * Alignment macro
 * ---------------------------------------------------------------- */

#if defined(_MSC_VER)
#  define ALWAN_ALIGN(n) __declspec(align(n))
#elif defined(__GNUC__) || defined(__clang__)
#  define ALWAN_ALIGN(n) __attribute__((aligned(n)))
#else
#  define ALWAN_ALIGN(n)
#endif

/* ----------------------------------------------------------------
 * Type Aliases
 * ---------------------------------------------------------------- */

#if defined(__AVX2__)
#  include <immintrin.h>
   typedef __m256   alwan_simd_f32;
   typedef __m256d  alwan_simd_f64;
   typedef __m256i  alwan_simd_u8;
   typedef __m256i  alwan_simd_u16;
   typedef __m256i  alwan_simd_i32;
   typedef __m256   alwan_simd_f32_mask;
   typedef __m256d  alwan_simd_f64_mask;
#elif defined(__AVX__)
#  include <immintrin.h>
   typedef __m256   alwan_simd_f32;
   typedef __m256d  alwan_simd_f64;
   typedef __m128i  alwan_simd_u8;       /* 128-bit: AVX has no 256-bit int */
   typedef __m128i  alwan_simd_u16;
   typedef __m128i  alwan_simd_i32;
   typedef __m256   alwan_simd_f32_mask;
   typedef __m256d  alwan_simd_f64_mask;
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#  include <emmintrin.h>
#  if defined(__SSE3__)
#    include <pmmintrin.h>
#  endif
#  if defined(__SSSE3__)
#    include <tmmintrin.h>
#  endif
#  if defined(__SSE4_1__)
#    include <smmintrin.h>
#  endif
#  if defined(__SSE4_2__)
#    include <nmmintrin.h>
#  endif
   typedef __m128   alwan_simd_f32;
   typedef __m128d  alwan_simd_f64;
   typedef __m128i  alwan_simd_u8;
   typedef __m128i  alwan_simd_u16;
   typedef __m128i  alwan_simd_i32;
   typedef __m128   alwan_simd_f32_mask;
   typedef __m128d  alwan_simd_f64_mask;
#elif defined(__aarch64__) || defined(__ARM_NEON)
#  include <arm_neon.h>
   typedef float32x4_t  alwan_simd_f32;
#  if defined(__aarch64__)
   typedef float64x2_t  alwan_simd_f64;
#  else
   typedef double        alwan_simd_f64;   /* ARMv7: no f64 SIMD, scalar fallback */
#  endif
   typedef uint8x16_t   alwan_simd_u8;
   typedef uint16x8_t   alwan_simd_u16;
   typedef int32x4_t    alwan_simd_i32;
   typedef uint32x4_t   alwan_simd_f32_mask;
#  if defined(__aarch64__)
   typedef uint64x2_t   alwan_simd_f64_mask;
#  else
   typedef uint64_t     alwan_simd_f64_mask; /* ARMv7: scalar mask */
#  endif
#else
   typedef float    alwan_simd_f32;
   typedef double   alwan_simd_f64;
   typedef uint8_t  alwan_simd_u8;
   typedef uint16_t alwan_simd_u16;
   typedef int32_t  alwan_simd_i32;
   typedef int      alwan_simd_f32_mask;
   typedef int      alwan_simd_f64_mask;
#endif

/* ----------------------------------------------------------------
 * SVML Detection
 * Intel SVML provides true SIMD vector math intrinsics
 * (_mm_cbrt_ps, _mm256_sin_pd, etc.)
 * Intel compilers always have it; MSVC x64 ships svml_dispmd.lib.
 * ---------------------------------------------------------------- */

#if defined(__INTEL_COMPILER) || defined(__INTEL_LLVM_COMPILER)
#  define ALWAN_HAS_SVML 1
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#  define ALWAN_HAS_SVML 1
#else
#  define ALWAN_HAS_SVML 0
#endif

#endif /* ALWAN_SIMD_TYPES_H */
