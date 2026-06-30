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
#include "../alwan_math.h"   /* must come before per-lane ALWAN_POW_F32/etc.
                                 * uses below; alwan_math.h supplies det-mode
                                 * redefinitions when ALWAN_DETERMINISTIC=1. */
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
    r[0] = ALWAN_CBRT_F32(v[0]); r[1] = ALWAN_CBRT_F32(v[1]); r[2] = ALWAN_CBRT_F32(v[2]); r[3] = ALWAN_CBRT_F32(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_exp(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_exp_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = ALWAN_EXP_F32(v[0]); r[1] = ALWAN_EXP_F32(v[1]); r[2] = ALWAN_EXP_F32(v[2]); r[3] = ALWAN_EXP_F32(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_log_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = ALWAN_LN_F32(v[0]); r[1] = ALWAN_LN_F32(v[1]); r[2] = ALWAN_LN_F32(v[2]); r[3] = ALWAN_LN_F32(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log2(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_log2_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = ALWAN_LOG2_F32(v[0]); r[1] = ALWAN_LOG2_F32(v[1]); r[2] = ALWAN_LOG2_F32(v[2]); r[3] = ALWAN_LOG2_F32(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log10(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_log10_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = ALWAN_LOG10_F32(v[0]); r[1] = ALWAN_LOG10_F32(v[1]); r[2] = ALWAN_LOG10_F32(v[2]); r[3] = ALWAN_LOG10_F32(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sin(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_sin_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = ALWAN_SIN_F32(v[0]); r[1] = ALWAN_SIN_F32(v[1]); r[2] = ALWAN_SIN_F32(v[2]); r[3] = ALWAN_SIN_F32(v[3]);
    return _mm_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cos(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm_cos_ps(a);
#else
    ALWAN_ALIGN(16) float v[4], r[4];
    _mm_store_ps(v, a);
    r[0] = ALWAN_COS_F32(v[0]); r[1] = ALWAN_COS_F32(v[1]); r[2] = ALWAN_COS_F32(v[2]); r[3] = ALWAN_COS_F32(v[3]);
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
    r[0] = ALWAN_POW_F32(b[0], ex[0]); r[1] = ALWAN_POW_F32(b[1], ex[1]);
    r[2] = ALWAN_POW_F32(b[2], ex[2]); r[3] = ALWAN_POW_F32(b[3], ex[3]);
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
    r[0] = ALWAN_ATAN2_F32(vy[0], vx[0]); r[1] = ALWAN_ATAN2_F32(vy[1], vx[1]);
    r[2] = ALWAN_ATAN2_F32(vy[2], vx[2]); r[3] = ALWAN_ATAN2_F32(vy[3], vx[3]);
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

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd_native(alwan_simd_f32 a, alwan_simd_f32 b) {
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

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum_native(alwan_simd_f32 a) {
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
    r[0] = ALWAN_CBRT_F64(v[0]); r[1] = ALWAN_CBRT_F64(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_exp(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_exp_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = ALWAN_EXP_F64(v[0]); r[1] = ALWAN_EXP_F64(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_log_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = ALWAN_LN_F64(v[0]); r[1] = ALWAN_LN_F64(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log2(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_log2_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = ALWAN_LOG2_F64(v[0]); r[1] = ALWAN_LOG2_F64(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log10(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_log10_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = ALWAN_LOG10_F64(v[0]); r[1] = ALWAN_LOG10_F64(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sin(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_sin_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = ALWAN_SIN_F64(v[0]); r[1] = ALWAN_SIN_F64(v[1]);
    return _mm_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cos(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm_cos_pd(a);
#else
    ALWAN_ALIGN(16) double v[2], r[2];
    _mm_store_pd(v, a);
    r[0] = ALWAN_COS_F64(v[0]); r[1] = ALWAN_COS_F64(v[1]);
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
    r[0] = ALWAN_POW_F64(b[0], ex[0]); r[1] = ALWAN_POW_F64(b[1], ex[1]);
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
    r[0] = ALWAN_ATAN2_F64(vy[0], vx[0]); r[1] = ALWAN_ATAN2_F64(vy[1], vx[1]);
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

ALWAN_INLINE int alwan_simd_f32_mask_all_set(alwan_simd_f32_mask m) { return _mm_movemask_ps(m) == 0xF; }
ALWAN_INLINE int alwan_simd_f64_mask_all_set(alwan_simd_f64_mask m) { return _mm_movemask_pd(m) == 0x3; }

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_and(alwan_simd_f32_mask a, alwan_simd_f32_mask b) { return _mm_and_ps(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_or (alwan_simd_f32_mask a, alwan_simd_f32_mask b) { return _mm_or_ps(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_and(alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return _mm_and_pd(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_or (alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return _mm_or_pd(a, b); }

/* ----------------------------------------------------------------
 * Float64 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_min_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm_max_pd(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    return _mm_min_pd(_mm_max_pd(v, lo), hi);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum_native(alwan_simd_f64 a) {
    __m128d t = _mm_shuffle_pd(a, a, 1);
    return _mm_add_pd(a, t);
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

/* ----------------------------------------------------------------
 * AoS <-> SoA 3-Channel Deinterleave / Interleave
 * ---------------------------------------------------------------- */

/* f32: load 4 RGB pixels (12 floats) -> 3 x __m128 SoA */
ALWAN_FORCE_INLINE void alwan_simd_f32_deinterleave3(float const *src,
                                                 alwan_simd_f32 *ch0, alwan_simd_f32 *ch1, alwan_simd_f32 *ch2) {
    __m128 v0 = _mm_loadu_ps(src);      /* a0 b0 c0 a1 */
    __m128 v1 = _mm_loadu_ps(src + 4);  /* b1 c1 a2 b2 */
    __m128 v2 = _mm_loadu_ps(src + 8);  /* c2 a3 b3 c3 */

    __m128 t0 = _mm_shuffle_ps(v0, v1, _MM_SHUFFLE(1, 0, 2, 1)); /* b0 c0 b1 c1 */
    __m128 t1 = _mm_shuffle_ps(v1, v2, _MM_SHUFFLE(2, 1, 3, 2)); /* a2 b2 a3 b3 */

    *ch0 = _mm_shuffle_ps(v0, t1, _MM_SHUFFLE(2, 0, 3, 0)); /* a0 a1 a2 a3 */
    *ch1 = _mm_shuffle_ps(t0, t1, _MM_SHUFFLE(3, 1, 2, 0)); /* b0 b1 b2 b3 */
    *ch2 = _mm_shuffle_ps(t0, v2, _MM_SHUFFLE(3, 0, 3, 1)); /* c0 c1 c2 c3 */
}

/* f32: store 3 x __m128 SoA -> 4 RGB pixels (12 floats) */
ALWAN_FORCE_INLINE void alwan_simd_f32_interleave3(float *dst,
                                               alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    __m128 u0 = _mm_unpacklo_ps(a, b);  /* a0 b0 a1 b1 */
    __m128 u1 = _mm_unpackhi_ps(a, b);  /* a2 b2 a3 b3 */
    __m128 bc_hi = _mm_unpackhi_ps(b, c); /* b2 c2 b3 c3 */

    /* v0 = [a0, b0, c0, a1] */
    __m128 tc = _mm_shuffle_ps(c, u0, _MM_SHUFFLE(2, 2, 0, 0));
    __m128 v0 = _mm_shuffle_ps(u0, tc, _MM_SHUFFLE(2, 0, 1, 0));
    _mm_storeu_ps(dst, v0);

    /* v1 = [b1, c1, a2, b2] */
    __m128 tc2 = _mm_shuffle_ps(u0, c, _MM_SHUFFLE(1, 1, 3, 3));
    __m128 v1 = _mm_shuffle_ps(tc2, u1, _MM_SHUFFLE(1, 0, 2, 0));
    _mm_storeu_ps(dst + 4, v1);

    /* v2 = [c2, a3, b3, c3] */
    __m128 tc3 = _mm_shuffle_ps(bc_hi, u1, _MM_SHUFFLE(2, 2, 1, 1));
    __m128 v2 = _mm_shuffle_ps(tc3, bc_hi, _MM_SHUFFLE(3, 2, 2, 0));
    _mm_storeu_ps(dst + 8, v2);
}

/* f64: load 2 RGB pixels (6 doubles) -> 3 x __m128d SoA */
ALWAN_FORCE_INLINE void alwan_simd_f64_deinterleave3(double const *src,
                                                 alwan_simd_f64 *ch0, alwan_simd_f64 *ch1, alwan_simd_f64 *ch2) {
    __m128d v0 = _mm_loadu_pd(src);      /* a0 b0 */
    __m128d v1 = _mm_loadu_pd(src + 2);  /* c0 a1 */
    __m128d v2 = _mm_loadu_pd(src + 4);  /* b1 c1 */

    *ch0 = _mm_shuffle_pd(v0, v1, 0x2); /* a0 a1 */
    *ch1 = _mm_shuffle_pd(v0, v2, 0x1); /* b0 b1 */
    *ch2 = _mm_shuffle_pd(v1, v2, 0x2); /* c0 c1 */
}

/* f64: store 3 x __m128d SoA -> 2 RGB pixels (6 doubles) */
ALWAN_FORCE_INLINE void alwan_simd_f64_interleave3(double *dst,
                                               alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    _mm_storeu_pd(dst,     _mm_shuffle_pd(a, b, 0x0)); /* a0 b0 */
    _mm_storeu_pd(dst + 2, _mm_shuffle_pd(c, a, 0x2)); /* c0 a1 */
    _mm_storeu_pd(dst + 4, _mm_shuffle_pd(b, c, 0x3)); /* b1 c1 */
}

/* ================================================================
 * Fast Approximation Functions
 * ================================================================ */

/* ----------------------------------------------------------------
 * Float32 ALWAN_POW_F64(x, 2.4) -- fast vectorized via exp2(2.4 * ALWAN_LOG2_F64(x))
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow24(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm_pow_ps(x, _mm_set1_ps(2.4f));
#else
    __m128 zero = _mm_setzero_ps();
    __m128 v = _mm_max_ps(x, zero);
    __m128 is_pos = _mm_cmpgt_ps(v, zero);

    /* ALWAN_LOG2_F64(v) via IEEE 754 decomposition */
    __m128i iv = _mm_castps_si128(v);
    __m128i exp_i = _mm_sub_epi32(_mm_srli_epi32(iv, 23), _mm_set1_epi32(127));
    __m128 e = _mm_cvtepi32_ps(exp_i);
    __m128i mant_bits = _mm_or_si128(
        _mm_and_si128(iv, _mm_set1_epi32(0x007FFFFF)),
        _mm_set1_epi32(0x3F800000));
    __m128 m = _mm_castsi128_ps(mant_bits);

    /* Minimax polynomial ALWAN_LOG2_F64(m) on [1, 2) */
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1); matches scalar twin */
    __m128 one_l = _mm_set1_ps(1.0f);
    __m128 s  = _mm_div_ps(_mm_sub_ps(m, one_l), _mm_add_ps(m, one_l));
    __m128 s2 = _mm_mul_ps(s, s);
    __m128 lp = _mm_add_ps(_mm_set1_ps(1.0f/11.0f), _mm_mul_ps(s2, _mm_set1_ps(1.0f/13.0f)));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/9.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/7.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/5.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/3.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(one_l, _mm_mul_ps(s2, lp));
    __m128 log2_m = _mm_mul_ps(_mm_set1_ps(2.8853901f), _mm_mul_ps(s, lp));

    __m128 y = _mm_mul_ps(_mm_set1_ps(2.4f), _mm_add_ps(e, log2_m));

    /* exp2(y) */
    __m128 yi = alwan_simd_f32_floor(y);
    __m128 yf = _mm_sub_ps(y, yi);
    __m128i n = _mm_cvttps_epi32(yi);

    /* Minimax polynomial 2^yf on [0, 1) */
    /* exp2(yf) on [0,1): degree-6 Taylor; matches scalar twin */
    __m128 e2 = _mm_add_ps(_mm_set1_ps(1.5252734e-05f),
                _mm_mul_ps(yf, _mm_set1_ps(1.3215487e-06f)));
    e2 = _mm_add_ps(_mm_set1_ps(0.00015403530f), _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.0013333558f),  _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.009618129f),   _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.055504109f),   _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.24022651f),    _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.6931472f),     _mm_mul_ps(yf, e2));
    __m128 exp2f = _mm_add_ps(_mm_set1_ps(1.0f), _mm_mul_ps(yf, e2));

    __m128i scale_i = _mm_slli_epi32(_mm_add_epi32(n, _mm_set1_epi32(127)), 23);
    __m128 scale = _mm_castsi128_ps(scale_i);
    __m128 result = _mm_mul_ps(scale, exp2f);
    return _mm_and_ps(is_pos, result);
#endif
}

/* ----------------------------------------------------------------
 * Float32 ALWAN_POW_F64(x, 1/2.4) -- fast vectorized via exp2((1/2.4) * ALWAN_LOG2_F64(x))
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow_inv24(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm_pow_ps(x, _mm_set1_ps(1.0f / 2.4f));
#else
    __m128 zero = _mm_setzero_ps();
    __m128 v = _mm_max_ps(x, zero);
    __m128 is_pos = _mm_cmpgt_ps(v, zero);

    /* ALWAN_LOG2_F64(v) via IEEE 754 decomposition */
    __m128i iv = _mm_castps_si128(v);
    __m128i exp_i = _mm_sub_epi32(_mm_srli_epi32(iv, 23), _mm_set1_epi32(127));
    __m128 e = _mm_cvtepi32_ps(exp_i);
    __m128i mant_bits = _mm_or_si128(
        _mm_and_si128(iv, _mm_set1_epi32(0x007FFFFF)),
        _mm_set1_epi32(0x3F800000));
    __m128 m = _mm_castsi128_ps(mant_bits);

    /* Minimax polynomial ALWAN_LOG2_F64(m) on [1, 2) */
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1); matches scalar twin */
    __m128 one_l = _mm_set1_ps(1.0f);
    __m128 s  = _mm_div_ps(_mm_sub_ps(m, one_l), _mm_add_ps(m, one_l));
    __m128 s2 = _mm_mul_ps(s, s);
    __m128 lp = _mm_add_ps(_mm_set1_ps(1.0f/11.0f), _mm_mul_ps(s2, _mm_set1_ps(1.0f/13.0f)));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/9.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/7.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/5.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(_mm_set1_ps(1.0f/3.0f), _mm_mul_ps(s2, lp));
    lp = _mm_add_ps(one_l, _mm_mul_ps(s2, lp));
    __m128 log2_m = _mm_mul_ps(_mm_set1_ps(2.8853901f), _mm_mul_ps(s, lp));

    __m128 y = _mm_mul_ps(_mm_set1_ps(0.41666667f), _mm_add_ps(e, log2_m));

    /* exp2(y) */
    __m128 yi = alwan_simd_f32_floor(y);
    __m128 yf = _mm_sub_ps(y, yi);
    __m128i n = _mm_cvttps_epi32(yi);

    /* Minimax polynomial 2^yf on [0, 1) */
    /* exp2(yf) on [0,1): degree-6 Taylor; matches scalar twin */
    __m128 e2 = _mm_add_ps(_mm_set1_ps(1.5252734e-05f),
                _mm_mul_ps(yf, _mm_set1_ps(1.3215487e-06f)));
    e2 = _mm_add_ps(_mm_set1_ps(0.00015403530f), _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.0013333558f),  _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.009618129f),   _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.055504109f),   _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.24022651f),    _mm_mul_ps(yf, e2));
    e2 = _mm_add_ps(_mm_set1_ps(0.6931472f),     _mm_mul_ps(yf, e2));
    __m128 exp2f = _mm_add_ps(_mm_set1_ps(1.0f), _mm_mul_ps(yf, e2));

    __m128i scale_i = _mm_slli_epi32(_mm_add_epi32(n, _mm_set1_epi32(127)), 23);
    __m128 scale = _mm_castsi128_ps(scale_i);
    __m128 result = _mm_mul_ps(scale, exp2f);
    return _mm_and_ps(is_pos, result);
#endif
}

/* ----------------------------------------------------------------
 * Float64 ALWAN_POW_F64(x, 2.4) -- fast log2/exp2 decomposition (SSE2, 2-lane)
 * Valid for normal-range inputs; subnormal inputs may be inaccurate.
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow24(alwan_simd_f64 x) {
#if ALWAN_HAS_SVML
    return _mm_pow_pd(x, _mm_set1_pd(2.4));
#else
    __m128d zero   = _mm_setzero_pd();
    __m128d v      = _mm_max_pd(x, zero);
    __m128d is_pos = _mm_cmpgt_pd(v, zero);

    /* Extract binary exponent: (bits >> 52) - 1023 */
    __m128i iv      = _mm_castpd_si128(v);
    __m128i exp_i64 = _mm_sub_epi64(
        _mm_srli_epi64(iv, 52),
        _mm_set1_epi64x(1023LL));

    /* Convert exp_i64 to f64 via bias trick */
    __m128i const cvt_magic_i = _mm_set1_epi64x(0x4330000000000000LL + 1022LL);
    __m128d const cvt_magic_d = _mm_set1_pd(4503599627371518.0); /* 2^52 + 1022 */
    __m128d e = _mm_sub_pd(
        _mm_castsi128_pd(_mm_add_epi64(exp_i64, cvt_magic_i)),
        cvt_magic_d);

    /* Extract mantissa in [1.0, 2.0) */
    __m128i mant_bits = _mm_or_si128(
        _mm_and_si128(iv, _mm_set1_epi64x(0x000FFFFFFFFFFFFFLL)),
        _mm_set1_epi64x(0x3FF0000000000000LL));
    __m128d m = _mm_castsi128_pd(mant_bits);

    /* ALWAN_LOG2_F64(m) on [1, 2): Horner polynomial, t = m - 1 */
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1) in [0,1/3):
       log2(m) = (2/ln2)*(s + s^3/3 + s^5/5 + s^7/7 + s^9/9). Far more accurate
       near m->2 than a Taylor series in (m-1), which diverges there. */
    __m128d one_l = _mm_set1_pd(1.0);
    __m128d s  = _mm_div_pd(_mm_sub_pd(m, one_l), _mm_add_pd(m, one_l));
    __m128d s2 = _mm_mul_pd(s, s);
    __m128d lp = _mm_add_pd(_mm_set1_pd(1.0/15.0), _mm_mul_pd(s2, _mm_set1_pd(1.0/17.0)));
    lp = _mm_add_pd(_mm_set1_pd(1.0/13.0), _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/11.0), _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/9.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/7.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/5.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/3.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(one_l, _mm_mul_pd(s2, lp));
    __m128d log2_m = _mm_mul_pd(_mm_set1_pd(2.8853900817779268), _mm_mul_pd(s, lp));

    /* y = 2.4 * ALWAN_LOG2_F64(x) */
    __m128d y = _mm_mul_pd(_mm_set1_pd(2.4), _mm_add_pd(e, log2_m));

    /* Split y = yi (integer) + yf (fraction) */
    __m128d yi = alwan_simd_f64_floor(y);
    __m128d yf = _mm_sub_pd(y, yi);

    /* exp2(yf) on [0, 1): Horner polynomial */
    /* exp2(yf) on [0,1): degree-6 Taylor (ln2^k/k!) */
    __m128d e2 = _mm_add_pd(_mm_set1_pd(1.0178086009239696e-07),
                 _mm_mul_pd(yf, _mm_set1_pd(7.0549116208011209e-09)));
    e2 = _mm_add_pd(_mm_set1_pd(1.3215486790144305e-06), _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(1.5252733804059838e-05), _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.00015403530393381606), _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.0013333558146428441),  _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.0096181291076284769),  _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.055504108664821576),   _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.24022650695910069),    _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.69314718055994529),    _mm_mul_pd(yf, e2));
    __m128d exp2f = _mm_add_pd(_mm_set1_pd(1.0),         _mm_mul_pd(yf, e2));

    /* Scale: 2^yi = float with exponent field = (yi + 1023) << 52
     * _mm_cvttpd_epi32 gives 2xi32 in low 64 bits; sign-extend to i64. */
    __m128i yi_i32  = _mm_cvttpd_epi32(yi);
    __m128i yi_sign = _mm_srai_epi32(yi_i32, 31);     /* all-ones if negative */
    __m128i yi_i64  = _mm_unpacklo_epi32(yi_i32, yi_sign); /* 2xi64 sign-extended */
    __m128i scale_i = _mm_slli_epi64(
        _mm_add_epi64(yi_i64, _mm_set1_epi64x(1023LL)), 52);
    __m128d scale   = _mm_castsi128_pd(scale_i);

    __m128d result  = _mm_mul_pd(scale, exp2f);
    return _mm_and_pd(is_pos, result);
#endif
}

/* ----------------------------------------------------------------
 * Float64 ALWAN_POW_F64(x, 1/2.4) -- fast log2/exp2 decomposition (SSE2, 2-lane)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow_inv24(alwan_simd_f64 x) {
#if ALWAN_HAS_SVML
    return _mm_pow_pd(x, _mm_set1_pd(1.0 / 2.4));
#else
    __m128d zero   = _mm_setzero_pd();
    __m128d v      = _mm_max_pd(x, zero);
    __m128d is_pos = _mm_cmpgt_pd(v, zero);
    __m128i iv      = _mm_castpd_si128(v);
    __m128i exp_i64 = _mm_sub_epi64(
        _mm_srli_epi64(iv, 52),
        _mm_set1_epi64x(1023LL));
    __m128i const cvt_magic_i = _mm_set1_epi64x(0x4330000000000000LL + 1022LL);
    __m128d const cvt_magic_d = _mm_set1_pd(4503599627371518.0);
    __m128d e = _mm_sub_pd(
        _mm_castsi128_pd(_mm_add_epi64(exp_i64, cvt_magic_i)),
        cvt_magic_d);
    __m128i mant_bits = _mm_or_si128(
        _mm_and_si128(iv, _mm_set1_epi64x(0x000FFFFFFFFFFFFFLL)),
        _mm_set1_epi64x(0x3FF0000000000000LL));
    __m128d m = _mm_castsi128_pd(mant_bits);
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1) in [0,1/3):
       log2(m) = (2/ln2)*(s + s^3/3 + s^5/5 + s^7/7 + s^9/9). Far more accurate
       near m->2 than a Taylor series in (m-1), which diverges there. */
    __m128d one_l = _mm_set1_pd(1.0);
    __m128d s  = _mm_div_pd(_mm_sub_pd(m, one_l), _mm_add_pd(m, one_l));
    __m128d s2 = _mm_mul_pd(s, s);
    __m128d lp = _mm_add_pd(_mm_set1_pd(1.0/15.0), _mm_mul_pd(s2, _mm_set1_pd(1.0/17.0)));
    lp = _mm_add_pd(_mm_set1_pd(1.0/13.0), _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/11.0), _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/9.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/7.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/5.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(_mm_set1_pd(1.0/3.0),  _mm_mul_pd(s2, lp));
    lp = _mm_add_pd(one_l, _mm_mul_pd(s2, lp));
    __m128d log2_m = _mm_mul_pd(_mm_set1_pd(2.8853900817779268), _mm_mul_pd(s, lp));
    __m128d y = _mm_mul_pd(_mm_set1_pd(1.0 / 2.4), _mm_add_pd(e, log2_m));
    __m128d yi = alwan_simd_f64_floor(y);
    __m128d yf = _mm_sub_pd(y, yi);
    /* exp2(yf) on [0,1): degree-6 Taylor (ln2^k/k!) */
    __m128d e2 = _mm_add_pd(_mm_set1_pd(1.0178086009239696e-07),
                 _mm_mul_pd(yf, _mm_set1_pd(7.0549116208011209e-09)));
    e2 = _mm_add_pd(_mm_set1_pd(1.3215486790144305e-06), _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(1.5252733804059838e-05), _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.00015403530393381606), _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.0013333558146428441),  _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.0096181291076284769),  _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.055504108664821576),   _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.24022650695910069),    _mm_mul_pd(yf, e2));
    e2 = _mm_add_pd(_mm_set1_pd(0.69314718055994529),    _mm_mul_pd(yf, e2));
    __m128d exp2f = _mm_add_pd(_mm_set1_pd(1.0),         _mm_mul_pd(yf, e2));
    __m128i yi_i32  = _mm_cvttpd_epi32(yi);
    __m128i yi_sign = _mm_srai_epi32(yi_i32, 31);
    __m128i yi_i64  = _mm_unpacklo_epi32(yi_i32, yi_sign);
    __m128i scale_i = _mm_slli_epi64(
        _mm_add_epi64(yi_i64, _mm_set1_epi64x(1023LL)), 52);
    __m128d scale   = _mm_castsi128_pd(scale_i);
    __m128d result  = _mm_mul_pd(scale, exp2f);
    return _mm_and_pd(is_pos, result);
#endif
}

/* ----------------------------------------------------------------
 * Float32 cbrt (fast approximation)
 * Integer bit trick + two Newton-Raphson refinements
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt_fast(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm_cbrt_ps(x);
#else
    __m128 zero = _mm_setzero_ps();
    __m128 sign_mask = _mm_set1_ps(-0.0f);
    __m128 sign = _mm_and_ps(x, sign_mask);
    __m128 ax = _mm_andnot_ps(sign_mask, x);
    __m128 is_nonzero = _mm_cmpgt_ps(ax, zero);

    /* Initial estimate via integer bit trick (scalarized for SSE2 portability) */
    ALWAN_ALIGN(16) float av[4], yv[4];
    _mm_store_ps(av, ax);
    for (int k = 0; k < 4; k++) {
        union { float f; int32_t i; } u;
        u.f = av[k];
        u.i = u.i / 3 + 0x2a508f2e;
        yv[k] = u.f;
    }
    __m128 y = _mm_load_ps(yv);

    /* Two Newton-Raphson refinements: y = 2/3 * y + x/(3*y^2) */
    __m128 two_thirds = _mm_set1_ps(0.66666667f);
    __m128 one_third = _mm_set1_ps(0.33333333f);
    __m128 y2, ax_3;
    ax_3 = _mm_mul_ps(ax, one_third);

    y2 = _mm_mul_ps(y, y);
    y = _mm_add_ps(_mm_mul_ps(two_thirds, y), _mm_div_ps(ax_3, y2));

    y2 = _mm_mul_ps(y, y);
    y = _mm_add_ps(_mm_mul_ps(two_thirds, y), _mm_div_ps(ax_3, y2));

    /* Restore sign and zero mask */
    y = _mm_or_ps(y, sign);
    return _mm_and_ps(y, is_nonzero);
#endif
}

/* ----------------------------------------------------------------
 * Float64 cbrt (fast) -- forward to alwan_simd_f64_cbrt
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt_fast(alwan_simd_f64 x) {
    return alwan_simd_f64_cbrt(x);
}

#endif /* ALWAN_SIMD_SSE2_H */
