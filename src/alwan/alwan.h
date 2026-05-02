/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ============================================================================
 * Parameter convention (v2.0 â€” enforced by tools/api_convention_survey.py)
 * ============================================================================
 *
 *   1. ctx (alwan_ctx *) is LAST when present, never first or middle.
 *   2. Each *_stride immediately follows the buffer it strides (memcpy order).
 *   3. Output buffers come BEFORE input buffers; never an out after an in
 *      inside the buffer-stride block.
 *   4. count / width / height come AFTER the buffer-stride block.
 *   5. No ctx for pure value-typed math (*_v) functions.
 *
 * Canonical signatures:
 *
 *   fn(out, out_stride, in, in_stride, count, [extras]..., [ctx])
 *   fn(o0, out_stride, o1, o2, i0, in_stride, i1, i2, count, [extras]..., [ctx])
 *   fn(dst, dst_row_stride, src, src_row_stride, width, height, [extras]...)
 *   fn(out, in1, in1_stride, in2, in2_stride, count, [extras]...)         // batch
 *   fn_ex(out, out_stride, in, in_stride, count, out_fmt, in_fmt, [extras]...)
 *
 * Buffer naming (must match survey pattern):
 *   - Use: out, in, src, dst, buf, o0..o2, i0..i2, out0..out2, in0..in2,
 *     *_in, *_out, *_buf, *_chN.
 *   - Avoid bare names like rgb_data, linear, encoded â€” suffix with _in/_out.
 *   - Value-input pointers (alwan_xyz/alwan_rgb/alwan_mat3x3 const *) are
 *     KNOBS, not buffers â€” they belong in the [extras] tail.
 *
 * Extras tail ordering:
 *   pixel formats (out_fmt, in_fmt), then matrices / white points / structs,
 *   then enums, then scalar tuning params, then ctx.
 *
 * See CONTRIBUTING.md for rationale and examples.
 * ============================================================================
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
    ALWAN_E_NOMEM  = -4,  /* Memory allocation failed */
    ALWAN_E_DIVZERO = -5  /* Division by zero would occur */
} alwan_status;

/* ----------------------------------------------------------------
 * Pixel format for typed map functions
 * ---------------------------------------------------------------- */
typedef enum {
    ALWAN_PIXEL_U8  = 0,  /* uint8_t  [0,255]   -> [0.0, 1.0] */
    ALWAN_PIXEL_U16 = 1,  /* uint16_t [0,65535]  -> [0.0, 1.0] */
    ALWAN_PIXEL_F32 = 2,  /* alwan_f32                              */
    ALWAN_PIXEL_F64 = 3,  /* alwan_f64                             */
    ALWAN_PIXEL_F16 = 4   /* IEEE 754 binary16 (half-alwan_f32)     */
} alwan_pixel_format;

/* Video signal range */
typedef enum {
    ALWAN_VIDEO_RANGE_FULL   = 0,  /* 0 to (2^N - 1) */
    ALWAN_VIDEO_RANGE_NARROW = 1   /* 16*2^(N-8) to 235*2^(N-8) (SMPTE) */
} alwan_video_range;

/* ----------------------------------------------------------------
 * Data semantic classification (Color Interop Forum)
 *
 * Distinguishes color data from non-color data (normals, displacement,
 * masks) to prevent inappropriate color management.
 * ---------------------------------------------------------------- */
typedef enum {
    ALWAN_DATA_COLOR     = 0,  /* Color data â€” apply color management */
    ALWAN_DATA_NON_COLOR = 1,  /* Non-color (normals, masks, displacement) â€” pass through */
    ALWAN_DATA_UNKNOWN   = 2   /* Unknown â€” application should decide */
} alwan_data_semantic;

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
    char const *runtime_data_root;    /* Reserved: runtime data loading is not implemented (planned for alwan 3.0.0). Field is ignored. */
    uint32_t flags;                   /* Reserved for future use (must be 0) */
} alwan_config;

/* Create a new context with optional configuration */
alwan_ctx *alwan_create(alwan_config const *cfg);

/* Destroy context and release all resources */
void alwan_destroy(alwan_ctx *ctx);

/* ACES tone curve interpolation method */
typedef enum {
    ALWAN_ACES_INTERP_BSPLINE = 0,  /* Quadratic B-spline (Academy CTL reference, default) */
    ALWAN_ACES_INTERP_HERMITE = 1,  /* Legacy piecewise Hermite approximation */
    ALWAN_ACES_INTERP_OCIO = 2      /* OCIO GradingRGBCurve (monotone cubic Hermite, pixel-exact OCIO match) */
} alwan_aces_interp;

/* Set the ACES tone curve interpolation method.
 * Default: ALWAN_ACES_INTERP_BSPLINE.
 * Use ALWAN_ACES_INTERP_OCIO for pixel-exact match with OpenColorIO. */
void alwan_set_aces_interp(alwan_aces_interp method);
alwan_aces_interp alwan_get_aces_interp(void);

/* ----------------------------------------------------------------
 * Math Types & Semantic Color Types
 * ---------------------------------------------------------------- */
#include "alwan_types.h"

/* ----------------------------------------------------------------
 * Data Loading
 *
 * NOTE: Only embedded mode (ALWAN_EMBED_DATA=1, the default) is supported.
 * Runtime mode (ALWAN_EMBED_DATA=0) is NOT implemented. It is planned for
 * alwan 3.0.0. Attempting to build with ALWAN_EMBED_DATA=0 will produce
 * a compile-time error in alwan_data.c.
 * ---------------------------------------------------------------- */

 /* Standard illuminant xy chromaticity data getters
 * Each returns 2 values: x, y chromaticity coordinates
 * In embedded mode: returns pointer to static data (no deallocation needed) */

/* Illuminant A (incandescent tungsten) */
int alwan_data_get_illuminant_a_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_a_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D50 (horizon daylight) */
int alwan_data_get_illuminant_d50_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_d50_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D55 (mid-morning daylight) */
int alwan_data_get_illuminant_d55_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_d55_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D60 (daylight) */
int alwan_data_get_illuminant_d60_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_d60_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D65 (noon daylight) */
int alwan_data_get_illuminant_d65_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_d65_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant E (equal energy) */
int alwan_data_get_illuminant_e_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_e_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant B (direct sunlight) */
int alwan_data_get_illuminant_b_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_b_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant C (average daylight) */
int alwan_data_get_illuminant_c_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_c_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D75 (daylight 7500K) */
int alwan_data_get_illuminant_d75_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_illuminant_d75_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Get sRGB primaries (6 values: rx, ry, gx, gy, bx, by) */
int alwan_data_get_srgb_primaries_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
int alwan_data_get_srgb_primaries_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* NOTE: alwan_data_free_f64/f32 are declared only when ALWAN_EMBED_DATA=0.
 * Runtime mode is not implemented; this block exists for future use (alwan 3.0.0). */
#if !ALWAN_EMBED_DATA
void alwan_data_free_f64(alwan_f64 *data, alwan_ctx *ctx);
void alwan_data_free_f32(alwan_f32 *data, alwan_ctx *ctx);
#endif

/* ----------------------------------------------------------------
 * Math Operations
 * ---------------------------------------------------------------- */

/* Multiply two 3x3 matrices: out = a * b */
void alwan_mat3_mul_f32(alwan_mat3x3_f32 *out, alwan_mat3x3_f32 const *a, alwan_mat3x3_f32 const *b);
void alwan_mat3_mul_f64(alwan_mat3x3_f64 *out, alwan_mat3x3_f64 const *a, alwan_mat3x3_f64 const *b);

/* Invert a 3x3 matrix using partial-pivot Gaussian elimination
 * Returns ALWAN_OK on success, ALWAN_E_RANGE if matrix is singular */
int alwan_mat3_inv_f32(alwan_mat3x3_f32 *out, alwan_mat3x3_f32 const *m);
int alwan_mat3_inv_f64(alwan_mat3x3_f64 *out, alwan_mat3x3_f64 const *m);

/* Multiply matrix by vector: out = m * v */
void alwan_mat3_mulv_f32(alwan_vec3_f32 *out, alwan_mat3x3_f32 const *m, alwan_vec3_f32 const *v);
void alwan_mat3_mulv_f64(alwan_vec3_f64 *out, alwan_mat3x3_f64 const *m, alwan_vec3_f64 const *v);

/* Create identity matrix */
void alwan_mat3_identity_f32(alwan_mat3x3_f32 *out);
void alwan_mat3_identity_f64(alwan_mat3x3_f64 *out);

/* Compute the determinant of a 3x3 matrix */
alwan_f32  alwan_mat3_det_f32(alwan_mat3x3_f32 const *m);
alwan_f64 alwan_mat3_det_f64(alwan_mat3x3_f64 const *m);

/* Mapmatrix-vector multiplication: out[i] = m * in[i]
 * Transforms array of 3D vectors by the same matrix
 * vec_out: output vectors (stride out_stride between consecutive vectors)
 * matrix: transformation matrix (applied to all vectors)
 * vec_in: input vectors (stride in_stride between consecutive vectors)
 * count: number of vectors to transform
 * in_stride: input stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * out_stride: output stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * Returns ALWAN_OK on success */
int alwan_mat3_transform_f32_map_interleave(alwan_f32 *vec_out, size_t out_stride, alwan_f32 const *vec_in, size_t in_stride, size_t count, alwan_mat3x3_f32 const *matrix);
int alwan_mat3_transform_f64_map_interleave(alwan_f64 *vec_out, size_t out_stride, alwan_f64 const *vec_in, size_t in_stride, size_t count, alwan_mat3x3_f64 const *matrix);

/* Typed mat3 transform: accepts void* buffers with pixel format */
int alwan_mat3_transform_map_interleave_ex(void *vec_out, size_t out_stride, void const *vec_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_mat3x3_f64 const *matrix, alwan_pixel_format in_fmt);

/* ----------------------------------------------------------------
 * Collect / Scatter utilities (typed <-> alwan_f64)
 * ---------------------------------------------------------------- */

/* Collect: load typed 3-channel pixels into alwan_f64 triplets */
int alwan_collect3_f64(alwan_f64 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt);
int alwan_collect3_f32(alwan_f32 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt);

/* Scatter: store alwan_f64 triplets into typed 3-channel pixels */
int alwan_scatter3_f64(void *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt);
int alwan_scatter3_f32(void *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt);

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
    ALWAN_RGB_SPACE_LINEAR_REC709,          /* Linear Rec.709 (no transfer function) */
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
    ALWAN_RGB_SPACE_GAMMA18_REC709,     /* Rec.709 primaries + gamma 1.8 OETF */

    /* ColorInterop Display Color Spaces (Section 2.1) */
    ALWAN_RGB_SPACE_REC1886_REC709,     /* Rec.709 primaries + BT.1886 EOTF (gamma 2.4) */
    ALWAN_RGB_SPACE_REC2100_PQ,         /* Rec.2020 primaries + PQ (SMPTE ST.2084) */
    ALWAN_RGB_SPACE_REC2100_HLG,        /* Rec.2020 primaries + HLG (BT.2100) */
    ALWAN_RGB_SPACE_DISPLAY_P3_HDR,     /* Display P3 primaries + PQ (SMPTE ST.2084) */

    ALWAN_RGB_SPACE_COUNT               /* Sentinel: number of enum values */
} alwan_rgb_space;

/* Backward compatibility alias */
#define ALWAN_RGB_SPACE_LINEAR_SRGB ALWAN_RGB_SPACE_LINEAR_REC709

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
int alwan_data_get_illuminant_xy_f64(alwan_f64 **data, size_t *count, alwan_illuminant illuminant, alwan_ctx *ctx);
int alwan_data_get_illuminant_xy_f32(alwan_f32 **data, size_t *count, alwan_illuminant illuminant, alwan_ctx *ctx);

/* View transform identifiers */
typedef enum {
    ALWAN_VIEW_ACES_REC709,        /* ACES RRT + ODT Rec.709 */
    ALWAN_VIEW_AGX,                /* AgX base (full pipeline with inset/outset matrices) */
    ALWAN_VIEW_AGX_PUNCHY,         /* AgX punchy variant (high contrast + saturation) */
    ALWAN_VIEW_AGX_GOLDEN,         /* AgX golden variant (warm highlights, cool shadows) */
    ALWAN_VIEW_BT2446A_HDR_TO_SDR, /* BT.2446 Method A: HDR to SDR tone mapping */
    ALWAN_VIEW_BT2446A_SDR_TO_HDR,  /* BT.2446 Method A: SDR to HDR inverse mapping */
    ALWAN_VIEW_KHRONOS_PBR_NEUTRAL, /* Khronos PBR Neutral tone mapping (glTF/WebGL) */
    ALWAN_VIEW_REINHARD_EXT,        /* Reinhard Extended (luminance-based, Reinhard 2002) */
    ALWAN_VIEW_UCHIMURA,            /* Uchimura / Gran Turismo (parametric S-curve) */
    ALWAN_VIEW_LOTTES,              /* Lottes / AMD Cauldron (parametric rational curve) */
    ALWAN_VIEW_TONY_MCMAPFACE,       /* Somewhat Boring Display Transform (Stachowiak 2023) */
    ALWAN_VIEW_BT2446B_SDR_TO_HDR,   /* BT.2446 Method B: SDR to HDR up-conversion */
    ALWAN_VIEW_BT2446C_HDR_TO_SDR,   /* BT.2446 Method C: HDR to SDR (quantization-aware) */
    ALWAN_VIEW_BT2390_HDR_TO_SDR,    /* BT.2390 EETF: HDR to SDR (Hermite spline) */
    ALWAN_VIEW_REINHARD_CALIBRATED,  /* Reinhard calibrated (key-based, Reinhard 2002) */
    ALWAN_VIEW_EXPOSURE              /* Exposure-based with shoulder compression */
} alwan_view_transform;

/* Alpha handling mode for RGBA image conversion */
typedef enum {
    ALWAN_ALPHA_STRAIGHT     = 0, /* Alpha is independent; pass through unchanged */
    ALWAN_ALPHA_PREMULTIPLIED = 1  /* RGB is premultiplied by alpha; unpremultiply before
                                    * color conversion, repremultiply after */
} alwan_alpha_mode;

/* RGB space descriptor with primaries, white point, and transfer functions (f32) */
typedef struct {
    alwan_f32 primaries_xy[6];          /* rx, ry, gx, gy, bx, by in CIE xy chromaticity */
    alwan_f32 white_xy[2];              /* wx, wy in CIE xy chromaticity */
    alwan_transfer_function oetf;   /* OETF (Opto-Electronic Transfer Function), use ALWAN_TF_LINEAR for none */
    alwan_transfer_function eotf;   /* EOTF (Electro-Optical Transfer Function), use ALWAN_TF_LINEAR for none */
    alwan_mat3x3_f32 rgb_to_xyz;   /* precomputed RGB->XYZ NPM, valid when has_matrices != 0 */
    alwan_mat3x3_f32 xyz_to_rgb;   /* precomputed XYZ->RGB inverse NPM, valid when has_matrices != 0 */
    int has_matrices;               /* non-zero if rgb_to_xyz/xyz_to_rgb are valid */
} alwan_rgb_space_desc_f32;

/* RGB space descriptor with primaries, white point, and transfer functions (f64) */
typedef struct {
    alwan_f64 primaries_xy[6];         /* rx, ry, gx, gy, bx, by in CIE xy chromaticity */
    alwan_f64 white_xy[2];             /* wx, wy in CIE xy chromaticity */
    alwan_transfer_function oetf;   /* OETF (Opto-Electronic Transfer Function), use ALWAN_TF_LINEAR for none */
    alwan_transfer_function eotf;   /* EOTF (Electro-Optical Transfer Function), use ALWAN_TF_LINEAR for none */
    alwan_mat3x3_f64 rgb_to_xyz;   /* precomputed RGB->XYZ NPM, valid when has_matrices != 0 */
    alwan_mat3x3_f64 xyz_to_rgb;   /* precomputed XYZ->RGB inverse NPM, valid when has_matrices != 0 */
    int has_matrices;               /* non-zero if rgb_to_xyz/xyz_to_rgb are valid */
} alwan_rgb_space_desc_f64;

/* Derive RGB<->XYZ conversion matrices from primaries and white point
 * Returns ALWAN_OK on success, ALWAN_E_RANGE if primaries/white form singular matrix */
int alwan_rgb_derive_matrices_f64(alwan_mat3x3_f64 *rgb_to_xyz,
                               alwan_mat3x3_f64 *xyz_to_rgb,
                               alwan_rgb_space_desc_f64 const *desc);
int alwan_rgb_derive_matrices_f32(alwan_mat3x3_f32 *rgb_to_xyz,
                               alwan_mat3x3_f32 *xyz_to_rgb,
                               alwan_rgb_space_desc_f32 const *desc);

/* Convert linear RGB to XYZ for a given color space
 * space: RGB color space descriptor (primaries and white point)
 * rgb: input linear RGB color
 * xyz: output XYZ color
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_to_xyz_f32(alwan_xyz_f32 *xyz,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_rgb_f32 const *rgb);
int alwan_rgb_to_xyz_f64(alwan_xyz_f64 *xyz,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_rgb_f64 const *rgb);

/* Convert XYZ to linear RGB for a given color space
 * space: RGB color space descriptor (primaries and white point)
 * xyz: input XYZ color
 * rgb: output linear RGB color (may be out of gamut)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_xyz_to_rgb_f32(alwan_rgb_f32 *rgb,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_xyz_f32 const *xyz);
int alwan_xyz_to_rgb_f64(alwan_rgb_f64 *rgb,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_xyz_f64 const *xyz);

/* Get RGB color space descriptor by enum
 * Loads primaries and white point from data files and populates descriptor
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
int alwan_rgb_get_space_descriptor_f64(alwan_rgb_space_desc_f64 *desc, alwan_rgb_space space, alwan_ctx *ctx);
int alwan_rgb_get_space_descriptor_f32(alwan_rgb_space_desc_f32 *desc, alwan_rgb_space space, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * RGB Color Space Conversion
 * ---------------------------------------------------------------- */

/* Convert RGB color from one color space to another
 * Handles chromatic adaptation if white points differ (using Bradford CAT by default)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_convert_f64(alwan_rgb_f64 *dst_rgb, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_rgb_f64 const *src_rgb, alwan_ctx *ctx);
int alwan_rgb_convert_f32(alwan_rgb_f32 *dst_rgb, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_rgb_f32 const *src_rgb, alwan_ctx *ctx);

/* MapRGB color space conversion for arrays of colors
 * More efficient than calling alwan_rgb_convert_f64 in a loop
 * count: number of RGB triplets to convert
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_rgb_convert_map_interleave_f64(alwan_rgb_f64 *dst_rgb, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_rgb_f64 const *src_rgb, size_t count, alwan_ctx *ctx);
int alwan_rgb_convert_map_interleave_f32(alwan_rgb_f32 *dst_rgb, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_rgb_f32 const *src_rgb, size_t count, alwan_ctx *ctx);

/* Convert a 2D image between RGB color spaces with format conversion.
 * Handles EOTF/OETF, chromatic adaptation (Bradford), and U8/U16/F32/F64.
 * dst/src: pixel buffers (3-channel, tightly packed per pixel)
 * dst_fmt/src_fmt: pixel format (ALWAN_PIXEL_U8, _U16, _F32, _F64)
 * dst_row_stride/src_row_stride: bytes between consecutive rows
 * width/height: image dimensions in pixels
 * ctx: context (required when src and dst white points differ)
 * src_space/dst_space: RGB space descriptors (primaries, white point, TFs)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_image_convert_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_ctx *ctx);
int alwan_image_convert_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_ctx *ctx);

/* Convert a 2D RGBA image between RGB color spaces with format conversion.
 * Same pipeline as alwan_image_convert_f64 but with 4-channel (RGBA) pixels.
 * The alpha channel is preserved through the conversion:
 *   ALWAN_ALPHA_STRAIGHT:      alpha copied unchanged, RGB converted independently
 *   ALWAN_ALPHA_PREMULTIPLIED: RGB unpremultiplied before conversion, repremultiplied after
 * dst/src: pixel buffers (4-channel RGBA, tightly packed per pixel)
 * All other parameters identical to alwan_image_convert_f64.
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_image_convert_rgba_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_alpha_mode alpha_mode, alwan_ctx *ctx);
int alwan_image_convert_rgba_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_alpha_mode alpha_mode, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * sRGB Convenience Functions
 * Direct conversions for sRGB (the most common color space)
 * All assume D65 white point and linear RGB (apply EOTF first if needed)
 * ---------------------------------------------------------------- */

/* sRGB <-> XYZ (D65 white point) */
int alwan_srgb_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_rgb_f32 const *rgb);
int alwan_srgb_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_rgb_f64 const *rgb);
int alwan_xyz_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_xyz_f32 const *xyz);
int alwan_xyz_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_xyz_f64 const *xyz);

/* sRGB <-> Lab (D65 white point) */
int alwan_srgb_to_lab_f32(alwan_lab_f32 *lab, alwan_rgb_f32 const *rgb);
int alwan_srgb_to_lab_f64(alwan_lab_f64 *lab, alwan_rgb_f64 const *rgb);
int alwan_lab_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_lab_f32 const *lab);
int alwan_lab_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_lab_f64 const *lab);

/* sRGB <-> Oklab (D65 assumed by Oklab) */
int alwan_srgb_to_oklab_f32(alwan_oklab_f32 *oklab, alwan_rgb_f32 const *rgb);
int alwan_srgb_to_oklab_f64(alwan_oklab_f64 *oklab, alwan_rgb_f64 const *rgb);
int alwan_oklab_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_oklab_f32 const *oklab);
int alwan_oklab_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_oklab_f64 const *oklab);

/* MapsRGB convenience conversions */
int alwan_srgb_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_srgb_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_xyz_to_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
int alwan_xyz_to_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

int alwan_srgb_to_lab_f32_map_interleave(alwan_f32 *lab_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_srgb_to_lab_f64_map_interleave(alwan_f64 *lab_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_lab_to_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *lab_in, size_t in_stride, size_t count);
int alwan_lab_to_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *lab_in, size_t in_stride, size_t count);

int alwan_srgb_to_oklab_f32_map_interleave(alwan_f32 *oklab_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_srgb_to_oklab_f64_map_interleave(alwan_f64 *oklab_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_oklab_to_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *oklab_in, size_t in_stride, size_t count);
int alwan_oklab_to_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *oklab_in, size_t in_stride, size_t count);

/* Typed sRGB convenience map functions (_ex variants) */
int alwan_srgb_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_srgb_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_lab_to_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_srgb_to_oklab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_oklab_to_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* ----------------------------------------------------------------
 * Direct RGB <-> Perceptual Space Conversions
 * Convenience functions that skip the manual XYZ intermediate step
 * ---------------------------------------------------------------- */

/* RGB <-> Lab (requires white point for Lab) */
int alwan_rgb_to_lab_f32(alwan_lab_f32 *lab,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_rgb_f32 const *rgb,
                         alwan_xyz_f32 const *white_xyz);
int alwan_rgb_to_lab_f64(alwan_lab_f64 *lab,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_rgb_f64 const *rgb,
                         alwan_xyz_f64 const *white_xyz);
int alwan_lab_to_rgb_f32(alwan_rgb_f32 *rgb,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_lab_f32 const *lab,
                         alwan_xyz_f32 const *white_xyz);
int alwan_lab_to_rgb_f64(alwan_rgb_f64 *rgb,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_lab_f64 const *lab,
                         alwan_xyz_f64 const *white_xyz);

/* RGB <-> Luv (requires white point for Luv) */
int alwan_rgb_to_luv_f32(alwan_luv_f32 *luv,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_rgb_f32 const *rgb,
                         alwan_xyz_f32 const *white_xyz);
int alwan_rgb_to_luv_f64(alwan_luv_f64 *luv,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_rgb_f64 const *rgb,
                         alwan_xyz_f64 const *white_xyz);
int alwan_luv_to_rgb_f32(alwan_rgb_f32 *rgb,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_luv_f32 const *luv,
                         alwan_xyz_f32 const *white_xyz);
int alwan_luv_to_rgb_f64(alwan_rgb_f64 *rgb,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_luv_f64 const *luv,
                         alwan_xyz_f64 const *white_xyz);

/* RGB <-> Oklab (Oklab assumes D65, handles chromatic adaptation if needed) */
int alwan_rgb_to_oklab_f32(alwan_oklab_f32 *oklab, alwan_rgb_space_desc_f32 const *space, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_oklab_f64(alwan_oklab_f64 *oklab, alwan_rgb_space_desc_f64 const *space, alwan_rgb_f64 const *rgb);
int alwan_oklab_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_rgb_space_desc_f32 const *space, alwan_oklab_f32 const *oklab);
int alwan_oklab_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_rgb_space_desc_f64 const *space, alwan_oklab_f64 const *oklab);

/* RGB <-> Oklch (cylindrical Oklab) */
int alwan_rgb_to_oklch_f32(alwan_oklch_f32 *oklch, alwan_rgb_space_desc_f32 const *space, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_oklch_f64(alwan_oklch_f64 *oklch, alwan_rgb_space_desc_f64 const *space, alwan_rgb_f64 const *rgb);
int alwan_oklch_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_rgb_space_desc_f32 const *space, alwan_oklch_f32 const *oklch);
int alwan_oklch_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_rgb_space_desc_f64 const *space, alwan_oklch_f64 const *oklch);

/* ----------------------------------------------------------------
 * Direct XYZ <-> Cylindrical Conversions
 * Skip the cartesian intermediate step
 * ---------------------------------------------------------------- */

/* XYZ <-> LCh(ab) (cylindrical Lab) */
void alwan_xyz_to_lch_f32(alwan_lch_f32 *lch, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *white_xyz);
void alwan_xyz_to_lch_f64(alwan_lch_f64 *lch, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *white_xyz);
void alwan_lch_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_lch_f32 const *lch, alwan_xyz_f32 const *white_xyz);
void alwan_lch_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_lch_f64 const *lch, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> LCh(uv) (cylindrical Luv) */
void alwan_xyz_to_lchuv_f32(alwan_lchuv_f32 *lchuv, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *white_xyz);
void alwan_xyz_to_lchuv_f64(alwan_lchuv_f64 *lchuv, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *white_xyz);
void alwan_lchuv_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_lchuv_f32 const *lchuv, alwan_xyz_f32 const *white_xyz);
void alwan_lchuv_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_lchuv_f64 const *lchuv, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> Oklch (cylindrical Oklab, D65 assumed) */
void alwan_xyz_to_oklch_f32(alwan_oklch_f32 *oklch, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_oklch_f64(alwan_oklch_f64 *oklch, alwan_xyz_f64 const *xyz);
void alwan_oklch_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_oklch_f32 const *oklch);
void alwan_oklch_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_oklch_f64 const *oklch);

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
    ALWAN_GAMUT_MAP_HPMINDE,         /* HPMINDE (Hue-Preserving Minimum Î”E) - P9.5 */
    ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE /* Lightness Preserving - P9.5 */
} alwan_gamut_map_method;

