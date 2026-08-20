/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Embedded table definitions: the Jakob 2019 spectral coefficient cubes.
 *
 * 18 cubes x 2 precisions = 55 MB of CSV preprocessed twice. Splitting
 * these out drops ~110 MB of preprocessing off api/alwan_spectrum_upsample.c
 * and leaves that file as ordinary code. See alwan_data_tables.c for the
 * split rationale.
 *
 * Layout is PLANAR and b-fastest: index = (r*RES + g)*RES + b. This is not
 * the interleaved r-fastest cube layout, which is why these are read by
 * alwan_jakob2019_coeff_sample_* rather than alwan_table3d_sample_*.
 *
 * This directory holds every array reached through a FLOAT coordinate. Arrays
 * read at compile-time-constant indices stay in their core headers so they
 * remain constant-foldable into immediates.
 *
 * Blocks appear in the SAME ORDER as the declarations in alwan_data_tables.h.
 * That order equality is the invariant tools/check_table_registry.py enforces.
 *
 * The #if wrapper on each block is inert today: the switch defaults to 1. It
 * is here so enabling the feature later flips a default in
 * alwan_data_tables_config.h instead of editing forty blocks.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_data_tables.h"

/* The f32 pass of every dual-declared table narrows f64 CSV literals. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* ---- alwan_table_jakob2019_srgb_c0 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_SRGB
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_srgb_c0_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_srgb_c0_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_srgb_c1 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_SRGB
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_srgb_c1_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_srgb_c1_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_srgb_c2 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_SRGB
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_srgb_c2_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_srgb_c2_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_prophoto_c0 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_prophoto_c0_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_prophotorgb.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_prophoto_c0_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_prophotorgb.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_prophoto_c1 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_prophoto_c1_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_prophotorgb.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_prophoto_c1_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_prophotorgb.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_prophoto_c2 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_prophoto_c2_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_prophotorgb.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_prophoto_c2_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_prophotorgb.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_aces_c0 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_ACES
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_aces_c0_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_aces2065_1.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_aces_c0_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_aces2065_1.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_aces_c1 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_ACES
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_aces_c1_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_aces2065_1.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_aces_c1_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_aces2065_1.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_aces_c2 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_ACES
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_aces_c2_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_aces2065_1.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_aces_c2_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_aces2065_1.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_rec2020_c0 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_REC2020
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_rec2020_c0_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_rec2020.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_rec2020_c0_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_rec2020.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_rec2020_c1 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_REC2020
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_rec2020_c1_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_rec2020.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_rec2020_c1_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_rec2020.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_rec2020_c2 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_REC2020
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_rec2020_c2_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_rec2020.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_rec2020_c2_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_rec2020.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_ergb_c0 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_ERGB
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_ergb_c0_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_ergb.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_ergb_c0_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_ergb.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_ergb_c1 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_ERGB
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_ergb_c1_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_ergb.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_ergb_c1_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_ergb.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_ergb_c2 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_ERGB
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_ergb_c2_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_ergb.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_ergb_c2_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_ergb.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_xyz_c0 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_XYZ
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_xyz_c0_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_xyz.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_xyz_c0_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c0_xyz.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_xyz_c1 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_XYZ
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_xyz_c1_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_xyz.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_xyz_c1_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c1_xyz.csv"
};
#endif
#endif

/* ---- alwan_table_jakob2019_xyz_c2 ----
 * rank 3, 64^3, TRILINEAR, planar b-fastest.
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}. */
#if ALWAN_TABLE_JAKOB2019_XYZ
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_jakob2019_xyz_c2_f32[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_xyz.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_jakob2019_xyz_c2_f64[ALWAN_TABLE_JAKOB2019_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_lut_c2_xyz.csv"
};
#endif
#endif


ALWAN_DIAG_POP
