/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Core f64 setup - defines ALWAN_CORE_* macros for double precision.
 * No include guard (paired with alwan_core_teardown.h).
 */

/* Scalar type */
#define ALWAN_CORE_T        double
#define ALWAN_CORE_SUFFIX   _f64

/* Token-pasting: FN for data (OKLAB_M1_f64), FNV for functions (alwan_foo_f64_v)
 * FN/FNV expand their argument (ok for definitions, breaks if base is a #define alias).
 * FNLIT/FNVLIT do NOT expand their argument (safe for cross-header calls). */
#define ALWAN_CORE_FN2_(base, sfx) base##sfx
#define ALWAN_CORE_FN_(base, sfx)  ALWAN_CORE_FN2_(base, sfx)
#define ALWAN_CORE_FN(base)        ALWAN_CORE_FN_(base, ALWAN_CORE_SUFFIX)
#define ALWAN_CORE_FNV2_(base, sfx) base##sfx##_v
#define ALWAN_CORE_FNV_(base, sfx)  ALWAN_CORE_FNV2_(base, sfx)
#define ALWAN_CORE_FNV(base)        ALWAN_CORE_FNV_(base, ALWAN_CORE_SUFFIX)
/* Direct-paste: base is NOT expanded (## at level 1 suppresses expansion) */
#define ALWAN_CORE_FNLIT(base)      base##_f64
#define ALWAN_CORE_FNVLIT(base)     base##_f64_v

/* Math macros */
#define ALWAN_CORE_ABS(x)       ALWAN_ABS_F64(x)
#define ALWAN_CORE_SQRT(x)      ALWAN_SQRT_F64(x)
#define ALWAN_CORE_CBRT(x)      ALWAN_CBRT_F64(x)
#define ALWAN_CORE_SIN(x)       ALWAN_SIN_F64(x)
#define ALWAN_CORE_COS(x)       ALWAN_COS_F64(x)
#define ALWAN_CORE_TAN(x)       ALWAN_TAN_F64(x)
#define ALWAN_CORE_TANH(x)      ALWAN_TANH_F64(x)
#define ALWAN_CORE_ATAN(x)      ALWAN_ATAN_F64(x)
#define ALWAN_CORE_ACOS(x)      ALWAN_ACOS_F64(x)
#define ALWAN_CORE_ATAN2(y, x)  ALWAN_ATAN2_F64(y, x)
#define ALWAN_CORE_POW(x, y)    ALWAN_POW_F64(x, y)
#define ALWAN_CORE_EXP(x)       ALWAN_EXP_F64(x)
/* Deterministic-aware sRGB primitives. In fast mode these compute via
 * libm pow; in det mode they call the polynomial implementations from
 * core/alwan_deterministic.h. docs/determinism.md */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
#  include "alwan_deterministic.h"
#  define ALWAN_CORE_SRGB_OETF(x)    alwan_det_srgb_oetf_f64(x)
#  define ALWAN_CORE_SRGB_EOTF(x)    alwan_det_srgb_eotf_f64(x)
#  define ALWAN_CORE_BT2020_OETF(x)  alwan_det_bt2020_oetf_f64(x)
#  define ALWAN_CORE_BT2020_EOTF(x)  alwan_det_bt2020_eotf_f64(x)
#else
#  include "alwan_fast_pow.h"
/* Fast mode: route the gamma-2.4 leg through the scalar pow twins of the SIMD
 * kernels (alwan_fast_pow*_f64), so the scalar _v path and the polynomial SIMD
 * map path agree on platforms without SVML. */
#  define ALWAN_CORE_SRGB_OETF(x)  ((x) <= ALWAN_LITERAL_F64(0.0031308) ? \
        (x) * ALWAN_LITERAL_F64(12.92) : \
        ALWAN_LITERAL_F64(1.055) * alwan_fast_pow_inv24_f64(x) - ALWAN_LITERAL_F64(0.055))
#  define ALWAN_CORE_SRGB_EOTF(x)  ((x) <= ALWAN_LITERAL_F64(0.04045) ? \
        (x) / ALWAN_LITERAL_F64(12.92) : \
        alwan_fast_pow24_f64(((x) + ALWAN_LITERAL_F64(0.055)) / ALWAN_LITERAL_F64(1.055)))
#  define ALWAN_CORE_BT2020_OETF(x)  ((x) < ALWAN_LITERAL_F64(0.018) ? \
        (x) * ALWAN_LITERAL_F64(4.5) : \
        ALWAN_LITERAL_F64(1.099) * ALWAN_POW_F64((x), ALWAN_LITERAL_F64(0.45)) - ALWAN_LITERAL_F64(0.099))
#  define ALWAN_CORE_BT2020_EOTF(x)  ((x) < (ALWAN_LITERAL_F64(4.5) * ALWAN_LITERAL_F64(0.018)) ? \
        (x) / ALWAN_LITERAL_F64(4.5) : \
        ALWAN_POW_F64(((x) + ALWAN_LITERAL_F64(0.099)) / ALWAN_LITERAL_F64(1.099), ALWAN_LITERAL_F64(1.0)/ALWAN_LITERAL_F64(0.45)))
