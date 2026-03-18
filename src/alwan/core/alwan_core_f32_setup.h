/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Core f32 setup - defines ALWAN_CORE_* macros for float precision.
 * No include guard (paired with alwan_core_teardown.h).
 */

/* Scalar type */
#define ALWAN_CORE_T        float
#define ALWAN_CORE_SUFFIX   _f32

/* Token-pasting: FN for data (OKLAB_M1_f32), FNV for functions (alwan_foo_f32_v)
 * FN/FNV expand their argument (ok for definitions, breaks if base is a #define alias).
 * FNLIT/FNVLIT do NOT expand their argument (safe for cross-header calls). */
#define ALWAN_CORE_FN2_(base, sfx) base##sfx
#define ALWAN_CORE_FN_(base, sfx)  ALWAN_CORE_FN2_(base, sfx)
#define ALWAN_CORE_FN(base)        ALWAN_CORE_FN_(base, ALWAN_CORE_SUFFIX)
#define ALWAN_CORE_FNV2_(base, sfx) base##sfx##_v
#define ALWAN_CORE_FNV_(base, sfx)  ALWAN_CORE_FNV2_(base, sfx)
#define ALWAN_CORE_FNV(base)        ALWAN_CORE_FNV_(base, ALWAN_CORE_SUFFIX)
/* Direct-paste: base is NOT expanded (## at level 1 suppresses expansion) */
#define ALWAN_CORE_FNLIT(base)      base##_f32
#define ALWAN_CORE_FNVLIT(base)     base##_f32_v

/* Math macros */
#define ALWAN_CORE_ABS(x)       ALWAN_ABS_F32(x)
#define ALWAN_CORE_SQRT(x)      ALWAN_SQRT_F32(x)
#define ALWAN_CORE_CBRT(x)      ALWAN_CBRT_F32(x)
#define ALWAN_CORE_SIN(x)       ALWAN_SIN_F32(x)
#define ALWAN_CORE_COS(x)       ALWAN_COS_F32(x)
#define ALWAN_CORE_TAN(x)       ALWAN_TAN_F32(x)
#define ALWAN_CORE_TANH(x)      ALWAN_TANH_F32(x)
#define ALWAN_CORE_ATAN(x)      ALWAN_ATAN_F32(x)
#define ALWAN_CORE_ACOS(x)      ALWAN_ACOS_F32(x)
#define ALWAN_CORE_ATAN2(y, x)  ALWAN_ATAN2_F32(y, x)
#define ALWAN_CORE_POW(x, y)    ALWAN_POW_F32(x, y)
#define ALWAN_CORE_EXP(x)       ALWAN_EXP_F32(x)
#define ALWAN_CORE_LN(x)        ALWAN_LN_F32(x)
#define ALWAN_CORE_LOG2(x)      ALWAN_LOG2_F32(x)
#define ALWAN_CORE_LOG10(x)     ALWAN_LOG10_F32(x)
#define ALWAN_CORE_FLOOR(x)     ALWAN_FLOOR_F32(x)
#define ALWAN_CORE_CEIL(x)      ALWAN_CEIL_F32(x)
#define ALWAN_CORE_FMOD(x, y)   ALWAN_FMOD_F32(x, y)

/* Literals and constants */
#define ALWAN_CORE_LITERAL(x)   ALWAN_LITERAL_F32(x)
#define ALWAN_CORE_EPSILON      ALWAN_EPSILON_F32
#define ALWAN_CORE_PI           ALWAN_PI_F32
#define ALWAN_CORE_ZERO         ALWAN_ZERO_F32
#define ALWAN_CORE_ONE          ALWAN_ONE_F32

/* Branchless select (same for both precisions) */
#define ALWAN_CORE_SELECT(cond, t, f) ((cond) ? (t) : (f))

