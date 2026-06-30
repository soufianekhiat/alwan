/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map SIMD definitions - included once per precision pass.
 * No include guard (paired with alwan_map_simd_undef.h).
 *
 * Expects before inclusion:
 *   ALWAN_MAP_F32 defined -- for f32 pass
 *   ALWAN_MAP_F64 defined -- for f64 pass
 * (exactly one must be defined)
 */

/* Name mangling */
#define ALWAN_MAP_CAT2_(a, b) a##b
#define ALWAN_MAP_CAT_(a, b)  ALWAN_MAP_CAT2_(a, b)

/* Deterministic mode forces all *_map_kernels.inc SIMD bodies to fall
 * through to their scalar tail loops. Most colorspace kernels diverge
 * from their scalar-equivalent at last bit even with /fp:precise -- the
 * vector path uses precomputed reciprocals (`vx * (1/wx)` instead of
 * `vx / wx`), pre-multiplied constants, and slightly different operation
 * orderings in matrix multiplies. Lane-unpacking each one individually
 * is ~50 helpers; collapsing the SIMD width to 1 in det mode is one
 * line and every kernel benefits. road_to_determinism.md sec 8.
 *
 * Element-wise SIMD usage outside the kernel files (alwan_rgb.c's
 * OETF/EOTF apply functions through alwan_map_internal.h) is unaffected
 * because those gate on ALWAN_SIMD_WIDTH, not ALWAN_MAP_SIMD_WIDTH.
 * Their helpers already lane-unpack to the canonical scalar in det mode.
 */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
#  define ALWAN__MAP_SIMD_WIDTH_F32  1
#  define ALWAN__MAP_SIMD_WIDTH_F64  1
#else
#  define ALWAN__MAP_SIMD_WIDTH_F32  ALWAN_SIMD_F32_WIDTH
#  define ALWAN__MAP_SIMD_WIDTH_F64  ALWAN_SIMD_F64_WIDTH
#endif

#ifdef ALWAN_MAP_F32

#define ALWAN_MAP_SUFFIX       _f32
#define ALWAN_MAP_NAME(base)   ALWAN_MAP_CAT_(base, _f32)
#define ALWAN_MAP_SIMD_WIDTH   ALWAN__MAP_SIMD_WIDTH_F32
#define ALWAN_MAP_TILE_PIXELS  4096
#define alwan_map_lane         float
#define alwan_map_simd         alwan_simd_f32
#define alwan_map_simd_mask    alwan_simd_f32_mask
#define alwan_map_simd_set1(v)      alwan_simd_f32_set1(v)
#define alwan_map_simd_zero         alwan_simd_f32_zero
#define alwan_map_simd_load         alwan_simd_f32_load
#define alwan_map_simd_store        alwan_simd_f32_store
#define alwan_map_simd_add          alwan_simd_f32_add
#define alwan_map_simd_sub          alwan_simd_f32_sub
#define alwan_map_simd_mul          alwan_simd_f32_mul
#define alwan_map_simd_div          alwan_simd_f32_div
#define alwan_map_simd_neg          alwan_simd_f32_neg
#define alwan_map_simd_abs          alwan_simd_f32_abs
#define alwan_map_simd_fmadd        alwan_simd_f32_fmadd
#define alwan_map_simd_fmsub        alwan_simd_f32_fmsub
#define alwan_map_simd_sqrt         alwan_simd_f32_sqrt
#define alwan_map_simd_cbrt         alwan_simd_f32_cbrt
#define alwan_map_simd_pow          alwan_simd_f32_pow
#define alwan_map_simd_exp          alwan_simd_f32_exp
#define alwan_map_simd_log          alwan_simd_f32_log
#define alwan_map_simd_log2         alwan_simd_f32_log2
#define alwan_map_simd_log10        alwan_simd_f32_log10
#define alwan_map_simd_sin          alwan_simd_f32_sin
#define alwan_map_simd_cos          alwan_simd_f32_cos
#define alwan_map_simd_atan2        alwan_simd_f32_atan2
#define alwan_map_simd_floor        alwan_simd_f32_floor
#define alwan_map_simd_ceil         alwan_simd_f32_ceil
#define alwan_map_simd_round        alwan_simd_f32_round
#define alwan_map_simd_trunc        alwan_simd_f32_trunc
#define alwan_map_simd_min          alwan_simd_f32_min
#define alwan_map_simd_max          alwan_simd_f32_max
#define alwan_map_simd_clamp        alwan_simd_f32_clamp
#define alwan_map_simd_rcp          alwan_simd_f32_rcp
#define alwan_map_simd_cmpeq        alwan_simd_f32_cmpeq
#define alwan_map_simd_cmplt        alwan_simd_f32_cmplt
#define alwan_map_simd_cmple        alwan_simd_f32_cmple
#define alwan_map_simd_cmpgt        alwan_simd_f32_cmpgt
#define alwan_map_simd_cmpge        alwan_simd_f32_cmpge
#define alwan_map_simd_select       alwan_simd_f32_select
#define alwan_map_simd_mask_and     alwan_simd_f32_mask_and
#define alwan_map_simd_mask_or      alwan_simd_f32_mask_or
#define alwan_map_simd_pow24        alwan_simd_f32_pow24
#define alwan_map_simd_pow_inv24    alwan_simd_f32_pow_inv24
#define alwan_map_simd_cbrt_fast    alwan_simd_f32_cbrt_fast
#define alwan_map_simd_deinterleave3 alwan_simd_f32_deinterleave3
#define alwan_map_simd_interleave3   alwan_simd_f32_interleave3
#define alwan_map_simd_mask_all_set  alwan_simd_f32_mask_all_set

