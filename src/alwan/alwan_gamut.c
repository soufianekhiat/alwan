/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * M11: Gamut Utilities & Mapping
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Random number generation (simple LCG for reproducibility)
 * ---------------------------------------------------------------- */

typedef struct {
    unsigned int state;
} alwan_rng;

static void alwan_rng_init(alwan_rng *rng, unsigned int seed) {
    rng->state = seed;
}

/* Generate random number in [0, 1] */
static alwan_scalar alwan_rng_uniform(alwan_rng *rng) {
    /* Simple LCG: Numerical Recipes parameters */
    rng->state = rng->state * 1664525u + 1013904223u;
    return (alwan_scalar)rng->state / (alwan_scalar)0xFFFFFFFFu;
}

/* ----------------------------------------------------------------
 * M11: Gamut Volume Estimation (Monte Carlo)
 * ---------------------------------------------------------------- */

int alwan_gamut_volume_mc(alwan_rgb_space_desc const *space,
                          size_t num_samples,
                          unsigned int seed,
                          alwan_scalar *volume) {
    (void)num_samples;  /* Not used - determinant is exact */
    (void)seed;         /* Not used - determinant is exact */

    if (!space || !volume) {
        return ALWAN_E_INVALID;
    }

    /* Derive RGB->XYZ matrix */
    alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices(space, &rgb_to_xyz, &xyz_to_rgb);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Compute determinant of RGB->XYZ matrix
     * The volume of the RGB gamut in XYZ space is |det(M)| */
    alwan_scalar const det =
        rgb_to_xyz.m[0] * (rgb_to_xyz.m[4] * rgb_to_xyz.m[8] - rgb_to_xyz.m[5] * rgb_to_xyz.m[7]) -
        rgb_to_xyz.m[1] * (rgb_to_xyz.m[3] * rgb_to_xyz.m[8] - rgb_to_xyz.m[5] * rgb_to_xyz.m[6]) +
        rgb_to_xyz.m[2] * (rgb_to_xyz.m[3] * rgb_to_xyz.m[7] - rgb_to_xyz.m[4] * rgb_to_xyz.m[6]);

    if (ALWAN_FABS(det) < ALWAN_EPSILON) {
        return ALWAN_E_RANGE;
    }

    /* Volume is the absolute value of the determinant */
    *volume = ALWAN_FABS(det);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * M11: Gamut Mapping
 * ---------------------------------------------------------------- */

/* Clip RGB to [0,1] range */
static void gamut_map_clip_single(alwan_vec3 const *rgb_in, alwan_vec3 *rgb_out) {
    for (int i = 0; i < 3; i++) {
        if (rgb_in->v[i] < ALWAN_LITERAL(0.0)) {
            rgb_out->v[i] = ALWAN_LITERAL(0.0);
        } else if (rgb_in->v[i] > ALWAN_LITERAL(1.0)) {
            rgb_out->v[i] = ALWAN_LITERAL(1.0);
        } else {
            rgb_out->v[i] = rgb_in->v[i];
        }
    }
}

/* Hue-preserving gamut mapping: scale towards neutral until in gamut */
static void gamut_map_hue_preserving_single(alwan_vec3 const *rgb_in, alwan_vec3 *rgb_out) {
    /* If already in gamut, return as-is */
    if (rgb_in->v[0] >= ALWAN_LITERAL(0.0) && rgb_in->v[0] <= ALWAN_LITERAL(1.0) &&
        rgb_in->v[1] >= ALWAN_LITERAL(0.0) && rgb_in->v[1] <= ALWAN_LITERAL(1.0) &&
        rgb_in->v[2] >= ALWAN_LITERAL(0.0) && rgb_in->v[2] <= ALWAN_LITERAL(1.0)) {
        *rgb_out = *rgb_in;
        return;
    }

    /* Find the neutral point (luminance-preserving gray) */
    alwan_scalar const L = ALWAN_LITERAL(0.2126) * rgb_in->v[0] +
                           ALWAN_LITERAL(0.7152) * rgb_in->v[1] +
                           ALWAN_LITERAL(0.0722) * rgb_in->v[2];

    /* Clamp L to [0,1] */
    alwan_scalar const L_clamped = (L < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                                    (L > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : L;

    /* Binary search for the largest t where t*rgb_in + (1-t)*L_clamped is in [0,1]^3 */
    alwan_scalar t_min = ALWAN_LITERAL(0.0);
    alwan_scalar t_max = ALWAN_LITERAL(1.0);
    alwan_vec3 neutral;
    neutral.v[0] = neutral.v[1] = neutral.v[2] = L_clamped;

    /* Initialize output to neutral (t=0 fallback) */
    *rgb_out = neutral;

    for (int iter = 0; iter < 20; iter++) {  /* 20 iterations gives ~1e-6 precision */
        alwan_scalar const t = (t_min + t_max) * ALWAN_LITERAL(0.5);
        alwan_vec3 test;
        test.v[0] = t * rgb_in->v[0] + (ALWAN_LITERAL(1.0) - t) * neutral.v[0];
        test.v[1] = t * rgb_in->v[1] + (ALWAN_LITERAL(1.0) - t) * neutral.v[1];
        test.v[2] = t * rgb_in->v[2] + (ALWAN_LITERAL(1.0) - t) * neutral.v[2];

        /* Check if in gamut */
        if (test.v[0] >= ALWAN_LITERAL(0.0) && test.v[0] <= ALWAN_LITERAL(1.0) &&
            test.v[1] >= ALWAN_LITERAL(0.0) && test.v[1] <= ALWAN_LITERAL(1.0) &&
            test.v[2] >= ALWAN_LITERAL(0.0) && test.v[2] <= ALWAN_LITERAL(1.0)) {
            /* In gamut, try larger t */
            t_min = t;
            *rgb_out = test;
        } else {
            /* Out of gamut, try smaller t */
            t_max = t;
        }
    }
}

int alwan_gamut_map(alwan_gamut_map_method method,
                    alwan_vec3 const *rgb_in,
                    size_t count,
                    alwan_vec3 *rgb_out) {
    if (!rgb_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        switch (method) {
            case ALWAN_GAMUT_MAP_CLIP:
                gamut_map_clip_single(&rgb_in[i], &rgb_out[i]);
                break;
            case ALWAN_GAMUT_MAP_HUE_PRESERVING:
                gamut_map_hue_preserving_single(&rgb_in[i], &rgb_out[i]);
                break;
            default:
                return ALWAN_E_INVALID;
        }
    }

    return ALWAN_OK;
}

/* Map XYZ to RGB gamut with hue preservation */
int alwan_gamut_map_xyz_to_rgb(alwan_ctx *ctx,
                                alwan_rgb_space_desc const *space,
                                alwan_vec3 const *xyz_in,
                                alwan_vec3 *rgb_out) {
    (void)ctx;  /* Reserved for future use */

    if (!space || !xyz_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Derive XYZ->RGB matrix */
    alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices(space, &rgb_to_xyz, &xyz_to_rgb);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Convert XYZ to RGB (may be out of gamut) */
    alwan_vec3 rgb_raw;
    alwan_mat3_mulv(&xyz_to_rgb, xyz_in, &rgb_raw);

    /* Apply hue-preserving gamut mapping */
    gamut_map_hue_preserving_single(&rgb_raw, rgb_out);

    return ALWAN_OK;
}
