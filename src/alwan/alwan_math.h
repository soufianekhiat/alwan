/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * alwan_math.h — math function routing layer.
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
 * See road_to_determinism.md for the full plan.
 */

#ifndef ALWAN_MATH_H
#define ALWAN_MATH_H

#include "alwan_platform.h"

/* ----------------------------------------------------------------
 * Fused multiply-add
 *
 * In fast mode we use libm fma() / fmaf() — on x86 + -mfma and on all
 * aarch64, this lowers to a hardware FMA instruction (1 rounding,
 * faster, more accurate). On platforms without HW FMA, libm falls
 * back to a software emulation that is still 1-rounding (slow but
 * correctly rounded).
 *
 * In deterministic mode (future), we force `(a)*(b)+(c)` with
 * 2 roundings — bit-identical regardless of hardware FMA support.
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

# error "ALWAN_DETERMINISTIC=1 is not yet implemented. \
See road_to_determinism.md (workstream 3) for the plan. \
Set ALWAN_DETERMINISTIC=0 to use libm (fast mode)."

/* Future shape, for reference:
 *
 *   #include "core/alwan_deterministic.h"
 *
 *   #undef  ALWAN_POW
 *   #define ALWAN_POW(x, y)   alwan_det_pow_f64((x), (y))
 *   #undef  ALWAN_POW_F32
 *   #define ALWAN_POW_F32(x, y)  alwan_det_pow_f32((x), (y))
 *   #undef  ALWAN_POW_F64
 *   #define ALWAN_POW_F64(x, y)  alwan_det_pow_f64((x), (y))
 *
 *   ... same for EXP, LOG, LOG2, LOG10, LN, SIN, COS, TAN,
 *   ASIN, ACOS, ATAN, ATAN2, SQRT, CBRT, FMOD, SINH, COSH, TANH ...
 *
 *   #undef  ALWAN_FMA
 *   #define ALWAN_FMA(a, b, c)   ((a) * (b) + (c))   // forced 2-rounding
 *   #undef  ALWAN_FMAF
 *   #define ALWAN_FMAF(a, b, c)  ((a) * (b) + (c))
 */

#endif /* ALWAN_DETERMINISTIC */

#endif /* ALWAN_MATH_H */
