/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Platform Abstraction Layer
 * Backend detection, scalar type, math macros, branchless SELECT,
 * utility functions, and constants.
 *
 * Backends: 0 = C, 1 = HLSL, 2 = GLSL, 3 = Halide
 */

#ifndef ALWAN_PLATFORM_H
#define ALWAN_PLATFORM_H

/* ================================================================
 * 1.1 Backend Detection
 * ================================================================ */

#define ALWAN_BACKEND_C      0
#define ALWAN_BACKEND_HLSL   1
#define ALWAN_BACKEND_GLSL   2
#define ALWAN_BACKEND_HALIDE 3

#ifndef ALWAN_BACKEND
# if defined(__HLSL_VERSION)
#   define ALWAN_BACKEND ALWAN_BACKEND_HLSL
# elif defined(GL_core_profile) || defined(GL_es_profile)
#   define ALWAN_BACKEND ALWAN_BACKEND_GLSL
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

#elif ALWAN_BACKEND == ALWAN_BACKEND_GLSL
  /* GLSL backend */
# define ALWAN_SCALAR_IS_FLOAT 1
# define ALWAN_EPSILON 1e-6
# define ALWAN_LITERAL(x) (x)

#elif ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
  /* Halide backend
   * ALWAN_SCALAR_IS_FLOAT selects Float(32) vs Float(64) precision.
   *
   * alwan_halide_scalar inherits from Halide::Expr and adds an implicit
   * constructor from double so that bare numeric literals in CSV data files
   * (e.g. #include "data/matrices/oklab_m2.csv") can initialise
   * alwan_scalar arrays and structs without wrapping every value in
   * ALWAN_LITERAL(). */
# include <Halide.h>
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

  struct alwan_halide_scalar : Halide::Expr {
      using Halide::Expr::Expr;
      alwan_halide_scalar() = default;
      alwan_halide_scalar(double v)
          : Halide::Expr(Halide::Internal::make_const(
                Halide::Float(ALWAN_HALIDE_FLOAT_BITS), v)) {}
      alwan_halide_scalar(const Halide::Expr& e) : Halide::Expr(e) {}
  };
  typedef alwan_halide_scalar alwan_scalar;

#endif

/* ================================================================
 * 1.2.1 Integer Type Abstractions (C backend only)
 * ================================================================ */
#if ALWAN_BACKEND == ALWAN_BACKEND_C
# include <stdint.h>
  typedef uint16_t alwan_uint16;
  typedef uint8_t  alwan_uint8;
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
# define ALWAN_UNUSED(x) (void)(x)
#elif ALWAN_BACKEND == ALWAN_BACKEND_HLSL
# define ALWAN_INLINE    inline
# define ALWAN_CONSTEXPR static const
# define ALWAN_TYPE_DEF
# define ALWAN_UNUSED(x)
#elif ALWAN_BACKEND == ALWAN_BACKEND_GLSL
# define ALWAN_INLINE
# define ALWAN_CONSTEXPR const
# define ALWAN_TYPE_DEF
# define ALWAN_UNUSED(x)
#elif ALWAN_BACKEND == ALWAN_BACKEND_HALIDE
# define ALWAN_INLINE    inline
# define ALWAN_CONSTEXPR static const
# define ALWAN_TYPE_DEF  typedef
# define ALWAN_UNUSED(x) (void)(x)
#endif

/* ================================================================
 * 1.3.1 Parameter Passing Macros
 *
 * Abstraction for passing large types (mat3x3) and output params:
 *   C/Halide : const pointer for input, pointer for output + deref
 *   HLSL     : in/out qualifiers + direct access
 *
 * Usage:
 *   void foo(ALWAN_PARAM_MAT3_IN m, ALWAN_PARAM_SCALAR_OUT s) {
 *       alwan_scalar a = ALWAN_REF(m).m[0];
 *       ALWAN_REF(s) = a;
 *   }
 *   alwan_mat3x3 mat; alwan_scalar out;
 *   foo(ALWAN_ADDR(mat), ALWAN_ADDR(out));
 * ================================================================ */

#if ALWAN_BACKEND == ALWAN_BACKEND_HLSL || ALWAN_BACKEND == ALWAN_BACKEND_GLSL
  /* HLSL/GLSL: in/out qualifiers, direct member access */
# define ALWAN_PARAM_MAT3_IN    alwan_mat3x3
# define ALWAN_PARAM_MAT3_OUT   out alwan_mat3x3
# define ALWAN_PARAM_VEC3_OUT   out alwan_vec3
# define ALWAN_PARAM_SCALAR_OUT out alwan_scalar
# define ALWAN_REF(p)           (p)
# define ALWAN_ADDR(x)          (x)
#else
  /* C/Halide: pointer parameters, dereference access */
# define ALWAN_PARAM_MAT3_IN    alwan_mat3x3 const *
# define ALWAN_PARAM_MAT3_OUT   alwan_mat3x3 *
# define ALWAN_PARAM_VEC3_OUT   alwan_vec3 *
# define ALWAN_PARAM_SCALAR_OUT alwan_scalar *
# define ALWAN_REF(p)           (*(p))
# define ALWAN_ADDR(x)          (&(x))
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

#elif ALWAN_BACKEND == ALWAN_BACKEND_GLSL
  /* GLSL backend: intrinsics */
# define ALWAN_ABS(x)       abs(x)
# define ALWAN_SQRT(x)      sqrt(x)
# define ALWAN_CBRT(x)      pow(x, 1.0 / 3.0)
# define ALWAN_SIN(x)       sin(x)
# define ALWAN_COS(x)       cos(x)
# define ALWAN_TAN(x)       tan(x)
# define ALWAN_TANH(x)      tanh(x)
# define ALWAN_ATAN(x)      atan(x)
# define ALWAN_ACOS(x)      acos(x)
# define ALWAN_ATAN2(y, x)  atan(y, x)
# define ALWAN_POW(x, y)    pow(x, y)
# define ALWAN_EXP(x)       exp(x)
# define ALWAN_LN(x)        log(x)
# define ALWAN_LOG2(x)      log2(x)
# define ALWAN_LOG10(x)     (log(x) / log(10.0))
# define ALWAN_FLOOR(x)     floor(x)
# define ALWAN_CEIL(x)      ceil(x)
# define ALWAN_FMOD(x, y)   mod(x, y)

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

#elif ALWAN_BACKEND == ALWAN_BACKEND_GLSL
  /* GLSL: map to builtins */