/* Scalar math wrappers (precision-parameterized) */
#define alwan_map_scalar_abs(x)       ALWAN_ABS_F32(x)
#define alwan_map_scalar_sqrt(x)      ALWAN_SQRT_F32(x)
#define alwan_map_scalar_cbrt(x)      ALWAN_CBRT_F32(x)
#define alwan_map_scalar_sin(x)       ALWAN_SIN_F32(x)
#define alwan_map_scalar_cos(x)       ALWAN_COS_F32(x)
#define alwan_map_scalar_tan(x)       ALWAN_TAN_F32(x)
#define alwan_map_scalar_atan2(y,x)   ALWAN_ATAN2_F32(y,x)
#define alwan_map_scalar_pow(x,y)     ALWAN_POW_F32(x,y)
#define alwan_map_scalar_exp(x)       ALWAN_EXP_F32(x)
#define alwan_map_scalar_log(x)       ALWAN_LN_F32(x)
#define alwan_map_scalar_log2(x)      ALWAN_LOG2_F32(x)
#define alwan_map_scalar_log10(x)     ALWAN_LOG10_F32(x)
#define alwan_map_scalar_floor(x)     ALWAN_FLOOR_F32(x)
#define alwan_map_scalar_ceil(x)      ALWAN_CEIL_F32(x)
#define alwan_map_scalar_fmod(x,y)    ALWAN_FMOD_F32(x,y)

/* Direct-paste: base is NOT expanded (## at level 1 suppresses expansion) */
#define ALWAN_MAP_LIT(base)         base##_f32
#define ALWAN_MAP_LITV(base)        base##_f32_v
#define ALWAN_MAP_LITCAT3_(a,s,b)   a##s##b
/* ALWAN_MAP_LITCAT: uses ## directly so 'a' is never macro-expanded.
 * Must NOT delegate to ALWAN_MAP_LITCAT3_ as that would expand 'a'
 * (e.g. alwan_xyz_to_oklab -> alwan_xyz_to_oklab_f64 via compat #define). */
#define ALWAN_MAP_LITCAT(a, b)      a##_f32##b

#elif defined(ALWAN_MAP_F64)

