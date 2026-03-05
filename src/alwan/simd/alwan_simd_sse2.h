/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * SIMD SSE2 backend (x64 baseline) with progressive SSE3/SSSE3/SSE4.x upgrades
 */

#ifndef ALWAN_SIMD_SSE2_H
#define ALWAN_SIMD_SSE2_H

#include "alwan_simd_types.h"
#include "../alwan_platform.h"
#include <math.h>

/* SVML intrinsics need immintrin.h (already included for AVX+, but not for SSE2 baseline) */
#if ALWAN_HAS_SVML
#  include <immintrin.h>
#endif

/* ================================================================
 * Float32 Operations (128-bit, 4 lanes)
 * ================================================================ */

/* ----------------------------------------------------------------
 * Float32 Arithmetic
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_add_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sub(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_sub_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_mul(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_mul_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_div(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_div_ps(a, b); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_neg(alwan_simd_f32 a) {
    return _mm_xor_ps(a, _mm_set1_ps(-0.0f));
}

/* ----------------------------------------------------------------
 * Float32 Broadcast / Zero
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_set1(float v) { return _mm_set1_ps(v); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_zero(void) { return _mm_setzero_ps(); }

/* ----------------------------------------------------------------
 * Float32 Math
 * SVML: true SIMD vector math when available
 * Fallback: per-lane libm for exact results
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a) { return _mm_sqrt_ps(a); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_abs(alwan_simd_f32 a) {
    __m128i mask = _mm_set1_epi32(0x7FFFFFFF);
    return _mm_and_ps(a, _mm_castsi128_ps(mask));
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_cbrt_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = cbrtf(v[0]); r[1] = cbrtf(v[1]); r[2] = cbrtf(v[2]); r[3] = cbrtf(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_exp(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_exp_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = expf(v[0]); r[1] = expf(v[1]); r[2] = expf(v[2]); r[3] = expf(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_log_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = logf(v[0]); r[1] = logf(v[1]); r[2] = logf(v[2]); r[3] = logf(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log2(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_log2_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = log2f(v[0]); r[1] = log2f(v[1]); r[2] = log2f(v[2]); r[3] = log2f(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log10(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_log10_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = log10f(v[0]); r[1] = log10f(v[1]); r[2] = log10f(v[2]); r[3] = log10f(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sin(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_sin_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = sinf(v[0]); r[1] = sinf(v[1]); r[2] = sinf(v[2]); r[3] = sinf(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cos(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_cos_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = cosf(v[0]); r[1] = cosf(v[1]); r[2] = cosf(v[2]); r[3] = cosf(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 e) {
#if ALWAN_HAS_SVML
    return _mm_pow_ps(base, e);
#else
    ALWAN_ALIGN(16) float b[4], ex[4], r[4];
    _mm_store_ps(b, base);
    _mm_store_ps(ex, e);
    r[0] = powf(b[0], ex[0]); r[1] = powf(b[1], ex[1]);
    r[2] = powf(b[2], ex[2]); r[3] = powf(b[3], ex[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_atan2(alwan_simd_f32 y, alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm_atan2_ps(y, x);
#else
    ALWAN_ALIGN(16) float vy[4], vx[4], r[4];
    _mm_store_ps(vy, y);
    _mm_store_ps(vx, x);
    r[0] = atan2f(vy[0], vx[0]); r[1] = atan2f(vy[1], vx[1]);
    r[2] = atan2f(vy[2], vx[2]); r[3] = atan2f(vy[3], vx[3]);
    return _mm_load_ps(r);
#endif
}

/* ----------------------------------------------------------------
 * Float32 FMA (SSE2: emulate with mul+add)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return _mm_add_ps(_mm_mul_ps(a, b), c);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmsub(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return _mm_sub_ps(_mm_mul_ps(a, b), c);
}

/* ----------------------------------------------------------------
 * Float32 Comparison -> Mask
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpeq(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_cmpeq_ps(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmplt(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_cmplt_ps(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmple(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_cmple_ps(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpgt(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_cmpgt_ps(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpge(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_cmpge_ps(a, b); }

/* ----------------------------------------------------------------
 * Float32 Select
 * SSE4.1: blendvps (1 instr); SSE2: AND/ANDNOT/OR (3 instr)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m, alwan_simd_f32 a, alwan_simd_f32 b) {
#if defined(__SSE4_1__)
    return _mm_blendv_ps(b, a, m);
#else
    return _mm_or_ps(_mm_and_ps(m, a), _mm_andnot_ps(m, b));
#endif
}

/* ----------------------------------------------------------------
 * Float32 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_min(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_min_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_max(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm_max_ps(a, b); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_clamp(alwan_simd_f32 v, alwan_simd_f32 lo, alwan_simd_f32 hi) {
    return _mm_min_ps(_mm_max_ps(v, lo), hi);
}

/* ----------------------------------------------------------------
 * Float32 Horizontal Add
 * SSE3: haddps; SSE2: shuffle+add
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) {
#if defined(__SSE3__)
    return _mm_hadd_ps(a, b);
#else
    __m128 t0 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 0, 2, 0));
    __m128 t1 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 1, 3, 1));
    return _mm_add_ps(t0, t1);
#endif
}

/* ----------------------------------------------------------------
 * Float32 Horizontal Sum
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum(alwan_simd_f32 a) {
#if defined(__SSE3__)
    __m128 t = _mm_hadd_ps(a, a);
    return _mm_hadd_ps(t, t);
#else
    __m128 t1 = _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 t2 = _mm_add_ps(a, t1);
    __m128 t3 = _mm_shuffle_ps(t2, t2, _MM_SHUFFLE(0, 1, 2, 3));
    return _mm_add_ps(t2, t3);
#endif
}

/* ----------------------------------------------------------------
 * Float32 Rounding
 * SSE4.1: roundps; SSE2: cast-to-int with fixup
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_floor_ps(a);
#else
    __m128i ti = _mm_cvttps_epi32(a);
    __m128 t = _mm_cvtepi32_ps(ti);
    __m128 mask = _mm_cmpgt_ps(t, a);
    __m128 one = _mm_set1_ps(1.0f);
    return _mm_sub_ps(t, _mm_and_ps(mask, one));
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_ceil_ps(a);
#else
    __m128i ti = _mm_cvttps_epi32(a);
    __m128 t = _mm_cvtepi32_ps(ti);
    __m128 mask = _mm_cmplt_ps(t, a);
    __m128 one = _mm_set1_ps(1.0f);
    return _mm_add_ps(t, _mm_and_ps(mask, one));
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
#else
    return _mm_cvtepi32_ps(_mm_cvtps_epi32(a));
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_trunc(alwan_simd_f32 a) {
#if defined(__SSE4_1__)
    return _mm_round_ps(a, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
#else
    return _mm_cvtepi32_ps(_mm_cvttps_epi32(a));
#endif
}

/* ----------------------------------------------------------------
 * Float32 Reciprocal (exact: 1.0/x via divps, NOT approximate rcpps)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_rcp(alwan_simd_f32 a) {
    return _mm_div_ps(_mm_set1_ps(1.0f), a);
}

/* ----------------------------------------------------------------
 * Float32 Load / Store
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_load(float const *ptr) { return _mm_load_ps(ptr); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_loadu(float const *ptr) { return _mm_loadu_ps(ptr); }
ALWAN_INLINE void alwan_simd_f32_store(float *ptr, alwan_simd_f32 v) { _mm_store_ps(ptr, v); }
ALWAN_INLINE void alwan_simd_f32_storeu(float *ptr, alwan_simd_f32 v) { _mm_storeu_ps(ptr, v); }

/* ================================================================
 * Float64 Operations (128-bit, 2 lanes)
 * ================================================================ */