# define alwan_min(a, b)       min(a, b)
# define alwan_max(a, b)       max(a, b)
# define alwan_min3(a, b, c)   min(min(a, b), c)
# define alwan_max3(a, b, c)   max(max(a, b), c)
# define alwan_clamp(x, lo, hi) clamp(x, lo, hi)
# define alwan_saturate(x)     clamp(x, 0.0, 1.0)
# define alwan_lerp(a, b, t)   mix(a, b, t)

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
  /* HLSL/GLSL/Halide: no diagnostic pragmas */
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
#define ALWAN_D65_x  ALWAN_LITERAL(0.31270)
#define ALWAN_D65_y  ALWAN_LITERAL(0.32900)
#define ALWAN_D65_X  ALWAN_LITERAL(95.04559271)
#define ALWAN_D65_Y  ALWAN_LITERAL(100.0)
#define ALWAN_D65_Z  ALWAN_LITERAL(108.90577508)

/* Standard RGB primaries (CIE xy chromaticity) */

/* ITU-R BT.709 / sRGB primaries */
#define ALWAN_BT709_RED_x     ALWAN_LITERAL(0.64)
#define ALWAN_BT709_RED_y     ALWAN_LITERAL(0.33)
#define ALWAN_BT709_GREEN_x   ALWAN_LITERAL(0.30)
#define ALWAN_BT709_GREEN_y   ALWAN_LITERAL(0.60)
#define ALWAN_BT709_BLUE_x    ALWAN_LITERAL(0.15)
#define ALWAN_BT709_BLUE_y    ALWAN_LITERAL(0.06)

/* ITU-R BT.2020 primaries */
#define ALWAN_BT2020_RED_x    ALWAN_LITERAL(0.708)
#define ALWAN_BT2020_RED_y    ALWAN_LITERAL(0.292)
#define ALWAN_BT2020_GREEN_x  ALWAN_LITERAL(0.170)
#define ALWAN_BT2020_GREEN_y  ALWAN_LITERAL(0.797)
#define ALWAN_BT2020_BLUE_x   ALWAN_LITERAL(0.131)
#define ALWAN_BT2020_BLUE_y   ALWAN_LITERAL(0.046)

/* Display P3 / P3-D65 primaries */
#define ALWAN_P3_RED_x        ALWAN_LITERAL(0.680)
#define ALWAN_P3_RED_y        ALWAN_LITERAL(0.320)
#define ALWAN_P3_GREEN_x      ALWAN_LITERAL(0.265)
#define ALWAN_P3_GREEN_y      ALWAN_LITERAL(0.690)
#define ALWAN_P3_BLUE_x       ALWAN_LITERAL(0.150)
#define ALWAN_P3_BLUE_y       ALWAN_LITERAL(0.060)

/* ACES AP1 / ACEScg primaries */
#define ALWAN_AP1_RED_x       ALWAN_LITERAL(0.713)
#define ALWAN_AP1_RED_y       ALWAN_LITERAL(0.293)
#define ALWAN_AP1_GREEN_x     ALWAN_LITERAL(0.165)
#define ALWAN_AP1_GREEN_y     ALWAN_LITERAL(0.830)
#define ALWAN_AP1_BLUE_x      ALWAN_LITERAL(0.128)
#define ALWAN_AP1_BLUE_y      ALWAN_LITERAL(0.044)

/* ITU luma coefficients (kr, kg, kb)
 * kg = 1 - kr - kb for each standard */
#define ALWAN_LUMA_KR_BT601   ALWAN_LITERAL(0.299)
#define ALWAN_LUMA_KG_BT601   ALWAN_LITERAL(0.587)
#define ALWAN_LUMA_KB_BT601   ALWAN_LITERAL(0.114)

#define ALWAN_LUMA_KR_BT709   ALWAN_LITERAL(0.2126)
#define ALWAN_LUMA_KG_BT709   ALWAN_LITERAL(0.7152)
#define ALWAN_LUMA_KB_BT709   ALWAN_LITERAL(0.0722)

#define ALWAN_LUMA_KR_BT2020  ALWAN_LITERAL(0.2627)
#define ALWAN_LUMA_KG_BT2020  ALWAN_LITERAL(0.6780)
#define ALWAN_LUMA_KB_BT2020  ALWAN_LITERAL(0.0593)

/* ACES AP1 luminance coefficients (Y row of AP1-to-XYZ matrix) */
#define ALWAN_LUMA_KR_AP1     ALWAN_LITERAL(0.27222871678091454)
#define ALWAN_LUMA_KG_AP1     ALWAN_LITERAL(0.67408176581114831)
#define ALWAN_LUMA_KB_AP1     ALWAN_LITERAL(0.053689517407937051)

/* ACES AP0 / ACES2065-1 luminance coefficients (Y row of AP0-to-XYZ matrix) */
#define ALWAN_LUMA_KR_AP0     ALWAN_LITERAL(0.34396644976507512181)
#define ALWAN_LUMA_KG_AP0     ALWAN_LITERAL(0.72816609661348574711)
#define ALWAN_LUMA_KB_AP0     ALWAN_LITERAL(-0.07213254637856078560)

/* Display P3 / P3-D65 luminance coefficients (Y row of Display P3-to-XYZ matrix) */
#define ALWAN_LUMA_KR_P3      ALWAN_LITERAL(0.22897456406974869836)
#define ALWAN_LUMA_KG_P3      ALWAN_LITERAL(0.69173852183650641479)
#define ALWAN_LUMA_KB_P3      ALWAN_LITERAL(0.07928691409374499879)

/* DCI-P3 luminance coefficients (Y row of DCI-P3-to-XYZ matrix) */
#define ALWAN_LUMA_KR_DCIP3   ALWAN_LITERAL(0.20949167791273051731)
#define ALWAN_LUMA_KG_DCIP3   ALWAN_LITERAL(0.72159525416104375317)
#define ALWAN_LUMA_KB_DCIP3   ALWAN_LITERAL(0.06891306792622581287)

/* Adobe RGB (1998) luminance coefficients (Y row of AdobeRGB-to-XYZ matrix) */
#define ALWAN_LUMA_KR_ADOBE   ALWAN_LITERAL(0.29734497525053604772)
#define ALWAN_LUMA_KG_ADOBE   ALWAN_LITERAL(0.62736356625546607635)
#define ALWAN_LUMA_KB_ADOBE   ALWAN_LITERAL(0.07529145849399787594)