#define ALWAN_MAP_SUFFIX       _f64
#define ALWAN_MAP_NAME(base)   ALWAN_MAP_CAT_(base, _f64)
#define ALWAN_MAP_SIMD_WIDTH   ALWAN__MAP_SIMD_WIDTH_F64
#define ALWAN_MAP_TILE_PIXELS  2048
#define alwan_map_lane         double
#define alwan_map_simd         alwan_simd_f64
#define alwan_map_simd_mask    alwan_simd_f64_mask
#define alwan_map_simd_set1(v)      alwan_simd_f64_set1(v)
#define alwan_map_simd_zero         alwan_simd_f64_zero
#define alwan_map_simd_load         alwan_simd_f64_load
#define alwan_map_simd_store        alwan_simd_f64_store
#define alwan_map_simd_add          alwan_simd_f64_add
#define alwan_map_simd_sub          alwan_simd_f64_sub
#define alwan_map_simd_mul          alwan_simd_f64_mul
#define alwan_map_simd_div          alwan_simd_f64_div
#define alwan_map_simd_neg          alwan_simd_f64_neg
#define alwan_map_simd_abs          alwan_simd_f64_abs
#define alwan_map_simd_fmadd        alwan_simd_f64_fmadd
#define alwan_map_simd_fmsub        alwan_simd_f64_fmsub
#define alwan_map_simd_sqrt         alwan_simd_f64_sqrt
#define alwan_map_simd_cbrt         alwan_simd_f64_cbrt
#define alwan_map_simd_pow          alwan_simd_f64_pow
#define alwan_map_simd_exp          alwan_simd_f64_exp
#define alwan_map_simd_log          alwan_simd_f64_log
#define alwan_map_simd_log2         alwan_simd_f64_log2
#define alwan_map_simd_log10        alwan_simd_f64_log10
#define alwan_map_simd_sin          alwan_simd_f64_sin
#define alwan_map_simd_cos          alwan_simd_f64_cos
#define alwan_map_simd_atan2        alwan_simd_f64_atan2
#define alwan_map_simd_floor        alwan_simd_f64_floor
#define alwan_map_simd_ceil         alwan_simd_f64_ceil
#define alwan_map_simd_round        alwan_simd_f64_round
#define alwan_map_simd_trunc        alwan_simd_f64_trunc
#define alwan_map_simd_min          alwan_simd_f64_min
#define alwan_map_simd_max          alwan_simd_f64_max
#define alwan_map_simd_clamp        alwan_simd_f64_clamp
#define alwan_map_simd_rcp          alwan_simd_f64_rcp
#define alwan_map_simd_cmpeq        alwan_simd_f64_cmpeq
#define alwan_map_simd_cmplt        alwan_simd_f64_cmplt
#define alwan_map_simd_cmple        alwan_simd_f64_cmple
#define alwan_map_simd_cmpgt        alwan_simd_f64_cmpgt
#define alwan_map_simd_cmpge        alwan_simd_f64_cmpge
#define alwan_map_simd_select       alwan_simd_f64_select
#define alwan_map_simd_mask_and     alwan_simd_f64_mask_and
#define alwan_map_simd_mask_or      alwan_simd_f64_mask_or
#define alwan_map_simd_pow24        alwan_simd_f64_pow24
#define alwan_map_simd_pow_inv24    alwan_simd_f64_pow_inv24
#define alwan_map_simd_cbrt_fast    alwan_simd_f64_cbrt_fast
#define alwan_map_simd_deinterleave3 alwan_simd_f64_deinterleave3
#define alwan_map_simd_interleave3   alwan_simd_f64_interleave3
#define alwan_map_simd_mask_all_set  alwan_simd_f64_mask_all_set

/* Scalar math wrappers (precision-parameterized) */
#define alwan_map_scalar_abs(x)       ALWAN_ABS_F64(x)
#define alwan_map_scalar_sqrt(x)      ALWAN_SQRT_F64(x)
#define alwan_map_scalar_cbrt(x)      ALWAN_CBRT_F64(x)
#define alwan_map_scalar_sin(x)       ALWAN_SIN_F64(x)
#define alwan_map_scalar_cos(x)       ALWAN_COS_F64(x)
#define alwan_map_scalar_tan(x)       ALWAN_TAN_F64(x)
#define alwan_map_scalar_atan2(y,x)   ALWAN_ATAN2_F64(y,x)
#define alwan_map_scalar_pow(x,y)     ALWAN_POW_F64(x,y)
#define alwan_map_scalar_exp(x)       ALWAN_EXP_F64(x)
#define alwan_map_scalar_log(x)       ALWAN_LN_F64(x)
#define alwan_map_scalar_log2(x)      ALWAN_LOG2_F64(x)
#define alwan_map_scalar_log10(x)     ALWAN_LOG10_F64(x)
#define alwan_map_scalar_floor(x)     ALWAN_FLOOR_F64(x)
#define alwan_map_scalar_ceil(x)      ALWAN_CEIL_F64(x)
#define alwan_map_scalar_fmod(x,y)    ALWAN_FMOD_F64(x,y)

