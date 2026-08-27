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
/* Every illuminant SPD, observer CMF and camera sensitivity: the 360-830nm
 * 1nm curves that alwan_spd_illuminant_*, alwan_xyz_from_spd_* and
 * alwan_spd_camera_sensitivity_* copy into an alwan_spd. */
#ifndef ALWAN_TABLES_SPD
#  define ALWAN_TABLES_SPD (!ALWAN_DATA_TABLES_MINIMAL)
#endif
/* CRI / CQS / TM-30 / SSI reference data: the Robertson CCT locus and the
 * TCS / VS / CES reflectance sample sets. */
#ifndef ALWAN_TABLES_QUALITY
#  define ALWAN_TABLES_QUALITY (!ALWAN_DATA_TABLES_MINIMAL)
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

/* --- Illuminant SPDs, one switch per alwan_illuminant --- */
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_A
#  define ALWAN_TABLE_SPD_ILLUMINANT_A ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D50
#  define ALWAN_TABLE_SPD_ILLUMINANT_D50 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D55
#  define ALWAN_TABLE_SPD_ILLUMINANT_D55 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D65
#  define ALWAN_TABLE_SPD_ILLUMINANT_D65 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_E
#  define ALWAN_TABLE_SPD_ILLUMINANT_E ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F1
#  define ALWAN_TABLE_SPD_ILLUMINANT_F1 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F2
#  define ALWAN_TABLE_SPD_ILLUMINANT_F2 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F3
#  define ALWAN_TABLE_SPD_ILLUMINANT_F3 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F4
#  define ALWAN_TABLE_SPD_ILLUMINANT_F4 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F5
#  define ALWAN_TABLE_SPD_ILLUMINANT_F5 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F6
#  define ALWAN_TABLE_SPD_ILLUMINANT_F6 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F7
#  define ALWAN_TABLE_SPD_ILLUMINANT_F7 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F8
#  define ALWAN_TABLE_SPD_ILLUMINANT_F8 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F9
#  define ALWAN_TABLE_SPD_ILLUMINANT_F9 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F10
#  define ALWAN_TABLE_SPD_ILLUMINANT_F10 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F11
#  define ALWAN_TABLE_SPD_ILLUMINANT_F11 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_F12
#  define ALWAN_TABLE_SPD_ILLUMINANT_F12 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_B
#  define ALWAN_TABLE_SPD_ILLUMINANT_B ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_C
#  define ALWAN_TABLE_SPD_ILLUMINANT_C ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D60
#  define ALWAN_TABLE_SPD_ILLUMINANT_D60 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D75
#  define ALWAN_TABLE_SPD_ILLUMINANT_D75 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D40
#  define ALWAN_TABLE_SPD_ILLUMINANT_D40 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D45
#  define ALWAN_TABLE_SPD_ILLUMINANT_D45 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_D93
#  define ALWAN_TABLE_SPD_ILLUMINANT_D93 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_B1
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_B1 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_B2
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_B2 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_B3
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_B3 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_B4
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_B4 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_B5
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_B5 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_BH1
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_BH1 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_RGB1
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_RGB1 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_V1
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_V1 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_LED_V2
#  define ALWAN_TABLE_SPD_ILLUMINANT_LED_V2 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_HP1
#  define ALWAN_TABLE_SPD_ILLUMINANT_HP1 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_HP2
#  define ALWAN_TABLE_SPD_ILLUMINANT_HP2 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_HP3
#  define ALWAN_TABLE_SPD_ILLUMINANT_HP3 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_HP4
#  define ALWAN_TABLE_SPD_ILLUMINANT_HP4 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_SPD_ILLUMINANT_HP5
#  define ALWAN_TABLE_SPD_ILLUMINANT_HP5 ALWAN_TABLES_SPD
#endif

/* --- Observer colour matching functions, one switch per observer --- */
#ifndef ALWAN_TABLE_CMF_CIE_1931_2DEG
#  define ALWAN_TABLE_CMF_CIE_1931_2DEG ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CMF_CIE_1964_10DEG
#  define ALWAN_TABLE_CMF_CIE_1964_10DEG ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CMF_CIE_2012_2DEG
#  define ALWAN_TABLE_CMF_CIE_2012_2DEG ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CMF_CIE_2012_10DEG
#  define ALWAN_TABLE_CMF_CIE_2012_10DEG ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CMF_STOCKMAN_SHARPE_2DEG
#  define ALWAN_TABLE_CMF_STOCKMAN_SHARPE_2DEG ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CMF_CIE_2015_2DEG
#  define ALWAN_TABLE_CMF_CIE_2015_2DEG ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CMF_CIE_2015_10DEG
#  define ALWAN_TABLE_CMF_CIE_2015_10DEG ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CMF_WRIGHT_GUILD_1931
#  define ALWAN_TABLE_CMF_WRIGHT_GUILD_1931 ALWAN_TABLES_SPD
#endif

/* --- Camera spectral sensitivities --- */
#ifndef ALWAN_TABLE_CAMERA_NIKON_5100
#  define ALWAN_TABLE_CAMERA_NIKON_5100 ALWAN_TABLES_SPD
#endif
#ifndef ALWAN_TABLE_CAMERA_SIGMA_SDMERILL
#  define ALWAN_TABLE_CAMERA_SIGMA_SDMERILL ALWAN_TABLES_SPD
#endif

/* --- Smits 1999 / Mallett 2019 spectral upsampling bases --- */
#ifndef ALWAN_TABLE_SMITS1999
#  define ALWAN_TABLE_SMITS1999 ALWAN_TABLES_SPECTRAL
#endif
#ifndef ALWAN_TABLE_MALLETT2019
#  define ALWAN_TABLE_MALLETT2019 ALWAN_TABLES_SPECTRAL
#endif

/* --- AgX SB2383 inset matrix --- */
#ifndef ALWAN_TABLE_AGX_SB2383_INSET
#  define ALWAN_TABLE_AGX_SB2383_INSET ALWAN_TABLES_AGX
#endif

/* --- Light quality: CCT locus and reflectance sample sets --- */
#ifndef ALWAN_TABLE_ROBERTSON_LOCUS
#  define ALWAN_TABLE_ROBERTSON_LOCUS ALWAN_TABLES_QUALITY
#endif
#ifndef ALWAN_TABLE_TCS_REFLECTANCE
#  define ALWAN_TABLE_TCS_REFLECTANCE ALWAN_TABLES_QUALITY
#endif
#ifndef ALWAN_TABLE_VS_REFLECTANCE
#  define ALWAN_TABLE_VS_REFLECTANCE ALWAN_TABLES_QUALITY
#endif
#ifndef ALWAN_TABLE_CES_REFLECTANCE
#  define ALWAN_TABLE_CES_REFLECTANCE ALWAN_TABLES_QUALITY
#endif
#ifndef ALWAN_TABLE_SSI_BIN_WEIGHTS
#  define ALWAN_TABLE_SSI_BIN_WEIGHTS ALWAN_TABLES_QUALITY
#endif
#ifndef ALWAN_TABLE_SSI_SPECTRAL_WEIGHTS
#  define ALWAN_TABLE_SSI_SPECTRAL_WEIGHTS ALWAN_TABLES_QUALITY
#endif

#endif /* ALWAN_DATA_TABLES_CONFIG_H */