/* ProPhoto RGB / ROMM RGB luminance coefficients (Y row of ProPhoto-to-XYZ matrix) */
#define ALWAN_LUMA_KR_PROPHOTO ALWAN_LITERAL(0.28807112822929331619)
#define ALWAN_LUMA_KG_PROPHOTO ALWAN_LITERAL(0.71184321781010140295)
#define ALWAN_LUMA_KB_PROPHOTO ALWAN_LITERAL(0.00008565396052593485)

/* sRGB transfer function constants (IEC 61966-2-1) */
#define ALWAN_SRGB_EOTF_THRESH  ALWAN_LITERAL(0.04045)
#define ALWAN_SRGB_OETF_THRESH  ALWAN_LITERAL(0.0031308)
#define ALWAN_SRGB_LINEAR_GAIN  ALWAN_LITERAL(12.92)
#define ALWAN_SRGB_GAMMA        ALWAN_LITERAL(2.4)
#define ALWAN_SRGB_A            ALWAN_LITERAL(1.055)
#define ALWAN_SRGB_B            ALWAN_LITERAL(0.055)

/* ================================================================
 * 1.9 Channel Range Constants
 *
 * Min/max defines for each color space channel with finite bounds.
 * Channels with unbounded ranges (e.g. Lab a*, b*) have no define.
 * Source of truth: docs/ranges.md
 * ================================================================ */

/* --- Device-Dependent Spaces --- */

/* RGB [0, 1] per channel (SDR in-gamut) */
#define ALWAN_RGB_R_MIN   ALWAN_ZERO
#define ALWAN_RGB_R_MAX   ALWAN_ONE
#define ALWAN_RGB_G_MIN   ALWAN_ZERO
#define ALWAN_RGB_G_MAX   ALWAN_ONE
#define ALWAN_RGB_B_MIN   ALWAN_ZERO
#define ALWAN_RGB_B_MAX   ALWAN_ONE

/* HSV [0, 1] per channel (hue normalized) */
#define ALWAN_HSV_H_MIN   ALWAN_ZERO
#define ALWAN_HSV_H_MAX   ALWAN_ONE
#define ALWAN_HSV_S_MIN   ALWAN_ZERO
#define ALWAN_HSV_S_MAX   ALWAN_ONE
#define ALWAN_HSV_V_MIN   ALWAN_ZERO
#define ALWAN_HSV_V_MAX   ALWAN_ONE

/* HSL [0, 1] per channel (hue normalized) */
#define ALWAN_HSL_H_MIN   ALWAN_ZERO
#define ALWAN_HSL_H_MAX   ALWAN_ONE
#define ALWAN_HSL_S_MIN   ALWAN_ZERO
#define ALWAN_HSL_S_MAX   ALWAN_ONE
#define ALWAN_HSL_L_MIN   ALWAN_ZERO
#define ALWAN_HSL_L_MAX   ALWAN_ONE

/* HSP [0, 1] per channel */
#define ALWAN_HSP_H_MIN   ALWAN_ZERO
#define ALWAN_HSP_H_MAX   ALWAN_ONE
#define ALWAN_HSP_S_MIN   ALWAN_ZERO
#define ALWAN_HSP_S_MAX   ALWAN_ONE
#define ALWAN_HSP_P_MIN   ALWAN_ZERO
#define ALWAN_HSP_P_MAX   ALWAN_ONE

/* HSPLog [0, 1] per channel (log-stretched saturation) */
#define ALWAN_HSPLOG_H_MIN ALWAN_ZERO
#define ALWAN_HSPLOG_H_MAX ALWAN_ONE
#define ALWAN_HSPLOG_S_MIN ALWAN_ZERO
#define ALWAN_HSPLOG_S_MAX ALWAN_ONE
#define ALWAN_HSPLOG_P_MIN ALWAN_ZERO
#define ALWAN_HSPLOG_P_MAX ALWAN_ONE

/* HSY [0, 1] per channel */
#define ALWAN_HSY_H_MIN   ALWAN_ZERO
#define ALWAN_HSY_H_MAX   ALWAN_ONE
#define ALWAN_HSY_S_MIN   ALWAN_ZERO
#define ALWAN_HSY_S_MAX   ALWAN_ONE
#define ALWAN_HSY_Y_MIN   ALWAN_ZERO
#define ALWAN_HSY_Y_MAX   ALWAN_ONE

/* HWB [0, 1] per channel */
#define ALWAN_HWB_H_MIN   ALWAN_ZERO
#define ALWAN_HWB_H_MAX   ALWAN_ONE
#define ALWAN_HWB_W_MIN   ALWAN_ZERO
#define ALWAN_HWB_W_MAX   ALWAN_ONE
#define ALWAN_HWB_B_MIN   ALWAN_ZERO
#define ALWAN_HWB_B_MAX   ALWAN_ONE

/* CMY [0, 1] per channel */
#define ALWAN_CMY_C_MIN   ALWAN_ZERO
#define ALWAN_CMY_C_MAX   ALWAN_ONE
#define ALWAN_CMY_M_MIN   ALWAN_ZERO
#define ALWAN_CMY_M_MAX   ALWAN_ONE
#define ALWAN_CMY_Y_MIN   ALWAN_ZERO
#define ALWAN_CMY_Y_MAX   ALWAN_ONE

/* CMYK [0, 1] per channel */
#define ALWAN_CMYK_C_MIN  ALWAN_ZERO
#define ALWAN_CMYK_C_MAX  ALWAN_ONE
#define ALWAN_CMYK_M_MIN  ALWAN_ZERO
#define ALWAN_CMYK_M_MAX  ALWAN_ONE
#define ALWAN_CMYK_Y_MIN  ALWAN_ZERO
#define ALWAN_CMYK_Y_MAX  ALWAN_ONE
#define ALWAN_CMYK_K_MIN  ALWAN_ZERO
#define ALWAN_CMYK_K_MAX  ALWAN_ONE

/* YCbCr: Y [0,1], Cb/Cr [-0.5, 0.5] */
#define ALWAN_YCBCR_Y_MIN  ALWAN_ZERO
#define ALWAN_YCBCR_Y_MAX  ALWAN_ONE
#define ALWAN_YCBCR_CB_MIN ALWAN_LITERAL(-0.5)
#define ALWAN_YCBCR_CB_MAX ALWAN_LITERAL(0.5)
#define ALWAN_YCBCR_CR_MIN ALWAN_LITERAL(-0.5)
#define ALWAN_YCBCR_CR_MAX ALWAN_LITERAL(0.5)

