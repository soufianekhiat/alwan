/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 */

#ifndef ALWAN_H
#define ALWAN_H

#include "alwan_config.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO: add support for YPbPr, xvYCC, YCoCg, YUV, etc.

/* ----------------------------------------------------------------
 * Error codes
 * ---------------------------------------------------------------- */
typedef enum {
    ALWAN_OK       =  0,  /* Success */
    ALWAN_E_INVALID = -1, /* Invalid argument */
    ALWAN_E_NODATA = -2,  /* Data not found or not loaded */
    ALWAN_E_RANGE  = -3,  /* Value out of valid range */
    ALWAN_E_NOMEM  = -4   /* Memory allocation failed */
} alwan_status;

/* ----------------------------------------------------------------
 * Context & Configuration
 * ---------------------------------------------------------------- */

/* Opaque context handle */
typedef struct alwan_ctx alwan_ctx;

/* Allocation function pointers */
typedef void *(*alwan_alloc_fn)(size_t size, size_t align);
typedef void  (*alwan_free_fn)(void *ptr);

/* Configuration structure */
typedef struct {
    alwan_alloc_fn alloc_cb;          /* Optional custom allocator (NULL = default) */
    alwan_free_fn  free_cb;           /* Optional custom deallocator (NULL = default) */
    char const *runtime_data_root;    /* Optional data path for ALWAN_EMBED_DATA=0 (NULL = use env or default) */
    uint32_t flags;                   /* Reserved for future use (must be 0) */
} alwan_config;

/* Create a new context with optional configuration */
alwan_ctx *alwan_create(alwan_config const *cfg);

