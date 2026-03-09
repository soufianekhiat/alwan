/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * SIMD AVX backend (256-bit float, 128-bit int, no FMA unless __FMA__ detected)
 * AVX implies SSE4.2 and below.
 */

#ifndef ALWAN_SIMD_AVX_H
#define ALWAN_SIMD_AVX_H

#include "alwan_simd_types.h"
#include "../alwan_platform.h"
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
    r[0] = cbrtf(v[0]); r[1] = cbrtf(v[1]); r[2] = cbrtf(v[2]); r[3] = cbrtf(v[3]);
    r[4] = cbrtf(v[4]); r[5] = cbrtf(v[5]); r[6] = cbrtf(v[6]); r[7] = cbrtf(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_exp(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_exp_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = expf(v[0]); r[1] = expf(v[1]); r[2] = expf(v[2]); r[3] = expf(v[3]);
    r[4] = expf(v[4]); r[5] = expf(v[5]); r[6] = expf(v[6]); r[7] = expf(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_log_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = logf(v[0]); r[1] = logf(v[1]); r[2] = logf(v[2]); r[3] = logf(v[3]);
    r[4] = logf(v[4]); r[5] = logf(v[5]); r[6] = logf(v[6]); r[7] = logf(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log2(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_log2_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = log2f(v[0]); r[1] = log2f(v[1]); r[2] = log2f(v[2]); r[3] = log2f(v[3]);
    r[4] = log2f(v[4]); r[5] = log2f(v[5]); r[6] = log2f(v[6]); r[7] = log2f(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log10(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_log10_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = log10f(v[0]); r[1] = log10f(v[1]); r[2] = log10f(v[2]); r[3] = log10f(v[3]);
    r[4] = log10f(v[4]); r[5] = log10f(v[5]); r[6] = log10f(v[6]); r[7] = log10f(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sin(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_sin_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = sinf(v[0]); r[1] = sinf(v[1]); r[2] = sinf(v[2]); r[3] = sinf(v[3]);
    r[4] = sinf(v[4]); r[5] = sinf(v[5]); r[6] = sinf(v[6]); r[7] = sinf(v[7]);
    return _mm256_load_ps(r);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cos(alwan_simd_f32 a) {
#if ALWAN_HAS_SVML
    return _mm256_cos_ps(a);
#else
    ALWAN_ALIGN(32) float v[8], r[8];
    _mm256_store_ps(v, a);
    r[0] = cosf(v[0]); r[1] = cosf(v[1]); r[2] = cosf(v[2]); r[3] = cosf(v[3]);
    r[4] = cosf(v[4]); r[5] = cosf(v[5]); r[6] = cosf(v[6]); r[7] = cosf(v[7]);
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
    r[0] = powf(b[0], ex[0]); r[1] = powf(b[1], ex[1]); r[2] = powf(b[2], ex[2]); r[3] = powf(b[3], ex[3]);
    r[4] = powf(b[4], ex[4]); r[5] = powf(b[5], ex[5]); r[6] = powf(b[6], ex[6]); r[7] = powf(b[7], ex[7]);
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
    r[0] = atan2f(vy[0], vx[0]); r[1] = atan2f(vy[1], vx[1]); r[2] = atan2f(vy[2], vx[2]); r[3] = atan2f(vy[3], vx[3]);
    r[4] = atan2f(vy[4], vx[4]); r[5] = atan2f(vy[5], vx[5]); r[6] = atan2f(vy[6], vx[6]); r[7] = atan2f(vy[7], vx[7]);
    return _mm256_load_ps(r);
#endif
}

/* ----------------------------------------------------------------
 * Float32 FMA
 * FMA3 may be available without AVX2 (e.g. AMD Piledriver: -mavx -mfma)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
#if defined(__FMA__)
    return _mm256_fmadd_ps(a, b, c);
#else
    return _mm256_add_ps(_mm256_mul_ps(a, b), c);
#endif
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmsub(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
#if defined(__FMA__)
    return _mm256_fmsub_ps(a, b, c);
#else
    return _mm256_sub_ps(_mm256_mul_ps(a, b), c);
#endif
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

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_hadd_ps(a, b); }

/* Full horizontal sum: reduce 8 floats to a single broadcast value.
 * vhaddps operates in-lane, so we need cross-lane reduction. */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum(alwan_simd_f32 a) {
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
 * Float64 Operations (256-bit, 4 lanes)
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
    r[0] = cbrt(v[0]); r[1] = cbrt(v[1]); r[2] = cbrt(v[2]); r[3] = cbrt(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_exp(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_exp_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = exp(v[0]); r[1] = exp(v[1]); r[2] = exp(v[2]); r[3] = exp(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_log_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = log(v[0]); r[1] = log(v[1]); r[2] = log(v[2]); r[3] = log(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log2(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_log2_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = log2(v[0]); r[1] = log2(v[1]); r[2] = log2(v[2]); r[3] = log2(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log10(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_log10_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = log10(v[0]); r[1] = log10(v[1]); r[2] = log10(v[2]); r[3] = log10(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sin(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_sin_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = sin(v[0]); r[1] = sin(v[1]); r[2] = sin(v[2]); r[3] = sin(v[3]);
    return _mm256_load_pd(r);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cos(alwan_simd_f64 a) {
#if ALWAN_HAS_SVML
    return _mm256_cos_pd(a);
#else
    ALWAN_ALIGN(32) double v[4], r[4];
    _mm256_store_pd(v, a);
    r[0] = cos(v[0]); r[1] = cos(v[1]); r[2] = cos(v[2]); r[3] = cos(v[3]);
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
    r[0] = pow(b[0], ex[0]); r[1] = pow(b[1], ex[1]);
    r[2] = pow(b[2], ex[2]); r[3] = pow(b[3], ex[3]);
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
    r[0] = atan2(vy[0], vx[0]); r[1] = atan2(vy[1], vx[1]);
    r[2] = atan2(vy[2], vx[2]); r[3] = atan2(vy[3], vx[3]);
    return _mm256_load_pd(r);
#endif
}

/* ----------------------------------------------------------------
 * Float64 FMA
 * FMA3 may be available without AVX2 (e.g. AMD Piledriver: -mavx -mfma)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmadd(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
#if defined(__FMA__)
    return _mm256_fmadd_pd(a, b, c);
#else
    return _mm256_add_pd(_mm256_mul_pd(a, b), c);
#endif
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmsub(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
#if defined(__FMA__)
    return _mm256_fmsub_pd(a, b, c);
#else
    return _mm256_sub_pd(_mm256_mul_pd(a, b), c);
#endif
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

/* ----------------------------------------------------------------
 * Float64 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_min_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_max_pd(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    return _mm256_min_pd(_mm256_max_pd(v, lo), hi);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum(alwan_simd_f64 a) {
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
 * Integer ops remain 128-bit (AVX has no 256-bit integer)
 * ================================================================ */

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_load(uint8_t const *ptr) { return _mm_load_si128((const __m128i *)ptr); }
ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_loadu(uint8_t const *ptr) { return _mm_loadu_si128((const __m128i *)ptr); }
ALWAN_INLINE void alwan_simd_u8_store(uint8_t *ptr, alwan_simd_u8 v) { _mm_store_si128((__m128i *)ptr, v); }
ALWAN_INLINE void alwan_simd_u8_storeu(uint8_t *ptr, alwan_simd_u8 v) { _mm_storeu_si128((__m128i *)ptr, v); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) {
    /* AVX implies SSE4.1, so pmovzxbd is available.
     * Need to widen 8 bytes -> 8 floats via two 128-bit ops + combine. */
    __m128i lo4 = _mm_cvtepu8_epi32(v);
    __m128i hi4 = _mm_cvtepu8_epi32(_mm_srli_si128(v, 4));
    return _mm256_set_m128(_mm_cvtepi32_ps(hi4), _mm_cvtepi32_ps(lo4));
}

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_shuffle(alwan_simd_u8 v, alwan_simd_u8 mask) {
    return _mm_shuffle_epi8(v, mask); /* SSSE3 implied by AVX */
}

/* ----------------------------------------------------------------
 * AoS <-> SoA 3-Channel Deinterleave / Interleave
 * ---------------------------------------------------------------- */

/* f32: load 8 RGB pixels (24 floats) -> 3 x __m256 SoA
 * Split into 2 x SSE deinterleave on 128-bit halves, then combine. */
ALWAN_INLINE void alwan_simd_f32_deinterleave3(float const *src,
                                                 alwan_simd_f32 *ch0, alwan_simd_f32 *ch1, alwan_simd_f32 *ch2) {
    /* Low 4 pixels (12 floats) */
    __m128 lo0 = _mm_loadu_ps(src);
    __m128 lo1 = _mm_loadu_ps(src + 4);
    __m128 lo2 = _mm_loadu_ps(src + 8);

    __m128 t0 = _mm_shuffle_ps(lo0, lo1, _MM_SHUFFLE(1, 0, 2, 1));
    __m128 t1 = _mm_shuffle_ps(lo1, lo2, _MM_SHUFFLE(2, 1, 3, 2));
    __m128 r_lo = _mm_shuffle_ps(lo0, t1, _MM_SHUFFLE(2, 0, 3, 0));
    __m128 g_lo = _mm_shuffle_ps(t0,  t1, _MM_SHUFFLE(3, 1, 2, 0));
    __m128 b_lo = _mm_shuffle_ps(t0,  lo2, _MM_SHUFFLE(3, 0, 3, 1));

    /* High 4 pixels (12 floats) */
    __m128 hi0 = _mm_loadu_ps(src + 12);
    __m128 hi1 = _mm_loadu_ps(src + 16);
    __m128 hi2 = _mm_loadu_ps(src + 20);

    __m128 t2 = _mm_shuffle_ps(hi0, hi1, _MM_SHUFFLE(1, 0, 2, 1));
    __m128 t3 = _mm_shuffle_ps(hi1, hi2, _MM_SHUFFLE(2, 1, 3, 2));
    __m128 r_hi = _mm_shuffle_ps(hi0, t3, _MM_SHUFFLE(2, 0, 3, 0));
    __m128 g_hi = _mm_shuffle_ps(t2,  t3, _MM_SHUFFLE(3, 1, 2, 0));
    __m128 b_hi = _mm_shuffle_ps(t2,  hi2, _MM_SHUFFLE(3, 0, 3, 1));

    /* Combine 128-bit halves into 256-bit */
    *ch0 = _mm256_insertf128_ps(_mm256_castps128_ps256(r_lo), r_hi, 1);
    *ch1 = _mm256_insertf128_ps(_mm256_castps128_ps256(g_lo), g_hi, 1);
    *ch2 = _mm256_insertf128_ps(_mm256_castps128_ps256(b_lo), b_hi, 1);
}

/* f32: store 3 x __m256 SoA -> 8 RGB pixels (24 floats) */
ALWAN_INLINE void alwan_simd_f32_interleave3(float *dst,
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

/* f64: AVX has 4-wide f64. Load 4 RGB pixels (12 doubles) -> 3 x __m256d SoA.
 * Split into 2 x SSE f64 deinterleave, combine. */
ALWAN_INLINE void alwan_simd_f64_deinterleave3(double const *src,
                                                 alwan_simd_f64 *ch0, alwan_simd_f64 *ch1, alwan_simd_f64 *ch2) {
    __m128d lo0 = _mm_loadu_pd(src);      /* a0 b0 */
    __m128d lo1 = _mm_loadu_pd(src + 2);  /* c0 a1 */
    __m128d lo2 = _mm_loadu_pd(src + 4);  /* b1 c1 */
    __m128d hi0 = _mm_loadu_pd(src + 6);  /* a2 b2 */
    __m128d hi1 = _mm_loadu_pd(src + 8);  /* c2 a3 */
    __m128d hi2 = _mm_loadu_pd(src + 10); /* b3 c3 */

    __m128d r_lo = _mm_shuffle_pd(lo0, lo1, 0x2); /* a0 a1 */
    __m128d g_lo = _mm_shuffle_pd(lo0, lo2, 0x1); /* b0 b1 */
    __m128d b_lo = _mm_shuffle_pd(lo1, lo2, 0x2); /* c0 c1 */
    __m128d r_hi = _mm_shuffle_pd(hi0, hi1, 0x2); /* a2 a3 */
    __m128d g_hi = _mm_shuffle_pd(hi0, hi2, 0x1); /* b2 b3 */
    __m128d b_hi = _mm_shuffle_pd(hi1, hi2, 0x2); /* c2 c3 */

    *ch0 = _mm256_insertf128_pd(_mm256_castpd128_pd256(r_lo), r_hi, 1);
    *ch1 = _mm256_insertf128_pd(_mm256_castpd128_pd256(g_lo), g_hi, 1);
    *ch2 = _mm256_insertf128_pd(_mm256_castpd128_pd256(b_lo), b_hi, 1);
}

/* f64: store 3 x __m256d SoA -> 4 RGB pixels (12 doubles) */
ALWAN_INLINE void alwan_simd_f64_interleave3(double *dst,
                                               alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    __m128d a_lo = _mm256_castpd256_pd128(a);
    __m128d b_lo = _mm256_castpd256_pd128(b);
    __m128d c_lo = _mm256_castpd256_pd128(c);
    __m128d a_hi = _mm256_extractf128_pd(a, 1);
    __m128d b_hi = _mm256_extractf128_pd(b, 1);
    __m128d c_hi = _mm256_extractf128_pd(c, 1);

    _mm_storeu_pd(dst,      _mm_shuffle_pd(a_lo, b_lo, 0x0)); /* a0 b0 */
    _mm_storeu_pd(dst + 2,  _mm_shuffle_pd(c_lo, a_lo, 0x2)); /* c0 a1 */
    _mm_storeu_pd(dst + 4,  _mm_shuffle_pd(b_lo, c_lo, 0x3)); /* b1 c1 */
    _mm_storeu_pd(dst + 6,  _mm_shuffle_pd(a_hi, b_hi, 0x0)); /* a2 b2 */
    _mm_storeu_pd(dst + 8,  _mm_shuffle_pd(c_hi, a_hi, 0x2)); /* c2 a3 */
    _mm_storeu_pd(dst + 10, _mm_shuffle_pd(b_hi, c_hi, 0x3)); /* b3 c3 */
}

/* ================================================================
 * Fast approximation functions (f32)
 * Trade precision for throughput -- suitable for perceptual colour ops
 * AVX has no 256-bit integer: split into 128-bit halves for bit ops
 * ================================================================ */

/* ----------------------------------------------------------------
 * Fast pow(x, 2.4) -- SIMD log2/exp2 decomposition (split-128 path)
 * ---------------------------------------------------------------- */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow24(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm256_pow_ps(x, _mm256_set1_ps(2.4f));
#else
    /* Split into 128-bit halves for integer bit manipulation */
    __m128 lo = _mm256_castps256_ps128(x);
    __m128 hi = _mm256_extractf128_ps(x, 1);

    /* Process each half (SSE2 integer ops on 128-bit) */
    __m128 zero128 = _mm_setzero_ps();

    /* --- Process lo half --- */
    __m128 v_lo = _mm_max_ps(lo, zero128);
    __m128 pos_lo = _mm_cmpgt_ps(v_lo, zero128);
    __m128i iv_lo = _mm_castps_si128(v_lo);
    __m128i exp_lo = _mm_sub_epi32(_mm_srli_epi32(iv_lo, 23), _mm_set1_epi32(127));
    __m128 e_lo = _mm_cvtepi32_ps(exp_lo);
    __m128i mant_lo = _mm_or_si128(_mm_and_si128(iv_lo, _mm_set1_epi32(0x007FFFFF)), _mm_set1_epi32(0x3F800000));
    __m128 m_lo = _mm_castsi128_ps(mant_lo);
    __m128 t_lo = _mm_sub_ps(m_lo, _mm_set1_ps(1.0f));
    __m128 log2_lo = _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(1.44269504f),
                     _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(-0.72134752f),
                     _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(0.48089835f),
                     _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(-0.36067376f),
                     _mm_mul_ps(t_lo, _mm_set1_ps(0.28854314f))))))))));
    __m128 y_lo = _mm_mul_ps(_mm_set1_ps(2.4f), _mm_add_ps(e_lo, log2_lo));
    __m128 yi_lo = _mm_floor_ps(y_lo);
    __m128 yf_lo = _mm_sub_ps(y_lo, yi_lo);
    __m128i n_lo = _mm_cvttps_epi32(yi_lo);
    __m128 exp2_lo = _mm_add_ps(_mm_set1_ps(1.0f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.69314718f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.24022651f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.05550411f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.00961813f),
                     _mm_mul_ps(yf_lo, _mm_set1_ps(0.00133335f)))))))))));
    __m128i sc_lo = _mm_slli_epi32(_mm_add_epi32(n_lo, _mm_set1_epi32(127)), 23);
    __m128 res_lo = _mm_and_ps(pos_lo, _mm_mul_ps(_mm_castsi128_ps(sc_lo), exp2_lo));

    /* --- Process hi half --- */
    __m128 v_hi = _mm_max_ps(hi, zero128);
    __m128 pos_hi = _mm_cmpgt_ps(v_hi, zero128);
    __m128i iv_hi = _mm_castps_si128(v_hi);
    __m128i exp_hi = _mm_sub_epi32(_mm_srli_epi32(iv_hi, 23), _mm_set1_epi32(127));
    __m128 e_hi = _mm_cvtepi32_ps(exp_hi);
    __m128i mant_hi = _mm_or_si128(_mm_and_si128(iv_hi, _mm_set1_epi32(0x007FFFFF)), _mm_set1_epi32(0x3F800000));
    __m128 m_hi = _mm_castsi128_ps(mant_hi);
    __m128 t_hi = _mm_sub_ps(m_hi, _mm_set1_ps(1.0f));
    __m128 log2_hi = _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(1.44269504f),
                     _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(-0.72134752f),
                     _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(0.48089835f),
                     _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(-0.36067376f),
                     _mm_mul_ps(t_hi, _mm_set1_ps(0.28854314f))))))))));
    __m128 y_hi = _mm_mul_ps(_mm_set1_ps(2.4f), _mm_add_ps(e_hi, log2_hi));
    __m128 yi_hi = _mm_floor_ps(y_hi);
    __m128 yf_hi = _mm_sub_ps(y_hi, yi_hi);
    __m128i n_hi = _mm_cvttps_epi32(yi_hi);
    __m128 exp2_hi = _mm_add_ps(_mm_set1_ps(1.0f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.69314718f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.24022651f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.05550411f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.00961813f),
                     _mm_mul_ps(yf_hi, _mm_set1_ps(0.00133335f)))))))))));
    __m128i sc_hi = _mm_slli_epi32(_mm_add_epi32(n_hi, _mm_set1_epi32(127)), 23);
    __m128 res_hi = _mm_and_ps(pos_hi, _mm_mul_ps(_mm_castsi128_ps(sc_hi), exp2_hi));

    /* Combine halves */
    return _mm256_set_m128(res_hi, res_lo);
