/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Embedded table definitions: the AgX Blender 57^3 cube.
 *
 * Alone in its own translation unit because its CSV is 15 MB on one line
 * and is preprocessed twice, once per precision. See alwan_data_tables.c
 * for the split rationale.
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

/* ---- alwan_table_agx_blender_cube ----
 * rank 3, 57^3 x 3, TETRAHEDRAL. Layout r-fastest,
 * index = ((b*RES + g)*RES + r)*3 + ch. Values are power-2.4 encoded display
 * output. Reader: alwan_table3d_sample_tetrahedral via agx_blender_lut3d_sample. */
#if ALWAN_TABLE_AGX_BLENDER_CUBE
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_agx_blender_cube_f32[ALWAN_TABLE_AGX_BLENDER_CUBE_SIZE] = {
#include "agx_blender_lut3d.csv"
};
#endif
#if ALWAN_WITH_F64
alwan_f64 const alwan_table_agx_blender_cube_f64[ALWAN_TABLE_AGX_BLENDER_CUBE_SIZE] = {
#include "agx_blender_lut3d.csv"
};
#endif
#endif


ALWAN_DIAG_POP
