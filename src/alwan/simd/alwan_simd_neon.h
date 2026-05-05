/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * SIMD NEON backend (ARM/AArch64) - 128-bit, 4xf32, 2xf64
 */

#ifndef ALWAN_SIMD_NEON_H
#define ALWAN_SIMD_NEON_H

#include "alwan_simd_types.h"
#include "../alwan_math.h"   /* must come before per-lane ALWAN_POW_F32/etc.
                                 * uses below; alwan_math.h supplies det-mode
                                 * redefinitions when ALWAN_DETERMINISTIC=1. */
#include <arm_neon.h>
#include <math.h>

/* ================================================================
 * Float32 Operations (128-bit, 4 lanes)
 * ================================================================ */

/* ----------------------------------------------------------------
 * Float32 Arithmetic
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_add(alwan_simd_f32 a, alwan_simd_f32 b) { return vaddq_f32(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sub(alwan_simd_f32 a, alwan_simd_f32 b) { return vsubq_f32(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_mul(alwan_simd_f32 a, alwan_simd_f32 b) { return vmulq_f32(a, b); }

#if defined(__aarch64__)
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_div(alwan_simd_f32 a, alwan_simd_f32 b) { return vdivq_f32(a, b); }
#else
/* ARMv7 NEON lacks vdivq_f32; use reciprocal estimate + Newton-Raphson */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_div(alwan_simd_f32 a, alwan_simd_f32 b) {
    float32x4_t recip = vrecpeq_f32(b);
    recip = vmulq_f32(recip, vrecpsq_f32(b, recip));
    recip = vmulq_f32(recip, vrecpsq_f32(b, recip));
    return vmulq_f32(a, recip);
}
#endif

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_neg(alwan_simd_f32 a) { return vnegq_f32(a); }

/* ----------------------------------------------------------------
 * Float32 Broadcast / Zero
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_set1(float v) { return vdupq_n_f32(v); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_zero(void) { return vdupq_n_f32(0.0f); }

/* ----------------------------------------------------------------
 * Float32 Math
 * Transcendentals: per-lane libm calls (no SVML on ARM)
 * ---------------------------------------------------------------- */