/* Estimate RGB gamut volume using Monte Carlo sampling
 * space: RGB color space descriptor
 * num_samples: number of random samples (e.g., 1000000)
 * seed: random seed for reproducibility
 * volume: output volume estimate (in XYZ units cubed)
 * Returns ALWAN_OK on success */
int alwan_gamut_volume_mc_f64(alwan_f64 *volume,
                          alwan_rgb_space_desc_f64 const *space,
                          size_t num_samples,
                          unsigned int seed);
int alwan_gamut_volume_mc_f32(alwan_f32 *volume,
                          alwan_rgb_space_desc_f32 const *space,
                          size_t num_samples,
                          unsigned int seed);

/* Map RGB colors to [0,1] gamut using specified method
 * Map operation with stride support for efficient array processing
 * rgb_out: output RGB colors (stride out_stride between consecutive triplets)
 * method: gamut mapping method (clip, hue-preserving)
 * rgb_in: input RGB colors (may be out of gamut, stride in_stride between triplets)
 * count: number of RGB triplets to process
 * in_stride: stride for input (in bytes, typically 3*sizeof(alwan_f64) for packed)
 * out_stride: stride for output (in bytes, typically 3*sizeof(alwan_f64) for packed)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if method not supported */
int alwan_gamut_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_gamut_map_method method);
int alwan_gamut_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_gamut_map_method method);

/* Map XYZ color to RGB gamut with hue preservation
 * ctx: optional context (can be NULL)
 * space: target RGB space
 * xyz_in: input XYZ color (may be out of RGB gamut)
 * rgb_out: output RGB color (mapped to [0,1] with preserved hue in JCh)
 * Returns ALWAN_OK on success */
int alwan_gamut_map_xyz_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_space_desc_f64 const *space, alwan_xyz_f64 const *xyz_in, alwan_ctx *ctx);
int alwan_gamut_map_xyz_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_space_desc_f32 const *space, alwan_xyz_f32 const *xyz_in, alwan_ctx *ctx);

/* CSS Color Level 4 Section 13.2 OKLCh gamut mapping (binary search on chroma)
 * Maps out-of-gamut linear sRGB to in-gamut linear sRGB using OKLCh binary search
 * with deltaEOK JND criterion (threshold 0.02)
 * rgb_out: output in-gamut linear sRGB triplets
 * rgb_in: input linear sRGB triplets (may be out of gamut)
 * count: number of RGB triplets
 * in_stride, out_stride: stride in bytes */
int alwan_css_gamut_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_css_gamut_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

/* Gamut map planar */
int alwan_gamut_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_gamut_map_method method);
int alwan_gamut_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_gamut_map_method method);
int alwan_css_gamut_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_css_gamut_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Gamut map _ex (typed pixel format) */
int alwan_gamut_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_gamut_map_method method, alwan_pixel_format in_fmt);
int alwan_gamut_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_gamut_map_method method, alwan_pixel_format in_fmt);
int alwan_css_gamut_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_css_gamut_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* ----------------------------------------------------------------
 * Transfer Functions (OETF/EOTF)
 * ---------------------------------------------------------------- */

/* Apply Opto-Electronic Transfer Function (linear -> encoded)
 * tf: transfer function to apply
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
int alwan_oetf_apply_f64(alwan_f64 *encoded_out, size_t out_stride, alwan_f64 const *linear_in, size_t in_stride, size_t count, alwan_transfer_function tf);
int alwan_oetf_apply_f32(alwan_f32 *encoded_out, size_t out_stride, alwan_f32 const *linear_in, size_t in_stride, size_t count, alwan_transfer_function tf);

/* Apply Electro-Optical Transfer Function (encoded -> linear)
 * tf: transfer function to apply
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
int alwan_eotf_apply_f64(alwan_f64 *linear_out, size_t out_stride, alwan_f64 const *encoded_in, size_t in_stride, size_t count, alwan_transfer_function tf);
int alwan_eotf_apply_f32(alwan_f32 *linear_out, size_t out_stride, alwan_f32 const *encoded_in, size_t in_stride, size_t count, alwan_transfer_function tf);

/* ----------------------------------------------------------------
 * Integer-to-Float Normalization (ColorInterop Section 1.5)
 * Uses (2^N - 1) normalization. Supported bit depths: 8, 10, 12, 16.
 * ---------------------------------------------------------------- */
int alwan_uint_to_float_f64(alwan_f64 *out, alwan_uint16 const *in, int bit_depth, size_t count);
int alwan_uint_to_float_f32(alwan_f32 *out, alwan_uint16 const *in, int bit_depth, size_t count);
int alwan_float_to_uint_f64(alwan_uint16 *out, alwan_f64 const *in, int bit_depth, size_t count);
int alwan_float_to_uint_f32(alwan_uint16 *out, alwan_f32 const *in, int bit_depth, size_t count);

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
 * in_stride: stride between input RGB triplets (in bytes, typically 3*sizeof(alwan_f64))
 * rgb_out: output RGB triplets (display-referred)
 * out_stride: stride between output RGB triplets (in bytes, typically 3*sizeof(alwan_f64))
 *
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if transform not supported */
int alwan_view_transform_apply_f64(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
int alwan_view_transform_apply_f32(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Color Space Conversions
 * ---------------------------------------------------------------- */

/* XYZ <-> xyY conversions */
void alwan_xyz_to_xyy_f32(alwan_xyy_f32 *xyy, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_xyy_f64(alwan_xyy_f64 *xyy, alwan_xyz_f64 const *xyz);
void alwan_xyy_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_xyy_f32 const *xyy);
void alwan_xyy_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_xyy_f64 const *xyy);

/* XYZ <-> Lab conversions (requires white point in XYZ) */
void alwan_xyz_to_lab_f32(alwan_lab_f32 *lab, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *white_xyz);
void alwan_xyz_to_lab_f64(alwan_lab_f64 *lab, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *white_xyz);
void alwan_lab_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_lab_f32 const *lab, alwan_xyz_f32 const *white_xyz);
void alwan_lab_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_lab_f64 const *lab, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> Luv conversions (requires white point in XYZ) */
void alwan_xyz_to_luv_f32(alwan_luv_f32 *luv, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *white_xyz);
void alwan_xyz_to_luv_f64(alwan_luv_f64 *luv, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *white_xyz);
void alwan_luv_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_luv_f32 const *luv, alwan_xyz_f32 const *white_xyz);
void alwan_luv_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_luv_f64 const *luv, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> U*V*W* conversions (CIE 1964 uniform color space for CRI)
 * Based on CIE 1960 UCS chromaticity diagram
 * Used specifically for Color Rendering Index calculations
 * Requires white point in XYZ */
void alwan_xyz_to_uvw_f32(alwan_uvw_f32 *uvw, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *white_xyz);
void alwan_xyz_to_uvw_f64(alwan_uvw_f64 *uvw, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *white_xyz);
void alwan_uvw_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_uvw_f32 const *uvw, alwan_xyz_f32 const *white_xyz);
void alwan_uvw_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_uvw_f64 const *uvw, alwan_xyz_f64 const *white_xyz);

/* Lab <-> LCh(ab) conversions */
void alwan_lab_to_lch_f32(alwan_lch_f32 *lch, alwan_lab_f32 const *lab);
void alwan_lab_to_lch_f64(alwan_lch_f64 *lch, alwan_lab_f64 const *lab);
void alwan_lch_to_lab_f32(alwan_lab_f32 *lab, alwan_lch_f32 const *lch);
void alwan_lch_to_lab_f64(alwan_lab_f64 *lab, alwan_lch_f64 const *lch);

/* Luv <-> LCh(uv) conversions */
void alwan_luv_to_lchuv_f32(alwan_lchuv_f32 *lchuv, alwan_luv_f32 const *luv);
void alwan_luv_to_lchuv_f64(alwan_lchuv_f64 *lchuv, alwan_luv_f64 const *luv);
void alwan_lchuv_to_luv_f32(alwan_luv_f32 *luv, alwan_lchuv_f32 const *lchuv);
void alwan_lchuv_to_luv_f64(alwan_luv_f64 *luv, alwan_lchuv_f64 const *lchuv);

/* ----------------------------------------------------------------
 * Map Color Space Conversions (with stride support)
 * ---------------------------------------------------------------- */

/* MapXYZ <-> Lab conversions
 * count: number of color triplets to convert
 * in_stride: input stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * out_stride: output stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64)) */
int alwan_xyz_to_lab_f32_map_interleave(alwan_f32 *lab_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_lab_f64_map_interleave(alwan_f64 *lab_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

int alwan_lab_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *lab_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_lab_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *lab_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

/* MapXYZ <-> Luv conversions */
int alwan_xyz_to_luv_f32_map_interleave(alwan_f32 *luv_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_luv_f64_map_interleave(alwan_f64 *luv_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

int alwan_luv_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *luv_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_luv_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *luv_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

/* MapLab <-> LCh conversions */
int alwan_lab_to_lch_f32_map_interleave(alwan_f32 *lch_out, size_t out_stride, alwan_f32 const *lab_in, size_t in_stride, size_t count);
int alwan_lab_to_lch_f64_map_interleave(alwan_f64 *lch_out, size_t out_stride, alwan_f64 const *lab_in, size_t in_stride, size_t count);

int alwan_lch_to_lab_f32_map_interleave(alwan_f32 *lab_out, size_t out_stride, alwan_f32 const *lch_in, size_t in_stride, size_t count);
int alwan_lch_to_lab_f64_map_interleave(alwan_f64 *lab_out, size_t out_stride, alwan_f64 const *lch_in, size_t in_stride, size_t count);

/* MapLuv <-> LCh(uv) conversions */
int alwan_luv_to_lchuv_f32_map_interleave(alwan_f32 *lchuv_out, size_t out_stride, alwan_f32 const *luv_in, size_t in_stride, size_t count);
int alwan_luv_to_lchuv_f64_map_interleave(alwan_f64 *lchuv_out, size_t out_stride, alwan_f64 const *luv_in, size_t in_stride, size_t count);

int alwan_lchuv_to_luv_f32_map_interleave(alwan_f32 *luv_out, size_t out_stride, alwan_f32 const *lchuv_in, size_t in_stride, size_t count);
int alwan_lchuv_to_luv_f64_map_interleave(alwan_f64 *luv_out, size_t out_stride, alwan_f64 const *lchuv_in, size_t in_stride, size_t count);

/* MapXYZ <-> xyY conversions */
int alwan_xyz_to_xyy_f32_map_interleave(alwan_f32 *xyy_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
int alwan_xyz_to_xyy_f64_map_interleave(alwan_f64 *xyy_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

int alwan_xyy_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *xyy_in, size_t in_stride, size_t count);
int alwan_xyy_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *xyy_in, size_t in_stride, size_t count);

/* Typed colorspace map functions (_ex variants) */
int alwan_xyz_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_lab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_luv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_luv_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_lab_to_lch_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_lch_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_luv_to_lchuv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_lchuv_to_luv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_xyy_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyy_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> Oklab conversions (modern perceptually uniform space, D65 assumed) */
void alwan_xyz_to_oklab_f32(alwan_oklab_f32 *oklab, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_oklab_f64(alwan_oklab_f64 *oklab, alwan_xyz_f64 const *xyz);
void alwan_oklab_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_oklab_f32 const *oklab);
void alwan_oklab_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_oklab_f64 const *oklab);

/* Oklab <-> Oklch conversions (cylindrical Oklab) */
void alwan_oklab_to_oklch_f32(alwan_oklch_f32 *oklch, alwan_oklab_f32 const *oklab);
void alwan_oklab_to_oklch_f64(alwan_oklch_f64 *oklch, alwan_oklab_f64 const *oklab);
void alwan_oklch_to_oklab_f32(alwan_oklab_f32 *oklab, alwan_oklch_f32 const *oklch);
void alwan_oklch_to_oklab_f64(alwan_oklab_f64 *oklab, alwan_oklch_f64 const *oklch);

/* MapXYZ <-> Oklab conversions */
int alwan_xyz_to_oklab_f32_map_interleave(alwan_f32 *oklab_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
int alwan_xyz_to_oklab_f64_map_interleave(alwan_f64 *oklab_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

int alwan_oklab_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *oklab_in, size_t in_stride, size_t count);
int alwan_oklab_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *oklab_in, size_t in_stride, size_t count);

/* MapOklab <-> Oklch conversions */
int alwan_oklab_to_oklch_f32_map_interleave(alwan_f32 *oklch_out, size_t out_stride, alwan_f32 const *oklab_in, size_t in_stride, size_t count);
int alwan_oklab_to_oklch_f64_map_interleave(alwan_f64 *oklch_out, size_t out_stride, alwan_f64 const *oklab_in, size_t in_stride, size_t count);

int alwan_oklch_to_oklab_f32_map_interleave(alwan_f32 *oklab_out, size_t out_stride, alwan_f32 const *oklch_in, size_t in_stride, size_t count);
int alwan_oklch_to_oklab_f64_map_interleave(alwan_f64 *oklab_out, size_t out_stride, alwan_f64 const *oklch_in, size_t in_stride, size_t count);

/* Typed Oklab map functions (_ex variants) */
int alwan_xyz_to_oklab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_oklab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_oklab_to_oklch_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_oklch_to_oklab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Lab <-> DIN99 conversions (DIN99 Family - German color difference standards)
 * - variant: 0 = DIN99/ASTM, 1 = DIN99b, 2 = DIN99c, 3 = DIN99d
 * - All variants provide improved perceptual uniformity over CIE Lab
 * - Input Lab should be D65 adapted
 */
void alwan_lab_to_din99_f32(alwan_din99_f32 *din99, alwan_lab_f32 const *lab, int variant);
void alwan_lab_to_din99_f64(alwan_din99_f64 *din99, alwan_lab_f64 const *lab, int variant);
void alwan_din99_to_lab_f32(alwan_lab_f32 *lab, alwan_din99_f32 const *din99, int variant);
void alwan_din99_to_lab_f64(alwan_lab_f64 *lab, alwan_din99_f64 const *din99, int variant);

/* RGB <-> ICtCp conversions (ITU-R BT.2100 HDR color space)
 * - RGB input/output is linear BT.2020 RGB
 * - use_pq: 1 for PQ (Perceptual Quantizer), 0 for HLG (Hybrid Log-Gamma)
 */
void alwan_rgb_to_ictcp_f32(alwan_ictcp_f32 *ictcp, alwan_rgb_f32 const *rgb, int use_pq);
void alwan_rgb_to_ictcp_f64(alwan_ictcp_f64 *ictcp, alwan_rgb_f64 const *rgb, int use_pq);
void alwan_ictcp_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_ictcp_f32 const *ictcp, int use_pq);
void alwan_ictcp_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_ictcp_f64 const *ictcp, int use_pq);

/* XYZ <-> ICtCp conversions (via BT.2020 RGB)
 * - XYZ is assumed to be D65 adapted
 * - use_pq: 1 for PQ (Perceptual Quantizer), 0 for HLG (Hybrid Log-Gamma)
 */
void alwan_xyz_to_ictcp_f32(alwan_ictcp_f32 *ictcp, alwan_xyz_f32 const *xyz, int use_pq);
void alwan_xyz_to_ictcp_f64(alwan_ictcp_f64 *ictcp, alwan_xyz_f64 const *xyz, int use_pq);
void alwan_ictcp_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_ictcp_f32 const *ictcp, int use_pq);
void alwan_ictcp_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_ictcp_f64 const *ictcp, int use_pq);

/* MapRGB <-> ICtCp conversions */
int alwan_rgb_to_ictcp_f32_map_interleave(alwan_f32 *ictcp_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, int use_pq);
int alwan_rgb_to_ictcp_f64_map_interleave(alwan_f64 *ictcp_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, int use_pq);

int alwan_ictcp_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *ictcp_in, size_t in_stride, size_t count, int use_pq);
int alwan_ictcp_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *ictcp_in, size_t in_stride, size_t count, int use_pq);

/* MapXYZ <-> ICtCp conversions */
int alwan_xyz_to_ictcp_f32_map_interleave(alwan_f32 *ictcp_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, int use_pq);
int alwan_xyz_to_ictcp_f64_map_interleave(alwan_f64 *ictcp_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, int use_pq);

int alwan_ictcp_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *ictcp_in, size_t in_stride, size_t count, int use_pq);
int alwan_ictcp_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *ictcp_in, size_t in_stride, size_t count, int use_pq);

/* Typed ICtCp map functions (_ex variants) */
int alwan_rgb_to_ictcp_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
int alwan_ictcp_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
int alwan_xyz_to_ictcp_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
int alwan_ictcp_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);

/* Jzazbz <-> XYZ conversions (Perceptually uniform HDR color space)
 * - XYZ input/output is D65 adapted
 * - Jzazbz: Jz (lightness), az (red-green), bz (yellow-blue)
 */
void alwan_xyz_to_jzazbz_f32(alwan_jzazbz_f32 *jzazbz, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_jzazbz_f64(alwan_jzazbz_f64 *jzazbz, alwan_xyz_f64 const *xyz);
void alwan_jzazbz_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_jzazbz_f32 const *jzazbz);
void alwan_jzazbz_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_jzazbz_f64 const *jzazbz);

/* Jzazbz <-> JzCzhz conversions (cylindrical coordinates)
 * - JzCzhz: Jz (lightness), Cz (chroma), hz (hue in radians)
 */
void alwan_jzazbz_to_jzczhz_f32(alwan_jzczhz_f32 *jzczhz, alwan_jzazbz_f32 const *jzazbz);
void alwan_jzazbz_to_jzczhz_f64(alwan_jzczhz_f64 *jzczhz, alwan_jzazbz_f64 const *jzazbz);
void alwan_jzczhz_to_jzazbz_f32(alwan_jzazbz_f32 *jzazbz, alwan_jzczhz_f32 const *jzczhz);
void alwan_jzczhz_to_jzazbz_f64(alwan_jzazbz_f64 *jzazbz, alwan_jzczhz_f64 const *jzczhz);

/* MapXYZ <-> JzAzBz conversions */
int alwan_xyz_to_jzazbz_f32_map_interleave(alwan_f32 *jzazbz_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
int alwan_xyz_to_jzazbz_f64_map_interleave(alwan_f64 *jzazbz_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

int alwan_jzazbz_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *jzazbz_in, size_t in_stride, size_t count);
int alwan_jzazbz_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *jzazbz_in, size_t in_stride, size_t count);

/* MapJzAzBz <-> JzCzhz conversions */
int alwan_jzazbz_to_jzczhz_f32_map_interleave(alwan_f32 *jzczhz_out, size_t out_stride, alwan_f32 const *jzazbz_in, size_t in_stride, size_t count);
int alwan_jzazbz_to_jzczhz_f64_map_interleave(alwan_f64 *jzczhz_out, size_t out_stride, alwan_f64 const *jzazbz_in, size_t in_stride, size_t count);

int alwan_jzczhz_to_jzazbz_f32_map_interleave(alwan_f32 *jzazbz_out, size_t out_stride, alwan_f32 const *jzczhz_in, size_t in_stride, size_t count);
int alwan_jzczhz_to_jzazbz_f64_map_interleave(alwan_f64 *jzazbz_out, size_t out_stride, alwan_f64 const *jzczhz_in, size_t in_stride, size_t count);

/* Typed JzAzBz map functions (_ex variants) */
int alwan_xyz_to_jzazbz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_jzazbz_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_jzazbz_to_jzczhz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_jzczhz_to_jzazbz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Hunter Lab <-> XYZ conversions (Earlier Lab-type color space)
 * - XYZ input/output is D65 adapted by default
 * - Hunter Lab: L (lightness), a (red-green), b (yellow-blue)
 * - Uses square roots instead of cube roots (unlike CIE Lab)
 * - Ka and Kb coefficients are illuminant-dependent
 */
void alwan_xyz_to_hunter_lab_f32(alwan_hunter_lab_f32 *hunter_lab, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_hunter_lab_f64(alwan_hunter_lab_f64 *hunter_lab, alwan_xyz_f64 const *xyz);
void alwan_hunter_lab_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_hunter_lab_f32 const *hunter_lab);
void alwan_hunter_lab_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_hunter_lab_f64 const *hunter_lab);

/* Hunter Lab <-> XYZ conversions with custom illuminant
 * - xyz_n: Reference white point (e.g., D50, D65, or custom illuminant)
 * - Ka and Kb are calculated automatically based on xyz_n
 */
void alwan_xyz_to_hunter_lab_custom_f32(alwan_hunter_lab_f32 *hunter_lab, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *xyz_n);
void alwan_xyz_to_hunter_lab_custom_f64(alwan_hunter_lab_f64 *hunter_lab, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *xyz_n);
void alwan_hunter_lab_to_xyz_custom_f32(alwan_xyz_f32 *xyz, alwan_hunter_lab_f32 const *hunter_lab, alwan_xyz_f32 const *xyz_n);
void alwan_hunter_lab_to_xyz_custom_f64(alwan_xyz_f64 *xyz, alwan_hunter_lab_f64 const *hunter_lab, alwan_xyz_f64 const *xyz_n);

/* IPT <-> XYZ conversions (Image Processing Transform)
 * - XYZ input/output is D65 adapted
 * - IPT: I (intensity/lightness), P (red-green), T (yellow-blue)
 * - Improved hue uniformity over CIELAB
 * - Uses power function nonlinearity (exponent 0.43)
 */
void alwan_xyz_to_ipt_f32(alwan_ipt_f32 *ipt, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_ipt_f64(alwan_ipt_f64 *ipt, alwan_xyz_f64 const *xyz);
void alwan_ipt_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_ipt_f32 const *ipt);
void alwan_ipt_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_ipt_f64 const *ipt);

/* IPT <-> IPTch conversions (cylindrical coordinates)
 * - IPTch: I (intensity), C (chroma), h (hue in radians)
 */
void alwan_ipt_to_iptch_f32(alwan_iptch_f32 *iptch, alwan_ipt_f32 const *ipt);
void alwan_ipt_to_iptch_f64(alwan_iptch_f64 *iptch, alwan_ipt_f64 const *ipt);
void alwan_iptch_to_ipt_f32(alwan_ipt_f32 *ipt, alwan_iptch_f32 const *iptch);
void alwan_iptch_to_ipt_f64(alwan_ipt_f64 *ipt, alwan_iptch_f64 const *iptch);

/* MapXYZ <-> IPT conversions */
int alwan_xyz_to_ipt_f32_map_interleave(alwan_f32 *ipt_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
int alwan_xyz_to_ipt_f64_map_interleave(alwan_f64 *ipt_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

int alwan_ipt_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *ipt_in, size_t in_stride, size_t count);
int alwan_ipt_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *ipt_in, size_t in_stride, size_t count);

/* Typed IPT map functions (_ex variants) */
int alwan_xyz_to_ipt_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_ipt_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* ProLab <-> XYZ conversions (Perceptually Uniform Projective)
 * - XYZ input/output is D65 adapted by default
 * - ProLab: Uses projective transformation for improved uniformity
 * - Based on Konovalenko et al. (2021)
 */
void alwan_xyz_to_prolab_f32(alwan_prolab_f32 *prolab, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_prolab_f64(alwan_prolab_f64 *prolab, alwan_xyz_f64 const *xyz);
void alwan_prolab_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_prolab_f32 const *prolab);
void alwan_prolab_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_prolab_f64 const *prolab);

/* ProLab <-> XYZ conversions with custom illuminant
 * - xyz_n: Reference white point (e.g., D50, D65, or custom illuminant)
 */
void alwan_xyz_to_prolab_custom_f32(alwan_prolab_f32 *prolab, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *xyz_n);
void alwan_xyz_to_prolab_custom_f64(alwan_prolab_f64 *prolab, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *xyz_n);
void alwan_prolab_to_xyz_custom_f32(alwan_xyz_f32 *xyz, alwan_prolab_f32 const *prolab, alwan_xyz_f32 const *xyz_n);
void alwan_prolab_to_xyz_custom_f64(alwan_xyz_f64 *xyz, alwan_prolab_f64 const *prolab, alwan_xyz_f64 const *xyz_n);

/* OSA-UCS <-> XYZ conversions (Optical Society of America Uniform Color Scales)
 * - XYZ input/output is D65 adapted
 * - OSA-UCS: L (lightness), j (yellowness), g (greenness)
 * - Forward transform is exact, inverse is approximate (iterative solution)
 * - Note: Inverse transformation has lower precision than other color spaces
 */
void alwan_xyz_to_osa_ucs_f32(alwan_osa_ucs_f32 *osa_ucs, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_osa_ucs_f64(alwan_osa_ucs_f64 *osa_ucs, alwan_xyz_f64 const *xyz);
void alwan_osa_ucs_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_osa_ucs_f32 const *osa_ucs);
void alwan_osa_ucs_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_osa_ucs_f64 const *osa_ucs);

/* CIE 1960 UCS <-> XYZ conversions (Uniform Chromaticity Scale)
 * - CIE 1960 UCS: u, v chromaticity coordinates + Y luminance
 * - Used for CCT calculations and color rendering metrics
 * - Precursor to CIE 1976 u'v' (CIELUV) chromaticity diagram
 */
void alwan_xyz_to_ucs_f32(alwan_ucs_f32 *ucs, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_ucs_f64(alwan_ucs_f64 *ucs, alwan_xyz_f64 const *xyz);
void alwan_ucs_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_ucs_f32 const *ucs);
void alwan_ucs_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_ucs_f64 const *ucs);

/* hdr-CIELAB <-> XYZ conversions (HDR extension of CIELAB)
 * - Fairchild & Wyble (2010) HDR-CIELAB model
 * - Designed for high dynamic range imagery (Y > 100)
 * - Maintains perceptual uniformity across extended luminance range
 * - XYZ input/output is D65 adapted, Y can exceed 100
 */
void alwan_xyz_to_hdr_cielab_f32(alwan_lab_f32 *hdr_lab, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_hdr_cielab_f64(alwan_lab_f64 *hdr_lab, alwan_xyz_f64 const *xyz);
void alwan_hdr_cielab_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_lab_f32 const *hdr_lab);
void alwan_hdr_cielab_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_lab_f64 const *hdr_lab);

/* hdr-IPT <-> XYZ conversions (HDR extension of IPT)
 * - Fairchild (2010) HDR-IPT model
 * - Extends IPT to high dynamic range
 * - Better hue preservation than hdr-CIELAB for HDR content
 * - XYZ input/output is D65 adapted, supports extended luminance
 */
