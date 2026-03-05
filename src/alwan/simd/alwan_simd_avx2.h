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

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) { return _mm256_hadd_ps(a, b); }
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

/* ----------------------------------------------------------------
 * Float64 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_min_pd(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return _mm256_max_pd(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    return _mm256_min_pd(_mm256_max_pd(v, lo), hi);
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

#endif /* ALWAN_SIMD_AVX2_H */
