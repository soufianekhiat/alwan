/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * SIMD scalar fallback: width=1, plain C
 */

#ifndef ALWAN_SIMD_SCALAR_H
#define ALWAN_SIMD_SCALAR_H

#include "alwan_simd_types.h"
#include "../alwan_math.h"   /* must come before ALWAN_POW_F32/etc. uses below;
                                 * alwan_math.h supplies det-mode redefinitions
                                 * when ALWAN_DETERMINISTIC=1. */
#include <math.h>

/* ----------------------------------------------------------------
 * Float32 Arithmetic
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) { return a + b; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sub(alwan_simd_f32 a, alwan_simd_f32 b) { return a - b; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_mul(alwan_simd_f32 a, alwan_simd_f32 b) { return a * b; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_div(alwan_simd_f32 a, alwan_simd_f32 b) { return a / b; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_neg(alwan_simd_f32 a) { return -a; }

/* ----------------------------------------------------------------
 * Float32 Broadcast / Zero
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_set1(float v) { return v; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_zero(void) { return 0.0f; }

/* ----------------------------------------------------------------
 * Float32 Math (EXACT)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a) { return ALWAN_SQRT_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_abs(alwan_simd_f32 a) { return ALWAN_ABS_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 exp) { return ALWAN_POW_F32(base, exp); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt(alwan_simd_f32 a) { return ALWAN_CBRT_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_exp(alwan_simd_f32 a) { return ALWAN_EXP_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log(alwan_simd_f32 a) { return ALWAN_LN_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log2(alwan_simd_f32 a) { return ALWAN_LOG2_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log10(alwan_simd_f32 a) { return ALWAN_LOG10_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sin(alwan_simd_f32 a) { return ALWAN_SIN_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cos(alwan_simd_f32 a) { return ALWAN_COS_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_atan2(alwan_simd_f32 y, alwan_simd_f32 x) { return ALWAN_ATAN2_F32(y, x); }

/* ----------------------------------------------------------------
 * Float32 FMA
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) { return a * b + c; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmsub(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) { return a * b - c; }

/* ----------------------------------------------------------------
 * Float32 Comparison -> Mask
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpeq(alwan_simd_f32 a, alwan_simd_f32 b) { return a == b; }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmplt(alwan_simd_f32 a, alwan_simd_f32 b) { return a < b; }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmple(alwan_simd_f32 a, alwan_simd_f32 b) { return a <= b; }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpgt(alwan_simd_f32 a, alwan_simd_f32 b) { return a > b; }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpge(alwan_simd_f32 a, alwan_simd_f32 b) { return a >= b; }

/* ----------------------------------------------------------------
 * Float32 Select (branchless ternary: mask ? a : b)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m, alwan_simd_f32 a, alwan_simd_f32 b) {
    return m ? a : b;
}

/* ----------------------------------------------------------------
 * Float32 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_min(alwan_simd_f32 a, alwan_simd_f32 b) { return a < b ? a : b; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_max(alwan_simd_f32 a, alwan_simd_f32 b) { return a > b ? a : b; }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_clamp(alwan_simd_f32 v, alwan_simd_f32 lo, alwan_simd_f32 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* ----------------------------------------------------------------
 * Float32 Horizontal
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd_native(alwan_simd_f32 a, alwan_simd_f32 b) {
    (void)b;
    return a; /* width=1 */
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum_native(alwan_simd_f32 a) {
    return a; /* width=1, already scalar */
}

