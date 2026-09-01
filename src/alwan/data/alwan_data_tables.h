/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * THE SINGLE PLACE. Every embedded array reached through a FLOAT coordinate is
 * declared here, next to the name of the reader that reads it, and so is every
 * embedded array reached through an INTEGER row index. Open this file to find
 * any table.
 *
 * The float-coordinate tables came first, because (int)NaN reading two
 * gigabytes past a LUT is what built the gate. The integer-row tables joined
 * them because the same three problems apply without the crash: an extent
 * spelled as a sizeof at the call site drifts from the array, a CSV #included
 * once per consumer ships four copies of the same bytes, and a table with no
 * declaration has no enable switch. They read through alwan_table_row where
 * the others read through alwan_table_coord -- one gate, one policy, one
 * ALWAN_READ_DATA_NO_BOUND_CHECK.
 *
 * A DECLARATION HEADER, not a comment index: an extern that drifts from its
 * definition fails the build, so this cannot rot the way a hand-maintained
 * list does. tools/check_table_registry.py enforces what the compiler cannot
 * (one definition per extern, same order, a reader for each, and a WHY line
 * for every float-indexed array that stays put).
 *
 * EXTENTS ARE ENUM CONSTANTS, never struct fields. With no /GL in this build a
 * size read through a runtime descriptor becomes a real load inside the index
 * arithmetic -- for a rank-3 table that is two loads and two multiplies per
 * corner, eight corners, per pixel, and the loads alias-block. As an enum it
 * stays an immediate in every translation unit.
 *
 * DEFINITIONS live in data/alwan_data_tables*.c -- four files, split by
 * MEASURED compile weight rather than taste (see the head comment of each).
 * data/alwan_data_tables_api.c is the fourth and holds the integer-row tables.
 * tools/check_table_registry.py hardcodes that list, so a fifth file needs an
 * edit there before its definitions count. READERS live in
 * core/alwan_table_core.{inc,h}, because a reader in a .c is either
 * out-of-line -- a real call per pixel, since this build sets no /GL and no
 * /LTCG -- or static-inline and invisible outside one TU. Neither is
 * acceptable on a path running at hundreds of Mpix/s.
 */

#ifndef ALWAN_DATA_TABLES_H
#define ALWAN_DATA_TABLES_H

#include "../alwan_types.h"
#include "alwan_data_tables_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if ALWAN_WITH_F32
#  define ALWAN_TABLE_EXTERN_F32(name, extent) extern alwan_f32 const name##_f32[extent];
#else
#  define ALWAN_TABLE_EXTERN_F32(name, extent)
#endif
/* Declared in every build, not only an f64 one. A declaration costs nothing,
 * and the f64-internal facades read f64 tables from their f32 entry points, so
 * gating the declaration on ALWAN_WITH_F64 deletes the table out from under the
 * f32 API exactly as the ALWAN_TABLE_EXTERN_F64_ONLY comment below describes.
 * The definitions are gated the same way, on ALWAN_WITH_F64_FACADE. */
#define ALWAN_TABLE_EXTERN_F64(name, extent) extern alwan_f64 const name##_f64[extent];

/* One table, both precisions, one extent. The extent cannot drift between the
 * twins because there is only one of it. */
/* Reference data that is f64 in EVERY build, an f32-only one included.
 *
 * The light-quality metrics (CRI, TM-30, SSI, Robertson CCT) are defined on f64
 * reference tables, and BOTH precisions read the f64 table on purpose so the
 * f32 entry points return the same numbers as the f64 ones -- see the comment
 * at the head of api/alwan_quality.c. Declaring these through the dual
 * ALWAN_TABLE_EXTERN gates the f64 half on ALWAN_WITH_F64, which deletes the
 * table out from under the f32 API: ALWAN_BUILD_ONLY_F32 stopped compiling the
 * moment these moved into the registry, because no solution configuration
 * builds that switch. They are read once per evaluation, never per pixel, so
 * carrying f64 in an f32-only build costs ~42 KB of .rdata and no speed.
 *
 * Declaring them this way also means no f32 twin is emitted, which is correct:
 * nothing reads one. */
#define ALWAN_TABLE_EXTERN_F64_ONLY(name, extent) extern alwan_f64 const name##_f64[extent];

#define ALWAN_TABLE_EXTERN(name, extent) \
    ALWAN_TABLE_EXTERN_F32(name, extent) \
    ALWAN_TABLE_EXTERN_F64(name, extent)