#if defined(__aarch64__)
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a) { return vsqrtq_f32(a); }
#else
/* ARMv7 NEON lacks vsqrtq_f32; use reciprocal sqrt estimate + Newton-Raphson */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sqrt(alwan_simd_f32 a) {
    /* Avoid ALWAN_SQRT_F64(0) producing NaN from rsqrt estimate */
    float32x4_t zero = vdupq_n_f32(0.0f);
    uint32x4_t is_zero = vceqq_f32(a, zero);
    float32x4_t rsqrt = vrsqrteq_f32(a);
    rsqrt = vmulq_f32(rsqrt, vrsqrtsq_f32(vmulq_f32(a, rsqrt), rsqrt));
    rsqrt = vmulq_f32(rsqrt, vrsqrtsq_f32(vmulq_f32(a, rsqrt), rsqrt));
    float32x4_t result = vmulq_f32(a, rsqrt);
    return vbslq_f32(is_zero, zero, result);
}
#endif

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_abs(alwan_simd_f32 a) { return vabsq_f32(a); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt(alwan_simd_f32 a) {
    ALWAN_ALIGN(16) float v[4], r[4];
    vst1q_f32(v, a);
    r[0] = ALWAN_CBRT_F32(v[0]); r[1] = ALWAN_CBRT_F32(v[1]); r[2] = ALWAN_CBRT_F32(v[2]); r[3] = ALWAN_CBRT_F32(v[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_exp(alwan_simd_f32 a) {
    ALWAN_ALIGN(16) float v[4], r[4];
    vst1q_f32(v, a);
    r[0] = ALWAN_EXP_F32(v[0]); r[1] = ALWAN_EXP_F32(v[1]); r[2] = ALWAN_EXP_F32(v[2]); r[3] = ALWAN_EXP_F32(v[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log(alwan_simd_f32 a) {
    ALWAN_ALIGN(16) float v[4], r[4];
    vst1q_f32(v, a);
    r[0] = ALWAN_LN_F32(v[0]); r[1] = ALWAN_LN_F32(v[1]); r[2] = ALWAN_LN_F32(v[2]); r[3] = ALWAN_LN_F32(v[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log2(alwan_simd_f32 a) {
    ALWAN_ALIGN(16) float v[4], r[4];
    vst1q_f32(v, a);
    r[0] = ALWAN_LOG2_F32(v[0]); r[1] = ALWAN_LOG2_F32(v[1]); r[2] = ALWAN_LOG2_F32(v[2]); r[3] = ALWAN_LOG2_F32(v[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_log10(alwan_simd_f32 a) {
    ALWAN_ALIGN(16) float v[4], r[4];
    vst1q_f32(v, a);
    r[0] = ALWAN_LOG10_F32(v[0]); r[1] = ALWAN_LOG10_F32(v[1]); r[2] = ALWAN_LOG10_F32(v[2]); r[3] = ALWAN_LOG10_F32(v[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_sin(alwan_simd_f32 a) {
    ALWAN_ALIGN(16) float v[4], r[4];
    vst1q_f32(v, a);
    r[0] = ALWAN_SIN_F32(v[0]); r[1] = ALWAN_SIN_F32(v[1]); r[2] = ALWAN_SIN_F32(v[2]); r[3] = ALWAN_SIN_F32(v[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cos(alwan_simd_f32 a) {
    ALWAN_ALIGN(16) float v[4], r[4];
    vst1q_f32(v, a);
    r[0] = ALWAN_COS_F32(v[0]); r[1] = ALWAN_COS_F32(v[1]); r[2] = ALWAN_COS_F32(v[2]); r[3] = ALWAN_COS_F32(v[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow(alwan_simd_f32 base, alwan_simd_f32 e) {
    ALWAN_ALIGN(16) float b[4], ex[4], r[4];
    vst1q_f32(b, base);
    vst1q_f32(ex, e);
    r[0] = ALWAN_POW_F32(b[0], ex[0]); r[1] = ALWAN_POW_F32(b[1], ex[1]);
    r[2] = ALWAN_POW_F32(b[2], ex[2]); r[3] = ALWAN_POW_F32(b[3], ex[3]);
    return vld1q_f32(r);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_atan2(alwan_simd_f32 y, alwan_simd_f32 x) {
    ALWAN_ALIGN(16) float vy[4], vx[4], r[4];
    vst1q_f32(vy, y);
    vst1q_f32(vx, x);
    r[0] = ALWAN_ATAN2_F32(vy[0], vx[0]); r[1] = ALWAN_ATAN2_F32(vy[1], vx[1]);
    r[2] = ALWAN_ATAN2_F32(vy[2], vx[2]); r[3] = ALWAN_ATAN2_F32(vy[3], vx[3]);
    return vld1q_f32(r);
}

/* ----------------------------------------------------------------
 * Float32 FMA (AArch64 always has vfmaq_f32)
 * ---------------------------------------------------------------- */

#if defined(__aarch64__) || defined(__ARM_FEATURE_FMA)
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return vfmaq_f32(c, a, b);  /* c + a*b */
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmsub(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return vfmaq_f32(vnegq_f32(c), a, b);  /* (-c) + a*b = a*b - c */
}
#else
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmadd(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return vmlaq_f32(c, a, b);  /* c + a*b */
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_fmsub(alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    return vsubq_f32(vmulq_f32(a, b), c);
}
#endif

/* ----------------------------------------------------------------
 * Float32 Comparison -> Mask
 * NEON compares return uint32x4_t (all-ones or all-zeros per lane)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpeq(alwan_simd_f32 a, alwan_simd_f32 b) { return vceqq_f32(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmplt(alwan_simd_f32 a, alwan_simd_f32 b) { return vcltq_f32(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmple(alwan_simd_f32 a, alwan_simd_f32 b) { return vcleq_f32(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpgt(alwan_simd_f32 a, alwan_simd_f32 b) { return vcgtq_f32(a, b); }
ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_cmpge(alwan_simd_f32 a, alwan_simd_f32 b) { return vcgeq_f32(a, b); }

/* ----------------------------------------------------------------
 * Float32 Select (bitwise select: mask ? a : b)
 * vbslq_f32(mask, a, b): for each bit, selects from a if mask bit is 1, else b
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_select(alwan_simd_f32_mask m, alwan_simd_f32 a, alwan_simd_f32 b) {
    return vbslq_f32(m, a, b);
}

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_and(alwan_simd_f32_mask a, alwan_simd_f32_mask b) {
    return vandq_u32(a, b);
}

ALWAN_INLINE alwan_simd_f32_mask alwan_simd_f32_mask_or(alwan_simd_f32_mask a, alwan_simd_f32_mask b) {
    return vorrq_u32(a, b);
}

ALWAN_INLINE int alwan_simd_f32_mask_all_set(alwan_simd_f32_mask m) {
#if defined(__aarch64__)
    return vminvq_u32(m) != 0;
#else
    uint32x2_t t = vand_u32(vget_low_u32(m), vget_high_u32(m));
    return vget_lane_u32(t, 0) != 0 && vget_lane_u32(t, 1) != 0;
#endif
}

/* ----------------------------------------------------------------
 * Float32 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_min(alwan_simd_f32 a, alwan_simd_f32 b) { return vminq_f32(a, b); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_max(alwan_simd_f32 a, alwan_simd_f32 b) { return vmaxq_f32(a, b); }

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_clamp(alwan_simd_f32 v, alwan_simd_f32 lo, alwan_simd_f32 hi) {
    return vminq_f32(vmaxq_f32(v, lo), hi);
}

/* ----------------------------------------------------------------
 * Float32 Horizontal Add
 * AArch64: vpaddq_f32 (full 128-bit pairwise add)
 * ARMv7: vpadd_f32 (64-bit pairwise add, needs two passes)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd_native(alwan_simd_f32 a, alwan_simd_f32 b) {
#if defined(__aarch64__)
    /* vpaddq_f32 does pairwise add: [a0+a1, a2+a3, b0+b1, b2+b3] */
    return vpaddq_f32(a, b);
#else
    float32x2_t a_lo = vget_low_f32(a);
    float32x2_t a_hi = vget_high_f32(a);
    float32x2_t b_lo = vget_low_f32(b);
    float32x2_t b_hi = vget_high_f32(b);
    float32x2_t sum_a = vpadd_f32(a_lo, a_hi);
    float32x2_t sum_b = vpadd_f32(b_lo, b_hi);
    return vcombine_f32(sum_a, sum_b);
#endif
}

/* ----------------------------------------------------------------
 * Float32 Horizontal Sum
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum_native(alwan_simd_f32 a) {
#if defined(__aarch64__)
    float32x4_t t = vpaddq_f32(a, a);
    return vpaddq_f32(t, t);
#else
    float32x2_t lo = vget_low_f32(a);
    float32x2_t hi = vget_high_f32(a);
    float32x2_t sum = vpadd_f32(lo, hi);
    sum = vpadd_f32(sum, sum);
    return vcombine_f32(sum, sum);
#endif
}

/* ----------------------------------------------------------------
 * Float32 Rounding (AArch64 has native rounding; ARMv7 uses cast-to-int)
 * ---------------------------------------------------------------- */

#if defined(__aarch64__)

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) { return vrndmq_f32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a)  { return vrndpq_f32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a) { return vrndnq_f32(a); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_trunc(alwan_simd_f32 a) { return vrndq_f32(a); }

#else

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_floor(alwan_simd_f32 a) {
    int32x4_t ti = vcvtq_s32_f32(a);
    float32x4_t t = vcvtq_f32_s32(ti);
    uint32x4_t mask = vcgtq_f32(t, a);
    float32x4_t one = vdupq_n_f32(1.0f);
    return vsubq_f32(t, vbslq_f32(mask, one, vdupq_n_f32(0.0f)));
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_ceil(alwan_simd_f32 a) {
    int32x4_t ti = vcvtq_s32_f32(a);
    float32x4_t t = vcvtq_f32_s32(ti);
    uint32x4_t mask = vcltq_f32(t, a);
    float32x4_t one = vdupq_n_f32(1.0f);
    return vaddq_f32(t, vbslq_f32(mask, one, vdupq_n_f32(0.0f)));
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_round(alwan_simd_f32 a) {
    /* Round to nearest even: add 0.5, truncate (simple approximation) */
    float32x4_t half = vdupq_n_f32(0.5f);
    float32x4_t sign = vbslq_f32(vcltq_f32(a, vdupq_n_f32(0.0f)),
                                   vdupq_n_f32(-0.5f), half);
    return vcvtq_f32_s32(vcvtq_s32_f32(vaddq_f32(a, sign)));
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_trunc(alwan_simd_f32 a) {
    return vcvtq_f32_s32(vcvtq_s32_f32(a));
}

#endif

/* ----------------------------------------------------------------
 * Float32 Reciprocal (exact: 1.0/x via divq, NOT approximate vrecpeq)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_rcp(alwan_simd_f32 a) {
    return alwan_simd_f32_div(vdupq_n_f32(1.0f), a);
}

/* ----------------------------------------------------------------
 * Float32 Load / Store
 * NEON: vld1q/vst1q handle both aligned and unaligned access
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_load(float const *ptr)  { return vld1q_f32(ptr); }
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_loadu(float const *ptr) { return vld1q_f32(ptr); }
ALWAN_INLINE void alwan_simd_f32_store(float *ptr, alwan_simd_f32 v)  { vst1q_f32(ptr, v); }
ALWAN_INLINE void alwan_simd_f32_storeu(float *ptr, alwan_simd_f32 v) { vst1q_f32(ptr, v); }

/* ================================================================
 * Float64 Operations (128-bit, 2 lanes) -- AArch64 only
 * ================================================================ */

#if defined(__aarch64__)

/* ----------------------------------------------------------------
 * Float64 Arithmetic
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_add(alwan_simd_f64 a, alwan_simd_f64 b) { return vaddq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sub(alwan_simd_f64 a, alwan_simd_f64 b) { return vsubq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_mul(alwan_simd_f64 a, alwan_simd_f64 b) { return vmulq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_div(alwan_simd_f64 a, alwan_simd_f64 b) { return vdivq_f64(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_neg(alwan_simd_f64 a) { return vnegq_f64(a); }

/* ----------------------------------------------------------------
 * Float64 Broadcast / Zero
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_set1(double v) { return vdupq_n_f64(v); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_zero(void)     { return vdupq_n_f64(0.0); }

/* ----------------------------------------------------------------
 * Float64 Math
 * All transcendentals use per-lane libm calls
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sqrt(alwan_simd_f64 a) { return vsqrtq_f64(a); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_abs(alwan_simd_f64 a) { return vabsq_f64(a); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt(alwan_simd_f64 a) {
    ALWAN_ALIGN(16) double v[2], r[2];
    vst1q_f64(v, a);
    r[0] = ALWAN_CBRT_F64(v[0]); r[1] = ALWAN_CBRT_F64(v[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_exp(alwan_simd_f64 a) {
    ALWAN_ALIGN(16) double v[2], r[2];
    vst1q_f64(v, a);
    r[0] = ALWAN_EXP_F64(v[0]); r[1] = ALWAN_EXP_F64(v[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log(alwan_simd_f64 a) {
    ALWAN_ALIGN(16) double v[2], r[2];
    vst1q_f64(v, a);
    r[0] = ALWAN_LN_F64(v[0]); r[1] = ALWAN_LN_F64(v[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log2(alwan_simd_f64 a) {
    ALWAN_ALIGN(16) double v[2], r[2];
    vst1q_f64(v, a);
    r[0] = ALWAN_LOG2_F64(v[0]); r[1] = ALWAN_LOG2_F64(v[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log10(alwan_simd_f64 a) {
    ALWAN_ALIGN(16) double v[2], r[2];
    vst1q_f64(v, a);
    r[0] = ALWAN_LOG10_F64(v[0]); r[1] = ALWAN_LOG10_F64(v[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sin(alwan_simd_f64 a) {
    ALWAN_ALIGN(16) double v[2], r[2];
    vst1q_f64(v, a);
    r[0] = ALWAN_SIN_F64(v[0]); r[1] = ALWAN_SIN_F64(v[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cos(alwan_simd_f64 a) {
    ALWAN_ALIGN(16) double v[2], r[2];
    vst1q_f64(v, a);
    r[0] = ALWAN_COS_F64(v[0]); r[1] = ALWAN_COS_F64(v[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow(alwan_simd_f64 base, alwan_simd_f64 e) {
    ALWAN_ALIGN(16) double b[2], ex[2], r[2];
    vst1q_f64(b, base);
    vst1q_f64(ex, e);
    r[0] = ALWAN_POW_F64(b[0], ex[0]); r[1] = ALWAN_POW_F64(b[1], ex[1]);
    return vld1q_f64(r);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_atan2(alwan_simd_f64 y, alwan_simd_f64 x) {
    ALWAN_ALIGN(16) double vy[2], vx[2], r[2];
    vst1q_f64(vy, y);
    vst1q_f64(vx, x);
    r[0] = ALWAN_ATAN2_F64(vy[0], vx[0]); r[1] = ALWAN_ATAN2_F64(vy[1], vx[1]);
    return vld1q_f64(r);
}

/* ----------------------------------------------------------------
 * Float64 FMA (AArch64 always has vfmaq_f64)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmadd(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    return vfmaq_f64(c, a, b);  /* c + a*b */
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmsub(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    return vfmaq_f64(vnegq_f64(c), a, b);  /* (-c) + a*b = a*b - c */
}

/* ----------------------------------------------------------------
 * Float64 Comparison -> Mask
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpeq(alwan_simd_f64 a, alwan_simd_f64 b) { return vceqq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmplt(alwan_simd_f64 a, alwan_simd_f64 b) { return vcltq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmple(alwan_simd_f64 a, alwan_simd_f64 b) { return vcleq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpgt(alwan_simd_f64 a, alwan_simd_f64 b) { return vcgtq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpge(alwan_simd_f64 a, alwan_simd_f64 b) { return vcgeq_f64(a, b); }

/* ----------------------------------------------------------------
 * Float64 Select (bitwise select: mask ? a : b)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_select(alwan_simd_f64_mask m, alwan_simd_f64 a, alwan_simd_f64 b) {
    return vbslq_f64(m, a, b);
}

ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_and(alwan_simd_f64_mask a, alwan_simd_f64_mask b) {
    return vandq_u64(a, b);
}

ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_or(alwan_simd_f64_mask a, alwan_simd_f64_mask b) {
    return vorrq_u64(a, b);
}

ALWAN_INLINE int alwan_simd_f64_mask_all_set(alwan_simd_f64_mask m) {
    return vgetq_lane_u64(m, 0) != 0 && vgetq_lane_u64(m, 1) != 0;
}

/* ----------------------------------------------------------------
 * Float64 Min / Max / Clamp
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return vminq_f64(a, b); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return vmaxq_f64(a, b); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    return vminq_f64(vmaxq_f64(v, lo), hi);
}

/* ----------------------------------------------------------------
 * Float64 Rounding (AArch64 has native rounding instructions)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_floor(alwan_simd_f64 a) { return vrndmq_f64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_ceil(alwan_simd_f64 a)  { return vrndpq_f64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_round(alwan_simd_f64 a) { return vrndnq_f64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_trunc(alwan_simd_f64 a) { return vrndq_f64(a); }

/* ----------------------------------------------------------------
 * Float64 Reciprocal (exact: 1.0/x)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_rcp(alwan_simd_f64 a) {
    return vdivq_f64(vdupq_n_f64(1.0), a);
}

/* ----------------------------------------------------------------
 * Float64 Load / Store
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_load(double const *ptr)  { return vld1q_f64(ptr); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_loadu(double const *ptr) { return vld1q_f64(ptr); }
ALWAN_INLINE void alwan_simd_f64_store(double *ptr, alwan_simd_f64 v)  { vst1q_f64(ptr, v); }
ALWAN_INLINE void alwan_simd_f64_storeu(double *ptr, alwan_simd_f64 v) { vst1q_f64(ptr, v); }

/* Native horizontal sum (2-lane f64). Order is implementation-defined
 * by NEON; deterministic mode bypasses this via alwan_simd_reduce.h. */
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum_native(alwan_simd_f64 a) {
    return vdupq_n_f64(vaddvq_f64(a));
}

#else /* !__aarch64__ -- ARMv7 f64 scalar fallback */

/* ARMv7 NEON has no 64-bit float SIMD; scalarize all f64 operations */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_add(alwan_simd_f64 a, alwan_simd_f64 b) { return a + b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sub(alwan_simd_f64 a, alwan_simd_f64 b) { return a - b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_mul(alwan_simd_f64 a, alwan_simd_f64 b) { return a * b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_div(alwan_simd_f64 a, alwan_simd_f64 b) { return a / b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_neg(alwan_simd_f64 a) { return -a; }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_set1(double v) { return v; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_zero(void) { return 0.0; }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sqrt(alwan_simd_f64 a) { return ALWAN_SQRT_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_abs(alwan_simd_f64 a) { return ALWAN_ABS_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt(alwan_simd_f64 a) { return ALWAN_CBRT_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_exp(alwan_simd_f64 a) { return ALWAN_EXP_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log(alwan_simd_f64 a) { return ALWAN_LN_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log2(alwan_simd_f64 a) { return ALWAN_LOG2_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_log10(alwan_simd_f64 a) { return ALWAN_LOG10_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_sin(alwan_simd_f64 a) { return ALWAN_SIN_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cos(alwan_simd_f64 a) { return ALWAN_COS_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow(alwan_simd_f64 base, alwan_simd_f64 e) { return ALWAN_POW_F64(base, e); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_atan2(alwan_simd_f64 y, alwan_simd_f64 x) { return ALWAN_ATAN2_F64(y, x); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmadd(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) { return a * b + c; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_fmsub(alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) { return a * b - c; }

ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpeq(alwan_simd_f64 a, alwan_simd_f64 b) { return a == b ? ~(uint64_t)0 : 0; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmplt(alwan_simd_f64 a, alwan_simd_f64 b) { return a <  b ? ~(uint64_t)0 : 0; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmple(alwan_simd_f64 a, alwan_simd_f64 b) { return a <= b ? ~(uint64_t)0 : 0; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpgt(alwan_simd_f64 a, alwan_simd_f64 b) { return a >  b ? ~(uint64_t)0 : 0; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_cmpge(alwan_simd_f64 a, alwan_simd_f64 b) { return a >= b ? ~(uint64_t)0 : 0; }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_select(alwan_simd_f64_mask m, alwan_simd_f64 a, alwan_simd_f64 b) { return m ? a : b; }

ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_and(alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return a & b; }
ALWAN_INLINE alwan_simd_f64_mask alwan_simd_f64_mask_or(alwan_simd_f64_mask a, alwan_simd_f64_mask b) { return a | b; }

ALWAN_INLINE int alwan_simd_f64_mask_all_set(alwan_simd_f64_mask m) { return m != 0; }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_min(alwan_simd_f64 a, alwan_simd_f64 b) { return a < b ? a : b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_max(alwan_simd_f64 a, alwan_simd_f64 b) { return a > b ? a : b; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_clamp(alwan_simd_f64 v, alwan_simd_f64 lo, alwan_simd_f64 hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_floor(alwan_simd_f64 a) { return ALWAN_FLOOR_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_ceil(alwan_simd_f64 a) { return ALWAN_CEIL_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_round(alwan_simd_f64 a) { return ALWAN_ROUND_F64(a); }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_trunc(alwan_simd_f64 a) { return ALWAN_TRUNC_F64(a); }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_rcp(alwan_simd_f64 a) { return 1.0 / a; }

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_load(double const *ptr) { return *ptr; }
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_loadu(double const *ptr) { return *ptr; }
ALWAN_INLINE void alwan_simd_f64_store(double *ptr, alwan_simd_f64 v) { *ptr = v; }
ALWAN_INLINE void alwan_simd_f64_storeu(double *ptr, alwan_simd_f64 v) { *ptr = v; }

/* ARMv7 fallback f64 is already scalar; "horizontal sum" is identity. */
ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum_native(alwan_simd_f64 a) { return a; }

#endif /* __aarch64__ f64 */

/* ================================================================
 * Integer Operations
 * ================================================================ */

/* ----------------------------------------------------------------
 * Integer Load / Store
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_u8  alwan_simd_u8_load(uint8_t const *ptr)   { return vld1q_u8(ptr); }
ALWAN_INLINE alwan_simd_u8  alwan_simd_u8_loadu(uint8_t const *ptr)  { return vld1q_u8(ptr); }
ALWAN_INLINE void alwan_simd_u8_store(uint8_t *ptr, alwan_simd_u8 v)  { vst1q_u8(ptr, v); }
ALWAN_INLINE void alwan_simd_u8_storeu(uint8_t *ptr, alwan_simd_u8 v) { vst1q_u8(ptr, v); }

ALWAN_INLINE alwan_simd_u16 alwan_simd_u16_load(uint16_t const *ptr)  { return vld1q_u16(ptr); }
ALWAN_INLINE alwan_simd_u16 alwan_simd_u16_loadu(uint16_t const *ptr) { return vld1q_u16(ptr); }
ALWAN_INLINE void alwan_simd_u16_store(uint16_t *ptr, alwan_simd_u16 v)  { vst1q_u16(ptr, v); }
ALWAN_INLINE void alwan_simd_u16_storeu(uint16_t *ptr, alwan_simd_u16 v) { vst1q_u16(ptr, v); }

/* ----------------------------------------------------------------
 * Integer Widening: u8 -> f32, u16 -> f32
 * Widen lowest 4 bytes/shorts to 4x f32
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_u8_to_f32(alwan_simd_u8 v) {
    /* Extract low 8 bytes, widen u8->u16 */
    uint16x8_t u16 = vmovl_u8(vget_low_u8(v));
    /* Extract low 4 shorts, widen u16->u32 */
    uint32x4_t u32 = vmovl_u16(vget_low_u16(u16));
    /* Convert u32 -> f32 */
    return vcvtq_f32_u32(u32);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_u16_to_f32(alwan_simd_u16 v) {
    /* Extract low 4 shorts, widen u16->u32 */
    uint32x4_t u32 = vmovl_u16(vget_low_u16(v));
    /* Convert u32 -> f32 */
    return vcvtq_f32_u32(u32);
}

/* ----------------------------------------------------------------
 * u8 Shuffle (byte permutation)
 * AArch64: vqtbl1q_u8 (full 128-bit table lookup)
 * ARMv7: vtbl2_u8 (two 64-bit table halves)
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_u8 alwan_simd_u8_shuffle(alwan_simd_u8 v, alwan_simd_u8 mask) {
#if defined(__aarch64__)
    return vqtbl1q_u8(v, mask);
#else
    uint8x8x2_t tbl;
    tbl.val[0] = vget_low_u8(v);
    tbl.val[1] = vget_high_u8(v);
    uint8x8_t lo = vtbl2_u8(tbl, vget_low_u8(mask));
    uint8x8_t hi = vtbl2_u8(tbl, vget_high_u8(mask));
    return vcombine_u8(lo, hi);
#endif
}

/* ----------------------------------------------------------------
 * AoS <-> SoA 3-Channel Deinterleave / Interleave
 * ---------------------------------------------------------------- */

/* f32: vld3q/vst3q provides native 3-channel deinterleave */
ALWAN_INLINE void alwan_simd_f32_deinterleave3(float const *src,
                                                 alwan_simd_f32 *ch0, alwan_simd_f32 *ch1, alwan_simd_f32 *ch2) {
    float32x4x3_t rgb = vld3q_f32(src);
    *ch0 = rgb.val[0];
    *ch1 = rgb.val[1];
    *ch2 = rgb.val[2];
}

ALWAN_INLINE void alwan_simd_f32_interleave3(float *dst,
                                               alwan_simd_f32 a, alwan_simd_f32 b, alwan_simd_f32 c) {
    float32x4x3_t rgb;
    rgb.val[0] = a;
    rgb.val[1] = b;
    rgb.val[2] = c;
    vst3q_f32(dst, rgb);
}

#if defined(__aarch64__)
/* f64: AArch64 native vld3q_f64 */
ALWAN_INLINE void alwan_simd_f64_deinterleave3(double const *src,
                                                 alwan_simd_f64 *ch0, alwan_simd_f64 *ch1, alwan_simd_f64 *ch2) {
    float64x2x3_t rgb = vld3q_f64(src);
    *ch0 = rgb.val[0];
    *ch1 = rgb.val[1];
    *ch2 = rgb.val[2];
}

ALWAN_INLINE void alwan_simd_f64_interleave3(double *dst,
                                               alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    float64x2x3_t rgb;
    rgb.val[0] = a;
    rgb.val[1] = b;
    rgb.val[2] = c;
    vst3q_f64(dst, rgb);
}
#else
/* ARMv7: scalar f64 fallback */
ALWAN_INLINE void alwan_simd_f64_deinterleave3(double const *src,
                                                 alwan_simd_f64 *ch0, alwan_simd_f64 *ch1, alwan_simd_f64 *ch2) {
    *ch0 = src[0]; *ch1 = src[1]; *ch2 = src[2];
}

ALWAN_INLINE void alwan_simd_f64_interleave3(double *dst,
                                               alwan_simd_f64 a, alwan_simd_f64 b, alwan_simd_f64 c) {
    dst[0] = a; dst[1] = b; dst[2] = c;
}
#endif

/* ================================================================
 * Fast Approximation Functions
 * ================================================================ */

/* ----------------------------------------------------------------
 * Float32 ALWAN_POW_F64(x, 2.4) -- fast vectorized via exp2(2.4 * ALWAN_LOG2_F64(x))
 * Uses IEEE 754 integer bit tricks + minimax polynomials
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow24(alwan_simd_f32 x) {
    float32x4_t zero = vdupq_n_f32(0.0f);
    float32x4_t v = vmaxq_f32(x, zero);
    uint32x4_t is_pos = vcgtq_f32(v, zero);

    /* ALWAN_LOG2_F64(v) via IEEE 754 decomposition */
    int32x4_t iv = vreinterpretq_s32_f32(v);
    int32x4_t exp_i = vsubq_s32(vshrq_n_s32(iv, 23), vdupq_n_s32(127));
    float32x4_t e = vcvtq_f32_s32(exp_i);
    int32x4_t mant_bits = vorrq_s32(
        vandq_s32(iv, vdupq_n_s32(0x007FFFFF)),
        vdupq_n_s32(0x3F800000));
    float32x4_t m = vreinterpretq_f32_s32(mant_bits);

    /* Minimax polynomial ALWAN_LOG2_F64(m) on [1, 2) */
    float32x4_t t = vsubq_f32(m, vdupq_n_f32(1.0f));
    float32x4_t log2_m = vmulq_f32(t, vaddq_f32(vdupq_n_f32(1.44269504f),
                         vmulq_f32(t, vaddq_f32(vdupq_n_f32(-0.72134752f),
                         vmulq_f32(t, vaddq_f32(vdupq_n_f32(0.48089835f),
                         vmulq_f32(t, vaddq_f32(vdupq_n_f32(-0.36067376f),
                         vmulq_f32(t, vdupq_n_f32(0.28854314f))))))))));

    float32x4_t y = vmulq_f32(vdupq_n_f32(2.4f), vaddq_f32(e, log2_m));

    /* exp2(y) */
    float32x4_t yi = alwan_simd_f32_floor(y);
    float32x4_t yf = vsubq_f32(y, yi);
    int32x4_t n = vcvtq_s32_f32(yi);

    /* Minimax polynomial 2^yf on [0, 1) */
    float32x4_t exp2f_val = vaddq_f32(vdupq_n_f32(1.0f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.69314718f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.24022651f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.05550411f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.00961813f),
                            vmulq_f32(yf, vdupq_n_f32(0.00133335f)))))))))));

    int32x4_t scale_i = vshlq_n_s32(vaddq_s32(n, vdupq_n_s32(127)), 23);
    float32x4_t scale = vreinterpretq_f32_s32(scale_i);
    float32x4_t result = vmulq_f32(scale, exp2f_val);

    /* Zero out negative/zero inputs */
    return vreinterpretq_f32_u32(vandq_u32(is_pos, vreinterpretq_u32_f32(result)));
}

/* ----------------------------------------------------------------
 * Float32 ALWAN_POW_F64(x, 1/2.4) -- fast vectorized via exp2((1/2.4) * ALWAN_LOG2_F64(x))
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_pow_inv24(alwan_simd_f32 x) {
    float32x4_t zero = vdupq_n_f32(0.0f);
    float32x4_t v = vmaxq_f32(x, zero);
    uint32x4_t is_pos = vcgtq_f32(v, zero);

    /* ALWAN_LOG2_F64(v) via IEEE 754 decomposition */
    int32x4_t iv = vreinterpretq_s32_f32(v);
    int32x4_t exp_i = vsubq_s32(vshrq_n_s32(iv, 23), vdupq_n_s32(127));
    float32x4_t e = vcvtq_f32_s32(exp_i);
    int32x4_t mant_bits = vorrq_s32(
        vandq_s32(iv, vdupq_n_s32(0x007FFFFF)),
        vdupq_n_s32(0x3F800000));
    float32x4_t m = vreinterpretq_f32_s32(mant_bits);

    /* Minimax polynomial ALWAN_LOG2_F64(m) on [1, 2) */
    float32x4_t t = vsubq_f32(m, vdupq_n_f32(1.0f));
    float32x4_t log2_m = vmulq_f32(t, vaddq_f32(vdupq_n_f32(1.44269504f),
                         vmulq_f32(t, vaddq_f32(vdupq_n_f32(-0.72134752f),
                         vmulq_f32(t, vaddq_f32(vdupq_n_f32(0.48089835f),
                         vmulq_f32(t, vaddq_f32(vdupq_n_f32(-0.36067376f),
                         vmulq_f32(t, vdupq_n_f32(0.28854314f))))))))));

    float32x4_t y = vmulq_f32(vdupq_n_f32(0.41666667f), vaddq_f32(e, log2_m));

    /* exp2(y) */
    float32x4_t yi = alwan_simd_f32_floor(y);
    float32x4_t yf = vsubq_f32(y, yi);
    int32x4_t n = vcvtq_s32_f32(yi);

    /* Minimax polynomial 2^yf on [0, 1) */
    float32x4_t exp2f_val = vaddq_f32(vdupq_n_f32(1.0f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.69314718f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.24022651f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.05550411f),
                            vmulq_f32(yf, vaddq_f32(vdupq_n_f32(0.00961813f),
                            vmulq_f32(yf, vdupq_n_f32(0.00133335f)))))))))));

    int32x4_t scale_i = vshlq_n_s32(vaddq_s32(n, vdupq_n_s32(127)), 23);
    float32x4_t scale = vreinterpretq_f32_s32(scale_i);
    float32x4_t result = vmulq_f32(scale, exp2f_val);

    /* Zero out negative/zero inputs */
    return vreinterpretq_f32_u32(vandq_u32(is_pos, vreinterpretq_u32_f32(result)));
}

/* ----------------------------------------------------------------
 * Float64 ALWAN_POW_F64(x, 2.4) -- fast log2/exp2 on AArch64, scalar fallback
 * Valid for normal-range inputs; subnormal inputs may be inaccurate.
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow24(alwan_simd_f64 x) {
#if defined(__aarch64__)
    float64x2_t zero   = vdupq_n_f64(0.0);
    float64x2_t v      = vmaxq_f64(x, zero);
    uint64x2_t  is_pos = vcgtq_f64(v, zero);
    int64x2_t   iv     = vreinterpretq_s64_f64(v);
    /* Extract binary exponent: (unsigned_bits >> 52) - 1023 */
    int64x2_t exp_i64  = vsubq_s64(
        vreinterpretq_s64_u64(vshrq_n_u64(vreinterpretq_u64_s64(iv), 52)),
        vdupq_n_s64(1023LL));
    /* Convert exponent to f64 directly (AArch64 has vcvtq_f64_s64) */
    float64x2_t e = vcvtq_f64_s64(exp_i64);
    /* Extract mantissa in [1.0, 2.0) */
    int64x2_t mant_bits = vorrq_s64(
        vandq_s64(iv, vdupq_n_s64(0x000FFFFFFFFFFFFFLL)),
        vdupq_n_s64(0x3FF0000000000000LL));
    float64x2_t m = vreinterpretq_f64_s64(mant_bits);
    /* ALWAN_LOG2_F64(m) on [1, 2): Horner polynomial, t = m - 1 */
    float64x2_t t = vsubq_f64(m, vdupq_n_f64(1.0));
    float64x2_t log2_m = vmulq_f64(t, vaddq_f64(vdupq_n_f64(1.4426950408889634),
                         vmulq_f64(t, vaddq_f64(vdupq_n_f64(-0.7213475204049363),
                         vmulq_f64(t, vaddq_f64(vdupq_n_f64(0.4808983469618909),
                         vmulq_f64(t, vaddq_f64(vdupq_n_f64(-0.3606737602744954),
                         vmulq_f64(t, vdupq_n_f64(0.28854301595785953))))))))));
    float64x2_t y  = vmulq_f64(vdupq_n_f64(2.4), vaddq_f64(e, log2_m));
    float64x2_t yi = vrndmq_f64(y);  /* floor */
    float64x2_t yf = vsubq_f64(y, yi);
    /* exp2(yf) on [0, 1): Horner polynomial */
    float64x2_t exp2f = vaddq_f64(vdupq_n_f64(1.0),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.6931471805599453),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.24022650695910071),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.05550410866482158),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.009618129107628477),
                        vmulq_f64(yf, vdupq_n_f64(0.0013333558146428443)))))))))));
    /* Scale: 2^yi = float with exponent field = (yi + 1023) << 52 */
    int64x2_t yi_i64  = vcvtq_s64_f64(yi);
    int64x2_t scale_i = vshlq_n_s64(vaddq_s64(yi_i64, vdupq_n_s64(1023LL)), 52);
    float64x2_t result = vmulq_f64(vreinterpretq_f64_s64(scale_i), exp2f);
    return vreinterpretq_f64_u64(vandq_u64(is_pos, vreinterpretq_u64_f64(result)));
#else
    return alwan_simd_f64_pow(x, alwan_simd_f64_set1(2.4));
#endif
}

/* ----------------------------------------------------------------
 * Float64 ALWAN_POW_F64(x, 1/2.4) -- fast log2/exp2 on AArch64, scalar fallback
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_pow_inv24(alwan_simd_f64 x) {
#if defined(__aarch64__)
    float64x2_t zero   = vdupq_n_f64(0.0);
    float64x2_t v      = vmaxq_f64(x, zero);
    uint64x2_t  is_pos = vcgtq_f64(v, zero);
    int64x2_t   iv     = vreinterpretq_s64_f64(v);
    int64x2_t exp_i64  = vsubq_s64(
        vreinterpretq_s64_u64(vshrq_n_u64(vreinterpretq_u64_s64(iv), 52)),
        vdupq_n_s64(1023LL));
    float64x2_t e = vcvtq_f64_s64(exp_i64);
    int64x2_t mant_bits = vorrq_s64(
        vandq_s64(iv, vdupq_n_s64(0x000FFFFFFFFFFFFFLL)),
        vdupq_n_s64(0x3FF0000000000000LL));
    float64x2_t m = vreinterpretq_f64_s64(mant_bits);
    float64x2_t t = vsubq_f64(m, vdupq_n_f64(1.0));
    float64x2_t log2_m = vmulq_f64(t, vaddq_f64(vdupq_n_f64(1.4426950408889634),
                         vmulq_f64(t, vaddq_f64(vdupq_n_f64(-0.7213475204049363),
                         vmulq_f64(t, vaddq_f64(vdupq_n_f64(0.4808983469618909),
                         vmulq_f64(t, vaddq_f64(vdupq_n_f64(-0.3606737602744954),
                         vmulq_f64(t, vdupq_n_f64(0.28854301595785953))))))))));
    float64x2_t y  = vmulq_f64(vdupq_n_f64(1.0 / 2.4), vaddq_f64(e, log2_m));
    float64x2_t yi = vrndmq_f64(y);
    float64x2_t yf = vsubq_f64(y, yi);
    float64x2_t exp2f = vaddq_f64(vdupq_n_f64(1.0),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.6931471805599453),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.24022650695910071),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.05550410866482158),
                        vmulq_f64(yf, vaddq_f64(vdupq_n_f64(0.009618129107628477),
                        vmulq_f64(yf, vdupq_n_f64(0.0013333558146428443)))))))))));
    int64x2_t yi_i64  = vcvtq_s64_f64(yi);
    int64x2_t scale_i = vshlq_n_s64(vaddq_s64(yi_i64, vdupq_n_s64(1023LL)), 52);
    float64x2_t result = vmulq_f64(vreinterpretq_f64_s64(scale_i), exp2f);
    return vreinterpretq_f64_u64(vandq_u64(is_pos, vreinterpretq_u64_f64(result)));
#else
    return alwan_simd_f64_pow(x, alwan_simd_f64_set1(1.0 / 2.4));
#endif
}

/* ----------------------------------------------------------------
 * Float32 cbrt (fast approximation)
 * Integer bit trick + two Newton-Raphson refinements
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_cbrt_fast(alwan_simd_f32 x) {
    float32x4_t zero = vdupq_n_f32(0.0f);
    float32x4_t sign_bit = vdupq_n_f32(-0.0f);
    float32x4_t sign = vreinterpretq_f32_u32(vandq_u32(
        vreinterpretq_u32_f32(x), vreinterpretq_u32_f32(sign_bit)));
    float32x4_t ax = vabsq_f32(x);
    uint32x4_t is_nonzero = vcgtq_f32(ax, zero);

    /* Initial estimate via integer bit trick */
    ALWAN_ALIGN(16) float av[4], yv[4];
    vst1q_f32(av, ax);
    for (int k = 0; k < 4; k++) {
        union { float f; int32_t i; } u;
        u.f = av[k];
        u.i = u.i / 3 + 0x2a508f2e;
        yv[k] = u.f;
    }
    float32x4_t y = vld1q_f32(yv);

    /* Two Newton-Raphson refinements: y = 2/3 * y + x/(3*y^2) */
    float32x4_t two_thirds = vdupq_n_f32(0.66666667f);
    float32x4_t one_third = vdupq_n_f32(0.33333333f);
    float32x4_t ax_3 = vmulq_f32(ax, one_third);
    float32x4_t y2;

    y2 = vmulq_f32(y, y);
    y = vaddq_f32(vmulq_f32(two_thirds, y), alwan_simd_f32_div(ax_3, y2));

    y2 = vmulq_f32(y, y);
    y = vaddq_f32(vmulq_f32(two_thirds, y), alwan_simd_f32_div(ax_3, y2));

    /* Restore sign */
    y = vreinterpretq_f32_u32(vorrq_u32(
        vreinterpretq_u32_f32(y), vreinterpretq_u32_f32(sign)));
    /* Zero out where input was zero */
    return vreinterpretq_f32_u32(vandq_u32(is_nonzero, vreinterpretq_u32_f32(y)));
}

/* ----------------------------------------------------------------
 * Float64 cbrt (fast) -- forward to alwan_simd_f64_cbrt
 * ---------------------------------------------------------------- */

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_cbrt_fast(alwan_simd_f64 x) {
    return alwan_simd_f64_cbrt(x);
}

#endif /* ALWAN_SIMD_NEON_H */