void alwan_xyz_to_hdr_ipt_f32(alwan_ipt_f32 *hdr_ipt, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_hdr_ipt_f64(alwan_ipt_f64 *hdr_ipt, alwan_xyz_f64 const *xyz);
void alwan_hdr_ipt_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_ipt_f32 const *hdr_ipt);
void alwan_hdr_ipt_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_ipt_f64 const *hdr_ipt);

/* IgPgTg <-> XYZ conversions (Improved IPT variant)
 * - Ebner & Fairchild (1998) improved hue uniformity
 * - Better than IPT for certain hue angles
 * - XYZ input/output is D65 adapted
 */
void alwan_xyz_to_igpgtg_f32(alwan_igpgtg_f32 *igpgtg, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_igpgtg_f64(alwan_igpgtg_f64 *igpgtg, alwan_xyz_f64 const *xyz);
void alwan_igpgtg_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_igpgtg_f32 const *igpgtg);
void alwan_igpgtg_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_igpgtg_f64 const *igpgtg);

/* ICaCb <-> XYZ conversions (Image Difference Color Space)
 * - Zhang & Wandell (1996, 1997)
 * - Optimized for image difference metrics
 * - XYZ input/output is D65 adapted
 */
void alwan_xyz_to_icacb_f32(alwan_icacb_f32 *icacb, alwan_xyz_f32 const *xyz);
void alwan_xyz_to_icacb_f64(alwan_icacb_f64 *icacb, alwan_xyz_f64 const *xyz);
void alwan_icacb_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_icacb_f32 const *icacb);
void alwan_icacb_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_icacb_f64 const *icacb);

/* Prismatic <-> RGB conversions (Pridmore 2021)
 * - Perceptually uniform cylindrical color space
 * - P (purity), r (red-green), i (intensity)
 * - RGB input/output in [0, 1] range
 */
void alwan_rgb_to_prismatic_f32(alwan_prismatic_f32 *prismatic, alwan_rgb_f32 const *rgb);
void alwan_rgb_to_prismatic_f64(alwan_prismatic_f64 *prismatic, alwan_rgb_f64 const *rgb);
void alwan_prismatic_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_prismatic_f32 const *prismatic);
void alwan_prismatic_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_prismatic_f64 const *prismatic);

/* HCL <-> RGB conversions (Sarifuddin 2005)
 * - Hue-Chroma-Luminance polar coordinate system
 * - Better perceptual properties than HSL
 * - RGB input/output in [0, 1] range
 */
void alwan_rgb_to_hcl_f32(alwan_hcl_f32 *hcl, alwan_rgb_f32 const *rgb);
void alwan_rgb_to_hcl_f64(alwan_hcl_f64 *hcl, alwan_rgb_f64 const *rgb);
void alwan_hcl_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_hcl_f32 const *hcl);
void alwan_hcl_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_hcl_f64 const *hcl);

/* IHLS <-> RGB conversions (Improved HLS - Hanbury 2003)
 * - Improved Hue-Lightness-Saturation
 * - Better perceptual properties than standard HSL
 * - RGB input/output in [0, 1] range
 */
void alwan_rgb_to_ihls_f32(alwan_ihls_f32 *ihls, alwan_rgb_f32 const *rgb);
void alwan_rgb_to_ihls_f64(alwan_ihls_f64 *ihls, alwan_rgb_f64 const *rgb);
void alwan_ihls_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_ihls_f32 const *ihls);
void alwan_ihls_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_ihls_f64 const *ihls);

/* HLC <-> LCH conversions (cylindrical CIELAB, H-L-C ordering)
 * Pure reordering of CIE LCH(ab): H=[0-360], L=[0-100], C=[0-~181]
 * Reference: CIE 015:2004 */
void alwan_lch_to_hlc_f32(alwan_hlc_f32 *hlc, alwan_lch_f32 const *lch);
void alwan_lch_to_hlc_f64(alwan_hlc_f64 *hlc, alwan_lch_f64 const *lch);
void alwan_hlc_to_lch_f32(alwan_lch_f32 *lch, alwan_hlc_f32 const *hlc);
void alwan_hlc_to_lch_f64(alwan_lch_f64 *lch, alwan_hlc_f64 const *hlc);

/* Cubehelix <-> sRGB conversions (Green 2011)
 * Monotonic-luminance helical scheme for data visualization
 * h: hue [degrees], s: saturation [0+], l: lightness [0-1]
 * Reference: Green, D.A., 2011, BASI, 39, 289 */
void alwan_cubehelix_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_cubehelix_f32 const *ch);
void alwan_cubehelix_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_cubehelix_f64 const *ch);
void alwan_rgb_to_cubehelix_f32(alwan_cubehelix_f32 *ch, alwan_rgb_f32 const *rgb);
void alwan_rgb_to_cubehelix_f64(alwan_cubehelix_f64 *ch, alwan_rgb_f64 const *rgb);

/* HSLuv <-> sRGB conversions (Boronine)
 * Human-friendly HSL via CIE LCHuv with sRGB gamut boundary
 * h: [0-360], s: [0-100] (% of max chroma at h,l), l: [0-100]
 * Input/output sRGB is encoded (with OETF), [0-1]
 * Reference: https://www.hsluv.org/ */
void alwan_hsluv_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_hsluv_f32 const *hsluv);
void alwan_hsluv_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_hsluv_f64 const *hsluv);
void alwan_srgb_to_hsluv_f32(alwan_hsluv_f32 *hsluv, alwan_rgb_f32 const *srgb);
void alwan_srgb_to_hsluv_f64(alwan_hsluv_f64 *hsluv, alwan_rgb_f64 const *srgb);

/* HPLuv <-> sRGB conversions (Boronine)
 * Pastel variant of HSLuv: all (h,s,l) triples guaranteed in sRGB gamut
 * Uses minimum chroma across all hues at given lightness
 * Reference: https://www.hsluv.org/ */
void alwan_hpluv_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_hpluv_f32 const *hpluv);
void alwan_hpluv_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_hpluv_f64 const *hpluv);
void alwan_srgb_to_hpluv_f32(alwan_hpluv_f32 *hpluv, alwan_rgb_f32 const *srgb);
void alwan_srgb_to_hpluv_f64(alwan_hpluv_f64 *hpluv, alwan_rgb_f64 const *srgb);

/* Okhsl <-> sRGB conversions (Ottosson 2021)
 * Perceptually uniform HSL in Oklab space
 * h: [0-1], s: [0-1], l: [0-1]
 * Input/output sRGB is encoded (with OETF), [0-1]
 * Reference: https://bottosson.github.io/posts/colorpicker/ */
void alwan_okhsl_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_okhsl_f32 const *okhsl);
void alwan_okhsl_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_okhsl_f64 const *okhsl);
void alwan_srgb_to_okhsl_f32(alwan_okhsl_f32 *okhsl, alwan_rgb_f32 const *srgb);
void alwan_srgb_to_okhsl_f64(alwan_okhsl_f64 *okhsl, alwan_rgb_f64 const *srgb);

/* Okhsv <-> sRGB conversions (Ottosson 2021)
 * Perceptually uniform HSV in Oklab space
 * h: [0-1], s: [0-1], v: [0-1]
 * Input/output sRGB is encoded (with OETF), [0-1]
 * Reference: https://bottosson.github.io/posts/colorpicker/ */
void alwan_okhsv_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_okhsv_f32 const *okhsv);
void alwan_okhsv_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_okhsv_f64 const *okhsv);
void alwan_srgb_to_okhsv_f32(alwan_okhsv_f32 *okhsv, alwan_rgb_f32 const *srgb);
void alwan_srgb_to_okhsv_f64(alwan_okhsv_f64 *okhsv, alwan_rgb_f64 const *srgb);

/* ----------------------------------------------------------------
 * Extended Colorspace Batch Map Functions
 * ---------------------------------------------------------------- */

/* XYZ <-> IgPgTg batch maps */
int alwan_xyz_to_igpgtg_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_igpgtg_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_igpgtg_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_igpgtg_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_igpgtg_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_igpgtg_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> ICaCb batch maps */
int alwan_xyz_to_icacb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_icacb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_icacb_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_icacb_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_icacb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_icacb_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> hdr-CIELAB batch maps */
int alwan_xyz_to_hdr_cielab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_hdr_cielab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_hdr_cielab_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_hdr_cielab_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_hdr_cielab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hdr_cielab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> hdr-IPT batch maps */
int alwan_xyz_to_hdr_ipt_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_hdr_ipt_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_hdr_ipt_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_hdr_ipt_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_hdr_ipt_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hdr_ipt_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> UCS batch maps */
int alwan_xyz_to_ucs_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_ucs_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_ucs_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_ucs_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_ucs_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_ucs_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> OSA-UCS batch maps */
int alwan_xyz_to_osa_ucs_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_osa_ucs_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_osa_ucs_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_osa_ucs_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_osa_ucs_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_osa_ucs_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> Hunter Lab batch maps (D65 default white) */
int alwan_xyz_to_hunter_lab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_hunter_lab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_hunter_lab_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_hunter_lab_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_hunter_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hunter_lab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> Hunter Lab batch maps (custom white point) */
int alwan_xyz_to_hunter_lab_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_hunter_lab_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_hunter_lab_to_xyz_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_hunter_lab_to_xyz_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_hunter_lab_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_hunter_lab_to_xyz_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> ProLab batch maps (D65 default white) */
int alwan_xyz_to_prolab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_prolab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_prolab_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_prolab_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_xyz_to_prolab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_prolab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> ProLab batch maps (custom white point) */
int alwan_xyz_to_prolab_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_prolab_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_prolab_to_xyz_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_prolab_to_xyz_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_prolab_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_prolab_to_xyz_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> UVW batch maps (with white point) */
int alwan_xyz_to_uvw_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_uvw_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_uvw_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_uvw_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_uvw_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_uvw_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* RGB <-> Prismatic batch maps */
int alwan_rgb_to_prismatic_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_rgb_to_prismatic_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_prismatic_to_rgb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_prismatic_to_rgb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_rgb_to_prismatic_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_prismatic_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* RGB <-> HCL batch maps */
int alwan_rgb_to_hcl_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_rgb_to_hcl_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_hcl_to_rgb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_hcl_to_rgb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_rgb_to_hcl_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hcl_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* RGB <-> IHLS batch maps */
int alwan_rgb_to_ihls_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_rgb_to_ihls_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_ihls_to_rgb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
int alwan_ihls_to_rgb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
int alwan_rgb_to_ihls_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_ihls_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Lab <-> DIN99 batch maps (with variant: 0=DIN99, 1=b, 2=c, 3=d) */
int alwan_lab_to_din99_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int variant);
int alwan_lab_to_din99_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int variant);
int alwan_din99_to_lab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int variant);
int alwan_din99_to_lab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int variant);
int alwan_lab_to_din99_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);
int alwan_din99_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);

/* ----------------------------------------------------------------
 * Color Difference (Î”E) Metrics
 * ---------------------------------------------------------------- */

/* Î”E*76 - Euclidean distance in Lab space */
alwan_f32  alwan_delta_e_76_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_76_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* Î”E OK - Euclidean distance in Oklab space (CSS Color Level 4 JND criterion) */
alwan_f32  alwan_delta_e_ok_f32(alwan_oklab_f32 const *a, alwan_oklab_f32 const *b);
alwan_f64 alwan_delta_e_ok_f64(alwan_oklab_f64 const *a, alwan_oklab_f64 const *b);

/* Î”E*94 - CIE 1994 color difference (graphic arts defaults: kL=1, K1=0.045, K2=0.015) */
alwan_f32  alwan_delta_e_94_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_94_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* Î”E CMC(l:c) - CMC color difference (defaults: l=2, c=1 for acceptability) */
void alwan_delta_e_cmc_params_default_f32(alwan_delta_e_cmc_params_f32 *p);
void alwan_delta_e_cmc_params_default_f64(alwan_delta_e_cmc_params_f64 *p);
alwan_f32  alwan_delta_e_cmc_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2, alwan_delta_e_cmc_params_f32 const *params);
alwan_f64 alwan_delta_e_cmc_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2, alwan_delta_e_cmc_params_f64 const *params);

/* Î”E*00 - CIEDE2000 color difference (most perceptually uniform) */
alwan_f32  alwan_delta_e_2000_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_2000_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* Î”E ITP - ITU-R BT.2124 HDR color difference in ICtCp space (scalar_factor default: 720) */
alwan_f32  alwan_delta_e_itp_f32(alwan_ictcp_f32 const *ictcp1, alwan_ictcp_f32 const *ictcp2, alwan_delta_e_itp_params_f32 const *params);
alwan_f64 alwan_delta_e_itp_f64(alwan_ictcp_f64 const *ictcp1, alwan_ictcp_f64 const *ictcp2, alwan_delta_e_itp_params_f64 const *params);

/* Î”E HyAB - Hybrid Delta E, improved perceptual metric */
alwan_f32  alwan_delta_e_hyab_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_hyab_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* Î”E DIN99 - Euclidean distance in DIN99 space (variant: 0=DIN99, 1=b, 2=c, 3=d) */
alwan_f32  alwan_delta_e_din99_f32(alwan_din99_f32 const *din99_1, alwan_din99_f32 const *din99_2);
alwan_f64 alwan_delta_e_din99_f64(alwan_din99_f64 const *din99_1, alwan_din99_f64 const *din99_2);

/* Î”E CAM02-LCD - CIECAM02 Large Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam02_lcd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam02_lcd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* Î”E CAM02-SCD - CIECAM02 Small Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam02_scd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam02_scd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* Î”E CAM16-LCD - CAM16 Large Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam16_lcd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam16_lcd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* Î”E CAM16-SCD - CAM16 Small Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam16_scd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam16_scd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* Î”E CAM02-UCS - CIECAM02 Uniform Color Space (Luo et al. 2006)
 * Uses K_L=1.0, c1=0.007, c2=0.0228 for general-purpose color difference */
alwan_f32  alwan_delta_e_cam02_ucs_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam02_ucs_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* Î”E CAM16-UCS - CAM16 Uniform Color Space (Li et al. 2017)
 * Uses K_L=1.0, c1=0.007, c2=0.0228 for general-purpose color difference */
alwan_f32  alwan_delta_e_cam16_ucs_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam16_ucs_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* Î”E ZCAM - Euclidean distance in ZCAM UCS (Jzazbz) space */
alwan_f32  alwan_delta_e_zcam_f32(alwan_jzazbz_f32 const *jab1, alwan_jzazbz_f32 const *jab2);
alwan_f64 alwan_delta_e_zcam_f64(alwan_jzazbz_f64 const *jab1, alwan_jzazbz_f64 const *jab2);

/* ----------------------------------------------------------------
 * Batch Color Difference (Î”E) Computations
 * Compare arrays of colors efficiently
 * ---------------------------------------------------------------- */

/* Batch Î”E*76 - Euclidean distance in Lab space
 * delta_e_out: output Î”E values (count elements)
 * lab1_in: first array of Lab colors
 * lab2_in: second array of Lab colors
 * count: number of color pairs to compare
 * in1_stride: stride for lab1_in in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * in2_stride: stride for lab2_in in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * Returns ALWAN_OK on success */
int alwan_delta_e_76_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count);
int alwan_delta_e_76_f64_batch(alwan_f64 *delta_e_out, alwan_f64 const *lab1_in, size_t in1_stride, alwan_f64 const *lab2_in, size_t in2_stride, size_t count);

/* Batch Î”E*00 - CIEDE2000 color difference
 * Strides in1_stride/in2_stride are in bytes (typically 3*sizeof(alwan_f32/alwan_f64)) */
int alwan_delta_e_2000_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count);
int alwan_delta_e_2000_f64_batch(alwan_f64 *delta_e_out,
                             alwan_f64 const *lab1_in, size_t in1_stride,
                             alwan_f64 const *lab2_in, size_t in2_stride,
                             size_t count);

/* Batch Î”E*94 - CIE 1994 color difference
 * Strides in1_stride/in2_stride are in bytes (typically 3*sizeof(alwan_f32/alwan_f64)) */
int alwan_delta_e_94_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count);
int alwan_delta_e_94_f64_batch(alwan_f64 *delta_e_out, alwan_f64 const *lab1_in, size_t in1_stride, alwan_f64 const *lab2_in, size_t in2_stride, size_t count);

/* Batch Î”E CMC(l:c) - CMC color difference
 * Strides in1_stride/in2_stride are in bytes (typically 3*sizeof(alwan_f32/alwan_f64)) */
int alwan_delta_e_cmc_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count, alwan_f32 l, alwan_f32 c);
int alwan_delta_e_cmc_f64_batch(alwan_f64 *delta_e_out, alwan_f64 const *lab1_in, size_t in1_stride, alwan_f64 const *lab2_in, size_t in2_stride, size_t count, alwan_f64 l, alwan_f64 c);

/* Typed delta E batch functions (_ex variants) */
int alwan_delta_e_76_batch_ex(alwan_f64 *delta_e_out,
                               void const *lab1_in, size_t in1_stride,
                               void const *lab2_in, size_t in2_stride,
                               size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);
int alwan_delta_e_2000_batch_ex(alwan_f64 *delta_e_out,
                                 void const *lab1_in, size_t in1_stride,
                                 void const *lab2_in, size_t in2_stride,
                                 size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);
int alwan_delta_e_94_batch_ex(alwan_f64 *delta_e_out,
                               void const *lab1_in, size_t in1_stride,
                               void const *lab2_in, size_t in2_stride,
                               size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);
int alwan_delta_e_cmc_batch_ex(alwan_f64 *delta_e_out,
                                void const *lab1_in, size_t in1_stride,
                                void const *lab2_in, size_t in2_stride,
                                size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt,
                                alwan_f64 l, alwan_f64 c);

/* ----------------------------------------------------------------
 * Whiteness & Yellowness Indices
 * ---------------------------------------------------------------- */

/* Illuminant/Observer pairs for ASTM E313 calculations */
typedef enum {
    ALWAN_ASTM_E313_C_2DEG = 0,    /* Illuminant C, CIE 1931 2Â° observer */
    ALWAN_ASTM_E313_D65_2DEG = 1,  /* Illuminant D65, CIE 1931 2Â° observer */
    ALWAN_ASTM_E313_C_10DEG = 2,   /* Illuminant C, CIE 1964 10Â° observer */
    ALWAN_ASTM_E313_D65_10DEG = 3  /* Illuminant D65, CIE 1964 10Â° observer */
} alwan_astm_e313_illuminant;

/* ASTM E313 Yellowness Index
 * xyz: CIE XYZ tristimulus values (normalized to Y=100 for perfect white)
 * illuminant: illuminant/observer pair (C/2Â°, D65/2Â°, C/10Â°, or D65/10Â°)
 * Returns: Yellowness Index (YI) value */
alwan_f32  alwan_yellowness_astm_e313_f32(alwan_xyz_f32 const *xyz, alwan_astm_e313_illuminant illuminant);
alwan_f64 alwan_yellowness_astm_e313_f64(alwan_xyz_f64 const *xyz, alwan_astm_e313_illuminant illuminant);

/* ASTM E313 Whiteness Index
 * xyz: CIE XYZ tristimulus values (normalized to Y=100 for perfect white)
 * illuminant: illuminant/observer pair (C/2Â°, D65/2Â°, C/10Â°, or D65/10Â°)
 * Returns: Whiteness Index (WI) value */
alwan_f32  alwan_whiteness_astm_e313_f32(alwan_xyz_f32 const *xyz, alwan_astm_e313_illuminant illuminant);
alwan_f64 alwan_whiteness_astm_e313_f64(alwan_xyz_f64 const *xyz, alwan_astm_e313_illuminant illuminant);

/* CIE 2004 Whiteness Index
 * xy: CIE 1931 chromaticity coordinates (x, y)
 * Y: CIE Y tristimulus value (luminance factor)
 * xy_n: reference white chromaticity coordinates
 * Returns: CIE Whiteness (W) value
 * Note: Also computes Tint (T), but this function only returns W.
 *       Tint = 900(xn - x) - 650(yn - y) for 2Â° observer
 *       Tint = 1000(xn - x) - 650(yn - y) for 10Â° observer */
alwan_f32  alwan_whiteness_cie2004_f32(alwan_vec2_f32 const *xy, alwan_f32 Y, alwan_vec2_f32 const *xy_n);
alwan_f64 alwan_whiteness_cie2004_f64(alwan_vec2_f64 const *xy, alwan_f64 Y, alwan_vec2_f64 const *xy_n);

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
int alwan_cat_matrix_f32(alwan_mat3x3_f32 *out,
                         alwan_xyz_f32 const *src_white_xyz,
                         alwan_xyz_f32 const *dst_white_xyz,
                         alwan_cat_method method);
int alwan_cat_matrix_f64(alwan_mat3x3_f64 *out,
                         alwan_xyz_f64 const *src_white_xyz,
                         alwan_xyz_f64 const *dst_white_xyz,
                         alwan_cat_method method);

/* Apply chromatic adaptation to XYZ colors (map operation)
 * xyz_in: input XYZ colors (stride in_stride between consecutive colors)
 * count: number of colors to transform
 * in_stride: stride for input (in bytes, typically 3*sizeof(alwan_f32/alwan_f64) for packed array)
 * src_white_xyz: source white point in XYZ
 * dst_white_xyz: destination white point in XYZ
 * method: CAT method
 * xyz_out: output XYZ colors (stride out_stride between consecutive colors)
 * out_stride: stride for output (in bytes, typically 3*sizeof(alwan_f32/alwan_f64) for packed array)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if parameters are invalid */
int alwan_xyz_adapt_f32(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f32 const *src_white_xyz, alwan_xyz_f32 const *dst_white_xyz, alwan_cat_method method);
int alwan_xyz_adapt_f64(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f64 const *src_white_xyz, alwan_xyz_f64 const *dst_white_xyz, alwan_cat_method method);

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
int alwan_cat_zhai2018_f32(alwan_xyz_f32 *xyz_out,
                           alwan_xyz_f32 const *xyz_in,
                           alwan_xyz_f32 const *xyz_src,
                           alwan_xyz_f32 const *xyz_dst,
                           alwan_f32 D_src,
                           alwan_f32 D_dst,
                           alwan_xyz_f32 const *xyz_baseline,
                           alwan_cat_method transform);
int alwan_cat_zhai2018_f64(alwan_xyz_f64 *xyz_out,
                           alwan_xyz_f64 const *xyz_in,
                           alwan_xyz_f64 const *xyz_src,
                           alwan_xyz_f64 const *xyz_dst,
                           alwan_f64 D_src,
                           alwan_f64 D_dst,
                           alwan_xyz_f64 const *xyz_baseline,
                           alwan_cat_method transform);

/* ----------------------------------------------------------------
 * Spectral Power Distributions (SPD)
 * ---------------------------------------------------------------- */

/* Observer type (standard color matching functions) */

typedef enum {
    ALWAN_OBSERVER_CIE_1931_2DEG = 0,  /* CIE 1931 2Â° standard observer */
    ALWAN_OBSERVER_CIE_1964_10DEG = 1, /* CIE 1964 10Â° standard observer */
    ALWAN_OBSERVER_CIE_2012_2DEG = 2,  /* CIE 2012 2Â° standard observer (physiologically-based) */
    ALWAN_OBSERVER_CIE_2012_10DEG = 3, /* CIE 2012 10Â° standard observer (physiologically-based) */

    /* Extended observers */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG = 4,  /* Stockman & Sharpe 2000 2Â° cone fundamentals */
    ALWAN_OBSERVER_CIE_2015_2DEG = 5,         /* CIE 2015 2Â° cone-fundamental-based observer */
    ALWAN_OBSERVER_CIE_2015_10DEG = 6,        /* CIE 2015 10Â° cone-fundamental-based observer */
    ALWAN_OBSERVER_WRIGHT_GUILD_1931 = 7      /* Wright & Guild 1931 2Â° RGB CMFs (historical) */
} alwan_observer_type;

/* Camera/Sensor spectral sensitivity identifiers */
typedef enum {
    ALWAN_CAMERA_NIKON_5100,        /* Nikon D5100 (NPL measured) */
    ALWAN_CAMERA_SIGMA_SDMERILL     /* Sigma SD Merill (NPL measured) */
} alwan_camera_sensitivity;

/* Get white point XYZ for a standard illuminant
 * Computes XYZ tristimulus values from illuminant xy chromaticity (Y normalized to 1.0)
 * Returns ALWAN_E_INVALID if illuminant not supported */
int alwan_illuminant_white_point_f64(alwan_xyz_f64 *out_xyz,
                                   alwan_illuminant illuminant,
                                   alwan_observer_type observer);
