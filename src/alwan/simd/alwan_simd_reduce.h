/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_simd_reduce.h -- deterministic-aware horizontal reductions.
 *
 * Each backend exposes its native horizontal-sum / pairwise-add as
 * `*_native` (e.g., `alwan_simd_f32_hsum_native`). Native impls are
 * fast but use whatever lane order the SIMD ISA finds convenient,
 * which differs across SSE2 (4-lane tree), AVX (8-lane tree),
 * NEON (`vaddvq_*`, implementation-defined order), and the scalar
 * backend (identity).
 *
 * For determinism mode (`ALWAN_DETERMINISTIC=1`) the public names
 * defined here use a canonical left-to-right scalar reduction:
 *
 *     acc = lane[0]
 *     for i in 1..W:  acc = acc + lane[i]
 *
 * This is platform-independent and bit-exact across SSE/AVX/NEON/
 * scalar. The cost is the loss of native horizontal-add hardware
 * acceleration; reductions become the bottleneck instead of element-
 * wise math, but the compromise is intentional -- det mode trades
 * perf for reproducibility.
 *
 * Element-wise SIMD ops (per-lane add/mul/sqrt/...) are unaffected:
 * each lane runs the same op so width doesn't change the bit-exact
 * result.
 *
 * Include order: alwan_simd.h includes the chosen backend (which
 * defines the *_native ops), then includes this header which adds
 * the public dispatcher names. Call sites only ever use the public
 * names -- they should never reference the *_native suffix directly.
 *
 * See docs/determinism.md.
 */

#ifndef ALWAN_SIMD_REDUCE_H
#define ALWAN_SIMD_REDUCE_H

#include "alwan_simd_types.h"

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

/* Canonical scalar reductions -- bit-exact regardless of backend. */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum(alwan_simd_f32 a) {
    ALWAN_ALIGN(64) float lanes[ALWAN_SIMD_F32_WIDTH];
    alwan_simd_f32_storeu(lanes, a);
    float acc = lanes[0];
    for (int i = 1; i < ALWAN_SIMD_F32_WIDTH; i++) {
        acc = acc + lanes[i];
    }
    return alwan_simd_f32_set1(acc);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum(alwan_simd_f64 a) {
    ALWAN_ALIGN(64) double lanes[ALWAN_SIMD_F64_WIDTH];
    alwan_simd_f64_storeu(lanes, a);
    double acc = lanes[0];
    for (int i = 1; i < ALWAN_SIMD_F64_WIDTH; i++) {
        acc = acc + lanes[i];
    }
    return alwan_simd_f64_set1(acc);
}

/* Pairwise add (hadd) is harder to define canonically: the natural
 * deterministic answer is `[a0+a1, a2+a3, ..., b0+b1, b2+b3, ...]`,
 * but the actual lane assignment varies between SSE/AVX/NEON. For
 * determinism we materialize lanes, compute pairs in order, and
 * rebuild via load. */
ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) {
    ALWAN_ALIGN(64) float la[ALWAN_SIMD_F32_WIDTH];
    ALWAN_ALIGN(64) float lb[ALWAN_SIMD_F32_WIDTH];
    ALWAN_ALIGN(64) float out[ALWAN_SIMD_F32_WIDTH];
    alwan_simd_f32_storeu(la, a);
    alwan_simd_f32_storeu(lb, b);
    /* x86 _mm_hadd_ps interleaves a-pairs then b-pairs; we mirror
     * that layout deterministically. */
    int half = ALWAN_SIMD_F32_WIDTH / 2;
    for (int i = 0; i < half; i++) {
        out[i]        = la[2 * i] + la[2 * i + 1];
        out[half + i] = lb[2 * i] + lb[2 * i + 1];
    }
    return alwan_simd_f32_loadu(out);
}

#else /* !ALWAN_DETERMINISTIC -- fast path, forward to native. */

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hsum(alwan_simd_f32 a) {
    return alwan_simd_f32_hsum_native(a);
}

ALWAN_INLINE alwan_simd_f64 alwan_simd_f64_hsum(alwan_simd_f64 a) {
    return alwan_simd_f64_hsum_native(a);
}

ALWAN_INLINE alwan_simd_f32 alwan_simd_f32_hadd(alwan_simd_f32 a, alwan_simd_f32 b) {
    return alwan_simd_f32_hadd_native(a, b);
}

#endif /* ALWAN_DETERMINISTIC */

#endif /* ALWAN_SIMD_REDUCE_H */
