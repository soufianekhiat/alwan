/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
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

/* Get D65 illuminant data (2 values: x, y)
 * In embedded mode: returns pointer to static data (no deallocation needed)
 * In runtime mode: allocates memory (caller must free with alwan_data_free) */
int alwan_data_get_d65(alwan_ctx *ctx, Scalar **data, size_t *count);

/* Get D60 illuminant data (2 values: x, y) */
int alwan_data_get_d60(alwan_ctx *ctx, Scalar **data, size_t *count);

/* Get sRGB primaries (6 values: rx, ry, gx, gy, bx, by) */
int alwan_data_get_srgb_primaries(alwan_ctx *ctx, Scalar **data, size_t *count);

#if !ALWAN_EMBED_DATA
/* Free data allocated by runtime loader (no-op in embedded mode) */
void alwan_data_free(alwan_ctx *ctx, Scalar *data);
#endif

/* ----------------------------------------------------------------
 * Math Types
 * ---------------------------------------------------------------- */

/* 3-component vector */
typedef struct {
    Scalar v[3];
} alwan_vec3;

/* 3x3 matrix stored in row-major order: [m00 m01 m02 m10 m11 m12 m20 m21 m22] */
typedef struct {
    Scalar m[9];
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

/* RGB space descriptor with primaries, white point, and transfer function names */
typedef struct {
    Scalar primaries_xy[6];  /* rx, ry, gx, gy, bx, by in CIE xy chromaticity */
    Scalar white_xy[2];       /* wx, wy in CIE xy chromaticity */
    char const *oetf_name;    /* Optional: name of OETF (e.g., "srgb", "pq") */
    char const *eotf_name;    /* Optional: name of EOTF (e.g., "srgb", "pq") */
} alwan_rgb_space_desc;

/* Derive RGB↔XYZ conversion matrices from primaries and white point
 * Returns ALWAN_OK on success, ALWAN_E_RANGE if primaries/white form singular matrix */
int alwan_rgb_derive_matrices(alwan_rgb_space_desc const *desc,
                               alwan_mat3x3 *rgb_to_xyz,
                               alwan_mat3x3 *xyz_to_rgb);

/* ----------------------------------------------------------------
 * Transfer Functions (OETF/EOTF)
 * ---------------------------------------------------------------- */

/* Apply Opto-Electronic Transfer Function (linear → encoded)
 * Supported names: "srgb", "pq"/"st2084", "hlg", "acesproxy"
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if name not recognized */
int alwan_oetf_apply(char const *name,
                     Scalar const *linear, size_t count, size_t in_stride,
                     Scalar *encoded, size_t out_stride);

/* Apply Electro-Optical Transfer Function (encoded → linear)
 * Supported names: "srgb", "pq"/"st2084", "hlg", "bt1886", "acesproxy"
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if name not recognized */
int alwan_eotf_apply(char const *name,
                     Scalar const *encoded, size_t count, size_t in_stride,
                     Scalar *linear, size_t out_stride);

/* ----------------------------------------------------------------
 * View Transforms (Display Rendering)
 * ---------------------------------------------------------------- */

/* Apply a view transform (display rendering transform) to RGB data
 * View transforms convert scene-referred RGB to display-referred RGB
 * Supported names: "aces_rec709", "aces_rec2020", "agx", "agx_punchy"
 *
 * ctx: optional context (can be NULL for stateless transforms)
 * name: view transform name
 * rgb_in: input RGB triplets (scene-referred, typically ACES AP1 or linear)
 * count: number of RGB triplets
 * in_stride: stride between input RGB triplets (in Scalars, typically 3)
 * rgb_out: output RGB triplets (display-referred)
 * out_stride: stride between output RGB triplets (in Scalars, typically 3)
 *
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if name not recognized */
int alwan_view_transform_apply(alwan_ctx *ctx,
                                char const *name,
                                Scalar const *rgb_in, size_t count, size_t in_stride,
                                Scalar *rgb_out, size_t out_stride);

/* ----------------------------------------------------------------
 * Color Space Conversions
 * ---------------------------------------------------------------- */

/* XYZ ↔ xyY conversions */
void alwan_xyz_to_xyy(alwan_vec3 const *xyz, alwan_vec3 *xyy);
void alwan_xyy_to_xyz(alwan_vec3 const *xyy, alwan_vec3 *xyz);

/* XYZ ↔ Lab conversions (requires white point in XYZ) */
void alwan_xyz_to_lab(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *lab);
void alwan_lab_to_xyz(alwan_vec3 const *lab, alwan_vec3 const *white_xyz, alwan_vec3 *xyz);

/* XYZ ↔ Luv conversions (requires white point in XYZ) */
void alwan_xyz_to_luv(alwan_vec3 const *xyz, alwan_vec3 const *white_xyz, alwan_vec3 *luv);
void alwan_luv_to_xyz(alwan_vec3 const *luv, alwan_vec3 const *white_xyz, alwan_vec3 *xyz);

/* Lab ↔ LCh(ab) conversions */
void alwan_lab_to_lch(alwan_vec3 const *lab, alwan_vec3 *lch);
void alwan_lch_to_lab(alwan_vec3 const *lch, alwan_vec3 *lab);

/* Luv ↔ LCh(uv) conversions */
void alwan_luv_to_lchuv(alwan_vec3 const *luv, alwan_vec3 *lchuv);
void alwan_lchuv_to_luv(alwan_vec3 const *lchuv, alwan_vec3 *luv);

/* ----------------------------------------------------------------
 * Color Difference (ΔE) Metrics
 * ---------------------------------------------------------------- */

/* ΔE*76 - Euclidean distance in Lab space */
Scalar alwan_delta_e_76(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE*94 - CIE 1994 color difference (graphic arts defaults: kL=1, K1=0.045, K2=0.015) */
Scalar alwan_delta_e_94(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ΔE CMC(l:c) - CMC color difference (defaults: l=2, c=1 for acceptability) */
Scalar alwan_delta_e_cmc(alwan_vec3 const *lab1, alwan_vec3 const *lab2, Scalar l, Scalar c);

/* ΔE*00 - CIEDE2000 color difference (most perceptually uniform) */
Scalar alwan_delta_e_2000(alwan_vec3 const *lab1, alwan_vec3 const *lab2);

/* ----------------------------------------------------------------
 * Chromatic Adaptation Transform (CAT)
 * ---------------------------------------------------------------- */

/* Chromatic Adaptation Transform (CAT) method */
typedef enum {
    ALWAN_CAT_XYZ_SCALING = 0,  /* Von Kries in XYZ space (simplest) */
    ALWAN_CAT_BRADFORD    = 1,  /* Bradford (most common, used in ICC) */
    ALWAN_CAT_CAT02       = 2,  /* CAT02 (from CIECAM02) */
    ALWAN_CAT_CAT16       = 3   /* CAT16 (from CAM16) */
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
int alwan_xyz_adapt(Scalar const *xyz_in, size_t count, size_t in_stride,
                    alwan_vec3 const *src_white_xyz,
                    alwan_vec3 const *dst_white_xyz,
                    alwan_cat_method method,
                    Scalar *xyz_out, size_t out_stride);

/* ----------------------------------------------------------------
 * Utility Functions
 * ---------------------------------------------------------------- */

/* Clamp scalar to [min, max] */
static inline Scalar alwan_clamp(Scalar x, Scalar min, Scalar max) {
    return (x < min) ? min : (x > max) ? max : x;
}

/* Linear interpolation (numerically stable) */
static inline Scalar alwan_lerp(Scalar a, Scalar b, Scalar t) {
    return ((Scalar)1.0 - t) * a + t * b;
}

#ifdef __cplusplus
}
#endif

#endif /* ALWAN_H */
