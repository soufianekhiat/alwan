/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Data loading.
 *
 * Only embedded mode (ALWAN_EMBED_DATA=1) is implemented.
 * Runtime mode (ALWAN_EMBED_DATA=0) is NOT supported in the current release.
 * It is planned as a feature for alwan 3.0.0.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if ALWAN_EMBED_DATA

/* ----------------------------------------------------------------
 * Embedded data mode: compile-time data inclusion
 * ---------------------------------------------------------------- */

/* Disable float conversion warnings for embedded CSV data */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* Illuminant chromaticity (x, y) and sRGB primary tables: dual f32/f64
 * declaration so the f32 getters return native float data instead of narrowing
 * doubles through a runtime cache. Each twin is fed by the same CSV; the f64
 * twin is byte-identical to the former array. */

/* Illuminant A (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_a_xy_f32[] = {
#include "../data/illuminants_xy/a_xy.csv"
};
#endif
/* Compiled in every build: the documented f64-internal facades read these f64
 * tables from their f32 entry points, so the data has to exist even when the
 * f64 public surface is excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_a_xy_f64[] = {
#include "../data/illuminants_xy/a_xy.csv"
};
#endif

/* Illuminant D50 (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_d50_xy_f32[] = {
#include "../data/illuminants_xy/d50_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d50_xy_f64[] = {
#include "../data/illuminants_xy/d50_xy.csv"
};
#endif

/* Illuminant D55 (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_d55_xy_f32[] = {
#include "../data/illuminants_xy/d55_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d55_xy_f64[] = {
#include "../data/illuminants_xy/d55_xy.csv"
};
#endif

/* Illuminant D60 (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_d60_xy_f32[] = {
#include "../data/illuminants_xy/d60_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d60_xy_f64[] = {
#include "../data/illuminants_xy/d60_xy.csv"
};
#endif

/* Illuminant D65 (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_d65_xy_f32[] = {
#include "../data/illuminants_xy/d65_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d65_xy_f64[] = {
#include "../data/illuminants_xy/d65_xy.csv"
};
#endif

/* Illuminant E (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_e_xy_f32[] = {
#include "../data/illuminants_xy/e_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_e_xy_f64[] = {
#include "../data/illuminants_xy/e_xy.csv"
};
#endif

/* Illuminant B (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_b_xy_f32[] = {
#include "../data/illuminants_xy/b_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_b_xy_f64[] = {
#include "../data/illuminants_xy/b_xy.csv"
};
#endif

/* Illuminant C (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_c_xy_f32[] = {
#include "../data/illuminants_xy/c_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_c_xy_f64[] = {
#include "../data/illuminants_xy/c_xy.csv"
};
#endif

/* Illuminant D75 (x, y) */
#if ALWAN_WITH_F32
static alwan_f32 const g_d75_xy_f32[] = {
#include "../data/illuminants_xy/d75_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d75_xy_f64[] = {
#include "../data/illuminants_xy/d75_xy.csv"
};
#endif

/* Additional D-series illuminants */
#if ALWAN_WITH_F32
static alwan_f32 const g_d40_xy_f32[] = {
#include "../data/illuminants_xy/d40_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d40_xy_f64[] = {
#include "../data/illuminants_xy/d40_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_d45_xy_f32[] = {
#include "../data/illuminants_xy/d45_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d45_xy_f64[] = {
#include "../data/illuminants_xy/d45_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_d93_xy_f32[] = {
#include "../data/illuminants_xy/d93_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_d93_xy_f64[] = {
#include "../data/illuminants_xy/d93_xy.csv"
};
#endif

