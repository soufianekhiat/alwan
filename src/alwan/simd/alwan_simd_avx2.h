/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * SIMD AVX2 backend (256-bit float+int, FMA3)
 */

#ifndef ALWAN_SIMD_AVX2_H
#define ALWAN_SIMD_AVX2_H

#include "alwan_simd_types.h"
#include "../alwan_math.h"   /* must come before per-lane ALWAN_POW_F32/etc.
                                 * uses below; alwan_math.h supplies det-mode
                                 * redefinitions when ALWAN_DETERMINISTIC=1. */
#include <math.h>

/* ================================================================
 * Float32 Operations (256-bit, 8 lanes)
 * ================================================================ */

/* ----------------------------------------------------------------
 * Float32 Arithmetic
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_add_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sub(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_sub_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_mul(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_mul_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_div(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_div_ps(a, b); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_neg(alwan_simd_f32 a) {
    return _mm256_xor_ps(a, _mm256_set1_ps(-0.0f));
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_set1(float v) { return _mm256_set1_ps(v); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_zero(void) { return _mm256_setzero_ps(); }

/* ----------------------------------------------------------------
 * Float32 Math
 * SVML: true SIMD vector math when available
 * Fallback: per-lane libm for exact results
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a) { return _mm256_sqrt_ps(a); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_abs(alwan_simd_f32 a) {
    __m256i mask = _mm256_set1_epi32(0x7FFFFFFF);
    return _mm256_and_ps(a, _mm256_castsi256_ps(mask));
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_cbrt_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = ALWAN_CBRT_F32(v[0]); r[1] = ALWAN_CBRT_F32(v[1]); r[2] = ALWAN_CBRT_F32(v[2]); r[3] = ALWAN_CBRT_F32(v[3]);
    r[4] = ALWAN_CBRT_F32(v[4]); r[5] = ALWAN_CBRT_F32(v[5]); r[6] = ALWAN_CBRT_F32(v[6]); r[7] = ALWAN_CBRT_F32(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_exp(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_exp_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = ALWAN_EXP_F32(v[0]); r[1] = ALWAN_EXP_F32(v[1]); r[2] = ALWAN_EXP_F32(v[2]); r[3] = ALWAN_EXP_F32(v[3]);
    r[4] = ALWAN_EXP_F32(v[4]); r[5] = ALWAN_EXP_F32(v[5]); r[6] = ALWAN_EXP_F32(v[6]); r[7] = ALWAN_EXP_F32(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_log_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = ALWAN_LN_F32(v[0]); r[1] = ALWAN_LN_F32(v[1]); r[2] = ALWAN_LN_F32(v[2]); r[3] = ALWAN_LN_F32(v[3]);
    r[4] = ALWAN_LN_F32(v[4]); r[5] = ALWAN_LN_F32(v[5]); r[6] = ALWAN_LN_F32(v[6]); r[7] = ALWAN_LN_F32(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log2(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_log2_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = ALWAN_LOG2_F32(v[0]); r[1] = ALWAN_LOG2_F32(v[1]); r[2] = ALWAN_LOG2_F32(v[2]); r[3] = ALWAN_LOG2_F32(v[3]);
    r[4] = ALWAN_LOG2_F32(v[4]); r[5] = ALWAN_LOG2_F32(v[5]); r[6] = ALWAN_LOG2_F32(v[6]); r[7] = ALWAN_LOG2_F32(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log10(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_log10_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = ALWAN_LOG10_F32(v[0]); r[1] = ALWAN_LOG10_F32(v[1]); r[2] = ALWAN_LOG10_F32(v[2]); r[3] = ALWAN_LOG10_F32(v[3]);
    r[4] = ALWAN_LOG10_F32(v[4]); r[5] = ALWAN_LOG10_F32(v[5]); r[6] = ALWAN_LOG10_F32(v[6]); r[7] = ALWAN_LOG10_F32(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sin(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_sin_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = ALWAN_SIN_F32(v[0]); r[1] = ALWAN_SIN_F32(v[1]); r[2] = ALWAN_SIN_F32(v[2]); r[3] = ALWAN_SIN_F32(v[3]);
    r[4] = ALWAN_SIN_F32(v[4]); r[5] = ALWAN_SIN_F32(v[5]); r[6] = ALWAN_SIN_F32(v[6]); r[7] = ALWAN_SIN_F32(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cos(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_cos_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = ALWAN_COS_F32(v[0]); r[1] = ALWAN_COS_F32(v[1]); r[2] = ALWAN_COS_F32(v[2]); r[3] = ALWAN_COS_F32(v[3]);
    r[4] = ALWAN_COS_F32(v[4]); r[5] = ALWAN_COS_F32(v[5]); r[6] = ALWAN_COS_F32(v[6]); r[7] = ALWAN_COS_F32(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 e) {
#if ALWAN_HAS_SVML
    return _mm256_pow_ps(base, e);
#else
    ALWAN_ALIGN(32) float b[8], ex[8], r[8];
    _mm256_store_ps(b, base);
    _mm256_store_ps(ex, e);
    r[0] = ALWAN_POW_F32(b[0], ex[0]); r[1] = ALWAN_POW_F32(b[1], ex[1]); r[2] = ALWAN_POW_F32(b[2], ex[2]); r[3] = ALWAN_POW_F32(b[3], ex[3]);
    r[4] = ALWAN_POW_F32(b[4], ex[4]); r[5] = ALWAN_POW_F32(b[5], ex[5]); r[6] = ALWAN_POW_F32(b[6], ex[6]); r[7] = ALWAN_POW_F32(b[7], ex[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_atan2(alwan_simd_f32 y, alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm256_atan2_ps(y, x);
#else
    ALWAN_ALIGN(32) float vy[8], vx[8], r[8];
    _mm256_store_ps(vy, y);
    _mm256_store_ps(vx, x);
    r[0] = ALWAN_ATAN2_F32(vy[0], vx[0]); r[1] = ALWAN_ATAN2_F32(vy[1], vx[1]); r[2] = ALWAN_ATAN2_F32(vy[2], vx[2]); r[3] = ALWAN_ATAN2_F32(vy[3], vx[3]);
    r[4] = ALWAN_ATAN2_F32(vy[4], vx[4]); r[5] = ALWAN_ATAN2_F32(vy[5], vx[5]); r[6] = ALWAN_ATAN2_F32(vy[6], vx[6]); r[7] = ALWAN_ATAN2_F32(vy[7], vx[7]);
    return _mm256_load_ps(r);
#endif
}

/* ----------------------------------------------------------------
 * Float32 FMA (AVX2 implies FMA3)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return _mm256_fmadd_ps(a, b, c);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmsub(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return _mm256_fmsub_ps(a, b, c);
}

/* ----------------------------------------------------------------
 * Float32 Comparison -> Mask
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpeq(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_cmp_ps(a, b, _CMP_EQ_OQ); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmplt(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_cmp_ps(a, b, _CMP_LT_OQ); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmple(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_cmp_ps(a, b, _CMP_LE_OQ); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpgt(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_cmp_ps(a, b, _CMP_GT_OQ); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpge(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_cmp_ps(a, b, _CMP_GE_OQ); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m, alwan_simd_f32 a, alwan_simd_f32 b) {
    return _mm256_blendv_ps(b, a, m);
}

/* ----------------------------------------------------------------
 * Float32 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_min(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_min_ps(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_max(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_max_ps(a, b); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_clamp(alwan_simd_f32 v, alwan_simd_f32 lo, alwan_simd_f32 hi) {
    return _mm256_min_ps(_mm256_max_ps(v, lo), hi);
}

/* ----------------------------------------------------------------
 * Float32 Horizontal / Rounding
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd_native(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_hadd_ps(a, b); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum_native(alwan_simd_f32 a) {
    __m128 lo = _mm256_castps256_ps128(a);
    __m128 hi = _mm256_extractf128_ps(a, 1);
    __m128 sum128 = _mm_add_ps(lo, hi);
    __m128 t1 = _mm_shuffle_ps(sum128, sum128, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 t2 = _mm_add_ps(sum128, t1);
    __m128 t3 = _mm_shuffle_ps(t2, t2, _MM_SHUFFLE(0, 1, 2, 3));
    __m128 r = _mm_add_ps(t2, t3);
    return _mm256_insertf128_ps(_mm256_castps128_ps256(r), r, 1);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) { return _mm256_floor_ps(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a) { return _mm256_ceil_ps(a); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a) {
    return _mm256_round_ps(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_trunc(alwan_simd_f32 a) {
    return _mm256_round_ps(a, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_rcp(alwan_simd_f32 a) {
    return _mm256_div_ps(_mm256_set1_ps(1.0f), a);
}

/* ----------------------------------------------------------------
 * Float32 Load / Store
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_load(float const *ptr) { return _mm256_load_ps(ptr); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_loadu(float const *ptr) { return _mm256_loadu_ps(ptr); }
ALWAN_INLINE void alwan_simd_f32_store(float *ptr, alwan_simd_f32 v) { _mm256_store_ps(ptr, v); }
ALWAN_INLINE void alwan_simd_f32_storeu(float *ptr, alwan_simd_f32 v) { _mm256_storeu_ps(ptr, v); }

/* ================================================================
 * Float64 Operations (256-bit, 4 lanes, with FMA3)
 * ================================================================ */

