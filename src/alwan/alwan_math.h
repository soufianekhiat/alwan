/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_math.h -- math function routing layer.
 *
 * Single source of truth for math primitives. Every libm call in
 * src/alwan/ should route through one of these macros, never call
 * pow/exp/log/sin/cos directly.
 *
 * Three roles:
 *
 *   1. Wrap the existing ALWAN_POW / ALWAN_EXP / ... macros from
 *      alwan_platform.h with a public-facing umbrella so call sites
 *      have a stable include.
 *   2. Add FMA macros (ALWAN_FMA / ALWAN_FMAF) that didn't exist
 *      before, so deterministic mode can force 2-rounding.
 *   3. Provide a hook for ALWAN_DETERMINISTIC=1 to redirect every
 *      transcendental to a polynomial implementation in
 *      core/alwan_deterministic.h. While that file is being written,
 *      ALWAN_DETERMINISTIC=1 emits a compile error pointing here.
 *
 * See docs/determinism.md for the full plan.
 */

#ifndef ALWAN_MATH_H
#define ALWAN_MATH_H

#include "alwan_platform.h"

/* Scalar fast-mode pow(x,2.4)/pow(x,1/2.4) twins of the SIMD pow kernels, so
 * the scalar _v path matches the SIMD map path in fast mode on platforms
 * without an accurate vector pow (no SVML). Header-guarded; the inline
 * functions are unused (and thus not emitted) in deterministic mode. */
#include "core/alwan_fast_pow.h"

/* ----------------------------------------------------------------
 * Fused multiply-add
 *
 * In fast mode we use libm fma() / fmaf() -- on x86 + -mfma and on all
 * aarch64, this lowers to a hardware FMA instruction (1 rounding,
 * faster, more accurate). On platforms without HW FMA, libm falls
 * back to a software emulation that is still 1-rounding (slow but
 * correctly rounded).
 *
 * In deterministic mode (future), we force `(a)*(b)+(c)` with
 * 2 roundings -- bit-identical regardless of hardware FMA support.
 * The build also sets `-ffp-contract=off` (or `/fp:precise`) so the
 * compiler can't re-fuse it back into FMA behind our back.
 *
 * Today (fast mode only): libm path.
 * ---------------------------------------------------------------- */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
# include <math.h>
# define ALWAN_FMA(a, b, c)   fma((a), (b), (c))
# define ALWAN_FMAF(a, b, c)  fmaf((a), (b), (c))
#else
/* Shader backends already have FMA via the language (HLSL mad, GLSL
 * fma, Halide). The exact mapping depends on the compiler; we leave
 * the call as `(a)*(b)+(c)` and let the shader backend fuse if it
 * wants to. */
# define ALWAN_FMA(a, b, c)   ((a) * (b) + (c))
# define ALWAN_FMAF(a, b, c)  ((a) * (b) + (c))
#endif

/* ----------------------------------------------------------------
 * ALWAN_DETERMINISTIC routing hook
 *
 * When the user builds with -DALWAN_DETERMINISTIC=ON (CMake), the
 * macro ALWAN_DETERMINISTIC=1 is defined. This block will eventually
 * redirect every math macro to alwan_det_pow_f64 / alwan_det_exp_f64
 * etc. defined in core/alwan_deterministic.h.
 *
 * Until that header exists, ALWAN_DETERMINISTIC=1 is a hard error:
 * we'd rather fail to build than silently fall through to libm and
 * lie about determinism.
 * ---------------------------------------------------------------- */

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

/* alwan_det_* primitives:
 *   sRGB / BT.2020 OETF & EOTF      (Phase 1, 2)
 *   log2 / exp2 / pow_pos / cbrt    (Phase 2b infrastructure)
 *
 * Phase 3+ (PQ, HLG, ACES splines) compose pow_pos/exp/log; phase 5
 * (Lab/Oklab cube root) is alwan_det_cbrt directly. */
# include "core/alwan_deterministic.h"

/* Route fastmode pow / cbrt through deterministic positive-base
 * versions. Most callers clamp the base to >= 0 before calling, so
 * the "positive only" restriction matches existing usage. */
# undef  ALWAN_POW
# undef  ALWAN_POW_F32
# undef  ALWAN_POW_F64
# define ALWAN_POW(x, y)      alwan_det_pow_pos_f64((x), (y))
# define ALWAN_POW_F32(x, y)  alwan_det_pow_pos_f32((x), (y))
# define ALWAN_POW_F64(x, y)  alwan_det_pow_pos_f64((x), (y))

# undef  ALWAN_CBRT
# undef  ALWAN_CBRT_F32
# undef  ALWAN_CBRT_F64
# define ALWAN_CBRT(x)        alwan_det_cbrt_f64((x))
# define ALWAN_CBRT_F32(x)    alwan_det_cbrt_f32((x))
# define ALWAN_CBRT_F64(x)    alwan_det_cbrt_f64((x))

# undef  ALWAN_EXP
# undef  ALWAN_EXP_F32
# undef  ALWAN_EXP_F64
# define ALWAN_EXP(x)         alwan_det_exp_f64((x))
# define ALWAN_EXP_F32(x)     alwan_det_exp_f32((x))
# define ALWAN_EXP_F64(x)     alwan_det_exp_f64((x))

# undef  ALWAN_LN
# undef  ALWAN_LN_F32
# undef  ALWAN_LN_F64
# define ALWAN_LN(x)          alwan_det_log_f64((x))
# define ALWAN_LN_F32(x)      alwan_det_log_f32((x))
# define ALWAN_LN_F64(x)      alwan_det_log_f64((x))

# undef  ALWAN_LOG2
# undef  ALWAN_LOG2_F32
# undef  ALWAN_LOG2_F64
# define ALWAN_LOG2(x)        alwan_det_log2_f64((x))
# define ALWAN_LOG2_F32(x)    alwan_det_log2_f32((x))
# define ALWAN_LOG2_F64(x)    alwan_det_log2_f64((x))

/* exp2 has no top-level fast-mode macro; we add it now since the
 * det path makes it cheap to expose. */

/* Force 2-rounding multiply-add. The build also passes
 * `-ffp-contract=off` (clang/gcc) or `/fp:precise` (MSVC) so the
 * compiler can't re-fuse this back into hardware FMA. */
# undef  ALWAN_FMA
# undef  ALWAN_FMAF
# define ALWAN_FMA(a, b, c)   ((a) * (b) + (c))
# define ALWAN_FMAF(a, b, c)  ((a) * (b) + (c))

#endif /* ALWAN_DETERMINISTIC */

#endif /* ALWAN_MATH_H */