/* Destroy context and release all resources */
void alwan_destroy(alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Data Loading
 * ---------------------------------------------------------------- */

 /* Standard illuminant xy chromaticity data getters
 * Each returns 2 values: x, y chromaticity coordinates
 * In embedded mode: returns pointer to static data (no deallocation needed)
 * In runtime mode: allocates memory (caller must free with alwan_data_free) */

/* Illuminant A (incandescent tungsten) */
int alwan_data_get_illuminant_a(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* Illuminant D50 (horizon daylight) */
int alwan_data_get_illuminant_d50(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* Illuminant D55 (mid-morning daylight) */
int alwan_data_get_illuminant_d55(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* P8: Illuminant D60 (daylight) */
int alwan_data_get_illuminant_d60(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* Illuminant D65 (noon daylight) */
int alwan_data_get_illuminant_d65(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* Illuminant E (equal energy) */
int alwan_data_get_illuminant_e(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* P8: Illuminant B (direct sunlight) */
int alwan_data_get_illuminant_b(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* P8: Illuminant C (average daylight) */
int alwan_data_get_illuminant_c(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

/* P8: Illuminant D75 (daylight 7500K) */
int alwan_data_get_illuminant_d75(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

// TODO: Add F1-F12 fluorescent illuminant getters once xy data is generated

/* Get sRGB primaries (6 values: rx, ry, gx, gy, bx, by) */
int alwan_data_get_srgb_primaries(alwan_ctx *ctx, alwan_scalar **data, size_t *count);

#if !ALWAN_EMBED_DATA
/* Free data allocated by runtime loader (no-op in embedded mode) */
void alwan_data_free(alwan_ctx *ctx, alwan_scalar *data);
#endif

/* ----------------------------------------------------------------
 * Math Types
 * ---------------------------------------------------------------- */

/* 3-component vector */
typedef struct {
    alwan_scalar v[3];
} alwan_vec3;

// TODO: add alwan_vec4 for CMYK, alpha channel, etc.

/* 3x3 matrix stored in row-major order: [m00 m01 m02 m10 m11 m12 m20 m21 m22] */
typedef struct {
    alwan_scalar m[9];
} alwan_mat3x3;

/* ----------------------------------------------------------------
 * Math Operations
 * ---------------------------------------------------------------- */

/* Multiply two 3x3 matrices: out = a * b */
void alwan_mat3_mul(alwan_mat3x3 const *a, alwan_mat3x3 const *b, alwan_mat3x3 *out);

/* Invert a 3x3 matrix using partial-pivot Gaussian elimination
 * Returns ALWAN_OK on success, ALWAN_E_RANGE if matrix is singular */
int alwan_mat3_inv(alwan_mat3x3 const *m, alwan_mat3x3 *out);

/* Multiply matrix by vector: out = m * v */
void alwan_mat3_mulv(alwan_mat3x3 const *m, alwan_vec3 const *v, alwan_vec3 *out);

/* Create identity matrix */
void alwan_mat3_identity(alwan_mat3x3 *out);

/* ----------------------------------------------------------------
 * RGB Color Spaces
 * ---------------------------------------------------------------- */

/* RGB color space identifiers */
typedef enum {
    /* Core spaces */
    ALWAN_RGB_SPACE_SRGB,
    ALWAN_RGB_SPACE_BT709,
    ALWAN_RGB_SPACE_DISPLAY_P3,
    ALWAN_RGB_SPACE_BT2020,
    ALWAN_RGB_SPACE_ACES2065_1,
    ALWAN_RGB_SPACE_ACESCG,
    ALWAN_RGB_SPACE_ACESPROXY,

    /* Adobe RGB (1998) - Photography/print workflow */
    ALWAN_RGB_SPACE_ADOBE_RGB_1998,

    /* ProPhoto RGB - Wide gamut professional */
    ALWAN_RGB_SPACE_PROPHOTO_RGB,

    /* Cinema/Broadcast spaces */
    ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT,
    ALWAN_RGB_SPACE_BLACKMAGIC_WIDE_GAMUT,
    ALWAN_RGB_SPACE_V_GAMUT,
    ALWAN_RGB_SPACE_S_GAMUT,
    ALWAN_RGB_SPACE_S_GAMUT3,
    ALWAN_RGB_SPACE_S_GAMUT3_CINE,
    ALWAN_RGB_SPACE_CINEMA_GAMUT,
    ALWAN_RGB_SPACE_REDWIDEGAMUTRGB,
    ALWAN_RGB_SPACE_DCI_P3,
    ALWAN_RGB_SPACE_P3_D65,

    /* Legacy spaces */
    ALWAN_RGB_SPACE_NTSC_1953,
    ALWAN_RGB_SPACE_NTSC_1987,
    ALWAN_RGB_SPACE_PAL_SECAM,
    ALWAN_RGB_SPACE_APPLE_RGB,
    ALWAN_RGB_SPACE_COLORMATCH_RGB
} alwan_rgb_space;

/* Transfer function identifiers (OETF/EOTF) */
typedef enum {
    ALWAN_TF_SRGB,
    ALWAN_TF_BT709,        /* Same as BT.2020 */
    ALWAN_TF_BT2020,       /* Same as BT.709 */
    ALWAN_TF_PQ,           /* Perceptual Quantizer (SMPTE ST 2084) */
    ALWAN_TF_ST2084,       /* Alias for PQ */
    ALWAN_TF_HLG,          /* Hybrid Log-Gamma (BT.2100) */
    ALWAN_TF_BT1886,       /* BT.1886 EOTF only */
    ALWAN_TF_ACESPROXY,    /* ACES Proxy */

    /* Extended Transfer Functions */
    /* Sony S-Log Family */
    ALWAN_TF_SLOG,         /* Sony S-Log */
    ALWAN_TF_SLOG2,        /* Sony S-Log2 */
    ALWAN_TF_SLOG3,        /* Sony S-Log3 */

    /* Canon C-Log Family */
    ALWAN_TF_CLOG,         /* Canon C-Log */
    ALWAN_TF_CLOG2,        /* Canon C-Log2 */
    ALWAN_TF_CLOG3,        /* Canon C-Log3 */

    /* Panasonic V-Log */
    ALWAN_TF_VLOG,         /* Panasonic V-Log */

    /* ARRI LogC Family */
    ALWAN_TF_LOGC3,        /* ARRI LogC3 */
    ALWAN_TF_LOGC4,        /* ARRI LogC4 */

    /* Red Log Family */
    ALWAN_TF_REDLOG,       /* RED REDLog */
    ALWAN_TF_REDLOGFILM,   /* RED REDLogFilm */
    ALWAN_TF_LOG3G10,      /* RED Log3G10 */

    /* Blackmagic Film Gen 5 */
    ALWAN_TF_BMDFILM,      /* Blackmagic Film Gen 5 */

    /* Filmlight T-Log / E-Log */
    ALWAN_TF_TLOG,         /* Filmlight T-Log */
    ALWAN_TF_ELOG,         /* Filmlight E-Log */

    /* GoPro Protune */
    ALWAN_TF_PROTUNE,      /* GoPro Protune */

    /* Standard Gamma Variants */
    ALWAN_TF_GAMMA22,      /* Gamma 2.2 */
    ALWAN_TF_GAMMA24,      /* Gamma 2.4 */
    ALWAN_TF_GAMMA26,      /* Gamma 2.6 */
    ALWAN_TF_GAMMA28,      /* Gamma 2.8 */

    /* Nikon N-Log */
    ALWAN_TF_NLOG          /* Nikon N-Log */
} alwan_transfer_function;

/* Standard illuminant identifiers */
typedef enum {
	ALWAN_ILLUMINANT_A,    /* Incandescent / Tungsten */
	ALWAN_ILLUMINANT_B,    /* CIE Illuminant B (direct sunlight) */
	ALWAN_ILLUMINANT_C,    /* CIE Illuminant C (average daylight) */
    ALWAN_ILLUMINANT_D40,  /* Daylight 4000K (P8.3) */
    ALWAN_ILLUMINANT_D45,  /* Daylight 4500K (P8.3) */
    ALWAN_ILLUMINANT_D50,  /* Daylight 5000K */
	ALWAN_ILLUMINANT_D55,  /* Daylight 5500K */
	ALWAN_ILLUMINANT_D60,  /* Daylight 6000K */
	ALWAN_ILLUMINANT_D65,  /* Daylight 6500K */
	ALWAN_ILLUMINANT_D75,  /* Daylight 7500K */
    ALWAN_ILLUMINANT_D93,  /* Daylight 9300K (P8.3) */
    ALWAN_ILLUMINANT_E,    /* Equal energy */
    ALWAN_ILLUMINANT_F1,   /* Fluorescent */
    ALWAN_ILLUMINANT_F2,
    ALWAN_ILLUMINANT_F3,
    ALWAN_ILLUMINANT_F4,
    ALWAN_ILLUMINANT_F5,
    ALWAN_ILLUMINANT_F6,
    ALWAN_ILLUMINANT_F7,
    ALWAN_ILLUMINANT_F8,
    ALWAN_ILLUMINANT_F9,
    ALWAN_ILLUMINANT_F10,
    ALWAN_ILLUMINANT_F11,
    ALWAN_ILLUMINANT_F12,

    /* P8.3: LED illuminants */
    ALWAN_ILLUMINANT_LED_B1,    /* LED B1 (blue-pumped phosphor) */
    ALWAN_ILLUMINANT_LED_B2,    /* LED B2 */
    ALWAN_ILLUMINANT_LED_B3,    /* LED B3 */
    ALWAN_ILLUMINANT_LED_B4,    /* LED B4 */
    ALWAN_ILLUMINANT_LED_B5,    /* LED B5 */
    ALWAN_ILLUMINANT_LED_BH1,   /* LED BH1 (high CRI) */
    ALWAN_ILLUMINANT_LED_RGB1,  /* LED RGB1 (RGB LED mix) */
    ALWAN_ILLUMINANT_LED_V1,    /* LED V1 (violet-pumped) */
    ALWAN_ILLUMINANT_LED_V2,    /* LED V2 */

    /* P8.3: High Pressure illuminants */
    ALWAN_ILLUMINANT_HP1,   /* High Pressure 1 (mercury) */
    ALWAN_ILLUMINANT_HP2,   /* High Pressure 2 */
    ALWAN_ILLUMINANT_HP3,   /* High Pressure 3 */
    ALWAN_ILLUMINANT_HP4,   /* High Pressure 4 */
    ALWAN_ILLUMINANT_HP5    /* High Pressure 5 */
} alwan_illuminant;

/* Enum-based illuminant xy chromaticity accessor
 * Returns xy chromaticity coordinates for the specified illuminant
 * Returns 2 values: x, y chromaticity coordinates
 * Returns ALWAN_E_INVALID if illuminant not supported or xy data not available */
int alwan_data_get_illuminant_xy(alwan_ctx *ctx, alwan_illuminant illuminant,
                                   alwan_scalar **data, size_t *count);

/* View transform identifiers */
typedef enum {
    ALWAN_VIEW_ACES_REC709,  /* ACES RRT + ODT Rec.709 */
    ALWAN_VIEW_AGX,          /* AgX base */
    ALWAN_VIEW_AGX_PUNCHY    /* AgX punchy variant */
} alwan_view_transform;

/* RGB space descriptor with primaries, white point, and transfer function names */
typedef struct {
    alwan_scalar primaries_xy[6];  /* rx, ry, gx, gy, bx, by in CIE xy chromaticity */
    alwan_scalar white_xy[2];       /* wx, wy in CIE xy chromaticity */
    char const *oetf_name;    /* Optional: name of OETF (e.g., "srgb", "pq") */
    char const *eotf_name;    /* Optional: name of EOTF (e.g., "srgb", "pq") */
} alwan_rgb_space_desc;

/* Derive RGB<->XYZ conversion matrices from primaries and white point
 * Returns ALWAN_OK on success, ALWAN_E_RANGE if primaries/white form singular matrix */
int alwan_rgb_derive_matrices(alwan_rgb_space_desc const *desc,
                               alwan_mat3x3 *rgb_to_xyz,
                               alwan_mat3x3 *xyz_to_rgb);

/* Get RGB color space descriptor by enum
 * Loads primaries and white point from data files and populates descriptor
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
int alwan_rgb_get_space_descriptor(alwan_ctx *ctx, alwan_rgb_space space, alwan_rgb_space_desc *desc);

/* ----------------------------------------------------------------
 * RGB Color Space Conversion
 * ---------------------------------------------------------------- */

/* Convert RGB color from one color space to another
 * Handles chromatic adaptation if white points differ (using Bradford CAT by default)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_convert(alwan_ctx *ctx,
                      alwan_rgb_space_desc const *src_space,
                      alwan_rgb_space_desc const *dst_space,
                      alwan_vec3 const *src_rgb,
                      alwan_vec3 *dst_rgb);

/* Bulk RGB color space conversion for arrays of colors
 * More efficient than calling alwan_rgb_convert in a loop
 * count: number of RGB triplets to convert
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_convert_bulk(alwan_ctx *ctx,
                            alwan_rgb_space_desc const *src_space,
                            alwan_rgb_space_desc const *dst_space,
                            alwan_vec3 const *src_rgb,
                            alwan_vec3 *dst_rgb,
                            size_t count);

/* ----------------------------------------------------------------
 * M11: Gamut Utilities & Mapping
 * ---------------------------------------------------------------- */

/* Gamut mapping method */
typedef enum {
    ALWAN_GAMUT_MAP_CLIP = 0,         /* Simple clipping to [0,1] */
    ALWAN_GAMUT_MAP_HUE_PRESERVING = 1 /* Project to gamut boundary preserving hue */
} alwan_gamut_map_method;

/* Estimate RGB gamut volume using Monte Carlo sampling
 * space: RGB color space descriptor
 * num_samples: number of random samples (e.g., 1000000)
 * seed: random seed for reproducibility
 * volume: output volume estimate (in XYZ units cubed)
 * Returns ALWAN_OK on success */
int alwan_gamut_volume_mc(alwan_rgb_space_desc const *space,
                          size_t num_samples,
                          unsigned int seed,
                          alwan_scalar *volume);

/* Map RGB colors to gamut using specified method
 * method: gamut mapping method
 * rgb_in: input RGB colors (may be out of gamut)
 * count: number of RGB triplets
 * rgb_out: output RGB colors (mapped to [0,1] gamut)
 * Returns ALWAN_OK on success */
int alwan_gamut_map(alwan_gamut_map_method method,
                    alwan_vec3 const *rgb_in,
                    size_t count,
                    alwan_vec3 *rgb_out);

/* Map XYZ color to RGB gamut with hue preservation
 * ctx: optional context (can be NULL)
 * space: target RGB space
 * xyz_in: input XYZ color (may be out of RGB gamut)
 * rgb_out: output RGB color (mapped to [0,1] with preserved hue in JCh)
 * Returns ALWAN_OK on success */
int alwan_gamut_map_xyz_to_rgb(alwan_ctx *ctx,
                                alwan_rgb_space_desc const *space,
                                alwan_vec3 const *xyz_in,
                                alwan_vec3 *rgb_out);

/* ----------------------------------------------------------------
 * Transfer Functions (OETF/EOTF)
 * ---------------------------------------------------------------- */

/* Apply Opto-Electronic Transfer Function (linear -> encoded)
 * tf: transfer function to apply
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
int alwan_oetf_apply(alwan_transfer_function tf,
                     alwan_scalar const *linear, size_t count, size_t in_stride,
                     alwan_scalar *encoded, size_t out_stride);

/* Apply Electro-Optical Transfer Function (encoded -> linear)
 * tf: transfer function to apply
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
int alwan_eotf_apply(alwan_transfer_function tf,
                     alwan_scalar const *encoded, size_t count, size_t in_stride,
                     alwan_scalar *linear, size_t out_stride);

/* ----------------------------------------------------------------
 * View Transforms (Display Rendering)
 * ---------------------------------------------------------------- */

/* Apply a view transform (display rendering transform) to RGB data
 * View transforms convert scene-referred RGB to display-referred RGB
 *
 * ctx: optional context (can be NULL for stateless transforms)
 * vt: view transform to apply
 * rgb_in: input RGB triplets (scene-referred, typically ACES AP1 or linear)
 * count: number of RGB triplets
 * in_stride: stride between input RGB triplets (in Scalars, typically 3)
 * rgb_out: output RGB triplets (display-referred)
 * out_stride: stride between output RGB triplets (in Scalars, typically 3)
 *
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if transform not supported */
int alwan_view_transform_apply(alwan_ctx *ctx,
                                alwan_view_transform vt,
                                alwan_scalar const *rgb_in, size_t count, size_t in_stride,
                                alwan_scalar *rgb_out, size_t out_stride);

/* ----------------------------------------------------------------
 * Color Space Conversions
 * ---------------------------------------------------------------- */

/* XYZ <-> xyY conversions */
void alwan_xyz_to_xyy(alwan_vec3 const *xyz, alwan_vec3 *xyy);
void alwan_xyy_to_xyz(alwan_vec3 const *xyy, alwan_vec3 *xyz);

/* XYZ <-> Lab conversions (requires white point in XYZ) */
void alwan_xyz_to_lab(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *lab);
void alwan_lab_to_xyz(alwan_vec3 const *lab, alwan_vec3 const *white_xyz, alwan_vec3 *xyz);

/* XYZ <-> Luv conversions (requires white point in XYZ) */
void alwan_xyz_to_luv(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *luv);
void alwan_luv_to_xyz(alwan_vec3 const *luv, alwan_vec3 const *white_xyz, alwan_vec3 *xyz);

/* XYZ <-> U*V*W* conversions (CIE 1964 uniform color space for CRI)
 * Based on CIE 1960 UCS chromaticity diagram
 * Used specifically for Color Rendering Index calculations
 * Requires white point in XYZ */
void alwan_xyz_to_uvw(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *uvw);
void alwan_uvw_to_xyz(alwan_vec3 const *uvw, alwan_vec3 const *white_xyz, alwan_vec3 *xyz);

/* Lab <-> LCh(ab) conversions */
void alwan_lab_to_lch(alwan_vec3 const *lab, alwan_vec3 *lch);
void alwan_lch_to_lab(alwan_vec3 const *lch, alwan_vec3 *lab);

/* Luv <-> LCh(uv) conversions */
void alwan_luv_to_lchuv(alwan_vec3 const *luv, alwan_vec3 *lchuv);
void alwan_lchuv_to_luv(alwan_vec3 const *lchuv, alwan_vec3 *luv);

/* XYZ <-> Oklab conversions (modern perceptually uniform space, D65 assumed) */
void alwan_xyz_to_oklab(alwan_vec3 const *xyz, alwan_vec3 *oklab);
void alwan_oklab_to_xyz(alwan_vec3 const *oklab, alwan_vec3 *xyz);

/* Oklab <-> Oklch conversions (cylindrical Oklab) */
void alwan_oklab_to_oklch(alwan_vec3 const *oklab, alwan_vec3 *oklch);
void alwan_oklch_to_oklab(alwan_vec3 const *oklch, alwan_vec3 *oklab);

/* Lab <-> DIN99 conversions (DIN99 Family - German color difference standards)
 * - variant: 0 = DIN99/ASTM, 1 = DIN99b, 2 = DIN99c, 3 = DIN99d
 * - All variants provide improved perceptual uniformity over CIE Lab
 * - Input Lab should be D65 adapted
 */
void alwan_lab_to_din99(alwan_vec3 const *lab, alwan_vec3 *din99, int variant);
void alwan_din99_to_lab(alwan_vec3 const *din99, alwan_vec3 *lab, int variant);

/* RGB <-> ICtCp conversions (ITU-R BT.2100 HDR color space)
 * - RGB input/output is linear BT.2020 RGB
 * - use_pq: 1 for PQ (Perceptual Quantizer), 0 for HLG (Hybrid Log-Gamma)
 */
void alwan_rgb_to_ictcp(alwan_vec3 const *rgb, alwan_vec3 *ictcp, int use_pq);
void alwan_ictcp_to_rgb(alwan_vec3 const *ictcp, alwan_vec3 *rgb, int use_pq);

/* XYZ <-> ICtCp conversions (via BT.2020 RGB)
 * - XYZ is assumed to be D65 adapted
 * - use_pq: 1 for PQ (Perceptual Quantizer), 0 for HLG (Hybrid Log-Gamma)
 */
void alwan_xyz_to_ictcp(alwan_vec3 const *xyz, alwan_vec3 *ictcp, int use_pq);
void alwan_ictcp_to_xyz(alwan_vec3 const *ictcp, alwan_vec3 *xyz, int use_pq);

/* Jzazbz <-> XYZ conversions (Perceptually uniform HDR color space)
 * - XYZ input/output is D65 adapted
 * - Jzazbz: Jz (lightness), az (red-green), bz (yellow-blue)
 */
void alwan_xyz_to_jzazbz(alwan_vec3 const *xyz, alwan_vec3 *jzazbz);
void alwan_jzazbz_to_xyz(alwan_vec3 const *jzazbz, alwan_vec3 *xyz);

/* Jzazbz <-> JzCzhz conversions (cylindrical coordinates)
 * - JzCzhz: Jz (lightness), Cz (chroma), hz (hue in radians)
 */
void alwan_jzazbz_to_jzczhz(alwan_vec3 const *jzazbz, alwan_vec3 *jzczhz);
void alwan_jzczhz_to_jzazbz(alwan_vec3 const *jzczhz, alwan_vec3 *jzazbz);

/* Hunter Lab <-> XYZ conversions (Earlier Lab-type color space)
 * - XYZ input/output is D65 adapted by default
 * - Hunter Lab: L (lightness), a (red-green), b (yellow-blue)
 * - Uses square roots instead of cube roots (unlike CIE Lab)
 * - Ka and Kb coefficients are illuminant-dependent
 */
void alwan_xyz_to_hunter_lab(alwan_vec3 const *xyz, alwan_vec3 *hunter_lab);
void alwan_hunter_lab_to_xyz(alwan_vec3 const *hunter_lab, alwan_vec3 *xyz);

/* Hunter Lab <-> XYZ conversions with custom illuminant
 * - xyz_n: Reference white point (e.g., D50, D65, or custom illuminant)
 * - Ka and Kb are calculated automatically based on xyz_n
 */
void alwan_xyz_to_hunter_lab_custom(alwan_vec3 const *xyz, alwan_vec3 *hunter_lab, alwan_vec3 const *xyz_n);
void alwan_hunter_lab_to_xyz_custom(alwan_vec3 const *hunter_lab, alwan_vec3 *xyz, alwan_vec3 const *xyz_n);

/* IPT <-> XYZ conversions (Image Processing Transform)
 * - XYZ input/output is D65 adapted
 * - IPT: I (intensity/lightness), P (red-green), T (yellow-blue)
 * - Improved hue uniformity over CIELAB
 * - Uses power function nonlinearity (exponent 0.43)
 */
void alwan_xyz_to_ipt(alwan_vec3 const *xyz, alwan_vec3 *ipt);
void alwan_ipt_to_xyz(alwan_vec3 const *ipt, alwan_vec3 *xyz);

/* IPT <-> IPTch conversions (cylindrical coordinates)
 * - IPTch: I (intensity), C (chroma), h (hue in radians)
 */
void alwan_ipt_to_iptch(alwan_vec3 const *ipt, alwan_vec3 *iptch);
void alwan_iptch_to_ipt(alwan_vec3 const *iptch, alwan_vec3 *ipt);

/* ProLab <-> XYZ conversions (Perceptually Uniform Projective)
 * - XYZ input/output is D65 adapted by default
 * - ProLab: Uses projective transformation for improved uniformity
 * - Based on Konovalenko et al. (2021)
 */
void alwan_xyz_to_prolab(alwan_vec3 const *xyz, alwan_vec3 *prolab);
void alwan_prolab_to_xyz(alwan_vec3 const *prolab, alwan_vec3 *xyz);

/* ProLab <-> XYZ conversions with custom illuminant
 * - xyz_n: Reference white point (e.g., D50, D65, or custom illuminant)
 */
void alwan_xyz_to_prolab_custom(alwan_vec3 const *xyz, alwan_vec3 *prolab, alwan_vec3 const *xyz_n);
void alwan_prolab_to_xyz_custom(alwan_vec3 const *prolab, alwan_vec3 *xyz, alwan_vec3 const *xyz_n);

/* OSA-UCS <-> XYZ conversions (Optical Society of America Uniform Color Scales)
 * - XYZ input/output is D65 adapted
 * - OSA-UCS: L (lightness), j (yellowness), g (greenness)
 * - Forward transform is exact, inverse is approximate (iterative solution)
 * - Note: Inverse transformation has lower precision than other color spaces
 */
void alwan_xyz_to_osa_ucs(alwan_vec3 const *xyz, alwan_vec3 *osa_ucs);
void alwan_osa_ucs_to_xyz(alwan_vec3 const *osa_ucs, alwan_vec3 *xyz);

/* ----------------------------------------------------------------
 * Color Difference (ΔE) Metrics
 * ---------------------------------------------------------------- */

 // TODO: comment other names for ΔE metrics ΔE*ab, ...
 // TODO: add ΔE*ITP, ΔE*HyAB

/* ΔE*76 - Euclidean distance in Lab space */
alwan_scalar alwan_delta_e_76(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE*94 - CIE 1994 color difference (graphic arts defaults: kL=1, K1=0.045, K2=0.015) */
alwan_scalar alwan_delta_e_94(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE CMC(l:c) - CMC color difference (defaults: l=2, c=1 for acceptability) */
alwan_scalar alwan_delta_e_cmc(alwan_vec3 const *lab1, alwan_vec3 const *lab2, alwan_scalar l, alwan_scalar c);

/* ΔE*00 - CIEDE2000 color difference (most perceptually uniform) */
alwan_scalar alwan_delta_e_2000(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE ITP - ITU-R BT.2124 HDR color difference in ICtCp space (scalar_factor default: 720) */
alwan_scalar alwan_delta_e_itp(alwan_vec3 const *ictcp1, alwan_vec3 const *ictcp2, alwan_scalar scalar_factor);

/* ΔE HyAB - Hybrid Delta E, improved perceptual metric */
alwan_scalar alwan_delta_e_hyab(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE DIN99 - Euclidean distance in DIN99 space (variant: 0=DIN99, 1=b, 2=c, 3=d) */
alwan_scalar alwan_delta_e_din99(alwan_vec3 const *din99_1, alwan_vec3 const *din99_2);

/* ΔE CAM02-LCD - CIECAM02 Large Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam02_lcd(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE CAM02-SCD - CIECAM02 Small Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam02_scd(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE CAM16-LCD - CAM16 Large Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam16_lcd(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE CAM16-SCD - CAM16 Small Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam16_scd(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE ZCAM - Euclidean distance in ZCAM UCS (Jzazbz) space */
alwan_scalar alwan_delta_e_zcam(alwan_vec3 const *jab1, alwan_vec3 const *jab2);

/* ----------------------------------------------------------------
 * Whiteness & Yellowness Indices
 * ---------------------------------------------------------------- */

/* Illuminant/Observer pairs for ASTM E313 calculations */
typedef enum {
    ALWAN_ASTM_E313_C_2DEG = 0,    /* Illuminant C, CIE 1931 2° observer */
    ALWAN_ASTM_E313_D65_2DEG = 1,  /* Illuminant D65, CIE 1931 2° observer */
    ALWAN_ASTM_E313_C_10DEG = 2,   /* Illuminant C, CIE 1964 10° observer */
    ALWAN_ASTM_E313_D65_10DEG = 3  /* Illuminant D65, CIE 1964 10° observer */
} alwan_astm_e313_illuminant;

/* ASTM E313 Yellowness Index
 * xyz: CIE XYZ tristimulus values (normalized to Y=100 for perfect white)
 * illuminant: illuminant/observer pair (C/2°, D65/2°, C/10°, or D65/10°)
 * Returns: Yellowness Index (YI) value */
alwan_scalar alwan_yellowness_astm_e313(alwan_vec3 const *xyz, alwan_astm_e313_illuminant illuminant);

/* ASTM E313 Whiteness Index
 * xyz: CIE XYZ tristimulus values (normalized to Y=100 for perfect white)
 * illuminant: illuminant/observer pair (C/2°, D65/2°, C/10°, or D65/10°)
 * Returns: Whiteness Index (WI) value */
alwan_scalar alwan_whiteness_astm_e313(alwan_vec3 const *xyz, alwan_astm_e313_illuminant illuminant);

/* CIE 2004 Whiteness Index
 * xy: CIE 1931 chromaticity coordinates (x, y)
 * Y: CIE Y tristimulus value (luminance factor)
 * xy_n: reference white chromaticity coordinates
 * Returns: CIE Whiteness (W) value
 * Note: Also computes Tint (T), but this function only returns W.
 *       Tint = 900(xn - x) - 650(yn - y) for 2° observer
 *       Tint = 1000(xn - x) - 650(yn - y) for 10° observer */
alwan_scalar alwan_whiteness_cie2004(alwan_vec3 const *xy, alwan_scalar Y, alwan_vec3 const *xy_n);

/* ----------------------------------------------------------------
 * Chromatic Adaptation Transform (CAT)
 * ---------------------------------------------------------------- */

// TODO: add comment when chromatic adaptation is an identity etc.

/* Chromatic Adaptation Transform (CAT) method */
typedef enum {
    ALWAN_CAT_XYZ_SCALING = 0,  /* Von Kries in XYZ space (simplest) */
    ALWAN_CAT_BRADFORD    = 1,  /* Bradford (most common, used in ICC) */
    ALWAN_CAT_CAT02       = 2,  /* CAT02 (from CIECAM02) */
    ALWAN_CAT_CAT16       = 3,  /* CAT16 (from CAM16) */

    /* P7: Extended CAT methods */
    ALWAN_CAT_SHARP           = 4,  /* Sharp transform */
    ALWAN_CAT_FAIRCHILD       = 5,  /* Fairchild 1990 */
    ALWAN_CAT_CMCCAT97        = 6,  /* CMC CAT97 */
    ALWAN_CAT_CMCCAT2000      = 7,  /* CMC CAT2000 */
    ALWAN_CAT_CAT02_BRILL_2008 = 8, /* CAT02 Brill 2008 variant */
    ALWAN_CAT_BIANCO_2010     = 9,  /* Bianco 2010 */
    ALWAN_CAT_BIANCO_PC_2010  = 10  /* Bianco PC 2010 */
} alwan_cat_method;

/* Compute chromatic adaptation matrix from source to destination white point
 * src_white_xyz: source white point in XYZ (normalized to Y=1)
 * dst_white_xyz: destination white point in XYZ (normalized to Y=1)
 * method: CAT method (Bradford, CAT02, CAT16, or XYZ scaling)
 * out: output 3x3 adaptation matrix
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if white points are invalid */
int alwan_cat_matrix(alwan_vec3 const *src_white_xyz,
                     alwan_vec3 const *dst_white_xyz,
                     alwan_cat_method method,
                     alwan_mat3x3 *out);

/* Apply chromatic adaptation to XYZ colors (bulk operation)
 * xyz_in: input XYZ colors (stride in_stride between consecutive colors)
 * count: number of colors to transform
 * in_stride: stride for input (in Scalars, typically 3 for packed array)
 * src_white_xyz: source white point in XYZ
 * dst_white_xyz: destination white point in XYZ
 * method: CAT method
 * xyz_out: output XYZ colors (stride out_stride between consecutive colors)
 * out_stride: stride for output (in Scalars, typically 3 for packed array)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if parameters are invalid */
int alwan_xyz_adapt(alwan_scalar const *xyz_in, size_t count, size_t in_stride,
                    alwan_vec3 const *src_white_xyz,
                    alwan_vec3 const *dst_white_xyz,
                    alwan_cat_method method,
                    alwan_scalar *xyz_out, size_t out_stride);

/* ----------------------------------------------------------------
 * Spectral Power Distributions (SPD)
 * ---------------------------------------------------------------- */

/* Spectral Power Distribution (SPD) - uniformly sampled spectrum
 * Represents a spectrum as discrete samples at regular wavelength intervals */
typedef struct {
    alwan_scalar *values;        /* SPD values (power/reflectance/transmittance) */
    alwan_scalar wavelength_min; /* Starting wavelength (nm) */
    alwan_scalar wavelength_max; /* Ending wavelength (nm) */
    size_t count;          /* Number of samples */
} alwan_spd;

/* Observer type (standard color matching functions) */

// TODO: add other observers (e.g., Stockman & Sharpe (2000) physiological, 2015 XYZF CMF, ...)
typedef enum {
    ALWAN_OBSERVER_CIE_1931_2DEG = 0,  /* CIE 1931 2° standard observer */
    ALWAN_OBSERVER_CIE_1964_10DEG = 1, /* CIE 1964 10° standard observer */
    ALWAN_OBSERVER_CIE_2012_2DEG = 2,  /* CIE 2012 2° standard observer (physiologically-based) */
    ALWAN_OBSERVER_CIE_2012_10DEG = 3, /* CIE 2012 10° standard observer (physiologically-based) */

    /* P8: Extended observers */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG = 4  /* Stockman & Sharpe 2000 2° cone fundamentals */
} alwan_observer_type;

/* Get white point XYZ for a standard illuminant
 * Computes XYZ tristimulus values from illuminant xy chromaticity (Y normalized to 1.0)
 * Returns ALWAN_E_INVALID if illuminant not supported */
int alwan_illuminant_white_point(alwan_illuminant illuminant,
                                   alwan_observer_type observer,
                                   alwan_vec3 *out_xyz);

/* SPD resampling method */
// TODO: Add nearest (optional: higher-order methods (e.g., spline))
typedef enum {
    ALWAN_RESAMPLE_LINEAR = 0,      /* Linear interpolation */
    ALWAN_RESAMPLE_CATMULL_ROM = 1  /* Catmull-Rom spline (smoother) */
} alwan_resample_method;

/* SPD extrapolation mode (for values outside measured range) */
// TODO: add extrapolate with specified constant value
// TODO: add extrapolate with higher-order methods
typedef enum {
    ALWAN_EXTRAPOLATE_ZERO = 0,     /* Clamp to zero outside range (default for reflectance) */
    ALWAN_EXTRAPOLATE_CONSTANT = 1, /* Repeat edge values (good for smooth SPDs) */
    ALWAN_EXTRAPOLATE_LINEAR = 2    /* Linear extrapolation from edge slope */
} alwan_extrapolate_mode;

/* SPD integration method for computing XYZ */
// TODO: Add rectangular rule
typedef enum {
    ALWAN_INTEGRATE_TRAPEZOID = 0,  /* Trapezoidal rule (fast) */
    ALWAN_INTEGRATE_SIMPSON = 1     /* Simpson's rule (more accurate) */
} alwan_integrate_method;

/* Create SPD with uniform sampling
 * wavelength_min: starting wavelength (nm)
 * wavelength_max: ending wavelength (nm)
 * count: number of samples
 * out: output SPD structure (values allocated internally)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
int alwan_spd_create(alwan_ctx *ctx,
                     alwan_scalar wavelength_min,
                     alwan_scalar wavelength_max,
                     size_t count,
                     alwan_spd *out);

/* Destroy SPD and free allocated values */
void alwan_spd_destroy(alwan_ctx *ctx, alwan_spd *spd);

/* Load standard illuminant SPD
 * ctx: context (for data path and allocation)
 * ill: illuminant to load
 * out: output SPD structure
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if illuminant not supported */
int alwan_spd_illuminant(alwan_ctx *ctx, alwan_illuminant ill, alwan_spd *out);

/* Generate blackbody (Planckian) SPD at given temperature
 * Uses Planck's law to compute spectral radiance
 * ctx: context
 * temperature_K: color temperature in Kelvin (typically 1000-25000K)
 * wavelength_min: starting wavelength (nm)
 * wavelength_max: ending wavelength (nm)
 * count: number of samples
 * out: output SPD structure (values allocated internally)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if temperature out of range */
int alwan_spd_blackbody(alwan_ctx *ctx,
                        alwan_scalar temperature_K,
                        alwan_scalar wavelength_min,
                        alwan_scalar wavelength_max,
                        size_t count,
                        alwan_spd *out);

/* Resample SPD to new wavelength range/count
 * ctx: context
 * src: source SPD
 * wavelength_min: new starting wavelength (nm)
 * wavelength_max: new ending wavelength (nm)
 * count: new number of samples
 * method: resampling method (linear or Catmull-Rom)
 * extrapolate: extrapolation mode for out-of-range values
 * dst: destination SPD (values allocated internally)
 * Returns ALWAN_OK on success */
int alwan_spd_resample(alwan_ctx *ctx,
                       alwan_spd const *src,
                       alwan_scalar wavelength_min,
                       alwan_scalar wavelength_max,
                       size_t count,
                       alwan_resample_method method,
                       alwan_extrapolate_mode extrapolate,
                       alwan_spd *dst);

/* Compute XYZ tristimulus values from SPD
 * ctx: context
 * spd: spectral power distribution (reflectance or emission)
 * illuminant: illuminant SPD (NULL = assume spd is already weighted by illuminant)
 * observer: observer type (CIE 1931/1964/2012 2° or 10°)
 * method: integration method (trapezoid or Simpson)
 * bandpass_nm: bandpass width for Stearns & Stearns correction (0 = no correction)
 * xyz_out: output XYZ tristimulus values
 * Returns ALWAN_OK on success */
int alwan_xyz_from_spd(alwan_ctx *ctx,
                       alwan_spd const *spd,
                       alwan_spd const *illuminant,
                       alwan_observer_type observer,
                       alwan_integrate_method method,
                       alwan_scalar bandpass_nm,
                       alwan_vec3 *xyz_out);

/* ----------------------------------------------------------------
 * P8.1: Spectral Upsampling - RGB to Spectrum Conversion
 * ---------------------------------------------------------------- */

/* Smits1999: RGB to spectrum conversion using basis spectra mixing
 * Reference: Smits, Brian. "An RGB to Spectrum Conversion for Reflectances" (1999)
 * ctx: context (for allocation)
 * rgb: input RGB values (assumed to be in sRGB colorspace, clamped to [0, 1])
 * out_spd: output spectral power distribution (wavelength range: 380-720nm, 10 samples)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
int alwan_rgb_to_spectrum_smits1999(alwan_ctx *ctx,
                                     alwan_vec3 const *rgb,
                                     alwan_spd *out_spd);

/* Mallett2019: RGB to spectrum conversion using spectral primary decomposition
 * Reference: Mallett & Yuksel. "Spectral Primary Decomposition for Rendering with sRGB Reflectance" (2019)
 * ctx: context (for allocation)
 * rgb: input RGB values (assumed to be in sRGB colorspace)
 * out_spd: output spectral power distribution (wavelength range: 380-780nm, 81 samples at 5nm intervals)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
int alwan_rgb_to_spectrum_mallett2019(alwan_ctx *ctx,
                                       alwan_vec3 const *rgb,
                                       alwan_spd *out_spd);

/* Jakob2019: RGB to spectrum using high-quality basis spectra
 * Reference: Jakob & Hanika. "A Low-Dimensional Function Space for Efficient Spectral Upsampling" (2019)
 * ctx: context (for allocation)
 * rgb: input RGB values (assumed to be in sRGB colorspace, clamped to [0, 1])
 * out_spd: output spectral power distribution (wavelength range: 360-780nm, 85 samples at 5nm intervals)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure
 * Note: This is a simplified implementation using Smits-style algorithm with Jakob2019 basis spectra.
 *       Full Jakob2019 would use polynomial LUT, but this provides high-quality results. */
int alwan_rgb_to_spectrum_jakob2019(alwan_ctx *ctx,
                                      alwan_vec3 const *rgb,
                                      alwan_spd *out_spd);

/* TODO(P8.1): Meng2015: XYZ to spectrum using optimization
 * Reference: Meng et al. "Physically Meaningful Rendering using Tristimulus Colours" (2015)
 * Requires: Iterative optimization solver, computationally intensive
 * Implementation notes:
 *   - Uses constrained optimization to find smooth reflectance spectrum
 *   - Parameters: interval (wavelength spacing), tolerance, max_iterations
 *   - Slower than Smits/Mallett but produces smooth, physically plausible spectra
 * Status: NOT YET IMPLEMENTED - requires numerical optimization library */

/* ----------------------------------------------------------------
 * CIECAM02 Color Appearance Model
 * ---------------------------------------------------------------- */

/* CIECAM02 surround condition */
typedef enum {
    ALWAN_CIECAM02_SURROUND_AVERAGE = 0,  /* Average surround (F=1.0, c=0.69, Nc=1.0) - typical viewing */
    ALWAN_CIECAM02_SURROUND_DIM = 1,      /* Dim surround (F=0.9, c=0.59, Nc=0.95) - dark room with screen */
    ALWAN_CIECAM02_SURROUND_DARK = 2      /* Dark surround (F=0.8, c=0.525, Nc=0.8) - cutsheet transparency */
} alwan_ciecam02_surround;

/* CIECAM02 viewing conditions */
typedef struct {
    alwan_vec3 white_xyz;                  /* Reference white in XYZ (Y typically 100) */
    alwan_scalar adapting_luminance;             /* Luminance of adapting field (La) in cd/m² (typically 20% of white Y) */
    alwan_scalar background_luminance;           /* Relative luminance of background (Yb/Yw, typically 0.2 for 20% gray) */
    alwan_ciecam02_surround surround;      /* Viewing surround condition */
    int discount_illuminant;               /* 1 to discount illuminant, 0 otherwise (affects D) */
} alwan_ciecam02_viewing_conditions;

/* CIECAM02 appearance correlates (all outputs from forward transform) */
typedef struct {
    alwan_scalar J;  /* Lightness (0-100) */
    alwan_scalar C;  /* Chroma (0+) */
    alwan_scalar h;  /* Hue angle (0-360 degrees) */
    alwan_scalar s;  /* Saturation (0+) */
    alwan_scalar Q;  /* Brightness (0+) */
    alwan_scalar M;  /* Colorfulness (0+) */
    alwan_scalar H;  /* Hue quadrature (0-400) */
} alwan_ciecam02_correlates;

/* CIECAM02 forward transform: XYZ -> appearance correlates
 * xyz: input color in XYZ (same white point as viewing conditions)
 * vc: viewing conditions
 * out: output appearance correlates (J, C, h, s, Q, M, H)
 * Returns ALWAN_OK on success */
int alwan_ciecam02_forward(alwan_vec3 const *xyz,
                            alwan_ciecam02_viewing_conditions const *vc,
                            alwan_ciecam02_correlates *out);

/* CIECAM02 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
int alwan_ciecam02_inverse(alwan_ciecam02_correlates const *correlates,
                            alwan_ciecam02_viewing_conditions const *vc,
                            alwan_vec3 *xyz_out);

/* ----------------------------------------------------------------
 * CAM16 Color Appearance Model
 * ---------------------------------------------------------------- */

/* CAM16 surround condition (same as CIECAM02) */
typedef enum {
    ALWAN_CAM16_SURROUND_AVERAGE = 0,  /* Average surround (F=1.0, c=0.69, Nc=1.0) - typical viewing */
    ALWAN_CAM16_SURROUND_DIM = 1,      /* Dim surround (F=0.9, c=0.59, Nc=0.95) - dark room with screen */
    ALWAN_CAM16_SURROUND_DARK = 2      /* Dark surround (F=0.8, c=0.525, Nc=0.8) - cutsheet transparency */
} alwan_cam16_surround;

/* CAM16 viewing conditions (similar to CIECAM02 but uses CAT16) */
typedef struct {
    alwan_vec3 white_xyz;                  /* Reference white in XYZ (Y typically 100) */
    alwan_scalar adapting_luminance;             /* Luminance of adapting field (La) in cd/m² */
    alwan_scalar background_luminance;           /* Relative luminance of background (Yb/Yw, typically 0.2) */
    alwan_cam16_surround surround;         /* Viewing surround condition */
    int discount_illuminant;               /* 1 to discount illuminant, 0 otherwise */
} alwan_cam16_viewing_conditions;

/* CAM16 appearance correlates */
typedef struct {
    alwan_scalar J;  /* Lightness (0-100) */
    alwan_scalar C;  /* Chroma (0+) */
    alwan_scalar h;  /* Hue angle (0-360 degrees) */
    alwan_scalar s;  /* Saturation (0+) */
    alwan_scalar Q;  /* Brightness (0+) */
    alwan_scalar M;  /* Colorfulness (0+) */
    alwan_scalar H;  /* Hue quadrature (0-400) */
} alwan_cam16_correlates;

/* CAM16 forward transform: XYZ -> appearance correlates
 * xyz: input color in XYZ (same white point as viewing conditions)
 * vc: viewing conditions
 * out: output appearance correlates (J, C, h, s, Q, M, H)
 * Returns ALWAN_OK on success */
int alwan_cam16_forward(alwan_vec3 const *xyz,
                        alwan_cam16_viewing_conditions const *vc,
                        alwan_cam16_correlates *out);

/* CAM16 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
int alwan_cam16_inverse(alwan_cam16_correlates const *correlates,
                        alwan_cam16_viewing_conditions const *vc,
                        alwan_vec3 *xyz_out);

/* CAM16-UCS (Uniform Color Space) transform for perceptual distance metrics
 * Converts CAM16 JMh to CAM16-UCS Jab for computing perceptual distances
 * correlates: input CAM16 correlates (J, M, h used)
 * Jab_out: output CAM16-UCS coordinates [J', a', b']
 * Returns ALWAN_OK on success */
int alwan_cam16_to_ucs(alwan_cam16_correlates const *correlates,
                       alwan_vec3 *Jab_out);

/* Inverse CAM16-UCS transform
 * Converts CAM16-UCS Jab back to CAM16 JMh
 * Jab: input CAM16-UCS coordinates [J', a', b']
 * correlates_out: output CAM16 correlates (J, M, h filled; other fields set to 0)
 * Returns ALWAN_OK on success */
int alwan_cam16_from_ucs(alwan_vec3 const *Jab,
                         alwan_cam16_correlates *correlates_out);

/* ----------------------------------------------------------------
 * ZCAM - HDR Color Appearance Model
 * Based on Safdar et al. (2021), uses Jzazbz color space
 * Supports HDR luminance range 0.001-10,000 cd/m²
 * ---------------------------------------------------------------- */

/* ZCAM surround condition */
typedef enum {
    ALWAN_ZCAM_SURROUND_AVERAGE = 0,  /* Average surround (Fs=1.0, c=0.69, Nc=1.0) */
    ALWAN_ZCAM_SURROUND_DIM = 1,      /* Dim surround (Fs=0.9, c=0.59, Nc=0.9) */
    ALWAN_ZCAM_SURROUND_DARK = 2      /* Dark surround (Fs=0.8, c=0.525, Nc=0.8) */
} alwan_zcam_surround;

/* ZCAM viewing conditions */
typedef struct {
    alwan_vec3 xyz_w;                  /* White point (absolute XYZ in cd/m²) */
    alwan_scalar La;                   /* Adapting luminance (cd/m²) */
    alwan_scalar Yb;                   /* Background luminance factor */
    alwan_zcam_surround surround;      /* Viewing surround condition */
    int discount_illuminant;           /* 1 to discount illuminant, 0 otherwise */
} alwan_zcam_viewing_conditions;

/* ZCAM appearance correlates (comprehensive HDR attributes) */
typedef struct {
    alwan_scalar Jz;     /* Lightness */
    alwan_scalar Cz;     /* Chroma */
    alwan_scalar hz;     /* Hue angle (degrees) */
    alwan_scalar Qz;     /* Brightness */
    alwan_scalar Mz;     /* Colorfulness */
    alwan_scalar Sz;     /* Saturation */
    alwan_scalar Vz;     /* Vividness */
    alwan_scalar Kz;     /* Blackness */
    alwan_scalar Wz;     /* Whiteness */
} alwan_zcam_correlates;

/* ZCAM forward transform: XYZ -> appearance correlates
 * xyz: absolute XYZ tristimulus values (cd/m²)
 * vc: viewing conditions
 * out: computed appearance correlates
 * Returns 0 on success, -1 on error */
int alwan_zcam_forward(alwan_vec3 const *xyz,
                       alwan_zcam_viewing_conditions const *vc,
                       alwan_zcam_correlates *out);

/* ZCAM inverse transform: appearance correlates -> XYZ (approximate)
 * correlates: appearance correlates
 * vc: viewing conditions
 * xyz: output XYZ tristimulus values (cd/m²)
 * Returns 0 on success, -1 on error
 * Note: Inverse is approximate due to complexity */
int alwan_zcam_inverse(alwan_zcam_correlates const *correlates,
                       alwan_zcam_viewing_conditions const *vc,
                       alwan_vec3 *xyz);

/* ZCAM to UCS (Uniform Color Space) for color difference
 * correlates: input ZCAM correlates
 * Jab_out: output ZCAM-UCS coordinates [Jz, az, bz]
 * Returns 0 on success, -1 on error */
int alwan_zcam_to_ucs(alwan_zcam_correlates const *correlates,
                      alwan_vec3 *Jab_out);

/* ----------------------------------------------------------------
 * RLAB Color Appearance Model
 * Based on Fairchild (1993, 1996)
 * Cross-media color reproduction model
 * ---------------------------------------------------------------- */

/* RLAB surround condition */
typedef enum {
    ALWAN_RLAB_SURROUND_AVERAGE = 0,  /* Average surround (sigma=1/2.3) */
    ALWAN_RLAB_SURROUND_DIM = 1,      /* Dim surround (sigma=1/2.9) */
    ALWAN_RLAB_SURROUND_DARK = 2      /* Dark surround (sigma=1/3.5) */
} alwan_rlab_surround;

/* RLAB viewing conditions */
typedef struct {
    alwan_vec3 xyz_w;              /* White point (XYZ, Y=100) */
    alwan_vec3 xyz_n;              /* Reference white (usually D65) */
    alwan_rlab_surround surround;  /* Viewing surround */
    int D_factor;                  /* Discounting factor: 0=auto, 1=hard copy, 2=soft copy, 3=transparency */
} alwan_rlab_viewing_conditions;

/* RLAB appearance correlates */
typedef struct {
    alwan_scalar L;    /* Lightness */
    alwan_scalar C;    /* Chroma */
    alwan_scalar h;    /* Hue angle (degrees) */
    alwan_scalar s;    /* Saturation */
    alwan_scalar a;    /* Red-green opponent */
    alwan_scalar b;    /* Yellow-blue opponent */
} alwan_rlab_correlates;

/* RLAB forward transform: XYZ -> appearance correlates
 * Returns 0 on success, -1 on error */
int alwan_rlab_forward(alwan_vec3 const *xyz,
                       alwan_rlab_viewing_conditions const *vc,
                       alwan_rlab_correlates *out);

/* RLAB inverse transform: appearance correlates -> XYZ
 * Returns 0 on success, -1 on error */
int alwan_rlab_inverse(alwan_rlab_correlates const *correlates,
                       alwan_rlab_viewing_conditions const *vc,
                       alwan_vec3 *xyz);

/* ----------------------------------------------------------------
 * Hunt Color Appearance Model
 * Based on Hunt (1991, 1995)
 * Comprehensive historical CAM
 * ---------------------------------------------------------------- */

/* Hunt surround condition */
typedef enum {
    ALWAN_HUNT_SURROUND_NORMAL = 0,  /* Normal scenes (Nc=1.0, Nb=75) */
    ALWAN_HUNT_SURROUND_DIM = 1,     /* TV/CRT dim surrounds (Nc=1.0, Nb=25) */
    ALWAN_HUNT_SURROUND_DARK = 2     /* Projected transparencies dark (Nc=0.7, Nb=10) */
} alwan_hunt_surround;

/* Hunt viewing conditions */
typedef struct {
    alwan_vec3 xyz_w;                  /* White point (XYZ, Y=100) */
    alwan_scalar La;                   /* Adapting luminance (cd/m²) */
    alwan_scalar Yb;                   /* Background luminance factor */
    alwan_hunt_surround surround;      /* Viewing surround */
    int discount_illuminant;           /* 1 to discount illuminant, 0 otherwise */
} alwan_hunt_viewing_conditions;

/* Hunt appearance correlates */
typedef struct {
    alwan_scalar J;    /* Lightness */
    alwan_scalar C;    /* Chroma */
    alwan_scalar h;    /* Hue angle (degrees) */
    alwan_scalar s;    /* Saturation */
    alwan_scalar Q;    /* Brightness */
    alwan_scalar M;    /* Colourfulness */
} alwan_hunt_correlates;

/* Hunt forward transform: XYZ -> appearance correlates
 * Returns 0 on success, -1 on error
 * Note: Hunt inverse is not implemented due to extreme complexity */
int alwan_hunt_forward(alwan_vec3 const *xyz,
                       alwan_hunt_viewing_conditions const *vc,
                       alwan_hunt_correlates *out);

/* ----------------------------------------------------------------
 * M9: Convenience Color Models (HSV, HSL, CMY, CMYK, YCbCr)
 * ---------------------------------------------------------------- */

/* RGB <-> HSV conversions (all values in [0, 1]) */
int alwan_rgb_to_hsv(alwan_vec3 const *rgb, alwan_vec3 *hsv_out);
int alwan_hsv_to_rgb(alwan_vec3 const *hsv, alwan_vec3 *rgb_out);

/* RGB <-> HSL conversions (all values in [0, 1]) */
int alwan_rgb_to_hsl(alwan_vec3 const *rgb, alwan_vec3 *hsl_out);
int alwan_hsl_to_rgb(alwan_vec3 const *hsl, alwan_vec3 *rgb_out);

/* RGB <-> CMY conversions (all values in [0, 1]) */
int alwan_rgb_to_cmy(alwan_vec3 const *rgb, alwan_vec3 *cmy_out);
int alwan_cmy_to_rgb(alwan_vec3 const *cmy, alwan_vec3 *rgb_out);

// TODO: add alwan_vec4 for CMYK output
/* CMY <-> CMYK conversions (all values in [0, 1]) */
int alwan_cmy_to_cmyk(alwan_vec3 const *cmy, alwan_scalar *c, alwan_scalar *m, alwan_scalar *y, alwan_scalar *k);
int alwan_cmyk_to_cmy(alwan_scalar c, alwan_scalar m, alwan_scalar y, alwan_scalar k, alwan_vec3 *cmy_out);

/* YCbCr standard identifiers */
typedef enum {
    ALWAN_YCBCR_BT601,    /* ITU-R BT.601 (SD) */
    ALWAN_YCBCR_BT709,    /* ITU-R BT.709 (HD) */
    ALWAN_YCBCR_BT2020    /* ITU-R BT.2020 (UHD) */
} alwan_ycbcr_standard;

/* RGB <-> YCbCr conversions (RGB in [0, 1], YCbCr full range [0, 1]) */
int alwan_rgb_to_ycbcr(alwan_vec3 const *rgb, alwan_ycbcr_standard standard, alwan_vec3 *ycbcr_out);
int alwan_ycbcr_to_rgb(alwan_vec3 const *ycbcr, alwan_ycbcr_standard standard, alwan_vec3 *rgb_out);

/* RGB <-> YcCbcCrc conversions (constant luminance, BT.2020) */
int alwan_rgb_to_yccbccrc(alwan_vec3 const *rgb, alwan_vec3 *yccbccrc_out);
int alwan_yccbccrc_to_rgb(alwan_vec3 const *yccbccrc, alwan_vec3 *rgb_out);

/* ----------------------------------------------------------------
 * M10: Light Quality & CCT (Correlated Color Temperature)
 * ---------------------------------------------------------------- */

/* CCT estimation from chromaticity coordinates (xy) */
/* McCamy approximation: fast, ~2% accuracy above 2800K */
alwan_scalar alwan_cct_mccamy_xy(alwan_vec3 const *xy);

/* Robertson method: accurate, iterative lookup against Planckian locus */
/* Returns CCT in Kelvin, or negative value on error */
alwan_scalar alwan_cct_robertson_xy(alwan_vec3 const *xy);

/* CRI (Color Rendering Index) Ra - average of 8 TCS samples */
/* Requires SPD (spectral power distribution) */
/* Returns CRI Ra value [0, 100], or negative on error */
alwan_scalar alwan_cri_ra(alwan_ctx *ctx, alwan_spd const *test_spd);

/* CQS (Color Quality Scale) - NIST metric using 15 saturated samples */
/* Returns CQS value [0, 100], or negative on error */
/* Note: Full implementation requires CMCCAT2000 CAT and VS sample data */
alwan_scalar alwan_cqs_calculate(alwan_ctx *ctx, alwan_spd const *test_spd);

/* TM-30 (IES Method) - Fidelity (Rf) using 99 CES samples */
/* Returns Rf value [0, 100], or negative on error */
/* Note: Full implementation requires CIECAM02 and 99 CES sample data */
alwan_scalar alwan_tm30_rf(alwan_ctx *ctx, alwan_spd const *test_spd);

/* CIE 224:2017 Color Fidelity Index (Rf) using 99 CES samples */
/* Returns Rf value [0, 100], or negative on error */
/* Note: Uses same algorithm as TM-30 but per CIE 224:2017 standard */
alwan_scalar alwan_cie224_rf(alwan_ctx *ctx, alwan_spd const *test_spd);

/* SSI (Spectral Similarity Index) - Academy/SMPTE ST 2122 */
/* Measures spectral similarity between test and reference light sources */
/* test_spd: test illuminant SPD
 * reference_spd: reference illuminant SPD
 * Returns SSI value [0, 100], where 100 = perfect match, or negative on error */
alwan_scalar alwan_ssi_calculate(alwan_ctx *ctx, alwan_spd const *test_spd, alwan_spd const *reference_spd);

/* CIE Special Metamerism Index: Change in Illuminant */
/* Quantifies color mismatch when samples that match under reference illuminant are viewed under test illuminant */
/* sample_reflectance: reflectance spectrum of sample
 * reference_reflectance: reflectance spectrum of reference
 * reference_illuminant: illuminant under which samples match (e.g., D65)
 * test_illuminant: illuminant under which to evaluate mismatch (e.g., A)
 * observer: observer type (2° or 10°)
 * Returns metamerism index (ΔE*ab under test illuminant), or negative on error */
alwan_scalar alwan_metamerism_index(alwan_ctx *ctx,
                                     alwan_spd const *sample_reflectance,
                                     alwan_spd const *reference_reflectance,
                                     alwan_spd const *reference_illuminant,
                                     alwan_spd const *test_illuminant,
                                     alwan_observer_type observer);

/* ----------------------------------------------------------------
 * Utility Functions
 * ---------------------------------------------------------------- */

/* Minimum of two values */
static inline alwan_scalar alwan_min(alwan_scalar a, alwan_scalar b) {
    return (a < b) ? a : b;
}

/* Maximum of two values */
static inline alwan_scalar alwan_max(alwan_scalar a, alwan_scalar b) {
    return (a > b) ? a : b;
}

/* Minimum of three values */
static inline alwan_scalar alwan_min3(alwan_scalar a, alwan_scalar b, alwan_scalar c) {
    alwan_scalar m = a;
    if (b < m) m = b;
    if (c < m) m = c;
    return m;
}

/* Maximum of three values */
static inline alwan_scalar alwan_max3(alwan_scalar a, alwan_scalar b, alwan_scalar c) {
    alwan_scalar m = a;
    if (b > m) m = b;
    if (c > m) m = c;
    return m;
}

/* Clamp scalar to [min, max] */
static inline alwan_scalar alwan_clamp(alwan_scalar x, alwan_scalar min, alwan_scalar max) {
    return (x < min) ? min : (x > max) ? max : x;
}

/* Saturate (clamp to [0, 1]) */
static inline alwan_scalar alwan_saturate(alwan_scalar x) {
    if (x < ALWAN_LITERAL(0.0)) return ALWAN_LITERAL(0.0);
    if (x > ALWAN_LITERAL(1.0)) return ALWAN_LITERAL(1.0);
    return x;
}

/* Linear interpolation (numerically stable) */
static inline alwan_scalar alwan_lerp(alwan_scalar a, alwan_scalar b, alwan_scalar t) {
    return (ALWAN_LITERAL(1.0) - t) * a + t * b;
}

#ifdef __cplusplus
}
#endif

#endif /* ALWAN_H */