/* ----------------------------------------------------------------
 * Float64 Arithmetic
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_add(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_add_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sub(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_sub_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_mul(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_mul_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_div(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_div_pd(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_neg(alwan_simd_f64 a) {
    return _mm_xor_pd(a, _mm_set1_pd(-0.0));
}

/* ----------------------------------------------------------------
 * Float64 Broadcast / Zero
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_set1(double v) { return _mm_set1_pd(v); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_zero(void) { return _mm_setzero_pd(); }

/* ----------------------------------------------------------------
 * Float64 Math
 * SVML: true SIMD vector math when available
 * Fallback: per-lane libm for exact results
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sqrt(alwan_simd_f64 a) { return _mm_sqrt_pd(a); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_abs(alwan_simd_f64 a) {
    return _mm_andnot_pd(_mm_set1_pd(-0.0), a);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_cbrt_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = cbrt(v[0]); r[1] = cbrt(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_exp(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_exp_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = exp(v[0]); r[1] = exp(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_log_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = log(v[0]); r[1] = log(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log2(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_log2_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = log2(v[0]); r[1] = log2(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log10(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_log10_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = log10(v[0]); r[1] = log10(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sin(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_sin_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = sin(v[0]); r[1] = sin(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cos(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_cos_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = cos(v[0]); r[1] = cos(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow(alwan_simd_f64 base, alwan_simd_f64 e) {
#if ALWAN_HAS_SVML
    return _mm_pow_pd(base, e);
#else
    ALWAN_ALIGN(16) double b[2], ex[2], r[2];
    _mm_store_pd(b, base);
    _mm_store_pd(ex, e);
    r[0] = pow(b[0], ex[0]); r[1] = pow(b[1], ex[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_atan2(alwan_simd_f64 y, alwan_simd_f64 x) {
#if ALWAN_HAS_SVML
    return _mm_atan2_pd(y, x);
#else
    ALWAN_ALIGN(16) double vy[2], vx[2], r[2];
    _mm_store_pd(vy, y);
    _mm_store_pd(vx, x);
    r[0] = atan2(vy[0], vx[0]); r[1] = atan2(vy[1], vx[1]);
    return _mm_load_pd(r);
#endif
}

/* ----------------------------------------------------------------
 * Float64 FMA (SSE2: emulate with mul+add)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmadd(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    return _mm_add_pd(_mm_mul_pd(a, b), c);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmsub(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    return _mm_sub_pd(_mm_mul_pd(a, b), c);
}

/* ----------------------------------------------------------------
 * Float64 Comparison -> Mask
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpeq(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_cmpeq_pd(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmplt(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_cmplt_pd(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmple(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_cmple_pd(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpgt(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_cmpgt_pd(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpge(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_cmpge_pd(a, b); }

/* ----------------------------------------------------------------
 * Float64 Select
 * SSE4.1: blendvpd (1 instr); SSE2: AND/ANDNOT/OR (3 instr)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_select(alwan_simd_f64_mask m, alwan_simd_f64 a, alwan_simd_f64 b) {
#if defined(__SSE4_1__)
    return _mm_blendv_pd(b, a, m);
#else
    return _mm_or_pd(_mm_and_pd(m, a), _mm_andnot_pd(m, b));
#endif
}

/* ----------------------------------------------------------------
 * Float64 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_min_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_max_pd(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    return _mm_min_pd(_mm_max_pd(v, lo), hi);
}

/* ----------------------------------------------------------------
 * Float64 Rounding
 * SSE4.1: roundpd; SSE2: cast-to-int with fixup
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_floor(alwan_simd_f64 a) {
#if defined(__SSE4_1__)
    return _mm_floor_pd(a);
#else
    __m128i ti = _mm_cvttpd_epi32(a);
    __m128d t = _mm_cvtepi32_pd(ti);
    __m128d mask = _mm_cmpgt_pd(t, a);
    __m128d one = _mm_set1_pd(1.0);
    return _mm_sub_pd(t, _mm_and_pd(mask, one));
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_ceil(alwan_simd_f64 a) {
#if defined(__SSE4_1__)
    return _mm_ceil_pd(a);
#else
    __m128i ti = _mm_cvttpd_epi32(a);
    __m128d t = _mm_cvtepi32_pd(ti);
    __m128d mask = _mm_cmplt_pd(t, a);
    __m128d one = _mm_set1_pd(1.0);
    return _mm_add_pd(t, _mm_and_pd(mask, one));
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_round(alwan_simd_f64 a) {
#if defined(__SSE4_1__)
    return _mm_round_pd(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
#else
    return _mm_cvtepi32_pd(_mm_cvtpd_epi32(a));
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_trunc(alwan_simd_f64 a) {
#if defined(__SSE4_1__)
    return _mm_round_pd(a, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
#else
    return _mm_cvtepi32_pd(_mm_cvttpd_epi32(a));
#endif
}

/* ----------------------------------------------------------------
 * Float64 Reciprocal (exact: 1.0/x)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_rcp(alwan_simd_f64 a) {
    return _mm_div_pd(_mm_set1_pd(1.0), a);
}

/* ----------------------------------------------------------------
 * Float64 Load / Store
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_load(double const *ptr) { return _mm_load_pd(ptr); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_loadu(double const *ptr) { return _mm_loadu_pd(ptr); }
ALWAN_INLINE void alwan_simd_f64_store(double *ptr, alwan_simd_f64 v) { _mm_store_pd(ptr, v); }
ALWAN_INLINE void alwan_simd_f64_storeu(double *ptr, alwan_simd_f64 v) { _mm_storeu_pd(ptr, v); }

/* ================================================================
 * Integer Operations
 * ================================================================ */