/* YCoCg: Y [0,1], Co/Cg [-0.5, 0.5] */
#define ALWAN_YCOCG_Y_MIN  ALWAN_ZERO
#define ALWAN_YCOCG_Y_MAX  ALWAN_ONE
#define ALWAN_YCOCG_CO_MIN ALWAN_LITERAL(-0.5)
#define ALWAN_YCOCG_CO_MAX ALWAN_LITERAL(0.5)
#define ALWAN_YCOCG_CG_MIN ALWAN_LITERAL(-0.5)
#define ALWAN_YCOCG_CG_MAX ALWAN_LITERAL(0.5)

/* YcCbcCrc: Yc [0,1], Cbc/Crc [-0.5, 0.5] */
#define ALWAN_YCCBCCRC_YC_MIN  ALWAN_ZERO
#define ALWAN_YCCBCCRC_YC_MAX  ALWAN_ONE
#define ALWAN_YCCBCCRC_CBC_MIN ALWAN_LITERAL(-0.5)
#define ALWAN_YCCBCCRC_CBC_MAX ALWAN_LITERAL(0.5)
#define ALWAN_YCCBCCRC_CRC_MIN ALWAN_LITERAL(-0.5)
#define ALWAN_YCCBCCRC_CRC_MAX ALWAN_LITERAL(0.5)

/* Prismatic [0, 1] per channel */
#define ALWAN_PRISMATIC_L_MIN ALWAN_ZERO
#define ALWAN_PRISMATIC_L_MAX ALWAN_ONE
#define ALWAN_PRISMATIC_S_MIN ALWAN_ZERO
#define ALWAN_PRISMATIC_S_MAX ALWAN_ONE
#define ALWAN_PRISMATIC_H_MIN ALWAN_ZERO
#define ALWAN_PRISMATIC_H_MAX ALWAN_ONE

/* IHLS: H [0, 2pi), L [0,1], S [0,1] */
#define ALWAN_IHLS_H_MIN  ALWAN_ZERO
#define ALWAN_IHLS_H_MAX  (ALWAN_LITERAL(2.0) * ALWAN_PI)
#define ALWAN_IHLS_L_MIN  ALWAN_ZERO
#define ALWAN_IHLS_L_MAX  ALWAN_ONE
#define ALWAN_IHLS_S_MIN  ALWAN_ZERO
#define ALWAN_IHLS_S_MAX  ALWAN_ONE

/* --- CIE Colorimetric Spaces --- */

/* XYZ: all [0, +inf) — only MIN defined */
#define ALWAN_XYZ_X_MIN   ALWAN_ZERO
#define ALWAN_XYZ_Y_MIN   ALWAN_ZERO
#define ALWAN_XYZ_Z_MIN   ALWAN_ZERO

/* xyY: x,y [0,1], Y [0, +inf) */
#define ALWAN_XYY_X_MIN   ALWAN_ZERO
#define ALWAN_XYY_X_MAX   ALWAN_ONE
#define ALWAN_XYY_Y_MIN   ALWAN_ZERO
#define ALWAN_XYY_Y_MAX   ALWAN_ONE
#define ALWAN_XYY_YL_MIN  ALWAN_ZERO  /* Y (luminance), no upper bound */

/* Lab: L [0, 100], a/b unbounded */
#define ALWAN_LAB_L_MIN   ALWAN_ZERO
#define ALWAN_LAB_L_MAX   ALWAN_LITERAL(100.0)

/* Luv: L [0, 100], u/v unbounded */
#define ALWAN_LUV_L_MIN   ALWAN_ZERO
#define ALWAN_LUV_L_MAX   ALWAN_LITERAL(100.0)

/* LCh(ab): L [0, 100], C [0, +inf), h [0, 360) */
#define ALWAN_LCH_L_MIN   ALWAN_ZERO
#define ALWAN_LCH_L_MAX   ALWAN_LITERAL(100.0)
#define ALWAN_LCH_C_MIN   ALWAN_ZERO
#define ALWAN_LCH_H_MIN   ALWAN_ZERO
#define ALWAN_LCH_H_MAX   ALWAN_LITERAL(360.0)

/* LCh(uv): L [0, 100], C [0, +inf), h [0, 360) */
#define ALWAN_LCHUV_L_MIN ALWAN_ZERO
#define ALWAN_LCHUV_L_MAX ALWAN_LITERAL(100.0)
#define ALWAN_LCHUV_C_MIN ALWAN_ZERO
#define ALWAN_LCHUV_H_MIN ALWAN_ZERO
#define ALWAN_LCHUV_H_MAX ALWAN_LITERAL(360.0)

/* UCS (CIE 1960): U,V [0,1], W [0, +inf) */
#define ALWAN_UCS_U_MIN   ALWAN_ZERO
#define ALWAN_UCS_U_MAX   ALWAN_ONE
#define ALWAN_UCS_V_MIN   ALWAN_ZERO
#define ALWAN_UCS_V_MAX   ALWAN_ONE
#define ALWAN_UCS_W_MIN   ALWAN_ZERO

/* UVW (CIE 1964): U,V unbounded, W [0, +inf) */
#define ALWAN_UVW_W_MIN   ALWAN_ZERO

/* Hunter Lab: L [0, 100], a/b unbounded */
#define ALWAN_HUNTER_LAB_L_MIN ALWAN_ZERO
#define ALWAN_HUNTER_LAB_L_MAX ALWAN_LITERAL(100.0)

/* DIN99: L99 [0, 100], a99/b99 unbounded */
#define ALWAN_DIN99_L_MIN ALWAN_ZERO
#define ALWAN_DIN99_L_MAX ALWAN_LITERAL(100.0)

/* --- Modern Perceptual Spaces --- */

/* Oklab: L [0, 1], a/b unbounded */
#define ALWAN_OKLAB_L_MIN ALWAN_ZERO
#define ALWAN_OKLAB_L_MAX ALWAN_ONE

/* Oklch: L [0, 1], C [0, +inf), h [-pi, pi] */
#define ALWAN_OKLCH_L_MIN ALWAN_ZERO
#define ALWAN_OKLCH_L_MAX ALWAN_ONE
#define ALWAN_OKLCH_C_MIN ALWAN_ZERO
#define ALWAN_OKLCH_H_MIN (-ALWAN_PI)
#define ALWAN_OKLCH_H_MAX ALWAN_PI

