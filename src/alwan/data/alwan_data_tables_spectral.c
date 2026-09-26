/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Embedded table definitions: the Jakob 2019 spectral coefficient tables.
 *
 * Six gamuts, each rgb2spec_opt's own table (3 x 64^3 x 3 float32) and its
 * lightness axis, 160 MB of CSV preprocessed once. Splitting these out keeps
 * that preprocessing off api/alwan_spectrum_upsample.c and leaves that file as
 * ordinary code. See alwan_data_tables.c for the split rationale.
 *
 * The layout is the tool's, described with the declarations in
 * alwan_data_tables.h, and alwan__jakob2019_fetch reads it.
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

/* The CSV literals are the float32 values rgb2spec_opt wrote, each spelled so that
 * it parses back to the same float. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* ---- alwan_table_jakob2019_srgb_scale ---- sRGB, D65.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_SRGB
alwan_f32 const alwan_table_jakob2019_srgb_scale_f32[ALWAN_TABLE_JAKOB2019_SCALE_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_srgb_scale.csv"
};
#endif

/* ---- alwan_table_jakob2019_srgb_coeff ---- sRGB, D65.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_SRGB
alwan_f32 const alwan_table_jakob2019_srgb_coeff_f32[ALWAN_TABLE_JAKOB2019_COEFF_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_srgb_coeff.csv"
};
#endif

/* ---- alwan_table_jakob2019_prophoto_scale ---- ProPhoto RGB, D50.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
alwan_f32 const alwan_table_jakob2019_prophoto_scale_f32[ALWAN_TABLE_JAKOB2019_SCALE_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_prophoto_scale.csv"
};
#endif

/* ---- alwan_table_jakob2019_prophoto_coeff ---- ProPhoto RGB, D50.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
alwan_f32 const alwan_table_jakob2019_prophoto_coeff_f32[ALWAN_TABLE_JAKOB2019_COEFF_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_prophoto_coeff.csv"
};
#endif

/* ---- alwan_table_jakob2019_aces_scale ---- ACES2065-1, D60.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_ACES
alwan_f32 const alwan_table_jakob2019_aces_scale_f32[ALWAN_TABLE_JAKOB2019_SCALE_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_aces_scale.csv"
};
#endif

/* ---- alwan_table_jakob2019_aces_coeff ---- ACES2065-1, D60.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_ACES
alwan_f32 const alwan_table_jakob2019_aces_coeff_f32[ALWAN_TABLE_JAKOB2019_COEFF_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_aces_coeff.csv"
};
#endif

/* ---- alwan_table_jakob2019_rec2020_scale ---- Rec.2020, D65.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_REC2020
alwan_f32 const alwan_table_jakob2019_rec2020_scale_f32[ALWAN_TABLE_JAKOB2019_SCALE_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_rec2020_scale.csv"
};
#endif

/* ---- alwan_table_jakob2019_rec2020_coeff ---- Rec.2020, D65.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_REC2020
alwan_f32 const alwan_table_jakob2019_rec2020_coeff_f32[ALWAN_TABLE_JAKOB2019_COEFF_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_rec2020_coeff.csv"
};
#endif

/* ---- alwan_table_jakob2019_ergb_scale ---- eRGB, illuminant E.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_ERGB
alwan_f32 const alwan_table_jakob2019_ergb_scale_f32[ALWAN_TABLE_JAKOB2019_SCALE_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_ergb_scale.csv"
};
#endif

/* ---- alwan_table_jakob2019_ergb_coeff ---- eRGB, illuminant E.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_ERGB
alwan_f32 const alwan_table_jakob2019_ergb_coeff_f32[ALWAN_TABLE_JAKOB2019_COEFF_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_ergb_coeff.csv"
};
#endif

/* ---- alwan_table_jakob2019_xyz_scale ---- CIE XYZ, illuminant E.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_XYZ
alwan_f32 const alwan_table_jakob2019_xyz_scale_f32[ALWAN_TABLE_JAKOB2019_SCALE_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_xyz_scale.csv"
};
#endif

/* ---- alwan_table_jakob2019_xyz_coeff ---- CIE XYZ, illuminant E.
 * Reader: alwan__jakob2019_fetch. f32 in every build: both precisions read it. */
#if ALWAN_TABLE_JAKOB2019_XYZ
alwan_f32 const alwan_table_jakob2019_xyz_coeff_f32[ALWAN_TABLE_JAKOB2019_COEFF_SIZE] = {
#include "spectral_lut/jakob2019/jakob2019_xyz_coeff.csv"
};
#endif


ALWAN_DIAG_POP
