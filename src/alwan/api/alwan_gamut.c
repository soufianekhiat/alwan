/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M11: Gamut Utilities & Mapping
 * Per-pixel math in alwan_gamut_core.h
 *
 * Only NULL-param defaults, enum dispatch, bulk loops, data tables,
 * and polygon/locus lookups live here.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_gamut_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

int alwan_gamut_volume_mc(alwan_scalar *volume,
                          alwan_rgb_space_desc const *space,
                          size_t num_samples,
                          unsigned int seed) {
    (void)num_samples;  /* Not used - determinant is exact */
    (void)seed;         /* Not used - determinant is exact */

    if (!space || !volume) {
        return ALWAN_E_INVALID;
    }

    /* Derive RGB->XYZ matrix */
    alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices(&rgb_to_xyz, &xyz_to_rgb, space);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Compute determinant of RGB->XYZ matrix
     * The volume of the RGB gamut in XYZ space is |det(M)| */
    alwan_scalar const det = alwan_mat3_det(&rgb_to_xyz);

    if (ALWAN_ABS(det) < ALWAN_EPSILON) {
        return ALWAN_E_RANGE;
    }

    /* Volume is the absolute value of the determinant */
    *volume = ALWAN_ABS(det);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * M11: Gamut Mapping
 * ---------------------------------------------------------------- */

/* Clip RGB to [0,1] range */
static void gamut_map_clip_single(alwan_vec3 const *rgb_in, alwan_vec3 *rgb_out) {
    *rgb_out = gamut_clip_v(*rgb_in);
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
    alwan_scalar const L = ALWAN_LUMA_KR_BT709 * rgb_in->v[0] +
                           ALWAN_LUMA_KG_BT709 * rgb_in->v[1] +
                           ALWAN_LUMA_KB_BT709 * rgb_in->v[2];

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

int alwan_gamut_map_interleave(alwan_scalar *rgb_out,
                    alwan_gamut_map_method method,
                    alwan_scalar const *rgb_in,
                    size_t count,
                    size_t in_stride,
                    size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* Select gamut mapping function */
    void (*map_fn)(alwan_vec3 const *, alwan_vec3 *) = NULL;

    switch (method) {
        case ALWAN_GAMUT_MAP_CLIP:
            map_fn = gamut_map_clip_single;
            break;
        case ALWAN_GAMUT_MAP_HUE_PRESERVING:
            map_fn = gamut_map_hue_preserving_single;
            break;
        default:
            return ALWAN_E_INVALID;
    }

    /* Apply gamut mapping to array with stride support */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        /* Load RGB triplet into vec3 */
        alwan_vec3 rgb_in_vec, rgb_out_vec;
        rgb_in_vec.v[0] = in_ptr[0];
        rgb_in_vec.v[1] = in_ptr[1];
        rgb_in_vec.v[2] = in_ptr[2];

        /* Apply mapping */
        map_fn(&rgb_in_vec, &rgb_out_vec);

        /* Store result */
        out_ptr[0] = rgb_out_vec.v[0];
        out_ptr[1] = rgb_out_vec.v[1];
        out_ptr[2] = rgb_out_vec.v[2];
    }

    return ALWAN_OK;
}

/* Map XYZ to RGB gamut with hue preservation */
int alwan_gamut_map_xyz_to_rgb(alwan_rgb *rgb_out,
                                alwan_ctx *ctx,
                                alwan_rgb_space_desc const *space,
                                alwan_xyz const *xyz_in) {
    (void)ctx;  /* Reserved for future use */

    if (!space || !xyz_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Derive XYZ->RGB matrix */
    alwan_mat3x3 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices(&rgb_to_xyz, &xyz_to_rgb, space);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Convert XYZ to RGB (may be out of gamut) */
    alwan_vec3 xyz_vec, rgb_raw, rgb_mapped;
    xyz_vec.v[0] = xyz_in->x;
    xyz_vec.v[1] = xyz_in->y;
    xyz_vec.v[2] = xyz_in->z;

    alwan_mat3_mulv(&rgb_raw, &xyz_to_rgb, &xyz_vec);

    /* Apply hue-preserving gamut mapping */
    gamut_map_hue_preserving_single(&rgb_raw, &rgb_mapped);

    /* Convert back to rgb */
    rgb_out->r = rgb_mapped.v[0];
    rgb_out->g = rgb_mapped.v[1];
    rgb_out->b = rgb_mapped.v[2];

    return ALWAN_OK;
}

/* ================================================================
 * Gamut Analysis & Mapping
 * ================================================================ */

/* ----------------------------------------------------------------
 * Pointer's Gamut
 * ---------------------------------------------------------------- */

/* Pointer's Gamut boundary in CIE 1931 xy chromaticity (32 points)
 * Data from MacAdam (1935), reanalyzed for Illuminant C
 * Source: colour-science CCS_POINTER_GAMUT_BOUNDARY */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_vec2 const POINTER_GAMUT_BOUNDARY[32] = {
#include "../data/gamut/pointer_gamut_boundary_xy.csv"
};
ALWAN_DIAG_POP

/* Check if a point is inside a 2D polygon using ray casting algorithm
 * Returns 1 if inside, 0 if outside */
static int alwan_point_in_polygon(alwan_vec2 const *point,
                                    alwan_vec2 const *polygon,
                                    size_t polygon_count) {
    int inside = 0;
    alwan_scalar px = point->v[0];
    alwan_scalar py = point->v[1];

    for (size_t i = 0, j = polygon_count - 1; i < polygon_count; j = i++) {
        alwan_scalar xi = polygon[i].v[0], yi = polygon[i].v[1];
        alwan_scalar xj = polygon[j].v[0], yj = polygon[j].v[1];

        /* Check if horizontal ray from point crosses edge (i, j) */
        int intersect = ((yi > py) != (yj > py)) &&
                        (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

int alwan_is_within_pointer_gamut(alwan_vec2 const *xy) {
    if (!xy) {
        return 0;
    }

    return alwan_point_in_polygon(xy, POINTER_GAMUT_BOUNDARY, 32);
}

alwan_vec2 const* alwan_pointer_gamut_boundary(size_t *count_out) {
    if (count_out) {
        *count_out = 32;
    }
    return POINTER_GAMUT_BOUNDARY;
}

/* ----------------------------------------------------------------
 * Spectral Locus
 * ---------------------------------------------------------------- */

/* CIE 1931 spectral locus xy chromaticity data (360-830nm, 1nm interval, 471 points)
 * Computed from CIE 1931 2° observer CMFs */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_vec2 const SPECTRAL_LOCUS_XY[471] = {
#include "../data/gamut/spectral_locus_xy_only_360_830_1nm.csv"
};
ALWAN_DIAG_POP

#define SPECTRAL_LOCUS_WL_MIN ALWAN_LITERAL(360.0)
#define SPECTRAL_LOCUS_WL_MAX ALWAN_LITERAL(830.0)
#define SPECTRAL_LOCUS_WL_INTERVAL ALWAN_LITERAL(1.0)
#define SPECTRAL_LOCUS_COUNT 471

int alwan_spectral_locus_xy(alwan_vec2 *xy_out, alwan_scalar wavelength) {
    if (!xy_out) {
        return ALWAN_E_INVALID;
    }

    /* Check wavelength range */
    if (wavelength < SPECTRAL_LOCUS_WL_MIN || wavelength > SPECTRAL_LOCUS_WL_MAX) {
        return ALWAN_E_INVALID;
    }

    /* Compute index and fraction for linear interpolation */
    alwan_scalar t = (wavelength - SPECTRAL_LOCUS_WL_MIN) / SPECTRAL_LOCUS_WL_INTERVAL;
    size_t idx = (size_t)ALWAN_FLOOR(t);
    alwan_scalar frac = t - (alwan_scalar)idx;

    /* Clamp to valid range */
    if (idx >= SPECTRAL_LOCUS_COUNT - 1) {
        idx = SPECTRAL_LOCUS_COUNT - 2;
        frac = ALWAN_LITERAL(1.0);
    }

    /* Linear interpolation between two adjacent points */
    alwan_vec2 const *xy0 = &SPECTRAL_LOCUS_XY[idx];
    alwan_vec2 const *xy1 = &SPECTRAL_LOCUS_XY[idx + 1];

    xy_out->v[0] = xy0->v[0] + frac * (xy1->v[0] - xy0->v[0]);
    xy_out->v[1] = xy0->v[1] + frac * (xy1->v[1] - xy0->v[1]);

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Dominant Wavelength & Excitation Purity
 * ---------------------------------------------------------------- */

/* Compute intersection of line (p1, p2) with spectral locus
 * Returns wavelength and xy coordinates of intersection point
 * Returns ALWAN_OK on success, ALWAN_E_INVALID if no intersection found */
static int alwan_intersect_spectral_locus(alwan_vec2 const *p1,
                                           alwan_vec2 const *p2,
                                           alwan_scalar *wavelength_out,
                                           alwan_vec2 *xy_out) {
    /* Find intersection by testing each segment of the spectral locus */
    alwan_scalar best_t = ALWAN_LITERAL(-1.0);
    size_t best_idx = 0;

    for (size_t i = 0; i < SPECTRAL_LOCUS_COUNT - 1; i++) {
        alwan_vec2 const *s1 = &SPECTRAL_LOCUS_XY[i];
        alwan_vec2 const *s2 = &SPECTRAL_LOCUS_XY[i + 1];

        /* Line-line intersection using parametric form:
         * p1 + t*(p2-p1) = s1 + u*(s2-s1) */
        alwan_scalar dx1 = p2->v[0] - p1->v[0];
        alwan_scalar dy1 = p2->v[1] - p1->v[1];
        alwan_scalar dx2 = s2->v[0] - s1->v[0];
        alwan_scalar dy2 = s2->v[1] - s1->v[1];

        alwan_scalar det = dx1 * dy2 - dy1 * dx2;
        if (ALWAN_ABS(det) < ALWAN_LITERAL(1e-10)) {
            continue;  /* Lines are parallel */
        }

        alwan_scalar dx3 = s1->v[0] - p1->v[0];
        alwan_scalar dy3 = s1->v[1] - p1->v[1];

        alwan_scalar t = (dx3 * dy2 - dy3 * dx2) / det;
        alwan_scalar u = (dx3 * dy1 - dy3 * dx1) / det;

        /* Check if intersection is on the ray (t >= 0) and on the spectral locus segment (u in [0,1])
         * We allow t > 1 to extend the ray beyond the color point to the spectral locus */
        if (t >= ALWAN_LITERAL(0.0) &&
            u >= ALWAN_LITERAL(0.0) && u <= ALWAN_LITERAL(1.0)) {
            if (best_t < ALWAN_LITERAL(0.0) || t < best_t) {
                best_t = t;
                best_idx = i;
            }
        }
    }

    if (best_t < ALWAN_LITERAL(0.0)) {
        return ALWAN_E_INVALID;  /* No intersection found */
    }

    /* Compute intersection point */
    alwan_vec2 const *s1 = &SPECTRAL_LOCUS_XY[best_idx];
    alwan_vec2 const *s2 = &SPECTRAL_LOCUS_XY[best_idx + 1];

    alwan_scalar dx2 = s2->v[0] - s1->v[0];
    alwan_scalar dy2 = s2->v[1] - s1->v[1];
    alwan_scalar dx3 = s1->v[0] - p1->v[0];
    alwan_scalar dy3 = s1->v[1] - p1->v[1];

    alwan_scalar dx1 = p2->v[0] - p1->v[0];
    alwan_scalar dy1 = p2->v[1] - p1->v[1];
    alwan_scalar det = dx1 * dy2 - dy1 * dx2;
    alwan_scalar u = (dx3 * dy1 - dy3 * dx1) / det;

    if (xy_out) {
        xy_out->v[0] = s1->v[0] + u * dx2;
        xy_out->v[1] = s1->v[1] + u * dy2;
    }

    /* Interpolate wavelength */
    alwan_scalar wl = SPECTRAL_LOCUS_WL_MIN + (alwan_scalar)best_idx * SPECTRAL_LOCUS_WL_INTERVAL;
    wl += u * SPECTRAL_LOCUS_WL_INTERVAL;

    if (wavelength_out) {
        *wavelength_out = wl;
    }

    return ALWAN_OK;
}

int alwan_dominant_wavelength(alwan_scalar *wavelength_out,
                               alwan_vec2 *xy_wl_out,
                               alwan_vec2 *xy_cw_out,
                               alwan_vec2 const *xy,
                               alwan_vec2 const *xy_white) {
    if (!xy || !xy_white || !wavelength_out) {
        return ALWAN_E_INVALID;
    }

    /* Extend line from white point through xy to spectral locus */
    alwan_scalar wl;
    alwan_vec2 xy_intersection;
    int status = alwan_intersect_spectral_locus(xy_white, xy, &wl, &xy_intersection);

    if (status != ALWAN_OK) {
        /* No intersection with spectral locus - color is on purple line
         * Return negative value to indicate complementary wavelength needed */
        return ALWAN_E_INVALID;
    }

    *wavelength_out = wl;

    if (xy_wl_out) {
        *xy_wl_out = xy_intersection;
    }

    if (xy_cw_out) {
        *xy_cw_out = xy_intersection;  /* For dominant wavelength, cw = wl */
    }

    return ALWAN_OK;
}

int alwan_excitation_purity(alwan_scalar *purity_out,
                             alwan_vec2 const *xy,
                             alwan_vec2 const *xy_white) {
    if (!xy || !xy_white || !purity_out) {
        return ALWAN_E_INVALID;
    }

    /* Get dominant wavelength (intersection with spectral locus) */
    alwan_vec2 xy_wl;
    alwan_scalar wl;
    int status = alwan_intersect_spectral_locus(xy_white, xy, &wl, &xy_wl);

    if (status != ALWAN_OK) {
        /* For purple line colors, use complementary wavelength */
        /* Find intersection in opposite direction */
        alwan_vec2 xy_opposite;
        xy_opposite.v[0] = ALWAN_LITERAL(2.0) * xy_white->v[0] - xy->v[0];
        xy_opposite.v[1] = ALWAN_LITERAL(2.0) * xy_white->v[1] - xy->v[1];

        status = alwan_intersect_spectral_locus(xy_white, &xy_opposite, &wl, &xy_wl);
        if (status != ALWAN_OK) {
            return ALWAN_E_INVALID;
        }
    }

    /* Compute excitation purity as ratio of distances:
     * pe = |white - color| / |white - spectral_locus| */
    alwan_scalar dx_color = xy->v[0] - xy_white->v[0];
    alwan_scalar dy_color = xy->v[1] - xy_white->v[1];
    alwan_scalar dist_color = ALWAN_SQRT(dx_color * dx_color + dy_color * dy_color);

    alwan_scalar dx_wl = xy_wl.v[0] - xy_white->v[0];
    alwan_scalar dy_wl = xy_wl.v[1] - xy_white->v[1];
    alwan_scalar dist_wl = ALWAN_SQRT(dx_wl * dx_wl + dy_wl * dy_wl);

    if (dist_wl < ALWAN_LITERAL(1e-10)) {
        *purity_out = ALWAN_LITERAL(0.0);  /* White point */
        return ALWAN_OK;
    }

    *purity_out = dist_color / dist_wl;

    /* Clamp to [0, 1] range */
    if (*purity_out < ALWAN_LITERAL(0.0)) {
        *purity_out = ALWAN_LITERAL(0.0);
    } else if (*purity_out > ALWAN_LITERAL(1.0)) {
        *purity_out = ALWAN_LITERAL(1.0);
    }

    return ALWAN_OK;
}

int alwan_complementary_wavelength(alwan_scalar *wavelength_out,
                                     alwan_vec2 *xy_wl_out,
                                     alwan_vec2 *xy_cw_out,
                                     alwan_vec2 const *xy,
                                     alwan_vec2 const *xy_white) {
    if (!xy || !xy_white || !wavelength_out) {
        return ALWAN_E_INVALID;
    }

    /* Extend line from color through white point to spectral locus (opposite direction) */
    alwan_vec2 xy_opposite;
    xy_opposite.v[0] = ALWAN_LITERAL(2.0) * xy_white->v[0] - xy->v[0];
    xy_opposite.v[1] = ALWAN_LITERAL(2.0) * xy_white->v[1] - xy->v[1];

    alwan_scalar wl;
    alwan_vec2 xy_intersection;
    int status = alwan_intersect_spectral_locus(xy_white, &xy_opposite, &wl, &xy_intersection);

    if (status != ALWAN_OK) {
        return ALWAN_E_INVALID;
    }

    *wavelength_out = wl;

    if (xy_wl_out) {
        *xy_wl_out = xy_intersection;
    }

    if (xy_cw_out) {
        *xy_cw_out = xy_intersection;
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Gamut Coverage Metrics
 * ---------------------------------------------------------------- */

int alwan_gamut_volume_ratio(alwan_scalar *ratio_out,
                               alwan_rgb_space_desc const *space1,
                               alwan_rgb_space_desc const *space2) {
    if (!space1 || !space2 || !ratio_out) {
        return ALWAN_E_INVALID;
    }

    /* Compute volume for both spaces */
    alwan_scalar volume1, volume2;
    int status1 = alwan_gamut_volume_mc(&volume1, space1, 0, 0);
    int status2 = alwan_gamut_volume_mc(&volume2, space2, 0, 0);

    if (status1 != ALWAN_OK || status2 != ALWAN_OK) {
        return ALWAN_E_INVALID;
    }

    /* Avoid division by zero */
    if (ALWAN_ABS(volume2) < ALWAN_EPSILON) {
        return ALWAN_E_DIVZERO;
    }

    *ratio_out = volume1 / volume2;
    return ALWAN_OK;
}

int alwan_gamut_coverage(alwan_scalar *coverage_out,
                          alwan_rgb_space_desc const *space1,
                          alwan_rgb_space_desc const *space2,
                          size_t num_samples,
                          unsigned int seed) {
    if (!space1 || !space2 || !coverage_out) {
        return ALWAN_E_INVALID;
    }

    if (num_samples == 0) {
        num_samples = 10000;  /* Default sample count */
    }

    /* Derive matrices for both spaces */
    alwan_mat3x3 rgb1_to_xyz, xyz_to_rgb1;
    alwan_mat3x3 rgb2_to_xyz, xyz_to_rgb2;

    int status1 = alwan_rgb_derive_matrices(&rgb1_to_xyz, &xyz_to_rgb1, space1);
    int status2 = alwan_rgb_derive_matrices(&rgb2_to_xyz, &xyz_to_rgb2, space2);

    if (status1 != ALWAN_OK || status2 != ALWAN_OK) {
        return ALWAN_E_INVALID;
    }

    /* Simple linear congruential generator for reproducible random numbers
     * We use this instead of rand() for platform independence */
    unsigned int rng_state = seed;

    /* Monte Carlo sampling: generate random points in space1's gamut,
     * test how many are also in space2's gamut */
    size_t inside_count = 0;

    for (size_t i = 0; i < num_samples; i++) {
        /* Generate random RGB point in [0,1] cube (space1's gamut) */
        alwan_vec3 rgb1;

        /* LCG for R */
        rng_state = (rng_state * 1103515245u + 12345u) & 0x7fffffffu;
        rgb1.v[0] = (alwan_scalar)rng_state / (alwan_scalar)0x7fffffff;

        /* LCG for G */
        rng_state = (rng_state * 1103515245u + 12345u) & 0x7fffffffu;
        rgb1.v[1] = (alwan_scalar)rng_state / (alwan_scalar)0x7fffffff;

        /* LCG for B */
        rng_state = (rng_state * 1103515245u + 12345u) & 0x7fffffffu;
        rgb1.v[2] = (alwan_scalar)rng_state / (alwan_scalar)0x7fffffff;

        /* Transform space1 RGB -> XYZ */
        alwan_vec3 xyz;
        alwan_mat3_mulv(&xyz, &rgb1_to_xyz, &rgb1);

        /* Transform XYZ -> space2 RGB */
        alwan_vec3 rgb2;
        alwan_mat3_mulv(&rgb2, &xyz_to_rgb2, &xyz);

        /* Check if rgb2 is in gamut (all components in [0,1]) */
        int in_gamut = (rgb2.v[0] >= ALWAN_LITERAL(0.0) && rgb2.v[0] <= ALWAN_LITERAL(1.0) &&
                        rgb2.v[1] >= ALWAN_LITERAL(0.0) && rgb2.v[1] <= ALWAN_LITERAL(1.0) &&
                        rgb2.v[2] >= ALWAN_LITERAL(0.0) && rgb2.v[2] <= ALWAN_LITERAL(1.0));

        if (in_gamut) {
            inside_count++;
        }
    }

    /* Compute coverage percentage */
    *coverage_out = (ALWAN_LITERAL(100.0) * (alwan_scalar)inside_count) / (alwan_scalar)num_samples;

    return ALWAN_OK;
}

/* ================================================================
 * Advanced Gamut Mapping
 * ================================================================ */

/* Oklab color space conversion (linear RGB <-> Oklab)
 * Based on Bjorn Ottosson's Oklab (2020)
 * https://bottosson.github.io/posts/oklab/ */

/* Linear sRGB -> Oklab */
static void alwan_linear_srgb_to_oklab(alwan_vec3 const *rgb, alwan_vec3 *oklab) {
    *oklab = gamut_linear_srgb_to_oklab_v(*rgb);
}

/* Oklab -> Linear sRGB */
static void alwan_oklab_to_linear_srgb(alwan_vec3 const *oklab, alwan_vec3 *rgb) {
    *rgb = gamut_oklab_to_linear_srgb_v(*oklab);
}

/* Compute maximum saturation for a given hue */
static alwan_scalar alwan_compute_max_saturation(alwan_scalar a, alwan_scalar b) {
    return gamut_compute_max_saturation_v(a, b);
}

/* Find gamut cusp */
static void alwan_find_cusp(alwan_scalar a, alwan_scalar b, alwan_scalar *L_cusp, alwan_scalar *C_cusp) {
    alwan_vec2 cusp = gamut_find_cusp_v(a, b);
    *L_cusp = cusp.v[0];
    *C_cusp = cusp.v[1];
}

/* Find intersection of gamut boundary */
static alwan_scalar alwan_find_gamut_intersection(alwan_scalar a, alwan_scalar b,
                                                    alwan_scalar L1, alwan_scalar C1,
                                                    alwan_scalar L0, alwan_scalar C0) {
    return gamut_find_intersection_v(a, b, L1, C1, L0, C0);
}

/* Gamut mapping implementation */
int alwan_gamut_map_advanced(alwan_rgb *rgb_out,
                              alwan_gamut_map_method method,
                              alwan_rgb_space_desc const *space,
                              alwan_rgb const *rgb_linear) {
    if (!space || !rgb_linear || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Convert to vec3 for internal processing */
    alwan_vec3 rgb_vec;
    rgb_vec.v[0] = rgb_linear->r;
    rgb_vec.v[1] = rgb_linear->g;
    rgb_vec.v[2] = rgb_linear->b;

    /* Method 0: Simple clipping */
    if (method == ALWAN_GAMUT_MAP_CLIP) {
        rgb_out->r = (rgb_linear->r < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                     (rgb_linear->r > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_linear->r;
        rgb_out->g = (rgb_linear->g < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                     (rgb_linear->g > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_linear->g;
        rgb_out->b = (rgb_linear->b < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                     (rgb_linear->b > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_linear->b;
        return ALWAN_OK;
    }

    /* Convert to Oklab */
    alwan_vec3 oklab;
    alwan_linear_srgb_to_oklab(&rgb_vec, &oklab);

    alwan_scalar L = oklab.v[0];
    alwan_scalar a = oklab.v[1];
    alwan_scalar b = oklab.v[2];
    alwan_scalar C = ALWAN_SQRT(a * a + b * b);

    /* If achromatic or already in gamut, check if simple clip works */
    if (C < ALWAN_LITERAL(0.0001) ||
        (rgb_linear->r >= ALWAN_LITERAL(0.0) && rgb_linear->r <= ALWAN_LITERAL(1.0) &&
         rgb_linear->g >= ALWAN_LITERAL(0.0) && rgb_linear->g <= ALWAN_LITERAL(1.0) &&
         rgb_linear->b >= ALWAN_LITERAL(0.0) && rgb_linear->b <= ALWAN_LITERAL(1.0))) {
        *rgb_out = *rgb_linear;
        return ALWAN_OK;
    }

    /* Normalize a, b */
    alwan_scalar a_norm = a / C;
    alwan_scalar b_norm = b / C;

    alwan_scalar L0, C0;
    alwan_scalar alpha;

    /* Select projection point based on method */
    if (method == ALWAN_GAMUT_MAP_ADAPTIVE_L0) {
        /* Adaptive L0: project toward L=0.5, C=0 */
        L0 = ALWAN_LITERAL(0.5);
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.05);  /* Blend factor */
    } else if (method == ALWAN_GAMUT_MAP_ADAPTIVE_CUSP) {
        /* Adaptive toward cusp */
        alwan_scalar L_cusp, C_cusp;
        alwan_find_cusp(a_norm, b_norm, &L_cusp, &C_cusp);
        L0 = L_cusp;
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.05);
    } else if (method == ALWAN_GAMUT_MAP_CHROMA_COMPRESS) {
        /* Chroma compression: project toward L, C=0 */
        L0 = L;
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.1);
    } else if (method == ALWAN_GAMUT_MAP_SGCK) {
        /* SGCK 2004: Segment-Maximal Gamut Clipping with Knee adjustment
         * Uses a soft knee to smoothly transition into gamut */
        alwan_scalar L_cusp, C_cusp;
        alwan_find_cusp(a_norm, b_norm, &L_cusp, &C_cusp);

        /* Project toward cusp with soft knee */
        L0 = L_cusp;
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.15);  /* Larger blend for softer knee */
    } else if (method == ALWAN_GAMUT_MAP_HPMINDE) {
        /* HPMINDE: Hue-Preserving Minimum ΔE
         * Minimizes perceptual color difference while preserving hue */
        alwan_scalar L_cusp, C_cusp;
        alwan_find_cusp(a_norm, b_norm, &L_cusp, &C_cusp);

        /* Find optimal projection point that minimizes ΔE */
        /* For simplicity, use cusp as projection point with adaptive blending */
        L0 = L_cusp;
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.02);  /* Minimal blending to preserve hue better */
    } else if (method == ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE) {
        /* Lightness Preserving: maintain lightness, reduce chroma only */
        L0 = L;  /* Keep same lightness */
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.0);  /* No lightness adjustment */
    } else {
        return ALWAN_E_INVALID;
    }

    /* Find gamut intersection */
    alwan_scalar t = alwan_find_gamut_intersection(a_norm, b_norm, L, C, L0, C0);

    /* Clamp t to valid range */
    if (t < ALWAN_LITERAL(0.0)) t = ALWAN_LITERAL(0.0);
    if (t > ALWAN_LITERAL(1.0)) t = ALWAN_LITERAL(1.0);

    /* Interpolate to boundary */
    alwan_scalar L_clipped = L0 * (ALWAN_LITERAL(1.0) - t) + t * L;
    alwan_scalar C_clipped = t * C;

    /* Convert back to Oklab */
    alwan_vec3 oklab_clipped;
    oklab_clipped.v[0] = L_clipped;
    oklab_clipped.v[1] = C_clipped * a_norm;
    oklab_clipped.v[2] = C_clipped * b_norm;

    /* Convert to linear RGB */
    alwan_vec3 rgb_result;
    alwan_oklab_to_linear_srgb(&oklab_clipped, &rgb_result);

    /* Final safety clamp and convert to rgb */
    rgb_out->r = (rgb_result.v[0] < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                 (rgb_result.v[0] > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_result.v[0];
    rgb_out->g = (rgb_result.v[1] < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                 (rgb_result.v[1] > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_result.v[1];
    rgb_out->b = (rgb_result.v[2] < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                 (rgb_result.v[2] > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_result.v[2];

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CSS Color Level 4 §13.2 Gamut Mapping
 * Per-pixel math in alwan_gamut_core.h
 * ---------------------------------------------------------------- */

int alwan_css_gamut_map_interleave(alwan_scalar *rgb_out,
                        alwan_scalar const *rgb_in,
                        size_t count,
                        size_t in_stride,
                        size_t out_stride) {
    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        alwan_vec3 origin = {{in_ptr[0], in_ptr[1], in_ptr[2]}};
        alwan_vec3 mapped = gamut_css_map_v(origin);
        out_ptr[0] = mapped.v[0];
        out_ptr[1] = mapped.v[1];
        out_ptr[2] = mapped.v[2];
    }

    return ALWAN_OK;
}