/* ================================================================
 * HOMED HERE -- rank 1, scalar element
 * ================================================================ */

/* Both AgX contrast curves are 4096-entry sigmoids over normalized log2. */
enum { ALWAN_TABLE_AGX_CONTRAST_SIZE = 4096 };

/* ---- agx_default_contrast -- rank 1, 4096, LINEAR (delta blend) ------------
 * Reader: agx_lut_eval -> alwan_table1d_sample_linear_delta
 * Source: sobotka/AgX LUTs/AgX_Default_Contrast.spi1d
 *         via alwan_dev/gendata/data/agx_contrast_lut.py */
#if ALWAN_TABLE_AGX_DEFAULT_CONTRAST
ALWAN_TABLE_EXTERN(alwan_table_agx_default_contrast, ALWAN_TABLE_AGX_CONTRAST_SIZE)
#endif

/* ---- agx_sb2383_contrast -- rank 1, 4096, LINEAR (delta blend) -------------
 * Reader: agx_lut_eval -> alwan_table1d_sample_linear_delta
 * Source: sobotka/SB2383-Configuration-Generation, Jed Smith tunable sigmoid
 *         via alwan_dev/gendata/data/agx_sb2383.py */
#if ALWAN_TABLE_AGX_SB2383_CONTRAST
ALWAN_TABLE_EXTERN(alwan_table_agx_sb2383_contrast, ALWAN_TABLE_AGX_CONTRAST_SIZE)
#endif

/* ================================================================
 * HOMED HERE -- rank 3, interleaved RGB cube, R-fastest
 * ================================================================ */

/* ---- agx_blender_cube -- rank 3, 57^3 x 3, TETRAHEDRAL --------------------
 * Reader: agx_blender_lut3d_sample -> alwan_table3d_sample_tetrahedral
 * Source: extern/agx/blender/luts/AgX_Base_sRGB.cube
 *         via alwan_dev/gendata/data/agx_blender.py
 * Values are power-2.4 encoded display output. */
enum {
    ALWAN_TABLE_AGX_BLENDER_CUBE_RES  = 57,
    ALWAN_TABLE_AGX_BLENDER_CUBE_SIZE = 57 * 57 * 57 * 3
};
#if ALWAN_TABLE_AGX_BLENDER_CUBE
ALWAN_TABLE_EXTERN(alwan_table_agx_blender_cube, ALWAN_TABLE_AGX_BLENDER_CUBE_SIZE)
#endif

/* ================================================================
 * HOMED HERE -- rank 3, PLANAR scalar cubes, B-fastest
 *
 * Jakob 2019 stores three independent polynomial coefficient fields per gamut,
 * so these are three scalar cubes rather than one interleaved RGB cube, and
 * the index runs b-fastest. That is why they are read by
 * alwan_jakob2019_coeff_sample_* and NOT by alwan_table3d_sample_trilinear,
 * whose layout is interleaved and r-fastest. They share the addressing gate.
 * ================================================================ */

enum {
    ALWAN_TABLE_JAKOB2019_RES  = 64,
    ALWAN_TABLE_JAKOB2019_SIZE = 64 * 64 * 64
};

/* ---- jakob2019_srgb c0/c1/c2 -- rank 3, 64^3, TRILINEAR------------------
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}
 * Source: alwan_dev/gendata/data/jakob2019.py */
#if ALWAN_TABLE_JAKOB2019_SRGB
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_srgb_c0, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_srgb_c1, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_srgb_c2, ALWAN_TABLE_JAKOB2019_SIZE)
#endif

/* ---- jakob2019_prophoto c0/c1/c2 -- rank 3, 64^3, TRILINEAR--------------
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}
 * Source: alwan_dev/gendata/data/jakob2019.py */
#if ALWAN_TABLE_JAKOB2019_PROPHOTO
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_prophoto_c0, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_prophoto_c1, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_prophoto_c2, ALWAN_TABLE_JAKOB2019_SIZE)
#endif

/* ---- jakob2019_aces c0/c1/c2 -- rank 3, 64^3, TRILINEAR------------------
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}
 * Source: alwan_dev/gendata/data/jakob2019.py */
#if ALWAN_TABLE_JAKOB2019_ACES
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_aces_c0, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_aces_c1, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_aces_c2, ALWAN_TABLE_JAKOB2019_SIZE)
#endif