#endif
#define ALWAN_CORE_LN(x)        ALWAN_LN_F64(x)
#define ALWAN_CORE_LOG2(x)      ALWAN_LOG2_F64(x)
#define ALWAN_CORE_LOG10(x)     ALWAN_LOG10_F64(x)
#define ALWAN_CORE_FLOOR(x)     ALWAN_FLOOR_F64(x)
#define ALWAN_CORE_ROUND(x)     ALWAN_ROUND_F64(x)
#define ALWAN_CORE_CEIL(x)      ALWAN_CEIL_F64(x)
#define ALWAN_CORE_FMOD(x, y)   ALWAN_FMOD_F64(x, y)

/* Literals and constants */
#define ALWAN_CORE_LITERAL(x)   ALWAN_LITERAL_F64(x)
#define ALWAN_CORE_EPSILON      ALWAN_EPSILON_F64
#define ALWAN_CORE_PI           ALWAN_PI_F64
#define ALWAN_CORE_ZERO         ALWAN_ZERO_F64
#define ALWAN_CORE_ONE          ALWAN_ONE_F64

/* Branchless select (same for both precisions) */
#define ALWAN_CORE_SELECT(cond, t, f) ((cond) ? (t) : (f))

/* BT.709 luma coefficients */
#define ALWAN_CORE_LUMA_KR_BT709  ALWAN_CORE_LITERAL(0.2126)
#define ALWAN_CORE_LUMA_KG_BT709  ALWAN_CORE_LITERAL(0.7152)
#define ALWAN_CORE_LUMA_KB_BT709  ALWAN_CORE_LITERAL(0.0722)

/* Utility functions */
#define ALWAN_CORE_MIN       alwan_min_f64
#define ALWAN_CORE_MAX       alwan_max_f64
#define ALWAN_CORE_MIN3      alwan_min3_f64
#define ALWAN_CORE_MAX3      alwan_max3_f64
#define ALWAN_CORE_CLAMP     alwan_clamp_f64
#define ALWAN_CORE_SATURATE  alwan_saturate_f64
#define ALWAN_CORE_LERP      alwan_lerp_f64

/* See the note in alwan_core_f32_setup.h. */
#define ALWAN_CORE_PARAM_OUT ALWAN_CORE_T *
#define ALWAN_CORE_REF(p)    (*(p))
#define ALWAN_CORE_ADDR(x)   (&(x))
#define ALWAN_CORE_PARAM_ARRAY_OUT(type, name, n) type name[n]

/* Type aliases */
#define ALWAN_CORE_VEC2               alwan_vec2_f64
#define ALWAN_CORE_VEC3               alwan_vec3_f64
#define ALWAN_CORE_MAT3X3             alwan_mat3x3_f64
#define ALWAN_CORE_MAT4X4             alwan_mat4x4_f64
#define ALWAN_CORE_TABLE_CELL         alwan_table_cell_f64
#define ALWAN_CORE_RGB                alwan_rgb_f64
#define ALWAN_CORE_PRIM               alwan_aces_primaries_f64
#define ALWAN_CORE_CMYK               alwan_cmyk_f64
#define ALWAN_CORE_CMY                alwan_cmy_f64
#define ALWAN_CORE_HSV                alwan_hsv_f64
#define ALWAN_CORE_HSL                alwan_hsl_f64
#define ALWAN_CORE_HSP                alwan_hsp_f64
#define ALWAN_CORE_HSPLOG             alwan_hsplog_f64
#define ALWAN_CORE_HSY                alwan_hsy_f64
#define ALWAN_CORE_HWB                alwan_hwb_f64
#define ALWAN_CORE_XYZ                alwan_xyz_f64
#define ALWAN_CORE_XYY                alwan_xyy_f64
#define ALWAN_CORE_LAB                alwan_lab_f64
#define ALWAN_CORE_LUV                alwan_luv_f64
#define ALWAN_CORE_LCH                alwan_lch_f64
#define ALWAN_CORE_LCHUV              alwan_lchuv_f64
#define ALWAN_CORE_OKLAB              alwan_oklab_f64
#define ALWAN_CORE_OKLCH              alwan_oklch_f64
#define ALWAN_CORE_JZAZBZ             alwan_jzazbz_f64
#define ALWAN_CORE_JZCZHZ             alwan_jzczhz_f64
#define ALWAN_CORE_ICTCP              alwan_ictcp_f64
#define ALWAN_CORE_IPT                alwan_ipt_f64
#define ALWAN_CORE_IGPGTG             alwan_igpgtg_f64
#define ALWAN_CORE_ICACB              alwan_icacb_f64
#define ALWAN_CORE_YCBCR              alwan_ycbcr_f64
#define ALWAN_CORE_YCOCG              alwan_ycocg_f64
#define ALWAN_CORE_YCCBCCRC           alwan_yccbccrc_f64
#define ALWAN_CORE_UVW                alwan_uvw_f64
#define ALWAN_CORE_DIN99              alwan_din99_f64
#define ALWAN_CORE_HUNTER_LAB         alwan_hunter_lab_f64
#define ALWAN_CORE_IPTCH              alwan_iptch_f64
#define ALWAN_CORE_PROLAB             alwan_prolab_f64
#define ALWAN_CORE_OSA_UCS            alwan_osa_ucs_f64
#define ALWAN_CORE_UCS                alwan_ucs_f64
#define ALWAN_CORE_PRISMATIC          alwan_prismatic_f64
#define ALWAN_CORE_HCL                alwan_hcl_f64
#define ALWAN_CORE_IHLS               alwan_ihls_f64
#define ALWAN_CORE_CAM_JAB            alwan_cam_jab_f64
#define ALWAN_CORE_HSLUV              alwan_hsluv_f64
#define ALWAN_CORE_HPLUV              alwan_hpluv_f64
#define ALWAN_CORE_OKHSL              alwan_okhsl_f64
#define ALWAN_CORE_OKHSV              alwan_okhsv_f64
#define ALWAN_CORE_CUBEHELIX          alwan_cubehelix_f64
#define ALWAN_CORE_HLC                alwan_hlc_f64
#define ALWAN_CORE_ST2086_METADATA    alwan_st2086_metadata_f64
#define ALWAN_CORE_CONTENT_LIGHT_LEVEL alwan_content_light_level_f64
#define ALWAN_CORE_RGB_SPACE_DESC     alwan_rgb_space_desc_f64