/* ----------------------------------------------------------------
 * Float64 Arithmetic
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_add(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_add_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sub(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_sub_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_mul(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_mul_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_div(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_div_pd(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_neg(alwan_simd_f64 a) {
    return _mm256_xor_pd(a, _mm256_set1_pd(-0.0));
}

/* ----------------------------------------------------------------
 * Float64 Broadcast / Zero
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_set1(double v) { return _mm256_set1_pd(v); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_zero(void) { return _mm256_setzero_pd(); }

/* ----------------------------------------------------------------
 * Float64 Math
 * SVML: true SIMD vector math when available
 * Fallback: per-lane libm for exact results
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sqrt(alwan_simd_f64 a) { return _mm256_sqrt_pd(a); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_abs(alwan_simd_f64 a) {
    return _mm256_andnot_pd(_mm256_set1_pd(-0.0), a);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_cbrt_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = ALWAN_CBRT_F64(v[0]); r[1] = ALWAN_CBRT_F64(v[1]); r[2] = ALWAN_CBRT_F64(v[2]); r[3] = ALWAN_CBRT_F64(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_exp(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_exp_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = ALWAN_EXP_F64(v[0]); r[1] = ALWAN_EXP_F64(v[1]); r[2] = ALWAN_EXP_F64(v[2]); r[3] = ALWAN_EXP_F64(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_log_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = ALWAN_LN_F64(v[0]); r[1] = ALWAN_LN_F64(v[1]); r[2] = ALWAN_LN_F64(v[2]); r[3] = ALWAN_LN_F64(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log2(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_log2_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = ALWAN_LOG2_F64(v[0]); r[1] = ALWAN_LOG2_F64(v[1]); r[2] = ALWAN_LOG2_F64(v[2]); r[3] = ALWAN_LOG2_F64(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log10(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_log10_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = ALWAN_LOG10_F64(v[0]); r[1] = ALWAN_LOG10_F64(v[1]); r[2] = ALWAN_LOG10_F64(v[2]); r[3] = ALWAN_LOG10_F64(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sin(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_sin_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = ALWAN_SIN_F64(v[0]); r[1] = ALWAN_SIN_F64(v[1]); r[2] = ALWAN_SIN_F64(v[2]); r[3] = ALWAN_SIN_F64(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cos(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_cos_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = ALWAN_COS_F64(v[0]); r[1] = ALWAN_COS_F64(v[1]); r[2] = ALWAN_COS_F64(v[2]); r[3] = ALWAN_COS_F64(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow(alwan_simd_f64 base, alwan_simd_f64 e) {
#if ALWAN_HAS_SVML
    return _mm256_pow_pd(base, e);
#else
    ALWAN_ALIGN(32) double b[4], ex[4], r[4];
    _mm256_store_pd(b, base);
    _mm256_store_pd(ex, e);
    r[0] = ALWAN_POW_F64(b[0], ex[0]); r[1] = ALWAN_POW_F64(b[1], ex[1]);
    r[2] = ALWAN_POW_F64(b[2], ex[2]); r[3] = ALWAN_POW_F64(b[3], ex[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_atan2(alwan_simd_f64 y, alwan_simd_f64 x) {
#if ALWAN_HAS_SVML
    return _mm256_atan2_pd(y, x);
#else
    ALWAN_ALIGN(32) double vy[4], vx[4], r[4];
    _mm256_store_pd(vy, y);
    _mm256_store_pd(vx, x);
    r[0] = ALWAN_ATAN2_F64(vy[0], vx[0]); r[1] = ALWAN_ATAN2_F64(vy[1], vx[1]);
    r[2] = ALWAN_ATAN2_F64(vy[2], vx[2]); r[3] = ALWAN_ATAN2_F64(vy[3], vx[3]);
    return _mm256_load_pd(r);
#endif
}

/* ----------------------------------------------------------------
 * Float64 FMA (AVX2 implies FMA3)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmadd(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    return _mm256_fmadd_pd(a, b, c);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmsub(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    return _mm256_fmsub_pd(a, b, c);
}

/* ----------------------------------------------------------------
 * Float64 Comparison -> Mask
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpeq(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_cmp_pd(a, b, _CMP_EQ_OQ); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmplt(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_cmp_pd(a, b, _CMP_LT_OQ); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmple(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_cmp_pd(a, b, _CMP_LE_OQ); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpgt(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_cmp_pd(a, b, _CMP_GT_OQ); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpge(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_cmp_pd(a, b, _CMP_GE_OQ); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_select(alwan_simd_f64_mask m, alwan_simd_f64 a, alwan_simd_f64 b) {
    return _mm256_blendv_pd(b, a, m);
}

ALWAN_INLINE int alwan_simd_f32_mask_all_set(alwan_simd_f32_mask m) { return _mm256_movemask_ps(m) == 0xFF; }
ALWAN_INLINE int alwan_simd_f64_mask_all_set(alwan_simd_f64_mask m) { return _mm256_movemask_pd(m) == 0xF; }

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_and(alwan_simd_f32_mask a, alwan_simd_f32_mask b) { return _mm256_and_ps(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_or (alwan_simd_f32_mask a, alwan_simd_f32_mask b) { return _mm256_or_ps(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_and(alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return _mm256_and_pd(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_or (alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return _mm256_or_pd(a, b); }

/* ----------------------------------------------------------------
 * Float64 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_min_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_max_pd(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    return _mm256_min_pd(_mm256_max_pd(v, lo), hi);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum_native(alwan_simd_f64 a) {
    __m128d lo = _mm256_castpd256_pd128(a);
    __m128d hi = _mm256_extractf128_pd(a, 1);
    __m128d sum128 = _mm_add_pd(lo, hi);
    __m128d t = _mm_shuffle_pd(sum128, sum128, 1);
    __m128d r = _mm_add_pd(sum128, t);
    return _mm256_insertf128_pd(_mm256_castpd128_pd256(r), r, 1);
}

/* ----------------------------------------------------------------
 * Float64 Rounding (AVX native)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_floor(alwan_simd_f64 a) { return _mm256_floor_pd(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_ceil(alwan_simd_f64 a) { return _mm256_ceil_pd(a); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_round(alwan_simd_f64 a) {
    return _mm256_round_pd(a, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_trunc(alwan_simd_f64 a) {
    return _mm256_round_pd(a, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
}

/* ----------------------------------------------------------------
 * Float64 Reciprocal (exact: 1.0/x)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_rcp(alwan_simd_f64 a) {
    return _mm256_div_pd(_mm256_set1_pd(1.0), a);
}

/* ----------------------------------------------------------------
 * Float64 Load / Store
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_load(double const *ptr) { return _mm256_load_pd(ptr); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_loadu(double const *ptr) { return _mm256_loadu_pd(ptr); }
ALWAN_INLINE void alwan_simd_f64_store(double *ptr, alwan_simd_f64 v) { _mm256_store_pd(ptr, v); }
ALWAN_INLINE void alwan_simd_f64_storeu(double *ptr, alwan_simd_f64 v) { _mm256_storeu_pd(ptr, v); }

/* ================================================================
 * AVX2: 256-bit Integer ops
 * ================================================================ */

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_load(uint8_t const *ptr) { return _mm256_load_si256((const __m256i *)ptr); }
ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_loadu(uint8_t const *ptr) { return _mm256_loadu_si256((const __m256i *)ptr); }
ALWAN_INLINE void alwan_simd_u8_store(uint8_t *ptr, alwan_simd_u8 v) { _mm256_store_si256((__m256i *)ptr, v); }
ALWAN_INLINE void alwan_simd_u8_storeu(uint8_t *ptr, alwan_simd_u8 v) { _mm256_storeu_si256((__m256i *)ptr, v); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) {
    __m256i i32 = _mm256_cvtepu8_epi32(_mm256_castsi256_si128(v));
    return _mm256_cvtepi32_ps(i32);
}

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_shuffle(alwan_simd_u8 v, alwan_simd_u8 mask) {
    return _mm256_shuffle_epi8(v, mask); /* AVX2 vpshufb (in-lane) */
}

