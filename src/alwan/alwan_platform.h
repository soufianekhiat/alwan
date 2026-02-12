/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Platform Abstraction Layer
 * Single source of truth for backend detection, scalar type,
 * math macros, branchless SELECT, utility functions, and constants.
 *
 * Backends: 0 = C, 1 = HLSL, 2 = Halide
 */

#ifndef ALWAN_PLATFORM_H
#define ALWAN_PLATFORM_H

/* ================================================================
 * 1.1 Backend Detection
 * ================================================================ */

#define ALWAN_BACKEND_C      0
#define ALWAN_BACKEND_HLSL   1
#define ALWAN_BACKEND_HALIDE 2

#ifndef ALWAN_BACKEND
# if defined(__HLSL_VERSION)
#   define ALWAN_BACKEND ALWAN_BACKEND_HLSL
# elif defined(HALIDE_HALIDERUNTIME_H)
#   define ALWAN_BACKEND ALWAN_BACKEND_HALIDE
# else
#   define ALWAN_BACKEND ALWAN_BACKEND_C
# endif
#endif

/* ================================================================
 * 1.2 Scalar Type + Literals
 * ================================================================ */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
  /* C/C++ backend */
# ifndef ALWAN_SCALAR_IS_FLOAT
#   define ALWAN_SCALAR_IS_FLOAT 0  /* default: double for parity with Colour-Science (Python) Library */
# endif

# if ALWAN_SCALAR_IS_FLOAT
    typedef float  alwan_scalar;
#   define ALWAN_EPSILON 1e-6f
#   define ALWAN_LITERAL(x) x##f
# else
    typedef double alwan_scalar;
#   define ALWAN_EPSILON 1e-12
#   define ALWAN_LITERAL(x) x
# endif

#elif ALWAN_BACKEND == ALWAN_BACKEND_HLSL
  /* HLSL backend */
# define ALWAN_SCALAR_IS_FLOAT 1
  typedef float  alwan_scalar;
# define ALWAN_EPSILON 1e-6f
# define ALWAN_LITERAL(x) (x)

#elif ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
  /* Halide backend
   * ALWAN_SCALAR_IS_FLOAT selects Float(32) vs Float(64) precision. */
# include <Halide.h>
  typedef Halide::Expr alwan_scalar;
# ifndef ALWAN_SCALAR_IS_FLOAT
#   define ALWAN_SCALAR_IS_FLOAT 1  /* default: float for GPU pipelines */
# endif
# if ALWAN_SCALAR_IS_FLOAT
#   define ALWAN_EPSILON 1e-6f
#   define ALWAN_HALIDE_FLOAT_BITS 32
#   define ALWAN_LITERAL(x) Halide::Internal::make_const(Halide::Float(32), (x))
# else
#   define ALWAN_EPSILON 1e-12
#   define ALWAN_HALIDE_FLOAT_BITS 64
#   define ALWAN_LITERAL(x) Halide::Internal::make_const(Halide::Float(64), (x))
# endif

#endif

/* Common literal shortcuts */
#define ALWAN_ZERO ALWAN_LITERAL(0.0)
#define ALWAN_ONE  ALWAN_LITERAL(1.0)

/* ================================================================
 * 1.3 Qualifier Macros
 * ================================================================ */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
# define ALWAN_INLINE    static inline
# define ALWAN_CONSTEXPR static const
# define ALWAN_TYPE_DEF  typedef
#elif ALWAN_BACKEND == ALWAN_BACKEND_HLSL
# define ALWAN_INLINE    inline
# define ALWAN_CONSTEXPR static const
# define ALWAN_TYPE_DEF
#elif ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
# define ALWAN_INLINE    inline
# define ALWAN_CONSTEXPR static const
# define ALWAN_TYPE_DEF  typedef
#endif

/* ================================================================
 * 1.4 Math Macros
 * ================================================================ */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
  /* C/C++ backend: dispatch float vs double */
