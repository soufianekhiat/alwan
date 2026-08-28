/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ALWAN_CORE_* aliases for GPU single-precision passes.
 *
 * Shared body for the three GPU bootstrap headers (alwan_glsl.h,
 * alwan_hlsl.h, alwan_halide.h). Adding a new semantic color type now
 * means editing one row here instead of three byte-identical files.
 *
 * Not for direct inclusion -- only the bootstrap headers should pull this in,
 * after they have included alwan_platform.h and alwan_types.h.
 */

#define ALWAN_CORE_T                   alwan_scalar
#define ALWAN_CORE_FNLIT(base)         base
#define ALWAN_CORE_FNVLIT(base)        base##_v
/* Name-mangling vocabulary: on GPU there is a single (float) instantiation, so
 * the suffixed name IS the base name. (The C setup pastes _f32/_f64.) Without
 * these, any core built on ALWAN_CORE_FN -- e.g. AgX -- can't compile for GPU. */
#define ALWAN_CORE_SUFFIX
#define ALWAN_CORE_FN(base)            base
#define ALWAN_CORE_FNV(base)           base##_v
/* Deterministic-aware in C; on GPU these route to the piecewise ALWAN_SRGB_*
 * from alwan_platform.h (hardware pow -- ULP-approximate vs the C det leg). */