/* LED illuminants */
#if ALWAN_WITH_F32
static alwan_f32 const g_led_b1_xy_f32[] = {
#include "../data/illuminants_xy/led-b1_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_b1_xy_f64[] = {
#include "../data/illuminants_xy/led-b1_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_b2_xy_f32[] = {
#include "../data/illuminants_xy/led-b2_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_b2_xy_f64[] = {
#include "../data/illuminants_xy/led-b2_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_b3_xy_f32[] = {
#include "../data/illuminants_xy/led-b3_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_b3_xy_f64[] = {
#include "../data/illuminants_xy/led-b3_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_b4_xy_f32[] = {
#include "../data/illuminants_xy/led-b4_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_b4_xy_f64[] = {
#include "../data/illuminants_xy/led-b4_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_b5_xy_f32[] = {
#include "../data/illuminants_xy/led-b5_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_b5_xy_f64[] = {
#include "../data/illuminants_xy/led-b5_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_bh1_xy_f32[] = {
#include "../data/illuminants_xy/led-bh1_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_bh1_xy_f64[] = {
#include "../data/illuminants_xy/led-bh1_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_rgb1_xy_f32[] = {
#include "../data/illuminants_xy/led-rgb1_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_rgb1_xy_f64[] = {
#include "../data/illuminants_xy/led-rgb1_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_v1_xy_f32[] = {
#include "../data/illuminants_xy/led-v1_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_v1_xy_f64[] = {
#include "../data/illuminants_xy/led-v1_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_led_v2_xy_f32[] = {
#include "../data/illuminants_xy/led-v2_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_led_v2_xy_f64[] = {
#include "../data/illuminants_xy/led-v2_xy.csv"
};
#endif

/* High Pressure illuminants */
#if ALWAN_WITH_F32
static alwan_f32 const g_hp1_xy_f32[] = {
#include "../data/illuminants_xy/hp1_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_hp1_xy_f64[] = {
#include "../data/illuminants_xy/hp1_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_hp2_xy_f32[] = {
#include "../data/illuminants_xy/hp2_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_hp2_xy_f64[] = {
#include "../data/illuminants_xy/hp2_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_hp3_xy_f32[] = {
#include "../data/illuminants_xy/hp3_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_hp3_xy_f64[] = {
#include "../data/illuminants_xy/hp3_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_hp4_xy_f32[] = {
#include "../data/illuminants_xy/hp4_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_hp4_xy_f64[] = {
#include "../data/illuminants_xy/hp4_xy.csv"
};
#endif

#if ALWAN_WITH_F32
static alwan_f32 const g_hp5_xy_f32[] = {
#include "../data/illuminants_xy/hp5_xy.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_hp5_xy_f64[] = {
#include "../data/illuminants_xy/hp5_xy.csv"
};
#endif