int alwan_illuminant_white_point_f32(alwan_xyz_f32 *out_xyz,
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
int alwan_spd_create_f64(alwan_spd_f64 *out, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx);
int alwan_spd_create_f32(alwan_spd_f32 *out, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx);

/* Destroy SPD and free allocated values */
void alwan_spd_destroy_f64(alwan_spd_f64 *spd, alwan_ctx *ctx);
void alwan_spd_destroy_f32(alwan_spd_f32 *spd, alwan_ctx *ctx);

/* Load standard illuminant SPD
 * ctx: context (for data path and allocation)
 * ill: illuminant to load
 * out: output SPD structure
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if illuminant not supported */
int alwan_spd_illuminant_f64(alwan_spd_f64 *out, alwan_illuminant ill, alwan_ctx *ctx);
int alwan_spd_illuminant_f32(alwan_spd_f32 *out, alwan_illuminant ill, alwan_ctx *ctx);

/* Generate blackbody (Planckian) SPD at given temperature
 * Uses Planck's law to compute spectral radiance
 * ctx: context
 * temperature_K: color temperature in Kelvin (typically 1000-25000K)
 * wavelength_min: starting wavelength (nm)
 * wavelength_max: ending wavelength (nm)
 * count: number of samples
 * out: output SPD structure (values allocated internally)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if temperature out of range */
int alwan_spd_blackbody_f64(alwan_spd_f64 *out, alwan_f64 temperature_K, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx);
int alwan_spd_blackbody_f32(alwan_spd_f32 *out, alwan_f32 temperature_K, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx);

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
int alwan_spd_resample_f64(alwan_spd_f64 *dst, alwan_spd_f64 const *src, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx);
int alwan_spd_resample_f32(alwan_spd_f32 *dst, alwan_spd_f32 const *src, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx);

/* Compute XYZ tristimulus values from SPD
 * ctx: context
 * spd: spectral power distribution (reflectance or emission)
 * illuminant: illuminant SPD (NULL = assume spd is already weighted by illuminant)
 * observer: observer type (CIE 1931/1964/2012 2Â° or 10Â°)
 * method: integration method (trapezoid or Simpson)
 * bandpass_nm: bandpass width for Stearns & Stearns correction (0 = no correction)
 * xyz_out: output XYZ tristimulus values
 * Returns ALWAN_OK on success */
int alwan_xyz_from_spd_f64(alwan_xyz_f64 *xyz_out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, alwan_f64 bandpass_nm, alwan_ctx *ctx);
int alwan_xyz_from_spd_f32(alwan_xyz_f32 *xyz_out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, alwan_f32 bandpass_nm, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Camera Sensitivities
 * ---------------------------------------------------------------- */

/* Load camera RGB spectral sensitivities
 * Loads R, G, B sensitivity curves for specified camera
 * All three output SPDs must be pre-created with desired wavelength range/count
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if camera not supported */
int alwan_spd_camera_sensitivity_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b, alwan_camera_sensitivity camera, alwan_ctx *ctx);
int alwan_spd_camera_sensitivity_f32(alwan_spd_f32 *spd_r, alwan_spd_f32 *spd_g, alwan_spd_f32 *spd_b, alwan_camera_sensitivity camera, alwan_ctx *ctx);

/* Compute XYZ from SPD using camera sensitivities
 * Similar to alwan_xyz_from_spd_f64 but uses camera RGB sensitivities instead of standard observer
 * Returns ALWAN_OK on success */
int alwan_xyz_from_spd_camera_f64(alwan_xyz_f64 *xyz_out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, alwan_camera_sensitivity camera, alwan_integrate_method method, alwan_ctx *ctx);
int alwan_xyz_from_spd_camera_f32(alwan_xyz_f32 *xyz_out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, alwan_camera_sensitivity camera, alwan_integrate_method method, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Spectral Shape Descriptors
 * ---------------------------------------------------------------- */

/* Analyze SPD shape characteristics
 * Computes peak wavelength, FWHM, centroid, and bandwidth
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if SPD is invalid */
int alwan_spd_analyze_shape_f64(alwan_spd_shape_f64 *shape_out, alwan_spd_f64 const *spd);
int alwan_spd_analyze_shape_f32(alwan_spd_shape_f32 *shape_out, alwan_spd_f32 const *spd);

/* ----------------------------------------------------------------
 * Gamut Analysis & Mapping
 * ---------------------------------------------------------------- */

/* Check if xy chromaticity is within Pointer's Gamut
 * Pointer's Gamut represents the boundary of real surface colors under illuminant C
 * xy: CIE 1931 xy chromaticity coordinates
 * Returns 1 if inside Pointer's Gamut, 0 otherwise */
int alwan_is_within_pointer_gamut_f32(alwan_vec2_f32 const *xy);
int alwan_is_within_pointer_gamut_f64(alwan_vec2_f64 const *xy);

/* Get Pointer's Gamut boundary points
 * Returns array of xy chromaticity coordinates defining the boundary
 * count_out: receives the number of boundary points (32)
 * Returns pointer to internal static data (do not free) */
alwan_vec2_f64 const* alwan_pointer_gamut_boundary(size_t *count_out);

/* Get CIE 1931 spectral locus xy chromaticity for a given wavelength
 * Computes xy chromaticity from CIE 1931 2Â° observer CMFs for monochromatic light
 * wavelength: wavelength in nm (360-830nm)
 * xy_out: output xy chromaticity coordinates
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if wavelength out of range */
int alwan_spectral_locus_xy_f32(alwan_vec2_f32 *xy_out, alwan_f32 wavelength);
int alwan_spectral_locus_xy_f64(alwan_vec2_f64 *xy_out, alwan_f64 wavelength);

/* Compute dominant wavelength for a color
 * Dominant wavelength is the wavelength of monochromatic light that,
 * when mixed with the white point, matches the given color's hue
 * xy: CIE 1931 xy chromaticity coordinates of the color
 * xy_white: white point xy chromaticity (e.g., illuminant D65)
 * wavelength_out: receives dominant wavelength in nm (or negative for complementary)
 * xy_wl_out: receives xy of the spectral locus point (optional, can be NULL)
 * xy_cw_out: receives xy of the color-white intersection (optional, can be NULL)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if color is on/near the purple line */
int alwan_dominant_wavelength_f32(alwan_f32 *wavelength_out,
                               alwan_vec2_f32 *xy_wl_out,
                               alwan_vec2_f32 *xy_cw_out,
                               alwan_vec2_f32 const *xy,
                               alwan_vec2_f32 const *xy_white);
int alwan_dominant_wavelength_f64(alwan_f64 *wavelength_out,
                               alwan_vec2_f64 *xy_wl_out,
                               alwan_vec2_f64 *xy_cw_out,
                               alwan_vec2_f64 const *xy,
                               alwan_vec2_f64 const *xy_white);

/* Compute excitation purity for a color
 * Excitation purity is the ratio of the distance from the white point to the color,
 * divided by the distance from the white point to the spectrum locus, along the
 * line connecting them (0 = white, 1 = spectral/maximum saturation)
 * xy: CIE 1931 xy chromaticity coordinates of the color
 * xy_white: white point xy chromaticity (e.g., illuminant D65)
 * purity_out: receives excitation purity [0-1]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_excitation_purity_f32(alwan_f32 *purity_out,
                             alwan_vec2_f32 const *xy,
                             alwan_vec2_f32 const *xy_white);
int alwan_excitation_purity_f64(alwan_f64 *purity_out,
                             alwan_vec2_f64 const *xy,
                             alwan_vec2_f64 const *xy_white);

/* Compute complementary wavelength for a color
 * Complementary wavelength is used for colors on the purple line (no dominant wavelength)
 * It is the wavelength on the opposite side of the white point
 * xy: CIE 1931 xy chromaticity coordinates of the color
 * xy_white: white point xy chromaticity (e.g., illuminant D65)
 * wavelength_out: receives complementary wavelength in nm
 * xy_wl_out: receives xy of the spectral locus point (optional, can be NULL)
 * xy_cw_out: receives xy of the color-white intersection (optional, can be NULL)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_complementary_wavelength_f32(alwan_f32 *wavelength_out,
                                     alwan_vec2_f32 *xy_wl_out,
                                     alwan_vec2_f32 *xy_cw_out,
                                     alwan_vec2_f32 const *xy,
                                     alwan_vec2_f32 const *xy_white);
int alwan_complementary_wavelength_f64(alwan_f64 *wavelength_out,
                                     alwan_vec2_f64 *xy_wl_out,
                                     alwan_vec2_f64 *xy_cw_out,
                                     alwan_vec2_f64 const *xy,
                                     alwan_vec2_f64 const *xy_white);

/* Compute gamut volume ratio between two RGB color spaces
 * Computes the ratio of gamut volumes: volume(space1) / volume(space2)
 * space1: first RGB color space descriptor
 * space2: second RGB color space descriptor
 * ratio_out: receives volume ratio
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_gamut_volume_ratio_f64(alwan_f64 *ratio_out,
                               alwan_rgb_space_desc_f64 const *space1,
                               alwan_rgb_space_desc_f64 const *space2);
int alwan_gamut_volume_ratio_f32(alwan_f32 *ratio_out,
                               alwan_rgb_space_desc_f32 const *space1,
                               alwan_rgb_space_desc_f32 const *space2);

/* Compute gamut coverage percentage between two RGB color spaces
 * Computes what percentage of space1's gamut is covered by space2's gamut
 * Uses Monte Carlo sampling to estimate overlap
 * space1: reference RGB color space (the gamut we're measuring coverage of)
 * space2: comparison RGB color space (the gamut we're comparing against)
 * num_samples: number of Monte Carlo samples (recommended: 10000+)
 * seed: random seed for reproducibility
 * coverage_out: receives coverage percentage [0-100]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_gamut_coverage_f64(alwan_f64 *coverage_out,
                          alwan_rgb_space_desc_f64 const *space1,
                          alwan_rgb_space_desc_f64 const *space2,
                          size_t num_samples,
                          unsigned int seed);
int alwan_gamut_coverage_f32(alwan_f32 *coverage_out,
                          alwan_rgb_space_desc_f32 const *space1,
                          alwan_rgb_space_desc_f32 const *space2,
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
int alwan_gamut_map_advanced_f32(alwan_rgb_f32 *rgb_out,
                                  alwan_gamut_map_method method,
                                  alwan_rgb_space_desc_f32 const *space,
                                  alwan_rgb_f32 const *rgb_linear);
int alwan_gamut_map_advanced_f64(alwan_rgb_f64 *rgb_out,
                                  alwan_gamut_map_method method,
                                  alwan_rgb_space_desc_f64 const *space,
                                  alwan_rgb_f64 const *rgb_linear);

/* ----------------------------------------------------------------
 * Spectral Upsampling - RGB to Spectrum Conversion
 * ---------------------------------------------------------------- */

/* Smits1999: RGB to spectrum conversion using basis spectra mixing
 * Reference: Smits, Brian. "An RGB to Spectrum Conversion for Reflectances" (1999)
 * ctx: context (for allocation)
 * rgb: input RGB values (assumed to be in sRGB colorspace, clamped to [0, 1])
 * out_spd: output spectral power distribution (wavelength range: 380-720nm, 10 samples)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
int alwan_rgb_to_spectrum_smits1999_f64(alwan_spd_f64 *out_spd, alwan_rgb_f64 const *rgb, alwan_ctx *ctx);
int alwan_rgb_to_spectrum_smits1999_f32(alwan_spd_f32 *out_spd, alwan_rgb_f32 const *rgb, alwan_ctx *ctx);

/* Mallett2019: RGB to spectrum conversion using spectral primary decomposition
 * Reference: Mallett & Yuksel. "Spectral Primary Decomposition for Rendering with sRGB Reflectance" (2019)
 * ctx: context (for allocation)
 * rgb: input RGB values (assumed to be in sRGB colorspace)
 * out_spd: output spectral power distribution (wavelength range: 380-780nm, 81 samples at 5nm intervals)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
int alwan_rgb_to_spectrum_mallett2019_f64(alwan_spd_f64 *out_spd, alwan_rgb_f64 const *rgb, alwan_ctx *ctx);
int alwan_rgb_to_spectrum_mallett2019_f32(alwan_spd_f32 *out_spd, alwan_rgb_f32 const *rgb, alwan_ctx *ctx);

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
int alwan_rgb_to_spectrum_jakob2019_f64(alwan_spd_f64 *out_spd, alwan_jakob2019_gamut gamut, alwan_rgb_f64 const *rgb, alwan_ctx *ctx);
int alwan_rgb_to_spectrum_jakob2019_f32(alwan_spd_f32 *out_spd, alwan_jakob2019_gamut gamut, alwan_rgb_f32 const *rgb, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * CIECAM02 Color Appearance Model
 * ---------------------------------------------------------------- */

/* CIECAM02 forward transform: XYZ -> appearance correlates
 * xyz: input color in XYZ (same white point as viewing conditions)
 * vc: viewing conditions
 * out: output appearance correlates (J, C, h, s, Q, M, H)
 * Returns ALWAN_OK on success */
int alwan_ciecam02_forward_f32(alwan_ciecam02_correlates_f32 *out,
                                alwan_xyz_f32 const *xyz,
                                alwan_ciecam02_viewing_conditions_f32 const *vc);
int alwan_ciecam02_forward_f64(alwan_ciecam02_correlates_f64 *out,
                                alwan_xyz_f64 const *xyz,
                                alwan_ciecam02_viewing_conditions_f64 const *vc);

/* CIECAM02 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
int alwan_ciecam02_inverse_f32(alwan_xyz_f32 *xyz_out,
                                alwan_ciecam02_correlates_f32 const *correlates,
                                alwan_ciecam02_viewing_conditions_f32 const *vc);
int alwan_ciecam02_inverse_f64(alwan_xyz_f64 *xyz_out,
                                alwan_ciecam02_correlates_f64 const *correlates,
                                alwan_ciecam02_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * CAM16 Color Appearance Model
 * ---------------------------------------------------------------- */

/* CAM16 forward transform: XYZ -> appearance correlates
 * xyz: input color in XYZ (same white point as viewing conditions)
 * vc: viewing conditions
 * out: output appearance correlates (J, C, h, s, Q, M, H)
 * Returns ALWAN_OK on success */
int alwan_cam16_forward_f32(alwan_cam16_correlates_f32 *out,
                             alwan_xyz_f32 const *xyz,
                             alwan_cam16_viewing_conditions_f32 const *vc);
int alwan_cam16_forward_f64(alwan_cam16_correlates_f64 *out,
                             alwan_xyz_f64 const *xyz,
                             alwan_cam16_viewing_conditions_f64 const *vc);

/* CAM16 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
int alwan_cam16_inverse_f32(alwan_xyz_f32 *xyz_out,
                             alwan_cam16_correlates_f32 const *correlates,
                             alwan_cam16_viewing_conditions_f32 const *vc);
int alwan_cam16_inverse_f64(alwan_xyz_f64 *xyz_out,
                             alwan_cam16_correlates_f64 const *correlates,
                             alwan_cam16_viewing_conditions_f64 const *vc);

/* MapCIECAM02 forward transform
 * xyz_in: input XYZ colors (stride in_stride between consecutive colors)
 * count: number of colors to process
 * correlates_out: output appearance correlates (count elements)
 * Returns ALWAN_OK on success */
int alwan_ciecam02_forward_f32_map_interleave(alwan_ciecam02_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f32 const *vc, size_t count);
int alwan_ciecam02_forward_f64_map_interleave(alwan_ciecam02_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count);

/* MapCIECAM02 inverse transform
 * correlates_in: input appearance correlates (count elements)
 * xyz_out: output XYZ colors (stride out_stride between consecutive colors)
 * Returns ALWAN_OK on success */
int alwan_ciecam02_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f32 const *correlates_in, alwan_ciecam02_viewing_conditions_f32 const *vc, size_t count);
int alwan_ciecam02_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f64 const *correlates_in, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count);

/* MapCAM16 forward transform */
int alwan_cam16_forward_f32_map_interleave(alwan_cam16_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f32 const *vc, size_t count);
int alwan_cam16_forward_f64_map_interleave(alwan_cam16_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f64 const *vc, size_t count);

/* MapCAM16 inverse transform */
int alwan_cam16_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_cam16_correlates_f32 const *correlates_in, alwan_cam16_viewing_conditions_f32 const *vc, size_t count);
int alwan_cam16_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_cam16_correlates_f64 const *correlates_in, alwan_cam16_viewing_conditions_f64 const *vc, size_t count);

/* Typed CAM map functions (_ex variants) */
int alwan_ciecam02_forward_map_interleave_ex(alwan_ciecam02_correlates_f64 *correlates_out, void const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format in_fmt);
int alwan_ciecam02_inverse_map_interleave_ex(void *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f64 const *correlates_in, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format out_fmt);
int alwan_cam16_forward_map_interleave_ex(alwan_cam16_correlates_f64 *correlates_out, void const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format in_fmt);
int alwan_cam16_inverse_map_interleave_ex(void *xyz_out, size_t out_stride, alwan_cam16_correlates_f64 const *correlates_in, alwan_cam16_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format out_fmt);

/* CAM16-UCS (Uniform Color Space) transform for perceptual distance metrics
 * Converts CAM16 JMh to CAM16-UCS Jab for computing perceptual distances
 * correlates: input CAM16 correlates (J, M, h used)
 * Jab_out: output CAM16-UCS coordinates [J', a', b']
 * Returns ALWAN_OK on success */
int alwan_cam16_to_ucs_f32(alwan_cam_jab_f32 *Jab_out,
                            alwan_cam16_correlates_f32 const *correlates);
int alwan_cam16_to_ucs_f64(alwan_cam_jab_f64 *Jab_out,
                            alwan_cam16_correlates_f64 const *correlates);

/* Inverse CAM16-UCS transform
 * Converts CAM16-UCS Jab back to CAM16 JMh
 * Jab: input CAM16-UCS coordinates [J', a', b']
 * correlates_out: output CAM16 correlates (J, M, h filled; other fields set to 0)
 * Returns ALWAN_OK on success */
int alwan_cam16_from_ucs_f32(alwan_cam16_correlates_f32 *correlates_out,
                              alwan_cam_jab_f32 const *Jab);
int alwan_cam16_from_ucs_f64(alwan_cam16_correlates_f64 *correlates_out,
                              alwan_cam_jab_f64 const *Jab);

/* ----------------------------------------------------------------
 * ZCAM - HDR Color Appearance Model
 * Based on Safdar et al. (2021), uses Jzazbz color space
 * Supports HDR luminance range 0.001-10,000 cd/m^2
 * ---------------------------------------------------------------- */

/* ZCAM forward transform: XYZ -> appearance correlates
 * xyz: absolute XYZ tristimulus values (cd/m^2)
 * vc: viewing conditions
 * out: computed appearance correlates
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
int alwan_zcam_forward_f32(alwan_zcam_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_zcam_viewing_conditions_f32 const *vc);
int alwan_zcam_forward_f64(alwan_zcam_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_zcam_viewing_conditions_f64 const *vc);

/* ZCAM inverse transform: appearance correlates -> XYZ (approximate)
 * correlates: appearance correlates
 * vc: viewing conditions
 * xyz: output XYZ tristimulus values (cd/m^2)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments
 * Note: Inverse is approximate due to complexity */
int alwan_zcam_inverse_f32(alwan_xyz_f32 *xyz,
                            alwan_zcam_correlates_f32 const *correlates,
                            alwan_zcam_viewing_conditions_f32 const *vc);
int alwan_zcam_inverse_f64(alwan_xyz_f64 *xyz,
                            alwan_zcam_correlates_f64 const *correlates,
                            alwan_zcam_viewing_conditions_f64 const *vc);

/* ZCAM to UCS (Uniform Color Space) for color difference
 * correlates: input ZCAM correlates
 * Jab_out: output ZCAM-UCS coordinates [Jz, az, bz]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
int alwan_zcam_to_ucs_f32(alwan_jzazbz_f32 *Jab_out,
                           alwan_zcam_correlates_f32 const *correlates);
int alwan_zcam_to_ucs_f64(alwan_jzazbz_f64 *Jab_out,
                           alwan_zcam_correlates_f64 const *correlates);

/* ----------------------------------------------------------------
 * RLAB Color Appearance Model
 * Based on Fairchild (1993, 1996)
 * Cross-media color reproduction model
 * ---------------------------------------------------------------- */

/* RLAB forward transform: XYZ -> appearance correlates
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
int alwan_rlab_forward_f32(alwan_rlab_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_rlab_viewing_conditions_f32 const *vc);
int alwan_rlab_forward_f64(alwan_rlab_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_rlab_viewing_conditions_f64 const *vc);

/* RLAB inverse transform: appearance correlates -> XYZ
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
int alwan_rlab_inverse_f32(alwan_xyz_f32 *xyz,
                            alwan_rlab_correlates_f32 const *correlates,
                            alwan_rlab_viewing_conditions_f32 const *vc);
int alwan_rlab_inverse_f64(alwan_xyz_f64 *xyz,
                            alwan_rlab_correlates_f64 const *correlates,
                            alwan_rlab_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * Hunt Color Appearance Model
 * Based on Hunt (1991, 1995)
 * Comprehensive historical CAM
 * ---------------------------------------------------------------- */

/* Hunt forward transform: XYZ -> appearance correlates
 * Returns 0 on success, -1 on error
 * Note: Hunt inverse is not implemented due to extreme complexity */
int alwan_hunt_forward_f32(alwan_hunt_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_hunt_viewing_conditions_f32 const *vc);
int alwan_hunt_forward_f64(alwan_hunt_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_hunt_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * Hellwig2022 Color Appearance Model
 * Based on Hellwig and Fairchild (2022)
 * Improved CAM16 with Helmholtz-Kohlrausch effect support
 * ---------------------------------------------------------------- */

/* Hellwig2022 forward transform: XYZ -> appearance correlates */
int alwan_hellwig2022_forward_f32(alwan_hellwig2022_correlates_f32 *out,
                                   alwan_xyz_f32 const *xyz,
                                   alwan_hellwig2022_viewing_conditions_f32 const *vc);
int alwan_hellwig2022_forward_f64(alwan_hellwig2022_correlates_f64 *out,
                                   alwan_xyz_f64 const *xyz,
                                   alwan_hellwig2022_viewing_conditions_f64 const *vc);

/* Hellwig2022 inverse transform: appearance correlates -> XYZ */
int alwan_hellwig2022_inverse_f32(alwan_xyz_f32 *xyz_out,
                                   alwan_hellwig2022_correlates_f32 const *correlates,
                                   alwan_hellwig2022_viewing_conditions_f32 const *vc);
int alwan_hellwig2022_inverse_f64(alwan_xyz_f64 *xyz_out,
                                   alwan_hellwig2022_correlates_f64 const *correlates,
                                   alwan_hellwig2022_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * Kim2009 Color Appearance Model
 * Based on Kim, Weyrich and Kautz (2009)
 * Specialized for rendering applications
 * ---------------------------------------------------------------- */

/* Kim2009 forward transform: XYZ -> appearance correlates */
int alwan_kim2009_forward_f32(alwan_kim2009_correlates_f32 *out,
                               alwan_xyz_f32 const *xyz,
                               alwan_kim2009_viewing_conditions_f32 const *vc);
int alwan_kim2009_forward_f64(alwan_kim2009_correlates_f64 *out,
                               alwan_xyz_f64 const *xyz,
                               alwan_kim2009_viewing_conditions_f64 const *vc);

/* Kim2009 inverse transform: appearance correlates -> XYZ */
int alwan_kim2009_inverse_f32(alwan_xyz_f32 *xyz_out,
                               alwan_kim2009_correlates_f32 const *correlates,
                               alwan_kim2009_viewing_conditions_f32 const *vc);
int alwan_kim2009_inverse_f64(alwan_xyz_f64 *xyz_out,
                               alwan_kim2009_correlates_f64 const *correlates,
                               alwan_kim2009_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * LLAB Color Appearance Model
 * Based on Luo, Lo and Kuo (1996)
 * Cross-media color reproduction model
 * ---------------------------------------------------------------- */

/* LLAB forward transform: XYZ -> appearance correlates */
int alwan_llab_forward_f32(alwan_llab_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_llab_viewing_conditions_f32 const *vc);
int alwan_llab_forward_f64(alwan_llab_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_llab_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * ATD95 Color Vision Model
 * Based on Guth's ATD (1995)
 * Advanced temporal dynamics model
 * ---------------------------------------------------------------- */

/* ATD95 forward transform: XYZ -> correlates */
int alwan_atd95_forward_f32(alwan_atd95_correlates_f32 *out,
                             alwan_xyz_f32 const *xyz,
                             alwan_atd95_viewing_conditions_f32 const *vc);
int alwan_atd95_forward_f64(alwan_atd95_correlates_f64 *out,
                             alwan_xyz_f64 const *xyz,
                             alwan_atd95_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * Nayatani95 Color Appearance Model
 * Based on Nayatani et al. (1995)
 * Japanese color appearance model
 * ---------------------------------------------------------------- */

/* Nayatani95 forward transform: XYZ -> appearance correlates */
int alwan_nayatani95_forward_f32(alwan_nayatani95_correlates_f32 *out,
                                  alwan_xyz_f32 const *xyz,
                                  alwan_nayatani95_viewing_conditions_f32 const *vc);
int alwan_nayatani95_forward_f64(alwan_nayatani95_correlates_f64 *out,
                                  alwan_xyz_f64 const *xyz,
                                  alwan_nayatani95_viewing_conditions_f64 const *vc);

/* ----------------------------------------------------------------
 * M9: Convenience Color Models (HSV, HSL, CMY, CMYK, YCbCr)
 * ---------------------------------------------------------------- */

/* RGB <-> HSV conversions (all values in [0, 1])
 * Operates on encoded (display-referred) sRGB. For linear input, apply sRGB OETF first. */
int alwan_rgb_to_hsv_f32(alwan_hsv_f32 *hsv_out, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_hsv_f64(alwan_hsv_f64 *hsv_out, alwan_rgb_f64 const *rgb);
int alwan_hsv_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsv_f32 const *hsv);
int alwan_hsv_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsv_f64 const *hsv);

/* RGB <-> HSL conversions (all values in [0, 1])
 * Operates on encoded (display-referred) sRGB. For linear input, apply sRGB OETF first. */
int alwan_rgb_to_hsl_f32(alwan_hsl_f32 *hsl_out, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_hsl_f64(alwan_hsl_f64 *hsl_out, alwan_rgb_f64 const *rgb);
int alwan_hsl_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsl_f32 const *hsl);
int alwan_hsl_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsl_f64 const *hsl);

/* Linear sRGB <-> HSV conversions
 * Applies sRGB OETF/EOTF internally so the caller works in linear light. */
int alwan_linear_srgb_to_hsv_f32(alwan_hsv_f32 *hsv_out, alwan_rgb_f32 const *rgb);
int alwan_linear_srgb_to_hsv_f64(alwan_hsv_f64 *hsv_out, alwan_rgb_f64 const *rgb);
int alwan_hsv_to_linear_srgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsv_f32 const *hsv);
int alwan_hsv_to_linear_srgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsv_f64 const *hsv);

/* Linear sRGB <-> HSL conversions
 * Applies sRGB OETF/EOTF internally so the caller works in linear light. */
int alwan_linear_srgb_to_hsl_f32(alwan_hsl_f32 *hsl_out, alwan_rgb_f32 const *rgb);
int alwan_linear_srgb_to_hsl_f64(alwan_hsl_f64 *hsl_out, alwan_rgb_f64 const *rgb);
int alwan_hsl_to_linear_srgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsl_f32 const *hsl);
int alwan_hsl_to_linear_srgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsl_f64 const *hsl);

/*
 * RGB <-> HSP conversions (all values in [0, 1])
 * HSP: Hue, Saturation, Perceived brightness
 * Reference: Darel Rex Finley (2006), http://alienryderflex.com/hsp.html
 * P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights.
 * H and S identical to HSV. Used by DaVinci Resolve. */
int alwan_rgb_to_hsp_f32(alwan_hsp_f32 *hsp_out, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_hsp_f64(alwan_hsp_f64 *hsp_out, alwan_rgb_f64 const *rgb);
int alwan_hsp_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsp_f32 const *hsp);
int alwan_hsp_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsp_f64 const *hsp);

/*
 * RGB <-> HSPLog conversions (all values in [0, 1])
 * HSPLog: HSP with logarithmic saturation stretching.
 * S_log = log10(1 + 9*S), expanding low saturation values.
 * Designed for log/flat-encoded footage.
 * Inspired by Nobe Color Remap / DaVinci Resolve "HSP Log".
 * NOTE: No published specification exists; see alwan_types.h. */