/* Jzazbz: Jz [0, 1], az/bz unbounded */
#define ALWAN_JZAZBZ_JZ_MIN ALWAN_ZERO
#define ALWAN_JZAZBZ_JZ_MAX ALWAN_ONE

/* JzCzhz: Jz [0, 1], Cz [0, +inf), hz [-pi, pi] */
#define ALWAN_JZCZHZ_JZ_MIN ALWAN_ZERO
#define ALWAN_JZCZHZ_JZ_MAX ALWAN_ONE
#define ALWAN_JZCZHZ_CZ_MIN ALWAN_ZERO
#define ALWAN_JZCZHZ_HZ_MIN (-ALWAN_PI)
#define ALWAN_JZCZHZ_HZ_MAX ALWAN_PI

/* ICtCp: I [0, 1], Ct/Cp unbounded */
#define ALWAN_ICTCP_I_MIN ALWAN_ZERO
#define ALWAN_ICTCP_I_MAX ALWAN_ONE

/* IPT: I [0, 1], P/T unbounded */
#define ALWAN_IPT_I_MIN   ALWAN_ZERO
#define ALWAN_IPT_I_MAX   ALWAN_ONE

/* IPTch: I [0, 1], C [0, +inf), h [-pi, pi] */
#define ALWAN_IPTCH_I_MIN ALWAN_ZERO
#define ALWAN_IPTCH_I_MAX ALWAN_ONE
#define ALWAN_IPTCH_C_MIN ALWAN_ZERO
#define ALWAN_IPTCH_H_MIN (-ALWAN_PI)
#define ALWAN_IPTCH_H_MAX ALWAN_PI

/* IgPgTg: Ig [0, 1], Pg/Tg unbounded */
#define ALWAN_IGPGTG_IG_MIN ALWAN_ZERO
#define ALWAN_IGPGTG_IG_MAX ALWAN_ONE

/* ICaCb: I [0, +inf), Ca/Cb unbounded */
#define ALWAN_ICACB_I_MIN ALWAN_ZERO

/* ProLab: L [0, 100], a/b unbounded */
#define ALWAN_PROLAB_L_MIN ALWAN_ZERO
#define ALWAN_PROLAB_L_MAX ALWAN_LITERAL(100.0)

/* HCL: H [-pi, pi], C [0, +inf), L [0, 1] */
#define ALWAN_HCL_H_MIN  (-ALWAN_PI)
#define ALWAN_HCL_H_MAX  ALWAN_PI
#define ALWAN_HCL_C_MIN  ALWAN_ZERO
#define ALWAN_HCL_L_MIN  ALWAN_ZERO
#define ALWAN_HCL_L_MAX  ALWAN_ONE

/* --- Color Appearance Models --- */

/* CIECAM02: J [0,100], C [0,+inf), h [0,360), H [0,400] */
#define ALWAN_CIECAM02_J_MIN ALWAN_ZERO
#define ALWAN_CIECAM02_J_MAX ALWAN_LITERAL(100.0)
#define ALWAN_CIECAM02_C_MIN ALWAN_ZERO
#define ALWAN_CIECAM02_H_MIN ALWAN_ZERO
#define ALWAN_CIECAM02_H_MAX ALWAN_LITERAL(360.0)
#define ALWAN_CIECAM02_S_MIN ALWAN_ZERO
#define ALWAN_CIECAM02_Q_MIN ALWAN_ZERO
#define ALWAN_CIECAM02_M_MIN ALWAN_ZERO
#define ALWAN_CIECAM02_HQ_MIN ALWAN_ZERO
#define ALWAN_CIECAM02_HQ_MAX ALWAN_LITERAL(400.0)

/* CAM16: J [0,100], C [0,+inf), h [0,360), H [0,400] */
#define ALWAN_CAM16_J_MIN ALWAN_ZERO
#define ALWAN_CAM16_J_MAX ALWAN_LITERAL(100.0)
#define ALWAN_CAM16_C_MIN ALWAN_ZERO
#define ALWAN_CAM16_H_MIN ALWAN_ZERO
#define ALWAN_CAM16_H_MAX ALWAN_LITERAL(360.0)
#define ALWAN_CAM16_S_MIN ALWAN_ZERO
#define ALWAN_CAM16_Q_MIN ALWAN_ZERO
#define ALWAN_CAM16_M_MIN ALWAN_ZERO
#define ALWAN_CAM16_HQ_MIN ALWAN_ZERO
#define ALWAN_CAM16_HQ_MAX ALWAN_LITERAL(400.0)

/* ZCAM: Jz [0,100], hz [0,360), Kz [0,100], Wz [0,100] */
#define ALWAN_ZCAM_JZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_JZ_MAX ALWAN_LITERAL(100.0)
#define ALWAN_ZCAM_CZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_HZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_HZ_MAX ALWAN_LITERAL(360.0)
#define ALWAN_ZCAM_QZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_MZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_SZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_VZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_KZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_KZ_MAX ALWAN_LITERAL(100.0)
#define ALWAN_ZCAM_WZ_MIN ALWAN_ZERO
#define ALWAN_ZCAM_WZ_MAX ALWAN_LITERAL(100.0)

/* Hellwig2022: J [0,100], h [0,360) */
#define ALWAN_HELLWIG2022_J_MIN ALWAN_ZERO
#define ALWAN_HELLWIG2022_J_MAX ALWAN_LITERAL(100.0)
#define ALWAN_HELLWIG2022_C_MIN ALWAN_ZERO
#define ALWAN_HELLWIG2022_H_MIN ALWAN_ZERO
#define ALWAN_HELLWIG2022_H_MAX ALWAN_LITERAL(360.0)
#define ALWAN_HELLWIG2022_S_MIN ALWAN_ZERO
#define ALWAN_HELLWIG2022_Q_MIN ALWAN_ZERO
#define ALWAN_HELLWIG2022_M_MIN ALWAN_ZERO

/* Hunt: J [0,100], h [0,360) */
#define ALWAN_HUNT_J_MIN  ALWAN_ZERO
#define ALWAN_HUNT_J_MAX  ALWAN_LITERAL(100.0)
#define ALWAN_HUNT_C_MIN  ALWAN_ZERO
#define ALWAN_HUNT_H_MIN  ALWAN_ZERO
#define ALWAN_HUNT_H_MAX  ALWAN_LITERAL(360.0)
#define ALWAN_HUNT_S_MIN  ALWAN_ZERO
#define ALWAN_HUNT_Q_MIN  ALWAN_ZERO
#define ALWAN_HUNT_M_MIN  ALWAN_ZERO