/* Parameter structs */
#define ALWAN_CORE_DELTA_E_CMC_PARAMS          alwan_delta_e_cmc_params_f64
#define ALWAN_CORE_DELTA_E_ITP_PARAMS          alwan_delta_e_itp_params_f64
#define ALWAN_CORE_CSF_BARTEN1999_PARAMS       alwan_csf_barten1999_params_f64
#define ALWAN_CORE_ATMOSPHERE_PARAMS           alwan_atmosphere_params_f64

/* SPD structs */
#define ALWAN_CORE_SPD                         alwan_spd_f64
#define ALWAN_CORE_SPD_SHAPE                   alwan_spd_shape_f64

/* CAM viewing conditions */
#define ALWAN_CORE_CIECAM02_VIEWING_CONDITIONS alwan_ciecam02_viewing_conditions_f64
#define ALWAN_CORE_CAM16_VIEWING_CONDITIONS    alwan_cam16_viewing_conditions_f64
#define ALWAN_CORE_ZCAM_VIEWING_CONDITIONS     alwan_zcam_viewing_conditions_f64
#define ALWAN_CORE_RLAB_VIEWING_CONDITIONS     alwan_rlab_viewing_conditions_f64
#define ALWAN_CORE_HUNT_VIEWING_CONDITIONS     alwan_hunt_viewing_conditions_f64
#define ALWAN_CORE_HELLWIG2022_VIEWING_CONDITIONS alwan_hellwig2022_viewing_conditions_f64
#define ALWAN_CORE_KIM2009_VIEWING_CONDITIONS  alwan_kim2009_viewing_conditions_f64
#define ALWAN_CORE_LLAB_VIEWING_CONDITIONS     alwan_llab_viewing_conditions_f64
#define ALWAN_CORE_ATD95_VIEWING_CONDITIONS    alwan_atd95_viewing_conditions_f64
#define ALWAN_CORE_NAYATANI95_VIEWING_CONDITIONS alwan_nayatani95_viewing_conditions_f64

/* CAM correlates */
#define ALWAN_CORE_CIECAM02_CORRELATES         alwan_ciecam02_correlates_f64
#define ALWAN_CORE_CAM16_CORRELATES            alwan_cam16_correlates_f64
#define ALWAN_CORE_ZCAM_CORRELATES             alwan_zcam_correlates_f64
#define ALWAN_CORE_RLAB_CORRELATES             alwan_rlab_correlates_f64
#define ALWAN_CORE_HUNT_CORRELATES             alwan_hunt_correlates_f64
#define ALWAN_CORE_HELLWIG2022_CORRELATES      alwan_hellwig2022_correlates_f64
#define ALWAN_CORE_KIM2009_CORRELATES          alwan_kim2009_correlates_f64
#define ALWAN_CORE_LLAB_CORRELATES             alwan_llab_correlates_f64
#define ALWAN_CORE_ATD95_CORRELATES            alwan_atd95_correlates_f64
#define ALWAN_CORE_NAYATANI95_CORRELATES       alwan_nayatani95_correlates_f64
#define ALWAN_CORE_CAM18SL_CORRELATES          alwan_cam18sl_correlates_f64
#define ALWAN_CORE_CAM20U_CORRELATES           alwan_cam20u_correlates_f64