/* ----------------------------------------------------------------
 * Float32 Rounding
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) { return ALWAN_FLOOR_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a) { return ALWAN_CEIL_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a) { return ALWAN_ROUND_F32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_trunc(alwan_simd_f32 a) { return ALWAN_TRUNC_F32(a); }

/* ----------------------------------------------------------------
 * Float32 Reciprocal (exact)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_rcp(alwan_simd_f32 a) { return 1.0f / a; }

/* ----------------------------------------------------------------
 * Float32 Load / Store
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_load(float const *ptr) { return *ptr; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_loadu(float const *ptr) { return *ptr; }
ALWAN_INLINE void alwan_simd_f32_store(float *ptr, alwan_simd_f32 v) { *ptr = v; }
ALWAN_INLINE void alwan_simd_f32_storeu(float *ptr, alwan_simd_f32 v) { *ptr = v; }

/* ----------------------------------------------------------------
 * Float64 Operations (mirror of f32)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_add(alwan_simd_f64 a, alwan_simd_f64 b) { return a + b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sub(alwan_simd_f64 a, alwan_simd_f64 b) { return a - b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_mul(alwan_simd_f64 a, alwan_simd_f64 b) { return a * b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_div(alwan_simd_f64 a, alwan_simd_f64 b) { return a / b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_neg(alwan_simd_f64 a) { return -a; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_set1(double v) { return v; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_zero(void) { return 0.0; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sqrt(alwan_simd_f64 a) { return ALWAN_SQRT_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_abs(alwan_simd_f64 a) { return ALWAN_ABS_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow(alwan_simd_f64 base, alwan_simd_f64 exp) { return ALWAN_POW_F64(base, exp); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt(alwan_simd_f64 a) { return ALWAN_CBRT_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_exp(alwan_simd_f64 a) { return ALWAN_EXP_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log(alwan_simd_f64 a) { return ALWAN_LN_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log2(alwan_simd_f64 a) { return ALWAN_LOG2_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log10(alwan_simd_f64 a) { return ALWAN_LOG10_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sin(alwan_simd_f64 a) { return ALWAN_SIN_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cos(alwan_simd_f64 a) { return ALWAN_COS_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_atan2(alwan_simd_f64 y, alwan_simd_f64 x) { return ALWAN_ATAN2_F64(y, x); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmadd(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) { return a * b + c; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmsub(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) { return a * b - c; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpeq(alwan_simd_f64 a, alwan_simd_f64 b) { return a == b; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmplt(alwan_simd_f64 a, alwan_simd_f64 b) { return a < b; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmple(alwan_simd_f64 a, alwan_simd_f64 b) { return a <= b; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpgt(alwan_simd_f64 a, alwan_simd_f64 b) { return a > b; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpge(alwan_simd_f64 a, alwan_simd_f64 b) { return a >= b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_select(alwan_simd_f64_mask m, alwan_simd_f64 a, alwan_simd_f64 b) { return m ? a : b; }

ALWAN_INLINE int alwan_simd_f32_mask_all_set(alwan_simd_f32_mask m) { return m != 0; }
ALWAN_INLINE int alwan_simd_f64_mask_all_set(alwan_simd_f64_mask m) { return m != 0; }

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_and(alwan_simd_f32_mask a, alwan_simd_f32_mask b) { return a & b; }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_or (alwan_simd_f32_mask a, alwan_simd_f32_mask b) { return a | b; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_and(alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return a & b; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_or (alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return a | b; }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return a < b ? a : b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return a > b ? a : b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_floor(alwan_simd_f64 a) { return ALWAN_FLOOR_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_ceil(alwan_simd_f64 a) { return ALWAN_CEIL_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_round(alwan_simd_f64 a) { return ALWAN_ROUND_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_trunc(alwan_simd_f64 a) { return ALWAN_TRUNC_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_rcp(alwan_simd_f64 a) { return 1.0 / a; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_load(double const *ptr) { return *ptr; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_loadu(double const *ptr) { return *ptr; }
ALWAN_INLINE void alwan_simd_f64_store(double *ptr, alwan_simd_f64 v) { *ptr = v; }
ALWAN_INLINE void alwan_simd_f64_storeu(double *ptr, alwan_simd_f64 v) { *ptr = v; }

/* Scalar backend: 1-lane f64 vector — "horizontal sum" is identity. */
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum_native(alwan_simd_f64 a) { return a; }

