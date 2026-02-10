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
int alwan_data_get_illuminant_a(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D50 (horizon daylight) */
int alwan_data_get_illuminant_d50(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D55 (mid-morning daylight) */
int alwan_data_get_illuminant_d55(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D60 (daylight) */
int alwan_data_get_illuminant_d60(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D65 (noon daylight) */
int alwan_data_get_illuminant_d65(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant E (equal energy) */
int alwan_data_get_illuminant_e(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant B (direct sunlight) */
int alwan_data_get_illuminant_b(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant C (average daylight) */
int alwan_data_get_illuminant_c(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D75 (daylight 7500K) */
int alwan_data_get_illuminant_d75(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

/* Get sRGB primaries (6 values: rx, ry, gx, gy, bx, by) */
int alwan_data_get_srgb_primaries(alwan_scalar **data, size_t *count, alwan_ctx *ctx);

#if !ALWAN_EMBED_DATA
/* Free data allocated by runtime loader (no-op in embedded mode) */
void alwan_data_free(alwan_ctx *ctx, alwan_scalar *data);
#endif

/* ----------------------------------------------------------------
 * Math Types
 * ---------------------------------------------------------------- */

/* 2-component vector (for xy chromaticity coordinates) */
typedef struct {
    alwan_scalar v[2];
} alwan_vec2;

/* 3-component vector */
typedef struct {
    alwan_scalar v[3];
} alwan_vec3;

/* 3x3 matrix stored in row-major order: [m00 m01 m02 m10 m11 m12 m20 m21 m22] */
typedef struct {
    alwan_scalar m[9];
} alwan_mat3x3;

/* ----------------------------------------------------------------
 * Semantic Color Types (for type-safe I/O)
 * ---------------------------------------------------------------- */

/*
 * Semantic color types provide type safety while maintaining API versatility.
 * All types are layout-compatible with alwan_vec3, allowing:
 *   - Direct casting: (alwan_vec3*)&rgb
 *   - Member pointer passing: alwan_function(&rgb.r, &out.r)
 *   - Array interpretation: alwan_scalar* ptr = &rgb.r;
 */

/* RGB color (red, green, blue) - typically linear or encoded depending on context */
typedef struct {
    alwan_scalar r, g, b;
} alwan_rgb;

/* CMYK color (cyan, magenta, yellow, black) - note: uses 4 components, not compatible with vec3 */
typedef struct {
    alwan_scalar c, m, y, k;
} alwan_cmyk;

/* CMY color (cyan, magenta, yellow) */
typedef struct {
    alwan_scalar c, m, y;
} alwan_cmy;

/* HSV color (hue [0-360°], saturation [0-1], value [0-1]) */
typedef struct {
    alwan_scalar h, s, v;
} alwan_hsv;

/* HSL color (hue [0-360°], saturation [0-1], lightness [0-1]) */
typedef struct {
    alwan_scalar h, s, l;
} alwan_hsl;

/* XYZ tristimulus values (CIE 1931) */
typedef struct {
    alwan_scalar x, y, z;
} alwan_xyz;

/* xyY chromaticity + luminance (CIE 1931) */
typedef struct {
    alwan_scalar x, y, Y;
} alwan_xyy;

/* CIE Lab color (L* [0-100], a*, b*) */
typedef struct {
    alwan_scalar L, a, b;
} alwan_lab;

/* CIE Luv color (L* [0-100], u*, v*) */
typedef struct {
    alwan_scalar L, u, v;
} alwan_luv;

/* CIE LCh (cylindrical Lab): Lightness, Chroma, Hue */
typedef struct {
    alwan_scalar L, C, h;
} alwan_lch;

/* CIE LCh(uv) (cylindrical Luv): Lightness, Chroma, Hue */
typedef struct {
    alwan_scalar L, C, h;
} alwan_lchuv;

/* Oklab color space (modern perceptual) */
typedef struct {
    alwan_scalar L, a, b;
} alwan_oklab;

/* Oklch color space (cylindrical Oklab) */
typedef struct {
    alwan_scalar L, C, h;
} alwan_oklch;

/* Jzazbz color space (HDR perceptual) */
typedef struct {
    alwan_scalar Jz, az, bz;
} alwan_jzazbz;

/* JzCzhz (cylindrical Jzazbz) */
typedef struct {
    alwan_scalar Jz, Cz, hz;
} alwan_jzczhz;

/* ICtCp color space (ITU-R BT.2100 HDR) */
typedef struct {
    alwan_scalar I, Ct, Cp;
} alwan_ictcp;

/* IPT color space */
typedef struct {
    alwan_scalar I, P, T;
} alwan_ipt;

/* IgPgTg color space (Ebner & Fairchild 1998) */
typedef struct {
    alwan_scalar Ig, Pg, Tg;
} alwan_igpgtg;

/* ICaCb color space (Zhang & Wandell 1996, 1997) */
typedef struct {
    alwan_scalar I, Ca, Cb;
} alwan_icacb;

/* YCbCr color (luma + chroma) */
typedef struct {
    alwan_scalar Y, Cb, Cr;
} alwan_ycbcr;

/* YCoCg color space (luma + orange-cyan + green-magenta) */
typedef struct {
    alwan_scalar Y, Co, Cg;
} alwan_ycocg;

/* Y'Cb'Cr'c' color space (luma + chroma with reduced range) */
typedef struct
{
    alwan_scalar Yc, Cbc, Crc;
} alwan_yccbccrc;

/* UVW color space (CIE 1964) */
typedef struct {
    alwan_scalar U, V, W;
} alwan_uvw;

/* DIN99 color space */
typedef struct {
    alwan_scalar L99, a99, b99;
} alwan_din99;

/* Hunter Lab color space */
typedef struct {
    alwan_scalar L, a, b;
} alwan_hunter_lab;

/* IPT cylindrical form (I, Chroma, Hue) */
typedef struct {
    alwan_scalar I, C, h;
} alwan_iptch;

/* ProLab color space */
typedef struct {
    alwan_scalar L, a, b;
} alwan_prolab;

/* OSA-UCS color space (Ljg) */
typedef struct {
    alwan_scalar L, j, g;
} alwan_osa_ucs;

/* CIE 1960 UCS color space */
typedef struct {
    alwan_scalar U, V, W;
} alwan_ucs;

/* Prismatic color space */
typedef struct {
    alwan_scalar L, s, h;
} alwan_prismatic;

/* HCL color space (Hue, Chroma, Luminance - in RGB context) */
typedef struct {
    alwan_scalar H, C, L;
} alwan_hcl;

/* IHLS color space (Improved HLS) */
typedef struct {
    alwan_scalar H, L, S;
} alwan_ihls;

/* CAM Jab color (J, a, b - for CAM02/CAM16) */
typedef struct {
    alwan_scalar J, a, b;
} alwan_cam_jab;

/*
 * Usage examples:
 *
 * // Direct function call with semantic types
 * alwan_rgb rgb_in = {1.0, 0.5, 0.2};
 * alwan_xyz xyz_out;
 * alwan_rgb_to_xyz((alwan_vec3*)&rgb_in, (alwan_vec3*)&xyz_out);
 *
 * // Member pointer passing
 * alwan_function(&rgb_in.r, &xyz_out.x);
 *
 * // Array access
 * alwan_scalar* rgb_array = &rgb_in.r;
 * for (int i = 0; i < 3; i++) {
 *     rgb_array[i] *= 2.0;
 * }
 */

/* ----------------------------------------------------------------
 * Math Operations
 * ---------------------------------------------------------------- */

/* Multiply two 3x3 matrices: out = a * b */
void alwan_mat3_mul(alwan_mat3x3 *out, alwan_mat3x3 const *a, alwan_mat3x3 const *b);

/* Invert a 3x3 matrix using partial-pivot Gaussian elimination
 * Returns ALWAN_OK on success, ALWAN_E_RANGE if matrix is singular */
int alwan_mat3_inv(alwan_mat3x3 *out, alwan_mat3x3 const *m);

/* Multiply matrix by vector: out = m * v */
void alwan_mat3_mulv(alwan_vec3 *out, alwan_mat3x3 const *m, alwan_vec3 const *v);

/* Create identity matrix */
void alwan_mat3_identity(alwan_mat3x3 *out);

/* Compute the determinant of a 3x3 matrix */
alwan_scalar alwan_mat3_det(alwan_mat3x3 const *m);

/* Bulk matrix-vector multiplication: out[i] = m * in[i]
 * Transforms array of 3D vectors by the same matrix
 * vec_out: output vectors (stride out_stride between consecutive vectors)
 * matrix: transformation matrix (applied to all vectors)
 * vec_in: input vectors (stride in_stride between consecutive vectors)
 * count: number of vectors to transform
 * in_stride: input stride in bytes (typically 3*sizeof(alwan_scalar))
 * out_stride: output stride in bytes (typically 3*sizeof(alwan_scalar))
 * Returns ALWAN_OK on success */
int alwan_mat3_transform_bulk(alwan_scalar *vec_out,
                              alwan_mat3x3 const *matrix,
                              alwan_scalar const *vec_in,
                              size_t count,
                              size_t in_stride,
                              size_t out_stride);

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

    /* ACES Family extensions */
    ALWAN_RGB_SPACE_ACESCC,         /* ACES Color Correction */
    ALWAN_RGB_SPACE_ACESCCT,        /* ACES Color Correction with toe */

    /* ARRI Camera Spaces */
    ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_3,
    ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_4,
    ALWAN_RGB_SPACE_ARRI_LOGC3,         /* ARRI LogC3 (WG3 primaries + LogC3 OETF) */
    ALWAN_RGB_SPACE_ARRI_LOGC4,         /* ARRI LogC4 (WG4 primaries + LogC4 OETF) */

    /* RED Camera Spaces (extended) */
    ALWAN_RGB_SPACE_REDCOLOR,       /* RED Color 1 */
    ALWAN_RGB_SPACE_REDCOLOR2,      /* RED Color 2 */
    ALWAN_RGB_SPACE_REDCOLOR3,      /* RED Color 3 */
    ALWAN_RGB_SPACE_REDCOLOR4,      /* RED Color 4 */
    ALWAN_RGB_SPACE_DRAGONCOLOR,    /* RED Dragon Color */
    ALWAN_RGB_SPACE_DRAGONCOLOR2,   /* RED Dragon Color 2 */
    ALWAN_RGB_SPACE_REDLOG,         /* REDLog (REDWideGamutRGB primaries + REDLog OETF) */

    /* Sony Camera Spaces (extended) */
    ALWAN_RGB_SPACE_VENICE_S_GAMUT3,
    ALWAN_RGB_SPACE_VENICE_S_GAMUT3_CINE,
    ALWAN_RGB_SPACE_S_LOG,          /* S-Log (S-Gamut3 primaries + S-Log OETF) */
    ALWAN_RGB_SPACE_S_LOG2,         /* S-Log2 (S-Gamut3 primaries + S-Log2 OETF) */
    ALWAN_RGB_SPACE_S_LOG3,         /* S-Log3 (S-Gamut3 primaries + S-Log3 OETF) */

    /* Historical/Reference */
    ALWAN_RGB_SPACE_CIE_RGB,        /* CIE 1931 RGB */

    /* Professional/Photography (extended) */
    ALWAN_RGB_SPACE_ADOBE_WIDE_GAMUT_RGB,
    ALWAN_RGB_SPACE_ROMM_RGB,       /* Reference Output Medium Metric RGB */
    ALWAN_RGB_SPACE_RIMM_RGB,       /* Reference Input Medium Metric RGB */
    ALWAN_RGB_SPACE_ERIMM_RGB,      /* Extended RIMM RGB */

    /* DaVinci/FilmLight */
    ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT,
    ALWAN_RGB_SPACE_FILMLIGHT_T_LOG,    /* FilmLight T-Log (E-Gamut primaries + T-Log OETF) */

    /* Fujifilm Camera Spaces */
    ALWAN_RGB_SPACE_F_GAMUT,
    ALWAN_RGB_SPACE_FUJIFILM_F_LOG,     /* Fujifilm F-Log (F-Gamut primaries + F-Log OETF) */

    /* Nikon Camera Spaces */
    ALWAN_RGB_SPACE_N_GAMUT,
    ALWAN_RGB_SPACE_N_LOG,              /* N-Log (N-Gamut primaries + N-Log OETF) */

    /* DJI Camera Spaces */
    ALWAN_RGB_SPACE_DJI_D_GAMUT,

    /* GoPro Camera Spaces */
    ALWAN_RGB_SPACE_PROTUNE_NATIVE,

    /* Legacy Broadcast (extended) */
    ALWAN_RGB_SPACE_ITU_R_BT470_525,
    ALWAN_RGB_SPACE_ITU_R_BT470_625,
    ALWAN_RGB_SPACE_SMPTE_240M,
    ALWAN_RGB_SPACE_SMPTE_C,

    /* Digital Cinema & Mastering */
    ALWAN_RGB_SPACE_DCDM_XYZ,

    /* Print/Specialized Spaces */
    ALWAN_RGB_SPACE_BEST_RGB,
    ALWAN_RGB_SPACE_BETA_RGB,
    ALWAN_RGB_SPACE_DON_RGB_4,
    ALWAN_RGB_SPACE_EKTA_SPACE_PS5,
    ALWAN_RGB_SPACE_MAX_RGB,
    ALWAN_RGB_SPACE_RUSSELL_RGB,

    /* Historical/Reference (additional) */
    ALWAN_RGB_SPACE_SHARP_RGB,
    ALWAN_RGB_SPACE_ECI_RGB_V2,

    /* ========== EXISTING SPACES (kept for compatibility) ========== */

    /* Adobe RGB (1998) - Photography/print workflow */
    ALWAN_RGB_SPACE_ADOBE_RGB_1998,

    /* ProPhoto RGB - Wide gamut professional */
    ALWAN_RGB_SPACE_PROPHOTO_RGB,

    /* Cinema/Broadcast spaces */
    ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT,
    ALWAN_RGB_SPACE_DAVINCI_INTERMEDIATE, /* DaVinci Intermediate (DaVinci WG primaries + intermediate encoding) */
    ALWAN_RGB_SPACE_BLACKMAGIC_WIDE_GAMUT,
    ALWAN_RGB_SPACE_BLACKMAGIC_FILM,      /* Blackmagic Design Film (Film Generation 1-4) */
    ALWAN_RGB_SPACE_BLACKMAGIC_FILM_GEN5, /* Blackmagic Film Generation 5 */
    ALWAN_RGB_SPACE_V_GAMUT,
    ALWAN_RGB_SPACE_V_LOG,          /* V-Log (V-Gamut primaries + V-Log OETF) */
    ALWAN_RGB_SPACE_S_GAMUT,
    ALWAN_RGB_SPACE_S_GAMUT3,
    ALWAN_RGB_SPACE_S_GAMUT3_CINE,
    ALWAN_RGB_SPACE_CINEMA_GAMUT,
    ALWAN_RGB_SPACE_CANON_LOG,      /* Canon Log (Cinema Gamut primaries + Canon Log OETF) */
    ALWAN_RGB_SPACE_REDWIDEGAMUTRGB,
    ALWAN_RGB_SPACE_DCI_P3,
    ALWAN_RGB_SPACE_DCI_P3_P,           /* DCI-P3+ (extended primaries) */
    ALWAN_RGB_SPACE_P3_D65,

    /* Legacy spaces */
    ALWAN_RGB_SPACE_NTSC_1953,
    ALWAN_RGB_SPACE_NTSC_1987,
    ALWAN_RGB_SPACE_PAL_SECAM,
    ALWAN_RGB_SPACE_EBU_TECH_3213_E,    /* EBU Tech. 3213-E (European Broadcasting Union) */
    ALWAN_RGB_SPACE_APPLE_RGB,
    ALWAN_RGB_SPACE_COLORMATCH_RGB,

    /* Additional RGB spaces */
    ALWAN_RGB_SPACE_ALEXA_WIDE_GAMUT,       /* ARRI ALEXA Wide Gamut */
    ALWAN_RGB_SPACE_P3_D60,                  /* P3 with D60 white point */
    ALWAN_RGB_SPACE_XTREME_RGB,             /* Xtreme RGB (HP/Microsoft extended gamut) */
    ALWAN_RGB_SPACE_LINEAR_SRGB,            /* Linear sRGB (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_REC2020,         /* Linear Rec.2020 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_ADOBE_RGB_1998,  /* Linear Adobe RGB (1998) */
    ALWAN_RGB_SPACE_LINEAR_P3_D65,          /* Linear P3-D65 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_DISPLAY_P3,      /* Linear Display P3 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_PROPHOTO_RGB,    /* Linear ProPhoto RGB (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_DCI_P3,          /* Linear DCI-P3 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_ADOBE_WIDE_GAMUT_RGB,  /* Linear Adobe Wide Gamut RGB */
    ALWAN_RGB_SPACE_LINEAR_APPLE_RGB,       /* Linear Apple RGB (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_COLORMATCH_RGB,  /* Linear ColorMatch RGB (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_P3_D60,          /* Linear P3-D60 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_BT470_525,       /* Linear BT.470-525 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_BT470_625,       /* Linear BT.470-625 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_SMPTE_240M,      /* Linear SMPTE 240M (no transfer function) */

    /* Specialized/Standard spaces */
    ALWAN_RGB_SPACE_ITU_T_H273_22_UNSPECIFIED,  /* ITU-T H.273 code point 22 (Unspecified) */
    ALWAN_RGB_SPACE_ITU_T_H273_GENERIC_FILM,    /* ITU-T H.273 Generic Film */
    ALWAN_RGB_SPACE_PLASA_ANSI_E154,            /* PLASA ANSI E1.54 (Entertainment lighting standard) */

    /* Gamma-encoded variants (simple power-law gamma instead of complex transfer functions) */
    ALWAN_RGB_SPACE_GAMMA22_REC709,     /* Rec.709 primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA22_ADOBE_RGB,  /* Adobe RGB primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA22_P3_D65,     /* P3-D65 primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA22_AP1,        /* ACEScg (AP1) primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA18_REC709      /* Rec.709 primaries + gamma 1.8 OETF */
} alwan_rgb_space;

/* Transfer function identifiers (OETF/EOTF) */
typedef enum {
    ALWAN_TF_LINEAR = 0,   /* Linear / Identity (no transfer function) */
    ALWAN_TF_SRGB,
    ALWAN_TF_BT709,        /* Same as BT.2020 */
    ALWAN_TF_BT2020,       /* Same as BT.709 */
    ALWAN_TF_PQ,           /* Perceptual Quantizer (SMPTE ST 2084) */
    ALWAN_TF_ST2084,       /* Alias for PQ */
    ALWAN_TF_HLG,          /* Hybrid Log-Gamma (BT.2100) */
    ALWAN_TF_BT1886,       /* BT.1886 EOTF only */
    ALWAN_TF_ACESPROXY,    /* ACES Proxy */
    ALWAN_TF_ACESCC,       /* ACEScc (log encoding for color correction) */
    ALWAN_TF_ACESCCT,      /* ACEScct (log encoding with toe for grading) */

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

    /* Blackmagic Film */
    ALWAN_TF_BMDFILM,      /* Blackmagic Film Gen 5 */
    ALWAN_TF_BMDFILM4,     /* Blackmagic Film Gen 4 (Broadcast Film) */

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
    ALWAN_TF_NLOG,         /* Nikon N-Log */

    /* Film Log Encoding */
    ALWAN_TF_CINEON,       /* Cineon / DPX film log encoding */

    /* Apple Log (iPhone 15 Pro+) */
    ALWAN_TF_APPLE_LOG,    /* Apple Log (iPhone 15 Pro, BT.2020 primaries) */

    /* Fujifilm F-Log / F-Log2 */
    ALWAN_TF_FLOG,         /* Fujifilm F-Log */
    ALWAN_TF_FLOG2,        /* Fujifilm F-Log2 */

    /* Leica L-Log */
    ALWAN_TF_LLOG,         /* Leica L-Log */

    /* DJI D-Log */
    ALWAN_TF_DLOG,         /* DJI D-Log */

    /* Digital Cinema */
    ALWAN_TF_DCDM,         /* DCDM gamma 2.6 (SMPTE ST 428-1) */

    /* Academy Density Exchange (SMPTE ST 2065-3) */
    ALWAN_TF_ADX10,        /* ADX 10-bit (printing density to code value) */
    ALWAN_TF_ADX16,        /* ADX 16-bit (printing density to code value) */

    /* Game Engine Interop */
    ALWAN_TF_UNITY_LINEAR = ALWAN_TF_LINEAR  /* Unity linear (alias for ALWAN_TF_LINEAR) */
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

    /* LED illuminants */
    ALWAN_ILLUMINANT_LED_B1,    /* LED B1 (blue-pumped phosphor) */
    ALWAN_ILLUMINANT_LED_B2,    /* LED B2 */
    ALWAN_ILLUMINANT_LED_B3,    /* LED B3 */
    ALWAN_ILLUMINANT_LED_B4,    /* LED B4 */
    ALWAN_ILLUMINANT_LED_B5,    /* LED B5 */
    ALWAN_ILLUMINANT_LED_BH1,   /* LED BH1 (high CRI) */
    ALWAN_ILLUMINANT_LED_RGB1,  /* LED RGB1 (RGB LED mix) */
    ALWAN_ILLUMINANT_LED_V1,    /* LED V1 (violet-pumped) */
    ALWAN_ILLUMINANT_LED_V2,    /* LED V2 */

    /* High Pressure illuminants */
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
int alwan_data_get_illuminant_xy(alwan_scalar **data, size_t *count,
                                   alwan_ctx *ctx, alwan_illuminant illuminant);

/* View transform identifiers */
typedef enum {
    ALWAN_VIEW_ACES_REC709,  /* ACES RRT + ODT Rec.709 */
    ALWAN_VIEW_AGX,          /* AgX base */
    ALWAN_VIEW_AGX_PUNCHY    /* AgX punchy variant */
} alwan_view_transform;

/* RGB space descriptor with primaries, white point, and transfer functions */
typedef struct {
    alwan_scalar primaries_xy[6];  /* rx, ry, gx, gy, bx, by in CIE xy chromaticity */
    alwan_scalar white_xy[2];       /* wx, wy in CIE xy chromaticity */
    alwan_transfer_function oetf;   /* OETF (Opto-Electronic Transfer Function), use ALWAN_TF_LINEAR for none */
    alwan_transfer_function eotf;   /* EOTF (Electro-Optical Transfer Function), use ALWAN_TF_LINEAR for none */
} alwan_rgb_space_desc;

/* Derive RGB<->XYZ conversion matrices from primaries and white point
 * Returns ALWAN_OK on success, ALWAN_E_RANGE if primaries/white form singular matrix */
int alwan_rgb_derive_matrices(alwan_mat3x3 *rgb_to_xyz,
                               alwan_mat3x3 *xyz_to_rgb,
                               alwan_rgb_space_desc const *desc);

/* Convert linear RGB to XYZ for a given color space
 * space: RGB color space descriptor (primaries and white point)
 * rgb: input linear RGB color
 * xyz: output XYZ color
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_to_xyz(alwan_xyz *xyz,
                     alwan_rgb_space_desc const *space,
                     alwan_rgb const *rgb);

/* Convert XYZ to linear RGB for a given color space
 * space: RGB color space descriptor (primaries and white point)
 * xyz: input XYZ color
 * rgb: output linear RGB color (may be out of gamut)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_xyz_to_rgb(alwan_rgb *rgb,
                     alwan_rgb_space_desc const *space,
                     alwan_xyz const *xyz);

/* Get RGB color space descriptor by enum
 * Loads primaries and white point from data files and populates descriptor
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
int alwan_rgb_get_space_descriptor(alwan_rgb_space_desc *desc, alwan_ctx *ctx, alwan_rgb_space space);

/* ----------------------------------------------------------------
 * RGB Color Space Conversion
 * ---------------------------------------------------------------- */

/* Convert RGB color from one color space to another
 * Handles chromatic adaptation if white points differ (using Bradford CAT by default)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_convert(alwan_rgb *dst_rgb,
                      alwan_ctx *ctx,
                      alwan_rgb_space_desc const *src_space,
                      alwan_rgb_space_desc const *dst_space,
                      alwan_rgb const *src_rgb);

/* Bulk RGB color space conversion for arrays of colors
 * More efficient than calling alwan_rgb_convert in a loop
 * count: number of RGB triplets to convert
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_convert_bulk(alwan_rgb *dst_rgb,
                            alwan_ctx *ctx,
                            alwan_rgb_space_desc const *src_space,
                            alwan_rgb_space_desc const *dst_space,
                            alwan_rgb const *src_rgb,
                            size_t count);

/* ----------------------------------------------------------------
 * sRGB Convenience Functions
 * Direct conversions for sRGB (the most common color space)
 * All assume D65 white point and linear RGB (apply EOTF first if needed)
 * ---------------------------------------------------------------- */

/* sRGB <-> XYZ (D65 white point) */
int alwan_srgb_to_xyz(alwan_xyz *xyz, alwan_rgb const *rgb);
int alwan_xyz_to_srgb(alwan_rgb *rgb, alwan_xyz const *xyz);

/* sRGB <-> Lab (D65 white point) */
int alwan_srgb_to_lab(alwan_lab *lab, alwan_rgb const *rgb);
int alwan_lab_to_srgb(alwan_rgb *rgb, alwan_lab const *lab);

/* sRGB <-> Oklab (D65 assumed by Oklab) */
int alwan_srgb_to_oklab(alwan_oklab *oklab, alwan_rgb const *rgb);
int alwan_oklab_to_srgb(alwan_rgb *rgb, alwan_oklab const *oklab);

/* Bulk sRGB convenience conversions */
int alwan_srgb_to_xyz_bulk(alwan_scalar *xyz_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride);

int alwan_xyz_to_srgb_bulk(alwan_scalar *rgb_out,
                           alwan_scalar const *xyz_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride);

int alwan_srgb_to_lab_bulk(alwan_scalar *lab_out,
                           alwan_scalar const *rgb_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride);

int alwan_lab_to_srgb_bulk(alwan_scalar *rgb_out,
                           alwan_scalar const *lab_in,
                           size_t count,
                           size_t in_stride,
                           size_t out_stride);

int alwan_srgb_to_oklab_bulk(alwan_scalar *oklab_out,
                             alwan_scalar const *rgb_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride);

int alwan_oklab_to_srgb_bulk(alwan_scalar *rgb_out,
                             alwan_scalar const *oklab_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride);

/* ----------------------------------------------------------------
 * Direct RGB <-> Perceptual Space Conversions
 * Convenience functions that skip the manual XYZ intermediate step
 * ---------------------------------------------------------------- */

/* RGB <-> Lab (requires white point for Lab) */
int alwan_rgb_to_lab(alwan_lab *lab,
                     alwan_rgb_space_desc const *space,
                     alwan_rgb const *rgb,
                     alwan_xyz const *white_xyz);
int alwan_lab_to_rgb(alwan_rgb *rgb,
                     alwan_rgb_space_desc const *space,
                     alwan_lab const *lab,
                     alwan_xyz const *white_xyz);

/* RGB <-> Luv (requires white point for Luv) */
int alwan_rgb_to_luv(alwan_luv *luv,
                     alwan_rgb_space_desc const *space,
                     alwan_rgb const *rgb,
                     alwan_xyz const *white_xyz);
int alwan_luv_to_rgb(alwan_rgb *rgb,
                     alwan_rgb_space_desc const *space,
                     alwan_luv const *luv,
                     alwan_xyz const *white_xyz);

/* RGB <-> Oklab (Oklab assumes D65, handles chromatic adaptation if needed) */
int alwan_rgb_to_oklab(alwan_oklab *oklab, alwan_rgb_space_desc const *space, alwan_rgb const *rgb);
int alwan_oklab_to_rgb(alwan_rgb *rgb, alwan_rgb_space_desc const *space, alwan_oklab const *oklab);

/* RGB <-> Oklch (cylindrical Oklab) */
int alwan_rgb_to_oklch(alwan_oklch *oklch, alwan_rgb_space_desc const *space, alwan_rgb const *rgb);
int alwan_oklch_to_rgb(alwan_rgb *rgb, alwan_rgb_space_desc const *space, alwan_oklch const *oklch);

/* ----------------------------------------------------------------
 * Direct XYZ <-> Cylindrical Conversions
 * Skip the cartesian intermediate step
 * ---------------------------------------------------------------- */

/* XYZ <-> LCh(ab) (cylindrical Lab) */
void alwan_xyz_to_lch(alwan_lch *lch, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
void alwan_lch_to_xyz(alwan_xyz *xyz, alwan_lch const *lch, alwan_xyz const *white_xyz);

/* XYZ <-> LCh(uv) (cylindrical Luv) */
void alwan_xyz_to_lchuv(alwan_lchuv *lchuv, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
void alwan_lchuv_to_xyz(alwan_xyz *xyz, alwan_lchuv const *lchuv, alwan_xyz const *white_xyz);

/* XYZ <-> Oklch (cylindrical Oklab, D65 assumed) */
void alwan_xyz_to_oklch(alwan_oklch *oklch, alwan_xyz const *xyz);
void alwan_oklch_to_xyz(alwan_xyz *xyz, alwan_oklch const *oklch);

/* ----------------------------------------------------------------
 * M11: Gamut Utilities & Mapping
 * ---------------------------------------------------------------- */

/* Gamut mapping method */
typedef enum {
    ALWAN_GAMUT_MAP_CLIP = 0,         /* Simple clipping to [0,1] */
    ALWAN_GAMUT_MAP_HUE_PRESERVING = 1, /* Project to gamut boundary preserving hue */
    ALWAN_GAMUT_MAP_ADAPTIVE_L0,     /* Adaptive L0 (project toward L=0.5) - P9.5 */
    ALWAN_GAMUT_MAP_ADAPTIVE_CUSP,   /* Adaptive toward cusp (hue-dependent) - P9.5 */
    ALWAN_GAMUT_MAP_CHROMA_COMPRESS, /* Chroma compression - P9.5 */
    ALWAN_GAMUT_MAP_SGCK,            /* SGCK 2004 (Segment-Maximal Gamut Clipping with Knee) - P9.5 */
    ALWAN_GAMUT_MAP_HPMINDE,         /* HPMINDE (Hue-Preserving Minimum ΔE) - P9.5 */
    ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE /* Lightness Preserving - P9.5 */
} alwan_gamut_map_method;

/* Estimate RGB gamut volume using Monte Carlo sampling
 * space: RGB color space descriptor
 * num_samples: number of random samples (e.g., 1000000)
 * seed: random seed for reproducibility
 * volume: output volume estimate (in XYZ units cubed)
 * Returns ALWAN_OK on success */
int alwan_gamut_volume_mc(alwan_scalar *volume,
                          alwan_rgb_space_desc const *space,
                          size_t num_samples,
                          unsigned int seed);

/* Map RGB colors to [0,1] gamut using specified method
 * Bulk operation with stride support for efficient array processing
 * rgb_out: output RGB colors (stride out_stride between consecutive triplets)
 * method: gamut mapping method (clip, hue-preserving)
 * rgb_in: input RGB colors (may be out of gamut, stride in_stride between triplets)
 * count: number of RGB triplets to process
 * in_stride: stride for input (in bytes, typically 3*sizeof(alwan_scalar) for packed)
 * out_stride: stride for output (in bytes, typically 3*sizeof(alwan_scalar) for packed)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if method not supported */
int alwan_gamut_map(alwan_scalar *rgb_out,
                    alwan_gamut_map_method method,
                    alwan_scalar const *rgb_in,
                    size_t count,
                    size_t in_stride,
                    size_t out_stride);

/* Map XYZ color to RGB gamut with hue preservation
 * ctx: optional context (can be NULL)
 * space: target RGB space
 * xyz_in: input XYZ color (may be out of RGB gamut)
 * rgb_out: output RGB color (mapped to [0,1] with preserved hue in JCh)
 * Returns ALWAN_OK on success */
int alwan_gamut_map_xyz_to_rgb(alwan_rgb *rgb_out,
                                alwan_ctx *ctx,
                                alwan_rgb_space_desc const *space,
                                alwan_xyz const *xyz_in);

/* ----------------------------------------------------------------
 * Transfer Functions (OETF/EOTF)
 * ---------------------------------------------------------------- */

/* Apply Opto-Electronic Transfer Function (linear -> encoded)
 * tf: transfer function to apply
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
int alwan_oetf_apply(alwan_scalar *encoded,
                     alwan_transfer_function tf,
                     alwan_scalar const *linear,
                     size_t count, size_t in_stride, size_t out_stride);

/* Apply Electro-Optical Transfer Function (encoded -> linear)
 * tf: transfer function to apply
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
int alwan_eotf_apply(alwan_scalar *linear,
                     alwan_transfer_function tf,
                     alwan_scalar const *encoded,
                     size_t count, size_t in_stride, size_t out_stride);

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
 * in_stride: stride between input RGB triplets (in bytes, typically 3*sizeof(alwan_scalar))
 * rgb_out: output RGB triplets (display-referred)
 * out_stride: stride between output RGB triplets (in bytes, typically 3*sizeof(alwan_scalar))
 *
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if transform not supported */
int alwan_view_transform_apply(alwan_scalar *rgb_out,
                                alwan_ctx *ctx,
                                alwan_view_transform vt,
                                alwan_scalar const *rgb_in,
                                size_t count, size_t in_stride, size_t out_stride);

/* ----------------------------------------------------------------
 * Color Space Conversions
 * ---------------------------------------------------------------- */

/* XYZ <-> xyY conversions */
void alwan_xyz_to_xyy(alwan_xyy *xyy, alwan_xyz const *xyz);
void alwan_xyy_to_xyz(alwan_xyz *xyz, alwan_xyy const *xyy);

/* XYZ <-> Lab conversions (requires white point in XYZ) */
void alwan_xyz_to_lab(alwan_lab *lab, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
void alwan_lab_to_xyz(alwan_xyz *xyz, alwan_lab const *lab, alwan_xyz const *white_xyz);

/* XYZ <-> Luv conversions (requires white point in XYZ) */
void alwan_xyz_to_luv(alwan_luv *luv, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
void alwan_luv_to_xyz(alwan_xyz *xyz, alwan_luv const *luv, alwan_xyz const *white_xyz);

/* XYZ <-> U*V*W* conversions (CIE 1964 uniform color space for CRI)
 * Based on CIE 1960 UCS chromaticity diagram
 * Used specifically for Color Rendering Index calculations
 * Requires white point in XYZ */
void alwan_xyz_to_uvw(alwan_uvw *uvw, alwan_xyz const *xyz, alwan_xyz const *white_xyz);
void alwan_uvw_to_xyz(alwan_xyz *xyz, alwan_uvw const *uvw, alwan_xyz const *white_xyz);

/* Lab <-> LCh(ab) conversions */
void alwan_lab_to_lch(alwan_lch *lch, alwan_lab const *lab);
void alwan_lch_to_lab(alwan_lab *lab, alwan_lch const *lch);

/* Luv <-> LCh(uv) conversions */
void alwan_luv_to_lchuv(alwan_lchuv *lchuv, alwan_luv const *luv);
void alwan_lchuv_to_luv(alwan_luv *luv, alwan_lchuv const *lchuv);

/* ----------------------------------------------------------------
 * Bulk Color Space Conversions (with stride support)
 * ---------------------------------------------------------------- */

/* Bulk XYZ <-> Lab conversions
 * count: number of color triplets to convert
 * in_stride: input stride in bytes (typically 3*sizeof(alwan_scalar))
 * out_stride: output stride in bytes (typically 3*sizeof(alwan_scalar)) */
int alwan_xyz_to_lab_bulk(alwan_scalar *lab_out,
                          alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_lab_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *lab_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

/* Bulk XYZ <-> Luv conversions */
int alwan_xyz_to_luv_bulk(alwan_scalar *luv_out,
                          alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_luv_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *luv_in,
                          alwan_xyz const *white_xyz,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

/* Bulk Lab <-> LCh conversions */
int alwan_lab_to_lch_bulk(alwan_scalar *lch_out,
                          alwan_scalar const *lab_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_lch_to_lab_bulk(alwan_scalar *lab_out,
                          alwan_scalar const *lch_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

/* Bulk Luv <-> LCh(uv) conversions */
int alwan_luv_to_lchuv_bulk(alwan_scalar *lchuv_out,
                            alwan_scalar const *luv_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

int alwan_lchuv_to_luv_bulk(alwan_scalar *luv_out,
                            alwan_scalar const *lchuv_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

/* Bulk XYZ <-> xyY conversions */
int alwan_xyz_to_xyy_bulk(alwan_scalar *xyy_out,
                          alwan_scalar const *xyz_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_xyy_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *xyy_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

/* XYZ <-> Oklab conversions (modern perceptually uniform space, D65 assumed) */
void alwan_xyz_to_oklab(alwan_oklab *oklab, alwan_xyz const *xyz);
void alwan_oklab_to_xyz(alwan_xyz *xyz, alwan_oklab const *oklab);

/* Oklab <-> Oklch conversions (cylindrical Oklab) */
void alwan_oklab_to_oklch(alwan_oklch *oklch, alwan_oklab const *oklab);
void alwan_oklch_to_oklab(alwan_oklab *oklab, alwan_oklch const *oklch);

/* Bulk XYZ <-> Oklab conversions */
int alwan_xyz_to_oklab_bulk(alwan_scalar *oklab_out,
                            alwan_scalar const *xyz_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

int alwan_oklab_to_xyz_bulk(alwan_scalar *xyz_out,
                            alwan_scalar const *oklab_in,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

/* Bulk Oklab <-> Oklch conversions */
int alwan_oklab_to_oklch_bulk(alwan_scalar *oklch_out,
                              alwan_scalar const *oklab_in,
                              size_t count,
                              size_t in_stride,
                              size_t out_stride);

int alwan_oklch_to_oklab_bulk(alwan_scalar *oklab_out,
                              alwan_scalar const *oklch_in,
                              size_t count,
                              size_t in_stride,
                              size_t out_stride);

/* Lab <-> DIN99 conversions (DIN99 Family - German color difference standards)
 * - variant: 0 = DIN99/ASTM, 1 = DIN99b, 2 = DIN99c, 3 = DIN99d
 * - All variants provide improved perceptual uniformity over CIE Lab
 * - Input Lab should be D65 adapted
 */
void alwan_lab_to_din99(alwan_din99 *din99, alwan_lab const *lab, int variant);
void alwan_din99_to_lab(alwan_lab *lab, alwan_din99 const *din99, int variant);

/* RGB <-> ICtCp conversions (ITU-R BT.2100 HDR color space)
 * - RGB input/output is linear BT.2020 RGB
 * - use_pq: 1 for PQ (Perceptual Quantizer), 0 for HLG (Hybrid Log-Gamma)
 */
void alwan_rgb_to_ictcp(alwan_ictcp *ictcp, alwan_rgb const *rgb, int use_pq);
void alwan_ictcp_to_rgb(alwan_rgb *rgb, alwan_ictcp const *ictcp, int use_pq);

/* XYZ <-> ICtCp conversions (via BT.2020 RGB)
 * - XYZ is assumed to be D65 adapted
 * - use_pq: 1 for PQ (Perceptual Quantizer), 0 for HLG (Hybrid Log-Gamma)
 */
void alwan_xyz_to_ictcp(alwan_ictcp *ictcp, alwan_xyz const *xyz, int use_pq);
void alwan_ictcp_to_xyz(alwan_xyz *xyz, alwan_ictcp const *ictcp, int use_pq);

/* Bulk RGB <-> ICtCp conversions */
int alwan_rgb_to_ictcp_bulk(alwan_scalar *ictcp_out,
                            alwan_scalar const *rgb_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

int alwan_ictcp_to_rgb_bulk(alwan_scalar *rgb_out,
                            alwan_scalar const *ictcp_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

/* Bulk XYZ <-> ICtCp conversions */
int alwan_xyz_to_ictcp_bulk(alwan_scalar *ictcp_out,
                            alwan_scalar const *xyz_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

int alwan_ictcp_to_xyz_bulk(alwan_scalar *xyz_out,
                            alwan_scalar const *ictcp_in,
                            int use_pq,
                            size_t count,
                            size_t in_stride,
                            size_t out_stride);

/* Jzazbz <-> XYZ conversions (Perceptually uniform HDR color space)
 * - XYZ input/output is D65 adapted
 * - Jzazbz: Jz (lightness), az (red-green), bz (yellow-blue)
 */
void alwan_xyz_to_jzazbz(alwan_jzazbz *jzazbz, alwan_xyz const *xyz);
void alwan_jzazbz_to_xyz(alwan_xyz *xyz, alwan_jzazbz const *jzazbz);

/* Jzazbz <-> JzCzhz conversions (cylindrical coordinates)
 * - JzCzhz: Jz (lightness), Cz (chroma), hz (hue in radians)
 */
void alwan_jzazbz_to_jzczhz(alwan_jzczhz *jzczhz, alwan_jzazbz const *jzazbz);
void alwan_jzczhz_to_jzazbz(alwan_jzazbz *jzazbz, alwan_jzczhz const *jzczhz);

/* Bulk XYZ <-> JzAzBz conversions */
int alwan_xyz_to_jzazbz_bulk(alwan_scalar *jzazbz_out,
                             alwan_scalar const *xyz_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride);

int alwan_jzazbz_to_xyz_bulk(alwan_scalar *xyz_out,
                             alwan_scalar const *jzazbz_in,
                             size_t count,
                             size_t in_stride,
                             size_t out_stride);

/* Bulk JzAzBz <-> JzCzhz conversions */
int alwan_jzazbz_to_jzczhz_bulk(alwan_scalar *jzczhz_out,
                                alwan_scalar const *jzazbz_in,
                                size_t count,
                                size_t in_stride,
                                size_t out_stride);

int alwan_jzczhz_to_jzazbz_bulk(alwan_scalar *jzazbz_out,
                                alwan_scalar const *jzczhz_in,
                                size_t count,
                                size_t in_stride,
                                size_t out_stride);

/* Hunter Lab <-> XYZ conversions (Earlier Lab-type color space)
 * - XYZ input/output is D65 adapted by default
 * - Hunter Lab: L (lightness), a (red-green), b (yellow-blue)
 * - Uses square roots instead of cube roots (unlike CIE Lab)
 * - Ka and Kb coefficients are illuminant-dependent
 */
void alwan_xyz_to_hunter_lab(alwan_hunter_lab *hunter_lab, alwan_xyz const *xyz);
void alwan_hunter_lab_to_xyz(alwan_xyz *xyz, alwan_hunter_lab const *hunter_lab);

/* Hunter Lab <-> XYZ conversions with custom illuminant
 * - xyz_n: Reference white point (e.g., D50, D65, or custom illuminant)
 * - Ka and Kb are calculated automatically based on xyz_n
 */
void alwan_xyz_to_hunter_lab_custom(alwan_hunter_lab *hunter_lab, alwan_xyz const *xyz, alwan_xyz const *xyz_n);
void alwan_hunter_lab_to_xyz_custom(alwan_xyz *xyz, alwan_hunter_lab const *hunter_lab, alwan_xyz const *xyz_n);

/* IPT <-> XYZ conversions (Image Processing Transform)
 * - XYZ input/output is D65 adapted
 * - IPT: I (intensity/lightness), P (red-green), T (yellow-blue)
 * - Improved hue uniformity over CIELAB
 * - Uses power function nonlinearity (exponent 0.43)
 */
void alwan_xyz_to_ipt(alwan_ipt *ipt, alwan_xyz const *xyz);
void alwan_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *ipt);

/* IPT <-> IPTch conversions (cylindrical coordinates)
 * - IPTch: I (intensity), C (chroma), h (hue in radians)
 */
void alwan_ipt_to_iptch(alwan_iptch *iptch, alwan_ipt const *ipt);
void alwan_iptch_to_ipt(alwan_ipt *ipt, alwan_iptch const *iptch);

/* Bulk XYZ <-> IPT conversions */
int alwan_xyz_to_ipt_bulk(alwan_scalar *ipt_out,
                          alwan_scalar const *xyz_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_ipt_to_xyz_bulk(alwan_scalar *xyz_out,
                          alwan_scalar const *ipt_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

/* ProLab <-> XYZ conversions (Perceptually Uniform Projective)
 * - XYZ input/output is D65 adapted by default
 * - ProLab: Uses projective transformation for improved uniformity
 * - Based on Konovalenko et al. (2021)
 */
void alwan_xyz_to_prolab(alwan_prolab *prolab, alwan_xyz const *xyz);
void alwan_prolab_to_xyz(alwan_xyz *xyz, alwan_prolab const *prolab);

/* ProLab <-> XYZ conversions with custom illuminant
 * - xyz_n: Reference white point (e.g., D50, D65, or custom illuminant)
 */
void alwan_xyz_to_prolab_custom(alwan_prolab *prolab, alwan_xyz const *xyz, alwan_xyz const *xyz_n);
void alwan_prolab_to_xyz_custom(alwan_xyz *xyz, alwan_prolab const *prolab, alwan_xyz const *xyz_n);

/* OSA-UCS <-> XYZ conversions (Optical Society of America Uniform Color Scales)
 * - XYZ input/output is D65 adapted
 * - OSA-UCS: L (lightness), j (yellowness), g (greenness)
 * - Forward transform is exact, inverse is approximate (iterative solution)
 * - Note: Inverse transformation has lower precision than other color spaces
 */
void alwan_xyz_to_osa_ucs(alwan_osa_ucs *osa_ucs, alwan_xyz const *xyz);
void alwan_osa_ucs_to_xyz(alwan_xyz *xyz, alwan_osa_ucs const *osa_ucs);

/* CIE 1960 UCS <-> XYZ conversions (Uniform Chromaticity Scale)
 * - CIE 1960 UCS: u, v chromaticity coordinates + Y luminance
 * - Used for CCT calculations and color rendering metrics
 * - Precursor to CIE 1976 u'v' (CIELUV) chromaticity diagram
 */
void alwan_xyz_to_ucs(alwan_ucs *ucs, alwan_xyz const *xyz);
void alwan_ucs_to_xyz(alwan_xyz *xyz, alwan_ucs const *ucs);

/* hdr-CIELAB <-> XYZ conversions (HDR extension of CIELAB)
 * - Fairchild & Wyble (2010) HDR-CIELAB model
 * - Designed for high dynamic range imagery (Y > 100)
 * - Maintains perceptual uniformity across extended luminance range
 * - XYZ input/output is D65 adapted, Y can exceed 100
 */
void alwan_xyz_to_hdr_cielab(alwan_lab *hdr_lab, alwan_xyz const *xyz);
void alwan_hdr_cielab_to_xyz(alwan_xyz *xyz, alwan_lab const *hdr_lab);

/* hdr-IPT <-> XYZ conversions (HDR extension of IPT)
 * - Fairchild (2010) HDR-IPT model
 * - Extends IPT to high dynamic range
 * - Better hue preservation than hdr-CIELAB for HDR content
 * - XYZ input/output is D65 adapted, supports extended luminance
 */
void alwan_xyz_to_hdr_ipt(alwan_ipt *hdr_ipt, alwan_xyz const *xyz);
void alwan_hdr_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *hdr_ipt);

/* IgPgTg <-> XYZ conversions (Improved IPT variant)
 * - Ebner & Fairchild (1998) improved hue uniformity
 * - Better than IPT for certain hue angles
 * - XYZ input/output is D65 adapted
 */
void alwan_xyz_to_igpgtg(alwan_igpgtg *igpgtg, alwan_xyz const *xyz);
void alwan_igpgtg_to_xyz(alwan_xyz *xyz, alwan_igpgtg const *igpgtg);

/* ICaCb <-> XYZ conversions (Image Difference Color Space)
 * - Zhang & Wandell (1996, 1997)
 * - Optimized for image difference metrics
 * - XYZ input/output is D65 adapted
 */
void alwan_xyz_to_icacb(alwan_icacb *icacb, alwan_xyz const *xyz);
void alwan_icacb_to_xyz(alwan_xyz *xyz, alwan_icacb const *icacb);

/* Prismatic <-> RGB conversions (Pridmore 2021)
 * - Perceptually uniform cylindrical color space
 * - P (purity), r (red-green), i (intensity)
 * - RGB input/output in [0, 1] range
 */
void alwan_rgb_to_prismatic(alwan_prismatic *prismatic, alwan_rgb const *rgb);
void alwan_prismatic_to_rgb(alwan_rgb *rgb, alwan_prismatic const *prismatic);

/* HCL <-> RGB conversions (Sarifuddin 2005)
 * - Hue-Chroma-Luminance polar coordinate system
 * - Better perceptual properties than HSL
 * - RGB input/output in [0, 1] range
 */
void alwan_rgb_to_hcl(alwan_hcl *hcl, alwan_rgb const *rgb);
void alwan_hcl_to_rgb(alwan_rgb *rgb, alwan_hcl const *hcl);

/* IHLS <-> RGB conversions (Improved HLS - Hanbury 2003)
 * - Improved Hue-Lightness-Saturation
 * - Better perceptual properties than standard HSL
 * - RGB input/output in [0, 1] range
 */
void alwan_rgb_to_ihls(alwan_ihls *ihls, alwan_rgb const *rgb);
void alwan_ihls_to_rgb(alwan_rgb *rgb, alwan_ihls const *ihls);

/* ----------------------------------------------------------------
 * Color Difference (ΔE) Metrics
 * ---------------------------------------------------------------- */

/* ΔE*76 - Euclidean distance in Lab space */
alwan_scalar alwan_delta_e_76(alwan_lab const *lab1, alwan_lab const *lab2);

/* ΔE*94 - CIE 1994 color difference (graphic arts defaults: kL=1, K1=0.045, K2=0.015) */
alwan_scalar alwan_delta_e_94(alwan_lab const *lab1, alwan_lab const *lab2);

/* ΔE CMC(l:c) - CMC color difference (defaults: l=2, c=1 for acceptability) */
alwan_scalar alwan_delta_e_cmc(alwan_lab const *lab1, alwan_lab const *lab2, alwan_scalar l, alwan_scalar c);

/* ΔE*00 - CIEDE2000 color difference (most perceptually uniform) */
alwan_scalar alwan_delta_e_2000(alwan_lab const *lab1, alwan_lab const *lab2);

/* ΔE ITP - ITU-R BT.2124 HDR color difference in ICtCp space (scalar_factor default: 720) */
alwan_scalar alwan_delta_e_itp(alwan_ictcp const *ictcp1, alwan_ictcp const *ictcp2, alwan_scalar scalar_factor);

/* ΔE HyAB - Hybrid Delta E, improved perceptual metric */
alwan_scalar alwan_delta_e_hyab(alwan_lab const *lab1, alwan_lab const *lab2);

/* ΔE DIN99 - Euclidean distance in DIN99 space (variant: 0=DIN99, 1=b, 2=c, 3=d) */
alwan_scalar alwan_delta_e_din99(alwan_din99 const *din99_1, alwan_din99 const *din99_2);

/* ΔE CAM02-LCD - CIECAM02 Large Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam02_lcd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2);

/* ΔE CAM02-SCD - CIECAM02 Small Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam02_scd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2);

/* ΔE CAM16-LCD - CAM16 Large Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam16_lcd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2);

/* ΔE CAM16-SCD - CAM16 Small Color Difference in UCS space */
alwan_scalar alwan_delta_e_cam16_scd(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2);

/* ΔE CAM02-UCS - CIECAM02 Uniform Color Space (Luo et al. 2006)
 * Uses K_L=1.0, c1=0.007, c2=0.0228 for general-purpose color difference */
alwan_scalar alwan_delta_e_cam02_ucs(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2);

/* ΔE CAM16-UCS - CAM16 Uniform Color Space (Li et al. 2017)
 * Uses K_L=1.0, c1=0.007, c2=0.0228 for general-purpose color difference */
alwan_scalar alwan_delta_e_cam16_ucs(alwan_cam_jab const *jab1, alwan_cam_jab const *jab2);

/* ΔE ZCAM - Euclidean distance in ZCAM UCS (Jzazbz) space */
alwan_scalar alwan_delta_e_zcam(alwan_jzazbz const *jab1, alwan_jzazbz const *jab2);

/* ----------------------------------------------------------------
 * Batch Color Difference (ΔE) Computations
 * Compare arrays of colors efficiently
 * ---------------------------------------------------------------- */

/* Batch ΔE*76 - Euclidean distance in Lab space
 * delta_e_out: output ΔE values (count elements)
 * lab1_in: first array of Lab colors (stride in1_stride between colors)
 * lab2_in: second array of Lab colors (stride in2_stride between colors)
 * count: number of color pairs to compare
 * Returns ALWAN_OK on success */
int alwan_delta_e_76_batch(alwan_scalar *delta_e_out,
                           alwan_scalar const *lab1_in,
                           alwan_scalar const *lab2_in,
                           size_t count,
                           size_t in1_stride,
                           size_t in2_stride);

/* Batch ΔE*00 - CIEDE2000 color difference */
int alwan_delta_e_2000_batch(alwan_scalar *delta_e_out,
                             alwan_scalar const *lab1_in,
                             alwan_scalar const *lab2_in,
                             size_t count,
                             size_t in1_stride,
                             size_t in2_stride);

/* Batch ΔE*94 - CIE 1994 color difference */
int alwan_delta_e_94_batch(alwan_scalar *delta_e_out,
                           alwan_scalar const *lab1_in,
                           alwan_scalar const *lab2_in,
                           size_t count,
                           size_t in1_stride,
                           size_t in2_stride);

/* Batch ΔE CMC(l:c) - CMC color difference */
int alwan_delta_e_cmc_batch(alwan_scalar *delta_e_out,
                            alwan_scalar const *lab1_in,
                            alwan_scalar const *lab2_in,
                            alwan_scalar l,
                            alwan_scalar c,
                            size_t count,
                            size_t in1_stride,
                            size_t in2_stride);

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
alwan_scalar alwan_yellowness_astm_e313(alwan_xyz const *xyz, alwan_astm_e313_illuminant illuminant);

/* ASTM E313 Whiteness Index
 * xyz: CIE XYZ tristimulus values (normalized to Y=100 for perfect white)
 * illuminant: illuminant/observer pair (C/2°, D65/2°, C/10°, or D65/10°)
 * Returns: Whiteness Index (WI) value */
alwan_scalar alwan_whiteness_astm_e313(alwan_xyz const *xyz, alwan_astm_e313_illuminant illuminant);

/* CIE 2004 Whiteness Index
 * xy: CIE 1931 chromaticity coordinates (x, y)
 * Y: CIE Y tristimulus value (luminance factor)
 * xy_n: reference white chromaticity coordinates
 * Returns: CIE Whiteness (W) value
 * Note: Also computes Tint (T), but this function only returns W.
 *       Tint = 900(xn - x) - 650(yn - y) for 2° observer
 *       Tint = 1000(xn - x) - 650(yn - y) for 10° observer */
alwan_scalar alwan_whiteness_cie2004(alwan_vec2 const *xy, alwan_scalar Y, alwan_vec2 const *xy_n);

/* ----------------------------------------------------------------
 * Chromatic Adaptation Transform (CAT)
 * ---------------------------------------------------------------- */

/* Chromatic Adaptation Transform (CAT) method */
typedef enum {
    ALWAN_CAT_XYZ_SCALING = 0,  /* Von Kries in XYZ space (simplest) */
    ALWAN_CAT_BRADFORD    = 1,  /* Bradford (most common, used in ICC) */
    ALWAN_CAT_CAT02       = 2,  /* CAT02 (from CIECAM02) */
    ALWAN_CAT_CAT16       = 3,  /* CAT16 (from CAM16) */

    /* Extended CAT methods */
    ALWAN_CAT_SHARP           = 4,  /* Sharp transform */
    ALWAN_CAT_FAIRCHILD       = 5,  /* Fairchild 1990 */
    ALWAN_CAT_CMCCAT97        = 6,  /* CMC CAT97 */
    ALWAN_CAT_CMCCAT2000      = 7,  /* CMC CAT2000 */
    ALWAN_CAT_CAT02_BRILL_2008 = 8, /* CAT02 Brill 2008 variant */
    ALWAN_CAT_BIANCO_2010     = 9,  /* Bianco 2010 */
    ALWAN_CAT_BIANCO_PC_2010  = 10, /* Bianco PC 2010 */

    /* Two-step CAT methods */
    ALWAN_CAT_ZHAI_2018       = 11  /* Zhai & Luo 2018 two-step CAT */
} alwan_cat_method;

/* Compute chromatic adaptation matrix from source to destination white point
 * src_white_xyz: source white point in XYZ (normalized to Y=1)
 * dst_white_xyz: destination white point in XYZ (normalized to Y=1)
 * method: CAT method (Bradford, CAT02, CAT16, or XYZ scaling)
 * out: output 3x3 adaptation matrix
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if white points are invalid */
int alwan_cat_matrix(alwan_mat3x3 *out,
                     alwan_xyz const *src_white_xyz,
                     alwan_xyz const *dst_white_xyz,
                     alwan_cat_method method);

/* Apply chromatic adaptation to XYZ colors (bulk operation)
 * xyz_in: input XYZ colors (stride in_stride between consecutive colors)
 * count: number of colors to transform
 * in_stride: stride for input (in bytes, typically 3*sizeof(alwan_scalar) for packed array)
 * src_white_xyz: source white point in XYZ
 * dst_white_xyz: destination white point in XYZ
 * method: CAT method
 * xyz_out: output XYZ colors (stride out_stride between consecutive colors)
 * out_stride: stride for output (in bytes, typically 3*sizeof(alwan_scalar) for packed array)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if parameters are invalid */
int alwan_xyz_adapt(alwan_scalar *xyz_out,
                    alwan_xyz const *src_white_xyz,
                    alwan_xyz const *dst_white_xyz,
                    alwan_cat_method method,
                    alwan_scalar const *xyz_in,
                    size_t count, size_t in_stride, size_t out_stride);

/* Zhai & Luo 2018 two-step chromatic adaptation
 * Adapts XYZ from input illuminant to output illuminant via baseline illuminant
 * xyz_in: input XYZ color under source illuminant (Y=100 scale)
 * xyz_src: source illuminant XYZ (Y=100 scale)
 * xyz_dst: destination illuminant XYZ (Y=100 scale)
 * D_src: degree of adaptation for source illuminant [0,1] (1=full adaptation)
 * D_dst: degree of adaptation for destination illuminant [0,1] (1=full adaptation)
 * xyz_baseline: baseline illuminant XYZ (NULL for equal-energy white [100,100,100])
 * transform: underlying CAT transform (ALWAN_CAT_CAT02 or ALWAN_CAT_CAT16)
 * xyz_out: output adapted XYZ color
 * Returns ALWAN_OK on success */
int alwan_cat_zhai2018(alwan_xyz *xyz_out,
                       alwan_xyz const *xyz_in,
                       alwan_xyz const *xyz_src,
                       alwan_xyz const *xyz_dst,
                       alwan_scalar D_src,
                       alwan_scalar D_dst,
                       alwan_xyz const *xyz_baseline,
                       alwan_cat_method transform);

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

typedef enum {
    ALWAN_OBSERVER_CIE_1931_2DEG = 0,  /* CIE 1931 2° standard observer */
    ALWAN_OBSERVER_CIE_1964_10DEG = 1, /* CIE 1964 10° standard observer */
    ALWAN_OBSERVER_CIE_2012_2DEG = 2,  /* CIE 2012 2° standard observer (physiologically-based) */
    ALWAN_OBSERVER_CIE_2012_10DEG = 3, /* CIE 2012 10° standard observer (physiologically-based) */

    /* Extended observers */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG = 4,  /* Stockman & Sharpe 2000 2° cone fundamentals */
    ALWAN_OBSERVER_CIE_2015_2DEG = 5,         /* CIE 2015 2° cone-fundamental-based observer */
    ALWAN_OBSERVER_CIE_2015_10DEG = 6,        /* CIE 2015 10° cone-fundamental-based observer */
    ALWAN_OBSERVER_WRIGHT_GUILD_1931 = 7      /* Wright & Guild 1931 2° RGB CMFs (historical) */
} alwan_observer_type;

/* Camera/Sensor spectral sensitivity identifiers */
typedef enum {
    ALWAN_CAMERA_NIKON_5100,        /* Nikon D5100 (NPL measured) */
    ALWAN_CAMERA_SIGMA_SDMERILL     /* Sigma SD Merill (NPL measured) */
} alwan_camera_sensitivity;

/* Spectral shape descriptor
 * Compact representation of SPD characteristics */
typedef struct {
    alwan_scalar peak_wavelength;  /* Peak wavelength (nm) */
    alwan_scalar peak_value;        /* Peak power/reflectance value */
    alwan_scalar fwhm;              /* Full width at half maximum (nm) */
    alwan_scalar centroid;          /* Weighted mean wavelength (nm) */
    alwan_scalar bandwidth;         /* Total wavelength range (nm) */
} alwan_spd_shape;

/* Get white point XYZ for a standard illuminant
 * Computes XYZ tristimulus values from illuminant xy chromaticity (Y normalized to 1.0)
 * Returns ALWAN_E_INVALID if illuminant not supported */
int alwan_illuminant_white_point(alwan_xyz *out_xyz,
                                   alwan_illuminant illuminant,
                                   alwan_observer_type observer);

/* SPD resampling method */
typedef enum {
    ALWAN_RESAMPLE_LINEAR = 0,      /* Linear interpolation */
    ALWAN_RESAMPLE_CATMULL_ROM = 1  /* Catmull-Rom spline (smoother) */
} alwan_resample_method;

/* SPD extrapolation mode (for values outside measured range) */
typedef enum {
    ALWAN_EXTRAPOLATE_ZERO = 0,     /* Clamp to zero outside range (default for reflectance) */
    ALWAN_EXTRAPOLATE_CONSTANT = 1, /* Repeat edge values (good for smooth SPDs) */
    ALWAN_EXTRAPOLATE_LINEAR = 2    /* Linear extrapolation from edge slope */
} alwan_extrapolate_mode;

/* SPD integration method for computing XYZ */
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
int alwan_spd_create(alwan_spd *out,
                     alwan_ctx *ctx,
                     alwan_scalar wavelength_min,
                     alwan_scalar wavelength_max,
                     size_t count);

/* Destroy SPD and free allocated values */
void alwan_spd_destroy(alwan_ctx *ctx, alwan_spd *spd);

/* Load standard illuminant SPD
 * ctx: context (for data path and allocation)
 * ill: illuminant to load
 * out: output SPD structure
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if illuminant not supported */
int alwan_spd_illuminant(alwan_spd *out, alwan_ctx *ctx, alwan_illuminant ill);

/* Generate blackbody (Planckian) SPD at given temperature
 * Uses Planck's law to compute spectral radiance
 * ctx: context
 * temperature_K: color temperature in Kelvin (typically 1000-25000K)
 * wavelength_min: starting wavelength (nm)
 * wavelength_max: ending wavelength (nm)
 * count: number of samples
 * out: output SPD structure (values allocated internally)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if temperature out of range */
int alwan_spd_blackbody(alwan_spd *out,
                        alwan_ctx *ctx,
                        alwan_scalar temperature_K,
                        alwan_scalar wavelength_min,
                        alwan_scalar wavelength_max,
                        size_t count);

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
int alwan_spd_resample(alwan_spd *dst,
                       alwan_ctx *ctx,
                       alwan_spd const *src,
                       alwan_scalar wavelength_min,
                       alwan_scalar wavelength_max,
                       size_t count,
                       alwan_resample_method method,
                       alwan_extrapolate_mode extrapolate);

/* Compute XYZ tristimulus values from SPD
 * ctx: context
 * spd: spectral power distribution (reflectance or emission)
 * illuminant: illuminant SPD (NULL = assume spd is already weighted by illuminant)
 * observer: observer type (CIE 1931/1964/2012 2° or 10°)
 * method: integration method (trapezoid or Simpson)
 * bandpass_nm: bandpass width for Stearns & Stearns correction (0 = no correction)
 * xyz_out: output XYZ tristimulus values
 * Returns ALWAN_OK on success */
int alwan_xyz_from_spd(alwan_xyz *xyz_out,
                       alwan_ctx *ctx,
                       alwan_spd const *spd,
                       alwan_spd const *illuminant,
                       alwan_observer_type observer,
                       alwan_integrate_method method,
                       alwan_scalar bandpass_nm);

/* ----------------------------------------------------------------
 * Camera Sensitivities
 * ---------------------------------------------------------------- */

/* Load camera RGB spectral sensitivities
 * Loads R, G, B sensitivity curves for specified camera
 * All three output SPDs must be pre-created with desired wavelength range/count
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if camera not supported */
int alwan_spd_camera_sensitivity(alwan_spd *spd_r,
                                   alwan_spd *spd_g,
                                   alwan_spd *spd_b,
                                   alwan_ctx *ctx,
                                   alwan_camera_sensitivity camera);

/* Compute XYZ from SPD using camera sensitivities
 * Similar to alwan_xyz_from_spd but uses camera RGB sensitivities instead of standard observer
 * Returns ALWAN_OK on success */
int alwan_xyz_from_spd_camera(alwan_xyz *xyz_out,
                               alwan_ctx *ctx,
                               alwan_spd const *spd,
                               alwan_spd const *illuminant,
                               alwan_camera_sensitivity camera,
                               alwan_integrate_method method);

/* ----------------------------------------------------------------
 * Spectral Shape Descriptors
 * ---------------------------------------------------------------- */

/* Analyze SPD shape characteristics
 * Computes peak wavelength, FWHM, centroid, and bandwidth
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if SPD is invalid */
int alwan_spd_analyze_shape(alwan_spd_shape *shape_out, alwan_spd const *spd);

/* ----------------------------------------------------------------
 * Gamut Analysis & Mapping
 * ---------------------------------------------------------------- */

/* Check if xy chromaticity is within Pointer's Gamut
 * Pointer's Gamut represents the boundary of real surface colors under illuminant C
 * xy: CIE 1931 xy chromaticity coordinates
 * Returns 1 if inside Pointer's Gamut, 0 otherwise */
int alwan_is_within_pointer_gamut(alwan_vec2 const *xy);

/* Get Pointer's Gamut boundary points
 * Returns array of xy chromaticity coordinates defining the boundary
 * count_out: receives the number of boundary points (32)
 * Returns pointer to internal static data (do not free) */
alwan_vec2 const* alwan_pointer_gamut_boundary(size_t *count_out);

/* Get CIE 1931 spectral locus xy chromaticity for a given wavelength
 * Computes xy chromaticity from CIE 1931 2° observer CMFs for monochromatic light
 * wavelength: wavelength in nm (360-830nm)
 * xy_out: output xy chromaticity coordinates
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if wavelength out of range */
int alwan_spectral_locus_xy(alwan_vec2 *xy_out, alwan_scalar wavelength);

/* Compute dominant wavelength for a color
 * Dominant wavelength is the wavelength of monochromatic light that,
 * when mixed with the white point, matches the given color's hue
 * xy: CIE 1931 xy chromaticity coordinates of the color
 * xy_white: white point xy chromaticity (e.g., illuminant D65)
 * wavelength_out: receives dominant wavelength in nm (or negative for complementary)
 * xy_wl_out: receives xy of the spectral locus point (optional, can be NULL)
 * xy_cw_out: receives xy of the color-white intersection (optional, can be NULL)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if color is on/near the purple line */
int alwan_dominant_wavelength(alwan_scalar *wavelength_out,
                               alwan_vec2 *xy_wl_out,
                               alwan_vec2 *xy_cw_out,
                               alwan_vec2 const *xy,
                               alwan_vec2 const *xy_white);

/* Compute excitation purity for a color
 * Excitation purity is the ratio of the distance from the white point to the color,
 * divided by the distance from the white point to the spectrum locus, along the
 * line connecting them (0 = white, 1 = spectral/maximum saturation)
 * xy: CIE 1931 xy chromaticity coordinates of the color
 * xy_white: white point xy chromaticity (e.g., illuminant D65)
 * purity_out: receives excitation purity [0-1]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_excitation_purity(alwan_scalar *purity_out,
                             alwan_vec2 const *xy,
                             alwan_vec2 const *xy_white);

/* Compute complementary wavelength for a color
 * Complementary wavelength is used for colors on the purple line (no dominant wavelength)
 * It is the wavelength on the opposite side of the white point
 * xy: CIE 1931 xy chromaticity coordinates of the color
 * xy_white: white point xy chromaticity (e.g., illuminant D65)
 * wavelength_out: receives complementary wavelength in nm
 * xy_wl_out: receives xy of the spectral locus point (optional, can be NULL)
 * xy_cw_out: receives xy of the color-white intersection (optional, can be NULL)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_complementary_wavelength(alwan_scalar *wavelength_out,
                                     alwan_vec2 *xy_wl_out,
                                     alwan_vec2 *xy_cw_out,
                                     alwan_vec2 const *xy,
                                     alwan_vec2 const *xy_white);

/* Compute gamut volume ratio between two RGB color spaces
 * Computes the ratio of gamut volumes: volume(space1) / volume(space2)
 * space1: first RGB color space descriptor
 * space2: second RGB color space descriptor
 * ratio_out: receives volume ratio
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_gamut_volume_ratio(alwan_scalar *ratio_out,
                               alwan_rgb_space_desc const *space1,
                               alwan_rgb_space_desc const *space2);

/* Compute gamut coverage percentage between two RGB color spaces
 * Computes what percentage of space1's gamut is covered by space2's gamut
 * Uses Monte Carlo sampling to estimate overlap
 * space1: reference RGB color space (the gamut we're measuring coverage of)
 * space2: comparison RGB color space (the gamut we're comparing against)
 * num_samples: number of Monte Carlo samples (recommended: 10000+)
 * seed: random seed for reproducibility
 * coverage_out: receives coverage percentage [0-100]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_gamut_coverage(alwan_scalar *coverage_out,
                          alwan_rgb_space_desc const *space1,
                          alwan_rgb_space_desc const *space2,
                          size_t num_samples,
                          unsigned int seed);

/* ----------------------------------------------------------------
 * Advanced Gamut Mapping Algorithms
 * ---------------------------------------------------------------- */

/* Map out-of-gamut RGB color to valid gamut
 * Maps an RGB color (possibly out of [0,1] range) back into valid gamut
 * using perceptually-aware algorithms
 * method: gamut mapping algorithm to use
 * space: RGB color space descriptor (primaries and white point)
 * rgb_linear: input RGB color in linear (not gamma-corrected) space
 * rgb_out: receives mapped RGB color (guaranteed in [0,1])
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_gamut_map_advanced(alwan_rgb *rgb_out,
                              alwan_gamut_map_method method,
                              alwan_rgb_space_desc const *space,
                              alwan_rgb const *rgb_linear);

/* ----------------------------------------------------------------
 * Spectral Upsampling - RGB to Spectrum Conversion
 * ---------------------------------------------------------------- */

/* Smits1999: RGB to spectrum conversion using basis spectra mixing
 * Reference: Smits, Brian. "An RGB to Spectrum Conversion for Reflectances" (1999)
 * ctx: context (for allocation)
 * rgb: input RGB values (assumed to be in sRGB colorspace, clamped to [0, 1])
 * out_spd: output spectral power distribution (wavelength range: 380-720nm, 10 samples)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
int alwan_rgb_to_spectrum_smits1999(alwan_spd *out_spd,
                                     alwan_ctx *ctx,
                                     alwan_rgb const *rgb);

/* Mallett2019: RGB to spectrum conversion using spectral primary decomposition
 * Reference: Mallett & Yuksel. "Spectral Primary Decomposition for Rendering with sRGB Reflectance" (2019)
 * ctx: context (for allocation)
 * rgb: input RGB values (assumed to be in sRGB colorspace)
 * out_spd: output spectral power distribution (wavelength range: 380-780nm, 81 samples at 5nm intervals)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
int alwan_rgb_to_spectrum_mallett2019(alwan_spd *out_spd,
                                       alwan_ctx *ctx,
                                       alwan_rgb const *rgb);

/* Jakob2019 gamut enum - specifies which RGB color space to use for spectral upsampling */
typedef enum {
	ALWAN_JAKOB2019_SRGB = 0,        /* sRGB (standard RGB, Rec.709 primaries) */
	ALWAN_JAKOB2019_PROPHOTO_RGB,    /* ProPhoto RGB (wide gamut) */
	ALWAN_JAKOB2019_ACES2065_1,      /* ACES2065-1 (Academy Color Encoding System) */
	ALWAN_JAKOB2019_REC2020,         /* Rec.2020 (ITU-R BT.2020, HDR/UHD TV) */
	ALWAN_JAKOB2019_ERGB,            /* Extended RGB */
	ALWAN_JAKOB2019_XYZ              /* CIE XYZ */
} alwan_jakob2019_gamut;

/* Jakob2019: RGB to spectrum using polynomial LUT
 * Reference: Jakob & Hanika. "A Low-Dimensional Function Space for Efficient Spectral Upsampling" (2019)
 * ctx: context (for allocation)
 * gamut: RGB color space / gamut to use for upsampling
 * rgb: input RGB values (in the specified gamut, clamped to [0, 1])
 * out_spd: output spectral power distribution (wavelength range: 360-780nm, 85 samples at 5nm intervals)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure, ALWAN_E_INVALID_PARAM if gamut is invalid
 * Note: Requires pre-generated LUT data for the specified gamut (see generate_data.ps1) */
int alwan_rgb_to_spectrum_jakob2019(alwan_spd *out_spd,
                                      alwan_ctx *ctx,
                                      alwan_jakob2019_gamut gamut,
                                      alwan_rgb const *rgb);

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
    alwan_xyz white_xyz;                   /* Reference white in XYZ (Y typically 100) */
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
int alwan_ciecam02_forward(alwan_ciecam02_correlates *out,
                            alwan_xyz const *xyz,
                            alwan_ciecam02_viewing_conditions const *vc);

/* CIECAM02 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
int alwan_ciecam02_inverse(alwan_xyz *xyz_out,
                            alwan_ciecam02_correlates const *correlates,
                            alwan_ciecam02_viewing_conditions const *vc);

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
    alwan_xyz white_xyz;                   /* Reference white in XYZ (Y typically 100) */
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
int alwan_cam16_forward(alwan_cam16_correlates *out,
                        alwan_xyz const *xyz,
                        alwan_cam16_viewing_conditions const *vc);

/* CAM16 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
int alwan_cam16_inverse(alwan_xyz *xyz_out,
                        alwan_cam16_correlates const *correlates,
                        alwan_cam16_viewing_conditions const *vc);

/* Bulk CIECAM02 forward transform
 * xyz_in: input XYZ colors (stride in_stride between consecutive colors)
 * count: number of colors to process
 * correlates_out: output appearance correlates (count elements)
 * Returns ALWAN_OK on success */
int alwan_ciecam02_forward_bulk(alwan_ciecam02_correlates *correlates_out,
                                alwan_scalar const *xyz_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count,
                                size_t in_stride);

/* Bulk CIECAM02 inverse transform
 * correlates_in: input appearance correlates (count elements)
 * xyz_out: output XYZ colors (stride out_stride between consecutive colors)
 * Returns ALWAN_OK on success */
int alwan_ciecam02_inverse_bulk(alwan_scalar *xyz_out,
                                alwan_ciecam02_correlates const *correlates_in,
                                alwan_ciecam02_viewing_conditions const *vc,
                                size_t count,
                                size_t out_stride);

/* Bulk CAM16 forward transform */
int alwan_cam16_forward_bulk(alwan_cam16_correlates *correlates_out,
                             alwan_scalar const *xyz_in,
                             alwan_cam16_viewing_conditions const *vc,
                             size_t count,
                             size_t in_stride);

/* Bulk CAM16 inverse transform */
int alwan_cam16_inverse_bulk(alwan_scalar *xyz_out,
                             alwan_cam16_correlates const *correlates_in,
                             alwan_cam16_viewing_conditions const *vc,
                             size_t count,
                             size_t out_stride);

/* CAM16-UCS (Uniform Color Space) transform for perceptual distance metrics
 * Converts CAM16 JMh to CAM16-UCS Jab for computing perceptual distances
 * correlates: input CAM16 correlates (J, M, h used)
 * Jab_out: output CAM16-UCS coordinates [J', a', b']
 * Returns ALWAN_OK on success */
int alwan_cam16_to_ucs(alwan_cam_jab *Jab_out,
                       alwan_cam16_correlates const *correlates);

/* Inverse CAM16-UCS transform
 * Converts CAM16-UCS Jab back to CAM16 JMh
 * Jab: input CAM16-UCS coordinates [J', a', b']
 * correlates_out: output CAM16 correlates (J, M, h filled; other fields set to 0)
 * Returns ALWAN_OK on success */
int alwan_cam16_from_ucs(alwan_cam16_correlates *correlates_out,
                         alwan_cam_jab const *Jab);

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
    alwan_xyz xyz_w;                   /* White point (absolute XYZ in cd/m²) */
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
int alwan_zcam_forward(alwan_zcam_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_zcam_viewing_conditions const *vc);

/* ZCAM inverse transform: appearance correlates -> XYZ (approximate)
 * correlates: appearance correlates
 * vc: viewing conditions
 * xyz: output XYZ tristimulus values (cd/m²)
 * Returns 0 on success, -1 on error
 * Note: Inverse is approximate due to complexity */
int alwan_zcam_inverse(alwan_xyz *xyz,
                       alwan_zcam_correlates const *correlates,
                       alwan_zcam_viewing_conditions const *vc);

/* ZCAM to UCS (Uniform Color Space) for color difference
 * correlates: input ZCAM correlates
 * Jab_out: output ZCAM-UCS coordinates [Jz, az, bz]
 * Returns 0 on success, -1 on error */
int alwan_zcam_to_ucs(alwan_jzazbz *Jab_out,
                      alwan_zcam_correlates const *correlates);

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
    alwan_xyz xyz_w;               /* White point (XYZ, Y=100) */
    alwan_xyz xyz_n;               /* Reference white (usually D65) */
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
int alwan_rlab_forward(alwan_rlab_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_rlab_viewing_conditions const *vc);

/* RLAB inverse transform: appearance correlates -> XYZ
 * Returns 0 on success, -1 on error */
int alwan_rlab_inverse(alwan_xyz *xyz,
                       alwan_rlab_correlates const *correlates,
                       alwan_rlab_viewing_conditions const *vc);

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
    alwan_xyz xyz_w;                   /* White point (XYZ, Y=100) */
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
int alwan_hunt_forward(alwan_hunt_correlates *out,
                       alwan_xyz const *xyz,
                       alwan_hunt_viewing_conditions const *vc);

/* ----------------------------------------------------------------
 * Hellwig2022 Color Appearance Model
 * Based on Hellwig and Fairchild (2022)
 * Improved CAM16 with Helmholtz-Kohlrausch effect support
 * ---------------------------------------------------------------- */

/* Hellwig2022 uses same surround conditions as CIECAM02/CAM16 */
typedef enum {
    ALWAN_HELLWIG2022_SURROUND_AVERAGE = 0,  /* Average surround (F=1.0, c=0.69, Nc=1.0) */
    ALWAN_HELLWIG2022_SURROUND_DIM = 1,      /* Dim surround (F=0.9, c=0.59, Nc=0.95) */
    ALWAN_HELLWIG2022_SURROUND_DARK = 2      /* Dark surround (F=0.8, c=0.525, Nc=0.8) */
} alwan_hellwig2022_surround;

/* Hellwig2022 viewing conditions */
typedef struct {
    alwan_xyz white_xyz;                       /* Reference white in XYZ (Y typically 100) */
    alwan_scalar adapting_luminance;           /* Luminance of adapting field (La) in cd/m² */
    alwan_scalar background_luminance;         /* Relative luminance of background (Yb/Yw, typically 0.2) */
    alwan_hellwig2022_surround surround;       /* Viewing surround condition */
    int discount_illuminant;                   /* 1 to discount illuminant, 0 otherwise */
} alwan_hellwig2022_viewing_conditions;

/* Hellwig2022 appearance correlates */
typedef struct {
    alwan_scalar J;  /* Lightness (0-100) */
    alwan_scalar C;  /* Chroma (0+) */
    alwan_scalar h;  /* Hue angle (0-360 degrees) */
    alwan_scalar s;  /* Saturation (0+) */
    alwan_scalar Q;  /* Brightness (0+) */
    alwan_scalar M;  /* Colorfulness (0+) */
    alwan_scalar H;  /* Hue quadrature (0-400) */
} alwan_hellwig2022_correlates;

/* Hellwig2022 forward transform: XYZ -> appearance correlates */
int alwan_hellwig2022_forward(alwan_hellwig2022_correlates *out,
                               alwan_xyz const *xyz,
                               alwan_hellwig2022_viewing_conditions const *vc);

/* Hellwig2022 inverse transform: appearance correlates -> XYZ */
int alwan_hellwig2022_inverse(alwan_xyz *xyz_out,
                               alwan_hellwig2022_correlates const *correlates,
                               alwan_hellwig2022_viewing_conditions const *vc);

/* ----------------------------------------------------------------
 * Kim2009 Color Appearance Model
 * Based on Kim, Weyrich and Kautz (2009)
 * Specialized for rendering applications
 * ---------------------------------------------------------------- */

/* Kim2009 viewing conditions */
typedef struct {
    alwan_xyz white_xyz;               /* Reference white in XYZ (Y typically 100) */
    alwan_scalar La;                   /* Adapting luminance (cd/m²) */
    alwan_scalar Yb;                   /* Background luminance factor */
    int discount_illuminant;           /* 1 to discount illuminant, 0 otherwise */
} alwan_kim2009_viewing_conditions;

/* Kim2009 appearance correlates */
typedef struct {
    alwan_scalar J;  /* Lightness */
    alwan_scalar C;  /* Chroma */
    alwan_scalar h;  /* Hue angle (degrees) */
} alwan_kim2009_correlates;

/* Kim2009 forward transform: XYZ -> appearance correlates */
int alwan_kim2009_forward(alwan_kim2009_correlates *out,
                           alwan_xyz const *xyz,
                           alwan_kim2009_viewing_conditions const *vc);

/* Kim2009 inverse transform: appearance correlates -> XYZ */
int alwan_kim2009_inverse(alwan_xyz *xyz_out,
                           alwan_kim2009_correlates const *correlates,
                           alwan_kim2009_viewing_conditions const *vc);

/* ----------------------------------------------------------------
 * LLAB Color Appearance Model
 * Based on Luo, Lo and Kuo (1996)
 * Cross-media color reproduction model
 * ---------------------------------------------------------------- */

/* LLAB surround condition */
typedef enum {
    ALWAN_LLAB_SURROUND_AVERAGE = 0,  /* Average surround */
    ALWAN_LLAB_SURROUND_DIM = 1,      /* Dim surround */
    ALWAN_LLAB_SURROUND_DARK = 2      /* Dark surround */
} alwan_llab_surround;

/* LLAB viewing conditions */
typedef struct {
    alwan_xyz white_xyz;           /* Reference white in XYZ (Y typically 100) */
    alwan_xyz xyz_0;               /* Illuminant of test condition */
    alwan_xyz xyz_r;               /* Illuminant of reference condition */
    alwan_scalar Y_b;              /* Background luminance factor */
    alwan_llab_surround surround;  /* Viewing surround */
    int D_factor;                  /* Degree of adaptation (0-1 or auto) */
} alwan_llab_viewing_conditions;

/* LLAB appearance correlates */
typedef struct {
    alwan_scalar L;  /* Lightness */
    alwan_scalar Ch; /* Chroma */
    alwan_scalar h;  /* Hue angle (degrees) */
    alwan_scalar s;  /* Saturation */
} alwan_llab_correlates;

/* LLAB forward transform: XYZ -> appearance correlates */
int alwan_llab_forward(alwan_llab_correlates *out,
                        alwan_xyz const *xyz,
                        alwan_llab_viewing_conditions const *vc);

/* ----------------------------------------------------------------
 * ATD95 Color Vision Model
 * Based on Guth's ATD (1995)
 * Advanced temporal dynamics model
 * ---------------------------------------------------------------- */

/* ATD95 viewing conditions */
typedef struct {
    alwan_xyz white_xyz;           /* Reference white in XYZ */
    alwan_scalar Y_0;              /* Absolute adapting field luminance in cd/m² */
    alwan_scalar sigma;            /* Saturation adjustment parameter */
    alwan_scalar k1;               /* Adaptation parameter 1 */
    alwan_scalar k2;               /* Adaptation parameter 2 */
} alwan_atd95_viewing_conditions;

/* ATD95 correlates */
typedef struct {
    alwan_scalar H;    /* Hue */
    alwan_scalar C;    /* Chroma */
    alwan_scalar Br;   /* Brightness */
    alwan_scalar A_1;  /* First achromatic response */
    alwan_scalar T_1;  /* First tritanopic response */
    alwan_scalar D_1;  /* First deuteranopic response */
    alwan_scalar A_2;  /* Second achromatic response */
    alwan_scalar T_2;  /* Second tritanopic response */
    alwan_scalar D_2;  /* Second deuteranopic response */
} alwan_atd95_correlates;

/* ATD95 forward transform: XYZ -> correlates */
int alwan_atd95_forward(alwan_atd95_correlates *out,
                         alwan_xyz const *xyz,
                         alwan_atd95_viewing_conditions const *vc);

/* ----------------------------------------------------------------
 * Nayatani95 Color Appearance Model
 * Based on Nayatani et al. (1995)
 * Japanese color appearance model
 * ---------------------------------------------------------------- */

/* Nayatani95 viewing conditions */
typedef struct {
    alwan_xyz white_xyz;               /* Reference white in XYZ (Y typically 100) */
    alwan_scalar L_0;                  /* Absolute luminance of reference white (cd/m²) */
    alwan_scalar Y_0;                  /* Relative luminance of background */
    alwan_scalar E_0;                  /* Illuminance of reference field (lux) */
    alwan_scalar E_0r;                 /* Normalizing factor */
} alwan_nayatani95_viewing_conditions;

/* Nayatani95 appearance correlates */
typedef struct {
    alwan_scalar L_star_N;  /* Perceived lightness */
    alwan_scalar C;         /* Chroma */
    alwan_scalar theta;     /* Hue angle (radians) */
    alwan_scalar S;         /* Saturation */
    alwan_scalar B_r;       /* Brightness */
    alwan_scalar L_star_P;  /* Brightness-to-lightness ratio */
} alwan_nayatani95_correlates;

/* Nayatani95 forward transform: XYZ -> appearance correlates */
int alwan_nayatani95_forward(alwan_nayatani95_correlates *out,
                               alwan_xyz const *xyz,
                               alwan_nayatani95_viewing_conditions const *vc);

/* ----------------------------------------------------------------
 * M9: Convenience Color Models (HSV, HSL, CMY, CMYK, YCbCr)
 * ---------------------------------------------------------------- */

/* RGB <-> HSV conversions (all values in [0, 1]) */
int alwan_rgb_to_hsv(alwan_hsv *hsv_out, alwan_rgb const *rgb);
int alwan_hsv_to_rgb(alwan_rgb *rgb_out, alwan_hsv const *hsv);

/* RGB <-> HSL conversions (all values in [0, 1]) */
int alwan_rgb_to_hsl(alwan_hsl *hsl_out, alwan_rgb const *rgb);
int alwan_hsl_to_rgb(alwan_rgb *rgb_out, alwan_hsl const *hsl);

/* RGB <-> CMY conversions (all values in [0, 1]) */
int alwan_rgb_to_cmy(alwan_cmy *cmy_out, alwan_rgb const *rgb);
int alwan_cmy_to_rgb(alwan_rgb *rgb_out, alwan_cmy const *cmy);

/* CMY <-> CMYK conversions (all values in [0, 1]) */
int alwan_cmy_to_cmyk(alwan_scalar *c, alwan_scalar *m, alwan_scalar *y, alwan_scalar *k, alwan_cmy const *cmy);
int alwan_cmyk_to_cmy(alwan_cmy *cmy_out, alwan_scalar c, alwan_scalar m, alwan_scalar y, alwan_scalar k);

/* YCbCr standard identifiers */
typedef enum {
    ALWAN_YCBCR_BT601,    /* ITU-R BT.601 (SD) */
    ALWAN_YCBCR_BT709,    /* ITU-R BT.709 (HD) */
    ALWAN_YCBCR_BT2020    /* ITU-R BT.2020 (UHD) */
} alwan_ycbcr_standard;

/* RGB <-> YCbCr conversions (RGB in [0, 1], YCbCr full range [0, 1]) */
int alwan_rgb_to_ycbcr(alwan_ycbcr *ycbcr_out, alwan_rgb const *rgb, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb(alwan_rgb *rgb_out, alwan_ycbcr const *ycbcr, alwan_ycbcr_standard standard);

/* RGB <-> YcCbcCrc conversions (constant luminance, BT.2020) */
int alwan_rgb_to_yccbccrc(alwan_yccbccrc *yccbccrc_out, alwan_rgb const *rgb);
int alwan_yccbccrc_to_rgb(alwan_rgb *rgb_out, alwan_yccbccrc const *yccbccrc);

/* RGB <-> YCoCg conversions (video compression, real-time graphics)
 * - Y: luma, Co: orange chrominance, Cg: green chrominance
 * - Reversible integer transform (exact round-trip with proper scaling)
 * - Used in H.264/AVC and video codecs */
int alwan_rgb_to_ycocg(alwan_ycocg *ycocg_out, alwan_rgb const *rgb);

/* Bulk convenience color model conversions */
int alwan_rgb_to_hsv_bulk(alwan_scalar *hsv_out,
                          alwan_scalar const *rgb_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_hsv_to_rgb_bulk(alwan_scalar *rgb_out,
                          alwan_scalar const *hsv_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_rgb_to_hsl_bulk(alwan_scalar *hsl_out,
                          alwan_scalar const *rgb_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);

int alwan_hsl_to_rgb_bulk(alwan_scalar *rgb_out,
                          alwan_scalar const *hsl_in,
                          size_t count,
                          size_t in_stride,
                          size_t out_stride);
int alwan_ycocg_to_rgb(alwan_rgb *rgb_out, alwan_ycocg const *ycocg);

/* ----------------------------------------------------------------
 * M10: Light Quality & CCT (Correlated Color Temperature)
 * ---------------------------------------------------------------- */

/* CCT estimation from chromaticity coordinates (xy) */
/* McCamy approximation: fast, ~2% accuracy above 2800K */
alwan_scalar alwan_cct_mccamy_xy(alwan_vec2 const *xy);

/* Robertson method: accurate, iterative lookup against Planckian locus */
/* Returns CCT in Kelvin, or negative value on error */
alwan_scalar alwan_cct_robertson_xy(alwan_vec2 const *xy);

/* Hernandez-Andres 1999: accurate analytical formula
 * Valid range: 3000K - 50000K (extended formula for higher CCT)
 * Reference: Hernandez-Andres et al. (1999) */
alwan_scalar alwan_cct_hernandez_xy(alwan_vec2 const *xy);

/* Kang 2002: CCT to xy chromaticity (forward transform)
 * Valid range: 1667K - 25000K
 * Reference: Kang et al. (2002) */
void alwan_cct_to_xy_kang(alwan_vec2 *xy_out, alwan_scalar cct);

/* Kang 2002: xy chromaticity to CCT (inverse, uses Newton-Raphson)
 * Valid range: 1667K - 25000K
 * Reference: Kang et al. (2002) */
alwan_scalar alwan_cct_kang_xy(alwan_vec2 const *xy);

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
 * Color Vision & Perception
 * ---------------------------------------------------------------- */

/* Color Blindness Simulation (CVD - Color Vision Deficiency) */

/* CVD (Color Vision Deficiency) types */
typedef enum {
    ALWAN_CVD_PROTANOPIA = 0,     /* Red-blind (L-cone absent) */
    ALWAN_CVD_DEUTERANOPIA = 1,   /* Green-blind (M-cone absent) */
    ALWAN_CVD_TRITANOPIA = 2,     /* Blue-blind (S-cone absent) */
    ALWAN_CVD_PROTANOMALY = 3,    /* Red-weak (L-cone deficient) */
    ALWAN_CVD_DEUTERANOMALY = 4,  /* Green-weak (M-cone deficient) */
    ALWAN_CVD_TRITANOMALY = 5     /* Blue-weak (S-cone deficient) */
} alwan_cvd_type;

/* Simulate color vision deficiency (color blindness)
 * rgb_in: input linear RGB color [0, 1]
 * cvd_type: type of color vision deficiency
 * severity: severity of deficiency [0, 1] (1.0 = complete, 0.0 = normal vision)
 *          (only applies to anomalous trichromacy types)
 * rgb_out: output simulated RGB color as seen by person with CVD
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error
 * Algorithm: Brettel, Viénot & Mollon (1997) simulation using confusion lines */
int alwan_simulate_cvd(alwan_rgb *rgb_out,
                        alwan_rgb const *rgb_in,
                        alwan_cvd_type cvd_type,
                        alwan_scalar severity);

/* Luminous Efficiency Functions */

/* Vision type for luminous efficiency */
typedef enum {
    ALWAN_VISION_PHOTOPIC = 0,  /* Photopic (daytime, cone-based) - V(λ) */
    ALWAN_VISION_SCOTOPIC = 1,  /* Scotopic (nighttime, rod-based) - V'(λ) */
    ALWAN_VISION_MESOPIC = 2    /* Mesopic (twilight, mixed rod/cone) */
} alwan_vision_type;

/* Get luminous efficiency for a given wavelength and vision type
 * wavelength: wavelength in nanometers [360, 830]
 * vision_type: photopic, scotopic, or mesopic
 * Returns luminous efficiency value [0, 1], or negative on error
 * Data: CIE photopic V(λ) 1924/1988, CIE scotopic V'(λ) 1951 */
alwan_scalar alwan_luminous_efficiency(alwan_scalar wavelength, alwan_vision_type vision_type);

/* Calculate photopic luminance from SPD
 * spd: spectral power distribution
 * Returns photopic luminance in cd/m², or negative on error */
alwan_scalar alwan_photopic_luminance(alwan_ctx *ctx, alwan_spd const *spd);

/* Calculate scotopic luminance from SPD
 * spd: spectral power distribution
 * Returns scotopic luminance in cd/m², or negative on error */
alwan_scalar alwan_scotopic_luminance(alwan_ctx *ctx, alwan_spd const *spd);

/* Calculate mesopic luminance from SPD
 * spd: spectral power distribution
 * adaptation_level: adaptation luminance level in cd/m² [0.001, 10]
 * Returns mesopic luminance in cd/m², or negative on error
 * Uses CIE 191:2010 mesopic vision model */
alwan_scalar alwan_mesopic_luminance(alwan_ctx *ctx,
                                      alwan_spd const *spd,
                                      alwan_scalar adaptation_level);

/* Contrast Sensitivity Function (CSF) */

/* Calculate contrast sensitivity for spatial frequency (simplified model)
 * spatial_frequency: spatial frequency in cycles per degree [0.1, 60]
 * luminance: background luminance in cd/m² [0.01, 10000]
 * Returns contrast sensitivity (1/contrast_threshold), or negative on error
 * Uses simplified Barten CSF model (1999) */
alwan_scalar alwan_csf(alwan_scalar spatial_frequency, alwan_scalar luminance);

/* ================================================================
 * Barten 1999 Full Model - Contrast Sensitivity Functions
 * Reference: Barten (1999), colour-science implementation
 * ================================================================ */

/* Pupil diameter using Barten (1999) method
 * L: Average luminance in cd/m²
 * X_0: Angular size of object in degrees (x direction)
 * Y_0: Angular size of object in degrees (y direction), -1 to use X_0
 * Returns: Pupil diameter in millimeters */
alwan_scalar alwan_pupil_diameter_barten1999(alwan_scalar L,
                                              alwan_scalar X_0,
                                              alwan_scalar Y_0);

/* Retinal illuminance using Barten (1999) method
 * L: Average luminance in cd/m²
 * d: Pupil diameter in millimeters
 * apply_stiles_crawford: Whether to apply Stiles-Crawford correction (1=yes, 0=no)
 * Returns: Retinal illuminance in Trolands */
alwan_scalar alwan_retinal_illuminance_barten1999(alwan_scalar L,
                                                   alwan_scalar d,
                                                   int apply_stiles_crawford);

/* Optical MTF (Modulation Transfer Function) using Barten (1999) method
 * u: Spatial frequency in cycles per degree
 * sigma: Standard deviation of line-spread function (use alwan_sigma_barten1999)
 * Returns: Optical MTF value [0, 1] */
alwan_scalar alwan_optical_mtf_barten1999(alwan_scalar u, alwan_scalar sigma);

/* Standard deviation of line-spread function using Barten (1999) method
 * sigma_0: Constant sigma_0 in degrees (default: 0.5/60 = 0.00833...)
 * C_ab: Spherical aberration in degrees/mm (default: 0.08/60 = 0.00133...)
 * d: Pupil diameter in millimeters
 * Returns: Standard deviation sigma in degrees */
alwan_scalar alwan_sigma_barten1999(alwan_scalar sigma_0,
                                     alwan_scalar C_ab,
                                     alwan_scalar d);

/* Maximum angular size using Barten (1999) method
 * u: Spatial frequency in cycles per degree
 * X_0: Angular size of object in degrees
 * X_max: Maximum angular size of integration area in degrees (default: 12)
 * N_max: Maximum number of integration cycles (default: 15)
 * Returns: Maximum angular size in degrees */
alwan_scalar alwan_maximum_angular_size_barten1999(alwan_scalar u,
                                                    alwan_scalar X_0,
                                                    alwan_scalar X_max,
                                                    alwan_scalar N_max);

/* Full Barten (1999) CSF model parameters */
typedef struct {
    alwan_scalar sigma;    /* Line-spread function std dev (degrees) */
    alwan_scalar k;        /* Signal-to-noise ratio (default: 3.0) */
    alwan_scalar T;        /* Integration time in seconds (default: 0.1) */
    alwan_scalar X_0;      /* Angular size x in degrees (default: 60) */
    alwan_scalar Y_0;      /* Angular size y in degrees (-1 = use X_0) */
    alwan_scalar X_max;    /* Max integration area x in degrees (default: 12) */
    alwan_scalar Y_max;    /* Max integration area y in degrees (-1 = use X_max) */
    alwan_scalar N_max;    /* Max integration cycles (default: 15) */
    alwan_scalar n;        /* Quantum efficiency (default: 0.03) */
    alwan_scalar p;        /* Photon conversion factor (default: 1.2274e6) */
    alwan_scalar E;        /* Retinal illuminance in Trolands */
    alwan_scalar phi_0;    /* Neural noise spectral density (default: 3e-8) */
    alwan_scalar u_0;      /* Lateral inhibition cutoff frequency (default: 7) */
} alwan_csf_barten1999_params;

/* Initialize CSF parameters with defaults
 * Defaults valid for standard observer, age 20-30, good vision */
void alwan_csf_barten1999_params_default(alwan_csf_barten1999_params *params);

/* Full Barten (1999) CSF using all parameters
 * u: Spatial frequency in cycles per degree
 * params: Model parameters (use NULL for defaults)
 * Returns: Contrast sensitivity S */
alwan_scalar alwan_csf_barten1999(alwan_scalar u,
                                   alwan_csf_barten1999_params const *params);

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
    
/* ----------------------------------------------------------------
 * Advanced Mathematical & Utility Functions
 * ---------------------------------------------------------------- */

/* Interpolation method types */
typedef enum {
    ALWAN_INTERP_LINEAR = 0,     /* Linear interpolation (default) */
    ALWAN_INTERP_CUBIC,           /* Cubic interpolation */
    ALWAN_INTERP_LANCZOS,         /* Lanczos windowed sinc */
    ALWAN_INTERP_SPRAGUE,         /* Sprague 5th order (for smooth spectra) */
    ALWAN_INTERP_LAGRANGE,        /* Lagrange polynomial */
    ALWAN_INTERP_AKIMA            /* Akima spline (non-overshooting) */
} alwan_interp_method;

/* Extrapolation method types */
typedef enum {
    ALWAN_EXTRAP_CONSTANT = 0,    /* Constant (use boundary value) */
    ALWAN_EXTRAP_LINEAR,          /* Linear extrapolation */
    ALWAN_EXTRAP_POLYNOMIAL,      /* Polynomial extrapolation */
    ALWAN_EXTRAP_EXPONENTIAL,     /* Exponential decay (for SPDs) */
    ALWAN_EXTRAP_REFLECT,         /* Reflective boundary */
    ALWAN_EXTRAP_NATURAL          /* Natural neighbor extrapolation */
} alwan_extrap_method;

/* Color Checker target types */
typedef enum {
    ALWAN_COLORCHECKER_CLASSIC = 0,      /* ColorChecker Classic 24-patch */
    ALWAN_COLORCHECKER_SG,                /* ColorChecker SG 140-patch */
    ALWAN_COLORCHECKER_DIGITAL_SG,        /* ColorChecker Digital SG */
    ALWAN_BABELCOLOR_AVERAGE,             /* BabelColor Average */
    ALWAN_BABELCOLOR_HCT                  /* BabelColor HCT */
} alwan_colorchecker_type;

/* Advanced Interpolation
 * Interpolates data points (x_in, y_in) to output points x_out
 * x_in: input x coordinates (must be sorted ascending)
 * y_in: input y values
 * count_in: number of input points
 * x_out: output x coordinates
 * y_out: output y values (allocated by caller)
 * count_out: number of output points
 * method: interpolation method
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_interpolate(alwan_scalar const *x_in, alwan_scalar const *y_in, size_t count_in,
                       alwan_scalar const *x_out, alwan_scalar *y_out, size_t count_out,
                       alwan_interp_method method);

/* Enhanced Extrapolation
 * Extrapolates data points (x_in, y_in) to output points x_out
 * Uses specified method for points outside the input range
 * x_in: input x coordinates (must be sorted ascending)
 * y_in: input y values
 * count_in: number of input points
 * x_out: output x coordinates
 * y_out: output y values (allocated by caller)
 * count_out: number of output points
 * method: extrapolation method
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_extrapolate(alwan_scalar const *x_in, alwan_scalar const *y_in, size_t count_in,
                       alwan_scalar const *x_out, alwan_scalar *y_out, size_t count_out,
                       alwan_extrap_method method);

/* CCT and Duv Optimization
 * Computes Correlated Color Temperature (CCT) and distance from Planckian locus (Duv)
 * using iterative least-squares optimization
 * xy: CIE 1931 xy chromaticity coordinates
 * cct_out: receives CCT in Kelvin
 * duv_out: receives Duv (distance from Planckian locus, can be NULL)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if xy is invalid
 * Accuracy: CCT <= 1K, Duv <= 0.0001 */
int alwan_cct_duv_optimize(alwan_scalar *cct_out, alwan_scalar *duv_out, alwan_vec2 const *xy);

/* Tristimulus Optimization
 * Finds a spectral power distribution that matches target XYZ tristimulus values
 * target_xyz: target XYZ tristimulus values
 * observer: observer type (e.g., CIE 1931 2°)
 * ctx: context for SPD allocation
 * spd_out: receives optimized SPD (must be pre-allocated with desired wavelength range)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error
 * Note: Multiple SPDs can match the same XYZ (metamerism), this finds one solution */
int alwan_optimize_spectrum_for_xyz(alwan_spd *spd_out,
                                      alwan_xyz const *target_xyz,
                                      alwan_observer_type observer,
                                      alwan_ctx *ctx);

/* 1D Table Interpolation
 * Interpolates a value from a 1D lookup table
 * table: 1D LUT array
 * size: number of elements in table
 * x: input coordinate [0, 1] (normalized)
 * method: interpolation method (LINEAR or CUBIC)
 * Returns interpolated value */
alwan_scalar alwan_table_interp_1d(alwan_scalar const *table, size_t size,
                                    alwan_scalar x, alwan_interp_method method);

/* 3D Table Interpolation (Trilinear)
 * Interpolates RGB values from a 3D lookup table using trilinear method
 * table: 3D LUT array (R-major: table[r][g][b])
 * sizes: dimensions [size_r, size_g, size_b]
 * rgb_in: input RGB coordinates [0, 1] (normalized)
 * rgb_out: receives interpolated RGB values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_table_interp_3d_trilinear(alwan_rgb *rgb_out,
                                     alwan_scalar const *table, size_t const sizes[3],
                                     alwan_rgb const *rgb_in);

/* 3D Table Interpolation (Tetrahedral)
 * Interpolates RGB values from a 3D lookup table using tetrahedral method
 * Tetrahedral is more accurate than trilinear for color transforms
 * table: 3D LUT array (R-major: table[r][g][b])
 * sizes: dimensions [size_r, size_g, size_b]
 * rgb_in: input RGB coordinates [0, 1] (normalized)
 * rgb_out: receives interpolated RGB values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_table_interp_3d_tetrahedral(alwan_rgb *rgb_out,
                                       alwan_scalar const *table, size_t const sizes[3],
                                       alwan_rgb const *rgb_in);

/* ----------------------------------------------------------------
 * Data & Reference Sets
 * ---------------------------------------------------------------- */

/* Munsell Renotation Data
 * Convert Munsell notation (Hue, Value, Chroma) to XYZ tristimulus values
 * Uses the Munsell Renotation Data (1943)
 * hue: Munsell hue [0, 100] (continuous)
 * value: Munsell value [0, 10]
 * chroma: Munsell chroma [0, 20+]
 * illuminant: illuminant for XYZ calculation
 * xyz: receives XYZ tristimulus values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_munsell_to_xyz(alwan_xyz *xyz,
                         alwan_scalar hue, alwan_scalar value, alwan_scalar chroma,
                         alwan_illuminant illuminant);

/* Convert XYZ tristimulus values to Munsell notation (Hue, Value, Chroma)
 * xyz: XYZ tristimulus values
 * illuminant: illuminant for XYZ calculation
 * hue: receives Munsell hue [0, 100]
 * value: receives Munsell value [0, 10]
 * chroma: receives Munsell chroma [0, 20+]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_xyz_to_munsell(alwan_scalar *hue, alwan_scalar *value, alwan_scalar *chroma,
                         alwan_xyz const *xyz, alwan_illuminant illuminant);

/* Color Checker Data
 * Get XYZ tristimulus values for a Color Checker patch
 * type: Color Checker target type
 * illuminant: illuminant for XYZ calculation
 * patch_index: patch index [0, num_patches-1]
 * xyz: receives XYZ tristimulus values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_color_checker_data(alwan_xyz *xyz,
                              alwan_colorchecker_type type, alwan_illuminant illuminant,
                              size_t patch_index);

/* Get number of patches in a Color Checker target
 * type: Color Checker target type
 * Returns number of patches, or 0 on error */
size_t alwan_color_checker_num_patches(alwan_colorchecker_type type);

/* NCS (Natural Color System) Data
 * Convert NCS notation to XYZ tristimulus values
 * ncs_notation: NCS notation string (e.g., "S 1050-Y90R")
 * xyz: receives XYZ tristimulus values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_ncs_to_xyz(alwan_xyz *xyz, char const *ncs_notation);

/* Convert XYZ tristimulus values to NCS notation
 * xyz: XYZ tristimulus values
 * ncs_notation: receives NCS notation string (allocated by caller)
 * notation_size: size of notation buffer (should be >= 32)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_xyz_to_ncs(char *ncs_notation, size_t notation_size, alwan_xyz const *xyz);

/* Additional RGB Space Definitions
 * Get RGB space primaries and white point by enum
 * space: RGB color space identifier
 * primaries: receives RGB primaries as xy chromaticities (3x2 matrix)
 * white_point: receives white point xy chromaticity
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
int alwan_rgb_space_by_enum(alwan_scalar primaries[6], alwan_vec2 *white_point, alwan_rgb_space space);

/* Get RGB space transfer functions
 * space: RGB color space identifier
 * oetf: receives OETF (Opto-Electronic Transfer Function)
 * eotf: receives EOTF (Electro-Optical Transfer Function)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
int alwan_rgb_space_get_tfs(alwan_transfer_function *oetf, alwan_transfer_function *eotf, alwan_rgb_space space);

/* ================================================================
 * Color Correction & Grading Tools
 * ================================================================ */

/* Lift/Gamma/Gain (LGG) color correction
 * rgb_in: input RGB values (linear, [0,1] for normal range)
 * lift: lift adjustment per channel (shadows) - typical range [-1, 1]
 * gamma: gamma adjustment per channel (midtones) - typical range [0.0001, 10]
 * gain: gain adjustment per channel (highlights) - typical range [0, 2]
 * rgb_out: output RGB values
 * Formula: rgb_out = ((rgb_in + lift) ^ (1/gamma)) * gain
 * Returns ALWAN_OK on success */
int alwan_lgg_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb_in, alwan_rgb const *lift,
                    alwan_rgb const *gamma, alwan_rgb const *gain);

/* Color matrix grading preset types */
typedef enum {
    ALWAN_COLOR_MATRIX_SEPIA = 0,           /* Sepia tone effect */
    ALWAN_COLOR_MATRIX_VINTAGE,             /* Vintage look */
    ALWAN_COLOR_MATRIX_BLEACH_BYPASS,       /* Bleach bypass effect */
    ALWAN_COLOR_MATRIX_COOL,                /* Cool tone shift */
    ALWAN_COLOR_MATRIX_WARM,                /* Warm tone shift */
    ALWAN_COLOR_MATRIX_MONOCHROME,          /* Black and white */
    ALWAN_COLOR_MATRIX_NIGHT_VISION         /* Night vision look */
} alwan_color_matrix_preset;

/* Apply color matrix transformation (custom or preset)
 * rgb_in: input RGB values
 * matrix_3x3: 3x3 color transformation matrix
 * rgb_out: output RGB values
 * Returns ALWAN_OK on success */
int alwan_color_matrix_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb_in,
                              alwan_mat3x3 const *matrix_3x3);

/* Get preset color grading matrix
 * preset: preset type from alwan_color_matrix_preset
 * matrix_3x3: receives the preset matrix
 * Returns ALWAN_OK on success, ALWAN_E_INVALID for unknown preset */
int alwan_color_matrix_get_preset(alwan_mat3x3 *matrix_3x3, alwan_color_matrix_preset preset);

/* Printer lights color correction (film-style)
 * rgb_in: input RGB values (linear)
 * red_lights: red printer light adjustment (0-50, default 25)
 * green_lights: green printer light adjustment (0-50, default 25)
 * blue_lights: blue printer light adjustment (0-50, default 25)
 * rgb_out: output RGB values
 * Each light unit represents approximately 0.025 log exposure change
 * Returns ALWAN_OK on success */
int alwan_printer_lights_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb_in,
                                alwan_scalar red_lights, alwan_scalar green_lights,
                                alwan_scalar blue_lights);

/* ================================================================
 * Camera Profiling / Polynomial Color Correction
 * Reference: Cheung et al. (2004), Finlayson et al. (2015)
 * ================================================================ */

/* Cheung 2004 polynomial expansion terms
 * Values represent the number of terms in the expanded polynomial */
typedef enum {
    ALWAN_POLY_CHEUNG_3  = 3,   /* [R, G, B] */
    ALWAN_POLY_CHEUNG_4  = 4,   /* [R, G, B, 1] */
    ALWAN_POLY_CHEUNG_5  = 5,   /* [R, G, B, RG, 1] */
    ALWAN_POLY_CHEUNG_7  = 7,   /* [R, G, B, RG, RB, GB, 1] */
    ALWAN_POLY_CHEUNG_8  = 8,   /* [R, G, B, RG, RB, GB, RGB, 1] */
    ALWAN_POLY_CHEUNG_10 = 10,  /* [R, G, B, RG, RB, GB, R², G², B², 1] */
    ALWAN_POLY_CHEUNG_11 = 11,  /* [R, G, B, RG, RB, GB, R², G², B², RGB, 1] */
    ALWAN_POLY_CHEUNG_14 = 14,  /* [R, G, B, RG, RB, GB, R², G², B², RGB, R²G, RG², R²B, 1] */
    ALWAN_POLY_CHEUNG_16 = 16,  /* 16-term expansion */
    ALWAN_POLY_CHEUNG_17 = 17,  /* 17-term expansion */
    ALWAN_POLY_CHEUNG_19 = 19,  /* 19-term expansion */
    ALWAN_POLY_CHEUNG_20 = 20,  /* 20-term expansion */
    ALWAN_POLY_CHEUNG_22 = 22,  /* 22-term expansion */
    ALWAN_POLY_CHEUNG_35 = 35   /* Full 35-term expansion (maximum) */
} alwan_poly_cheung_terms;

/* Polynomial expansion - Cheung 2004 method
 * Expands RGB to higher-dimensional polynomial space for camera profiling.
 * rgb: input RGB triplet [0,1]
 * terms: number of terms (from alwan_poly_cheung_terms)
 * out: output array (must be at least 'terms' elements)
 * Returns ALWAN_OK on success */
int alwan_poly_expand_cheung2004(alwan_scalar *out, alwan_rgb const *rgb,
                                  alwan_poly_cheung_terms terms);

/* Polynomial expansion - Finlayson 2015 method
 * rgb: input RGB triplet [0,1]
 * degree: polynomial degree (1-4)
 * root_poly: if true, use root-polynomial expansion
 * out: output array (size depends on degree: 3,6,10,15 for degrees 1,2,3,4)
 * out_size: receives actual output size
 * Returns ALWAN_OK on success */
int alwan_poly_expand_finlayson2015(alwan_scalar *out, int *out_size,
                                     alwan_rgb const *rgb, int degree, int root_poly);

/* Polynomial expansion - Vandermonde method
 * a: input array (typically RGB)
 * a_size: size of input array
 * degree: polynomial degree
 * out: output array
 * out_size: receives actual output size
 * Returns ALWAN_OK on success */
int alwan_poly_expand_vandermonde(alwan_scalar *out, int *out_size,
                                   alwan_scalar const *a, int a_size, int degree);

/* Compute colour correction matrix using Cheung 2004 method
 * M_T: test (measured) RGB values, Nx3 array (row-major)
 * M_R: reference RGB values, Nx3 array (row-major)
 * num_samples: number of color samples (N)
 * terms: polynomial expansion terms
 * matrix_out: receives the correction matrix (terms x 3, row-major)
 * Returns ALWAN_OK on success */
int alwan_colour_correction_matrix_cheung2004(alwan_scalar *matrix_out,
                                               alwan_scalar const *M_T,
                                               alwan_scalar const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms);

/* Apply colour correction using Cheung 2004 method
 * rgb: input RGB to correct
 * matrix: correction matrix from alwan_colour_correction_matrix_cheung2004
 * terms: must match terms used to compute the matrix
 * rgb_out: corrected RGB output
 * Returns ALWAN_OK on success */
int alwan_colour_correct_cheung2004(alwan_rgb *rgb_out, alwan_rgb const *rgb,
                                     alwan_scalar const *matrix, alwan_poly_cheung_terms terms);

/* Compute colour correction matrix using Finlayson 2015 method
 * M_T: test (measured) RGB values, Nx3 array (row-major)
 * M_R: reference RGB values, Nx3 array (row-major)
 * num_samples: number of color samples (N)
 * degree: polynomial degree (1-4)
 * root_poly: if true, use root-polynomial expansion
 * matrix_out: receives the correction matrix
 * matrix_size: receives matrix size
 * Returns ALWAN_OK on success */
int alwan_colour_correction_matrix_finlayson2015(alwan_scalar *matrix_out, int *matrix_size,
                                                  alwan_scalar const *M_T,
                                                  alwan_scalar const *M_R,
                                                  int num_samples, int degree, int root_poly);

/* Apply colour correction using Finlayson 2015 method
 * rgb: input RGB to correct
 * matrix: correction matrix from alwan_colour_correction_matrix_finlayson2015
 * degree: must match degree used to compute the matrix
 * root_poly: must match root_poly used to compute the matrix
 * rgb_out: corrected RGB output
 * Returns ALWAN_OK on success */
int alwan_colour_correct_finlayson2015(alwan_rgb *rgb_out, alwan_rgb const *rgb,
                                        alwan_scalar const *matrix, int degree, int root_poly);

/* White balance multipliers from neutral gray measurement
 * Given a measured RGB value that should be neutral gray,
 * computes the multipliers to normalize it.
 * measured_gray: measured RGB of a neutral gray target
 * multipliers_out: receives RGB multipliers (normalized so min = 1.0)
 * Returns ALWAN_OK on success */
int alwan_white_balance_from_gray(alwan_rgb *multipliers_out, alwan_rgb const *measured_gray);

/* Apply white balance multipliers
 * rgb: input RGB
 * multipliers: RGB multipliers from alwan_white_balance_from_gray
 * rgb_out: white-balanced RGB output
 * Returns ALWAN_OK on success */
int alwan_white_balance_apply(alwan_rgb *rgb_out, alwan_rgb const *rgb,
                               alwan_rgb const *multipliers);

/* ================================================================
 * Optical Phenomena - Rayleigh Scattering
 * Reference: Bodhaine et al. (1999), colour-science implementation
 * ================================================================ */

/* Atmospheric parameters for Rayleigh calculations */
typedef struct {
    alwan_scalar CO2_concentration;  /* CO2 concentration in ppm (default: 300) */
    alwan_scalar temperature;        /* Temperature in Kelvin (default: 288.15) */
    alwan_scalar pressure;           /* Pressure in Pascals (default: 101325) */
    alwan_scalar latitude;           /* Latitude in degrees (default: 0) */
    alwan_scalar altitude;           /* Altitude in meters (default: 0) */
} alwan_atmosphere_params;

/* Initialize atmosphere parameters with defaults
 * CO2: 300 ppm, T: 288.15 K, P: 101325 Pa, lat: 0, alt: 0 */
void alwan_atmosphere_params_default(alwan_atmosphere_params *params);

/* Rayleigh scattering cross section per molecule (sigma)
 * Van de Hulst (1957) method with Bodhaine et al. (1999) corrections
 * wavelength_nm: wavelength in nanometers
 * params: atmospheric parameters (use NULL for defaults, only CO2 and temp used)
 * Returns: cross section in cm^2 */
alwan_scalar alwan_rayleigh_cross_section(alwan_scalar wavelength_nm,
                                           alwan_atmosphere_params const *params);

/* Rayleigh optical depth through atmosphere
 * Computes tau_R(lambda) using Bodhaine et al. (1999) method
 * wavelength_nm: wavelength in nanometers
 * params: atmospheric parameters (use NULL for defaults)
 * Returns: optical depth (dimensionless) */
alwan_scalar alwan_rayleigh_optical_depth(alwan_scalar wavelength_nm,
                                           alwan_atmosphere_params const *params);

/* Rayleigh scattering spectral distribution
 * Fills an array with Rayleigh optical depth values across a wavelength range.
 * wavelength_start: start wavelength in nm
 * wavelength_end: end wavelength in nm
 * wavelength_step: wavelength step in nm
 * params: atmospheric parameters (use NULL for defaults)
 * out: output array (must be large enough for (end-start)/step + 1 values)
 * out_count: receives the number of values written
 * Returns 0 on success, -1 on error */
int alwan_rayleigh_spd(alwan_scalar wavelength_start, alwan_scalar wavelength_end,
                        alwan_scalar wavelength_step,
                        alwan_atmosphere_params const *params,
                        alwan_scalar *out, int *out_count);

/* ================================================================
 * ACES Fixed Functions (RRT Components)
 * Reference: OpenColorIO, Academy Color Encoding System
 * ================================================================ */

/* ACES RedMod03 - Red channel modification (RRT v0.3) */
int alwan_aces_redmod03(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/* ACES RedMod10 - Red channel modification (RRT v1.0) */
int alwan_aces_redmod10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/* ACES Glow03 - Flare/glow effect (RRT v0.3) */
int alwan_aces_glow03(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/* ACES Glow10 - Flare/glow effect (RRT v1.0) */
int alwan_aces_glow10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/* ACES DarkToDim10 - Surround compensation (RRT v1.0) */
int alwan_aces_dark_to_dim10(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/* ACES GamutComp13 parameters */
typedef struct {
    alwan_scalar lim_cyan;     /* Cyan limit (default: 1.147) */
    alwan_scalar lim_magenta;  /* Magenta limit (default: 1.264) */
    alwan_scalar lim_yellow;   /* Yellow limit (default: 1.312) */
    alwan_scalar thr_cyan;     /* Cyan threshold (default: 0.815) */
    alwan_scalar thr_magenta;  /* Magenta threshold (default: 0.803) */
    alwan_scalar thr_yellow;   /* Yellow threshold (default: 0.880) */
    alwan_scalar power;        /* Compression power (default: 1.2) */
} alwan_aces_gamut_comp13_params;

/* Initialize GamutComp13 parameters with ACES 1.3 defaults */
void alwan_aces_gamut_comp13_params_default(alwan_aces_gamut_comp13_params *params);

/* ACES GamutComp13 - Gamut compression (ACES 1.3) */
int alwan_aces_gamut_comp13(alwan_rgb *rgb_out,
                            alwan_rgb const *rgb_in,
                            alwan_aces_gamut_comp13_params const *params);

/* ACES GamutComp13 Inverse - Gamut decompression (ACES 1.3) */
int alwan_aces_gamut_comp13_inv(alwan_rgb *rgb_out,
                                 alwan_rgb const *rgb_in,
                                 alwan_aces_gamut_comp13_params const *params);

/* ================================================================
 * Blue Light Artifact Fix (Neon Suppression) LMT
 *
 * A legacy LMT that fixes artifacts in bright saturated blues and reds
 * from cameras whose gamuts extend outside AP0 primaries.
 *
 * Note: Superseded by Reference Gamut Compression (alwan_aces_gamut_comp13)
 * in ACES 1.3+. Provided for compatibility with older workflows.
 *
 * Input/Output: AP0 linear (ACES2065-1)
 * ================================================================ */

/**
 * @brief Apply Blue Light Artifact Fix (Neon Suppression) LMT
 * @param rgb_out Output AP0 linear color with blue light fix applied
 * @param rgb_in Input AP0 linear color
 * @return ALWAN_OK on success
 */
int alwan_aces_blue_light_fix(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief Apply inverse Blue Light Artifact Fix
 * @param rgb_out Output AP0 linear color (original)
 * @param rgb_in Input AP0 linear color (with fix applied)
 * @return ALWAN_OK on success
 */
int alwan_aces_blue_light_fix_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief Inverse of Glow03 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
int alwan_aces_glow03_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief Inverse of Glow10 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
int alwan_aces_glow10_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief Inverse of RedMod03 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
int alwan_aces_redmod03_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief Inverse of RedMod10 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
int alwan_aces_redmod10_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief ACES 1.0 Look LMT - emulates ACES 1.0 look with ACES 1.0.3+ RRT
 *
 * This LMT applies the difference between ACES 1.0 and ACES 1.0.3+ rendering.
 * Use this when you want images processed with ACES 1.0.3+ (or later) RRT
 * to have the look of ACES 1.0.
 *
 * @param rgb_out Output AP1 linear color with ACES 1.0 look applied
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
int alwan_aces_look_1_0(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief Inverse of ACES 1.0 Look LMT
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color (with ACES 1.0 look)
 * @return ALWAN_OK on success
 */
int alwan_aces_look_1_0_inv(alwan_rgb *rgb_out, alwan_rgb const *rgb_in);

/**
 * @brief Parametric LMT parameters (CDL-style color grading)
 *
 * Applies: out = (in * slope + offset) ^ power, then saturation adjustment.
 * All parameters default to neutral (slope=1, offset=0, power=1, saturation=1).
 */
typedef struct {
    alwan_scalar slope[3];      /* Per-channel gain (r, g, b). Default: 1.0 */
    alwan_scalar offset[3];     /* Per-channel offset (r, g, b). Default: 0.0 */
    alwan_scalar power[3];      /* Per-channel gamma (r, g, b). Default: 1.0 */
    alwan_scalar saturation;    /* Global saturation. Default: 1.0 */
} alwan_aces_lmt_params;

/**
 * @brief Initialize LMT params to neutral (identity transform)
 * @param params Output params struct
 */
void alwan_aces_lmt_params_init(alwan_aces_lmt_params *params);

/**
 * @brief Apply parametric LMT (CDL-style color grading)
 *
 * Applies slope, offset, power (SOP) per channel, then saturation adjustment.
 * Input and output are in AP1 (ACEScg) linear space.
 *
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @param params LMT parameters
 * @return ALWAN_OK on success
 */
int alwan_aces_lmt_apply(alwan_rgb *rgb_out,
                         alwan_rgb const *rgb_in,
                         alwan_aces_lmt_params const *params);

/* ================================================================
 * ACES 1.x Output Transforms (RRT + ODT)
 * Reference: Academy Color Encoding System v1.3
 * Note: These implement the complete RRT+ODT pipeline for ACES 1.x
 * ================================================================ */

/**
 * ACES 1.x Output Transform presets
 * Input: ACES2065-1 (AP0) linear
 * Output: Display-encoded RGB
 */
typedef enum {
    /* SDR Displays */
    ALWAN_ACES1_OUT_REC709_100NIT,        /* Rec.709, 100 nits, BT.1886 */
    ALWAN_ACES1_OUT_SRGB_100NIT,          /* sRGB, 100 nits, sRGB EOTF */
    ALWAN_ACES1_OUT_SRGB_D60_100NIT,      /* sRGB (D60 sim), 100 nits */

    /* P3 Displays */
    ALWAN_ACES1_OUT_P3DCI_48NIT,          /* P3-DCI, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D60_48NIT,          /* P3-D60, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D65_48NIT,          /* P3-D65, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D65_100NIT,         /* P3-D65 (Display P3), 100 nits */

    /* Rec.2020 Displays */
    ALWAN_ACES1_OUT_REC2020_100NIT,       /* Rec.2020, 100 nits, BT.1886 */
    ALWAN_ACES1_OUT_REC2020_1000NIT_PQ,   /* Rec.2020, 1000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_2000NIT_PQ,   /* Rec.2020, 2000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_4000NIT_PQ,   /* Rec.2020, 4000 nits, PQ */

    /* Cinema */
    ALWAN_ACES1_OUT_DCDM_48NIT,           /* DCDM X'Y'Z', 48 nits, Gamma 2.6 */

    ALWAN_ACES1_OUT_COUNT
} alwan_aces1_output;

/**
 * ACES 1.x Output Transform (RRT + ODT)
 *
 * Implements the complete ACES 1.3 rendering pipeline:
 *   1. RRT (Reference Rendering Transform) - tone mapping and color processing
 *   2. ODT (Output Device Transform) - display-specific conversion
 *
 * @param rgb_out  Output RGB, display-encoded
 * @param rgb_in   Input RGB in ACES2065-1 (AP0 linear), scene-referred
 * @param output   Output transform preset (display configuration)
 * @return         ALWAN_OK on success
 */
int alwan_aces1_output_transform(alwan_rgb *rgb_out,
                                  alwan_rgb const *rgb_in,
                                  alwan_aces1_output output);

/**
 * ACES 1.x Output Transform (Inverse)
 *
 * Inverse of the ACES 1.x output transform for round-trip workflows.
 *
 * @param rgb_out  Output RGB in ACES2065-1 (AP0 linear)
 * @param rgb_in   Input RGB, display-encoded
 * @param output   Output transform preset (display configuration)
 * @return         ALWAN_OK on success
 */
int alwan_aces1_output_transform_inv(alwan_rgb *rgb_out,
                                      alwan_rgb const *rgb_in,
                                      alwan_aces1_output output);

/* Rec.2100 Surround adjustment */
int alwan_rec2100_surround(alwan_rgb *rgb_out, alwan_rgb const *rgb_in, alwan_scalar gamma);

/* ================================================================
 * ACES 2.0 Components
 * ================================================================ */

/* ACES TonescaleCompress20 - Tonescale compression (ACES 2.0) */
int alwan_aces_tonescale_compress20(alwan_rgb *rgb_out,
                                     alwan_rgb const *rgb_in,
                                     alwan_scalar peak_luminance);

/* ACES RGB to JMh20 encoding primaries */
typedef struct {
    alwan_scalar red_x, red_y;
    alwan_scalar green_x, green_y;
    alwan_scalar blue_x, blue_y;
    alwan_scalar white_x, white_y;
} alwan_aces_primaries;

/* Initialize primaries with AP1 defaults */
void alwan_aces_primaries_ap1_default(alwan_aces_primaries *primaries);

/* ACES RGB to JMh20 - Convert to color appearance coordinates (ACES 2.0) */
int alwan_aces_rgb_to_jmh20(alwan_vec3 *jmh_out,
                            alwan_rgb const *rgb_in,
                            alwan_aces_primaries const *primaries);

/* ACES JMh to RGB20 - Convert from color appearance coordinates (ACES 2.0) */
int alwan_aces_jmh_to_rgb20(alwan_rgb *rgb_out,
                            alwan_vec3 const *jmh_in,
                            alwan_aces_primaries const *primaries);

/* ACES GamutCompress20 - Gamut compression in JMh space (ACES 2.0)
 *
 * Compresses colors to fit within the limit gamut while preserving hue.
 * This operates on JMh values (output of alwan_aces_rgb_to_jmh20).
 *
 * jmh_in:        Input JMh values (J=lightness, M=colorfulness, h=hue in degrees)
 * peak_luminance: Peak display luminance in nits (1-10000)
 * limit_primaries: Display gamut primaries (use AP1 for wide gamut)
 * jmh_out:       Output compressed JMh values
 *
 * Note: For RGB-to-RGB gamut compression, chain with rgb_to_jmh20/jmh_to_rgb20.
 */
int alwan_aces_gamut_compress20(alwan_vec3 *jmh_out,
                                 alwan_vec3 const *jmh_in,
                                 alwan_scalar peak_luminance,
                                 alwan_aces_primaries const *limit_primaries);

/**
 * ACES 2.0: Gamut Compression (Inverse)
 * Expands JMh colors from display gamut back to scene-referred gamut.
 * Parameters same as forward function.
 */
int alwan_aces_gamut_compress20_inv(alwan_vec3 *jmh_out,
                                     alwan_vec3 const *jmh_in,
                                     alwan_scalar peak_luminance,
                                     alwan_aces_primaries const *limit_primaries);

/* ================================================================
 * ACES 2.0 Output Transform (Unified API)
 * ================================================================ */

/**
 * ACES 2.0 Output Transform presets.
 * Each preset defines: limiting primaries, peak luminance, and display EOTF.
 */
typedef enum {
    /* SDR Displays (100 nits) */
    ALWAN_ACES2_OUT_REC709_100NIT_BT1886,    /* Rec.709, 100 nits, BT.1886 EOTF */
    ALWAN_ACES2_OUT_SRGB_100NIT,              /* sRGB, 100 nits, sRGB EOTF */
    ALWAN_ACES2_OUT_P3D65_100NIT_SRGB,        /* Display P3, 100 nits, sRGB piecewise */
    ALWAN_ACES2_OUT_P3D65_100NIT_G22,         /* Display P3, 100 nits, Gamma 2.2 */

    /* HDR Displays (PQ / ST.2084) */
    ALWAN_ACES2_OUT_P3D65_1000NIT_PQ,         /* Display P3, 1000 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_500NIT_PQ,        /* Rec.2100, 500 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_1000NIT_PQ,       /* Rec.2100, 1000 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_2000NIT_PQ,       /* Rec.2100, 2000 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_4000NIT_PQ,       /* Rec.2100, 4000 nits, PQ */

    /* HDR Displays (HLG) */
    ALWAN_ACES2_OUT_REC2100_1000NIT_HLG,      /* Rec.2100, 1000 nits, HLG */

    /* Cinema */
    ALWAN_ACES2_OUT_DCDM_48NIT,               /* DCDM X'Y'Z', 48 nits, Gamma 2.6 */
    ALWAN_ACES2_OUT_P3DCI_48NIT,              /* P3-DCI, 48 nits, Gamma 2.6 */

    ALWAN_ACES2_OUT_COUNT
} alwan_aces2_output;

/**
 * ACES 2.0 Output Transform (Unified API)
 *
 * Complete ACES 2.0 rendering pipeline:
 *   1. Input (AP1 linear) -> JMh
 *   2. Tonescale + Chroma compression
 *   3. Gamut compression to limiting primaries
 *   4. JMh -> RGB (limiting primaries)
 *   5. Chromatic adaptation (D60 -> D65 if needed)
 *   6. Display encoding (EOTF)
 *
 * @param rgb_out  Output RGB, display-encoded [0,1]
 * @param rgb_in   Input RGB in ACEScg (AP1 linear), scene-referred
 * @param output   Output transform preset (display configuration)
 * @return         ALWAN_OK on success
 */
int alwan_aces2_output_transform(alwan_rgb *rgb_out,
                                  alwan_rgb const *rgb_in,
                                  alwan_aces2_output output);

/**
 * ACES 2.0 Output Transform (Inverse)
 *
 * Inverse of the output transform for round-trip workflows.
 * Converts display-encoded RGB back to ACEScg (AP1 linear).
 *
 * @param rgb_out  Output RGB in ACEScg (AP1 linear), scene-referred
 * @param rgb_in   Input RGB, display-encoded [0,1]
 * @param output   Output transform preset (display configuration)
 * @return         ALWAN_OK on success
 */
int alwan_aces2_output_transform_inv(alwan_rgb *rgb_out,
                                      alwan_rgb const *rgb_in,
                                      alwan_aces2_output output);

/**
 * ACES 2.0 Output Transform (Custom)
 *
 * Custom output transform with user-specified parameters.
 *
 * @param rgb_out         Output RGB, display-encoded [0,1]
 * @param rgb_in          Input RGB in ACEScg (AP1 linear)
 * @param peak_luminance  Peak display luminance in nits (1-10000)
 * @param limit_primaries Display gamut primaries
 * @param eotf            Display EOTF (e.g., ALWAN_TF_BT1886, ALWAN_TF_PQ)
 * @return                ALWAN_OK on success
 */
int alwan_aces2_output_transform_custom(alwan_rgb *rgb_out,
                                         alwan_rgb const *rgb_in,
                                         alwan_scalar peak_luminance,
                                         alwan_aces_primaries const *limit_primaries,
                                         alwan_transfer_function eotf);

#ifdef __cplusplus
}
#endif

#endif /* ALWAN_H */