/* Direct-paste: base is NOT expanded (## at level 1 suppresses expansion) */
#define ALWAN_MAP_LIT(base)         base##_f64
#define ALWAN_MAP_LITV(base)        base##_f64_v
#define ALWAN_MAP_LITCAT3_(a,s,b)   a##s##b
/* ALWAN_MAP_LITCAT: uses ## directly so 'a' is never macro-expanded. */
#define ALWAN_MAP_LITCAT(a, b)      a##_f64##b

#else
#error "Define ALWAN_MAP_F32 or ALWAN_MAP_F64 before including alwan_map_simd_defs.h"
#endif

/* SIMD helper redirect macros - let kernel code call without explicit suffix */
#define alwan__mat3_mul_simd_map  ALWAN_MAP_NAME(alwan__mat3_mul_simd)

/* Parameterized norm macros for .inc files (use parameterized helpers, not compile-time) */
#if ALWAN_NORMALIZE_RANGES
#define ALWAN_MAP_P_NORM_MUL(d, n, f)       ALWAN_MAP_NAME(alwan__norm_lane_mul)((d), (n), (alwan_map_lane)(f))
#define ALWAN_MAP_P_NORM_ADD(d, n, o)       ALWAN_MAP_NAME(alwan__norm_lane_add)((d), (n), (alwan_map_lane)(o))
#define ALWAN_MAP_P_NORM_AFFINE(d, n, s, o) ALWAN_MAP_NAME(alwan__norm_lane_affine)((d), (n), (alwan_map_lane)(s), (alwan_map_lane)(o))
#else
#define ALWAN_MAP_P_NORM_MUL(d, n, f)       ((void)0)
#define ALWAN_MAP_P_NORM_ADD(d, n, o)       ((void)0)
#define ALWAN_MAP_P_NORM_AFFINE(d, n, s, o) ((void)0)
#endif

/* ----------------------------------------------------------------
 * Parameterized tiled loop macros for .inc files
 * Use precision-parameterized types and tile helpers.
 * ---------------------------------------------------------------- */

/* AoS interleaved tiled loop (3-channel, no extra params) */
#define ALWAN_MAP3_P_TILED(in_base, in_s, out_base, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS; \
            ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS]; \
            ALWAN_MAP_NAME(alwan__load_tile_aos3)(ci0_, ci1_, ci2_, (in_base), off_, (in_s), tile_); \
            kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
            ALWAN_MAP_NAME(alwan__store_tile_aos3)((out_base), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)

/* Planar tiled loop (3-channel, no extra params) */
#define ALWAN_MAP3_P_TILED_PLANAR(in0, in1, in2, in_s, out0, out1, out2, out_s, cnt, kernel) \
    do { \
        size_t off_ = 0; \
        while (off_ < (cnt)) { \
            size_t tile_ = (cnt) - off_; \
            if (tile_ > ALWAN_MAP_TILE_PIXELS) tile_ = ALWAN_MAP_TILE_PIXELS; \
            ALWAN_ALIGN(32) alwan_map_lane ci0_[ALWAN_MAP_TILE_PIXELS], ci1_[ALWAN_MAP_TILE_PIXELS], ci2_[ALWAN_MAP_TILE_PIXELS]; \
            ALWAN_ALIGN(32) alwan_map_lane co0_[ALWAN_MAP_TILE_PIXELS], co1_[ALWAN_MAP_TILE_PIXELS], co2_[ALWAN_MAP_TILE_PIXELS]; \
            ALWAN_MAP_NAME(alwan__load_tile_planar3)(ci0_, ci1_, ci2_, (in0), (in1), (in2), off_, (in_s), tile_); \
            kernel(co0_, co1_, co2_, ci0_, ci1_, ci2_, tile_); \
            ALWAN_MAP_NAME(alwan__store_tile_planar3)((out0), (out1), (out2), off_, (out_s), co0_, co1_, co2_, tile_); \
            off_ += tile_; \
        } \
    } while (0)
