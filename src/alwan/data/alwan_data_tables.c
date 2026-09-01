/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Embedded table definitions: rank-1 curves (light, ~200 KB of CSV).
 *
 * WHY THREE .c FILES AND NOT ONE. src/alwan/data holds 73 MB of CSV. Each
 * table is #included twice (f32 and f64 twins from the same CSV), so the
 * AgX Blender cube alone preprocesses to ~30 MB and the Jakob2019 set to
 * ~110 MB. Merging all of it produces a single ~150 MB translation unit on
 * a machine that already OOMs under parallel builds. Split by measured
 * compile weight: light curves here, the 57^3 cube in _cubes.c, the
 * spectral cubes in _spectral.c. Three, not more, and not by taste.
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

/* ---- alwan_table_agx_default_contrast ----
 * rank 1, 4096, LINEAR (delta blend). Reader: alwan_table1d_sample_linear_delta
 * via agx_lut_eval. Source: sobotka/AgX AgX_Default_Contrast.spi1d. */
#if ALWAN_TABLE_AGX_DEFAULT_CONTRAST
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_agx_default_contrast_f32[ALWAN_TABLE_AGX_CONTRAST_SIZE] = {
#include "agx_default_contrast_lut.csv"
};
#endif
/* Compiled in every build: the documented f64-internal facades read these f64
 * tables from their f32 entry points, so the data has to exist even when the
 * f64 public surface is excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_agx_default_contrast_f64[ALWAN_TABLE_AGX_CONTRAST_SIZE] = {
#include "agx_default_contrast_lut.csv"
};
#endif
#endif

/* ---- alwan_table_agx_sb2383_contrast ----
 * rank 1, 4096, LINEAR (delta blend). Reader: alwan_table1d_sample_linear_delta
 * via agx_lut_eval. Source: sobotka/SB2383, Jed Smith tunable sigmoid. */
#if ALWAN_TABLE_AGX_SB2383_CONTRAST
#if ALWAN_WITH_F32
alwan_f32 const alwan_table_agx_sb2383_contrast_f32[ALWAN_TABLE_AGX_CONTRAST_SIZE] = {
#include "agx_sb2383_contrast_lut.csv"
};
#endif
#if ALWAN_WITH_F64_FACADE
alwan_f64 const alwan_table_agx_sb2383_contrast_f64[ALWAN_TABLE_AGX_CONTRAST_SIZE] = {
#include "agx_sb2383_contrast_lut.csv"
};
#endif
#endif


ALWAN_DIAG_POP