/* Kim2009: J [0,100], h [0,360) */
#define ALWAN_KIM2009_J_MIN ALWAN_ZERO
#define ALWAN_KIM2009_J_MAX ALWAN_LITERAL(100.0)
#define ALWAN_KIM2009_C_MIN ALWAN_ZERO
#define ALWAN_KIM2009_H_MIN ALWAN_ZERO
#define ALWAN_KIM2009_H_MAX ALWAN_LITERAL(360.0)

/* LLAB: L [0,100], h [0,360) */
#define ALWAN_LLAB_L_MIN  ALWAN_ZERO
#define ALWAN_LLAB_L_MAX  ALWAN_LITERAL(100.0)
#define ALWAN_LLAB_CH_MIN ALWAN_ZERO
#define ALWAN_LLAB_H_MIN  ALWAN_ZERO
#define ALWAN_LLAB_H_MAX  ALWAN_LITERAL(360.0)
#define ALWAN_LLAB_S_MIN  ALWAN_ZERO

/* ATD95: H [0,360) */
#define ALWAN_ATD95_H_MIN ALWAN_ZERO
#define ALWAN_ATD95_H_MAX ALWAN_LITERAL(360.0)
#define ALWAN_ATD95_C_MIN ALWAN_ZERO
#define ALWAN_ATD95_BR_MIN ALWAN_ZERO

/* RLAB: L [0,100], h [0,360) */
#define ALWAN_RLAB_L_MIN  ALWAN_ZERO
#define ALWAN_RLAB_L_MAX  ALWAN_LITERAL(100.0)
#define ALWAN_RLAB_C_MIN  ALWAN_ZERO
#define ALWAN_RLAB_H_MIN  ALWAN_ZERO
#define ALWAN_RLAB_H_MAX  ALWAN_LITERAL(360.0)
#define ALWAN_RLAB_S_MIN  ALWAN_ZERO

/* Nayatani95: L*N [0,100], theta [0, 2pi) */
#define ALWAN_NAYATANI95_L_MIN     ALWAN_ZERO
#define ALWAN_NAYATANI95_L_MAX     ALWAN_LITERAL(100.0)
#define ALWAN_NAYATANI95_C_MIN     ALWAN_ZERO
#define ALWAN_NAYATANI95_THETA_MIN ALWAN_ZERO
#define ALWAN_NAYATANI95_THETA_MAX (ALWAN_LITERAL(2.0) * ALWAN_PI)
#define ALWAN_NAYATANI95_S_MIN     ALWAN_ZERO
#define ALWAN_NAYATANI95_BR_MIN    ALWAN_ZERO

/* ================================================================
 * 1.10 Range Normalization
 *
 * When ALWAN_NORMALIZE_RANGES is 1, API functions rescale bounded
 * output channels to [0, 1] and expect [0, 1] inputs for those
 * channels. Core (_v) functions are NOT affected.
 * Unbounded channels (e.g. Lab a*, chroma) are NOT rescaled.
 * Default: 0 (disabled — original mathematical ranges).
 * ================================================================ */

#ifndef ALWAN_NORMALIZE_RANGES
# define ALWAN_NORMALIZE_RANGES 1
#endif

#define ALWAN__TWOPI (ALWAN_LITERAL(2.0) * ALWAN_PI)

#if ALWAN_NORMALIZE_RANGES

/* Lab: L [0,100] -> [0,1] */
#define ALWAN_NORM_LAB(p)   do { (p)->L *= ALWAN_LITERAL(0.01); } while(0)
#define ALWAN_DENORM_LAB(p) do { (p)->L *= ALWAN_LITERAL(100.0); } while(0)

/* Luv: L [0,100] -> [0,1] */
#define ALWAN_NORM_LUV(p)   do { (p)->L *= ALWAN_LITERAL(0.01); } while(0)
#define ALWAN_DENORM_LUV(p) do { (p)->L *= ALWAN_LITERAL(100.0); } while(0)

/* LCh(ab): L [0,100] -> [0,1], h [0,360) -> [0,1] */
#define ALWAN_NORM_LCH(p)   do { (p)->L *= ALWAN_LITERAL(0.01); (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_LCH(p) do { (p)->L *= ALWAN_LITERAL(100.0); (p)->h *= ALWAN_LITERAL(360.0); } while(0)

/* LCh(uv): L [0,100] -> [0,1], h [0,360) -> [0,1] */
#define ALWAN_NORM_LCHUV(p)   do { (p)->L *= ALWAN_LITERAL(0.01); (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_LCHUV(p) do { (p)->L *= ALWAN_LITERAL(100.0); (p)->h *= ALWAN_LITERAL(360.0); } while(0)

/* Oklch: h [-pi, pi] -> [0,1] (L already [0,1]) */
#define ALWAN_NORM_OKLCH(p)   do { (p)->h = ((p)->h + ALWAN_PI) / ALWAN__TWOPI; } while(0)
#define ALWAN_DENORM_OKLCH(p) do { (p)->h = (p)->h * ALWAN__TWOPI - ALWAN_PI; } while(0)

/* JzCzhz: hz [-pi, pi] -> [0,1] (Jz already [0,1]) */
#define ALWAN_NORM_JZCZHZ(p)   do { (p)->hz = ((p)->hz + ALWAN_PI) / ALWAN__TWOPI; } while(0)
#define ALWAN_DENORM_JZCZHZ(p) do { (p)->hz = (p)->hz * ALWAN__TWOPI - ALWAN_PI; } while(0)

/* IPTch: h [-pi, pi] -> [0,1] (I already [0,1]) */
#define ALWAN_NORM_IPTCH(p)   do { (p)->h = ((p)->h + ALWAN_PI) / ALWAN__TWOPI; } while(0)
#define ALWAN_DENORM_IPTCH(p) do { (p)->h = (p)->h * ALWAN__TWOPI - ALWAN_PI; } while(0)

/* YCbCr: Cb [-0.5,0.5] -> [0,1], Cr [-0.5,0.5] -> [0,1] */
#define ALWAN_NORM_YCBCR(p)   do { (p)->Cb += ALWAN_LITERAL(0.5); (p)->Cr += ALWAN_LITERAL(0.5); } while(0)
#define ALWAN_DENORM_YCBCR(p) do { (p)->Cb -= ALWAN_LITERAL(0.5); (p)->Cr -= ALWAN_LITERAL(0.5); } while(0)