/* ----------------------------------------------------------------
 * AoS <-> SoA 3-Channel Deinterleave / Interleave
 * ---------------------------------------------------------------- */

/* f32: load 8 RGB pixels (24 floats) -> 3 x __m256 SoA */
ALWAN_FORCE_INLINE void alwan_simd_f32_deinterleave3(float const *src,
                                                 alwan_simd_f32 *ch0, alwan_simd_f32 *ch1, alwan_simd_f32 *ch2) {
    /* Low 4 pixels */
    __m128 lo0 = _mm_loadu_ps(src);
    __m128 lo1 = _mm_loadu_ps(src + 4);
    __m128 lo2 = _mm_loadu_ps(src + 8);

    __m128 t0 = _mm_shuffle_ps(lo0, lo1, _MM_SHUFFLE(1, 0, 2, 1));
    __m128 t1 = _mm_shuffle_ps(lo1, lo2, _MM_SHUFFLE(2, 1, 3, 2));
    __m128 r_lo = _mm_shuffle_ps(lo0, t1, _MM_SHUFFLE(2, 0, 3, 0));
    __m128 g_lo = _mm_shuffle_ps(t0,  t1, _MM_SHUFFLE(3, 1, 2, 0));
    __m128 b_lo = _mm_shuffle_ps(t0,  lo2, _MM_SHUFFLE(3, 0, 3, 1));

    /* High 4 pixels */
    __m128 hi0 = _mm_loadu_ps(src + 12);
    __m128 hi1 = _mm_loadu_ps(src + 16);
    __m128 hi2 = _mm_loadu_ps(src + 20);

    __m128 t2 = _mm_shuffle_ps(hi0, hi1, _MM_SHUFFLE(1, 0, 2, 1));
    __m128 t3 = _mm_shuffle_ps(hi1, hi2, _MM_SHUFFLE(2, 1, 3, 2));
    __m128 r_hi = _mm_shuffle_ps(hi0, t3, _MM_SHUFFLE(2, 0, 3, 0));
    __m128 g_hi = _mm_shuffle_ps(t2,  t3, _MM_SHUFFLE(3, 1, 2, 0));
    __m128 b_hi = _mm_shuffle_ps(t2,  hi2, _MM_SHUFFLE(3, 0, 3, 1));

    *ch0 = _mm256_insertf128_ps(_mm256_castps128_ps256(r_lo), r_hi, 1);
    *ch1 = _mm256_insertf128_ps(_mm256_castps128_ps256(g_lo), g_hi, 1);
    *ch2 = _mm256_insertf128_ps(_mm256_castps128_ps256(b_lo), b_hi, 1);
}