/* sRGB primaries (rx, ry, gx, gy, bx, by) */
#if ALWAN_WITH_F32
static alwan_f32 const g_srgb_primaries_3x2_f32[] = {
#include "../data/srgb_primaries_3x2.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
static alwan_f64 const g_srgb_primaries_3x2_f64[] = {
#include "../data/srgb_primaries_3x2.csv"
};
#endif

/* ----------------------------------------------------------------
 * CAT Matrices (Chromatic Adaptation Transform)
 * ---------------------------------------------------------------- */

/* CAT matrices: dual f32/f64 declaration so the templated f32 path reads
 * native float data instead of narrowing doubles per access. Each twin is
 * fed by the same CSV; the f64 twin is byte-identical to the former array. */

/* Bradford CAT matrix (most common, used in ICC profiles) */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_bradford_f32[9] = {
#include "../data/matrices/cat_bradford.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_bradford_f64[9] = {
#include "../data/matrices/cat_bradford.csv"
};
#endif

/* CAT02 matrix (from CIECAM02) */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_cat02_f32[9] = {
#include "../data/matrices/cat_cat02.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_cat02_f64[9] = {
#include "../data/matrices/cat_cat02.csv"
};
#endif

/* CAT16 matrix (from CAM16) */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_cat16_f32[9] = {
#include "../data/matrices/cat_cat16.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_cat16_f64[9] = {
#include "../data/matrices/cat_cat16.csv"
};
#endif

/* Sharp CAT matrix */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_sharp_f32[9] = {
#include "../data/matrices/cat_sharp.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_sharp_f64[9] = {
#include "../data/matrices/cat_sharp.csv"
};
#endif

/* Fairchild 1990 CAT matrix */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_fairchild_f32[9] = {
#include "../data/matrices/cat_fairchild.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_fairchild_f64[9] = {
#include "../data/matrices/cat_fairchild.csv"
};
#endif

/* CMCCAT97 matrix */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_cmccat97_f32[9] = {
#include "../data/matrices/cat_cmccat97.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_cmccat97_f64[9] = {
#include "../data/matrices/cat_cmccat97.csv"
};
#endif

/* CMCCAT2000 matrix */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_cmccat2000_f32[9] = {
#include "../data/matrices/cat_cmccat2000.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_cmccat2000_f64[9] = {
#include "../data/matrices/cat_cmccat2000.csv"
};
#endif

/* CAT02 Brill 2008 variant matrix */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_cat02_brill_2008_f32[9] = {
#include "../data/matrices/cat_cat02_brill_2008.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_cat02_brill_2008_f64[9] = {
#include "../data/matrices/cat_cat02_brill_2008.csv"
};
#endif

/* Bianco 2010 CAT matrix */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_bianco_2010_f32[9] = {
#include "../data/matrices/cat_bianco_2010.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_bianco_2010_f64[9] = {
#include "../data/matrices/cat_bianco_2010.csv"
};
#endif

/* Bianco PC 2010 CAT matrix */
#if ALWAN_WITH_F32
alwan_f32 const g_cat_bianco_pc_2010_f32[9] = {
#include "../data/matrices/cat_bianco_pc_2010.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const g_cat_bianco_pc_2010_f64[9] = {
#include "../data/matrices/cat_bianco_pc_2010.csv"
};
#endif

/* ----------------------------------------------------------------
 * CAM Matrices (Color Appearance Models)
 * ---------------------------------------------------------------- */

/* Hunt-Pointer-Estevez matrix (XYZ to LMS cone response) */
alwan_f64 const g_hpe[9] = {
#include "../data/matrices/hpe.csv"
};

/* Inverse Hunt-Pointer-Estevez matrix (LMS to XYZ) */
alwan_f64 const g_hpe_inv[9] = {
#include "../data/matrices/hpe_inv.csv"
};

/* ----------------------------------------------------------------
 * ICtCp Matrices (ITU-R BT.2100 HDR)
 * ---------------------------------------------------------------- */

/* BT.2020 RGB to LMS cone response matrix */
alwan_f64 const g_ictcp_rgb_to_lms[9] = {
#include "../data/ictcp_rgb_to_lms.csv"
};

/* LMS to BT.2020 RGB inverse matrix */
alwan_f64 const g_ictcp_lms_to_rgb[9] = {
#include "../data/ictcp_lms_to_rgb.csv"
};

/* LMS' to ICtCp matrix for PQ */
alwan_f64 const g_ictcp_lms_p_to_ictcp_pq[9] = {
#include "../data/ictcp_lms_p_to_ictcp_pq.csv"
};

/* ICtCp to LMS' inverse matrix for PQ */
alwan_f64 const g_ictcp_ictcp_to_lms_p_pq[9] = {
#include "../data/ictcp_ictcp_to_lms_p_pq.csv"
};

/* LMS' to ICtCp matrix for HLG */
alwan_f64 const g_ictcp_lms_p_to_ictcp_hlg[9] = {
#include "../data/ictcp_lms_p_to_ictcp_hlg.csv"
};

/* ICtCp to LMS' inverse matrix for HLG */
alwan_f64 const g_ictcp_ictcp_to_lms_p_hlg[9] = {
#include "../data/ictcp_ictcp_to_lms_p_hlg.csv"
};

/* XYZ (D65) to BT.2020 RGB matrix */
alwan_f64 const g_ictcp_xyz_to_bt2020[9] = {
#include "../data/ictcp_xyz_to_bt2020.csv"
};

/* BT.2020 RGB to XYZ (D65) matrix */
alwan_f64 const g_ictcp_bt2020_to_xyz[9] = {
#include "../data/ictcp_bt2020_to_xyz.csv"
};

/* ----------------------------------------------------------------
 * IPT Matrices and Constants (Ebner & Fairchild 1998)
 * ---------------------------------------------------------------- */

/* IPT nonlinearity exponent */
alwan_f64 const g_ipt_exponent = {
#include "../data/ipt_exponent.csv"
};

/* XYZ (D65) to LMS matrix for IPT */
alwan_f64 const g_ipt_xyz_to_lms[9] = {
#include "../data/ipt_xyz_to_lms.csv"
};

/* LMS to XYZ inverse matrix for IPT */
alwan_f64 const g_ipt_lms_to_xyz[9] = {
#include "../data/ipt_lms_to_xyz.csv"
};

/* LMS' to IPT matrix */
alwan_f64 const g_ipt_lms_p_to_ipt[9] = {
#include "../data/ipt_lms_p_to_ipt.csv"
};

/* IPT to LMS' inverse matrix */
alwan_f64 const g_ipt_ipt_to_lms_p[9] = {
#include "../data/ipt_ipt_to_lms_p.csv"
};

ALWAN_DIAG_POP

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_data_get_illuminant_a_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;  /* Unused in embedded mode */
    *data = (alwan_f64 *)g_a_xy_f64;
    *count = sizeof(g_a_xy_f64) / sizeof(g_a_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d50_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d50_xy_f64;
    *count = sizeof(g_d50_xy_f64) / sizeof(g_d50_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d55_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d55_xy_f64;
    *count = sizeof(g_d55_xy_f64) / sizeof(g_d55_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d60_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d60_xy_f64;
    *count = sizeof(g_d60_xy_f64) / sizeof(g_d60_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d65_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d65_xy_f64;
    *count = sizeof(g_d65_xy_f64) / sizeof(g_d65_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_e_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_e_xy_f64;
    *count = sizeof(g_e_xy_f64) / sizeof(g_e_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_b_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_b_xy_f64;
    *count = sizeof(g_b_xy_f64) / sizeof(g_b_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_c_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_c_xy_f64;
    *count = sizeof(g_c_xy_f64) / sizeof(g_c_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d75_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d75_xy_f64;
    *count = sizeof(g_d75_xy_f64) / sizeof(g_d75_xy_f64[0]);
    return ALWAN_OK;
}

/* Additional D-series illuminants */
int alwan_data_get_illuminant_d40(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d40_xy_f64;
    *count = sizeof(g_d40_xy_f64) / sizeof(g_d40_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d45(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d45_xy_f64;
    *count = sizeof(g_d45_xy_f64) / sizeof(g_d45_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_d93(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_d93_xy_f64;
    *count = sizeof(g_d93_xy_f64) / sizeof(g_d93_xy_f64[0]);
    return ALWAN_OK;
}

/* LED illuminants */
int alwan_data_get_illuminant_led_b1(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_b1_xy_f64;
    *count = sizeof(g_led_b1_xy_f64) / sizeof(g_led_b1_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b2(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_b2_xy_f64;
    *count = sizeof(g_led_b2_xy_f64) / sizeof(g_led_b2_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b3(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_b3_xy_f64;
    *count = sizeof(g_led_b3_xy_f64) / sizeof(g_led_b3_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b4(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_b4_xy_f64;
    *count = sizeof(g_led_b4_xy_f64) / sizeof(g_led_b4_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_b5(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_b5_xy_f64;
    *count = sizeof(g_led_b5_xy_f64) / sizeof(g_led_b5_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_bh1(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_bh1_xy_f64;
    *count = sizeof(g_led_bh1_xy_f64) / sizeof(g_led_bh1_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_rgb1(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_rgb1_xy_f64;
    *count = sizeof(g_led_rgb1_xy_f64) / sizeof(g_led_rgb1_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_v1(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_v1_xy_f64;
    *count = sizeof(g_led_v1_xy_f64) / sizeof(g_led_v1_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_led_v2(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_led_v2_xy_f64;
    *count = sizeof(g_led_v2_xy_f64) / sizeof(g_led_v2_xy_f64[0]);
    return ALWAN_OK;
}

/* High Pressure illuminants */
int alwan_data_get_illuminant_hp1(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_hp1_xy_f64;
    *count = sizeof(g_hp1_xy_f64) / sizeof(g_hp1_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp2(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_hp2_xy_f64;
    *count = sizeof(g_hp2_xy_f64) / sizeof(g_hp2_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp3(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_hp3_xy_f64;
    *count = sizeof(g_hp3_xy_f64) / sizeof(g_hp3_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp4(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_hp4_xy_f64;
    *count = sizeof(g_hp4_xy_f64) / sizeof(g_hp4_xy_f64[0]);
    return ALWAN_OK;
}

int alwan_data_get_illuminant_hp5(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_hp5_xy_f64;
    *count = sizeof(g_hp5_xy_f64) / sizeof(g_hp5_xy_f64[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_xy_f64(alwan_f64 **data, size_t *count, alwan_illuminant illuminant, alwan_ctx *ctx) {
    switch (illuminant) {
        case ALWAN_ILLUMINANT_A:   return alwan_data_get_illuminant_a_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_D50: return alwan_data_get_illuminant_d50_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_D55: return alwan_data_get_illuminant_d55_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_D60: return alwan_data_get_illuminant_d60_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_D65: return alwan_data_get_illuminant_d65_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_E:   return alwan_data_get_illuminant_e_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_B:   return alwan_data_get_illuminant_b_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_C:   return alwan_data_get_illuminant_c_f64(data, count, ctx);
        case ALWAN_ILLUMINANT_D75: return alwan_data_get_illuminant_d75_f64(data, count, ctx);
        /* Additional illuminants */
        case ALWAN_ILLUMINANT_D40: return alwan_data_get_illuminant_d40(data, count, ctx);
        case ALWAN_ILLUMINANT_D45: return alwan_data_get_illuminant_d45(data, count, ctx);
        case ALWAN_ILLUMINANT_D93: return alwan_data_get_illuminant_d93(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B1: return alwan_data_get_illuminant_led_b1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B2: return alwan_data_get_illuminant_led_b2(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B3: return alwan_data_get_illuminant_led_b3(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B4: return alwan_data_get_illuminant_led_b4(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_B5: return alwan_data_get_illuminant_led_b5(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_BH1: return alwan_data_get_illuminant_led_bh1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_RGB1: return alwan_data_get_illuminant_led_rgb1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_V1: return alwan_data_get_illuminant_led_v1(data, count, ctx);
        case ALWAN_ILLUMINANT_LED_V2: return alwan_data_get_illuminant_led_v2(data, count, ctx);
        case ALWAN_ILLUMINANT_HP1: return alwan_data_get_illuminant_hp1(data, count, ctx);
        case ALWAN_ILLUMINANT_HP2: return alwan_data_get_illuminant_hp2(data, count, ctx);
        case ALWAN_ILLUMINANT_HP3: return alwan_data_get_illuminant_hp3(data, count, ctx);
        case ALWAN_ILLUMINANT_HP4: return alwan_data_get_illuminant_hp4(data, count, ctx);
        case ALWAN_ILLUMINANT_HP5: return alwan_data_get_illuminant_hp5(data, count, ctx);
        default:
            return ALWAN_E_INVALID;  /* Unsupported illuminant or no xy data */
    }
}

alwan_status alwan_data_get_srgb_primaries_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f64 *)g_srgb_primaries_3x2_f64;
    *count = sizeof(g_srgb_primaries_3x2_f64) / sizeof(g_srgb_primaries_3x2_f64[0]);
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

/* ----------------------------------------------------------------
 * f32 Variants
 *
 * Native f32: each getter returns its dedicated f32 CSV twin directly
 * (decimal -> float at compile time), with no runtime narrowing.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F32
alwan_status alwan_data_get_illuminant_a_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_a_xy_f32;
    *count = sizeof(g_a_xy_f32) / sizeof(g_a_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d50_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_d50_xy_f32;
    *count = sizeof(g_d50_xy_f32) / sizeof(g_d50_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d55_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_d55_xy_f32;
    *count = sizeof(g_d55_xy_f32) / sizeof(g_d55_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d60_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_d60_xy_f32;
    *count = sizeof(g_d60_xy_f32) / sizeof(g_d60_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d65_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_d65_xy_f32;
    *count = sizeof(g_d65_xy_f32) / sizeof(g_d65_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_e_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_e_xy_f32;
    *count = sizeof(g_e_xy_f32) / sizeof(g_e_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_b_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_b_xy_f32;
    *count = sizeof(g_b_xy_f32) / sizeof(g_b_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_c_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_c_xy_f32;
    *count = sizeof(g_c_xy_f32) / sizeof(g_c_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_d75_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_d75_xy_f32;
    *count = sizeof(g_d75_xy_f32) / sizeof(g_d75_xy_f32[0]);
    return ALWAN_OK;
}

alwan_status alwan_data_get_illuminant_xy_f32(alwan_f32 **data, size_t *count, alwan_illuminant illuminant, alwan_ctx *ctx) {
    /* Native f32: every illuminant has a dedicated f32 CSV twin, so return its
     * pointer directly (no f64 narrowing fallback). The main illuminants
     * delegate to their per-illuminant getters; the rest are inlined here. */
    switch (illuminant) {
        case ALWAN_ILLUMINANT_A:   return alwan_data_get_illuminant_a_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_D50: return alwan_data_get_illuminant_d50_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_D55: return alwan_data_get_illuminant_d55_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_D60: return alwan_data_get_illuminant_d60_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_D65: return alwan_data_get_illuminant_d65_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_E:   return alwan_data_get_illuminant_e_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_B:   return alwan_data_get_illuminant_b_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_C:   return alwan_data_get_illuminant_c_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_D75: return alwan_data_get_illuminant_d75_f32(data, count, ctx);
        case ALWAN_ILLUMINANT_D40:
            *data = (alwan_f32 *)g_d40_xy_f32; *count = sizeof(g_d40_xy_f32) / sizeof(g_d40_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_D45:
            *data = (alwan_f32 *)g_d45_xy_f32; *count = sizeof(g_d45_xy_f32) / sizeof(g_d45_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_D93:
            *data = (alwan_f32 *)g_d93_xy_f32; *count = sizeof(g_d93_xy_f32) / sizeof(g_d93_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_B1:
            *data = (alwan_f32 *)g_led_b1_xy_f32; *count = sizeof(g_led_b1_xy_f32) / sizeof(g_led_b1_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_B2:
            *data = (alwan_f32 *)g_led_b2_xy_f32; *count = sizeof(g_led_b2_xy_f32) / sizeof(g_led_b2_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_B3:
            *data = (alwan_f32 *)g_led_b3_xy_f32; *count = sizeof(g_led_b3_xy_f32) / sizeof(g_led_b3_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_B4:
            *data = (alwan_f32 *)g_led_b4_xy_f32; *count = sizeof(g_led_b4_xy_f32) / sizeof(g_led_b4_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_B5:
            *data = (alwan_f32 *)g_led_b5_xy_f32; *count = sizeof(g_led_b5_xy_f32) / sizeof(g_led_b5_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_BH1:
            *data = (alwan_f32 *)g_led_bh1_xy_f32; *count = sizeof(g_led_bh1_xy_f32) / sizeof(g_led_bh1_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_RGB1:
            *data = (alwan_f32 *)g_led_rgb1_xy_f32; *count = sizeof(g_led_rgb1_xy_f32) / sizeof(g_led_rgb1_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_V1:
            *data = (alwan_f32 *)g_led_v1_xy_f32; *count = sizeof(g_led_v1_xy_f32) / sizeof(g_led_v1_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_LED_V2:
            *data = (alwan_f32 *)g_led_v2_xy_f32; *count = sizeof(g_led_v2_xy_f32) / sizeof(g_led_v2_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_HP1:
            *data = (alwan_f32 *)g_hp1_xy_f32; *count = sizeof(g_hp1_xy_f32) / sizeof(g_hp1_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_HP2:
            *data = (alwan_f32 *)g_hp2_xy_f32; *count = sizeof(g_hp2_xy_f32) / sizeof(g_hp2_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_HP3:
            *data = (alwan_f32 *)g_hp3_xy_f32; *count = sizeof(g_hp3_xy_f32) / sizeof(g_hp3_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_HP4:
            *data = (alwan_f32 *)g_hp4_xy_f32; *count = sizeof(g_hp4_xy_f32) / sizeof(g_hp4_xy_f32[0]); return ALWAN_OK;
        case ALWAN_ILLUMINANT_HP5:
            *data = (alwan_f32 *)g_hp5_xy_f32; *count = sizeof(g_hp5_xy_f32) / sizeof(g_hp5_xy_f32[0]); return ALWAN_OK;
        default:
            return ALWAN_E_INVALID;  /* Unsupported illuminant or no xy data */
    }
}

alwan_status alwan_data_get_srgb_primaries_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx) {
    (void)ctx;
    *data = (alwan_f32 *)g_srgb_primaries_3x2_f32;
    *count = sizeof(g_srgb_primaries_3x2_f32) / sizeof(g_srgb_primaries_3x2_f32[0]);
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

#else
    /* Runtime data loading (ALWAN_EMBED_DATA=0) is NOT implemented.
     * It is planned for alwan 3.0.0. Build with ALWAN_EMBED_DATA=1 (the default). */
    #error "Runtime data loading (ALWAN_EMBED_DATA=0) is not implemented. Planned for alwan 3.0.0. Use ALWAN_EMBED_DATA=1."
#endif /* ALWAN_EMBED_DATA */

/* ----------------------------------------------------------------
 * Illuminant White Point Calculation (works in both modes)
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64_FACADE
alwan_status alwan_illuminant_white_point_f64(alwan_xyz_f64 *out_xyz,
                                   alwan_illuminant illuminant,
                                   alwan_observer_type observer) {
    if (!out_xyz) {
        return ALWAN_E_INVALID;
    }

    /* For CIE 1931 2-deg observer, use pre-computed xy chromaticity values for efficiency */
    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        /* Get xy chromaticity data for the illuminant */
        alwan_f64 *xy_data = NULL;
        size_t count = 0;
        int status = alwan_data_get_illuminant_xy_f64(&xy_data, &count, illuminant, NULL);

        if (status != ALWAN_OK || count < 2) {
            return ALWAN_E_INVALID;
        }

        /* Extract x and y chromaticity coordinates */
        alwan_f64 x = xy_data[0];
        alwan_f64 y = xy_data[1];

        /* Convert xy to XYZ with Y = 1.0 (normalized)
         * Formula: X = x * Y / y
         *          Y = 1.0
         *          Z = (1 - x - y) * Y / y */
        alwan_f64 const Y = ALWAN_LITERAL(1.0);

        if (y <= ALWAN_LITERAL(0.0)) {
            return ALWAN_E_INVALID;  /* Invalid chromaticity */
        }

        out_xyz->x = x * Y / y;                    /* X */
        out_xyz->y = Y;                             /* Y */
        out_xyz->z = (ALWAN_LITERAL(1.0) - x - y) * Y / y;  /* Z */

        return ALWAN_OK;
    }

    /* For other observers, compute from illuminant SPD + observer CMF integration */
    alwan_spd_f64 illum_spd;
    int status = alwan_spd_illuminant_f64(&illum_spd, illuminant, NULL);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Integrate illuminant SPD with observer CMFs to get XYZ */
    alwan_xyz_f64 xyz_unnormalized;
    status = alwan_xyz_from_spd_f64(&xyz_unnormalized, &illum_spd, NULL, observer, ALWAN_INTEGRATE_SIMPSON, ALWAN_LITERAL(0.0), NULL);

    alwan_spd_destroy_f64(&illum_spd, NULL);

    if (status != ALWAN_OK) {
        return status;
    }

    /* Normalize to Y = 1.0 */
    if (xyz_unnormalized.y <= ALWAN_LITERAL(0.0)) {
        return ALWAN_E_INVALID;  /* Invalid Y value */
    }

    alwan_f64 norm_factor = ALWAN_LITERAL(1.0) / xyz_unnormalized.y;
    out_xyz->x = xyz_unnormalized.x * norm_factor;
    out_xyz->y = ALWAN_LITERAL(1.0);
    out_xyz->z = xyz_unnormalized.z * norm_factor;

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
alwan_status alwan_illuminant_white_point_f32(alwan_xyz_f32 *out_xyz,
                                   alwan_illuminant illuminant,
                                   alwan_observer_type observer) {
    if (!out_xyz) return ALWAN_E_INVALID;

    /* Fully native f32: the CIE 1931 2-deg branch is pure xy->XYZ over the native
     * f32 chromaticity data; the general-observer branch integrates the f32
     * illuminant SPD against the f32 observer CMFs (alwan_spd is now native f32).
     * Both mirror the f64 worker's algorithm exactly, in f32. */
    if (observer == ALWAN_OBSERVER_CIE_1931_2DEG) {
        alwan_f32 *xy_data = NULL;
        size_t count = 0;
        int status = alwan_data_get_illuminant_xy_f32(&xy_data, &count, illuminant, NULL);

        if (status != ALWAN_OK || count < 2) {
            return ALWAN_E_INVALID;
        }

        /* Extract x and y chromaticity coordinates */
        alwan_f32 x = xy_data[0];
        alwan_f32 y = xy_data[1];

        /* Convert xy to XYZ with Y = 1.0 (normalized)
         * Formula: X = x * Y / y
         *          Y = 1.0
         *          Z = (1 - x - y) * Y / y */
        alwan_f32 const Y = 1.0f;

        if (y <= 0.0f) {
            return ALWAN_E_INVALID;  /* Invalid chromaticity */
        }

        out_xyz->x = x * Y / y;                    /* X */
        out_xyz->y = Y;                             /* Y */
        out_xyz->z = (1.0f - x - y) * Y / y;       /* Z */

        return ALWAN_OK;
    }

    /* General observer: native f32 SPD + observer-CMF integration (alwan_spd is
     * native f32, so this needs no f64 path and works in an f32-only build). */
    alwan_spd_f32 illum_spd;
    int status = alwan_spd_illuminant_f32(&illum_spd, illuminant, NULL);
    if (status != ALWAN_OK) {
        return status;
    }

    alwan_xyz_f32 xyz_unnormalized;
    status = alwan_xyz_from_spd_f32(&xyz_unnormalized, &illum_spd, NULL, observer,
                                    ALWAN_INTEGRATE_SIMPSON, 0.0f, NULL);
    alwan_spd_destroy_f32(&illum_spd, NULL);
    if (status != ALWAN_OK) {
        return status;
    }

    if (xyz_unnormalized.y <= 0.0f) {
        return ALWAN_E_INVALID;  /* Invalid Y value */
    }

    alwan_f32 norm_factor = 1.0f / xyz_unnormalized.y;
    out_xyz->x = xyz_unnormalized.x * norm_factor;
    out_xyz->y = 1.0f;
    out_xyz->z = xyz_unnormalized.z * norm_factor;
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