/* ----------------------------------------------------------------
 * Integer Load / Store & Conversion
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_u8  alwan_simd_u8_load(uint8_t const *ptr) { return *ptr; }
ALWAN_INLINE alwan_simd_u8  alwan_simd_u8_loadu(uint8_t const *ptr) { return *ptr; }
ALWAN_INLINE void           alwan_simd_u8_store(uint8_t *ptr, alwan_simd_u8 v) { *ptr = v; }
ALWAN_INLINE void           alwan_simd_u8_storeu(uint8_t *ptr, alwan_simd_u8 v) { *ptr = v; }

ALWAN_INLINE alwan_simd_u16 alwan_simd_u16_load(uint16_t const *ptr) { return *ptr; }
ALWAN_INLINE alwan_simd_u16 alwan_simd_u16_loadu(uint16_t const *ptr) { return *ptr; }
ALWAN_INLINE void           alwan_simd_u16_store(uint16_t *ptr, alwan_simd_u16 v) { *ptr = v; }
ALWAN_INLINE void           alwan_simd_u16_storeu(uint16_t *ptr, alwan_simd_u16 v) { *ptr = v; }

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) { return (float)v; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_u16_to_f32(alwan_simd_u16 v) { return (float)v; }

ALWAN_INLINE alwan_simd_u8  alwan_simd_f32_to_u8(alwan_simd_f32 v) {
    int iv = (int)(v + 0.5f);
    if (iv < 0) iv = 0;
    if (iv > 255) iv = 255;
    return (uint8_t)iv;
}

ALWAN_INLINE alwan_simd_u16 alwan_simd_f32_to_u16(alwan_simd_f32 v) {
    int iv = (int)(v + 0.5f);
    if (iv < 0) iv = 0;
    if (iv > 65535) iv = 65535;
    return (uint16_t)iv;
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32_norm(alwan_simd_u8 v) { return (float)v / 255.0f; }
ALWAN_INLINE alwan_simd_f32 alwan_simd_u16_to_f32_norm(alwan_simd_u16 v, alwan_simd_f32 inv_max) { return (float)v * inv_max; }

ALWAN_INLINE alwan_simd_u8  alwan_simd_f32_to_u8_norm(alwan_simd_f32 v) { return alwan_simd_f32_to_u8(v * 255.0f); }
ALWAN_INLINE alwan_simd_u16 alwan_simd_f32_to_u16_norm(alwan_simd_f32 v, alwan_simd_f32 max_val) { return alwan_simd_f32_to_u16(v * max_val); }

/* ----------------------------------------------------------------
 * AoS <-> SoA 3-Channel Deinterleave / Interleave
 * ---------------------------------------------------------------- */

ALWAN_INLINE void alwan_simd_f32_deinterleave3(float const *src,
                                                 alwan_simd_f32 *ch0, alwan_simd_f32 *ch1, alwan_simd_f32 *ch2) {
    *ch0 = src[0]; *ch1 = src[1]; *ch2 = src[2];
}

ALWAN_INLINE void alwan_simd_f32_interleave3(float *dst,
                                               alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    dst[0] = a; dst[1] = b; dst[2] = c;
}

ALWAN_INLINE void alwan_simd_f64_deinterleave3(double const *src,
                                                 alwan_simd_f64 *ch0, alwan_simd_f64 *ch1, alwan_simd_f64 *ch2) {
    *ch0 = src[0]; *ch1 = src[1]; *ch2 = src[2];
}

ALWAN_INLINE void alwan_simd_f64_interleave3(double *dst,
                                               alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    dst[0] = a; dst[1] = b; dst[2] = c;
}