/* f32: store 3 x __m256 SoA -> 8 RGB pixels (24 floats) */
ALWAN_FORCE_INLINE void alwan_simd_f32_interleave3(float *dst,
                                               alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    /* Low 4 pixels */
    __m128 a_lo = _mm256_castps256_ps128(a);
    __m128 b_lo = _mm256_castps256_ps128(b);
    __m128 c_lo = _mm256_castps256_ps128(c);

    __m128 u0 = _mm_unpacklo_ps(a_lo, b_lo);
    __m128 u1 = _mm_unpackhi_ps(a_lo, b_lo);
    __m128 bc_hi_lo = _mm_unpackhi_ps(b_lo, c_lo);

    __m128 tc = _mm_shuffle_ps(c_lo, u0, _MM_SHUFFLE(2, 2, 0, 0));
    _mm_storeu_ps(dst, _mm_shuffle_ps(u0, tc, _MM_SHUFFLE(2, 0, 1, 0)));

    __m128 tc2 = _mm_shuffle_ps(u0, c_lo, _MM_SHUFFLE(1, 1, 3, 3));
    _mm_storeu_ps(dst + 4, _mm_shuffle_ps(tc2, u1, _MM_SHUFFLE(1, 0, 2, 0)));

    __m128 tc3 = _mm_shuffle_ps(bc_hi_lo, u1, _MM_SHUFFLE(2, 2, 1, 1));
    _mm_storeu_ps(dst + 8, _mm_shuffle_ps(tc3, bc_hi_lo, _MM_SHUFFLE(3, 2, 2, 0)));

    /* High 4 pixels */
    __m128 a_hi = _mm256_extractf128_ps(a, 1);
    __m128 b_hi = _mm256_extractf128_ps(b, 1);
    __m128 c_hi = _mm256_extractf128_ps(c, 1);

    __m128 u2 = _mm_unpacklo_ps(a_hi, b_hi);
    __m128 u3 = _mm_unpackhi_ps(a_hi, b_hi);
    __m128 bc_hi_hi = _mm_unpackhi_ps(b_hi, c_hi);

    __m128 tc4 = _mm_shuffle_ps(c_hi, u2, _MM_SHUFFLE(2, 2, 0, 0));
    _mm_storeu_ps(dst + 12, _mm_shuffle_ps(u2, tc4, _MM_SHUFFLE(2, 0, 1, 0)));

    __m128 tc5 = _mm_shuffle_ps(u2, c_hi, _MM_SHUFFLE(1, 1, 3, 3));
    _mm_storeu_ps(dst + 16, _mm_shuffle_ps(tc5, u3, _MM_SHUFFLE(1, 0, 2, 0)));

    __m128 tc6 = _mm_shuffle_ps(bc_hi_hi, u3, _MM_SHUFFLE(2, 2, 1, 1));
    _mm_storeu_ps(dst + 20, _mm_shuffle_ps(tc6, bc_hi_hi, _MM_SHUFFLE(3, 2, 2, 0)));
}