/* YCoCg: Co [-0.5,0.5] -> [0,1], Cg [-0.5,0.5] -> [0,1] */
#define ALWAN_NORM_YCOCG(p)   do { (p)->Co += ALWAN_LITERAL(0.5); (p)->Cg += ALWAN_LITERAL(0.5); } while(0)
#define ALWAN_DENORM_YCOCG(p) do { (p)->Co -= ALWAN_LITERAL(0.5); (p)->Cg -= ALWAN_LITERAL(0.5); } while(0)

/* YcCbcCrc: Cbc [-0.5,0.5] -> [0,1], Crc [-0.5,0.5] -> [0,1] */
#define ALWAN_NORM_YCCBCCRC(p)   do { (p)->Cbc += ALWAN_LITERAL(0.5); (p)->Crc += ALWAN_LITERAL(0.5); } while(0)
#define ALWAN_DENORM_YCCBCCRC(p) do { (p)->Cbc -= ALWAN_LITERAL(0.5); (p)->Crc -= ALWAN_LITERAL(0.5); } while(0)

/* HCL: H [-pi, pi] -> [0,1] (L already [0,1]) */
#define ALWAN_NORM_HCL(p)   do { (p)->H = ((p)->H + ALWAN_PI) / ALWAN__TWOPI; } while(0)
#define ALWAN_DENORM_HCL(p) do { (p)->H = (p)->H * ALWAN__TWOPI - ALWAN_PI; } while(0)

/* IHLS: H [0, 2pi) -> [0,1] */
#define ALWAN_NORM_IHLS(p)   do { (p)->H /= ALWAN__TWOPI; } while(0)
#define ALWAN_DENORM_IHLS(p) do { (p)->H *= ALWAN__TWOPI; } while(0)

/* DIN99: L99 [0,100] -> [0,1] */
#define ALWAN_NORM_DIN99(p)   do { (p)->L99 *= ALWAN_LITERAL(0.01); } while(0)
#define ALWAN_DENORM_DIN99(p) do { (p)->L99 *= ALWAN_LITERAL(100.0); } while(0)

/* Hunter Lab: L [0,100] -> [0,1] */
#define ALWAN_NORM_HUNTER_LAB(p)   do { (p)->L *= ALWAN_LITERAL(0.01); } while(0)
#define ALWAN_DENORM_HUNTER_LAB(p) do { (p)->L *= ALWAN_LITERAL(100.0); } while(0)

/* ProLab: L [0,100] -> [0,1] */
#define ALWAN_NORM_PROLAB(p)   do { (p)->L *= ALWAN_LITERAL(0.01); } while(0)
#define ALWAN_DENORM_PROLAB(p) do { (p)->L *= ALWAN_LITERAL(100.0); } while(0)

/* CAM Jab (UCS): J [0,100] -> [0,1] */
#define ALWAN_NORM_CAM_JAB(p)   do { (p)->J *= ALWAN_LITERAL(0.01); } while(0)
#define ALWAN_DENORM_CAM_JAB(p) do { (p)->J *= ALWAN_LITERAL(100.0); } while(0)

/* CIECAM02: J [0,100] -> [0,1], h [0,360) -> [0,1], H [0,400] -> [0,1] */
#define ALWAN_NORM_CIECAM02(p)   do { (p)->J *= ALWAN_LITERAL(0.01); \
    (p)->h /= ALWAN_LITERAL(360.0); (p)->H /= ALWAN_LITERAL(400.0); } while(0)
#define ALWAN_DENORM_CIECAM02(p) do { (p)->J *= ALWAN_LITERAL(100.0); \
    (p)->h *= ALWAN_LITERAL(360.0); (p)->H *= ALWAN_LITERAL(400.0); } while(0)

/* CAM16: J [0,100] -> [0,1], h [0,360) -> [0,1], H [0,400] -> [0,1] */
#define ALWAN_NORM_CAM16(p)   do { (p)->J *= ALWAN_LITERAL(0.01); \
    (p)->h /= ALWAN_LITERAL(360.0); (p)->H /= ALWAN_LITERAL(400.0); } while(0)
#define ALWAN_DENORM_CAM16(p) do { (p)->J *= ALWAN_LITERAL(100.0); \
    (p)->h *= ALWAN_LITERAL(360.0); (p)->H *= ALWAN_LITERAL(400.0); } while(0)

/* ZCAM: Jz [0,100] -> [0,1], hz [0,360) -> [0,1], Kz [0,100] -> [0,1], Wz [0,100] -> [0,1] */
#define ALWAN_NORM_ZCAM(p)   do { (p)->Jz *= ALWAN_LITERAL(0.01); \
    (p)->hz /= ALWAN_LITERAL(360.0); (p)->Kz *= ALWAN_LITERAL(0.01); \
    (p)->Wz *= ALWAN_LITERAL(0.01); } while(0)
#define ALWAN_DENORM_ZCAM(p) do { (p)->Jz *= ALWAN_LITERAL(100.0); \
    (p)->hz *= ALWAN_LITERAL(360.0); (p)->Kz *= ALWAN_LITERAL(100.0); \
    (p)->Wz *= ALWAN_LITERAL(100.0); } while(0)

/* Hellwig2022: J [0,100] -> [0,1], h [0,360) -> [0,1], H [0,400] -> [0,1] */
#define ALWAN_NORM_HELLWIG2022(p)   do { (p)->J *= ALWAN_LITERAL(0.01); \
    (p)->h /= ALWAN_LITERAL(360.0); (p)->H /= ALWAN_LITERAL(400.0); } while(0)
#define ALWAN_DENORM_HELLWIG2022(p) do { (p)->J *= ALWAN_LITERAL(100.0); \
    (p)->h *= ALWAN_LITERAL(360.0); (p)->H *= ALWAN_LITERAL(400.0); } while(0)

/* Hunt: J [0,100] -> [0,1], h [0,360) -> [0,1] */
#define ALWAN_NORM_HUNT(p)   do { (p)->J *= ALWAN_LITERAL(0.01); (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_HUNT(p) do { (p)->J *= ALWAN_LITERAL(100.0); (p)->h *= ALWAN_LITERAL(360.0); } while(0)

/* Kim2009: J [0,100] -> [0,1], h [0,360) -> [0,1] */
#define ALWAN_NORM_KIM2009(p)   do { (p)->J *= ALWAN_LITERAL(0.01); (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_KIM2009(p) do { (p)->J *= ALWAN_LITERAL(100.0); (p)->h *= ALWAN_LITERAL(360.0); } while(0)

