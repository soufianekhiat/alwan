/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_fast_pow.h - scalar fast-mode pow(x, 2.4) / pow(x, 1/2.4).
 *
 * These are the *scalar* twins of the vectorised alwan_simd_*_pow24 /
 * pow_inv24 in simd/. They evaluate the IDENTICAL log2/exp2 decomposition
 * (atanh-series log2 on the mantissa + degree-10 exp2 Taylor) with the same
 * coefficients and the same operation order, so the scalar _v API and the
 * SIMD map kernels produce matching results in fast (non-deterministic) mode
 * -- on platforms without an accurate vector pow (no SVML, e.g. NEON), where
 * the scalar path would otherwise use libm and diverge from the polynomial
 * SIMD path. In deterministic mode the det polynomials are used instead and
 * this file is unused. See docs/determinism.md and the SIMD pow kernels.
 *
 * Accuracy vs libm over [0,1]: ~5e-10 absolute (pow_inv24/pow24), i.e. the
 * f64 fast-mode floor; this is the documented "fast mode is approximate"
 * trade-off. The bit-exact mantissa/exponent split (matching the SIMD
 * intrinsics) keeps floor(y) identical to the vector path, so there is no
 * power-of-two boundary divergence between scalar and SIMD.
 */
#ifndef ALWAN_FAST_POW_H
#define ALWAN_FAST_POW_H

#include "../alwan_types.h"
#include "../simd/alwan_simd_types.h"   /* ALWAN_HAS_SVML */
#include <stdint.h>
#include <math.h>

#ifndef ALWAN_INLINE
#  define ALWAN_INLINE static inline
#endif

/* When SVML is present the SIMD pow kernels use _mm_pow_* (libm-accurate), so
 * the scalar twin must also be libm to match. Without SVML the SIMD kernels use
 * the polynomial below, so the scalar twin uses the same polynomial. This keeps
 * the scalar _v path and the SIMD map path in agreement on every platform. */

/* ---- f64 ---------------------------------------------------------------- */

ALWAN_INLINE alwan_f64 alwan__fast_pow_pos_f64(alwan_f64 x, alwan_f64 p) {
    union { alwan_f64 d; uint64_t u; } bits;
    union { uint64_t u; alwan_f64 d; } mb, sc;
    alwan_f64 e, m, s, s2, lp, log2_m, y, yi, yf, e2, exp2f;
    int64_t e_i, yi_i;
    if (!(x > 0.0)) return 0.0;
    bits.d = x;
    e_i = (int64_t)(bits.u >> 52) - 1023;
    e   = (alwan_f64)e_i;
    mb.u = (bits.u & 0x000FFFFFFFFFFFFFULL) | 0x3FF0000000000000ULL;
    m    = mb.d;                                   /* mantissa in [1, 2) */
    /* log2(m) on [1,2) via atanh series, s=(m-1)/(m+1) in [0,1/3) */
    s  = (m - 1.0) / (m + 1.0);
    s2 = s * s;
    lp = 1.0/15.0 + s2 * (1.0/17.0);
    lp = 1.0/13.0 + s2 * lp;
    lp = 1.0/11.0 + s2 * lp;
    lp = 1.0/9.0  + s2 * lp;
    lp = 1.0/7.0  + s2 * lp;
    lp = 1.0/5.0  + s2 * lp;
    lp = 1.0/3.0  + s2 * lp;
    lp = 1.0      + s2 * lp;
    log2_m = 2.8853900817779268 * (s * lp);       /* 2/ln2 */
    y  = p * (e + log2_m);
    yi = floor(y);
    yf = y - yi;
    /* exp2(yf) on [0,1): degree-10 Taylor (ln2^k/k!) */
    e2 = 1.0178086009239696e-07 + yf * 7.0549116208011209e-09;
    e2 = 1.3215486790144305e-06 + yf * e2;
    e2 = 1.5252733804059838e-05 + yf * e2;
    e2 = 0.00015403530393381606 + yf * e2;
    e2 = 0.0013333558146428441  + yf * e2;
    e2 = 0.0096181291076284769  + yf * e2;
    e2 = 0.055504108664821576   + yf * e2;
    e2 = 0.24022650695910069    + yf * e2;
    e2 = 0.69314718055994529    + yf * e2;
    exp2f = 1.0 + yf * e2;
    yi_i = (int64_t)yi;
    sc.u = (uint64_t)(yi_i + 1023) << 52;          /* 2^yi */
    return sc.d * exp2f;
}