/* f64: delegate to SSE-level operations (same as AVX) */
ALWAN_FORCE_INLINE void alwan_simd_f64_deinterleave3(double const *src,
                                                 alwan_simd_f64 *ch0, alwan_simd_f64 *ch1, alwan_simd_f64 *ch2) {
    __m128d lo0 = _mm_loadu_pd(src);
    __m128d lo1 = _mm_loadu_pd(src + 2);
    __m128d lo2 = _mm_loadu_pd(src + 4);
    __m128d hi0 = _mm_loadu_pd(src + 6);
    __m128d hi1 = _mm_loadu_pd(src + 8);
    __m128d hi2 = _mm_loadu_pd(src + 10);

    __m128d r_lo = _mm_shuffle_pd(lo0, lo1, 0x2);
    __m128d g_lo = _mm_shuffle_pd(lo0, lo2, 0x1);
    __m128d b_lo = _mm_shuffle_pd(lo1, lo2, 0x2);
    __m128d r_hi = _mm_shuffle_pd(hi0, hi1, 0x2);
    __m128d g_hi = _mm_shuffle_pd(hi0, hi2, 0x1);
    __m128d b_hi = _mm_shuffle_pd(hi1, hi2, 0x2);

    *ch0 = _mm256_insertf128_pd(_mm256_castpd128_pd256(r_lo), r_hi, 1);
    *ch1 = _mm256_insertf128_pd(_mm256_castpd128_pd256(g_lo), g_hi, 1);
    *ch2 = _mm256_insertf128_pd(_mm256_castpd128_pd256(b_lo), b_hi, 1);
}

ALWAN_FORCE_INLINE void alwan_simd_f64_interleave3(double *dst,
                                               alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    __m128d a_lo = _mm256_castpd256_pd128(a);
    __m128d b_lo = _mm256_castpd256_pd128(b);
    __m128d c_lo = _mm256_castpd256_pd128(c);
    __m128d a_hi = _mm256_extractf128_pd(a, 1);
    __m128d b_hi = _mm256_extractf128_pd(b, 1);
    __m128d c_hi = _mm256_extractf128_pd(c, 1);

    _mm_storeu_pd(dst,      _mm_shuffle_pd(a_lo, b_lo, 0x0));
    _mm_storeu_pd(dst + 2,  _mm_shuffle_pd(c_lo, a_lo, 0x2));
    _mm_storeu_pd(dst + 4,  _mm_shuffle_pd(b_lo, c_lo, 0x3));
    _mm_storeu_pd(dst + 6,  _mm_shuffle_pd(a_hi, b_hi, 0x0));
    _mm_storeu_pd(dst + 8,  _mm_shuffle_pd(c_hi, a_hi, 0x2));
    _mm_storeu_pd(dst + 10, _mm_shuffle_pd(b_hi, c_hi, 0x3));
}

/* ================================================================
 * Fast approximation functions (f32)
 * Trade precision for throughput -- suitable for perceptual colour ops
 * ================================================================ */