/* ----------------------------------------------------------------
 * Float32 Fast ALWAN_POW_F64(x, 2.4) / ALWAN_POW_F64(x, 1/2.4) -- log2/exp2 decomposition
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow24(alwan_simd_f32 x) {
    if (x <= 0.0f) return 0.0f;
    union { float f; int32_t i; } u;
    u.f = x;
    float e = (float)((u.i >> 23) - 127);
    u.i = (u.i & 0x007FFFFF) | 0x3F800000;
    float m = u.f;
    float t = m - 1.0f;
    /* Minimax polynomial ALWAN_LOG2_F64(m) on [1, 2), max error ~2e-7 */
    float log2_m = t * (1.44269504f + t * (-0.72134752f + t * (0.48089835f + t * (-0.36067376f + t * 0.28854314f))));
    float y = 2.4f * (e + log2_m);
    float yi = ALWAN_FLOOR_F32(y);
    float yf = y - yi;
    int32_t n = (int32_t)yi;
    /* Minimax polynomial 2^yf on [0, 1), max error ~1e-7 */
    float exp2f_v = 1.0f + yf * (0.69314718f + yf * (0.24022651f + yf * (0.05550411f + yf * (0.00961813f + yf * 0.00133335f))));
    u.i = (n + 127) << 23;
    return u.f * exp2f_v;
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow_inv24(alwan_simd_f32 x) {
    if (x <= 0.0f) return 0.0f;
    union { float f; int32_t i; } u;
    u.f = x;
    float e = (float)((u.i >> 23) - 127);
    u.i = (u.i & 0x007FFFFF) | 0x3F800000;
    float m = u.f;
    float t = m - 1.0f;
    /* Minimax polynomial ALWAN_LOG2_F64(m) on [1, 2), max error ~2e-7 */
    float log2_m = t * (1.44269504f + t * (-0.72134752f + t * (0.48089835f + t * (-0.36067376f + t * 0.28854314f))));
    float y = 0.41666667f * (e + log2_m);
    float yi = ALWAN_FLOOR_F32(y);
    float yf = y - yi;
    int32_t n = (int32_t)yi;
    /* Minimax polynomial 2^yf on [0, 1), max error ~1e-7 */
    float exp2f_v = 1.0f + yf * (0.69314718f + yf * (0.24022651f + yf * (0.05550411f + yf * (0.00961813f + yf * 0.00133335f))));
    u.i = (n + 127) << 23;
    return u.f * exp2f_v;
}

/* ----------------------------------------------------------------
 * Float64 ALWAN_POW_F64(x, 2.4) / ALWAN_POW_F64(x, 1/2.4) -- exact via libm
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow24(alwan_simd_f64 x) {
    return ALWAN_POW_F64(x, 2.4);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow_inv24(alwan_simd_f64 x) {
    return ALWAN_POW_F64(x, 1.0 / 2.4);
}

/* ----------------------------------------------------------------
 * Fast cube root -- integer bit trick + Newton-Raphson
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt_fast(alwan_simd_f32 x) {
    if (x == 0.0f) return 0.0f;
    float sign = (x < 0.0f) ? -1.0f : 1.0f;
    float ax = ALWAN_ABS_F32(x);
    union { float f; int32_t i; } u;
    u.f = ax;
    u.i = u.i / 3 + 0x2a508f2e;
    float y = u.f;
    /* Two Newton-Raphson refinements */
    float ax_over_3 = ax * 0.33333333f;
    y = 0.66666667f * y + ax_over_3 / (y * y);
    y = 0.66666667f * y + ax_over_3 / (y * y);
    return sign * y;
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt_fast(alwan_simd_f64 x) {
    if (x == 0.0) return 0.0;
    double sign = (x < 0.0) ? -1.0 : 1.0;
    double ax = ALWAN_ABS_F64(x);
    union { double d; int64_t i; } u;
    u.d = ax;
    u.i = u.i / 3 + 0x2a9f7893782da1ceLL;
    double y = u.d;
    double ax_over_3 = ax / 3.0;
    y = (2.0 / 3.0) * y + ax_over_3 / (y * y);
    y = (2.0 / 3.0) * y + ax_over_3 / (y * y);
    y = (2.0 / 3.0) * y + ax_over_3 / (y * y);
    return sign * y;
}

#endif /* ALWAN_SIMD_SCALAR_H */