#define ALWAN_CORE_SRGB_OETF(x)        ALWAN_SRGB_OETF(x)
#define ALWAN_CORE_SRGB_EOTF(x)        ALWAN_SRGB_EOTF(x)
#define ALWAN_CORE_BT2020_OETF(x)      ALWAN_BT2020_OETF(x)
#define ALWAN_CORE_BT2020_EOTF(x)      ALWAN_BT2020_EOTF(x)
#define ALWAN_CORE_ABS(x)              ALWAN_ABS(x)
#define ALWAN_CORE_SQRT(x)             ALWAN_SQRT(x)
#define ALWAN_CORE_CBRT(x)             ALWAN_CBRT(x)
#define ALWAN_CORE_SIN(x)              ALWAN_SIN(x)
#define ALWAN_CORE_COS(x)              ALWAN_COS(x)
#define ALWAN_CORE_TAN(x)              ALWAN_TAN(x)
#define ALWAN_CORE_TANH(x)             ALWAN_TANH(x)
#define ALWAN_CORE_ATAN(x)             ALWAN_ATAN(x)
#define ALWAN_CORE_ACOS(x)             ALWAN_ACOS(x)
#define ALWAN_CORE_ATAN2(y, x)         ALWAN_ATAN2(y, x)
#define ALWAN_CORE_POW(x, y)           ALWAN_POW(x, y)
#define ALWAN_CORE_EXP(x)              ALWAN_EXP(x)
#define ALWAN_CORE_LN(x)               ALWAN_LN(x)
#define ALWAN_CORE_LOG2(x)             ALWAN_LOG2(x)
#define ALWAN_CORE_LOG10(x)            ALWAN_LOG10(x)
#define ALWAN_CORE_FLOOR(x)            ALWAN_FLOOR(x)
#define ALWAN_CORE_ROUND(x)            ALWAN_ROUND(x)
#define ALWAN_CORE_CEIL(x)             ALWAN_CEIL(x)
#define ALWAN_CORE_FMOD(x, y)          ALWAN_FMOD(x, y)
#define ALWAN_CORE_LITERAL(x)          ALWAN_LITERAL(x)
#define ALWAN_CORE_EPSILON             ALWAN_EPSILON
#define ALWAN_CORE_PI                  ALWAN_PI
#define ALWAN_CORE_ZERO                ALWAN_ZERO
#define ALWAN_CORE_ONE                 ALWAN_ONE
#define ALWAN_CORE_SELECT(cond, t, f)  ALWAN_SELECT(cond, t, f)
#define ALWAN_CORE_MIN                 alwan_min
#define ALWAN_CORE_MAX                 alwan_max
#define ALWAN_CORE_MIN3                alwan_min3
#define ALWAN_CORE_MAX3                alwan_max3
#define ALWAN_CORE_CLAMP               alwan_clamp
#define ALWAN_CORE_SATURATE            alwan_saturate
#define ALWAN_CORE_LERP                alwan_lerp
#define ALWAN_CORE_VEC2                alwan_vec2
#define ALWAN_CORE_VEC3                alwan_vec3
#define ALWAN_CORE_MAT3X3              alwan_mat3x3
#define ALWAN_CORE_MAT4X4              alwan_mat4x4
#define ALWAN_CORE_TABLE_CELL          alwan_table_cell
#define ALWAN_CORE_RGB                 alwan_rgb
#define ALWAN_CORE_CMYK                alwan_cmyk
#define ALWAN_CORE_CMY                 alwan_cmy
#define ALWAN_CORE_HSV                 alwan_hsv
#define ALWAN_CORE_HSL                 alwan_hsl
#define ALWAN_CORE_HSP                 alwan_hsp
#define ALWAN_CORE_HSPLOG              alwan_hsplog
#define ALWAN_CORE_HSY                 alwan_hsy
#define ALWAN_CORE_XYZ                 alwan_xyz
#define ALWAN_CORE_XYY                 alwan_xyy
#define ALWAN_CORE_LAB                 alwan_lab
#define ALWAN_CORE_LUV                 alwan_luv
#define ALWAN_CORE_LCH                 alwan_lch
#define ALWAN_CORE_LCHUV               alwan_lchuv
#define ALWAN_CORE_OKLAB               alwan_oklab
#define ALWAN_CORE_OKLCH               alwan_oklch
#define ALWAN_CORE_JZAZBZ              alwan_jzazbz
#define ALWAN_CORE_JZCZHZ              alwan_jzczhz
#define ALWAN_CORE_ICTCP               alwan_ictcp
#define ALWAN_CORE_IPT                 alwan_ipt
#define ALWAN_CORE_IGPGTG              alwan_igpgtg
#define ALWAN_CORE_ICACB               alwan_icacb
#define ALWAN_CORE_YCBCR               alwan_ycbcr
#define ALWAN_CORE_YCOCG               alwan_ycocg
#define ALWAN_CORE_YCCBCCRC            alwan_yccbccrc
#define ALWAN_CORE_UVW                 alwan_uvw
#define ALWAN_CORE_DIN99               alwan_din99
#define ALWAN_CORE_HUNTER_LAB          alwan_hunter_lab
#define ALWAN_CORE_IPTCH               alwan_iptch
#define ALWAN_CORE_PROLAB              alwan_prolab
#define ALWAN_CORE_OSA_UCS             alwan_osa_ucs
#define ALWAN_CORE_UCS                 alwan_ucs
#define ALWAN_CORE_PRISMATIC           alwan_prismatic
#define ALWAN_CORE_HCL                 alwan_hcl
#define ALWAN_CORE_IHLS                alwan_ihls
#define ALWAN_CORE_CAM_JAB             alwan_cam_jab
#define ALWAN_CORE_HSLUV               alwan_hsluv
#define ALWAN_CORE_HPLUV               alwan_hpluv
#define ALWAN_CORE_OKHSL               alwan_okhsl
#define ALWAN_CORE_OKHSV               alwan_okhsv
#define ALWAN_CORE_CUBEHELIX           alwan_cubehelix
#define ALWAN_CORE_HLC                 alwan_hlc
#define ALWAN_CORE_ST2086_METADATA     alwan_st2086_metadata
#define ALWAN_CORE_CONTENT_LIGHT_LEVEL alwan_content_light_level

/* In deterministic mode, pull in the GPU single-precision deterministic
 * transcendentals (alwan_det_srgb_eotf etc.) so the det-aware ALWAN_SRGB_* macros
 * from alwan_platform.h resolve. Mirrors the C setup, which includes it too. */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
#include "core/alwan_deterministic.h"
#endif