/* ----------------------------------------------------------------
 * Fast ALWAN_POW_F64(x, 2.4) -- SIMD log2/exp2 decomposition
 * ---------------------------------------------------------------- */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow24(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm256_pow_ps(x, _mm256_set1_ps(2.4f));
#else
    __m256 zero = _mm256_setzero_ps();
    __m256 v = _mm256_max_ps(x, zero);
    __m256 is_pos = _mm256_cmp_ps(v, zero, _CMP_GT_OQ);
    __m256i iv = _mm256_castps_si256(v);
    __m256i exp_i = _mm256_sub_epi32(_mm256_srli_epi32(iv, 23), _mm256_set1_epi32(127));
    __m256 e = _mm256_cvtepi32_ps(exp_i);
    __m256i mant_bits = _mm256_or_si256(
        _mm256_and_si256(iv, _mm256_set1_epi32(0x007FFFFF)),
        _mm256_set1_epi32(0x3F800000));
    __m256 m = _mm256_castsi256_ps(mant_bits);
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1); matches scalar twin */
    __m256 one_l = _mm256_set1_ps(1.0f);
    __m256 s  = _mm256_div_ps(_mm256_sub_ps(m, one_l), _mm256_add_ps(m, one_l));
    __m256 s2 = _mm256_mul_ps(s, s);
    __m256 lp = _mm256_add_ps(_mm256_set1_ps(1.0f/11.0f), _mm256_mul_ps(s2, _mm256_set1_ps(1.0f/13.0f)));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/9.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/7.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/5.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/3.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(one_l, _mm256_mul_ps(s2, lp));
    __m256 log2_m = _mm256_mul_ps(_mm256_set1_ps(2.8853901f), _mm256_mul_ps(s, lp));
    __m256 y = _mm256_mul_ps(_mm256_set1_ps(2.4f), _mm256_add_ps(e, log2_m));
    __m256 yi = _mm256_floor_ps(y);
    __m256 yf = _mm256_sub_ps(y, yi);
    __m256i n = _mm256_cvttps_epi32(yi);
    /* exp2(yf) on [0,1): degree-6 Taylor; matches scalar twin */
    __m256 e2 = _mm256_add_ps(_mm256_set1_ps(1.5252734e-05f),
                _mm256_mul_ps(yf, _mm256_set1_ps(1.3215487e-06f)));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.00015403530f), _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.0013333558f),  _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.009618129f),   _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.055504109f),   _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.24022651f),    _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.6931472f),     _mm256_mul_ps(yf, e2));
    __m256 exp2f = _mm256_add_ps(_mm256_set1_ps(1.0f), _mm256_mul_ps(yf, e2));
    __m256i scale_i = _mm256_slli_epi32(_mm256_add_epi32(n, _mm256_set1_epi32(127)), 23);
    __m256 scale = _mm256_castsi256_ps(scale_i);
    __m256 result = _mm256_mul_ps(scale, exp2f);
    return _mm256_and_ps(is_pos, result);
#endif
}

/* ----------------------------------------------------------------
 * Fast ALWAN_POW_F64(x, 1/2.4) -- SIMD log2/exp2 decomposition
 * ---------------------------------------------------------------- */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow_inv24(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm256_pow_ps(x, _mm256_set1_ps(1.0f / 2.4f));
#else
    __m256 zero = _mm256_setzero_ps();
    __m256 v = _mm256_max_ps(x, zero);
    __m256 is_pos = _mm256_cmp_ps(v, zero, _CMP_GT_OQ);
    __m256i iv = _mm256_castps_si256(v);
    __m256i exp_i = _mm256_sub_epi32(_mm256_srli_epi32(iv, 23), _mm256_set1_epi32(127));
    __m256 e = _mm256_cvtepi32_ps(exp_i);
    __m256i mant_bits = _mm256_or_si256(
        _mm256_and_si256(iv, _mm256_set1_epi32(0x007FFFFF)),
        _mm256_set1_epi32(0x3F800000));
    __m256 m = _mm256_castsi256_ps(mant_bits);
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1); matches scalar twin */
    __m256 one_l = _mm256_set1_ps(1.0f);
    __m256 s  = _mm256_div_ps(_mm256_sub_ps(m, one_l), _mm256_add_ps(m, one_l));
    __m256 s2 = _mm256_mul_ps(s, s);
    __m256 lp = _mm256_add_ps(_mm256_set1_ps(1.0f/11.0f), _mm256_mul_ps(s2, _mm256_set1_ps(1.0f/13.0f)));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/9.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/7.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/5.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(_mm256_set1_ps(1.0f/3.0f), _mm256_mul_ps(s2, lp));
    lp = _mm256_add_ps(one_l, _mm256_mul_ps(s2, lp));
    __m256 log2_m = _mm256_mul_ps(_mm256_set1_ps(2.8853901f), _mm256_mul_ps(s, lp));
    __m256 y = _mm256_mul_ps(_mm256_set1_ps(1.0f / 2.4f), _mm256_add_ps(e, log2_m));
    __m256 yi = _mm256_floor_ps(y);
    __m256 yf = _mm256_sub_ps(y, yi);
    __m256i n = _mm256_cvttps_epi32(yi);
    /* exp2(yf) on [0,1): degree-6 Taylor; matches scalar twin */
    __m256 e2 = _mm256_add_ps(_mm256_set1_ps(1.5252734e-05f),
                _mm256_mul_ps(yf, _mm256_set1_ps(1.3215487e-06f)));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.00015403530f), _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.0013333558f),  _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.009618129f),   _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.055504109f),   _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.24022651f),    _mm256_mul_ps(yf, e2));
    e2 = _mm256_add_ps(_mm256_set1_ps(0.6931472f),     _mm256_mul_ps(yf, e2));
    __m256 exp2f = _mm256_add_ps(_mm256_set1_ps(1.0f), _mm256_mul_ps(yf, e2));
    __m256i scale_i = _mm256_slli_epi32(_mm256_add_epi32(n, _mm256_set1_epi32(127)), 23);
    __m256 scale = _mm256_castsi256_ps(scale_i);
    __m256 result = _mm256_mul_ps(scale, exp2f);
    return _mm256_and_ps(is_pos, result);
#endif
}