int alwan_rgb_to_hsplog_f32(alwan_hsplog_f32 *hsplog_out, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_hsplog_f64(alwan_hsplog_f64 *hsplog_out, alwan_rgb_f64 const *rgb);
int alwan_hsplog_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsplog_f32 const *hsplog);
int alwan_hsplog_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsplog_f64 const *hsplog);

/*
 * RGB <-> HSY conversions (all values in [0, 1])
 * HSY: Hue, Saturation, Luma (weighted linear luma)
 * Reference: Kuzma Shapran "HCY" (chilliant.com); Krita KoColorConversions.cpp
 * Y = BT.601 weighted luma, S uses luma-aware max_sat remapping.
 * Used by DaVinci Resolve. */
int alwan_rgb_to_hsy_f32(alwan_hsy_f32 *hsy_out, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_hsy_f64(alwan_hsy_f64 *hsy_out, alwan_rgb_f64 const *rgb);
int alwan_hsy_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsy_f32 const *hsy);
int alwan_hsy_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsy_f64 const *hsy);

/* RGB <-> CMY conversions (all values in [0, 1]) */
int alwan_rgb_to_cmy_f32(alwan_cmy_f32 *cmy_out, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_cmy_f64(alwan_cmy_f64 *cmy_out, alwan_rgb_f64 const *rgb);
int alwan_cmy_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_cmy_f32 const *cmy);
int alwan_cmy_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_cmy_f64 const *cmy);

/* CMY <-> CMYK conversions (all values in [0, 1]) */
int alwan_cmy_to_cmyk_f32(alwan_cmyk_f32 *cmyk_out, alwan_cmy_f32 const *cmy);
int alwan_cmy_to_cmyk_f64(alwan_cmyk_f64 *cmyk_out, alwan_cmy_f64 const *cmy);
int alwan_cmyk_to_cmy_f32(alwan_cmy_f32 *cmy_out, alwan_cmyk_f32 const *cmyk);
int alwan_cmyk_to_cmy_f64(alwan_cmy_f64 *cmy_out, alwan_cmyk_f64 const *cmyk);

/* YCbCr standard identifiers */
typedef enum {
    ALWAN_YCBCR_BT601,    /* ITU-R BT.601 (SD) */
    ALWAN_YCBCR_BT709,    /* ITU-R BT.709 (HD) */
    ALWAN_YCBCR_BT2020    /* ITU-R BT.2020 (UHD) */
} alwan_ycbcr_standard;

/* RGB <-> YCbCr conversions (RGB in [0, 1], YCbCr full range [0, 1]) */
int alwan_rgb_to_ycbcr_f64(alwan_ycbcr_f64 *ycbcr_out, alwan_rgb_f64 const *rgb, alwan_ycbcr_standard standard);
int alwan_rgb_to_ycbcr_f32(alwan_ycbcr_f32 *ycbcr_out, alwan_rgb_f32 const *rgb, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_ycbcr_f64 const *ycbcr, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_ycbcr_f32 const *ycbcr, alwan_ycbcr_standard standard);

/* RGB <-> YcCbcCrc conversions (constant luminance, BT.2020)
 * bit_depth: 8, 10, 12, or 16 -- controls legal range scaling */
int alwan_rgb_to_yccbccrc_f32(alwan_yccbccrc_f32 *yccbccrc_out, alwan_rgb_f32 const *rgb, int bit_depth);
int alwan_rgb_to_yccbccrc_f64(alwan_yccbccrc_f64 *yccbccrc_out, alwan_rgb_f64 const *rgb, int bit_depth);
int alwan_yccbccrc_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_yccbccrc_f32 const *yccbccrc, int bit_depth);
int alwan_yccbccrc_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_yccbccrc_f64 const *yccbccrc, int bit_depth);

/* YCbCr legal <-> full range conversion
 * Converts between full-range [0,1] and legal/narrow range with proper chroma centering.
 * bit_depth: 8, 10, 12, or 16 */
int alwan_ycbcr_full_to_legal_f32(alwan_ycbcr_f32 *out, alwan_ycbcr_f32 const *in, int bit_depth);
int alwan_ycbcr_full_to_legal_f64(alwan_ycbcr_f64 *out, alwan_ycbcr_f64 const *in, int bit_depth);
int alwan_ycbcr_legal_to_full_f32(alwan_ycbcr_f32 *out, alwan_ycbcr_f32 const *in, int bit_depth);
int alwan_ycbcr_legal_to_full_f64(alwan_ycbcr_f64 *out, alwan_ycbcr_f64 const *in, int bit_depth);

/* RGB <-> YCoCg conversions (video compression, real-time graphics)
 * - Y: luma, Co: orange chrominance, Cg: green chrominance
 * - Reversible integer transform (exact round-trip with proper scaling)
 * - Used in H.264/AVC and video codecs */
int alwan_rgb_to_ycocg_f32(alwan_ycocg_f32 *ycocg_out, alwan_rgb_f32 const *rgb);
int alwan_rgb_to_ycocg_f64(alwan_ycocg_f64 *ycocg_out, alwan_rgb_f64 const *rgb);

/* ----------------------------------------------------------------
 * Relative Luminance (Y)
 * Computes Y = kr*R + kg*G + kb*B for a given standard or color space.
 * Input RGB must be linear (scene-referred). For encoded RGB, apply EOTF first.
 * ---------------------------------------------------------------- */

/* Luma/luminance standard identifiers */
typedef enum {
    ALWAN_LUMA_BT601,       /* ITU-R BT.601 (SD) */
    ALWAN_LUMA_BT709,       /* ITU-R BT.709 / sRGB (HD) */
    ALWAN_LUMA_BT2020,      /* ITU-R BT.2020 (UHD) */
    ALWAN_LUMA_ACES_AP1,    /* ACES AP1 / ACEScg */
    ALWAN_LUMA_ACES_AP0,    /* ACES AP0 / ACES2065-1 */
    ALWAN_LUMA_DISPLAY_P3,  /* Display P3 / P3-D65 */
    ALWAN_LUMA_DCI_P3,      /* DCI-P3 (theater) */
    ALWAN_LUMA_ADOBE_RGB,   /* Adobe RGB (1998) */
    ALWAN_LUMA_PROPHOTO_RGB /* ProPhoto RGB / ROMM RGB */
} alwan_luma_standard;

/* Per-pixel relative luminance from standard enum */
int alwan_relative_luminance_f64(alwan_f64 *Y_out,
                             alwan_rgb_f64 const *rgb,
                             alwan_luma_standard standard);
int alwan_relative_luminance_f32(alwan_f32 *Y_out,
                             alwan_rgb_f32 const *rgb,
                             alwan_luma_standard standard);

/* Per-pixel relative luminance from explicit coefficients */
int alwan_relative_luminance_kr_kb_f32(alwan_f32 *Y_out,
                                       alwan_rgb_f32 const *rgb,
                                       alwan_f32 kr, alwan_f32 kb);
int alwan_relative_luminance_kr_kb_f64(alwan_f64 *Y_out,
                                       alwan_rgb_f64 const *rgb,
                                       alwan_f64 kr, alwan_f64 kb);

/* Per-pixel relative luminance from RGB space descriptor (extracts Y row from NPM) */
int alwan_relative_luminance_space_f32(alwan_f32 *Y_out,
                                       alwan_rgb_f32 const *rgb,
                                       alwan_rgb_space_desc_f32 const *space);
int alwan_relative_luminance_space_f64(alwan_f64 *Y_out,
                                       alwan_rgb_f64 const *rgb,
                                       alwan_rgb_space_desc_f64 const *space);

/* Batch relative luminance (3-channel input, 1-channel output) */
int alwan_relative_luminance_f32_map_interleave(alwan_f32 *Y_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_luma_standard standard);
int alwan_relative_luminance_f64_map_interleave(alwan_f64 *Y_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_luma_standard standard);

int alwan_relative_luminance_space_f32_map_interleave(alwan_f32 *Y_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_space_desc_f32 const *space);
int alwan_relative_luminance_space_f64_map_interleave(alwan_f64 *Y_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_space_desc_f64 const *space);

/* Mapconvenience color model conversions */
int alwan_rgb_to_hsv_f32_map_interleave(alwan_f32 *hsv_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_hsv_f64_map_interleave(alwan_f64 *hsv_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_hsv_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsv_in, size_t in_stride, size_t count);
int alwan_hsv_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsv_in, size_t in_stride, size_t count);

int alwan_rgb_to_hsl_f32_map_interleave(alwan_f32 *hsl_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_hsl_f64_map_interleave(alwan_f64 *hsl_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_hsl_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsl_in, size_t in_stride, size_t count);
int alwan_hsl_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsl_in, size_t in_stride, size_t count);

/* Map HSP conversions */
int alwan_rgb_to_hsp_f32_map_interleave(alwan_f32 *hsp_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_hsp_f64_map_interleave(alwan_f64 *hsp_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_hsp_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsp_in, size_t in_stride, size_t count);
int alwan_hsp_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsp_in, size_t in_stride, size_t count);

/* Map HSPLog conversions */
int alwan_rgb_to_hsplog_f32_map_interleave(alwan_f32 *hsplog_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_hsplog_f64_map_interleave(alwan_f64 *hsplog_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_hsplog_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsplog_in, size_t in_stride, size_t count);
int alwan_hsplog_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsplog_in, size_t in_stride, size_t count);

/* Map HSY conversions */
int alwan_rgb_to_hsy_f32_map_interleave(alwan_f32 *hsy_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_hsy_f64_map_interleave(alwan_f64 *hsy_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_hsy_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsy_in, size_t in_stride, size_t count);
int alwan_hsy_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsy_in, size_t in_stride, size_t count);

/* Typed convenience HSV/HSL map functions (_ex variants) */
int alwan_rgb_to_hsv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsv_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsl_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsl_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Typed HSP/HSPLog/HSY map functions (_ex variants) */
int alwan_rgb_to_hsp_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsp_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsplog_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsplog_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsy_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsy_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsp_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsp_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsplog_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsplog_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsy_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsy_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map linear sRGB <-> HSV conversions */
int alwan_linear_srgb_to_hsv_f32_map_interleave(alwan_f32 *hsv_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_linear_srgb_to_hsv_f64_map_interleave(alwan_f64 *hsv_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_hsv_to_linear_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsv_in, size_t in_stride, size_t count);
int alwan_hsv_to_linear_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsv_in, size_t in_stride, size_t count);

/* Map linear sRGB <-> HSL conversions */
int alwan_linear_srgb_to_hsl_f32_map_interleave(alwan_f32 *hsl_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_linear_srgb_to_hsl_f64_map_interleave(alwan_f64 *hsl_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

int alwan_hsl_to_linear_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsl_in, size_t in_stride, size_t count);
int alwan_hsl_to_linear_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsl_in, size_t in_stride, size_t count);

/* Typed linear sRGB <-> HSV/HSL map functions (_ex variants) */
int alwan_linear_srgb_to_hsv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsv_to_linear_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_linear_srgb_to_hsl_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsl_to_linear_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_linear_srgb_to_hsv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsv_to_linear_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_linear_srgb_to_hsl_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsl_to_linear_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map CMY conversions */
int alwan_rgb_to_cmy_f32_map_interleave(alwan_f32 *cmy_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_cmy_f64_map_interleave(alwan_f64 *cmy_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);
int alwan_cmy_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *cmy_in, size_t in_stride, size_t count);
int alwan_cmy_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *cmy_in, size_t in_stride, size_t count);
int alwan_rgb_to_cmy_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_cmy_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map CMYK conversions (4-channel output/input) */
int alwan_cmy_to_cmyk_f32_map_interleave(alwan_f32 *cmyk_out, size_t out_stride, alwan_f32 const *cmy_in, size_t in_stride, size_t count);
int alwan_cmy_to_cmyk_f64_map_interleave(alwan_f64 *cmyk_out, size_t out_stride, alwan_f64 const *cmy_in, size_t in_stride, size_t count);
int alwan_cmyk_to_cmy_f32_map_interleave(alwan_f32 *cmy_out, size_t out_stride, alwan_f32 const *cmyk_in, size_t in_stride, size_t count);
int alwan_cmyk_to_cmy_f64_map_interleave(alwan_f64 *cmy_out, size_t out_stride, alwan_f64 const *cmyk_in, size_t in_stride, size_t count);
int alwan_cmy_to_cmyk_f32_map_planar(alwan_f32 *out_c, size_t out_stride, alwan_f32 *out_m, alwan_f32 *out_y, alwan_f32 *out_k, alwan_f32 const *in_c, size_t in_stride, alwan_f32 const *in_m, alwan_f32 const *in_y, size_t count);
int alwan_cmy_to_cmyk_f64_map_planar(alwan_f64 *out_c, size_t out_stride, alwan_f64 *out_m, alwan_f64 *out_y, alwan_f64 *out_k, alwan_f64 const *in_c, size_t in_stride, alwan_f64 const *in_m, alwan_f64 const *in_y, size_t count);
int alwan_cmyk_to_cmy_f32_map_planar(alwan_f32 *out_c, size_t out_stride, alwan_f32 *out_m, alwan_f32 *out_y, alwan_f32 const *in_c, size_t in_stride, alwan_f32 const *in_m, alwan_f32 const *in_y, alwan_f32 const *in_k, size_t count);
int alwan_cmyk_to_cmy_f64_map_planar(alwan_f64 *out_c, size_t out_stride, alwan_f64 *out_m, alwan_f64 *out_y, alwan_f64 const *in_c, size_t in_stride, alwan_f64 const *in_m, alwan_f64 const *in_y, alwan_f64 const *in_k, size_t count);
int alwan_cmy_to_cmyk_map_interleave_ex(void *cmyk_out, size_t out_stride, void const *cmy_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_cmyk_to_cmy_map_interleave_ex(void *cmy_out, size_t out_stride, void const *cmyk_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_cmy_to_cmyk_map_planar_ex(void *out_c, size_t out_stride, void *out_m, void *out_y, void *out_k, void const *in_c, size_t in_stride, void const *in_m, void const *in_y, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_cmyk_to_cmy_map_planar_ex(void *out_c, size_t out_stride, void *out_m, void *out_y, void const *in_c, size_t in_stride, void const *in_m, void const *in_y, void const *in_k, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map YCoCg conversions */
int alwan_rgb_to_ycocg_f32_map_interleave(alwan_f32 *ycocg_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_ycocg_f64_map_interleave(alwan_f64 *ycocg_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);
int alwan_ycocg_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *ycocg_in, size_t in_stride, size_t count);
int alwan_ycocg_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *ycocg_in, size_t in_stride, size_t count);
int alwan_rgb_to_ycocg_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_ycocg_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map HWB conversions */
int alwan_rgb_to_hwb_f32_map_interleave(alwan_f32 *hwb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
int alwan_rgb_to_hwb_f64_map_interleave(alwan_f64 *hwb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);
int alwan_hwb_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hwb_in, size_t in_stride, size_t count);
int alwan_hwb_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hwb_in, size_t in_stride, size_t count);
int alwan_hsv_to_hwb_f32_map_interleave(alwan_f32 *hwb_out, size_t out_stride, alwan_f32 const *hsv_in, size_t in_stride, size_t count);
int alwan_hsv_to_hwb_f64_map_interleave(alwan_f64 *hwb_out, size_t out_stride, alwan_f64 const *hsv_in, size_t in_stride, size_t count);
int alwan_hwb_to_hsv_f32_map_interleave(alwan_f32 *hsv_out, size_t out_stride, alwan_f32 const *hwb_in, size_t in_stride, size_t count);
int alwan_hwb_to_hsv_f64_map_interleave(alwan_f64 *hsv_out, size_t out_stride, alwan_f64 const *hwb_in, size_t in_stride, size_t count);
int alwan_rgb_to_hwb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hwb_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsv_to_hwb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hwb_to_hsv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map YCbCr conversions (with standard) */
int alwan_rgb_to_ycbcr_f32_map_interleave(alwan_f32 *ycbcr_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
int alwan_rgb_to_ycbcr_f64_map_interleave(alwan_f64 *ycbcr_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *ycbcr_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *ycbcr_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
int alwan_rgb_to_ycbcr_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);

/* Map YcCbcCrc conversions (with bit_depth) */
int alwan_rgb_to_yccbccrc_f32_map_interleave(alwan_f32 *yccbccrc_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, int bit_depth);
int alwan_rgb_to_yccbccrc_f64_map_interleave(alwan_f64 *yccbccrc_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, int bit_depth);
int alwan_yccbccrc_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *yccbccrc_in, size_t in_stride, size_t count, int bit_depth);
int alwan_yccbccrc_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *yccbccrc_in, size_t in_stride, size_t count, int bit_depth);
int alwan_rgb_to_yccbccrc_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
int alwan_yccbccrc_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);

/* Map YCbCr legal/full range conversions (with bit_depth) */
int alwan_ycbcr_full_to_legal_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int bit_depth);
int alwan_ycbcr_full_to_legal_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int bit_depth);
int alwan_ycbcr_legal_to_full_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int bit_depth);
int alwan_ycbcr_legal_to_full_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int bit_depth);
int alwan_ycbcr_full_to_legal_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
int alwan_ycbcr_legal_to_full_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);

void alwan_ycocg_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_ycocg_f32 const *ycocg);
void alwan_ycocg_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_ycocg_f64 const *ycocg);

/* ----------------------------------------------------------------
 * M10: Light Quality & CCT (Correlated Color Temperature)
 * ---------------------------------------------------------------- */

/* CCT estimation from chromaticity coordinates (xy) */
/* McCamy approximation: fast, ~2% accuracy above 2800K */
alwan_f32  alwan_cct_mccamy_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_mccamy_xy_f64(alwan_vec2_f64 const *xy);

/* Robertson method: accurate, iterative lookup against Planckian locus */
/* Returns CCT in Kelvin, or negative value on error */
alwan_f32  alwan_cct_robertson_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_robertson_xy_f64(alwan_vec2_f64 const *xy);

/* Hernandez-Andres 1999: accurate analytical formula
 * Valid range: 3000K - 50000K (extended formula for higher CCT)
 * Reference: Hernandez-Andres et al. (1999) */
alwan_f32  alwan_cct_hernandez_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_hernandez_xy_f64(alwan_vec2_f64 const *xy);

/* Kang 2002: CCT to xy chromaticity (forward transform)
 * Valid range: 1667K - 25000K
 * Reference: Kang et al. (2002) */
void alwan_cct_to_xy_kang_f32(alwan_vec2_f32 *xy_out, alwan_f32 cct);
void alwan_cct_to_xy_kang_f64(alwan_vec2_f64 *xy_out, alwan_f64 cct);

/* Kang 2002: xy chromaticity to CCT (inverse, uses Newton-Raphson)
 * Valid range: 1667K - 25000K
 * Reference: Kang et al. (2002) */
alwan_f32  alwan_cct_kang_xy_f32(alwan_vec2_f32 const *xy);
alwan_f64 alwan_cct_kang_xy_f64(alwan_vec2_f64 const *xy);

/* CRI (Color Rendering Index) Ra - average of 8 TCS samples */
/* Requires SPD (spectral power distribution) */
/* Returns CRI Ra value [0, 100], or negative on error */
alwan_f64 alwan_cri_ra_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
alwan_f32 alwan_cri_ra_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);

/* CQS (Color Quality Scale) - NIST metric using 15 saturated samples */
/* Returns CQS value [0, 100], or negative on error */
/* Note: Full implementation requires CMCCAT2000 CAT and VS sample data */
alwan_f64 alwan_cqs_calculate_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
alwan_f32 alwan_cqs_calculate_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);

/* TM-30 (IES Method) - Fidelity (Rf) using 99 CES samples */
/* Returns Rf value [0, 100], or negative on error */
/* Note: Full implementation requires CIECAM02 and 99 CES sample data */
alwan_f64 alwan_tm30_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
alwan_f32 alwan_tm30_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);

/* CIE 224:2017 Color Fidelity Index (Rf) using 99 CES samples */
/* Returns Rf value [0, 100], or negative on error */
/* Note: Uses same algorithm as TM-30 but per CIE 224:2017 standard */
alwan_f64 alwan_cie224_rf_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
alwan_f32 alwan_cie224_rf_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);

/* SSI (Spectral Similarity Index) - Academy/SMPTE ST 2122 */
/* Measures spectral similarity between test and reference light sources */
/* test_spd: test illuminant SPD
 * reference_spd: reference illuminant SPD
 * Returns SSI value [0, 100], where 100 = perfect match, or negative on error */
alwan_f64 alwan_ssi_calculate_f64(alwan_spd_f64 const *test_spd, alwan_spd_f64 const *reference_spd, alwan_ctx *ctx);
alwan_f32 alwan_ssi_calculate_f32(alwan_spd_f32 const *test_spd, alwan_spd_f32 const *reference_spd, alwan_ctx *ctx);

/* CIE Special Metamerism Index: Change in Illuminant */
/* Quantifies color mismatch when samples that match under reference illuminant are viewed under test illuminant */
/* sample_reflectance: reflectance spectrum of sample
 * reference_reflectance: reflectance spectrum of reference
 * reference_illuminant: illuminant under which samples match (e.g., D65)
 * test_illuminant: illuminant under which to evaluate mismatch (e.g., A)
 * observer: observer type (2Â° or 10Â°)
 * Returns metamerism index (Î”E*ab under test illuminant), or negative on error */
alwan_f64 alwan_metamerism_index_f64(alwan_spd_f64 const *sample_reflectance, alwan_spd_f64 const *reference_reflectance, alwan_spd_f64 const *reference_illuminant, alwan_spd_f64 const *test_illuminant, alwan_observer_type observer, alwan_ctx *ctx);
alwan_f32 alwan_metamerism_index_f32(alwan_spd_f32 const *sample_reflectance, alwan_spd_f32 const *reference_reflectance, alwan_spd_f32 const *reference_illuminant, alwan_spd_f32 const *test_illuminant, alwan_observer_type observer, alwan_ctx *ctx);

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

/* CVD simulation model selection */
typedef enum {
    ALWAN_CVD_MODEL_BRETTEL = 0,    /* Brettel, Vienot & Mollon 1997 (confusion lines) */
    ALWAN_CVD_MODEL_MACHADO = 1     /* Machado, Oliveira & Fernandes 2009 (cone shift) */
} alwan_cvd_model;

/* Simulate color vision deficiency (color blindness)
 * rgb_in: input linear RGB color [0, 1]
 * cvd_type: type of color vision deficiency
 * severity: severity of deficiency [0, 1] (1.0 = complete, 0.0 = normal vision)
 *          (only applies to anomalous trichromacy types)
 * rgb_out: output simulated RGB color as seen by person with CVD
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error
 * Algorithm: Brettel, Vienot & Mollon (1997) simulation using confusion lines */
int alwan_simulate_cvd_f32(alwan_rgb_f32 *rgb_out,
                           alwan_rgb_f32 const *rgb_in,
                           alwan_cvd_type cvd_type,
                           alwan_f32 severity);
int alwan_simulate_cvd_f64(alwan_rgb_f64 *rgb_out,
                           alwan_rgb_f64 const *rgb_in,
                           alwan_cvd_type cvd_type,
                           alwan_f64 severity);

/* CVD Simulation Batch Map Functions */
int alwan_simulate_cvd_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
int alwan_simulate_cvd_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
int alwan_simulate_protanopia_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 severity);
int alwan_simulate_protanopia_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 severity);
int alwan_simulate_deuteranopia_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 severity);
int alwan_simulate_deuteranopia_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 severity);
int alwan_simulate_tritanopia_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 severity);
int alwan_simulate_tritanopia_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 severity);
int alwan_simulate_cvd_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);
int alwan_simulate_protanopia_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
int alwan_simulate_deuteranopia_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
int alwan_simulate_tritanopia_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);

/* Machado 2009 CVD Simulation
 * Models anomalous trichromacy via cone spectral sensitivity shifting.
 * More physiologically accurate than Brettel for partial deficiency.
 * Uses precomputed sRGB->sRGB 3x3 matrices at 11 severity levels,
 * interpolated for continuous parameterization.
 *
 * Reference: Machado, Oliveira & Fernandes (2009), IEEE TVCG 15(6).
 *
 * cvd_type: PROTANOPIA/PROTANOMALY -> protan, DEUTERANOPIA/DEUTERANOMALY -> deutan,
 *           TRITANOPIA/TRITANOMALY -> tritan
 * severity: [0, 1] where 0 = normal vision, 1 = full dichromacy */
int alwan_simulate_cvd_machado_f32(alwan_rgb_f32 *rgb_out,
                                   alwan_rgb_f32 const *rgb_in,
                                   alwan_cvd_type cvd_type,
                                   alwan_f32 severity);
int alwan_simulate_cvd_machado_f64(alwan_rgb_f64 *rgb_out,
                                   alwan_rgb_f64 const *rgb_in,
                                   alwan_cvd_type cvd_type,
                                   alwan_f64 severity);

/* Model-selectable CVD simulation (dispatches to Brettel or Machado) */
int alwan_simulate_cvd_ex_f32(alwan_rgb_f32 *rgb_out,
                              alwan_rgb_f32 const *rgb_in,
                              alwan_cvd_type cvd_type,
                              alwan_f32 severity,
                              alwan_cvd_model model);
int alwan_simulate_cvd_ex_f64(alwan_rgb_f64 *rgb_out,
                              alwan_rgb_f64 const *rgb_in,
                              alwan_cvd_type cvd_type,
                              alwan_f64 severity,
                              alwan_cvd_model model);

/* Machado 2009 batch map functions */
int alwan_simulate_cvd_machado_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
int alwan_simulate_cvd_machado_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
int alwan_simulate_cvd_machado_f32_map_planar(alwan_f32 *out_r, size_t out_stride, alwan_f32 *out_g, alwan_f32 *out_b, alwan_f32 const *in_r, size_t in_stride, alwan_f32 const *in_g, alwan_f32 const *in_b, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
int alwan_simulate_cvd_machado_f64_map_planar(alwan_f64 *out_r, size_t out_stride, alwan_f64 *out_g, alwan_f64 *out_b, alwan_f64 const *in_r, size_t in_stride, alwan_f64 const *in_g, alwan_f64 const *in_b, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
int alwan_simulate_cvd_machado_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);
int alwan_simulate_cvd_machado_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);

/* Luminous Efficiency Functions */

/* Vision type for luminous efficiency */
typedef enum {
    ALWAN_VISION_PHOTOPIC = 0,  /* Photopic (daytime, cone-based) - V(lambda) */
    ALWAN_VISION_SCOTOPIC = 1,  /* Scotopic (nighttime, rod-based) - V'(lambda) */
    ALWAN_VISION_MESOPIC = 2    /* Mesopic (twilight, mixed rod/cone) */
} alwan_vision_type;

