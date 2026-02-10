/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Internal helpers and utilities
 */

#ifndef ALWAN_INTERNAL_H
#define ALWAN_INTERNAL_H

#include "alwan.h"  /* For alwan_alloc_fn, alwan_free_fn, alwan_scalar */
#include <math.h>

/* Mathematical constants */
#if ALWAN_SCALAR_IS_FLOAT
  #define ALWAN_PI      3.14159265358979323846f
#else
  #define ALWAN_PI      3.14159265358979323846
#endif

/* Standard illuminant D65 white point (Y=100 scale)
 * From colour-science: CCS_ILLUMINANTS['CIE 1931 2 Degree Standard Observer']['D65']
 * xy = [0.31270, 0.32900] -> XYZ = [95.04559271, 100.00000000, 108.90577508] */
#define ALWAN_D65_X  ALWAN_LITERAL(95.04559271)
#define ALWAN_D65_Y  ALWAN_LITERAL(100.0)
#define ALWAN_D65_Z  ALWAN_LITERAL(108.90577508)

/* alwan_scalar-aware math functions to avoid double/float conversion warnings */
#if ALWAN_SCALAR_IS_FLOAT
  #define ALWAN_ABS(x)  fabsf(x)
  #define ALWAN_SQRT(x)  sqrtf(x)
  #define ALWAN_CBRT(x)  cbrtf(x)
  #define ALWAN_SIN(x)   sinf(x)
  #define ALWAN_COS(x)   cosf(x)
  #define ALWAN_TAN(x)   tanf(x)
  #define ALWAN_TANH(x)  tanhf(x)
  #define ALWAN_ATAN(x)  atanf(x)
  #define ALWAN_ACOS(x)  acosf(x)
  #define ALWAN_ATAN2(y, x) atan2f(y, x)
  #define ALWAN_POW(x, y) powf(x, y)
  #define ALWAN_EXP(x)   expf(x)
  #define ALWAN_LN(x)    logf(x) // Neperian Log (log base e)
  #define ALWAN_LOG2(x)  log2f(x)
  #define ALWAN_LOG10(x) log10f(x)
  #define ALWAN_FLOOR(x) floorf(x)
  #define ALWAN_CEIL(x)  ceilf(x)
  #define ALWAN_FMOD(x, y) fmodf(x, y)
  #define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-5)
#else
  #define ALWAN_ABS(x)  fabs(x)
  #define ALWAN_SQRT(x)  sqrt(x)
  #define ALWAN_CBRT(x)  cbrt(x)
  #define ALWAN_SIN(x)   sin(x)
  #define ALWAN_COS(x)   cos(x)
  #define ALWAN_TAN(x)   tan(x)
  #define ALWAN_TANH(x)  tanh(x)
  #define ALWAN_ATAN(x)  atan(x)
  #define ALWAN_ACOS(x)  acos(x)
  #define ALWAN_ATAN2(y, x) atan2(y, x)
  #define ALWAN_POW(x, y) pow(x, y)
  #define ALWAN_EXP(x)   exp(x)
  #define ALWAN_LN(x)    log(x) // Neperian Log (log base e)
  #define ALWAN_LOG2(x)  log2(x)
  #define ALWAN_LOG10(x) log10(x)
  #define ALWAN_FLOOR(x) floor(x)
  #define ALWAN_CEIL(x)  ceil(x)
  #define ALWAN_FMOD(x, y) fmod(x, y)
  #define ALWAN_TEST_TOLERANCE ALWAN_LITERAL(1e-12)
#endif

/* ----------------------------------------------------------------
 * Internal context structure (shared across modules)
 * ---------------------------------------------------------------- */

struct alwan_ctx {
    /* Allocation callbacks */
    alwan_alloc_fn alloc_fn;
    alwan_free_fn  free_fn;

    /* Configuration */
    char *runtime_data_root;  /* Owned copy (if non-NULL) */
    uint32_t flags;

    /* Future: data cache, registry, etc. */
};

/* ----------------------------------------------------------------
 * Safe allocation helper (overflow protection)
 * ---------------------------------------------------------------- */

/* Check for multiplication overflow before allocation.
 * Returns 0 if overflow would occur, otherwise returns the safe size. */
static inline size_t alwan_safe_array_size(size_t count, size_t elem_size) {
    if (elem_size == 0) return 0;
    if (count > SIZE_MAX / elem_size) return 0;  /* Overflow */
    return count * elem_size;
}

/* ----------------------------------------------------------------
 * Embedded Data (extern declarations)
 * ---------------------------------------------------------------- */

#if ALWAN_EMBED_DATA

/* CAT matrices (3x3 = 9 elements) */
extern alwan_scalar const g_cat_bradford[9];
extern alwan_scalar const g_cat_cat02[9];
extern alwan_scalar const g_cat_cat16[9];
extern alwan_scalar const g_cat_sharp[9];
extern alwan_scalar const g_cat_fairchild[9];
extern alwan_scalar const g_cat_cmccat97[9];
extern alwan_scalar const g_cat_cmccat2000[9];
extern alwan_scalar const g_cat_cat02_brill_2008[9];
extern alwan_scalar const g_cat_bianco_2010[9];
extern alwan_scalar const g_cat_bianco_pc_2010[9];

/* CAM matrices (Hunt-Pointer-Estevez) */
extern alwan_scalar const g_hpe[9];
extern alwan_scalar const g_hpe_inv[9];

/* ICtCp matrices */
extern alwan_scalar const g_ictcp_rgb_to_lms[9];
extern alwan_scalar const g_ictcp_lms_to_rgb[9];
extern alwan_scalar const g_ictcp_lms_p_to_ictcp_pq[9];
extern alwan_scalar const g_ictcp_ictcp_to_lms_p_pq[9];
extern alwan_scalar const g_ictcp_lms_p_to_ictcp_hlg[9];
extern alwan_scalar const g_ictcp_ictcp_to_lms_p_hlg[9];
extern alwan_scalar const g_ictcp_xyz_to_bt2020[9];
extern alwan_scalar const g_ictcp_bt2020_to_xyz[9];

/* IPT matrices and constants */
extern alwan_scalar const g_ipt_exponent;
extern alwan_scalar const g_ipt_xyz_to_lms[9];
extern alwan_scalar const g_ipt_lms_to_xyz[9];
extern alwan_scalar const g_ipt_lms_p_to_ipt[9];
extern alwan_scalar const g_ipt_ipt_to_lms_p[9];

#endif /* ALWAN_EMBED_DATA */

#endif /* ALWAN_INTERNAL_H */