#if ALWAN_HAS_SVML
ALWAN_INLINE alwan_f64 alwan_fast_pow24_f64(alwan_f64 x)     { return pow(x, 2.4); }
ALWAN_INLINE alwan_f64 alwan_fast_pow_inv24_f64(alwan_f64 x) { return pow(x, 1.0 / 2.4); }
#else
ALWAN_INLINE alwan_f64 alwan_fast_pow24_f64(alwan_f64 x)     { return alwan__fast_pow_pos_f64(x, 2.4); }
ALWAN_INLINE alwan_f64 alwan_fast_pow_inv24_f64(alwan_f64 x) { return alwan__fast_pow_pos_f64(x, 1.0 / 2.4); }
#endif

/* ---- f32 ---------------------------------------------------------------- */

ALWAN_INLINE alwan_f32 alwan__fast_pow_pos_f32(alwan_f32 x, alwan_f32 p) {
    union { alwan_f32 f; uint32_t u; } bits;
    union { uint32_t u; alwan_f32 f; } mb, sc;
    alwan_f32 e, m, s, s2, lp, log2_m, y, yi, yf, e2, exp2f;
    int32_t e_i, yi_i;
    if (!(x > 0.0f)) return 0.0f;
    bits.f = x;
    e_i = (int32_t)(bits.u >> 23) - 127;
    e   = (alwan_f32)e_i;
    mb.u = (bits.u & 0x007FFFFFu) | 0x3F800000u;
    m    = mb.f;                                   /* mantissa in [1, 2) */
    s  = (m - 1.0f) / (m + 1.0f);
    s2 = s * s;
    lp = 1.0f/11.0f + s2 * (1.0f/13.0f);
    lp = 1.0f/9.0f  + s2 * lp;
    lp = 1.0f/7.0f  + s2 * lp;
    lp = 1.0f/5.0f  + s2 * lp;
    lp = 1.0f/3.0f  + s2 * lp;
    lp = 1.0f       + s2 * lp;
    log2_m = 2.8853901f * (s * lp);                /* 2/ln2 */
    y  = p * (e + log2_m);
    yi = floorf(y);
    yf = y - yi;
    e2 = 1.5252734e-05f + yf * 1.3215487e-06f;
    e2 = 0.00015403530f + yf * e2;
    e2 = 0.0013333558f  + yf * e2;
    e2 = 0.009618129f   + yf * e2;
    e2 = 0.055504109f   + yf * e2;
    e2 = 0.24022651f    + yf * e2;
    e2 = 0.6931472f     + yf * e2;
    exp2f = 1.0f + yf * e2;
    yi_i = (int32_t)yi;
    sc.u = (uint32_t)(yi_i + 127) << 23;           /* 2^yi */
    return sc.f * exp2f;
}

#if ALWAN_HAS_SVML
ALWAN_INLINE alwan_f32 alwan_fast_pow24_f32(alwan_f32 x)     { return powf(x, 2.4f); }
ALWAN_INLINE alwan_f32 alwan_fast_pow_inv24_f32(alwan_f32 x) { return powf(x, 1.0f / 2.4f); }
#else
ALWAN_INLINE alwan_f32 alwan_fast_pow24_f32(alwan_f32 x)     { return alwan__fast_pow_pos_f32(x, 2.4f); }
ALWAN_INLINE alwan_f32 alwan_fast_pow_inv24_f32(alwan_f32 x) { return alwan__fast_pow_pos_f32(x, 1.0f / 2.4f); }
#endif

#endif /* ALWAN_FAST_POW_H */