/* LLAB: L [0,100] -> [0,1], h [0,360) -> [0,1] */
#define ALWAN_NORM_LLAB(p)   do { (p)->L *= ALWAN_LITERAL(0.01); (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_LLAB(p) do { (p)->L *= ALWAN_LITERAL(100.0); (p)->h *= ALWAN_LITERAL(360.0); } while(0)

/* ATD95: H [0,360) -> [0,1] */
#define ALWAN_NORM_ATD95(p)   do { (p)->H /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_ATD95(p) do { (p)->H *= ALWAN_LITERAL(360.0); } while(0)

/* RLAB: L [0,100] -> [0,1], h [0,360) -> [0,1] */
#define ALWAN_NORM_RLAB(p)   do { (p)->L *= ALWAN_LITERAL(0.01); (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_RLAB(p) do { (p)->L *= ALWAN_LITERAL(100.0); (p)->h *= ALWAN_LITERAL(360.0); } while(0)

/* Nayatani95: L_star_N [0,100] -> [0,1], theta [0,2pi) -> [0,1] */
#define ALWAN_NORM_NAYATANI95(p)   do { (p)->L_star_N *= ALWAN_LITERAL(0.01); (p)->theta /= ALWAN__TWOPI; } while(0)
#define ALWAN_DENORM_NAYATANI95(p) do { (p)->L_star_N *= ALWAN_LITERAL(100.0); (p)->theta *= ALWAN__TWOPI; } while(0)

/* CAM18sl: h [0,360) -> [0,1] */
#define ALWAN_NORM_CAM18SL(p)   do { (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_CAM18SL(p) do { (p)->h *= ALWAN_LITERAL(360.0); } while(0)

/* CAM20u: h [0,360) -> [0,1] */
#define ALWAN_NORM_CAM20U(p)   do { (p)->h /= ALWAN_LITERAL(360.0); } while(0)
#define ALWAN_DENORM_CAM20U(p) do { (p)->h *= ALWAN_LITERAL(360.0); } while(0)

#else /* ALWAN_NORMALIZE_RANGES == 0: all no-ops */

#define ALWAN_NORM_LAB(p)           ((void)0)
#define ALWAN_DENORM_LAB(p)         ((void)0)
#define ALWAN_NORM_LUV(p)           ((void)0)
#define ALWAN_DENORM_LUV(p)         ((void)0)
#define ALWAN_NORM_LCH(p)           ((void)0)
#define ALWAN_DENORM_LCH(p)         ((void)0)
#define ALWAN_NORM_LCHUV(p)         ((void)0)
#define ALWAN_DENORM_LCHUV(p)       ((void)0)
#define ALWAN_NORM_OKLCH(p)         ((void)0)
#define ALWAN_DENORM_OKLCH(p)       ((void)0)
#define ALWAN_NORM_JZCZHZ(p)        ((void)0)
#define ALWAN_DENORM_JZCZHZ(p)      ((void)0)
#define ALWAN_NORM_IPTCH(p)         ((void)0)
#define ALWAN_DENORM_IPTCH(p)       ((void)0)
#define ALWAN_NORM_YCBCR(p)         ((void)0)
#define ALWAN_DENORM_YCBCR(p)       ((void)0)
#define ALWAN_NORM_YCOCG(p)         ((void)0)
#define ALWAN_DENORM_YCOCG(p)       ((void)0)
#define ALWAN_NORM_YCCBCCRC(p)      ((void)0)
#define ALWAN_DENORM_YCCBCCRC(p)    ((void)0)
#define ALWAN_NORM_HCL(p)           ((void)0)
#define ALWAN_DENORM_HCL(p)         ((void)0)
#define ALWAN_NORM_IHLS(p)          ((void)0)
#define ALWAN_DENORM_IHLS(p)        ((void)0)
#define ALWAN_NORM_DIN99(p)         ((void)0)
#define ALWAN_DENORM_DIN99(p)       ((void)0)
#define ALWAN_NORM_HUNTER_LAB(p)    ((void)0)
#define ALWAN_DENORM_HUNTER_LAB(p)  ((void)0)
#define ALWAN_NORM_PROLAB(p)        ((void)0)
#define ALWAN_DENORM_PROLAB(p)      ((void)0)
#define ALWAN_NORM_CAM_JAB(p)       ((void)0)
#define ALWAN_DENORM_CAM_JAB(p)     ((void)0)
#define ALWAN_NORM_CIECAM02(p)      ((void)0)
#define ALWAN_DENORM_CIECAM02(p)    ((void)0)
#define ALWAN_NORM_CAM16(p)         ((void)0)
#define ALWAN_DENORM_CAM16(p)       ((void)0)
#define ALWAN_NORM_ZCAM(p)          ((void)0)
#define ALWAN_DENORM_ZCAM(p)        ((void)0)
#define ALWAN_NORM_HELLWIG2022(p)   ((void)0)
#define ALWAN_DENORM_HELLWIG2022(p) ((void)0)
#define ALWAN_NORM_HUNT(p)          ((void)0)
#define ALWAN_DENORM_HUNT(p)        ((void)0)
#define ALWAN_NORM_KIM2009(p)       ((void)0)
#define ALWAN_DENORM_KIM2009(p)     ((void)0)
#define ALWAN_NORM_LLAB(p)          ((void)0)
#define ALWAN_DENORM_LLAB(p)        ((void)0)
#define ALWAN_NORM_ATD95(p)         ((void)0)
#define ALWAN_DENORM_ATD95(p)       ((void)0)
#define ALWAN_NORM_RLAB(p)          ((void)0)
#define ALWAN_DENORM_RLAB(p)        ((void)0)
#define ALWAN_NORM_NAYATANI95(p)    ((void)0)
#define ALWAN_DENORM_NAYATANI95(p)  ((void)0)
#define ALWAN_NORM_CAM18SL(p)       ((void)0)
#define ALWAN_DENORM_CAM18SL(p)     ((void)0)
#define ALWAN_NORM_CAM20U(p)        ((void)0)
#define ALWAN_DENORM_CAM20U(p)      ((void)0)

#endif /* ALWAN_NORMALIZE_RANGES */

/* Test tolerance (depends on scalar precision) */
#if ALWAN_SCALAR_IS_FLOAT
# define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-5)
#else
# define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-12)
#endif

#endif /* ALWAN_PLATFORM_H */
