/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ============================================================================
 * Parameter convention (v2.0 -- enforced by tools/api_convention_survey.py)
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
 *   - Avoid bare names like rgb_data, linear, encoded -- suffix with _in/_out.
 *   - Value-input pointers (alwan_xyz/alwan_rgb/alwan_mat3x3 const *) are
 *     KNOBS, not buffers -- they belong in the [extras] tail.
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
 *
 * Every status-returning function declares alwan_status. The two
 * predicate functions (alwan_is_within_pointer_gamut_*) return a
 * plain int boolean instead.
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
    ALWAN_PIXEL_F16 = 4   /* IEEE 754 binary16 (half-float)     */
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
    ALWAN_DATA_COLOR     = 0,  /* Color data -- apply color management */
    ALWAN_DATA_NON_COLOR = 1,  /* Non-color (normals, masks, displacement) -- pass through */
    ALWAN_DATA_UNKNOWN   = 2   /* Unknown -- application should decide */
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
    alwan_alloc_fn alloc_cb;          /* Optional custom allocator (NULL = default); set both callbacks or neither */
    alwan_free_fn  free_cb;           /* Optional custom deallocator (NULL = default) */
    char const *runtime_data_root;    /* Reserved: runtime data loading is not implemented and is not scheduled. Field is ignored. */
    uint32_t flags;                   /* Reserved for future use (must be 0) */
} alwan_config;

/* Create a new context with optional configuration.
 * Returns NULL when allocation fails, and also when the configuration cannot be meant: a
 * non-zero `flags`, or one of `alloc_cb` / `free_cb` without the other. */
alwan_ctx *alwan_create(alwan_config const *cfg);

/* Destroy context and release all resources */
void alwan_destroy(alwan_ctx *ctx);

/* Version of the linked library binary ("major.minor.patch").
 * Compiled into the library, not the header: when a dynamically loaded
 * alwan is older or newer than the headers you built against, this
 * reports the binary's own version (compare with ALWAN_VERSION_STRING). */
char const *alwan_version_string(void);

/* How the linked library was compiled. These switches change results, not only
 * speed, and they are fixed when alwan is built: an application compiled with other
 * values gets numbers it does not expect. Compare normalize_ranges with the
 * ALWAN_NORMALIZE_RANGES your own code sees. */
typedef struct {
    int version_major;
    int version_minor;
    int version_patch;
    int normalize_ranges;    /* ALWAN_NORMALIZE_RANGES: bounded channels reported on [0, 1] */
    int deterministic;       /* ALWAN_DETERMINISTIC: polynomial transcendentals, same bits everywhere */
    int with_f32;            /* the _f32 entry points are compiled */
    int with_f64;            /* the _f64 entry points are compiled */
    int data_tables_minimal; /* ALWAN_DATA_TABLES_MINIMAL: the switchable tables are out */
} alwan_build_info;

alwan_status alwan_get_build_info(alwan_build_info *info_out);

/* ACES 1.x tone curve method. A parameter of alwan_aces1_output_transform_{T}
 * and its maps, and a field of the context for the view transform (below).
 * Until 3.0.0 it was a process-wide global, alwan_set_aces_interp, which two
 * threads with different settings raced on and which survived every context.
 * The three values select different curve chains, and which chain runs also
 * depends on the output preset; docs/api/context.md has the table. The inverse
 * transform inverts the B-spline chain whatever the forward was given. */
typedef enum {
    ALWAN_ACES_INTERP_BSPLINE = 0,  /* Quadratic B-spline (Academy CTL reference) */
    ALWAN_ACES_INTERP_HERMITE = 1,  /* Legacy piecewise Hermite approximation */
    ALWAN_ACES_INTERP_OCIO = 2      /* OCIO GradingRGBCurve (monotone cubic Hermite, pixel-exact OCIO match) */
} alwan_aces_interp;

/* The method the view transform runs under ALWAN_VIEW_ACES_REC709
 * (alwan_view_transform_apply_{T}, _unclamped and the view maps). A new context
 * holds ALWAN_ACES_INTERP_BSPLINE, and a NULL context reads as that too.
 * ALWAN_E_INVALID for a NULL context or a method outside the enum, and the
 * field is then untouched. The direct alwan_aces1_output_transform_{T} calls
 * take the method as a parameter and do not read the context. */
alwan_status alwan_ctx_set_aces_interp(alwan_ctx *ctx, alwan_aces_interp method);
alwan_aces_interp alwan_ctx_get_aces_interp(alwan_ctx const *ctx);

/* ----------------------------------------------------------------
 * Math Types & Semantic Color Types
 * ---------------------------------------------------------------- */
#include "alwan_types.h"

/* ----------------------------------------------------------------
 * Data Loading
 *
 * NOTE: Only embedded mode (ALWAN_EMBED_DATA=1, the default) is supported.
 * Runtime mode (ALWAN_EMBED_DATA=0) is NOT implemented and is not scheduled.
 * Attempting to build with ALWAN_EMBED_DATA=0 will produce a compile-time
 * error in alwan_data.c.
 * ---------------------------------------------------------------- */

 /* Standard illuminant xy chromaticity data getters
 * Each returns 2 values: x, y chromaticity coordinates
 * In embedded mode: returns pointer to static data (no deallocation needed) */

/* Illuminant A (incandescent tungsten) */
alwan_status alwan_data_get_illuminant_a_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_a_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D50 (horizon daylight) */
alwan_status alwan_data_get_illuminant_d50_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d50_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D55 (mid-morning daylight) */
alwan_status alwan_data_get_illuminant_d55_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d55_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D60 (daylight) */
alwan_status alwan_data_get_illuminant_d60_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d60_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D65 (noon daylight) */
alwan_status alwan_data_get_illuminant_d65_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d65_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant E (equal energy) */
alwan_status alwan_data_get_illuminant_e_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_e_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant B (direct sunlight) */
alwan_status alwan_data_get_illuminant_b_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_b_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant C (average daylight) */
alwan_status alwan_data_get_illuminant_c_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_c_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Illuminant D75 (daylight 7500K) */
alwan_status alwan_data_get_illuminant_d75_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_d75_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* Get sRGB primaries (6 values: rx, ry, gx, gy, bx, by) */
alwan_status alwan_data_get_srgb_primaries_f64(alwan_f64 **data, size_t *count, alwan_ctx *ctx);
alwan_status alwan_data_get_srgb_primaries_f32(alwan_f32 **data, size_t *count, alwan_ctx *ctx);

/* NOTE: alwan_data_free_f64/f32 are declared only when ALWAN_EMBED_DATA=0.
 * Runtime mode is not implemented; this block exists for future use. */
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
alwan_status alwan_mat3_inv_f32(alwan_mat3x3_f32 *out, alwan_mat3x3_f32 const *m);
alwan_status alwan_mat3_inv_f64(alwan_mat3x3_f64 *out, alwan_mat3x3_f64 const *m);

/* Multiply matrix by vector: out = m * v */
void alwan_mat3_mulv_f32(alwan_vec3_f32 *out, alwan_mat3x3_f32 const *m, alwan_vec3_f32 const *v);
void alwan_mat3_mulv_f64(alwan_vec3_f64 *out, alwan_mat3x3_f64 const *m, alwan_vec3_f64 const *v);

/* Create identity matrix */
void alwan_mat3_identity_f32(alwan_mat3x3_f32 *out);
void alwan_mat3_identity_f64(alwan_mat3x3_f64 *out);

/* Compute the determinant of a 3x3 matrix */
alwan_f32  alwan_mat3_det_f32(alwan_mat3x3_f32 const *m);
alwan_f64 alwan_mat3_det_f64(alwan_mat3x3_f64 const *m);

/* Matrix-vector map multiplication: out[i] = m * in[i]
 * Transforms array of 3D vectors by the same matrix
 * vec_out: output vectors (stride out_stride between consecutive vectors)
 * out_stride: output stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * vec_in: input vectors (stride in_stride between consecutive vectors)
 * in_stride: input stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * count: number of vectors to transform
 * matrix: transformation matrix (applied to all vectors)
 * Returns ALWAN_OK on success */
alwan_status alwan_mat3_transform_f32_map_interleave(alwan_f32 *vec_out, size_t out_stride, alwan_f32 const *vec_in, size_t in_stride, size_t count, alwan_mat3x3_f32 const *matrix);
alwan_status alwan_mat3_transform_f64_map_interleave(alwan_f64 *vec_out, size_t out_stride, alwan_f64 const *vec_in, size_t in_stride, size_t count, alwan_mat3x3_f64 const *matrix);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_mat3_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_mat3x3_f32 const *matrix);
alwan_status alwan_mat3_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_mat3x3_f64 const *matrix);

/* Typed mat3 transform: accepts void* buffers with pixel format */
alwan_status alwan_mat3_transform_map_interleave_ex(void *vec_out, size_t out_stride, void const *vec_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_mat3x3_f64 const *matrix);

/* ----------------------------------------------------------------
 * Collect / Scatter utilities (typed <-> alwan_f64)
 * ---------------------------------------------------------------- */

/* Collect: load typed 3-channel pixels into alwan_f64 triplets */
alwan_status alwan_collect3_f64(alwan_f64 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt);
alwan_status alwan_collect3_f32(alwan_f32 *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format in_fmt);

/* Scatter: store alwan_f64 triplets into typed 3-channel pixels */
alwan_status alwan_scatter3_f64(void *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt);
alwan_status alwan_scatter3_f32(void *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt);

/* ----------------------------------------------------------------
 * RGB Color Spaces
 * ---------------------------------------------------------------- */

/* RGB color space identifiers.
 *
 * ABI CONTRACT: every enumerator below is pinned to an EXPLICIT integer value.
 * These values are part of the public ABI and are also used as positional
 * indices into the embedded matrix tables (see alwan_rgb_matrices_embedded.h,
 * whose array order MUST match this enum). DO NOT reorder existing entries or
 * reuse retired values -- only append new spaces with the next free value,
 * immediately before ALWAN_RGB_SPACE_COUNT.
 *
 * Banner groups below: standard/display -> ACES -> camera vendors (ARRI, RED,
 * Sony, FilmLight, Fujifilm, Nikon, DJI, GoPro) -> legacy broadcast ->
 * cinema/print -> wide-gamut/linear variants -> gamma-encoded -> HDR display.
 */
typedef enum {
    /* === Standard / display spaces === */
    ALWAN_RGB_SPACE_SRGB = 0,
    ALWAN_RGB_SPACE_BT709 = 1,
    ALWAN_RGB_SPACE_DISPLAY_P3 = 2,
    ALWAN_RGB_SPACE_BT2020 = 3,
    ALWAN_RGB_SPACE_ACES2065_1 = 4,
    ALWAN_RGB_SPACE_ACESCG = 5,
    ALWAN_RGB_SPACE_ACESPROXY = 6,

    /* ACES Family extensions */
    ALWAN_RGB_SPACE_ACESCC = 7,         /* ACES Color Correction */
    ALWAN_RGB_SPACE_ACESCCT = 8,        /* ACES Color Correction with toe */

    /* ARRI Camera Spaces */
    ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_3 = 9,
    ALWAN_RGB_SPACE_ARRI_WIDE_GAMUT_4 = 10,
    ALWAN_RGB_SPACE_ARRI_LOGC3 = 11,         /* ARRI LogC3 (WG3 primaries + LogC3 OETF) */
    ALWAN_RGB_SPACE_ARRI_LOGC4 = 12,         /* ARRI LogC4 (WG4 primaries + LogC4 OETF) */

    /* RED Camera Spaces (extended) */
    ALWAN_RGB_SPACE_REDCOLOR = 13,       /* RED Color 1 */
    ALWAN_RGB_SPACE_REDCOLOR2 = 14,      /* RED Color 2 */
    ALWAN_RGB_SPACE_REDCOLOR3 = 15,      /* RED Color 3 */
    ALWAN_RGB_SPACE_REDCOLOR4 = 16,      /* RED Color 4 */
    ALWAN_RGB_SPACE_DRAGONCOLOR = 17,    /* RED Dragon Color */
    ALWAN_RGB_SPACE_DRAGONCOLOR2 = 18,   /* RED Dragon Color 2 */
    ALWAN_RGB_SPACE_REDLOG = 19,         /* REDLog (REDWideGamutRGB primaries + REDLog OETF) */

    /* Sony Camera Spaces (extended) */
    ALWAN_RGB_SPACE_VENICE_S_GAMUT3 = 20,
    ALWAN_RGB_SPACE_VENICE_S_GAMUT3_CINE = 21,
    ALWAN_RGB_SPACE_S_LOG = 22,          /* S-Log (S-Gamut3 primaries + S-Log OETF) */
    ALWAN_RGB_SPACE_S_LOG2 = 23,         /* S-Log2 (S-Gamut3 primaries + S-Log2 OETF) */
    ALWAN_RGB_SPACE_S_LOG3 = 24,         /* S-Log3 (S-Gamut3 primaries + S-Log3 OETF) */

    /* Historical/Reference */
    ALWAN_RGB_SPACE_CIE_RGB = 25,        /* CIE 1931 RGB */

    /* Professional/Photography (extended) */
    ALWAN_RGB_SPACE_ADOBE_WIDE_GAMUT_RGB = 26,
    ALWAN_RGB_SPACE_ROMM_RGB = 27,       /* Reference Output Medium Metric RGB */
    ALWAN_RGB_SPACE_RIMM_RGB = 28,       /* Reference Input Medium Metric RGB */
    ALWAN_RGB_SPACE_ERIMM_RGB = 29,      /* Extended RIMM RGB */

    /* DaVinci/FilmLight */
    ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT = 30,
    ALWAN_RGB_SPACE_FILMLIGHT_T_LOG = 31,    /* FilmLight T-Log (E-Gamut primaries + T-Log OETF) */

    /* Fujifilm Camera Spaces */
    ALWAN_RGB_SPACE_F_GAMUT = 32,
    ALWAN_RGB_SPACE_FUJIFILM_F_LOG = 33,     /* Fujifilm F-Log (F-Gamut primaries + F-Log OETF) */

    /* Nikon Camera Spaces */
    ALWAN_RGB_SPACE_N_GAMUT = 34,
    ALWAN_RGB_SPACE_N_LOG = 35,              /* N-Log (N-Gamut primaries + N-Log OETF) */

    /* DJI Camera Spaces */
    ALWAN_RGB_SPACE_DJI_D_GAMUT = 36,

    /* GoPro Camera Spaces */
    ALWAN_RGB_SPACE_PROTUNE_NATIVE = 37,

    /* Legacy Broadcast (extended) */
    ALWAN_RGB_SPACE_ITU_R_BT470_525 = 38,
    ALWAN_RGB_SPACE_ITU_R_BT470_625 = 39,
    ALWAN_RGB_SPACE_SMPTE_240M = 40,
    ALWAN_RGB_SPACE_SMPTE_C = 41,

    /* Digital Cinema & Mastering */
    ALWAN_RGB_SPACE_DCDM_XYZ = 42,

    /* Print/Specialized Spaces */
    ALWAN_RGB_SPACE_BEST_RGB = 43,
    ALWAN_RGB_SPACE_BETA_RGB = 44,
    ALWAN_RGB_SPACE_DON_RGB_4 = 45,
    ALWAN_RGB_SPACE_EKTA_SPACE_PS5 = 46,
    ALWAN_RGB_SPACE_MAX_RGB = 47,
    ALWAN_RGB_SPACE_RUSSELL_RGB = 48,

    /* Historical/Reference (additional) */
    ALWAN_RGB_SPACE_SHARP_RGB = 49,
    ALWAN_RGB_SPACE_ECI_RGB_V2 = 50,

    /* ========== EXISTING SPACES (kept for compatibility) ========== */

    /* Adobe RGB (1998) - Photography/print workflow */
    ALWAN_RGB_SPACE_ADOBE_RGB_1998 = 51,

    /* ProPhoto RGB - Wide gamut professional */
    ALWAN_RGB_SPACE_PROPHOTO_RGB = 52,

    /* Cinema/Broadcast spaces */
    ALWAN_RGB_SPACE_DAVINCI_WIDE_GAMUT = 53,
    ALWAN_RGB_SPACE_DAVINCI_INTERMEDIATE = 54, /* DaVinci Intermediate (DaVinci WG primaries + intermediate encoding) */
    ALWAN_RGB_SPACE_BLACKMAGIC_WIDE_GAMUT = 55,
    ALWAN_RGB_SPACE_BLACKMAGIC_FILM = 56,      /* Blackmagic Design Film, the Broadcast Film Gen 4 curve */
    ALWAN_RGB_SPACE_BLACKMAGIC_FILM_GEN5 = 57, /* Blackmagic Film Generation 5 */
    ALWAN_RGB_SPACE_V_GAMUT = 58,
    ALWAN_RGB_SPACE_V_LOG = 59,          /* V-Log (V-Gamut primaries + V-Log OETF) */
    ALWAN_RGB_SPACE_S_GAMUT = 60,
    ALWAN_RGB_SPACE_S_GAMUT3 = 61,
    ALWAN_RGB_SPACE_S_GAMUT3_CINE = 62,
    ALWAN_RGB_SPACE_CINEMA_GAMUT = 63,
    ALWAN_RGB_SPACE_CANON_LOG = 64,      /* Canon Log (Cinema Gamut primaries + Canon Log OETF) */
    ALWAN_RGB_SPACE_REDWIDEGAMUTRGB = 65,
    ALWAN_RGB_SPACE_DCI_P3 = 66,
    ALWAN_RGB_SPACE_DCI_P3_P = 67,           /* DCI-P3+ (extended primaries) */
    ALWAN_RGB_SPACE_P3_D65 = 68,

    /* Legacy spaces */
    ALWAN_RGB_SPACE_NTSC_1953 = 69,
    ALWAN_RGB_SPACE_NTSC_1987 = 70,
    ALWAN_RGB_SPACE_PAL_SECAM = 71,
    ALWAN_RGB_SPACE_EBU_TECH_3213_E = 72,    /* EBU Tech. 3213-E (European Broadcasting Union) */
    ALWAN_RGB_SPACE_APPLE_RGB = 73,
    ALWAN_RGB_SPACE_COLORMATCH_RGB = 74,

    /* Additional RGB spaces */
    ALWAN_RGB_SPACE_ALEXA_WIDE_GAMUT = 75,       /* ARRI ALEXA Wide Gamut */
    ALWAN_RGB_SPACE_P3_D60 = 76,                  /* P3 with D60 white point */
    ALWAN_RGB_SPACE_XTREME_RGB = 77,             /* Xtreme RGB (HP/Microsoft extended gamut) */
    ALWAN_RGB_SPACE_LINEAR_REC709 = 78,          /* Linear Rec.709 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_REC2020 = 79,         /* Linear Rec.2020 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_ADOBE_RGB_1998 = 80,  /* Linear Adobe RGB (1998) */
    ALWAN_RGB_SPACE_LINEAR_P3_D65 = 81,          /* Linear P3-D65 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_DISPLAY_P3 = 82,      /* Linear Display P3 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_PROPHOTO_RGB = 83,    /* Linear ProPhoto RGB (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_DCI_P3 = 84,          /* Linear DCI-P3 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_ADOBE_WIDE_GAMUT_RGB = 85,  /* Linear Adobe Wide Gamut RGB */
    ALWAN_RGB_SPACE_LINEAR_APPLE_RGB = 86,       /* Linear Apple RGB (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_COLORMATCH_RGB = 87,  /* Linear ColorMatch RGB (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_P3_D60 = 88,          /* Linear P3-D60 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_BT470_525 = 89,       /* Linear BT.470-525 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_BT470_625 = 90,       /* Linear BT.470-625 (no transfer function) */
    ALWAN_RGB_SPACE_LINEAR_SMPTE_240M = 91,      /* Linear SMPTE 240M (no transfer function) */

    /* Specialized/Standard spaces */
    ALWAN_RGB_SPACE_ITU_T_H273_22_UNSPECIFIED = 92,  /* ITU-T H.273 code point 22 (Unspecified) */
    ALWAN_RGB_SPACE_ITU_T_H273_GENERIC_FILM = 93,    /* ITU-T H.273 Generic Film */
    ALWAN_RGB_SPACE_PLASA_ANSI_E154 = 94,            /* PLASA ANSI E1.54 (Entertainment lighting standard) */

    /* Gamma-encoded variants (simple power-law gamma instead of complex transfer functions) */
    ALWAN_RGB_SPACE_GAMMA22_REC709 = 95,     /* Rec.709 primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA22_ADOBE_RGB = 96,  /* Adobe RGB primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA22_P3_D65 = 97,     /* P3-D65 primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA22_AP1 = 98,        /* ACEScg (AP1) primaries + gamma 2.2 OETF */
    ALWAN_RGB_SPACE_GAMMA18_REC709 = 99,     /* Rec.709 primaries + gamma 1.8 OETF */

    /* ColorInterop Display Color Spaces (Section 2.1) */
    ALWAN_RGB_SPACE_REC1886_REC709 = 100,     /* Rec.709 primaries + BT.1886 EOTF (gamma 2.4) */
    ALWAN_RGB_SPACE_REC2100_PQ = 101,         /* Rec.2020 primaries + PQ (SMPTE ST.2084) */
    ALWAN_RGB_SPACE_REC2100_HLG = 102,        /* Rec.2020 primaries + HLG (BT.2100) */
    ALWAN_RGB_SPACE_DISPLAY_P3_HDR = 103,     /* Display P3 primaries + PQ (SMPTE ST.2084) */

    /* The Color Interop Forum's texture spaces alwan lacked, and two camera gamuts */
    ALWAN_RGB_SPACE_LINEAR_CIE_XYZ_D65 = 104, /* CIE XYZ, linear, D65-relative (lin_ciexyzd65_scene) */
    ALWAN_RGB_SPACE_SRGB_AP1 = 105,           /* ACEScg (AP1) primaries + sRGB curve (srgb_ap1_scene) */
    ALWAN_RGB_SPACE_GAMMA24_REC709 = 106,     /* Rec.709 primaries + gamma 2.4 (g24_rec709_scene) */
    ALWAN_RGB_SPACE_FILMLIGHT_E_GAMUT_2 = 107, /* FilmLight E-Gamut 2, primaries only */
    ALWAN_RGB_SPACE_F_GAMUT_C = 108,          /* Fujifilm F-Gamut C, primaries only */

    ALWAN_RGB_SPACE_COUNT = 109  /* Sentinel: number of enum values */
} alwan_rgb_space;

/* Backward compatibility alias */
#define ALWAN_RGB_SPACE_LINEAR_SRGB ALWAN_RGB_SPACE_LINEAR_REC709

/* Transfer function identifiers (OETF/EOTF) */
typedef enum {
    ALWAN_TF_LINEAR = 0,   /* Linear / Identity (no transfer function) */
    ALWAN_TF_SRGB = 1,
    ALWAN_TF_BT709 = 2, /* Same as BT.2020 */
    ALWAN_TF_BT2020 = 3, /* Same as BT.709 */
    ALWAN_TF_PQ = 4, /* Perceptual Quantizer (SMPTE ST 2084) */
    ALWAN_TF_ST2084 = 5, /* Alias for PQ */
    ALWAN_TF_HLG = 6, /* Hybrid Log-Gamma (BT.2100) */
    ALWAN_TF_BT1886 = 7, /* BT.1886 EOTF only */
    ALWAN_TF_ACESPROXY = 8, /* ACES Proxy */
    ALWAN_TF_ACESCC = 9, /* ACEScc (log encoding for color correction) */
    ALWAN_TF_ACESCCT = 10, /* ACEScct (log encoding with toe for grading) */

    /* Extended Transfer Functions */
    /* Sony S-Log Family */
    ALWAN_TF_SLOG = 11, /* Sony S-Log */
    ALWAN_TF_SLOG2 = 12, /* Sony S-Log2 */
    ALWAN_TF_SLOG3 = 13, /* Sony S-Log3 */

    /* Canon C-Log Family */
    ALWAN_TF_CLOG = 14, /* Canon C-Log */
    ALWAN_TF_CLOG2 = 15, /* Canon C-Log2 */
    ALWAN_TF_CLOG3 = 16, /* Canon C-Log3 */

    /* Panasonic V-Log */
    ALWAN_TF_VLOG = 17, /* Panasonic V-Log */

    /* ARRI LogC Family */
    ALWAN_TF_LOGC3 = 18, /* ARRI LogC3 */
    ALWAN_TF_LOGC4 = 19, /* ARRI LogC4 */

    /* Red Log Family */
    ALWAN_TF_REDLOG = 20, /* RED REDLog */
    ALWAN_TF_REDLOGFILM = 21, /* RED REDLogFilm */
    ALWAN_TF_LOG3G10 = 22, /* RED Log3G10 */

    /* Blackmagic Film */
    ALWAN_TF_BMDFILM = 23, /* Blackmagic Film Gen 5 */
    ALWAN_TF_BMDFILM4 = 24, /* Blackmagic Film Gen 4 (Broadcast Film) */

    /* FilmLight T-Log, Olympus/OM System OM-Log400 */
    ALWAN_TF_TLOG = 25, /* FilmLight T-Log (pairs with E-Gamut primaries) */
    ALWAN_TF_ELOG = 26, /* Olympus/OM System OM-Log400. Named "E-Log" for
                         * historical reasons; it is not a FilmLight curve. */

    /* GoPro Protune */
    ALWAN_TF_PROTUNE = 27, /* GoPro Protune */

    /* Standard Gamma Variants */
    ALWAN_TF_GAMMA22 = 28, /* Gamma 2.2 */
    ALWAN_TF_GAMMA24 = 29, /* Gamma 2.4 */
    ALWAN_TF_GAMMA26 = 30, /* Gamma 2.6 */
    ALWAN_TF_GAMMA28 = 31, /* Gamma 2.8 */

    /* Nikon N-Log */
    ALWAN_TF_NLOG = 32, /* Nikon N-Log */

    /* Film Log Encoding */
    ALWAN_TF_CINEON = 33, /* Cineon / DPX film log encoding */

    /* Apple Log (iPhone 15 Pro+) */
    ALWAN_TF_APPLE_LOG = 34, /* Apple Log (iPhone 15 Pro, BT.2020 primaries) */

    /* Fujifilm F-Log / F-Log2 */
    ALWAN_TF_FLOG = 35, /* Fujifilm F-Log */
    ALWAN_TF_FLOG2 = 36, /* Fujifilm F-Log2 */

    /* Leica L-Log */
    ALWAN_TF_LLOG = 37, /* Leica L-Log */

    /* DJI D-Log */
    ALWAN_TF_DLOG = 38, /* DJI D-Log */

    /* Digital Cinema */
    ALWAN_TF_DCDM = 39, /* DCDM gamma 2.6 (SMPTE ST 428-1) */

    /* Academy Density Exchange (SMPTE ST 2065-3) */
    ALWAN_TF_ADX10 = 40, /* ADX 10-bit (printing density to code value) */
    ALWAN_TF_ADX16 = 41, /* ADX 16-bit (printing density to code value) */

    /* Photographic and legacy working-space curves. Appended, so nothing above renumbers. */
    ALWAN_TF_GAMMA18 = 42,   /* Gamma 1.8 (Apple RGB, ColorMatch RGB) */
    ALWAN_TF_ROMM = 43,      /* ROMM RGB (ISO 22028-2), the ProPhoto RGB encoding */
    ALWAN_TF_RIMM = 44,      /* RIMM RGB (ISO 22028-3) */
    ALWAN_TF_ERIMM = 45,     /* ERIMM RGB (ISO 22028-3), log over 0.001 to 316.2 */
    ALWAN_TF_LSTAR = 46,     /* CIE 1976 lightness (ECI RGB v2) */
    ALWAN_TF_SMPTE240M = 47, /* SMPTE ST 240 OETF */
    ALWAN_TF_ADOBE_RGB = 48, /* Adobe gamma 563/256 = 2.19921875 */
    ALWAN_TF_DAVINCI_INTERMEDIATE = 49, /* DaVinci Intermediate, the DaVinci Wide Gamut delivery curve */

    /* ITU-T H.273 transfer characteristics without an earlier alwan curve */
    ALWAN_TF_H273_LOG = 50,      /* H.273 transfer 9: logarithmic, 100:1 range, 0 below 0.01 */
    ALWAN_TF_H273_LOG_SQRT = 51, /* H.273 transfer 10: logarithmic, 100 sqrt(10):1 range */
    ALWAN_TF_XVYCC = 52,         /* IEC 61966-2-4 xvYCC, H.273 transfer 11: BT.709 extended by symmetry */
    ALWAN_TF_BT1361 = 53,        /* BT.1361 extended colour gamut, H.273 transfer 12 */
    ALWAN_TF_SYCC = 54,          /* IEC 61966-2-1 sYCC, H.273 transfer 13: sRGB extended by symmetry */
    ALWAN_TF_BT2020_12BIT = 55,  /* BT.2020 for 12-bit systems, alpha 1.0993 and beta 0.0181, H.273 transfer 15 */

    /* Log curves from colour-science's registry */
    ALWAN_TF_LOG3G12 = 56,       /* RED Log3G12, odd about 0 */
    ALWAN_TF_PANALOG = 57,       /* Panavision Panalog */
    ALWAN_TF_VIPERLOG = 58,      /* Thomson ViperLog */
    ALWAN_TF_PLOG = 59,          /* Josh Pines' pivoted log: 445 at 0.18, negative gamma 0.6, 0.002 density per code value */
    ALWAN_TF_FILMIC_PRO6 = 60,   /* FiLMiC Pro 6; the EOTF inverts it by Newton's method */
    ALWAN_TF_MILOG = 61,         /* Xiaomi Mi-Log, Apple Log's form with Xiaomi's constants */
    ALWAN_TF_LOG2 = 62,          /* log2 over -6.5 to +6.5 stops around 0.18 */

    /* Medical display */
    ALWAN_TF_DICOM_GSDF = 63,    /* DICOM PS3.14 Grayscale Standard Display Function: signal x 1023 is the JND index, luminance is ABSOLUTE cd/m2 (0.05 to about 3993), as PQ's is nits */

    ALWAN_TF_COUNT = 64,         /* Sentinel: number of curves */

    /* Game Engine Interop */
    ALWAN_TF_UNITY_LINEAR = ALWAN_TF_LINEAR  /* Unity linear (alias for ALWAN_TF_LINEAR) */
} alwan_transfer_function;

/* Standard illuminant identifiers */
typedef enum {
	ALWAN_ILLUMINANT_A = 0, /* Incandescent / Tungsten */
	ALWAN_ILLUMINANT_B = 1, /* CIE Illuminant B (direct sunlight) */
	ALWAN_ILLUMINANT_C = 2, /* CIE Illuminant C (average daylight) */
    ALWAN_ILLUMINANT_D40 = 3, /* Daylight 4000K (P8.3) */
    ALWAN_ILLUMINANT_D45 = 4, /* Daylight 4500K (P8.3) */
    ALWAN_ILLUMINANT_D50 = 5, /* Daylight 5000K */
	ALWAN_ILLUMINANT_D55 = 6, /* Daylight 5500K */
	ALWAN_ILLUMINANT_D60 = 7, /* Daylight 6000K */
	ALWAN_ILLUMINANT_D65 = 8, /* Daylight 6500K */
	ALWAN_ILLUMINANT_D75 = 9, /* Daylight 7500K */
    ALWAN_ILLUMINANT_D93 = 10, /* Daylight 9300K (P8.3) */
    ALWAN_ILLUMINANT_E = 11, /* Equal energy */
    ALWAN_ILLUMINANT_F1 = 12, /* Fluorescent */
    ALWAN_ILLUMINANT_F2 = 13,
    ALWAN_ILLUMINANT_F3 = 14,
    ALWAN_ILLUMINANT_F4 = 15,
    ALWAN_ILLUMINANT_F5 = 16,
    ALWAN_ILLUMINANT_F6 = 17,
    ALWAN_ILLUMINANT_F7 = 18,
    ALWAN_ILLUMINANT_F8 = 19,
    ALWAN_ILLUMINANT_F9 = 20,
    ALWAN_ILLUMINANT_F10 = 21,
    ALWAN_ILLUMINANT_F11 = 22,
    ALWAN_ILLUMINANT_F12 = 23,

    /* LED illuminants */
    ALWAN_ILLUMINANT_LED_B1 = 24, /* LED B1 (blue-pumped phosphor) */
    ALWAN_ILLUMINANT_LED_B2 = 25, /* LED B2 */
    ALWAN_ILLUMINANT_LED_B3 = 26, /* LED B3 */
    ALWAN_ILLUMINANT_LED_B4 = 27, /* LED B4 */
    ALWAN_ILLUMINANT_LED_B5 = 28, /* LED B5 */
    ALWAN_ILLUMINANT_LED_BH1 = 29, /* LED BH1 (high CRI) */
    ALWAN_ILLUMINANT_LED_RGB1 = 30, /* LED RGB1 (RGB LED mix) */
    ALWAN_ILLUMINANT_LED_V1 = 31, /* LED V1 (violet-pumped) */
    ALWAN_ILLUMINANT_LED_V2 = 32, /* LED V2 */

    /* High Pressure illuminants */
    ALWAN_ILLUMINANT_HP1 = 33, /* High Pressure 1 (mercury) */
    ALWAN_ILLUMINANT_HP2 = 34, /* High Pressure 2 */
    ALWAN_ILLUMINANT_HP3 = 35, /* High Pressure 3 */
    ALWAN_ILLUMINANT_HP4 = 36, /* High Pressure 4 */
    ALWAN_ILLUMINANT_HP5 = 37, /* High Pressure 5 */

    /* CIE FL3.x fluorescents. colour-science spells these FL3.1 to FL3.15; alwan spells
     * its fluorescents F1 to F12, so they are F3_1 to F3_15 here. F3_1 is not a variant
     * spelling of F3, which is colour's FL3: they are different lamps. */
    ALWAN_ILLUMINANT_F3_1 = 38,
    ALWAN_ILLUMINANT_F3_2 = 39,
    ALWAN_ILLUMINANT_F3_3 = 40,
    ALWAN_ILLUMINANT_F3_4 = 41,
    ALWAN_ILLUMINANT_F3_5 = 42,
    ALWAN_ILLUMINANT_F3_6 = 43,
    ALWAN_ILLUMINANT_F3_7 = 44,
    ALWAN_ILLUMINANT_F3_8 = 45,
    ALWAN_ILLUMINANT_F3_9 = 46,
    ALWAN_ILLUMINANT_F3_10 = 47,
    ALWAN_ILLUMINANT_F3_11 = 48,
    ALWAN_ILLUMINANT_F3_12 = 49,
    ALWAN_ILLUMINANT_F3_13 = 50,
    ALWAN_ILLUMINANT_F3_14 = 51,
    ALWAN_ILLUMINANT_F3_15 = 52,

    /* Indoor daylight */
    ALWAN_ILLUMINANT_ID50 = 53, /* Indoor daylight 5000K */
    ALWAN_ILLUMINANT_ID65 = 54, /* Indoor daylight 6500K */

    /* Light sources from colour-science SDS_LIGHT_SOURCES, reachable by colour's own
     * names. Most are real lamps as measured. Each is 380-780nm held flat across the
     * rest of the 360-830nm table range, the convention the F3.x and ID entries use.
     *
     * Two are not lamps. LS_SA and LS_SC are CIE illuminants A and C as tabulated in
     * RIT's PointerData spreadsheet: over 380-780nm they match ALWAN_ILLUMINANT_A to
     * 5e-4 and ALWAN_ILLUMINANT_C exactly. Below 380nm they are the flat hold, where
     * A and C carry real data, so prefer ALWAN_ILLUMINANT_A and _C. colour warns that
     * the RIT spreadsheet names no source and its names cannot be verified.
     *
     * LS_INCANDESCENT and LS_60_AW_SOFT_WHITE are one measurement: colour ships them
     * bit-identical, from two sheets of one NIST spreadsheet. Both keep a value so
     * every entry stays reachable by colour's name.
     *
     * The LS_ prefix keeps all 56 apart from the standards above, including the two
     * that are those standards by a second, lower-fidelity route. */

    /* RIT PointerData spreadsheet (Pointer 1980); names unverified upstream */
    ALWAN_ILLUMINANT_LS_NATURAL = 55, /* Natural */
    ALWAN_ILLUMINANT_LS_PHILIPS_TL84 = 56, /* Philips TL-84 */
    ALWAN_ILLUMINANT_LS_SA = 57, /* SA: CIE A, 380-780nm only; prefer ALWAN_ILLUMINANT_A */
    ALWAN_ILLUMINANT_LS_SC = 58, /* SC: CIE C, 380-780nm only; prefer ALWAN_ILLUMINANT_C */
    ALWAN_ILLUMINANT_LS_T8_LUXLINE_PLUS_WHITE = 59, /* T8 Luxline Plus White */
    ALWAN_ILLUMINANT_LS_T8_POLYLUX_3000 = 60, /* T8 Polylux 3000 */
    ALWAN_ILLUMINANT_LS_T8_POLYLUX_4000 = 61, /* T8 Polylux 4000 */
    ALWAN_ILLUMINANT_LS_THORN_KOLOR_RITE = 62, /* Thorn Kolor-rite */

    /* NIST CQS simulation 7.4, traditional sources */
    ALWAN_ILLUMINANT_LS_COOL_WHITE_FL = 63, /* Cool White FL */
    ALWAN_ILLUMINANT_LS_DAYLIGHT_FL = 64, /* Daylight FL */
    ALWAN_ILLUMINANT_LS_HPS = 65, /* HPS */
    ALWAN_ILLUMINANT_LS_INCANDESCENT = 66, /* Incandescent; same data as LS_60_AW_SOFT_WHITE */
    ALWAN_ILLUMINANT_LS_LPS = 67, /* LPS */
    ALWAN_ILLUMINANT_LS_MERCURY = 68, /* Mercury */
    ALWAN_ILLUMINANT_LS_METAL_HALIDE = 69, /* Metal Halide */
    ALWAN_ILLUMINANT_LS_NEODIMIUM_INCANDESCENT = 70, /* Neodimium Incandescent */
    ALWAN_ILLUMINANT_LS_SUPER_HPS = 71, /* Super HPS */
    ALWAN_ILLUMINANT_LS_TRIPHOSPHOR_FL = 72, /* Triphosphor FL */

    /* NIST CQS simulation 7.4, LED sources */
    ALWAN_ILLUMINANT_LS_3LED_1 = 73, /* 3-LED-1 (457/540/605) */
    ALWAN_ILLUMINANT_LS_3LED_2 = 74, /* 3-LED-2 (473/545/616) */
    ALWAN_ILLUMINANT_LS_3LED_2_YELLOW = 75, /* 3-LED-2 Yellow */
    ALWAN_ILLUMINANT_LS_3LED_3 = 76, /* 3-LED-3 (465/546/614) */
    ALWAN_ILLUMINANT_LS_3LED_4 = 77, /* 3-LED-4 (455/547/623) */
    ALWAN_ILLUMINANT_LS_4LED_NO_YELLOW = 78, /* 4-LED No Yellow */
    ALWAN_ILLUMINANT_LS_4LED_YELLOW = 79, /* 4-LED Yellow */
    ALWAN_ILLUMINANT_LS_4LED_1 = 80, /* 4-LED-1 (461/526/576/624) */
    ALWAN_ILLUMINANT_LS_4LED_2 = 81, /* 4-LED-2 (447/512/573/627) */
    ALWAN_ILLUMINANT_LS_LUXEON_WW_2880 = 82, /* Luxeon WW 2880 */
    ALWAN_ILLUMINANT_LS_PHOS_1 = 83, /* PHOS-1 */
    ALWAN_ILLUMINANT_LS_PHOS_2 = 84, /* PHOS-2 */
    ALWAN_ILLUMINANT_LS_PHOS_3 = 85, /* PHOS-3 */
    ALWAN_ILLUMINANT_LS_PHOS_4 = 86, /* PHOS-4 */
    ALWAN_ILLUMINANT_LS_PHOSPHOR_LED_YAG = 87, /* Phosphor LED YAG */

    /* NIST CQS simulation 7.4, Philips sources */
    ALWAN_ILLUMINANT_LS_60_AW_SOFT_WHITE = 88, /* 60 A/W (Soft White); same data as LS_INCANDESCENT */
    ALWAN_ILLUMINANT_LS_C100S54 = 89, /* C100S54 (HPS) */
    ALWAN_ILLUMINANT_LS_C100S54C = 90, /* C100S54C (HPS) */
    ALWAN_ILLUMINANT_LS_F32T8_TL830 = 91, /* F32T8/TL830 (Triphosphor) */
    ALWAN_ILLUMINANT_LS_F32T8_TL835 = 92, /* F32T8/TL835 (Triphosphor) */
    ALWAN_ILLUMINANT_LS_F32T8_TL841 = 93, /* F32T8/TL841 (Triphosphor) */
    ALWAN_ILLUMINANT_LS_F32T8_TL850 = 94, /* F32T8/TL850 (Triphosphor) */
    ALWAN_ILLUMINANT_LS_F32T8_TL865_PLUS = 95, /* F32T8/TL865/PLUS (Triphosphor) */
    ALWAN_ILLUMINANT_LS_F34_CW_RS_EW = 96, /* F34/CW/RS/EW (Cool White FL) */
    ALWAN_ILLUMINANT_LS_F34T12_LW_RS_EW = 97, /* F34T12/LW/RS/EW */
    ALWAN_ILLUMINANT_LS_F34T12WW_RS_EW = 98, /* F34T12WW/RS/EW (Warm White FL) */
    ALWAN_ILLUMINANT_LS_F40_C50 = 99, /* F40/C50 (Broadband FL) */
    ALWAN_ILLUMINANT_LS_F40_C75 = 100, /* F40/C75 (Broadband FL) */
    ALWAN_ILLUMINANT_LS_F40_CWX = 101, /* F40/CWX (Broadband FL) */
    ALWAN_ILLUMINANT_LS_F40_DX = 102, /* F40/DX (Broadband FL) */
    ALWAN_ILLUMINANT_LS_F40_DXTP = 103, /* F40/DXTP (Delux FL) */
    ALWAN_ILLUMINANT_LS_F40_N = 104, /* F40/N (Natural FL) */
    ALWAN_ILLUMINANT_LS_H38HT_100 = 105, /* H38HT-100 (Mercury) */
    ALWAN_ILLUMINANT_LS_H38JA_100_DX = 106, /* H38JA-100/DX (Mercury DX) */
    ALWAN_ILLUMINANT_LS_MHC100_U_MP_3K = 107, /* MHC100/U/MP/3K */
    ALWAN_ILLUMINANT_LS_MHC100_U_MP_4K = 108, /* MHC100/U/MP/4K */
    ALWAN_ILLUMINANT_LS_SDW_T_100W_LV = 109, /* SDW-T 100W/LV (Super HPS) */

    /* Projectors and xenon arc lamps */
    ALWAN_ILLUMINANT_LS_KINOTON_75P = 110 /* Kinoton 75P */
} alwan_illuminant;

/* Enum-based illuminant xy chromaticity accessor
 * Returns xy chromaticity coordinates for the specified illuminant
 * Returns 2 values: x, y chromaticity coordinates
 * Returns ALWAN_E_INVALID if illuminant not supported or xy data not available */
alwan_status alwan_data_get_illuminant_xy_f64(alwan_f64 **data, size_t *count, alwan_illuminant illuminant, alwan_ctx *ctx);
alwan_status alwan_data_get_illuminant_xy_f32(alwan_f32 **data, size_t *count, alwan_illuminant illuminant, alwan_ctx *ctx);

/* View transform identifiers */
typedef enum {
    ALWAN_VIEW_ACES_REC709 = 0, /* ACES RRT + ODT Rec.709 */
    ALWAN_VIEW_AGX_ORIGINAL = 1, /* AgX original (Troy Sobotka's base picture formation) */
    ALWAN_VIEW_AGX_PUNCHY = 2, /* AgX punchy variant (high contrast + saturation) */
    ALWAN_VIEW_AGX_GOLDEN = 3, /* AgX golden variant (warm highlights, cool shadows) */
    ALWAN_VIEW_AGX_SB2383 = 4, /* AgX SB2383 experiment (Sobotka, rotate/inset + own log2 range) */
    ALWAN_VIEW_AGX_BLENDER = 5, /* AgX Blender (EaryChow) -- baked 57^3 3D LUT with gamut guard rail */
    ALWAN_VIEW_BT2446A_HDR_TO_SDR = 6, /* BT.2446 Method A: HDR to SDR tone mapping */
    ALWAN_VIEW_BT2446A_SDR_TO_HDR = 7, /* BT.2446 Method A: SDR to HDR inverse mapping */
    ALWAN_VIEW_KHRONOS_PBR_NEUTRAL = 8, /* Khronos PBR Neutral tone mapping (glTF/WebGL) */
    ALWAN_VIEW_REINHARD_EXT = 9, /* Reinhard Extended (luminance-based, Reinhard 2002) */
    ALWAN_VIEW_UCHIMURA = 10, /* Uchimura / Gran Turismo (parametric S-curve) */
    ALWAN_VIEW_LOTTES = 11, /* Lottes / AMD Cauldron (parametric rational curve) */
    ALWAN_VIEW_TONY_MCMAPFACE = 12, /* Somewhat Boring Display Transform (Stachowiak 2023) */
    ALWAN_VIEW_BT2446B_SDR_TO_HDR = 13, /* BT.2446 Method B: SDR to HDR up-conversion */
    ALWAN_VIEW_BT2446C_HDR_TO_SDR = 14, /* BT.2446 Method C: HDR to SDR (quantization-aware) */
    ALWAN_VIEW_BT2390_HDR_TO_SDR = 15, /* BT.2390 EETF: HDR to SDR (Hermite spline) */
    ALWAN_VIEW_REINHARD_CALIBRATED = 16, /* Reinhard calibrated (key-based, Reinhard 2002) */
    ALWAN_VIEW_EXPOSURE = 17 /* Exposure-based with shoulder compression */
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

/* Derive RGB<->XYZ conversion matrices from primaries and white point.
 *
 * ALWAN_E_INVALID for a NULL argument. ALWAN_E_RANGE when the primaries or the
 * white point make a singular matrix: three primaries on a line, three the same,
 * or a white with y = 0. NOTHING IS WRITTEN on either refusal, so a caller who
 * ignores the status does not get a plausible-looking matrix by accident. The
 * core this delegates to fills a singular inverse with the IDENTITY, branchlessly,
 * because a shader cannot return a status; these entry points can, and do. */
alwan_status alwan_rgb_derive_matrices_f64(alwan_mat3x3_f64 *rgb_to_xyz,
                               alwan_mat3x3_f64 *xyz_to_rgb,
                               alwan_rgb_space_desc_f64 const *desc);
alwan_status alwan_rgb_derive_matrices_f32(alwan_mat3x3_f32 *rgb_to_xyz,
                               alwan_mat3x3_f32 *xyz_to_rgb,
                               alwan_rgb_space_desc_f32 const *desc);

/* Convert linear RGB to XYZ for a given color space
 * space: RGB color space descriptor (primaries and white point)
 * rgb: input linear RGB color
 * xyz: output XYZ color
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_rgb_to_xyz_f32(alwan_xyz_f32 *xyz,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_xyz_f64(alwan_xyz_f64 *xyz,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_rgb_f64 const *rgb);

/* Convert XYZ to linear RGB for a given color space
 * space: RGB color space descriptor (primaries and white point)
 * xyz: input XYZ color
 * rgb: output linear RGB color (may be out of gamut)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_xyz_to_rgb_f32(alwan_rgb_f32 *rgb,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_xyz_f32 const *xyz);
alwan_status alwan_xyz_to_rgb_f64(alwan_rgb_f64 *rgb,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_xyz_f64 const *xyz);

/* Get RGB color space descriptor by enum
 * Loads primaries and white point from data files and populates descriptor
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
alwan_status alwan_rgb_get_space_descriptor_f64(alwan_rgb_space_desc_f64 *desc, alwan_rgb_space space, alwan_ctx *ctx);
alwan_status alwan_rgb_get_space_descriptor_f32(alwan_rgb_space_desc_f32 *desc, alwan_rgb_space space, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * RGB Color Space Conversion
 * ---------------------------------------------------------------- */

/* Convert RGB color from one color space to another
 * Handles chromatic adaptation if white points differ (using Bradford CAT by default)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_rgb_convert_f64(alwan_rgb_f64 *dst_rgb, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_rgb_f64 const *src_rgb, alwan_ctx *ctx);
alwan_status alwan_rgb_convert_f32(alwan_rgb_f32 *dst_rgb, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_rgb_f32 const *src_rgb, alwan_ctx *ctx);

/* MapRGB color space conversion for arrays of colors
 * More efficient than calling alwan_rgb_convert_f64 in a loop
 * count: number of RGB triplets to convert
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_rgb_convert_map_interleave_f64(alwan_rgb_f64 *dst_rgb, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_rgb_f64 const *src_rgb, size_t count, alwan_ctx *ctx);
alwan_status alwan_rgb_convert_map_interleave_f32(alwan_rgb_f32 *dst_rgb, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_rgb_f32 const *src_rgb, size_t count, alwan_ctx *ctx);

/* Convert a 2D image between RGB color spaces with format conversion.
 * Handles EOTF/OETF, chromatic adaptation (Bradford), and every pixel format:
 * U8, U16, F16, F32 and F64, in any pairing of source and destination.
 * dst/src: pixel buffers (3-channel, tightly packed per pixel)
 * dst_fmt/src_fmt: pixel format (ALWAN_PIXEL_U8, _U16, _F16, _F32, _F64).
 *   F16 is IEEE 754 binary16 in a uint16_t. It has been supported for as long as
 *   the others and this comment used to leave it out.
 * dst_row_stride/src_row_stride: bytes between consecutive rows
 * width/height: image dimensions in pixels
 * ctx: context (required when src and dst white points differ)
 * src_space/dst_space: RGB space descriptors (primaries, white point, TFs)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_image_convert_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_ctx *ctx);
alwan_status alwan_image_convert_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_ctx *ctx);

/* Convert a 2D RGBA image between RGB color spaces with format conversion.
 * Same pipeline as alwan_image_convert_f64 but with 4-channel (RGBA) pixels.
 * The alpha channel is preserved through the conversion:
 *   ALWAN_ALPHA_STRAIGHT:      alpha copied unchanged, RGB converted independently
 *   ALWAN_ALPHA_PREMULTIPLIED: RGB unpremultiplied before conversion, repremultiplied after
 * dst/src: pixel buffers (4-channel RGBA, tightly packed per pixel)
 * All other parameters identical to alwan_image_convert_f64.
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_image_convert_rgba_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_alpha_mode alpha_mode, alwan_ctx *ctx);

/* alwan_image_convert with the data semantic decided once for the whole image.
 * alwan_data_semantic has been declared since 2.0.0 and nothing read it; this
 * does what its own comments say and no more:
 *   ALWAN_DATA_COLOR      alwan_image_convert_{T}, exactly
 *   ALWAN_DATA_NON_COLOR  the numbers pass through and only the pixel format
 *                         changes: a normal map, a mask or a displacement keeps
 *                         its values, U8 0..255 becoming F32 0..1 and back.
 *                         Same format in and out is a row copy, every bit
 *                         pattern kept. src_space, dst_space and ctx are not
 *                         read and may be NULL.
 *   ALWAN_DATA_UNKNOWN    ALWAN_E_INVALID: the enum says the application
 *                         decides, and a library that guessed would be deciding.
 * Returns what alwan_image_convert_{T} returns for COLOR, ALWAN_OK or
 * ALWAN_E_INVALID (NULL buffer, zero size, unknown format) for NON_COLOR. */
alwan_status alwan_image_convert_data_f64(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_data_semantic semantic, alwan_ctx *ctx);
alwan_status alwan_image_convert_data_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_data_semantic semantic, alwan_ctx *ctx);
alwan_status alwan_image_convert_rgba_f32(void *dst, size_t dst_row_stride, void const *src, size_t src_row_stride, size_t width, size_t height, alwan_pixel_format dst_fmt, alwan_pixel_format src_fmt, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_alpha_mode alpha_mode, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * sRGB Convenience Functions
 * Direct conversions for sRGB (the most common color space)
 * All assume D65 white point and linear RGB (apply EOTF first if needed)
 * ---------------------------------------------------------------- */

/* sRGB <-> XYZ (D65 white point) */
alwan_status alwan_srgb_to_xyz_f32(alwan_xyz_f32 *xyz, alwan_rgb_f32 const *rgb);
alwan_status alwan_srgb_to_xyz_f64(alwan_xyz_f64 *xyz, alwan_rgb_f64 const *rgb);
alwan_status alwan_xyz_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_xyz_f32 const *xyz);
alwan_status alwan_xyz_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_xyz_f64 const *xyz);

/* sRGB <-> Lab (D65 white point) */
alwan_status alwan_srgb_to_lab_f32(alwan_lab_f32 *lab, alwan_rgb_f32 const *rgb);
alwan_status alwan_srgb_to_lab_f64(alwan_lab_f64 *lab, alwan_rgb_f64 const *rgb);
alwan_status alwan_lab_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_lab_f32 const *lab);
alwan_status alwan_lab_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_lab_f64 const *lab);

/* sRGB <-> Oklab (D65 assumed by Oklab) */
alwan_status alwan_srgb_to_oklab_f32(alwan_oklab_f32 *oklab, alwan_rgb_f32 const *rgb);
alwan_status alwan_srgb_to_oklab_f64(alwan_oklab_f64 *oklab, alwan_rgb_f64 const *rgb);
alwan_status alwan_oklab_to_srgb_f32(alwan_rgb_f32 *rgb, alwan_oklab_f32 const *oklab);
alwan_status alwan_oklab_to_srgb_f64(alwan_rgb_f64 *rgb, alwan_oklab_f64 const *oklab);

/* sRGB convenience conversions (map variants) */
alwan_status alwan_srgb_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_srgb_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_xyz_to_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

alwan_status alwan_srgb_to_lab_f32_map_interleave(alwan_f32 *lab_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_srgb_to_lab_f64_map_interleave(alwan_f64 *lab_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_lab_to_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *lab_in, size_t in_stride, size_t count);
alwan_status alwan_lab_to_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *lab_in, size_t in_stride, size_t count);

alwan_status alwan_srgb_to_oklab_f32_map_interleave(alwan_f32 *oklab_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_srgb_to_oklab_f64_map_interleave(alwan_f64 *oklab_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_oklab_to_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *oklab_in, size_t in_stride, size_t count);
alwan_status alwan_oklab_to_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *oklab_in, size_t in_stride, size_t count);

/* Typed sRGB convenience map functions (_ex variants) */
alwan_status alwan_srgb_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_srgb_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_lab_to_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_srgb_to_oklab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklab_to_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* ----------------------------------------------------------------
 * Direct RGB <-> Perceptual Space Conversions
 * Convenience functions that skip the manual XYZ intermediate step
 * ---------------------------------------------------------------- */

/* RGB <-> Lab (requires white point for Lab) */
alwan_status alwan_rgb_to_lab_f32(alwan_lab_f32 *lab,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_rgb_f32 const *rgb,
                         alwan_xyz_f32 const *white_xyz);
alwan_status alwan_rgb_to_lab_f64(alwan_lab_f64 *lab,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_rgb_f64 const *rgb,
                         alwan_xyz_f64 const *white_xyz);
alwan_status alwan_lab_to_rgb_f32(alwan_rgb_f32 *rgb,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_lab_f32 const *lab,
                         alwan_xyz_f32 const *white_xyz);
alwan_status alwan_lab_to_rgb_f64(alwan_rgb_f64 *rgb,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_lab_f64 const *lab,
                         alwan_xyz_f64 const *white_xyz);

/* RGB <-> Luv (requires white point for Luv) */
alwan_status alwan_rgb_to_luv_f32(alwan_luv_f32 *luv,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_rgb_f32 const *rgb,
                         alwan_xyz_f32 const *white_xyz);
alwan_status alwan_rgb_to_luv_f64(alwan_luv_f64 *luv,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_rgb_f64 const *rgb,
                         alwan_xyz_f64 const *white_xyz);
alwan_status alwan_luv_to_rgb_f32(alwan_rgb_f32 *rgb,
                         alwan_rgb_space_desc_f32 const *space,
                         alwan_luv_f32 const *luv,
                         alwan_xyz_f32 const *white_xyz);
alwan_status alwan_luv_to_rgb_f64(alwan_rgb_f64 *rgb,
                         alwan_rgb_space_desc_f64 const *space,
                         alwan_luv_f64 const *luv,
                         alwan_xyz_f64 const *white_xyz);

/* RGB <-> Oklab (Oklab assumes D65, handles chromatic adaptation if needed) */
alwan_status alwan_rgb_to_oklab_f32(alwan_oklab_f32 *oklab, alwan_rgb_space_desc_f32 const *space, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_oklab_f64(alwan_oklab_f64 *oklab, alwan_rgb_space_desc_f64 const *space, alwan_rgb_f64 const *rgb);
alwan_status alwan_oklab_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_rgb_space_desc_f32 const *space, alwan_oklab_f32 const *oklab);
alwan_status alwan_oklab_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_rgb_space_desc_f64 const *space, alwan_oklab_f64 const *oklab);

/* RGB <-> Oklch (cylindrical Oklab) */
alwan_status alwan_rgb_to_oklch_f32(alwan_oklch_f32 *oklch, alwan_rgb_space_desc_f32 const *space, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_oklch_f64(alwan_oklch_f64 *oklch, alwan_rgb_space_desc_f64 const *space, alwan_rgb_f64 const *rgb);
alwan_status alwan_oklch_to_rgb_f32(alwan_rgb_f32 *rgb, alwan_rgb_space_desc_f32 const *space, alwan_oklch_f32 const *oklch);
alwan_status alwan_oklch_to_rgb_f64(alwan_rgb_f64 *rgb, alwan_rgb_space_desc_f64 const *space, alwan_oklch_f64 const *oklch);

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
    ALWAN_GAMUT_MAP_ADAPTIVE_L0 = 2, /* Adaptive L0 (project toward L=0.5) - P9.5 */
    ALWAN_GAMUT_MAP_ADAPTIVE_CUSP = 3, /* Adaptive toward cusp (hue-dependent) - P9.5 */
    ALWAN_GAMUT_MAP_CHROMA_COMPRESS = 4, /* Chroma compression - P9.5 */
    ALWAN_GAMUT_MAP_SGCK = 5, /* SGCK 2004 (Segment-Maximal Gamut Clipping with Knee) - P9.5 */
    ALWAN_GAMUT_MAP_HPMINDE = 6, /* HPMINDE (Hue-Preserving Minimum dE) - P9.5 */
    ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE = 7 /* Lightness Preserving - P9.5 */
} alwan_gamut_map_method;

/* Method selection guide (see docs/gamut_mapping.md for the full discussion):
 *
 *   method              mechanism                   preserves            use when
 *   ------------------  --------------------------  -------------------  -------------------------------------
 *   CLIP                per-channel clamp           nothing perceptual   final encode; content already ~in gamut
 *   HUE_PRESERVING      RGB scale toward neutral    RGB channel ratios   real-time paths (no Oklab cost)
 *   ADAPTIVE_L0         Oklab project toward L=0.5  hue; balances L/C    general-purpose photographic default
 *   ADAPTIVE_CUSP       Oklab project toward cusp   hue; max chroma      saturated graphics / brand colors
 *   CHROMA_COMPRESS     reduce C, hold L and h      lightness + hue      hue fidelity paramount (CSS-like)
 *   SGCK                knee-compressed segment     gradient smoothness  wide->narrow images with smooth ramps
 *   HPMINDE             min dE on hue leaf          colorimetric close   proofing (smallest visible error)
 *   LIGHTNESS_PRESERVE  hold L, sacrifice C         lightness contrast   text overlays / skin tones
 *
 * Honesty notes: (1) the perceptual (Oklab) boundary model is currently
 * sRGB-anchored -- for targets wider than sRGB the Oklab methods over-compress;
 * (2) for HDR (PQ/HLG, absolute nits) use alwan_hdr_gamut_map_ictcp instead. */

/* Exact RGB gamut volume in linear XYZ.
 * The RGB unit cube maps to a parallelepiped under the RGB->XYZ matrix M,
 * whose volume is exactly |det(M)| -- a closed-form result, not an estimate.
 * For the volume in a nonlinear space see alwan_gamut_volume_perceptual below.
 * space:  RGB color space descriptor
 * volume: output volume (in XYZ units cubed)
 * Returns ALWAN_OK on success */
alwan_status alwan_gamut_volume_f64(alwan_f64 *volume,
                          alwan_rgb_space_desc_f64 const *space);
alwan_status alwan_gamut_volume_f32(alwan_f32 *volume,
                          alwan_rgb_space_desc_f32 const *space);

/* Perceptual gamut volume: the RGB unit cube's image in Lab, Oklab or XYZ,
 * measured deterministically. An n^3 lattice of RGB points is mapped into the
 * target space, each cell is cut into six tetrahedra, and their volumes are
 * summed. No Monte Carlo (two calls agree to the bit) and no convex hull (a
 * concave boundary is measured as concave). This is the piecewise-linear
 * image of the cube, and it converges to the true volume as the lattice is
 * refined: for a linear target it is exact at n = 1, where it equals
 * alwan_gamut_volume's |det M| to every digit; in Lab and Oklab it moves by
 * about 3e-3 from n = 32 to 64 and 5e-4 from 64 to 96 on sRGB.
 * space:  RGB color space descriptor. Lab is taken relative to the space's
 *         own white, so the answer is the space's coverage of Lab under its
 *         own illuminant, with no adaptation involved.
 * target: ALWAN_GAMUT_VOLUME_LAB (units of L*a*b*, sRGB is about 8.2e5),
 *         ALWAN_GAMUT_VOLUME_OKLAB (sRGB about 0.054),
 *         ALWAN_GAMUT_VOLUME_XYZ (equals alwan_gamut_volume)
 * n:      lattice cells per axis; 0 takes ALWAN_GAMUT_VOLUME_DEFAULT_N (64),
 *         above 4096 is ALWAN_E_RANGE. Cost is n^3 conversions; memory is two
 *         (n + 1)^2 slabs, from ctx or the default allocator.
 * Reference: colour-science RGB_colourspace_volume_MonteCarlo(space, ...,
 * illuminant_Lab = the space's white, chromatic_adaptation_transform = None)
 * asks the same question of Lab by sampling, and suite 18 pins the two
 * against each other within that method's noise.
 * Returns ALWAN_OK, ALWAN_E_INVALID for a NULL argument or an unknown target,
 * ALWAN_E_RANGE for n > 4096, ALWAN_E_NOMEM if the slabs cannot be had. */
typedef enum {
    ALWAN_GAMUT_VOLUME_LAB = 0,
    ALWAN_GAMUT_VOLUME_OKLAB = 1,
    ALWAN_GAMUT_VOLUME_XYZ = 2
} alwan_gamut_volume_space;
#define ALWAN_GAMUT_VOLUME_DEFAULT_N 64
alwan_status alwan_gamut_volume_perceptual_f64(alwan_f64 *volume, alwan_rgb_space_desc_f64 const *space, alwan_gamut_volume_space target, size_t n, alwan_ctx *ctx);
alwan_status alwan_gamut_volume_perceptual_f32(alwan_f32 *volume, alwan_rgb_space_desc_f32 const *space, alwan_gamut_volume_space target, size_t n, alwan_ctx *ctx);

/* Map RGB colors to [0,1] gamut using specified method
 * Map operation with stride support for efficient array processing
 * rgb_out: output RGB colors (stride out_stride between consecutive triplets)
 * out_stride: stride for output (in bytes, typically 3*sizeof(alwan_f64) for packed)
 * rgb_in: input RGB colors (may be out of gamut, stride in_stride between triplets)
 * in_stride: stride for input (in bytes, typically 3*sizeof(alwan_f64) for packed)
 * count: number of RGB triplets to process
 * method: gamut mapping method (one of alwan_gamut_map_method: CLIP,
 *         HUE_PRESERVING, ADAPTIVE_L0, ADAPTIVE_CUSP, CHROMA_COMPRESS,
 *         SGCK, HPMINDE, LIGHTNESS_PRESERVE)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if method not supported */
alwan_status alwan_gamut_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_gamut_map_method method);
alwan_status alwan_gamut_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_gamut_map_method method);

/* Map XYZ color to RGB gamut with hue preservation
 * rgb_out: output RGB color (mapped to [0,1] with preserved hue in JCh)
 * space: target RGB space
 * xyz_in: input XYZ color (may be out of RGB gamut)
 * ctx: optional context (can be NULL)
 * Returns ALWAN_OK on success */
alwan_status alwan_gamut_map_xyz_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_space_desc_f64 const *space, alwan_xyz_f64 const *xyz_in, alwan_ctx *ctx);

/* Buffer form, strides in bytes; the scalar in a loop, validated once, bit-identical to it (suite 174). */
alwan_status alwan_gamut_map_xyz_to_rgb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_ctx *ctx);
alwan_status alwan_gamut_map_xyz_to_rgb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_ctx *ctx);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 174). */
alwan_status alwan_gamut_map_xyz_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_ctx *ctx);
alwan_status alwan_gamut_map_xyz_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_ctx *ctx);
alwan_status alwan_gamut_map_xyz_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_space_desc_f32 const *space, alwan_xyz_f32 const *xyz_in, alwan_ctx *ctx);

/* CSS Color Level 4 Section 13.2 OKLCh gamut mapping (binary search on chroma)
 * Maps out-of-gamut linear sRGB to in-gamut linear sRGB using OKLCh binary search
 * with deltaEOK JND criterion (threshold 0.02)
 * rgb_out: output in-gamut linear sRGB triplets
 * rgb_in: input linear sRGB triplets (may be out of gamut)
 * count: number of RGB triplets
 * in_stride, out_stride: stride in bytes */
alwan_status alwan_css_gamut_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_css_gamut_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

/* Gamut map planar */
alwan_status alwan_gamut_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_gamut_map_method method);
alwan_status alwan_gamut_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_gamut_map_method method);
alwan_status alwan_css_gamut_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_css_gamut_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Gamut map _ex (typed pixel format) */
alwan_status alwan_gamut_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_gamut_map_method method, alwan_pixel_format in_fmt);
alwan_status alwan_gamut_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_gamut_map_method method, alwan_pixel_format in_fmt);
alwan_status alwan_css_gamut_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_css_gamut_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* ----------------------------------------------------------------
 * Transfer Functions (OETF/EOTF)
 * ---------------------------------------------------------------- */

/* Apply Opto-Electronic Transfer Function (linear -> encoded)
 * tf: transfer function to apply
 * Outside [0, 1] the curves differ, and the difference is the contract
 * (measured 2026-09-22, pinned in suite 34):
 *   ALWAN_TF_LINEAR   passes any value through, both directions.
 *   ALWAN_TF_SRGB     is extended by its own two segments: below zero the
 *                     linear toe continues (12.92 x, so -0.5 encodes to -6.46
 *                     and -0.1 decodes to -0.0077), above 1 the power curve
 *                     continues (4.0 encodes to 1.82), and a round trip
 *                     returns the value either side. That is CLF's
 *                     monCurveFwd and OpenColorIO's default negative style.
 *                     It is NOT the mirrored form, sign(x) f(|x|), which CLF
 *                     names monCurveMirrorFwd and which -0.5 would encode to
 *                     -0.74; alwan does not offer that through this enum.
 *   the pure gammas, PQ and HLG clamp a negative input to 0 and continue
 *                     above 1 by their own formula. PQ's EOTF is defined on
 *                     [0, 1] only: an encoded value above 1 decodes to no
 *                     luminance, NaN under the libm build and an arbitrary
 *                     finite number under the deterministic one (its pow is
 *                     a polynomial for a positive base), the same in every
 *                     lane of a buffer. Clamp on the way in if that is not
 *                     wanted.
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
alwan_status alwan_oetf_apply_f64(alwan_f64 *encoded_out, size_t out_stride, alwan_f64 const *linear_in, size_t in_stride, size_t count, alwan_transfer_function tf);
alwan_status alwan_oetf_apply_f32(alwan_f32 *encoded_out, size_t out_stride, alwan_f32 const *linear_in, size_t in_stride, size_t count, alwan_transfer_function tf);

/* Apply Electro-Optical Transfer Function (encoded -> linear)
 * tf: transfer function to apply
 * Outside [0, 1]: the contract stated above alwan_oetf_apply applies in this
 * direction too; sRGB decodes 1.5 to 2.54 and -0.1 to -0.0077, PQ decodes an
 * encoded value above 1 to NaN, and the rest clamp a negative input to 0.
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if function not supported */
alwan_status alwan_eotf_apply_f64(alwan_f64 *linear_out, size_t out_stride, alwan_f64 const *encoded_in, size_t in_stride, size_t count, alwan_transfer_function tf);
alwan_status alwan_eotf_apply_f32(alwan_f32 *linear_out, size_t out_stride, alwan_f32 const *encoded_in, size_t in_stride, size_t count, alwan_transfer_function tf);

/* ----------------------------------------------------------------
 * Integer-to-Float Normalization (ColorInterop Section 1.5)
 * Uses (2^N - 1) normalization. Supported bit depths: 8, 10, 12, 16.
 * ---------------------------------------------------------------- */
alwan_status alwan_uint_to_float_f64(alwan_f64 *out, alwan_uint16 const *in, int bit_depth, size_t count);
alwan_status alwan_uint_to_float_f32(alwan_f32 *out, alwan_uint16 const *in, int bit_depth, size_t count);
alwan_status alwan_float_to_uint_f64(alwan_uint16 *out, alwan_f64 const *in, int bit_depth, size_t count);
alwan_status alwan_float_to_uint_f32(alwan_uint16 *out, alwan_f32 const *in, int bit_depth, size_t count);

/* ----------------------------------------------------------------
 * View Transforms (Display Rendering)
 * ---------------------------------------------------------------- */

/* Apply a view transform (display rendering transform) to RGB data
 * View transforms convert scene-referred RGB to display-referred RGB
 *
 * rgb_out: output RGB triplets (display-referred)
 * out_stride: stride between output RGB triplets (in bytes, typically 3*sizeof(alwan_f64))
 * rgb_in: input RGB triplets (scene-referred, typically ACES AP1 or linear)
 * in_stride: stride between input RGB triplets (in bytes, typically 3*sizeof(alwan_f64))
 * count: number of RGB triplets
 * vt: view transform to apply
 * ctx: optional context (can be NULL for stateless transforms)
 *
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if transform not supported */
alwan_status alwan_view_transform_apply_f64(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
alwan_status alwan_view_transform_apply_f32(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
/* Unclamped variant: the per-channel tone mappers (REINHARD_EXT, UCHIMURA,
 * LOTTES, TONY_MCMAPFACE, REINHARD_CALIBRATED, EXPOSURE) return the raw
 * operator output -- no display [0,1] clamp, out-of-gamut chroma preserved
 * (numerical pow-domain guards are retained where the math requires).
 * Transforms whose [0,1] output is definitional (AgX, ACES, BT.2446/2390,
 * PBR Neutral) return identical results through both entry points. */
alwan_status alwan_view_transform_apply_unclamped_f64(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
alwan_status alwan_view_transform_apply_unclamped_f32(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
/* Bulk (map) variants of the view transform. The view workers are scalar, so
 * these are byte-identical to alwan_view_transform_apply; the _ex variant adds
 * typed (u8/u16/f16/f32/f64) I/O for the image pipeline. */
alwan_status alwan_view_transform_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
alwan_status alwan_view_transform_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_view_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
alwan_status alwan_view_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_view_transform vt, alwan_ctx *ctx);
alwan_status alwan_view_transform_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_view_transform vt, alwan_ctx *ctx);

/* JP2499 -- Juan Pablo Zambrano's "2499" picture formation, a purely analytical
 * display-rendering transform (ratio-preserving Michaelis-Menten tonescale with
 * per-primary hue-flight / chroma-attenuation / purity controls, plus optional
 * cube-tip split-toning). Unlike the fixed AgX views this is parameterized;
 * input is linear Rec.709, output is display-linear Rec.709 (apply the display
 * OETF as with the AgX views).
 *
 * For log-encoded / non-Rec.709 footage, decode + convert to linear Rec.709
 * first with the existing bulk helpers, then apply -- e.g.
 *     alwan_eotf_apply_f64(buf, s, buf, s, n*3, ALWAN_TF_LOGC3);      // log->lin
 *     alwan_rgb_convert_f64(...src_space, rec709_space...);           // gamut->709
 *     alwan_jp2499_apply_f64(out, ..., &params);
 * (kept out of the core so JP2499 stays reusable and byte-exact across paths.) */
/* Fields: chroma_attenuation = per-primary path-to-white rate (cpr,cpg,cpb);
 * hue_flight = per-primary hue rotation (ored,ogr,ob); purity = per-primary
 * complementary restore (r/g/b_restore); peak_luminance = display nits (Lp,
 * <=0 defaults to 100). Native dual precision like the other param structs. */
/* white_tip / black_tip (alwan_rgb): cube-tip split-toning -- per-channel colour
 * tint added to the display output, weighted toward highlights (white_tip) and
 * shadows (black_tip). Zero = no tint (the default). The chroma_attenuation /
 * hue_flight / purity triples are one scalar knob per primary (not colours), so
 * they stay plain [3]. */
typedef struct { alwan_f32 chroma_attenuation[3]; alwan_f32 hue_flight[3]; alwan_f32 purity[3]; alwan_f32 peak_luminance; alwan_rgb_f32 white_tip; alwan_rgb_f32 black_tip; } alwan_jp2499_params_f32;
typedef struct { alwan_f64 chroma_attenuation[3]; alwan_f64 hue_flight[3]; alwan_f64 purity[3]; alwan_f64 peak_luminance; alwan_rgb_f64 white_tip; alwan_rgb_f64 black_tip; } alwan_jp2499_params_f64;

/* JP2499's validated default look (non-zero hue geometry). All-zero params give
 * the plain ratio-preserving tonescale with no hue shaping. */
alwan_jp2499_params_f32 alwan_jp2499_default_params_f32(void);
alwan_jp2499_params_f64 alwan_jp2499_default_params_f64(void);
alwan_status alwan_jp2499_apply_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_jp2499_params_f64 const *params);
alwan_status alwan_jp2499_apply_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_jp2499_params_f32 const *params);
/* Interleaved bulk (map) variants -- byte-identical to the apply loop. */
alwan_status alwan_jp2499_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_jp2499_params_f64 const *params);
alwan_status alwan_jp2499_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_jp2499_params_f32 const *params);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_jp2499_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_jp2499_params_f32 const *params);
alwan_status alwan_jp2499_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_jp2499_params_f64 const *params);
/* Typed (u8/u16/f16/f32/f64) variant for the image pipeline (f64 params). */
alwan_status alwan_jp2499_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_jp2499_params_f64 const *params);

/* Analytic AgX -- a fully parameterized, geometric AgX picture-formation engine
 * (Troy Sobotka's design: everything geometric except the single 1D sigmoid).
 * Pipeline: clamp -> inset matrix -> log2 guard-rail encode -> Jed Smith tunable
 * sigmoid -> outset matrix (complementary restore) -> cube-tip split-tone ->
 * sRGB EOTF. Input is linear Rec.709, output is display-linear (same convention
 * as the fixed AgX views); for log/non-Rec.709 footage decode + convert first
 * (see the JP2499 note above). The named AgX views are fixed presets; this is
 * the tunable engine. With the default (SB2383) parameters it reproduces the
 * ALWAN_VIEW_AGX_SB2383 view.
 *
 * Fields: inset/outset = geometric 3x3 (row-major); outset identity = none.
 * log2_min/log2_max = absolute log2 guard-rail bounds (what the encoder takes,
 * not EV). pivot_input = scene-linear grey the sigmoid pivots on (0.18);
 * pivot_output = its display-encoded value; slope = contrast at the pivot;
 * toe_power/shoulder_power = the sigmoid's lower/upper shape.
 *
 * inset/outset are alwan_mat3x3 (row-major 3x3); outset identity = no restore.
 *
 * Cube tips (Troy Sobotka's split-tone technique): instead of the sigmoid
 * running a rigid 0->1 per channel, its upper (white) and lower (black)
 * termination points "tilt" per channel, while the middle fulcrum stays pinned
 * so the three channels cross cleanly at mid-grey (no muddy split tone). Each tip
 * takes {angle, force, offset}: angle = hue on the wheel (radians, 0=R/2pi3=G/
 * 4pi3=B); force = chromatic tilt (upper lowers the complementary channels toward
 * the hue; lower raises the hue channels); offset = achromatic wash (upper washes
 * the whites down, lower lifts the blacks -- a film-like haze). All zero = a rigid
 * 0->1 sigmoid (no tinting). Native dual precision like the other param structs. */
/* tip_middle_angle/force: middle tint (Troy's third control) -- tints the fulcrum
 * itself (a per-channel pivot), so the axis reads lower-tip -> middle-tint ->
 * upper-tip while the three channels still converge at one clean crossover.
 * Balanced (chroma, not brightness); force 0 = neutral fulcrum.
 *
 * primary_rotation/primary_inset/primary_purity ([3], per primary -- scalar knobs,
 * not colours): the geometric inset (Troy's "each primary a wheel"). rotation =
 * per-primary hue turn, inset = pull toward achromatic (rendering-space
 * desaturation), purity = complementary restore. They do NOT act until you call
 * alwan_agx_build_geometry, which rebuilds inset/outset from them (reusing the
 * JP2499 chromaticity construction); by default the baked SB2383 inset stands so
 * the default reproduces the SB2383 view exactly. */
typedef struct { alwan_mat3x3_f32 inset; alwan_f32 log2_min; alwan_f32 log2_max; alwan_f32 pivot_input; alwan_f32 pivot_output; alwan_f32 slope; alwan_f32 toe_power; alwan_f32 shoulder_power; alwan_mat3x3_f32 outset; alwan_f32 tip_upper_angle; alwan_f32 tip_upper_force; alwan_f32 tip_upper_offset; alwan_f32 tip_lower_angle; alwan_f32 tip_lower_force; alwan_f32 tip_lower_offset; alwan_f32 tip_middle_angle; alwan_f32 tip_middle_force; alwan_f32 primary_rotation[3]; alwan_f32 primary_inset[3]; alwan_f32 primary_purity[3]; } alwan_agx_params_f32;
typedef struct { alwan_mat3x3_f64 inset; alwan_f64 log2_min; alwan_f64 log2_max; alwan_f64 pivot_input; alwan_f64 pivot_output; alwan_f64 slope; alwan_f64 toe_power; alwan_f64 shoulder_power; alwan_mat3x3_f64 outset; alwan_f64 tip_upper_angle; alwan_f64 tip_upper_force; alwan_f64 tip_upper_offset; alwan_f64 tip_lower_angle; alwan_f64 tip_lower_force; alwan_f64 tip_lower_offset; alwan_f64 tip_middle_angle; alwan_f64 tip_middle_force; alwan_f64 primary_rotation[3]; alwan_f64 primary_inset[3]; alwan_f64 primary_purity[3]; } alwan_agx_params_f64;

/* Default parameters reproduce the SB2383 AgX look (Sobotka inset + Jed Smith
 * sigmoid slope 2.4 / powers 1.5, identity outset, no split-tone, no middle
 * tint; primary_* all zero). */
alwan_agx_params_f32 alwan_agx_default_params_f32(void);
alwan_agx_params_f64 alwan_agx_default_params_f64(void);
/* Rebuild params->inset / params->outset from the per-primary geometry knobs
 * (primary_rotation/inset/purity). Call after editing those (e.g. the primary
 * wheels); overwrites the baked inset/outset. All-zero knobs -> identity (no
 * inset). Reuses the JP2499 geometric chromaticity construction. */
void alwan_agx_build_geometry_f32(alwan_agx_params_f32 *params);
void alwan_agx_build_geometry_f64(alwan_agx_params_f64 *params);
alwan_status alwan_agx_apply_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_agx_params_f64 const *params);
alwan_status alwan_agx_apply_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_agx_params_f32 const *params);
/* Interleaved bulk (map) variants -- byte-identical to the apply loop. */
alwan_status alwan_agx_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_agx_params_f64 const *params);
alwan_status alwan_agx_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_agx_params_f32 const *params);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_agx_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_agx_params_f32 const *params);
alwan_status alwan_agx_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_agx_params_f64 const *params);
/* Typed (u8/u16/f16/f32/f64) variant for the image pipeline (f64 params). */
alwan_status alwan_agx_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_agx_params_f64 const *params);

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
 * out_stride: output stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * in_stride: input stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * count: number of color triplets to convert */
alwan_status alwan_xyz_to_lab_f32_map_interleave(alwan_f32 *lab_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_lab_f64_map_interleave(alwan_f64 *lab_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

alwan_status alwan_lab_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *lab_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_lab_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *lab_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

/* MapXYZ <-> Luv conversions */
alwan_status alwan_xyz_to_luv_f32_map_interleave(alwan_f32 *luv_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_luv_f64_map_interleave(alwan_f64 *luv_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

alwan_status alwan_luv_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *luv_in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_luv_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *luv_in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);

/* MapLab <-> LCh conversions */
alwan_status alwan_lab_to_lch_f32_map_interleave(alwan_f32 *lch_out, size_t out_stride, alwan_f32 const *lab_in, size_t in_stride, size_t count);
alwan_status alwan_lab_to_lch_f64_map_interleave(alwan_f64 *lch_out, size_t out_stride, alwan_f64 const *lab_in, size_t in_stride, size_t count);

alwan_status alwan_lch_to_lab_f32_map_interleave(alwan_f32 *lab_out, size_t out_stride, alwan_f32 const *lch_in, size_t in_stride, size_t count);
alwan_status alwan_lch_to_lab_f64_map_interleave(alwan_f64 *lab_out, size_t out_stride, alwan_f64 const *lch_in, size_t in_stride, size_t count);

/* MapLuv <-> LCh(uv) conversions */
alwan_status alwan_luv_to_lchuv_f32_map_interleave(alwan_f32 *lchuv_out, size_t out_stride, alwan_f32 const *luv_in, size_t in_stride, size_t count);
alwan_status alwan_luv_to_lchuv_f64_map_interleave(alwan_f64 *lchuv_out, size_t out_stride, alwan_f64 const *luv_in, size_t in_stride, size_t count);

alwan_status alwan_lchuv_to_luv_f32_map_interleave(alwan_f32 *luv_out, size_t out_stride, alwan_f32 const *lchuv_in, size_t in_stride, size_t count);
alwan_status alwan_lchuv_to_luv_f64_map_interleave(alwan_f64 *luv_out, size_t out_stride, alwan_f64 const *lchuv_in, size_t in_stride, size_t count);

/* MapXYZ <-> xyY conversions */
alwan_status alwan_xyz_to_xyy_f32_map_interleave(alwan_f32 *xyy_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_xyy_f64_map_interleave(alwan_f64 *xyy_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

alwan_status alwan_xyy_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *xyy_in, size_t in_stride, size_t count);
alwan_status alwan_xyy_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *xyy_in, size_t in_stride, size_t count);

/* Typed colorspace map functions (_ex variants) */
alwan_status alwan_xyz_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_lab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_luv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_luv_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_lab_to_lch_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_lch_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_luv_to_lchuv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_lchuv_to_luv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_xyy_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyy_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

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
alwan_status alwan_xyz_to_oklab_f32_map_interleave(alwan_f32 *oklab_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_oklab_f64_map_interleave(alwan_f64 *oklab_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

alwan_status alwan_oklab_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *oklab_in, size_t in_stride, size_t count);
alwan_status alwan_oklab_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *oklab_in, size_t in_stride, size_t count);

/* MapOklab <-> Oklch conversions */
alwan_status alwan_oklab_to_oklch_f32_map_interleave(alwan_f32 *oklch_out, size_t out_stride, alwan_f32 const *oklab_in, size_t in_stride, size_t count);
alwan_status alwan_oklab_to_oklch_f64_map_interleave(alwan_f64 *oklch_out, size_t out_stride, alwan_f64 const *oklab_in, size_t in_stride, size_t count);

alwan_status alwan_oklch_to_oklab_f32_map_interleave(alwan_f32 *oklab_out, size_t out_stride, alwan_f32 const *oklch_in, size_t in_stride, size_t count);
alwan_status alwan_oklch_to_oklab_f64_map_interleave(alwan_f64 *oklab_out, size_t out_stride, alwan_f64 const *oklch_in, size_t in_stride, size_t count);

/* Typed Oklab map functions (_ex variants) */
alwan_status alwan_xyz_to_oklab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklab_to_oklch_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklch_to_oklab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

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
alwan_status alwan_rgb_to_ictcp_f32_map_interleave(alwan_f32 *ictcp_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, int use_pq);
alwan_status alwan_rgb_to_ictcp_f64_map_interleave(alwan_f64 *ictcp_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, int use_pq);

alwan_status alwan_ictcp_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *ictcp_in, size_t in_stride, size_t count, int use_pq);
alwan_status alwan_ictcp_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *ictcp_in, size_t in_stride, size_t count, int use_pq);

/* MapXYZ <-> ICtCp conversions */
alwan_status alwan_xyz_to_ictcp_f32_map_interleave(alwan_f32 *ictcp_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, int use_pq);
alwan_status alwan_xyz_to_ictcp_f64_map_interleave(alwan_f64 *ictcp_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, int use_pq);

alwan_status alwan_ictcp_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *ictcp_in, size_t in_stride, size_t count, int use_pq);
alwan_status alwan_ictcp_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *ictcp_in, size_t in_stride, size_t count, int use_pq);

/* Typed ICtCp map functions (_ex variants) */
alwan_status alwan_rgb_to_ictcp_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
alwan_status alwan_ictcp_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
alwan_status alwan_xyz_to_ictcp_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
alwan_status alwan_ictcp_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);

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
alwan_status alwan_xyz_to_jzazbz_f32_map_interleave(alwan_f32 *jzazbz_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_jzazbz_f64_map_interleave(alwan_f64 *jzazbz_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

alwan_status alwan_jzazbz_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *jzazbz_in, size_t in_stride, size_t count);
alwan_status alwan_jzazbz_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *jzazbz_in, size_t in_stride, size_t count);

/* MapJzAzBz <-> JzCzhz conversions */
alwan_status alwan_jzazbz_to_jzczhz_f32_map_interleave(alwan_f32 *jzczhz_out, size_t out_stride, alwan_f32 const *jzazbz_in, size_t in_stride, size_t count);
alwan_status alwan_jzazbz_to_jzczhz_f64_map_interleave(alwan_f64 *jzczhz_out, size_t out_stride, alwan_f64 const *jzazbz_in, size_t in_stride, size_t count);

alwan_status alwan_jzczhz_to_jzazbz_f32_map_interleave(alwan_f32 *jzazbz_out, size_t out_stride, alwan_f32 const *jzczhz_in, size_t in_stride, size_t count);
alwan_status alwan_jzczhz_to_jzazbz_f64_map_interleave(alwan_f64 *jzazbz_out, size_t out_stride, alwan_f64 const *jzczhz_in, size_t in_stride, size_t count);

/* Typed JzAzBz map functions (_ex variants) */
alwan_status alwan_xyz_to_jzazbz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_jzazbz_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_jzazbz_to_jzczhz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_jzczhz_to_jzazbz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

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

/* Buffer form, strides in bytes; the scalar in a loop, validated once, bit-identical to it (suite 174). */
alwan_status alwan_ipt_to_iptch_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_iptch_to_ipt_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_ipt_to_iptch_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_iptch_to_ipt_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 174). */
alwan_status alwan_ipt_to_iptch_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_iptch_to_ipt_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_ipt_to_iptch_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_iptch_to_ipt_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* MapXYZ <-> IPT conversions */
alwan_status alwan_xyz_to_ipt_f32_map_interleave(alwan_f32 *ipt_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_ipt_f64_map_interleave(alwan_f64 *ipt_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count);

alwan_status alwan_ipt_to_xyz_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *ipt_in, size_t in_stride, size_t count);
alwan_status alwan_ipt_to_xyz_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *ipt_in, size_t in_stride, size_t count);

/* Typed IPT map functions (_ex variants) */
alwan_status alwan_xyz_to_ipt_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ipt_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

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
 * MacAdam (1978), as colour-science's XYZ_to_OSA_UCS and OSA_UCS_to_XYZ.
 * - XYZ input/output is D65 adapted, Y = 100 scale
 * - OSA-UCS: L (lightness), j (yellowness), g (greenness)
 * - The inverse is exact: a cubic for Y0 and a Newton solve on the red cube
 *   root, twenty fixed steps, agreeing with the reference to 3e-11 in XYZ.
 *   OUTPUT CHANGED on 2026-09-22: until then it was an approximation with
 *   invented coefficients that returned D65 white 11 per cent off, and its
 *   test printed the miss and passed. See alwan_osa_ucs_core.inc. */
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

/* The rest of colour-science's colour models, each as its function of the same name, in
 * its order of operations and with its signed power sign(a) |a|^p. */

/* Yrg (Kirk 2019): luminance Y and chromaticities r, g from Kirk's LMS, XYZ in 0-1, as
 * XYZ_to_Yrg and Yrg_to_XYZ. Black has l = m = 0, as colour-science's safe division
 * gives; the inverse is ALWAN_E_INVALID where 0.68990272 l + 0.34832189 m is 0. */
alwan_status alwan_xyz_to_yrg_f32(alwan_vec3_f32 *yrg_out, alwan_xyz_f32 const *xyz);
alwan_status alwan_xyz_to_yrg_f64(alwan_vec3_f64 *yrg_out, alwan_xyz_f64 const *xyz);
alwan_status alwan_yrg_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_vec3_f32 const *yrg);
alwan_status alwan_yrg_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_vec3_f64 const *yrg);

/* IPT Ragoo 2021: IPT's structure refitted, with its own four matrices and exponent
 * 0.4071 where IPT has 0.43, XYZ in 0-1, as XYZ_to_IPT_Ragoo2021 and
 * IPT_Ragoo2021_to_XYZ. */
alwan_status alwan_xyz_to_ipt_ragoo2021_f32(alwan_ipt_f32 *ipt_out, alwan_xyz_f32 const *xyz);
alwan_status alwan_xyz_to_ipt_ragoo2021_f64(alwan_ipt_f64 *ipt_out, alwan_xyz_f64 const *xyz);
alwan_status alwan_ipt_ragoo2021_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_ipt_f32 const *ipt);
alwan_status alwan_ipt_ragoo2021_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_ipt_f64 const *ipt);

/* sUCS (Li and Luo 2024): IPT's LMS, exponent 0.43 and sUCS's own Iab matrix, Iab in
 * 0-100 for XYZ in 0-1, as XYZ_to_sUCS and sUCS_to_XYZ. */
alwan_status alwan_xyz_to_sucs_f32(alwan_vec3_f32 *iab_out, alwan_xyz_f32 const *xyz);
alwan_status alwan_xyz_to_sucs_f64(alwan_vec3_f64 *iab_out, alwan_xyz_f64 const *xyz);
alwan_status alwan_sucs_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_vec3_f32 const *iab);
alwan_status alwan_sucs_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_vec3_f64 const *iab);

/* Izazbz: the Jzazbz pipeline before its lightness step, for absolute D65 XYZ in cd/m^2,
 * as XYZ_to_Izazbz and Izazbz_to_XYZ. SAFDAR2021 is ZCAM's form, with its own LMS' matrix
 * and I offset by d_0 = 3.7035226210190005e-11. An unknown method is ALWAN_E_INVALID. */
typedef enum {
    ALWAN_IZAZBZ_SAFDAR2017 = 0,
    ALWAN_IZAZBZ_SAFDAR2021 = 1
} alwan_izazbz_method;
alwan_status alwan_xyz_to_izazbz_f32(alwan_vec3_f32 *izazbz_out, alwan_xyz_f32 const *xyz_d65, alwan_izazbz_method method);
alwan_status alwan_xyz_to_izazbz_f64(alwan_vec3_f64 *izazbz_out, alwan_xyz_f64 const *xyz_d65, alwan_izazbz_method method);
alwan_status alwan_izazbz_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_vec3_f32 const *izazbz, alwan_izazbz_method method);
alwan_status alwan_izazbz_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_vec3_f64 const *izazbz, alwan_izazbz_method method);

/* Hunter Rdab, XYZ in 0-100, as XYZ_to_Hunter_Rdab and Hunter_Rdab_to_XYZ. xyz_n NULL is
 * Hunter's D65 table (95.02, 100, 108.82) with its K_ab (172.3, 67.2); k_ab NULL with an
 * xyz_n derives K_a, K_b by Hunter 1966 (alwan_hunter_coefficients). ALWAN_E_INVALID for a
 * white with a component <= 0, and in the inverse for K_a or K_b = 0. */
alwan_status alwan_xyz_to_hunter_rdab_f32(alwan_vec3_f32 *rdab_out, alwan_xyz_f32 const *xyz,
                                          alwan_xyz_f32 const *xyz_n, alwan_vec2_f32 const *k_ab);
alwan_status alwan_xyz_to_hunter_rdab_f64(alwan_vec3_f64 *rdab_out, alwan_xyz_f64 const *xyz,
                                          alwan_xyz_f64 const *xyz_n, alwan_vec2_f64 const *k_ab);
alwan_status alwan_hunter_rdab_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_vec3_f32 const *rdab,
                                          alwan_xyz_f32 const *xyz_n, alwan_vec2_f32 const *k_ab);
alwan_status alwan_hunter_rdab_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_vec3_f64 const *rdab,
                                          alwan_xyz_f64 const *xyz_n, alwan_vec2_f64 const *k_ab);

/* ----------------------------------------------------------------
 * Named colour palettes
 *
 * Lookup and search compare names as ASCII lowercase with every space, tab and
 * newline removed, so "pinkest pink", "PINKEST PINK" and "PinkestPink" are one name.
 * ---------------------------------------------------------------- */

typedef enum {
    ALWAN_PALETTE_FREETONE = 0,     /* Stuart Semple's Freetone: 1,310 colours in device CMYK */
    ALWAN_PALETTE_CSS_COLOR_3 = 1,  /* CSS Color Module Level 3 keywords: 147 sRGB colours */
    ALWAN_PALETTE_COUNT
} alwan_palette;

typedef enum {
    ALWAN_PALETTE_MODEL_CMYK = 0,   /* device C, M, Y, K in [0, 1]; no printing condition attached */
    ALWAN_PALETTE_MODEL_SRGB = 1    /* sRGB-encoded R, G, B in [0, 1], value[3] = 0 */
} alwan_palette_model;

typedef struct {
    char const *name;               /* as the palette spells it, spacing included; static storage */
    alwan_palette_model model;
    alwan_f64 value[4];
} alwan_palette_entry;

/* The number of colours in a palette; 0 for an unknown one. */
size_t alwan_palette_size(alwan_palette palette);
/* The colour at an index; ALWAN_E_INVALID past the end or for an unknown palette. */
alwan_status alwan_palette_entry_at(alwan_palette_entry *entry_out, alwan_palette palette, size_t index);
/* The index of the colour whose name matches; ALWAN_E_NODATA when none does. */
alwan_status alwan_palette_find(size_t *index_out, alwan_palette palette, char const *name);
/* Every colour whose name contains text. match_count_out receives the number of matches;
 * the first capacity indices go to indices_out, which may be NULL when capacity is 0. */
alwan_status alwan_palette_search(size_t *match_count_out, size_t *indices_out, size_t capacity, alwan_palette palette,
                                  char const *text);

/* HEX notation. alwan_rgb_to_hex writes "#rrggbb" and its terminator, 8 chars, each
 * channel truncated to 0-255 as colour-science's RGB_to_HEX does; a channel outside
 * [0, 1] is ALWAN_E_INVALID. alwan_hex_to_rgb reads six digits as colour-science's
 * HEX_to_RGB does and three as CSS does, each digit doubled (#abc is #aabbcc); the
 * '#' is optional, and any other length or a non-hex digit is ALWAN_E_INVALID. */
alwan_status alwan_rgb_to_hex_f32(char *hex_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_hex_f64(char *hex_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hex_to_rgb_f32(alwan_rgb_f32 *rgb_out, char const *hex);
alwan_status alwan_hex_to_rgb_f64(alwan_rgb_f64 *rgb_out, char const *hex);

/* A CSS Color 3 keyword to sRGB-encoded RGB in [0, 1], as colour-science's
 * keyword_to_RGB_CSSColor3; ALWAN_E_NODATA for an unknown keyword. */
alwan_status alwan_css_color_3_keyword_to_rgb_f32(alwan_rgb_f32 *rgb_out, char const *keyword);
alwan_status alwan_css_color_3_keyword_to_rgb_f64(alwan_rgb_f64 *rgb_out, char const *keyword);

/* A CMYK printing characterisation, built by alwan_cmyk_model_* in the charts section. */
typedef struct alwan_cmyk_model_s alwan_cmyk_model;
typedef struct alwan_cmyk_inverse_s alwan_cmyk_inverse;

/* The palette colour nearest a Lab value by CIEDE2000. lab is relative to the
 * palette's white: the characterisation's (D50 for FOGRA39) for a CMYK palette, which
 * needs cmyk_model and is ALWAN_E_INVALID without one; D65 for an sRGB palette, which
 * ignores it. Ties go to the lower index. delta_e_out may be NULL. */
alwan_status alwan_palette_nearest_f32(size_t *index_out, alwan_f32 *delta_e_out, alwan_palette palette,
                                       alwan_lab_f32 const *lab, alwan_cmyk_model const *cmyk_model);
alwan_status alwan_palette_nearest_f64(size_t *index_out, alwan_f64 *delta_e_out, alwan_palette palette,
                                       alwan_lab_f64 const *lab, alwan_cmyk_model const *cmyk_model);

/* ----------------------------------------------------------------
 * Test patterns
 *
 * Rendered at any size as the pattern's native R'G'B' signal in fractions of white,
 * 0 black and 1 100 % white, below 0 where the pattern goes below black. Stripe edges
 * are the pattern's own fractions of the width rounded to the nearest sample, band
 * edges the same of the height. row_stride is in bytes: at least width x 3 values
 * interleaved, width values planar.
 *
 * The EBU Tech 3325 patterns read a 10-bit code c as (c - 64) / 876, so 50 % grey (502)
 * is 0.5 and super white (1019) 955/876. Their patches are squares of H/7.5 samples, 1 %
 * of a 16:9 picture, centred on the standard's measurement points; the windows keep the
 * picture's aspect. Every edge rounds to the nearest sample.
 * ---------------------------------------------------------------- */

typedef enum {
    ALWAN_PATTERN_BARS_100_0_100_0 = 0,    /* ITU-R BT.471-1 (a), eight bars at 100 % */
    ALWAN_PATTERN_BARS_100_0_75_0 = 1,     /* ITU-R BT.471-1 (b), the EBU colour bars */
    ALWAN_PATTERN_BARS_100_0_100_25 = 2,   /* ITU-R BT.471-1 (c) */
    ALWAN_PATTERN_BARS_75_7_5_75_7_5 = 3,  /* ITU-R BT.471-1 (d), 7.5 % setup */
    ALWAN_PATTERN_ARIB_STD_B28 = 4,        /* ARIB STD-B28 multiformat colour bar, the basis of SMPTE RP 219 */
    ALWAN_PATTERN_EBU_1 = 5,               /* EBU Tech 3325 EBU_1: peak white and four black patches on 50 % grey */
    ALWAN_PATTERN_EBU_2 = 6,               /* EBU_2: EBU_1 with the white patch at 109 % super white */
    ALWAN_PATTERN_EBU_3 = 7,               /* EBU_3-1 to 3-13: a white patch at measurement point ebu_point */
    ALWAN_PATTERN_EBU_3_WINDOW = 8,        /* EBU_3-1_4, _10, _25, _81: a centred white window of ebu_area % */
    ALWAN_PATTERN_EBU_3_BLACK = 9,         /* EBU_3-black */
    ALWAN_PATTERN_EBU_3_WHITE = 10,        /* EBU_3-white: peak white frame */
    ALWAN_PATTERN_EBU_4 = 11,              /* EBU_4-1 to 4-20: the grey-scale patch ebu_step, on black */
    ALWAN_PATTERN_EBU_5 = 12,              /* EBU_5: a BT.709 primary or EBU test colour, ebu_colour, on black */
    ALWAN_PATTERN_EBU_12_GREY = 13,        /* EBU_12-grey: 50 % grey frame */
    ALWAN_PATTERN_PLUGE_BT814 = 14,        /* ITU-R BT.814-4 Annex 2 PLUGE for HDTV, UHDTV and HDR */
    ALWAN_PATTERN_BT1729_SWEEP_H = 15,     /* ITU-R BT.1729 zone 8: horizontal sweep, 60 to 960 cycles across */
    ALWAN_PATTERN_BT1729_SWEEP_V = 16,     /* zone 14: vertical sweep, 32 to 540 cycles down */
    ALWAN_PATTERN_BT1729_STAIRCASE = 17,   /* zone 11: luminance staircase in 10 % steps */
    ALWAN_PATTERN_COUNT
} alwan_pattern;

/* The BT.814-4 higher level patch: 940 for SDR, 399 for PQ and HLG alike (Tables 2 and 3). */
typedef enum {
    ALWAN_PATTERN_PLUGE_SDR = 0,
    ALWAN_PATTERN_PLUGE_HDR = 1
} alwan_pattern_pluge_range;

/* The ARIB STD-B28 pattern 2 area the standard leaves to the user. */
typedef enum {
    ALWAN_PATTERN_B28_75_WHITE = 0,
    ALWAN_PATTERN_B28_100_WHITE = 1,
    ALWAN_PATTERN_B28_PLUS_I = 2
} alwan_pattern_b28_choice;

/* Zero-initialise; NULL means every default. The ebu_ numbers are Tech 3325's own
 * (EBU_3-7 is ebu_point 7), 0 reads as the first of each series, and a number outside
 * its series is ALWAN_E_INVALID whatever the pattern. */
typedef struct {
    alwan_pattern_b28_choice b28_choice;
    unsigned ebu_point;   /* EBU_3: measurement point 1 to 13 (Tech 3325 Figure 7) */
    unsigned ebu_area;    /* EBU_3_WINDOW: white area in percent, 4, 10, 25 or 81 */
    unsigned ebu_step;    /* EBU_4: grey-scale measurement number 1 to 20 (Table 5) */
    unsigned ebu_colour;  /* EBU_5: 1 to 15 the EBU test colours (Table 7), 16 red, 17 green, 18 blue (Table 6) */
    alwan_pattern_pluge_range pluge_range; /* PLUGE_BT814: the higher level patch, SDR or HDR */
} alwan_pattern_params;

alwan_status alwan_pattern_render_f32(alwan_f32 *rgb_out, size_t row_stride, size_t width, size_t height,
                                      alwan_pattern pattern, alwan_pattern_params const *params);
alwan_status alwan_pattern_render_f64(alwan_f64 *rgb_out, size_t row_stride, size_t width, size_t height,
                                      alwan_pattern pattern, alwan_pattern_params const *params);
alwan_status alwan_pattern_render_planar_f32(alwan_f32 *r_out, size_t row_stride, alwan_f32 *g_out, alwan_f32 *b_out,
                                             size_t width, size_t height, alwan_pattern pattern,
                                             alwan_pattern_params const *params);
alwan_status alwan_pattern_render_planar_f64(alwan_f64 *r_out, size_t row_stride, alwan_f64 *g_out, alwan_f64 *b_out,
                                             size_t width, size_t height, alwan_pattern pattern,
                                             alwan_pattern_params const *params);

/* ----------------------------------------------------------------
 * Colour selection
 *
 * A soft mask per pixel, 1 where the pixel belongs to the chosen colours, 0 where
 * it does not, and the edge in between, by four approaches:
 *
 *   qualifier   hue, chroma and lightness ranges with soft edges (the secondary-
 *               grading "HSL qualifier"), evaluated in HSV, OkLCh or CIE LCh(ab)
 *   distance    nearness to one key colour: Oklab Euclidean, CIE 1976 or CIEDE2000
 *   example     a Gaussian fitted to sample pixels in Oklab, keyed on Mahalanobis
 *               distance: pick a few pixels of the thing and select its kind
 *   chroma key  the green / blue screen difference matte, with despill
 *
 * Pixels are the space's ENCODED values, as an image arrives; each approach
 * decodes with the space's EOTF where it needs linear light. HSV is taken on the
 * encoded values, as alwan_rgb_to_hsv does. Oklab is evaluated on the space's
 * XYZ without adaptation, so it is meant for D65 spaces; CIE Lab uses the space's
 * own white. The units are the same in every build, and are the normalised ones:
 * hue is a fraction of a turn in [0, 1) measured from the +a axis (HSV's own
 * convention), lightness is in [0, 1] (HSV V, Oklab L, L* / 100), and chroma keeps
 * its scale, which ALWAN_NORMALIZE_RANGES leaves alone too (HSV S in 0..1, Oklab C
 * about 0..0.4, C*ab about 0..150). A normalised CIE LCh hue (h / 360) is
 * already in turns; a normalised OkLCh hue is h / pi on [-1, 1], so half of it,
 * plus 1 when negative, is the turn.
 *
 * Soft edges are linear: 1 inside the range (or below the tolerance), falling to
 * 0 over `softness` outside it; softness 0 is a hard edge. Strides are bytes and
 * must be at least one element (a mask value, or three channels) wide. The work
 * is done in double; the f32 forms widen and narrow. No published reference
 * defines these masks: suite 181 holds the conversions to colour-science and the
 * masks to the definitions stated here.
 * ---------------------------------------------------------------- */
typedef enum {
    ALWAN_SELECT_HSV = 0,     /* hue, saturation, value of the encoded RGB */
    ALWAN_SELECT_OKLCH = 1,   /* Oklab L, C, h */
    ALWAN_SELECT_CIELCH = 2   /* CIE L*, C*ab, h_ab under the space's white */
} alwan_select_model;

typedef struct {
    alwan_select_model model;
    alwan_f64 hue_center;       /* turns, [0, 1) */
    alwan_f64 hue_width;        /* turns, the full width of the range; 1 or more is every hue */
    alwan_f64 hue_softness;     /* turns of fall-off either side */
    alwan_f64 chroma_min, chroma_max, chroma_softness;   /* HSV S, Oklab C or C*ab */
    alwan_f64 light_min, light_max, light_softness;      /* [0, 1]: HSV V, Oklab L or L* / 100 */
    int invert;                 /* nonzero: 1 - mask */
} alwan_select_qualifier_params;

/* Every hue, chroma and lightness, in OkLCh, hard edges: a mask of 1 everywhere
 * until ranges are narrowed. */
void alwan_select_qualifier_params_init(alwan_select_qualifier_params *params);

/* The mask is the smallest of the hue, chroma and lightness weights. A neutral
 * pixel has no hue (HSV reports 0, the LCh forms atan2 of zeros, also 0), so a
 * hue range near red also takes greys unless chroma_min excludes them.
 * ALWAN_E_INVALID for a NULL, a stride too small, a model outside the enum, a
 * non-finite or negative width or softness, or a min above its max. */
alwan_status alwan_select_qualifier_f32(alwan_f32 *mask_out, size_t mask_stride, alwan_f32 const *rgb, size_t rgb_stride, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_select_qualifier_params const *params);
alwan_status alwan_select_qualifier_f64(alwan_f64 *mask_out, size_t mask_stride, alwan_f64 const *rgb, size_t rgb_stride, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_select_qualifier_params const *params);

typedef enum {
    ALWAN_SELECT_METRIC_OKLAB = 0,   /* Euclidean in Oklab (about 0.02 is a just-noticeable step) */
    ALWAN_SELECT_METRIC_DE76 = 1,    /* CIE 1976 Delta E, Lab under the space's white */
    ALWAN_SELECT_METRIC_DE2000 = 2   /* CIEDE2000 */
} alwan_select_metric;

/* 1 for pixels within `tolerance` of key_rgb (encoded, the same space) under the
 * metric, falling to 0 at tolerance + softness. */
alwan_status alwan_select_distance_f32(alwan_f32 *mask_out, size_t mask_stride, alwan_f32 const *rgb, size_t rgb_stride, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_f32 const key_rgb[3], alwan_select_metric metric, alwan_f32 tolerance, alwan_f32 softness);
alwan_status alwan_select_distance_f64(alwan_f64 *mask_out, size_t mask_stride, alwan_f64 const *rgb, size_t rgb_stride, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_f64 const key_rgb[3], alwan_select_metric metric, alwan_f64 tolerance, alwan_f64 softness);

/* Select by example. The fit takes sample pixels (at least two, encoded, the
 * same space), converts them to Oklab and keeps their mean and sample covariance
 * (divided by n - 1) with `ridge` added to its diagonal; ridge > 0 is what lets
 * a handful of near-identical samples fit, and about 1e-5 is a sensible floor in
 * Oklab units. ALWAN_E_DIVZERO when the covariance is singular (samples on a
 * line or a plane, ridge 0). The mask is 1 within `tolerance` Mahalanobis units
 * of the mean (2 to 3 takes most of what the samples represent) and falls to 0
 * over `softness`. */
typedef struct {
    alwan_f64 mean[3];             /* Oklab L, a, b */
    alwan_f64 covariance[9];       /* row-major, ridge included */
    alwan_f64 inv_covariance[9];
} alwan_select_example;

alwan_status alwan_select_example_fit_f32(alwan_select_example *model_out, alwan_f32 const *samples, size_t sample_stride, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_f64 ridge);
alwan_status alwan_select_example_fit_f64(alwan_select_example *model_out, alwan_f64 const *samples, size_t sample_stride, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_f64 ridge);
alwan_status alwan_select_example_f32(alwan_f32 *mask_out, size_t mask_stride, alwan_f32 const *rgb, size_t rgb_stride, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_select_example const *model, alwan_f32 tolerance, alwan_f32 softness);
alwan_status alwan_select_example_f64(alwan_f64 *mask_out, size_t mask_stride, alwan_f64 const *rgb, size_t rgb_stride, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_select_example const *model, alwan_f64 tolerance, alwan_f64 softness);

/* Chroma key: the difference matte of the Vlahos patents as Smith and Blinn,
 * "Blue Screen Matting", SIGGRAPH 1996, write it. With S the screen channel and
 * the reference R = balance * red + (1 - balance) * other (other = blue for a
 * green screen, green for a blue one),
 *     alpha = clamp(1 - gain * (S - R), 0, 1)
 * so the screen is 0 and anything with no excess of the screen channel is 1.
 * fg_out (optional) receives the pixels, and with `despill` the screen channel
 * limited to R, which removes screen light spilled onto the subject. Values are
 * taken as given (no decoding): key in the space the plate was shot in. balance
 * in [0, 1], gain >= 0; ALWAN_E_INVALID otherwise. Not testable against a
 * reference: no library implements this exact matte, so suite 181 checks the
 * formula's properties. */
typedef enum {
    ALWAN_KEY_GREEN = 0,
    ALWAN_KEY_BLUE = 1
} alwan_key_screen;

alwan_status alwan_key_chroma_f32(alwan_f32 *alpha_out, size_t alpha_stride, alwan_f32 *fg_out, size_t fg_stride, alwan_f32 const *rgb, size_t rgb_stride, size_t count, alwan_key_screen screen, alwan_f32 balance, alwan_f32 gain, int despill);
alwan_status alwan_key_chroma_f64(alwan_f64 *alpha_out, size_t alpha_stride, alwan_f64 *fg_out, size_t fg_stride, alwan_f64 const *rgb, size_t rgb_stride, size_t count, alwan_key_screen screen, alwan_f64 balance, alwan_f64 gain, int despill);

/* ----------------------------------------------------------------
 * Illuminant estimation from an image (colour constancy)
 *
 * The colour of the light, read off the picture itself, for images with no
 * camera data to derive it from. One family covers the classic estimators, the
 * framework of van de Weijer, Gevers and Gijsenij, "Edge-Based Color Constancy",
 * IEEE TIP 16(9), 2007:
 *
 *     e(n, p, sigma) = ( sum over x of | d^n f_sigma / dx^n |^p ) ^ (1 / p)
 *
 * per channel, over the pixels that are not excluded, then scaled to unit length.
 * f_sigma is the image smoothed by a Gaussian of sigma pixels. The parameters
 * name the estimators the literature knows:
 *
 *     order  minkowski  sigma
 *       0        1        0     Grey World (Buchsbaum 1980)
 *       0        0        0     White Patch, max-RGB (p = infinity)
 *       0        p        0     Shades of Grey (Finlayson and Trezzi 2004), p = 6 typical
 *       0        p      > 0     general Grey World
 *       1        p      > 0     first-order Grey-Edge
 *       2        p      > 0     second-order Grey-Edge
 *
 * The first-order magnitude is sqrt(fx^2 + fy^2) and the second-order one
 * sqrt(fxx^2 + 4 fxy^2 + fyy^2), each channel on its own. The Gaussian and its
 * derivatives are sampled over +-floor(3 sigma + 0.5) pixels with the image edge
 * replicated; the second-derivative kernel has its mean removed. Pixels within
 * sigma + 1 of the border are left out, and so is every pixel whose largest
 * channel reaches `saturation`, with its eight neighbours: a clipped highlight
 * reports the sensor's limit, not the light. `exclude`, when given, is one byte a
 * pixel, rows exclude_row_stride bytes apart, nonzero for a pixel to leave out
 * as well (a known object, a region with a second light).
 *
 * The estimate is linear RGB in whatever space the image is in, unit length.
 * alwan_illuminant_correct divides each channel by illuminant * sqrt(3), the
 * von Kries correction that maps a light of equal channels to itself. rgb is
 * three values a pixel, rows row_stride bytes apart; the work is done in double.
 *
 * ALWAN_E_INVALID for a NULL, a zero width or height, a row stride under three
 * values a pixel, an order outside 0..2, a negative Minkowski power
 * or one between 0 and 1, a negative or non-finite sigma, a sigma below 1/6 at
 * order 1 or 2 (its kernel would be a single tap and has no derivative), or a
 * non-finite pixel. ALWAN_E_RANGE when no pixel is left to estimate from, or all
 * of them are zero.
 *
 * Reproduces the authors' MATLAB code (general_cc.m), run as an oracle, to 1e-12;
 * see suite 183.
 * ---------------------------------------------------------------- */

typedef struct {
    int order;               /* 0 the pixels, 1 the gradient, 2 the second derivatives */
    alwan_f64 minkowski;     /* p >= 1; 0 takes the maximum */
    alwan_f64 sigma;         /* Gaussian scale in pixels; 0 is no smoothing (order 0 only) */
    alwan_f64 saturation;    /* a pixel whose largest channel reaches this is left out; 0 leaves none out */
} alwan_constancy_params;

/* Grey World: order 0, Minkowski 1, sigma 0, no saturation limit. */
void alwan_constancy_params_init(alwan_constancy_params *params);

alwan_status alwan_illuminant_estimate_f32(alwan_f32 illuminant_out[3], alwan_f32 const *rgb, size_t row_stride, size_t width, size_t height, unsigned char const *exclude, size_t exclude_row_stride, alwan_constancy_params const *params);
alwan_status alwan_illuminant_estimate_f64(alwan_f64 illuminant_out[3], alwan_f64 const *rgb, size_t row_stride, size_t width, size_t height, unsigned char const *exclude, size_t exclude_row_stride, alwan_constancy_params const *params);

/* out = in / (illuminant * sqrt(3)) per channel, count pixels at byte strides.
 * ALWAN_E_INVALID for a NULL, a stride under three values, or a channel of the
 * illuminant that is not positive and finite. In place is allowed. */
alwan_status alwan_illuminant_correct_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 const illuminant[3]);
alwan_status alwan_illuminant_correct_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 const illuminant[3]);

/* Flat artwork as a palette. A rendered pattern, a chart or any flat design is a
 * handful of colours, and a conversion that is expensive per pixel (the CMYK
 * inverse, a spectral upsampling) is cheap per colour: extract the palette,
 * convert its entries, apply the converted entries back.
 *
 * alwan_palette_extract collects the distinct colours of count RGB pixels, in
 * order of first appearance, comparing exactly; count_out receives how many.
 * More than max_colors is ALWAN_E_RANGE with count_out = max_colors + 1 and
 * the first max_colors in palette_out: a sweep or a photograph is not a
 * palette and the scan stops there rather than growing quadratically.
 * A NaN pixel is ALWAN_E_INVALID.
 *
 * alwan_palette_apply writes, for every pixel, the `channels` values of
 * `values` at the index the pixel's colour has in `palette` (values holds
 * palette_count x channels, entry-major), so a CMYK conversion of the
 * palette becomes a CMYK image in one pass. A pixel whose colour is not in
 * the palette is ALWAN_E_RANGE and the output before it is written.
 * Strides in bytes, 0 packed, in the pattern family's sense. */
alwan_status alwan_palette_extract_f64(alwan_f64 *palette_out, size_t max_colors, size_t *count_out, alwan_f64 const *rgb, size_t stride, size_t count);
alwan_status alwan_palette_extract_f32(alwan_f32 *palette_out, size_t max_colors, size_t *count_out, alwan_f32 const *rgb, size_t stride, size_t count);
alwan_status alwan_palette_apply_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *rgb, size_t in_stride, size_t count, alwan_f64 const *palette, size_t palette_count, alwan_f64 const *values, size_t channels);
alwan_status alwan_palette_apply_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *rgb, size_t in_stride, size_t count, alwan_f32 const *palette, size_t palette_count, alwan_f32 const *values, size_t channels);

/* Colour quantisation: a palette of at most max_colors entries for count 8-bit RGB pixels
 * (pixel_stride bytes apart, at least 3), and optionally each pixel's entry.
 *
 *   ALWAN_QUANTIZE_MEDIAN_CUT  median cut (Heckbert 1982), as Pillow's
 *                              Image.quantize(method=MEDIANCUT) computes it
 *   ALWAN_QUANTIZE_FAST_OCTREE a two-level octree, as Pillow's
 *                              Image.quantize(method=FASTOCTREE) computes it
 *   ALWAN_QUANTIZE_MAX_COVERAGE farthest-point sampling of the distinct colours, as
 *                              Pillow's Image.quantize(method=MAXCOVERAGE) computes it
 *
 * MEDIAN_CUT: the distinct colours of count 8-bit
 * RGB pixels (pixel_stride bytes apart, at least 3) are split into at most max_colors
 * boxes, each time the box with the most pixels on the channel whose range, weighted
 * 77 : 150 : 29, is widest, at the median of its pixel count; a box of one colour is
 * not split. palette_out receives count_out entries of three bytes, each the rounded
 * mean of its box's pixels, in Pillow's order; index_out, when not NULL, one entry a
 * pixel: the nearest palette entry in squared distance, as Pillow maps it. An image of
 * more than 65536 distinct colours is first reduced by dropping low bits, as Pillow
 * does. palette_out must hold 3 * max_colors bytes. ALWAN_E_INVALID for a NULL, no
 * pixels, max_colors 0 or a stride under 3; ALWAN_E_RANGE for max_colors above 65536.
 * Suite 188 holds the palette and the index map to Pillow exactly.
 *
 * FAST_OCTREE: pixels are counted in a 16 x 16 x 16 cube of cells (the top four bits
 * of each channel) and a 4 x 4 x 4 one. Every occupied coarse cell gets an entry and
 * the rest of the palette goes to the most populated fine cells, whose pixels leave
 * their coarse cell; a coarse cell left empty frees its entry for another fine cell.
 * Entries are the coarse cells, most populated first, then the fine ones, each the
 * mean of its pixels in float, truncated; a pixel maps to its fine cell's entry, else
 * its coarse cell's, through a table. Cells of
 * equal count are ordered by index; where max_colors is below the number of occupied
 * coarse cells, the pixels of a coarse cell without an entry map to the nearest entry
 * (Pillow maps them to entry 0). count_out counts entries that hold pixels, where
 * Pillow pads to max_colors with black. Suite 207 holds it to Pillow.
 *
 * MAX_COVERAGE: the first entry is the distinct colour farthest from the mean pixel,
 * each next one the colour farthest from its nearest entry so far, in squared RGB
 * distance, ties to the first colour in the order Pillow's hash table walks them (which
 * is reproduced); pixels map to the nearest entry. Entries are image colours, so the
 * extremes are kept and the bulk of the image is served by few entries. count_out
 * stops at the number of distinct colours, where Pillow repeats one. Suite 208 holds
 * palette and index map to Pillow exactly.
 *
 * An unknown method is ALWAN_E_INVALID. */
typedef enum {
    ALWAN_QUANTIZE_MEDIAN_CUT = 0,
    ALWAN_QUANTIZE_FAST_OCTREE = 1,
    ALWAN_QUANTIZE_MAX_COVERAGE = 2
} alwan_quantize_method;

alwan_status alwan_quantize_u8(unsigned char *palette_out, size_t *count_out, unsigned int *index_out, unsigned char const *rgb, size_t pixel_stride, size_t count, size_t max_colors, alwan_quantize_method method);

/* A 3D histogram of RGB values: counts_out[(r * bins + g) * bins + b] receives how many
 * of count pixels (three values, stride bytes apart) fall in each cell of a bins^3
 * lattice over [lo, hi] per channel, the data a colour-cube or point-cloud view is
 * drawn from. It bins as numpy.histogramdd: edges lo + i (hi - lo) / bins with the last
 * exactly hi, a value's cell is the last edge at or below it, a value equal to hi counts
 * in the last cell, and a pixel with a channel outside [lo, hi] or NaN is not counted.
 * counts_out holds bins^3 entries. ALWAN_E_INVALID for a NULL, a stride under three
 * values, or lo not below hi; ALWAN_E_RANGE for bins 0 or above 1024. Suite 189. */
alwan_status alwan_histogram3d_f32(unsigned int *counts_out, size_t bins, alwan_f32 const *rgb, size_t stride, size_t count, alwan_f32 const lo[3], alwan_f32 const hi[3]);
alwan_status alwan_histogram3d_f64(unsigned int *counts_out, size_t bins, alwan_f64 const *rgb, size_t stride, size_t count, alwan_f64 const lo[3], alwan_f64 const hi[3]);

/* Edge-aware smoothing: each method smooths src while keeping the edges of a guide image
 * (src itself when guide is NULL), the base layer of a base-and-detail edit, a matte
 * refinement, or flash / no-flash denoising.
 *
 *   ALWAN_EDGE_FILTER_GUIDED               He, Sun and Tang, ECCV 2010: a local linear
 *                                          model of the guide in every window; guide 1 or 3
 *                                          channels (OpenCV ximgproc::guidedFilter)
 *   ALWAN_EDGE_FILTER_JOINT_BILATERAL      Tomasi and Manduchi 1998, joint form of
 *                                          Petschnigg et al. and Eisemann and Durand 2004:
 *                                          weights from distance and guide likeness (L1)
 *                                          (ximgproc::jointBilateralFilter)
 *   ALWAN_EDGE_FILTER_ROLLING_GUIDANCE     Zhang, Shen, Xu and Jia, ECCV 2014: the joint
 *                                          bilateral filter iterated on its own output;
 *                                          the guide is not used (ximgproc::rollingGuidanceFilter)
 *   ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_NC  Gastal and Oliveira, SIGGRAPH 2011, normalized
 *                                          convolution (ximgproc::dtFilter DTF_NC)
 *   ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_RF  the same, recursive filtering (DTF_RF)
 *   ALWAN_EDGE_FILTER_FAST_GLOBAL_SMOOTHER Min et al., IEEE TIP 2014: weighted least squares
 *                                          solved row by row and column by column
 *                                          (ximgproc::fastGlobalSmootherFilter)
 *
 * src has src_channels (1 to 4) values a pixel and the guide guide_channels (1 to 4; 1 or
 * 3 for GUIDED), rows at the given byte strides; out has src's layout and may be src.
 * params NULL is every default (alwan_edge_filter_params_{T} lists them). Each method
 * follows the OpenCV function named: GUIDED to 1.8e-7 (suite 190), JOINT_BILATERAL to
 * 4.8e-7 with the colour Gaussian evaluated exactly where OpenCV reads a 4096-bin table,
 * ROLLING_GUIDANCE (from the source) to 7.7e-7 (suite 194), DOMAIN_TRANSFORM to 1.9e-6 (NC)
 * and 1.5e-7 (RF) (suite 195), FAST_GLOBAL_SMOOTHER to 7.8e-6, its guide in any units where
 * OpenCV takes 8 bits (suite 196). ALWAN_E_INVALID for a NULL, a zero size, a channel count
 * out of range, a stride too small, a non-finite value or an unknown method; ALWAN_E_RANGE
 * for a parameter out of its method's range. */
typedef enum {
    ALWAN_EDGE_FILTER_GUIDED = 0,
    ALWAN_EDGE_FILTER_JOINT_BILATERAL = 1,
    ALWAN_EDGE_FILTER_ROLLING_GUIDANCE = 2,
    ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_NC = 3,
    ALWAN_EDGE_FILTER_DOMAIN_TRANSFORM_RF = 4,
    ALWAN_EDGE_FILTER_FAST_GLOBAL_SMOOTHER = 5
} alwan_edge_filter_method;

alwan_status alwan_edge_filter_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t src_channels, alwan_f32 const *guide, size_t guide_row_stride, size_t guide_channels, size_t width, size_t height, alwan_edge_filter_method method, alwan_edge_filter_params_f32 const *params);
alwan_status alwan_edge_filter_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t src_channels, alwan_f64 const *guide, size_t guide_row_stride, size_t guide_channels, size_t width, size_t height, alwan_edge_filter_method method, alwan_edge_filter_params_f64 const *params);

/* Local contrast: contrast that adapts to each neighbourhood.
 *
 *   ALWAN_LOCAL_CONTRAST_LAPLACIAN  the local Laplacian filter (Paris, Hasinoff and Kautz,
 *                                   SIGGRAPH 2011, the fast form of Aubry et al. 2014):
 *                                   differences under sigma are detail, raised to alpha
 *                                   (below 1 adds detail, the "clarity" look; above 1
 *                                   smooths it), larger ones are edges, scaled by beta
 *                                   (below 1 compresses the range), without halos. MATLAB's
 *                                   locallapfilt, to 1.5e-6 (suite 198)
 *   ALWAN_LOCAL_CONTRAST_CLAHE      contrast-limited adaptive histogram equalisation
 *                                   (Zuiderveld 1994): each of tiles_x x tiles_y tiles gets
 *                                   its clipped histogram's cumulative sum as a tone curve,
 *                                   blended between the four nearest tiles. One channel;
 *                                   OpenCV's createCLAHE bit for bit (suite 197)
 *
 * alwan_local_contrast_u8 and _u16 run both (the Laplacian through double in 0..1, rounded
 * back as MATLAB rounds integer output); alwan_local_contrast_{T} runs LAPLACIAN and
 * refuses CLAHE, whose histogram bins need integer data. Rows at the given byte strides;
 * out may be src. params NULL is every default. ALWAN_E_INVALID for a NULL, a zero size, a
 * channel count out of range (CLAHE takes 1), a stride too small, a non-finite value or an
 * unknown method; ALWAN_E_RANGE for a parameter out of its range. */
typedef enum {
    ALWAN_LOCAL_CONTRAST_LAPLACIAN = 0,
    ALWAN_LOCAL_CONTRAST_CLAHE = 1
} alwan_local_contrast_method;

/* Each method reads its own fields; a zero field is its default. */
typedef struct {
    double sigma;            /* LAPLACIAN: the largest difference that is detail, data units; 0 reads as 0.4 */
    double alpha;            /* LAPLACIAN: detail exponent; 0 reads as 0.5. With alpha < 1, differences
                              * under 0.01 are left alone (Paris et al.'s noise level) */
    double beta;             /* LAPLACIAN: edge scale; 0 reads as 1 */
    size_t intensity_levels; /* LAPLACIAN: remapped copies blended per pixel; 0 is MATLAB's count
                              * from alpha (16 to 50) */
    int separate_channels;   /* LAPLACIAN: 0 filters the luma of 3 channels and scales the channels
                              * by it (MATLAB's 'luminance'); non-zero filters each channel */
    size_t tiles_x, tiles_y; /* CLAHE: the tile grid; 0 reads as 8 */
    double clip_limit;       /* CLAHE: clip level in mean bin counts; 0 reads as OpenCV's 40, a negative
                              * value turns clipping off. With 65536 bins it floors to one count on
                              * tiles under 65536 / clip_limit pixels, so 16-bit limits rarely differ */
} alwan_local_contrast_params;

alwan_status alwan_local_contrast_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_local_contrast_method method, alwan_local_contrast_params const *params);
alwan_status alwan_local_contrast_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_local_contrast_method method, alwan_local_contrast_params const *params);
alwan_status alwan_local_contrast_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_local_contrast_method method, alwan_local_contrast_params const *params);
alwan_status alwan_local_contrast_u16(unsigned short *out, size_t out_row_stride, unsigned short const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_local_contrast_method method, alwan_local_contrast_params const *params);

/* Denoising. Each method removes noise its own way; they fail differently (see
 * docs/api/image-tools.md).
 *
 *   ALWAN_DENOISE_TV_CHAMBOLLE           the Rudin, Osher and Fatemi model, sum (u - f)^2 / 2
 *                                        + weight sum |grad u|, by Chambolle's 2004 dual
 *                                        iterations; flat regions go flat, edges stay sharp
 *                                        (scikit-image denoise_tv_chambolle, exact, suite 200)
 *   ALWAN_DENOISE_NL_MEANS               Buades, Coll and Morel 2005: each pixel the mean of the
 *                                        pixels whose surrounding patch looks like its own
 *                                        (OpenCV fastNlMeansDenoising, bit for bit, suite 201)
 *   ALWAN_DENOISE_ANISOTROPIC_DIFFUSION  Perona and Malik 1990: eight-neighbour diffusion that
 *                                        slows across large differences (OpenCV
 *                                        ximgproc::anisotropicDiffusion, bit for bit per
 *                                        iteration, suite 202)
 *   ALWAN_DENOISE_DCT                    Yu and Sapiro 2011: every block's DCT coefficients
 *                                        under 3 sigma zeroed, the overlapping estimates
 *                                        averaged; three channels in an opponent space
 *                                        (OpenCV xphoto::dctDenoising, suite 204, whose last
 *                                        row and column come out 0 or NaN where these do not)
 *   ALWAN_DENOISE_WAVELET                wavelet shrinkage: each detail sub-band of a
 *                                        multilevel orthogonal wavelet transform soft- or
 *                                        hard-thresholded, BayesShrink (Chang et al. 2000) or
 *                                        VisuShrink (Donoho and Johnstone 1994), the noise
 *                                        estimated from the finest diagonal band unless given
 *                                        (scikit-image denoise_wavelet, suite 206)
 *
 * alwan_denoise_u8 runs every method on 8-bit data (TV through double in 0..1, rounded
 * back); alwan_denoise_{T} runs TV_CHAMBOLLE, DCT and WAVELET, and ALWAN_E_INVALID for the two
 * methods whose references are 8-bit. src has 1 to 4 channels, each denoised on its own
 * except that NL means and diffusion measure differences over all of them and DCT turns
 * three channels to an opponent space first; rows at the given byte strides. out may be
 * src, except for NL_MEANS. params NULL is every default. */
typedef enum {
    ALWAN_DENOISE_TV_CHAMBOLLE = 0,
    ALWAN_DENOISE_NL_MEANS = 1,
    ALWAN_DENOISE_ANISOTROPIC_DIFFUSION = 2,
    ALWAN_DENOISE_DCT = 3,
    ALWAN_DENOISE_WAVELET = 4
} alwan_denoise_method;

/* The orthogonal wavelets of ALWAN_DENOISE_WAVELET: Daubechies and symlets. */
typedef enum {
    ALWAN_WAVELET_DB1 = 0, /* Haar */
    ALWAN_WAVELET_DB2 = 1,
    ALWAN_WAVELET_DB3 = 2,
    ALWAN_WAVELET_DB4 = 3,
    ALWAN_WAVELET_DB5 = 4,
    ALWAN_WAVELET_DB6 = 5,
    ALWAN_WAVELET_DB7 = 6,
    ALWAN_WAVELET_DB8 = 7,
    ALWAN_WAVELET_SYM2 = 8,
    ALWAN_WAVELET_SYM3 = 9,
    ALWAN_WAVELET_SYM4 = 10,
    ALWAN_WAVELET_SYM5 = 11,
    ALWAN_WAVELET_SYM6 = 12,
    ALWAN_WAVELET_SYM7 = 13,
    ALWAN_WAVELET_SYM8 = 14
} alwan_wavelet;

/* Each method reads its own fields; a zero field is its default. */
typedef struct {
    double weight;          /* TV_CHAMBOLLE: fidelity against smoothness, values in 0..1; 0 reads as 0.1 */
    double tolerance;       /* TV_CHAMBOLLE: stop when the energy changes by less than this times its first
                             * value; 0 reads as 2e-4 (a tiny value runs every iteration) */
    size_t iterations;      /* TV_CHAMBOLLE: at most, 0 reads as 200. ANISOTROPIC_DIFFUSION: 0 reads as 10 */
    double h;               /* NL_MEANS: strength in 0..255 units; 0 reads as 10 */
    size_t template_window; /* NL_MEANS: patch side, odd; 0 reads as 7 */
    size_t search_window;   /* NL_MEANS: search side, odd; 0 reads as 21 */
    double alpha;           /* ANISOTROPIC_DIFFUSION: step, 0.1 to 0.2 keeps it stable; 0 reads as 0.15 */
    double k;               /* ANISOTROPIC_DIFFUSION: edge threshold, a fraction of full scale per channel;
                             * 0 reads as 0.05 */
    double sigma;           /* DCT, WAVELET: the noise's standard deviation in the data's units (0..255
                             * for 8-bit). DCT: 0 reads as 10 for 8-bit data, 10 / 255 for floats.
                             * WAVELET: 0 estimates it from the finest diagonal sub-band */
    size_t block_size;      /* DCT: the side of the DCT block, 2 to 64, at most the image's sides;
                             * 0 reads as 16 */
    int wavelet;            /* WAVELET: an alwan_wavelet; 0 is ALWAN_WAVELET_DB1 (Haar), scikit-image's
                             * default */
    size_t wavelet_levels;  /* WAVELET: decomposition levels; 0 reads as the maximum minus 3, at least 1 */
    int wavelet_visushrink; /* WAVELET: non-zero thresholds by VisuShrink; 0 by BayesShrink */
    int wavelet_hard;       /* WAVELET: non-zero thresholds hard; 0 soft */
} alwan_denoise_params;

alwan_status alwan_denoise_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_denoise_method method, alwan_denoise_params const *params);
alwan_status alwan_denoise_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_denoise_method method, alwan_denoise_params const *params);
alwan_status alwan_denoise_u8(unsigned char *out, size_t out_row_stride, unsigned char const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_denoise_method method, alwan_denoise_params const *params);

/* Sharpening: the detail of an image amplified.
 *
 *   ALWAN_SHARPEN_UNSHARP_MASK  out = in + amount (in - Gaussian(in)), the detail a Gaussian
 *                               of standard deviation radius removes added back amount
 *                               times; scikit-image's filters.unsharp_mask (suite 203)
 *
 * src has 1 to 4 channels, each sharpened on its own, rows at the given byte strides; out
 * may be src. Sharpening R, G and B apart can fringe colour at edges; sharpen a lightness
 * channel for a gentler result. params NULL is every default. ALWAN_E_INVALID for a NULL,
 * a zero size, a channel count out of range, a stride too small, a non-finite value or an
 * unknown method; ALWAN_E_RANGE for a radius not above 0 or above 1000. */
typedef enum {
    ALWAN_SHARPEN_UNSHARP_MASK = 0
} alwan_sharpen_method;

/* Each method reads its own fields; a zero field is its default. */
typedef struct {
    double radius; /* UNSHARP_MASK: the Gaussian's standard deviation in pixels; 0 reads as 1 */
    double amount; /* UNSHARP_MASK: how many times the detail is added back (may be negative);
                    * 0 reads as 1 */
    int clip;      /* UNSHARP_MASK: non-zero clips to [0, 1], or [-1, 1] when the image has a
                    * negative value, as scikit-image does by default; 0 leaves values as they
                    * come (scikit-image's preserve_range) */
} alwan_sharpen_params;

alwan_status alwan_sharpen_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_sharpen_method method, alwan_sharpen_params const *params);
alwan_status alwan_sharpen_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 const *src, size_t src_row_stride, size_t channels, size_t width, size_t height, alwan_sharpen_method method, alwan_sharpen_params const *params);

/* Colour transfer: the look of a reference image carried onto a source. The two need not
 * be the same size; pixels are `channels` values at the given byte strides (a count, not a
 * width and height, since neither method looks at neighbours). out may be src.
 *
 *   ALWAN_COLOR_TRANSFER_HISTOGRAM_MATCH  each of 1 to 4 channels remapped so its cumulative
 *                                         distribution matches the reference's, as
 *                                         scikit-image's match_histograms (bit for bit,
 *                                         suite 191). Channels are matched apart, so a colour
 *                                         image's drift unless the space decorrelates them:
 *                                         match in Oklab or CIELAB for a gentler transfer
 *   ALWAN_COLOR_TRANSFER_REINHARD2001     Reinhard, Ashikhmin, Gooch and Shirley 2001: mean
 *                                         and standard deviation of each channel matched in
 *                                         Ruderman's l alpha beta (log LMS). Three channels of
 *                                         linear RGB; LMS floored at 1e-6 before the log; a
 *                                         source channel with no spread is shifted, not
 *                                         scaled; not clamped (suite 193)
 *
 * params NULL is the full transfer. ALWAN_E_INVALID for a NULL, no pixels, a channel count
 * out of range (REINHARD2001 takes 3), a stride too small, a non-finite value or an unknown
 * method; ALWAN_E_RANGE for an amount outside 0..1. */
typedef enum {
    ALWAN_COLOR_TRANSFER_HISTOGRAM_MATCH = 0,
    ALWAN_COLOR_TRANSFER_REINHARD2001 = 1
} alwan_color_transfer_method;

typedef struct {
    double amount; /* the result blended with the source, source + amount (transfer - source), in
                    * 0..1; 0 reads as 1, the full transfer */
} alwan_color_transfer_params;

alwan_status alwan_color_transfer_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *src, size_t src_stride, size_t src_count, alwan_f32 const *ref, size_t ref_stride, size_t ref_count, size_t channels, alwan_color_transfer_method method, alwan_color_transfer_params const *params);
alwan_status alwan_color_transfer_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *src, size_t src_stride, size_t src_count, alwan_f64 const *ref, size_t ref_stride, size_t ref_count, size_t channels, alwan_color_transfer_method method, alwan_color_transfer_params const *params);

/* Haze removal.
 *
 *   ALWAN_DEHAZE_DARK_CHANNEL  the dark channel prior (He, Sun and Tang, CVPR 2009 / TPAMI
 *                              2011), the transmission refined by the guided filter
 *                              (ALWAN_EDGE_FILTER_GUIDED) as in their Guided Image
 *                              Filtering paper
 *
 * DARK_CHANNEL:
 * A hazy image is I = J t + A (1 - t); in most patches of a clear outdoor image some
 * channel is near zero, so the patch minimum of I / A measures the haze:
 *
 *   A  = among the brightest top_fraction of the dark channel (the patch minimum of the
 *        channel minimum), the pixel with the largest channel mean
 *   t  = 1 - omega (patch minimum of min_c I_c / A_c), then guided by I
 *   J  = (I - A) / max(t, t0) + A
 *
 * rgb is linear light, three values a pixel, rows row_stride bytes apart; out receives J
 * (it may exceed 1). transmission_out (one value a pixel, may be NULL) receives the
 * refined t, airlight_out (may be NULL) A. Patches are clipped at the border. init: the
 * paper's patch radius 7 (15 x 15), omega 0.95, t0 0.1, top 0.1 %, guided filter radius
 * 30 with eps 1e-3; scale the radii with the image. guide_radius 0 skips the refinement.
 * ALWAN_E_INVALID for a NULL, a zero size, a stride too small, a parameter out of range
 * or a non-finite pixel; ALWAN_E_RANGE when the airlight found has a channel at or
 * below 0. There is no reference implementation to compare with; suite 192 hazes a scene
 * with a known A and t and measures what is recovered. */
typedef struct {
    size_t patch_radius;
    alwan_f64 omega;          /* the haze kept for depth, 0..1 */
    alwan_f64 t0;             /* the transmission floor */
    alwan_f64 top_fraction;   /* of the dark channel searched for the airlight */
    size_t guide_radius;      /* 0: no refinement */
    alwan_f64 guide_eps;
} alwan_dehaze_params;

typedef enum {
    ALWAN_DEHAZE_DARK_CHANNEL = 0
} alwan_dehaze_method;

/* params NULL is alwan_dehaze_params_init's values; an unknown method is ALWAN_E_INVALID. */
void alwan_dehaze_params_init(alwan_dehaze_params *params);
alwan_status alwan_dehaze_f32(alwan_f32 *out, size_t out_row_stride, alwan_f32 *transmission_out, size_t t_row_stride, alwan_f32 airlight_out[3], alwan_f32 const *rgb, size_t row_stride, size_t width, size_t height, alwan_dehaze_method method, alwan_dehaze_params const *params);
alwan_status alwan_dehaze_f64(alwan_f64 *out, size_t out_row_stride, alwan_f64 *transmission_out, size_t t_row_stride, alwan_f64 airlight_out[3], alwan_f64 const *rgb, size_t row_stride, size_t width, size_t height, alwan_dehaze_method method, alwan_dehaze_params const *params);

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
alwan_status alwan_xyz_to_igpgtg_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_igpgtg_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_igpgtg_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_igpgtg_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_igpgtg_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_igpgtg_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> ICaCb batch maps */
alwan_status alwan_xyz_to_icacb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_icacb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_icacb_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_icacb_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_icacb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_icacb_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> hdr-CIELAB batch maps */
alwan_status alwan_xyz_to_hdr_cielab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_hdr_cielab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_hdr_cielab_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_hdr_cielab_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_hdr_cielab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hdr_cielab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> hdr-IPT batch maps */
alwan_status alwan_xyz_to_hdr_ipt_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_hdr_ipt_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_hdr_ipt_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_hdr_ipt_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_hdr_ipt_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hdr_ipt_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> UCS batch maps */
alwan_status alwan_xyz_to_ucs_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_ucs_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_ucs_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_ucs_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_ucs_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ucs_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> OSA-UCS batch maps */
alwan_status alwan_xyz_to_osa_ucs_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_osa_ucs_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_osa_ucs_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_osa_ucs_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_osa_ucs_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_osa_ucs_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> Hunter Lab batch maps (D65 default white) */
alwan_status alwan_xyz_to_hunter_lab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_hunter_lab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_hunter_lab_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_hunter_lab_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_hunter_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hunter_lab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> Hunter Lab batch maps (custom white point) */
alwan_status alwan_xyz_to_hunter_lab_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_hunter_lab_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_hunter_lab_to_xyz_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_hunter_lab_to_xyz_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_hunter_lab_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_hunter_lab_to_xyz_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> ProLab batch maps (D65 default white) */
alwan_status alwan_xyz_to_prolab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_prolab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_prolab_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_prolab_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_xyz_to_prolab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_prolab_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* XYZ <-> ProLab batch maps (custom white point) */
alwan_status alwan_xyz_to_prolab_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_prolab_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_prolab_to_xyz_custom_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_prolab_to_xyz_custom_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_prolab_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_prolab_to_xyz_custom_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* XYZ <-> UVW batch maps (with white point) */
alwan_status alwan_xyz_to_uvw_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_uvw_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_uvw_to_xyz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_uvw_to_xyz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_uvw_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_uvw_to_xyz_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* RGB <-> Prismatic batch maps */
alwan_status alwan_rgb_to_prismatic_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_prismatic_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_prismatic_to_rgb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_prismatic_to_rgb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_prismatic_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_prismatic_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* RGB <-> HCL batch maps */
alwan_status alwan_rgb_to_hcl_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hcl_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_hcl_to_rgb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_hcl_to_rgb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hcl_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hcl_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* RGB <-> IHLS batch maps */
alwan_status alwan_rgb_to_ihls_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_ihls_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_ihls_to_rgb_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count);
alwan_status alwan_ihls_to_rgb_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_ihls_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ihls_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Lab <-> DIN99 batch maps (with variant: 0=DIN99, 1=b, 2=c, 3=d) */
alwan_status alwan_lab_to_din99_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int variant);
alwan_status alwan_lab_to_din99_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int variant);
alwan_status alwan_din99_to_lab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int variant);
alwan_status alwan_din99_to_lab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int variant);
alwan_status alwan_lab_to_din99_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);
alwan_status alwan_din99_to_lab_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);

/* ----------------------------------------------------------------
 * Color Difference (dE) Metrics
 * ---------------------------------------------------------------- */

/* dE*76 - Euclidean distance in Lab space */
alwan_f32  alwan_delta_e_76_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_76_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* dE OK - Euclidean distance in Oklab space (CSS Color Level 4 JND criterion) */
alwan_f32  alwan_delta_e_ok_f32(alwan_oklab_f32 const *a, alwan_oklab_f32 const *b);
alwan_f64 alwan_delta_e_ok_f64(alwan_oklab_f64 const *a, alwan_oklab_f64 const *b);

/* dE*94 - CIE 1994 color difference (graphic arts defaults: kL=1, K1=0.045, K2=0.015) */
alwan_f32  alwan_delta_e_94_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_94_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* dE CMC(l:c) - CMC color difference (defaults: l=2, c=1 for acceptability) */
void alwan_delta_e_cmc_params_default_f32(alwan_delta_e_cmc_params_f32 *p);
void alwan_delta_e_cmc_params_default_f64(alwan_delta_e_cmc_params_f64 *p);
alwan_f32  alwan_delta_e_cmc_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2, alwan_delta_e_cmc_params_f32 const *params);
alwan_f64 alwan_delta_e_cmc_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2, alwan_delta_e_cmc_params_f64 const *params);

/* dE*00 - CIEDE2000 color difference (most perceptually uniform) */
alwan_f32  alwan_delta_e_2000_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_2000_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* dE ITP - ITU-R BT.2124 HDR color difference in ICtCp space (scalar_factor default: 720) */
alwan_f32  alwan_delta_e_itp_f32(alwan_ictcp_f32 const *ictcp1, alwan_ictcp_f32 const *ictcp2, alwan_delta_e_itp_params_f32 const *params);
alwan_f64 alwan_delta_e_itp_f64(alwan_ictcp_f64 const *ictcp1, alwan_ictcp_f64 const *ictcp2, alwan_delta_e_itp_params_f64 const *params);

/* dE HyAB - Hybrid Delta E, improved perceptual metric */
alwan_f32  alwan_delta_e_hyab_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2);
alwan_f64 alwan_delta_e_hyab_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2);

/* dE HyCH - Huang et al. 2015: the CIEDE2000 terms, city block in lightness and
 * Euclidean across chroma and hue. textiles sets k_L to 2, as CIEDE2000 does for
 * textile work; anything else leaves it at 1. CIEDE2000's R_T has no part in it. */
alwan_f32  alwan_delta_e_hych_f32(alwan_lab_f32 const *lab1, alwan_lab_f32 const *lab2, int textiles);
alwan_f64 alwan_delta_e_hych_f64(alwan_lab_f64 const *lab1, alwan_lab_f64 const *lab2, int textiles);

/* The power function Huang et al. 2015 fitted to each colour difference formula, to
 * straighten its relation to visual judgements: dE' = a * dE^b. The pair (a, b) is the
 * one published for that formula. An unknown formula is ALWAN_E_INVALID, and so is a
 * negative difference, which no formula produces. */
typedef enum {
    ALWAN_HUANG2015_CIE1976 = 0,
    ALWAN_HUANG2015_CIE1994 = 1,
    ALWAN_HUANG2015_CIE2000 = 2,
    ALWAN_HUANG2015_CMC = 3,
    ALWAN_HUANG2015_CAM02_LCD = 4,
    ALWAN_HUANG2015_CAM02_SCD = 5,
    ALWAN_HUANG2015_CAM02_UCS = 6,
    ALWAN_HUANG2015_CAM16_UCS = 7,
    ALWAN_HUANG2015_DIN99D = 8,
    ALWAN_HUANG2015_OSA = 9,
    ALWAN_HUANG2015_OSA_GP_EUCLIDEAN = 10,
    ALWAN_HUANG2015_ULAB = 11,
    ALWAN_HUANG2015_COUNT
} alwan_huang2015_formula;

alwan_status alwan_power_function_huang2015_f32(alwan_f32 *out, alwan_f32 delta_e, alwan_huang2015_formula formula);
alwan_status alwan_power_function_huang2015_f64(alwan_f64 *out, alwan_f64 delta_e, alwan_huang2015_formula formula);

/* STRESS, the standardised residual sum of squares of Garcia et al. 2007: how far a set
 * of computed differences sits from the visual differences it should track, after the
 * scale factor between them is taken out. Zero is perfect agreement, and the measure is
 * a fraction, not a percentage. count must be at least 1, and the two arrays are read
 * in step, one pair per judgement. A degenerate set, where the computed and visual
 * differences have
 * no overlap to scale by, is ALWAN_E_DIVZERO; colour-science yields zero there. */
alwan_status alwan_index_stress_f32(alwan_f32 *stress_out, alwan_f32 const *delta_e, alwan_f32 const *delta_v,
                                    size_t count);
alwan_status alwan_index_stress_f64(alwan_f64 *stress_out, alwan_f64 const *delta_e, alwan_f64 const *delta_v,
                                    size_t count);

/* dE DIN99 - Euclidean distance in DIN99 space (variant: 0=DIN99, 1=b, 2=c, 3=d) */
alwan_f32  alwan_delta_e_din99_f32(alwan_din99_f32 const *din99_1, alwan_din99_f32 const *din99_2);
alwan_f64 alwan_delta_e_din99_f64(alwan_din99_f64 const *din99_1, alwan_din99_f64 const *din99_2);

/* dE CAM02-LCD - CIECAM02 Large Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam02_lcd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam02_lcd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* dE CAM02-SCD - CIECAM02 Small Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam02_scd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam02_scd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* dE CAM16-LCD - CAM16 Large Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam16_lcd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam16_lcd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* dE CAM16-SCD - CAM16 Small Color Difference in UCS space */
alwan_f32  alwan_delta_e_cam16_scd_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam16_scd_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* dE CAM02-UCS - CIECAM02 Uniform Color Space (Luo et al. 2006)
 * Uses K_L=1.0, c1=0.007, c2=0.0228 for general-purpose color difference */
alwan_f32  alwan_delta_e_cam02_ucs_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam02_ucs_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* dE CAM16-UCS - CAM16 Uniform Color Space (Li et al. 2017)
 * Uses K_L=1.0, c1=0.007, c2=0.0228 for general-purpose color difference */
alwan_f32  alwan_delta_e_cam16_ucs_f32(alwan_cam_jab_f32 const *jab1, alwan_cam_jab_f32 const *jab2);
alwan_f64 alwan_delta_e_cam16_ucs_f64(alwan_cam_jab_f64 const *jab1, alwan_cam_jab_f64 const *jab2);

/* dE ZCAM - Euclidean distance in ZCAM UCS (Jzazbz) space */
alwan_f32  alwan_delta_e_zcam_f32(alwan_jzazbz_f32 const *jab1, alwan_jzazbz_f32 const *jab2);
alwan_f64 alwan_delta_e_zcam_f64(alwan_jzazbz_f64 const *jab1, alwan_jzazbz_f64 const *jab2);

/* ----------------------------------------------------------------
 * Batch Color Difference (dE) Computations
 * Compare arrays of colors efficiently
 * ---------------------------------------------------------------- */

/* Batch dE*76 - Euclidean distance in Lab space
 * delta_e_out: output dE values (count elements)
 * lab1_in: first array of Lab colors
 * in1_stride: stride for lab1_in in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * lab2_in: second array of Lab colors
 * in2_stride: stride for lab2_in in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * count: number of color pairs to compare
 * Returns ALWAN_OK on success */
alwan_status alwan_delta_e_76_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count);
alwan_status alwan_delta_e_76_f64_batch(alwan_f64 *delta_e_out, alwan_f64 const *lab1_in, size_t in1_stride, alwan_f64 const *lab2_in, size_t in2_stride, size_t count);

/* Every colour-difference metric over two buffers, one distance per pixel.
 * a and b are strided buffers of the metric's colour type read as three scalars
 * in struct order; out is a strided buffer of scalars. Each is its scalar in a
 * loop and agrees with it to the bit (suite 173). ALWAN_E_INVALID for a NULL
 * buffer, or NULL params where the scalar takes them. The four _batch forms
 * below are the older spelling of the same thing for dE76, dE94, dE2000 and
 * CMC, with a packed output; they stay. */
alwan_status alwan_delta_e_76_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_76_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_94_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_94_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_2000_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_2000_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_hyab_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_hyab_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_ok_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_ok_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_din99_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_din99_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_zcam_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_zcam_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam02_ucs_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam02_ucs_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam02_lcd_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam02_lcd_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam02_scd_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam02_scd_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam16_ucs_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam16_ucs_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam16_lcd_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam16_lcd_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam16_scd_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cam16_scd_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count);
alwan_status alwan_delta_e_cmc_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count, alwan_delta_e_cmc_params_f32 const *params);
alwan_status alwan_delta_e_itp_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count, alwan_delta_e_itp_params_f32 const *params);
alwan_status alwan_delta_e_hych_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *a, size_t a_stride, alwan_f32 const *b, size_t b_stride, size_t count, int textiles);
alwan_status alwan_delta_e_cmc_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count, alwan_delta_e_cmc_params_f64 const *params);
alwan_status alwan_delta_e_itp_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count, alwan_delta_e_itp_params_f64 const *params);
alwan_status alwan_delta_e_hych_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *a, size_t a_stride, alwan_f64 const *b, size_t b_stride, size_t count, int textiles);

/* Batch dE*00 - CIEDE2000 color difference
 * Strides in1_stride/in2_stride are in bytes (typically 3*sizeof(alwan_f32/alwan_f64)) */
alwan_status alwan_delta_e_2000_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count);
alwan_status alwan_delta_e_2000_f64_batch(alwan_f64 *delta_e_out,
                             alwan_f64 const *lab1_in, size_t in1_stride,
                             alwan_f64 const *lab2_in, size_t in2_stride,
                             size_t count);

/* Batch dE*94 - CIE 1994 color difference
 * Strides in1_stride/in2_stride are in bytes (typically 3*sizeof(alwan_f32/alwan_f64)) */
alwan_status alwan_delta_e_94_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count);
alwan_status alwan_delta_e_94_f64_batch(alwan_f64 *delta_e_out, alwan_f64 const *lab1_in, size_t in1_stride, alwan_f64 const *lab2_in, size_t in2_stride, size_t count);

/* Batch dE CMC(l:c) - CMC color difference
 * Strides in1_stride/in2_stride are in bytes (typically 3*sizeof(alwan_f32/alwan_f64)) */
alwan_status alwan_delta_e_cmc_f32_batch(alwan_f32 *delta_e_out, alwan_f32 const *lab1_in, size_t in1_stride, alwan_f32 const *lab2_in, size_t in2_stride, size_t count, alwan_f32 l, alwan_f32 c);
alwan_status alwan_delta_e_cmc_f64_batch(alwan_f64 *delta_e_out, alwan_f64 const *lab1_in, size_t in1_stride, alwan_f64 const *lab2_in, size_t in2_stride, size_t count, alwan_f64 l, alwan_f64 c);

/* Typed delta E batch functions (_ex variants) */
alwan_status alwan_delta_e_76_batch_ex(alwan_f64 *delta_e_out,
                               void const *lab1_in, size_t in1_stride,
                               void const *lab2_in, size_t in2_stride,
                               size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);
alwan_status alwan_delta_e_2000_batch_ex(alwan_f64 *delta_e_out,
                                 void const *lab1_in, size_t in1_stride,
                                 void const *lab2_in, size_t in2_stride,
                                 size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);
alwan_status alwan_delta_e_94_batch_ex(alwan_f64 *delta_e_out,
                               void const *lab1_in, size_t in1_stride,
                               void const *lab2_in, size_t in2_stride,
                               size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt);
/* params as the scalar takes them (NULL for the defaults, l = 2, c = 1); until 3.0.0
 * this form took raw l and c where the scalar and the map took the struct. */
alwan_status alwan_delta_e_cmc_batch_ex(alwan_f64 *delta_e_out,
                                void const *lab1_in, size_t in1_stride,
                                void const *lab2_in, size_t in2_stride,
                                size_t count, alwan_pixel_format lab1_fmt, alwan_pixel_format lab2_fmt,
                                alwan_delta_e_cmc_params_f64 const *params);

/* ----------------------------------------------------------------
 * Whiteness & Yellowness Indices
 * ---------------------------------------------------------------- */

/* Illuminant/Observer pairs for ASTM E313 calculations */
typedef enum {
    ALWAN_ASTM_E313_C_2DEG = 0,    /* Illuminant C, CIE 1931 2 deg observer */
    ALWAN_ASTM_E313_D65_2DEG = 1,  /* Illuminant D65, CIE 1931 2 deg observer */
    ALWAN_ASTM_E313_C_10DEG = 2,   /* Illuminant C, CIE 1964 10 deg observer */
    ALWAN_ASTM_E313_D65_10DEG = 3  /* Illuminant D65, CIE 1964 10 deg observer */
} alwan_astm_e313_illuminant;

/* ASTM E313 Yellowness Index, YI = 100 (Cx X - Cz Z) / Y.
 * xyz: CIE XYZ tristimulus values (normalized to Y=100 for perfect white)
 * illuminant: illuminant/observer pair (C/2 deg, D65/2 deg, C/10 deg, or D65/10 deg)
 * yi_out receives the index. ALWAN_E_INVALID for a NULL or an illuminant outside
 * the enum, ALWAN_E_RANGE for Y at zero, where the index is undefined.
 * Until 3.0.0 these three returned the value and signalled failure as -1, which a
 * yellowness index or a whiteness index can legitimately be; the status and the
 * out parameter are the form the rest of the family (Berger, Taube, Stensby,
 * Ganz, ASTM D1925) already took. */
alwan_status alwan_yellowness_astm_e313_f32(alwan_f32 *yi_out, alwan_xyz_f32 const *xyz, alwan_astm_e313_illuminant illuminant);
alwan_status alwan_yellowness_astm_e313_f64(alwan_f64 *yi_out, alwan_xyz_f64 const *xyz, alwan_astm_e313_illuminant illuminant);

/* ASTM E313 Whiteness Index, WI = 3.388 Z - 3 Y. The formula does not depend on
 * the illuminant/observer pair; the parameter is kept beside the yellowness
 * one and must still be inside the enum. ALWAN_E_INVALID for a NULL or an
 * illuminant outside the enum. */
alwan_status alwan_whiteness_astm_e313_f32(alwan_f32 *wi_out, alwan_xyz_f32 const *xyz, alwan_astm_e313_illuminant illuminant);
alwan_status alwan_whiteness_astm_e313_f64(alwan_f64 *wi_out, alwan_xyz_f64 const *xyz, alwan_astm_e313_illuminant illuminant);

/* CIE 2004 Whiteness Index, W = Y + 800 (xn - x) + 1700 (yn - y).
 * xy: CIE 1931 chromaticity coordinates (x, y)
 * Y: CIE Y tristimulus value (luminance factor)
 * xy_n: reference white chromaticity coordinates
 * w_out receives W. The tint, 900 (xn - x) - 650 (yn - y) for the 2 degree
 * observer and 1000 (xn - x) - 650 (yn - y) for the 10 degree one, is not
 * returned here; alwan_whiteness_ganz1979 returns a tint with its whiteness.
 * ALWAN_E_INVALID for a NULL. */
alwan_status alwan_whiteness_cie2004_f32(alwan_f32 *w_out, alwan_vec2_f32 const *xy, alwan_f32 Y, alwan_vec2_f32 const *xy_n);
alwan_status alwan_whiteness_cie2004_f64(alwan_f64 *w_out, alwan_vec2_f64 const *xy, alwan_f64 Y, alwan_vec2_f64 const *xy_n);

/* ----------------------------------------------------------------
 * Lightness, luminance, Munsell value, whiteness and yellowness
 *
 * The rest of colour-science's lightness, luminance, Munsell value, whiteness and
 * yellowness registries, in its forms. Each method takes Y in its own domain, noted
 * with it. ALWAN_E_INVALID for an unknown method, a NULL output or a divisor of zero.
 * ---------------------------------------------------------------- */
typedef enum {
    ALWAN_LIGHTNESS_CIE1976 = 0,                    /* L*, Y in [0, 100] against Y_n */
    ALWAN_LIGHTNESS_GLASSER1958 = 1,                /* Y in [0, 100] */
    ALWAN_LIGHTNESS_WYSZECKI1963 = 2,               /* W*, Y in [0, 100], meant for 1 to 98 */
    ALWAN_LIGHTNESS_FAIRCHILD2010 = 3,              /* hdr-CIELAB 2010, Y in [0, 1] and above */
    ALWAN_LIGHTNESS_FAIRCHILD2011_CIELAB = 4,       /* hdr-CIELAB 2011, Y in [0, 1] and above */
    ALWAN_LIGHTNESS_FAIRCHILD2011_IPT = 5,          /* hdr-IPT 2011 */
    ALWAN_LIGHTNESS_ABEBE2017_MICHAELIS_MENTEN = 6, /* Y in cd/m2 against the adapting Y_n */
    ALWAN_LIGHTNESS_ABEBE2017_STEVENS = 7
} alwan_lightness_method;

/* Lightness of Y, and the luminance of a lightness. Glasser 1958 and Wyszecki 1963
 * invert in closed form, as colour-science does not. params NULL is every default. */
alwan_status alwan_lightness_f64(alwan_f64 *L_out, alwan_f64 Y, alwan_lightness_method method, alwan_lightness_params_f64 const *params);
alwan_status alwan_lightness_f32(alwan_f32 *L_out, alwan_f32 Y, alwan_lightness_method method, alwan_lightness_params_f32 const *params);
alwan_status alwan_luminance_from_lightness_f64(alwan_f64 *Y_out, alwan_f64 L, alwan_lightness_method method, alwan_lightness_params_f64 const *params);
alwan_status alwan_luminance_from_lightness_f32(alwan_f32 *Y_out, alwan_f32 L, alwan_lightness_method method, alwan_lightness_params_f32 const *params);

typedef enum {
    ALWAN_MUNSELL_VALUE_ASTM_D1535 = 0,     /* the exact inverse of D1535's quintic */
    ALWAN_MUNSELL_VALUE_PRIEST1920 = 1,
    ALWAN_MUNSELL_VALUE_MUNSELL1933 = 2,
    ALWAN_MUNSELL_VALUE_MOON1943 = 3,
    ALWAN_MUNSELL_VALUE_SAUNDERSON1944 = 4,
    ALWAN_MUNSELL_VALUE_LADD1955 = 5,
    ALWAN_MUNSELL_VALUE_MCCAMY1987 = 6
} alwan_munsell_value_method;

typedef enum {
    ALWAN_MUNSELL_LUMINANCE_ASTM_D1535 = 0, /* ASTM D1535-08 */
    ALWAN_MUNSELL_LUMINANCE_NEWHALL1943 = 1 /* Newhall, Nickerson and Judd 1943 */
} alwan_munsell_luminance_method;

/* Munsell value, 0 to 10, of Y in [0, 100], and the Y of a Munsell value. */
alwan_status alwan_munsell_value_f64(alwan_f64 *V_out, alwan_f64 Y, alwan_munsell_value_method method);
alwan_status alwan_munsell_value_f32(alwan_f32 *V_out, alwan_f32 Y, alwan_munsell_value_method method);
alwan_status alwan_luminance_from_munsell_value_f64(alwan_f64 *Y_out, alwan_f64 V, alwan_munsell_luminance_method method);
alwan_status alwan_luminance_from_munsell_value_f32(alwan_f32 *Y_out, alwan_f32 V, alwan_munsell_luminance_method method);

/* Whiteness: Berger 1959 and Taube 1960 against the illuminant's XYZ_0, Stensby 1968 of
 * CIELAB, Ganz 1979 of xy and Y, with its tint. XYZ in [0, 100]. */
alwan_status alwan_whiteness_berger1959_f64(alwan_f64 *W_out, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *xyz_0);
alwan_status alwan_whiteness_berger1959_f32(alwan_f32 *W_out, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *xyz_0);
alwan_status alwan_whiteness_taube1960_f64(alwan_f64 *W_out, alwan_xyz_f64 const *xyz, alwan_xyz_f64 const *xyz_0);
alwan_status alwan_whiteness_taube1960_f32(alwan_f32 *W_out, alwan_xyz_f32 const *xyz, alwan_xyz_f32 const *xyz_0);
alwan_status alwan_whiteness_stensby1968_f64(alwan_f64 *W_out, alwan_lab_f64 const *lab);
alwan_status alwan_whiteness_stensby1968_f32(alwan_f32 *W_out, alwan_lab_f32 const *lab);
alwan_status alwan_whiteness_ganz1979_f64(alwan_f64 *W_out, alwan_f64 *T_out, alwan_vec2_f64 const *xy, alwan_f64 Y);
alwan_status alwan_whiteness_ganz1979_f32(alwan_f32 *W_out, alwan_f32 *T_out, alwan_vec2_f32 const *xy, alwan_f32 Y);

/* Yellowness: ASTM D1925 and the ASTM E313 alternative. XYZ in [0, 100]; a Y of 0 is
 * ALWAN_E_INVALID, where colour-science returns 0. */
alwan_status alwan_yellowness_astm_d1925_f64(alwan_f64 *YI_out, alwan_xyz_f64 const *xyz);
alwan_status alwan_yellowness_astm_d1925_f32(alwan_f32 *YI_out, alwan_xyz_f32 const *xyz);
alwan_status alwan_yellowness_astm_e313_alternative_f64(alwan_f64 *YI_out, alwan_xyz_f64 const *xyz);
alwan_status alwan_yellowness_astm_e313_alternative_f32(alwan_f32 *YI_out, alwan_xyz_f32 const *xyz);

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
    ALWAN_CAT_ZHAI_2018       = 11, /* Zhai & Luo 2018 two-step CAT */

    /* The classic von Kries cone space: the Hunt-Pointer-Estevez matrix normalised to
     * equal energy, which CIE 1994 and Fairchild 1990 both work in. This is not the
     * unnormalised HPE matrix alwan embeds for IPT; the two differ in every element. */
    ALWAN_CAT_VON_KRIES       = 12
} alwan_cat_method;

/* Compute chromatic adaptation matrix from source to destination white point
 * src_white_xyz: source white point in XYZ (normalized to Y=1)
 * dst_white_xyz: destination white point in XYZ (normalized to Y=1)
 * method: CAT method (Bradford, CAT02, CAT16, or XYZ scaling)
 * out: output 3x3 adaptation matrix
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if white points are invalid */
alwan_status alwan_cat_matrix_f32(alwan_mat3x3_f32 *out,
                         alwan_xyz_f32 const *src_white_xyz,
                         alwan_xyz_f32 const *dst_white_xyz,
                         alwan_cat_method method);
alwan_status alwan_cat_matrix_f64(alwan_mat3x3_f64 *out,
                         alwan_xyz_f64 const *src_white_xyz,
                         alwan_xyz_f64 const *dst_white_xyz,
                         alwan_cat_method method);

/* Apply chromatic adaptation to XYZ colors (map operation)
 * xyz_out: output XYZ colors (stride out_stride between consecutive colors)
 * out_stride: stride for output (in bytes, typically 3*sizeof(alwan_f32/alwan_f64) for packed array)
 * xyz_in: input XYZ colors (stride in_stride between consecutive colors)
 * in_stride: stride for input (in bytes, typically 3*sizeof(alwan_f32/alwan_f64) for packed array)
 * count: number of colors to transform
 * src_white_xyz: source white point in XYZ
 * dst_white_xyz: destination white point in XYZ
 * method: CAT method
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if parameters are invalid */
alwan_status alwan_xyz_adapt_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f32 const *src_white_xyz, alwan_xyz_f32 const *dst_white_xyz, alwan_cat_method method);
alwan_status alwan_xyz_adapt_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *xyz_in, size_t in_stride, size_t count, alwan_xyz_f64 const *src_white_xyz, alwan_xyz_f64 const *dst_white_xyz, alwan_cat_method method);

/* Zhai & Luo 2018 two-step chromatic adaptation
 * Adapts XYZ from input illuminant to output illuminant via baseline illuminant
 * xyz_in: input XYZ color under source illuminant
 * xyz_src: source illuminant XYZ, on the scale of xyz_in
 * xyz_dst: destination illuminant XYZ, on ANY scale: each white enters as
 *          Y_w / RGB_w, which is scale-free, so (0.9505, 1, 1.0891) and
 *          (95.05, 100, 108.91) adapt identically. Until 2026-09-21 the three
 *          whites had to share one scale, or the result was scaled with them
 * D_src: degree of adaptation for source illuminant [0,1] (1=full adaptation)
 * D_dst: degree of adaptation for destination illuminant [0,1] (1=full adaptation)
 * xyz_baseline: baseline illuminant XYZ (NULL for equal-energy white [100,100,100])
 * transform: underlying CAT transform (ALWAN_CAT_CAT02 or ALWAN_CAT_CAT16)
 * xyz_out: output adapted XYZ color
 * Returns ALWAN_OK on success,
 *         ALWAN_E_INVALID if any of xyz_out/xyz_in/xyz_src/xyz_dst is NULL or
 *                         transform is not ALWAN_CAT_CAT02/ALWAN_CAT_CAT16,
 *         ALWAN_E_RANGE   if D_src or D_dst is outside [0, 1] */
alwan_status alwan_cat_zhai2018_f32(alwan_xyz_f32 *xyz_out,
                           alwan_xyz_f32 const *xyz_in,
                           alwan_xyz_f32 const *xyz_src,
                           alwan_xyz_f32 const *xyz_dst,
                           alwan_f32 D_src,
                           alwan_f32 D_dst,
                           alwan_xyz_f32 const *xyz_baseline,
                           alwan_cat_method transform);
alwan_status alwan_cat_zhai2018_f64(alwan_xyz_f64 *xyz_out,
                           alwan_xyz_f64 const *xyz_in,
                           alwan_xyz_f64 const *xyz_src,
                           alwan_xyz_f64 const *xyz_dst,
                           alwan_f64 D_src,
                           alwan_f64 D_dst,
                           alwan_xyz_f64 const *xyz_baseline,
                           alwan_cat_method transform);

/* The same adaptation over a buffer of XYZ triples, strides in bytes. The
 * per-white gains are computed once, then applied per pixel in the arithmetic
 * the one-colour form uses, so the two agree to the bit. Same validation and
 * the same status codes. */
alwan_status alwan_cat_zhai2018_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count,
                           alwan_xyz_f32 const *xyz_src, alwan_xyz_f32 const *xyz_dst, alwan_f32 D_src, alwan_f32 D_dst,
                           alwan_xyz_f32 const *xyz_baseline, alwan_cat_method transform);
alwan_status alwan_cat_zhai2018_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count,
                           alwan_xyz_f64 const *xyz_src, alwan_xyz_f64 const *xyz_dst, alwan_f64 D_src, alwan_f64 D_dst,
                           alwan_xyz_f64 const *xyz_baseline, alwan_cat_method transform);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_cat_zhai2018_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *xyz_src, alwan_xyz_f32 const *xyz_dst, alwan_f32 D_src, alwan_f32 D_dst, alwan_xyz_f32 const *xyz_baseline, alwan_cat_method transform);
alwan_status alwan_cat_zhai2018_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *xyz_src, alwan_xyz_f64 const *xyz_dst, alwan_f64 D_src, alwan_f64 D_dst, alwan_xyz_f64 const *xyz_baseline, alwan_cat_method transform);

/* CIE 1994 chromatic adaptation model (CIE 109-1994).
 *
 * Adapts a stimulus seen under one adapting field to the corresponding colour under
 * another. This is not a matrix: the exponents depend on the adapting luminances, so every
 * stimulus is adapted on its own terms.
 *
 * xyz_in: stimulus under the test field, Y = 100 scale
 * xy_o1, xy_o2: chromaticities of the test and reference adapting fields
 * Y_o: luminance factor of the adapting background. The model is defined on [18, 100];
 *      outside it the result is extrapolation and the call still returns it.
 * E_o1, E_o2: illuminance of the test and reference fields, lux
 * n: noise term, 1 in the published model
 * Returns ALWAN_E_INVALID on a NULL argument or a non-positive y in either chromaticity. */
alwan_status alwan_cat_cie1994_f32(alwan_xyz_f32 *xyz_out,
                          alwan_xyz_f32 const *xyz_in,
                          alwan_vec2_f32 const *xy_o1,
                          alwan_vec2_f32 const *xy_o2,
                          alwan_f32 Y_o, alwan_f32 E_o1, alwan_f32 E_o2, alwan_f32 n);
alwan_status alwan_cat_cie1994_f64(alwan_xyz_f64 *xyz_out,
                          alwan_xyz_f64 const *xyz_in,
                          alwan_vec2_f64 const *xy_o1,
                          alwan_vec2_f64 const *xy_o2,
                          alwan_f64 Y_o, alwan_f64 E_o1, alwan_f64 E_o2, alwan_f64 n);

/* vK20 chromatic adaptation model (Fairchild 2020).
 *
 * Adapts towards a weighted mixture of three whites rather than one: the previous white
 * xyz_p, the current white xyz_n and a fixed reference xyz_r, weighted by D_p, D_n and
 * D_r. Fairchild's own condition is D_n = 0.7, D_r = 0.3, D_p = 0; D_n = 1 with the other
 * two at zero is a plain von Kries adaptation to the current white.
 *
 * xyz_r: NULL for the model's own reference illuminant (0.97941176, 1, 1.73235294)
 * transform: the cone space to work in, any matrix method of alwan_cat_method
 *
 * Scale matters here, unlike the matrix transforms. The default reference white sits on
 * the Y = 1 scale, so the stimulus and the other two whites must be on it as well: feeding
 * Y = 100 values while leaving xyz_r at its default mixes two scales and moves the result
 * by a different factor in each channel, not by 100. To work at Y = 100, pass an xyz_r
 * scaled to match.
 *
 * Returns ALWAN_E_INVALID on a NULL argument or a method that has no matrix. */
alwan_status alwan_cat_vk20_f32(alwan_xyz_f32 *xyz_out,
                          alwan_xyz_f32 const *xyz_in,
                          alwan_xyz_f32 const *xyz_p,
                          alwan_xyz_f32 const *xyz_n,
                          alwan_xyz_f32 const *xyz_r,
                          alwan_f32 D_n, alwan_f32 D_r, alwan_f32 D_p,
                          alwan_cat_method transform);
alwan_status alwan_cat_vk20_f64(alwan_xyz_f64 *xyz_out,
                          alwan_xyz_f64 const *xyz_in,
                          alwan_xyz_f64 const *xyz_p,
                          alwan_xyz_f64 const *xyz_n,
                          alwan_xyz_f64 const *xyz_r,
                          alwan_f64 D_n, alwan_f64 D_r, alwan_f64 D_p,
                          alwan_cat_method transform);

/* Li 2025 chromatic adaptation model.
 *
 * A von Kries step in CAT16 cone space whose degree of adaptation follows the CIECAM form
 * D = F (1 - (1/3.6) exp((-L_A - 42) / 92)), clamped to [0, 1], and which carries the two
 * whites' luminances through, so a change in adapting luminance is not lost.
 *
 * xyz_in, xyz_ws, xyz_wd: stimulus, source white, destination white, Y = 100 scale
 * L_A: adapting field luminance, cd/m2
 * F_surround: 1.0 average, 0.9 dim, 0.8 dark
 * discount_illuminant: non-zero to force D = 1
 * Returns ALWAN_E_INVALID on a NULL argument. */
alwan_status alwan_cat_li2025_f32(alwan_xyz_f32 *xyz_out,
                          alwan_xyz_f32 const *xyz_in,
                          alwan_xyz_f32 const *xyz_ws,
                          alwan_xyz_f32 const *xyz_wd,
                          alwan_f32 L_A, alwan_f32 F_surround, int discount_illuminant);
alwan_status alwan_cat_li2025_f64(alwan_xyz_f64 *xyz_out,
                          alwan_xyz_f64 const *xyz_in,
                          alwan_xyz_f64 const *xyz_ws,
                          alwan_xyz_f64 const *xyz_wd,
                          alwan_f64 L_A, alwan_f64 F_surround, int discount_illuminant);

/* ----------------------------------------------------------------
 * Spectral Power Distributions (SPD)
 * ---------------------------------------------------------------- */

/* Observer type (standard color matching functions) */

typedef enum {
    ALWAN_OBSERVER_CIE_1931_2DEG = 0,  /* CIE 1931 2 deg standard observer */
    ALWAN_OBSERVER_CIE_1964_10DEG = 1, /* CIE 1964 10 deg standard observer */
    /* CIE 2012 and CIE 2015 are ONE observer under two names: the physiologically
     * based CMFs derived from the CIE 2006 cone fundamentals, which CVRL published
     * as the "2012" proposal and CIE 170-2:2015 standardised. Each pair returns the
     * same CMFs, bit for bit, from one table set. Both names stay so code written
     * against either keeps compiling; the 2015 names are the standard's. */
    ALWAN_OBSERVER_CIE_2012_2DEG = 2,  /* Same observer as ALWAN_OBSERVER_CIE_2015_2DEG */
    ALWAN_OBSERVER_CIE_2012_10DEG = 3, /* Same observer as ALWAN_OBSERVER_CIE_2015_10DEG */

    /* Extended observers */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG = 4,  /* Stockman & Sharpe 2000 2 deg cone fundamentals */
    ALWAN_OBSERVER_CIE_2015_2DEG = 5,         /* CIE 170-2:2015 2 deg cone-fundamental-based observer */
    ALWAN_OBSERVER_CIE_2015_10DEG = 6,        /* CIE 170-2:2015 10 deg cone-fundamental-based observer */
    ALWAN_OBSERVER_WRIGHT_GUILD_1931 = 7,     /* Wright & Guild 1931 2 deg RGB CMFs (historical) */

    /* Tabulated over part of 360-830nm and zero outside it. A CMF is zero where the
     * observer has no response, which is not the constant hold the illuminants use.
     * Smith & Pokorny is l/m/s cone fundamentals in the x/y/z slots, as Stockman &
     * Sharpe already is; both Stiles & Burch sets are r/g/b RGB CMFs, as Wright &
     * Guild is. */
    ALWAN_OBSERVER_STOCKMAN_SHARPE_10DEG = 8,   /* Stockman & Sharpe 10 deg cone fundamentals, 390-830 */
    ALWAN_OBSERVER_SMITH_POKORNY_1975 = 9,      /* Smith & Pokorny 1975 normal trichromats, 380-780 */
    ALWAN_OBSERVER_STILES_BURCH_1955_2DEG = 10, /* Stiles & Burch 1955 2 deg RGB CMFs, 390-730 */
    ALWAN_OBSERVER_STILES_BURCH_1959_10DEG = 11 /* Stiles & Burch 1959 10 deg RGB CMFs, 390-830 */
} alwan_observer_type;

/* Get white point XYZ for a standard illuminant
 * Computes XYZ tristimulus values from illuminant xy chromaticity (Y normalized to 1.0)
 * Returns ALWAN_E_INVALID if illuminant not supported */
alwan_status alwan_illuminant_white_point_f64(alwan_xyz_f64 *out_xyz,
                                   alwan_illuminant illuminant,
                                   alwan_observer_type observer);
alwan_status alwan_illuminant_white_point_f32(alwan_xyz_f32 *out_xyz,
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
    ALWAN_EXTRAPOLATE_LINEAR = 2,   /* Linear extrapolation from edge slope */
    /* The edge slope, floored at zero. A measured source that stops short of
     * the table it is written into has to be continued somehow, and a straight
     * line drawn from the last two samples crosses zero and keeps going: a
     * negative radiance is not a value the source can take. This keeps the
     * slope where it is positive and stops at zero where it is not.
     *
     * It is for sources whose measurement ends well inside the table, where
     * holding the last value flat across 100 nm or more would invent a plateau
     * the instrument never saw. Over a short gap ALWAN_EXTRAPOLATE_CONSTANT is
     * the better answer and the existing tables use it.
     *
     * Only the one mode was added. Other continuations are arguable, and an
     * arguable convention wants a source that needs it rather than a place in
     * an enum. */
    ALWAN_EXTRAPOLATE_LINEAR_CLAMP_ZERO = 3
} alwan_extrapolate_mode;

/* SPD integration method for computing XYZ */
typedef enum {
    ALWAN_INTEGRATE_TRAPEZOID = 0,  /* Trapezoidal rule (fast) */
    ALWAN_INTEGRATE_SIMPSON = 1     /* Simpson's rule (more accurate) */
} alwan_integrate_method;

/* Create SPD with uniform sampling
 * out: output SPD structure (values allocated internally)
 * wavelength_min: starting wavelength (nm)
 * wavelength_max: ending wavelength (nm)
 * count: number of samples
 * ctx: context (for allocation)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
alwan_status alwan_spd_create_f64(alwan_spd_f64 *out, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx);
alwan_status alwan_spd_create_f32(alwan_spd_f32 *out, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx);

/* Destroy SPD and free allocated values */
void alwan_spd_destroy_f64(alwan_spd_f64 *spd, alwan_ctx *ctx);
void alwan_spd_destroy_f32(alwan_spd_f32 *spd, alwan_ctx *ctx);

/* Load standard illuminant SPD
 * out: output SPD structure
 * ill: illuminant to load
 * ctx: context (for data path and allocation)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if illuminant not supported */
alwan_status alwan_spd_illuminant_f64(alwan_spd_f64 *out, alwan_illuminant ill, alwan_ctx *ctx);
alwan_status alwan_spd_illuminant_f32(alwan_spd_f32 *out, alwan_illuminant ill, alwan_ctx *ctx);

/* Generate blackbody (Planckian) SPD at given temperature
 * Uses Planck's law to compute spectral radiance
 * out: output SPD structure (values allocated internally)
 * temperature_K: color temperature in Kelvin (typically 1000-25000K)
 * wavelength_min: starting wavelength (nm)
 * wavelength_max: ending wavelength (nm)
 * count: number of samples
 * ctx: context
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if temperature out of range */
alwan_status alwan_spd_blackbody_f64(alwan_spd_f64 *out, alwan_f64 temperature_K, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx);
alwan_status alwan_spd_blackbody_f32(alwan_spd_f32 *out, alwan_f32 temperature_K, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx);

/* Resample SPD to new wavelength range/count
 * dst: destination SPD (values allocated internally)
 * src: source SPD
 * wavelength_min: new starting wavelength (nm)
 * wavelength_max: new ending wavelength (nm)
 * count: new number of samples
 * method: resampling method (linear or Catmull-Rom)
 * extrapolate: extrapolation mode for out-of-range values
 * ctx: context
 * Returns ALWAN_OK on success, and ALWAN_E_INVALID for a method or an
 * extrapolation mode that is not one of the enumerated values.
 *
 * That last part changed when ALWAN_EXTRAPOLATE_LINEAR_CLAMP_ZERO was added.
 * Both enums were read by an if/else chain whose last arm took everything
 * else, so an unrecognised method resampled by Catmull-Rom and an
 * unrecognised extrapolation mode extrapolated linearly, silently. With more
 * values to mistype, a wrong argument is better refused than guessed at. */
alwan_status alwan_spd_resample_f64(alwan_spd_f64 *dst, alwan_spd_f64 const *src, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx);
alwan_status alwan_spd_resample_f32(alwan_spd_f32 *dst, alwan_spd_f32 const *src, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx);

/* The seven ISO 7589 sensitometric sources, tabulated at 10 nm from 350 to 690 nm,
 * the Printer from 350 to 560 nm. They are not in alwan_illuminant, and the reason
 * is what a caller has to know about them: five of the seven are still climbing
 * where the tabulation stops, the tungsten and photoflood sources rising toward a
 * Planckian peak well past 690 nm. Holding the last value flat across the rest of a
 * 360-830 nm table, as every alwan_illuminant does, would understate them by 16% at
 * 780 nm against the Academy's own measurement of the studio tungsten source; a
 * straight line overstates by 6%. Neither is a number a standard's name should
 * carry, so alwan invents none: the tables ship as tabulated and the caller chooses. */
typedef enum {
    ALWAN_ISO7589_PHOTOGRAPHIC_DAYLIGHT = 0,
    ALWAN_ISO7589_SENSITOMETRIC_DAYLIGHT = 1,
    ALWAN_ISO7589_STUDIO_TUNGSTEN = 2,
    ALWAN_ISO7589_SENSITOMETRIC_STUDIO_TUNGSTEN = 3,
    ALWAN_ISO7589_PHOTOFLOOD = 4,
    ALWAN_ISO7589_SENSITOMETRIC_PHOTOFLOOD = 5,
    ALWAN_ISO7589_SENSITOMETRIC_PRINTER = 6  /* 350-560 nm; 22 samples where the others have 35 */
} alwan_iso7589_source;

/* The source exactly as tabulated: 35 samples on 350-690 nm at 10 nm, 22 on 350-560 nm
 * for the Printer. Creates the SPD. ALWAN_E_NODATA when the table was compiled out. */
alwan_status alwan_spd_iso7589_native_f64(alwan_spd_f64 *out, alwan_iso7589_source source, alwan_ctx *ctx);
alwan_status alwan_spd_iso7589_native_f32(alwan_spd_f32 *out, alwan_iso7589_source source, alwan_ctx *ctx);

/* The source on the caller's grid, by alwan_spd_resample from the native tabulation
 * with the caller's method and extrapolation mode. Nothing is done to the values on the
 * way: this is the native table and one resample, so ALWAN_EXTRAPOLATE_CONSTANT is the
 * flat hold the other illuminants have, ALWAN_EXTRAPOLATE_ZERO is what the CMFs have,
 * and ALWAN_EXTRAPOLATE_LINEAR_CLAMP_ZERO continues the end slope and stops at zero,
 * which the two descending daylight sources reach. For a Planckian continuation, which
 * is the physics of the five ascending sources, see alwan_spd_extend_planckian.
 *
 * Studio tungsten is also carried by the camera pack as alwan_spd_iso7589_tungsten,
 * the Academy's tabulation on 380-780 nm at 5 nm. They are the same curve at scalings
 * 200 apart: normalised at 560 nm they agree exactly over 380-690 nm. */
alwan_status alwan_spd_iso7589_f64(alwan_spd_f64 *out, alwan_iso7589_source source, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx);
alwan_status alwan_spd_iso7589_f32(alwan_spd_f32 *out, alwan_iso7589_source source, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_resample_method method, alwan_extrapolate_mode extrapolate, alwan_ctx *ctx);

/* An SPD continued past its ends along a Planckian curve, for a source that is
 * near-thermal where the tabulation stops: tungsten, photoflood, an incandescent lamp.
 *
 * The temperature is fitted to the last fit_count samples of src (0 fits every sample)
 * by least squares with the scale free, so only the SHAPE of the window sets T; a
 * golden-section search over 1000-25000 K, the range alwan_spd_blackbody accepts. The
 * fitted scale is then not used. Beyond each end the curve is the Planckian at T scaled
 * to pass through the end sample, so the result is continuous, and inside src's range
 * it is alwan_spd_resample with ALWAN_RESAMPLE_LINEAR: this call is
 * ALWAN_EXTRAPOLATE_CONSTANT with the hold replaced by the continuation. temperature_out
 * may be NULL.
 *
 * Measured on ISO 7589 studio tungsten against the Academy's tabulation of the same
 * source, which runs to 780 nm where colour's stops at 690: the Planckian tail lands
 * within a few percent over 695-780 nm, the flat hold 16% low, the straight line 6%
 * high (alwan_dev suite 155 prints the figures). ALWAN_E_INVALID for fewer than two
 * samples in the fit window. */
alwan_status alwan_spd_extend_planckian_f64(alwan_spd_f64 *dst, alwan_spd_f64 const *src, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, size_t fit_count, alwan_f64 *temperature_out, alwan_ctx *ctx);
alwan_status alwan_spd_extend_planckian_f32(alwan_spd_f32 *dst, alwan_spd_f32 const *src, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, size_t fit_count, alwan_f32 *temperature_out, alwan_ctx *ctx);

/* Compute XYZ tristimulus values from SPD
 * xyz_out: output XYZ tristimulus values
 * spd: spectral power distribution (reflectance or emission)
 * illuminant: illuminant SPD (NULL = assume spd is already weighted by illuminant)
 * observer: any alwan_observer_type
 * method: integration method (trapezoid or Simpson; any other value is ALWAN_E_INVALID)
 * bandpass_nm: bandpass width for the Stearns & Stearns (1988) correction, or 0
 *              for none. The correction is derived for a triangular passband
 *              whose width EQUALS the sampling interval, so this asserts the
 *              SPD's own spacing rather than selecting a strength: a value that
 *              disagrees with (wavelength_max - wavelength_min) / (count - 1),
 *              or an SPD of fewer than three samples, is ALWAN_E_INVALID rather
 *              than a correction applied at the wrong width.
 * ctx: context
 * Returns ALWAN_OK on success, ALWAN_E_INVALID for a bandpass that is not the
 * SPD's interval or an unknown integration method */
alwan_status alwan_xyz_from_spd_f64(alwan_xyz_f64 *xyz_out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, alwan_f64 bandpass_nm, alwan_ctx *ctx);
alwan_status alwan_xyz_from_spd_f32(alwan_xyz_f32 *xyz_out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, alwan_f32 bandpass_nm, alwan_ctx *ctx);

/* ASTM E308 tristimulus values, the method industrial colorimetry reports. The zero
 * value of every field is E308's preferred practice and colour-science's default. */
typedef struct {
    int observer_range;  /* 0: E308's practice range, 360-780 nm; non-zero: the observer's 360-830 */
    int tables_at_5nm;   /* 0: 5 nm data summed on its own samples (the omission method);
                          * non-zero: through E2022 weighting tables at 5 nm */
    int tables_at_20nm;  /* 0: 20 nm data interpolated to 10 nm first; non-zero: E2022 tables at 20 nm */
} alwan_astm_e308_params;

/* XYZ of a spectrum by ASTM E308-15, as colour-science's sd_to_XYZ_ASTME308 computes it,
 * scaled so a perfect reflector has Y = 100. The spectrum is sampled at 1, 5, 10 or
 * 20 nm on whole nanometres, starting and ending on a multiple of its interval (of
 * 10 nm for 20 nm data that is interpolated). At 1 nm, and at 5 nm by the omission
 * method, it is summed on its own samples over the range, held at its end values where
 * it is shorter. At 10 nm, and at 5 or 20 nm through tables, E2022 weighting factors
 * are folded onto its own extent (the E308 adjustment). At 20 nm the default
 * interpolates to 10 nm first. The illuminant is read at every whole nanometre,
 * linearly between its samples, so a 1 nm illuminant agrees with colour-science
 * exactly; NULL is the equal-energy illuminant E. ALWAN_E_INVALID for any other
 * interval or alignment, a spectrum outside the range, or fewer than three 20 nm
 * samples to interpolate. alwan_xyz_from_spd, the trapezoid and Simpson integrals, is
 * unscaled; this is what a spectrophotometer's software reports. */
alwan_status alwan_xyz_from_spd_astm_e308_f64(alwan_xyz_f64 *xyz_out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, alwan_observer_type observer, alwan_astm_e308_params const *params, alwan_ctx *ctx);
alwan_status alwan_xyz_from_spd_astm_e308_f32(alwan_xyz_f32 *xyz_out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, alwan_observer_type observer, alwan_astm_e308_params const *params, alwan_ctx *ctx);

/* ASTM E2022 tristimulus weighting factors for a measurement interval of interval_nm (5,
 * 10 or 20) over 360-780 nm (observer_range 0) or 360-830 (non-zero): *node_count rows of
 * X, Y and Z weights at 360, 360 + interval, ..., scaled so the Y weights sum to 100.
 * weights_out NULL returns the row count alone. colour-science's
 * tristimulus_weighting_factors_ASTME2022. */
alwan_status alwan_astm_e2022_weights_f64(alwan_f64 *weights_out, size_t *node_count, alwan_spd_f64 const *illuminant, alwan_observer_type observer, int interval_nm, int observer_range, alwan_ctx *ctx);
alwan_status alwan_astm_e2022_weights_f32(alwan_f32 *weights_out, size_t *node_count, alwan_spd_f32 const *illuminant, alwan_observer_type observer, int interval_nm, int observer_range, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Camera RGB from an SPD
 * ---------------------------------------------------------------- */

/* A camera's RGB response to an SPD, for the camera at a registry index
 * (alwan_camera_find, alwan_camera_count). The camera's sensitivities are resampled onto
 * the SPD's grid, linearly and zero outside their own range, multiplied by the SPD and
 * the illuminant (NULL for none) and integrated by trapezoid or Simpson.
 *
 * ALWAN_E_INVALID for NULL rgb_out or spd, or another integration method; ALWAN_E_RANGE
 * for an index past the registry; ALWAN_E_NODATA when that camera's table was compiled
 * out.
 *
 * Before 3.0.0 the camera was an alwan_camera_sensitivity enum naming the two NPL
 * cameras, and the same integration was also reachable as alwan_xyz_from_spd_camera,
 * which returned camera RGB in an alwan_xyz. The enum, that function and
 * alwan_spd_camera_sensitivity are gone. The NPL cameras are registry indices 52 and 53,
 * and give the same numbers, to the bit, as they did. */
alwan_status alwan_camera_rgb_from_spd_f64(alwan_rgb_f64 *rgb_out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, size_t camera, alwan_integrate_method method, alwan_ctx *ctx);
alwan_status alwan_camera_rgb_from_spd_f32(alwan_rgb_f32 *rgb_out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, size_t camera, alwan_integrate_method method, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Spectral foundation: observers, generated sources, multispectral images
 * ---------------------------------------------------------------- */

/* An observer's three functions as SPDs on 360-830 nm at 1 nm, created by the call
 * and destroyed by the caller: x-bar, y-bar and z-bar, or the L, M and S cone
 * fundamentals for ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG. ALWAN_E_NODATA when the
 * observer's table was compiled out (data/alwan_data_tables_config.h). */
alwan_status alwan_spd_observer_f64(alwan_spd_f64 *x_bar, alwan_spd_f64 *y_bar, alwan_spd_f64 *z_bar, alwan_observer_type observer, alwan_ctx *ctx);
alwan_status alwan_spd_observer_f32(alwan_spd_f32 *x_bar, alwan_spd_f32 *y_bar, alwan_spd_f32 *z_bar, alwan_observer_type observer, alwan_ctx *ctx);

/* CIE daylight: the D-series SPD at a chromaticity, CIE 015:2004, S0 + M1 S1 + M2 S2.
 * round_m1_m2 non-zero rounds M1 and M2 to three decimals, as CIE 015 does for the
 * tabulated illuminants and colour-science does by default. The basis is carried on
 * 360-830 nm at 5 nm and interpolated linearly; a grid reaching outside 360-830 nm is
 * ALWAN_E_RANGE. For the chromaticity of a nominal temperature use
 * alwan_d_series_illuminant_xy, scaling the CIE illuminants' nominal CCT by
 * 1.4388 / 1.4380 (D65 is 6504 K). Creates the SPD. */
alwan_status alwan_spd_cie_daylight_f64(alwan_spd_f64 *out, alwan_vec2_f64 const *xy, int round_m1_m2, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx);
alwan_status alwan_spd_cie_daylight_f32(alwan_spd_f32 *out, alwan_vec2_f32 const *xy, int round_m1_m2, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx);

/* A Gaussian source: peak 1 at peak_nm, fwhm_nm wide at half maximum. Creates the SPD. */
alwan_status alwan_spd_gaussian_f64(alwan_spd_f64 *out, alwan_f64 peak_nm, alwan_f64 fwhm_nm, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx);
alwan_status alwan_spd_gaussian_f32(alwan_spd_f32 *out, alwan_f32 peak_nm, alwan_f32 fwhm_nm, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx);

/* LEDs by Ohno (2005): each emitter is (g + 2 g^5) / 3 with
 * g = exp(-((lambda - peak) / half_width)^2), and the SPD is their sum weighted by
 * peak_power (NULL for 1 each). led_count = 1 is a single LED. Creates the SPD. */
alwan_status alwan_spd_led_ohno2005_f64(alwan_spd_f64 *out, alwan_f64 const *peak_nm, alwan_f64 const *half_width_nm, alwan_f64 const *peak_power, size_t led_count, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t count, alwan_ctx *ctx);
alwan_status alwan_spd_led_ohno2005_f32(alwan_spd_f32 *out, alwan_f32 const *peak_nm, alwan_f32 const *half_width_nm, alwan_f32 const *peak_power, size_t led_count, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t count, alwan_ctx *ctx);

/* Integration weights for multispectral images.
 *
 * weights_out receives 3 x band_count values, row-major, such that for a spectrum of
 * band_count samples uniformly on [wavelength_min, wavelength_max]
 *
 *     channel_c = sum_i weights_out[c * band_count + i] * sample_i
 *
 * is alwan_xyz_from_spd's integral of that spectrum against the three responses under
 * the illuminant (NULL for none): same interpolation of the responses (linear, edges
 * held) and the illuminant (linear, zero outside), same trapezoid or Simpson rule. The
 * responses are any three SPDs, an observer or a camera. normalize non-zero scales every
 * row so a perfect reflector, all ones, gives channel 1 exactly 1: Y = 1 for an observer.
 * Compute the weights once per illuminant and apply them to every pixel with
 * alwan_spectral_to_tristimulus_{T}_map_interleave. */
alwan_status alwan_spectral_weights_f64(alwan_f64 *weights_out, size_t band_count, alwan_f64 wavelength_min, alwan_f64 wavelength_max, alwan_spd_f64 const *illuminant, alwan_spd_f64 const *response_0, alwan_spd_f64 const *response_1, alwan_spd_f64 const *response_2, alwan_integrate_method method, int normalize);
alwan_status alwan_spectral_weights_f32(alwan_f32 *weights_out, size_t band_count, alwan_f32 wavelength_min, alwan_f32 wavelength_max, alwan_spd_f32 const *illuminant, alwan_spd_f32 const *response_0, alwan_spd_f32 const *response_1, alwan_spd_f32 const *response_2, alwan_integrate_method method, int normalize);

/* The same weights for a standard observer. */
alwan_status alwan_spectral_weights_observer_f64(alwan_f64 *weights_out, size_t band_count, alwan_f64 wavelength_min, alwan_f64 wavelength_max, alwan_spd_f64 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, int normalize, alwan_ctx *ctx);
alwan_status alwan_spectral_weights_observer_f32(alwan_f32 *weights_out, size_t band_count, alwan_f32 wavelength_min, alwan_f32 wavelength_max, alwan_spd_f32 const *illuminant, alwan_observer_type observer, alwan_integrate_method method, int normalize, alwan_ctx *ctx);

/* A multispectral buffer to three channels: in holds band_count samples per pixel
 * (in_stride bytes between pixels), out three values per pixel. Sums run left to right,
 * so the result is the same in every build. */
alwan_status alwan_spectral_to_tristimulus_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, size_t band_count, alwan_f64 const *weights);
alwan_status alwan_spectral_to_tristimulus_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, size_t band_count, alwan_f32 const *weights);

/* The same sum over a typed cube: void pointers and an alwan_pixel_format on each side
 * (out_fmt, in_fmt, as the other _map_interleave_ex), so u8, u16, f16, f32 or f64
 * spectra go straight in and the tristimulus comes out in the format the pipeline
 * keeps. Weights are f64 and the sum always runs in f64, then stores to out_fmt: an
 * f32/f32 call gets the f64 sum narrowed, not the f32 bulk form's sum; f64/f64 is the
 * f64 bulk form exactly. Tiles hold fewer pixels the more bands there are, so no buffer
 * leaves the stack. A build without f64 narrows the weights once and sums in f32, and
 * refuses more than 4096 bands with ALWAN_E_RANGE, the scratch the weights go into.
 * The upsamplers' _ex forms are declared beside them below. */
alwan_status alwan_spectral_to_tristimulus_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, size_t band_count, alwan_f64 const *weights);

/* Spectral OpenEXR files: the Fichet, Pacanowski and Wilkie layout.
 * (An OpenEXR Layout for Spectral Images, Journal of Computer Graphics Techniques
 * 10(3), 2021.) alwan reads and writes no files; this is what a caller needs to know to
 * turn such a file into the in buffer above, and back.
 *
 * Layers and channels. Emissive data is layer S0, plus S1 to S3 when polarised; reflective
 * data is layer T. Each channel is one wavelength or frequency: layer, a dot, the value
 * with a decimal COMMA because OpenEXR reserves the dot, an optional E exponent, an
 * optional SI multiplier and the unit m or Hz. So S0.560,5nm is emissive radiance at
 * 560.5 nm. The header must carry spectralLayoutVersion "1.0", and an emissive file also
 * emissiveUnits, one of "W", "W.m^-2", "W.sr^-1" or "W.m^-2.sr^-1".
 *
 * Building in. band_count is the number of S0 (or T) channels, and each pixel's samples go
 * in ascending wavelength. Order them by the parsed value, not by channel order in the
 * file: OpenEXR hands channels back sorted by name whatever order they were written in,
 * which puts S0.1000nm before S0.380nm (checked against OpenEXR 3.4), and a frequency
 * layout runs the other way. alwan_spectral_weights takes the samples as uniform
 * on [wavelength_min, wavelength_max]; a file need not be, so check the spacing and
 * resample (alwan_spd_resample) when it is not.
 *
 * Integrating. The paper's own preview is CIE 1931 2 degree to linear sRGB. For S0 that is
 * weights with no illuminant and normalize 0. For T it is weights under D65 with normalize
 * non-zero, which is its division by Y of D65. One difference to expect at the edges: with
 * no filter attribute the paper treats each band as a gate reaching halfway to its
 * neighbours, and the first and last bands as full width, where alwan integrates point
 * samples by trapezoid or Simpson, both of which weight the end samples less than the
 * interior ones.
 *
 * Not covered: the S1 to S3 Stokes layers and the bi-spectral T.<in>.<out> re-radiation
 * channels have no counterpart here, and only S0 or T reduce to a tristimulus. */

/* ----------------------------------------------------------------
 * Spectral camera characterisation
 *
 * The camera registry has 54 slots. 0 to 51 are the rawtoaces-data pack (Academy
 * Software Foundation, Apache-2.0, data/camera_sensitivities/rawtoaces/LICENSE.txt), on
 * 380-780 nm at 5 nm, with the pack's 190-patch IDT training set and ISO 7589 studio
 * tungsten beside it. 52 and 53 are the two NPL cameras, "Nikon" "5100 (NPL)" and
 * "Sigma" "SDMerill (NPL)" under colour-science's names, on 360-830 nm at 1 nm. A
 * camera's index is stable: slots are fixed whatever is compiled in, and new cameras are
 * appended. A camera whose table was compiled out answers ALWAN_E_NODATA from its slot.
 *
 * The f32 entry points widen to f64, compute and narrow, like the CCM fits.
 * ---------------------------------------------------------------- */

/* The registry's slot count: 54, or 0 in a build carrying no camera table at all. */
size_t alwan_camera_count(void);

/* A camera's index by make and model, ASCII case-insensitive. Alternative model and
 * maker names from rawtoaces' alias table are accepted ("Canon" "EOS 400D" is the
 * Digital Rebel XTi). ALWAN_E_NODATA when no camera matches. */
alwan_status alwan_camera_find(size_t *index_out, char const *make, char const *model);

/* The make and model of a camera; the strings are static. ALWAN_E_RANGE past the end. */
alwan_status alwan_camera_info(char const **make_out, char const **model_out, size_t index);

/* A camera's R, G, B spectral sensitivities as three SPDs on the camera's own grid,
 * 380-780 nm at 5 nm for the pack and 360-830 nm at 1 nm for the NPL cameras, relative
 * units. Creates the SPDs. */
alwan_status alwan_camera_sensitivities_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b, size_t index, alwan_ctx *ctx);
alwan_status alwan_camera_sensitivities_f32(alwan_spd_f32 *spd_r, alwan_spd_f32 *spd_g, alwan_spd_f32 *spd_b, size_t index, alwan_ctx *ctx);

/* The IDT training reflectances: how many, and one as an SPD on 380-780 nm at 5 nm. */
size_t alwan_idt_training_count(void);
alwan_status alwan_spd_idt_training_f64(alwan_spd_f64 *out, size_t patch_index, alwan_ctx *ctx);
alwan_status alwan_spd_idt_training_f32(alwan_spd_f32 *out, size_t patch_index, alwan_ctx *ctx);

/* ISO 7589 studio tungsten, the Academy's variant, on 380-780 nm at 5 nm. */
alwan_status alwan_spd_iso7589_tungsten_f64(alwan_spd_f64 *out, alwan_ctx *ctx);
alwan_status alwan_spd_iso7589_tungsten_f32(alwan_spd_f32 *out, alwan_ctx *ctx);

/* White balance multipliers of a camera under an illuminant:
 * 1 / sum(sensitivity * illuminant) per channel, scaled so the smallest is 1. The
 * three sensitivities must share one grid; the illuminant is read on it. */
alwan_status alwan_idt_white_balance_f64(alwan_rgb_f64 *white_balance_out, alwan_spd_f64 const *sens_r, alwan_spd_f64 const *sens_g, alwan_spd_f64 const *sens_b, alwan_spd_f64 const *illuminant);
alwan_status alwan_idt_white_balance_f32(alwan_rgb_f32 *white_balance_out, alwan_spd_f32 const *sens_r, alwan_spd_f32 const *sens_g, alwan_spd_f32 const *sens_b, alwan_spd_f32 const *illuminant);

/* Which of a set of candidate illuminants a camera was shot under, given the white
 * balance it needed. For each candidate the multipliers it would call for are computed
 * with alwan_idt_white_balance above, and the one closest to the measured white_balance
 * wins, by
 *
 *     sum over channels of (candidate_multiplier / measured_multiplier - 1)^2
 *
 * which is a relative error and has no separate normalising step. index_out receives the
 * winner's position in candidates; sse_out, when not NULL, receives its error, which is 0
 * when the measurement came from a candidate exactly. Equal errors keep the earlier
 * candidate.
 *
 * The candidates are the caller's, read on the sensitivities' own grid, so this does not
 * decide what a sensible bank is. colour-science's rawtoaces v1 bank is 50 sources on
 * 380-780 nm at 5 nm: daylights every 500 K from 4000 K to 25000 K, blackbodies from
 * 1000 K to 3500 K, and ISO 7589 studio tungsten, all of which alwan can build from
 * alwan_spd_cie_daylight, alwan_spd_blackbody and alwan_spd_iso7589_tungsten.
 *
 * ALWAN_E_INVALID for no candidates or a measured multiplier that is not positive, since
 * the error divides by it. A candidate that integrates to nothing in some channel is
 * reported as ALWAN_E_RANGE rather than passed over, so a degenerate entry in the array
 * is visible. Matches colour.characterisation.best_illuminant. */
alwan_status alwan_best_illuminant_f64(size_t *index_out, alwan_f64 *sse_out, alwan_rgb_f64 const *white_balance, alwan_spd_f64 const *sens_r, alwan_spd_f64 const *sens_g, alwan_spd_f64 const *sens_b, alwan_spd_f64 const *candidates, size_t candidate_count);
alwan_status alwan_best_illuminant_f32(size_t *index_out, alwan_f32 *sse_out, alwan_rgb_f32 const *white_balance, alwan_spd_f32 const *sens_r, alwan_spd_f32 const *sens_g, alwan_spd_f32 const *sens_b, alwan_spd_f32 const *candidates, size_t candidate_count);

/* What the IDT fit minimises. LAB is rawtoaces v1: the norm of the CIE Lab
 * differences, against the ACES white, over every training patch. JZAZBZ is the sum of
 * per-patch Jzazbz distances (colour-science's optimisation_factory_Jzazbz). */
typedef enum {
    ALWAN_IDT_OBJECTIVE_LAB    = 0,
    ALWAN_IDT_OBJECTIVE_JZAZBZ = 1
} alwan_idt_objective;

/* IDT options. Zero-initialised is the default: rawtoaces v1's Lab objective, the
 * training XYZ adapted to the ACES white with CAT02, and a 500-iteration budget. */
typedef struct {
    alwan_idt_objective objective;
    int skip_chromatic_adaptation;   /* non-zero: leave the training XYZ under the illuminant's white */
    int max_iterations;              /* 0: 500 */
} alwan_idt_params;

void alwan_idt_params_init(alwan_idt_params *params);

/* The ACES input transform of a camera from its spectral sensitivities, ACES P-2013-001
 * Method A as rawtoaces v1 computes it.
 *
 * The training reflectances (NULL for the built-in 190 patches) are rendered under the
 * illuminant, normalised by the camera's peak channel, into white-balanced camera RGB
 * and, through the CIE 1931 observer, into XYZ adapted to the ACES white. idt_out is the
 * 3x3 from white-balanced camera RGB to ACES2065-1 whose rows sum to 1, so camera white
 * lands on ACES white, fitted by BFGS on the chosen objective; white_balance_out is the
 * multipliers. Apply both with alwan_camera_rgb_to_aces2065_1_{T}_map_interleave.
 *
 * The solve runs on the sensitivities' own grid, which must be shared by all three; the
 * illuminant, training spectra and observer are read on it. rawtoaces-data and
 * colour-science both use 380-780 nm at 5 nm. params may be NULL for the defaults.
 * Agrees with colour.matrix_idt to the precision its optimiser reaches, about 1e-8. */
alwan_status alwan_idt_matrix_f64(alwan_mat3x3_f64 *idt_out, alwan_rgb_f64 *white_balance_out, alwan_spd_f64 const *sens_r, alwan_spd_f64 const *sens_g, alwan_spd_f64 const *sens_b, alwan_spd_f64 const *illuminant, alwan_spd_f64 const *training, size_t training_count, alwan_idt_params const *params, alwan_ctx *ctx);
alwan_status alwan_idt_matrix_f32(alwan_mat3x3_f32 *idt_out, alwan_rgb_f32 *white_balance_out, alwan_spd_f32 const *sens_r, alwan_spd_f32 const *sens_g, alwan_spd_f32 const *sens_b, alwan_spd_f32 const *illuminant, alwan_spd_f32 const *training, size_t training_count, alwan_idt_params const *params, alwan_ctx *ctx);

/* A camera's spectral sensitivities recovered from a photographed chart, Jiang, Liu, Gu
 * and Suesstrunk 2013: for a camera that was never measured. camera_rgb holds the
 * linear, black-subtracted response of each of patch_count patches (rgb_stride bytes
 * apart), reflectances their spectra, illuminant the light they were shot under.
 * Each channel is fitted as a combination of basis_components principal components
 * (0 for all 6) of the 52 rawtoaces-data cameras; patch_count must be at least that.
 * The result is on 380-780 nm at 5 nm, scaled so the largest value of the three is 1,
 * and can go straight to alwan_idt_matrix. A basis only spans what its cameras share:
 * expect the shape, not the fine structure, of an unusual sensor. Matches
 * colour.recovery.RGB_to_msds_camera_sensitivities_Jiang2013 given the same basis.
 * Creates the three SPDs. */
alwan_status alwan_camera_sensitivities_from_chart_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b, alwan_f64 const *camera_rgb, size_t rgb_stride, alwan_spd_f64 const *reflectances, size_t patch_count, alwan_spd_f64 const *illuminant, size_t basis_components, alwan_ctx *ctx);
alwan_status alwan_camera_sensitivities_from_chart_f32(alwan_spd_f32 *spd_r, alwan_spd_f32 *spd_g, alwan_spd_f32 *spd_b, alwan_f32 const *camera_rgb, size_t rgb_stride, alwan_spd_f32 const *reflectances, size_t patch_count, alwan_spd_f32 const *illuminant, size_t basis_components, alwan_ctx *ctx);

/* The same recovery over a basis the caller supplies, for a sensor family the
 * embedded basis does not span, or to measure the embedded one: basis holds
 * 3 x basis_components x ALWAN_CAMERA_BASIS_BANDS values, channel-major (R, G,
 * B), then component, then wavelength on 380-780 nm at 5 nm, which is the
 * layout of the shipped rawtoaces basis (data/camera_sensitivities/rawtoaces/
 * basis_pca6.csv) and of colour-science's PCA_Jiang2013 output transposed.
 * Every component is used; patch_count must be at least basis_components.
 * Does not need the embedded table, so it is available under
 * ALWAN_TABLES_CAMERAS=0. ALWAN_E_INVALID for a NULL basis or zero components.
 * Suite 121 holds it to colour-science over bases built without the camera
 * being recovered (leave-one-out), which is the measurement the embedded
 * basis's figure comes from. */
#define ALWAN_CAMERA_BASIS_BANDS 81
alwan_status alwan_camera_sensitivities_from_chart_basis_f64(alwan_spd_f64 *spd_r, alwan_spd_f64 *spd_g, alwan_spd_f64 *spd_b, alwan_f64 const *camera_rgb, size_t rgb_stride, alwan_spd_f64 const *reflectances, size_t patch_count, alwan_spd_f64 const *illuminant, alwan_f64 const *basis, size_t basis_components, alwan_ctx *ctx);
alwan_status alwan_camera_sensitivities_from_chart_basis_f32(alwan_spd_f32 *spd_r, alwan_spd_f32 *spd_g, alwan_spd_f32 *spd_b, alwan_f32 const *camera_rgb, size_t rgb_stride, alwan_spd_f32 const *reflectances, size_t patch_count, alwan_spd_f32 const *illuminant, alwan_f32 const *basis, size_t basis_components, alwan_ctx *ctx);

/* Camera RGB to ACES2065-1 with an IDT: white balance normalised so its smallest
 * multiplier is 1, clip at 1 when clip is non-zero (keeps saturated sensor values
 * achromatic), the IDT matrix, then the exposure factor (1 for none; the value that puts
 * an 18 % grey at 0.18). colour.camera_RGB_to_ACES2065_1. */
alwan_status alwan_camera_rgb_to_aces2065_1_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_mat3x3_f64 const *idt, alwan_rgb_f64 const *white_balance, alwan_f64 exposure, int clip);
alwan_status alwan_camera_rgb_to_aces2065_1_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_mat3x3_f32 const *idt, alwan_rgb_f32 const *white_balance, alwan_f32 exposure, int clip);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_camera_rgb_to_aces2065_1_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_mat3x3_f32 const *idt, alwan_rgb_f32 const *white_balance, alwan_f32 exposure, int clip);
alwan_status alwan_camera_rgb_to_aces2065_1_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_mat3x3_f64 const *idt, alwan_rgb_f64 const *white_balance, alwan_f64 exposure, int clip);

/* Spectral radiance (or a reflectance, with the illuminant it is lit by) to ACES2065-1
 * relative exposure values through the Academy's Reference Input Capture Device, with
 * the ACES 0.5 % flare. illuminant NULL is D65. Unless skip_chromatic_adaptation is
 * set, the result is adapted from the illuminant's white to the ACES white with CAT02.
 * The SPDs are read on the RICD's 360-830 nm 1 nm grid, linearly, edges held.
 * colour.sd_to_aces_relative_exposure_values, with one difference recorded in
 * docs/alwan_decisions.md: alwan takes the illuminant's white over 360-830 nm where
 * colour-science trims to 360-780 nm. */
alwan_status alwan_spd_to_aces2065_1_f64(alwan_rgb_f64 *out, alwan_spd_f64 const *spd, alwan_spd_f64 const *illuminant, int skip_chromatic_adaptation, alwan_ctx *ctx);
alwan_status alwan_spd_to_aces2065_1_f32(alwan_rgb_f32 *out, alwan_spd_f32 const *spd, alwan_spd_f32 const *illuminant, int skip_chromatic_adaptation, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * The DNG colour model
 *
 * What a raw converter computes from a DNG camera profile's colour tags (DNG 1.6,
 * chapter 6). alwan_dng_profile_{T} holds the tags. They are interpolated between
 * the two calibration illuminants linearly in 1/CCT, the white's CCT read by
 * Robertson 1968; outside the two CCTs the nearer tag is used. This follows the
 * white paper where it and the DNG SDK differ, as colour-hdri does, and matches
 * colour_hdri.models.dng. ALWAN_E_NODATA when the Robertson locus table is
 * compiled out. The f32 entry points compute in f64.
 * ---------------------------------------------------------------- */

/* One tag at a CCT: m1 at or below cct_1, m2 at or above cct_2, linear in 1/CCT
 * between. The two illuminants may come in either order. */
alwan_status alwan_dng_interpolate_matrix_f64(alwan_mat3x3_f64 *matrix_out, alwan_f64 cct, alwan_f64 cct_1, alwan_f64 cct_2, alwan_mat3x3_f64 const *m1, alwan_mat3x3_f64 const *m2);
alwan_status alwan_dng_interpolate_matrix_f32(alwan_mat3x3_f32 *matrix_out, alwan_f32 cct, alwan_f32 cct_1, alwan_f32 cct_2, alwan_mat3x3_f32 const *m1, alwan_mat3x3_f32 const *m2);

/* XYZ to camera space at a white: AnalogBalance x CameraCalibration x ColorMatrix,
 * each interpolated at the white's CCT. */
alwan_status alwan_dng_xyz_to_camera_matrix_f64(alwan_mat3x3_f64 *matrix_out, alwan_dng_profile_f64 const *profile, alwan_vec2_f64 const *white_xy);
alwan_status alwan_dng_xyz_to_camera_matrix_f32(alwan_mat3x3_f32 *matrix_out, alwan_dng_profile_f32 const *profile, alwan_vec2_f32 const *white_xy);

/* The camera neutral of a white (what AsShotNeutral stores), with G = 1. */
alwan_status alwan_dng_xy_to_camera_neutral_f64(alwan_rgb_f64 *neutral_out, alwan_dng_profile_f64 const *profile, alwan_vec2_f64 const *white_xy);
alwan_status alwan_dng_xy_to_camera_neutral_f32(alwan_rgb_f32 *neutral_out, alwan_dng_profile_f32 const *profile, alwan_vec2_f32 const *white_xy);

/* The white of a camera neutral: the fixed point of the function above, iterated
 * from (1/3, 1/3). ALWAN_E_RANGE when it has not converged after 200 steps. */
alwan_status alwan_dng_camera_neutral_to_xy_f64(alwan_vec2_f64 *white_xy_out, alwan_dng_profile_f64 const *profile, alwan_rgb_f64 const *camera_neutral);
alwan_status alwan_dng_camera_neutral_to_xy_f32(alwan_vec2_f32 *white_xy_out, alwan_dng_profile_f32 const *profile, alwan_rgb_f32 const *camera_neutral);

/* Camera space to XYZ under the DNG connection white, D50 at (0.3457, 0.3585), for
 * a white balance. With ForwardMatrix tags: ForwardMatrix x the diagonal that takes
 * the white's reference neutral to 1 x (AnalogBalance x CameraCalibration)^-1, and
 * cat is not used. Without: the inverse of alwan_dng_xyz_to_camera_matrix, then a
 * chromatic adaptation from the white to D50 with cat; the DNG SDK uses
 * ALWAN_CAT_BRADFORD. */
alwan_status alwan_dng_camera_to_xyz_matrix_f64(alwan_mat3x3_f64 *matrix_out, alwan_dng_profile_f64 const *profile, alwan_vec2_f64 const *white_xy, alwan_cat_method cat);
alwan_status alwan_dng_camera_to_xyz_matrix_f32(alwan_mat3x3_f32 *matrix_out, alwan_dng_profile_f32 const *profile, alwan_vec2_f32 const *white_xy, alwan_cat_method cat);

/* dcraw's highlight blend, as colour-hdri carries it, for white-balanced camera RGB.
 * Channels at or above min(multipliers) x threshold are clipped there in an
 * opponent space; each pixel keeps its own lightness and takes the chroma magnitude
 * of its clipped version, so clipped highlights stay neutral instead of turning
 * magenta. threshold 0 reads as 0.99. */
alwan_status alwan_highlights_recovery_blend_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_rgb_f64 const *multipliers, alwan_f64 threshold);
alwan_status alwan_highlights_recovery_blend_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_rgb_f32 const *multipliers, alwan_f32 threshold);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_highlights_recovery_blend_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_rgb_f32 const *multipliers, alwan_f32 threshold);
alwan_status alwan_highlights_recovery_blend_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_rgb_f64 const *multipliers, alwan_f64 threshold);

/* ----------------------------------------------------------------
 * Bayer demosaicing
 *
 * A Bayer colour filter array records one channel per photosite. Demosaicing
 * rebuilds the other two. Input is a linear, black-subtracted CFA plane, one value
 * per pixel; output is camera RGB, three values per pixel. Row strides are in bytes.
 * Decoding raw files is out of scope (LibRaw, the DNG SDK). Matches colour-demosaicing
 * (colour_demosaicing.bayer), borders included. The images must be at least 2 x 2.
 * ---------------------------------------------------------------- */

/* The colour of the top-left 2 x 2 block, read row by row. */
typedef enum {
    ALWAN_CFA_RGGB = 0,
    ALWAN_CFA_BGGR = 1,
    ALWAN_CFA_GRBG = 2,
    ALWAN_CFA_GBRG = 3
} alwan_cfa_pattern;

typedef enum {
    ALWAN_DEMOSAIC_BILINEAR            = 0, /* each channel averaged over its own sites */
    ALWAN_DEMOSAIC_MALVAR2004          = 1, /* Malvar, He and Cutler: gradient-corrected linear filters */
    ALWAN_DEMOSAIC_MENON2007           = 2, /* Menon, Andriani and Calvagno: directional filtering with an
                                             * a posteriori decision (DDFAPD), with its refining step */
    ALWAN_DEMOSAIC_MENON2007_NO_REFINE = 3  /* the same, without the refining step */
} alwan_demosaic_method;

/* The mosaic an RGB image would record: each site keeps its layout's channel. */
alwan_status alwan_cfa_bayer_mosaic_f64(alwan_f64 *cfa_out, size_t cfa_row_stride, alwan_f64 const *rgb, size_t rgb_row_stride, size_t width, size_t height, alwan_cfa_pattern pattern);
alwan_status alwan_cfa_bayer_mosaic_f32(alwan_f32 *cfa_out, size_t cfa_row_stride, alwan_f32 const *rgb, size_t rgb_row_stride, size_t width, size_t height, alwan_cfa_pattern pattern);

/* Demosaic a Bayer plane into RGB. */
alwan_status alwan_cfa_bayer_demosaic_f64(alwan_f64 *rgb_out, size_t rgb_row_stride, alwan_f64 const *cfa, size_t cfa_row_stride, size_t width, size_t height, alwan_cfa_pattern pattern, alwan_demosaic_method method);
alwan_status alwan_cfa_bayer_demosaic_f32(alwan_f32 *rgb_out, size_t rgb_row_stride, alwan_f32 const *cfa, size_t cfa_row_stride, size_t width, size_t height, alwan_cfa_pattern pattern, alwan_demosaic_method method);

/* ----------------------------------------------------------------
 * Spectral Shape Descriptors
 * ---------------------------------------------------------------- */

/* Analyze SPD shape characteristics
 * Computes peak wavelength, FWHM, centroid, and bandwidth
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if SPD is invalid */
alwan_status alwan_spd_analyze_shape_f64(alwan_spd_shape_f64 *shape_out, alwan_spd_f64 const *spd);
alwan_status alwan_spd_analyze_shape_f32(alwan_spd_shape_f32 *shape_out, alwan_spd_f32 const *spd);

/* ----------------------------------------------------------------
 * Gamut Analysis & Mapping
 * ---------------------------------------------------------------- */

/* Check if xy chromaticity is within Pointer's Gamut
 * Pointer's Gamut represents the boundary of real surface colors under illuminant C
 * xy: CIE 1931 xy chromaticity coordinates
 * Returns 1 if inside Pointer's Gamut, 0 otherwise */
int alwan_is_within_pointer_gamut_f32(alwan_vec2_f32 const *xy);
int alwan_is_within_pointer_gamut_f64(alwan_vec2_f64 const *xy);

/* The Lab extent of an RGB space: the smallest and largest L*, a* and b* the
 * space can reach, as six values in the order L min, L max, a min, a max,
 * b min, b max.
 *
 * Taken over the eight corners of the unit RGB cube. L* is monotonic in Y and Y
 * is linear in RGB, so its extremes are at corners by construction; a* and b*
 * are differences of non-linear functions and theirs need not be, which makes
 * this a bound over the corners rather than a proof about the whole cube.
 * Measured against 400,000 points drawn uniformly from the sRGB cube, nothing
 * exceeded it. colour-science's RGB_colourspace_limits takes the corners too.
 *
 * The space's own white is the Lab reference white, so the limits describe the
 * space on its own terms and are not comparable across two spaces with
 * different white points without adapting first. */
alwan_status alwan_rgb_space_limits_f64(alwan_f64 *limits_out, alwan_rgb_space_desc_f64 const *space);
alwan_status alwan_rgb_space_limits_f32(alwan_f32 *limits_out, alwan_rgb_space_desc_f32 const *space);

/* ----------------------------------------------------------------
 * Ellipse fitting
 *
 * Halir and Flusser (1998), "Numerically Stable Direct Least Squares Fitting
 * of Ellipses". Fitzgibbon, Pilu and Fisher's direct method constrains the fit
 * to an ellipse rather than any conic, so the answer cannot come back a
 * hyperbola however the points are spread; Halir and Flusser's contribution is
 * to split the scatter matrix so the eigenproblem is 3x3 and well conditioned
 * instead of 6x6 and nearly singular.
 *
 * Two ways of writing one ellipse. The GENERAL form is six coefficients,
 *
 *     a x^2 + b x y + c y^2 + d x + e y + f = 0
 *
 * which is what the fit produces. The CANONICAL form is five numbers: centre
 * x, centre y, the two semi-axes, and the rotation in degrees. The two axes
 * come back in the order colour-science returns them, along and across the
 * rotation, and are NOT sorted into major and minor.
 *
 * The six coefficients have no natural scale, since multiplying all of them by
 * a non-zero constant is the same ellipse. They come back with unit 2-norm and
 * a > 0, which is always well defined here: an ellipse has 4ac > b^2, so a and
 * c share a sign and neither is zero. The canonical form is unaffected.
 *
 * Fewer than five points is ALWAN_E_RANGE, since five determine a conic.
 * Points that no ellipse fits, such as points on a line, are ALWAN_E_RANGE as
 * well rather than a degenerate answer.
 * ---------------------------------------------------------------- */

alwan_status alwan_ellipse_fit_halir1998_f64(alwan_f64 *coefficients_out, alwan_vec2_f64 const *points, size_t count);
alwan_status alwan_ellipse_fit_halir1998_f32(alwan_f32 *coefficients_out, alwan_vec2_f32 const *points, size_t count);

/* General to canonical and back. canonical_out takes 5 values, coefficients 6.
 * A general form that is not an ellipse is ALWAN_E_RANGE. */
alwan_status alwan_ellipse_canonical_f64(alwan_f64 *canonical_out, alwan_f64 const *coefficients);
alwan_status alwan_ellipse_canonical_f32(alwan_f32 *canonical_out, alwan_f32 const *coefficients);
alwan_status alwan_ellipse_general_f64(alwan_f64 *coefficients_out, alwan_f64 const *canonical);
alwan_status alwan_ellipse_general_f32(alwan_f32 *coefficients_out, alwan_f32 const *canonical);

/* ----------------------------------------------------------------
 * Optimal colour solid (Rosch-MacAdam)
 *
 * The set of every tristimulus a SURFACE can have, under one light and one
 * observer. A reflectance lies in [0, 1] at each wavelength, so the set is
 *
 *     { sum_i R_i w_i : 0 <= R_i <= 1 },   w_i = k S_i xbar_i
 *
 * with k such that a perfect reflector reads Y = 1. That is a zonotope, which
 * is what makes membership exact here rather than a mesh test: a zonotope's
 * support in a direction is sum_i max(0, n . w_i), and its facet normals are
 * the cross products of pairs of generators, so alwan_colour_solid_contains
 * needs no convex hull and no triangulation.
 *
 * The vertices are the classical optimal colour stimuli, the reflectances with
 * at most two transitions, and there are bins * (bins - 1) + 2 of them. They
 * come back in the order colour-science's generate_pulse_waves produces, so
 * the two can be compared term by term; suite 164 does.
 *
 * bins is the number of wavelength bands, 3 to 256. Building the solid is
 * cubic in it and the object then holds bins * (bins - 1) / 2 facets, so a
 * caller picks bins for the resolution it needs rather than the largest.
 * A band is wholly on or wholly off in this construction, so the quadrature is
 * rectangles, not the trapezoid rule alwan_xyz_from_spd would apply.
 * ---------------------------------------------------------------- */

typedef struct alwan_colour_solid_s alwan_colour_solid;

alwan_status alwan_colour_solid_create_f64(alwan_colour_solid **out, alwan_f64 wavelength_min, alwan_f64 wavelength_max, size_t bins, alwan_observer_type observer, alwan_illuminant illuminant, alwan_ctx *ctx);
alwan_status alwan_colour_solid_create_f32(alwan_colour_solid **out, alwan_f32 wavelength_min, alwan_f32 wavelength_max, size_t bins, alwan_observer_type observer, alwan_illuminant illuminant, alwan_ctx *ctx);
void alwan_colour_solid_destroy(alwan_colour_solid *solid, alwan_ctx *ctx);

/* What was built: the vertex count is bins * (bins - 1) + 2, the facet count is
 * how many of the bins * (bins - 1) / 2 generator pairs spanned a plane. */
size_t alwan_colour_solid_num_vertices(alwan_colour_solid const *solid);
size_t alwan_colour_solid_num_facets(alwan_colour_solid const *solid);
size_t alwan_colour_solid_bins(alwan_colour_solid const *solid);

/* The optimal colour stimuli. ALWAN_E_RANGE when capacity is short of
 * alwan_colour_solid_num_vertices. */
alwan_status alwan_colour_solid_vertices_f64(alwan_xyz_f64 *out, size_t capacity, alwan_colour_solid const *solid);
alwan_status alwan_colour_solid_vertices_f32(alwan_xyz_f32 *out, size_t capacity, alwan_colour_solid const *solid);

/* The solid's white point: every band fully reflected, so Y is 1 by
 * construction and X and Z are the illuminant's. */
alwan_status alwan_colour_solid_white_f64(alwan_xyz_f64 *out, alwan_colour_solid const *solid);
alwan_status alwan_colour_solid_white_f32(alwan_xyz_f32 *out, alwan_colour_solid const *solid);

/* Whether a tristimulus is a colour a surface could have. Exact for the solid
 * on this band grid: *inside is 1 when the point violates no facet, and the
 * facets are all of them. tolerance is in the same units as XYZ and is applied
 * outward, so 0 tests the solid itself and a small positive value admits a
 * point that rounding put just outside. A NaN component is ALWAN_E_INVALID. */
alwan_status alwan_colour_solid_contains_f64(int *inside, alwan_colour_solid const *solid, alwan_xyz_f64 const *xyz, alwan_f64 tolerance);
alwan_status alwan_colour_solid_contains_f32(int *inside, alwan_colour_solid const *solid, alwan_xyz_f32 const *xyz, alwan_f32 tolerance);

/* Pointer's Gamut as a VOLUME rather than the chromaticity outline above.
 *
 * Pointer (1980) measured the greatest chroma a real surface colour reaches at
 * each CIELAB lightness and hue, on a regular grid: sixteen lightnesses from 15
 * to 90 in steps of 5, and thirty-six hues from 0 to 350 in steps of 10.
 * alwan_pointer_gamut_max_chroma_{T} interpolates that table bilinearly,
 * wrapping in hue because hue is a circle and refusing outside 15 to 90 in
 * lightness, where the measurement says nothing: that is ALWAN_E_RANGE rather
 * than an extrapolation.
 *
 * NOT A CONVEX HULL, which is what colour-science's is_within_pointer_gamut
 * tests against, and the difference is not small. The gamut is not convex, so
 * hulling it admits colours the measurement says are not there: push each of
 * the 575 non-zero grid directions 25 per cent past its tabulated maximum
 * chroma and colour still calls 80 of them inside; push it 2 per cent past and
 * it calls 329 of 575 inside, more than half. Reading the table for what it
 * says answers those correctly by construction.
 *
 * alwan_pointer_gamut_white_{T} is the white the table is referenced to. It is
 * NOT ALWAN_ILLUMINANT_C, which rounds to (0.31006, 0.31616): Pointer's data is
 * against illuminant C computed to more places, (0.31005673430392799,
 * 0.31614570478920401), and
 * the difference moves a Lab by about 0.01, enough to change an answer at the
 * boundary. The _xyz_{T} predicate uses it, so a caller passing XYZ does not
 * have to know. */
alwan_status alwan_pointer_gamut_max_chroma_f64(alwan_f64 *chroma_out, alwan_f64 lightness, alwan_f64 hue_deg);
alwan_status alwan_pointer_gamut_max_chroma_f32(alwan_f32 *chroma_out, alwan_f32 lightness, alwan_f32 hue_deg);
alwan_status alwan_pointer_gamut_white_f64(alwan_xyz_f64 *out);
alwan_status alwan_pointer_gamut_white_f32(alwan_xyz_f32 *out);
int alwan_is_within_pointer_gamut_lab_f64(alwan_lab_f64 const *lab);
int alwan_is_within_pointer_gamut_lab_f32(alwan_lab_f32 const *lab);
int alwan_is_within_pointer_gamut_xyz_f64(alwan_xyz_f64 const *xyz);
int alwan_is_within_pointer_gamut_xyz_f32(alwan_xyz_f32 const *xyz);

/* Get Pointer's Gamut boundary points
 * Returns array of xy chromaticity coordinates defining the boundary
 * count_out: receives the number of boundary points (32)
 * Returns pointer to internal static data (do not free) */
alwan_vec2_f64 const* alwan_pointer_gamut_boundary(size_t *count_out);

/* Get CIE 1931 spectral locus xy chromaticity for a given wavelength
 * Computes xy chromaticity from CIE 1931 2 deg observer CMFs for monochromatic light
 * xy_out: output xy chromaticity coordinates
 * wavelength: wavelength in nm (360-830nm)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if wavelength out of range */
alwan_status alwan_spectral_locus_xy_f32(alwan_vec2_f32 *xy_out, alwan_f32 wavelength);
alwan_status alwan_spectral_locus_xy_f64(alwan_vec2_f64 *xy_out, alwan_f64 wavelength);

/* Dominant wavelength, as colour-science's dominant_wavelength on the CIE 1931 2 deg
 * locus at 1 nm. The ray from xy_white through xy meets the locus, closed by the line
 * of purples from 830 nm to 360 nm, at xy_wl_out. On the spectrum locus the wavelength
 * is that point's, interpolated between the 1 nm samples where colour-science snaps to
 * the nearest, and xy_cw_out is xy_wl_out. On the line of purples the wavelength is the
 * negated complementary one, the opposite ray's, and xy_cw_out is that locus point.
 * xy_wl_out and xy_cw_out may be NULL. ALWAN_E_INVALID when xy is xy_white. */
alwan_status alwan_dominant_wavelength_f32(alwan_f32 *wavelength_out,
                               alwan_vec2_f32 *xy_wl_out,
                               alwan_vec2_f32 *xy_cw_out,
                               alwan_vec2_f32 const *xy,
                               alwan_vec2_f32 const *xy_white);
alwan_status alwan_dominant_wavelength_f64(alwan_f64 *wavelength_out,
                               alwan_vec2_f64 *xy_wl_out,
                               alwan_vec2_f64 *xy_cw_out,
                               alwan_vec2_f64 const *xy,
                               alwan_vec2_f64 const *xy_white);

/* Excitation purity, |xy - xy_white| / |xy_wl - xy_white| with xy_wl the dominant
 * wavelength's point, on the line of purples for a purple: 0 at the white point, 1 on
 * the closed locus, above 1 outside it (not clamped), as colour-science's
 * excitation_purity. Colorimetric purity is excitation purity times y_wl / y, as
 * colour-science's colorimetric_purity; ALWAN_E_INVALID for y = 0. */
alwan_status alwan_excitation_purity_f32(alwan_f32 *purity_out,
                             alwan_vec2_f32 const *xy,
                             alwan_vec2_f32 const *xy_white);
alwan_status alwan_excitation_purity_f64(alwan_f64 *purity_out,
                             alwan_vec2_f64 const *xy,
                             alwan_vec2_f64 const *xy_white);
alwan_status alwan_colorimetric_purity_f32(alwan_f32 *purity_out,
                               alwan_vec2_f32 const *xy,
                               alwan_vec2_f32 const *xy_white);
alwan_status alwan_colorimetric_purity_f64(alwan_f64 *purity_out,
                               alwan_vec2_f64 const *xy,
                               alwan_vec2_f64 const *xy_white);

/* Complementary wavelength, as colour-science's complementary_wavelength: the dominant
 * wavelength of the ray from xy_white away from xy, with the same outputs. When that
 * ray meets the line of purples, the wavelength is the negated dominant one of xy.
 * The f32 twins of these functions compute in double. */
alwan_status alwan_complementary_wavelength_f32(alwan_f32 *wavelength_out,
                                     alwan_vec2_f32 *xy_wl_out,
                                     alwan_vec2_f32 *xy_cw_out,
                                     alwan_vec2_f32 const *xy,
                                     alwan_vec2_f32 const *xy_white);
alwan_status alwan_complementary_wavelength_f64(alwan_f64 *wavelength_out,
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
alwan_status alwan_gamut_volume_ratio_f64(alwan_f64 *ratio_out,
                               alwan_rgb_space_desc_f64 const *space1,
                               alwan_rgb_space_desc_f64 const *space2);
alwan_status alwan_gamut_volume_ratio_f32(alwan_f32 *ratio_out,
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
alwan_status alwan_gamut_coverage_f64(alwan_f64 *coverage_out,
                          alwan_rgb_space_desc_f64 const *space1,
                          alwan_rgb_space_desc_f64 const *space2,
                          size_t num_samples,
                          unsigned int seed);
alwan_status alwan_gamut_coverage_f32(alwan_f32 *coverage_out,
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
 * method: any alwan_gamut_map_method. CLIP clamps in `space`'s own cube;
 *         HUE_PRESERVING is the projection the plain alwan_gamut_{T} maps run,
 *         applied in the working space; 2 to 7 are the Oklab projections.
 *         HUE_PRESERVING was ALWAN_E_INVALID here until 2026-09-22, and only
 *         for a colour outside the gamut, which is when a caller asks.
 * space: RGB color space descriptor (primaries and white point)
 * rgb_linear: input RGB color in linear (not gamma-corrected) space
 * rgb_out: receives mapped RGB color (guaranteed in [0,1])
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_gamut_map_advanced_f32(alwan_rgb_f32 *rgb_out,
                                  alwan_gamut_map_method method,
                                  alwan_rgb_space_desc_f32 const *space,
                                  alwan_rgb_f32 const *rgb_linear);
alwan_status alwan_gamut_map_advanced_f64(alwan_rgb_f64 *rgb_out,
                                  alwan_gamut_map_method method,
                                  alwan_rgb_space_desc_f64 const *space,
                                  alwan_rgb_f64 const *rgb_linear);

/* Buffer form, strides in bytes; the scalar in a loop, validated once, bit-identical to it (suite 174). */
alwan_status alwan_gamut_map_advanced_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_gamut_map_method method, alwan_rgb_space_desc_f32 const *space);
alwan_status alwan_gamut_map_advanced_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_gamut_map_method method, alwan_rgb_space_desc_f64 const *space);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 174). */
alwan_status alwan_gamut_map_advanced_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_gamut_map_method method, alwan_rgb_space_desc_f32 const *space);
alwan_status alwan_gamut_map_advanced_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_gamut_map_method method, alwan_rgb_space_desc_f64 const *space);

/* HDR gamut mapping in ICtCp (PQ) -- the HDR counterpart of the SDR methods
 * above. rgb_linear is linear BT.2020 RGB in ABSOLUTE cd/m2 (nits); the colour
 * is mapped into the display volume [0, peak_nits]^3 by chroma reduction in
 * ICtCp: intensity clamped to the displayable range, hue angle preserved
 * (Ct/Cp scaled jointly), chroma binary-searched to the boundary, with a
 * dE-ITP (BT.2124) JND early-out to the naive clip when indistinguishable.
 * In-volume inputs pass through bit-exactly. Output guaranteed in [0, peak]. */
alwan_status alwan_hdr_gamut_map_ictcp_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_linear, alwan_f32 peak_nits);
alwan_status alwan_hdr_gamut_map_ictcp_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_linear, alwan_f64 peak_nits);

/* CSS Color 4 gamut mapping parameterized by target space: same Oklch
 * chroma-reduction + deltaEOK-JND algorithm as alwan_css_gamut_*, but the
 * destination gamut is target_space's unit cube (e.g. Display-P3, Rec.2020)
 * instead of hard-wired sRGB. Input/output are LINEAR target-space RGB.
 * Assumes a D65-white target (no chromatic adaptation vs the sRGB-anchored
 * Oklab pivot). With the sRGB descriptor this matches alwan_css_gamut_* on
 * linear values. */
alwan_status alwan_css_gamut_space_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_space_desc_f32 const *target_space, alwan_rgb_f32 const *rgb_in);
alwan_status alwan_css_gamut_space_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_space_desc_f64 const *target_space, alwan_rgb_f64 const *rgb_in);

/* Spatial picture-formation gamut mapping (docs/gamut_spatial_formation.md).
 * Image-aware: reshapes the whole record field to fit [0,peak]^3 while
 * preserving local per-channel gradient structure (no increment<->decrement
 * flips), spreading the correction spatially instead of clipping flat. Unlike
 * the pixel-independent alwan_gamut_map_method family this needs width/height
 * and works on a full interleaved RGB image.
 *   s          : [0,1] spatial spread. 1 = touch only out-of-gamut pixels
 *                (near clip); 0 = reshape the whole image; between = a smooth
 *                spatial-distance falloff.
 *   reach      : spatial falloff radius in pixels (<=0 => image diagonal).
 *   beta       : structure-vs-fidelity balance (<=0 => default).
 *   compress   : gradient compression c in (0,1] (<=0 => 1 = preserve).
 *   depth_sigma: softness of depth-jump gating (<=0 or depth NULL => ignore).
 *   iterations : fixed solver iterations (<=0 => default; determinism).
 *   peak       : cube upper bound (1.0 for SDR).
 * depth is optional (NULL => single global envelope): one scalar per pixel,
 * used to stop coupling across occlusion boundaries. */
/* Picture-formation (image-aware) gamut methods. Distinct from the
 * pixel-independent alwan_gamut_map_method family; these reshape the whole
 * field. All 18 members share the alwan_gamut_map_spatial entry point +
 * params; each enumerator below documents its own method, and
 * docs/picture_formation.md carries the measured 15-constraint matrix for
 * every one of them. Implementation lives in src/alwan/experimental/
 * (research tier: the enum and entry point are stable, the operators'
 * internals may still evolve). */
typedef enum {
    ALWAN_GAMUT_FORM_GRADIENT = 0, /* per-channel gradient-domain field (halo-prone baseline).
                                    * Objective: minimise |grad(out)-grad(in)| per channel (Poisson)
                                    * s.t. gamut (POCS). NOT max(RGB)-monotone -> reverses the carrier. */
    ALWAN_GAMUT_FORM_GAIN = 1, /* ratio-preserving smooth-gain field: scale each pixel to
                                    * fit (hue/chroma kept, luminance lost), optionally spread. `s` = spread.
                                    * Objective: per-pixel gain so max(RGB)<=peak, then spatially smoothed
                                    * -> the smoothing breaks carrier monotonicity -> flips (worst case). */
    ALWAN_GAMUT_FORM_DENSITY = 2, /* luminance-preserving: keep the luma gradient (the surface),
                                    * collapse chroma per-channel to each bound. Zero luminance
                                    * flips by construction. Per pixel.
                                    * Objective: constrain luminance == input; minimise chroma to fit. */
    ALWAN_GAMUT_FORM_SURFACE = 3, /* DENSITY + surface envelope: the chroma-collapse amount is
                                    * smoothed *within* a segment (depth map = surface labels,
                                    * gated at boundaries) so a surface is treated uniformly, plus
                                    * a luminance shoulder for over-range luma. `s` = how uniform
                                    * (1 = per pixel, 0 = fully surface-uniform). CONSUMES: segmentation.
                                    * Objective: constrain luminance; minimise the within-segment variance
                                    * of the collapse. Spatial coupling -> reverses max(RGB) carrier. */
    ALWAN_GAMUT_FORM_OPTICAL = 4, /* SURFACE + the Kitaoka additive/multiplicative split (density
                                    * model): bright *increments* (additive / light) desaturate to
                                    * white, coloured *decrements* (multiplicative / absorptive
                                    * surface) keep their hue. Increment weight from luminance.
                                    * Luminance-preserving (0 luma flips). CONSUMES: segmentation.
                                    * Objective: as SURFACE, but split the collapse target by luminance;
                                    * spatial coupling -> reverses max(RGB) carrier. */
    ALWAN_GAMUT_FORM_CARRIER = 5, /* Carrier-conserving (Hamiltonian): max(RGB) = luminance +
                                    * chrominance is treated as a conserved carrier. A *per-pixel*
                                    * monotone shoulder maps the carrier into gamut (0 max(RGB)
                                    * flips by construction), then the non-max channels are
                                    * refilled by the additive/multiplicative split: bright +
                                    * desaturated (increment/light) -> white, saturated
                                    * (decrement/surface) -> keep hue. No spatial coupling of the
                                    * carrier, so it never reverses the envelope.
                                    * Objective: CONSERVE max(RGB) carrier (monotone -> 0 flips); constrain
                                    * purity to attenuate 0->1 from boundary to maximal emission (R=G=B). */
    ALWAN_GAMUT_FORM_XJUNCTION = 6, /* CARRIER, additive veil = windowed dark channel (box min over
                                    * a neighbourhood of min(RGB)) = Koschmieder airlight. High veil
                                    * -> additive -> white; near-zero -> multiplicative -> keep hue.
                                    * Spatial but only refills non-max channels, so carrier stays
                                    * per-pixel monotone (0 flips). Box-window (crude) veil.
                                    * Objective: same as CARRIER; airlight weight = box dark-channel veil. */
    ALWAN_GAMUT_FORM_FLUX = 7, /* Like XJUNCTION but the veil is a *diffused field* (guided /
                                    * matting-Laplacian filter of the dark channel) instead of a box
                                    * window: no boundary/junction is ever detected, the veil is the
                                    * smooth envelope the picture sits on and the residual is the
                                    * surface (a flux-field / Grady-style decomposition). Reads
                                    * boundary-free content (clouds) natively. Still 0 flips.
                                    * Objective: same as CARRIER; airlight weight = diffused dark-channel veil. */
    ALWAN_GAMUT_FORM_DENSITY_LOG = 8, /* Both-end density S-curve. A *monotone* S on the carrier gives a
                                    * toe and a shoulder; chroma collapses at BOTH ends (toward black
                                    * in shadows, toward white in highlights) via a factor that is 0 at
                                    * black and at maximal emission, 1 in the mid. Because the collapse
                                    * only refills non-max channels toward the shouldered carrier, it
                                    * stays carrier-safe (0 max(RGB) flips) unlike a luminance toe.
                                    * Objective: conserve carrier (monotone S); constrain chroma -> 0 at
                                    * BOTH ends (toe = black, shoulder = white). */
    ALWAN_GAMUT_FORM_COMPLETION = 9, /* FLUX + volumetric veil: the diffused dark-channel veil is scaled
                                    * by depth (path length through the participating volume =
                                    * Koschmieder airlight), so it is a diffuse layer filling the
                                    * VOLUME (vision through layers of dye) rather than a flat 2D blur.
                                    * Uses the `depth` argument; falls back to FLUX if depth is NULL.
                                    * Carrier-safe (0 flips). CONSUMES: depth.
                                    * Objective: same as FLUX; veil weighted by depth (path length). */
    ALWAN_GAMUT_FORM_PICTURE = 10, /* The synthesis. A per-pixel monotone shoulder on the carrier
                                    * max(RGB) (0 flips), and the non-max channels reconstructed as
                                    * lerp(white(m'), ratio-preserve(m'), keep) with a SINGLE keep term:
                                    *   keep = cf * (1 - a)
                                    *   cf = toe(m)*shoulder(m)  -> both-end density collapse (chroma
                                    *        -> black in shadow, -> white/R=G=B at the scene's actual
                                    *        maximal emission; adaptive, gradual, integrable),
                                    *   a  = diffused dark-channel veil / m  -> additive airlight
                                    *        (light -> white) vs multiplicative surface (-> keep hue).
                                    * Self-contained (veil from the pixels; no depth/seg needed),
                                    * carrier-safe, both ends integrate, boundaries emergent. The
                                    * culmination: carrier conservation + density S + field veil. */
    ALWAN_GAMUT_FORM_MOMENT = 11, /* Energy-rotation formation (MacAdam moment + Troy's rotation).
                                    * Two conserved quantities: the carrier max(RGB) (0 flips) and the
                                    * energy E, with whiteness=kinetic=sin^2(phi) and chroma=potential=
                                    * cos^2(phi) an orthogonal quadrature (matching orthogonal retinal
                                    * ganglion firing). Expressed excitation purity = sin(2*phi) -> 0 at
                                    * both the black and white infinities, peak at mid: the both-end
                                    * collapse as an identity, not a fitted curve. phi rides the carrier
                                    * in reciprocal/Binet coordinates (M ~ 1/Y). Per-pixel, self-contained.
                                    * Stays linear (Grassmannian-additive) throughout: no Lab/Oklab.
                                    * NOTE: ratio-preserve whitening drifts perceived hue (blue->purple,
                                    * the Abney "varying textural base colour"), but per Troy that is a
                                    * COGNITIVE/contextual computation, not fixable by any Cartesian
                                    * colour space -- so it is deliberately left to the viewer's
                                    * scene-level decomposition, not "corrected" per-pixel. */
    ALWAN_GAMUT_FORM_HEMISPHERE = 12, /* The parameter-free synthesis (Troy's "hemisphere"). The scalar
                                    * integration I = max(RGB) is mapped by a C2 log-logistic whose
                                    * slope (gradient gain) is a smooth bump: maximal at the exposure,
                                    * rolling to ZERO at both energy infinities. Every quantity is
                                    * derived, nothing tuned: pivot = log geometric-mean carrier (scene
                                    * adaptation, invariant per light field); width fixed by requiring
                                    * unit log-log gamma at the pivot (contrast preserved at the
                                    * exposure). Hue-agnostic purity collapse only near display max
                                    * (no per-record adjustment). Per-pixel monotone => the global
                                    * integration ordering is preserved (depth-safe, 0 carrier flips).
                                    * Objective: invariant integration structure -- "not a creative
                                    * decision"; C2 (no false segmentation from slope kinks). */
    ALWAN_GAMUT_FORM_CHANNEL = 13, /* Channel integration (Troy Sobotka's AgX architecture): SB2383
                                    * per-primary inset -> per-channel C2 log-logistic -> matched inverse
                                    * outset -> soft guard rails. Every constant is a display standard,
                                    * NONE scene-derived (temporal constancy -- "not a creative decision"):
                                    * window = log2(0.18) +/- (10, 6.5) stops (18% grey camera exposure);
                                    * slope spans the 8-bit display across the window; the sigmoid is
                                    * affine-normalized (window bottom -> display 0, top -> 1: neutral DC
                                    * respected and the volume spanned) then anchored at 18% mid-grey via
                                    * gamma = log(0.18)/log(0.5); outset = inset^-1 (per-primary
                                    * complementary restore with negative lobes); rails = asymptotic toes
                                    * 0.004 (black) / 0.02 (white), no clamp contour at the "discretized
                                    * infinity". Whiteness and hue sweep EMERGE from staggered channel
                                    * saturation -- no purity term. Per-pixel; output display-referred
                                    * [0,1]. NOTE: not carrier-monotone (AgX-class inset crosstalk); the
                                    * depth invariant rides the per-record integrations. */
    ALWAN_GAMUT_FORM_HEMISPHERE_ABS = 14, /* The Absolute Hemisphere: HEMISPHERE's carrier tone made
                                    * temporally invariant. Same C2 log-logistic on I = max(RGB),
                                    * but on CHANNEL's FIXED absolute window (log2(0.18) +/- (10,6.5)),
                                    * affine-normalized (DC respected), 18% mid-grey anchored, with
                                    * asymptotic guard rails; hue-agnostic ratio-preserve
                                    * reconstruction with purity collapse only at maximal emission.
                                    * Carrier-monotone AND hue-agnostic AND scene-invariant -- the
                                    * nearest-complete carrier operator (12-of-13 under the original
                                    * constraint set; its lone RATIO failure is what the COMPLETE
                                    * operators below remove -- see docs/picture_formation.md). The
                                    * trade vs CHANNEL: being carrier-monotone and hue-agnostic rules
                                    * out channel integration, so form-through-highlight is weaker. */
    ALWAN_GAMUT_FORM_WARP = 15, /* HEMISPHERE_ABS + a monotone smooth local-contrast WARP that
                                    * restores the scene curvature the tonescale flattened (a
                                    * form-through-highlight refinement). Closed-form, no solver:
                                    * f_out = m + gamma*(f_H - m) on the log-carrier, m a smooth
                                    * pivot, gamma = 1 + b*blur(|mean-curvature of the log scene
                                    * carrier|) >= 0; reconstructed by a per-pixel POSITIVE
                                    * ratio-preserving scale with a carrier cap (RATIO-safe: a
                                    * uniform scale, never a per-channel clamp). gamma >= 0 and
                                    * smooth => carrier-monotone (no flip) by construction, analytic
                                    * => no pooling. Inherits HEMISPHERE_ABS's constraint row but
                                    * raises isophote-through-highlight and removes its highlight
                                    * curvature steepening. See docs/picture_formation.md. */
    ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK = 16, /* The first all-constraints operator, look-preserving.
                                    * HEMISPHERE_ABS's carrier tone with its lone RATIO failure removed
                                    * by two changes: the asymptotic HIGH rail runs on the carrier (a
                                    * single scalar) so reconstruction is a uniform per-pixel scale
                                    * (never a per-channel clamp that rotates hue), and a desaturation
                                    * FLOOR (purity-keep capped < 1) lifts the min channel off 0 as a
                                    * uniform chroma scale (direction preserved). Pointwise =>
                                    * carrier-monotone, hue-agnostic and scene-invariant AND
                                    * RATIO-clean: satisfies ALL 15 numerically-testable constraints
                                    * in the shipped matrix. This variant keeps HEMISPHERE_ABS's exact
                                    * window, so the look/contrast is unchanged.
                                    * See docs/picture_formation.md. */
    ALWAN_GAMUT_FORM_COMPLETE = 17, /* The same all-15 operator, isophote-through-highlight-maximised:
                                    * a wider carrier window (the OpenSNOPT-optimal point of the
                                    * feasible set) preserves more scene curvature through the
                                    * highlight, at the cost of a brighter, softer, lifted look. Prefer
                                    * COMPLETE_HEMI_LOOK for the reference HEMISPHERE_ABS appearance;
                                    * this for maximum form-through-highlight. NOTE: the wider window
                                    * puts the display peak 9.85 stops over mid-grey (scene 166), so
                                    * ordinary footage never reaches white (scene 16 -> 0.945). See
                                    * docs/picture_formation.md. */
    ALWAN_GAMUT_FORM_COMPLETE_PEAK = 18 /* COMPLETE's purity shelf, white rail and exact 18% anchor on
                                    * the family's 16.5-stop window, so the display peak is reached at
                                    * the same scene value as HEMISPHERE_ABS and CHANNEL (6.5 stops
                                    * over mid-grey, scene 16.3), with the 0.997 purity cap so the
                                    * interior of the display range is left alone. The anchor is kept
                                    * by deriving the tone exponent from the pivot's actual normalised
                                    * position instead of assuming it sits at 0.5, which only holds for
                                    * a symmetric window; the shelf stays at COMPLETE's scene positions
                                    * (+4.3 to +13.8 stops) rather than moving with the window; the
                                    * cap is HEMISPHERE_ABS's, since COMPLETE's 0.861 desaturates every
                                    * saturated pixel by 13.9% from black up. Same operator, same
                                    * 15-of-15 row. See docs/picture_formation.md. */
} alwan_gamut_formation_method;

/* `s` is a per-family knob (its meaning depends on `method`):
 *   GRADIENT           : position of the locked/free band (s=1 -> almost all locked).
 *   GAIN               : spread of the darkening / halo control (s=1 -> per-pixel, tightest).
 *   SURFACE, OPTICAL   : spatial uniformity of the chroma collapse within a surface
 *                        (0 = fully surface-uniform, 1 = per-pixel); smoothing iters = (1-s)*iterations.
 *   DENSITY            : additive whitening beyond the minimal fit
 *                        (0 = minimal collapse, keep colour; 1 = full neutral/white for OOG pixels).
 *   CARRIER, XJUNCTION, FLUX : extra airlight whitening ON TOP of the mandatory purity
 *                        attenuation. These always attenuate excitation purity linearly from 0 at the
 *                        gamut boundary to full R=G=B (white) at the image's ACTUAL maximal emission
 *                        (adaptive: a scene that barely exceeds gamut only desaturates a little), so
 *                        the gradient tilts into desaturation across the boundary, spreads over the
 *                        whole flux range, and stays integrable; `s` adds airlight bias beyond that
 *                        (0 = just the mandatory attenuation, 1 = full white for airlight regions).
 * The carrier family (DENSITY/CARRIER/XJUNCTION/FLUX) applies `s` per pixel, so it never
 * reintroduces max(RGB) carrier flips. */
typedef struct {
    alwan_gamut_formation_method method;
    alwan_f32 s, reach, beta, compress, depth_sigma;
    int iterations;
    alwan_f32 peak;
} alwan_gamut_spatial_params_f32;
typedef struct {
    alwan_gamut_formation_method method;
    alwan_f64 s, reach, beta, compress, depth_sigma; /* see per-method note above; `s` meaning depends on `method` */
    int iterations;
    alwan_f64 peak;
} alwan_gamut_spatial_params_f64;

alwan_status alwan_gamut_map_spatial_f32(alwan_f32 *out, alwan_f32 const *in, alwan_f32 const *depth, int width, int height, alwan_gamut_spatial_params_f32 const *params, alwan_ctx *ctx);
alwan_status alwan_gamut_map_spatial_f64(alwan_f64 *out, alwan_f64 const *in, alwan_f64 const *depth, int width, int height, alwan_gamut_spatial_params_f64 const *params, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * EXPERIMENTAL picture formation (src/alwan/experimental/)
 * These relax the library's guarantees: NON-DETERMINISTIC, inlined constants (no gendata), and NOT
 * part of the constraint-tested surface. Research code, subject to change. Interleaved RGB, W*H*3.
 * ---------------------------------------------------------------- */

/* HYBRID-point base operator: y = (1-q)*COMPLETE_HEMI_LOOK(x) + q*CHANNEL(x), q frozen from x. */
alwan_status alwan_picture_form_hybrid_exp_f32(alwan_f32 *out, alwan_f32 const *in, int width, int height, alwan_ctx *ctx);
alwan_status alwan_picture_form_hybrid_exp_f64(alwan_f64 *out, alwan_f64 const *in, int width, int height, alwan_ctx *ctx);

/* Global exposure operator: y = F_base(2^e0 x), scalar per-pixel exposure e0, MONOTONE and hue-stable.
 * Only a scalar exposure reaches the picture and the base is hue-stable => structurally hue-preserving
 * (no chroma/illuminant/purity variable). 'strength' is the compression gain k: strength<=0 gives
 * e0=0 = the exact pointwise baseline; ~0.15 applies e0 = -cap*tanh(k*(z-zbar)/cap) (z=log2(max x),
 * zbar = mean of the non-near-black carrier), a global soft toe+shoulder whose z->z+e0 map is strictly
 * increasing (slope >= 1-k), so the output carrier never inverts scene order -- no halos, no
 * enclosed-region lift -- while burning highlights (recovering blown chroma) and gently lifting
 * shadows. NB strict monotonicity in z makes this a GLOBAL curve; local surround-dependent adaptation
 * is mathematically incompatible with it. 'iterations' is reserved for a future non-monotone
 * evidence-driven target. 'pivot' (linear carrier units): > 0 anchors the curve at the FIXED
 * log2(pivot) (0.18 = mid-grey) -- frame-independent, TCONST/INVR-clean, the video mode; <= 0 uses
 * the scene-adaptive robust mean (per-frame centring; shifts with content). Interleaved RGB, W*H*3. */
alwan_status alwan_picture_form_global_exp_f32(alwan_f32 *out, alwan_f32 const *in, int width, int height, int iterations, alwan_f32 strength, alwan_f32 pivot, alwan_ctx *ctx);
alwan_status alwan_picture_form_global_exp_f64(alwan_f64 *out, alwan_f64 const *in, int width, int height, int iterations, alwan_f64 strength, alwan_f64 pivot, alwan_ctx *ctx);
/* The same solve, returning the exposure field it applied: e_out is width*height scalars in stops,
 * one per pixel, so that Y = F_base(2^e_out X). out receives the picture and may be NULL. */
alwan_status alwan_picture_form_global_exp_field_f32(alwan_f32 *e_out, alwan_f32 *out, alwan_f32 const *in, int width, int height, int iterations, alwan_f32 strength, alwan_f32 pivot, alwan_ctx *ctx);
alwan_status alwan_picture_form_global_exp_field_f64(alwan_f64 *e_out, alwan_f64 *out, alwan_f64 const *in, int width, int height, int iterations, alwan_f64 strength, alwan_f64 pivot, alwan_ctx *ctx);

/* Per-pixel junction evidence P(Continuation / Transmission / Occlusion) from local gradient structure
 * (the gradient-formation reframe). DIAGNOSTIC only -- does not modify the picture. Occlusion = strong
 * carrier boundary; Transmission = additive light lifting the dark channel; Continuation = one
 * integrated illuminated volume (a warm-lit surface reads here, not as a pink layer). 'out' is
 * W*H*3 interleaved = (P_C, P_T, P_O) per pixel, summing to 1. */
alwan_status alwan_picture_form_evidence_f32(alwan_f32 *out, alwan_f32 const *in, int width, int height, alwan_ctx *ctx);
alwan_status alwan_picture_form_evidence_f64(alwan_f64 *out, alwan_f64 const *in, int width, int height, alwan_ctx *ctx);

/* Evidence-gated LOCAL exposure operator: y = F_base(2^e x), where e adapts region-locally with
 * region structure from the junction evidence (adaptation base smoothed within continuation, cut at
 * occlusion boundaries), and carrier order on reliable edges (|dz|>0.05 stop) repaired by an
 * increasing-penalty solve (rho 30->3000). The deliberate NON-monotone counterpart of
 * alwan_picture_form_global_exp: z-monotonicity is traded for scene-structured local adaptation,
 * with the explicit carrier-order constraint as the safety rail; hue safety unchanged (scalar
 * exposure into the hue-stable base). strength = adaptation gain (<=0 => exact baseline);
 * iterations = repair iterations (<=0 -> default). pivot: > 0 centres the drive at the FIXED
 * log2(pivot) (closes the frame-derived-pivot half of the TCONST gap; the operator stays spatially
 * local); <= 0 scene-adaptive. Interleaved RGB, W*H*3. */
alwan_status alwan_picture_form_local_exp_f32(alwan_f32 *out, alwan_f32 const *in, int width, int height, int iterations, alwan_f32 strength, alwan_f32 pivot, alwan_ctx *ctx);
alwan_status alwan_picture_form_local_exp_f64(alwan_f64 *out, alwan_f64 const *in, int width, int height, int iterations, alwan_f64 strength, alwan_f64 pivot, alwan_ctx *ctx);
/* The same solve, returning the exposure field it converged to: e_out is width*height scalars in
 * stops. out receives the picture and may be NULL. */
alwan_status alwan_picture_form_local_exp_field_f32(alwan_f32 *e_out, alwan_f32 *out, alwan_f32 const *in, int width, int height, int iterations, alwan_f32 strength, alwan_f32 pivot, alwan_ctx *ctx);
alwan_status alwan_picture_form_local_exp_field_f64(alwan_f64 *e_out, alwan_f64 *out, alwan_f64 const *in, int width, int height, int iterations, alwan_f64 strength, alwan_f64 pivot, alwan_ctx *ctx);
/* The same again with the two accelerators. relax > 0 replaces the fixed 0.02 step with
 * relax * g / diag(H): the anchor's curvature is 2 everywhere and the order penalty's is
 * 2*rho*Jc^2 only where its hinge bites, so one step size is either unstable on the second or
 * glacial on the first, and 0.02 chose glacial. Scaling by the diagonal leaves the fixed point
 * alone, so relax is a rate and not a different answer. momentum is a heavy ball on top and buys
 * much less: at a fixed step its rate floor is sqrt(momentum). Both 0 is the plain step exactly. */
alwan_status alwan_picture_form_local_exp_field_m_f32(alwan_f32 *e_out, alwan_f32 *out, alwan_f32 const *in, int width, int height, int iterations, alwan_f32 strength, alwan_f32 pivot, alwan_f32 momentum, alwan_f32 relax, alwan_gamut_formation_method form_method, alwan_ctx *ctx);
alwan_status alwan_picture_form_local_exp_field_m_f64(alwan_f64 *e_out, alwan_f64 *out, alwan_f64 const *in, int width, int height, int iterations, alwan_f64 strength, alwan_f64 pivot, alwan_f64 momentum, alwan_f64 relax, alwan_gamut_formation_method form_method, alwan_ctx *ctx);

/* EXPERIMENTAL: one frame of a moving picture, resuming the previous frame's solve.
 *
 * The still-picture call solves each frame from nothing, which is not tractable per frame and
 * would in any case make the picture jump the instant the light changes. This carries both of
 * the solver's states between frames instead: e_io is the exposure field in stops and base_io
 * the smoothed adaptation base, each width*height, both read when warm is non-zero and both
 * written back. Allocate them once, pass warm = 0 on the first frame and 1 on every frame
 * after.
 *
 * iterations and base_sweeps are this frame's budget. At convergence (about 60 and 200) every
 * frame is solved independently and the answer matches the still-picture call. Far below it the
 * fields lag the scene, and the lag is the point: the picture arrives at the new light over
 * several frames the way an eye does, for a few sweeps of work per frame rather than a full
 * solve. What the lag cannot yet be given is a time constant in seconds, so it is whatever the
 * budget and the frame rate make it; see docs/alwan_future.md. */
alwan_status alwan_picture_form_local_exp_resume_f32(alwan_f32 *e_io, alwan_f32 *base_io, alwan_f32 *out, alwan_f32 const *in, int width, int height, int warm, int iterations, int base_sweeps, alwan_f32 strength, alwan_f32 pivot, alwan_ctx *ctx);
alwan_status alwan_picture_form_local_exp_resume_f64(alwan_f64 *e_io, alwan_f64 *base_io, alwan_f64 *out, alwan_f64 const *in, int width, int height, int warm, int iterations, int base_sweeps, alwan_f64 strength, alwan_f64 pivot, alwan_ctx *ctx);

/* EXPERIMENTAL: the same frame with a heavy-ball velocity carried alongside the field.
 *
 * v_io is a third width*height state, read and written exactly like e_io, starting at rest when
 * warm is 0. momentum 0 with relax 0 reproduces alwan_picture_form_local_exp_resume bit for bit.
 *
 * form_method chooses the formation operator the solve is wrapped around.
 * ALWAN_GAMUT_FORM_COMPLETE_HEMI_LOOK is what the non-_m entries use; its 0.861 purity cap
 * desaturates every saturated pixel by about 14% from black up. ALWAN_GAMUT_FORM_COMPLETE_PEAK
 * reaches display white at essentially the same scene value with a 0.997 cap, so the interior of
 * the display range is left alone. The solve is indifferent: it only reads max(RGB) of the result.
 *
 * warm takes a third value here: 0 is a cold start, 1 resumes the previous frame, and 2 says this
 * frame begins a new shot. On a cut, warm 1 carries the previous scene's field into the new one,
 * and since the field may only move so far per frame it stays there for the better part of a
 * second, which reads as the old shot ghosting through the new one. An eye carries its level of
 * adaptation across a cut, not a memory of what it was looking at, so warm 2 keeps the field's
 * mean exactly and takes its structure from the frame in hand. Detecting the cut is the caller's,
 * since only the caller knows whether a large change is an edit or an event in the scene.
 *
 * This separates two things the plain call ties together. Without it the per-frame budget is
 * both how much work a frame costs and how long the picture takes to follow a change in the
 * light, so a real-time budget also fixes the adaptation time and a slow adaptation can only be
 * bought by refusing to converge. With momentum the solve arrives in a few frames at a small
 * budget. Getting a time constant in seconds is then the caller's, and the way to do it is to
 * filter the returned field toward the solved one, uniformly. A small relax also slows the
 * picture down, but it slows each pixel by its own curvature, so a hinge-bound pixel lags a free
 * one and the field arrives unevenly. relax is a solver rate; a filter is a time constant. */
alwan_status alwan_picture_form_local_exp_resume_m_f32(alwan_f32 *e_io, alwan_f32 *v_io, alwan_f32 *base_io, alwan_f32 *out, alwan_f32 const *in, int width, int height, int warm, int iterations, int base_sweeps, alwan_f32 strength, alwan_f32 pivot, alwan_f32 momentum, alwan_f32 relax, alwan_gamut_formation_method form_method, alwan_ctx *ctx);
alwan_status alwan_picture_form_local_exp_resume_m_f64(alwan_f64 *e_io, alwan_f64 *v_io, alwan_f64 *base_io, alwan_f64 *out, alwan_f64 const *in, int width, int height, int warm, int iterations, int base_sweeps, alwan_f64 strength, alwan_f64 pivot, alwan_f64 momentum, alwan_f64 relax, alwan_gamut_formation_method form_method, alwan_ctx *ctx);

/* EXPERIMENTAL: what a frame gives the solver, before any repair iteration has run.
 *
 * Three width*height fields, each of which may be NULL:
 *   carrier_out  log2(max(R,G,B)), the scene's own carrier in stops
 *   base_out     that carrier smoothed by evidence-gated Jacobi and cut at occlusion boundaries,
 *                the solver's reading of how lit a region is rather than a pixel
 *   target_out   the anchor the exposure field is pulled toward, -strength*(base - log2(pivot)),
 *                capped at one stop either way. That cap is why a scene that brightens by four
 *                stops does not get four stops back.
 *
 * The exposure field itself is not here: it is state carried between frames, which the caller
 * already holds in e_io. These three are what changes when the scene does. */
alwan_status alwan_picture_form_local_exp_inputs_f32(alwan_f32 *carrier_out, alwan_f32 *base_out, alwan_f32 *target_out, alwan_f32 const *in, int width, int height, alwan_f32 strength, alwan_f32 pivot, alwan_ctx *ctx);
alwan_status alwan_picture_form_local_exp_inputs_f64(alwan_f64 *carrier_out, alwan_f64 *base_out, alwan_f64 *target_out, alwan_f64 const *in, int width, int height, alwan_f64 strength, alwan_f64 pivot, alwan_ctx *ctx);

/* EXPERIMENTAL: the picture from a given exposure field, y = F_base(2^e x).
 *
 * This is the solve's own last step, so a caller who does something to the field between the
 * solve and the picture, filters it toward a level, blends two, clamps it, still gets the
 * operator's answer for the field it chose. e is width*height stops; in and out are interleaved
 * RGB, width*height*3. */
alwan_status alwan_picture_form_local_exp_apply_f32(alwan_f32 *out, alwan_f32 const *in, alwan_f32 const *e, int width, int height, alwan_gamut_formation_method form_method, alwan_ctx *ctx);
alwan_status alwan_picture_form_local_exp_apply_f64(alwan_f64 *out, alwan_f64 const *in, alwan_f64 const *e, int width, int height, alwan_gamut_formation_method form_method, alwan_ctx *ctx);

/* EXPERIMENTAL: a time constant in seconds on the exposure level, and its asymmetry.
 *
 * The resumed solve converges every frame at a small budget, so the field's structure is the
 * frame in hand and nothing smears. What carries from one frame to the next is a level of
 * adaptation, which is also what an eye carries, so the lag is applied to the mean alone:
 * level_io follows the solved field's mean with a first-order response, closing
 * 1 - exp(-dt_seconds / tau) of the remaining gap per frame, and the difference goes back on every
 * pixel as a uniform offset. The structure does not move.
 *
 * tau_light_seconds applies when the field is falling, the light having gone up, and
 * tau_dark_seconds when it is rising: light adaptation is a matter of seconds and dark adaptation
 * of minutes. A tau <= 0 is instant in that direction.
 *
 * e_in is the solved field and e_out the lagged one, each width*height stops; they may be the
 * same buffer. level_io is one scalar the caller keeps between frames. warm 0 sets it to the
 * solved mean, so a first frame is not dragged toward zero; any other value filters. A cut is
 * not a reset: an eye does not re-adapt the instant an edit happens. Form the picture from
 * e_out with alwan_picture_form_local_exp_apply. */
alwan_status alwan_picture_form_local_exp_lag_f32(alwan_f32 *e_out, alwan_f32 *level_io, alwan_f32 const *e_in, int width, int height, int warm, alwan_f32 dt_seconds, alwan_f32 tau_light_seconds, alwan_f32 tau_dark_seconds);
alwan_status alwan_picture_form_local_exp_lag_f64(alwan_f64 *e_out, alwan_f64 *level_io, alwan_f64 const *e_in, int width, int height, int warm, alwan_f64 dt_seconds, alwan_f64 tau_light_seconds, alwan_f64 tau_dark_seconds);

/* ----------------------------------------------------------------
 * EXPERIMENTAL: fit an RGB encoding space to a dataset
 *
 * Given colours in some RGB space, find primaries, a white point, a transfer function and a
 * scale so that the dataset survives quantisation to `bits` with the least perceptual damage.
 * The transfer function is a free power, or the sRGB curve because its decode is free on a
 * GPU. The scale is the linear value that encodes to code 1.0 (1 is the usual Y = 1 white):
 * without it a dataset that stops short of the white, or whose bright colours sit off the
 * white, cannot reach the top codes in any tight triangle. It costs one multiply on a GPU
 * and can be folded into the matrix. The result comes back in the ordinary
 * alwan_rgb_space_desc (chromaticities and Y = 1 matrices) plus alwan_fit_tf: under the sRGB
 * branch the descriptor is complete (oetf/eotf = ALWAN_TF_SRGB); under the power branch the
 * exponent rides in alwan_fit_tf and the descriptor's enums are set only when the exponent
 * lands on one of 2.2, 2.4, 2.6 or 2.8. Encoding is code = oetf(linear / scale).
 *
 * The solver descends a smooth surrogate (the expected quantisation error, code step times
 * the Jacobian from code to perceptual coordinates, plus a hinge on values outside 0..scale) with
 * a deterministic Nelder-Mead; the report always carries the TRUE quantised round trip. Three
 * ways to run again: step() resumes, restart() rebuilds the simplex around the current best
 * with new move sizes, and begin() or solve() with a filled descriptor warm-starts from it.
 * The returned space is always the best candidate evaluated so far on the current dataset.
 * When the dataset is a sample of a larger image, put the image's extremes in it (brightest
 * pixels, outermost chromaticities): the clip penalty only protects the samples it sees.
 * ---------------------------------------------------------------- */

typedef enum { ALWAN_FIT_TF_POWER = 0, ALWAN_FIT_TF_SRGB = 1, ALWAN_FIT_TF_AUTO = 2 } alwan_fit_tf_kind;
/* What "error" means to the fit. OKLAB, ITP and DE2000 measure the stimulus. DISPLAY measures
 * what an ordinary sRGB screen shows: XYZ to Rec.709, clamped to the display's range, sRGB
 * OETF, euclidean on the code values (a distance of 0.0039 is one 8-bit code); a colour the
 * screen cannot show costs nothing there, because it clamps to the same place either way.
 * LINEAR measures the stored values themselves, euclidean, for a data texture whose channels
 * are numbers and not an appearance: a normal map, a roughness or displacement map. */
typedef enum { ALWAN_FIT_METRIC_OKLAB = 0, ALWAN_FIT_METRIC_ITP = 1, ALWAN_FIT_METRIC_DE2000 = 2,
               ALWAN_FIT_METRIC_DISPLAY = 3, ALWAN_FIT_METRIC_LINEAR = 4 } alwan_rgb_fit_metric;
enum { ALWAN_FIT_LOCK_WHITE = 1, ALWAN_FIT_LOCK_PRIMARIES = 2, ALWAN_FIT_LOCK_TF = 4, ALWAN_FIT_LOCK_SCALE = 8,
       ALWAN_FIT_LOCK_OFFSET = 16 };

/* What going out of the fitted gamut costs the objective.
 *
 * FORBID charges the overshoot itself, per sample and once more on the single worst one, so
 * the answer is a gamut that holds the whole dataset.
 *
 * PRICED charges what clipping actually does: the metric distance between the colour and what
 * it becomes once clamped, in the same units as the quantisation error. The two are then
 * comparable and the solver weighs them itself, sample by sample, as often as each happens. On
 * a peaked distribution (a normal map, one material, a single-lit scene) it will trade a small
 * fraction of clipped border colours for a tighter gamut and more precision everywhere else.
 * clip_weight still scales the charge: above 1 buys back caution, below 1 more aggression. */
typedef enum { ALWAN_FIT_CLIP_FORBID = 0, ALWAN_FIT_CLIP_PRICED = 1 } alwan_fit_clip_policy;

/* The fitted transfer function. The full decode of a channel c is
 *
 *     linear_c = scale * ( offset_c + (1 - offset_c) * eotf(code_c) )
 *
 * gamma is used by POWER only (eotf(x) = x^gamma). scale is the linear value that code 1.0
 * reaches when offset is 0; on input 0 means 1, else 1e-8..1e8. offset is the per-channel black
 * point in units of the scale, 0..0.95: code 0 decodes to it rather than to zero. Data that
 * lives away from the origin cannot use its code range without it, because a gamut is a cone
 * from the origin; set params.fit_offset to let the solver move it. */
typedef struct { alwan_fit_tf_kind kind; alwan_f32 gamma; alwan_f32 scale; alwan_f32 offset[3]; } alwan_fit_tf_f32;
typedef struct { alwan_fit_tf_kind kind; alwan_f64 gamma; alwan_f64 scale; alwan_f64 offset[3]; } alwan_fit_tf_f64;

/* Fit parameters. Fill with alwan_rgb_fit_params_init, then change what you need. */
typedef struct {
    int bits;                          /* target depth, 8 */
    int bits_channel[3];               /* per-channel depth, 0 to take `bits`; {5,6,5} is a BC1 endpoint */
    int fit_offset;                    /* non-zero: the per-channel black point joins the search */
    int block_size;                    /* block fit only: tile edge in texels, 4 for BC1 */
    int index_bits;                    /* block fit only: bits per texel index, 2 for BC1 */
    alwan_fit_tf_kind tf;              /* POWER or SRGB; AUTO is accepted by solve() only */
    alwan_rgb_fit_metric metric;       /* objective and report units, Oklab by default */
    alwan_f32 percentile;              /* tail the objective minimises, 0.999; 0 means mean only.
                                          Must lie in [0, 1]: anything else, NaN included, is
                                          ALWAN_E_INVALID from every entry point */
    alwan_f32 clip_weight;             /* penalty per unit of linear value outside 0..scale (in units of the scale),
                                          per sample and on the single worst overshoot; 1.0. It buys clipping with
                                          precision, so it sets where on that trade the answer lands, and it is in the
                                          METRIC's units: 1.0 is calibrated for OKLAB, and a metric whose numbers run
                                          larger (DISPLAY, ITP) needs it raised in proportion or the fit will accept
                                          clipped pixels. 0 lets the percentile decide alone */
    alwan_f32 srgb_margin;             /* AUTO: power must beat sRGB by this fraction, 0.10 */
    alwan_fit_clip_policy clip_policy;  /* FORBID (default) or PRICED, see above */
    unsigned lock;                     /* ALWAN_FIT_LOCK_* bits: keep those where the start put them */
    alwan_f32 gamma_min, gamma_max;    /* 1.0 and 3.0 */
    alwan_f32 step_xy, step_log_gamma; /* initial move sizes: 0.01 in xy, 0.1 in log gamma and log scale */
    alwan_f32 tolerance;               /* converged when the simplex spread falls under this, 1e-6 */
    alwan_f32 const *weights;          /* optional, one per sample, NULL for uniform */
} alwan_rgb_fit_params_f32;
typedef struct {
    int bits;
    int bits_channel[3];
    int fit_offset;
    int block_size;
    int index_bits;
    alwan_fit_tf_kind tf;
    alwan_rgb_fit_metric metric;
    alwan_f64 percentile;
    alwan_f64 clip_weight;
    alwan_f64 srgb_margin;
    alwan_fit_clip_policy clip_policy;
    unsigned lock;
    alwan_f64 gamma_min, gamma_max;
    alwan_f64 step_xy, step_log_gamma;
    alwan_f64 tolerance;
    alwan_f64 const *weights;
} alwan_rgb_fit_params_f64;

/* What a space does to the dataset, measured with the real quantiser. */
typedef struct {
    alwan_f32 mean_error, tail_error, max_error;   /* metric units, weighted mean / percentile / worst */
    alwan_f32 clipped_fraction;                    /* weighted share of samples with a channel outside 0..scale */
    alwan_f32 objective;                           /* the smooth surrogate the solver minimised */
    int iterations;                                /* Nelder-Mead iterations so far on this state */
    int converged;
} alwan_rgb_fit_report_f32;
typedef struct {
    alwan_f64 mean_error, tail_error, max_error;
    alwan_f64 clipped_fraction;
    alwan_f64 objective;
    int iterations;
    int converged;
} alwan_rgb_fit_report_f64;

typedef struct alwan_rgb_fit_state_f32 alwan_rgb_fit_state_f32;   /* opaque: dataset cache and simplex */
typedef struct alwan_rgb_fit_state_f64 alwan_rgb_fit_state_f64;

void alwan_rgb_fit_params_init_f32(alwan_rgb_fit_params_f32 *params);
void alwan_rgb_fit_params_init_f64(alwan_rgb_fit_params_f64 *params);

/* Cache the dataset (linear RGB triplets in data_space, `stride` bytes apart) and set the
 * starting point. space and tf NULL: the analytic start from the data (enclosing triangle,
 * weighted white, exponent by a bracketed search, scale from the largest channel value).
 * Given: the warm start; a given space with no scale starts at scale 1. */
alwan_status alwan_rgb_fit_begin_f32(alwan_rgb_fit_state_f32 **state, alwan_f32 const *data, size_t stride, size_t count, alwan_rgb_space_desc_f32 const *data_space, alwan_rgb_space_desc_f32 const *space, alwan_fit_tf_f32 const *tf, alwan_rgb_fit_params_f32 const *params, alwan_ctx *ctx);
alwan_status alwan_rgb_fit_begin_f64(alwan_rgb_fit_state_f64 **state, alwan_f64 const *data, size_t stride, size_t count, alwan_rgb_space_desc_f64 const *data_space, alwan_rgb_space_desc_f64 const *space, alwan_fit_tf_f64 const *tf, alwan_rgb_fit_params_f64 const *params, alwan_ctx *ctx);

/* One move. Writes the best space so far and its true report on every call; never worse
 * than the previous call. report may be NULL. */
alwan_status alwan_rgb_fit_step_f32(alwan_rgb_space_desc_f32 *space, alwan_fit_tf_f32 *tf, alwan_rgb_fit_report_f32 *report, alwan_rgb_fit_state_f32 *state);
alwan_status alwan_rgb_fit_step_f64(alwan_rgb_space_desc_f64 *space, alwan_fit_tf_f64 *tf, alwan_rgb_fit_report_f64 *report, alwan_rgb_fit_state_f64 *state);

/* Rebuild the simplex around the current best with new move sizes (step_log_gamma also
 * moves log scale), keeping the dataset cache: converge coarse, tighten, go again. */
alwan_status alwan_rgb_fit_restart_f32(alwan_rgb_fit_state_f32 *state, alwan_f32 step_xy, alwan_f32 step_log_gamma);
alwan_status alwan_rgb_fit_restart_f64(alwan_rgb_fit_state_f64 *state, alwan_f64 step_xy, alwan_f64 step_log_gamma);

void alwan_rgb_fit_end_f32(alwan_rgb_fit_state_f32 *state);
void alwan_rgb_fit_end_f64(alwan_rgb_fit_state_f64 *state);

/* begin, step until converged or max_iterations, end. space and tf are in-out: a zeroed
 * descriptor starts from the data, a filled one is the warm start. With params->tf = AUTO
 * both branches are solved and sRGB is kept unless the power wins by srgb_margin. */
alwan_status alwan_rgb_fit_solve_f32(alwan_rgb_space_desc_f32 *space, alwan_fit_tf_f32 *tf, alwan_rgb_fit_report_f32 *report, alwan_f32 const *data, size_t stride, size_t count, alwan_rgb_space_desc_f32 const *data_space, alwan_rgb_fit_params_f32 const *params, int max_iterations, alwan_ctx *ctx);
alwan_status alwan_rgb_fit_solve_f64(alwan_rgb_space_desc_f64 *space, alwan_fit_tf_f64 *tf, alwan_rgb_fit_report_f64 *report, alwan_f64 const *data, size_t stride, size_t count, alwan_rgb_space_desc_f64 const *data_space, alwan_rgb_fit_params_f64 const *params, int max_iterations, alwan_ctx *ctx);

/* The fit for a block-compressed texture.
 *
 * A block format does not store a colour per texel. It stores two endpoints for a tile and an
 * index per texel choosing among a few colours evenly spaced between them, so a tile is forced
 * onto a line segment: BC1 is two RGB565 endpoints per 4x4 tile and a 2-bit index, which is
 * params.bits_channel = {5,6,5}, block_size = 4, index_bits = 2.
 *
 * This exists because the ordinary fit is the wrong objective for such a format. Measured
 * through a real BC1 codec, two thirds to nine tenths of its error is the index rather than the
 * endpoint quantisation, so a fit that only makes endpoints land better works on the smaller
 * half. The index error is not a constant either: it is how far a tile's colours sit off the
 * line through them, and a different space is a different linear map, so the residual moves
 * with it.
 *
 * The objective here is the codec itself, run on a subsample of tiles, with no smooth surrogate
 * in between: the palette assignment is a nearest-of-few choice, so the objective is piecewise
 * constant and only usable because it is averaged over thousands of texels. The number reported
 * is therefore the number minimised. It costs more per iteration than the cloud fit.
 *
 * image is width*height interleaved triplets in data_space, tightly packed. */
alwan_status alwan_rgb_fit_blocks_solve_f32(alwan_rgb_space_desc_f32 *space, alwan_fit_tf_f32 *tf, alwan_rgb_fit_report_f32 *report, alwan_f32 const *image, int width, int height, alwan_rgb_space_desc_f32 const *data_space, alwan_rgb_fit_params_f32 const *params, int max_iterations, alwan_ctx *ctx);
alwan_status alwan_rgb_fit_blocks_solve_f64(alwan_rgb_space_desc_f64 *space, alwan_fit_tf_f64 *tf, alwan_rgb_fit_report_f64 *report, alwan_f64 const *image, int width, int height, alwan_rgb_space_desc_f64 const *data_space, alwan_rgb_fit_params_f64 const *params, int max_iterations, alwan_ctx *ctx);

/* Score any space on the same block round trip, fitted or not: the baseline comparison. */
alwan_status alwan_rgb_fit_blocks_evaluate_f32(alwan_rgb_fit_report_f32 *report, alwan_rgb_space_desc_f32 const *space, alwan_fit_tf_f32 const *tf, alwan_f32 const *image, int width, int height, alwan_rgb_space_desc_f32 const *data_space, alwan_rgb_fit_params_f32 const *params, alwan_ctx *ctx);
alwan_status alwan_rgb_fit_blocks_evaluate_f64(alwan_rgb_fit_report_f64 *report, alwan_rgb_space_desc_f64 const *space, alwan_fit_tf_f64 const *tf, alwan_f64 const *image, int width, int height, alwan_rgb_space_desc_f64 const *data_space, alwan_rgb_fit_params_f64 const *params, alwan_ctx *ctx);

/* Score any space on the dataset, fitted or not: the baseline comparison. */
alwan_status alwan_rgb_fit_evaluate_f32(alwan_rgb_fit_report_f32 *report, alwan_rgb_space_desc_f32 const *space, alwan_fit_tf_f32 const *tf, alwan_f32 const *data, size_t stride, size_t count, alwan_rgb_space_desc_f32 const *data_space, alwan_rgb_fit_params_f32 const *params, alwan_ctx *ctx);
alwan_status alwan_rgb_fit_evaluate_f64(alwan_rgb_fit_report_f64 *report, alwan_rgb_space_desc_f64 const *space, alwan_fit_tf_f64 const *tf, alwan_f64 const *data, size_t stride, size_t count, alwan_rgb_space_desc_f64 const *data_space, alwan_rgb_fit_params_f64 const *params, alwan_ctx *ctx);

/* PURE-corrected formation: COMPLETE_HEMI_LOOK's reconstruction with the purity rolloff gated by
 * emission evidence (junction P_T) -- "PURE = emission only". A brightly LIT SURFACE keeps its
 * chroma (no pastel collapse); additive/emissive records (glow, veil, flare -- dark-channel lift)
 * still roll to the white-infinity. Carrier and hue are EXACTLY those of COMPLETE_HEMI_LOOK
 * (the gate only scales chroma), so ORDER/MONO/DRIFT/RATIO are inherited unchanged; below the
 * purity shelf the picture is COMPLETE_HEMI_LOOK exactly. Interleaved RGB, W*H*3. */
alwan_status alwan_picture_form_pure_exp_f32(alwan_f32 *out, alwan_f32 const *in, int width, int height, alwan_ctx *ctx);
alwan_status alwan_picture_form_pure_exp_f64(alwan_f64 *out, alwan_f64 const *in, int width, int height, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Spectral Upsampling - RGB to Spectrum Conversion
 * ---------------------------------------------------------------- */

/* Smits1999: RGB to spectrum conversion using basis spectra mixing
 * Reference: Smits, Brian. "An RGB to Spectrum Conversion for Reflectances" (1999)
 * out_spd: output spectral power distribution (wavelength range: 380-720nm, 10 samples)
 * rgb: input RGB values (assumed to be in sRGB colorspace, clamped to [0, 1])
 * ctx: context (for allocation)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
alwan_status alwan_rgb_to_spectrum_smits1999_f64(alwan_spd_f64 *out_spd, alwan_rgb_f64 const *rgb, alwan_ctx *ctx);
alwan_status alwan_rgb_to_spectrum_smits1999_f32(alwan_spd_f32 *out_spd, alwan_rgb_f32 const *rgb, alwan_ctx *ctx);

/* The same recovery over a buffer: in holds three values per pixel, out holds
 * band_count, both strided in bytes. band_count follows the same in-and-out
 * rule as the other bulk upsamplers below. */
alwan_status alwan_rgb_to_spectrum_smits1999_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, size_t *band_count);
alwan_status alwan_rgb_to_spectrum_smits1999_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, size_t *band_count);

/* Mallett2019: RGB to spectrum conversion using spectral primary decomposition
 * Reference: Mallett & Yuksel. "Spectral Primary Decomposition for Rendering with sRGB Reflectance" (2019)
 * out_spd: output spectral power distribution (wavelength range: 380-780nm, 81 samples at 5nm intervals)
 * rgb: input RGB values (assumed to be in sRGB colorspace)
 * ctx: context (for allocation)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure */
alwan_status alwan_rgb_to_spectrum_mallett2019_f64(alwan_spd_f64 *out_spd, alwan_rgb_f64 const *rgb, alwan_ctx *ctx);
alwan_status alwan_rgb_to_spectrum_mallett2019_f32(alwan_spd_f32 *out_spd, alwan_rgb_f32 const *rgb, alwan_ctx *ctx);

/* The same recovery over a buffer, which is how an image is upsampled: in holds
 * three values per pixel and out holds band_count, both strided in bytes. The
 * basis is resolved once for the whole call rather than per pixel, and nothing
 * is allocated, so this is the form to use when there is more than one colour.
 *
 * band_count is in and out. With out NULL it receives the method's fixed sample
 * count and nothing is written, which is how a caller sizes the buffer; with
 * out set it must equal that count or the call is ALWAN_E_INVALID. The count
 * belongs to the method, so it is reported even where the basis was compiled
 * out, and only a call that would write returns ALWAN_E_NODATA there. */
alwan_status alwan_rgb_to_spectrum_mallett2019_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, size_t *band_count);
alwan_status alwan_rgb_to_spectrum_mallett2019_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, size_t *band_count);

/* Otsu2018: XYZ to reflectance by clustered basis functions
 * Reference: Otsu, Yamamoto and Hachisuka. "Reproducing Spectral Reflectances
 * from Tristimulus Colours" (2018)
 *
 * Takes XYZ, not RGB. A decision tree over CIE xy picks one of eight clusters,
 * and the reflectance is that cluster's mean plus a weighted sum of its three
 * basis functions. There is no RGB space to assume, so none is named.
 *
 * Output: 36 samples, 380-730nm at 10nm, clamped to [0, 1] as the reference does.
 * The XYZ is taken on the Y = 1 scale and under D65, which is what the embedded
 * cluster matrices were built for.
 *
 * Returns ALWAN_E_NODATA when the Otsu tables are compiled out. */
alwan_status alwan_xyz_to_spectrum_otsu2018_f64(alwan_spd_f64 *out_spd, alwan_xyz_f64 const *xyz, alwan_ctx *ctx);
alwan_status alwan_xyz_to_spectrum_otsu2018_f32(alwan_spd_f32 *out_spd, alwan_xyz_f32 const *xyz, alwan_ctx *ctx);

/* The same recovery over a buffer: in holds XYZ per pixel, out holds
 * band_count, both strided in bytes. band_count follows the same in-and-out
 * rule as the other bulk upsamplers above. */
alwan_status alwan_xyz_to_spectrum_otsu2018_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, size_t *band_count);
alwan_status alwan_xyz_to_spectrum_otsu2018_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, size_t *band_count);

/* Jakob2019 gamut enum - specifies which RGB color space to use for spectral upsampling */
typedef enum {
	ALWAN_JAKOB2019_SRGB = 0,        /* sRGB (standard RGB, Rec.709 primaries) */
	ALWAN_JAKOB2019_PROPHOTO_RGB = 1, /* ProPhoto RGB (wide gamut) */
	ALWAN_JAKOB2019_ACES2065_1 = 2, /* ACES2065-1 (Academy Color Encoding System) */
	ALWAN_JAKOB2019_REC2020 = 3, /* Rec.2020 (ITU-R BT.2020, HDR/UHD TV) */
	ALWAN_JAKOB2019_ERGB = 4, /* Extended RGB */
	ALWAN_JAKOB2019_XYZ = 5 /* CIE XYZ */
} alwan_jakob2019_gamut;

/* Jakob2019: RGB to spectrum using polynomial LUT
 * Reference: Jakob & Hanika. "A Low-Dimensional Function Space for Efficient Spectral Upsampling" (2019)
 * out_spd: output spectral power distribution (wavelength range: 360-780nm, 85 samples at 5nm intervals)
 * gamut: RGB color space / gamut to use for upsampling
 * rgb: input RGB values (in the specified gamut, clamped to [0, 1])
 * ctx: context (for allocation)
 * Returns ALWAN_OK on success, ALWAN_E_NOMEM on allocation failure, ALWAN_E_INVALID if gamut is invalid
 * Note: Requires pre-generated LUT data for the specified gamut (see generate_data.ps1) */
alwan_status alwan_rgb_to_spectrum_jakob2019_f64(alwan_spd_f64 *out_spd, alwan_jakob2019_gamut gamut, alwan_rgb_f64 const *rgb, alwan_ctx *ctx);
alwan_status alwan_rgb_to_spectrum_jakob2019_f32(alwan_spd_f32 *out_spd, alwan_jakob2019_gamut gamut, alwan_rgb_f32 const *rgb, alwan_ctx *ctx);

/* The same recovery over a buffer: in holds three values per pixel, out holds
 * band_count, both strided in bytes. The gamut's three cubes are resolved once
 * for the whole call rather than per pixel, which is the reason to prefer this
 * form for an image. band_count follows the same in-and-out rule as the other
 * bulk upsamplers, and with out NULL the gamut is not consulted at all. */
alwan_status alwan_rgb_to_spectrum_jakob2019_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, size_t *band_count, alwan_jakob2019_gamut gamut);
alwan_status alwan_rgb_to_spectrum_jakob2019_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, size_t *band_count, alwan_jakob2019_gamut gamut);

/* The _ex forms of the four upsamplers: void pointers and an alwan_pixel_format on
 * each side, so an image pipeline hands u8, u16, f16, f32 or f64 colours in and takes
 * spectra back in the format the renderer keeps, without converting either buffer.
 * Strides in bytes. Format order is (out_fmt, in_fmt), as the other _map_interleave_ex.
 *
 * They keep the bulk forms' band_count contract: out NULL reports the method's band
 * count, a wrong one is ALWAN_E_INVALID. Both formats f32 runs the f32 bulk form, both
 * f64 the f64 one, and either result is that form's to the bit; any other pair tiles
 * through f64 when either side is f64, f32 otherwise, and the result is the tiled
 * precision's bulk form converted at the edges. A single-precision build tiles through
 * the precision it has. A tile holds fewer pixels the more bands there are, 72 at 85
 * bands, so no buffer leaves the stack. The tristimulus direction is
 * alwan_spectral_to_tristimulus_map_interleave_ex, declared with the weights. */
alwan_status alwan_rgb_to_spectrum_smits1999_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, size_t *band_count);
alwan_status alwan_rgb_to_spectrum_mallett2019_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, size_t *band_count);
alwan_status alwan_xyz_to_spectrum_otsu2018_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, size_t *band_count);
alwan_status alwan_rgb_to_spectrum_jakob2019_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, size_t *band_count, alwan_jakob2019_gamut gamut);

/* ----------------------------------------------------------------
 * Film: spectral negative, print and reversal stocks
 *
 * A photographic stock as its datasheet describes it, profiled into tables a
 * pixel pipeline can read, and the pipeline itself: a scene spectrum exposes
 * three layers, each layer's characteristic curve turns log exposure into a dye
 * amount, the dyes and the base absorb the printer light, the print stock goes
 * through its own curve, and the projection light through the print's dyes
 * gives XYZ. Every spectrum in this section is on ALWAN_FILM_BANDS samples,
 * 380-780 nm at 10 nm, the grid the stocks were profiled on; resample with
 * alwan_spd_resample, or hand a Mallett 2019 spectrum (81 samples at 5 nm) to
 * the map form, which reads every second band.
 *
 * The profiles are spectral_film_lut's (Jan Lohse, MIT), digitised from the
 * manufacturers' sheets and profiled by its FilmSpectral: sensitivities
 * extrapolated and resampled, characteristic curves extended past the sheet
 * with a logistic roll-off and resampled to a uniform 1024-point log exposure
 * grid with D-min removed, status densities unmixed into per-layer activations,
 * dyes normalised so a unit activation reads 1 in its own status channel, the
 * 18 percent grey placed at 12.5 / ISO lux-seconds. alwan ships that output,
 * not the digitisation, and suite 158 holds this pipeline to that model's over
 * the same tables. The model is float32 throughout, so agreement is within
 * 1.1e-06 relative over nine cases and not to the bit. Grain, halation and
 * the interlayer diffusion are spatial and are not here.
 *
 * The catalogue is the whole of what the package exports at the pinned commit:
 * 87 profiles, a development-time, push or grade variant being its own curve
 * and so its own profile. Three of them carry a feature of the profiling worth
 * knowing before reading their tables, the model's own output, shipped as it
 * is: Verita 5206's adjusted base spectrum dips to -0.66 at a band, Vericolor
 * III's dye density to -0.49, and 5247's colour-masking matrix has a third
 * row of -9.3, -45.0, 55.3, so with masking on that layer's exposure noise is
 * amplified some fifty times (blue speckle in a frame's shadows). A caller
 * who wants 5247 without that passes masking = 0 to alwan_film_develop, or
 * zeroes color_masking in the profile before the look or render reads it.
 *
 * Every stock is a switch under ALWAN_TABLES_FILM; a stock compiled out gives
 * ALWAN_E_NODATA from alwan_film_get_profile and nothing else is reachable.
 * ---------------------------------------------------------------- */

enum { ALWAN_FILM_BANDS = 41, ALWAN_FILM_CURVE_COUNT = 1024 };

typedef enum {
    ALWAN_FILM_NONE = -1,                      /* no stock: a look with no print stage */
    ALWAN_FILM_KODAK_5203 = 0,                 /* Kodak Vision3 50D 5203, negative, ISO 50 */
    ALWAN_FILM_KODAK_5207 = 1,                 /* Kodak Vision3 250D 5207, negative, ISO 250 */
    ALWAN_FILM_KODAK_5213 = 2,                 /* Kodak Vision3 200T 5213, negative, ISO 200 */
    ALWAN_FILM_KODAK_5219 = 3,                 /* Kodak Vision3 500T 5219, negative, ISO 500 */
    ALWAN_FILM_KODAK_PORTRA_400 = 4,           /* Kodak Portra 400, negative, ISO 400 */
    ALWAN_FILM_KODAK_EKTAR_100 = 5,            /* Kodak Ektar 100, negative, ISO 100 */
    ALWAN_FILM_FUJI_ETERNA_500 = 6,            /* Fuji Eterna 500, negative, ISO 500 */
    ALWAN_FILM_KODAK_5222 = 7,                 /* Kodak 5222, black and white negative, ISO 250 */
    ALWAN_FILM_KODAK_2383 = 8,                 /* Kodak Vision 2383, print, no ISO */
    ALWAN_FILM_KODAK_2393 = 9,                 /* Kodak Vision Premier 2393, print, no ISO */
    ALWAN_FILM_FUJI_3513DI = 10,               /* Fuji Eterna-CP Type 3513DI, print, no ISO */
    ALWAN_FILM_KODAK_2302 = 11,                /* Kodak 2302, black and white print, no ISO */
    ALWAN_FILM_KODAK_EKTACHROME_100D = 12,     /* Kodak Ektachrome 100D, reversal, ISO 100 */
    ALWAN_FILM_FUJI_VELVIA_50 = 13,            /* Fuji Velvia 50, reversal, ISO 50 */
    ALWAN_FILM_AGFA_VISTA_100 = 14,            /* Agfa Vista 100, negative, ISO 100 */
    ALWAN_FILM_FUJI_C200 = 15,                 /* Fuji C200, negative, ISO 200 */
    ALWAN_FILM_FUJI_ETERNA_500_VIVID = 16,     /* Fuji Eterna 500 Vivid, negative, ISO 500 */
    ALWAN_FILM_FUJI_NATURA_1600 = 17,          /* Fuji Natura 1600, negative, ISO 1600 */
    ALWAN_FILM_FUJI_PRO_160C = 18,             /* Fuji Pro 160C, negative, ISO 160 */
    ALWAN_FILM_FUJI_PRO_160S = 19,             /* Fuji Pro 160S, negative, ISO 160 */
    ALWAN_FILM_FUJI_PRO_400H = 20,             /* Fuji Pro 400H, negative, ISO 400 */
    ALWAN_FILM_FUJI_SUPERIA_REALA = 21,        /* Fuji Superia Reala, negative, ISO 100 */
    ALWAN_FILM_FUJI_SUPERIA_XTRA_400 = 22,     /* Fuji Superia X-Tra 400, negative, ISO 400 */
    ALWAN_FILM_KODAK_5206 = 23,                /* Kodak Verita 200D 5206, negative, ISO 200 */
    ALWAN_FILM_KODAK_5247 = 24,                /* Kodak 5247, negative, ISO 16 */
    ALWAN_FILM_KODAK_5247_II = 25,             /* Kodak 5247 II, negative, ISO 100 */
    ALWAN_FILM_KODAK_5247_II_ALT = 26,         /* Kodak 5247 II Alt, negative, ISO 125 */
    ALWAN_FILM_KODAK_5248 = 27,                /* Kodak 5248, negative, ISO 25 */
    ALWAN_FILM_KODAK_EXR_5248 = 28,            /* Kodak EXR 100T 5248, negative, ISO 100 */
    ALWAN_FILM_KODAK_5250 = 29,                /* Kodak 5250, negative, ISO 50 */
    ALWAN_FILM_KODAK_5277 = 30,                /* Kodak Vision 320T 5277, negative, ISO 320 */
    ALWAN_FILM_KODAK_5293 = 31,                /* Kodak EXR 200T 5293, negative, ISO 250 */
    ALWAN_FILM_KODAK_AEROCOLOR = 32,           /* Kodak Aerocolor IV 2460, negative, ISO 125 */
    ALWAN_FILM_KODAK_AEROCOLOR_LOW = 33,       /* Kodak Aerocolor IV 2460 Low, negative, ISO 125 */
    ALWAN_FILM_KODAK_AEROCOLOR_HIGH = 34,      /* Kodak Aerocolor IV 2460 High, negative, ISO 125 */
    ALWAN_FILM_KODAK_GOLD_200 = 35,            /* Kodak Gold 200, negative, ISO 200 */
    ALWAN_FILM_KODAK_PORTRA_160 = 36,          /* Kodak Portra 160, negative, ISO 160 */
    ALWAN_FILM_KODAK_PORTRA_800 = 37,          /* Kodak Portra 800, negative, ISO 800 */
    ALWAN_FILM_KODAK_PORTRA_800_AT_1600 = 38,  /* Kodak Portra 800 @1600, negative, ISO 800 */
    ALWAN_FILM_KODAK_PORTRA_800_AT_3200 = 39,  /* Kodak Portra 800 @3200, negative, ISO 800 */
    ALWAN_FILM_KODAK_ULTRAMAX_400 = 40,        /* Kodak Ultramax 400, negative, ISO 400 */
    ALWAN_FILM_KODAK_VERICOLOR_III = 41,       /* Kodak Vericolor III, negative, ISO 160 */
    ALWAN_FILM_KODAK_5222_DEV_4 = 42,          /* Kodak 5222 Dev 4, black and white negative, ISO 250 */
    ALWAN_FILM_KODAK_5222_DEV_5 = 43,          /* Kodak 5222 Dev 5, black and white negative, ISO 250 */
    ALWAN_FILM_KODAK_5222_DEV_9 = 44,          /* Kodak 5222 Dev 9, black and white negative, ISO 250 */
    ALWAN_FILM_KODAK_5222_DEV_12 = 45,         /* Kodak 5222 Dev 12, black and white negative, ISO 250 */
    ALWAN_FILM_KODAK_TRI_X_400 = 46,           /* Kodak Tri-X 400, black and white negative, ISO 400 */
    ALWAN_FILM_KODAK_TRI_X_400_DEV_7 = 47,     /* Kodak Trix-X 400 Dev 7, black and white negative, ISO 400 */
    ALWAN_FILM_KODAK_TRI_X_400_DEV_9 = 48,     /* Kodak Trix-X 400 Dev 9, black and white negative, ISO 400 */
    ALWAN_FILM_KODAK_TRI_X_400_DEV_11 = 49,    /* Kodak Trix-X 400 Dev 11, black and white negative, ISO 400 */
    ALWAN_FILM_FUJI_3523XD = 50,               /* Fuji Eterna-CP 3523XD, print, no ISO */
    ALWAN_FILM_FUJI_CA_DPII = 51,              /* Fuji Crystal Archive DPII, print, no ISO */
    ALWAN_FILM_FUJI_CA_MAXIMA = 52,            /* Fuji Crystal Archive Maxima, print, no ISO */
    ALWAN_FILM_FUJI_CA_PRO_PDII = 53,          /* Fuji Crystal Archive Pro PDII, print, no ISO */
    ALWAN_FILM_FUJI_CA_SUPER_C = 54,           /* Fuji Crystal Archive Super Type C, print, no ISO */
    ALWAN_FILM_FUJIFLEX_NEW = 55,              /* Fujiflex Crystal Archive New Version, print, no ISO */
    ALWAN_FILM_FUJIFLEX_OLD = 56,              /* Fujiflex Crystal Archive Old Version, print, no ISO */
    ALWAN_FILM_KODAK_5381 = 57,                /* Kodak 5381, print, no ISO */
    ALWAN_FILM_KODAK_5383 = 58,                /* Kodak 5383, print, no ISO */
    ALWAN_FILM_KODAK_5384 = 59,                /* Kodak 5384, print, no ISO */
    ALWAN_FILM_KODAK_DURAFLEX_PLUS = 60,       /* Kodak Duraflex Plus, print, no ISO */
    ALWAN_FILM_KODAK_ENDURA_PREMIER = 61,      /* Kodak Endura Premier Paper, print, no ISO */
    ALWAN_FILM_KODAK_EXR_5386 = 62,            /* Kodak EXR 5386, print, no ISO */
    ALWAN_FILM_KODAK_PORTRA_ENDURA = 63,       /* Kodak Portra Endura Paper, print, no ISO */
    ALWAN_FILM_KODAK_SUPRA_ENDURA = 64,        /* Kodak Supra Endura Paper, print, no ISO */
    ALWAN_FILM_KODAK_2302_DEV_2 = 65,          /* Kodak 2302 Dev 2, black and white print, no ISO */
    ALWAN_FILM_KODAK_2302_DEV_3 = 66,          /* Kodak 2302 Dev 3, black and white print, no ISO */
    ALWAN_FILM_KODAK_2302_DEV_5 = 67,          /* Kodak 2302 Dev 5, black and white print, no ISO */
    ALWAN_FILM_KODAK_2302_DEV_7 = 68,          /* Kodak 2302 Dev 7, black and white print, no ISO */
    ALWAN_FILM_KODAK_2302_DEV_9 = 69,          /* Kodak 2302 Dev 9, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX = 70,             /* Kodak Professional Polymax Fine-Art Paper, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX_GRADE_MINUS_1 = 71, /* Kodak Polymax Fine-Art Paper Grade -1, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX_GRADE_0 = 72,     /* Kodak Polymax Fine-Art Paper Grade 0, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX_GRADE_1 = 73,     /* Kodak Polymax Fine-Art Paper Grade 1, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX_GRADE_2 = 74,     /* Kodak Polymax Fine-Art Paper Grade 2, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX_GRADE_3 = 75,     /* Kodak Polymax Fine-Art Paper Grade 3, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX_GRADE_4 = 76,     /* Kodak Polymax Fine-Art Paper Grade 4, black and white print, no ISO */
    ALWAN_FILM_KODAK_POLYMAX_GRADE_5 = 77,     /* Kodak Polymax Fine-Art Paper Grade 5, black and white print, no ISO */
    ALWAN_FILM_FUJI_FP100C = 78,               /* Fuji FP-100C, reversal, ISO 128 */
    ALWAN_FILM_FUJI_INSTAX_COLOR = 79,         /* Fuji Instax color, reversal, ISO 1000 */
    ALWAN_FILM_FUJI_PROVIA_100F = 80,          /* Fuji Provia 100F, reversal, ISO 100 */
    ALWAN_FILM_KODACHROME_64 = 81,             /* Kodachrome 64, reversal, ISO 64 */
    ALWAN_FILM_KODAK_AEROCHROME_III = 82,      /* Kodak Aerochrome III Infrared Film 1443, reversal, ISO 40 */
    ALWAN_FILM_KODAK_EKTACHROME_100D_ALT = 83, /* Kodak Ektachrome 100D alt., reversal, ISO 100 */
    ALWAN_FILM_ILFOCHROME_MICROGRAPHIC_M = 84, /* Ilfochrome Micrographic M, reversal print, no ISO */
    ALWAN_FILM_ILFOCHROME_MICROGRAPHIC_P = 85, /* Ilfochrome Micrographic P, reversal print, no ISO */
    ALWAN_FILM_KODAK_EKTACHROME_RADIANCE_III = 86, /* Kodak Ektachrome Radiance III Paper, reversal print, no ISO */
    ALWAN_FILM_STOCK_COUNT
} alwan_film_stock;

/* ISO 5-3 status densitometry and the ACES printing density. */
typedef enum {
    ALWAN_FILM_STATUS_A = 0,
    ALWAN_FILM_STATUS_M = 1,
    ALWAN_FILM_STATUS_APD = 2
} alwan_film_status;

/* A profiled stock. The pointers are into the embedded table and stay valid for
 * the life of the process; nothing is allocated. Spectral tables are
 * wavelength-major, three values per wavelength: sensitivity[k * 3 + c] is layer
 * c at 380 + 10 k nm. Filled by alwan_film_get_profile; a caller may also build
 * one by hand from their own tables, and every function checks the pointers.
 * A one-layer stock (black and white) stores its one column
 * three times and says so in layers; every function here reads column 0 of it
 * and repeats the answer into the other two. The characteristic curve is
 * density_curve[i * 3 + c], activation of layer c at log exposure
 * log_h_min + i (log_h_max - log_h_min) / (curve_count - 1), read linearly and
 * held flat outside. The masking matrix acts on log exposure, row-major, as the
 * interlayer inhibition at the stock's default strength, and is the identity
 * where the sheet gives none. */
typedef struct {
    alwan_film_stock stock;
    char const *name;
    char const *manufacturer;
    int year;
    int layers;            /* 3, or 1 for black and white */
    int is_print;          /* a print stock: exposed by a printer light, not a scene */
    int is_positive;       /* reversal: a positive image on the camera stock */
    alwan_f64 iso;         /* 0 where the sheet gives none */
    alwan_f64 exposure_kelvin;
    alwan_f64 log_h_min;
    alwan_f64 log_h_max;
    size_t curve_count;    /* ALWAN_FILM_CURVE_COUNT, the one grid size the reader takes */
    alwan_f64 log_h_ref[3];   /* log10 exposure of the 18 percent grey, per layer */
    alwan_f64 d_ref[3];       /* its activation */
    alwan_f64 d_min[3];       /* status D-min the sheet reported */
    alwan_f64 d_max[3];
    alwan_f64 gamma;
    alwan_f64 color_masking;  /* default strength; 0 means the matrix is the identity */
    alwan_f64 rms_granularity;
    alwan_f64 masking[9];
    alwan_f64 const *sensitivity;     /* [ALWAN_FILM_BANDS * 3], linear */
    alwan_f64 const *dye_density;     /* [ALWAN_FILM_BANDS * 3], per unit activation */
    alwan_f64 const *d_min_spectral;  /* [ALWAN_FILM_BANDS], base plus fog */
    alwan_f64 const *density_curve;   /* [curve_count * 3] */
} alwan_film_profile_f64;

typedef struct {
    alwan_film_stock stock;
    char const *name;
    char const *manufacturer;
    int year;
    int layers;
    int is_print;
    int is_positive;
    alwan_f32 iso;
    alwan_f32 exposure_kelvin;
    alwan_f32 log_h_min;
    alwan_f32 log_h_max;
    size_t curve_count;
    alwan_f32 log_h_ref[3];
    alwan_f32 d_ref[3];
    alwan_f32 d_min[3];
    alwan_f32 d_max[3];
    alwan_f32 gamma;
    alwan_f32 color_masking;
    alwan_f32 rms_granularity;
    alwan_f32 masking[9];
    alwan_f32 const *sensitivity;
    alwan_f32 const *dye_density;
    alwan_f32 const *d_min_spectral;
    alwan_f32 const *density_curve;
} alwan_film_profile_f32;

/* Name and maker of a stock, whether or not its table is compiled in.
 * ALWAN_E_INVALID for a value outside the enum. Either output may be NULL. */
alwan_status alwan_film_stock_info(alwan_film_stock stock, char const **name, char const **manufacturer);

/* The profile. ALWAN_E_INVALID for an unknown stock, ALWAN_E_NODATA for one
 * compiled out. */
alwan_status alwan_film_get_profile_f64(alwan_film_profile_f64 *out, alwan_film_stock stock);
alwan_status alwan_film_get_profile_f32(alwan_film_profile_f32 *out, alwan_film_stock stock);

/* Exposure. Layer c receives H_c = factor_c sum_k spd_k balance_k S_kc over the
 * 41 bands, a plain sum with no bandwidth, and log_h_out[c] = log10 of it floored
 * at 1e-16. balance is an optional per-band factor, the ratio of the reference
 * daylight spectrum to the scene white's, which is how the model white balances
 * the stock; NULL is no balance. factors are the per-layer scale that put the
 * grey at the stock's reference exposure, from alwan_film_calibrate; NULL is 1.
 * A one-layer stock fills all three outputs with its one layer. */
alwan_status alwan_film_expose_f64(alwan_f64 log_h_out[3], alwan_film_profile_f64 const *profile, alwan_f64 const *spd, alwan_f64 const *balance, alwan_f64 const *factors);
alwan_status alwan_film_expose_f32(alwan_f32 log_h_out[3], alwan_film_profile_f32 const *profile, alwan_f32 const *spd, alwan_f32 const *balance, alwan_f32 const *factors);

/* The factors that expose grey_spd, the scene's 18 percent grey, at exactly
 * log_h_ref on every layer: 10^log_h_ref divided by the unscaled exposure.
 * ALWAN_E_RANGE if the grey exposes a layer to nothing. */
alwan_status alwan_film_calibrate_f64(alwan_f64 factors_out[3], alwan_film_profile_f64 const *profile, alwan_f64 const *grey_spd, alwan_f64 const *balance);
alwan_status alwan_film_calibrate_f32(alwan_f32 factors_out[3], alwan_film_profile_f32 const *profile, alwan_f32 const *grey_spd, alwan_f32 const *balance);

/* Development: log exposure to per-layer activation through the characteristic
 * curve. With masking nonzero and three layers the masking matrix is applied to
 * the log exposures first, which is the interlayer inhibition of a colour
 * negative; a print stock or a one-layer stock ignores it. Outside the curve's
 * grid the end value holds. */
alwan_status alwan_film_develop_f64(alwan_f64 density_out[3], alwan_film_profile_f64 const *profile, alwan_f64 const log_h[3], int masking);
alwan_status alwan_film_develop_f32(alwan_f32 density_out[3], alwan_film_profile_f32 const *profile, alwan_f32 const log_h[3], int masking);

/* Spectral transmittance of the developed stock, 41 values:
 * 10^-(sum_c density_c dye_kc + d_min_k). The dye term is not clamped here;
 * alwan_film_project clamps it at zero, as the reference does, so a negative
 * activation cannot brighten the projection. */
alwan_status alwan_film_transmittance_f64(alwan_f64 *transmittance_out, alwan_film_profile_f64 const *profile, alwan_f64 const density[3]);
alwan_status alwan_film_transmittance_f32(alwan_f32 *transmittance_out, alwan_film_profile_f32 const *profile, alwan_f32 const density[3]);

/* The printer light, 41 values, that prints the negative's reference grey onto
 * the print stock at the print's reference exposure: the three printer lights
 * (the ACES printing density printer light split into red, green and blue)
 * mixed by a 3 x 3 solve through the grey's transmittance and the print's
 * sensitivities. red, green and blue are offsets in stops on the three lights,
 * 0 for the neutral print. A one-layer print stock gets a flat light set by
 * green alone. ALWAN_E_NODATA without the printer light table, ALWAN_E_RANGE if
 * the solve is singular. */
alwan_status alwan_film_printer_light_f64(alwan_f64 *light_out, alwan_film_profile_f64 const *negative, alwan_film_profile_f64 const *print, alwan_f64 red, alwan_f64 green, alwan_f64 blue);
alwan_status alwan_film_printer_light_f32(alwan_f32 *light_out, alwan_film_profile_f32 const *negative, alwan_film_profile_f32 const *print, alwan_f32 red, alwan_f32 green, alwan_f32 blue);

/* Printing: the developed negative's transmittance times the printer light,
 * integrated against the print stock's sensitivities, floored at 1e-5, through
 * the print's characteristic curve. No masking on a print. */
alwan_status alwan_film_print_f64(alwan_f64 density_out[3], alwan_film_profile_f64 const *negative, alwan_f64 const density[3], alwan_film_profile_f64 const *print, alwan_f64 const *light);
alwan_status alwan_film_print_f32(alwan_f32 density_out[3], alwan_film_profile_f32 const *negative, alwan_f32 const density[3], alwan_film_profile_f32 const *print, alwan_f32 const *light);

/* Projection or viewing: the developed stock's transmittance, dye term clamped
 * at zero, times the light, against the CIE 1931 2 degree observer at the 41
 * bands, as a plain sum. XYZ is in the light's units: a light with Y = 1 through
 * clear film gives Y = 1. ALWAN_E_NODATA without the observer table. */
alwan_status alwan_film_project_f64(alwan_xyz_f64 *xyz_out, alwan_film_profile_f64 const *profile, alwan_f64 const density[3], alwan_f64 const *light);
alwan_status alwan_film_project_f32(alwan_xyz_f32 *xyz_out, alwan_film_profile_f32 const *profile, alwan_f32 const density[3], alwan_f32 const *light);

/* ISO 5-3 status density of a transmittance: -log10 of it integrated against
 * the Status A, Status M or ACES printing density responsivities, whose columns
 * sum to 1 so clear film reads 0. Status A for prints and reversals, Status M
 * for negatives, as the sheets are measured. */
alwan_status alwan_film_status_density_f64(alwan_f64 density_out[3], alwan_f64 const *transmittance, alwan_film_status status);
alwan_status alwan_film_status_density_f32(alwan_f32 density_out[3], alwan_f32 const *transmittance, alwan_film_status status);

/* The whole trip for one spectrum: expose, develop (masking at the negative's
 * default strength), print if print is not NULL with printer_light, and project
 * the last stock with projection_light. printer_light may be NULL when print is.
 * factors and balance as alwan_film_expose. */
alwan_status alwan_film_render_f64(alwan_xyz_f64 *xyz_out, alwan_film_profile_f64 const *negative, alwan_film_profile_f64 const *print, alwan_f64 const *spd, alwan_f64 const *balance, alwan_f64 const *factors, alwan_f64 const *printer_light, alwan_f64 const *projection_light);
alwan_status alwan_film_render_f32(alwan_xyz_f32 *xyz_out, alwan_film_profile_f32 const *negative, alwan_film_profile_f32 const *print, alwan_f32 const *spd, alwan_f32 const *balance, alwan_f32 const *factors, alwan_f32 const *printer_light, alwan_f32 const *projection_light);

/* The same over a buffer: band_count samples per pixel in (41 on the film grid,
 * or 81 for a Mallett 2019 spectrum, of which every second band is read), XYZ
 * out, both strided in bytes. */
alwan_status alwan_film_render_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, size_t band_count, alwan_film_profile_f64 const *negative, alwan_film_profile_f64 const *print, alwan_f64 const *balance, alwan_f64 const *factors, alwan_f64 const *printer_light, alwan_f64 const *projection_light);
alwan_status alwan_film_render_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, size_t band_count, alwan_film_profile_f32 const *negative, alwan_film_profile_f32 const *print, alwan_f32 const *balance, alwan_f32 const *factors, alwan_f32 const *printer_light, alwan_f32 const *projection_light);

/* A look: the whole chain from scene-linear RGB footage, set up once and applied
 * to a buffer, so a caller never touches a spectrum.
 *
 * The input is scene-linear RGB AS SHOT, with the grey card at 0.18: that is
 * the calibration point, and nothing else is taken from the picture. The
 * exposure lesson behind this is worth stating: anchoring a frame's median or
 * mean at grey pushes a low-key scene by stops and makes any film look
 * overexposed, while the film itself was fine. Push or pull deliberately with
 * `stops`, or take the scale from the camera's exposure through the ISO 12232
 * model; do not take it from a statistic of the frame. Do not tone-map first
 * either: the negative's and the print's curves are the picture formation, and
 * the highlights above white are what the shoulder is for.
 *
 * Each pixel is scaled into the unit cube by m = max(1, max channel), upsampled
 * with Jakob 2019 in `gamut` (the space the RGB is in), scaled back by m and lit
 * by `light`, the scene's illuminant normalised to Y = 1, so a highlight above
 * white keeps its spectrum and its exposure. The negative is calibrated on 0.18
 * of that light; with balance_on_grey the calibration is per layer, which is
 * what an 85 filter does for a tungsten stock under daylight, and without it
 * one scale serves all three layers and the stock shows its cast. The print
 * gets the neutral printer light plus the three offsets in stops, and the last
 * stock is projected under the same light. The output is XYZ relative to that
 * stock's clear base: Y = 1 is paper white, ready for a display encode. */
typedef struct {
    alwan_film_stock negative;
    alwan_film_stock print;          /* ALWAN_FILM_NONE projects the negative or reversal itself */
    alwan_illuminant light;          /* the scene's light and the projector's; D65 by default */
    alwan_jakob2019_gamut gamut;     /* the RGB space of the input, for the upsampling */
    alwan_f64 stops;                 /* push (+) or pull (-) in stops; 0 is as shot */
    alwan_f64 red, green, blue;      /* printer light offsets in stops; 0 is the neutral print */
    int balance_on_grey;             /* 1: calibrate each layer on the grey; 0: one scale, the stock's balance shows */
} alwan_film_look;

/* D65, sRGB input, as shot, neutral print, balanced. print may be ALWAN_FILM_NONE. */
alwan_status alwan_film_look_default(alwan_film_look *look, alwan_film_stock negative, alwan_film_stock print);

/* Scene-linear RGB in, three values per pixel, XYZ out, both strided in bytes.
 * The ctx is for the illuminant table read; NULL is the default allocator.
 * ALWAN_E_NODATA when a stock, the illuminant, the observer or the gamut's
 * coefficient cubes are compiled out. */
alwan_status alwan_film_render_rgb_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_render_rgb_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_film_render_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_render_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);

/* The look in two halves, for a spatial operator between them. alwan_film_look_expose
 * stops at the negative's linear layer exposures, three per pixel, calibrated and
 * pushed: the light each layer received, before the log. alwan_film_look_finish
 * takes those exposures through the log, the masking, the curves, the print and the
 * projection. Back to back they are alwan_film_render_rgb to the bit; with
 * alwan_film_halation in between they are the exposed negative with light that
 * came back from the base. */
alwan_status alwan_film_look_expose_f64_map_interleave(alwan_f64 *exposure_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_expose_f32_map_interleave(alwan_f32 *exposure_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_film_look_expose_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_expose_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_finish_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *exposure_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_finish_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *exposure_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_film_look_finish_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_finish_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);

/* finish in two, for an operator on the developed negative: alwan_film_look_develop
 * takes linear exposures to the negative's layer activations (the log, the masking
 * and the curves), alwan_film_look_print takes activations through the print and
 * the projection. Back to back they are alwan_film_look_finish to the bit; with
 * alwan_film_grain_density between them they are the grainy negative printed. */
alwan_status alwan_film_look_develop_f64_map_interleave(alwan_f64 *density_out, size_t out_stride, alwan_f64 const *exposure_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_develop_f32_map_interleave(alwan_f32 *density_out, size_t out_stride, alwan_f32 const *exposure_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_film_look_develop_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_develop_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_print_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_f64 const *density_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_print_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_f32 const *density_in, size_t in_stride, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_film_look_print_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);
alwan_status alwan_film_look_print_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_film_look const *look, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * EXPERIMENTAL: film halation (src/alwan/experimental/alwan_film_halation.c)
 *
 * Light that exposes the emulsion goes on into the base, reflects off its rear
 * surface and comes back up to expose the emulsion again, a distance away. On
 * a stock with no anti-halation layer that is the glow around a bright source,
 * and it is red first, because the red-sensitive layer is the one nearest the
 * base. The kernel here is derived rather than drawn: light entering the base
 * with a Lambertian distribution, reflected at the base-to-air interface with
 * the unpolarised Fresnel reflectance, totally reflected past the critical
 * angle asin(1 / n), and returning at r = 2 d tan(theta) for a base of
 * thickness d and index n, gives
 *
 *   K(r) proportional to R(theta(r)) / (1 + (r / 2d)^2)^2,  tan(theta) = r / 2d
 *
 * which is a disc suppressed by the Fresnel reflectance (4 % at the centre for
 * n = 1.5), a sharp rim where total internal reflection starts, at
 * r_c = 2 d / sqrt(n^2 - 1), and a (2d / r)^4 tail beyond it. The rim radius in
 * pixels is the parameter, not the base thickness; it carries the thickness,
 * the pixel pitch and the film format in one number. The kernel is normalised to
 * sum 1 over its support, so each layer's strength is the fraction of its
 * exposure that comes back: H_c := H_c + strength_c (K * H_c), on linear
 * exposure, before development, with the edges replicated.
 *
 * NOT TESTABLE AGAINST A REFERENCE. No installed library implements this, so
 * suite 159 pins what can be pinned: the kernel's energy, symmetry, the rim at
 * r_c and the Fresnel suppression inside it, a point source becoming the kernel
 * exactly, and a flat field staying flat. Written from: the Fresnel and Snell
 * relations (Hecht, Optics, 5th ed., sections 4.6 and 4.7); the derivation in
 * jeremieLouvaert/ComfyUI-Darkroom, docs/halation-derivation.md
 * (https://github.com/jeremieLouvaert/ComfyUI-Darkroom), reproduced from the
 * physics rather than its code; and the per-layer additive form of
 * thatcherfreeman/utility-dctls, Effects/Halation.dctl, MIT
 * (https://github.com/thatcherfreeman/utility-dctls). Everything under
 * experimental/ is research code: constants may be inlined and results may
 * change between releases.
 * ---------------------------------------------------------------- */
typedef struct {
    alwan_f64 rim_radius;    /* pixels: r_c, where total internal reflection starts */
    alwan_f64 index;         /* refractive index of the base: 1.50 for PET, 1.48 for triacetate */
    alwan_f64 reach;         /* kernel support as a multiple of rim_radius; 3 by default */
    alwan_f64 strength[3];   /* fraction of each layer's exposure that comes back: red, green, blue */
} alwan_film_halation_params;

/* PET base, reach 3, strengths 0.08, 0.03, 0.01: red more than green more than
 * blue, as the layer order and the anti-halation dye make it. rim_radius must
 * be positive. */
alwan_status alwan_film_halation_params_default(alwan_film_halation_params *params, alwan_f64 rim_radius);

/* The kernel, (2 R + 1)^2 values row-major with R = ceil(reach * rim_radius),
 * summing to 1. kernel_out NULL reports R alone. */
alwan_status alwan_film_halation_kernel_f64(alwan_f64 *kernel_out, size_t *radius_out, alwan_film_halation_params const *params);
alwan_status alwan_film_halation_kernel_f32(alwan_f32 *kernel_out, size_t *radius_out, alwan_film_halation_params const *params);

/* In place on a linear exposure image, three values per pixel, rows row_stride
 * bytes apart. A direct convolution: width x height x (2 R + 1)^2 per layer, so a
 * rim of 8 pixels at reach 3 is 2,401 taps a pixel. */
alwan_status alwan_film_halation_f64(alwan_f64 *exposure, size_t row_stride, size_t width, size_t height, alwan_film_halation_params const *params, alwan_ctx *ctx);
alwan_status alwan_film_halation_f32(alwan_f32 *exposure, size_t row_stride, size_t width, size_t height, alwan_film_halation_params const *params, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * EXPERIMENTAL: film grain (src/alwan/experimental/alwan_film_grain.c)
 *
 * Newson, Faraj, Galerne and Delon's Boolean grain model: a developed layer is
 * a field of opaque discs, the dye clouds, whose centres are a Poisson process
 * and whose radii are log-normal, and a pixel's value is the fraction of its
 * area they cover. For coverage u the process intensity is
 *
 *   lambda(u) = -ln(1 - u) / (pi (r^2 + sigma_r^2))
 *
 * so the expected coverage is u itself: the model is mean-preserving, and the
 * grain is what the discreteness of the discs adds. It is rendered by Monte
 * Carlo, as the paper does: each output pixel takes `samples` points, each
 * jittered by a Gaussian of `filter_sigma`, and counts how many land inside a
 * disc; the discs of an input cell are drawn from a generator seeded by the
 * cell, the channel and `seed`, so the field is the same however the pixels
 * are visited and the same on every run. The radius is in pixels; smaller is
 * finer and dearer, since a cell near white holds ln(10^4) / (pi r^2) discs.
 *
 * alwan_film_grain renders any image in [0, 1], channels independent, with
 * the discs as large as the radius makes them. That is the paper's use, on a
 * picture. It is NOT how a colour negative's density is built, and the film
 * route below does not stack it through Nutting's relation: a layer's density
 * is thousands of dye clouds a fraction of a micrometre across, weakly
 * absorbing, through the depth of the emulsion, and one opaque disc field per
 * layer at D = 1 comes out a hundred times grainier than any datasheet.
 *
 * alwan_film_grain_density is that limit. Stack K thin Boolean fields whose
 * transmittances multiply and let K grow: ln T becomes a sum of many small
 * independent terms, Gaussian, with variance D ln10 pi r^2 / A over a sampling
 * area A, which is Selwyn's law, sigma_D^2 A proportional to D. The sheet's
 * RMS granularity is exactly that constant measured at D = 1 through a 48 um
 * aperture, so the size comes from the sheet and the pixel pitch on the film,
 * with no radius to choose:
 *
 *   sigma_D(pixel) = (rms / 1000) sqrt(D) sqrt(A_48 / pitch^2),  A_48 = pi 24^2 um^2
 *
 * added per layer and pixel as Gaussian noise on D from a stream seeded by
 * (seed, pixel, layer). Vision3 500T's 4.4 at a 4K scan of a 35 mm frame
 * (5.9 um a pixel) is 0.032 at D = 1. The mean density is kept exactly.
 *
 * NOT TESTABLE AGAINST A REFERENCE. The published implementation renders with
 * its own generator and sampling order, so no two implementations agree pixel
 * for pixel and none should be expected to. Suite 160 pins the models' own
 * statements: zero stays zero, a flat field keeps its mean, the same seed
 * gives the same bits, more samples give less noise, bigger discs are coarser;
 * and for the density route, the spread the formula gives to 5 %, Selwyn's
 * square root in D, the inverse pitch, a layer scale of zero leaving a layer
 * untouched. Written from: A. Newson, J. Delon, B. Galerne, "A Stochastic
 * Film Grain Model for Resolution-Independent Rendering", Computer Graphics
 * Forum 36(8), 2017; A. Newson, N. Faraj, B. Galerne, J. Delon, "Realistic
 * Film Grain Rendering", IPOL 7, 2017, https://doi.org/10.5201/ipol.2017.192
 * (the algorithm and its pseudo-code; the generator here is its own); and for
 * the limit, E. W. H. Selwyn, "A Theory of Graininess", Photographic Journal
 * 75, 1935, and J. C. Dainty and R. Shaw, "Image Science", Academic Press
 * 1974, chapter 8. Research code: results may change.
 * ---------------------------------------------------------------- */
typedef struct {
    alwan_f64 radius;        /* mean disc radius in pixels; 0.5 by default */
    alwan_f64 radius_sigma;  /* standard deviation of the log-normal radius; 0 for one size */
    alwan_f64 filter_sigma;  /* Gaussian jitter of each sample, in pixels; 0.8 by default */
    size_t samples;          /* Monte Carlo samples per pixel; 64 by default, more is smoother */
    unsigned long long seed; /* the field; the same seed is the same grain */
} alwan_film_grain_params;

alwan_status alwan_film_grain_params_default(alwan_film_grain_params *params, alwan_f64 radius);

/* In place on an image in [0, 1], `channels` (1 to 4) values per pixel, rows
 * row_stride bytes apart. Values outside [0, 1] are clamped first. */
alwan_status alwan_film_grain_f64(alwan_f64 *image, size_t row_stride, size_t width, size_t height, int channels, alwan_film_grain_params const *params, alwan_ctx *ctx);
alwan_status alwan_film_grain_f32(alwan_f32 *image, size_t row_stride, size_t width, size_t height, int channels, alwan_film_grain_params const *params, alwan_ctx *ctx);

typedef struct {
    alwan_f64 rms_granularity;   /* the sheet's number: RMS density x 1000 at D = 1, 48 um aperture; alwan_film_profile.rms_granularity */
    alwan_f64 pixel_pitch;       /* micrometres on the film: 5.9 for a 35 mm frame at 4K, 2.8 for Super 8 at 2K */
    alwan_f64 layer_scale[3];    /* per-layer multiplier on the spread; 1, 1, 1 by default */
    unsigned long long seed;
} alwan_film_grain_density_params;

/* rms_granularity and pixel_pitch positive; scales 1, seed 1. */
alwan_status alwan_film_grain_density_params_default(alwan_film_grain_density_params *params, alwan_f64 rms_granularity, alwan_f64 pixel_pitch);

/* In place on a negative's layer activations, three per pixel, rows row_stride
 * bytes apart: Selwyn grain, the mean density kept exactly, D held at zero
 * where it was zero. */
alwan_status alwan_film_grain_density_f64(alwan_f64 *density, size_t row_stride, size_t width, size_t height, alwan_film_grain_density_params const *params, alwan_ctx *ctx);
alwan_status alwan_film_grain_density_f32(alwan_f32 *density, size_t row_stride, size_t width, size_t height, alwan_film_grain_density_params const *params, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * CIECAM02 Color Appearance Model
 * ---------------------------------------------------------------- */

/* CIECAM02 forward transform: XYZ -> appearance correlates
 * xyz: input color in XYZ (same white point as viewing conditions)
 * vc: viewing conditions
 * out: output appearance correlates (J, C, h, s, Q, M, H)
 * Returns ALWAN_OK on success */
alwan_status alwan_ciecam02_forward_f32(alwan_ciecam02_correlates_f32 *out,
                                alwan_xyz_f32 const *xyz,
                                alwan_ciecam02_viewing_conditions_f32 const *vc);
alwan_status alwan_ciecam02_forward_f64(alwan_ciecam02_correlates_f64 *out,
                                alwan_xyz_f64 const *xyz,
                                alwan_ciecam02_viewing_conditions_f64 const *vc);

/* CIECAM02 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
alwan_status alwan_ciecam02_inverse_f32(alwan_xyz_f32 *xyz_out,
                                alwan_ciecam02_correlates_f32 const *correlates,
                                alwan_ciecam02_viewing_conditions_f32 const *vc);
alwan_status alwan_ciecam02_inverse_f64(alwan_xyz_f64 *xyz_out,
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
alwan_status alwan_cam16_forward_f32(alwan_cam16_correlates_f32 *out,
                             alwan_xyz_f32 const *xyz,
                             alwan_cam16_viewing_conditions_f32 const *vc);
alwan_status alwan_cam16_forward_f64(alwan_cam16_correlates_f64 *out,
                             alwan_xyz_f64 const *xyz,
                             alwan_cam16_viewing_conditions_f64 const *vc);

/* CAM16 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from input correlates (other fields are ignored)
 * correlates: input appearance correlates (J, C, h must be valid)
 * vc: viewing conditions
 * xyz_out: output color in XYZ
 * Returns ALWAN_OK on success */
alwan_status alwan_cam16_inverse_f32(alwan_xyz_f32 *xyz_out,
                             alwan_cam16_correlates_f32 const *correlates,
                             alwan_cam16_viewing_conditions_f32 const *vc);
alwan_status alwan_cam16_inverse_f64(alwan_xyz_f64 *xyz_out,
                             alwan_cam16_correlates_f64 const *correlates,
                             alwan_cam16_viewing_conditions_f64 const *vc);

/* CIECAM16 forward transform: XYZ -> appearance correlates (CIE 248:2022).
 *
 * The same seven correlates as CAM16, computed the same way apart from two things the
 * standard changes. The post-adaptation compression is linear below 0.26 and linear above
 * 150 instead of a bare power curve, which is what keeps very dark and very bright signals
 * behaving; and the adaptation term divides by a fixed 100 rather than by the white's own
 * Y. Inside that window, and with a white on the Y = 100 scale, CIECAM16 and CAM16 agree
 * to the bit, so this is worth reaching for at the extremes rather than everywhere.
 *
 * The surround factors CIE 248:2022 tabulates are the ones CAM16 uses, so
 * alwan_cam16_surround is shared rather than duplicated.
 *
 * Returns ALWAN_OK, ALWAN_E_INVALID on a NULL argument, or ALWAN_E_DIVZERO if the white's
 * Y or the background luminance is not positive. */
alwan_status alwan_ciecam16_forward_f32(alwan_ciecam16_correlates_f32 *out,
                             alwan_xyz_f32 const *xyz,
                             alwan_ciecam16_viewing_conditions_f32 const *vc);
alwan_status alwan_ciecam16_forward_f64(alwan_ciecam16_correlates_f64 *out,
                             alwan_xyz_f64 const *xyz,
                             alwan_ciecam16_viewing_conditions_f64 const *vc);

/* CIECAM16 inverse transform: appearance correlates -> XYZ
 * Uses J, C, h from the input correlates; the other fields are ignored. */
alwan_status alwan_ciecam16_inverse_f32(alwan_xyz_f32 *xyz_out,
                             alwan_ciecam16_correlates_f32 const *correlates,
                             alwan_ciecam16_viewing_conditions_f32 const *vc);
alwan_status alwan_ciecam16_inverse_f64(alwan_xyz_f64 *xyz_out,
                             alwan_ciecam16_correlates_f64 const *correlates,
                             alwan_ciecam16_viewing_conditions_f64 const *vc);

/* sCAM forward transform: XYZ -> appearance correlates (Li and Luo 2024).
 *
 * sCAM is assembled from parts alwan already has rather than built from scratch: the
 * stimulus is adapted to a D65 white by the Li 2025 model (alwan_cat_li2025), converted
 * through sUCS (alwan_xyz_to_sucs), and the correlates are computed from that.
 *
 * Ten correlates. J is the paper's I_a. V, K, W and D are vividness, blackness, whiteness
 * and depth, which no other model here reports. colour-science's specification also lists
 * HC, a hue composition string it never fills in, and that is not carried.
 *
 * Returns ALWAN_OK, ALWAN_E_INVALID on a NULL argument, or ALWAN_E_DIVZERO if the white's
 * Y or the background luminance is not positive. */
alwan_status alwan_scam_forward_f32(alwan_scam_correlates_f32 *out,
                             alwan_xyz_f32 const *xyz,
                             alwan_scam_viewing_conditions_f32 const *vc);
alwan_status alwan_scam_forward_f64(alwan_scam_correlates_f64 *out,
                             alwan_xyz_f64 const *xyz,
                             alwan_scam_viewing_conditions_f64 const *vc);

/* sCAM inverse transform: appearance correlates -> XYZ
 * Uses J, C and h from the input correlates; the other fields are ignored. */
alwan_status alwan_scam_inverse_f32(alwan_xyz_f32 *xyz_out,
                             alwan_scam_correlates_f32 const *correlates,
                             alwan_scam_viewing_conditions_f32 const *vc);
alwan_status alwan_scam_inverse_f64(alwan_xyz_f64 *xyz_out,
                             alwan_scam_correlates_f64 const *correlates,
                             alwan_scam_viewing_conditions_f64 const *vc);

/* MapCIECAM02 forward transform
 * correlates_out: output appearance correlates (count elements)
 * xyz_in: input XYZ colors (stride in_stride between consecutive colors)
 * in_stride: input stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * vc: viewing conditions
 * count: number of colors to process
 * Returns ALWAN_OK on success */
alwan_status alwan_ciecam02_forward_f32_map_interleave(alwan_ciecam02_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_ciecam02_forward_f64_map_interleave(alwan_ciecam02_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count);

/* MapCIECAM02 inverse transform
 * xyz_out: output XYZ colors (stride out_stride between consecutive colors)
 * out_stride: output stride in bytes (typically 3*sizeof(alwan_f32/alwan_f64))
 * correlates_in: input appearance correlates (count elements)
 * vc: viewing conditions
 * count: number of colors to process
 * Returns ALWAN_OK on success */
alwan_status alwan_ciecam02_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f32 const *correlates_in, alwan_ciecam02_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_ciecam02_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f64 const *correlates_in, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count);

/* MapCAM16 forward transform */
alwan_status alwan_cam16_forward_f32_map_interleave(alwan_cam16_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_cam16_forward_f64_map_interleave(alwan_cam16_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f64 const *vc, size_t count);

/* MapCAM16 inverse transform */
alwan_status alwan_cam16_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_cam16_correlates_f32 const *correlates_in, alwan_cam16_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_cam16_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_cam16_correlates_f64 const *correlates_in, alwan_cam16_viewing_conditions_f64 const *vc, size_t count);

/* Typed CAM map functions (_ex variants) */
alwan_status alwan_ciecam02_forward_map_interleave_ex(alwan_ciecam02_correlates_f64 *correlates_out, void const *xyz_in, size_t in_stride, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format in_fmt);
alwan_status alwan_ciecam02_inverse_map_interleave_ex(void *xyz_out, size_t out_stride, alwan_ciecam02_correlates_f64 const *correlates_in, alwan_ciecam02_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format out_fmt);
alwan_status alwan_cam16_forward_map_interleave_ex(alwan_cam16_correlates_f64 *correlates_out, void const *xyz_in, size_t in_stride, alwan_cam16_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format in_fmt);
alwan_status alwan_cam16_inverse_map_interleave_ex(void *xyz_out, size_t out_stride, alwan_cam16_correlates_f64 const *correlates_in, alwan_cam16_viewing_conditions_f64 const *vc, size_t count, alwan_pixel_format out_fmt);

/* CAM16-UCS (Uniform Color Space) transform for perceptual distance metrics
 * Converts CAM16 JMh to CAM16-UCS Jab for computing perceptual distances
 * correlates: input CAM16 correlates (J, M, h used)
 * Jab_out: output CAM16-UCS coordinates [J', a', b']
 * Returns ALWAN_OK on success */
alwan_status alwan_cam16_to_ucs_f32(alwan_cam_jab_f32 *Jab_out,
                            alwan_cam16_correlates_f32 const *correlates);
alwan_status alwan_cam16_to_ucs_f64(alwan_cam_jab_f64 *Jab_out,
                            alwan_cam16_correlates_f64 const *correlates);

/* Inverse CAM16-UCS transform
 * Converts CAM16-UCS Jab back to CAM16 JMh
 * Jab: input CAM16-UCS coordinates [J', a', b']
 * correlates_out: output CAM16 correlates (J, M, h filled; other fields set to 0)
 * Returns ALWAN_OK on success */
alwan_status alwan_cam16_from_ucs_f32(alwan_cam16_correlates_f32 *correlates_out,
                              alwan_cam_jab_f32 const *Jab);
alwan_status alwan_cam16_from_ucs_f64(alwan_cam16_correlates_f64 *correlates_out,
                              alwan_cam_jab_f64 const *Jab);

/* ----------------------------------------------------------------
 * ZCAM - HDR Color Appearance Model
 * Safdar, Hardeberg and Luo (2021), Optics Express 29(4), 6036, as
 * colour-science's XYZ_to_ZCAM and ZCAM_to_XYZ. Built on Izazbz in its Safdar
 * 2021 form, NOT on Jzazbz: I_z = M' - epsilon.
 * Supports HDR luminance range 0.001-10,000 cd/m^2
 *
 * Viewing conditions: xyz_w is the absolute white under the viewing illuminant,
 * La the adapting luminance in cd/m^2, Yb the luminance factor of the
 * background on the scale of xyz_w.y. The surround selects F_s and F together:
 * average 0.69 and 1.0, dim 0.59 and 0.9, dark 0.525 and 0.8. The stimulus is
 * adapted to D65 first, by the Zhai 2018 two-step CAT over CAT02 with the
 * model's degree of adaptation D(F, La), or D = 1 when discount_illuminant is
 * set. The white itself is not adapted.
 *
 * OUTPUT CHANGED on 2026-09-21. Before that the forward model had never been
 * compared with a reference, and was wrong in three places: F_s was read from
 * the F column, I_z was Jzazbz's, and the adaptation to D65 was missing.
 * Lightness was off by up to 18 of 100. See alwan_zcam_core.inc.
 * ---------------------------------------------------------------- */

/* ZCAM forward transform: XYZ -> appearance correlates
 * xyz: absolute XYZ tristimulus values (cd/m^2)
 * vc: viewing conditions
 * out: computed appearance correlates
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
alwan_status alwan_zcam_forward_f32(alwan_zcam_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_zcam_viewing_conditions_f32 const *vc);
alwan_status alwan_zcam_forward_f64(alwan_zcam_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_zcam_viewing_conditions_f64 const *vc);

/* ZCAM inverse transform: appearance correlates -> XYZ
 * correlates: appearance correlates. Jz, Mz and hz are read, the rest ignored,
 *             which is what alwan_zcam_from_ucs fills
 * vc: viewing conditions, the same ones the correlates were computed under
 * xyz: output XYZ tristimulus values (cd/m^2)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments
 * Exact and closed form: in f64 it returns the stimulus to 2e-13 of the white's
 * luminance, and in f32, whose correlates carry seven digits, to 4e-8 of it.
 * It was labelled approximate until 2026-09-21
 * because it was not the model's inverse at all. */
alwan_status alwan_zcam_inverse_f32(alwan_xyz_f32 *xyz,
                            alwan_zcam_correlates_f32 const *correlates,
                            alwan_zcam_viewing_conditions_f32 const *vc);
alwan_status alwan_zcam_inverse_f64(alwan_xyz_f64 *xyz,
                            alwan_zcam_correlates_f64 const *correlates,
                            alwan_zcam_viewing_conditions_f64 const *vc);

/* ZCAM over a buffer. XYZ triples with a byte stride on the XYZ side, one
 * correlates struct per pixel on the other, as the CAM16 maps. The white's
 * response, the CAT gains and the viewing-condition powers are computed once
 * per call rather than per pixel, in the arithmetic the one-colour form uses,
 * so a map and its scalar twin agree to the bit. The f32 maps compute in f64
 * and narrow, as the f32 scalars do. */
alwan_status alwan_zcam_forward_f32_map_interleave(alwan_zcam_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_zcam_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_zcam_forward_f64_map_interleave(alwan_zcam_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_zcam_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan_zcam_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_zcam_correlates_f32 const *correlates_in, alwan_zcam_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_zcam_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_zcam_correlates_f64 const *correlates_in, alwan_zcam_viewing_conditions_f64 const *vc, size_t count);

/* ZCAM to UCS (Uniform Color Space) for color difference
 * correlates: input ZCAM correlates
 * Jab_out: output ZCAM-UCS coordinates [Jz, az, bz]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
alwan_status alwan_zcam_to_ucs_f32(alwan_jzazbz_f32 *Jab_out,
                           alwan_zcam_correlates_f32 const *correlates);
alwan_status alwan_zcam_to_ucs_f64(alwan_jzazbz_f64 *Jab_out,
                           alwan_zcam_correlates_f64 const *correlates);

/* ZCAM-UCS back to correlates, the inverse of alwan_zcam_to_ucs
 * Jab: input ZCAM-UCS coordinates [Jz, az, bz]
 * correlates_out: output ZCAM correlates (Jz, Mz, hz filled; other fields set to 0)
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
alwan_status alwan_zcam_from_ucs_f32(alwan_zcam_correlates_f32 *correlates_out,
                             alwan_jzazbz_f32 const *Jab);
alwan_status alwan_zcam_from_ucs_f64(alwan_zcam_correlates_f64 *correlates_out,
                             alwan_jzazbz_f64 const *Jab);

/* ----------------------------------------------------------------
 * RLAB Color Appearance Model
 * Based on Fairchild (1993, 1996)
 * Cross-media color reproduction model
 * ---------------------------------------------------------------- */

/* RLAB forward transform: XYZ -> appearance correlates
 *
 * Matches colour.appearance.XYZ_to_RLAB to 2.8e-08 for every D_factor at the
 * RLAB reference adapting luminance of 318.31 cd/m^2.
 *
 * The absolute adapting luminance is vc->Y_n, in cd/m^2. RLAB's
 * incomplete-adaptation term depends on it whenever D < 1. A zero Y_n is
 * treated as the reference 318.31 cd/m^2, so a zero-initialised struct behaves
 * sensibly rather than taking a cube root of zero.
 *
 * Two defects were fixed 2026-08-27: the adaptation normalised by Y_n/Y_w
 * instead of by LMS_n, which left the reference space scaled by the white's Y
 * and returned L = 740.57 for the reference white instead of 100; and the
 * incomplete-adaptation term was absent, adaptation being blended linearly
 * toward none with (1 - D), which overpredicted the white's chroma by 4.4x at
 * D = 0.
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
alwan_status alwan_rlab_forward_f32(alwan_rlab_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_rlab_viewing_conditions_f32 const *vc);
alwan_status alwan_rlab_forward_f64(alwan_rlab_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_rlab_viewing_conditions_f64 const *vc);

/* RLAB inverse transform: appearance correlates -> XYZ
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on null arguments */
alwan_status alwan_rlab_inverse_f32(alwan_xyz_f32 *xyz,
                            alwan_rlab_correlates_f32 const *correlates,
                            alwan_rlab_viewing_conditions_f32 const *vc);
alwan_status alwan_rlab_inverse_f64(alwan_xyz_f64 *xyz,
                            alwan_rlab_correlates_f64 const *correlates,
                            alwan_rlab_viewing_conditions_f64 const *vc);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_rlab_forward_f32_map_interleave(alwan_rlab_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_rlab_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_rlab_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_rlab_correlates_f32 const *correlates_in, alwan_rlab_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_rlab_forward_f64_map_interleave(alwan_rlab_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_rlab_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan_rlab_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_rlab_correlates_f64 const *correlates_in, alwan_rlab_viewing_conditions_f64 const *vc, size_t count);

/* ----------------------------------------------------------------
 * Hunt Color Appearance Model
 * Based on Hunt (1991, 1995)
 * Comprehensive historical CAM
 * ---------------------------------------------------------------- */

/* Hunt forward transform: XYZ -> appearance correlates
 * Returns ALWAN_OK on success, or ALWAN_E_INVALID if out, xyz, or vc is NULL
 *
 * ZERO-INITIALISE THE VIEWING CONDITIONS. Hunt needs a background, a proximal
 * field, scotopic responses and two induction factors on top of the white and
 * the adapting luminance. Every one of those uses 0 as its "derive this for me"
 * sentinel, so
 *
 *     alwan_hunt_viewing_conditions_f64 vc = {0};
 *
 * and then set what you know. A struct left as stack garbage will produce
 * confident nonsense, which is exactly what happened to alwan's own f32/f64
 * twin test when the fields were added.
 *
 * Matches colour.appearance.XYZ_to_Hunt to 4.6e-08 on Fairchild's worked
 * examples. Before 2026-08-27 the chromatic adaptation was the algebraic
 * identity (D + 1 - D) * lms, the white's cone responses and both surround
 * factors were computed then discarded, the eccentricity factor was omitted and
 * brightness, lightness and chroma were ad-hoc, so every correlate was wrong by
 * one to two orders of magnitude. */
alwan_status alwan_hunt_forward_f32(alwan_hunt_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_hunt_viewing_conditions_f32 const *vc);
alwan_status alwan_hunt_forward_f64(alwan_hunt_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_hunt_viewing_conditions_f64 const *vc);

/* Hunt inverse: appearance correlates -> XYZ.
 * Returns ALWAN_OK, or ALWAN_E_INVALID if xyz, correlates or vc is NULL.
 *
 * PASS THE SAME VIEWING CONDITIONS THE FORWARD WAS GIVEN. Every derived
 * quantity is recomputed from them, so a different white, background or
 * adapting luminance answers a different question rather than failing.
 *
 * Reads J, C and h only. Q, s and M are functions of those three under the
 * given conditions, so they are recomputed rather than trusted, and a caller
 * building the struct by hand need only fill the three.
 *
 * This is closed form, not a search. The chromatic adaptation looks
 * stimulus-dependent but is not: its four per-channel parameters read the
 * white, the background and the proximal field only, so it undoes exactly, and
 * what remains reduces to one linear equation. The only iteration is a scalar
 * fixed point on the scotopic response, and it runs only when vc.S is left at
 * 0, since that is the field whose default is the stimulus Y. Round-trips the
 * sRGB gamut to 2.6e-13 in f64.
 *
 * Two limits, both inherited from clamps in the forward. A stimulus whose cone
 * response is negative is not recoverable, because the forward's f_n sends
 * every negative argument to the same value; the boundary member of that set
 * comes back instead. And a negative saturation is reported by the forward as
 * C = 0, which discards the chroma. Neither is reachable from inside a real
 * display gamut. */
alwan_status alwan_hunt_inverse_f32(alwan_xyz_f32 *xyz,
                            alwan_hunt_correlates_f32 const *correlates,
                            alwan_hunt_viewing_conditions_f32 const *vc);
alwan_status alwan_hunt_inverse_f64(alwan_xyz_f64 *xyz,
                            alwan_hunt_correlates_f64 const *correlates,
                            alwan_hunt_viewing_conditions_f64 const *vc);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_hunt_forward_f32_map_interleave(alwan_hunt_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_hunt_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_hunt_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_hunt_correlates_f32 const *correlates_in, alwan_hunt_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_hunt_forward_f64_map_interleave(alwan_hunt_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_hunt_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan_hunt_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_hunt_correlates_f64 const *correlates_in, alwan_hunt_viewing_conditions_f64 const *vc, size_t count);

/* ----------------------------------------------------------------
 * Hellwig2022 Color Appearance Model
 * Based on Hellwig and Fairchild (2022)
 * Improved CAM16 with Helmholtz-Kohlrausch effect support
 * ---------------------------------------------------------------- */

/* Hellwig2022 forward transform: XYZ -> appearance correlates */
alwan_status alwan_hellwig2022_forward_f32(alwan_hellwig2022_correlates_f32 *out,
                                   alwan_xyz_f32 const *xyz,
                                   alwan_hellwig2022_viewing_conditions_f32 const *vc);
alwan_status alwan_hellwig2022_forward_f64(alwan_hellwig2022_correlates_f64 *out,
                                   alwan_xyz_f64 const *xyz,
                                   alwan_hellwig2022_viewing_conditions_f64 const *vc);

/* Hellwig2022 inverse transform: appearance correlates -> XYZ */
alwan_status alwan_hellwig2022_inverse_f32(alwan_xyz_f32 *xyz_out,
                                   alwan_hellwig2022_correlates_f32 const *correlates,
                                   alwan_hellwig2022_viewing_conditions_f32 const *vc);
alwan_status alwan_hellwig2022_inverse_f64(alwan_xyz_f64 *xyz_out,
                                   alwan_hellwig2022_correlates_f64 const *correlates,
                                   alwan_hellwig2022_viewing_conditions_f64 const *vc);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_hellwig2022_forward_f32_map_interleave(alwan_hellwig2022_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_hellwig2022_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_hellwig2022_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_hellwig2022_correlates_f32 const *correlates_in, alwan_hellwig2022_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_hellwig2022_forward_f64_map_interleave(alwan_hellwig2022_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_hellwig2022_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan_hellwig2022_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_hellwig2022_correlates_f64 const *correlates_in, alwan_hellwig2022_viewing_conditions_f64 const *vc, size_t count);

/* ----------------------------------------------------------------
 * Kim2009 Color Appearance Model
 * Based on Kim, Weyrich and Kautz (2009)
 * Specialized for rendering applications
 * ---------------------------------------------------------------- */

/* Kim2009 forward transform: XYZ -> appearance correlates
 *
 * vc->media selects the published media parameter E. It matters a great deal:
 * for XYZ = [19.01, 20, 21.78] under D65 the four media give J = 51.18, 40.56,
 * 28.86 and 14.44. Zero, from a zero-initialised struct, is High-luminance LCD
 * (E = 1.0), which is what this returned unconditionally before the field
 * existed, under a comment that called it "CRT Displays". CRT is E = 1.4572.
 *
 * Matches colour.appearance.XYZ_to_Kim2009 to 4e-11 for all four media. */
/* Kim2009 forward transform: XYZ -> appearance correlates */
alwan_status alwan_kim2009_forward_f32(alwan_kim2009_correlates_f32 *out,
                               alwan_xyz_f32 const *xyz,
                               alwan_kim2009_viewing_conditions_f32 const *vc);
alwan_status alwan_kim2009_forward_f64(alwan_kim2009_correlates_f64 *out,
                               alwan_xyz_f64 const *xyz,
                               alwan_kim2009_viewing_conditions_f64 const *vc);

/* Kim2009 inverse transform: appearance correlates -> XYZ */
alwan_status alwan_kim2009_inverse_f32(alwan_xyz_f32 *xyz_out,
                               alwan_kim2009_correlates_f32 const *correlates,
                               alwan_kim2009_viewing_conditions_f32 const *vc);
alwan_status alwan_kim2009_inverse_f64(alwan_xyz_f64 *xyz_out,
                               alwan_kim2009_correlates_f64 const *correlates,
                               alwan_kim2009_viewing_conditions_f64 const *vc);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_kim2009_forward_f32_map_interleave(alwan_kim2009_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_kim2009_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_kim2009_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_kim2009_correlates_f32 const *correlates_in, alwan_kim2009_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_kim2009_forward_f64_map_interleave(alwan_kim2009_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_kim2009_viewing_conditions_f64 const *vc, size_t count);
alwan_status alwan_kim2009_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_kim2009_correlates_f64 const *correlates_in, alwan_kim2009_viewing_conditions_f64 const *vc, size_t count);

/* ----------------------------------------------------------------
 * LLAB Color Appearance Model
 * Based on Luo, Lo and Kuo (1996)
 * Cross-media color reproduction model
 * ---------------------------------------------------------------- */

/* LLAB forward transform: XYZ -> appearance correlates */
alwan_status alwan_llab_forward_f32(alwan_llab_correlates_f32 *out,
                            alwan_xyz_f32 const *xyz,
                            alwan_llab_viewing_conditions_f32 const *vc);
alwan_status alwan_llab_forward_f64(alwan_llab_correlates_f64 *out,
                            alwan_xyz_f64 const *xyz,
                            alwan_llab_viewing_conditions_f64 const *vc);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_llab_forward_f32_map_interleave(alwan_llab_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_llab_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_llab_forward_f64_map_interleave(alwan_llab_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_llab_viewing_conditions_f64 const *vc, size_t count);

/* ----------------------------------------------------------------
 * ATD95 Color Vision Model
 * Based on Guth's ATD (1995)
 * Advanced temporal dynamics model
 * ---------------------------------------------------------------- */

/* ATD95 forward transform: XYZ -> correlates
 *
 * H is the RATIO T_2/D_2 as the model defines it, not an angle. It is
 * unbounded and may be negative; do not expect [0, 360).
 *
 * Matches colour.appearance.XYZ_to_ATD95 to 4.3e-10 on Fairchild's worked
 * examples. Two defects were fixed 2026-08-27: the cone gain was applied
 * outside the 0.7 power instead of inside it, and H was returned as
 * degrees(atan2(T_2, D_2)) wrapped into [0, 360). */
alwan_status alwan_atd95_forward_f32(alwan_atd95_correlates_f32 *out,
                             alwan_xyz_f32 const *xyz,
                             alwan_atd95_viewing_conditions_f32 const *vc);
alwan_status alwan_atd95_forward_f64(alwan_atd95_correlates_f64 *out,
                             alwan_xyz_f64 const *xyz,
                             alwan_atd95_viewing_conditions_f64 const *vc);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_atd95_forward_f32_map_interleave(alwan_atd95_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_atd95_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_atd95_forward_f64_map_interleave(alwan_atd95_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_atd95_viewing_conditions_f64 const *vc, size_t count);

/* ----------------------------------------------------------------
 * Nayatani95 Color Appearance Model
 * Based on Nayatani et al. (1995)
 * Japanese color appearance model
 * ---------------------------------------------------------------- */

/* Nayatani95 forward transform: XYZ -> appearance correlates.
 * XYZ and the reference white are in the [0, 100] domain. Viewing conditions:
 * background luminance factor Y_0, field illuminance E_0 (lux), and normalising
 * illuminance E_0r (lux); the noise term is fixed at the model default (1.0).
 * Forward-only. Validated against colour-science colour.XYZ_to_Nayatani95. */
alwan_status alwan_nayatani95_forward_f32(alwan_nayatani95_correlates_f32 *out,
                                  alwan_xyz_f32 const *xyz,
                                  alwan_nayatani95_viewing_conditions_f32 const *vc);
alwan_status alwan_nayatani95_forward_f64(alwan_nayatani95_correlates_f64 *out,
                                  alwan_xyz_f64 const *xyz,
                                  alwan_nayatani95_viewing_conditions_f64 const *vc);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_nayatani95_forward_f32_map_interleave(alwan_nayatani95_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_nayatani95_viewing_conditions_f32 const *vc, size_t count);
alwan_status alwan_nayatani95_forward_f64_map_interleave(alwan_nayatani95_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_nayatani95_viewing_conditions_f64 const *vc, size_t count);

/* ----------------------------------------------------------------
 * M9: Convenience Color Models (HSV, HSL, CMY, CMYK, YCbCr)
 * ---------------------------------------------------------------- */

/* RGB <-> HSV conversions (all values in [0, 1])
 * Operates on encoded (display-referred) sRGB. For linear input, apply sRGB OETF first. */
alwan_status alwan_rgb_to_hsv_f32(alwan_hsv_f32 *hsv_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_hsv_f64(alwan_hsv_f64 *hsv_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hsv_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsv_f32 const *hsv);
alwan_status alwan_hsv_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsv_f64 const *hsv);

/* RGB <-> HSL conversions (all values in [0, 1])
 * Operates on encoded (display-referred) sRGB. For linear input, apply sRGB OETF first. */
alwan_status alwan_rgb_to_hsl_f32(alwan_hsl_f32 *hsl_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_hsl_f64(alwan_hsl_f64 *hsl_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hsl_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsl_f32 const *hsl);
alwan_status alwan_hsl_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsl_f64 const *hsl);

/* Linear sRGB <-> HSV conversions
 * Applies sRGB OETF/EOTF internally so the caller works in linear light. */
alwan_status alwan_linear_srgb_to_hsv_f32(alwan_hsv_f32 *hsv_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_linear_srgb_to_hsv_f64(alwan_hsv_f64 *hsv_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hsv_to_linear_srgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsv_f32 const *hsv);
alwan_status alwan_hsv_to_linear_srgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsv_f64 const *hsv);

/* Linear sRGB <-> HSL conversions
 * Applies sRGB OETF/EOTF internally so the caller works in linear light. */
alwan_status alwan_linear_srgb_to_hsl_f32(alwan_hsl_f32 *hsl_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_linear_srgb_to_hsl_f64(alwan_hsl_f64 *hsl_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hsl_to_linear_srgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsl_f32 const *hsl);
alwan_status alwan_hsl_to_linear_srgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsl_f64 const *hsl);

/*
 * RGB <-> HSP conversions (all values in [0, 1])
 * HSP: Hue, Saturation, Perceived brightness
 * Reference: Darel Rex Finley (2006), http://alienryderflex.com/hsp.html
 * P = sqrt(Pr*R^2 + Pg*G^2 + Pb*B^2) with BT.601 weights.
 * H and S identical to HSV. Used by DaVinci Resolve. */
alwan_status alwan_rgb_to_hsp_f32(alwan_hsp_f32 *hsp_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_hsp_f64(alwan_hsp_f64 *hsp_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hsp_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsp_f32 const *hsp);
alwan_status alwan_hsp_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsp_f64 const *hsp);

/*
 * RGB <-> HSPLog conversions (all values in [0, 1])
 * HSPLog: HSP with logarithmic saturation stretching.
 * S_log = log10(1 + 9*S), expanding low saturation values.
 * Designed for log/flat-encoded footage.
 * Inspired by Nobe Color Remap / DaVinci Resolve "HSP Log".
 * NOTE: No published specification exists; see alwan_types.h. */
alwan_status alwan_rgb_to_hsplog_f32(alwan_hsplog_f32 *hsplog_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_hsplog_f64(alwan_hsplog_f64 *hsplog_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hsplog_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsplog_f32 const *hsplog);
alwan_status alwan_hsplog_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsplog_f64 const *hsplog);

/*
 * RGB <-> HSY conversions (all values in [0, 1])
 * HSY: Hue, Saturation, Luma (weighted linear luma)
 * Reference: Kuzma Shapran "HCY" (chilliant.com); Krita KoColorConversions.cpp
 * Y = BT.601 weighted luma, S uses luma-aware max_sat remapping.
 * Used by DaVinci Resolve. */
alwan_status alwan_rgb_to_hsy_f32(alwan_hsy_f32 *hsy_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_hsy_f64(alwan_hsy_f64 *hsy_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hsy_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hsy_f32 const *hsy);
alwan_status alwan_hsy_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hsy_f64 const *hsy);

/* RGB <-> CMY conversions (all values in [0, 1]) */
alwan_status alwan_rgb_to_cmy_f32(alwan_cmy_f32 *cmy_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_cmy_f64(alwan_cmy_f64 *cmy_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_cmy_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_cmy_f32 const *cmy);
alwan_status alwan_cmy_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_cmy_f64 const *cmy);

/* CMY <-> CMYK conversions (all values in [0, 1]) */
alwan_status alwan_cmy_to_cmyk_f32(alwan_cmyk_f32 *cmyk_out, alwan_cmy_f32 const *cmy);
alwan_status alwan_cmy_to_cmyk_f64(alwan_cmyk_f64 *cmyk_out, alwan_cmy_f64 const *cmy);
alwan_status alwan_cmyk_to_cmy_f32(alwan_cmy_f32 *cmy_out, alwan_cmyk_f32 const *cmyk);
alwan_status alwan_cmyk_to_cmy_f64(alwan_cmy_f64 *cmy_out, alwan_cmyk_f64 const *cmyk);

/* YCbCr standard identifiers */
typedef enum {
    ALWAN_YCBCR_BT601 = 0, /* ITU-R BT.601 (SD) */
    ALWAN_YCBCR_BT709 = 1, /* ITU-R BT.709 (HD) */
    ALWAN_YCBCR_BT2020 = 2 /* ITU-R BT.2020 (UHD) */
} alwan_ycbcr_standard;

/* RGB <-> YCbCr conversions (nominal ranges: RGB [0, 1], YCbCr full range [0, 1]).
 * The decode is the RAW standard math: out-of-range excursions (super-black /
 * super-white, xvYCC) decode to values outside [0,1] and are PRESERVED -- no
 * silent gamut clamp. Use alwan_ycbcr_to_rgb_gamut_safe for guaranteed [0,1]
 * output (ALWAN_GAMUT_MAP_CLIP reproduces the pre-2.0 implicit clamping). */
alwan_status alwan_rgb_to_ycbcr_f64(alwan_ycbcr_f64 *ycbcr_out, alwan_rgb_f64 const *rgb, alwan_ycbcr_standard standard);
alwan_status alwan_rgb_to_ycbcr_f32(alwan_ycbcr_f32 *ycbcr_out, alwan_rgb_f32 const *rgb, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_ycbcr_f64 const *ycbcr, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_ycbcr_f32 const *ycbcr, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_ycbcr_f64 const *ycbcr, alwan_ycbcr_standard standard, alwan_gamut_map_method method);
alwan_status alwan_ycbcr_to_rgb_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_ycbcr_f32 const *ycbcr, alwan_ycbcr_standard standard, alwan_gamut_map_method method);

/* RGB <-> YcCbcCrc conversions (constant luminance, BT.2020)
 * bit_depth: 8, 10, 12, or 16 -- controls legal range scaling
 * The decode is raw (no gamut clamp -- see the YCbCr note above); use
 * alwan_yccbccrc_to_rgb_gamut_safe for guaranteed [0,1] linear output. */
alwan_status alwan_rgb_to_yccbccrc_f32(alwan_yccbccrc_f32 *yccbccrc_out, alwan_rgb_f32 const *rgb, int bit_depth);
alwan_status alwan_rgb_to_yccbccrc_f64(alwan_yccbccrc_f64 *yccbccrc_out, alwan_rgb_f64 const *rgb, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_yccbccrc_f32 const *yccbccrc, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_yccbccrc_f64 const *yccbccrc, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_yccbccrc_f32 const *yccbccrc, int bit_depth, alwan_gamut_map_method method);
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_yccbccrc_f64 const *yccbccrc, int bit_depth, alwan_gamut_map_method method);

/* YCbCr legal <-> full range conversion
 * Converts between full-range [0,1] and legal/narrow range with proper chroma centering.
 * bit_depth: 8, 10, 12, or 16 */
alwan_status alwan_ycbcr_full_to_legal_f32(alwan_ycbcr_f32 *out, alwan_ycbcr_f32 const *in, int bit_depth);
alwan_status alwan_ycbcr_full_to_legal_f64(alwan_ycbcr_f64 *out, alwan_ycbcr_f64 const *in, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_f32(alwan_ycbcr_f32 *out, alwan_ycbcr_f32 const *in, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_f64(alwan_ycbcr_f64 *out, alwan_ycbcr_f64 const *in, int bit_depth);

/* RGB <-> YCoCg conversions (video compression, real-time graphics)
 * - Y: luma, Co: orange chrominance, Cg: green chrominance
 * - Reversible integer transform (exact round-trip with proper scaling)
 * - Used in H.264/AVC and video codecs */
alwan_status alwan_rgb_to_ycocg_f32(alwan_ycocg_f32 *ycocg_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_ycocg_f64(alwan_ycocg_f64 *ycocg_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_ycocg_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_ycocg_f32 const *ycocg);
alwan_status alwan_ycocg_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_ycocg_f64 const *ycocg);

/* ----------------------------------------------------------------
 * Relative Luminance (Y)
 * Computes Y = kr*R + kg*G + kb*B for a given standard or color space.
 * Input RGB must be linear (scene-referred). For encoded RGB, apply EOTF first.
 * ---------------------------------------------------------------- */

/* Luma/luminance standard identifiers */
typedef enum {
    ALWAN_LUMA_BT601 = 0, /* ITU-R BT.601 (SD) */
    ALWAN_LUMA_BT709 = 1, /* ITU-R BT.709 / sRGB (HD) */
    ALWAN_LUMA_BT2020 = 2, /* ITU-R BT.2020 (UHD) */
    ALWAN_LUMA_ACES_AP1 = 3, /* ACES AP1 / ACEScg */
    ALWAN_LUMA_ACES_AP0 = 4, /* ACES AP0 / ACES2065-1 */
    ALWAN_LUMA_DISPLAY_P3 = 5, /* Display P3 / P3-D65 */
    ALWAN_LUMA_DCI_P3 = 6, /* DCI-P3 (theater) */
    ALWAN_LUMA_ADOBE_RGB = 7, /* Adobe RGB (1998) */
    ALWAN_LUMA_PROPHOTO_RGB = 8 /* ProPhoto RGB / ROMM RGB */
} alwan_luma_standard;

/* Per-pixel relative luminance from standard enum */
alwan_status alwan_relative_luminance_f64(alwan_f64 *Y_out,
                             alwan_rgb_f64 const *rgb,
                             alwan_luma_standard standard);
alwan_status alwan_relative_luminance_f32(alwan_f32 *Y_out,
                             alwan_rgb_f32 const *rgb,
                             alwan_luma_standard standard);

/* Per-pixel relative luminance from explicit coefficients */
alwan_status alwan_relative_luminance_kr_kb_f32(alwan_f32 *Y_out,
                                       alwan_rgb_f32 const *rgb,
                                       alwan_f32 kr, alwan_f32 kb);
alwan_status alwan_relative_luminance_kr_kb_f64(alwan_f64 *Y_out,
                                       alwan_rgb_f64 const *rgb,
                                       alwan_f64 kr, alwan_f64 kb);

/* Per-pixel relative luminance from RGB space descriptor (extracts Y row from NPM) */
alwan_status alwan_relative_luminance_space_f32(alwan_f32 *Y_out,
                                       alwan_rgb_f32 const *rgb,
                                       alwan_rgb_space_desc_f32 const *space);
alwan_status alwan_relative_luminance_space_f64(alwan_f64 *Y_out,
                                       alwan_rgb_f64 const *rgb,
                                       alwan_rgb_space_desc_f64 const *space);

/* Batch relative luminance (3-channel input, 1-channel output) */
alwan_status alwan_relative_luminance_f32_map_interleave(alwan_f32 *Y_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_luma_standard standard);
alwan_status alwan_relative_luminance_f64_map_interleave(alwan_f64 *Y_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_luma_standard standard);

alwan_status alwan_relative_luminance_space_f32_map_interleave(alwan_f32 *Y_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_space_desc_f32 const *space);
alwan_status alwan_relative_luminance_space_f64_map_interleave(alwan_f64 *Y_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_space_desc_f64 const *space);

/* Convenience color model conversions (map variants) */
alwan_status alwan_rgb_to_hsv_f32_map_interleave(alwan_f32 *hsv_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hsv_f64_map_interleave(alwan_f64 *hsv_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_hsv_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsv_in, size_t in_stride, size_t count);
alwan_status alwan_hsv_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsv_in, size_t in_stride, size_t count);

alwan_status alwan_rgb_to_hsl_f32_map_interleave(alwan_f32 *hsl_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hsl_f64_map_interleave(alwan_f64 *hsl_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_hsl_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsl_in, size_t in_stride, size_t count);
alwan_status alwan_hsl_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsl_in, size_t in_stride, size_t count);

/* Map HSP conversions */
alwan_status alwan_rgb_to_hsp_f32_map_interleave(alwan_f32 *hsp_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hsp_f64_map_interleave(alwan_f64 *hsp_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_hsp_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsp_in, size_t in_stride, size_t count);
alwan_status alwan_hsp_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsp_in, size_t in_stride, size_t count);

/* Map HSPLog conversions */
alwan_status alwan_rgb_to_hsplog_f32_map_interleave(alwan_f32 *hsplog_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hsplog_f64_map_interleave(alwan_f64 *hsplog_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_hsplog_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsplog_in, size_t in_stride, size_t count);
alwan_status alwan_hsplog_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsplog_in, size_t in_stride, size_t count);

/* Map HSY conversions */
alwan_status alwan_rgb_to_hsy_f32_map_interleave(alwan_f32 *hsy_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hsy_f64_map_interleave(alwan_f64 *hsy_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_hsy_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsy_in, size_t in_stride, size_t count);
alwan_status alwan_hsy_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsy_in, size_t in_stride, size_t count);

/* Typed convenience HSV/HSL map functions (_ex variants) */
alwan_status alwan_rgb_to_hsv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsv_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hsl_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsl_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Typed HSP/HSPLog/HSY map functions (_ex variants) */
alwan_status alwan_rgb_to_hsp_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsp_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hsplog_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsplog_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hsy_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsy_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map linear sRGB <-> HSV conversions */
alwan_status alwan_linear_srgb_to_hsv_f32_map_interleave(alwan_f32 *hsv_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_linear_srgb_to_hsv_f64_map_interleave(alwan_f64 *hsv_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_hsv_to_linear_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsv_in, size_t in_stride, size_t count);
alwan_status alwan_hsv_to_linear_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsv_in, size_t in_stride, size_t count);

/* Map linear sRGB <-> HSL conversions */
alwan_status alwan_linear_srgb_to_hsl_f32_map_interleave(alwan_f32 *hsl_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_linear_srgb_to_hsl_f64_map_interleave(alwan_f64 *hsl_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);

alwan_status alwan_hsl_to_linear_srgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hsl_in, size_t in_stride, size_t count);
alwan_status alwan_hsl_to_linear_srgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hsl_in, size_t in_stride, size_t count);

/* Typed linear sRGB <-> HSV/HSL map functions (_ex variants) */
alwan_status alwan_linear_srgb_to_hsv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsv_to_linear_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_linear_srgb_to_hsl_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsl_to_linear_srgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map CMY conversions */
alwan_status alwan_rgb_to_cmy_f32_map_interleave(alwan_f32 *cmy_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_cmy_f64_map_interleave(alwan_f64 *cmy_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_cmy_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *cmy_in, size_t in_stride, size_t count);
alwan_status alwan_cmy_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *cmy_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_cmy_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_cmy_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map CMYK conversions (4-channel output/input) */
alwan_status alwan_cmy_to_cmyk_f32_map_interleave(alwan_f32 *cmyk_out, size_t out_stride, alwan_f32 const *cmy_in, size_t in_stride, size_t count);
alwan_status alwan_cmy_to_cmyk_f64_map_interleave(alwan_f64 *cmyk_out, size_t out_stride, alwan_f64 const *cmy_in, size_t in_stride, size_t count);
alwan_status alwan_cmyk_to_cmy_f32_map_interleave(alwan_f32 *cmy_out, size_t out_stride, alwan_f32 const *cmyk_in, size_t in_stride, size_t count);
alwan_status alwan_cmyk_to_cmy_f64_map_interleave(alwan_f64 *cmy_out, size_t out_stride, alwan_f64 const *cmyk_in, size_t in_stride, size_t count);
alwan_status alwan_cmy_to_cmyk_f32_map_planar(alwan_f32 *out_c, size_t out_stride, alwan_f32 *out_m, alwan_f32 *out_y, alwan_f32 *out_k, alwan_f32 const *in_c, size_t in_stride, alwan_f32 const *in_m, alwan_f32 const *in_y, size_t count);
alwan_status alwan_cmy_to_cmyk_f64_map_planar(alwan_f64 *out_c, size_t out_stride, alwan_f64 *out_m, alwan_f64 *out_y, alwan_f64 *out_k, alwan_f64 const *in_c, size_t in_stride, alwan_f64 const *in_m, alwan_f64 const *in_y, size_t count);
alwan_status alwan_cmyk_to_cmy_f32_map_planar(alwan_f32 *out_c, size_t out_stride, alwan_f32 *out_m, alwan_f32 *out_y, alwan_f32 const *in_c, size_t in_stride, alwan_f32 const *in_m, alwan_f32 const *in_y, alwan_f32 const *in_k, size_t count);
alwan_status alwan_cmyk_to_cmy_f64_map_planar(alwan_f64 *out_c, size_t out_stride, alwan_f64 *out_m, alwan_f64 *out_y, alwan_f64 const *in_c, size_t in_stride, alwan_f64 const *in_m, alwan_f64 const *in_y, alwan_f64 const *in_k, size_t count);
alwan_status alwan_cmy_to_cmyk_map_interleave_ex(void *cmyk_out, size_t out_stride, void const *cmy_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_cmyk_to_cmy_map_interleave_ex(void *cmy_out, size_t out_stride, void const *cmyk_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_cmy_to_cmyk_map_planar_ex(void *out_c, size_t out_stride, void *out_m, void *out_y, void *out_k, void const *in_c, size_t in_stride, void const *in_m, void const *in_y, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_cmyk_to_cmy_map_planar_ex(void *out_c, size_t out_stride, void *out_m, void *out_y, void const *in_c, size_t in_stride, void const *in_m, void const *in_y, void const *in_k, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map YCoCg conversions */
alwan_status alwan_rgb_to_ycocg_f32_map_interleave(alwan_f32 *ycocg_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_ycocg_f64_map_interleave(alwan_f64 *ycocg_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_ycocg_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *ycocg_in, size_t in_stride, size_t count);
alwan_status alwan_ycocg_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *ycocg_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_ycocg_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ycocg_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* RGB <-> HWB conversions (Hue [0-1], Whiteness [0-1], Blackness [0-1]) */
alwan_status alwan_rgb_to_hwb_f32(alwan_hwb_f32 *hwb_out, alwan_rgb_f32 const *rgb);
alwan_status alwan_rgb_to_hwb_f64(alwan_hwb_f64 *hwb_out, alwan_rgb_f64 const *rgb);
alwan_status alwan_hwb_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_hwb_f32 const *hwb);
alwan_status alwan_hwb_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_hwb_f64 const *hwb);

/* Single-pixel HSV <-> HWB (same hexcone: w = (1-s)*v, b = 1-v) */
alwan_status alwan_hsv_to_hwb_f32(alwan_hwb_f32 *hwb_out, alwan_hsv_f32 const *hsv);
alwan_status alwan_hsv_to_hwb_f64(alwan_hwb_f64 *hwb_out, alwan_hsv_f64 const *hsv);
alwan_status alwan_hwb_to_hsv_f32(alwan_hsv_f32 *hsv_out, alwan_hwb_f32 const *hwb);
alwan_status alwan_hwb_to_hsv_f64(alwan_hsv_f64 *hsv_out, alwan_hwb_f64 const *hwb);

/* Map HWB conversions */
alwan_status alwan_rgb_to_hwb_f32_map_interleave(alwan_f32 *hwb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hwb_f64_map_interleave(alwan_f64 *hwb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count);
alwan_status alwan_hwb_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *hwb_in, size_t in_stride, size_t count);
alwan_status alwan_hwb_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *hwb_in, size_t in_stride, size_t count);
alwan_status alwan_hsv_to_hwb_f32_map_interleave(alwan_f32 *hwb_out, size_t out_stride, alwan_f32 const *hsv_in, size_t in_stride, size_t count);
alwan_status alwan_hsv_to_hwb_f64_map_interleave(alwan_f64 *hwb_out, size_t out_stride, alwan_f64 const *hsv_in, size_t in_stride, size_t count);
alwan_status alwan_hwb_to_hsv_f32_map_interleave(alwan_f32 *hsv_out, size_t out_stride, alwan_f32 const *hwb_in, size_t in_stride, size_t count);
alwan_status alwan_hwb_to_hsv_f64_map_interleave(alwan_f64 *hsv_out, size_t out_stride, alwan_f64 const *hwb_in, size_t in_stride, size_t count);
alwan_status alwan_rgb_to_hwb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hwb_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsv_to_hwb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hwb_to_hsv_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Map YCbCr conversions (with standard) */
alwan_status alwan_rgb_to_ycbcr_f32_map_interleave(alwan_f32 *ycbcr_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
alwan_status alwan_rgb_to_ycbcr_f64_map_interleave(alwan_f64 *ycbcr_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *ycbcr_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *ycbcr_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard);
alwan_status alwan_rgb_to_ycbcr_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);

/* Map YcCbcCrc conversions (with bit_depth) */
alwan_status alwan_rgb_to_yccbccrc_f32_map_interleave(alwan_f32 *yccbccrc_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, int bit_depth);
alwan_status alwan_rgb_to_yccbccrc_f64_map_interleave(alwan_f64 *yccbccrc_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *yccbccrc_in, size_t in_stride, size_t count, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *yccbccrc_in, size_t in_stride, size_t count, int bit_depth);
/* Gamut-safe bulk decode: raw decode, then per-pixel gamut mapping (output
 * guaranteed in [0,1]; CLIP = the pre-2.0 implicit clamp). Non-CLIP methods
 * run the scalar perceptual mapper per pixel -- prefer CLIP on hot paths. */
alwan_status alwan_ycbcr_to_rgb_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *ycbcr_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard, alwan_gamut_map_method method);
alwan_status alwan_ycbcr_to_rgb_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *ycbcr_in, size_t in_stride, size_t count, alwan_ycbcr_standard standard, alwan_gamut_map_method method);
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *yccbccrc_in, size_t in_stride, size_t count, int bit_depth, alwan_gamut_map_method method);
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *yccbccrc_in, size_t in_stride, size_t count, int bit_depth, alwan_gamut_map_method method);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 174). */
alwan_status alwan_ycbcr_to_rgb_gamut_safe_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_ycbcr_standard standard, alwan_gamut_map_method method);
alwan_status alwan_ycbcr_to_rgb_gamut_safe_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_ycbcr_standard standard, alwan_gamut_map_method method);
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth, alwan_gamut_map_method method);
alwan_status alwan_yccbccrc_to_rgb_gamut_safe_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth, alwan_gamut_map_method method);
alwan_status alwan_rgb_to_yccbccrc_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);

/* Map YCbCr legal/full range conversions (with bit_depth) */
alwan_status alwan_ycbcr_full_to_legal_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int bit_depth);
alwan_status alwan_ycbcr_full_to_legal_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, int bit_depth);
alwan_status alwan_ycbcr_full_to_legal_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);

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

/* Mired, 1e6 / CCT, and back. 0 is ALWAN_E_INVALID. */
alwan_status alwan_cct_to_mired_f64(alwan_f64 *mired_out, alwan_f64 cct);
alwan_status alwan_cct_to_mired_f32(alwan_f32 *mired_out, alwan_f32 cct);
alwan_status alwan_mired_to_cct_f64(alwan_f64 *cct_out, alwan_f64 mired);
alwan_status alwan_mired_to_cct_f32(alwan_f32 *cct_out, alwan_f32 mired);

/* The Planckian locus in CIE 1960 uv by Krystek 1985's rational fit, for 1000 K to
 * 15000 K; and its inverse, the CCT in that range whose locus point is nearest uv, solved
 * exactly where colour-science runs a Nelder-Mead search. ALWAN_E_RANGE when the nearest
 * point is an end of the range. */
alwan_status alwan_cct_to_uv_krystek1985_f64(alwan_vec2_f64 *uv_out, alwan_f64 cct);
alwan_status alwan_cct_to_uv_krystek1985_f32(alwan_vec2_f32 *uv_out, alwan_f32 cct);
alwan_status alwan_uv_to_cct_krystek1985_f64(alwan_f64 *cct_out, alwan_vec2_f64 const *uv);
alwan_status alwan_uv_to_cct_krystek1985_f32(alwan_f32 *cct_out, alwan_vec2_f32 const *uv);

/* The Planckian locus in CIE 1960 uv from Planck's law summed against an observer's 1 nm
 * CMFs, c2 = 1.4388e-2 m K, as colour-science's CCT_to_uv_Planck1900. */
alwan_status alwan_cct_to_uv_planck1900_f64(alwan_vec2_f64 *uv_out, alwan_f64 cct, alwan_observer_type observer, alwan_ctx *ctx);
alwan_status alwan_cct_to_uv_planck1900_f32(alwan_vec2_f32 *uv_out, alwan_f32 cct, alwan_observer_type observer, alwan_ctx *ctx);

/* A Planckian table: the locus above, sampled once and kept, so that a solve can read it
 * thousands of times without integrating the CMFs again. Ohno's temperatures rise
 * geometrically from start to end, the step easing off towards the top, as
 * colour-science's planckian_table builds them. Zero for start, end or spacing takes
 * Ohno's own 1000 K, 100000 K and 1.001; spacing must be greater than 1. */
typedef struct alwan_planckian_table_s alwan_planckian_table;

alwan_status alwan_planckian_table_create_f64(alwan_planckian_table **out, alwan_observer_type observer,
                                              alwan_f64 start, alwan_f64 end, alwan_f64 spacing, alwan_ctx *ctx);
alwan_status alwan_planckian_table_create_f32(alwan_planckian_table **out, alwan_observer_type observer,
                                              alwan_f32 start, alwan_f32 end, alwan_f32 spacing, alwan_ctx *ctx);
void alwan_planckian_table_destroy(alwan_planckian_table *table, alwan_ctx *ctx);
size_t alwan_planckian_table_size(alwan_planckian_table const *table);

/* Ohno 2013: the CCT and signed Duv of a CIE 1960 uv, from the table's nearest entry and
 * its two neighbours. The triangular solution is taken near the locus and the parabolic
 * one from |Duv| = 0.002 out, which is where colour-science switches. Duv is positive
 * above the locus, per Ohno 2013 and ANSI C78.377; duv_out may be NULL. A point whose
 * nearest entry is an end of the table is ALWAN_E_RANGE, since neither solution has the
 * neighbour it needs. */
alwan_status alwan_uv_to_cct_ohno2013_f64(alwan_f64 *cct_out, alwan_f64 *duv_out, alwan_vec2_f64 const *uv,
                                          alwan_planckian_table const *table);
alwan_status alwan_uv_to_cct_ohno2013_f32(alwan_f32 *cct_out, alwan_f32 *duv_out, alwan_vec2_f32 const *uv,
                                          alwan_planckian_table const *table);

/* The CCT, 4000 K to 25000 K, whose CIE daylight locus point (alwan_d_series_illuminant_xy)
 * is nearest xy, solved exactly on each side of the locus's 7000 K joint. ALWAN_E_RANGE
 * when the nearest point is an end of the range. */
alwan_status alwan_xy_to_cct_cie_d_f64(alwan_f64 *cct_out, alwan_vec2_f64 const *xy);
alwan_status alwan_xy_to_cct_cie_d_f32(alwan_f32 *cct_out, alwan_vec2_f32 const *xy);

/* CRI (Color Rendering Index) Ra - average of 8 TCS samples */
/* Requires SPD (spectral power distribution) */
/* Returns CRI Ra value, or negative on error.
 *
 * Ra is NOT clamped to [0, 100]. CIE 13.3 lets it go negative for very poor
 * sources, and clamping would hide exactly the ones the metric exists to flag.
 *
 * Accuracy, measured against colour.quality.colour_rendering_index over 35 CIE
 * illuminants: mean absolute deviation 0.054, worst 0.43 (HP1). Sources that
 * are their own reference land within 0.01 of 100. */
alwan_f64 alwan_cri_ra_f64(alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
alwan_f32 alwan_cri_ra_f32(alwan_spd_f32 const *test_spd, alwan_ctx *ctx);

/* CIE 13.3-1995 in full, where alwan_cri_ra gives only the average.
 *
 * rs holds the fourteen special indices R1 to R14, one per test colour sample, in the
 * standard's own order: the eight muted samples that Ra averages, then saturated red,
 * saturated yellow, saturated green and saturated blue, then Caucasian skin and leaf
 * green. ra is the average of the first eight; the standard defines no average over the
 * other six, and this call does not invent one.
 *
 * Nothing here is clamped. R9 in particular runs far below zero for a source with no red
 * content, which is the reading it exists to give: a high pressure sodium lamp scores
 * around -200 on it.
 *
 * Unlike alwan_cri_ra, which reports failure as -1 and so cannot be told apart from a
 * genuinely poor source, this call returns a status and touches spec_out only on
 * ALWAN_OK. */
#define ALWAN_CRI_SAMPLES 14

typedef struct {
    alwan_f64 ra;                     /* average of rs[0] to rs[7], as alwan_cri_ra */
    alwan_f64 rs[ALWAN_CRI_SAMPLES];  /* R1 to R14 */
} alwan_cri_f64;

typedef struct {
    alwan_f32 ra;
    alwan_f32 rs[ALWAN_CRI_SAMPLES];
} alwan_cri_f32;

alwan_status alwan_cri_specification_f64(alwan_cri_f64 *spec_out, alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
alwan_status alwan_cri_specification_f32(alwan_cri_f32 *spec_out, alwan_spd_f32 const *test_spd, alwan_ctx *ctx);

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

/* ANSI/IES TM-30-18 in full, where alwan_tm30_rf gives only the headline number.
 *
 * The 99 colour evaluation samples are sorted into 16 hue bins by where each one lands
 * under the reference illuminant, and the rendition is then described bin by bin: how
 * faithfully it renders (rfs), how much chroma it gains or loses (rcs, in percent) and how
 * far its hue turns (rhs). The 16 test and reference (a', b') averages are the vertices of
 * the colour vector graphic, and rg is 100 times the ratio of the areas those two polygons
 * enclose: above 100 the source saturates on average, below 100 it dulls.
 *
 * A hue bin with no samples in it leaves its averages, rfs, rcs and rhs at zero. No real
 * source empties a bin; the 99 samples were chosen so that none can. */
#define ALWAN_TM30_HUE_BINS 16
#define ALWAN_TM30_SAMPLES 99

typedef struct {
    alwan_f64 rf;                                          /* general fidelity, as alwan_tm30_rf */
    alwan_f64 rg;                                          /* gamut index */
    alwan_f64 rfs[ALWAN_TM30_HUE_BINS];                    /* local fidelity */
    alwan_f64 rcs[ALWAN_TM30_HUE_BINS];                    /* local chroma shift, percent */
    alwan_f64 rhs[ALWAN_TM30_HUE_BINS];                    /* local hue shift */
    alwan_f64 average_norms[ALWAN_TM30_HUE_BINS];          /* length of each reference average */
    alwan_f64 averages_test[ALWAN_TM30_HUE_BINS][2];       /* (a', b') under the test source */
    alwan_f64 averages_reference[ALWAN_TM30_HUE_BINS][2];  /* (a', b') under the reference */
    int bins[ALWAN_TM30_SAMPLES];                          /* the bin each sample fell in */
} alwan_tm30_f64;

typedef struct {
    alwan_f32 rf;
    alwan_f32 rg;
    alwan_f32 rfs[ALWAN_TM30_HUE_BINS];
    alwan_f32 rcs[ALWAN_TM30_HUE_BINS];
    alwan_f32 rhs[ALWAN_TM30_HUE_BINS];
    alwan_f32 average_norms[ALWAN_TM30_HUE_BINS];
    alwan_f32 averages_test[ALWAN_TM30_HUE_BINS][2];
    alwan_f32 averages_reference[ALWAN_TM30_HUE_BINS][2];
    int bins[ALWAN_TM30_SAMPLES];
} alwan_tm30_f32;

alwan_status alwan_tm30_specification_f64(alwan_tm30_f64 *spec_out, alwan_spd_f64 const *test_spd, alwan_ctx *ctx);
alwan_status alwan_tm30_specification_f32(alwan_tm30_f32 *spec_out, alwan_spd_f32 const *test_spd, alwan_ctx *ctx);

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
 * observer: observer type (2 deg or 10 deg)
 * Returns metamerism index (dE*ab under test illuminant), or negative on error */
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
 * rgb_out: output simulated RGB color as seen by person with CVD -- RAW
 *          simulation result, NOT gamut-clamped: saturated inputs can land
 *          outside [0,1]. Use alwan_simulate_cvd_gamut_safe (or the _machado/
 *          _ex twins) for guaranteed [0,1] output; ALWAN_GAMUT_MAP_CLIP
 *          reproduces the pre-2.0 implicit clamping.
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error
 * Algorithm: Brettel, Vienot & Mollon (1997) simulation using confusion lines */
alwan_status alwan_simulate_cvd_f32(alwan_rgb_f32 *rgb_out,
                           alwan_rgb_f32 const *rgb_in,
                           alwan_cvd_type cvd_type,
                           alwan_f32 severity);
alwan_status alwan_simulate_cvd_f64(alwan_rgb_f64 *rgb_out,
                           alwan_rgb_f64 const *rgb_in,
                           alwan_cvd_type cvd_type,
                           alwan_f64 severity);

/* CVD Simulation Batch Map Functions */
alwan_status alwan_simulate_cvd_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
alwan_status alwan_simulate_cvd_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
alwan_status alwan_simulate_protanopia_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 severity);
alwan_status alwan_simulate_protanopia_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 severity);
alwan_status alwan_simulate_deuteranopia_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 severity);
alwan_status alwan_simulate_deuteranopia_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 severity);
alwan_status alwan_simulate_tritanopia_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 severity);
alwan_status alwan_simulate_tritanopia_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 severity);
alwan_status alwan_simulate_cvd_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);
alwan_status alwan_simulate_protanopia_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
alwan_status alwan_simulate_deuteranopia_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
alwan_status alwan_simulate_tritanopia_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);

/* ----------------------------------------------------------------
 * Display tone response: GOG and GOGO
 *
 * A display on a meter gives pairs of drive and luminance, and these fit the
 * curve that joins them:
 *
 *   GOG    L = (gain d + offset)^gamma
 *   GOGO   L = (gain d + offset)^gamma + flare
 *
 * with d normalised to [0, 1] and L normalised too. The flare is what the
 * room, the screen surface and the meter add to black, and it is why a
 * measured display almost never reads zero at zero. Pass with_flare non-zero
 * to fit it; a GOG fit leaves it at zero.
 *
 * NOT TESTABLE AGAINST A REFERENCE. Nothing in colour-science fits either
 * model, so suite 167 uses the geometry instead: patches are generated from a
 * known model and the fit has to return its parameters. That is the stronger
 * check anyway, since it would catch two implementations being wrong the same
 * way. Published in Berns (1996), "Methods for characterizing CRT displays".
 *
 * rms_out, which may be NULL, is the root mean square LUMINANCE residual, and
 * it is the quantity the fit minimises. The fit gets there in two stages: for
 * a fixed gamma and flare the model straightens into a line whose best gain
 * and offset follow in closed form, so the search is over one or two
 * parameters rather than three or four, and a short fixed-iteration simplex
 * then finishes on the real residual. Both stages are deterministic, with a
 * fixed iteration count and no convergence test, so the answer does not move
 * between machines.
 *
 * A GOG fit needs at least four points and a GOGO fit five: a fit to as many
 * points as it has parameters says nothing about the display. Fewer is
 * ALWAN_E_RANGE.
 *
 * alwan_display_gog_invert_{T} is closed form. A luminance below the flare is
 * ALWAN_E_RANGE, since the display cannot go darker than its own black.
 * ---------------------------------------------------------------- */

alwan_status alwan_display_gog_fit_f64(alwan_display_gog_f64 *out, alwan_f64 *rms_out, alwan_f64 const *digital, alwan_f64 const *luminance, size_t count, int with_flare);
alwan_status alwan_display_gog_fit_f32(alwan_display_gog_f32 *out, alwan_f32 *rms_out, alwan_f32 const *digital, alwan_f32 const *luminance, size_t count, int with_flare);

alwan_status alwan_display_gog_eval_f64(alwan_f64 *luminance_out, alwan_display_gog_f64 const *model, alwan_f64 digital);
alwan_status alwan_display_gog_eval_f32(alwan_f32 *luminance_out, alwan_display_gog_f32 const *model, alwan_f32 digital);
alwan_status alwan_display_gog_invert_f64(alwan_f64 *digital_out, alwan_display_gog_f64 const *model, alwan_f64 luminance);
alwan_status alwan_display_gog_invert_f32(alwan_f32 *digital_out, alwan_display_gog_f32 const *model, alwan_f32 luminance);

/* ----------------------------------------------------------------
 * A measured display, and the LUT that calibrates it
 *
 * Run a ramp up each channel of a display with a meter on it, and these turn
 * the readings into a model of what that display does with any signal:
 *
 *     XYZ = M [ t_r(d_r), t_g(d_g), t_b(d_b) ]^T + XYZ_black
 *
 * t_c is that channel's tone curve, normalised to run 0 to 1 over the drive
 * range; M's columns are the primaries at full drive with the black taken off;
 * and the black is the reading at zero drive, carried once.
 *
 * Each channel is fitted as a GOGO on the RAW luminance ramp, not on a
 * black-subtracted one, and the order matters. Subtract the shared black first
 * and a channel whose own curve has a positive offset stops being a power law:
 * its emission at zero drive has gone into the black, and what is left is
 * (a d + b)^g - b^g, which no GOG can be. Fitted raw, the offset survives and
 * the flare comes back as that channel's reading of the display's black. The
 * stored curve is the normalised one, which is still a GOGO with a negative
 * flare, so alwan_display_gog_eval_{T} evaluates it directly.
 *
 * THE MODEL ASSUMES CHANNEL INDEPENDENCE AND ADDITIVITY, that each channel
 * depends only on its own drive and that the three add. Real displays deviate,
 * LCDs most of all, and ramps cannot fit that deviation. What alwan does
 * instead is let you measure it: the fit reports its residual, and a caller
 * with measurements OFF the ramps can push them through
 * alwan_display_model_forward_{T} and compare. A model that says how wrong it
 * is beats one that does not.
 *
 * alwan_display_model_fit_{T} takes one drive axis shared by three ramps of
 * measured XYZ. The ramps must start at drive 0, since the black reading is
 * what everything else is measured against, and the three readings there are
 * averaged as three measurements of one quantity. At least five points, since
 * each channel is fitted as a GOGO. rms_out, which may be NULL, is the worst of the three tone
 * residuals.
 *
 * alwan_display_model_invert_{T} gives the drive that produces an XYZ. The
 * drive comes back CLAMPED to what the display can reach, and excursion_out,
 * which may be NULL, is how far outside its gamut the request was, 0 when
 * inside. That is a reported clamp rather than a silent one: a display cannot
 * emit what it cannot emit, and the caller is told what was asked for.
 *
 * alwan_display_calibration_lut_{T} bakes the whole chain into a 3D LUT: the
 * source EOTF, the source primaries, a chromatic adaptation from the source
 * white onto the display's MEASURED white, and the model inverse. Feed it
 * source-encoded signal and it gives you drive. size is the cube edge, 2 to
 * 256, and the output is size^3 * 3 values R-fastest, which is what
 * alwan_cube_export_3d_{T} and alwan_table3d_sample_{T} expect.
 * worst_excursion, which may be NULL, is the largest excursion over the whole
 * cube, and it is the number that says whether the display can show the space
 * you asked it to show.
 *
 * NOT TESTABLE AGAINST A REFERENCE: colour-science has no display model and no
 * calibration bake. Suite 168 closes the loop instead, generating measurements
 * from a display it chose and requiring the fit to return it, then requiring
 * source colour to survive the LUT and the model together. The case that needs
 * no tolerance argument: calibrating a display that ALREADY IS the source
 * space must give the identity LUT. Published in Berns (1996), "Methods for
 * characterizing CRT displays", Displays 16(4).
 * ---------------------------------------------------------------- */

alwan_status alwan_display_model_fit_f64(alwan_display_model_f64 *out, alwan_f64 *rms_out, alwan_f64 const *drives, alwan_xyz_f64 const *ramp_red, alwan_xyz_f64 const *ramp_green, alwan_xyz_f64 const *ramp_blue, size_t count);
alwan_status alwan_display_model_fit_f32(alwan_display_model_f32 *out, alwan_f32 *rms_out, alwan_f32 const *drives, alwan_xyz_f32 const *ramp_red, alwan_xyz_f32 const *ramp_green, alwan_xyz_f32 const *ramp_blue, size_t count);

alwan_status alwan_display_model_forward_f64(alwan_xyz_f64 *xyz_out, alwan_display_model_f64 const *model, alwan_rgb_f64 const *drive);
alwan_status alwan_display_model_forward_f32(alwan_xyz_f32 *xyz_out, alwan_display_model_f32 const *model, alwan_rgb_f32 const *drive);

alwan_status alwan_display_model_invert_f64(alwan_rgb_f64 *drive_out, alwan_f64 *excursion_out, alwan_display_model_f64 const *model, alwan_xyz_f64 const *xyz);
alwan_status alwan_display_model_invert_f32(alwan_rgb_f32 *drive_out, alwan_f32 *excursion_out, alwan_display_model_f32 const *model, alwan_xyz_f32 const *xyz);

alwan_status alwan_display_calibration_lut_f64(alwan_f64 *lut_out, int size, alwan_display_model_f64 const *model, alwan_rgb_space_desc_f64 const *source, alwan_transfer_function source_eotf, alwan_cat_method cat, alwan_f64 *worst_excursion);
alwan_status alwan_display_calibration_lut_f32(alwan_f32 *lut_out, int size, alwan_display_model_f32 const *model, alwan_rgb_space_desc_f32 const *source, alwan_transfer_function source_eotf, alwan_cat_method cat, alwan_f32 *worst_excursion);

/* ----------------------------------------------------------------
 * Michaelis-Menten
 *
 * The saturating two-parameter curve, which arrived in colour science through
 * vision rather than through enzymes: a photoreceptor's response saturates the
 * way a reaction rate does. It is already inside the library twice, as
 * ALWAN_LIGHTNESS_ABEBE2017_MICHAELIS_MENTEN and as the JP2499 tonescale, each
 * reaching it through its own code. These make the relation itself callable.
 *
 *   Michaelis 1913   v = V_max S / (K_m + S)         S = v K_m / (V_max - v)
 *   Abebe 2017       v = V_max S / (b_m S + K_m)     S = v K_m / (V_max - b_m v)
 *
 * K_m is the substrate at which the rate reaches half of V_max, which is what
 * makes it the parameter worth fitting: it is a position on the input axis
 * rather than a shape. Abebe's b_m scales the substrate term in the
 * denominator; at 1 it gives Michaelis's form back exactly.
 *
 * ALWAN_E_DIVZERO where a denominator vanishes, which for the inverse is the
 * rate the curve approaches and never reaches. A saturating curve has no
 * answer for an input above its maximum, and saying so is more use than an
 * infinity. A non-finite argument is ALWAN_E_INVALID.
 *
 * Reference: Michaelis and Menten (1913); Abebe, Pouli, Larabi and Reinhard
 * (2017). Pinned against colour-science in suite 166.
 * ---------------------------------------------------------------- */

alwan_status alwan_michaelis_menten_rate_f64(alwan_f64 *rate_out, alwan_f64 substrate, alwan_f64 v_max, alwan_f64 k_m);
alwan_status alwan_michaelis_menten_rate_f32(alwan_f32 *rate_out, alwan_f32 substrate, alwan_f32 v_max, alwan_f32 k_m);
alwan_status alwan_michaelis_menten_substrate_f64(alwan_f64 *substrate_out, alwan_f64 rate, alwan_f64 v_max, alwan_f64 k_m);
alwan_status alwan_michaelis_menten_substrate_f32(alwan_f32 *substrate_out, alwan_f32 rate, alwan_f32 v_max, alwan_f32 k_m);

alwan_status alwan_michaelis_menten_rate_abebe2017_f64(alwan_f64 *rate_out, alwan_f64 substrate, alwan_f64 v_max, alwan_f64 k_m, alwan_f64 b_m);
alwan_status alwan_michaelis_menten_rate_abebe2017_f32(alwan_f32 *rate_out, alwan_f32 substrate, alwan_f32 v_max, alwan_f32 k_m, alwan_f32 b_m);
alwan_status alwan_michaelis_menten_substrate_abebe2017_f64(alwan_f64 *substrate_out, alwan_f64 rate, alwan_f64 v_max, alwan_f64 k_m, alwan_f64 b_m);
alwan_status alwan_michaelis_menten_substrate_abebe2017_f32(alwan_f32 *substrate_out, alwan_f32 rate, alwan_f32 v_max, alwan_f32 k_m, alwan_f32 b_m);

/* ----------------------------------------------------------------
 * Machado 2009 as a continuous model
 *
 * The call below interpolates between the eleven matrices the paper
 * tabulates. Those are right for those eleven severities ON THE DISPLAY THE
 * AUTHORS USED, a 1997 CRT, and they cannot answer for a different display.
 * This derives the matrix from the cone fundamentals instead, so any shift and
 * any display can be asked for.
 *
 * shift_l, shift_m and shift_s are the cone shifts in nanometres, not a
 * severity in [0, 1]. The paper's protanomaly and deuteranomaly run 0 to 20;
 * its tritanomaly tables use 5 to 59, and the authors say plainly that the
 * shift paradigm is an approximation there rather than a model of tritanopia.
 *
 * primary_r, primary_g and primary_b are the display's spectra. Pass NULL for
 * all three to get the 1997 CRT the published tables were made with, which is
 * embedded at 1 nm and reproduces colour-science's answer. Passing measured
 * spectra instead resamples them with alwan's own linear interpolation, which
 * is NOT what the reference does between samples: it uses Sprague, and the
 * difference reaches 1.8e-02 in the resulting matrix. A caller with measured
 * primaries should therefore expect their own numbers, not another library's.
 *
 * THE MODEL CAN GO SINGULAR, AND THIS DOES NOT GUARD AGAINST IT. Each row of
 * the opponent matrix is divided by its own sum, and for some display and some
 * shift that sum passes through zero. On the embedded Apple Studio Display it
 * happens near an L shift of 11.5 nm, where a coefficient reaches 1066: a
 * matrix that would destroy any image it touched. colour-science returns the
 * same value to 2.6e-08, because it is the model doing this rather than either
 * implementation, and alwan follows it rather than inventing a threshold for
 * where a large coefficient becomes an unusable one.
 *
 * So a caller sweeping shifts on measured primaries should look at what comes
 * back. The published tables never show this because they sample one display
 * at eleven points and none of them lands near a pole.
 * ---------------------------------------------------------------- */

typedef enum {
    ALWAN_DISPLAY_PRIMARIES_CRT_BRAINARD1997 = 0,  /* what the published tables use */
    ALWAN_DISPLAY_PRIMARIES_APPLE_STUDIO = 1,
    ALWAN_DISPLAY_PRIMARIES_COUNT = 2
} alwan_display_primaries;

/* The embedded display spectra, 390 to 830 nm at 1 nm. Creates three SPDs;
 * the caller destroys each with alwan_spd_destroy. */
alwan_status alwan_display_primaries_spd_f64(alwan_spd_f64 *r, alwan_spd_f64 *g, alwan_spd_f64 *b, alwan_display_primaries which, alwan_ctx *ctx);
alwan_status alwan_display_primaries_spd_f32(alwan_spd_f32 *r, alwan_spd_f32 *g, alwan_spd_f32 *b, alwan_display_primaries which, alwan_ctx *ctx);

/* The CVD matrix for a given set of cone shifts, in the display's own RGB. */
alwan_status alwan_cvd_matrix_machado2009_shift_f64(alwan_mat3x3_f64 *out, alwan_f64 shift_l, alwan_f64 shift_m, alwan_f64 shift_s, alwan_spd_f64 const *primary_r, alwan_spd_f64 const *primary_g, alwan_spd_f64 const *primary_b, alwan_ctx *ctx);
alwan_status alwan_cvd_matrix_machado2009_shift_f32(alwan_mat3x3_f32 *out, alwan_f32 shift_l, alwan_f32 shift_m, alwan_f32 shift_s, alwan_spd_f32 const *primary_r, alwan_spd_f32 const *primary_g, alwan_spd_f32 const *primary_b, alwan_ctx *ctx);

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
alwan_status alwan_simulate_cvd_machado_f32(alwan_rgb_f32 *rgb_out,
                                   alwan_rgb_f32 const *rgb_in,
                                   alwan_cvd_type cvd_type,
                                   alwan_f32 severity);
alwan_status alwan_simulate_cvd_machado_f64(alwan_rgb_f64 *rgb_out,
                                   alwan_rgb_f64 const *rgb_in,
                                   alwan_cvd_type cvd_type,
                                   alwan_f64 severity);

/* Model-selectable CVD simulation (dispatches to Brettel or Machado) */
alwan_status alwan_simulate_cvd_ex_f32(alwan_rgb_f32 *rgb_out,
                              alwan_rgb_f32 const *rgb_in,
                              alwan_cvd_type cvd_type,
                              alwan_f32 severity,
                              alwan_cvd_model model);
alwan_status alwan_simulate_cvd_ex_f64(alwan_rgb_f64 *rgb_out,
                              alwan_rgb_f64 const *rgb_in,
                              alwan_cvd_type cvd_type,
                              alwan_f64 severity,
                              alwan_cvd_model model);

/* Machado 2009 batch map functions */
alwan_status alwan_simulate_cvd_machado_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
alwan_status alwan_simulate_cvd_machado_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
alwan_status alwan_simulate_cvd_machado_f32_map_planar(alwan_f32 *out_r, size_t out_stride, alwan_f32 *out_g, alwan_f32 *out_b, alwan_f32 const *in_r, size_t in_stride, alwan_f32 const *in_g, alwan_f32 const *in_b, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
alwan_status alwan_simulate_cvd_machado_f64_map_planar(alwan_f64 *out_r, size_t out_stride, alwan_f64 *out_g, alwan_f64 *out_b, alwan_f64 const *in_r, size_t in_stride, alwan_f64 const *in_g, alwan_f64 const *in_b, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
alwan_status alwan_simulate_cvd_machado_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);
alwan_status alwan_simulate_cvd_machado_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);

/* Gamut-safe CVD simulation: raw simulation, then gamut mapping into sRGB
 * (output guaranteed in [0,1]; ALWAN_GAMUT_MAP_CLIP = the pre-2.0 implicit
 * post-simulation clip). */
alwan_status alwan_simulate_cvd_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_machado_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_machado_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_ex_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_cvd_type cvd_type, alwan_f32 severity, alwan_cvd_model model, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_ex_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_cvd_type cvd_type, alwan_f64 severity, alwan_cvd_model model, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 174). */
alwan_status alwan_simulate_cvd_gamut_safe_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_gamut_safe_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_machado_gamut_safe_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_machado_gamut_safe_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_machado_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method);
alwan_status alwan_simulate_cvd_machado_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method);

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

/* The luminous efficiency functions as datasets. alwan_vision_type names a regime and
 * each regime has one canonical function behind it, CIE 1924 for photopic and CIE 1951
 * for scotopic; these are the seven functions colour-science ships, five of which
 * compete for the photopic regime and are reachable only by name. Judd 1951 and
 * Judd-Vos 1978 correct V(lambda) below 460 nm, where the 1924 function is too low;
 * CIE 1964 is the 10 degree field; the CIE 2008 pair are the physiologically relevant
 * functions built on the Stockman and Sharpe cone fundamentals. */
typedef enum {
    ALWAN_LEF_CIE_1924_PHOTOPIC = 0,        /* V(lambda); what ALWAN_VISION_PHOTOPIC reads */
    ALWAN_LEF_JUDD_1951_PHOTOPIC = 1,
    ALWAN_LEF_JUDD_VOS_1978_PHOTOPIC = 2,
    ALWAN_LEF_CIE_1964_PHOTOPIC_10DEG = 3,
    ALWAN_LEF_CIE_2008_PHOTOPIC_2DEG = 4,
    ALWAN_LEF_CIE_2008_PHOTOPIC_10DEG = 5,
    ALWAN_LEF_CIE_1951_SCOTOPIC = 6         /* V'(lambda); what ALWAN_VISION_SCOTOPIC reads */
} alwan_lef;

/* A luminous efficiency function as an SPD on its own grid: 1 nm for six of them,
 * Judd 1951 at 10 nm over 370-770 nm as published. Each peaks at 1 or within 5e-3 of
 * it, as published (Judd 1951 at 0.995, the CIE 2008 pair a few 1e-6 under). Creates
 * the SPD. CIE 1924 and CIE 1951 are the same numbers alwan_luminous_efficiency
 * interpolates; the other five are registry tables and answer ALWAN_E_NODATA when
 * compiled out. */
alwan_status alwan_spd_lef_f64(alwan_spd_f64 *out, alwan_lef lef, alwan_ctx *ctx);
alwan_status alwan_spd_lef_f32(alwan_spd_f32 *out, alwan_lef lef, alwan_ctx *ctx);

/* Luminous flux of an SPD under any of the seven functions: K_m times the trapezoid of
 * V(lambda) S(lambda) over the SPD's own samples, V read from the function's table at
 * each sample and 0 outside the function's data, as colour-science's luminous_flux
 * computes it when handed a lef. K_m 0 reads as 683, or 1700 for CIE 1951, the
 * constants alwan_spd_luminous_flux uses. For the two canonical functions this and
 * alwan_spd_luminous_flux agree. ALWAN_E_INVALID for a negative K_m or fewer than two
 * samples. */
alwan_status alwan_spd_luminous_flux_lef_f64(alwan_f64 *flux_out, alwan_spd_f64 const *spd, alwan_lef lef, alwan_f64 K_m, alwan_ctx *ctx);
alwan_status alwan_spd_luminous_flux_lef_f32(alwan_f32 *flux_out, alwan_spd_f32 const *spd, alwan_lef lef, alwan_f32 K_m, alwan_ctx *ctx);

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

/* Luminous flux of an SPD: K_m times the trapezoid of V(lambda) S(lambda) over the SPD's
 * own samples, V(lambda) 0 outside its data, as colour-science computes it. vision is
 * ALWAN_VISION_PHOTOPIC (CIE 1924) or ALWAN_VISION_SCOTOPIC (CIE 1951); K_m 0 reads as 683
 * or 1700, colour-science's constants (alwan_photopic_luminance uses 683.002). Efficiency
 * is the V-weighted trapezoid over the plain one; efficacy is K_m times it, in lm/W.
 * ALWAN_E_INVALID for MESOPIC, which needs an adaptation level, a negative K_m, or an SPD
 * of fewer than two samples. */
alwan_status alwan_spd_luminous_flux_f64(alwan_f64 *flux_out, alwan_spd_f64 const *spd, alwan_vision_type vision, alwan_f64 K_m);
alwan_status alwan_spd_luminous_flux_f32(alwan_f32 *flux_out, alwan_spd_f32 const *spd, alwan_vision_type vision, alwan_f32 K_m);
alwan_status alwan_spd_luminous_efficiency_f64(alwan_f64 *efficiency_out, alwan_spd_f64 const *spd, alwan_vision_type vision);
alwan_status alwan_spd_luminous_efficiency_f32(alwan_f32 *efficiency_out, alwan_spd_f32 const *spd, alwan_vision_type vision);
alwan_status alwan_spd_luminous_efficacy_f64(alwan_f64 *efficacy_out, alwan_spd_f64 const *spd, alwan_vision_type vision, alwan_f64 K_m);
alwan_status alwan_spd_luminous_efficacy_f32(alwan_f32 *efficacy_out, alwan_spd_f32 const *spd, alwan_vision_type vision, alwan_f32 K_m);

/* Contrast Sensitivity Function (CSF) */

/* Calculate contrast sensitivity for spatial frequency (simplified model)
 * spatial_frequency: spatial frequency in cycles per degree [0.1, 60]
 * luminance: background luminance in cd/m^2 [0.01, 10000]
 * Returns contrast sensitivity (1/contrast_threshold), or negative on error
 * Uses simplified Barten CSF model (1999) */
alwan_f32 alwan_csf_f32(alwan_f32 spatial_frequency, alwan_f32 luminance);
alwan_f64 alwan_csf_f64(alwan_f64 spatial_frequency, alwan_f64 luminance);

/* Helmholtz-Kohlrausch effect (Nayatani 1997)
 *
 * How much brighter a chromatic stimulus looks than an achromatic one of the same
 * luminance. Both entry points take CIE 1960 UCS chromaticities: uv for the stimulus,
 * uv_c for the adapting field. Note this is the u, v chromaticity pair, not the U*V*W*
 * triple alwan_xyz_to_ucs returns.
 *
 * L_a: adapting luminance in cd/m^2
 * method: VCC weights the hue coefficient by -0.866, VAC by -0.134; nothing else differs
 *
 * The object variant returns a multiplier on luminance, exactly 1 when the stimulus sits
 * on the adapting field. The luminous variant is 0.4462 (object + 0.3086)^3.
 *
 * Both are native-range and have no normalization macro, as docs/ranges.md rule 2
 * requires: they are viewing-condition-dependent and have no fixed bound. Measured over
 * the reference sweep the object variant spans about [0.77, 1.40] and the luminous one
 * about [0.56, 2.24], but those are the sweep's extents, not limits of the model.
 *
 * Returns the effect, or a negative value on a NULL argument. */
alwan_f32 alwan_hke_object_nayatani1997_f32(alwan_vec2_f32 const *uv,
                                            alwan_vec2_f32 const *uv_c,
                                            alwan_f32 L_a,
                                            alwan_hke_nayatani1997_method method);
alwan_f64 alwan_hke_object_nayatani1997_f64(alwan_vec2_f64 const *uv,
                                            alwan_vec2_f64 const *uv_c,
                                            alwan_f64 L_a,
                                            alwan_hke_nayatani1997_method method);

alwan_f32 alwan_hke_luminous_nayatani1997_f32(alwan_vec2_f32 const *uv,
                                              alwan_vec2_f32 const *uv_c,
                                              alwan_f32 L_a,
                                              alwan_hke_nayatani1997_method method);
alwan_f64 alwan_hke_luminous_nayatani1997_f64(alwan_vec2_f64 const *uv,
                                              alwan_vec2_f64 const *uv_c,
                                              alwan_f64 L_a,
                                              alwan_hke_nayatani1997_method method);

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
    ALWAN_INTERP_CUBIC = 1, /* Catmull-Rom cubic, local. Not a cubic spline: for the C2 spline
                             * scipy and colour-science mean by that name, see
                             * alwan_interpolate_cubic_spline */
    ALWAN_INTERP_LANCZOS = 2, /* Lanczos windowed sinc */
    ALWAN_INTERP_SPRAGUE = 3, /* Sprague 5th order (for smooth spectra) */
    ALWAN_INTERP_LAGRANGE = 4, /* Lagrange polynomial */
    ALWAN_INTERP_AKIMA = 5, /* Akima spline (non-overshooting) */
    /* PCHIP, Fritsch and Carlson's monotone cubic. It reads the same four points
     * ALWAN_INTERP_CUBIC does, and differs in what it does with them: the node
     * derivatives are chosen so the curve never overshoots between samples. Where the
     * data turns, PCHIP flattens; ALWAN_INTERP_CUBIC is Catmull-Rom, which stays smooth
     * and rings. On a reflectance or a transfer curve that overshoot can leave values
     * outside the range the data never left.
     *
     * Matches scipy's PchipInterpolator, which is what colour-science wraps. */
    ALWAN_INTERP_PCHIP = 6
} alwan_interp_method;

/* Extrapolation method types */
typedef enum {
    ALWAN_EXTRAP_CONSTANT = 0,    /* Constant (use boundary value) */
    ALWAN_EXTRAP_LINEAR = 1, /* Linear extrapolation */
    ALWAN_EXTRAP_POLYNOMIAL = 2, /* Polynomial extrapolation */
    ALWAN_EXTRAP_EXPONENTIAL = 3, /* Exponential decay (for SPDs) */
    ALWAN_EXTRAP_REFLECT = 4, /* Reflective boundary */
    ALWAN_EXTRAP_NATURAL = 5 /* Natural neighbor extrapolation */
} alwan_extrap_method;

/* Color Checker target types.
 *
 * The values alwan carries are the published averages for a TYPE, xyY under D50 for the 1931 2
 * degree observer. A physical sheet is not its type: charts differ between production runs and
 * drift as they age, so for real calibration work use the individual target's own measurement.
 * OpenQualia (openqualia.org) standardises exactly that file, and alwan_dev's
 * gendata/openqualia.py reads one into this layout.
 *
 * alwan_color_checker_num_patches reports what is available, so a type with no data reports 0
 * and its lookups return ALWAN_E_NODATA. */
typedef enum {
    ALWAN_COLORCHECKER_CLASSIC = 0,      /* ColorChecker Classic 24-patch */
    ALWAN_COLORCHECKER_SG = 1, /* ColorChecker SG 140-patch, A1..N10, current formulation */
    ALWAN_COLORCHECKER_DIGITAL_SG = 2, /* the same physical target under its product name */
    ALWAN_BABELCOLOR_AVERAGE = 3, /* BabelColor's average of 30 Classic charts, 24 patches */
    ALWAN_BABELCOLOR_HCT = 4, /* BabelColor HCT: no data, reports 0 patches */
    /* The Classic through its production runs: the pigments changed in November 2014, and the
     * 1976 values are published under Illuminant C rather than D50. */
    ALWAN_COLORCHECKER_CLASSIC_1976 = 5,
    ALWAN_COLORCHECKER_CLASSIC_PRE2014 = 6,
    ALWAN_COLORCHECKER_CLASSIC_POST2014 = 7,
    ALWAN_COLORCHECKER_SG_PRE2014 = 8, /* the SG before the same change */
    ALWAN_TE226_V2 = 9, /* Image Engineering TE226 V2, 45 patches, published under D65 */
    /* Measured as reflectance rather than as tristimulus values, so these answer for any
     * illuminant and observer instead of only for the one they were published under. They have
     * no tristimulus table: alwan_color_checker_data integrates the spectrum for you. */
    ALWAN_COLORCHECKER_CLASSIC_OHTA = 10, /* Ohta 1997, 24 patches, 380-780 nm at 5 nm. ISO
                                           * 17321-1 carries the same numbers. */
    ALWAN_COLORCHECKER_PMC = 11, /* 30 patches of skin tones and memory colours, 400-700 nm at
                                  * 10 nm */
    /* Layout only. ISO 12641 fixes an IT8's 288 patches, A1..L22 then a 24 step grey ramp, and
     * leaves the colorimetry to the manufacturer and the production run, so there is no such
     * thing as canonical IT8 values and alwan carries none. Every target ships with its own
     * batch reference file, which is where its numbers live; alwan_dev's gendata/openqualia.py
     * reads one. What is standard, and what alwan can therefore give you, is the layout. */
    ALWAN_IT8_7_2 = 12
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
alwan_status alwan_interpolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                       alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                       alwan_interp_method method);
alwan_status alwan_interpolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                       alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                       alwan_interp_method method);

/* Cubic spline: C2 through every sample, with the node slopes from one global
 * tridiagonal solve. That solve is why this is its own entry point rather than an
 * alwan_interp_method: it needs count_in values of scratch, taken from ctx's
 * allocator, or the default one when ctx is NULL.
 *
 * NOT_A_KNOT is what "cubic spline" means in scipy.interpolate.CubicSpline by default
 * and in colour-science's CubicSplineInterpolator, and it is the one to use to match
 * them: the third derivative is continuous across the second and second-to-last
 * samples. NATURAL sets the second derivative to zero at both ends instead. The two
 * are different curves near the ends, measured 6.6e-2 and 0.12 apart on two noisy
 * spectra, so the textbook natural spline reproduces neither reference. Two samples
 * give the straight line under both; three under NOT_A_KNOT give the parabola, as
 * scipy does. */
typedef enum {
    ALWAN_SPLINE_NOT_A_KNOT = 0,
    ALWAN_SPLINE_NATURAL = 1
} alwan_spline_boundary;

/* x_in strictly increasing (a repeat or a NaN is ALWAN_E_INVALID), count_in >= 2,
 * count_out >= 1. Outputs outside [x_in[0], x_in[count_in - 1]] hold the end sample,
 * as alwan_interpolate does; scipy extrapolates the end cubic instead. ALWAN_E_NOMEM
 * if the scratch cannot be allocated. */
alwan_status alwan_interpolate_cubic_spline_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                                                alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                                                alwan_spline_boundary boundary, alwan_ctx *ctx);
alwan_status alwan_interpolate_cubic_spline_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                                                alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                                                alwan_spline_boundary boundary, alwan_ctx *ctx);

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
alwan_status alwan_extrapolate_f64(alwan_f64 const *x_in, alwan_f64 const *y_in, size_t count_in,
                       alwan_f64 const *x_out, alwan_f64 *y_out, size_t count_out,
                       alwan_extrap_method method);
alwan_status alwan_extrapolate_f32(alwan_f32 const *x_in, alwan_f32 const *y_in, size_t count_in,
                       alwan_f32 const *x_out, alwan_f32 *y_out, size_t count_out,
                       alwan_extrap_method method);

/* CCT and Duv Optimization
 * Computes Correlated Colour Temperature (CCT) and the signed distance from the
 * Planckian locus (Duv) by minimising Duv^2 over the locus.
 *
 * cct_out: receives CCT in Kelvin, clamped to [1667, 25000]
 * duv_out: receives signed Duv, positive above the locus and negative below,
 *          per Ohno 2013 / ANSI C78.377. May be NULL.
 * xy:      CIE 1931 xy chromaticity coordinates
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if xy or cct_out is NULL
 *
 * Accuracy, measured against colour-science Ohno 2013 over 1667-25000 K:
 *   Duv within 8.8e-05 everywhere tested
 *   CCT within 28 K (0.3%) for points on the locus, degrading to ~220 K at
 *   |Duv| = 0.01
 * The CCT residual is the cubic locus approximation this uses, not the
 * minimiser, which recovers its own locus to within 1e-13. An earlier version
 * of this comment claimed CCT <= 1 K; that was never achievable with an
 * approximated locus, and the solver of the time was in fact returning its
 * McCamy seed unrefined above 7300 K, for 5496 K of error at 25000 K. If you
 * need CCT tighter than this, the locus model has to change, not the search.
 *
 * Measured directly, which is the sharper way to say the same thing: take a
 * point that lies EXACTLY on the real Planckian locus, Planck's law integrated
 * against the 1931 observer's 1 nm colour matching functions, and this function
 * should return Duv = 0 and that point's own temperature. It returns
 *
 *     |Duv| up to 3.7e-04, worst at 1667 K, the bottom of the range
 *     CCT out by up to 26.9 K, which is 0.27 per cent
 *
 * and that gap is the locus model alone. For scale, an ANSI C78.377 bin is
 * 0.006 of Duv wide, so this is about six per cent of one. Suite 17 pins both
 * figures. If you need better, alwan_uv_to_cct_ohno2013_{T} reads a real
 * Planckian table rather than an approximation and is the accurate path. */
alwan_status alwan_cct_duv_optimize_f64(alwan_f64 *cct_out, alwan_f64 *duv_out, alwan_vec2_f64 const *xy);
alwan_status alwan_cct_duv_optimize_f32(alwan_f32 *cct_out, alwan_f32 *duv_out, alwan_vec2_f32 const *xy);

/* Tristimulus Optimization
 * Finds a spectral power distribution that MEETS the target XYZ under the given
 * observer, as a non-negative sum of seven Gaussians at 420, 470, 520, 570, 600,
 * 630 and 680 nm, 40 nm wide. Three equations in seven unknowns, solved for the
 * minimum-norm weights, so the match is exact rather than fitted: the recovered
 * SPD reproduces the target to about 1e-13 of Y through alwan_xyz_from_spd.
 * spd_out: receives the SPD. Its wavelength_min, wavelength_max and count are
 *          READ and honoured, and values[] must hold count entries. count = 0
 *          asks for the default grid, 380-780 nm at 81 samples, which is what
 *          this returned to every caller before 2026-09-22; values[] must then
 *          have room for 81. On any failure the values are set to zero.
 * target_xyz: target XYZ tristimulus values, on any scale
 * observer: any alwan_observer_type; it selects the CMFs the match is made under
 * ctx: context, or NULL
 * Returns ALWAN_OK on success,
 *         ALWAN_E_INVALID for a NULL argument or a grid of fewer than two samples,
 *         ALWAN_E_RANGE when the target is NOT a non-negative mixture of this
 *                       basis, which is the case for a saturated colour: the
 *                       three sRGB primaries all need a negative weight. No SPD
 *                       is returned in that case rather than one that misses the
 *                       target. For a saturated colour use
 *                       alwan_xyz_to_spectrum_otsu2018 or the RGB upsamplers,
 *                       which solve a better-posed problem.
 * OUTPUT CHANGED on 2026-09-22: before that this ignored the observer and the
 * target's X and Z, weighted every Gaussian by Y/7, forced an 81-sample
 * 380-780 nm grid over whatever the caller had allocated, and rejected the NULL
 * context it never used.
 * Note: Multiple SPDs can match the same XYZ (metamerism), this finds one solution */
alwan_status alwan_optimize_spectrum_for_xyz_f64(alwan_spd_f64 *spd_out, alwan_xyz_f64 const *target_xyz, alwan_observer_type observer, alwan_ctx *ctx);
alwan_status alwan_optimize_spectrum_for_xyz_f32(alwan_spd_f32 *spd_out, alwan_xyz_f32 const *target_xyz, alwan_observer_type observer, alwan_ctx *ctx);

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
alwan_status alwan_table_interp_3d_trilinear_f32(alwan_rgb_f32 *rgb_out,
                                     alwan_f32 const *table, size_t const sizes[3],
                                     alwan_rgb_f32 const *rgb_in);
alwan_status alwan_table_interp_3d_trilinear_f64(alwan_rgb_f64 *rgb_out,
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
alwan_status alwan_table_interp_3d_tetrahedral_f32(alwan_rgb_f32 *rgb_out,
                                       alwan_f32 const *table, size_t const sizes[3],
                                       alwan_rgb_f32 const *rgb_in);
alwan_status alwan_table_interp_3d_tetrahedral_f64(alwan_rgb_f64 *rgb_out,
                                       alwan_f64 const *table, size_t const sizes[3],
                                       alwan_rgb_f64 const *rgb_in);

/* ----------------------------------------------------------------
 * Data & Reference Sets
 * ---------------------------------------------------------------- */

/* Munsell Renotation Data
 * Convert Munsell notation (Hue, Value, Chroma) to XYZ tristimulus values
 * Uses the Munsell Renotation Data (1943)
 * xyz: receives XYZ tristimulus values
 * hue: Munsell hue [0, 100] (continuous)
 * value: Munsell value [0, 10]
 * chroma: Munsell chroma [0, 20+]
 * illuminant: illuminant for XYZ calculation
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_munsell_to_xyz_f64(alwan_xyz_f64 *xyz,
                         alwan_f64 hue, alwan_f64 value, alwan_f64 chroma,
                         alwan_illuminant illuminant);
alwan_status alwan_munsell_to_xyz_f32(alwan_xyz_f32 *xyz,
                         alwan_f32 hue, alwan_f32 value, alwan_f32 chroma,
                         alwan_illuminant illuminant);

/* Convert XYZ tristimulus values to Munsell notation (Hue, Value, Chroma)
 * hue: receives Munsell hue [0, 100]
 * value: receives Munsell value [0, 10]
 * chroma: receives Munsell chroma [0, 20+]
 * xyz: XYZ tristimulus values
 * illuminant: illuminant for XYZ calculation
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_xyz_to_munsell_f64(alwan_f64 *hue, alwan_f64 *value, alwan_f64 *chroma,
                         alwan_xyz_f64 const *xyz, alwan_illuminant illuminant);
alwan_status alwan_xyz_to_munsell_f32(alwan_f32 *hue, alwan_f32 *value, alwan_f32 *chroma,
                         alwan_xyz_f32 const *xyz, alwan_illuminant illuminant);

/* Color Checker Data
 * Get XYZ tristimulus values for a Color Checker patch
 * xyz: receives XYZ tristimulus values
 * type: Color Checker target type
 * illuminant: illuminant for XYZ calculation
 * patch_index: patch index [0, num_patches-1]
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on error */
alwan_status alwan_color_checker_data_f64(alwan_xyz_f64 *xyz,
                              alwan_colorchecker_type type, alwan_illuminant illuminant,
                              size_t patch_index);
alwan_status alwan_color_checker_data_f32(alwan_xyz_f32 *xyz,
                              alwan_colorchecker_type type, alwan_illuminant illuminant,
                              size_t patch_index);

/* Get number of patches in a Color Checker target
 * type: Color Checker target type
 * Returns number of patches, or 0 on error */
size_t alwan_color_checker_num_patches(alwan_colorchecker_type type);

/* The illuminant a target's published values are under, which is what alwan_color_checker_data
 * adapts FROM. D50 for most, Illuminant C for the 1976 Classic, D65 for the TE226. Returns
 * ALWAN_E_NODATA for a type alwan carries no measurements for. */
alwan_status alwan_color_checker_native_illuminant(alwan_illuminant *illuminant, alwan_colorchecker_type type);

/* One patch's reflectance spectrum, on the grid it was measured on. Creates the SPD, which the
 * caller destroys with alwan_spd_destroy. Hand it to alwan_xyz_from_spd with any illuminant and
 * observer: that is the right way to ask what a patch looks like under a light other than the
 * one its chart was published under, and it is not the same answer as adapting a tristimulus
 * table, which is an approximation of the question. Returns ALWAN_E_NODATA for a target alwan
 * has no spectra for. */
alwan_status alwan_color_checker_reflectance_f64(alwan_spd_f64 *out, alwan_colorchecker_type type, size_t patch_index, alwan_ctx *ctx);
alwan_status alwan_color_checker_reflectance_f32(alwan_spd_f32 *out, alwan_colorchecker_type type, size_t patch_index, alwan_ctx *ctx);

/* How many patches of a target alwan has spectra for, 0 when it has none. */
size_t alwan_color_checker_num_reflectances(alwan_colorchecker_type type);

/* The name of a patch in the order the data tables are stored: "dark skin" for a Classic, "A1"
 * for an SG or an IT8, "GS0" for the first grey of an IT8's ramp. NULL when alwan does not know
 * the target's layout or the index is past its end; the string is static.
 *
 * A name belongs to the target rather than to a measurement, so these are carried even where
 * the colorimetry is not, and alwan_color_checker_num_patch_names can therefore differ from
 * alwan_color_checker_num_patches: an IT8.7/2 has 288 patches and alwan has none of their
 * values. */
char const *alwan_color_checker_patch_name(alwan_colorchecker_type type, size_t patch_index);
size_t alwan_color_checker_num_patch_names(alwan_colorchecker_type type);

/* The rectangular colour field of a target: 6 by 4 for a Classic, 14 by 10 for an SG, 22 by 12
 * for an IT8.7/2 whose 24 greys follow that field in the name order. ALWAN_E_NODATA for a
 * target with no rectangular layout. */
alwan_status alwan_color_checker_grid(int *columns, int *rows, alwan_colorchecker_type type);

/* ----------------------------------------------------------------
 * Measured Charts
 *
 * The enum-keyed functions above answer for a target as a product, from values alwan embeds.
 * These answer for a target as an object, from the measurement file that shipped with it.
 *
 * That split is not a convenience. A ColorChecker has published values because every one is
 * meant to be the same chart. A professional target does not: an IT8 or a DT NGT2 is measured
 * per sheet or per batch, two off the same press differ, and a chart fades. ISO 12641 fixes an
 * IT8's layout and leaves the colorimetry to the manufacturer, which is why alwan carries
 * ALWAN_IT8_7_2's 288 patch names and none of its numbers. The numbers live in a file.
 *
 * alwan reads OpenQualia's Measurement File Standard (CGATS.17-2009 with a fixed set of header
 * keys, carried as .oqm.txt beside the target or reached from the QR label on it) and plain
 * CGATS batch reference files, which are the same structure. XYZ_* columns are used when
 * present, then LAB_*, then reflectance columns spelled SPEC_560, SPECTRAL_NM560 or nm560,
 * which are integrated against the file's own ILLUMINANT and OBSERVER.
 *
 * It also reads CxF3 (ISO 17972-1), which is what the spectrophotometer vendors write, into
 * the same object. The first byte that is not whitespace decides: an XML buffer goes to the
 * CxF reader, anything else to the CGATS one, so there is no format argument and nothing to
 * select. Every function below answers for either. In a CxF an Object is a patch,
 * ReflectanceSpectrum / ColorCIELab / ColorCIEXYZ carry the colorimetry in that same order of
 * preference, ColorCMYK and ColorRGB the device values, the ColorSpecification the illuminant,
 * observer and wavelength grid, and FileInformation the header keys. Element names are matched
 * on the local name, so cc:, cxf: and no prefix all read.
 *
 * A serial lookup is the application's job, and the pieces are here for it: read the QR code,
 * scan a directory of measurements, compare alwan_chart_header(chart, "SERIAL"), and on a miss
 * point the user at the vendor's measurement page. alwan does no network access.
 * ---------------------------------------------------------------- */

/* Which column family a loaded chart's XYZ came from. */
typedef enum {
    ALWAN_CHART_SOURCE_XYZ = 0,      /* XYZ_X / XYZ_Y / XYZ_Z, used as written */
    ALWAN_CHART_SOURCE_LAB = 1,      /* LAB_L / LAB_A / LAB_B, converted under the file's illuminant */
    ALWAN_CHART_SOURCE_SPECTRAL = 2  /* reflectance columns, integrated at load */
} alwan_chart_source;

/* The device values a file carried beside its colorimetry. */
typedef enum {
    ALWAN_CHART_DEVICE_NONE = 0,     /* no device columns */
    ALWAN_CHART_DEVICE_CMYK = 1,     /* CMYK_C, CMYK_M, CMYK_Y, CMYK_K, in percent */
    ALWAN_CHART_DEVICE_RGB = 2       /* RGB_R, RGB_G, RGB_B, in the file's own scale */
} alwan_chart_device;

/* Read a measurement file. The chart is allocated; release it with alwan_chart_destroy.
 * Returns ALWAN_E_INVALID for an unreadable or malformed file, ALWAN_E_NODATA for a
 * well-formed file that carries no colorimetry alwan can use. The buffer form does no file
 * I/O and does not take ownership of, or write to, the bytes it is given. */
alwan_status alwan_chart_load_f64(alwan_chart_f64 **out, char const *path, alwan_ctx *ctx);
alwan_status alwan_chart_load_f32(alwan_chart_f32 **out, char const *path, alwan_ctx *ctx);
alwan_status alwan_chart_load_buffer_f64(alwan_chart_f64 **out, char const *buf, size_t len, alwan_ctx *ctx);
alwan_status alwan_chart_load_buffer_f32(alwan_chart_f32 **out, char const *buf, size_t len, alwan_ctx *ctx);

void alwan_chart_destroy_f64(alwan_chart_f64 *chart, alwan_ctx *ctx);
void alwan_chart_destroy_f32(alwan_chart_f32 *chart, alwan_ctx *ctx);

/* One patch's XYZ under the file's own illuminant and observer, Y normalised to 1. Adapt it
 * with a CAT if you need another illuminant, or integrate the reflectance if the file has one:
 * those are different questions and the second is the better answer. */
alwan_status alwan_chart_xyz_f64(alwan_xyz_f64 *xyz, alwan_chart_f64 const *chart, size_t patch_index);
alwan_status alwan_chart_xyz_f32(alwan_xyz_f32 *xyz, alwan_chart_f32 const *chart, size_t patch_index);

size_t alwan_chart_num_patches_f64(alwan_chart_f64 const *chart);
size_t alwan_chart_num_patches_f32(alwan_chart_f32 const *chart);

/* SAMPLE_NAME if the file declared one, else SAMPLE_ID, else the 1-based row number. The
 * string belongs to the chart and dies with it. NULL past the end. */
char const *alwan_chart_patch_name_f64(alwan_chart_f64 const *chart, size_t patch_index);
char const *alwan_chart_patch_name_f32(alwan_chart_f32 const *chart, size_t patch_index);

/* The illuminant and observer the file declared, defaulting to D50 and the CIE 1931 2 degree
 * observer, which is what CGATS assumes when the keys are absent. */
alwan_status alwan_chart_native_illuminant_f64(alwan_illuminant *illuminant, alwan_observer_type *observer, alwan_chart_f64 const *chart);
alwan_status alwan_chart_native_illuminant_f32(alwan_illuminant *illuminant, alwan_observer_type *observer, alwan_chart_f32 const *chart);

alwan_chart_source alwan_chart_get_source_f64(alwan_chart_f64 const *chart);
alwan_chart_source alwan_chart_get_source_f32(alwan_chart_f32 const *chart);

/* A header value by key, case-insensitively: "SERIAL", "DESCRIPTOR", "TARGET_INSTRUMENT" and
 * whatever else the file carried. NULL when the file did not declare it. The _at form walks
 * every key in file order instead. Strings belong to the chart. */
char const *alwan_chart_header_f64(alwan_chart_f64 const *chart, char const *key);
char const *alwan_chart_header_f32(alwan_chart_f32 const *chart, char const *key);
size_t alwan_chart_num_headers_f64(alwan_chart_f64 const *chart);
size_t alwan_chart_num_headers_f32(alwan_chart_f32 const *chart);
alwan_status alwan_chart_header_at_f64(char const **key, char const **value, alwan_chart_f64 const *chart, size_t index);
alwan_status alwan_chart_header_at_f32(char const **key, char const **value, alwan_chart_f32 const *chart, size_t index);

/* One patch's reflectance on the grid the file measured it on. Creates the SPD, which the
 * caller destroys with alwan_spd_destroy. ALWAN_E_NODATA when the file carried no spectra.
 * alwan_chart_num_bands reports 0 in that case. */
alwan_status alwan_chart_reflectance_f64(alwan_spd_f64 *out, alwan_chart_f64 const *chart, size_t patch_index, alwan_ctx *ctx);
alwan_status alwan_chart_reflectance_f32(alwan_spd_f32 *out, alwan_chart_f32 const *chart, size_t patch_index, alwan_ctx *ctx);
size_t alwan_chart_num_bands_f64(alwan_chart_f64 const *chart);
size_t alwan_chart_num_bands_f32(alwan_chart_f32 const *chart);
/* The device values of a patch as the file wrote them: four for CMYK, three for RGB
 * with values_out[3] = 0. ALWAN_E_NODATA when the file carried none. */
alwan_chart_device alwan_chart_device_model_f64(alwan_chart_f64 const *chart);
alwan_chart_device alwan_chart_device_model_f32(alwan_chart_f32 const *chart);
alwan_status alwan_chart_device_values_f64(alwan_f64 *values_out, alwan_chart_f64 const *chart, size_t patch_index);
alwan_status alwan_chart_device_values_f32(alwan_f32 *values_out, alwan_chart_f32 const *chart, size_t patch_index);
/* The file's own LAB_L, LAB_A, LAB_B for a patch, as written, where alwan_chart_xyz
 * gives the tristimulus the loader derived. ALWAN_E_NODATA when the file had no Lab. */
alwan_status alwan_chart_lab_f64(alwan_lab_f64 *lab_out, alwan_chart_f64 const *chart, size_t patch_index);
alwan_status alwan_chart_lab_f32(alwan_lab_f32 *lab_out, alwan_chart_f32 const *chart, size_t patch_index);

/* CMYK printing characterisations: CMYK in [0, 1] to CIELAB relative to the data's
 * white (D50 for the ISO printing conditions), through an ISO 12642-2 (IT8.7/4) data
 * set. The target's complete CMY cubes on K = 0, 20, 40, 60, 80 and 100 are
 * interpolated multilinearly in Lab, then linearly in K; repeated patches are
 * averaged. alwan_cmyk_model_fogra39 builds from the embedded FOGRA39 (Fogra's
 * FOGRA39L data, ISO 12647-2:2004/Amd 1 coated paper, distributed unmodified with
 * Fogra as the source); _from_chart builds from any loaded IT8.7/4 set, such as a
 * CGATS.21 CRPC file, and is ALWAN_E_NODATA for a chart without CMYK or without the
 * IT8.7/4 cubes. CMYK outside [0, 1] is ALWAN_E_INVALID. */
alwan_status alwan_cmyk_model_from_chart_f64(alwan_cmyk_model **out, alwan_chart_f64 const *chart, alwan_ctx *ctx);
alwan_status alwan_cmyk_model_from_chart_f32(alwan_cmyk_model **out, alwan_chart_f32 const *chart, alwan_ctx *ctx);
alwan_status alwan_cmyk_model_fogra39(alwan_cmyk_model **out, alwan_ctx *ctx);
void alwan_cmyk_model_destroy(alwan_cmyk_model *model, alwan_ctx *ctx);
alwan_status alwan_cmyk_to_lab_f32(alwan_lab_f32 *lab_out, alwan_cmyk_f32 const *cmyk, alwan_cmyk_model const *model);
alwan_status alwan_cmyk_to_lab_f64(alwan_lab_f64 *lab_out, alwan_cmyk_f64 const *cmyk, alwan_cmyk_model const *model);

/* The other way: the CMYK that prints closest to a Lab, at the black the caller fixes.
 * The model is piecewise multilinear and a colour can be printed with more ink and less
 * black or the reverse, so the black is an input, not something to solve for: k in [0, 1]
 * is held and the search returns the C, M and Y nearest the target by CIEDE2000.
 * delta_e_out, which may be NULL, is what the search could not reach: zero or near it
 * inside the gamut, and the distance to the gamut's edge outside it. A colour the press
 * cannot print is not an error and is not clamped silently: the CMYK is the closest the
 * characterisation offers and the difference says how far it fell short. lab is relative
 * to the data's white, D50 for the ISO printing conditions. */
alwan_status alwan_lab_to_cmyk_f32(alwan_cmyk_f32 *cmyk_out, alwan_f32 *delta_e_out, alwan_lab_f32 const *lab,
                                   alwan_f32 k, alwan_cmyk_model const *model);
alwan_status alwan_lab_to_cmyk_f64(alwan_cmyk_f64 *cmyk_out, alwan_f64 *delta_e_out, alwan_lab_f64 const *lab,
                                   alwan_f64 k, alwan_cmyk_model const *model);

/* The same inverse, cached, so CMYK can be a per-pixel target.
 *
 * alwan_lab_to_cmyk_{T} scans a 729-node CMY cube and walks downhill from the best eight
 * of its nodes. Measured on the embedded FOGRA39 that is 1.2 ms a call, which makes a
 * 1920 x 1080 frame 42 minutes. alwan_cmyk_inverse_create runs that exact search once per
 * node of a regular grid over the Lab the characterisation can reach at one fixed black.
 * A query then interpolates the grid, scores the result and the eight corner inks of the
 * cell it landed in, and takes a short walk from the best: 25 us rather than 1.2 ms, so
 * the same frame is 53 seconds. Held to the exact search over 729 in-gamut targets, the
 * worst it falls behind is 0.0415 dE and the worst single ink differs by 0.0010.
 *
 * The build is size^3 exact searches and is the price of the cache: at size 17 that is
 * 4,913 of them, about 5 seconds. size is 4 to 64, and outside that is ALWAN_E_RANGE.
 *
 * A LAB OUTSIDE THE BOX THE CHARACTERISATION REACHES RUNS THE EXACT SEARCH INSTEAD, and
 * returns bit-identical results to calling it directly. The cache holds no data out there
 * and does not approximate where it has none. Far outside the gamut the difference has
 * several minima far apart in ink, and which one is deepest is found by that 729-node
 * scan; three separate attempts to substitute a small fixed set of starts for it changed
 * the answer by nothing, which is recorded in alwan_cmyk.c.
 *
 * The cost is real, so know which of your pixels pay it. Measured on a photograph, a
 * third fall outside the box and it converts at about 4x rather than 45x, and the reason
 * is not the saturated colour anyone expects: 29 per cent of that frame is below the
 * printable black, L 22.89 at k = 0, 4.9 per cent above the ceiling, and NONE outside on
 * a or b. What a press cannot hold in a photograph is the darkness. Flat saturated
 * artwork is the opposite case, outside on a and b, and has only a handful of distinct
 * colours, so convert its palette rather than its pixels.
 *
 * The black is fixed at build, as it is an input to the exact search: a colour can be
 * printed with more ink and less black or the reverse, so a cache spanning k would
 * interpolate between two different ink-sharing decisions and print neither.
 *
 * delta_e_out is MEASURED, not interpolated. The inks are put back through the forward
 * model and compared to the target by CIEDE2000, so the number includes the cache's own
 * error and cannot flatter itself. The map form reports the worst over the run.
 *
 * NOT TESTABLE AGAINST AN EXTERNAL REFERENCE: no library publishes an intermediate for
 * this. Argyll and littleCMS bake a B2A table into an ICC profile, the same idea in a
 * different container. Suite 143 holds it to alwan's own exact search, which is the
 * function it exists to approximate and so the oracle that means something. Data is
 * ISO 12642-2 (IT8.7/4); FOGRA39L is ISO 12647-2:2004/Amd 1 coated.
 *   https://www.color.org/icc_specs2.xalter  (B2A, the same idea in a profile)
 *   https://fogra.org
 */
alwan_status alwan_cmyk_inverse_create(alwan_cmyk_inverse **out, alwan_cmyk_model const *model, alwan_f64 k, int size, alwan_ctx *ctx);
void alwan_cmyk_inverse_destroy(alwan_cmyk_inverse *inv, alwan_ctx *ctx);
/* The grid size the cache was built at (nodes per axis), 0 for NULL. */
size_t alwan_cmyk_inverse_size(alwan_cmyk_inverse const *inv);
alwan_status alwan_cmyk_inverse_eval_f32(alwan_cmyk_f32 *cmyk_out, alwan_f32 *delta_e_out, alwan_lab_f32 const *lab, alwan_cmyk_inverse const *inv);
alwan_status alwan_cmyk_inverse_eval_f64(alwan_cmyk_f64 *cmyk_out, alwan_f64 *delta_e_out, alwan_lab_f64 const *lab, alwan_cmyk_inverse const *inv);
alwan_status alwan_cmyk_inverse_map_interleave_f32(alwan_f32 *cmyk_out, size_t out_stride, alwan_f32 const *lab_in, size_t in_stride, size_t count, alwan_f32 *worst_delta_e_out, alwan_cmyk_inverse const *inv);
alwan_status alwan_cmyk_inverse_map_interleave_f64(alwan_f64 *cmyk_out, size_t out_stride, alwan_f64 const *lab_in, size_t in_stride, size_t count, alwan_f64 *worst_delta_e_out, alwan_cmyk_inverse const *inv);

/* Write the chart back out, as OQM or as CGATS.17. The buffer form reports the length it
 * needs when called with a NULL buffer, and returns ALWAN_E_RANGE if what it was given
 * cannot hold the result and its terminator. Spectra are written when the chart has them.
 *
 * The two dialects differ in exactly two places, which is worth stating because it is
 * easy to assume a format change is bigger than it is: the identifier on the first line,
 * and how a reflectance column names its wavelength, SPECTRAL_NM560 against SPEC_560.
 * The rest, the header keys, NUMBER_OF_FIELDS, the DATA_FORMAT block, NUMBER_OF_SETS and
 * the data, is the same text in both, because CGATS.17's structure is what the OQM
 * writer already emitted.
 *
 * Neither writer invents a header. A CGATS file conventionally carries ORIGINATOR and
 * CREATED, and these do not add them, because a key that was not in the source chart
 * would come back from a reload as though it were and the round trip would stop being an
 * identity. Both dialects read back through alwan_chart_load, so that round trip is the
 * test of either. */
alwan_status alwan_chart_write_f64(char const *path, alwan_chart_f64 const *chart);
alwan_status alwan_chart_write_f32(char const *path, alwan_chart_f32 const *chart);
alwan_status alwan_chart_write_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_chart_f64 const *chart);
alwan_status alwan_chart_write_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_chart_f32 const *chart);
alwan_status alwan_chart_write_cgats17_f64(char const *path, alwan_chart_f64 const *chart);
alwan_status alwan_chart_write_cgats17_f32(char const *path, alwan_chart_f32 const *chart);
alwan_status alwan_chart_write_cgats17_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_chart_f64 const *chart);
alwan_status alwan_chart_write_cgats17_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_chart_f32 const *chart);

/* NCS (Natural Color System) Data
 * Convert NCS notation to XYZ tristimulus values
 * xyz: receives XYZ tristimulus values (Y=0--100 scale, D65)
 * ncs_notation: NCS notation string (e.g., "S 1050-Y90R")
 * Returns ALWAN_OK on success, ALWAN_E_INVALID on parse error
 * Approximate: uses published elementary-hue chromaticities (Hard & Sivik 1981)
 * with linear hue interpolation; does not reproduce the proprietary NCS atlas */
alwan_status alwan_ncs_to_xyz_f64(alwan_xyz_f64 *xyz, char const *ncs_notation);
alwan_status alwan_ncs_to_xyz_f32(alwan_xyz_f32 *xyz, char const *ncs_notation);

/* Convert XYZ tristimulus values to NCS notation
 * ncs_notation: receives NCS notation string (allocated by caller)
 * notation_size: size of notation buffer (should be >= 32)
 * xyz: XYZ tristimulus values
 * Returns ALWAN_E_INVALID -- inverse requires the proprietary NCS colour atlas */
alwan_status alwan_xyz_to_ncs_f64(char *ncs_notation, size_t notation_size, alwan_xyz_f64 const *xyz);
alwan_status alwan_xyz_to_ncs_f32(char *ncs_notation, size_t notation_size, alwan_xyz_f32 const *xyz);

/* Additional RGB Space Definitions
 * Get RGB space primaries and white point by enum
 * primaries: receives RGB primaries as xy chromaticities (3x2 matrix)
 * white_point: receives white point xy chromaticity
 * space: RGB color space identifier
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
alwan_status alwan_rgb_space_by_enum_f64(alwan_f64 primaries[6], alwan_vec2_f64 *white_point, alwan_rgb_space space);
alwan_status alwan_rgb_space_by_enum_f32(alwan_f32 primaries[6], alwan_vec2_f32 *white_point, alwan_rgb_space space);

/* Get RGB space transfer functions
 * oetf: receives OETF (Opto-Electronic Transfer Function)
 * eotf: receives EOTF (Electro-Optical Transfer Function)
 * space: RGB color space identifier
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if space is invalid */
alwan_status alwan_rgb_space_get_tfs_f64(alwan_transfer_function *oetf, alwan_transfer_function *eotf, alwan_rgb_space space);
alwan_status alwan_rgb_space_get_tfs_f32(alwan_transfer_function *oetf, alwan_transfer_function *eotf, alwan_rgb_space space);

/* ----------------------------------------------------------------
 * ITU-T H.273 code points (ISO/IEC 23091-2)
 *
 * colour_primaries, transfer_characteristics and matrix_coefficients, the colour
 * signalling of video streams and containers, to alwan's enums and back. Code 2,
 * unspecified, is ALWAN_E_NODATA; a reserved or out-of-range code is ALWAN_E_INVALID.
 * ---------------------------------------------------------------- */

/* H.273's chromaticities for a colour_primaries code: xy of R, G and B, and the white
 * point. */
alwan_status alwan_h273_color_primaries_f64(alwan_f64 primaries_xy[6], alwan_f64 white_xy[2], int color_primaries);
alwan_status alwan_h273_color_primaries_f32(alwan_f32 primaries_xy[6], alwan_f32 white_xy[2], int color_primaries);

/* The alwan space with a code's chromaticities, the linear one where alwan has it; 6
 * and 7 are the same. System M (4) carries Illuminant C as (0.31006, 0.31616), which
 * H.273 rounds to (0.310, 0.316). */
alwan_status alwan_h273_color_primaries_to_space(alwan_rgb_space *space_out, int color_primaries);

/* The lowest code whose primaries are the space's and whose white point agrees to
 * H.273's rounding (5e-4 in xy); ALWAN_E_NODATA when none does. */
alwan_status alwan_h273_color_primaries_from_space(int *color_primaries_out, alwan_rgb_space space);

/* The alwan curve for a transfer_characteristics code. 1 and 6 are BT.709's curve, 14
 * BT.2020's, 15 BT2020_12BIT, 16 PQ in cd/m2, 17 DCDM, 18 HLG. */
alwan_status alwan_h273_transfer_to_tf(alwan_transfer_function *tf_out, int transfer_characteristics);

/* The code for an alwan curve, ALWAN_E_NODATA for one H.273 does not list. ALWAN_TF_SRGB
 * is 13, the same on [0, 1]; below 0 sYCC mirrors the curve and alwan's sRGB continues
 * its linear segment. */
alwan_status alwan_h273_transfer_from_tf(int *transfer_characteristics_out, alwan_transfer_function tf);

/* Kr and Kb of a matrix_coefficients code. 12 and 13 derive them from the chromaticities
 * of color_primaries (H.273 equations 32 to 37), which the other codes ignore. 0
 * (identity), 8 (YCgCo), 11 (Y'D'zD'x) and 14 (ICtCp) have none: ALWAN_E_NODATA. */
alwan_status alwan_h273_matrix_coefficients_f64(alwan_f64 *kr_out, alwan_f64 *kb_out, int matrix_coefficients, int color_primaries);
alwan_status alwan_h273_matrix_coefficients_f32(alwan_f32 *kr_out, alwan_f32 *kb_out, int matrix_coefficients, int color_primaries);

/* ----------------------------------------------------------------
 * Color Correction & Grading Tools
 * ---------------------------------------------------------------- */

/* Lift/Gamma/Gain (LGG) color correction
 * rgb_out: output RGB values
 * rgb_in: input RGB values (linear, [0,1] for normal range)
 * lift: lift adjustment per channel (shadows) - typical range [-1, 1]
 * gamma: gamma adjustment per channel (midtones) - typical range [0.0001, 10]
 * gain: gain adjustment per channel (highlights) - typical range [0, 2]
 * Formula: rgb_out = ((rgb_in + lift) ^ (1/gamma)) * gain
 * Writes the result to rgb_out; this is a void function (no status returned). */
void alwan_lgg_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_rgb_f32 const *lift,
                    alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain);
void alwan_lgg_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_rgb_f64 const *lift,
                    alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);

/* Color matrix grading preset types */
typedef enum {
    ALWAN_COLOR_MATRIX_SEPIA = 0,           /* Sepia tone effect */
    ALWAN_COLOR_MATRIX_VINTAGE = 1, /* Vintage look */
    ALWAN_COLOR_MATRIX_BLEACH_BYPASS = 2, /* Bleach bypass effect */
    ALWAN_COLOR_MATRIX_COOL = 3, /* Cool tone shift */
    ALWAN_COLOR_MATRIX_WARM = 4, /* Warm tone shift */
    ALWAN_COLOR_MATRIX_MONOCHROME = 5, /* Black and white */
    ALWAN_COLOR_MATRIX_NIGHT_VISION = 6 /* Night vision look */
} alwan_color_matrix_preset_f64;
/* Historical naming kept; the enum is independent of precision. */
typedef alwan_color_matrix_preset_f64 alwan_color_matrix_preset_f32;

/* Apply color matrix transformation (custom or preset)
 * rgb_out: output RGB values
 * rgb_in: input RGB values
 * matrix_3x3: 3x3 color transformation matrix
 * Writes the result to rgb_out; this is a void function (no status returned). */
void alwan_color_matrix_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                              alwan_mat3x3_f32 const *matrix_3x3);
void alwan_color_matrix_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                              alwan_mat3x3_f64 const *matrix_3x3);

/* Get preset color grading matrix
 * matrix_3x3: receives the preset matrix
 * preset: preset type from alwan_color_matrix_preset_f64
 * Returns ALWAN_OK on success, ALWAN_E_INVALID for unknown preset */
alwan_status alwan_color_matrix_get_preset_f64(alwan_mat3x3_f64 *matrix_3x3, alwan_color_matrix_preset_f64 preset);
alwan_status alwan_color_matrix_get_preset_f32(alwan_mat3x3_f32 *matrix_3x3, alwan_color_matrix_preset_f32 preset);

/* Printer lights color correction (film-style)
 * rgb_out: output RGB values
 * rgb_in: input RGB values (linear)
 * red_lights: red printer light adjustment (0-50, default 25)
 * green_lights: green printer light adjustment (0-50, default 25)
 * blue_lights: blue printer light adjustment (0-50, default 25)
 * Each light unit represents approximately 0.025 log exposure change
 * Writes the result to rgb_out; this is a void function (no status returned). */
void alwan_printer_lights_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                alwan_f32 red_lights, alwan_f32 green_lights,
                                alwan_f32 blue_lights);
void alwan_printer_lights_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                alwan_f64 red_lights, alwan_f64 green_lights,
                                alwan_f64 blue_lights);

/* Color Correction Batch Map Functions */
alwan_status alwan_lgg_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f32 const *lift, alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain);
alwan_status alwan_lgg_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);

/* ----------------------------------------------------------------
 * Tonescale-region grading
 *
 * Canham, Punnappurath and Brown, "Adaptive Color Grading" (arXiv:2609.21169):
 * the tonescale split into four overlapping regions, darkest, dark, light and
 * lightest, by the pixel's intensity (the mean of its encoded RGB), each
 * region carrying a CIELAB a*b* offset. A region's membership is
 * clip(1 + slope (intensity - pivot), 0, 1): 1 on its own side of the pivot,
 * falling to 0 at |slope| per unit of intensity. The regions apply in that
 * order, each on the previous one's output, as
 *   v = v + w (encode(rgb(lab(decode(v)) + offset)) - v)
 * with the Lab white the space's own; the running value is held in [0, 1]
 * between regions, not after the last, as the authors' tables take their
 * input on that domain and return their output as it is (without the clamp a
 * light pixel's -10 then +10 on b* cancel where they should not). Input and
 * output are display-encoded RGB
 * in `space` (the paper grades Display P3); the pixel is evaluated, where the
 * authors' code evaluates a 17^3 table, and suite 179 holds the two to the
 * table's interpolation. The paper's thresholds come from a KNN over frame
 * histograms trained on annotations that cannot ship (no licence), so the
 * pivots are the caller's: a colourist's, or a predictor of their own.
 * The weight and the blend are cores (core/alwan_tonescale_grade_core.h).
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_f64 pivot[4];      /* darkest, dark, light, lightest; on the [0, 1] intensity scale */
    alwan_f64 slope[4];      /* -10, -5, 5, 10 in the paper: negative for the dark pair, positive for the light */
    alwan_f64 offset_a[4];   /* CIELAB a* offset per region */
    alwan_f64 offset_b[4];   /* CIELAB b* offset per region */
} alwan_tonescale_grade_params;

/* The paper's pivots and slopes, zero offsets (the identity). */
void alwan_tonescale_grade_params_init(alwan_tonescale_grade_params *params);

/* The four memberships at one intensity. */
alwan_status alwan_tonescale_grade_weights_f64(alwan_f64 weights_out[4], alwan_f64 intensity, alwan_tonescale_grade_params const *params);
alwan_status alwan_tonescale_grade_weights_f32(alwan_f32 weights_out[4], alwan_f32 intensity, alwan_tonescale_grade_params const *params);

/* One pixel, and buffers. Evaluation runs in double on both paths. Strides in
 * bytes, 0 packed. ALWAN_E_INVALID for a NULL, a parameter that is not finite,
 * a descriptor whose transfer functions are unknown or whose white has no y. */
alwan_status alwan_tonescale_grade_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_rgb_space_desc_f64 const *space, alwan_tonescale_grade_params const *params);
alwan_status alwan_tonescale_grade_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_rgb_space_desc_f32 const *space, alwan_tonescale_grade_params const *params);
alwan_status alwan_tonescale_grade_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_tonescale_grade_params const *params);
alwan_status alwan_tonescale_grade_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_tonescale_grade_params const *params);
/* Planar twin: the scalar per pixel over three planes under one stride. */
alwan_status alwan_tonescale_grade_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_rgb_space_desc_f64 const *space, alwan_tonescale_grade_params const *params);
alwan_status alwan_tonescale_grade_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_rgb_space_desc_f32 const *space, alwan_tonescale_grade_params const *params);
alwan_status alwan_lgg_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);
alwan_status alwan_color_matrix_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_mat3x3_f32 const *matrix);
alwan_status alwan_color_matrix_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_mat3x3_f64 const *matrix);
alwan_status alwan_color_matrix_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_mat3x3_f64 const *matrix);
alwan_status alwan_printer_lights_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_f32 red_lights, alwan_f32 green_lights, alwan_f32 blue_lights);
alwan_status alwan_printer_lights_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);
alwan_status alwan_printer_lights_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);

/* ----------------------------------------------------------------
 * OpenColorIO grading: primary
 *
 * OCIO's GradingPrimaryTransform (OpenColorIO 2.x, BSD-3-Clause), ALWAN_GRADING_PRIMARY: the primary
 * controls of a grading panel in three styles, each a fixed chain:
 *
 *   ALWAN_GRADING_LOG    brightness (in units of 6.25 / 1023), contrast about
 *                        0.5 + 0.5 * pivot, gamma between pivot_black and
 *                        pivot_white: for log-encoded images
 *   ALWAN_GRADING_LIN    offset, exposure in stops, contrast as a power about
 *                        0.18 * 2^pivot: for scene-linear images
 *   ALWAN_GRADING_VIDEO  offset, lift and gain about pivot_black and pivot_white,
 *                        gamma: for display-referred video
 *
 * then saturation about Rec.709 luma and a clamp to [clamp_black, clamp_white], in
 * every style. Each control is red, green, blue and master: added for brightness,
 * offset, exposure and lift, multiplied for contrast, gamma and gain. inverse = 1
 * runs the chain backwards. A lin contrast of 0.5 about the default pivot halves the
 * log distance of every value from 0.2034 (OCIO's default pivot parameter is 0.18
 * STOPS above 0.18, and init reproduces it); set pivot = 0 to pivot at 0.18 itself.
 *
 * alwan_grading_params_init gives OCIO's defaults for the style, an identity grade.
 * ALWAN_E_INVALID for a NULL, a style outside the enum, a non-finite control, a gamma
 * below 0.01 (log and video) or a lin contrast below 0.01, pivot_white less than 0.01
 * above pivot_black, or clamp_black above clamp_white. Computed in double; suite 184
 * holds it to PyOpenColorIO's float32 render.
 * ---------------------------------------------------------------- */

typedef enum {
    ALWAN_GRADING_LOG = 0,
    ALWAN_GRADING_LIN = 1,
    ALWAN_GRADING_VIDEO = 2
} alwan_grading_style;

typedef struct {
    alwan_f64 red, green, blue, master;
} alwan_grading_rgbm;

typedef struct {
    alwan_grading_rgbm brightness;   /* log */
    alwan_grading_rgbm contrast;     /* log, lin */
    alwan_grading_rgbm gamma;        /* log, video */
    alwan_grading_rgbm offset;       /* lin, video */
    alwan_grading_rgbm exposure;     /* lin, in stops */
    alwan_grading_rgbm lift;         /* video */
    alwan_grading_rgbm gain;         /* video */
    alwan_f64 saturation;
    alwan_f64 pivot;                 /* log: -1..1 around 0.5; lin: stops from 0.18 */
    alwan_f64 pivot_black, pivot_white;
    alwan_f64 clamp_black, clamp_white;   /* -DBL_MAX and DBL_MAX: no clamp */
} alwan_grading_primary;


/* ----------------------------------------------------------------
 * OpenColorIO grading: tone
 *
 * OCIO's GradingToneTransform: five zones of the tonescale, each moved by its own
 * piecewise-quadratic curve, and an S-contrast. Every zone takes red, green, blue and
 * master values around 1 (the channel's curve is applied, then the master's to all
 * three), and a start and width that place it on the scale:
 *
 *   blacks, whites     0.1..1.9: the slope at the bottom and top ends (start, width)
 *   shadows            0.2..1.8: start is where the zone ends, width is its PIVOT,
 *                      which must sit at least 0.01 below start
 *   highlights         0.2..1.8: start is where the zone begins, width its pivot,
 *                      at least 0.01 above start
 *   midtones           0.1..1.9: centre (start) and width of the bump
 *   scontrast          0.01..1.99, about 0.4 (log, video) or 0 (lin, log domain)
 *
 * Applied in that order: midtones, highlights, whites, shadows, blacks, S-contrast;
 * the inverse runs backwards. The lin style converts to OCIO's log domain first (a
 * log2 of 0.18-relative values with a linear toe below 0.0041) and back at the end.
 * The result is clamped above at 65504, the half-float maximum, as OCIO does; an
 * identity grade is a pass-through with no clamp.
 *
 * alwan_grading_params_init gives OCIO's defaults for the style, an identity grade.
 * ALWAN_E_INVALID for a NULL, a style outside the enum, a non-finite value, a value
 * outside the zone's range, a width below 0.01, shadows or highlights whose pivot
 * crosses their start, or an S-contrast outside [0.01, 1.99]. Computed in double;
 * suite 185 holds it to PyOpenColorIO.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_f64 red, green, blue, master, start, width;
} alwan_grading_rgbmsw;

typedef struct {
    alwan_grading_rgbmsw blacks, shadows, midtones, highlights, whites;
    alwan_f64 scontrast;
} alwan_grading_tone;


/* ----------------------------------------------------------------
 * OpenColorIO grading: RGB curves
 *
 * OCIO's GradingRGBCurveTransform: a curve per channel and a master curve applied to
 * all three after them, each a monotone B-spline through control points, as a
 * grading panel's curves tab. The spline is OCIO's GradingBSplineCurve: slopes are
 * estimated from the points unless the caller gives them (all zero means estimate),
 * and the curve continues straight beyond its end points. The x and y of the points
 * must each be non-decreasing, since the curve is inverted by solving it. The lin
 * style converts to OCIO's log domain first and back after, so its points are in
 * that domain (stops from 0.18; the default curve runs from -7 to 7).
 *
 * The spline is fitted in float, as OCIO fits it: its fit branches on float
 * thresholds, and a fit in double could take the other branch. Evaluation is in
 * double. A curve holds up to ALWAN_GRADING_CURVE_MAX_POINTS points; OCIO also caps
 * the four curves together at 120 knots, which this does not.
 *
 * alwan_grading_params_init sets OCIO's default, a three-point identity for every
 * curve. An identity set is a pass-through. ALWAN_E_INVALID for a NULL, a style
 * outside the enum, a curve with fewer than two or more than the maximum points, a
 * non-finite value, or a decreasing x or y. Suite 186 holds it to PyOpenColorIO.
 * ---------------------------------------------------------------- */

#define ALWAN_GRADING_CURVE_MAX_POINTS 32

typedef struct {
    alwan_f64 x, y;
} alwan_grading_point;

typedef struct {
    int count;
    alwan_grading_point points[ALWAN_GRADING_CURVE_MAX_POINTS];
    alwan_f64 slopes[ALWAN_GRADING_CURVE_MAX_POINTS];   /* all zero: estimated from the points */
} alwan_grading_curve;

typedef struct {
    alwan_grading_curve red, green, blue, master;
} alwan_grading_rgb_curve;


/* ----------------------------------------------------------------
 * OpenColorIO grading: hue curves
 *
 * OCIO's GradingHueCurveTransform: eight curves in OCIO's HSY space (hue with magenta
 * at 0, a saturation scaled per style, Rec.709 luma), the hue-selective controls of a
 * grading panel. Hue runs over [0, 1) and the hue curves repeat with that period:
 *
 *   hue_hue   hue -> hue           rotate chosen hues toward others (diagonal)
 *   hue_sat   hue -> sat gain      saturate or mute chosen hues (1 = no change)
 *   hue_lum   hue -> lum gain      brighten or darken chosen hues, less at low sat
 *   lum_sat   lum -> sat gain      e.g. mute the shadows
 *   sat_sat   sat -> sat           a saturation curve (diagonal)
 *   lum_lum   lum -> lum           a luma curve (diagonal)
 *   sat_lum   sat -> lum gain      brighten or darken by saturation
 *   hue_fx    hue -> hue offset    a hue shift added last (0 = no change)
 *
 * applied in OCIO's order, and each a spline of the GradingBSplineCurve family with
 * the type OCIO gives that role (periodic for the hue curves). In the lin style the
 * luma curves see OCIO's log domain, so lum_sat and lum_lum points run from -7 to 7
 * by default, and the luma gains multiply rather than add. OCIO can skip the HSY
 * conversion; this always converts, OCIO's default.
 *
 * alwan_grading_params_init sets OCIO's defaults for the style, an identity. The
 * points of a hue curve are wrapped into [0, 1), sorted and spaced as OCIO prepares
 * them; the other curves need non-decreasing x, and the diagonal ones (hue_hue,
 * sat_sat, lum_lum) non-decreasing y too, hue_hue with its x in [0, 1]. ALWAN_E_INVALID
 * for a NULL, a style outside the enum, a curve with fewer than two or more than the
 * maximum points, a non-finite value, or points that break those rules. Suite 187
 * holds it to PyOpenColorIO.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_grading_curve hue_hue, hue_sat, hue_lum, lum_sat, sat_sat, lum_lum, sat_lum, hue_fx;
} alwan_grading_hue_curve;


/* ----------------------------------------------------------------
 * OpenColorIO grading: exposure and contrast
 *
 * OCIO's ExposureContrastTransform, ALWAN_GRADING_EXPOSURE_CONTRAST: exposure in stops
 * and contrast (times gamma) about a pivot, in the three styles:
 *
 *   ALWAN_GRADING_LIN    pow(in 2^exposure / pivot, contrast) pivot, negatives to 0
 *   ALWAN_GRADING_VIDEO  the same with 2^exposure and the pivot raised to 1 / 1.83,
 *                        for display-referred video
 *   ALWAN_GRADING_LOG    in contrast + (exposure step - p) contrast + p, with
 *                        p = log2(pivot / 0.18) step + mid_gray, for log-encoded images
 *
 * Contrast and pivot are floored at 0.001; a contrast of exactly 1 only scales. OCIO's
 * inverse log style ignores the exposure step it is given (it uses 0.088), so it does not
 * invert its own forward transform at any other step; this inverse uses the step given.
 * ALWAN_E_INVALID for a style outside the enum or a non-finite value. Suite 205 holds it
 * to PyOpenColorIO.
 * ---------------------------------------------------------------- */

typedef struct {
    alwan_f64 exposure;           /* stops; 0: none */
    alwan_f64 contrast;           /* 1: none */
    alwan_f64 gamma;              /* multiplies contrast; 1: none */
    alwan_f64 pivot;              /* scene-linear value kept fixed; 0.18 */
    alwan_f64 log_exposure_step;  /* LOG: code values a stop; 0.088 */
    alwan_f64 log_mid_gray;       /* LOG: the code value of 0.18; 0.435 */
} alwan_grading_exposure_contrast;

/* ----------------------------------------------------------------
 * OpenColorIO grading: the family
 *
 * One entry point for every grading operation above. `op` picks the operation and reads
 * its block of params; `style` is the working space (log, lin or video) for all of them;
 * inverse = 1 runs the operation backwards. alwan_grading_params_init gives every block
 * OCIO's defaults for the style, an identity grade, and params NULL is that identity.
 * The map form grades count pixels of three values, stride bytes apart; out may be in.
 * ALWAN_E_INVALID for a NULL pixel pointer, a stride under three values, an unknown op,
 * or anything the operation's own rules above refuse.
 * ---------------------------------------------------------------- */

typedef enum {
    ALWAN_GRADING_PRIMARY = 0,           /* GradingPrimaryTransform */
    ALWAN_GRADING_TONE = 1,              /* GradingToneTransform */
    ALWAN_GRADING_RGB_CURVE = 2,         /* GradingRGBCurveTransform */
    ALWAN_GRADING_HUE_CURVE = 3,         /* GradingHueCurveTransform */
    ALWAN_GRADING_EXPOSURE_CONTRAST = 4  /* ExposureContrastTransform */
} alwan_grading_op;

typedef struct {
    alwan_grading_primary primary;
    alwan_grading_tone tone;
    alwan_grading_rgb_curve rgb_curve;
    alwan_grading_hue_curve hue_curve;
    alwan_grading_exposure_contrast exposure_contrast;
} alwan_grading_params;

void alwan_grading_params_init(alwan_grading_params *params, alwan_grading_style style);

alwan_status alwan_grading_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in, alwan_grading_op op, alwan_grading_style style, alwan_grading_params const *params, int inverse);
alwan_status alwan_grading_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in, alwan_grading_op op, alwan_grading_style style, alwan_grading_params const *params, int inverse);
alwan_status alwan_grading_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_grading_op op, alwan_grading_style style, alwan_grading_params const *params, int inverse);
alwan_status alwan_grading_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_grading_op op, alwan_grading_style style, alwan_grading_params const *params, int inverse);

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
 * out: output array (must be at least 'terms' elements)
 * rgb: input RGB triplet [0,1]
 * terms: number of terms (from alwan_poly_cheung_terms)
 * Returns ALWAN_OK on success */
alwan_status alwan_poly_expand_cheung2004_f64(alwan_f64 *out, alwan_rgb_f64 const *rgb,
                                  alwan_poly_cheung_terms terms);
alwan_status alwan_poly_expand_cheung2004_f32(alwan_f32 *out, alwan_rgb_f32 const *rgb,
                                  alwan_poly_cheung_terms terms);

/* Polynomial expansion - Finlayson 2015 method
 * out: output array (size depends on degree: 3,6,10,15 for degrees 1,2,3,4)
 * out_size: receives actual output size
 * rgb: input RGB triplet [0,1]
 * degree: polynomial degree (1-4)
 * root_poly: if true, use root-polynomial expansion
 * Returns ALWAN_OK on success */
alwan_status alwan_poly_expand_finlayson2015_f64(alwan_f64 *out, int *out_size,
                                     alwan_rgb_f64 const *rgb, int degree, int root_poly);
alwan_status alwan_poly_expand_finlayson2015_f32(alwan_f32 *out, int *out_size,
                                     alwan_rgb_f32 const *rgb, int degree, int root_poly);

/* Polynomial expansion - Vandermonde method
 * out: output array
 * out_size: receives actual output size
 * a: input array (typically RGB)
 * a_size: size of input array
 * degree: polynomial degree
 * Returns ALWAN_OK on success */
alwan_status alwan_poly_expand_vandermonde_f64(alwan_f64 *out, int *out_size,
                                   alwan_f64 const *a, int a_size, int degree);
alwan_status alwan_poly_expand_vandermonde_f32(alwan_f32 *out, int *out_size,
                                   alwan_f32 const *a, int a_size, int degree);

/* Compute colour correction matrix using Cheung 2004 method
 * matrix_out: receives the correction matrix (terms x 3, row-major)
 * M_T: test (measured) RGB values, Nx3 array (row-major)
 * M_R: reference RGB values, Nx3 array (row-major)
 * num_samples: number of color samples (N)
 * terms: polynomial expansion terms
 * Returns ALWAN_OK on success */
alwan_status alwan_colour_correction_matrix_cheung2004_f64(alwan_f64 *matrix_out,
                                               alwan_f64 const *M_T,
                                               alwan_f64 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms);
alwan_status alwan_colour_correction_matrix_cheung2004_f32(alwan_f32 *matrix_out,
                                               alwan_f32 const *M_T,
                                               alwan_f32 const *M_R,
                                               int num_samples,
                                               alwan_poly_cheung_terms terms);

/* Apply colour correction using Cheung 2004 method
 * rgb_out: corrected RGB output
 * rgb: input RGB to correct
 * matrix: correction matrix from alwan_colour_correction_matrix_cheung2004_f64
 * terms: must match terms used to compute the matrix
 * Writes the result to rgb_out; this is a void function (no status returned). */
void alwan_colour_correct_cheung2004_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                     alwan_f32 const *matrix, alwan_poly_cheung_terms terms);
void alwan_colour_correct_cheung2004_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                                     alwan_f64 const *matrix, alwan_poly_cheung_terms terms);

/* Compute colour correction matrix using Finlayson 2015 method
 * matrix_out: receives the correction matrix
 * matrix_size: receives matrix size
 * M_T: test (measured) RGB values, Nx3 array (row-major)
 * M_R: reference RGB values, Nx3 array (row-major)
 * num_samples: number of color samples (N)
 * degree: polynomial degree (1-4)
 * root_poly: if true, use root-polynomial expansion
 * Returns ALWAN_OK on success */
alwan_status alwan_colour_correction_matrix_finlayson2015_f64(alwan_f64 *matrix_out, int *matrix_size,
                                                  alwan_f64 const *M_T,
                                                  alwan_f64 const *M_R,
                                                  int num_samples, int degree, int root_poly);
alwan_status alwan_colour_correction_matrix_finlayson2015_f32(alwan_f32 *matrix_out, int *matrix_size,
                                                  alwan_f32 const *M_T,
                                                  alwan_f32 const *M_R,
                                                  int num_samples, int degree, int root_poly);

/* Apply colour correction using Finlayson 2015 method
 * rgb_out: corrected RGB output
 * rgb: input RGB to correct
 * matrix: correction matrix from alwan_colour_correction_matrix_finlayson2015_f64
 * degree: must match degree used to compute the matrix
 * root_poly: must match root_poly used to compute the matrix
 * Writes the result to rgb_out; this is a void function (no status returned). */
void alwan_colour_correct_finlayson2015_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                                        alwan_f32 const *matrix, int degree, int root_poly);
void alwan_colour_correct_finlayson2015_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                                        alwan_f64 const *matrix, int degree, int root_poly);

/* Buffer form, strides in bytes; the scalar in a loop, validated once, bit-identical to it (suite 174). */
alwan_status alwan_colour_correct_cheung2004_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 const *matrix, alwan_poly_cheung_terms terms);
alwan_status alwan_colour_correct_finlayson2015_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 const *matrix, int degree, int root_poly);
alwan_status alwan_colour_correct_cheung2004_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 const *matrix, alwan_poly_cheung_terms terms);
alwan_status alwan_colour_correct_finlayson2015_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 const *matrix, int degree, int root_poly);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 174). */
alwan_status alwan_colour_correct_cheung2004_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_f32 const *matrix, alwan_poly_cheung_terms terms);
alwan_status alwan_colour_correct_finlayson2015_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_f32 const *matrix, int degree, int root_poly);
alwan_status alwan_colour_correct_cheung2004_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_f64 const *matrix, alwan_poly_cheung_terms terms);
alwan_status alwan_colour_correct_finlayson2015_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_f64 const *matrix, int degree, int root_poly);

/* How a colour correction matrix is fitted. The zero value, or NULL, is the plain
 * least-squares fit of alwan_colour_correction_matrix_*, bit for bit.
 *
 *     minimise  sum_i w_i |reference_i - expanded(test_i) X|^2 + ridge |X|_F^2
 *
 * weights: one per sample, finite and not negative, f64 in both precisions; NULL for
 *          uniform. A weight of 0 drops a patch that glared or is scratched; weighting
 *          the neutral ramp or skin higher spends the fit's accuracy there.
 * ridge:   Tikhonov regularisation, finite and not negative, on every coefficient
 *          alike; 0 for none. It shrinks the high-order terms when the term count
 *          nears the patch count, and lets a fit have more terms than patches.
 * solver:  ALWAN_CCM_SOLVER_QR (the zero value) or ALWAN_CCM_SOLVER_SVD. QR refuses a
 *          rank-deficient system with ALWAN_E_DIVZERO: duplicate patches, a clipped
 *          channel, a chart short of levels. The SVD answers it with the
 *          minimum-norm fit over the numerical rank, and answers with fewer samples
 *          than terms too.
 * rcond:   SVD only: singular values at or below rcond x the largest count as zero;
 *          0 is numpy's default, DBL_EPSILON x the larger of rows and terms.
 * rank_out: optional; receives the rank the fit used. For QR that is the term count,
 *          or -1 when QR finds the system rank deficient.
 * Matches scikit-learn's Ridge(fit_intercept=False) with sample_weight on colour's
 * expansions, which are these term for term, and with the SVD solver
 * numpy.linalg.lstsq, rank included. */
typedef enum {
    ALWAN_CCM_SOLVER_QR  = 0, /* Householder QR on the expanded samples */
    ALWAN_CCM_SOLVER_SVD = 1  /* one-sided Jacobi SVD, the minimum-norm fit */
} alwan_ccm_solver;

typedef struct {
    alwan_f64 const *weights;
    alwan_f64 ridge;
    alwan_ccm_solver solver;
    alwan_f64 rcond;
    int *rank_out;
    /* Neutral-preserving constraint. Both NULL, or both set: three values each, the
     * measured neutral and what it must map to. The fit then reproduces that pair
     * EXACTLY, to round-off rather than to a tolerance, and is the best fit to
     * everything else subject to that.
     *
     * Give the neutral in the same units as M_T and its target in the units of M_R.
     * Each fit expands it with its own expansion, so a caller never builds the
     * expanded row and the same pair works for Cheung and Finlayson alike.
     *
     * A constraint costs residual everywhere else, which is the point: exactness at
     * one place is bought from the fit elsewhere. It also removes one degree of
     * freedom, so a fit that needed as many samples as terms now needs one fewer.
     *
     * rank_out, when asked for, still reports the rank the caller expects: the
     * constrained direction is determined rather than fitted, so a healthy fit of n
     * terms reports n.
     *
     * ALWAN_E_INVALID when only one of the two is set, when either holds a
     * non-finite value, or when the neutral expands to all zeros, which constrains
     * nothing and cannot be satisfied. */
    alwan_f64 const *neutral_in;
    alwan_f64 const *neutral_out;
    /* Robust loss. With robust_scale > 0 the fit minimises
     *
     *   sum_i w_i rho(|r_i| / robust_scale) + ridge |X|_F^2
     *
     * where r_i is sample i's residual over all three channels and rho is Huber's:
     * quadratic within robust_k of the scale, linear beyond it. 0 turns it off and is
     * the default, which leaves every fit exactly as it was.
     *
     * The residual is the NORM over the three channels, so a patch is an outlier as a
     * whole rather than per channel. That is what a glared or scratched patch is, and
     * it keeps a chart's colour from being pulled apart channel by channel.
     *
     * robust_scale is in the units of M_R and says how large a residual is still
     * ordinary; set it near the noise of a good patch. Solved by iteratively reweighted
     * least squares, so the caller's weights, the ridge, the solver choice and the
     * neutral constraint all keep working: each round is the same solve with the
     * weights multiplied by min(1, robust_k * robust_scale / |r_i|).
     *
     * ridge keeps meaning the penalty on the objective above, which is not the same
     * number as a ridge on a plain least squares: Huber's quadratic region carries a
     * 1/(2 scale^2) that the squared residual does not, and the library scales for it.
     * Without that a caller switching the robust loss on would silently have their
     * regularisation multiplied by 1/(2 scale^2), which is 200 at a scale of 0.05.
     *
     * ALWAN_E_INVALID for a robust_scale, robust_k, robust_iterations or robust_tol
     * that is negative or not finite. */
    alwan_f64 robust_scale;  /* Huber scale in the units of M_R; 0 is off */
    alwan_f64 robust_k;      /* tuning constant; 0 is 1.345, the usual 95% choice */
    int robust_iterations;   /* reweighting rounds at most; 0 is 50 */
    alwan_f64 robust_tol;    /* stop once no coefficient moves by more than this; 0 is 1e-12 */
} alwan_ccm_fit_params;

/* The two fits above with alwan_ccm_fit_params; the arguments before it are theirs.
 * Without a ridge the QR solver needs at least as many samples of non-zero weight as
 * terms. */
alwan_status alwan_ccm_fit_cheung2004_f64(alwan_f64 *matrix_out, alwan_f64 const *M_T, alwan_f64 const *M_R, int num_samples, alwan_poly_cheung_terms terms, alwan_ccm_fit_params const *params);
alwan_status alwan_ccm_fit_cheung2004_f32(alwan_f32 *matrix_out, alwan_f32 const *M_T, alwan_f32 const *M_R, int num_samples, alwan_poly_cheung_terms terms, alwan_ccm_fit_params const *params);
alwan_status alwan_ccm_fit_finlayson2015_f64(alwan_f64 *matrix_out, int *matrix_size, alwan_f64 const *M_T, alwan_f64 const *M_R, int num_samples, int degree, int root_poly, alwan_ccm_fit_params const *params);
alwan_status alwan_ccm_fit_finlayson2015_f32(alwan_f32 *matrix_out, int *matrix_size, alwan_f32 const *M_T, alwan_f32 const *M_R, int num_samples, int degree, int root_poly, alwan_ccm_fit_params const *params);

/* Leave-one-out: each sample of positive weight left out in turn, the rest fitted with
 * params (its weights, ridge and solver; rank_out is not written), and the fit's
 * prediction of the one left out compared with its reference. rms_out receives the root
 * mean square of those residuals over the samples predicted and the three channels, in
 * the reference's units. pred_out, when not NULL, receives num_samples x 3 predictions,
 * NaN for a sample of weight 0, for scoring in another space (dE, say). A fit that
 * learned the chart rather than the camera shows here and not in its own residuals.
 * When the samples left cannot fit the basis, the status is that fit's:
 * ALWAN_E_INVALID for fewer samples than terms, ALWAN_E_DIVZERO when QR finds the
 * system rank deficient. Matches scikit-learn's LeaveOneOut over LinearRegression and
 * Ridge. */
alwan_status alwan_ccm_loo_cheung2004_f64(alwan_f64 *rms_out, alwan_f64 *pred_out, alwan_f64 const *M_T, alwan_f64 const *M_R, int num_samples, alwan_poly_cheung_terms terms, alwan_ccm_fit_params const *params);
alwan_status alwan_ccm_loo_cheung2004_f32(alwan_f32 *rms_out, alwan_f32 *pred_out, alwan_f32 const *M_T, alwan_f32 const *M_R, int num_samples, alwan_poly_cheung_terms terms, alwan_ccm_fit_params const *params);
alwan_status alwan_ccm_loo_finlayson2015_f64(alwan_f64 *rms_out, alwan_f64 *pred_out, alwan_f64 const *M_T, alwan_f64 const *M_R, int num_samples, int degree, int root_poly, alwan_ccm_fit_params const *params);
alwan_status alwan_ccm_loo_finlayson2015_f32(alwan_f32 *rms_out, alwan_f32 *pred_out, alwan_f32 const *M_T, alwan_f32 const *M_R, int num_samples, int degree, int root_poly, alwan_ccm_fit_params const *params);

/* The Cheung term count with the lowest leave-one-out error, of all 14. rms_out, when not
 * NULL, receives the 14 errors in the enum's order (3, 4, 5, 7, 8, 10, 11, 14, 16, 17, 19,
 * 20, 22, 35), NaN for a count the samples left cannot fit. The smaller count wins a
 * tie. When no count can be fitted, the status is the first failure. */
alwan_status alwan_ccm_select_cheung2004_f64(alwan_poly_cheung_terms *terms_out, alwan_f64 *rms_out, alwan_f64 const *M_T, alwan_f64 const *M_R, int num_samples, alwan_ccm_fit_params const *params);
alwan_status alwan_ccm_select_cheung2004_f32(alwan_poly_cheung_terms *terms_out, alwan_f32 *rms_out, alwan_f32 const *M_T, alwan_f32 const *M_R, int num_samples, alwan_ccm_fit_params const *params);

/* Thin-plate spline in RGB (TPS-3D), Menesatti 2012 on Bookstein 1989: a smooth warp
 * through every control pair, exactly at smoothing 0, where a polynomial is a least-squares
 * fit whose residual on the patches is the price of its global shape. Reference:
 * colour-science's colour_correction_TPS3D, on its develop branch and not in the 0.4.7
 * release; suite 180 holds both kernels to it at 1e-12 through that code at a pinned commit.
 * (That code compares its kernel name after lowercasing it, so its own Bookstein branch is
 * unreachable and every fit it makes is polyharmonic; the generator runs the Bookstein
 * arithmetic the file contains, and says so. alwan reaches both.)
 *
 * Fit: the (N + 4) x (N + 4) system [K + s I, P; P^T, 0] [W; A] = [M_R; 0], K_ij =
 * U(|t_i - t_j|) with a zero diagonal, P = [1, R, G, B], solved by the Householder QR the
 * CCM fits use, in double whatever the precision (an f64-internal facade, see
 * ALWAN_WITH_F64_FACADE). weights_out receives (num_samples + 4) x 3 row-major: the N kernel
 * rows W, then the four affine rows A (constant, R, G, B). smoothing >= 0 relaxes the
 * interpolation (Tikhonov on K); 0 passes through the pairs to round-off. At least four
 * samples. ALWAN_E_INVALID for a NULL, fewer, a non-finite sample, a negative or non-finite
 * smoothing or a kernel outside the enum; ALWAN_E_DIVZERO when the system is singular
 * (coincident or coplanar control points).
 *
 * Apply: out = sum_j W_j U(|rgb - t_j|) + A_0 + A_R R + A_G G + A_B B in the pixel's own
 * precision, clamped to [0, 1] when the model's clip is set (the reference's default). The
 * model points at the caller's weights and at the control points the fit was given, M_T,
 * the same N x 3 buffer, which must outlive it. A void scalar like the polynomial applies:
 * a NULL or incomplete model writes nothing. The maps validate the model once and are the
 * scalar in a loop, bit-identical to it.
 *
 * Kernels: U(r) = r^2 log r^2 with r^2 floored at 1e-12 before the log (Bookstein, the
 * reference's floor, kept so a pixel on a control point returns its number), or U(r) = r,
 * the polyharmonic spline the three-dimensional thin plate is. */
typedef enum {
    ALWAN_TPS3D_KERNEL_BOOKSTEIN = 0,    /* r^2 log r^2 */
    ALWAN_TPS3D_KERNEL_POLYHARMONIC = 1  /* r */
} alwan_tps3d_kernel;

typedef struct {
    alwan_f32 const *weights;   /* (num_samples + 4) x 3 from alwan_tps3d_fit_f32 */
    alwan_f32 const *control;   /* num_samples x 3: the M_T the fit was given */
    int num_samples;
    alwan_tps3d_kernel kernel;  /* the kernel the fit used */
    int clip;                   /* nonzero: clamp the result to [0, 1] */
} alwan_tps3d_model_f32;

typedef struct {
    alwan_f64 const *weights;
    alwan_f64 const *control;
    int num_samples;
    alwan_tps3d_kernel kernel;
    int clip;
} alwan_tps3d_model_f64;

alwan_status alwan_tps3d_fit_f32(alwan_f32 *weights_out, alwan_f32 const *M_T, alwan_f32 const *M_R, int num_samples, alwan_f32 smoothing, alwan_tps3d_kernel kernel);
alwan_status alwan_tps3d_fit_f64(alwan_f64 *weights_out, alwan_f64 const *M_T, alwan_f64 const *M_R, int num_samples, alwan_f64 smoothing, alwan_tps3d_kernel kernel);
void alwan_tps3d_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb, alwan_tps3d_model_f32 const *model);
void alwan_tps3d_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb, alwan_tps3d_model_f64 const *model);
/* Buffer forms, strides in bytes; the scalar in a loop, the model validated once (suite 180). */
alwan_status alwan_tps3d_apply_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_tps3d_model_f32 const *model);
alwan_status alwan_tps3d_apply_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_tps3d_model_f64 const *model);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 180). */
alwan_status alwan_tps3d_apply_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_tps3d_model_f32 const *model);
alwan_status alwan_tps3d_apply_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_tps3d_model_f64 const *model);

/* White balance multipliers from neutral gray measurement
 * Given a measured RGB value that should be neutral gray,
 * computes the multipliers to normalize it.
 * measured_gray: measured RGB of a neutral gray target
 * multipliers_out: receives RGB multipliers (normalized so min = 1.0)
 * Writes the gain/matrix output; this is a void function (no status returned). */
void alwan_white_balance_from_gray_f32(alwan_rgb_f32 *multipliers_out, alwan_rgb_f32 const *measured_gray);
void alwan_white_balance_from_gray_f64(alwan_rgb_f64 *multipliers_out, alwan_rgb_f64 const *measured_gray);

/* Apply white balance multipliers
 * rgb_out: white-balanced RGB output
 * rgb: input RGB
 * multipliers: RGB multipliers from alwan_white_balance_from_gray
 * Writes the result to rgb_out; this is a void function (no status returned). */
void alwan_white_balance_apply_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb,
                               alwan_rgb_f32 const *multipliers);
void alwan_white_balance_apply_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb,
                               alwan_rgb_f64 const *multipliers);

/* White Balance Batch Map Functions */
alwan_status alwan_white_balance_apply_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f32 const *multipliers);
alwan_status alwan_white_balance_apply_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_rgb_f64 const *multipliers);
alwan_status alwan_white_balance_apply_map_interleave_ex(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *multipliers);

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
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if out or out_count is NULL,
 *         or ALWAN_E_RANGE if wavelength_step <= 0 or wavelength_start > wavelength_end */
alwan_status alwan_rayleigh_spd_f64(alwan_f64 wavelength_start, alwan_f64 wavelength_end,
                        alwan_f64 wavelength_step,
                        alwan_atmosphere_params_f64 const *params,
                        alwan_f64 *out, int *out_count);
alwan_status alwan_rayleigh_spd_f32(alwan_f32 wavelength_start, alwan_f32 wavelength_end,
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
    ALWAN_ACES1_OUT_REC709_100NIT = 0, /* Rec.709, 100 nits, BT.1886 */
    ALWAN_ACES1_OUT_SRGB_100NIT = 1, /* sRGB, 100 nits, sRGB EOTF */
    ALWAN_ACES1_OUT_SRGB_D60_100NIT = 2, /* sRGB (D60 sim), 100 nits */

    /* P3 Displays */
    ALWAN_ACES1_OUT_P3DCI_48NIT = 3, /* P3-DCI, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D60_48NIT = 4, /* P3-D60, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D65_48NIT = 5, /* P3-D65, 48 nits, Gamma 2.6 */
    ALWAN_ACES1_OUT_P3D65_100NIT = 6, /* P3-D65 (Display P3), 100 nits */

    /* Rec.2020 Displays */
    ALWAN_ACES1_OUT_REC2020_100NIT = 7, /* Rec.2020, 100 nits, BT.1886 */
    /* ACES 1.1 to 1.3 RRTODT.Academy.Rec2020_*nits_15nits_ST2084: the SSTS tone scale, 0.18
     * at 15 cd/m2, which is what every current ACES config ships. */
    ALWAN_ACES1_OUT_REC2020_1000NIT_PQ = 8, /* Rec.2020, 1000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_2000NIT_PQ = 9, /* Rec.2020, 2000 nits, PQ */
    ALWAN_ACES1_OUT_REC2020_4000NIT_PQ = 10, /* Rec.2020, 4000 nits, PQ */

    /* Cinema */
    ALWAN_ACES1_OUT_DCDM_48NIT = 11, /* DCDM X'Y'Z', 48 nits, Gamma 2.6 */

    /* ACES 1.0.3 ODT.Academy.Rec2020_ST2084_*nits: the earlier HDR ODTs, C5 + C9 splines with
     * 0.18 at 10 cd/m2. Kept under their own names; the values above are the current transforms. */
    ALWAN_ACES1_OUT_REC2020_1000NIT_PQ_V103 = 12, /* Rec.2020, 1000 nits, PQ, ACES 1.0.3 */
    ALWAN_ACES1_OUT_REC2020_2000NIT_PQ_V103 = 13, /* Rec.2020, 2000 nits, PQ, ACES 1.0.3 */
    ALWAN_ACES1_OUT_REC2020_4000NIT_PQ_V103 = 14, /* Rec.2020, 4000 nits, PQ, ACES 1.0.3 */

    ALWAN_ACES1_OUT_COUNT = 15
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
 * @param interp   Tone curve method; ALWAN_ACES_INTERP_BSPLINE is the Academy
 *                 reference, ALWAN_ACES_INTERP_OCIO promises OCIO's pixels
 * @return         ALWAN_OK; ALWAN_E_INVALID for a NULL, a preset or a method
 *                 outside its enum
 */
alwan_status alwan_aces1_output_transform_f32(alwan_rgb_f32 *rgb_out,
                                      alwan_rgb_f32 const *rgb_in,
                                      alwan_aces1_output output,
                                      alwan_aces_interp interp);
alwan_status alwan_aces1_output_transform_f64(alwan_rgb_f64 *rgb_out,
                                      alwan_rgb_f64 const *rgb_in,
                                      alwan_aces1_output output,
                                      alwan_aces_interp interp);

/* The same transform over a buffer of RGB triples, strides in bytes. The
 * preset is validated once; each pixel is the scalar transform, so a map and
 * its scalar twin agree to the bit. Same status codes. */
alwan_status alwan_aces1_output_transform_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_aces1_output output, alwan_aces_interp interp);
alwan_status alwan_aces1_output_transform_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_aces1_output output, alwan_aces_interp interp);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_aces1_output_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_aces1_output output, alwan_aces_interp interp);
alwan_status alwan_aces1_output_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_aces1_output output, alwan_aces_interp interp);

/**
 * ACES 1.x Output Transform (Inverse)
 *
 * Inverse of the ACES 1.x output transform for round-trip workflows. It
 * inverts the B-spline (and SSTS) chain: a forward run under HERMITE or OCIO
 * does not round-trip through it.
 *
 * @param rgb_out  Output RGB in ACES2065-1 (AP0 linear)
 * @param rgb_in   Input RGB, display-encoded
 * @param output   Output transform preset (display configuration)
 * @return         ALWAN_OK on success
 */
alwan_status alwan_aces1_output_transform_inv_f32(alwan_rgb_f32 *rgb_out,
                                          alwan_rgb_f32 const *rgb_in,
                                          alwan_aces1_output output);
alwan_status alwan_aces1_output_transform_inv_f64(alwan_rgb_f64 *rgb_out,
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
    ALWAN_ACES2_OUT_REC709_100NIT_BT1886 = 0, /* Rec.709, 100 nits, BT.1886 EOTF */
    ALWAN_ACES2_OUT_SRGB_100NIT = 1, /* sRGB, 100 nits, sRGB EOTF */
    ALWAN_ACES2_OUT_P3D65_100NIT_SRGB = 2, /* Display P3, 100 nits, sRGB piecewise */
    ALWAN_ACES2_OUT_P3D65_100NIT_G22 = 3, /* Display P3, 100 nits, Gamma 2.2 */

    /* HDR Displays (PQ / ST.2084) */
    ALWAN_ACES2_OUT_P3D65_1000NIT_PQ = 4, /* Display P3, 1000 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_500NIT_PQ = 5, /* Rec.2100, 500 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_1000NIT_PQ = 6, /* Rec.2100, 1000 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_2000NIT_PQ = 7, /* Rec.2100, 2000 nits, PQ */
    ALWAN_ACES2_OUT_REC2100_4000NIT_PQ = 8, /* Rec.2100, 4000 nits, PQ */

    /* HDR Displays (HLG) */
    ALWAN_ACES2_OUT_REC2100_1000NIT_HLG = 9, /* Rec.2100, 1000 nits, HLG */

    /* Cinema */
    ALWAN_ACES2_OUT_DCDM_48NIT = 10, /* DCDM X'Y'Z', 48 nits, Gamma 2.6 */
    ALWAN_ACES2_OUT_P3DCI_48NIT = 11, /* P3-DCI, 48 nits, Gamma 2.6 */

    ALWAN_ACES2_OUT_COUNT = 12
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
alwan_status alwan_aces2_output_transform_f32(alwan_rgb_f32 *rgb_out,
                                      alwan_rgb_f32 const *rgb_in,
                                      alwan_aces2_output output);
alwan_status alwan_aces2_output_transform_f64(alwan_rgb_f64 *rgb_out,
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
alwan_status alwan_aces2_output_transform_inv_f32(alwan_rgb_f32 *rgb_out,
                                          alwan_rgb_f32 const *rgb_in,
                                          alwan_aces2_output output);
alwan_status alwan_aces2_output_transform_inv_f64(alwan_rgb_f64 *rgb_out,
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
alwan_status alwan_aces2_output_transform_custom_f32(alwan_rgb_f32 *rgb_out,
                                             alwan_rgb_f32 const *rgb_in,
                                             alwan_f32 peak_luminance,
                                             alwan_aces_primaries_f32 const *limit_primaries,
                                             alwan_transfer_function eotf);
alwan_status alwan_aces2_output_transform_custom_f64(alwan_rgb_f64 *rgb_out,
                                             alwan_rgb_f64 const *rgb_in,
                                             alwan_f64 peak_luminance,
                                             alwan_aces_primaries_f64 const *limit_primaries,
                                             alwan_transfer_function eotf);

/* Display-linear (pre-encode) front half of the custom ACES 2.0 output
 * transform: tonescale + chroma compression + gamut compression, decoded in
 * the LIMIT primaries -- WITHOUT the [0,peak] clamp + display encode (eotf).
 * Out-of-gamut / over-range residuals are preserved (values can exceed [0,1]).
 * alwan_aces2_output_transform_custom == this + clamp + display encode (eotf), exactly. */
alwan_status alwan_aces2_output_transform_custom_display_linear_f32(alwan_rgb_f32 *rgb_out,
                                             alwan_rgb_f32 const *rgb_in,
                                             alwan_f32 peak_luminance,
                                             alwan_aces_primaries_f32 const *limit_primaries);
alwan_status alwan_aces2_output_transform_custom_display_linear_f64(alwan_rgb_f64 *rgb_out,
                                             alwan_rgb_f64 const *rgb_in,
                                             alwan_f64 peak_luminance,
                                             alwan_aces_primaries_f64 const *limit_primaries);

/**
 * ACES 2.0 Output Transform (Batch)
 * Pre-initializes parameters once, then processes count pixels.
 * Much faster than calling alwan_aces2_output_transform per pixel.
 *
 * @param out        Output interleaved RGB triplets (display-encoded)
 * @param out_stride Bytes between output RGB triplets
 * @param in         Input interleaved RGB triplets (AP1 linear)
 * @param in_stride  Bytes between input RGB triplets (typically 3*sizeof(alwan_f64))
 * @param count      Number of pixels
 * @param output     Output transform preset
 * @return           ALWAN_OK on success
 */
alwan_status alwan_aces2_output_transform_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_aces2_output output);
alwan_status alwan_aces2_output_transform_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_aces2_output output);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_aces2_output_transform_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_aces2_output output);
alwan_status alwan_aces2_output_transform_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_aces2_output output);

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
alwan_status alwan_maxcll_f32(alwan_f32 *maxcll_out, alwan_f32 const *rgb_in, size_t stride, size_t count);
alwan_status alwan_maxcll_f64(alwan_f64 *maxcll_out, alwan_f64 const *rgb_in, size_t stride, size_t count);

/* Maximum Frame Average Light Level (average of per-pixel max R/G/B) */
alwan_status alwan_maxfall_f32(alwan_f32 *maxfall_out, alwan_f32 const *rgb_in, size_t stride, size_t count);
alwan_status alwan_maxfall_f64(alwan_f64 *maxfall_out, alwan_f64 const *rgb_in, size_t stride, size_t count);

/* ----------------------------------------------------------------
 * Arbitrary Gamma Transfer Function
 * ---------------------------------------------------------------- */

/* Apply arbitrary gamma OETF: linear -> pow(linear, 1/gamma) */
alwan_status alwan_gamma_oetf_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 gamma);
alwan_status alwan_gamma_oetf_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 gamma);

/* Apply arbitrary gamma EOTF: encoded -> pow(encoded, gamma) */
alwan_status alwan_gamma_eotf_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 gamma);
alwan_status alwan_gamma_eotf_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 gamma);

/* ----------------------------------------------------------------
 * D-Series Illuminant from CCT
 * ---------------------------------------------------------------- */

/* Compute D-series illuminant xy chromaticity from CCT (4000-25000K)
 * xy_out: receives 2 values (x, y chromaticity)
 * cct: correlated color temperature in Kelvin */
alwan_status alwan_d_series_illuminant_xy_f64(alwan_vec2_f64 *xy_out, alwan_f64 cct);
alwan_status alwan_d_series_illuminant_xy_f32(alwan_vec2_f32 *xy_out, alwan_f32 cct);

/* ----------------------------------------------------------------
 * Weber & Michelson Contrast
 * ---------------------------------------------------------------- */

/* Weber contrast: (L_target - L_background) / L_background */
alwan_status alwan_weber_contrast_f64(alwan_f64 *result, alwan_f64 L_target, alwan_f64 L_bg);
alwan_status alwan_weber_contrast_f32(alwan_f32 *result, alwan_f32 L_target, alwan_f32 L_bg);

/* Michelson contrast: (L_max - L_min) / (L_max + L_min) */
alwan_status alwan_michelson_contrast_f64(alwan_f64 *result, alwan_f64 L_max, alwan_f64 L_min);
alwan_status alwan_michelson_contrast_f32(alwan_f32 *result, alwan_f32 L_max, alwan_f32 L_min);

/* ----------------------------------------------------------------
 * Accessibility Contrast Metrics
 * ---------------------------------------------------------------- */

/* WCAG 2.x Contrast Ratio: (L_lighter + 0.05) / (L_darker + 0.05)
 * Y1, Y2: relative luminances [0,1] of two colors
 * Returns contrast ratio [1, 21] (AA: >= 4.5, AAA: >= 7.0) */
alwan_status alwan_wcag_contrast_ratio_f32(alwan_f32 *result, alwan_f32 Y1, alwan_f32 Y2);
alwan_status alwan_wcag_contrast_ratio_f64(alwan_f64 *result, alwan_f64 Y1, alwan_f64 Y2);

/* APCA / SAPC (Advanced Perceptual Contrast Algorithm -- WCAG 3.0 draft)
 * Reference: Myndex APCA-W3, https://github.com/Myndex/apca-w3
 * srgb_text, srgb_bg: sRGB-encoded colors (0..1 per channel)
 * Lc_out: perceptual contrast value (positive = dark on light,
 *         negative = light on dark; magnitude is contrast level) */
alwan_status alwan_apca_contrast_f32(alwan_f32 *Lc_out,
                         alwan_rgb_f32 const *srgb_text,
                         alwan_rgb_f32 const *srgb_bg);
alwan_status alwan_apca_contrast_f64(alwan_f64 *Lc_out,
                         alwan_rgb_f64 const *srgb_text,
                         alwan_rgb_f64 const *srgb_bg);

/* ----------------------------------------------------------------
 * HDR Ecosystem: BT.2446 Methods B & C, BT.2390 EETF
 * ---------------------------------------------------------------- */

/* BT.2446 Method B: SDR to HDR up-conversion (parametric)
 * Y_sdr: input SDR luminance [0,1]
 * L_hdr: target HDR peak luminance (cd/m2)
 * L_sdr: source SDR peak luminance (cd/m2) */
alwan_status alwan_bt2446b_forward_f32(alwan_f32 *Y_hdr_out, alwan_f32 Y_sdr,
                            alwan_f32 L_hdr, alwan_f32 L_sdr);
alwan_status alwan_bt2446b_forward_f64(alwan_f64 *Y_hdr_out, alwan_f64 Y_sdr,
                            alwan_f64 L_hdr, alwan_f64 L_sdr);

/* BT.2446 Method C: HDR to SDR tone mapping (quantization-aware)
 * Y_hdr: input HDR luminance (PQ-encoded) [0,1]
 * L_hdr: peak HDR luminance (cd/m2)
 * L_sdr: peak SDR luminance (cd/m2) */
alwan_status alwan_bt2446c_forward_f32(alwan_f32 *Y_sdr_out, alwan_f32 Y_hdr,
                            alwan_f32 L_hdr, alwan_f32 L_sdr);
alwan_status alwan_bt2446c_forward_f64(alwan_f64 *Y_sdr_out, alwan_f64 Y_hdr,
                            alwan_f64 L_hdr, alwan_f64 L_sdr);

/* BT.2390 EETF: PQ-domain tone mapping (Hermite spline)
 * E_pq: PQ-encoded input [0,1]
 * LB, LW: source black/white levels (PQ-encoded)
 * LB_target, LW_target: target black/white levels (PQ-encoded) */
alwan_status alwan_bt2390_eetf_f32(alwan_f32 *E_out, alwan_f32 E_pq,
                       alwan_f32 LB, alwan_f32 LW,
                       alwan_f32 LB_target, alwan_f32 LW_target);
alwan_status alwan_bt2390_eetf_f64(alwan_f64 *E_out, alwan_f64 E_pq,
                       alwan_f64 LB, alwan_f64 LW,
                       alwan_f64 LB_target, alwan_f64 LW_target);

/* BT.2390 EETF with luminance parameters (cd/m2) */
alwan_status alwan_bt2390_eetf_luminance_f32(alwan_f32 *E_out, alwan_f32 E_pq,
                                 alwan_f32 L_source_peak,
                                 alwan_f32 L_target_peak);
alwan_status alwan_bt2390_eetf_luminance_f64(alwan_f64 *E_out, alwan_f64 E_pq,
                                 alwan_f64 L_source_peak,
                                 alwan_f64 L_target_peak);

/* Exposure-based tone mapping: 1 - exp(-2^exposure * L)
 * exposure: EV offset (0 = neutral, +1 = 1 stop brighter) */
alwan_status alwan_exposure_tonemap_f32(alwan_f32 *out, alwan_f32 L,
                            alwan_f32 exposure);
alwan_status alwan_exposure_tonemap_f64(alwan_f64 *out, alwan_f64 L,
                            alwan_f64 exposure);

/* Reinhard calibrated (key-based with white point adaptation)
 * key: exposure key (0.18 = standard 18% gray)
 * L_avg: log-average luminance of the scene
 * L_white: smallest luminance mapped to pure white */
alwan_status alwan_reinhard_calibrated_f32(alwan_f32 *out, alwan_f32 L,
                               alwan_f32 key, alwan_f32 L_avg,
                               alwan_f32 L_white);
alwan_status alwan_reinhard_calibrated_f64(alwan_f64 *out, alwan_f64 L,
                               alwan_f64 key, alwan_f64 L_avg,
                               alwan_f64 L_white);

/* ----------------------------------------------------------------
 * Global tone mapping operators
 *
 * The eleven global operators of colour-hdri over an image of interleaved RGB, plus
 * Drago 2003: the simple mappings collected by Banterle et al. 2011, Schlick 1994,
 * Tumblin, Hodgins and Guenter 1999, Reinhard and Devlin 2005 and Hable 2010. All but SIMPLE, GAMMA
 * and FILMIC read statistics of the whole image (its peak or log-average luminance),
 * so a pixel's result depends on every other pixel: map an image in one call, not
 * in tiles. alwan_tonemap_params_{T} is declared with the other parameter structs.
 * ---------------------------------------------------------------- */
typedef enum {
    ALWAN_TONEMAP_SIMPLE = 0,                 /* RGB / (RGB + 1), per channel */
    ALWAN_TONEMAP_NORMALIZATION = 1,          /* RGB / peak luminance */
    ALWAN_TONEMAP_GAMMA = 2,                  /* (2^ev RGB)^(1/gamma), per channel */
    ALWAN_TONEMAP_LOGARITHMIC = 3,            /* log10(1 + qL) / log10(1 + k L_max) */
    ALWAN_TONEMAP_EXPONENTIAL = 4,            /* 1 - exp(-qL / (k L_avg)), L_avg the log average */
    ALWAN_TONEMAP_LOGARITHMIC_MAPPING = 5,    /* (ln(1 + pL) / ln(1 + p L_max))^(1/q) */
    ALWAN_TONEMAP_EXPONENTIATION_MAPPING = 6, /* (L / L_max)^(p/q) */
    ALWAN_TONEMAP_SCHLICK1994 = 7,            /* pL / (pL - L + L_max) */
    ALWAN_TONEMAP_TUMBLIN1999 = 8,            /* Tumblin, Hodgins and Guenter 1999 */
    ALWAN_TONEMAP_REINHARD2004 = 9,           /* Reinhard and Devlin 2005, photoreceptor model */
    ALWAN_TONEMAP_FILMIC = 10,                /* Hable 2010, per channel */
    ALWAN_TONEMAP_DRAGO2003 = 11              /* Drago 2003, adaptive logarithmic mapping */
} alwan_tonemap_operator;

/* Tone maps count pixels, rows stride bytes apart; out may be in. params NULL is every
 * default. The luminance operators, NORMALIZATION and LOGARITHMIC to REINHARD2004,
 * take luminance as the weighted sum of R, G and B, and need every luminance finite
 * and not negative. NORMALIZATION divides by the peak luminance (a zero peak gives
 * black).
 *
 * REINHARD2004's global adaptation level, which light_adaptation below 1 mixes in,
 * is the paper's: I_g = c * mean(channel) + (1 - c) * mean(luminance), with both means
 * ARITHMETIC (Reinhard and Devlin 2005, p. 17, and Fig. 7's source). The log average
 * of luminance enters only the automatic contrast's key. colour-hdri takes the log
 * average in I_g as well, and alwan followed it until 3.0.0; on suite 130's image the
 * two are 0.314 of output apart. OpenCV's TonemapReinhard and pfstmo's reinhard05
 * take the mean, and suite 130 holds alwan to OpenCV where the term has an effect.
 *
 * REINHARD2004 maps each channel against its adaptation level; a zero channel
 * stays zero, and with chromatic_adaptation above 0 no channel may be negative. The
 * others scale RGB by L_d / L and map a pixel with zero luminance to black. SIMPLE,
 * GAMMA and FILMIC apply their formula to each value as it is. ALWAN_E_INVALID for an
 * unknown operator, a luminance or channel out of range as above, and a parameter out
 * of range: a negative p, q, k, gamma, contrast or display value, q or k below 1 for
 * LOGARITHMIC and EXPONENTIAL (colour-hdri raises them to 1), an adaptation outside
 * [0, 1], or automatic_contrast together with a contrast.
 *
 * DRAGO2003 is the adaptive logarithmic mapping of Drago, Myszkowski, Annen and
 * Chiba, "Adaptive Logarithmic Mapping For Displaying High Contrast Scenes",
 * Eurographics 2003. Luminance is divided by its own log average, and
 *
 *     L_d = (L_dmax / 100) / log10(L_wmax + 1)
 *           * log(L_w + 1) / log(2 + 8 (L_w / L_wmax)^(log b / log 0.5))
 *
 * so the base of the logarithm slides with the luminance: near black it is 2,
 * at the peak it is 10, and drago_bias sets how it moves between them. The
 * paper's recommended range is 0.7 to 0.9 and the default is 0.85. L_dmax is
 * display_peak, in cd/m2, which Tumblin 1999 also reads. drago_bias outside
 * (0, 1] is ALWAN_E_INVALID.
 *
 * Because the log average divides out, this operator is invariant to a uniform
 * scale of its input: doubling every pixel gives the same picture. That is the
 * operator behaving as published, not a normalisation applied on top.
 *
 * THE OPERATOR IS NOT MONOTONE AT EVERY BIAS, and that is the published
 * formula rather than anything alwan does to it. The denominator grows like
 * L^(log b / log 0.5) while the numerator grows like log L, so for a small
 * enough bias on a wide enough scene the denominator wins and a brighter pixel
 * comes out darker. Measured on a luminance ramp, the first inversion appears
 * at about
 *
 *     bias 0.50   3 decades of scene range
 *     bias 0.70   5 decades
 *     bias 0.80   8 decades
 *     bias 0.85   none up to 8 decades, and the same above it
 *
 * so the default is safe on anything a camera produces, while 0.70, which is
 * inside the paper's own recommended range of 0.7 to 0.9, inverts tones on a
 * five-decade scene. Suite 169 pins both ends of that. alwan does not clamp it:
 * the operator is what it is, and a caller choosing a low bias for a very high
 * range scene should know what it does rather than have it quietly repaired.
 *
 * OpenCV's TonemapDrago wraps the same formula in two steps of its own, an
 * affine rescale of the input to [0, 1] and a min-max rescale of the output,
 * and drops the 1 / log10(L_wmax + 1) term that the rescale makes redundant.
 * Suite 169 applies those two steps around alwan's result and matches OpenCV
 * to 4e-07 in float32, so what differs is the framing rather than the operator.
 *
 * OpenCV's gamma and saturation, which alwan does not have here (ALWAN_TONEMAP_GAMMA
 * is its own operator), raise an intermediate to a fractional power without
 * guarding a negative one, so they return NaN on some images and not others.
 * Suite 169 records which, since a defect that depends on the data is worth
 * pinning rather than describing. */
alwan_status alwan_tonemap_global_f64(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_tonemap_operator op, alwan_tonemap_params_f64 const *params);
alwan_status alwan_tonemap_global_f32(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_tonemap_operator op, alwan_tonemap_params_f32 const *params);

/* ----------------------------------------------------------------
 * Local tone mapping: Mantiuk 2006
 *
 * Every operator above is a curve: a pixel's result depends on that pixel and on
 * statistics of the image. This one is not. It takes the log luminance apart into
 * contrasts at every scale, shrinks each of them through a response curve, and then
 * solves for the image whose contrasts those are. A pixel's result therefore depends
 * on its neighbours, and on their neighbours, which is what lets it hold local texture
 * while losing global range, and which is also why it takes a width and a height
 * rather than a count.
 *
 * rgb_in and rgb_out are interleaved RGB, rows a stride apart; out may be in. params
 * NULL is every default. scale is how hard contrasts are pulled in, 0 reading as 0.7,
 * and larger means flatter. saturation is the exponent on each channel's ratio to
 * luminance, 0 reading as 1. iterations_out, which may be NULL, receives how many
 * conjugate gradient steps the solve took; it stops on a relative residual of 1e-3 and
 * caps at 100, and in practice it takes five to ten, so a result at the cap is worth
 * looking at.
 *
 * The luminance weights follow the rest of alwan: zero is the sRGB primaries' Y row.
 * OpenCV uses Rec.601 luma, 0.299, 0.587 and 0.114, which is a different quantity and
 * the wrong one for linear light; suite 170 passes it explicitly to compare.
 *
 * The output is NOT normalised. The solve is anchored at the image's own mean log
 * luminance, which is what the iteration's starting point fixes, so the result has a
 * meaningful level and the caller decides what to do with it. An image smaller than
 * 2 x 2 is ALWAN_E_RANGE, and a negative or non-finite luminance is ALWAN_E_INVALID.
 *
 * Reproduces OpenCV's TonemapMantiuk to 2e-06 in float32, which is that type's
 * precision, once OpenCV's own framing is taken off: it rescales its input to [0, 1]
 * before the operator and its output to [0, 1] afterwards, neither of which is in the
 * paper. Published in Mantiuk, Myszkowski and Seidel, "A Perceptual Framework for
 * Contrast Processing of High Dynamic Range Images", ACM TAP 3(3), 2006.
 * ---------------------------------------------------------------- */

alwan_status alwan_tonemap_mantiuk2006_f64(alwan_f64 *rgb_out, size_t out_row_stride, alwan_f64 const *rgb_in, size_t in_row_stride, size_t width, size_t height, alwan_tonemap_local_params_f64 const *params, int *iterations_out);
alwan_status alwan_tonemap_mantiuk2006_f32(alwan_f32 *rgb_out, size_t out_row_stride, alwan_f32 const *rgb_in, size_t in_row_stride, size_t width, size_t height, alwan_tonemap_local_params_f32 const *params, int *iterations_out);

/* ----------------------------------------------------------------
 * BT.2408: HLG and PQ in display light
 *
 * ITU-R BT.2408 converts between the two BT.2100 systems through display light on
 * a reference display, with HDR reference white at 203 cd/m2 (58 % PQ, 75 % HLG).
 * Inputs and outputs are three values per pixel, R G B; the primaries are BT.2020
 * on both sides and are not touched.
 *
 * hlg_peak_nits is the nominal peak of the HLG reference display, 0 for
 * 1000 cd/m2. The HLG system gamma follows it, 1.2 + 0.42 log10(Lw / 1000)
 * (BT.2100-2), with black at 0 cd/m2. sdr_white_nits is where SDR 100 % lands, 0 for
 * BT.2408's 203 cd/m2. A negative level is ALWAN_E_INVALID.
 * ---------------------------------------------------------------- */

/* HLG to PQ: the HLG EOTF of the reference display, then the PQ inverse EOTF. */
alwan_status alwan_bt2408_hlg_to_pq_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 hlg_peak_nits);
alwan_status alwan_bt2408_hlg_to_pq_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 hlg_peak_nits);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_bt2408_hlg_to_pq_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_f32 hlg_peak_nits);
alwan_status alwan_bt2408_hlg_to_pq_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_f64 hlg_peak_nits);

/* PQ to HLG: the PQ EOTF, then the HLG inverse EOTF of the reference display. PQ can
 * carry light above that display's peak: clip_to_peak non-zero limits it to the peak
 * first, as BT.2408 describes; zero keeps it, as an HLG signal above 1. */
alwan_status alwan_bt2408_pq_to_hlg_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 hlg_peak_nits, int clip_to_peak);
alwan_status alwan_bt2408_pq_to_hlg_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 hlg_peak_nits, int clip_to_peak);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_bt2408_pq_to_hlg_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_f32 hlg_peak_nits, int clip_to_peak);
alwan_status alwan_bt2408_pq_to_hlg_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_f64 hlg_peak_nits, int clip_to_peak);

/* SDR display light (linear, 1 = SDR reference white) placed at sdr_white_nits and
 * PQ-encoded. Apply the SDR EOTF (BT.1886, sRGB) first. */
alwan_status alwan_bt2408_sdr_to_pq_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 sdr_white_nits);
alwan_status alwan_bt2408_sdr_to_pq_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 sdr_white_nits);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_bt2408_sdr_to_pq_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_f32 sdr_white_nits);
alwan_status alwan_bt2408_sdr_to_pq_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_f64 sdr_white_nits);

/* The same placement for HLG, through the HLG inverse EOTF of the reference display. */
alwan_status alwan_bt2408_sdr_to_hlg_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 sdr_white_nits, alwan_f64 hlg_peak_nits);
alwan_status alwan_bt2408_sdr_to_hlg_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 sdr_white_nits, alwan_f32 hlg_peak_nits);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_bt2408_sdr_to_hlg_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_f32 sdr_white_nits, alwan_f32 hlg_peak_nits);
alwan_status alwan_bt2408_sdr_to_hlg_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_f64 sdr_white_nits, alwan_f64 hlg_peak_nits);

/* ----------------------------------------------------------------
 * PU21: perceptually uniform encoding of HDR luminance
 *
 * Mantiuk and Azimi 2021. Absolute linear values in cd/m2, as a reference display
 * would emit them, to values in which equal steps are about equally visible: the
 * encoding that lets PSNR and SSIM, built for display-referred SDR, say something
 * about HDR. For banding_glare, the variant the authors recommend and the zero value,
 * 0.005 cd/m2 encodes to about 0, 100 cd/m2 to about 256 (an 8-bit SDR display's
 * range, and the PU-PSNR peak) and 10000 to about 595. Luminance must not be negative. The
 * encode is the curve alone: pu21_encoder.m limits luminance to [0.005, 10000] first,
 * which a caller matching it does too. alwan_pu21_psnr does, as pu21_metric does.
 * ---------------------------------------------------------------- */
typedef enum {
    ALWAN_PU21_BANDING_GLARE = 0, /* banding with glare: the recommended default */
    ALWAN_PU21_BANDING       = 1,
    ALWAN_PU21_PEAKS         = 2,
    ALWAN_PU21_PEAKS_GLARE   = 3
} alwan_pu21_variant;

alwan_status alwan_pu21_encode_f64(alwan_f64 *out, alwan_f64 luminance, alwan_pu21_variant variant);
alwan_status alwan_pu21_encode_f32(alwan_f32 *out, alwan_f32 luminance, alwan_pu21_variant variant);
alwan_status alwan_pu21_decode_f64(alwan_f64 *out, alwan_f64 value, alwan_pu21_variant variant);
alwan_status alwan_pu21_decode_f32(alwan_f32 *out, alwan_f32 value, alwan_pu21_variant variant);
/* Each of R, G and B encoded or decoded alone, as pu21_metric treats colour images. */
alwan_status alwan_pu21_encode_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_pu21_variant variant);
alwan_status alwan_pu21_encode_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_pu21_variant variant);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_pu21_encode_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_pu21_variant variant);
alwan_status alwan_pu21_encode_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_pu21_variant variant);
alwan_status alwan_pu21_decode_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_pu21_variant variant);
alwan_status alwan_pu21_decode_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_pu21_variant variant);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_pu21_decode_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_pu21_variant variant);
alwan_status alwan_pu21_decode_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_pu21_variant variant);

/* PU-PSNR in dB, pu21_metric's PSNR: count pixels of channels values each (1 for
 * luminance, 3 for RGB), rows of values stride bytes apart, every value limited to
 * [0.005, 10000] cd/m2 and PU21 encoded, then 10 log10(256^2 / MSE). +inf when the
 * two images encode identically. */
alwan_status alwan_pu21_psnr_f64(alwan_f64 *psnr_out, alwan_f64 const *test, size_t test_stride, alwan_f64 const *ref, size_t ref_stride, size_t count, size_t channels, alwan_pu21_variant variant);
alwan_status alwan_pu21_psnr_f32(alwan_f32 *psnr_out, alwan_f32 const *test, size_t test_stride, alwan_f32 const *ref, size_t ref_stride, size_t count, size_t channels, alwan_pu21_variant variant);

/* ----------------------------------------------------------------
 * PSNR and CPSNR
 *
 * width x height pixels of channels values each (1 to 4), rows row_stride bytes apart.
 * border pixels are dropped from every edge before anything is accumulated: a
 * demosaicked image is unreliable at its edges, and the literature crops before it
 * compares, so a number computed any other way is not the number those papers quote.
 *
 * psnr_out receives one value per channel and cpsnr_out the single figure over all of
 * them; either may be NULL. CPSNR is NOT the mean of the per-channel values. It is
 * 10 log10(data_range^2 / MSE) with ONE MSE pooled across every channel, which is what
 * the demosaicing papers report and what scikit-image's peak_signal_noise_ratio returns
 * for a multi-channel image. Averaging the per-channel decibels instead gives a
 * different, wrong answer that looks plausible.
 *
 * data_range is the span the values can take: 1 for [0, 1], 255 for 8-bit. A channel
 * that matches exactly reports +inf, as alwan_pu21_psnr does.
 *
 * ALWAN_E_INVALID on a NULL image, both outputs NULL, channels outside 1 to 4, a
 * data_range that is not finite and positive, a zero dimension, or a border that leaves
 * no pixels.
 * ---------------------------------------------------------------- */
alwan_status alwan_psnr_f64(alwan_f64 *psnr_out, alwan_f64 *cpsnr_out, alwan_f64 const *test, size_t test_row_stride, alwan_f64 const *ref, size_t ref_row_stride, size_t width, size_t height, size_t channels, size_t border, alwan_f64 data_range);
alwan_status alwan_psnr_f32(alwan_f32 *psnr_out, alwan_f32 *cpsnr_out, alwan_f32 const *test, size_t test_row_stride, alwan_f32 const *ref, size_t ref_row_stride, size_t width, size_t height, size_t channels, size_t border, alwan_f32 data_range);

/* ----------------------------------------------------------------
 * SSIM: structural similarity, Wang, Bovik, Sheikh and Simoncelli 2004
 *
 * One channel of width x height values, rows row_stride bytes apart, both sides at
 * least 11. The paper's settings, as scikit-image's structural_similarity computes
 * them with gaussian_weights=True, sigma=1.5 and use_sample_covariance=False: an
 * 11-tap Gaussian window of sigma 1.5, K1 0.01 and K2 0.03, the borders reflected
 * with the edge sample repeated, and the mean of the map with a 5-pixel strip dropped
 * at every edge. data_range is the span the values can take: 1 for [0, 1], 255 for
 * 8-bit. 1 for identical images. Computes in double in both precisions.
 * ---------------------------------------------------------------- */
alwan_status alwan_ssim_f64(alwan_f64 *ssim_out, alwan_f64 const *test, size_t test_row_stride, alwan_f64 const *ref, size_t ref_row_stride, size_t width, size_t height, alwan_f64 data_range);
alwan_status alwan_ssim_f32(alwan_f32 *ssim_out, alwan_f32 const *test, size_t test_row_stride, alwan_f32 const *ref, size_t ref_row_stride, size_t width, size_t height, alwan_f32 data_range);

/* PU-SSIM, pu21_metric.m's SSIM: luminance from RGB with its weights (0.212656,
 * 0.715158, 0.072186) when channels is 3, or the values themselves when it is 1, in
 * cd/m2; limited to [0.005, 10000] and PU21 encoded; then alwan_ssim with a data range
 * of 256. pu21_metric.m calls MATLAB's ssim, which handles the borders differently:
 * this follows scikit-image, and the two agree away from the edges. */
alwan_status alwan_pu21_ssim_f64(alwan_f64 *ssim_out, alwan_f64 const *test, size_t test_row_stride, alwan_f64 const *ref, size_t ref_row_stride, size_t width, size_t height, size_t channels, alwan_pu21_variant variant);
alwan_status alwan_pu21_ssim_f32(alwan_f32 *ssim_out, alwan_f32 const *test, size_t test_row_stride, alwan_f32 const *ref, size_t ref_row_stride, size_t width, size_t height, size_t channels, alwan_pu21_variant variant);

/* ----------------------------------------------------------------
 * ISO 21496-1 gain maps
 *
 * A gain map stores, per pixel and channel, the log2 ratio between a base rendition
 * and an alternate one, typically SDR and HDR, so one file serves every display in
 * between:
 *
 *     G = log2((alternate + k_alt) / (base + k_base))
 *
 * normalised to [0, 1] between gain_map_min and gain_map_max and raised to gamma. A
 * display with headroom H applies the fraction
 *
 *     W = (H - H_base) / (H_alt - H_base), limited to [0, 1]
 *     out = (base + k_base) * 2^(G W) - k_alt
 *
 * A headroom is log2 of a peak over SDR reference white: 0 for SDR, 2 for a display
 * four times brighter. Values are linear light in the gain map's colour space. The
 * stored gains are before quantisation; an 8-bit map holds round(gain * 255). Either
 * rendition can be the base: H_alt < H_base is a map from HDR down to SDR. These are
 * the standard's formulas and libultrahdr's (lib/src/gainmapmath.cpp).
 *
 * alwan_gain_map_params_{T} is declared with the other parameter structs. Offsets
 * are used as given, 0 included; writers commonly use 1/64.
 * ---------------------------------------------------------------- */

/* The weight a display with the given headroom applies. ALWAN_E_INVALID when both
 * renditions have the same headroom, as the map then describes no range. */
alwan_status alwan_gain_map_weight_f64(alwan_f64 *weight_out, alwan_gain_map_params_f64 const *params, alwan_f64 display_hdr_headroom);
alwan_status alwan_gain_map_weight_f32(alwan_f32 *weight_out, alwan_gain_map_params_f32 const *params, alwan_f32 display_hdr_headroom);

/* Set gain_map_min and gain_map_max to the smallest and largest log2 ratios of a
 * pair of renditions, per channel, under the offsets already in params. A pixel with
 * no finite ratio in a channel (a value at or below minus its offset) is skipped
 * there; ALWAN_E_RANGE when a channel has none at all. */
alwan_status alwan_gain_map_measure_f64(alwan_gain_map_params_f64 *params, alwan_f64 const *base, size_t base_stride, alwan_f64 const *alternate, size_t alternate_stride, size_t count);
alwan_status alwan_gain_map_measure_f32(alwan_gain_map_params_f32 *params, alwan_f32 const *base, size_t base_stride, alwan_f32 const *alternate, size_t alternate_stride, size_t count);

/* A three-channel gain map from a base and an alternate rendition. A ratio outside
 * [gain_map_min, gain_map_max] stores 0 or 1, the standard's limits; a negative
 * ratio stores NaN. ALWAN_E_INVALID for gain_map_max < gain_map_min or a gamma that
 * is not positive. */
alwan_status alwan_gain_map_encode_f64_map_interleave(alwan_f64 *gain_out, size_t gain_stride, alwan_f64 const *base, size_t base_stride, alwan_f64 const *alternate, size_t alternate_stride, size_t count, alwan_gain_map_params_f64 const *params);
alwan_status alwan_gain_map_encode_f32_map_interleave(alwan_f32 *gain_out, size_t gain_stride, alwan_f32 const *base, size_t base_stride, alwan_f32 const *alternate, size_t alternate_stride, size_t count, alwan_gain_map_params_f32 const *params);

/* The rendition at a weight: 0 gives the base, 1 the alternate. gain_channels is 3,
 * or 1 for a single-channel map, whose one gain and channel-0 metadata apply to all
 * three channels. out may be base. */
alwan_status alwan_gain_map_apply_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *base, size_t base_stride, alwan_f64 const *gain, size_t gain_stride, size_t gain_channels, size_t count, alwan_gain_map_params_f64 const *params, alwan_f64 weight);
alwan_status alwan_gain_map_apply_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *base, size_t base_stride, alwan_f32 const *gain, size_t gain_stride, size_t gain_channels, size_t count, alwan_gain_map_params_f32 const *params, alwan_f32 weight);

/* ----------------------------------------------------------------
 * Exposure and bracket merging
 *
 * The camera exposure equations with EXIF numbers as plain inputs, and the
 * weighted radiance merge of an exposure bracket. N is the f-number, t the
 * exposure time in seconds, S the ISO speed. Matches colour-hdri
 * (colour_hdri.exposure, colour_hdri.generation).
 * ---------------------------------------------------------------- */

/* ISO 2720 reflected-light meter: the average scene luminance, cd/m2, that the
 * settings expose correctly, N^2 / t / S x k. k 0 is 12.5. */
alwan_status alwan_average_luminance_f64(alwan_f64 *luminance_out, alwan_f64 f_number, alwan_f64 exposure_time, alwan_f64 iso, alwan_f64 k);
alwan_status alwan_average_luminance_f32(alwan_f32 *luminance_out, alwan_f32 f_number, alwan_f32 exposure_time, alwan_f32 iso, alwan_f32 k);

/* ISO 2720 incident-light meter: the average illuminance, lux, N^2 / t / S x c.
 * c 0 is 250. */
alwan_status alwan_average_illuminance_f64(alwan_f64 *illuminance_out, alwan_f64 f_number, alwan_f64 exposure_time, alwan_f64 iso, alwan_f64 c);
alwan_status alwan_average_illuminance_f32(alwan_f32 *illuminance_out, alwan_f32 f_number, alwan_f32 exposure_time, alwan_f32 iso, alwan_f32 c);

/* Exposure value from a luminance, log2(L S / k), or an illuminance, log2(E S / c);
 * 0 takes the default k or c. */
alwan_status alwan_luminance_to_exposure_value_f64(alwan_f64 *ev_out, alwan_f64 luminance, alwan_f64 iso, alwan_f64 k);
alwan_status alwan_luminance_to_exposure_value_f32(alwan_f32 *ev_out, alwan_f32 luminance, alwan_f32 iso, alwan_f32 k);
alwan_status alwan_illuminance_to_exposure_value_f64(alwan_f64 *ev_out, alwan_f64 illuminance, alwan_f64 iso, alwan_f64 c);
alwan_status alwan_illuminance_to_exposure_value_f32(alwan_f32 *ev_out, alwan_f32 illuminance, alwan_f32 iso, alwan_f32 c);

/* EV100: the exposure value of the settings referred to ISO 100. */
alwan_status alwan_exposure_value_100_f64(alwan_f64 *ev100_out, alwan_f64 f_number, alwan_f64 exposure_time, alwan_f64 iso);
alwan_status alwan_exposure_value_100_f32(alwan_f32 *ev100_out, alwan_f32 f_number, alwan_f32 exposure_time, alwan_f32 iso);

/* ISO 12232 focal plane exposure, lux-seconds:
 * q L t F^2 / (N^2 i^2) + flare, with q = pi / 4 T f_v cos^4(angle). focal_length F
 * and image_distance i in metres, transmittance T (0.9 typical), vignetting f_v
 * (0.98), angle off axis in degrees (10). */
alwan_status alwan_focal_plane_exposure_f64(alwan_f64 *exposure_out, alwan_f64 luminance, alwan_f64 f_number, alwan_f64 exposure_time, alwan_f64 focal_length, alwan_f64 image_distance, alwan_f64 flare, alwan_f64 transmittance, alwan_f64 vignetting, alwan_f64 angle);
alwan_status alwan_focal_plane_exposure_f32(alwan_f32 *exposure_out, alwan_f32 luminance, alwan_f32 f_number, alwan_f32 exposure_time, alwan_f32 focal_length, alwan_f32 image_distance, alwan_f32 flare, alwan_f32 transmittance, alwan_f32 vignetting, alwan_f32 angle);

/* The focal plane exposure scaled for saturation-based speed, H S / 78. */
alwan_status alwan_saturation_based_speed_focal_plane_exposure_f64(alwan_f64 *exposure_out, alwan_f64 luminance, alwan_f64 f_number, alwan_f64 exposure_time, alwan_f64 iso, alwan_f64 focal_length, alwan_f64 image_distance, alwan_f64 flare, alwan_f64 transmittance, alwan_f64 vignetting, alwan_f64 angle);
alwan_status alwan_saturation_based_speed_focal_plane_exposure_f32(alwan_f32 *exposure_out, alwan_f32 luminance, alwan_f32 f_number, alwan_f32 exposure_time, alwan_f32 iso, alwan_f32 focal_length, alwan_f32 image_distance, alwan_f32 flare, alwan_f32 transmittance, alwan_f32 vignetting, alwan_f32 angle);

/* ISO 12232 exposure index, 10 / H. */
alwan_status alwan_exposure_index_f64(alwan_f64 *index_out, alwan_f64 focal_plane_exposure);
alwan_status alwan_exposure_index_f32(alwan_f32 *index_out, alwan_f32 focal_plane_exposure);

/* Lagarde and de Rousiers 2014: the factor that turns a camera's pixel values
 * into absolute luminance, 1 / (78 / (100 q) 2^EV100). */
alwan_status alwan_photometric_exposure_scale_factor_lagarde2014_f64(alwan_f64 *scale_out, alwan_f64 ev100, alwan_f64 transmittance, alwan_f64 vignetting, alwan_f64 angle);
alwan_status alwan_photometric_exposure_scale_factor_lagarde2014_f32(alwan_f32 *scale_out, alwan_f32 ev100, alwan_f32 transmittance, alwan_f32 vignetting, alwan_f32 angle);

/* How much a merged pixel trusts each exposure, from its normalised value. */
typedef enum {
    ALWAN_MERGE_WEIGHT_DEBEVEC1997     = 0, /* triangle on [0.01, 0.99], peak 1 at 0.5 */
    ALWAN_MERGE_WEIGHT_HAT             = 1, /* 1 - (2a - 1)^12 */
    ALWAN_MERGE_WEIGHT_NORMAL          = 2, /* Gaussian, mean 0.5, sigma 0.15 */
    ALWAN_MERGE_WEIGHT_DOUBLE_SIGMOID  = 3  /* 0 below 0.025 and above 0.975, 1 on [0.2, 0.8] */
} alwan_merge_weight;

/* One weighting function at one value. */
alwan_status alwan_hdr_merge_weight_f64(alwan_f64 *weight_out, alwan_f64 value, alwan_merge_weight fn);
alwan_status alwan_hdr_merge_weight_f32(alwan_f32 *weight_out, alwan_f32 value, alwan_merge_weight fn);

/* Merge an exposure bracket into one radiance image.
 *
 * images holds image_count pointers to RGB buffers of count pixels, one per
 * exposure, ordered from the shortest exposure to the longest; settings gives each
 * one's N, t and S. Values are normalised sensor data: they are limited to
 * [2.2e-16, 1], 1 being saturation. Each channel is weighted by fn, the shortest
 * exposure trusted fully at and above 0.5 and the longest at and below it, and the
 * result is sum(w d N^2 k / (t S)) / sum(w) with k = 12.5: the scene luminance scale
 * of an ISO 2720 meter.
 *
 * response, when not NULL, is the camera response curve applied to each value after
 * weighting: response_size samples on [0, 1] per channel, planar, R then G then B.
 * alwan_hdr_merge matches colour_hdri.image_stack_to_HDRI, except that the Debevec
 * weight is normalised by its own peak rather than by the image's largest weight. */
alwan_status alwan_hdr_merge_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *const *images, size_t in_stride, size_t count, alwan_exposure_settings_f64 const *settings, size_t image_count, alwan_merge_weight fn, alwan_f64 const *response, size_t response_size);
alwan_status alwan_hdr_merge_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *const *images, size_t in_stride, size_t count, alwan_exposure_settings_f32 const *settings, size_t image_count, alwan_merge_weight fn, alwan_f32 const *response, size_t response_size);

/* Camera response recovery, Debevec and Malik 1997. The zero value of every field
 * is colour-hdri's default. */
typedef struct {
    size_t samples;            /* Grossberg 2003 samples per exposure and channel; 0 is 1000 */
    size_t bins;               /* histogram bins and response resolution; 0 is 256 */
    alwan_f64 smoothing;       /* Debevec's lambda; 0 is 30 */
    alwan_merge_weight weight; /* weighting function; the zero value is Debevec 1997 */
    int extrapolation_degree;  /* polynomial through the weighted values, evaluated where the
                                * weight is 0; 0 is 7, negative leaves those values as solved */
    int keep_scale;            /* non-zero: leave exp(g) unscaled instead of peaking at 1 */
} alwan_crf_debevec1997_params;

/* Grossberg and Nayar 2003: the pixel values to sample, chosen so that each sample
 * sits at the same point of every exposure's histogram. For each of samples points
 * u on [0, 1], the bin whose cumulative histogram is nearest u, per exposure and
 * channel. bins_out receives samples x image_count x 3 bin indices. */
alwan_status alwan_crf_samples_grossberg2003_f64(size_t *bins_out, alwan_f64 const *const *images, size_t in_stride, size_t count, size_t image_count, size_t samples, size_t bins);
alwan_status alwan_crf_samples_grossberg2003_f32(size_t *bins_out, alwan_f32 const *const *images, size_t in_stride, size_t count, size_t image_count, size_t samples, size_t bins);

/* The camera response of a bracket, Debevec and Malik 1997: per channel, the exposure
 * that each normalised pixel value records, solved by least squares over Grossberg
 * samples with a smoothness term. response_out receives bins values per channel,
 * planar, R then G then B, which is the response alwan_hdr_merge takes. The bracket
 * is given as for alwan_hdr_merge. params NULL is the defaults. Matches
 * colour_hdri.camera_response_functions_Debevec1997. */
alwan_status alwan_crf_debevec1997_f64(alwan_f64 *response_out, alwan_f64 const *const *images, size_t in_stride, size_t count, alwan_exposure_settings_f64 const *settings, size_t image_count, alwan_crf_debevec1997_params const *params);
alwan_status alwan_crf_debevec1997_f32(alwan_f32 *response_out, alwan_f32 const *const *images, size_t in_stride, size_t count, alwan_exposure_settings_f32 const *settings, size_t image_count, alwan_crf_debevec1997_params const *params);

/* Camera response recovery, Robertson, Borman and Stevenson 2003. The zero value of
 * every field is OpenCV's default. */
typedef struct {
    size_t bins;         /* response resolution; 0 is 256 */
    size_t iterations;   /* at most this many merge-and-re-estimate rounds; 0 is 30 */
    alwan_f64 threshold; /* stop once the summed absolute change of the response, averaged over the
                          * channels, falls below it; 0 is 0.01, a tiny value runs every round */
} alwan_crf_robertson2003_params;

/* The camera response of a bracket, Robertson, Borman and Stevenson 2003: merge every
 * pixel with the current response, re-estimate the response as the mean exposure each
 * pixel value records, and repeat, starting from a linear response. The bracket is
 * given as for alwan_hdr_merge, and only the ratios of the exposures matter.
 * response_out receives bins values per channel, planar, R then G then B, normalised
 * to 1 at the middle value, which is the response alwan_hdr_merge takes. A value that
 * no pixel holds is filled from its neighbours; OpenCV leaves it NaN. ALWAN_E_RANGE
 * when no pixel holds the middle value. params NULL is the defaults. Matches OpenCV's
 * CalibrateRobertson on 8-bit brackets divided by 255. With no smoothness term it
 * needs exposures about a stop apart or closer: 3 stops apart leaves a sawtooth of
 * half a stop, where alwan_crf_debevec1997 does not. */
alwan_status alwan_crf_robertson2003_f64(alwan_f64 *response_out, alwan_f64 const *const *images, size_t in_stride, size_t count, alwan_exposure_settings_f64 const *settings, size_t image_count, alwan_crf_robertson2003_params const *params);
alwan_status alwan_crf_robertson2003_f32(alwan_f32 *response_out, alwan_f32 const *const *images, size_t in_stride, size_t count, alwan_exposure_settings_f32 const *settings, size_t image_count, alwan_crf_robertson2003_params const *params);

/* Exposure fusion: a bracket of exposures blended straight into one displayable picture,
 * with no response curve, radiance map or tone curve in between.
 *
 *   ALWAN_EXPOSURE_FUSION_MERTENS2007  Mertens, Kautz and Van Reeth, Pacific Graphics 2007
 */
typedef enum {
    ALWAN_EXPOSURE_FUSION_MERTENS2007 = 0
} alwan_exposure_fusion_method;

/* MERTENS2007: the zero value of every field is the paper's choice. OpenCV's
 * createMergeMertens() default leaves well-exposedness out:
 * ignore = ALWAN_FUSION_IGNORE_EXPOSURE. */
enum {
    ALWAN_FUSION_IGNORE_CONTRAST   = 1, /* leave out contrast (exponent 0) */
    ALWAN_FUSION_IGNORE_SATURATION = 2, /* leave out saturation */
    ALWAN_FUSION_IGNORE_EXPOSURE   = 4  /* leave out well-exposedness */
};
typedef struct {
    alwan_f64 contrast;   /* exponent on |Laplacian| of the luma; 0 is 1 */
    alwan_f64 saturation; /* exponent on the spread of R, G and B about their mean; 0 is 1 */
    alwan_f64 exposure;   /* exponent on the closeness of each channel to 0.5; 0 is 1 */
    alwan_f64 sigma;      /* width of that closeness; 0 is 0.2 */
    unsigned ignore;      /* ALWAN_FUSION_IGNORE_* bits */
    int levels;           /* pyramid levels, at most 64; 0 is floor(log2(min(width, height))) + 1 */
} alwan_exposure_fusion_params;

/* Fuse an exposure bracket into one displayable image by `method`. images holds
 * image_count pointers to interleaved RGB images of width x height, rows
 * in_row_stride bytes apart, in any order. The values are display-encoded, as a
 * camera's JPEGs are, 1 being white. Each pixel of each exposure is weighted by its
 * contrast, saturation and well-exposedness, and the images blend as Laplacian
 * pyramids under Gaussian pyramids of the weights. The result is not clamped: the
 * pyramid can overshoot [0, 1] near strong edges. params NULL is the defaults; an
 * unknown method is ALWAN_E_INVALID.
 * MERTENS2007 matches OpenCV's MergeMertens given 8-bit images divided by 255, except
 * where every exposure's weight is OpenCV's 1e-12 floor (contrast or saturation exactly
 * zero in all of them): OpenCV's float mean gives a neutral pixel a saturation near 1e-8
 * there and rounding decides the blend, while alwan computes the saturation from
 * pairwise channel differences, exactly 0, and returns the plain average (suite 122). */
alwan_status alwan_exposure_fusion_f64(alwan_f64 *rgb_out, size_t out_row_stride, alwan_f64 const *const *images, size_t in_row_stride, size_t image_count, size_t width, size_t height, alwan_exposure_fusion_method method, alwan_exposure_fusion_params const *params);
alwan_status alwan_exposure_fusion_f32(alwan_f32 *rgb_out, size_t out_row_stride, alwan_f32 const *const *images, size_t in_row_stride, size_t image_count, size_t width, size_t height, alwan_exposure_fusion_method method, alwan_exposure_fusion_params const *params);

/* ----------------------------------------------------------------
 * HDR Gamut Mapping
 * ---------------------------------------------------------------- */

/* Chroma compression in JzCzhz (hue-preserving)
 * Cz_max: maximum chroma at the input's (Jz, hz) for target gamut */
void alwan_hdr_gamut_map_jzczhz_f32(alwan_jzczhz_f32 *out, alwan_jzczhz_f32 const *in,
                                alwan_f32 Cz_max);
void alwan_hdr_gamut_map_jzczhz_f64(alwan_jzczhz_f64 *out, alwan_jzczhz_f64 const *in,
                                alwan_f64 Cz_max);

/* Buffer form, strides in bytes; the scalar in a loop, validated once, bit-identical to it (suite 174). */
alwan_status alwan_hdr_gamut_map_jzczhz_f32_map_interleave(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_f32 Cz_max);
alwan_status alwan_hdr_gamut_map_jzczhz_f64_map_interleave(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_f64 Cz_max);
/* Planar twin, one stride shared by the three planes; identical to the interleave form (suite 174). */
alwan_status alwan_hdr_gamut_map_jzczhz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_f32 Cz_max);
alwan_status alwan_hdr_gamut_map_jzczhz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_f64 Cz_max);

/* ----------------------------------------------------------------
 * Display Characterization
 * ---------------------------------------------------------------- */

/* Peak luminance normalization for PQ signals
 * Clips PQ absolute values to display-specific peak, re-encodes
 * display_peak: display peak luminance (cd/m2) */
alwan_status alwan_pq_normalize_peak_f32(alwan_f32 *pq_out, alwan_f32 pq_value,
                              alwan_f32 display_peak);
alwan_status alwan_pq_normalize_peak_f64(alwan_f64 *pq_out, alwan_f64 pq_value,
                              alwan_f64 display_peak);

/* Initialize ST.2086 metadata from display parameters */
alwan_status alwan_st2086_init_f32(alwan_st2086_metadata_f32 *meta,
                       alwan_f32 const display_primaries_xy[6],
                       alwan_f32 const white_point_xy[2],
                       alwan_f32 max_luminance,
                       alwan_f32 min_luminance);
alwan_status alwan_st2086_init_f64(alwan_st2086_metadata_f64 *meta,
                       alwan_f64 const display_primaries_xy[6],
                       alwan_f64 const white_point_xy[2],
                       alwan_f64 max_luminance,
                       alwan_f64 min_luminance);

/* Compute content light level info from linear RGB pixel data (cd/m2) */
alwan_status alwan_content_light_level_compute_f32(alwan_content_light_level_f32 *cll_out, alwan_f32 const *rgb_in, size_t stride, size_t count);
alwan_status alwan_content_light_level_compute_f64(alwan_content_light_level_f64 *cll_out, alwan_f64 const *rgb_in, size_t stride, size_t count);

/* ----------------------------------------------------------------
 * Hero Wavelength Spectral Sampling
 * ---------------------------------------------------------------- */

/* Sample a single wavelength from uniform [0,1] -> [380,780] nm */
alwan_status alwan_hero_wavelength_sample_f64(alwan_f64 *lambda_out, alwan_f64 u);
alwan_status alwan_hero_wavelength_sample_f32(alwan_f32 *lambda_out, alwan_f32 u);

/* Convert single wavelength to XYZ via Wyman 2013 analytic CMF fit */
void alwan_hero_wavelength_to_xyz_f32(alwan_xyz_f32 *xyz_out, alwan_f32 lambda);
void alwan_hero_wavelength_to_xyz_f64(alwan_xyz_f64 *xyz_out, alwan_f64 lambda);

/* Batch sampling with stratification: generates count wavelengths from seed
 * lambda_out: output wavelengths (count elements)
 * xyz_weights: output XYZ importance weights (count elements, can be NULL)
 * count: number of stratified wavelengths
 * seed: uniform [0,1] seed for hero wavelength */
alwan_status alwan_hero_wavelength_batch_f64(alwan_f64 *lambda_out,
                                 alwan_xyz_f64 *xyz_weights,
                                 size_t count,
                                 alwan_f64 seed);
alwan_status alwan_hero_wavelength_batch_f32(alwan_f32 *lambda_out,
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
alwan_status alwan_cam18sl_forward_f32(alwan_cam18sl_correlates_f32 *out,
                          alwan_xyz_f32 const *xyz,
                          alwan_f32 Y_b);
alwan_status alwan_cam18sl_forward_f64(alwan_cam18sl_correlates_f64 *out,
                          alwan_xyz_f64 const *xyz,
                          alwan_f64 Y_b);

/* CAM18sl inverse: appearance correlates -> XYZ */
alwan_status alwan_cam18sl_inverse_f32(alwan_xyz_f32 *xyz_out,
                          alwan_cam18sl_correlates_f32 const *correlates,
                          alwan_f32 Y_b);
alwan_status alwan_cam18sl_inverse_f64(alwan_xyz_f64 *xyz_out,
                          alwan_cam18sl_correlates_f64 const *correlates,
                          alwan_f64 Y_b);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_cam18sl_forward_f32_map_interleave(alwan_cam18sl_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_f32 Y_b, size_t count);
alwan_status alwan_cam18sl_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_cam18sl_correlates_f32 const *correlates_in, alwan_f32 Y_b, size_t count);
alwan_status alwan_cam18sl_forward_f64_map_interleave(alwan_cam18sl_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_f64 Y_b, size_t count);
alwan_status alwan_cam18sl_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_cam18sl_correlates_f64 const *correlates_in, alwan_f64 Y_b, size_t count);

/* ----------------------------------------------------------------
 * CAM20u - Color Appearance Model for Unrelated Color
 * Reference: Kim & Park (2020)
 * ---------------------------------------------------------------- */

/* CAM20u forward: XYZ -> appearance correlates
 * xyz: absolute XYZ tristimulus (cd/m2)
 * Y_b: background luminance (cd/m2)
 * L_a: adapting luminance (cd/m2) */
alwan_status alwan_cam20u_forward_f32(alwan_cam20u_correlates_f32 *out,
                         alwan_xyz_f32 const *xyz,
                         alwan_f32 Y_b,
                         alwan_f32 L_a);
alwan_status alwan_cam20u_forward_f64(alwan_cam20u_correlates_f64 *out,
                         alwan_xyz_f64 const *xyz,
                         alwan_f64 Y_b,
                         alwan_f64 L_a);

/* CAM20u inverse: appearance correlates -> XYZ */
alwan_status alwan_cam20u_inverse_f32(alwan_xyz_f32 *xyz_out,
                         alwan_cam20u_correlates_f32 const *correlates,
                         alwan_f32 Y_b,
                         alwan_f32 L_a);
alwan_status alwan_cam20u_inverse_f64(alwan_xyz_f64 *xyz_out,
                         alwan_cam20u_correlates_f64 const *correlates,
                         alwan_f64 Y_b,
                         alwan_f64 L_a);

/* Buffer forms, strides in bytes on the XYZ side, one correlates struct per pixel
 * on the other. Each is the scalar in a loop, validated once, and agrees with its
 * scalar twin to the bit (suite 172). */
alwan_status alwan_cam20u_forward_f32_map_interleave(alwan_cam20u_correlates_f32 *correlates_out, alwan_f32 const *xyz_in, size_t in_stride, alwan_f32 Y_b, alwan_f32 L_a, size_t count);
alwan_status alwan_cam20u_inverse_f32_map_interleave(alwan_f32 *xyz_out, size_t out_stride, alwan_cam20u_correlates_f32 const *correlates_in, alwan_f32 Y_b, alwan_f32 L_a, size_t count);
alwan_status alwan_cam20u_forward_f64_map_interleave(alwan_cam20u_correlates_f64 *correlates_out, alwan_f64 const *xyz_in, size_t in_stride, alwan_f64 Y_b, alwan_f64 L_a, size_t count);
alwan_status alwan_cam20u_inverse_f64_map_interleave(alwan_f64 *xyz_out, size_t out_stride, alwan_cam20u_correlates_f64 const *correlates_in, alwan_f64 Y_b, alwan_f64 L_a, size_t count);

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
alwan_status alwan_srgb_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_srgb_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_xyz_to_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_xyz_to_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_srgb_to_lab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_srgb_to_lab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_lab_to_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_lab_to_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_srgb_to_oklab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_srgb_to_oklab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_oklab_to_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_oklab_to_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* sRGB convenience planar _ex */
alwan_status alwan_srgb_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_srgb_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_lab_to_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_srgb_to_oklab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklab_to_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Colorspace planar (with white_xyz) */
alwan_status alwan_xyz_to_lab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_lab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_lab_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_lab_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_luv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_luv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_luv_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_luv_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_xyz_f64 const *white_xyz);

/* Colorspace planar (no extra params) */
alwan_status alwan_lab_to_lch_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_lab_to_lch_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_lch_to_lab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_lch_to_lab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_luv_to_lchuv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_luv_to_lchuv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_lchuv_to_luv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_lchuv_to_luv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_xyz_to_xyy_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_xyz_to_xyy_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_xyy_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_xyy_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Colorspace planar _ex */
alwan_status alwan_xyz_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_lab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_luv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_luv_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_lab_to_lch_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_lch_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_luv_to_lchuv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_lchuv_to_luv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_xyy_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyy_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Oklab planar */
alwan_status alwan_xyz_to_oklab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_xyz_to_oklab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_oklab_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_oklab_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_oklab_to_oklch_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_oklab_to_oklch_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_oklch_to_oklab_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_oklch_to_oklab_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Oklab planar _ex */
alwan_status alwan_xyz_to_oklab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklab_to_oklch_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_oklch_to_oklab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* ICtCp planar (with use_pq) */
alwan_status alwan_rgb_to_ictcp_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
alwan_status alwan_rgb_to_ictcp_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);
alwan_status alwan_ictcp_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
alwan_status alwan_ictcp_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);
alwan_status alwan_xyz_to_ictcp_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
alwan_status alwan_xyz_to_ictcp_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);
alwan_status alwan_ictcp_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int use_pq);
alwan_status alwan_ictcp_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int use_pq);

/* ICtCp planar _ex */
alwan_status alwan_rgb_to_ictcp_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
alwan_status alwan_ictcp_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
alwan_status alwan_xyz_to_ictcp_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);
alwan_status alwan_ictcp_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int use_pq);

/* JzAzBz planar */
alwan_status alwan_xyz_to_jzazbz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_xyz_to_jzazbz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_jzazbz_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_jzazbz_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_jzazbz_to_jzczhz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_jzazbz_to_jzczhz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_jzczhz_to_jzazbz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_jzczhz_to_jzazbz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* JzAzBz planar _ex */
alwan_status alwan_xyz_to_jzazbz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_jzazbz_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_jzazbz_to_jzczhz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_jzczhz_to_jzazbz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* IPT planar */
alwan_status alwan_xyz_to_ipt_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_xyz_to_ipt_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_ipt_to_xyz_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_ipt_to_xyz_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* IPT planar _ex */
alwan_status alwan_xyz_to_ipt_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ipt_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* HSP/HSPlog/HSY planar */
alwan_status alwan_rgb_to_hsp_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsp_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsp_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsp_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsplog_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsplog_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsplog_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsplog_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsy_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsy_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsy_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsy_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* HSP/HSPlog/HSY planar _ex */
alwan_status alwan_rgb_to_hsp_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsp_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hsplog_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsplog_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hsy_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsy_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Linear sRGB <-> HSV/HSL planar */
alwan_status alwan_linear_srgb_to_hsv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_linear_srgb_to_hsv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsv_to_linear_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsv_to_linear_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_linear_srgb_to_hsl_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_linear_srgb_to_hsl_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsl_to_linear_srgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsl_to_linear_srgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* Linear sRGB <-> HSV/HSL planar _ex */
alwan_status alwan_linear_srgb_to_hsv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsv_to_linear_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_linear_srgb_to_hsl_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsl_to_linear_srgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* HSV/HSL planar */
alwan_status alwan_rgb_to_hsv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsv_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsv_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsl_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hsl_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsl_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsl_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* HSV/HSL planar _ex */
alwan_status alwan_rgb_to_hsv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsv_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hsl_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsl_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* CMY/YCoCg/HWB planar */
alwan_status alwan_rgb_to_cmy_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_cmy_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_cmy_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_cmy_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_ycocg_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_ycocg_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_ycocg_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_ycocg_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hwb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_rgb_to_hwb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hwb_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hwb_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hsv_to_hwb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hsv_to_hwb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);
alwan_status alwan_hwb_to_hsv_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count);
alwan_status alwan_hwb_to_hsv_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count);

/* CMY/YCoCg/HWB planar _ex */
alwan_status alwan_rgb_to_cmy_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_cmy_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_ycocg_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ycocg_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hwb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hwb_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hsv_to_hwb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hwb_to_hsv_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* YCbCr planar */
alwan_status alwan_rgb_to_ycbcr_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_ycbcr_standard standard);
alwan_status alwan_rgb_to_ycbcr_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_ycbcr_standard standard);

/* YCbCr planar _ex */
alwan_status alwan_rgb_to_ycbcr_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);
alwan_status alwan_ycbcr_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_ycbcr_standard standard);

/* YcCbcCrc / legal-full planar */
alwan_status alwan_rgb_to_yccbccrc_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
alwan_status alwan_rgb_to_yccbccrc_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);
alwan_status alwan_ycbcr_full_to_legal_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
alwan_status alwan_ycbcr_full_to_legal_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, int bit_depth);

/* YcCbcCrc / legal-full planar _ex */
alwan_status alwan_rgb_to_yccbccrc_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
alwan_status alwan_yccbccrc_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
alwan_status alwan_ycbcr_full_to_legal_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);
alwan_status alwan_ycbcr_legal_to_full_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int bit_depth);

/* Extended color spaces planar */
alwan_status alwan_xyz_to_igpgtg_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_igpgtg_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_igpgtg_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_igpgtg_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_xyz_to_icacb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_icacb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_icacb_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_icacb_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_xyz_to_hdr_cielab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_hdr_cielab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_hdr_cielab_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_hdr_cielab_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_xyz_to_hdr_ipt_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_hdr_ipt_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_hdr_ipt_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_hdr_ipt_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_xyz_to_ucs_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_ucs_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_ucs_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_ucs_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_xyz_to_osa_ucs_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_osa_ucs_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_osa_ucs_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_osa_ucs_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_xyz_to_hunter_lab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_hunter_lab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_hunter_lab_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_hunter_lab_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_xyz_to_prolab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_xyz_to_prolab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_prolab_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_prolab_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_rgb_to_prismatic_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_rgb_to_prismatic_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_prismatic_to_rgb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_prismatic_to_rgb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_rgb_to_hcl_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_rgb_to_hcl_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_hcl_to_rgb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_hcl_to_rgb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_rgb_to_ihls_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_rgb_to_ihls_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);
alwan_status alwan_ihls_to_rgb_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count);
alwan_status alwan_ihls_to_rgb_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count);

/* Extended with white point planar */
alwan_status alwan_xyz_to_hunter_lab_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_hunter_lab_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_hunter_lab_to_xyz_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_hunter_lab_to_xyz_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_prolab_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_prolab_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_prolab_to_xyz_custom_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_prolab_to_xyz_custom_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_uvw_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_xyz_to_uvw_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_uvw_to_xyz_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_xyz_f32 const *white_xyz);
alwan_status alwan_uvw_to_xyz_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_xyz_f64 const *white_xyz);

/* DIN99 planar */
alwan_status alwan_lab_to_din99_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, int variant);
alwan_status alwan_lab_to_din99_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, int variant);
alwan_status alwan_din99_to_lab_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, int variant);
alwan_status alwan_din99_to_lab_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, int variant);

/* Extended planar _ex */
alwan_status alwan_xyz_to_igpgtg_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_igpgtg_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_icacb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_icacb_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_hdr_cielab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hdr_cielab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_hdr_ipt_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hdr_ipt_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_ucs_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ucs_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_osa_ucs_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_osa_ucs_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_hunter_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hunter_lab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_xyz_to_prolab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_prolab_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_prismatic_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_prismatic_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_hcl_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_hcl_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_rgb_to_ihls_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);
alwan_status alwan_ihls_to_rgb_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt);

/* Extended white point planar _ex */
alwan_status alwan_xyz_to_hunter_lab_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_hunter_lab_to_xyz_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_prolab_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_prolab_to_xyz_custom_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_xyz_to_uvw_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);
alwan_status alwan_uvw_to_xyz_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_xyz_f64 const *white_xyz);

/* DIN99 planar _ex */
alwan_status alwan_lab_to_din99_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);
alwan_status alwan_din99_to_lab_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, int variant);

/* CVD simulation planar */
alwan_status alwan_simulate_cvd_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_cvd_type cvd_type, alwan_f32 severity);
alwan_status alwan_simulate_cvd_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_cvd_type cvd_type, alwan_f64 severity);
alwan_status alwan_simulate_protanopia_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 severity);
alwan_status alwan_simulate_protanopia_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 severity);
alwan_status alwan_simulate_deuteranopia_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 severity);
alwan_status alwan_simulate_deuteranopia_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 severity);
alwan_status alwan_simulate_tritanopia_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 severity);
alwan_status alwan_simulate_tritanopia_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 severity);

/* CVD simulation planar _ex */
alwan_status alwan_simulate_cvd_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_cvd_type cvd_type, alwan_f64 severity);
alwan_status alwan_simulate_protanopia_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
alwan_status alwan_simulate_deuteranopia_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);
alwan_status alwan_simulate_tritanopia_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 severity);

/* Color correction planar */
alwan_status alwan_lgg_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_rgb_f32 const *lift, alwan_rgb_f32 const *gamma, alwan_rgb_f32 const *gain);
alwan_status alwan_lgg_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);
alwan_status alwan_color_matrix_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_mat3x3_f32 const *matrix);
alwan_status alwan_color_matrix_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_mat3x3_f64 const *matrix);
alwan_status alwan_printer_lights_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_f32 red_lights, alwan_f32 green_lights, alwan_f32 blue_lights);
alwan_status alwan_printer_lights_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);
alwan_status alwan_white_balance_apply_f32_map_planar(alwan_f32 *o0, size_t out_stride, alwan_f32 *o1, alwan_f32 *o2, alwan_f32 const *i0, size_t in_stride, alwan_f32 const *i1, alwan_f32 const *i2, size_t count, alwan_rgb_f32 const *multipliers);
alwan_status alwan_white_balance_apply_f64_map_planar(alwan_f64 *o0, size_t out_stride, alwan_f64 *o1, alwan_f64 *o2, alwan_f64 const *i0, size_t in_stride, alwan_f64 const *i1, alwan_f64 const *i2, size_t count, alwan_rgb_f64 const *multipliers);

/* Color correction planar _ex */
alwan_status alwan_lgg_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *lift, alwan_rgb_f64 const *gamma, alwan_rgb_f64 const *gain);
alwan_status alwan_color_matrix_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_mat3x3_f64 const *matrix);
alwan_status alwan_printer_lights_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_f64 red_lights, alwan_f64 green_lights, alwan_f64 blue_lights);
alwan_status alwan_white_balance_apply_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, alwan_rgb_f64 const *multipliers);

/* ----------------------------------------------------------------
 * LUT Baking
 * ---------------------------------------------------------------- */

/* Bake a 3D LUT by sampling an RGB color space conversion pipeline.
 * out: buffer of size^3 * 3 values (R-fastest, then G, then B)
 * size: cube edge length (e.g. 17, 33, 65)
 * src_space: source RGB color space descriptor
 * dst_space: destination RGB color space descriptor
 * ctx: context for chromatic adaptation (may be NULL if white points match)
 * Returns ALWAN_OK on success */
alwan_status alwan_bake_3dlut_f32(alwan_f32 *out, int size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_ctx *ctx);
alwan_status alwan_bake_3dlut_f64(alwan_f64 *out, int size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_ctx *ctx);

/* Bake a 3D LUT with a view transform applied after color space conversion.
 * The pipeline is: EOTF(src) -> combined matrix -> view transform -> OETF(dst)
 * vt: view transform to apply (use -1 or cast for no transform)
 * Returns ALWAN_OK on success */
alwan_status alwan_bake_3dlut_view_f32(alwan_f32 *out, int size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform vt, alwan_ctx *ctx);
alwan_status alwan_bake_3dlut_view_f64(alwan_f64 *out, int size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform vt, alwan_ctx *ctx);

/* Bake a 1D LUT by sampling a transfer function.
 * out: buffer of size values
 * size: number of samples (e.g. 256, 1024, 4096)
 * tf: transfer function to sample
 * encode: non-zero for OETF (linear->encoded), zero for EOTF (encoded->linear)
 * Returns ALWAN_OK on success */
alwan_status alwan_bake_1dlut_f32(alwan_f32 *out, int size,
                     alwan_transfer_function tf,
                     int encode);
alwan_status alwan_bake_1dlut_f64(alwan_f64 *out, int size,
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
void alwan_lut2d_dimensions(int size, int *width, int *height);

/* Flatten a 3D LUT into a 2D image buffer.
 * out: buffer of (size*size) * size * 3 values (row-major, RGB interleaved)
 * lut3d: source 3D LUT (size^3 * 3, R-fastest)
 * size: cube edge length
 * Returns ALWAN_OK on success */
alwan_status alwan_lut3d_to_2d_f32(alwan_f32 *out,
                       alwan_f32 const *lut3d,
                       int size);
alwan_status alwan_lut3d_to_2d_f64(alwan_f64 *out,
                       alwan_f64 const *lut3d,
                       int size);

/* Unflatten a 2D image buffer back to a 3D LUT.
 * out: buffer of size^3 * 3 values (R-fastest)
 * lut2d: source 2D image (size*size width, size height, RGB interleaved)
 * size: cube edge length
 * Returns ALWAN_OK on success */
alwan_status alwan_lut2d_to_3d_f32(alwan_f32 *out,
                       alwan_f32 const *lut2d,
                       int size);
alwan_status alwan_lut2d_to_3d_f64(alwan_f64 *out,
                       alwan_f64 const *lut2d,
                       int size);

/* Bake directly into a 2D image buffer (convenience: bake 3D + flatten).
 * out: buffer of (size*size) * size * 3 values
 * size: cube edge length
 * Returns ALWAN_OK on success */
alwan_status alwan_bake_2dlut_f32(alwan_f32 *out, int size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_ctx *ctx);
alwan_status alwan_bake_2dlut_f64(alwan_f64 *out, int size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Table readers
 *
 * Every array in the library that is addressed by a FLOAT coordinate is read
 * through one of these. The addressing gate lives in one place
 * (core/alwan_table_core.inc) and runs once per COORDINATE, before the cast to
 * int -- never per element read -- so the eight corner fetches of a trilinear
 * sample carry no bounds checks at all.
 *
 * BOUNDS POLICY. alwan does not silently clamp COLOUR VALUES. An array ADDRESS
 * is not a colour: lut[-3] has no unclamped meaning to preserve, and (int)NaN
 * is undefined behaviour that reads far outside the table. These readers clamp
 * ADDRESSES and leave every colour value untouched. By default:
 *
 *   coord < 0, or -inf  -> the table's first element, ALWAN_OK
 *   coord > 1, or +inf  -> the table's last element,  ALWAN_OK
 *   NaN                 -> the table's first element, ALWAN_OK
 *
 * Callers who need to be told instead of served the edge pass
 * `mode | ALWAN_SAMPLE_STRICT`, which returns ALWAN_E_RANGE and leaves *result
 * untouched. STRICT is scalar-only for now; a bulk reader given it returns
 * ALWAN_E_INVALID rather than ignoring it.
 *
 * ALWAN_E_INVALID is returned for a null pointer, size < 2, or a mode the rank
 * cannot honour. All three are loop-invariant and checked once at entry. A
 * mode is REJECTED, never silently downgraded: passing ALWAN_SAMPLE_BILINEAR
 * to the 2-d strip reader is an error, not a quiet substitution, because the
 * strip is a flattened cube and is genuinely sampled trilinearly.
 *
 * Accepted modes by rank:
 *   rank 1, scalar   NEAREST, LINEAR (default), CATMULL_ROM
 *   rank 1, mat3x3   NEAREST, LINEAR (default) -- not CATMULL_ROM: the
 *                    reference CVD model defines linear severity interpolation
 *   rank 2 grid      NEAREST, BILINEAR (LINEAR is accepted and resolves to it)
 *   rank 2 strip     NEAREST, TRILINEAR (default) -- BILINEAR is rejected here,
 *                    because the strip is a flattened cube and not a 2-d grid
 *   rank 3 cube      NEAREST, TRILINEAR (default), TETRAHEDRAL
 * ---------------------------------------------------------------- */

/* Caller-supplied 1D table.
 * result: output interpolated value
 * table: size entries
 * size: number of entries (>= 2)
 * coord: input [0,1] coordinate
 * mode: LINEAR (0, the default), NEAREST or CATMULL_ROM, optionally
 *       | ALWAN_SAMPLE_STRICT
 *
 * CATMULL_ROM is the four-tap interpolating cubic, with the outer taps clamped
 * at the ends. It passes through every sample but, unlike LINEAR, it can
 * OVERSHOOT the values it interpolates between, by up to about 1/8 of a step
 * across a hard edge. Clamp the result if the table's range is a contract. */
alwan_status alwan_table1d_sample_f32(alwan_f32 *result,
                        alwan_f32 const *table, int size,
                        alwan_f32 coord, alwan_sample_mode mode);
alwan_status alwan_table1d_sample_f64(alwan_f64 *result,
                        alwan_f64 const *table, int size,
                        alwan_f64 coord, alwan_sample_mode mode);

/* Caller-supplied 2D grid: rows x cols scalars, row-major.
 * result: output interpolated value
 * table: rows * cols values, index = row*cols + col
 * rows, cols: extents (both >= 2)
 * row_coord, col_coord: input [0,1] coordinates on each axis
 * mode: LINEAR (0, the default, resolves to bilinear), NEAREST or BILINEAR,
 *       optionally | ALWAN_SAMPLE_STRICT
 *
 * The two axes are independent and may have different extents, which is what
 * separates this from the 2-d strip reader below: that one is a cube flattened
 * into two dimensions and is sampled trilinearly. */
alwan_status alwan_table2d_grid_sample_f32(alwan_f32 *result,
                        alwan_f32 const *table, int rows, int cols,
                        alwan_f32 row_coord, alwan_f32 col_coord,
                        alwan_sample_mode mode);
alwan_status alwan_table2d_grid_sample_f64(alwan_f64 *result,
                        alwan_f64 const *table, int rows, int cols,
                        alwan_f64 row_coord, alwan_f64 col_coord,
                        alwan_sample_mode mode);

/* Caller-supplied 2D strip: a cube flattened to (size*size) x size.
 * result: output interpolated RGB
 * strip: (size*size) * size * 3 values, RGB interleaved
 * size: cube edge length (>= 2)
 * coord: input [0,1] RGB coordinate
 * mode: LINEAR (0, the default) or NEAREST or TRILINEAR, optionally
 *       | ALWAN_SAMPLE_STRICT */
alwan_status alwan_table2d_sample_f32(alwan_rgb_f32 *result,
                        alwan_f32 const *strip, int size,
                        alwan_rgb_f32 const *coord, alwan_sample_mode mode);
alwan_status alwan_table2d_sample_f64(alwan_rgb_f64 *result,
                        alwan_f64 const *strip, int size,
                        alwan_rgb_f64 const *coord, alwan_sample_mode mode);

/* Caller-supplied 3D cube, R-fastest, RGB interleaved.
 * result: output interpolated RGB
 * cube: size^3 * 3 values
 * size: cube edge length (>= 2)
 * coord: input [0,1] RGB coordinate
 * mode: LINEAR (0, the default, resolves to trilinear), NEAREST, TRILINEAR
 *       or TETRAHEDRAL, optionally | ALWAN_SAMPLE_STRICT */
alwan_status alwan_table3d_sample_f32(alwan_rgb_f32 *result,
                        alwan_f32 const *cube, int size,
                        alwan_rgb_f32 const *coord, alwan_sample_mode mode);
alwan_status alwan_table3d_sample_f64(alwan_rgb_f64 *result,
                        alwan_f64 const *cube, int size,
                        alwan_rgb_f64 const *coord, alwan_sample_mode mode);

/* Caller-supplied 1D table of 3x3 matrices (a severity or temperature ramp).
 * result: output interpolated matrix
 * table: size matrices
 * size: number of entries (>= 2)
 * coord: input [0,1] coordinate
 * mode: LINEAR (0, the default) or NEAREST, optionally | ALWAN_SAMPLE_STRICT */
alwan_status alwan_table1d_mat3_sample_f32(alwan_mat3x3_f32 *result,
                        alwan_mat3x3_f32 const *table, int size,
                        alwan_f32 coord, alwan_sample_mode mode);
alwan_status alwan_table1d_mat3_sample_f64(alwan_mat3x3_f64 *result,
                        alwan_mat3x3_f64 const *table, int size,
                        alwan_f64 coord, alwan_sample_mode mode);

/* ---- Named readers for embedded tables ----
 *
 * No buffer parameter: the table IS the identity, so a caller cannot pass a
 * mismatched size. These are declared and exported in every build
 * configuration; a future per-table build switch changes what a reader
 * RETURNS (ALWAN_E_NODATA), never whether it LINKS. */

/* Machado 2009 CVD simulation matrix at a given severity.
 * result: output 3x3 matrix
 * cvd_type: which deficiency to look up
 * severity: 0.0 (none) to 1.0 (full)
 * mode: LINEAR (0, the default) or NEAREST, optionally | ALWAN_SAMPLE_STRICT */
alwan_status alwan_machado_matrix_sample_f32(alwan_mat3x3_f32 *result,
                        alwan_cvd_type cvd_type,
                        alwan_f32 severity, alwan_sample_mode mode);
alwan_status alwan_machado_matrix_sample_f64(alwan_mat3x3_f64 *result,
                        alwan_cvd_type cvd_type,
                        alwan_f64 severity, alwan_sample_mode mode);

/* Jakob 2019 spectral upsampling polynomial coefficients for an RGB coordinate.
 * result_c012: output 3 coefficients (c0, c1, c2)
 * gamut: which gamut's coefficient cubes to read
 * coord: input [0,1] RGB coordinate
 * mode: LINEAR (0, the default) or NEAREST or TRILINEAR, optionally
 *       | ALWAN_SAMPLE_STRICT */
alwan_status alwan_jakob2019_coeff_sample_f32(alwan_f32 *result_c012,
                        alwan_jakob2019_gamut gamut,
                        alwan_rgb_f32 const *coord, alwan_sample_mode mode);
alwan_status alwan_jakob2019_coeff_sample_f64(alwan_f64 *result_c012,
                        alwan_jakob2019_gamut gamut,
                        alwan_rgb_f64 const *coord, alwan_sample_mode mode);

/* AgX contrast curve sample. Reads the same 4096-entry LUT the AgX view
 * transform reads, with the same delta blend, so results match bit for bit.
 * result: output display value
 * sb2383: 0 for the original AgX curve, non-zero for the SB2383 curve
 * coord: input [0,1] normalized log2 coordinate
 * mode: LINEAR (0, the default) or NEAREST, optionally | ALWAN_SAMPLE_STRICT */
alwan_status alwan_agx_contrast_sample_f32(alwan_f32 *result, int sb2383,
                        alwan_f32 coord, alwan_sample_mode mode);
alwan_status alwan_agx_contrast_sample_f64(alwan_f64 *result, int sb2383,
                        alwan_f64 coord, alwan_sample_mode mode);

/* AgX Blender 57^3 picture-formation cube sample.
 * result: output RGB (power-2.4 encoded display values)
 * coord: input [0,1] RGB coordinate in the log2 allocation space
 * mode: LINEAR (0, the default, resolves to trilinear), NEAREST, TRILINEAR
 *       or TETRAHEDRAL, optionally | ALWAN_SAMPLE_STRICT */
alwan_status alwan_agx_blender_cube_sample_f32(alwan_rgb_f32 *result,
                        alwan_rgb_f32 const *coord, alwan_sample_mode mode);
alwan_status alwan_agx_blender_cube_sample_f64(alwan_rgb_f64 *result,
                        alwan_rgb_f64 const *coord, alwan_sample_mode mode);

/* The six alwan_lut{1,2,3}d_sample_{T} delegates left in 3.0.0: they were the
 * table readers below with LINEAR / TRILINEAR / TRILINEAR filled in and a
 * different argument order, so one operation had two spellings. Use
 * alwan_table1d_sample_{T}(result, lut, size, t, ALWAN_SAMPLE_LINEAR),
 * alwan_table2d_sample_{T}(result, strip, size, rgb, ALWAN_SAMPLE_TRILINEAR) and
 * alwan_table3d_sample_{T}(result, cube, size, rgb, ALWAN_SAMPLE_TRILINEAR). */

/* Invert a 3D LUT: build the cube that undoes it.
 * out: output, out_size^3 * 3 values, R-fastest. Must not alias lut.
 * out_size: the inverse cube's edge length, 2 to 256
 * lut, size: the forward cube, size^3 * 3 values R-fastest
 * iterations: Newton steps per node; <= 0 means 20, above 200 is ALWAN_E_RANGE
 * out_worst_residual: may be NULL; receives the largest |F(G(y)) - y| over the
 *      inverse's own nodes, where F is the forward cube sampled trilinearly
 *
 * The inverse is addressed over [0, 1] in the FORWARD cube's output space, and
 * a node outside that domain has nothing to invert.
 *
 * A cube is not invertible everywhere, and this does not pretend otherwise.
 * Where the forward table flattens, one preimage is as good as another and the
 * iteration settles on one of them. Where a node lies outside the forward
 * table's image there is no preimage at all, and the iteration ends on the
 * nearest point it can reach inside the unit cube. Neither is reported as an
 * error, because neither is one: the residual is the answer. Read
 * out_worst_residual and judge the table against it, since a large worst
 * residual over an inverse that is meant to be exact says the forward table
 * folds, and over one baked from a clipping view transform says only that the
 * clipped region cannot come back.
 *
 * The Jacobian is central differences over half a forward cell and the step
 * count is fixed, so a deterministic build takes one path through it. Cost is
 * out_size^3 * iterations * 7 trilinear samples. */
alwan_status alwan_lut3d_invert_f64(alwan_f64 *out, int out_size,
                        alwan_f64 const *lut, int size,
                        int iterations, alwan_f64 *out_worst_residual);
alwan_status alwan_lut3d_invert_f32(alwan_f32 *out, int out_size,
                        alwan_f32 const *lut, int size,
                        int iterations, alwan_f32 *out_worst_residual);

/* ----------------------------------------------------------------
 * .cube File Import / Export
 * ---------------------------------------------------------------- */

/* Export a 3D LUT to a .cube file.
 * path: output file path
 * lut: 3D LUT data (size^3 * 3, R-fastest)
 * size: cube edge length
 * title: optional title string (NULL for no title) */
alwan_status alwan_cube_export_3d_f64(char const *path,
                          alwan_f64 const *lut,
                          int size,
                          char const *title);
alwan_status alwan_cube_export_3d_f32(char const *path,
                          alwan_f32 const *lut,
                          int size,
                          char const *title);

/* Export a 1D LUT to a .cube file.
 * path: output file path
 * lut: 1D LUT data (size values)
 * size: number of entries
 * title: optional title string (NULL for no title) */
alwan_status alwan_cube_export_1d_f64(char const *path,
                          alwan_f64 const *lut,
                          int size,
                          char const *title);
alwan_status alwan_cube_export_1d_f32(char const *path,
                          alwan_f32 const *lut,
                          int size,
                          char const *title);

/* Export a 3D LUT to a .cube format in a memory buffer.
 * buf: output buffer
 * buf_size: buffer capacity
 * bytes_written: actual bytes written (output)
 * lut: 3D LUT data (size^3 * 3, R-fastest)
 * size: cube edge length
 * title: optional title string (NULL for no title)
 * Returns ALWAN_E_RANGE if buffer too small */
alwan_status alwan_cube_export_3d_buffer_f64(char *buf, size_t buf_size, size_t *bytes_written,
                                 alwan_f64 const *lut,
                                 int size,
                                 char const *title);
alwan_status alwan_cube_export_3d_buffer_f32(char *buf, size_t buf_size, size_t *bytes_written,
                                 alwan_f32 const *lut,
                                 int size,
                                 char const *title);

/* Import a 3D LUT from a .cube file.
 * lut: output buffer (caller must allocate: size^3 * 3 elements, alwan_f32 or
 *      alwan_f64 to match the call), or NULL to query the size only: the
 *      header is parsed, out_size is set, and no data is read
 * out_size: receives the cube edge length
 * path: input file path
 *
 * These take no capacity, so they write as many entries as the file's own
 * LUT_3D_SIZE declares, and each call re-reads the file. The usual flow is
 * query, allocate, read, and between those two reads the file is whatever is
 * on disk at the time: a file that grows in between is written past the end of
 * a buffer sized for the smaller one.
 *
 * For a file you did not write, or one something else can touch, read the
 * bytes once and use alwan_cube_import_3d_buffer_{T} for both calls. The
 * buffer form parses the bytes you are holding, so the size it reports and the
 * size it obeys cannot differ, and no capacity argument is needed to say so.
 *
 * Note also that the size query parses the header and stops. It does not
 * validate the body, so a successful query means the header is sane and
 * nothing more. */
alwan_status alwan_cube_import_3d_f64(alwan_f64 *lut, int *out_size,
                          char const *path);
alwan_status alwan_cube_import_3d_f32(alwan_f32 *lut, int *out_size,
                          char const *path);

/* Import a 1D LUT from a .cube file.
 * lut: output buffer (caller must allocate: size elements, alwan_f32 or
 *      alwan_f64 to match the call), or NULL to query the size only
 * out_size: receives the number of entries
 * path: input file path
 *
 * The same applies as for the 3D path form above: no capacity, the file is
 * read twice, and a file that can change between the two reads should go
 * through the buffer form instead. */
alwan_status alwan_cube_import_1d_f64(alwan_f64 *lut, int *out_size,
                          char const *path);
alwan_status alwan_cube_import_1d_f32(alwan_f32 *lut, int *out_size,
                          char const *path);

/* Import a 3D LUT from a .cube format memory buffer.
 * lut: output buffer (caller must allocate: size^3 * 3 elements, alwan_f32 or
 *      alwan_f64 to match the call), or NULL to query the size only
 * out_size: receives the cube edge length
 * buf: input buffer
 * buf_len: buffer length */
alwan_status alwan_cube_import_3d_buffer_f64(alwan_f64 *lut, int *out_size,
                                 char const *buf, size_t buf_len);
alwan_status alwan_cube_import_3d_buffer_f32(alwan_f32 *lut, int *out_size,
                                 char const *buf, size_t buf_len);

/* ----------------------------------------------------------------
 * .spi1d, .spi3d and .3dl Import / Export
 *
 * Three more interchange formats beside .cube: Sony Pictures Imageworks
 * .spi1d and .spi3d, and Autodesk .3dl in both its Flame and Lustre
 * flavours. Like .cube they record samples and nothing about meaning, so
 * what a table converts from and to is the caller's to track.
 *
 * The caller allocates, and lut = NULL queries the size from the header
 * alone, the same two-pass idiom the .cube importers use and with the same
 * caveat: the file is read twice and can change in between.
 *
 * A 3D LUT is held R-fastest in memory, the order .cube uses on disk, so the
 * sample at (r, g, b) is at ((b * size + g) * size + r) * 3. Both formats
 * here are B-fastest on disk and both directions transpose. Suite 161 holds
 * each reader to OCIO's own evaluation of the same file rather than to an
 * alwan round trip, because a round trip cannot see an ordering that is
 * wrong in both directions.
 *
 * Every value is checked finite before it is stored: sscanf("%lf") turns
 * "1e999" into an infinity and "nan" into a NaN without failing, and one
 * non-finite corner poisons every cell that interpolates through it.
 * ---------------------------------------------------------------- */

/* Import a 1D LUT from a .spi1d file.
 * lut: output, size * channels values interleaved, or NULL to query
 * out_size: receives the entry count (the file's Length)
 * out_channels: receives 1 or 3 (the file's Components), may be NULL
 * out_domain: receives the file's From pair, may be NULL
 * path: input file path
 *
 * The domain is reported, never applied. alwan's 1D samplers address [0, 1],
 * so a file whose From line is not "0 1" has to be mapped by the caller
 * before the table is sampled. A file with no Components line reads as one
 * channel, which is what the writers that omit it mean. */
alwan_status alwan_spi1d_import_f64(alwan_f64 *lut, int *out_size, int *out_channels,
                                    alwan_f64 *out_domain, char const *path);
alwan_status alwan_spi1d_import_f32(alwan_f32 *lut, int *out_size, int *out_channels,
                                    alwan_f32 *out_domain, char const *path);

/* Export a 1D LUT to a .spi1d file.
 * lut: size * channels values interleaved
 * channels: 1 or 3
 * domain_min, domain_max: the From pair; domain_max must exceed domain_min */
alwan_status alwan_spi1d_export_f64(char const *path, alwan_f64 const *lut, int size, int channels,
                                    alwan_f64 domain_min, alwan_f64 domain_max);
alwan_status alwan_spi1d_export_f32(char const *path, alwan_f32 const *lut, int size, int channels,
                                    alwan_f32 domain_min, alwan_f32 domain_max);

/* Import a 3D LUT from a .spi3d file.
 * lut: output, size^3 * 3 values R-fastest, or NULL to query the size
 * out_size: receives the cube edge length, 2 to 256
 *
 * The format carries an explicit index triple per line, so the file's line
 * order does not matter and a short file is caught by the sample count rather
 * than by reading a partial cube. Only cubic tables are accepted: a file
 * declaring unequal dimensions is ALWAN_E_RANGE. */
alwan_status alwan_spi3d_import_f64(alwan_f64 *lut, int *out_size, char const *path);
alwan_status alwan_spi3d_import_f32(alwan_f32 *lut, int *out_size, char const *path);

/* Export a 3D LUT to a .spi3d file. lut is size^3 * 3 values, R-fastest. */
alwan_status alwan_spi3d_export_f64(char const *path, alwan_f64 const *lut, int size);
alwan_status alwan_spi3d_export_f32(char const *path, alwan_f32 const *lut, int size);

/* Import a 3D LUT from a .3dl file, either flavour.
 * lut: output, size^3 * 3 values R-fastest, or NULL to query the size
 * out_size: receives the cube edge length, 2 to 256
 *
 * .3dl holds integers, so the output bit depth decides what they mean. The
 * Lustre flavour states it in its "Mesh <e> <d>" header and that is used;
 * the Flame flavour does not, and the depth is then inferred as the smallest
 * of 8, 10, 12, 14 and 16 bits that holds the largest value in the file. A
 * table that never reaches its own maximum therefore reads one stop bright,
 * and nothing in the file can prevent that.
 *
 * The mesh line names the cube edge wherever it is not itself three numbers
 * long. At size 3 it is, and the file is still decidable: with L three-number
 * lines, L a perfect cube means every line is data and L - 1 a perfect cube
 * means the first is the mesh, and consecutive cubes differ by more than one
 * so both cannot hold. OCIO 2.5 does not make that distinction and cannot
 * read back the size-3 Flame file it writes; this reader can. */
alwan_status alwan_3dl_import_f64(alwan_f64 *lut, int *out_size, char const *path);
alwan_status alwan_3dl_import_f32(alwan_f32 *lut, int *out_size, char const *path);

/* Export a 3D LUT to a .3dl file. lut is size^3 * 3 values, R-fastest.
 * bit_depth: 8, 10, 12, 14 or 16; <= 0 means 12, which is what the Lustre
 *            writers use. Values are scaled by 2^bit_depth - 1, rounded and
 *            clamped into range, so a table outside [0, 1] loses what lies
 *            outside it: this format cannot carry it.
 *
 * The Lustre header goes on where the size is 2^e + 1, which is the only
 * shape that header can state, and the Flame flavour is written otherwise. */
alwan_status alwan_3dl_export_f64(char const *path, alwan_f64 const *lut, int size, int bit_depth);
alwan_status alwan_3dl_export_f32(char const *path, alwan_f32 const *lut, int size, int bit_depth);

/* Import a 3D LUT from a Cinespace .csp file.
 * lut: output, size^3 * 3 values R-fastest, or NULL to query
 * out_size: receives the cube edge length, 2 to 256
 * prelut_in, prelut_out: output, the per-channel prelut, channel 0's points
 *      then channel 1's then channel 2's, packed by the counts reported in
 *      out_prelut_size. Both may be NULL; see below.
 * out_prelut_size: receives the three per-channel point counts, may be NULL
 *
 * .csp is R-fastest on disk, like .cube, unlike .spi3d and .3dl.
 *
 * The prelut is a per-channel piecewise-linear remap applied to the input
 * BEFORE the cube is addressed, which is how a shaper for log material is
 * stored, and each channel carries its own point count. It cannot be
 * delivered through a cube-only signature, so it is not quietly dropped:
 * passing NULL for prelut_in and prelut_out against a file whose prelut is
 * not the identity returns ALWAN_E_INVALID. The ordinary file, whose prelut
 * is the two-point identity every writer emits when there is no shaper,
 * reads with NULL.
 *
 * Query first with lut NULL to size the buffers: out_size and the three
 * counts are set from the header, and no sample data is read. A prelut of
 * more than 1024 points per channel is taken as a real shaper rather than
 * compared point by point, so it is refused when no buffers are supplied. */
alwan_status alwan_csp_import_3d_f64(alwan_f64 *lut, int *out_size, alwan_f64 *prelut_in,
                                     alwan_f64 *prelut_out, int *out_prelut_size, char const *path);
alwan_status alwan_csp_import_3d_f32(alwan_f32 *lut, int *out_size, alwan_f32 *prelut_in,
                                     alwan_f32 *prelut_out, int *out_prelut_size, char const *path);

/* Export a 3D LUT to a Cinespace .csp file.
 * lut: size^3 * 3 values, R-fastest
 * prelut_in, prelut_out, prelut_size: the per-channel prelut, or all NULL to
 *      write the two-point identity. When prelut_size is given, both value
 *      buffers must be too, and each count is 2 to 65536. */
alwan_status alwan_csp_export_3d_f64(char const *path, alwan_f64 const *lut, int size,
                                     alwan_f64 const *prelut_in, alwan_f64 const *prelut_out,
                                     int const *prelut_size);
alwan_status alwan_csp_export_3d_f32(char const *path, alwan_f32 const *lut, int size,
                                     alwan_f32 const *prelut_in, alwan_f32 const *prelut_out,
                                     int const *prelut_size);

/* Import a Resolve .cube: a 1D shaper and a 3D cube in one file.
 * lut: output, size^3 * 3 values R-fastest, or NULL to query the sizes
 * out_size: receives the cube edge length, 2 to 256
 * shaper: output, shaper_size * 3 values, one RGB row per sample, or NULL
 * out_shaper_size: receives the shaper's sample count, or 0 where the file has
 *      none, may be NULL
 * out_range: receives the shaper's LUT_1D_INPUT_RANGE pair, may be NULL
 *
 * The shaper is a per-channel curve, uniformly sampled over that input range,
 * whose output addresses the cube: it is how a cube stays small over log
 * material. Its rows sit above the cube's in the file and carry no marker of
 * their own, so a reader that skips only the keyword takes the first of them
 * as cube samples. alwan_cube_import_3d_{T} therefore refuses a file with a
 * LUT_1D_SIZE outright, with ALWAN_E_INVALID, rather than reading it wrong,
 * and alwan_cube_import_1d_{T} refuses one with a LUT_3D_SIZE for the same
 * reason: neither table is the transform on its own.
 *
 * A file with no shaper reads here too, reporting a shaper size of 0 and
 * leaving the shaper buffer untouched, so this one call takes any .cube. A
 * file that does have one and a NULL shaper buffer is ALWAN_E_INVALID. */
alwan_status alwan_cube_import_3d_shaper_f64(alwan_f64 *lut, int *out_size, alwan_f64 *shaper,
                                             int *out_shaper_size, alwan_f64 *out_range,
                                             char const *path);
alwan_status alwan_cube_import_3d_shaper_f32(alwan_f32 *lut, int *out_size, alwan_f32 *shaper,
                                             int *out_shaper_size, alwan_f32 *out_range,
                                             char const *path);

/* Export a Resolve .cube. shaper_size 0 writes a plain .cube and ignores the
 * shaper and the range; otherwise shaper holds shaper_size * 3 values and
 * range_max must exceed range_min. title may be NULL. */
alwan_status alwan_cube_export_3d_shaper_f64(char const *path, alwan_f64 const *lut, int size,
                                             alwan_f64 const *shaper, int shaper_size,
                                             alwan_f64 range_min, alwan_f64 range_max,
                                             char const *title);
alwan_status alwan_cube_export_3d_shaper_f32(char const *path, alwan_f32 const *lut, int size,
                                             alwan_f32 const *shaper, int shaper_size,
                                             alwan_f32 range_min, alwan_f32 range_max,
                                             char const *title);

/* Read and write a Sony .spimtx: a 3x3 matrix and a per-channel offset.
 * matrix: row-major, as alwan_mat3x3 stores it
 * offset: three values in the same units as the data, may be NULL
 *
 * The file holds the offsets in 16-bit code units, so an offset of 65535 on
 * disk adds exactly 1.0 to that channel. Measured against OCIO rather than
 * read off a specification. These entry points take and return the offset in
 * the values' own units, so the file's convention stays in the file. */
alwan_status alwan_spimtx_import_f64(alwan_mat3x3_f64 *matrix, alwan_f64 *offset, char const *path);
alwan_status alwan_spimtx_import_f32(alwan_mat3x3_f32 *matrix, alwan_f32 *offset, char const *path);
alwan_status alwan_spimtx_export_f64(char const *path, alwan_mat3x3_f64 const *matrix,
                                     alwan_f64 const *offset);
alwan_status alwan_spimtx_export_f32(char const *path, alwan_mat3x3_f32 const *matrix,
                                     alwan_f32 const *offset);

/* ----------------------------------------------------------------
 * CLF Import
 *
 * CLF (Common LUT Format, SMPTE ST 2136-1) records a transform's operations
 * rather than only their samples, which is why an ACES LMT or an OCIO
 * transform survives a trip through it where a .cube does not. These read one
 * back and evaluate it.
 *
 * Every ProcessNode type CLF defines is understood: Matrix, Range, Exponent,
 * LUT1D, LUT3D, ASC_CDL and Log. A file carrying anything else, such as one of
 * the nodes OCIO writes into a CTF, or a style outside those seven nodes'
 * lists, is REFUSED with ALWAN_E_NODATA rather than partly applied, because a
 * pipeline missing one of its stages is not the transform, and a wrong answer
 * is worse than none.
 *
 * The semantics were measured against OpenColorIO reading the same file rather
 * than transcribed, and suite 162 pins them. Note that CLF orders a LUT3D
 * array with BLUE varying fastest where alwan and .cube are R-fastest; the
 * reader transposes, and so does the writer since 2026-09-18.
 * ---------------------------------------------------------------- */

typedef struct alwan_clf_s alwan_clf;

typedef enum {
    ALWAN_CLF_NODE_MATRIX = 0,
    ALWAN_CLF_NODE_RANGE = 1,
    ALWAN_CLF_NODE_EXPONENT = 2,
    ALWAN_CLF_NODE_LUT1D = 3,
    ALWAN_CLF_NODE_LUT3D = 4,
    ALWAN_CLF_NODE_ASC_CDL = 5,
    ALWAN_CLF_NODE_LOG = 6
} alwan_clf_node_type;

/* Read a ProcessList. The object owns its tables; free it with
 * alwan_clf_destroy. ctx is accepted for symmetry and not used.
 *
 * ALWAN_E_NODATA: not a CLF, an empty ProcessList, or a node type or style
 *      this reader does not implement. ALWAN_E_INVALID: a file that cannot be
 *      opened, or malformed XML, or a value that is not finite.
 *      ALWAN_E_RANGE: a file above 64 MiB, a ProcessList of more than 256
 *      nodes, or a LUT dimension outside 2 to 65536 (1D) or 2 to 256 (3D). */
alwan_status alwan_clf_import(alwan_clf **out, char const *path, alwan_ctx *ctx);
alwan_status alwan_clf_import_buffer(alwan_clf **out, char const *buf, size_t len, alwan_ctx *ctx);
void alwan_clf_destroy(alwan_clf *clf, alwan_ctx *ctx);

/* What was read: how many nodes, what each one is, and the ProcessList's id.
 * The id points into the object and lives as long as it does. */
size_t alwan_clf_node_count(alwan_clf const *clf);
alwan_status alwan_clf_node_type_at(alwan_clf_node_type *out, alwan_clf const *clf, size_t index);
char const *alwan_clf_id(alwan_clf const *clf);

/* Apply the whole ProcessList, in order, to interleaved RGB.
 * Strides are in bytes; 0 means tightly packed. Evaluation runs in double on
 * both paths, because the file is decimal text and there is nothing an f32
 * pass would preserve. */
alwan_status alwan_clf_apply_f64_map_interleave(alwan_f64 *out, size_t out_stride,
                                                alwan_f64 const *in, size_t in_stride,
                                                size_t count, alwan_clf const *clf);
alwan_status alwan_clf_apply_f32_map_interleave(alwan_f32 *out, size_t out_stride,
                                                alwan_f32 const *in, size_t in_stride,
                                                size_t count, alwan_clf const *clf);
/* Planar twin: the interleave form run on a packed tile, so identical to it, kernels included (suite 175). */
alwan_status alwan_clf_apply_f32_map_planar(alwan_f32 *out_ch0, size_t out_stride, alwan_f32 *out_ch1, alwan_f32 *out_ch2, alwan_f32 const *in_ch0, size_t in_stride, alwan_f32 const *in_ch1, alwan_f32 const *in_ch2, size_t count, alwan_clf const *clf);
alwan_status alwan_clf_apply_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_clf const *clf);

/* ----------------------------------------------------------------
 * CLF's parametric curves, without a file
 *
 * The Exponent and Log ProcessNodes are two families of curve with a handful
 * of parameters each, and most camera encodings are one of them: ACEScct is
 * cameraLinToLog in base 2 with a linear segment below 0.0078125, LogC3 is
 * the same shape in base 10. These evaluate a curve from its parameters,
 * forward or reverse, with exactly the arithmetic the reader gives the same
 * node in a file, so a curve published as CLF (or OpenColorIO) parameters is
 * reachable without writing the file. Measured against OpenColorIO's
 * ExponentTransform, ExponentWithLinearTransform, LogTransform,
 * LogAffineTransform and LogCameraTransform, suite 178, which also holds the
 * parametric form to the reader bit for bit. Evaluation runs in double on
 * both paths. Strides are in bytes; 0 means tightly packed, as for the CLF
 * apply above.
 * ---------------------------------------------------------------- */

typedef enum {
    ALWAN_CLF_EXPONENT_BASIC = 0,      /* max(x, 0)^g; the reverse is the 1/g power */
    ALWAN_CLF_EXPONENT_MONCURVE = 1    /* a linear segment below a / (g - 1), joined in value and
                                          slope to ((x + a) / (1 + a))^g; negatives stay on the segment */
} alwan_clf_exponent_style;

typedef struct {
    alwan_clf_exponent_style style;
    alwan_f64 gamma;    /* g, above 0; MONCURVE needs g above 1 */
    alwan_f64 offset;   /* a, MONCURVE only, 0 or above (sRGB is g 2.4, a 0.055) */
} alwan_clf_exponent_params;

typedef enum {
    ALWAN_CLF_LOG_PLAIN = 0,        /* log_base(x), the input floored at the smallest normal float32
                                       as OpenColorIO floors it; the reverse is base^x */
    ALWAN_CLF_LOG_LIN_TO_LOG = 1,   /* log_side_slope * log_base(lin_side_slope * x + lin_side_offset)
                                       + log_side_offset, the same floor; the reverse solves it */
    ALWAN_CLF_LOG_CAMERA = 2        /* LIN_TO_LOG with a linear segment below lin_side_break */
} alwan_clf_log_style;

typedef struct {
    alwan_clf_log_style style;
    alwan_f64 base;                 /* above 0 and not 1; CLF's log2 and log10 styles are PLAIN in base 2 or 10 */
    alwan_f64 log_side_slope;       /* LIN_TO_LOG and CAMERA; not 0 */
    alwan_f64 log_side_offset;
    alwan_f64 lin_side_slope;       /* not 0 */
    alwan_f64 lin_side_offset;
    alwan_f64 lin_side_break;       /* CAMERA: where the segment ends, in linear units; the log
                                       curve's argument there must be positive */
    alwan_f64 linear_slope;         /* CAMERA: the segment's slope, or 0 for the curve's own slope
                                       at the break, which is what a file without linearSlope means */
} alwan_clf_log_params;

/* Defaults: BASIC with gamma 1; LIN_TO_LOG in base 2 with unit slopes and zero offsets. */
void alwan_clf_exponent_params_init(alwan_clf_exponent_params *params);
void alwan_clf_log_params_init(alwan_clf_log_params *params);

/* Apply to count values. inverse non-zero applies the Rev / logToLin direction.
 * ALWAN_E_INVALID for a NULL, a style outside the enum, or parameters the curve
 * cannot be built from (see the fields). */
alwan_status alwan_clf_exponent_apply_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_clf_exponent_params const *params, int inverse);
alwan_status alwan_clf_exponent_apply_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_clf_exponent_params const *params, int inverse);
alwan_status alwan_clf_log_apply_f64(alwan_f64 *out, size_t out_stride, alwan_f64 const *in, size_t in_stride, size_t count, alwan_clf_log_params const *params, int inverse);
alwan_status alwan_clf_log_apply_f32(alwan_f32 *out, size_t out_stride, alwan_f32 const *in, size_t in_stride, size_t count, alwan_clf_log_params const *params, int inverse);

/* ----------------------------------------------------------------
 * Color Interop Forum -- Interop ID Strings
 *
 * Bidirectional lookup between alwan_rgb_space enum values and
 * canonical Color Interop Forum string identifiers.
 * Reference: ASWF Color Interop Forum specification
 * ---------------------------------------------------------------- */

/* Parse a Color Interop Forum ID string to an alwan_rgb_space enum.
 * Returns ALWAN_OK on success, ALWAN_E_NODATA if ID not recognized. */
alwan_status alwan_interop_parse(alwan_rgb_space *space, char const *id);

/* Get the Color Interop Forum ID string for an alwan_rgb_space enum.
 * Returns the canonical string, or NULL if the space has no interop ID. */
char const *alwan_interop_format(alwan_rgb_space space);

/* Get the total number of registered interop ID entries. */
size_t alwan_interop_count(void);

/* Get the interop entry at the given index (for enumeration).
 * space and id may be NULL if not needed.
 * Returns ALWAN_E_RANGE if index is out of bounds. */
alwan_status alwan_interop_entry_at(alwan_rgb_space *space, char const **id, size_t index);

/* What the interop tables know about a space, for a UI or a file writer that
 * has to decide rather than look up one string. Every field is derived from
 * the tables and the space's descriptor; nothing here is a judgement alwan
 * makes on its own:
 *   id                the ID alwan_interop_format returns: the Forum's
 *                     scene-referred ID where one is published, else the
 *                     Forum's display ID, else the alwan: namespace
 *   display_id        the Forum's display-referred ID for the same space,
 *                     when the Forum publishes one beside a scene-referred
 *                     one (srgb_rec709_display for sRGB, and so on); NULL
 *                     otherwise. A writer that has to label display-referred
 *                     pixels uses this one.
 *   published         1 if id is a Forum ID, 0 if it is the alwan: namespace
 *   scene_referred    1 if id ends in _scene
 *   display_referred  1 if id ends in _display, or display_id is set
 *   hdr               1 if the space's transfer function is PQ or HLG
 *   transfer          the space's transfer function, as its descriptor says
 * "Basic" as the Forum's texture-asset recommendation uses the word is NOT
 * here: the recommendation text is not vendored, so the subset cannot be
 * transcribed from it, and a guess would be worse than the absence.
 * Returns ALWAN_OK, ALWAN_E_INVALID for a NULL out, ALWAN_E_NODATA for a
 * space with no interop entry, and whatever the descriptor lookup returns. */
typedef struct {
    char const *id;
    char const *display_id;
    int published;
    int scene_referred;
    int display_referred;
    int hdr;
    alwan_transfer_function transfer;
} alwan_interop_info;
alwan_status alwan_interop_query(alwan_interop_info *out, alwan_rgb_space space);
/* The same, from an ID string: any ID alwan_interop_parse accepts, aliases included. */
alwan_status alwan_interop_query_id(alwan_interop_info *out, char const *id);

/* ----------------------------------------------------------------
 * Neural layer kernels (roadmap 3.10, step two)
 *
 * The layers a small colour-science network is made of, run by alwan itself
 * so that a deterministic build is bit-exact across backends: fixed summation
 * order, no fused multiply-add, the committed exp. The per-element kernels
 * are in core/alwan_nn_core.h, written against ALWAN_NN_READ_* accessors so a
 * shader binds its own buffers (see core/alwan_nn_reader.inc); these are
 * those kernels in a loop over the output, validated once.
 *
 * LAYOUT, and it is not PyTorch's. Tensors are channels-last: an image
 * element is in[(y * W + x) * C + c]. Convolution weights are HWIO,
 * w[((ky * KW + kx) * Cg + ci) * Cout + o] with Cg = Cin / groups; a dense
 * weight is w[i * n_out + o]. A model converted for alwan carries its weights
 * already in this order and nothing is transposed at run time. Shapes are the
 * caller's: the output buffer must hold what the layer produces, stated below.
 * f32 is the compute type every model in scope was trained in; f64 is the
 * reference path the f32 one is checked against.
 *
 * Reference: PyTorch, called from gendata on seeded random tensors, per layer
 * (suite 176). GELU is the tanh form, torch's approximate='tanh'.
 * ---------------------------------------------------------------- */

typedef enum {
    ALWAN_NN_ACTIVATION_RELU = 0,
    ALWAN_NN_ACTIVATION_LEAKY_RELU = 1,   /* alpha x below zero */
    ALWAN_NN_ACTIVATION_SIGMOID = 2,
    ALWAN_NN_ACTIVATION_TANH = 3,
    ALWAN_NN_ACTIVATION_GELU = 4          /* the tanh form */
} alwan_nn_activation_kind;

typedef enum { ALWAN_NN_POOL_MAX = 0, ALWAN_NN_POOL_AVG = 1 } alwan_nn_pool_kind;
typedef enum { ALWAN_NN_UPSAMPLE_NEAREST = 0, ALWAN_NN_UPSAMPLE_BILINEAR = 1 } alwan_nn_upsample_kind;

/* Dense: out[n_out] = in[n_in] w[n_in x n_out] + b[n_out]; b may be NULL. */
alwan_status alwan_nn_dense_f32(alwan_f32 *out, alwan_f32 const *in, alwan_f32 const *w, alwan_f32 const *b, int n_in, int n_out);
alwan_status alwan_nn_dense_f64(alwan_f64 *out, alwan_f64 const *in, alwan_f64 const *w, alwan_f64 const *b, int n_in, int n_out);

/* 2D convolution (cross-correlation, as every framework means it) of an
 * H x W x Cin image by KH x KW x (Cin / groups) x Cout weights, one stride and
 * one zero padding on both axes, b[Cout] or NULL. groups divides Cin and Cout;
 * groups == Cin == Cout is depthwise. The output is
 * ((H + 2 pad - KH) / stride + 1) x ((W + 2 pad - KW) / stride + 1) x Cout.
 * ALWAN_E_RANGE if the padded image is smaller than the kernel. */
alwan_status alwan_nn_conv2d_f32(alwan_f32 *out, alwan_f32 const *in, int H, int W, int Cin, alwan_f32 const *w, alwan_f32 const *b, int KH, int KW, int Cout, int stride, int pad, int groups);
alwan_status alwan_nn_conv2d_f64(alwan_f64 *out, alwan_f64 const *in, int H, int W, int Cin, alwan_f64 const *w, alwan_f64 const *b, int KH, int KW, int Cout, int stride, int pad, int groups);

/* Elementwise activation; alpha is read by LEAKY_RELU only. out may be in. */
alwan_status alwan_nn_activation_f32(alwan_f32 *out, alwan_f32 const *in, size_t count, alwan_nn_activation_kind kind, alwan_f32 alpha);
alwan_status alwan_nn_activation_f64(alwan_f64 *out, alwan_f64 const *in, size_t count, alwan_nn_activation_kind kind, alwan_f64 alpha);

/* Max or average pooling over k x k at one stride with zero padding, output
 * ((H + 2 pad - k) / stride + 1) squared x C. An average divides by k * k,
 * padded positions included (PyTorch's count_include_pad default); a max
 * skips them. pad above k / 2 is ALWAN_E_RANGE, PyTorch's own rule. */
alwan_status alwan_nn_pool2d_f32(alwan_f32 *out, alwan_f32 const *in, int H, int W, int C, int k, int stride, int pad, alwan_nn_pool_kind kind);
alwan_status alwan_nn_pool2d_f64(alwan_f64 *out, alwan_f64 const *in, int H, int W, int C, int k, int stride, int pad, alwan_nn_pool_kind kind);

/* Global average over the image: out[C]. */
alwan_status alwan_nn_global_avg_f32(alwan_f32 *out, alwan_f32 const *in, int H, int W, int C);
alwan_status alwan_nn_global_avg_f64(alwan_f64 *out, alwan_f64 const *in, int H, int W, int C);

/* Upsampling by an integer factor to (H scale) x (W scale) x C: nearest takes
 * the source at floor(dst / scale); bilinear is PyTorch's align_corners=False. */
alwan_status alwan_nn_upsample2d_f32(alwan_f32 *out, alwan_f32 const *in, int H, int W, int C, int scale, alwan_nn_upsample_kind kind);
alwan_status alwan_nn_upsample2d_f64(alwan_f64 *out, alwan_f64 const *in, int H, int W, int C, int scale, alwan_nn_upsample_kind kind);

/* Softmax over n, max-subtracted. out may be in. */
alwan_status alwan_nn_softmax_f32(alwan_f32 *out, alwan_f32 const *in, int n);
alwan_status alwan_nn_softmax_f64(alwan_f64 *out, alwan_f64 const *in, int n);

/* Elementwise sum, and channel concatenation of two H x W images. */
alwan_status alwan_nn_add_f32(alwan_f32 *out, alwan_f32 const *a, alwan_f32 const *b, size_t count);
alwan_status alwan_nn_add_f64(alwan_f64 *out, alwan_f64 const *a, alwan_f64 const *b, size_t count);
alwan_status alwan_nn_concat_channels_f32(alwan_f32 *out, alwan_f32 const *a, int Ca, alwan_f32 const *b, int Cb, int H, int W);
alwan_status alwan_nn_concat_channels_f64(alwan_f64 *out, alwan_f64 const *a, int Ca, alwan_f64 const *b, int Cb, int H, int W);


/* ----------------------------------------------------------------
 * Color Interop Forum -- float16 (half-float) Conversion
 * ---------------------------------------------------------------- */

/* Convert float16 (IEEE 754 binary16) samples to float32.
 * out: output float32 buffer
 * in: input uint16_t buffer containing float16 bit patterns
 * count: number of samples
 * No precision in the signature, so no precision suffix: until 3.0.0 these two
 * and the three above (lut2d_dimensions, interop_parse, interop_entry_at) came
 * as identical _f32/_f64 pairs, one forwarding to the other. */
alwan_status alwan_half_to_float(alwan_f32 *out, alwan_uint16 const *in, size_t count);

/* Convert float32 samples to float16 (IEEE 754 binary16).
 * out: output uint16_t buffer for float16 bit patterns
 * in: input float32 buffer
 * count: number of samples */
alwan_status alwan_float_to_half(alwan_uint16 *out, alwan_f32 const *in, size_t count);

/* ----------------------------------------------------------------
 * CLF (Common LUT Format) Export -- SMPTE ST 2136-1:2024
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
 * src_space/dst_space: source and destination RGB space descriptors
 * id: CLF ProcessList id attribute (NULL for default)
 * name: CLF ProcessList name attribute (NULL to omit)
 * lut_size: resolution for baked LUT1D nodes (default 4096 if < 2)
 * ctx: context for chromatic adaptation (may be NULL if white points match) */
alwan_status alwan_clf_export_f64(char const *path, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
alwan_status alwan_clf_export_f32(char const *path, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* Export a CLF file with a view transform (tone mapping) included.
 * Same as alwan_clf_export_f64 but adds a LUT3D ProcessNode for the
 * view transform between the color space conversion and OETF. */
alwan_status alwan_clf_export_view_f64(char const *path, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
alwan_status alwan_clf_export_view_f32(char const *path, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* Export CLF to a memory buffer.
 * Returns ALWAN_E_RANGE if buffer too small. */
alwan_status alwan_clf_export_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
alwan_status alwan_clf_export_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* Export CLF with view transform to a memory buffer. */
alwan_status alwan_clf_export_view_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);
alwan_status alwan_clf_export_view_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx);

/* ----------------------------------------------------------------
 * Video Signal Encoding / Decoding
 *
 * Combines transfer function + range scaling + quantization.
 * Pipeline: encode = linear -> OETF -> range scale -> quantize
 *           decode = dequantize -> range unscale -> EOTF -> linear
 * ---------------------------------------------------------------- */

/* Encode: linear RGB -> video signal (TF + range + quantization).
 * out: output buffer (3-channel packed pixels in out_fmt)
 * rgb_linear: input linear RGB triplets (count * 3 elements, alwan_f32 or alwan_f64 to match the call)
 * count: number of pixels
 * out_fmt: output pixel format (U8, U16, F32, F64)
 * space: RGB color space (determines OETF)
 * range: ALWAN_VIDEO_RANGE_FULL or ALWAN_VIDEO_RANGE_NARROW
 * bit_depth: video bit depth (8, 10, 12, 16); 0 = derive from out_fmt
 * ctx: context for space descriptor lookup */
alwan_status alwan_video_encode_f64(void *out, alwan_f64 const *rgb_linear, size_t count, alwan_pixel_format out_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);
alwan_status alwan_video_encode_f32(void *out, alwan_f32 const *rgb_linear, size_t count, alwan_pixel_format out_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);

/* Decode: video signal -> linear RGB.
 * rgb_linear: output linear RGB triplets (count * 3 elements, alwan_f32 or alwan_f64 to match the call)
 * in: input buffer (3-channel packed pixels in in_fmt)
 * count: number of pixels
 * in_fmt: input pixel format (U8, U16, F32, F64)
 * space: RGB color space (determines EOTF)
 * range: ALWAN_VIDEO_RANGE_FULL or ALWAN_VIDEO_RANGE_NARROW
 * bit_depth: video bit depth (8, 10, 12, 16); 0 = derive from in_fmt
 * ctx: context for space descriptor lookup */
alwan_status alwan_video_decode_f64(alwan_f64 *rgb_linear, void const *in, size_t count, alwan_pixel_format in_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);
alwan_status alwan_video_decode_f32(alwan_f32 *rgb_linear, void const *in, size_t count, alwan_pixel_format in_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ALWAN_H */
