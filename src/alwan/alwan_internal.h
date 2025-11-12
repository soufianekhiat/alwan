/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * Internal helpers and utilities
 */

#ifndef ALWAN_INTERNAL_H
#define ALWAN_INTERNAL_H

#include "alwan_config.h"
#include <math.h>

/* Mathematical constants */
#if ALWAN_SCALAR_IS_FLOAT
  #define ALWAN_PI      3.14159265358979323846f
#else
  #define ALWAN_PI      3.14159265358979323846
#endif

/* Scalar-aware math functions to avoid double/float conversion warnings */
#if ALWAN_SCALAR_IS_FLOAT
  #define ALWAN_FABS(x)  fabsf(x)
  #define ALWAN_SQRT(x)  sqrtf(x)
  #define ALWAN_SIN(x)   sinf(x)
  #define ALWAN_COS(x)   cosf(x)
  #define ALWAN_TAN(x)   tanf(x)
  #define ALWAN_ATAN2(y, x) atan2f(y, x)
  #define ALWAN_POW(x, y) powf(x, y)
  #define ALWAN_EXP(x)   expf(x)
  #define ALWAN_LOG(x)   logf(x)
  #define ALWAN_FLOOR(x) floorf(x)
  #define ALWAN_CEIL(x)  ceilf(x)
  #define ALWAN_LITERAL(x) x##f
  #define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-5)
#else
  #define ALWAN_FABS(x)  fabs(x)
  #define ALWAN_SQRT(x)  sqrt(x)
  #define ALWAN_SIN(x)   sin(x)
  #define ALWAN_COS(x)   cos(x)
  #define ALWAN_TAN(x)   tan(x)
  #define ALWAN_ATAN2(y, x) atan2(y, x)
  #define ALWAN_POW(x, y) pow(x, y)
  #define ALWAN_EXP(x)   exp(x)
  #define ALWAN_LOG(x)   log(x)
  #define ALWAN_FLOOR(x) floor(x)
  #define ALWAN_CEIL(x)  ceil(x)
  #define ALWAN_LITERAL(x) x
  #define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-12)
#endif

#endif /* ALWAN_INTERNAL_H */