/* ---- jakob2019_rec2020 c0/c1/c2 -- rank 3, 64^3, TRILINEAR---------------
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}
 * Source: alwan_dev/gendata/data/jakob2019.py */
#if ALWAN_TABLE_JAKOB2019_REC2020
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_rec2020_c0, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_rec2020_c1, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_rec2020_c2, ALWAN_TABLE_JAKOB2019_SIZE)
#endif

/* ---- jakob2019_ergb c0/c1/c2 -- rank 3, 64^3, TRILINEAR------------------
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}
 * Source: alwan_dev/gendata/data/jakob2019.py */
#if ALWAN_TABLE_JAKOB2019_ERGB
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_ergb_c0, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_ergb_c1, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_ergb_c2, ALWAN_TABLE_JAKOB2019_SIZE)
#endif

/* ---- jakob2019_xyz c0/c1/c2 -- rank 3, 64^3, TRILINEAR-------------------
 * Reader: alwan_jakob2019_coeff_sample_{f32,f64}
 * Source: alwan_dev/gendata/data/jakob2019.py */
#if ALWAN_TABLE_JAKOB2019_XYZ
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_xyz_c0, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_xyz_c1, ALWAN_TABLE_JAKOB2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_jakob2019_xyz_c2, ALWAN_TABLE_JAKOB2019_SIZE)
#endif


/* ================================================================
 * HOMED HERE -- rank 1 and rank 2, INTEGER row index
 *
 * These are not sampled by a float coordinate, so they are not the
 * crash class the gate was built for. They are here for the other
 * three reasons the registry exists: the extent stops being a
 * sizeof at the call site, the payload stops being #included once
 * per consumer, and each table gets an enable switch. Their reads go
 * through alwan_table_row for the same reason the float readers go
 * through alwan_table_coord -- one gate, one policy, one switch.
 * ================================================================ */

/* Every illuminant SPD, every observer CMF and every camera
 * sensitivity in this library is sampled 360-830nm at 1nm. One extent
 * for all 68 of them, so a regenerated CSV that changes length fails
 * the build instead of silently truncating a copy loop. */
enum { ALWAN_TABLE_SPD_360_830_1NM_SIZE = 471 };

/* ---- spd_illuminant_* -- 38 tables, rank 1, 471, INTEGER row --------
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, colour-science SDS_ILLUMINANTS, 360-830nm
 *         at 1nm. One table per alwan_illuminant, copied whole into an
 *         alwan_spd by alwan_spd_illuminant_*.
 * Declared in the order api/alwan_spd_impl.inc switches on:
 *   A D50 D55 D65 E F1..F12 B C D60 D75 D40 D45 D93
 *   LED-B1..B5 LED-BH1 LED-RGB1 LED-V1 LED-V2 HP1..HP5 */
