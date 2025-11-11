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
 * Supported names: "srgb"
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if name not recognized */
int alwan_oetf_apply(char const *name,
                     Scalar const *linear, size_t count, size_t in_stride,
                     Scalar *encoded, size_t out_stride);

/* Apply Electro-Optical Transfer Function (encoded → linear)
 * Supported names: "srgb"
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if name not recognized */
int alwan_eotf_apply(char const *name,
                     Scalar const *encoded, size_t count, size_t in_stride,
                     Scalar *linear, size_t out_stride);

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