#endif
}

/* ----------------------------------------------------------------
 * Fast pow(x, 1/2.4) -- SIMD log2/exp2 decomposition (split-128 path)
 * ---------------------------------------------------------------- */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow_inv24(alwan_simd_f32 x) {
#if ALWAN_HAS_SVML
    return _mm256_pow_ps(x, _mm256_set1_ps(1.0f / 2.4f));
#else
    /* Split into 128-bit halves for integer bit manipulation */
    __m128 lo = _mm256_castps256_ps128(x);
    __m128 hi = _mm256_extractf128_ps(x, 1);

    __m128 zero128 = _mm_setzero_ps();

    /* --- Process lo half --- */
    __m128 v_lo = _mm_max_ps(lo, zero128);
    __m128 pos_lo = _mm_cmpgt_ps(v_lo, zero128);
    __m128i iv_lo = _mm_castps_si128(v_lo);
    __m128i exp_lo = _mm_sub_epi32(_mm_srli_epi32(iv_lo, 23), _mm_set1_epi32(127));
    __m128 e_lo = _mm_cvtepi32_ps(exp_lo);
    __m128i mant_lo = _mm_or_si128(_mm_and_si128(iv_lo, _mm_set1_epi32(0x007FFFFF)), _mm_set1_epi32(0x3F800000));
    __m128 m_lo = _mm_castsi128_ps(mant_lo);
    __m128 t_lo = _mm_sub_ps(m_lo, _mm_set1_ps(1.0f));
    __m128 log2_lo = _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(1.44269504f),
                     _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(-0.72134752f),
                     _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(0.48089835f),
                     _mm_mul_ps(t_lo, _mm_add_ps(_mm_set1_ps(-0.36067376f),
                     _mm_mul_ps(t_lo, _mm_set1_ps(0.28854314f))))))))));
    __m128 y_lo = _mm_mul_ps(_mm_set1_ps(1.0f / 2.4f), _mm_add_ps(e_lo, log2_lo));
    __m128 yi_lo = _mm_floor_ps(y_lo);
    __m128 yf_lo = _mm_sub_ps(y_lo, yi_lo);
    __m128i n_lo = _mm_cvttps_epi32(yi_lo);
    __m128 exp2_lo = _mm_add_ps(_mm_set1_ps(1.0f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.69314718f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.24022651f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.05550411f),
                     _mm_mul_ps(yf_lo, _mm_add_ps(_mm_set1_ps(0.00961813f),
                     _mm_mul_ps(yf_lo, _mm_set1_ps(0.00133335f)))))))))));
    __m128i sc_lo = _mm_slli_epi32(_mm_add_epi32(n_lo, _mm_set1_epi32(127)), 23);
    __m128 res_lo = _mm_and_ps(pos_lo, _mm_mul_ps(_mm_castsi128_ps(sc_lo), exp2_lo));

    /* --- Process hi half --- */
    __m128 v_hi = _mm_max_ps(hi, zero128);
    __m128 pos_hi = _mm_cmpgt_ps(v_hi, zero128);
    __m128i iv_hi = _mm_castps_si128(v_hi);
    __m128i exp_hi = _mm_sub_epi32(_mm_srli_epi32(iv_hi, 23), _mm_set1_epi32(127));
    __m128 e_hi = _mm_cvtepi32_ps(exp_hi);
    __m128i mant_hi = _mm_or_si128(_mm_and_si128(iv_hi, _mm_set1_epi32(0x007FFFFF)), _mm_set1_epi32(0x3F800000));
    __m128 m_hi = _mm_castsi128_ps(mant_hi);
    __m128 t_hi = _mm_sub_ps(m_hi, _mm_set1_ps(1.0f));
    __m128 log2_hi = _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(1.44269504f),
                     _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(-0.72134752f),
                     _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(0.48089835f),
                     _mm_mul_ps(t_hi, _mm_add_ps(_mm_set1_ps(-0.36067376f),
                     _mm_mul_ps(t_hi, _mm_set1_ps(0.28854314f))))))))));
    __m128 y_hi = _mm_mul_ps(_mm_set1_ps(1.0f / 2.4f), _mm_add_ps(e_hi, log2_hi));
    __m128 yi_hi = _mm_floor_ps(y_hi);
    __m128 yf_hi = _mm_sub_ps(y_hi, yi_hi);
    __m128i n_hi = _mm_cvttps_epi32(yi_hi);
    __m128 exp2_hi = _mm_add_ps(_mm_set1_ps(1.0f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.69314718f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.24022651f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.05550411f),
                     _mm_mul_ps(yf_hi, _mm_add_ps(_mm_set1_ps(0.00961813f),
                     _mm_mul_ps(yf_hi, _mm_set1_ps(0.00133335f)))))))))));
    __m128i sc_hi = _mm_slli_epi32(_mm_add_epi32(n_hi, _mm_set1_epi32(127)), 23);
    __m128 res_hi = _mm_and_ps(pos_hi, _mm_mul_ps(_mm_castsi128_ps(sc_hi), exp2_hi));

    /* Combine halves */
    return _mm256_set_m128(res_hi, res_lo);
#endif
}

/* ----------------------------------------------------------------
 * Fast cbrt(x) -- bit-trick initial estimate + Newton-Raphson
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
 * Fast approximation functions (f64) -- delegate to exact paths
 * ================================================================ */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow24(alwan_simd_f64 x) {
    return alwan_simd_f64_pow(x, _mm256_set1_pd(2.4));
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow_inv24(alwan_simd_f64 x) {
    return alwan_simd_f64_pow(x, _mm256_set1_pd(1.0 / 2.4));
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt_fast(alwan_simd_f64 x) {
    return alwan_simd_f64_cbrt(x);
}

#endif /* ALWAN_SIMD_AVX_H */
