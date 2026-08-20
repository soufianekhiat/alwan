/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Per-table enable switches for the embedded data tables.
 *
 * NOT IMPLEMENTED YET. Every switch defaults to 1 and every definition block in
 * data/alwan_data_tables*.c is already wrapped in its switch, so turning the
 * feature on later flips defaults here instead of editing forty blocks. Nothing
 * in the library reads these for any purpose other than that wrapping.
 *
 * Two rules are already binding, because they are expensive to retrofit:
 *
 *   1. TABLE IDENTITY IS PINNED. Table names are never renamed, never reused,
 *      and new tables append. If identity were positional, disabling one table
 *      would silently renumber every table after it and any cached id or
 *      cross-version fixture would start reading the wrong data.
 *
 *   2. THE PUBLIC SURFACE IS CONFIGURATION-INVARIANT. Every reader declared in
 *      alwan.h is declared and exported in every configuration. Disabling a
 *      table changes what a reader RETURNS (ALWAN_E_NODATA), never whether it
 *      LINKS. Without this, buildsystem/alwan_exports.def would need one
 *      variant per configuration and a binary built with one table set would
 *      fail to load against a header from another.
 *
 * Compile-time only. This is unrelated to ALWAN_EMBED_DATA=0 runtime loading,
 * which is documented as unimplemented and planned for 3.0.0.
 */

#ifndef ALWAN_DATA_TABLES_CONFIG_H
#define ALWAN_DATA_TABLES_CONFIG_H

/* Set to 1 to flip every default off, then opt tables back in one -D at a
 * time. Both directions work without editing the library. */
#ifndef ALWAN_DATA_TABLES_MINIMAL
#  define ALWAN_DATA_TABLES_MINIMAL 0
#endif

/* Group switches. Per-table granularity is noise for most integrators, so a
 * group sets its members' defaults and "no spectral data" is one -D. */
#ifndef ALWAN_TABLES_AGX
#  define ALWAN_TABLES_AGX (!ALWAN_DATA_TABLES_MINIMAL)
#endif
#ifndef ALWAN_TABLES_SPECTRAL
#  define ALWAN_TABLES_SPECTRAL (!ALWAN_DATA_TABLES_MINIMAL)
#endif

/* --- AgX --- */
#ifndef ALWAN_TABLE_AGX_DEFAULT_CONTRAST
#  define ALWAN_TABLE_AGX_DEFAULT_CONTRAST ALWAN_TABLES_AGX
#endif
#ifndef ALWAN_TABLE_AGX_SB2383_CONTRAST
#  define ALWAN_TABLE_AGX_SB2383_CONTRAST ALWAN_TABLES_AGX
#endif
#ifndef ALWAN_TABLE_AGX_BLENDER_CUBE
#  define ALWAN_TABLE_AGX_BLENDER_CUBE ALWAN_TABLES_AGX
#endif

/* --- Jakob 2019 spectral upsampling --- */
#ifndef ALWAN_TABLE_JAKOB2019_SRGB
#  define ALWAN_TABLE_JAKOB2019_SRGB ALWAN_TABLES_SPECTRAL
#endif
#ifndef ALWAN_TABLE_JAKOB2019_PROPHOTO
#  define ALWAN_TABLE_JAKOB2019_PROPHOTO ALWAN_TABLES_SPECTRAL
#endif
#ifndef ALWAN_TABLE_JAKOB2019_ACES
#  define ALWAN_TABLE_JAKOB2019_ACES ALWAN_TABLES_SPECTRAL
#endif
#ifndef ALWAN_TABLE_JAKOB2019_REC2020
#  define ALWAN_TABLE_JAKOB2019_REC2020 ALWAN_TABLES_SPECTRAL
#endif
#ifndef ALWAN_TABLE_JAKOB2019_ERGB
#  define ALWAN_TABLE_JAKOB2019_ERGB ALWAN_TABLES_SPECTRAL
#endif
#ifndef ALWAN_TABLE_JAKOB2019_XYZ
#  define ALWAN_TABLE_JAKOB2019_XYZ ALWAN_TABLES_SPECTRAL
#endif

#endif /* ALWAN_DATA_TABLES_CONFIG_H */