/* Get luminous efficiency for a given wavelength and vision type
 * wavelength: wavelength in nanometers [360, 830]
 * vision_type: photopic, scotopic, or mesopic
 * Returns luminous efficiency value [0, 1], or negative on error
 * Data: CIE photopic V(lambda) 1924/1988, CIE scotopic V'(lambda) 1951 */
alwan_f32 alwan_luminous_efficiency_f32(alwan_f32 wavelength, alwan_vision_type vision_type);
alwan_f64 alwan_luminous_efficiency_f64(alwan_f64 wavelength, alwan_vision_type vision_type);

/* Calculate photopic luminance from SPD
 * spd: spectral power distribution
 * Returns photopic luminance in cd/m^2 (K_m=683.002 lm/W), or negative on error
 * Uses CIE 1924 photopic V(lambda) via trapezoidal integration */
alwan_f64 alwan_photopic_luminance_f64(alwan_spd_f64 const *spd, alwan_ctx *ctx);
alwan_f32 alwan_photopic_luminance_f32(alwan_spd_f32 const *spd, alwan_ctx *ctx);

/* Calculate scotopic luminance from SPD
 * spd: spectral power distribution
 * Returns scotopic luminance in cd/m^2 (K_m'=1699.998 lm/W), or negative on error
 * Uses CIE 1951 scotopic V'(lambda) via trapezoidal integration */
alwan_f64 alwan_scotopic_luminance_f64(alwan_spd_f64 const *spd, alwan_ctx *ctx);
alwan_f32 alwan_scotopic_luminance_f32(alwan_spd_f32 const *spd, alwan_ctx *ctx);

/* Calculate mesopic luminance from SPD (CIE 191:2010)
 * spd: spectral power distribution
 * adaptation_level: photopic adaptation luminance in cd/m^2 [0.001, 10]
 * Returns mesopic luminance in cd/m^2, or negative on error
 * Adaptation coefficient m from Goodman et al. (2007);
 * L_mes = m*L_p + (1-m)*(K_m/K_m')*L_s */
alwan_f64 alwan_mesopic_luminance_f64(alwan_spd_f64 const *spd, alwan_f64 adaptation_level, alwan_ctx *ctx);
alwan_f32 alwan_mesopic_luminance_f32(alwan_spd_f32 const *spd, alwan_f32 adaptation_level, alwan_ctx *ctx);

/* Contrast Sensitivity Function (CSF) */

/* Calculate contrast sensitivity for spatial frequency (simplified model)
 * spatial_frequency: spatial frequency in cycles per degree [0.1, 60]
 * luminance: background luminance in cd/m^2 [0.01, 10000]
 * Returns contrast sensitivity (1/contrast_threshold), or negative on error
 * Uses simplified Barten CSF model (1999) */
alwan_f32 alwan_csf_f32(alwan_f32 spatial_frequency, alwan_f32 luminance);
alwan_f64 alwan_csf_f64(alwan_f64 spatial_frequency, alwan_f64 luminance);

/* ----------------------------------------------------------------
 * Barten 1999 Full Model - Contrast Sensitivity Functions
 * Reference: Barten (1999), colour-science implementation
 * ---------------------------------------------------------------- */

/* Pupil diameter using Barten (1999) method
 * L: Average luminance in cd/m^2
 * X_0: Angular size of object in degrees (x direction)
 * Y_0: Angular size of object in degrees (y direction), -1 to use X_0
 * Returns: Pupil diameter in millimeters */
alwan_f32 alwan_pupil_diameter_barten1999_f32(alwan_f32 L,
                                              alwan_f32 X_0,
                                              alwan_f32 Y_0);
alwan_f64 alwan_pupil_diameter_barten1999_f64(alwan_f64 L,
                                              alwan_f64 X_0,
                                              alwan_f64 Y_0);

/* Retinal illuminance using Barten (1999) method
 * L: Average luminance in cd/m^2
 * d: Pupil diameter in millimeters
 * apply_stiles_crawford: Whether to apply Stiles-Crawford correction (1=yes, 0=no)
 * Returns: Retinal illuminance in Trolands */
alwan_f32 alwan_retinal_illuminance_barten1999_f32(alwan_f32 L,
                                                   alwan_f32 d,
                                                   int apply_stiles_crawford);
alwan_f64 alwan_retinal_illuminance_barten1999_f64(alwan_f64 L,
                                                   alwan_f64 d,
                                                   int apply_stiles_crawford);

/* Optical MTF (Modulation Transfer Function) using Barten (1999) method
 * u: Spatial frequency in cycles per degree
 * sigma: Standard deviation of line-spread function (use alwan_sigma_barten1999_f64)
 * Returns: Optical MTF value [0, 1] */
alwan_f32 alwan_optical_mtf_barten1999_f32(alwan_f32 u, alwan_f32 sigma);
alwan_f64 alwan_optical_mtf_barten1999_f64(alwan_f64 u, alwan_f64 sigma);

/* Standard deviation of line-spread function using Barten (1999) method
 * sigma_0: Constant sigma_0 in degrees (default: 0.5/60 = 0.00833...)
 * C_ab: Spherical aberration in degrees/mm (default: 0.08/60 = 0.00133...)
 * d: Pupil diameter in millimeters
 * Returns: Standard deviation sigma in degrees */
alwan_f32 alwan_sigma_barten1999_f32(alwan_f32 sigma_0,
                                     alwan_f32 C_ab,
                                     alwan_f32 d);
alwan_f64 alwan_sigma_barten1999_f64(alwan_f64 sigma_0,
                                     alwan_f64 C_ab,
                                     alwan_f64 d);

/* Maximum angular size using Barten (1999) method
 * u: Spatial frequency in cycles per degree
 * X_0: Angular size of object in degrees
 * X_max: Maximum angular size of integration area in degrees (default: 12)
 * N_max: Maximum number of integration cycles (default: 15)
 * Returns: Maximum angular size in degrees */
alwan_f32 alwan_maximum_angular_size_barten1999_f32(alwan_f32 u,
                                                    alwan_f32 X_0,
                                                    alwan_f32 X_max,
                                                    alwan_f32 N_max);
alwan_f64 alwan_maximum_angular_size_barten1999_f64(alwan_f64 u,
                                                    alwan_f64 X_0,
                                                    alwan_f64 X_max,
                                                    alwan_f64 N_max);

/* Initialize CSF parameters with defaults
 * Defaults valid for standard observer, age 20-30, good vision */
void alwan_csf_barten1999_params_default_f32(alwan_csf_barten1999_params_f32 *params);
void alwan_csf_barten1999_params_default_f64(alwan_csf_barten1999_params_f64 *params);

/* Full Barten (1999) CSF using all parameters
 * u: Spatial frequency in cycles per degree
 * params: Model parameters (use NULL for defaults)
 * Returns: Contrast sensitivity S */
alwan_f32 alwan_csf_barten1999_f32(alwan_f32 u,
                                   alwan_csf_barten1999_params_f32 const *params);
alwan_f64 alwan_csf_barten1999_f64(alwan_f64 u,
                                   alwan_csf_barten1999_params_f64 const *params);

/* Utility functions (alwan_min, alwan_max, alwan_min3, alwan_max3,
 * alwan_clamp, alwan_saturate, alwan_lerp) are defined in alwan_platform.h */

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
int alwan_interpolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                       alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                       alwan_interp_method method);
int alwan_interpolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                       alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
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
int alwan_extrapolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                       alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                       alwan_extrap_method method);
int alwan_extrapolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                       alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                       alwan_extrap_method method);

/* CCT and Duv Optimization
 * Computes Correlated Color Temperature (CCT) and distance from Planckian locus (Duv)
 * using iterative least-squares optimization
 * xy: CIE 1931 xy chromaticity coordinates
 * cct_out: receives CCT in Kelvin
 * duv_out: receives Duv (distance from Planckian locus, can be NULL)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if xy is invalid
 * Accuracy: CCT <= 1K, Duv <= 0.0001 */
int alwan_cct_duv_optimize_f64(alwan_f64 *cct_out, alwan_f64 *duv_out, alwan_vec2_f64 const *xy);
int alwan_cct_duv_optimize_f32(alwan_f32 *cct_out, alwan_f32 *duv_out, alwan_vec2_f32 const *xy);

/* Tristimulus Optimization
 * Finds a spectral power distribution that matches target XYZ tristimulus values
 * target_xyz: target XYZ tristimulus values
 * observer: observer type (e.g., CIE 1931 2Â°)
 * ctx: context for SPD allocation
 * spd_out: receives optimized SPD (must be pre-allocated with desired wavelength range)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error
 * Note: Multiple SPDs can match the same XYZ (metamerism), this finds one solution */
int alwan_optimize_spectrum_for_xyz_f64(alwan_spd_f64 *spd_out, alwan_xyz_f64 const *target_xyz, alwan_observer_type observer, alwan_ctx *ctx);
int alwan_optimize_spectrum_for_xyz_f32(alwan_spd_f32 *spd_out, alwan_xyz_f32 const *target_xyz, alwan_observer_type observer, alwan_ctx *ctx);

/* 1D Table Interpolation
 * Interpolates a value from a 1D lookup table
 * table: 1D LUT array
 * size: number of elements in table
 * x: input coordinate [0, 1] (normalized)
 * method: interpolation method (LINEAR or CUBIC)
 * Returns interpolated value */
alwan_f64 alwan_table_interp_1d_f64(alwan_f64 const *table, size_t size,
                                    alwan_f64 x, alwan_interp_method method);
alwan_f32 alwan_table_interp_1d_f32(alwan_f32 const *table, size_t size,
                                    alwan_f32 x, alwan_interp_method method);

/* 3D Table Interpolation (Trilinear)
 * Interpolates RGB values from a 3D lookup table using trilinear method
 * table: 3D LUT array (R-major: table[r][g][b])
 * sizes: dimensions [size_r, size_g, size_b]
 * rgb_in: input RGB coordinates [0, 1] (normalized)
 * rgb_out: receives interpolated RGB values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_table_interp_3d_trilinear_f32(alwan_rgb_f32 *rgb_out,
                                     alwan_f32 const *table, size_t const sizes[3],
                                     alwan_rgb_f32 const *rgb_in);
int alwan_table_interp_3d_trilinear_f64(alwan_rgb_f64 *rgb_out,
                                     alwan_f64 const *table, size_t const sizes[3],
                                     alwan_rgb_f64 const *rgb_in);

/* 3D Table Interpolation (Tetrahedral)
 * Interpolates RGB values from a 3D lookup table using tetrahedral method
 * Tetrahedral is more accurate than trilinear for color transforms
 * table: 3D LUT array (R-major: table[r][g][b])
 * sizes: dimensions [size_r, size_g, size_b]
 * rgb_in: input RGB coordinates [0, 1] (normalized)
 * rgb_out: receives interpolated RGB values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_table_interp_3d_tetrahedral_f32(alwan_rgb_f32 *rgb_out,
                                       alwan_f32 const *table, size_t const sizes[3],
                                       alwan_rgb_f32 const *rgb_in);
int alwan_table_interp_3d_tetrahedral_f64(alwan_rgb_f64 *rgb_out,
                                       alwan_f64 const *table, size_t const sizes[3],
                                       alwan_rgb_f64 const *rgb_in);

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
int alwan_munsell_to_xyz_f64(alwan_xyz_f64 *xyz,
                         alwan_f64 hue, alwan_f64 value, alwan_f64 chroma,
                         alwan_illuminant illuminant);
int alwan_munsell_to_xyz_f32(alwan_xyz_f32 *xyz,
                         alwan_f32 hue, alwan_f32 value, alwan_f32 chroma,
                         alwan_illuminant illuminant);

/* Convert XYZ tristimulus values to Munsell notation (Hue, Value, Chroma)
 * xyz: XYZ tristimulus values
 * illuminant: illuminant for XYZ calculation
 * hue: receives Munsell hue [0, 100]
 * value: receives Munsell value [0, 10]
 * chroma: receives Munsell chroma [0, 20+]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_xyz_to_munsell_f64(alwan_f64 *hue, alwan_f64 *value, alwan_f64 *chroma,
                         alwan_xyz_f64 const *xyz, alwan_illuminant illuminant);
int alwan_xyz_to_munsell_f32(alwan_f32 *hue, alwan_f32 *value, alwan_f32 *chroma,
                         alwan_xyz_f32 const *xyz, alwan_illuminant illuminant);

/* Color Checker Data
 * Get XYZ tristimulus values for a Color Checker patch
 * type: Color Checker target type
 * illuminant: illuminant for XYZ calculation
 * patch_index: patch index [0, num_patches-1]
 * xyz: receives XYZ tristimulus values
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
int alwan_color_checker_data_f64(alwan_xyz_f64 *xyz,
                              alwan_colorchecker_type type, alwan_illuminant illuminant,
                              size_t patch_index);
int alwan_color_checker_data_f32(alwan_xyz_f32 *xyz,
                              alwan_colorchecker_type type, alwan_illuminant illuminant,
                              size_t patch_index);

/* Get number of patches in a Color Checker target
 * type: Color Checker target type
 * Returns number of patches, or 0 on error */
size_t alwan_color_checker_num_patches(alwan_colorchecker_type type);

/* NCS (Natural Color System) Data
 * Convert NCS notation to XYZ tristimulus values
 * ncs_notation: NCS notation string (e.g., "S 1050-Y90R")
 * xyz: receives XYZ tristimulus values (Y=0â€“100 scale, D65)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on parse error
 * Approximate: uses published elementary-hue chromaticities (HÃ¥rd & Sivik 1981)
 * with linear hue interpolation; does not reproduce the proprietary NCS atlas */
int alwan_ncs_to_xyz_f64(alwan_xyz_f64 *xyz, char const *ncs_notation);
int alwan_ncs_to_xyz_f32(alwan_xyz_f32 *xyz, char const *ncs_notation);

/* Convert XYZ tristimulus values to NCS notation
 * xyz: XYZ tristimulus values
 * ncs_notation: receives NCS notation string (allocated by caller)
 * notation_size: size of notation buffer (should be >= 32)
 * Returns ALWAN_E_INVALID â€” inverse requires the proprietary NCS colour atlas */
int alwan_xyz_to_ncs_f64(char *ncs_notation, size_t notation_size, alwan_xyz_f64 const *xyz);
int alwan_xyz_to_ncs_f32(char *ncs_notation, size_t notation_size, alwan_xyz_f32 const *xyz);

/* Additional RGB Space Definitions
 * Get RGB space primaries and white point by enum
 * space: RGB color space identifier
 * primaries: receives RGB primaries as xy chromaticities (3x2 matrix)
 * white_point: receives white point xy chromaticity
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
int alwan_rgb_space_by_enum_f64(alwan_f64 primaries[6], alwan_vec2_f64 *white_point, alwan_rgb_space space);
int alwan_rgb_space_by_enum_f32(alwan_f32 primaries[6], alwan_vec2_f32 *white_point, alwan_rgb_space space);

/* Get RGB space transfer functions
 * space: RGB color space identifier
 * oetf: receives OETF (Opto-Electronic Transfer Function)
 * eotf: receives EOTF (Electro-Optical Transfer Function)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
int alwan_rgb_space_get_tfs_f64(alwan_transfer_function *oetf, alwan_transfer_function *eotf, alwan_rgb_space space);
int alwan_rgb_space_get_tfs_f32(alwan_transfer_function *oetf, alwan_transfer_function *eotf, alwan_rgb_space space);

/* ----------------------------------------------------------------
 * Color Correction & Grading Tools
 * ---------------------------------------------------------------- */

/* Lift/Gamma/Gain (LGG) color correction
 * rgb_in: input RGB values (linear, [0,1] for normal range)
 * lift: lift adjustment per channel (shadows) - typical range [-1, 1]
 * gamma: gamma adjustment per channel (midtones) - typical range [0.0001, 10]
 * gain: gain adjustment per channel (highlights) - typical range [0, 2]
 * rgb_out: output RGB values
 * Formula: rgb_out = ((rgb_in + lift) ^ (1/gamma)) * gain
 * Returns ALWAN_OK on success */
void alwan_lgg_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_rgb_f32 const *lift,
                    alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain);
void alwan_lgg_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_rgb_f64 const *lift,
                    alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);

/* Color matrix grading preset types */
typedef enum {
    ALWAN_COLOR_MATRIX_SEPIA = 0,           /* Sepia tone effect */
    ALWAN_COLOR_MATRIX_VINTAGE,             /* Vintage look */
    ALWAN_COLOR_MATRIX_BLEACH_BYPASS,       /* Bleach bypass effect */
    ALWAN_COLOR_MATRIX_COOL,                /* Cool tone shift */
    ALWAN_COLOR_MATRIX_WARM,                /* Warm tone shift */
    ALWAN_COLOR_MATRIX_MONOCHROME,          /* Black and white */
    ALWAN_COLOR_MATRIX_NIGHT_VISION         /* Night vision look */
} alwan_color_matrix_preset_f64;
/* Historical naming kept; the enum is independent of precision. */
typedef alwan_color_matrix_preset_f64 alwan_color_matrix_preset_f32;

/* Apply color matrix transformation (custom or preset)
 * rgb_in: input RGB values
 * matrix_3x3: 3x3 color transformation matrix
 * rgb_out: output RGB values
 * Returns ALWAN_OK on success */
void alwan_color_matrix_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                              alwan_mat3x3_f32 const *matrix_3x3);
void alwan_color_matrix_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                              alwan_mat3x3_f64 const *matrix_3x3);

/* Get preset color grading matrix
 * preset: preset type from alwan_color_matrix_preset_f64
 * matrix_3x3: receives the preset matrix
 * Returns ALWAN_OK on success, ALWAN_E_INVALID for unknown preset */
int alwan_color_matrix_get_preset_f64(alwan_mat3x3_f64 *matrix_3x3, alwan_color_matrix_preset_f64 preset);
int alwan_color_matrix_get_preset_f32(alwan_mat3x3_f32 *matrix_3x3, alwan_color_matrix_preset_f32 preset);

/* Printer lights color correction (film-style)
 * rgb_in: input RGB values (linear)
 * red_lights: red printer light adjustment (0-50, default 25)
 * green_lights: green printer light adjustment (0-50, default 25)
 * blue_lights: blue printer light adjustment (0-50, default 25)
 * rgb_out: output RGB values
 * Each light unit represents approximately 0.025 log exposure change
 * Returns ALWAN_OK on success */
void alwan_printer_lights_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                alwan_f32 red_lights, alwan_f32 green_lights,
                                alwan_f32 blue_lights);
void alwan_printer_lights_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                alwan_f64 red_lights, alwan_f64 green_lights,
                                alwan_f64 blue_lights);

/* Color Correction Batch Map Functions */
int alwan_lgg_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f32 const *gain, alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *lift);
int alwan_lgg_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f64 const *gain, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *lift);
int alwan_lgg_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);
int alwan_color_matrix_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_mat3x3_f32 const *matrix);
int alwan_color_matrix_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_mat3x3_f64 const *matrix);
int alwan_color_matrix_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_mat3x3_f64 const *matrix);
int alwan_printer_lights_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 red_lights, alwan_f32 green_lights, alwan_f32 blue_lights);
int alwan_printer_lights_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);
int alwan_printer_lights_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);

/* ----------------------------------------------------------------
 * Camera Profiling / Polynomial Color Correction
 * Reference: Cheung et al. (2004), Finlayson et al. (2015)
 * ---------------------------------------------------------------- */

/* Cheung 2004 polynomial expansion terms
 * Values represent the number of terms in the expanded polynomial */
typedef enum {
    ALWAN_POLY_CHEUNG_3  = 3,   /* [R, G, B] */
    ALWAN_POLY_CHEUNG_4  = 4,   /* [R, G, B, 1] */
    ALWAN_POLY_CHEUNG_5  = 5,   /* [R, G, B, RG, 1] */
    ALWAN_POLY_CHEUNG_7  = 7,   /* [R, G, B, RG, RB, GB, 1] */
    ALWAN_POLY_CHEUNG_8  = 8,   /* [R, G, B, RG, RB, GB, RGB, 1] */
    ALWAN_POLY_CHEUNG_10 = 10,  /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, 1] */
    ALWAN_POLY_CHEUNG_11 = 11,  /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, 1] */
    ALWAN_POLY_CHEUNG_14 = 14,  /* [R, G, B, RG, RB, GB, R^2, G^2, B^2, RGB, R^2G, RG^2, R^2B, 1] */
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
int alwan_poly_expand_cheung2004_f64(alwan_f64 *out, alwan_rgb_f64 const *rgb,
                                  alwan_poly_cheung_terms terms);
int alwan_poly_expand_cheung2004_f32(alwan_f32 *out, alwan_rgb_f32 const *rgb,
                                  alwan_poly_cheung_terms terms);

/* Polynomial expansion - Finlayson 2015 method
 * rgb: input RGB triplet [0,1]
 * degree: polynomial degree (1-4)
 * root_poly: if true, use root-polynomial expansion
 * out: output array (size depends on degree: 3,6,10,15 for degrees 1,2,3,4)
 * out_size: receives actual output size
 * Returns ALWAN_OK on success */
int alwan_poly_expand_finlayson2015_f64(alwan_f64 *out, int *out_size,
                                     alwan_rgb_f64 const *rgb, int degree, int root_poly);
int alwan_poly_expand_finlayson2015_f32(alwan_f32 *out, int *out_size,
                                     alwan_rgb_f32 const *rgb, int degree, int root_poly);

/* Polynomial expansion - Vandermonde method
 * a: input array (typically RGB)
 * a_size: size of input array
 * degree: polynomial degree
 * out: output array
 * out_size: receives actual output size
 * Returns ALWAN_OK on success */
int alwan_poly_expand_vandermonde_f64(alwan_f64 *out, int *out_size,
                                   alwan_f64 const *a, int a_size, int degree);
int alwan_poly_expand_vandermonde_f32(alwan_f32 *out, int *out_size,
                                   alwan_f32 const *a, int a_size, int degree);

/* Compute colour correction matrix using Cheung 2004 method
 * M_T: test (measured) RGB values, Nx3 array (row-major)
 * M_R: reference RGB values, Nx3 array (row-major)
 * num_samples: number of color samples (N)
 * terms: polynomial expansion terms
 * matrix_out: receives the correction matrix (terms x 3, row-major)
 * Returns ALWAN_OK on success */
int alwan_colour_correction_matrix_cheung2004_f64(alwan_f64 *matrix_out,
                                               alwan_f64 const *M_T,
                                               alwan_f64 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms);
int alwan_colour_correction_matrix_cheung2004_f32(alwan_f32 *matrix_out,
                                               alwan_f32 const *M_T,
                                               alwan_f32 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms);

/* Apply colour correction using Cheung 2004 method
 * rgb: input RGB to correct
 * matrix: correction matrix from alwan_colour_correction_matrix_cheung2004_f64
 * terms: must match terms used to compute the matrix
 * rgb_out: corrected RGB output
 * Returns ALWAN_OK on success */
void alwan_colour_correct_cheung2004_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                     alwan_f32 const *matrix, alwan_poly_cheung_terms terms);
void alwan_colour_correct_cheung2004_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                                     alwan_f64 const *matrix, alwan_poly_cheung_terms terms);

/* Compute colour correction matrix using Finlayson 2015 method
 * M_T: test (measured) RGB values, Nx3 array (row-major)
 * M_R: reference RGB values, Nx3 array (row-major)
 * num_samples: number of color samples (N)
 * degree: polynomial degree (1-4)
 * root_poly: if true, use root-polynomial expansion
 * matrix_out: receives the correction matrix
 * matrix_size: receives matrix size
 * Returns ALWAN_OK on success */
int alwan_colour_correction_matrix_finlayson2015_f64(alwan_f64 *matrix_out, int *matrix_size,
                                                  alwan_f64 const *M_T,
                                                  alwan_f64 const *M_R,
                                                  int num_samples, int degree, int root_poly);
int alwan_colour_correction_matrix_finlayson2015_f32(alwan_f32 *matrix_out, int *matrix_size,
                                                  alwan_f32 const *M_T,
                                                  alwan_f32 const *M_R,
                                                  int num_samples, int degree, int root_poly);

/* Apply colour correction using Finlayson 2015 method
 * rgb: input RGB to correct
 * matrix: correction matrix from alwan_colour_correction_matrix_finlayson2015_f64
 * degree: must match degree used to compute the matrix
 * root_poly: must match root_poly used to compute the matrix
 * rgb_out: corrected RGB output
 * Returns ALWAN_OK on success */
void alwan_colour_correct_finlayson2015_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                        alwan_f32 const *matrix, int degree, int root_poly);
void alwan_colour_correct_finlayson2015_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                                        alwan_f64 const *matrix, int degree, int root_poly);

/* White balance multipliers from neutral gray measurement
 * Given a measured RGB value that should be neutral gray,
 * computes the multipliers to normalize it.
 * measured_gray: measured RGB of a neutral gray target
 * multipliers_out: receives RGB multipliers (normalized so min = 1.0)
 * Returns ALWAN_OK on success */
void alwan_white_balance_from_gray_f32(alwan_rgb_f32 *multipliers_out, alwan_rgb_f32 const *measured_gray);
void alwan_white_balance_from_gray_f64(alwan_rgb_f64 *multipliers_out, alwan_rgb_f64 const *measured_gray);

/* Apply white balance multipliers
 * rgb: input RGB
 * multipliers: RGB multipliers from alwan_white_balance_from_gray
 * rgb_out: white-balanced RGB output
 * Returns ALWAN_OK on success */
void alwan_white_balance_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                               alwan_rgb_f32 const *multipliers);
void alwan_white_balance_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                               alwan_rgb_f64 const *multipliers);

/* White Balance Batch Map Functions */
int alwan_white_balance_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f32 const *multipliers);
int alwan_white_balance_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f64 const *multipliers);
int alwan_white_balance_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *multipliers);

/* ----------------------------------------------------------------
 * Optical Phenomena - Rayleigh Scattering
 * Reference: Bodhaine et al. (1999), colour-science implementation
 * ---------------------------------------------------------------- */

/* Initialize atmosphere parameters with defaults
 * CO2: 300 ppm, T: 288.15 K, P: 101325 Pa, lat: 0, alt: 0 */
void alwan_atmosphere_params_default_f64(alwan_atmosphere_params_f64 *params);
void alwan_atmosphere_params_default_f32(alwan_atmosphere_params_f32 *params);

/* Rayleigh scattering cross section per molecule (sigma)
 * Van de Hulst (1957) method with Bodhaine et al. (1999) corrections
 * wavelength_nm: wavelength in nanometers
 * params: atmospheric parameters (use NULL for defaults, only CO2 and temp used)
 * Returns: cross section in cm^2 */
alwan_f64 alwan_rayleigh_cross_section_f64(alwan_f64 wavelength_nm,
                                           alwan_atmosphere_params_f64 const *params);
