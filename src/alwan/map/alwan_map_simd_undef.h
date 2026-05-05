/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map SIMD teardown - undefines all alwan_map_* macros.
 * No include guard (used after each precision pass).
 */

/* Precision selector */
#undef ALWAN_MAP_F32
#undef ALWAN_MAP_F64

/* Name mangling */
#undef ALWAN_MAP_SUFFIX
#undef ALWAN_MAP_NAME
#undef ALWAN_MAP_CAT_
#undef ALWAN_MAP_CAT2_

/* Tile / width */
#undef ALWAN_MAP_SIMD_WIDTH
#undef ALWAN_MAP_TILE_PIXELS

/* Lane types (macros, not typedefs, so we can undef) */
#undef alwan_map_lane
#undef alwan_map_simd
#undef alwan_map_simd_mask

/* SIMD ops */
#undef alwan_map_simd_set1
#undef alwan_map_simd_zero
#undef alwan_map_simd_load
#undef alwan_map_simd_store
#undef alwan_map_simd_add
#undef alwan_map_simd_sub
#undef alwan_map_simd_mul
#undef alwan_map_simd_div
#undef alwan_map_simd_neg
#undef alwan_map_simd_abs
#undef alwan_map_simd_fmadd
#undef alwan_map_simd_fmsub
#undef alwan_map_simd_sqrt
#undef alwan_map_simd_cbrt
#undef alwan_map_simd_pow
#undef alwan_map_simd_exp
#undef alwan_map_simd_log
#undef alwan_map_simd_log2
#undef alwan_map_simd_log10
#undef alwan_map_simd_sin
#undef alwan_map_simd_cos
#undef alwan_map_simd_atan2
#undef alwan_map_simd_floor
#undef alwan_map_simd_ceil
#undef alwan_map_simd_round
#undef alwan_map_simd_trunc
#undef alwan_map_simd_min
#undef alwan_map_simd_max
#undef alwan_map_simd_clamp
#undef alwan_map_simd_rcp
#undef alwan_map_simd_cmpeq
#undef alwan_map_simd_cmplt
#undef alwan_map_simd_cmple
#undef alwan_map_simd_cmpgt
#undef alwan_map_simd_cmpge
#undef alwan_map_simd_select
#undef alwan_map_simd_mask_and
#undef alwan_map_simd_mask_or
#undef alwan_map_simd_pow24
#undef alwan_map_simd_pow_inv24
#undef alwan_map_simd_cbrt_fast
#undef alwan_map_simd_deinterleave3
#undef alwan_map_simd_interleave3
#undef alwan_map_simd_mask_all_set

/* Scalar math wrappers */
#undef alwan_map_scalar_abs
#undef alwan_map_scalar_sqrt
#undef alwan_map_scalar_cbrt
#undef alwan_map_scalar_sin
#undef alwan_map_scalar_cos
#undef alwan_map_scalar_tan
#undef alwan_map_scalar_atan2
#undef alwan_map_scalar_pow
#undef alwan_map_scalar_exp
#undef alwan_map_scalar_log
#undef alwan_map_scalar_log2
#undef alwan_map_scalar_log10
#undef alwan_map_scalar_floor
#undef alwan_map_scalar_ceil
#undef alwan_map_scalar_fmod

/* Direct-paste */
#undef ALWAN_MAP_LIT
#undef ALWAN_MAP_LITV
#undef ALWAN_MAP_LITCAT3_
#undef ALWAN_MAP_LITCAT

/* SIMD helper redirects */
#undef alwan__mat3_mul_simd_map

/* Parameterized norm macros */
#undef ALWAN_MAP_P_NORM_MUL
#undef ALWAN_MAP_P_NORM_ADD
#undef ALWAN_MAP_P_NORM_AFFINE