# include <math.h>
# if ALWAN_SCALAR_IS_FLOAT
#   define ALWAN_ABS(x)       fabsf(x)
#   define ALWAN_SQRT(x)      sqrtf(x)
#   define ALWAN_CBRT(x)      cbrtf(x)
#   define ALWAN_SIN(x)       sinf(x)
#   define ALWAN_COS(x)       cosf(x)
#   define ALWAN_TAN(x)       tanf(x)
#   define ALWAN_TANH(x)      tanhf(x)
#   define ALWAN_ATAN(x)      atanf(x)
#   define ALWAN_ACOS(x)      acosf(x)
#   define ALWAN_ATAN2(y, x)  atan2f(y, x)
#   define ALWAN_POW(x, y)    powf(x, y)
#   define ALWAN_EXP(x)       expf(x)
#   define ALWAN_LN(x)        logf(x)
#   define ALWAN_LOG2(x)      log2f(x)
#   define ALWAN_LOG10(x)     log10f(x)
#   define ALWAN_FLOOR(x)     floorf(x)
#   define ALWAN_CEIL(x)      ceilf(x)
#   define ALWAN_FMOD(x, y)   fmodf(x, y)
# else
#   define ALWAN_ABS(x)       fabs(x)
#   define ALWAN_SQRT(x)      sqrt(x)
#   define ALWAN_CBRT(x)      cbrt(x)
#   define ALWAN_SIN(x)       sin(x)
#   define ALWAN_COS(x)       cos(x)
#   define ALWAN_TAN(x)       tan(x)
#   define ALWAN_TANH(x)      tanh(x)
#   define ALWAN_ATAN(x)      atan(x)
#   define ALWAN_ACOS(x)      acos(x)
#   define ALWAN_ATAN2(y, x)  atan2(y, x)
#   define ALWAN_POW(x, y)    pow(x, y)
#   define ALWAN_EXP(x)       exp(x)
#   define ALWAN_LN(x)        log(x)
#   define ALWAN_LOG2(x)      log2(x)
#   define ALWAN_LOG10(x)     log10(x)
#   define ALWAN_FLOOR(x)     floor(x)
#   define ALWAN_CEIL(x)      ceil(x)
#   define ALWAN_FMOD(x, y)   fmod(x, y)
# endif

#elif ALWAN_BACKEND == ALWAN_BACKEND_HLSL
  /* HLSL backend: intrinsics */
# define ALWAN_ABS(x)       abs(x)
# define ALWAN_SQRT(x)      sqrt(x)
# define ALWAN_CBRT(x)      pow(x, 1.0f / 3.0f)
# define ALWAN_SIN(x)       sin(x)
# define ALWAN_COS(x)       cos(x)
# define ALWAN_TAN(x)       tan(x)
# define ALWAN_TANH(x)      tanh(x)
# define ALWAN_ATAN(x)      atan(x)
# define ALWAN_ACOS(x)      acos(x)
# define ALWAN_ATAN2(y, x)  atan2(y, x)
# define ALWAN_POW(x, y)    pow(x, y)
# define ALWAN_EXP(x)       exp(x)
# define ALWAN_LN(x)        log(x)
# define ALWAN_LOG2(x)      log2(x)
# define ALWAN_LOG10(x)     log10(x)
# define ALWAN_FLOOR(x)     floor(x)
# define ALWAN_CEIL(x)      ceil(x)
# define ALWAN_FMOD(x, y)   fmod(x, y)

#elif ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
  /* Halide backend: uses ALWAN_HALIDE_FLOAT_BITS for constant precision */
# define ALWAN_ABS(x)       Halide::abs(x)
# define ALWAN_SQRT(x)      Halide::sqrt(x)
# define ALWAN_CBRT(x)      Halide::pow(x, ALWAN_LITERAL(1.0 / 3.0))
# define ALWAN_SIN(x)       Halide::sin(x)
# define ALWAN_COS(x)       Halide::cos(x)
# define ALWAN_TAN(x)       Halide::tan(x)
# define ALWAN_TANH(x)      Halide::tanh(x)
# define ALWAN_ATAN(x)      Halide::atan(x)
# define ALWAN_ACOS(x)      Halide::acos(x)
# define ALWAN_ATAN2(y, x)  Halide::atan2(y, x)
# define ALWAN_POW(x, y)    Halide::pow(x, y)
# define ALWAN_EXP(x)       Halide::exp(x)
# define ALWAN_LN(x)        Halide::log(x)
# define ALWAN_LOG2(x)      (Halide::log(x) / Halide::log(ALWAN_LITERAL(2.0)))
# define ALWAN_LOG10(x)     (Halide::log(x) / Halide::log(ALWAN_LITERAL(10.0)))
# define ALWAN_FLOOR(x)     Halide::floor(x)
# define ALWAN_CEIL(x)      Halide::ceil(x)
# define ALWAN_FMOD(x, y)   ((x) - Halide::floor((x) / (y)) * (y))

#endif

/* ================================================================
 * 1.5 ALWAN_SELECT (branchless conditional)
 * ================================================================ */

#if ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
# define ALWAN_SELECT(cond, t, f) Halide::select((cond), (t), (f))
#else
# define ALWAN_SELECT(cond, t, f) ((cond) ? (t) : (f))
#endif

/* ================================================================
 * 1.6 Utility Functions
 * ================================================================ */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
  /* C/C++: inline functions (avoid double-evaluation) */

ALWAN_INLINE alwan_scalar alwan_min(alwan_scalar a, alwan_scalar b) {
    return (a < b) ? a : b;
}

ALWAN_INLINE alwan_scalar alwan_max(alwan_scalar a, alwan_scalar b) {
    return (a > b) ? a : b;
}

