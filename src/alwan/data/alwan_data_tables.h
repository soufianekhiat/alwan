/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * THE SINGLE PLACE. Every embedded array reached through a FLOAT coordinate is
 * declared here, next to the name of the reader that reads it. Open this file
 * to find any table.
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
 * DEFINITIONS live in data/alwan_data_tables*.c, split by MEASURED compile
 * weight rather than taste (see the head comment of each). READERS live in
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
#if ALWAN_WITH_F64
#  define ALWAN_TABLE_EXTERN_F64(name, extent) extern alwan_f64 const name##_f64[extent];
#else
#  define ALWAN_TABLE_EXTERN_F64(name, extent)
#endif

/* One table, both precisions, one extent. The extent cannot drift between the
 * twins because there is only one of it. */
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
 * ACES 2.0 cusp / reach_m hue tables (api/alwan_aces_ff.c) and ACES 1.x
 *   B-spline knot coefficients (core/alwan_aces_ff_core.inc) ARE indexed by a
 *   float, but every site already guards: the hue lookups clamp with
 *   `if (!(h > -1e6 && h < 1e6)) h = 0;` before the cast, and the spline sites
 *   clamp the integer after it. They are runtime-built parameters inside a
 *   struct rather than embedded arrays, so they would not belong in a
 *   data/*.c either. Folding them onto the shared gate is phase 2 tidying, not
 *   a crash fix.
 *
 * ================================================================
 * MIGRATION NOTE -- what has NOT moved, and in what order it should
 *
 * ~600 further file-scope arrays live in api/*.c and core/*.h. They are NOT
 * moved here, deliberately:
 *
 *   * The copy-shaped tables (38 illuminant SPDs, 21 observer CMFs,
 *     wright_guild, camera sensitivities in api/alwan_spd_impl.inc) are bulk
 *     copied into an alwan_spd, so the ARRAYS themselves carry no float index
 *     and move for tidiness and the enable switches only.
 *
 *     But the alwan_spd they are copied into IS sampled by a float, and that
 *     site needs a second look before it is called clean. spd_interpolate
 *     (api/alwan_spd_impl.inc) guards with `if (f_index <= 0)` and
 *     `if (f_index >= count - 1)`, both false for NaN, and then indexes with
 *     `size_t i0 = (size_t)f_index` and no post-cast clamp. Reached through
 *     the public alwan_spd_resample_* with a NaN wavelength range.
 *
 *     It does NOT fault on x86-64: (size_t)NaN yields exactly 2^63 there, and
 *     2^63 * sizeof(element) wraps to a byte offset of 0, so the read lands on
 *     values[0] and the output is NaN. That is undefined behaviour surviving
 *     on an arithmetic coincidence, not a guarantee -- an ABI where the
 *     conversion saturates to SIZE_MAX instead reads off the front of the
 *     buffer. It is left alone in THIS pass on purpose: it is not a reproduced
 *     crash, and routing it through the gate would change a NaN wavelength
 *     from producing NaN to producing values[0], which is a behaviour change
 *     in spectral output that deserves its own commit and its own reference
 *     comparison. tools/check_table_registry.py keeps the site in its
 *     inventory so it cannot be quietly forgotten.
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