alwan_f32 alwan_rayleigh_cross_section_f32(alwan_f32 wavelength_nm,
                                           alwan_atmosphere_params_f32 const *params);

/* Rayleigh optical depth through atmosphere
 * Computes tau_R(lambda) using Bodhaine et al. (1999) method
 * wavelength_nm: wavelength in nanometers
 * params: atmospheric parameters (use NULL for defaults)
 * Returns: optical depth (dimensionless) */
alwan_f64 alwan_rayleigh_optical_depth_f64(alwan_f64 wavelength_nm,
                                           alwan_atmosphere_params_f64 const *params);
alwan_f32 alwan_rayleigh_optical_depth_f32(alwan_f32 wavelength_nm,
                                           alwan_atmosphere_params_f32 const *params);

/* Rayleigh scattering spectral distribution
 * Fills an array with Rayleigh optical depth values across a wavelength range.
 * wavelength_start: start wavelength in nm
 * wavelength_end: end wavelength in nm
 * wavelength_step: wavelength step in nm
 * params: atmospheric parameters (use NULL for defaults)
 * out: output array (must be large enough for (end-start)/step + 1 values)
 * out_count: receives the number of values written
 * Returns 0 on success, -1 on error */
int alwan_rayleigh_spd_f64(alwan_f64 wavelength_start, alwan_f64 wavelength_end,
                        alwan_f64 wavelength_step,
                        alwan_atmosphere_params_f64 const *params,
                        alwan_f64 *out, int *out_count);
int alwan_rayleigh_spd_f32(alwan_f32 wavelength_start, alwan_f32 wavelength_end,
                        alwan_f32 wavelength_step,
                        alwan_atmosphere_params_f32 const *params,
                        alwan_f32 *out, int *out_count);

/* ----------------------------------------------------------------
 * ACES Fixed Functions (RRT Components)
 * Reference: OpenColorIO, Academy Color Encoding System
 * ---------------------------------------------------------------- */

/* ACES RedMod03 - Red channel modification (RRT v0.3) */
void alwan_aces_redmod03_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_redmod03_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/* ACES RedMod10 - Red channel modification (RRT v1.0) */
void alwan_aces_redmod10_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_redmod10_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/* ACES Glow03 - Flare/glow effect (RRT v0.3) */
void alwan_aces_glow03_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_glow03_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/* ACES Glow10 - Flare/glow effect (RRT v1.0) */
void alwan_aces_glow10_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_glow10_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/* ACES DarkToDim10 - Surround compensation (RRT v1.0) */
void alwan_aces_dark_to_dim10_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_dark_to_dim10_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/* ACES GamutComp13 parameters */
typedef struct { alwan_f32  lim_cyan, lim_magenta, lim_yellow, thr_cyan, thr_magenta, thr_yellow, power; } alwan_aces_gamut_comp13_params_f32;
typedef struct { alwan_f64 lim_cyan, lim_magenta, lim_yellow, thr_cyan, thr_magenta, thr_yellow, power; } alwan_aces_gamut_comp13_params_f64;

/* Initialize GamutComp13 parameters with ACES 1.3 defaults */
void alwan_aces_gamut_comp13_params_default_f32(alwan_aces_gamut_comp13_params_f32 *params);
void alwan_aces_gamut_comp13_params_default_f64(alwan_aces_gamut_comp13_params_f64 *params);

/* ACES GamutComp13 - Gamut compression (ACES 1.3) */
void alwan_aces_gamut_comp13_f32(alwan_rgb_f32 *rgb_out,
                            alwan_rgb_f32 const *rgb_in,
                            alwan_aces_gamut_comp13_params_f32 const *params);
void alwan_aces_gamut_comp13_f64(alwan_rgb_f64 *rgb_out,
                            alwan_rgb_f64 const *rgb_in,
                            alwan_aces_gamut_comp13_params_f64 const *params);

/* ACES GamutComp13 Inverse - Gamut decompression (ACES 1.3) */
void alwan_aces_gamut_comp13_inv_f32(alwan_rgb_f32 *rgb_out,
                                 alwan_rgb_f32 const *rgb_in,
                                 alwan_aces_gamut_comp13_params_f32 const *params);
void alwan_aces_gamut_comp13_inv_f64(alwan_rgb_f64 *rgb_out,
                                 alwan_rgb_f64 const *rgb_in,
                                 alwan_aces_gamut_comp13_params_f64 const *params);

/* ----------------------------------------------------------------
 * Blue Light Artifact Fix (Neon Suppression) LMT
 *
 * A legacy LMT that fixes artifacts in bright saturated blues and reds
 * from cameras whose gamuts extend outside AP0 primaries.
 *
 * Note: Superseded by Reference Gamut Compression (alwan_aces_gamut_comp13)
 * in ACES 1.3+. Provided for compatibility with older workflows.
 *
 * Input/Output: AP0 linear (ACES2065-1)
 * ---------------------------------------------------------------- */

/**
 * @brief Apply Blue Light Artifact Fix (Neon Suppression) LMT
 * @param rgb_out Output AP0 linear color with blue light fix applied
 * @param rgb_in Input AP0 linear color
 * @return ALWAN_OK on success
 */
void alwan_aces_blue_light_fix_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_blue_light_fix_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/**
 * @brief Apply inverse Blue Light Artifact Fix
 * @param rgb_out Output AP0 linear color (original)
 * @param rgb_in Input AP0 linear color (with fix applied)
 * @return ALWAN_OK on success
 */
void alwan_aces_blue_light_fix_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_blue_light_fix_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/**
 * @brief Inverse of Glow03 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
void alwan_aces_glow03_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_glow03_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/**
 * @brief Inverse of Glow10 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
void alwan_aces_glow10_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_glow10_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/**
 * @brief Inverse of RedMod03 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
void alwan_aces_redmod03_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_redmod03_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/**
 * @brief Inverse of RedMod10 fixed function
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color
 * @return ALWAN_OK on success
 */
void alwan_aces_redmod10_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_redmod10_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

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
void alwan_aces_look_1_0_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_look_1_0_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/**
 * @brief Inverse of ACES 1.0 Look LMT
 * @param rgb_out Output AP1 linear color
 * @param rgb_in Input AP1 linear color (with ACES 1.0 look)
 * @return ALWAN_OK on success
 */
void alwan_aces_look_1_0_inv_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in);
void alwan_aces_look_1_0_inv_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in);

/**
 * @brief Parametric LMT parameters (CDL-style color grading)
 *
 * Applies: out = (in * slope + offset) ^ power, then saturation adjustment.
 * All parameters default to neutral (slope=1, offset=0, power=1, saturation=1).
 */
typedef struct { alwan_f32  slope[3], offset[3], power[3], saturation; } alwan_aces_lmt_params_f32;
typedef struct { alwan_f64 slope[3], offset[3], power[3], saturation; } alwan_aces_lmt_params_f64;

/**
 * @brief Initialize LMT params to neutral (identity transform)
 * @param params Output params struct
 */
void alwan_aces_lmt_params_init_f32(alwan_aces_lmt_params_f32 *params);
void alwan_aces_lmt_params_init_f64(alwan_aces_lmt_params_f64 *params);

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
void alwan_aces_lmt_apply_f32(alwan_rgb_f32 *rgb_out,
                         alwan_rgb_f32 const *rgb_in,
                         alwan_aces_lmt_params_f32 const *params);
void alwan_aces_lmt_apply_f64(alwan_rgb_f64 *rgb_out,
                         alwan_rgb_f64 const *rgb_in,
                         alwan_aces_lmt_params_f64 const *params);

/* ----------------------------------------------------------------
 * ACES 1.x Output Transforms (RRT + ODT)
 * Reference: Academy Color Encoding System v1.3
 * Note: These implement the complete RRT+ODT pipeline for ACES 1.x
 * ---------------------------------------------------------------- */

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
int alwan_aces1_output_transform_f32(alwan_rgb_f32 *rgb_out,
                                      alwan_rgb_f32 const *rgb_in,
                                      alwan_aces1_output output);
int alwan_aces1_output_transform_f64(alwan_rgb_f64 *rgb_out,
                                      alwan_rgb_f64 const *rgb_in,
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
int alwan_aces1_output_transform_inv_f32(alwan_rgb_f32 *rgb_out,
                                          alwan_rgb_f32 const *rgb_in,
                                          alwan_aces1_output output);
int alwan_aces1_output_transform_inv_f64(alwan_rgb_f64 *rgb_out,
                                          alwan_rgb_f64 const *rgb_in,
                                          alwan_aces1_output output);

/* Rec.2100 Surround adjustment */
void alwan_rec2100_surround_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_f32 gamma);
void alwan_rec2100_surround_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_f64 gamma);

/* ----------------------------------------------------------------
 * ACES 2.0 Components
 * ---------------------------------------------------------------- */

/* ACES TonescaleCompress20 - Tonescale compression (ACES 2.0) */
void alwan_aces_tonescale_compress20_f32(alwan_rgb_f32 *rgb_out,
                                     alwan_rgb_f32 const *rgb_in,
                                     alwan_f32 peak_luminance);
void alwan_aces_tonescale_compress20_f64(alwan_rgb_f64 *rgb_out,
                                     alwan_rgb_f64 const *rgb_in,
                                     alwan_f64 peak_luminance);

/* ACES RGB to JMh20 encoding primaries */
typedef struct { alwan_f32  red_x, red_y, green_x, green_y, blue_x, blue_y, white_x, white_y; } alwan_aces_primaries_f32;
typedef struct { alwan_f64 red_x, red_y, green_x, green_y, blue_x, blue_y, white_x, white_y; } alwan_aces_primaries_f64;

/* Initialize primaries with AP1 defaults */
void alwan_aces_primaries_ap1_default_f32(alwan_aces_primaries_f32 *primaries);
void alwan_aces_primaries_ap1_default_f64(alwan_aces_primaries_f64 *primaries);

/* ACES RGB to JMh20 - Convert to color appearance coordinates (ACES 2.0) */
void alwan_aces_rgb_to_jmh20_f32(alwan_vec3_f32 *jmh_out,
                            alwan_rgb_f32 const *rgb_in,
                            alwan_aces_primaries_f32 const *primaries);
void alwan_aces_rgb_to_jmh20_f64(alwan_vec3_f64 *jmh_out,
                            alwan_rgb_f64 const *rgb_in,
                            alwan_aces_primaries_f64 const *primaries);

/* ACES JMh to RGB20 - Convert from color appearance coordinates (ACES 2.0) */
void alwan_aces_jmh_to_rgb20_f32(alwan_rgb_f32 *rgb_out,
                            alwan_vec3_f32 const *jmh_in,
                            alwan_aces_primaries_f32 const *primaries);
void alwan_aces_jmh_to_rgb20_f64(alwan_rgb_f64 *rgb_out,
                            alwan_vec3_f64 const *jmh_in,
                            alwan_aces_primaries_f64 const *primaries);

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
void alwan_aces_gamut_compress20_f32(alwan_vec3_f32 *jmh_out,
                                 alwan_vec3_f32 const *jmh_in,
                                 alwan_f32 peak_luminance,
                                 alwan_aces_primaries_f32 const *limit_primaries);
void alwan_aces_gamut_compress20_f64(alwan_vec3_f64 *jmh_out,
                                 alwan_vec3_f64 const *jmh_in,
                                 alwan_f64 peak_luminance,
                                 alwan_aces_primaries_f64 const *limit_primaries);

/**
 * ACES 2.0: Gamut Compression (Inverse)
 * Expands JMh colors from display gamut back to scene-referred gamut.
 * Parameters same as forward function.
 */
void alwan_aces_gamut_compress20_inv_f32(alwan_vec3_f32 *jmh_out,
                                     alwan_vec3_f32 const *jmh_in,
                                     alwan_f32 peak_luminance,
                                     alwan_aces_primaries_f32 const *limit_primaries);
void alwan_aces_gamut_compress20_inv_f64(alwan_vec3_f64 *jmh_out,
                                     alwan_vec3_f64 const *jmh_in,
                                     alwan_f64 peak_luminance,
                                     alwan_aces_primaries_f64 const *limit_primaries);

/* ----------------------------------------------------------------
 * ACES 2.0 Output Transform (Unified API)
 * ---------------------------------------------------------------- */

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
int alwan_aces2_output_transform_f32(alwan_rgb_f32 *rgb_out,
                                      alwan_rgb_f32 const *rgb_in,
                                      alwan_aces2_output output);
int alwan_aces2_output_transform_f64(alwan_rgb_f64 *rgb_out,
                                      alwan_rgb_f64 const *rgb_in,
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
int alwan_aces2_output_transform_inv_f32(alwan_rgb_f32 *rgb_out,
                                          alwan_rgb_f32 const *rgb_in,
                                          alwan_aces2_output output);
int alwan_aces2_output_transform_inv_f64(alwan_rgb_f64 *rgb_out,
                                          alwan_rgb_f64 const *rgb_in,
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
int alwan_aces2_output_transform_custom_f32(alwan_rgb_f32 *rgb_out,
                                             alwan_rgb_f32 const *rgb_in,
                                             alwan_f32 peak_luminance,
                                             alwan_aces_primaries_f32 const *limit_primaries,
                                             alwan_transfer_function eotf);
int alwan_aces2_output_transform_custom_f64(alwan_rgb_f64 *rgb_out,
                                             alwan_rgb_f64 const *rgb_in,
                                             alwan_f64 peak_luminance,
                                             alwan_aces_primaries_f64 const *limit_primaries,
                                             alwan_transfer_function eotf);

/**
 * ACES 2.0 Output Transform (Batch)
 * Pre-initializes parameters once, then processes count pixels.
 * Much faster than calling alwan_aces2_output_transform per pixel.
 *
 * @param out       Output interleaved RGB triplets (display-encoded)
 * @param in        Input interleaved RGB triplets (AP1 linear)
 * @param output    Output transform preset
 * @param count     Number of pixels
 * @param in_stride  Bytes between input RGB triplets (typically 3*sizeof(alwan_f64))
 * @param out_stride Bytes between output RGB triplets
 * @return          ALWAN_OK on success
 */
int alwan_aces2_output_transform_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_aces2_output output);
int alwan_aces2_output_transform_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_aces2_output output);

/* ----------------------------------------------------------------
 * HDR Pipeline Utilities
 * ---------------------------------------------------------------- */

/* HLG OOTF: scene-to-display transform per BT.2100-2
 * Lw: nominal peak luminance (cd/m2), default 1000
 * gamma_sys: system gamma, default 1.2 */
void alwan_hlg_ootf_f32(alwan_rgb_f32 *out, alwan_rgb_f32 const *in,
                    alwan_f32 Lw, alwan_f32 gamma_sys);
void alwan_hlg_ootf_f64(alwan_rgb_f64 *out, alwan_rgb_f64 const *in,
                    alwan_f64 Lw, alwan_f64 gamma_sys);

/* HLG inverse OOTF: display-to-scene */
void alwan_hlg_ootf_inv_f32(alwan_rgb_f32 *out, alwan_rgb_f32 const *in,
                        alwan_f32 Lw, alwan_f32 gamma_sys);
void alwan_hlg_ootf_inv_f64(alwan_rgb_f64 *out, alwan_rgb_f64 const *in,
                        alwan_f64 Lw, alwan_f64 gamma_sys);

/* Maximum Content Light Level (scan for max R/G/B across all pixels) */
int alwan_maxcll_f32(alwan_f32 *maxcll_out, alwan_f32 const *rgb_in, size_t stride, size_t count);
int alwan_maxcll_f64(alwan_f64 *maxcll_out, alwan_f64 const *rgb_in, size_t stride, size_t count);

/* Maximum Frame Average Light Level (average of per-pixel max R/G/B) */
int alwan_maxfall_f32(alwan_f32 *maxfall_out, alwan_f32 const *rgb_in, size_t stride, size_t count);
int alwan_maxfall_f64(alwan_f64 *maxfall_out, alwan_f64 const *rgb_in, size_t stride, size_t count);

/* ----------------------------------------------------------------
 * Arbitrary Gamma Transfer Function
 * ---------------------------------------------------------------- */

/* Apply arbitrary gamma OETF: linear -> pow(linear, 1/gamma) */
int alwan_gamma_oetf_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 gamma);
int alwan_gamma_oetf_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 gamma);

/* Apply arbitrary gamma EOTF: encoded -> pow(encoded, gamma) */
int alwan_gamma_eotf_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 gamma);
int alwan_gamma_eotf_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 gamma);

/* ----------------------------------------------------------------
 * D-Series Illuminant from CCT
 * ---------------------------------------------------------------- */

/* Compute D-series illuminant xy chromaticity from CCT (4000-25000K)
 * xy_out: receives 2 values (x, y chromaticity)
 * cct: correlated color temperature in Kelvin */
int alwan_d_series_illuminant_xy_f64(alwan_vec2_f64 *xy_out, alwan_f64 cct);
int alwan_d_series_illuminant_xy_f32(alwan_vec2_f32 *xy_out, alwan_f32 cct);

/* ----------------------------------------------------------------
 * Weber & Michelson Contrast
 * ---------------------------------------------------------------- */

/* Weber contrast: (L_target - L_background) / L_background */
int alwan_weber_contrast_f64(alwan_f64 *result, alwan_f64 L_target, alwan_f64 L_bg);
int alwan_weber_contrast_f32(alwan_f32 *result, alwan_f32 L_target, alwan_f32 L_bg);

/* Michelson contrast: (L_max - L_min) / (L_max + L_min) */
int alwan_michelson_contrast_f64(alwan_f64 *result, alwan_f64 L_max, alwan_f64 L_min);
int alwan_michelson_contrast_f32(alwan_f32 *result, alwan_f32 L_max, alwan_f32 L_min);

/* ----------------------------------------------------------------
 * Accessibility Contrast Metrics
 * ---------------------------------------------------------------- */

/* WCAG 2.x Contrast Ratio: (L_lighter + 0.05) / (L_darker + 0.05)
 * Y1, Y2: relative luminances [0,1] of two colors
 * Returns contrast ratio [1, 21] (AA: >= 4.5, AAA: >= 7.0) */
int alwan_wcag_contrast_ratio_f32(alwan_f32 *result, alwan_f32 Y1, alwan_f32 Y2);
int alwan_wcag_contrast_ratio_f64(alwan_f64 *result, alwan_f64 Y1, alwan_f64 Y2);

/* APCA / SAPC (Advanced Perceptual Contrast Algorithm â€” WCAG 3.0 draft)
 * Reference: Myndex APCA-W3, https://github.com/Myndex/apca-w3
 * srgb_text, srgb_bg: sRGB-encoded colors (0..1 per channel)
 * Lc_out: perceptual contrast value (positive = dark on light,
 *         negative = light on dark; magnitude is contrast level) */
int alwan_apca_contrast_f32(alwan_f32 *Lc_out,
                         alwan_rgb_f32 const *srgb_text,
                         alwan_rgb_f32 const *srgb_bg);
int alwan_apca_contrast_f64(alwan_f64 *Lc_out,
                         alwan_rgb_f64 const *srgb_text,
                         alwan_rgb_f64 const *srgb_bg);

/* ----------------------------------------------------------------
 * HDR Ecosystem: BT.2446 Methods B & C, BT.2390 EETF
 * ---------------------------------------------------------------- */

/* BT.2446 Method B: SDR to HDR up-conversion (parametric)
 * Y_sdr: input SDR luminance [0,1]
 * L_hdr: target HDR peak luminance (cd/m2)
 * L_sdr: source SDR peak luminance (cd/m2) */
int alwan_bt2446b_forward_f32(alwan_f32 *Y_hdr_out, alwan_f32 Y_sdr,
                            alwan_f32 L_hdr, alwan_f32 L_sdr);
int alwan_bt2446b_forward_f64(alwan_f64 *Y_hdr_out, alwan_f64 Y_sdr,
                            alwan_f64 L_hdr, alwan_f64 L_sdr);

/* BT.2446 Method C: HDR to SDR tone mapping (quantization-aware)
 * Y_hdr: input HDR luminance (PQ-encoded) [0,1]
 * L_hdr: peak HDR luminance (cd/m2)
 * L_sdr: peak SDR luminance (cd/m2) */
int alwan_bt2446c_forward_f32(alwan_f32 *Y_sdr_out, alwan_f32 Y_hdr,
                            alwan_f32 L_hdr, alwan_f32 L_sdr);
int alwan_bt2446c_forward_f64(alwan_f64 *Y_sdr_out, alwan_f64 Y_hdr,
                            alwan_f64 L_hdr, alwan_f64 L_sdr);

/* BT.2390 EETF: PQ-domain tone mapping (Hermite spline)
 * E_pq: PQ-encoded input [0,1]
 * LB, LW: source black/white levels (PQ-encoded)
 * LB_target, LW_target: target black/white levels (PQ-encoded) */
int alwan_bt2390_eetf_f32(alwan_f32 *E_out, alwan_f32 E_pq,
                       alwan_f32 LB, alwan_f32 LW,
                       alwan_f32 LB_target, alwan_f32 LW_target);
int alwan_bt2390_eetf_f64(alwan_f64 *E_out, alwan_f64 E_pq,
                       alwan_f64 LB, alwan_f64 LW,
                       alwan_f64 LB_target, alwan_f64 LW_target);

/* BT.2390 EETF with luminance parameters (cd/m2) */
int alwan_bt2390_eetf_luminance_f32(alwan_f32 *E_out, alwan_f32 E_pq,
                                 alwan_f32 L_source_peak,
                                 alwan_f32 L_target_peak);
int alwan_bt2390_eetf_luminance_f64(alwan_f64 *E_out, alwan_f64 E_pq,
                                 alwan_f64 L_source_peak,
                                 alwan_f64 L_target_peak);

/* Exposure-based tone mapping: 1 - exp(-2^exposure * L)
 * exposure: EV offset (0 = neutral, +1 = 1 stop brighter) */
int alwan_exposure_tonemap_f32(alwan_f32 *out, alwan_f32 L,
                            alwan_f32 exposure);
int alwan_exposure_tonemap_f64(alwan_f64 *out, alwan_f64 L,
                            alwan_f64 exposure);

/* Reinhard calibrated (key-based with white point adaptation)
 * key: exposure key (0.18 = standard 18% gray)
 * L_avg: log-average luminance of the scene
 * L_white: smallest luminance mapped to pure white */
int alwan_reinhard_calibrated_f32(alwan_f32 *out, alwan_f32 L,
                               alwan_f32 key, alwan_f32 L_avg,
                               alwan_f32 L_white);
int alwan_reinhard_calibrated_f64(alwan_f64 *out, alwan_f64 L,
                               alwan_f64 key, alwan_f64 L_avg,
                               alwan_f64 L_white);

/* ----------------------------------------------------------------
 * HDR Gamut Mapping
 * ---------------------------------------------------------------- */

/* Chroma compression in JzCzhz (hue-preserving)
 * Cz_max: maximum chroma at the input's (Jz, hz) for target gamut */
void alwan_hdr_gamut_map_jzczhz_f32(alwan_jzczhz_f32 *out, alwan_jzczhz_f32 const *in,
                                alwan_f32 Cz_max);
void alwan_hdr_gamut_map_jzczhz_f64(alwan_jzczhz_f64 *out, alwan_jzczhz_f64 const *in,
                                alwan_f64 Cz_max);

/* ----------------------------------------------------------------
 * Display Characterization
 * ---------------------------------------------------------------- */

/* Peak luminance normalization for PQ signals
 * Clips PQ absolute values to display-specific peak, re-encodes
 * display_peak: display peak luminance (cd/m2) */
int alwan_pq_normalize_peak_f32(alwan_f32 *pq_out, alwan_f32 pq_value,
                              alwan_f32 display_peak);
int alwan_pq_normalize_peak_f64(alwan_f64 *pq_out, alwan_f64 pq_value,
                              alwan_f64 display_peak);

/* Initialize ST.2086 metadata from display parameters */
int alwan_st2086_init_f32(alwan_st2086_metadata_f32 *meta,
                       alwan_f32 const display_primaries_xy[6],
                       alwan_f32 const white_point_xy[2],
                       alwan_f32 max_luminance,
                       alwan_f32 min_luminance);
int alwan_st2086_init_f64(alwan_st2086_metadata_f64 *meta,
                       alwan_f64 const display_primaries_xy[6],
                       alwan_f64 const white_point_xy[2],
                       alwan_f64 max_luminance,
                       alwan_f64 min_luminance);

/* Compute content light level info from linear RGB pixel data (cd/m2) */
int alwan_content_light_level_compute_f32(alwan_content_light_level_f32 *cll_out, alwan_f32 const *rgb_in, size_t stride, size_t count);
int alwan_content_light_level_compute_f64(alwan_content_light_level_f64 *cll_out, alwan_f64 const *rgb_in, size_t stride, size_t count);

/* ----------------------------------------------------------------
 * HWB Color Space (CSS Color Level 4)
 * ---------------------------------------------------------------- */

/* RGB <-> HWB conversions (Hue [0-1], Whiteness [0-1], Blackness [0-1]) */
void alwan_rgb_to_hwb_f32(alwan_hwb_f32 *hwb_out, alwan_rgb_f32 const *rgb);
void alwan_rgb_to_hwb_f64(alwan_hwb_f64 *hwb_out, alwan_rgb_f64 const *rgb);
void alwan_hwb_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hwb_f32 const *hwb);
void alwan_hwb_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hwb_f64 const *hwb);

/* ----------------------------------------------------------------
 * Hero Wavelength Spectral Sampling
 * ---------------------------------------------------------------- */

/* Sample a single wavelength from uniform [0,1] -> [380,780] nm */
int alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u);
int alwan_hero_wavelength_sample_f32(alwan_f32 *lambda_out, alwan_f32 u);

/* Convert single wavelength to XYZ via Wyman 2013 analytic CMF fit */
void alwan_hero_wavelength_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_f32 lambda);
void alwan_hero_wavelength_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_f64 lambda);

/* Batch sampling with stratification: generates count wavelengths from seed
 * lambda_out: output wavelengths (count elements)
 * xyz_weights: output XYZ importance weights (count elements, can be NULL)
 * count: number of stratified wavelengths
 * seed: uniform [0,1] seed for hero wavelength */
int alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out,
                                 alwan_xyz_f64 *xyz_weights,
                                 size_t count,
                                 alwan_f64 seed);
int alwan_hero_wavelength_batch_f32(alwan_f32 *lambda_out,
                                 alwan_xyz_f32 *xyz_weights,
                                 size_t count,
                                 alwan_f32 seed);

/* ----------------------------------------------------------------
 * CAM18sl - Color Appearance Model for Self-Luminous Stimuli
 * Reference: Hermans et al. (2018)
 * ---------------------------------------------------------------- */