/* ----------------------------------------------------------------
 * Fast ALWAN_CBRT_F64(x) -- bit-trick initial estimate + Newton-Raphson
 * ---------------------------------------------------------------- */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt_fast(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm256_cbrt_ps(x);
#else
    __m256 zero = _mm256_setzero_ps();
    __m256 sign_mask = _mm256_set1_ps(-0.0f);
    __m256 sign = _mm256_and_ps(x, sign_mask);
    __m256 ax = _mm256_andnot_ps(sign_mask, x);
    __m256 is_nonzero = _mm256_cmp_ps(ax, zero, _CMP_GT_OQ);

    /* Initial estimate: scalarize just the integer divide for bit trick */
    ALWAN_ALIGN(32) float av[8], yv[8];
    _mm256_store_ps(av, ax);
    for (int k = 0; k < 8; k++) {
        union { float f; int32_t i; } u;
        u.f = av[k];
        u.i = u.i / 3 + 0x2a508f2e;
        yv[k] = u.f;
    }
    __m256 y = _mm256_load_ps(yv);

    /* Two Newton-Raphson refinements in SIMD */
    __m256 two_thirds = _mm256_set1_ps(0.66666667f);
    __m256 ax_3 = _mm256_mul_ps(ax, _mm256_set1_ps(0.33333333f));
    __m256 y2;

    y2 = _mm256_mul_ps(y, y);
    y = _mm256_add_ps(_mm256_mul_ps(two_thirds, y), _mm256_div_ps(ax_3, y2));
    y2 = _mm256_mul_ps(y, y);
    y = _mm256_add_ps(_mm256_mul_ps(two_thirds, y), _mm256_div_ps(ax_3, y2));

    y = _mm256_or_ps(y, sign);
    return _mm256_and_ps(y, is_nonzero);
#endif
}

/* ================================================================
 * Fast approximation functions (f64) -- log2/exp2 decomposition
 * Same structure as f32 variants but with 64-bit integer ops (AVX2).
 * Valid for normal-range inputs; subnormal inputs may be inaccurate.
 * ================================================================ */

/* ----------------------------------------------------------------
 * Fast ALWAN_POW_F64(x, 2.4) -- SIMD log2/exp2 decomposition (f64, AVX2)
 * ---------------------------------------------------------------- */
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow24(alwan_simd_f64 x) {
#if ALWAN_HAS_SVML
    return _mm256_pow_pd(x, _mm256_set1_pd(2.4));
#else
    __m256d zero   = _mm256_setzero_pd();
    __m256d v      = _mm256_max_pd(x, zero);
    __m256d is_pos = _mm256_cmp_pd(v, zero, _CMP_GT_OQ);

    /* Extract binary exponent: (bits >> 52) - 1023 */
    __m256i iv      = _mm256_castpd_si256(v);
    __m256i exp_i64 = _mm256_sub_epi64(
        _mm256_srli_epi64(iv, 52),
        _mm256_set1_epi64x(1023LL));

    /* Convert exp_i64 (-1022..1023) to f64 via bias trick:
     * float_from_bits(magic_i + (e + 1022)) - (2^52 + 1022) = e */
    __m256i const cvt_magic_i = _mm256_set1_epi64x(0x4330000000000000LL + 1022LL);
    __m256d const cvt_magic_d = _mm256_set1_pd(4503599627371518.0); /* 2^52 + 1022 */
    __m256d e = _mm256_sub_pd(
        _mm256_castsi256_pd(_mm256_add_epi64(exp_i64, cvt_magic_i)),
        cvt_magic_d);

    /* Extract mantissa in [1.0, 2.0) */
    __m256i mant_bits = _mm256_or_si256(
        _mm256_and_si256(iv, _mm256_set1_epi64x(0x000FFFFFFFFFFFFFLL)),
        _mm256_set1_epi64x(0x3FF0000000000000LL));
    __m256d m = _mm256_castsi256_pd(mant_bits);

    /* ALWAN_LOG2_F64(m) on [1, 2): Horner polynomial, t = m - 1 */
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1) in [0,1/3):
       log2(m) = (2/ln2)*(s + s^3/3 + s^5/5 + s^7/7 + s^9/9). Far more accurate
       near m->2 than a Taylor series in (m-1), which diverges there. */
    __m256d one_l = _mm256_set1_pd(1.0);
    __m256d s  = _mm256_div_pd(_mm256_sub_pd(m, one_l), _mm256_add_pd(m, one_l));
    __m256d s2 = _mm256_mul_pd(s, s);
    __m256d lp = _mm256_add_pd(_mm256_set1_pd(1.0/15.0), _mm256_mul_pd(s2, _mm256_set1_pd(1.0/17.0)));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/13.0), _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/11.0), _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/9.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/7.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/5.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/3.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(one_l, _mm256_mul_pd(s2, lp));
    __m256d log2_m = _mm256_mul_pd(_mm256_set1_pd(2.8853900817779268), _mm256_mul_pd(s, lp));

    /* y = 2.4 * ALWAN_LOG2_F64(x) */
    __m256d y = _mm256_mul_pd(_mm256_set1_pd(2.4), _mm256_add_pd(e, log2_m));

    /* Split y = yi (integer part) + yf (fractional part) */
    __m256d yi = _mm256_floor_pd(y);
    __m256d yf = _mm256_sub_pd(y, yi);

    /* exp2(yf) on [0, 1): Horner polynomial */
    /* exp2(yf) on [0,1): degree-10 Taylor (ln2^k/k!) */
    __m256d e2 = _mm256_add_pd(_mm256_set1_pd(1.0178086009239696e-07),
                 _mm256_mul_pd(yf, _mm256_set1_pd(7.0549116208011209e-09)));
    e2 = _mm256_add_pd(_mm256_set1_pd(1.3215486790144305e-06), _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(1.5252733804059838e-05), _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.00015403530393381606), _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.0013333558146428441),  _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.0096181291076284769),  _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.055504108664821576),   _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.24022650695910069),    _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.69314718055994529),    _mm256_mul_pd(yf, e2));
    __m256d exp2f = _mm256_add_pd(_mm256_set1_pd(1.0),         _mm256_mul_pd(yf, e2));

    /* Scale: 2^yi = float with exponent field = (yi + 1023) << 52 */
    __m128i yi_i32  = _mm256_cvttpd_epi32(yi);           /* 4Ã—i32 in __m128i */
    __m256i yi_i64  = _mm256_cvtepi32_epi64(yi_i32);     /* sign-extend to 4Ã—i64 (AVX2) */
    __m256i scale_i = _mm256_slli_epi64(
        _mm256_add_epi64(yi_i64, _mm256_set1_epi64x(1023LL)), 52);
    __m256d scale   = _mm256_castsi256_pd(scale_i);

    __m256d result  = _mm256_mul_pd(scale, exp2f);
    return _mm256_and_pd(is_pos, result);