/* ----------------------------------------------------------------
 * Integer Load / Store & Conversion
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_load(uint8_t const *ptr) { return _mm_load_si128((const __m128i *)ptr); }
ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_loadu(uint8_t const *ptr) { return _mm_loadu_si128((const __m128i *)ptr); }
ALWAN_INLINE void alwan_simd_u8_store(uint8_t *ptr, alwan_simd_u8 v) { _mm_store_si128((__m128i *)ptr, v); }
ALWAN_INLINE void alwan_simd_u8_storeu(uint8_t *ptr, alwan_simd_u8 v) { _mm_storeu_si128((__m128i *)ptr, v); }

ALWAN_INLINE alwan_simd_u16 alwan_simd_u16_load(uint16_t const *ptr) { return _mm_load_si128((const __m128i *)ptr); }
ALWAN_INLINE alwan_simd_u16 alwan_simd_u16_loadu(uint16_t const *ptr) { return _mm_loadu_si128((const __m128i *)ptr); }
ALWAN_INLINE void alwan_simd_u16_store(uint16_t *ptr, alwan_simd_u16 v) { _mm_store_si128((__m128i *)ptr, v); }
ALWAN_INLINE void alwan_simd_u16_storeu(uint16_t *ptr, alwan_simd_u16 v) { _mm_storeu_si128((__m128i *)ptr, v); }

/* SSE4.1: pmovzxbd (1 instr); SSE2: unpack chain */
ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) {
#if defined(__SSE4_1__)
    __m128i i32 = _mm_cvtepu8_epi32(v);
    return _mm_cvtepi32_ps(i32);
#else
    __m128i zero = _mm_setzero_si128();
    __m128i u16 = _mm_unpacklo_epi8(v, zero);
    __m128i i32 = _mm_unpacklo_epi16(u16, zero);
    return _mm_cvtepi32_ps(i32);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_u16_to_f32(alwan_simd_u16 v) {
#if defined(__SSE4_1__)
    __m128i i32 = _mm_cvtepu16_epi32(v);
    return _mm_cvtepi32_ps(i32);
#else
    __m128i zero = _mm_setzero_si128();
    __m128i i32 = _mm_unpacklo_epi16(v, zero);
    return _mm_cvtepi32_ps(i32);
#endif
}

/* SSSE3: pshufb (1 instr); SSE2: scalar fallback */
ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_shuffle(alwan_simd_u8 v, alwan_simd_u8 mask) {
#if defined(__SSSE3__)
    return _mm_shuffle_epi8(v, mask);
#else
    ALWAN_ALIGN(16) uint8_t vb[16], mb[16], rb[16];
    _mm_store_si128((__m128i *)vb, v);
    _mm_store_si128((__m128i *)mb, mask);
    for (int i = 0; i < 16; i++)
        rb[i] = (mb[i] & 0x80) ? 0 : vb[mb[i] & 0x0F];
    return _mm_load_si128((const __m128i *)rb);
#endif
}

#endif /* ALWAN_SIMD_SSE2_H */