/* CAM18sl forward: XYZ -> appearance correlates
 * xyz: absolute XYZ tristimulus (cd/m2)
 * Y_b: background luminance (cd/m2) */
int alwan_cam18sl_forward_f32(alwan_cam18sl_correlates_f32 *out,
                          alwan_xyz_f32 const *xyz,
                          alwan_f32 Y_b);
int alwan_cam18sl_forward_f64(alwan_cam18sl_correlates_f64 *out,
                          alwan_xyz_f64 const *xyz,
                          alwan_f64 Y_b);

/* CAM18sl inverse: appearance correlates -> XYZ */
int alwan_cam18sl_inverse_f32(alwan_xyz_f32 *xyz_out,
                          alwan_cam18sl_correlates_f32 const *correlates,
                          alwan_f32 Y_b);
int alwan_cam18sl_inverse_f64(alwan_xyz_f64 *xyz_out,
                          alwan_cam18sl_correlates_f64 const *correlates,
                          alwan_f64 Y_b);

/* ----------------------------------------------------------------
 * CAM20u - Color Appearance Model for Unrelated Color
 * Reference: Kim & Park (2020)
 * ---------------------------------------------------------------- */

/* CAM20u forward: XYZ -> appearance correlates
 * xyz: absolute XYZ tristimulus (cd/m2)
 * Y_b: background luminance (cd/m2)
 * L_a: adapting luminance (cd/m2) */
int alwan_cam20u_forward_f32(alwan_cam20u_correlates_f32 *out,
                         alwan_xyz_f32 const *xyz,
                         alwan_f32 Y_b,
                         alwan_f32 L_a);
int alwan_cam20u_forward_f64(alwan_cam20u_correlates_f64 *out,
                         alwan_xyz_f64 const *xyz,
                         alwan_f64 Y_b,
                         alwan_f64 L_a);

/* CAM20u inverse: appearance correlates -> XYZ */
int alwan_cam20u_inverse_f32(alwan_xyz_f32 *xyz_out,
                         alwan_cam20u_correlates_f32 const *correlates,
                         alwan_f32 Y_b,
                         alwan_f32 L_a);
int alwan_cam20u_inverse_f64(alwan_xyz_f64 *xyz_out,
                         alwan_cam20u_correlates_f64 const *correlates,
                         alwan_f64 Y_b,
                         alwan_f64 L_a);

/* ----------------------------------------------------------------
 * Planar Map Functions (_map_planar)
 *
 * Process pixels stored as separate per-channel arrays (SoA layout).
 * Each channel is a separate pointer with per-sample stride.
 *
 * _map_planar:    alwan_f64 channel pointers
 * _map_planar_ex: void* channel pointers with pixel format
 * ---------------------------------------------------------------- */

/* sRGB convenience planar */
int alwan_srgb_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_srgb_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_xyz_to_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_xyz_to_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_srgb_to_lab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_srgb_to_lab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_lab_to_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_lab_to_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_srgb_to_oklab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_srgb_to_oklab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_oklab_to_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_oklab_to_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* sRGB convenience planar _ex */
int alwan_srgb_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_xyz_to_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_srgb_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_lab_to_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_srgb_to_oklab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_oklab_to_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);

/* Colorspace planar (with white_xyz) */
int alwan_xyz_to_lab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_lab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_lab_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_lab_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_luv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_luv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_luv_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_luv_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);

/* Colorspace planar (no extra params) */
int alwan_lab_to_lch_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_lab_to_lch_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_lch_to_lab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_lch_to_lab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_luv_to_lchuv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_luv_to_lchuv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_lchuv_to_luv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_lchuv_to_luv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_xyz_to_xyy_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_xyz_to_xyy_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_xyy_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_xyy_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Colorspace planar _ex */
int alwan_xyz_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_lab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_luv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_luv_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_lab_to_lch_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_lch_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_luv_to_lchuv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_lchuv_to_luv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_xyz_to_xyy_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_xyy_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);

/* Oklab planar */
int alwan_xyz_to_oklab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_xyz_to_oklab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_oklab_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_oklab_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_oklab_to_oklch_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_oklab_to_oklch_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_oklch_to_oklab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_oklch_to_oklab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Oklab planar _ex */
int alwan_xyz_to_oklab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_oklab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_oklab_to_oklch_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_oklch_to_oklab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* ICtCp planar (with use_pq) */
int alwan_rgb_to_ictcp_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
int alwan_rgb_to_ictcp_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);
int alwan_ictcp_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
int alwan_ictcp_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);
int alwan_xyz_to_ictcp_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
int alwan_xyz_to_ictcp_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);
int alwan_ictcp_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
int alwan_ictcp_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);

/* ICtCp planar _ex */
int alwan_rgb_to_ictcp_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
int alwan_ictcp_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
int alwan_xyz_to_ictcp_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
int alwan_ictcp_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);

/* JzAzBz planar */
int alwan_xyz_to_jzazbz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_xyz_to_jzazbz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_jzazbz_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_jzazbz_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_jzazbz_to_jzczhz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_jzazbz_to_jzczhz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_jzczhz_to_jzazbz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_jzczhz_to_jzazbz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* JzAzBz planar _ex */
int alwan_xyz_to_jzazbz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_jzazbz_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_jzazbz_to_jzczhz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_jzczhz_to_jzazbz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);

/* IPT planar */
int alwan_xyz_to_ipt_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_xyz_to_ipt_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_ipt_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_ipt_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* IPT planar _ex */
int alwan_xyz_to_ipt_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_ipt_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);

/* HSP/HSPlog/HSY planar */
int alwan_rgb_to_hsp_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_hsp_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsp_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsp_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_rgb_to_hsplog_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_hsplog_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsplog_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsplog_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_rgb_to_hsy_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_hsy_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsy_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsy_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* HSP/HSPlog/HSY planar _ex */
int alwan_rgb_to_hsp_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsp_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsplog_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsplog_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hsy_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsy_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Linear sRGB <-> HSV/HSL planar */
int alwan_linear_srgb_to_hsv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_linear_srgb_to_hsv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsv_to_linear_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsv_to_linear_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_linear_srgb_to_hsl_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_linear_srgb_to_hsl_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsl_to_linear_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsl_to_linear_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Linear sRGB <-> HSV/HSL planar _ex */
int alwan_linear_srgb_to_hsv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsv_to_linear_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_linear_srgb_to_hsl_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hsl_to_linear_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* HSV/HSL planar */
int alwan_rgb_to_hsv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_hsv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsv_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsv_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_rgb_to_hsl_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_hsl_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsl_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsl_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* HSV/HSL planar _ex */
int alwan_rgb_to_hsv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_hsv_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_rgb_to_hsl_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_hsl_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);

/* CMY/YCoCg/HWB planar */
int alwan_rgb_to_cmy_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_cmy_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_cmy_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_cmy_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_rgb_to_ycocg_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_ycocg_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_ycocg_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_ycocg_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_rgb_to_hwb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_rgb_to_hwb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hwb_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hwb_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hsv_to_hwb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hsv_to_hwb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
int alwan_hwb_to_hsv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
int alwan_hwb_to_hsv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* CMY/YCoCg/HWB planar _ex */
int alwan_rgb_to_cmy_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_cmy_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_rgb_to_ycocg_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_ycocg_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_rgb_to_hwb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_hwb_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_hsv_to_hwb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);
int alwan_hwb_to_hsv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format in_fmt, alwan_pixel_format out_fmt);

/* YCbCr planar */
int alwan_rgb_to_ycbcr_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_ycbcr_standard standard);
int alwan_rgb_to_ycbcr_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_ycbcr_standard standard);

/* YCbCr planar _ex */
int alwan_rgb_to_ycbcr_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);
int alwan_ycbcr_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);

/* YcCbcCrc / legal-full planar */
int alwan_rgb_to_yccbccrc_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
int alwan_rgb_to_yccbccrc_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);
int alwan_yccbccrc_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
int alwan_yccbccrc_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);
int alwan_ycbcr_full_to_legal_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
int alwan_ycbcr_full_to_legal_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);
int alwan_ycbcr_legal_to_full_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
int alwan_ycbcr_legal_to_full_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);

/* YcCbcCrc / legal-full planar _ex */
int alwan_rgb_to_yccbccrc_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
int alwan_yccbccrc_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
int alwan_ycbcr_full_to_legal_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
int alwan_ycbcr_legal_to_full_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);

/* Extended color spaces planar */
int alwan_xyz_to_igpgtg_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i2, size_t in_stride, alwan_f32 const *i0, alwan_f32 const *i1, size_t count);
int alwan_xyz_to_igpgtg_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_igpgtg_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_igpgtg_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_xyz_to_icacb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_xyz_to_icacb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_icacb_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_icacb_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_xyz_to_hdr_cielab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_xyz_to_hdr_cielab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_hdr_cielab_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_hdr_cielab_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_xyz_to_hdr_ipt_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_xyz_to_hdr_ipt_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_hdr_ipt_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_hdr_ipt_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_xyz_to_ucs_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_xyz_to_ucs_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_ucs_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_ucs_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_xyz_to_osa_ucs_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_xyz_to_osa_ucs_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_osa_ucs_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_osa_ucs_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_xyz_to_hunter_lab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_xyz_to_hunter_lab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_hunter_lab_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_hunter_lab_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_xyz_to_prolab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_xyz_to_prolab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_prolab_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_prolab_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_rgb_to_prismatic_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_rgb_to_prismatic_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_prismatic_to_rgb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_prismatic_to_rgb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_rgb_to_hcl_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_rgb_to_hcl_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_hcl_to_rgb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_hcl_to_rgb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_rgb_to_ihls_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_rgb_to_ihls_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
int alwan_ihls_to_rgb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
int alwan_ihls_to_rgb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);

/* Extended with white point planar */
int alwan_xyz_to_hunter_lab_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_hunter_lab_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_hunter_lab_to_xyz_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_hunter_lab_to_xyz_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_prolab_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_prolab_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_prolab_to_xyz_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_prolab_to_xyz_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_uvw_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_xyz_to_uvw_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
int alwan_uvw_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
int alwan_uvw_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);

/* DIN99 planar */
int alwan_lab_to_din99_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, int variant);
int alwan_lab_to_din99_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, int variant);
int alwan_din99_to_lab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, int variant);
int alwan_din99_to_lab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, int variant);

/* Extended planar _ex */
int alwan_xyz_to_igpgtg_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_igpgtg_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_icacb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_icacb_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_hdr_cielab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hdr_cielab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_hdr_ipt_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hdr_ipt_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_ucs_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_ucs_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_osa_ucs_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_osa_ucs_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_hunter_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hunter_lab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_xyz_to_prolab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_prolab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_prismatic_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_prismatic_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_hcl_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_hcl_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_rgb_to_ihls_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
int alwan_ihls_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Extended white point planar _ex */
int alwan_xyz_to_hunter_lab_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_hunter_lab_to_xyz_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_prolab_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_prolab_to_xyz_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_xyz_to_uvw_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
int alwan_uvw_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* DIN99 planar _ex */
int alwan_lab_to_din99_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);
int alwan_din99_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);

/* CVD simulation planar */
int alwan_simulate_cvd_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
int alwan_simulate_cvd_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
int alwan_simulate_protanopia_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 severity);
int alwan_simulate_protanopia_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 severity);
int alwan_simulate_deuteranopia_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 severity);
int alwan_simulate_deuteranopia_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 severity);
int alwan_simulate_tritanopia_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 severity);
int alwan_simulate_tritanopia_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 severity);

/* CVD simulation planar _ex */
int alwan_simulate_cvd_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);
int alwan_simulate_protanopia_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
int alwan_simulate_deuteranopia_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
int alwan_simulate_tritanopia_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);

/* Color correction planar */
int alwan_lgg_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_rgb_f32 const *lift, alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain);
int alwan_lgg_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);
int alwan_color_matrix_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_mat3x3_f32 const *matrix);
int alwan_color_matrix_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_mat3x3_f64 const *matrix);
int alwan_printer_lights_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 red_lights, alwan_f32 green_lights, alwan_f32 blue_lights);
int alwan_printer_lights_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);
int alwan_white_balance_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_rgb_f32 const *multipliers);
int alwan_white_balance_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_rgb_f64 const *multipliers);

/* Color correction planar _ex */
int alwan_lgg_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);
int alwan_color_matrix_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_mat3x3_f64 const *matrix);
int alwan_printer_lights_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);
int alwan_white_balance_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *multipliers);

/* ----------------------------------------------------------------
 * LUT Baking
 * ---------------------------------------------------------------- */

/* Bake a 3D LUT by sampling an RGB color space conversion pipeline.
 * out: buffer of size^3 * 3 values (R-fastest, then G, then B)
 * size: cube edge length (e.g. 17, 33, 65)
 * ctx: context for chromatic adaptation (may be NULL if white points match)
 * src_space: source RGB color space descriptor
 * dst_space: destination RGB color space descriptor
 * Returns ALWAN_OK on success */
int alwan_bake_3dlut_f32(alwan_f32 *out, int size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_ctx *ctx);
int alwan_bake_3dlut_f64(alwan_f64 *out, int size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_ctx *ctx);

/* Bake a 3D LUT with a view transform applied after color space conversion.
 * The pipeline is: EOTF(src) -> combined matrix -> view transform -> OETF(dst)
 * vt: view transform to apply (use -1 or cast for no transform)
 * Returns ALWAN_OK on success */
int alwan_bake_3dlut_view_f32(alwan_f32 *out, int size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform vt, alwan_ctx *ctx);
int alwan_bake_3dlut_view_f64(alwan_f64 *out, int size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform vt, alwan_ctx *ctx);

/* Bake a 1D LUT by sampling a transfer function.
 * out: buffer of size values
 * size: number of samples (e.g. 256, 1024, 4096)
 * tf: transfer function to sample
 * encode: non-zero for OETF (linear->encoded), zero for EOTF (encoded->linear)
 * Returns ALWAN_OK on success */
int alwan_bake_1dlut_f32(alwan_f32 *out, int size,
                     alwan_transfer_function tf,
                     int encode);
int alwan_bake_1dlut_f64(alwan_f64 *out, int size,
                     alwan_transfer_function tf,
                     int encode);

/* ----------------------------------------------------------------
 * 2D LUT (Flattened 3D LUT for GPU Textures)
 *
 * A size^3 3D LUT is flattened into a 2D image:
 *   width  = size * size (N blue slices side-by-side)
 *   height = size
 * Each slice is a constant-B plane; x = R, y = G.
 * Standard game engine convention (Unreal, Unity).
 * ---------------------------------------------------------------- */

/* Get the 2D image dimensions for a flattened 3D LUT.
 * size: 3D LUT edge length
 * width: output image width  (= size * size)
 * height: output image height (= size) */
void alwan_lut2d_dimensions_f32(int size, int *width, int *height);
void alwan_lut2d_dimensions_f64(int size, int *width, int *height);

/* Flatten a 3D LUT into a 2D image buffer.
 * out: buffer of (size*size) * size * 3 values (row-major, RGB interleaved)
 * lut3d: source 3D LUT (size^3 * 3, R-fastest)
 * size: cube edge length
 * Returns ALWAN_OK on success */
int alwan_lut3d_to_2d_f32(alwan_f32 *out,
                       alwan_f32 const *lut3d,
                       int size);
int alwan_lut3d_to_2d_f64(alwan_f64 *out,
                       alwan_f64 const *lut3d,
                       int size);

/* Unflatten a 2D image buffer back to a 3D LUT.
 * out: buffer of size^3 * 3 values (R-fastest)
 * lut2d: source 2D image (size*size width, size height, RGB interleaved)
 * size: cube edge length
 * Returns ALWAN_OK on success */
int alwan_lut2d_to_3d_f32(alwan_f32 *out,
                       alwan_f32 const *lut2d,
                       int size);
int alwan_lut2d_to_3d_f64(alwan_f64 *out,
                       alwan_f64 const *lut2d,
                       int size);

/* Bake directly into a 2D image buffer (convenience: bake 3D + flatten).
 * out: buffer of (size*size) * size * 3 values
 * size: cube edge length
 * Returns ALWAN_OK on success */
int alwan_bake_2dlut_f32(alwan_f32 *out, int size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_ctx *ctx);
int alwan_bake_2dlut_f64(alwan_f64 *out, int size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_ctx *ctx);

/* 1D LUT linear interpolation.
 * result: output interpolated value
 * lut: 1D LUT data (size entries)
 * t: input [0,1] coordinate
 * size: number of entries
 * Returns ALWAN_OK on success */
int alwan_lut1d_sample_f32(alwan_f32 *result,
                        alwan_f32 const *lut,
                        alwan_f32 t,
                        int size);
int alwan_lut1d_sample_f64(alwan_f64 *result,
                        alwan_f64 const *lut,
                        alwan_f64 t,
                        int size);

/* 2D LUT sampling (flattened 3D strip).
 * Trilinear interpolation on the 2D strip layout, matching GPU
 * texture sampling for game engine color grading LUTs.
 * result: output interpolated RGB
 * lut2d: 2D LUT data ((size*size) * size * 3, row-major RGB interleaved)
 * rgb: input [0,1] coordinate
 * size: cube edge length
 * Returns ALWAN_OK on success */
int alwan_lut2d_sample_f32(alwan_rgb_f32 *result,
                        alwan_f32 const *lut2d,
                        alwan_rgb_f32 const *rgb,
                        int size);
int alwan_lut2d_sample_f64(alwan_rgb_f64 *result,
                        alwan_f64 const *lut2d,
                        alwan_rgb_f64 const *rgb,
                        int size);

/* 3D LUT trilinear interpolation.
 * result: output interpolated RGB
 * lut: 3D LUT data (size^3 * 3, R-fastest)
 * rgb: input [0,1] coordinate
 * size: cube edge length
 * Returns ALWAN_OK on success */
int alwan_lut3d_sample_f32(alwan_rgb_f32 *result,
                        alwan_f32 const *lut,
                        alwan_rgb_f32 const *rgb,
                        int size);
int alwan_lut3d_sample_f64(alwan_rgb_f64 *result,
                        alwan_f64 const *lut,
                        alwan_rgb_f64 const *rgb,
                        int size);

/* ----------------------------------------------------------------
 * .cube File Import / Export
 * ---------------------------------------------------------------- */

/* Export a 3D LUT to a .cube file.
 * path: output file path
 * lut: 3D LUT data (size^3 * 3, R-fastest)
 * size: cube edge length
 * title: optional title string (NULL for no title) */
int alwan_cube_export_3d_f64(char const *path,
                          alwan_f64 const *lut,
                          int size,
                          char const *title);
int alwan_cube_export_3d_f32(char const *path,
                          alwan_f32 const *lut,
                          int size,
                          char const *title);

/* Export a 1D LUT to a .cube file.
 * path: output file path
 * lut: 1D LUT data (size values)
 * size: number of entries
 * title: optional title string (NULL for no title) */
int alwan_cube_export_1d_f64(char const *path,
                          alwan_f64 const *lut,
                          int size,
                          char const *title);
int alwan_cube_export_1d_f32(char const *path,
                          alwan_f32 const *lut,
                          int size,
                          char const *title);

/* Export a 3D LUT to a .cube format in a memory buffer.
 * buf: output buffer
 * buf_size: buffer capacity
 * bytes_written: actual bytes written (output)
 * Returns ALWAN_E_RANGE if buffer too small */
int alwan_cube_export_3d_buffer_f64(char *buf, size_t buf_size, size_t *bytes_written,
                                 alwan_f64 const *lut,
                                 int size,
                                 char const *title);
int alwan_cube_export_3d_buffer_f32(char *buf, size_t buf_size, size_t *bytes_written,
                                 alwan_f32 const *lut,
                                 int size,
                                 char const *title);

/* Import a 3D LUT from a .cube file.
 * lut: output buffer (caller must allocate: size^3 * 3 alwan_scalars)
 * out_size: receives the cube edge length
 * path: input file path */
int alwan_cube_import_3d_f64(alwan_f64 *lut, int *out_size,
                          char const *path);
int alwan_cube_import_3d_f32(alwan_f32 *lut, int *out_size,
                          char const *path);

/* Import a 1D LUT from a .cube file.
 * lut: output buffer (caller must allocate: size alwan_scalars)
 * out_size: receives the number of entries
 * path: input file path */
int alwan_cube_import_1d_f64(alwan_f64 *lut, int *out_size,
                          char const *path);
int alwan_cube_import_1d_f32(alwan_f32 *lut, int *out_size,
                          char const *path);

/* Import a 3D LUT from a .cube format memory buffer.
 * lut: output buffer (caller must allocate: size^3 * 3 alwan_scalars)
 * out_size: receives the cube edge length
 * buf: input buffer
 * buf_len: buffer length */
int alwan_cube_import_3d_buffer_f64(alwan_f64 *lut, int *out_size,
                                 char const *buf, size_t buf_len);
int alwan_cube_import_3d_buffer_f32(alwan_f32 *lut, int *out_size,
                                 char const *buf, size_t buf_len);

/* ----------------------------------------------------------------
 * Color Interop Forum â€” Interop ID Strings
 *
 * Bidirectional lookup between alwan_rgb_space enum values and
 * canonical Color Interop Forum string identifiers.
 * Reference: ASWF Color Interop Forum specification
 * ---------------------------------------------------------------- */

/* Parse a Color Interop Forum ID string to an alwan_rgb_space enum.
 * Returns ALWAN_OK on success, ALWAN_E_NODATA if ID not recognized. */
int alwan_interop_parse_f64(alwan_rgb_space *space, char const *id);
int alwan_interop_parse_f32(alwan_rgb_space *space, char const *id);

/* Get the Color Interop Forum ID string for an alwan_rgb_space enum.
 * Returns the canonical string, or NULL if the space has no interop ID. */
char const *alwan_interop_format(alwan_rgb_space space);

/* Get the total number of registered interop ID entries. */
size_t alwan_interop_count(void);

/* Get the interop entry at the given index (for enumeration).
 * space and id may be NULL if not needed.
 * Returns ALWAN_E_RANGE if index is out of bounds. */
int alwan_interop_entry_at_f64(alwan_rgb_space *space, char const **id, size_t index);
int alwan_interop_entry_at_f32(alwan_rgb_space *space, char const **id, size_t index);

/* ----------------------------------------------------------------
 * Color Interop Forum â€” float16 (half-alwan_f32) Conversion
 * ---------------------------------------------------------------- */

/* Convert float16 (IEEE 754 binary16) samples to float32.
 * out: output float32 buffer
 * in: input uint16_t buffer containing float16 bit patterns
 * count: number of samples */
int alwan_half_to_float_f64(alwan_f32 *out, alwan_uint16 const *in, size_t count);
int alwan_half_to_float_f32(alwan_f32 *out, alwan_uint16 const *in, size_t count);

/* Convert float32 samples to float16 (IEEE 754 binary16).
 * out: output uint16_t buffer for float16 bit patterns
 * in: input float32 buffer
 * count: number of samples */
int alwan_float_to_half_f64(alwan_uint16 *out, alwan_f32 const *in, size_t count);
int alwan_float_to_half_f32(alwan_uint16 *out, alwan_f32 const *in, size_t count);

/* ----------------------------------------------------------------
 * CLF (Common LUT Format) Export â€” SMPTE ST 2136-1:2024
 *
 * Serialize color space conversions as CLF XML for interchange
 * with OCIO, ACES, DaVinci Resolve, Baselight.
 * ---------------------------------------------------------------- */

/* Export a CLF file for a color space conversion.
 * Decomposes the conversion into ProcessNodes:
 *   EOTF (Exponent or LUT1D) -> Matrix (src RGB->XYZ) ->
 *   CAT Matrix (if needed) -> Matrix (XYZ->dst RGB) ->
 *   Range (gamut clamp) -> OETF (Exponent or LUT1D)
 *
 * path: output file path
 * ctx: context for chromatic adaptation (may be NULL if white points match)
 * src_space/dst_space: source and destination RGB space descriptors
 * id: CLF ProcessList id attribute (NULL for default)
 * name: CLF ProcessList name attribute (NULL to omit)
 * lut_size: resolution for baked LUT1D nodes (default 4096 if < 2) */
int alwan_clf_export_f64(char const *path, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
int alwan_clf_export_f32(char const *path, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* Export a CLF file with a view transform (tone mapping) included.
 * Same as alwan_clf_export_f64 but adds a LUT3D ProcessNode for the
 * view transform between the color space conversion and OETF. */
int alwan_clf_export_view_f64(char const *path, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
int alwan_clf_export_view_f32(char const *path, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* Export CLF to a memory buffer.
 * Returns ALWAN_E_RANGE if buffer too small. */
int alwan_clf_export_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
int alwan_clf_export_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* Export CLF with view transform to a memory buffer. */
int alwan_clf_export_view_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
int alwan_clf_export_view_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Video Signal Encoding / Decoding
 *
 * Combines transfer function + range scaling + quantization.
 * Pipeline: encode = linear -> OETF -> range scale -> quantize
 *           decode = dequantize -> range unscale -> EOTF -> linear
 * ---------------------------------------------------------------- */

/* Encode: linear RGB -> video signal (TF + range + quantization).
 * out: output buffer (3-channel packed pixels in out_fmt)
 * out_fmt: output pixel format (U8, U16, F32, F64)
 * rgb_linear: input linear RGB triplets (count * 3 alwan_scalars)
 * count: number of pixels
 * ctx: context for space descriptor lookup
 * space: RGB color space (determines OETF)
 * range: ALWAN_VIDEO_RANGE_FULL or ALWAN_VIDEO_RANGE_NARROW
 * bit_depth: video bit depth (8, 10, 12, 16); 0 = derive from out_fmt */
int alwan_video_encode_f64(void *out, alwan_pixel_format out_fmt, alwan_f64 const *rgb_linear, size_t count, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);
int alwan_video_encode_f32(void *out, alwan_pixel_format out_fmt, alwan_f32 const *rgb_linear, size_t count, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);

/* Decode: video signal -> linear RGB.
 * rgb_linear: output linear RGB triplets (count * 3 alwan_scalars)
 * in: input buffer (3-channel packed pixels in in_fmt)
 * in_fmt: input pixel format (U8, U16, F32, F64)
 * count: number of pixels
 * ctx: context for space descriptor lookup
 * space: RGB color space (determines EOTF)
 * range: ALWAN_VIDEO_RANGE_FULL or ALWAN_VIDEO_RANGE_NARROW
 * bit_depth: video bit depth (8, 10, 12, 16); 0 = derive from in_fmt */
int alwan_video_decode_f64(alwan_f64 *rgb_linear, void const *in, alwan_pixel_format in_fmt, size_t count, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);
int alwan_video_decode_f32(alwan_f32 *rgb_linear, void const *in, alwan_pixel_format in_fmt, size_t count, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ALWAN_H */