#endif
}

/* ----------------------------------------------------------------
 * Fast ALWAN_POW_F64(x, 1/2.4) -- SIMD log2/exp2 decomposition (f64, AVX2)
 * ---------------------------------------------------------------- */
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow_inv24(alwan_simd_f64 x) {
#if ALWAN_HAS_SVML
    return _mm256_pow_pd(x, _mm256_set1_pd(1.0 / 2.4));
#else
    __m256d zero   = _mm256_setzero_pd();
    __m256d v      = _mm256_max_pd(x, zero);
    __m256d is_pos = _mm256_cmp_pd(v, zero, _CMP_GT_OQ);
    __m256i iv      = _mm256_castpd_si256(v);
    __m256i exp_i64 = _mm256_sub_epi64(
        _mm256_srli_epi64(iv, 52),
        _mm256_set1_epi64x(1023LL));
    __m256i const cvt_magic_i = _mm256_set1_epi64x(0x4330000000000000LL + 1022LL);
    __m256d const cvt_magic_d = _mm256_set1_pd(4503599627371518.0);
    __m256d e = _mm256_sub_pd(
        _mm256_castsi256_pd(_mm256_add_epi64(exp_i64, cvt_magic_i)),
        cvt_magic_d);
    __m256i mant_bits = _mm256_or_si256(
        _mm256_and_si256(iv, _mm256_set1_epi64x(0x000FFFFFFFFFFFFFLL)),
        _mm256_set1_epi64x(0x3FF0000000000000LL));
    __m256d m = _mm256_castsi256_pd(mant_bits);
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1) in [0,1/3):
       log2(m) = (2/ln2)*(s + s^3/3 + s^5/5 + s^7/7 + s^9/9). Far more accurate
       near m->2 than a Taylor series in (m-1), which diverges there. */
    __m256d one_l = _mm256_set1_pd(1.0);
    __m256d s  = _mm256_div_pd(_mm256_sub_pd(m, one_l), _mm256_add_pd(m, one_l));
    __m256d s2 = _mm256_mul_pd(s, s);
    __m256d lp = _mm256_add_pd(_mm256_set1_pd(1.0/15.0), _mm256_mul_pd(s2, _mm256_set1_pd(1.0/17.0)));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/13.0), _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/11.0), _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/9.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/7.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/5.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(_mm256_set1_pd(1.0/3.0),  _mm256_mul_pd(s2, lp));
    lp = _mm256_add_pd(one_l, _mm256_mul_pd(s2, lp));
    __m256d log2_m = _mm256_mul_pd(_mm256_set1_pd(2.8853900817779268), _mm256_mul_pd(s, lp));
    __m256d y = _mm256_mul_pd(_mm256_set1_pd(1.0 / 2.4), _mm256_add_pd(e, log2_m));
    __m256d yi = _mm256_floor_pd(y);
    __m256d yf = _mm256_sub_pd(y, yi);
    /* exp2(yf) on [0,1): degree-10 Taylor (ln2^k/k!) */
    __m256d e2 = _mm256_add_pd(_mm256_set1_pd(1.0178086009239696e-07),
                 _mm256_mul_pd(yf, _mm256_set1_pd(7.0549116208011209e-09)));
    e2 = _mm256_add_pd(_mm256_set1_pd(1.3215486790144305e-06), _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(1.5252733804059838e-05), _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.00015403530393381606), _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.0013333558146428441),  _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.0096181291076284769),  _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.055504108664821576),   _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.24022650695910069),    _mm256_mul_pd(yf, e2));
    e2 = _mm256_add_pd(_mm256_set1_pd(0.69314718055994529),    _mm256_mul_pd(yf, e2));
    __m256d exp2f = _mm256_add_pd(_mm256_set1_pd(1.0),         _mm256_mul_pd(yf, e2));
    __m128i yi_i32  = _mm256_cvttpd_epi32(yi);
    __m256i yi_i64  = _mm256_cvtepi32_epi64(yi_i32);
    __m256i scale_i = _mm256_slli_epi64(
        _mm256_add_epi64(yi_i64, _mm256_set1_epi64x(1023LL)), 52);
    __m256d scale   = _mm256_castsi256_pd(scale_i);
    __m256d result  = _mm256_mul_pd(scale, exp2f);
    return _mm256_and_pd(is_pos, result);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt_fast(alwan_simd_f64 x) {
    return alwan_simd_f64_cbrt(x);
}

#endif /* ALWAN_SIMD_AVX2_H */