/* Utility functions */
#define ALWAN_CORE_MIN       alwan_min_f32
#define ALWAN_CORE_MAX       alwan_max_f32
#define ALWAN_CORE_MIN3      alwan_min3_f32
#define ALWAN_CORE_MAX3      alwan_max3_f32
#define ALWAN_CORE_CLAMP     alwan_clamp_f32
#define ALWAN_CORE_SATURATE  alwan_saturate_f32
#define ALWAN_CORE_LERP      alwan_lerp_f32

/* Type aliases */
#define ALWAN_CORE_VEC2               alwan_vec2_f32
#define ALWAN_CORE_VEC3               alwan_vec3_f32
#define ALWAN_CORE_MAT3X3             alwan_mat3x3_f32
#define ALWAN_CORE_MAT4X4             alwan_mat4x4_f32
#define ALWAN_CORE_RGB                alwan_rgb_f32
#define ALWAN_CORE_CMYK               alwan_cmyk_f32
#define ALWAN_CORE_CMY                alwan_cmy_f32
#define ALWAN_CORE_HSV                alwan_hsv_f32
#define ALWAN_CORE_HSL                alwan_hsl_f32
#define ALWAN_CORE_HSP                alwan_hsp_f32
#define ALWAN_CORE_HSPLOG             alwan_hsplog_f32
#define ALWAN_CORE_HSY                alwan_hsy_f32
#define ALWAN_CORE_XYZ                alwan_xyz_f32
#define ALWAN_CORE_XYY                alwan_xyy_f32
#define ALWAN_CORE_LAB                alwan_lab_f32
#define ALWAN_CORE_LUV                alwan_luv_f32
#define ALWAN_CORE_LCH                alwan_lch_f32
#define ALWAN_CORE_LCHUV              alwan_lchuv_f32
#define ALWAN_CORE_OKLAB              alwan_oklab_f32
#define ALWAN_CORE_OKLCH              alwan_oklch_f32
#define ALWAN_CORE_JZAZBZ             alwan_jzazbz_f32
#define ALWAN_CORE_JZCZHZ             alwan_jzczhz_f32
#define ALWAN_CORE_ICTCP              alwan_ictcp_f32
#define ALWAN_CORE_IPT                alwan_ipt_f32
#define ALWAN_CORE_IGPGTG             alwan_igpgtg_f32
#define ALWAN_CORE_ICACB              alwan_icacb_f32
#define ALWAN_CORE_YCBCR              alwan_ycbcr_f32
#define ALWAN_CORE_YCOCG              alwan_ycocg_f32
#define ALWAN_CORE_YCCBCCRC           alwan_yccbccrc_f32
#define ALWAN_CORE_UVW                alwan_uvw_f32
#define ALWAN_CORE_DIN99              alwan_din99_f32
#define ALWAN_CORE_HUNTER_LAB         alwan_hunter_lab_f32
#define ALWAN_CORE_IPTCH              alwan_iptch_f32
#define ALWAN_CORE_PROLAB             alwan_prolab_f32
#define ALWAN_CORE_OSA_UCS            alwan_osa_ucs_f32
#define ALWAN_CORE_UCS                alwan_ucs_f32
#define ALWAN_CORE_PRISMATIC          alwan_prismatic_f32
#define ALWAN_CORE_HCL                alwan_hcl_f32
#define ALWAN_CORE_IHLS               alwan_ihls_f32
#define ALWAN_CORE_CAM_JAB            alwan_cam_jab_f32
#define ALWAN_CORE_HSLUV              alwan_hsluv_f32
#define ALWAN_CORE_HPLUV              alwan_hpluv_f32
#define ALWAN_CORE_OKHSL              alwan_okhsl_f32
#define ALWAN_CORE_OKHSV              alwan_okhsv_f32
#define ALWAN_CORE_CUBEHELIX          alwan_cubehelix_f32
#define ALWAN_CORE_HLC                alwan_hlc_f32
#define ALWAN_CORE_ST2086_METADATA    alwan_st2086_metadata_f32
#define ALWAN_CORE_CONTENT_LIGHT_LEVEL alwan_content_light_level_f32