ALWAN_INLINE alwan_scalar alwan_min3(alwan_scalar a, alwan_scalar b, alwan_scalar c) {
    alwan_scalar m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

ALWAN_INLINE alwan_scalar alwan_max3(alwan_scalar a, alwan_scalar b, alwan_scalar c) {
    alwan_scalar m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}

ALWAN_INLINE alwan_scalar alwan_clamp(alwan_scalar x, alwan_scalar lo, alwan_scalar hi) {
    return (x < lo) ? lo : (x > hi) ? hi : x;
}

ALWAN_INLINE alwan_scalar alwan_saturate(alwan_scalar x) {
    if (x < ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    if (x > ALWAN_LITERAL(1.0)) return ALWAN_LITERAL(1.0);
    return x;
}

ALWAN_INLINE alwan_scalar alwan_lerp(alwan_scalar a, alwan_scalar b, alwan_scalar t) {
    return (ALWAN_LITERAL(1.0) - t) * a + t * b;
}

#elif ALWAN_BACKEND == ALWAN_BACKEND_HLSL
  /* HLSL: map to intrinsics */
# define alwan_min(a, b)       min(a, b)
# define alwan_max(a, b)       max(a, b)
# define alwan_min3(a, b, c)   min(min(a, b), c)
# define alwan_max3(a, b, c)   max(max(a, b), c)
# define alwan_clamp(x, lo, hi) clamp(x, lo, hi)
# define alwan_saturate(x)     saturate(x)
# define alwan_lerp(a, b, t)   lerp(a, b, t)

#elif ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
  /* Halide: map to Halide functions */
# define alwan_min(a, b)       Halide::min(a, b)
# define alwan_max(a, b)       Halide::max(a, b)
# define alwan_min3(a, b, c)   Halide::min(Halide::min(a, b), c)
# define alwan_max3(a, b, c)   Halide::max(Halide::max(a, b), c)
# define alwan_clamp(x, lo, hi) Halide::clamp(x, lo, hi)
# define alwan_saturate(x)     Halide::clamp(x, ALWAN_ZERO, ALWAN_ONE)
# define alwan_lerp(a, b, t)   Halide::lerp(a, b, t)

#endif

/* ================================================================
 * 1.7 Diagnostic Pragmas
 * ================================================================ */

#if ALWAN_BACKEND == ALWAN_BACKEND_C
  /* C/C++: compiler-specific pragma dispatch */
# if defined(_MSC_VER)
#   define ALWAN_DIAG_PUSH __pragma(warning(push))
#   define ALWAN_DIAG_POP  __pragma(warning(pop))
#   define ALWAN_DIAG_DISABLE_FLOAT_CONV __pragma(warning(disable: 4244 4305))
#   define ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC __pragma(warning(disable: 4211))
# elif defined(__clang__)
#   define ALWAN_DIAG_PUSH _Pragma("clang diagnostic push")
#   define ALWAN_DIAG_POP  _Pragma("clang diagnostic pop")
#   define ALWAN_DIAG_DISABLE_FLOAT_CONV \
      _Pragma("clang diagnostic ignored \"-Wimplicit-float-conversion\"") \
      _Pragma("clang diagnostic ignored \"-Wdouble-promotion\"")
#   define ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC /* no equivalent in clang */
# elif defined(__GNUC__)
#   define ALWAN_DIAG_PUSH _Pragma("GCC diagnostic push")
#   define ALWAN_DIAG_POP  _Pragma("GCC diagnostic pop")
#   define ALWAN_DIAG_DISABLE_FLOAT_CONV \
      _Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"") \
      _Pragma("GCC diagnostic ignored \"-Wdouble-promotion\"")
#   define ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC /* no equivalent in GCC */
# else
#   define ALWAN_DIAG_PUSH
#   define ALWAN_DIAG_POP
#   define ALWAN_DIAG_DISABLE_FLOAT_CONV
#   define ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC
# endif
#else
  /* HLSL/Halide: no diagnostic pragmas */
# define ALWAN_DIAG_PUSH
# define ALWAN_DIAG_POP
# define ALWAN_DIAG_DISABLE_FLOAT_CONV
# define ALWAN_DIAG_DISABLE_EXTERN_TO_STATIC
#endif

/* ================================================================
 * 1.8 Constants
 * ================================================================ */

/* Mathematical constants */
#if ALWAN_SCALAR_IS_FLOAT
# define ALWAN_PI ALWAN_LITERAL(3.14159265358979323846)
#else
# define ALWAN_PI ALWAN_LITERAL(3.14159265358979323846)
#endif

/* Standard illuminant D65 white point (Y=100 scale)
 * From colour-science: CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
 * xy = [0.31270, 0.32900] -> XYZ = [95.04559271, 100.00000000, 108.90577508] */
#define ALWAN_D65_X  ALWAN_LITERAL(95.04559271)
#define ALWAN_D65_Y  ALWAN_LITERAL(100.0)
#define ALWAN_D65_Z  ALWAN_LITERAL(108.90577508)

/* Test tolerance (depends on scalar precision) */
#if ALWAN_SCALAR_IS_FLOAT
# define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-5)
#else
# define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-12)
#endif

#endif /* ALWAN_PLATFORM_H */