#if ALWAN_TABLE_SPD_ILLUMINANT_A
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_a, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D50
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d50, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D55
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d55, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D65
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d65, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_E
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_e, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F1
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f1, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F2
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f2, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F3
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f3, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F4
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f4, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F5
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f5, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F6
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f6, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F7
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f7, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F8
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f8, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F9
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f9, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F10
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f10, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F11
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f11, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_F12
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_f12, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_B
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_b, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_C
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_c, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D60
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d60, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D75
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d75, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D40
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d40, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D45
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d45, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_D93
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_d93, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B1
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_b1, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B2
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_b2, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B3
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_b3, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B4
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_b4, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_B5
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_b5, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_BH1
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_bh1, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_RGB1
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_rgb1, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_V1
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_v1, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_LED_V2
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_led_v2, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_HP1
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_hp1, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_HP2
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_hp2, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_HP3
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_hp3, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_HP4
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_hp4, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif
#if ALWAN_TABLE_SPD_ILLUMINANT_HP5
ALWAN_TABLE_EXTERN(alwan_table_spd_illuminant_hp5, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_cie_1931_2deg x/y/z -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, CIE 1931 2 deg Standard Observer,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_CIE_1931_2DEG
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_1931_2deg_x, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_1931_2deg_y, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_1931_2deg_z, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_cie_1964_10deg x/y/z -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, CIE 1964 10 deg Standard Observer,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_CIE_1964_10DEG
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_1964_10deg_x, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_1964_10deg_y, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_1964_10deg_z, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_cie_2012_2deg x/y/z -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, CIE 2012 2 deg Standard Observer,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_CIE_2012_2DEG
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2012_2deg_x, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2012_2deg_y, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2012_2deg_z, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_cie_2012_10deg x/y/z -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, CIE 2012 10 deg Standard Observer,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_CIE_2012_10DEG
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2012_10deg_x, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2012_10deg_y, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2012_10deg_z, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_stockman_sharpe_2deg x/y/z -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, Stockman and Sharpe 2000 2 deg cone fundamentals,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_STOCKMAN_SHARPE_2DEG
ALWAN_TABLE_EXTERN(alwan_table_cmf_stockman_sharpe_2deg_x, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_stockman_sharpe_2deg_y, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_stockman_sharpe_2deg_z, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_cie_2015_2deg x/y/z -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, CIE 2015 2 deg cone fundamental observer,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_CIE_2015_2DEG
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2015_2deg_x, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2015_2deg_y, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2015_2deg_z, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_cie_2015_10deg x/y/z -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, CIE 2015 10 deg cone fundamental observer,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_CIE_2015_10DEG
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2015_10deg_x, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2015_10deg_y, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_cie_2015_10deg_z, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- cmf_wright_guild_1931 r/g/b -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, Wright and Guild 1931 2 deg RGB CMFs (historical),
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CMF_WRIGHT_GUILD_1931
ALWAN_TABLE_EXTERN(alwan_table_cmf_wright_guild_1931_r, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_wright_guild_1931_g, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_cmf_wright_guild_1931_b, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- camera_nikon_5100 r/g/b -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, Nikon D5100 spectral sensitivities,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CAMERA_NIKON_5100
ALWAN_TABLE_EXTERN(alwan_table_camera_nikon_5100_r, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_camera_nikon_5100_g, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_camera_nikon_5100_b, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ---- camera_sigma_sdmerill r/g/b -- rank 1, 471, INTEGER row ----
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: alwan_dev/gendata, Sigma SD Merrill spectral sensitivities,
 *         360-830nm at 1nm. */
#if ALWAN_TABLE_CAMERA_SIGMA_SDMERILL
ALWAN_TABLE_EXTERN(alwan_table_camera_sigma_sdmerill_r, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_camera_sigma_sdmerill_g, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_camera_sigma_sdmerill_b, ALWAN_TABLE_SPD_360_830_1NM_SIZE)
#endif

/* ================================================================
 * HOMED HERE -- spectral upsampling basis spectra, rank 1
 * ================================================================ */

enum { ALWAN_TABLE_SMITS1999_SIZE = 10 };

/* ---- smits1999 white/cyan/magenta/yellow/red/green/blue -------------
 * rank 1, 10, INTEGER row.
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: Smits 1999, An RGB to Spectrum Conversion for Reflectances.
 *         380-720nm at 10 samples, illuminant E, sRGB primaries. */
#if ALWAN_TABLE_SMITS1999
ALWAN_TABLE_EXTERN(alwan_table_smits1999_white, ALWAN_TABLE_SMITS1999_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_smits1999_cyan, ALWAN_TABLE_SMITS1999_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_smits1999_magenta, ALWAN_TABLE_SMITS1999_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_smits1999_yellow, ALWAN_TABLE_SMITS1999_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_smits1999_red, ALWAN_TABLE_SMITS1999_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_smits1999_green, ALWAN_TABLE_SMITS1999_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_smits1999_blue, ALWAN_TABLE_SMITS1999_SIZE)
#endif

enum { ALWAN_TABLE_MALLETT2019_SIZE = 81 };

/* ---- mallett2019 red/green/blue -- rank 1, 81, INTEGER row ----------
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: Mallett and Yuksel 2019 sRGB reflectance basis, 380-780nm. */
#if ALWAN_TABLE_MALLETT2019
ALWAN_TABLE_EXTERN(alwan_table_mallett2019_red, ALWAN_TABLE_MALLETT2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_mallett2019_green, ALWAN_TABLE_MALLETT2019_SIZE)
ALWAN_TABLE_EXTERN(alwan_table_mallett2019_blue, ALWAN_TABLE_MALLETT2019_SIZE)
#endif

enum {
    ALWAN_TABLE_AGX_SB2383_INSET_ROWS = 3,
    ALWAN_TABLE_AGX_SB2383_INSET_SIZE = 3 * 3
};

/* ---- agx_sb2383_inset -- 3x3 row-major read flat, INTEGER row -------
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: sobotka/SB2383-Configuration-Generation via
 *         alwan_dev/gendata/data/agx_sb2383.py. The same CSV the SB2383
 *         view bakes from, so there are no duplicated matrix constants. */
#if ALWAN_TABLE_AGX_SB2383_INSET
ALWAN_TABLE_EXTERN(alwan_table_agx_sb2383_inset, ALWAN_TABLE_AGX_SB2383_INSET_SIZE)
#endif

/* ================================================================
 * HOMED HERE -- light-quality tables (CCT locus, reflectance sets)
 *
 * The reflectance sets were 109 separate one-CSV arrays plus three
 * arrays of pointers to them. Flattened to one row-major table each:
 * the pointer indirection is what let a sample index run past the
 * end of the set without the compiler seeing it.
 * ================================================================ */

enum {
    ALWAN_TABLE_ROBERTSON_ROWS   = 31,
    ALWAN_TABLE_ROBERTSON_STRIDE = 4,
    ALWAN_TABLE_ROBERTSON_SIZE   = 31 * 4
};

/* ---- robertson_locus -- rank 2, 31 x 4, INTEGER row -----------------
 * Reader: alwan_table2d_row_at_{f32,f64}
 * Source: Robertson 1968 isotemperature lines, r = 0..600 MRD^-1.
 *         Stride 4: reciprocal_mrd, u, v, slope. */
#if ALWAN_TABLE_ROBERTSON_LOCUS
ALWAN_TABLE_EXTERN_F64_ONLY(alwan_table_robertson_locus, ALWAN_TABLE_ROBERTSON_SIZE)
#endif

/* SAMPLES is how many samples; SPECTRUM is how many wavelengths per
 * sample. Both were spelled *_COUNT in api/alwan_quality.c, where
 * CES_COUNT meant 95 wavelengths and the sample count 80 was a bare
 * literal at the loop. That is how an 80-vs-99 edit goes wrong. */
enum {
    ALWAN_TABLE_QUALITY_SPECTRUM = 95,   /* 360-830nm at 5nm */
    ALWAN_TABLE_TCS_SAMPLES      = 14,
    ALWAN_TABLE_TCS_SIZE         = 14 * 95,
    ALWAN_TABLE_VS_SAMPLES       = 15,
    ALWAN_TABLE_VS_SIZE          = 15 * 95,
    ALWAN_TABLE_CES_SAMPLES      = 99,
    ALWAN_TABLE_CES_SIZE         = 99 * 95
};

/* ---- tcs_reflectance -- rank 2, 14 x 95, INTEGER row ----------------
 * Reader: alwan_table2d_row_at_{f32,f64}
 * Source: CIE 13.3-1995 test colour samples TCS01..TCS14, 360-830nm at
 *         5nm. Row-major, one row per sample. The 14 per-sample CSVs
 *         concatenate inside one initializer because each ends with a
 *         trailing comma, so the flattening is the same tokens in the
 *         same order and cannot move a value. */
#if ALWAN_TABLE_TCS_REFLECTANCE
ALWAN_TABLE_EXTERN_F64_ONLY(alwan_table_tcs_reflectance, ALWAN_TABLE_TCS_SIZE)
#endif

/* ---- vs_reflectance -- rank 2, 15 x 95, INTEGER row -----------------
 * Reader: alwan_table2d_row_at_{f32,f64}
 * Source: CQS VS01..VS15 saturated test samples, 360-830nm at 5nm. */
#if ALWAN_TABLE_VS_REFLECTANCE
ALWAN_TABLE_EXTERN_F64_ONLY(alwan_table_vs_reflectance, ALWAN_TABLE_VS_SIZE)
#endif

/* ---- ces_reflectance -- rank 2, 80 x 95, INTEGER row ----------------
 * Reader: alwan_table2d_row_at_{f32,f64}
 * Source: IES TM-30 / CIE 224 colour evaluation samples, 360-830nm at
 *         5nm. ALWAN_TABLE_CES_SAMPLES is the single number that changes
 *         if the set is ever regenerated at 99 samples -- and the 99.0
 *         divisor in alwan_tm30_rf_* is a SEPARATE, separately-baselined
 *         decision that must not be changed with it. */
#if ALWAN_TABLE_CES_REFLECTANCE
ALWAN_TABLE_EXTERN_F64_ONLY(alwan_table_ces_reflectance, ALWAN_TABLE_CES_SIZE)
#endif

enum {
    ALWAN_TABLE_SSI_BIN_TAPS  = 11,
    ALWAN_TABLE_SSI_BIN_COUNT = 30
};

/* ---- daylight_basis_s012 -- rank 2, 3 x 95, INTEGER row -------------
 * The CIE 15:2004 daylight basis S0/S1/S2 on the 360-830 nm 5 nm quality grid,
 * concatenated in that order. Used to build the CIE 224:2017 / TM-30 reference
 * illuminant at an arbitrary CCT: S = S0 + M1*S1 + M2*S2. Before this existed
 * alwan_tm30_rf_* substituted D65 for every CCT >= 5000 K, which scored D50
 * against D65.
 * Reader: alwan_table2d_row_at_f64
 * Source: fixtures/daylight_basis_s012.csv */
enum {
    ALWAN_TABLE_DAYLIGHT_BASIS_ROWS = 3,
    ALWAN_TABLE_DAYLIGHT_BASIS_SIZE = 3 * 95
};
#if ALWAN_TABLE_DAYLIGHT_BASIS
ALWAN_TABLE_EXTERN_F64_ONLY(alwan_table_daylight_basis, ALWAN_TABLE_DAYLIGHT_BASIS_SIZE)
#endif

/* ---- ssi_bin_weights / ssi_spectral_weights -- rank 1, INTEGER row --
 * Reader: alwan_table1d_row_{f32,f64}
 * Source: Academy S-2018-001 / SMPTE ST 2122. bin_weights is the 11-tap
 *         trapezoidal 10nm binning kernel; spectral_weights is the 30
 *         per-bin weights covering 380-670nm. */
#if ALWAN_TABLE_SSI_BIN_WEIGHTS
ALWAN_TABLE_EXTERN_F64_ONLY(alwan_table_ssi_bin_weights, ALWAN_TABLE_SSI_BIN_TAPS)
#endif
#if ALWAN_TABLE_SSI_SPECTRAL_WEIGHTS
ALWAN_TABLE_EXTERN_F64_ONLY(alwan_table_ssi_spectral_weights, ALWAN_TABLE_SSI_BIN_COUNT)
#endif

/* ================================================================
 * STAYS PUT, AND WHY
 *
 * Compiler-unenforceable, so tools/check_table_registry.py checks it: any
 * file-scope array in src/alwan/ subscripted by a value derived from a float
 * is either declared above or listed here with a reason.
 *
 * MACHADO_PROTAN / MACHADO_DEUTAN / MACHADO_TRITAN -- 11 x mat3x3, indexed by
 *   a float severity.
 *   HOME:   core/alwan_vision_core.inc + core/alwan_vision_core.h
 *   READER: alwan_machado_matrix_sample_{f32,f64}
 *             -> alwan_table1d_mat3_sample_linear
 *   WHY:    ALWAN_CONSTEXPR is `static const`. These live in a header the
 *           HLSL/GLSL/Halide backends compile, and those backends cannot link
 *           against a .c. Externing them would break every GPU backend to
 *           reclaim ~3.5 KB of per-TU duplication across four objects. This is
 *           the one place where "definitions in a single .c" is refused
 *           outright rather than deferred. The crash was still fixed: the
 *           reader routes through the same addressing gate as everything else.
 *
 * ACES2_CHROMA_NORM_COS_V / _SIN_V [4], KRYSTEK_U_COEFFS / _V_COEFFS [6] --
 *   core tier, read at compile-time-constant indices inside Horner unrolls. No
 *   float ever reaches their subscript, so there is no crash class. Listed for
 *   completeness; no reader, and none wanted.
 *
 * astm_e313_yi_coeffs [4][2] (api/alwan_quality.c) -- indexed by the
 *   alwan_astm_e313_illuminant enum, which alwan_yellowness_astm_e313_* and
 *   alwan_whiteness_* validate before use.
 *   HOME:   api/alwan_quality.c
 *   READER: alwan_table_row_{f32,f64}_v, applied at the subscript in place
 *   WHY:    it has no CSV payload. Its eight numbers are published ASTM E313
 *           coefficients written as ALWAN_LITERAL() in the source, so moving
 *           it means retyping them into a data .c -- the one edit in this
 *           migration that could change a value, for a table small enough to
 *           read at a glance. It also carries ALWAN_LITERAL semantics, which
 *           narrow to float in an ALWAN_SCALAR_IS_FLOAT build and would not
 *           survive a move to a plain alwan_f64 definition unchanged. Give it
 *           an explicit first extent and route its subscript through the row
 *           gate where it stands. Same call for astm_e313_white_xy, which has
 *           zero readers anywhere in the tree.
 *
 * ACES 2.0 cusp / reach_m hue tables (api/alwan_aces_ff.c) and ACES 1.x
 *   B-spline knot coefficients (core/alwan_aces_ff_core.inc) ARE indexed by a
 *   float, but every site already guards: the hue lookups clamp with
 *   `if (!(h > -1e6 && h < 1e6)) h = 0;` before the cast, and the spline sites
 *   clamp the integer after it. They are runtime-built parameters inside a
 *   struct rather than embedded arrays, so they would not belong in a
 *   a data table .c either. Folding them onto the shared gate is phase 2 tidying, not
 *   a crash fix.
 *
 * ================================================================
 * MIGRATION NOTE -- what has NOT moved, and in what order it should
 *
 * ~600 further file-scope arrays live in the api and core sources. They are NOT
 * moved here, deliberately:
 *
 *   * DONE. The copy-shaped tables (38 illuminant SPDs, 24 observer CMFs
 *     including wright_guild, 6 camera sensitivities -- all of them 471
 *     samples, 360-830nm at 1nm) are declared above as alwan_table_spd_*,
 *     alwan_table_cmf_* and alwan_table_camera_* and defined in
 *     data/alwan_data_tables_api.c. They carry no float index; they moved for
 *     the extent, the single #include of each CSV, and the enable switches.
 *     The same pass moved the Smits1999 and Mallett2019 upsampling bases, the
 *     AgX SB2383 inset matrix, the Robertson locus, the TCS/VS/CES
 *     reflectance sets and the SSI weights.
 *
 *     The alwan_spd they are copied INTO is still sampled by a float, in
 *     spd_interpolate (api/alwan_spd_impl.inc). That is a runtime ALWAN_ALLOC
 *     buffer, not an embedded table, so it is out of this registry's scope --
 *     and since db7a01c it carries its own explicit NaN-to-low-edge guard
 *     before the `size_t i0 = (size_t)f_index` cast, matching the convention
 *     in core/alwan_table_core. tools/check_table_registry.py keeps the cast
 *     in its inventory so the site cannot be quietly reshaped away.
 *   * The ~290 compile-time-indexed arrays (coeffs[0], m.m[4], Horner unrolls)
 *     are explicitly REJECTED, not deferred. Wrapping them turns folded
 *     immediates into loads inside SIMD kernels and buys no safety.
 *   * The remaining ~20 arrays in api/alwan_data.c and
 *     api/alwan_reference_data.c are mechanical and lowest blast radius; do
 *     them late.
 *
 * ALSO NOT MERGED, AND IT IS A KNOWN INCONSISTENCY. alwan_table_interp_1d_*
 * and alwan_table_interp_3d_{trilinear,tetrahedral}_* (api/alwan_math.c,
 * api/alwan_math_impl.inc) are a THIRD public spelling of table sampling,
 * alongside alwan_lut*_sample_* and alwan_table*_sample_*. They were checked
 * against this crash class and they do NOT crash: their index is a size_t, so
 * (size_t)NaN becomes a huge positive value that the existing
 * `if (idx >= size - 1)` guard catches. They are safe by construction, not by
 * luck, but they resolve a NaN coordinate to the HIGH edge where the gate in
 * core/alwan_table_core resolves it to the LOW edge. Two public families with
 * opposite NaN conventions is a real wart.
 *
 * They are NOT folded onto the gate in this pass, deliberately: they accept
 * three INDEPENDENT per-axis sizes, which the cube readers here do not, and
 * alwan_table_interp_1d_* is keyed on alwan_interp_method (LINEAR/CUBIC)
 * rather than alwan_sample_mode. Merging them means either extending the
 * reader to non-cubic extents or narrowing a shipped API. That is a design
 * decision of its own, not a crash fix, and it belongs in phase 2 with the
 * NaN-convention question settled first. Test 102 pins their current
 * behaviour so the choice stays deliberate.
 *
 * A table's definition must never be removed in a different commit from the
 * one that repoints its consumer: splitting them yields either an
 * unused-static warning or a duplicate symbol.
 * ================================================================ */

#ifdef __cplusplus
}
#endif

#endif /* ALWAN_DATA_TABLES_H */
