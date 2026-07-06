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
#include "../map/alwan_map_internal.h"
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
static alwan_f64 alwan_rng_uniform(alwan_rng *rng) {
    /* Simple LCG: Numerical Recipes parameters */
    rng->state = rng->state * 1664525u + 1013904223u;
    return (alwan_f64)rng->state / (alwan_f64)0xFFFFFFFFu;
}

/* ----------------------------------------------------------------
 * M11: Gamut Volume Estimation (Monte Carlo)
 * ---------------------------------------------------------------- */

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
int alwan_gamut_volume_f64(alwan_f64 *volume,
                          alwan_rgb_space_desc_f64 const *space) {
    if (!space || !volume) {
        return ALWAN_E_INVALID;
    }

    /* Derive RGB->XYZ matrix */
    alwan_mat3x3_f64 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices_f64(&rgb_to_xyz, &xyz_to_rgb, space);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Compute determinant of RGB->XYZ matrix
     * The volume of the RGB gamut in XYZ space is |det(M)| */
    alwan_f64 const det = alwan_mat3_det_f64(&rgb_to_xyz);

    if (ALWAN_ABS(det) < ALWAN_EPSILON) {
        return ALWAN_E_RANGE;
    }

    /* Volume is the absolute value of the determinant */
    *volume = ALWAN_ABS(det);

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

/* ----------------------------------------------------------------
 * M11: Gamut Mapping
 * ---------------------------------------------------------------- */

/* Clip RGB to [0,1] range */
static void gamut_map_clip_single(alwan_vec3_f64 const *rgb_in, alwan_vec3_f64 *rgb_out) {
    *rgb_out = gamut_clip_f64_v(*rgb_in);
}

/* Hue-preserving gamut mapping: scale towards neutral until in gamut */
static void gamut_map_hue_preserving_single(alwan_vec3_f64 const *rgb_in, alwan_vec3_f64 *rgb_out) {
    /* If already in gamut, return as-is */
    if (rgb_in->v[0] >= ALWAN_LITERAL(0.0) && rgb_in->v[0] <= ALWAN_LITERAL(1.0) &&
        rgb_in->v[1] >= ALWAN_LITERAL(0.0) && rgb_in->v[1] <= ALWAN_LITERAL(1.0) &&
        rgb_in->v[2] >= ALWAN_LITERAL(0.0) && rgb_in->v[2] <= ALWAN_LITERAL(1.0)) {
        *rgb_out = *rgb_in;
        return;
    }

    /* Find the neutral point (luminance-preserving gray) */
    alwan_f64 const L = ALWAN_LUMA_KR_BT709 * rgb_in->v[0] +
                           ALWAN_LUMA_KG_BT709 * rgb_in->v[1] +
                           ALWAN_LUMA_KB_BT709 * rgb_in->v[2];

    /* Clamp L to [0,1] */
    alwan_f64 const L_clamped = (L < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                                    (L > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : L;

    /* Binary search for the largest t where t*rgb_in + (1-t)*L_clamped is in [0,1]^3 */
    alwan_f64 t_min = ALWAN_LITERAL(0.0);
    alwan_f64 t_max = ALWAN_LITERAL(1.0);
    alwan_vec3_f64 neutral;
    neutral.v[0] = neutral.v[1] = neutral.v[2] = L_clamped;

    /* Initialize output to neutral (t=0 fallback) */
    *rgb_out = neutral;

    for (int iter = 0; iter < 20; iter++) {  /* 20 iterations gives ~1e-6 precision */
        alwan_f64 const t = (t_min + t_max) * ALWAN_LITERAL(0.5);
        alwan_vec3_f64 test;
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

int alwan_gamut_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_gamut_map_method method) {
    if (!rgb_in || !rgb_out || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* SIMD fast path for simple clip */
    if (method == ALWAN_GAMUT_MAP_CLIP) {
        size_t processed = 0;
        while (processed < count) {
            size_t tile = count - processed;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
            alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
            alwan__gamut_clip_kernel(c0, c1, c2, tile);
            alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
            processed += tile;
        }
        return ALWAN_OK;
    }

    /* Select gamut mapping function */
    void (*map_fn)(alwan_vec3_f64 const *, alwan_vec3_f64 *) = NULL;

    switch (method) {
        case ALWAN_GAMUT_MAP_HUE_PRESERVING:
            map_fn = gamut_map_hue_preserving_single;
            break;
        default:
            return ALWAN_E_INVALID;
    }

    /* Apply gamut mapping to array with stride support */
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *in_ptr = (alwan_f64 const *)((char const *)rgb_in + i * in_stride);
        alwan_f64 *out_ptr = (alwan_f64 *)((char *)rgb_out + i * out_stride);

        /* Load RGB triplet into vec3 */
        alwan_vec3_f64 rgb_in_vec, rgb_out_vec;
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
#if ALWAN_WITH_F64
int alwan_gamut_map_xyz_to_rgb_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_space_desc_f64 const *space, alwan_xyz_f64 const *xyz_in, alwan_ctx *ctx) {
    (void)ctx;  /* Reserved for future use */

    if (!space || !xyz_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Derive XYZ->RGB matrix */
    alwan_mat3x3_f64 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices_f64(&rgb_to_xyz, &xyz_to_rgb, space);
    if (status != ALWAN_OK) {
        return status;
    }

    /* Convert XYZ to RGB (may be out of gamut) */
    alwan_vec3_f64 xyz_vec, rgb_raw, rgb_mapped;
    xyz_vec.v[0] = xyz_in->x;
    xyz_vec.v[1] = xyz_in->y;
    xyz_vec.v[2] = xyz_in->z;

    alwan_mat3_mulv_f64(&rgb_raw, &xyz_to_rgb, &xyz_vec);

    /* Apply hue-preserving gamut mapping */
    gamut_map_hue_preserving_single(&rgb_raw, &rgb_mapped);

    /* Convert back to rgb */
    rgb_out->r = rgb_mapped.v[0];
    rgb_out->g = rgb_mapped.v[1];
    rgb_out->b = rgb_mapped.v[2];

    return ALWAN_OK;
}
#endif

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
static alwan_vec2_f64 const POINTER_GAMUT_BOUNDARY[32] = {
#include "../data/gamut/pointer_gamut_boundary_xy.csv"
};
ALWAN_DIAG_POP

/* Check if a point is inside a 2D polygon using ray casting algorithm
 * Returns 1 if inside, 0 if outside */
static int alwan_point_in_polygon(alwan_vec2_f64 const *point,
                                    alwan_vec2_f64 const *polygon,
                                    size_t polygon_count) {
    int inside = 0;
    alwan_f64 px = point->v[0];
    alwan_f64 py = point->v[1];

    for (size_t i = 0, j = polygon_count - 1; i < polygon_count; j = i++) {
        alwan_f64 xi = polygon[i].v[0], yi = polygon[i].v[1];
        alwan_f64 xj = polygon[j].v[0], yj = polygon[j].v[1];

        /* Check if horizontal ray from point crosses edge (i, j) */
        int intersect = ((yi > py) != (yj > py)) &&
                        (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) {
            inside = !inside;
        }
    }

    return inside;
}

int alwan_is_within_pointer_gamut_f64(alwan_vec2_f64 const *xy) {
    if (!xy) {
        return 0;
    }

    return alwan_point_in_polygon(xy, POINTER_GAMUT_BOUNDARY, 32);
}

alwan_vec2_f64 const* alwan_pointer_gamut_boundary(size_t *count_out) {
    if (count_out) {
        *count_out = 32;
    }
    return POINTER_GAMUT_BOUNDARY;
}

/* ----------------------------------------------------------------
 * Spectral Locus
 * ---------------------------------------------------------------- */

/* CIE 1931 spectral locus xy chromaticity data (360-830nm, 1nm interval, 471 points)
 * Computed from CIE 1931 2 deg observer CMFs */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_vec2_f64 const SPECTRAL_LOCUS_XY[471] = {
#include "../data/gamut/spectral_locus_xy_only_360_830_1nm.csv"
};
ALWAN_DIAG_POP

#define SPECTRAL_LOCUS_WL_MIN ALWAN_LITERAL(360.0)
#define SPECTRAL_LOCUS_WL_MAX ALWAN_LITERAL(830.0)
#define SPECTRAL_LOCUS_WL_INTERVAL ALWAN_LITERAL(1.0)
#define SPECTRAL_LOCUS_COUNT 471

int alwan_spectral_locus_xy_f64(alwan_vec2_f64 *xy_out, alwan_f64 wavelength) {
    if (!xy_out) {
        return ALWAN_E_INVALID;
    }

    /* Check wavelength range */
    if (wavelength < SPECTRAL_LOCUS_WL_MIN || wavelength > SPECTRAL_LOCUS_WL_MAX) {
        return ALWAN_E_INVALID;
    }

    /* Compute index and fraction for linear interpolation */
    alwan_f64 t = (wavelength - SPECTRAL_LOCUS_WL_MIN) / SPECTRAL_LOCUS_WL_INTERVAL;
    size_t idx = (size_t)ALWAN_FLOOR(t);
    alwan_f64 frac = t - (alwan_f64)idx;

    /* Clamp to valid range */
    if (idx >= SPECTRAL_LOCUS_COUNT - 1) {
        idx = SPECTRAL_LOCUS_COUNT - 2;
        frac = ALWAN_LITERAL(1.0);
    }

    /* Linear interpolation between two adjacent points */
    alwan_vec2_f64 const *xy0 = &SPECTRAL_LOCUS_XY[idx];
    alwan_vec2_f64 const *xy1 = &SPECTRAL_LOCUS_XY[idx + 1];

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
static int alwan_intersect_spectral_locus(alwan_vec2_f64 const *p1,
                                           alwan_vec2_f64 const *p2,
                                           alwan_f64 *wavelength_out,
                                           alwan_vec2_f64 *xy_out) {
    /* Find intersection by testing each segment of the spectral locus */
    alwan_f64 best_t = ALWAN_LITERAL(-1.0);
    size_t best_idx = 0;

    for (size_t i = 0; i < SPECTRAL_LOCUS_COUNT - 1; i++) {
        alwan_vec2_f64 const *s1 = &SPECTRAL_LOCUS_XY[i];
        alwan_vec2_f64 const *s2 = &SPECTRAL_LOCUS_XY[i + 1];

        /* Line-line intersection using parametric form:
         * p1 + t*(p2-p1) = s1 + u*(s2-s1) */
        alwan_f64 dx1 = p2->v[0] - p1->v[0];
        alwan_f64 dy1 = p2->v[1] - p1->v[1];
        alwan_f64 dx2 = s2->v[0] - s1->v[0];
        alwan_f64 dy2 = s2->v[1] - s1->v[1];

        alwan_f64 det = dx1 * dy2 - dy1 * dx2;
        if (ALWAN_ABS(det) < ALWAN_LITERAL(1e-10)) {
            continue;  /* Lines are parallel */
        }

        alwan_f64 dx3 = s1->v[0] - p1->v[0];
        alwan_f64 dy3 = s1->v[1] - p1->v[1];

        alwan_f64 t = (dx3 * dy2 - dy3 * dx2) / det;
        alwan_f64 u = (dx3 * dy1 - dy3 * dx1) / det;

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
    alwan_vec2_f64 const *s1 = &SPECTRAL_LOCUS_XY[best_idx];
    alwan_vec2_f64 const *s2 = &SPECTRAL_LOCUS_XY[best_idx + 1];

    alwan_f64 dx2 = s2->v[0] - s1->v[0];
    alwan_f64 dy2 = s2->v[1] - s1->v[1];
    alwan_f64 dx3 = s1->v[0] - p1->v[0];
    alwan_f64 dy3 = s1->v[1] - p1->v[1];

    alwan_f64 dx1 = p2->v[0] - p1->v[0];
    alwan_f64 dy1 = p2->v[1] - p1->v[1];
    alwan_f64 det = dx1 * dy2 - dy1 * dx2;
    alwan_f64 u = (dx3 * dy1 - dy3 * dx1) / det;

    if (xy_out) {
        xy_out->v[0] = s1->v[0] + u * dx2;
        xy_out->v[1] = s1->v[1] + u * dy2;
    }

    /* Interpolate wavelength */
    alwan_f64 wl = SPECTRAL_LOCUS_WL_MIN + (alwan_f64)best_idx * SPECTRAL_LOCUS_WL_INTERVAL;
    wl += u * SPECTRAL_LOCUS_WL_INTERVAL;

    if (wavelength_out) {
        *wavelength_out = wl;
    }

    return ALWAN_OK;
}

int alwan_dominant_wavelength_f64(alwan_f64 *wavelength_out,
                               alwan_vec2_f64 *xy_wl_out,
                               alwan_vec2_f64 *xy_cw_out,
                               alwan_vec2_f64 const *xy,
                               alwan_vec2_f64 const *xy_white) {
    if (!xy || !xy_white || !wavelength_out) {
        return ALWAN_E_INVALID;
    }

    /* Extend line from white point through xy to spectral locus */
    alwan_f64 wl;
    alwan_vec2_f64 xy_intersection;
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

int alwan_excitation_purity_f64(alwan_f64 *purity_out,
                             alwan_vec2_f64 const *xy,
                             alwan_vec2_f64 const *xy_white) {
    if (!xy || !xy_white || !purity_out) {
        return ALWAN_E_INVALID;
    }

    /* Get dominant wavelength (intersection with spectral locus) */
    alwan_vec2_f64 xy_wl;
    alwan_f64 wl;
    int status = alwan_intersect_spectral_locus(xy_white, xy, &wl, &xy_wl);

    if (status != ALWAN_OK) {
        /* For purple line colors, use complementary wavelength */
        /* Find intersection in opposite direction */
        alwan_vec2_f64 xy_opposite;
        xy_opposite.v[0] = ALWAN_LITERAL(2.0) * xy_white->v[0] - xy->v[0];
        xy_opposite.v[1] = ALWAN_LITERAL(2.0) * xy_white->v[1] - xy->v[1];

        status = alwan_intersect_spectral_locus(xy_white, &xy_opposite, &wl, &xy_wl);
        if (status != ALWAN_OK) {
            return ALWAN_E_INVALID;
        }
    }

    /* Compute excitation purity as ratio of distances:
     * pe = |white - color| / |white - spectral_locus| */
    alwan_f64 dx_color = xy->v[0] - xy_white->v[0];
    alwan_f64 dy_color = xy->v[1] - xy_white->v[1];
    alwan_f64 dist_color = ALWAN_SQRT(dx_color * dx_color + dy_color * dy_color);

    alwan_f64 dx_wl = xy_wl.v[0] - xy_white->v[0];
    alwan_f64 dy_wl = xy_wl.v[1] - xy_white->v[1];
    alwan_f64 dist_wl = ALWAN_SQRT(dx_wl * dx_wl + dy_wl * dy_wl);

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

int alwan_complementary_wavelength_f64(alwan_f64 *wavelength_out,
                                     alwan_vec2_f64 *xy_wl_out,
                                     alwan_vec2_f64 *xy_cw_out,
                                     alwan_vec2_f64 const *xy,
                                     alwan_vec2_f64 const *xy_white) {
    if (!xy || !xy_white || !wavelength_out) {
        return ALWAN_E_INVALID;
    }

    /* Extend line from color through white point to spectral locus (opposite direction) */
    alwan_vec2_f64 xy_opposite;
    xy_opposite.v[0] = ALWAN_LITERAL(2.0) * xy_white->v[0] - xy->v[0];
    xy_opposite.v[1] = ALWAN_LITERAL(2.0) * xy_white->v[1] - xy->v[1];

    alwan_f64 wl;
    alwan_vec2_f64 xy_intersection;
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

/* f64-internal facade: compiled in all builds, see ALWAN_WITH_F64_FACADE */
#if ALWAN_WITH_F64_FACADE
int alwan_gamut_volume_ratio_f64(alwan_f64 *ratio_out,
                               alwan_rgb_space_desc_f64 const *space1,
                               alwan_rgb_space_desc_f64 const *space2) {
    if (!space1 || !space2 || !ratio_out) {
        return ALWAN_E_INVALID;
    }

    /* Compute volume for both spaces */
    alwan_f64 volume1, volume2;
    int status1 = alwan_gamut_volume_f64(&volume1, space1);
    int status2 = alwan_gamut_volume_f64(&volume2, space2);

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

int alwan_gamut_coverage_f64(alwan_f64 *coverage_out,
                          alwan_rgb_space_desc_f64 const *space1,
                          alwan_rgb_space_desc_f64 const *space2,
                          size_t num_samples,
                          unsigned int seed) {
    if (!space1 || !space2 || !coverage_out) {
        return ALWAN_E_INVALID;
    }

    if (num_samples == 0) {
        num_samples = 10000;  /* Default sample count */
    }

    /* Derive matrices for both spaces */
    alwan_mat3x3_f64 rgb1_to_xyz, xyz_to_rgb1;
    alwan_mat3x3_f64 rgb2_to_xyz, xyz_to_rgb2;

    int status1 = alwan_rgb_derive_matrices_f64(&rgb1_to_xyz, &xyz_to_rgb1, space1);
    int status2 = alwan_rgb_derive_matrices_f64(&rgb2_to_xyz, &xyz_to_rgb2, space2);

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
        alwan_vec3_f64 rgb1;

        /* LCG for R */
        rng_state = (rng_state * 1103515245u + 12345u) & 0x7fffffffu;
        rgb1.v[0] = (alwan_f64)rng_state / (alwan_f64)0x7fffffff;

        /* LCG for G */
        rng_state = (rng_state * 1103515245u + 12345u) & 0x7fffffffu;
        rgb1.v[1] = (alwan_f64)rng_state / (alwan_f64)0x7fffffff;

        /* LCG for B */
        rng_state = (rng_state * 1103515245u + 12345u) & 0x7fffffffu;
        rgb1.v[2] = (alwan_f64)rng_state / (alwan_f64)0x7fffffff;

        /* Transform space1 RGB -> XYZ */
        alwan_vec3_f64 xyz;
        alwan_mat3_mulv_f64(&xyz, &rgb1_to_xyz, &rgb1);

        /* Transform XYZ -> space2 RGB */
        alwan_vec3_f64 rgb2;
        alwan_mat3_mulv_f64(&rgb2, &xyz_to_rgb2, &xyz);

        /* Check if rgb2 is in gamut (all components in [0,1]) */
        int in_gamut = (rgb2.v[0] >= ALWAN_LITERAL(0.0) && rgb2.v[0] <= ALWAN_LITERAL(1.0) &&
                        rgb2.v[1] >= ALWAN_LITERAL(0.0) && rgb2.v[1] <= ALWAN_LITERAL(1.0) &&
                        rgb2.v[2] >= ALWAN_LITERAL(0.0) && rgb2.v[2] <= ALWAN_LITERAL(1.0));

        if (in_gamut) {
            inside_count++;
        }
    }

    /* Compute coverage percentage */
    *coverage_out = (ALWAN_LITERAL(100.0) * (alwan_f64)inside_count) / (alwan_f64)num_samples;

    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64_FACADE */

/* ================================================================
 * Advanced Gamut Mapping
 * ================================================================ */

/* Oklab color space conversion (linear RGB <-> Oklab)
 * Based on Bjorn Ottosson's Oklab (2020)
 * https://bottosson.github.io/posts/oklab/ */

/* Linear sRGB -> Oklab */
static void alwan_linear_srgb_to_oklab(alwan_vec3_f64 const *rgb, alwan_vec3_f64 *oklab) {
    *oklab = gamut_linear_srgb_to_oklab_f64_v(*rgb);
}

/* Oklab -> Linear sRGB */
static void alwan_oklab_to_linear_srgb(alwan_vec3_f64 const *oklab, alwan_vec3_f64 *rgb) {
    *rgb = gamut_oklab_to_linear_srgb_f64_v(*oklab);
}

/* Compute maximum saturation for a given hue */
static alwan_f64 alwan_compute_max_saturation(alwan_f64 a, alwan_f64 b) {
    return gamut_compute_max_saturation_f64_v(a, b);
}

/* Find gamut cusp */
static void alwan_find_cusp(alwan_f64 a, alwan_f64 b, alwan_f64 *L_cusp, alwan_f64 *C_cusp) {
    alwan_vec2_f64 cusp = gamut_find_cusp_f64_v(a, b);
    *L_cusp = cusp.v[0];
    *C_cusp = cusp.v[1];
}

/* Find intersection of gamut boundary */
static alwan_f64 alwan_find_gamut_intersection(alwan_f64 a, alwan_f64 b,
                                                    alwan_f64 L1, alwan_f64 C1,
                                                    alwan_f64 L0, alwan_f64 C0) {
    return gamut_find_intersection_f64_v(a, b, L1, C1, L0, C0);
}

/* ----------------------------------------------------------------
 * Working-space transform for the perceptual (Oklab) gamut methods.
 *
 * The Oklab cusp/boundary model used below is defined for *linear sRGB*.
 * To honour the caller-supplied `space`, the input RGB (expressed in
 * `space`) is converted into that linear-sRGB working space, mapped, then
 * converted back. Different `space` -> different result, so the parameter
 * is meaningful rather than ignored.
 *
 * When `space` is (colorimetrically) sRGB the composed transform is the
 * identity and is snapped to *exact* identity, so the common path is
 * bit-for-bit identical to a direct sRGB mapping (no round-trip noise).
 * The 1e-3 element bound is far tighter than the matrix gap between sRGB
 * and any other standard gamut (Adobe RGB, Display P3, BT.2020, ...), so
 * only genuinely-sRGB spaces snap.
 * ---------------------------------------------------------------- */
static int gamut_working_xform_f64(alwan_mat3x3_f64 *to_srgb,
                                   alwan_mat3x3_f64 *from_srgb,
                                   alwan_rgb_space_desc_f64 const *space) {
    alwan_mat3x3_f64 space_to_xyz, xyz_to_space;
    int rc = alwan_rgb_derive_matrices_f64(&space_to_xyz, &xyz_to_space, space);
    if (rc != ALWAN_OK) return rc;

    /* Reference linear sRGB: BT.709 primaries + D65 (published standard). */
    alwan_rgb_space_desc_f64 srgb;
    srgb.primaries_xy[0] = ALWAN_BT709_RED_x;   srgb.primaries_xy[1] = ALWAN_BT709_RED_y;
    srgb.primaries_xy[2] = ALWAN_BT709_GREEN_x; srgb.primaries_xy[3] = ALWAN_BT709_GREEN_y;
    srgb.primaries_xy[4] = ALWAN_BT709_BLUE_x;  srgb.primaries_xy[5] = ALWAN_BT709_BLUE_y;
    srgb.white_xy[0] = ALWAN_LITERAL(0.31271);  srgb.white_xy[1] = ALWAN_LITERAL(0.32902);
    srgb.oetf = ALWAN_TF_LINEAR; srgb.eotf = ALWAN_TF_LINEAR; srgb.has_matrices = 0;

    alwan_mat3x3_f64 srgb_to_xyz, xyz_to_srgb;
    rc = alwan_rgb_derive_matrices_f64(&srgb_to_xyz, &xyz_to_srgb, &srgb);
    if (rc != ALWAN_OK) return rc;

    alwan_mat3_mul_f64(to_srgb, &xyz_to_srgb, &space_to_xyz);
    alwan_mat3_mul_f64(from_srgb, &xyz_to_space, &srgb_to_xyz);

    {
        int is_identity = 1;
        for (int i = 0; i < 9 && is_identity; i++) {
            alwan_f64 const want = (i % 4 == 0) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
            if (ALWAN_ABS(to_srgb->m[i] - want) > ALWAN_LITERAL(1e-3)) is_identity = 0;
        }
        if (is_identity) {
            for (int i = 0; i < 9; i++) {
                alwan_f64 const v = (i % 4 == 0) ? ALWAN_LITERAL(1.0) : ALWAN_LITERAL(0.0);
                to_srgb->m[i] = v; from_srgb->m[i] = v;
            }
        }
    }
    return ALWAN_OK;
}

/* Gamut mapping implementation */
int alwan_gamut_map_advanced_f64(alwan_rgb_f64 *rgb_out,
                              alwan_gamut_map_method method,
                              alwan_rgb_space_desc_f64 const *space,
                              alwan_rgb_f64 const *rgb_linear) {
    if (!space || !rgb_linear || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Convert to vec3 for internal processing */
    alwan_vec3_f64 rgb_vec;
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

    /* Honour `space`: convert the input (expressed in `space`) into the
     * linear-sRGB working space the Oklab gamut model is defined for. */
    alwan_mat3x3_f64 to_srgb, from_srgb;
    {
        int xrc = gamut_working_xform_f64(&to_srgb, &from_srgb, space);
        if (xrc != ALWAN_OK) return xrc;
    }
    alwan_vec3_f64 rgb_work;
    alwan_mat3_mulv_f64(&rgb_work, &to_srgb, &rgb_vec);

    /* Convert to Oklab */
    alwan_vec3_f64 oklab;
    alwan_linear_srgb_to_oklab(&rgb_work, &oklab);

    alwan_f64 L = oklab.v[0];
    alwan_f64 a = oklab.v[1];
    alwan_f64 b = oklab.v[2];
    alwan_f64 C = ALWAN_SQRT(a * a + b * b);

    /* If achromatic or already in gamut, check if simple clip works */
    if (C < ALWAN_LITERAL(0.0001) ||
        (rgb_linear->r >= ALWAN_LITERAL(0.0) && rgb_linear->r <= ALWAN_LITERAL(1.0) &&
         rgb_linear->g >= ALWAN_LITERAL(0.0) && rgb_linear->g <= ALWAN_LITERAL(1.0) &&
         rgb_linear->b >= ALWAN_LITERAL(0.0) && rgb_linear->b <= ALWAN_LITERAL(1.0))) {
        *rgb_out = *rgb_linear;
        return ALWAN_OK;
    }

    /* Normalize a, b */
    alwan_f64 a_norm = a / C;
    alwan_f64 b_norm = b / C;

    alwan_f64 L0, C0;
    alwan_f64 alpha = ALWAN_LITERAL(0.0);
    (void)alpha;  /* Selected per-method below; reserved for a future blend, presently unused (mirrors the f32 path). */

    /* Select projection point based on method */
    if (method == ALWAN_GAMUT_MAP_ADAPTIVE_L0) {
        /* Adaptive L0: project toward L=0.5, C=0 */
        L0 = ALWAN_LITERAL(0.5);
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.05);  /* Blend factor */
    } else if (method == ALWAN_GAMUT_MAP_ADAPTIVE_CUSP) {
        /* Adaptive toward cusp */
        alwan_f64 L_cusp, C_cusp;
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
        alwan_f64 L_cusp, C_cusp;
        alwan_find_cusp(a_norm, b_norm, &L_cusp, &C_cusp);

        /* Project toward cusp with soft knee */
        L0 = L_cusp;
        C0 = ALWAN_LITERAL(0.0);
        alpha = ALWAN_LITERAL(0.15);  /* Larger blend for softer knee */
    } else if (method == ALWAN_GAMUT_MAP_HPMINDE) {
        /* HPMINDE: Hue-Preserving Minimum dE
         * Minimizes perceptual color difference while preserving hue */
        alwan_f64 L_cusp, C_cusp;
        alwan_find_cusp(a_norm, b_norm, &L_cusp, &C_cusp);

        /* Find optimal projection point that minimizes dE */
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
    alwan_f64 t = alwan_find_gamut_intersection(a_norm, b_norm, L, C, L0, C0);

    /* Clamp t to valid range */
    if (t < ALWAN_LITERAL(0.0)) t = ALWAN_LITERAL(0.0);
    if (t > ALWAN_LITERAL(1.0)) t = ALWAN_LITERAL(1.0);

    /* Interpolate to boundary */
    alwan_f64 L_clipped = L0 * (ALWAN_LITERAL(1.0) - t) + t * L;
    alwan_f64 C_clipped = t * C;

    /* Convert back to Oklab */
    alwan_vec3_f64 oklab_clipped;
    oklab_clipped.v[0] = L_clipped;
    oklab_clipped.v[1] = C_clipped * a_norm;
    oklab_clipped.v[2] = C_clipped * b_norm;

    /* Convert clipped Oklab back to linear sRGB (the working space) ... */
    alwan_vec3_f64 rgb_srgb;
    alwan_oklab_to_linear_srgb(&oklab_clipped, &rgb_srgb);

    /* ... then back into the caller's `space`. */
    alwan_vec3_f64 rgb_result;
    alwan_mat3_mulv_f64(&rgb_result, &from_srgb, &rgb_srgb);

    /* Final safety clamp and convert to rgb */
    rgb_out->r = (rgb_result.v[0] < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                 (rgb_result.v[0] > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_result.v[0];
    rgb_out->g = (rgb_result.v[1] < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                 (rgb_result.v[1] > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_result.v[1];
    rgb_out->b = (rgb_result.v[2] < ALWAN_LITERAL(0.0)) ? ALWAN_LITERAL(0.0) :
                 (rgb_result.v[2] > ALWAN_LITERAL(1.0)) ? ALWAN_LITERAL(1.0) : rgb_result.v[2];

    return ALWAN_OK;
}

int alwan_css_gamut_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count) {
    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    {
        size_t processed = 0;
        while (processed < count) {
            size_t tile = count - processed;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
            alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
            alwan__css_gamut_map_kernel(c0, c1, c2, c0, c1, c2, tile);
            alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
            processed += tile;
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Gamut map planar
 * ---------------------------------------------------------------- */

int alwan_gamut_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count, alwan_gamut_map_method method) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

    /* SIMD fast path for simple clip */
    if (method == ALWAN_GAMUT_MAP_CLIP) {
        size_t processed = 0;
        while (processed < count) {
            size_t tile = count - processed;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
            alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
            alwan__gamut_clip_kernel(c0, c1, c2, tile);
            alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
            processed += tile;
        }
        return ALWAN_OK;
    }

    void (*map_fn)(alwan_vec3_f64 const *, alwan_vec3_f64 *) = NULL;
    switch (method) {
        case ALWAN_GAMUT_MAP_HUE_PRESERVING:
            map_fn = gamut_map_hue_preserving_single;
            break;
        default:
            return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_vec3_f64 rgb_in_vec = {{
            *(alwan_f64 const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_f64 const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_f64 const *)((char const *)in_ch2 + i * in_stride)
        }};
        alwan_vec3_f64 rgb_out_vec;
        map_fn(&rgb_in_vec, &rgb_out_vec);
        *(alwan_f64 *)((char *)out_ch0 + i * out_stride) = rgb_out_vec.v[0];
        *(alwan_f64 *)((char *)out_ch1 + i * out_stride) = rgb_out_vec.v[1];
        *(alwan_f64 *)((char *)out_ch2 + i * out_stride) = rgb_out_vec.v[2];
    }

    return ALWAN_OK;
}

int alwan_css_gamut_f64_map_planar(alwan_f64 *out_ch0, size_t out_stride, alwan_f64 *out_ch1, alwan_f64 *out_ch2, alwan_f64 const *in_ch0, size_t in_stride, alwan_f64 const *in_ch1, alwan_f64 const *in_ch2, size_t count) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) {
        return ALWAN_E_INVALID;
    }

    {
        size_t processed = 0;
        while (processed < count) {
            size_t tile = count - processed;
            if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
            ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
            alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
            alwan__css_gamut_map_kernel(c0, c1, c2, c0, c1, c2, tile);
            alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
            processed += tile;
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Gamut map _ex (typed pixel format)
 * ---------------------------------------------------------------- */

int alwan_gamut_map_interleave_ex(void *rgb_out, size_t out_stride, void const *rgb_in, size_t in_stride, size_t count, alwan_pixel_format out_fmt, alwan_gamut_map_method method, alwan_pixel_format in_fmt) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    /* CLIP fast path: dual-precision dispatch */
    if (method == ALWAN_GAMUT_MAP_CLIP) {
        if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32)
            return alwan_gamut_clip_f32_map_interleave((float *)rgb_out, out_stride, (float const *)rgb_in, in_stride, count);
        if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64)
            return alwan_gamut_clip_f64_map_interleave((double *)rgb_out, out_stride, (double const *)rgb_in, in_stride, count);
        if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
            size_t off_ = 0;
            while (off_ < count) {
                size_t tile_ = count - off_;
                if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
                ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
                ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
                alwan__load_tile_typed_aos_f64(ibuf_, rgb_in, in_fmt, off_, in_stride, tile_, 3);
                alwan_gamut_clip_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_, 3 * sizeof(double), tile_);
                alwan__store_tile_typed_aos_f64(rgb_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
                off_ += tile_;
            }
        } else {
            size_t off_ = 0;
            while (off_ < count) {
                size_t tile_ = count - off_;
                if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
                ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
                ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
                alwan__load_tile_typed_aos_f32(ibuf_, rgb_in, in_fmt, off_, in_stride, tile_, 3);
                alwan_gamut_clip_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_, 3 * sizeof(float), tile_);
                alwan__store_tile_typed_aos_f32(rgb_out, out_fmt, off_, out_stride, obuf_, tile_, 3);
                off_ += tile_;
            }
        }
        return ALWAN_OK;
    }

    void (*map_fn)(alwan_vec3_f64 const *, alwan_vec3_f64 *) = NULL;
    switch (method) {
        case ALWAN_GAMUT_MAP_HUE_PRESERVING: map_fn = gamut_map_hue_preserving_single; break;
        default: return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_f64 sv[3];
        alwan__load3_typed(sv, (char const *)rgb_in + i * in_stride, in_fmt);
        alwan_vec3_f64 vin = {{sv[0], sv[1], sv[2]}}, vout;
        map_fn(&vin, &vout);
        alwan_f64 dv[3] = {vout.v[0], vout.v[1], vout.v[2]};
        alwan__store3_typed((char *)rgb_out + i * out_stride, dv, out_fmt);
    }
    return ALWAN_OK;
}

int alwan_gamut_map_planar_ex(void *out0, size_t out_stride, void *out1, void *out2, void const *in0, size_t in_stride, void const *in1, void const *in2, size_t count, alwan_pixel_format out_fmt, alwan_gamut_map_method method, alwan_pixel_format in_fmt) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;

    /* CLIP fast path: dual-precision dispatch */
    if (method == ALWAN_GAMUT_MAP_CLIP) {
        if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32)
            return alwan_gamut_clip_f32_map_planar((float *)out0, out_stride, (float *)out1, (float *)out2, (float const *)in0, in_stride, (float const *)in1, (float const *)in2, count);
        if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64)
            return alwan_gamut_clip_f64_map_planar((double *)out0, out_stride, (double *)out1, (double *)out2, (double const *)in0, in_stride, (double const *)in1, (double const *)in2, count);
        if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
            size_t off_ = 0;
            while (off_ < count) {
                size_t tile_ = count - off_;
                if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
                ALWAN_ALIGN(32) double ic0_[ALWAN_TILE_PIXELS_F64], ic1_[ALWAN_TILE_PIXELS_F64], ic2_[ALWAN_TILE_PIXELS_F64];
                ALWAN_ALIGN(32) double oc0_[ALWAN_TILE_PIXELS_F64], oc1_[ALWAN_TILE_PIXELS_F64], oc2_[ALWAN_TILE_PIXELS_F64];
                alwan__load_tile_typed_ch_f64(ic0_, in0, in_fmt, off_, in_stride, tile_);
                alwan__load_tile_typed_ch_f64(ic1_, in1, in_fmt, off_, in_stride, tile_);
                alwan__load_tile_typed_ch_f64(ic2_, in2, in_fmt, off_, in_stride, tile_);
                alwan_gamut_clip_f64_map_planar(oc0_, sizeof(double), oc1_, oc2_, ic0_, sizeof(double), ic1_, ic2_, tile_);
                alwan__store_tile_typed_ch_f64(out0, out_fmt, off_, out_stride, oc0_, tile_);
                alwan__store_tile_typed_ch_f64(out1, out_fmt, off_, out_stride, oc1_, tile_);
                alwan__store_tile_typed_ch_f64(out2, out_fmt, off_, out_stride, oc2_, tile_);
                off_ += tile_;
            }
        } else {
            size_t off_ = 0;
            while (off_ < count) {
                size_t tile_ = count - off_;
                if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
                ALWAN_ALIGN(32) float ic0_[ALWAN_TILE_PIXELS_F32], ic1_[ALWAN_TILE_PIXELS_F32], ic2_[ALWAN_TILE_PIXELS_F32];
                ALWAN_ALIGN(32) float oc0_[ALWAN_TILE_PIXELS_F32], oc1_[ALWAN_TILE_PIXELS_F32], oc2_[ALWAN_TILE_PIXELS_F32];
                alwan__load_tile_typed_ch_f32(ic0_, in0, in_fmt, off_, in_stride, tile_);
                alwan__load_tile_typed_ch_f32(ic1_, in1, in_fmt, off_, in_stride, tile_);
                alwan__load_tile_typed_ch_f32(ic2_, in2, in_fmt, off_, in_stride, tile_);
                alwan_gamut_clip_f32_map_planar(oc0_, sizeof(float), oc1_, oc2_, ic0_, sizeof(float), ic1_, ic2_, tile_);
                alwan__store_tile_typed_ch_f32(out0, out_fmt, off_, out_stride, oc0_, tile_);
                alwan__store_tile_typed_ch_f32(out1, out_fmt, off_, out_stride, oc1_, tile_);
                alwan__store_tile_typed_ch_f32(out2, out_fmt, off_, out_stride, oc2_, tile_);
                off_ += tile_;
            }
        }
        return ALWAN_OK;
    }

    void (*map_fn)(alwan_vec3_f64 const *, alwan_vec3_f64 *) = NULL;
    switch (method) {
        case ALWAN_GAMUT_MAP_HUE_PRESERVING: map_fn = gamut_map_hue_preserving_single; break;
        default: return ALWAN_E_INVALID;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_vec3_f64 vin = {{
            alwan__load1_typed((char const *)in0 + i * in_stride, in_fmt),
            alwan__load1_typed((char const *)in1 + i * in_stride, in_fmt),
            alwan__load1_typed((char const *)in2 + i * in_stride, in_fmt)
        }}, vout;
        map_fn(&vin, &vout);
        alwan__store1_typed((char *)out0 + i * out_stride, vout.v[0], out_fmt);
        alwan__store1_typed((char *)out1 + i * out_stride, vout.v[1], out_fmt);
        alwan__store1_typed((char *)out2 + i * out_stride, vout.v[2], out_fmt);
    }
    return ALWAN_OK;
}

ALWAN_EX_DELEGATE_DUAL(alwan_css_gamut_map_interleave_ex,
                       alwan_css_gamut_map_f32_map_interleave,
                       alwan_css_gamut_map_f64_map_interleave)

ALWAN_PLANAR_EX_DELEGATE_DUAL(alwan_css_gamut_map_planar_ex,
                               alwan_css_gamut_map_f32_map_planar,
                               alwan_css_gamut_map_f64_map_planar)

/* ================================================================
 * f32 wrappers for gamut metrics / mapping.
 *
 * Convert the f32 space descriptors to f64 and delegate.
 * ================================================================ */

#if ALWAN_WITH_F32
static void rgb_space_desc_f32_to_f64(alwan_rgb_space_desc_f64 *out, alwan_rgb_space_desc_f32 const *in) {
    for (int j = 0; j < 6; j++) out->primaries_xy[j] = (double)in->primaries_xy[j];
    out->white_xy[0] = (double)in->white_xy[0];
    out->white_xy[1] = (double)in->white_xy[1];
    out->oetf = in->oetf;
    out->eotf = in->eotf;
    for (int j = 0; j < 9; j++) {
        out->rgb_to_xyz.m[j] = (double)in->rgb_to_xyz.m[j];
        out->xyz_to_rgb.m[j] = (double)in->xyz_to_rgb.m[j];
    }
    out->has_matrices = in->has_matrices;
}

int alwan_gamut_volume_f32(alwan_f32 *volume,
                          alwan_rgb_space_desc_f32 const *space) {
    if (!space || !volume) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 tmp;
    rgb_space_desc_f32_to_f64(&tmp, space);
    alwan_f64 vol_f64 = 0.0;
    int rc = alwan_gamut_volume_f64(&vol_f64, &tmp);
    if (rc == ALWAN_OK) *volume = (alwan_f32)vol_f64;
    return rc;
}
#endif /* ALWAN_WITH_F32 */

#if ALWAN_WITH_F32
/* Hue-preserving gamut mapping, native f32 (mirrors gamut_map_hue_preserving_single).
 * XYZ->RGB is a genuine per-element colour transform, so it computes in float
 * throughout rather than widening to double. The gamut *metrics* below
 * (volume/coverage) intentionally stay f64-internal -- they are scalar
 * reductions where single precision costs accuracy for no benefit. */
static void gamut_map_hue_preserving_single_f32(alwan_vec3_f32 const *rgb_in, alwan_vec3_f32 *rgb_out) {
    if (rgb_in->v[0] >= 0.0f && rgb_in->v[0] <= 1.0f &&
        rgb_in->v[1] >= 0.0f && rgb_in->v[1] <= 1.0f &&
        rgb_in->v[2] >= 0.0f && rgb_in->v[2] <= 1.0f) {
        *rgb_out = *rgb_in;
        return;
    }

    alwan_f32 const L = (alwan_f32)ALWAN_LUMA_KR_BT709 * rgb_in->v[0] +
                        (alwan_f32)ALWAN_LUMA_KG_BT709 * rgb_in->v[1] +
                        (alwan_f32)ALWAN_LUMA_KB_BT709 * rgb_in->v[2];
    alwan_f32 const L_clamped = (L < 0.0f) ? 0.0f : (L > 1.0f) ? 1.0f : L;

    alwan_f32 t_min = 0.0f, t_max = 1.0f;
    alwan_vec3_f32 neutral;
    neutral.v[0] = neutral.v[1] = neutral.v[2] = L_clamped;
    *rgb_out = neutral;

    for (int iter = 0; iter < 20; iter++) {
        alwan_f32 const t = (t_min + t_max) * 0.5f;
        alwan_vec3_f32 test;
        test.v[0] = t * rgb_in->v[0] + (1.0f - t) * neutral.v[0];
        test.v[1] = t * rgb_in->v[1] + (1.0f - t) * neutral.v[1];
        test.v[2] = t * rgb_in->v[2] + (1.0f - t) * neutral.v[2];
        if (test.v[0] >= 0.0f && test.v[0] <= 1.0f &&
            test.v[1] >= 0.0f && test.v[1] <= 1.0f &&
            test.v[2] >= 0.0f && test.v[2] <= 1.0f) {
            t_min = t; *rgb_out = test;
        } else {
            t_max = t;
        }
    }
}

int alwan_gamut_map_xyz_to_rgb_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_space_desc_f32 const *space, alwan_xyz_f32 const *xyz_in, alwan_ctx *ctx) {
    (void)ctx;  /* Reserved for future use */
    if (!space || !xyz_in || !rgb_out) return ALWAN_E_INVALID;

    alwan_mat3x3_f32 rgb_to_xyz, xyz_to_rgb;
    int status = alwan_rgb_derive_matrices_f32(&rgb_to_xyz, &xyz_to_rgb, space);
    if (status != ALWAN_OK) return status;

    alwan_vec3_f32 xyz_vec, rgb_raw, rgb_mapped;
    xyz_vec.v[0] = xyz_in->x;
    xyz_vec.v[1] = xyz_in->y;
    xyz_vec.v[2] = xyz_in->z;

    alwan_mat3_mulv_f32(&rgb_raw, &xyz_to_rgb, &xyz_vec);
    gamut_map_hue_preserving_single_f32(&rgb_raw, &rgb_mapped);

    rgb_out->r = rgb_mapped.v[0];
    rgb_out->g = rgb_mapped.v[1];
    rgb_out->b = rgb_mapped.v[2];
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

#if ALWAN_WITH_F32
int alwan_gamut_volume_ratio_f32(alwan_f32 *ratio_out,
                               alwan_rgb_space_desc_f32 const *space1,
                               alwan_rgb_space_desc_f32 const *space2) {
    if (!space1 || !space2 || !ratio_out) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 a, b;
    rgb_space_desc_f32_to_f64(&a, space1);
    rgb_space_desc_f32_to_f64(&b, space2);
    alwan_f64 ratio64 = 0.0;
    int rc = alwan_gamut_volume_ratio_f64(&ratio64, &a, &b);
    if (rc == ALWAN_OK) *ratio_out = (alwan_f32)ratio64;
    return rc;
}

int alwan_gamut_coverage_f32(alwan_f32 *coverage_out,
                          alwan_rgb_space_desc_f32 const *space1,
                          alwan_rgb_space_desc_f32 const *space2,
                          size_t num_samples,
                          unsigned int seed) {
    if (!space1 || !space2 || !coverage_out) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 a, b;
    rgb_space_desc_f32_to_f64(&a, space1);
    rgb_space_desc_f32_to_f64(&b, space2);
    alwan_f64 cov64 = 0.0;
    int rc = alwan_gamut_coverage_f64(&cov64, &a, &b, num_samples, seed);
    if (rc == ALWAN_OK) *coverage_out = (alwan_f32)cov64;
    return rc;
}
#endif /* ALWAN_WITH_F32 */

/* ================================================================
 * Native f32 gamut analysis & advanced mapping.
 *
 * These mirror the f64 reference implementations above. They are
 * closed-form geometry/formula routines (point-in-polygon, spectral
 * locus table interpolation, wavelength/purity geometry, Oklab gamut
 * compression) -- not iterative solvers -- so they compute entirely in
 * single precision (a genuine native f32 path, no f64 widening). The
 * shared geometry tables (POINTER_GAMUT_BOUNDARY / SPECTRAL_LOCUS_XY)
 * are stored once as f64 and read as float at point-of-use.
 * ================================================================ */

#if ALWAN_WITH_F32

/* Ray-casting point-in-polygon against the (f64) Pointer's Gamut table. */
static int alwan_point_in_polygon_f32(alwan_vec2_f32 const *point,
                                      alwan_vec2_f64 const *polygon,
                                      size_t polygon_count) {
    int inside = 0;
    alwan_f32 px = point->v[0];
    alwan_f32 py = point->v[1];

    for (size_t i = 0, j = polygon_count - 1; i < polygon_count; j = i++) {
        alwan_f32 xi = (alwan_f32)polygon[i].v[0], yi = (alwan_f32)polygon[i].v[1];
        alwan_f32 xj = (alwan_f32)polygon[j].v[0], yj = (alwan_f32)polygon[j].v[1];

        int intersect = ((yi > py) != (yj > py)) &&
                        (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

int alwan_is_within_pointer_gamut_f32(alwan_vec2_f32 const *xy) {
    if (!xy) {
        return 0;
    }
    return alwan_point_in_polygon_f32(xy, POINTER_GAMUT_BOUNDARY, 32);
}

int alwan_spectral_locus_xy_f32(alwan_vec2_f32 *xy_out, alwan_f32 wavelength) {
    if (!xy_out) {
        return ALWAN_E_INVALID;
    }

    if (wavelength < (alwan_f32)SPECTRAL_LOCUS_WL_MIN ||
        wavelength > (alwan_f32)SPECTRAL_LOCUS_WL_MAX) {
        return ALWAN_E_INVALID;
    }

    alwan_f32 t = (wavelength - (alwan_f32)SPECTRAL_LOCUS_WL_MIN) /
                  (alwan_f32)SPECTRAL_LOCUS_WL_INTERVAL;
    size_t idx = (size_t)floorf(t);
    alwan_f32 frac = t - (alwan_f32)idx;

    if (idx >= (size_t)(SPECTRAL_LOCUS_COUNT - 1)) {
        idx = SPECTRAL_LOCUS_COUNT - 2;
        frac = 1.0f;
    }

    alwan_vec2_f64 const *xy0 = &SPECTRAL_LOCUS_XY[idx];
    alwan_vec2_f64 const *xy1 = &SPECTRAL_LOCUS_XY[idx + 1];

    xy_out->v[0] = (alwan_f32)xy0->v[0] + frac * ((alwan_f32)xy1->v[0] - (alwan_f32)xy0->v[0]);
    xy_out->v[1] = (alwan_f32)xy0->v[1] + frac * ((alwan_f32)xy1->v[1] - (alwan_f32)xy0->v[1]);
    return ALWAN_OK;
}

/* Intersection of ray (p1 -> p2) with the spectral locus; f32 mirror of
 * alwan_intersect_spectral_locus(). */
static int alwan_intersect_spectral_locus_f32(alwan_vec2_f32 const *p1,
                                              alwan_vec2_f32 const *p2,
                                              alwan_f32 *wavelength_out,
                                              alwan_vec2_f32 *xy_out) {
    alwan_f32 best_t = -1.0f;
    size_t best_idx = 0;

    for (size_t i = 0; i < (size_t)(SPECTRAL_LOCUS_COUNT - 1); i++) {
        alwan_f32 s1x = (alwan_f32)SPECTRAL_LOCUS_XY[i].v[0];
        alwan_f32 s1y = (alwan_f32)SPECTRAL_LOCUS_XY[i].v[1];
        alwan_f32 s2x = (alwan_f32)SPECTRAL_LOCUS_XY[i + 1].v[0];
        alwan_f32 s2y = (alwan_f32)SPECTRAL_LOCUS_XY[i + 1].v[1];

        alwan_f32 dx1 = p2->v[0] - p1->v[0];
        alwan_f32 dy1 = p2->v[1] - p1->v[1];
        alwan_f32 dx2 = s2x - s1x;
        alwan_f32 dy2 = s2y - s1y;

        alwan_f32 det = dx1 * dy2 - dy1 * dx2;
        if (fabsf(det) < 1e-10f) {
            continue;
        }

        alwan_f32 dx3 = s1x - p1->v[0];
        alwan_f32 dy3 = s1y - p1->v[1];

        alwan_f32 t = (dx3 * dy2 - dy3 * dx2) / det;
        alwan_f32 u = (dx3 * dy1 - dy3 * dx1) / det;

        if (t >= 0.0f && u >= 0.0f && u <= 1.0f) {
            if (best_t < 0.0f || t < best_t) {
                best_t = t;
                best_idx = i;
            }
        }
    }

    if (best_t < 0.0f) {
        return ALWAN_E_INVALID;
    }

    alwan_f32 s1x = (alwan_f32)SPECTRAL_LOCUS_XY[best_idx].v[0];
    alwan_f32 s1y = (alwan_f32)SPECTRAL_LOCUS_XY[best_idx].v[1];
    alwan_f32 s2x = (alwan_f32)SPECTRAL_LOCUS_XY[best_idx + 1].v[0];
    alwan_f32 s2y = (alwan_f32)SPECTRAL_LOCUS_XY[best_idx + 1].v[1];

    alwan_f32 dx2 = s2x - s1x;
    alwan_f32 dy2 = s2y - s1y;
    alwan_f32 dx3 = s1x - p1->v[0];
    alwan_f32 dy3 = s1y - p1->v[1];
    alwan_f32 dx1 = p2->v[0] - p1->v[0];
    alwan_f32 dy1 = p2->v[1] - p1->v[1];
    alwan_f32 det = dx1 * dy2 - dy1 * dx2;
    alwan_f32 u = (dx3 * dy1 - dy3 * dx1) / det;

    if (xy_out) {
        xy_out->v[0] = s1x + u * dx2;
        xy_out->v[1] = s1y + u * dy2;
    }

    alwan_f32 wl = (alwan_f32)SPECTRAL_LOCUS_WL_MIN +
                   (alwan_f32)best_idx * (alwan_f32)SPECTRAL_LOCUS_WL_INTERVAL;
    wl += u * (alwan_f32)SPECTRAL_LOCUS_WL_INTERVAL;

    if (wavelength_out) {
        *wavelength_out = wl;
    }
    return ALWAN_OK;
}

int alwan_dominant_wavelength_f32(alwan_f32 *wavelength_out,
                              alwan_vec2_f32 *xy_wl_out,
                              alwan_vec2_f32 *xy_cw_out,
                              alwan_vec2_f32 const *xy,
                              alwan_vec2_f32 const *xy_white) {
    if (!xy || !xy_white || !wavelength_out) {
        return ALWAN_E_INVALID;
    }

    alwan_f32 wl;
    alwan_vec2_f32 xy_intersection;
    int status = alwan_intersect_spectral_locus_f32(xy_white, xy, &wl, &xy_intersection);
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

int alwan_excitation_purity_f32(alwan_f32 *purity_out,
                            alwan_vec2_f32 const *xy,
                            alwan_vec2_f32 const *xy_white) {
    if (!xy || !xy_white || !purity_out) {
        return ALWAN_E_INVALID;
    }

    alwan_vec2_f32 xy_wl;
    alwan_f32 wl;
    int status = alwan_intersect_spectral_locus_f32(xy_white, xy, &wl, &xy_wl);
    if (status != ALWAN_OK) {
        alwan_vec2_f32 xy_opposite;
        xy_opposite.v[0] = 2.0f * xy_white->v[0] - xy->v[0];
        xy_opposite.v[1] = 2.0f * xy_white->v[1] - xy->v[1];

        status = alwan_intersect_spectral_locus_f32(xy_white, &xy_opposite, &wl, &xy_wl);
        if (status != ALWAN_OK) {
            return ALWAN_E_INVALID;
        }
    }

    alwan_f32 dx_color = xy->v[0] - xy_white->v[0];
    alwan_f32 dy_color = xy->v[1] - xy_white->v[1];
    alwan_f32 dist_color = sqrtf(dx_color * dx_color + dy_color * dy_color);

    alwan_f32 dx_wl = xy_wl.v[0] - xy_white->v[0];
    alwan_f32 dy_wl = xy_wl.v[1] - xy_white->v[1];
    alwan_f32 dist_wl = sqrtf(dx_wl * dx_wl + dy_wl * dy_wl);

    if (dist_wl < 1e-10f) {
        *purity_out = 0.0f;
        return ALWAN_OK;
    }

    *purity_out = dist_color / dist_wl;
    if (*purity_out < 0.0f) {
        *purity_out = 0.0f;
    } else if (*purity_out > 1.0f) {
        *purity_out = 1.0f;
    }
    return ALWAN_OK;
}

int alwan_complementary_wavelength_f32(alwan_f32 *wavelength_out,
                                   alwan_vec2_f32 *xy_wl_out,
                                   alwan_vec2_f32 *xy_cw_out,
                                   alwan_vec2_f32 const *xy,
                                   alwan_vec2_f32 const *xy_white) {
    if (!xy || !xy_white || !wavelength_out) {
        return ALWAN_E_INVALID;
    }

    alwan_vec2_f32 xy_opposite;
    xy_opposite.v[0] = 2.0f * xy_white->v[0] - xy->v[0];
    xy_opposite.v[1] = 2.0f * xy_white->v[1] - xy->v[1];

    alwan_f32 wl;
    alwan_vec2_f32 xy_intersection;
    int status = alwan_intersect_spectral_locus_f32(xy_white, &xy_opposite, &wl, &xy_intersection);
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

/* Working-space transform, f32 mirror of gamut_working_xform_f64. See that
 * function for the rationale (honour `space`; snap to exact identity for
 * colorimetric sRGB so the common path is unchanged). */
static int gamut_working_xform_f32(alwan_mat3x3_f32 *to_srgb,
                                   alwan_mat3x3_f32 *from_srgb,
                                   alwan_rgb_space_desc_f32 const *space) {
    alwan_mat3x3_f32 space_to_xyz, xyz_to_space;
    int rc = alwan_rgb_derive_matrices_f32(&space_to_xyz, &xyz_to_space, space);
    if (rc != ALWAN_OK) return rc;

    alwan_rgb_space_desc_f32 srgb;
    srgb.primaries_xy[0] = (alwan_f32)ALWAN_BT709_RED_x;   srgb.primaries_xy[1] = (alwan_f32)ALWAN_BT709_RED_y;
    srgb.primaries_xy[2] = (alwan_f32)ALWAN_BT709_GREEN_x; srgb.primaries_xy[3] = (alwan_f32)ALWAN_BT709_GREEN_y;
    srgb.primaries_xy[4] = (alwan_f32)ALWAN_BT709_BLUE_x;  srgb.primaries_xy[5] = (alwan_f32)ALWAN_BT709_BLUE_y;
    srgb.white_xy[0] = 0.31271f; srgb.white_xy[1] = 0.32902f;
    srgb.oetf = ALWAN_TF_LINEAR; srgb.eotf = ALWAN_TF_LINEAR; srgb.has_matrices = 0;

    alwan_mat3x3_f32 srgb_to_xyz, xyz_to_srgb;
    rc = alwan_rgb_derive_matrices_f32(&srgb_to_xyz, &xyz_to_srgb, &srgb);
    if (rc != ALWAN_OK) return rc;

    alwan_mat3_mul_f32(to_srgb, &xyz_to_srgb, &space_to_xyz);
    alwan_mat3_mul_f32(from_srgb, &xyz_to_space, &srgb_to_xyz);

    {
        int is_identity = 1;
        for (int i = 0; i < 9 && is_identity; i++) {
            alwan_f32 want = (i % 4 == 0) ? 1.0f : 0.0f;
            if (fabsf(to_srgb->m[i] - want) > 1e-3f) is_identity = 0;
        }
        if (is_identity) {
            for (int i = 0; i < 9; i++) {
                alwan_f32 v = (i % 4 == 0) ? 1.0f : 0.0f;
                to_srgb->m[i] = v; from_srgb->m[i] = v;
            }
        }
    }
    return ALWAN_OK;
}

int alwan_gamut_map_advanced_f32(alwan_rgb_f32 *rgb_out,
                             alwan_gamut_map_method method,
                             alwan_rgb_space_desc_f32 const *space,
                             alwan_rgb_f32 const *rgb_linear) {
    if (!space || !rgb_linear || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    alwan_vec3_f32 rgb_vec;
    rgb_vec.v[0] = rgb_linear->r;
    rgb_vec.v[1] = rgb_linear->g;
    rgb_vec.v[2] = rgb_linear->b;

    /* Method 0: Simple clipping (in `space`'s own [0,1] cube). */
    if (method == ALWAN_GAMUT_MAP_CLIP) {
        rgb_out->r = (rgb_linear->r < 0.0f) ? 0.0f : (rgb_linear->r > 1.0f) ? 1.0f : rgb_linear->r;
        rgb_out->g = (rgb_linear->g < 0.0f) ? 0.0f : (rgb_linear->g > 1.0f) ? 1.0f : rgb_linear->g;
        rgb_out->b = (rgb_linear->b < 0.0f) ? 0.0f : (rgb_linear->b > 1.0f) ? 1.0f : rgb_linear->b;
        return ALWAN_OK;
    }

    /* Honour `space`: into the linear-sRGB working space for the Oklab model. */
    alwan_mat3x3_f32 to_srgb, from_srgb;
    {
        int xrc = gamut_working_xform_f32(&to_srgb, &from_srgb, space);
        if (xrc != ALWAN_OK) return xrc;
    }
    alwan_vec3_f32 rgb_work;
    alwan_mat3_mulv_f32(&rgb_work, &to_srgb, &rgb_vec);

    alwan_vec3_f32 oklab = gamut_linear_srgb_to_oklab_f32_v(rgb_work);
    alwan_f32 L = oklab.v[0];
    alwan_f32 a = oklab.v[1];
    alwan_f32 b = oklab.v[2];
    alwan_f32 C = sqrtf(a * a + b * b);

    if (C < 0.0001f ||
        (rgb_linear->r >= 0.0f && rgb_linear->r <= 1.0f &&
         rgb_linear->g >= 0.0f && rgb_linear->g <= 1.0f &&
         rgb_linear->b >= 0.0f && rgb_linear->b <= 1.0f)) {
        *rgb_out = *rgb_linear;
        return ALWAN_OK;
    }

    alwan_f32 a_norm = a / C;
    alwan_f32 b_norm = b / C;

    alwan_f32 L0, C0;
    alwan_f32 alpha = 0.0f;
    (void)alpha;  /* Mirrors f64: alpha selected per-method, presently unused. */

    if (method == ALWAN_GAMUT_MAP_ADAPTIVE_L0) {
        L0 = 0.5f; C0 = 0.0f; alpha = 0.05f;
    } else if (method == ALWAN_GAMUT_MAP_ADAPTIVE_CUSP) {
        alwan_vec2_f32 cusp = gamut_find_cusp_f32_v(a_norm, b_norm);
        L0 = cusp.v[0]; C0 = 0.0f; alpha = 0.05f;
    } else if (method == ALWAN_GAMUT_MAP_CHROMA_COMPRESS) {
        L0 = L; C0 = 0.0f; alpha = 0.1f;
    } else if (method == ALWAN_GAMUT_MAP_SGCK) {
        alwan_vec2_f32 cusp = gamut_find_cusp_f32_v(a_norm, b_norm);
        L0 = cusp.v[0]; C0 = 0.0f; alpha = 0.15f;
    } else if (method == ALWAN_GAMUT_MAP_HPMINDE) {
        alwan_vec2_f32 cusp = gamut_find_cusp_f32_v(a_norm, b_norm);
        L0 = cusp.v[0]; C0 = 0.0f; alpha = 0.02f;
    } else if (method == ALWAN_GAMUT_MAP_LIGHTNESS_PRESERVE) {
        L0 = L; C0 = 0.0f; alpha = 0.0f;
    } else {
        return ALWAN_E_INVALID;
    }

    alwan_f32 t = gamut_find_intersection_f32_v(a_norm, b_norm, L, C, L0, C0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    alwan_f32 L_clipped = L0 * (1.0f - t) + t * L;
    alwan_f32 C_clipped = t * C;

    alwan_vec3_f32 oklab_clipped;
    oklab_clipped.v[0] = L_clipped;
    oklab_clipped.v[1] = C_clipped * a_norm;
    oklab_clipped.v[2] = C_clipped * b_norm;

    /* Back to linear sRGB (working space), then to the caller's `space`. */
    alwan_vec3_f32 rgb_srgb = gamut_oklab_to_linear_srgb_f32_v(oklab_clipped);
    alwan_vec3_f32 rgb_result;
    alwan_mat3_mulv_f32(&rgb_result, &from_srgb, &rgb_srgb);

    rgb_out->r = (rgb_result.v[0] < 0.0f) ? 0.0f : (rgb_result.v[0] > 1.0f) ? 1.0f : rgb_result.v[0];
    rgb_out->g = (rgb_result.v[1] < 0.0f) ? 0.0f : (rgb_result.v[1] > 1.0f) ? 1.0f : rgb_result.v[1];
    rgb_out->b = (rgb_result.v[2] < 0.0f) ? 0.0f : (rgb_result.v[2] > 1.0f) ? 1.0f : rgb_result.v[2];
    return ALWAN_OK;
}

#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * HDR gamut mapping in ICtCp (PQ)
 *
 * Maps a linear BT.2020 RGB colour in ABSOLUTE nits into the display volume
 * [0, peak_nits]^3 by chroma reduction in ICtCp: intensity I is clamped to the
 * displayable range, hue angle atan2(Ct, Cp) is preserved by construction
 * (Ct and Cp are scaled jointly), and chroma is reduced by binary search until
 * the colour fits. CSS-Color-4-shaped, but in the BT.2100 HDR appearance space
 * with the dE-ITP (BT.2124) just-noticeable-difference early-out.
 * ---------------------------------------------------------------- */

#define ALWAN__HDR_ICTCP_JND      ALWAN_LITERAL(1.0)   /* dE-ITP ~= 1 is just noticeable */
#define ALWAN__HDR_ICTCP_ITERS    32

#if ALWAN_WITH_F64
static alwan_f64 alwan__de_itp_raw_f64(alwan_ictcp_f64 a, alwan_ictcp_f64 b) {
    /* BT.2124: dE = 720 * sqrt(dI^2 + 0.25*dCt^2 + dCp^2) */
    alwan_f64 dI = a.I - b.I;
    alwan_f64 dT = (a.Ct - b.Ct) * ALWAN_LITERAL(0.5);
    alwan_f64 dP = a.Cp - b.Cp;
    return ALWAN_LITERAL(720.0) * ALWAN_SQRT(dI * dI + dT * dT + dP * dP);
}

int alwan_hdr_gamut_map_ictcp_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_linear, alwan_f64 peak_nits) {
    if (!rgb_out || !rgb_linear) return ALWAN_E_INVALID;
    if (peak_nits < ALWAN_LITERAL(1.0) || peak_nits > ALWAN_LITERAL(10000.0)) return ALWAN_E_INVALID;

    alwan_f64 const eps = peak_nits * ALWAN_LITERAL(1e-9);

    /* Fast path: already inside the display volume -> bit-exact passthrough. */
    if (rgb_linear->r >= ALWAN_LITERAL(0.0) && rgb_linear->r <= peak_nits &&
        rgb_linear->g >= ALWAN_LITERAL(0.0) && rgb_linear->g <= peak_nits &&
        rgb_linear->b >= ALWAN_LITERAL(0.0) && rgb_linear->b <= peak_nits) {
        *rgb_out = *rgb_linear;
        return ALWAN_OK;
    }

    alwan_ictcp_f64 target;
    alwan_rgb_to_ictcp_f64(&target, rgb_linear, 1 /* PQ */);

    /* Clamp intensity to the displayable range: I of display black / white. */
    {
        alwan_rgb_f64 white; alwan_rgb_f64 black;
        alwan_ictcp_f64 i_white, i_black;
        white.r = peak_nits; white.g = peak_nits; white.b = peak_nits;
        black.r = ALWAN_LITERAL(0.0); black.g = ALWAN_LITERAL(0.0); black.b = ALWAN_LITERAL(0.0);
        alwan_rgb_to_ictcp_f64(&i_white, &white, 1);
        alwan_rgb_to_ictcp_f64(&i_black, &black, 1);
        if (target.I > i_white.I) target.I = i_white.I;
        if (target.I < i_black.I) target.I = i_black.I;
    }

    /* Local-clip shortcut: if the naive clip is within one JND of the target,
     * it is visually indistinguishable from any smarter mapping. */
    {
        alwan_rgb_f64 clip;
        alwan_ictcp_f64 clip_ictcp;
        clip.r = rgb_linear->r < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (rgb_linear->r > peak_nits ? peak_nits : rgb_linear->r);
        clip.g = rgb_linear->g < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (rgb_linear->g > peak_nits ? peak_nits : rgb_linear->g);
        clip.b = rgb_linear->b < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (rgb_linear->b > peak_nits ? peak_nits : rgb_linear->b);
        alwan_rgb_to_ictcp_f64(&clip_ictcp, &clip, 1);
        if (alwan__de_itp_raw_f64(clip_ictcp, target) < ALWAN__HDR_ICTCP_JND) {
            *rgb_out = clip;
            return ALWAN_OK;
        }
    }

    /* Binary search on the joint chroma scale s in [0,1]: (I, s*Ct, s*Cp).
     * s = 0 is the achromatic colour at I (inside the volume by construction),
     * so the search always converges to the boundary along the hue leaf. */
    {
        alwan_rgb_f64 best;
        alwan_ictcp_f64 achro = target;
        alwan_f64 lo = ALWAN_LITERAL(0.0), hi = ALWAN_LITERAL(1.0);
        int it;
        achro.Ct = ALWAN_LITERAL(0.0); achro.Cp = ALWAN_LITERAL(0.0);
        alwan_ictcp_to_rgb_f64(&best, &achro, 1);
        for (it = 0; it < ALWAN__HDR_ICTCP_ITERS; it++) {
            alwan_f64 s = (lo + hi) * ALWAN_LITERAL(0.5);
            alwan_ictcp_f64 cand = target;
            alwan_rgb_f64 cand_rgb;
            cand.Ct = target.Ct * s; cand.Cp = target.Cp * s;
            alwan_ictcp_to_rgb_f64(&cand_rgb, &cand, 1);
            if (cand_rgb.r >= -eps && cand_rgb.r <= peak_nits + eps &&
                cand_rgb.g >= -eps && cand_rgb.g <= peak_nits + eps &&
                cand_rgb.b >= -eps && cand_rgb.b <= peak_nits + eps) {
                lo = s; best = cand_rgb;
            } else {
                hi = s;
            }
        }

        /* Final guarantee: the converged candidate can overhang the boundary by
         * at most the search tolerance -- clip it (documented, not silent). */
        rgb_out->r = best.r < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (best.r > peak_nits ? peak_nits : best.r);
        rgb_out->g = best.g < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (best.g > peak_nits ? peak_nits : best.g);
        rgb_out->b = best.b < ALWAN_LITERAL(0.0) ? ALWAN_LITERAL(0.0) : (best.b > peak_nits ? peak_nits : best.b);
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
static alwan_f32 alwan__de_itp_raw_f32(alwan_ictcp_f32 a, alwan_ictcp_f32 b) {
    alwan_f32 dI = a.I - b.I;
    alwan_f32 dT = (a.Ct - b.Ct) * 0.5f;
    alwan_f32 dP = a.Cp - b.Cp;
    return 720.0f * ALWAN_SQRT_F32(dI * dI + dT * dT + dP * dP);
}

int alwan_hdr_gamut_map_ictcp_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_linear, alwan_f32 peak_nits) {
    if (!rgb_out || !rgb_linear) return ALWAN_E_INVALID;
    if (peak_nits < 1.0f || peak_nits > 10000.0f) return ALWAN_E_INVALID;

    {
        alwan_f32 const eps = peak_nits * 1e-6f;
        alwan_ictcp_f32 target;

        if (rgb_linear->r >= 0.0f && rgb_linear->r <= peak_nits &&
            rgb_linear->g >= 0.0f && rgb_linear->g <= peak_nits &&
            rgb_linear->b >= 0.0f && rgb_linear->b <= peak_nits) {
            *rgb_out = *rgb_linear;
            return ALWAN_OK;
        }

        alwan_rgb_to_ictcp_f32(&target, rgb_linear, 1);

        {
            alwan_rgb_f32 white; alwan_rgb_f32 black;
            alwan_ictcp_f32 i_white, i_black;
            white.r = peak_nits; white.g = peak_nits; white.b = peak_nits;
            black.r = 0.0f; black.g = 0.0f; black.b = 0.0f;
            alwan_rgb_to_ictcp_f32(&i_white, &white, 1);
            alwan_rgb_to_ictcp_f32(&i_black, &black, 1);
            if (target.I > i_white.I) target.I = i_white.I;
            if (target.I < i_black.I) target.I = i_black.I;
        }

        {
            alwan_rgb_f32 clip;
            alwan_ictcp_f32 clip_ictcp;
            clip.r = rgb_linear->r < 0.0f ? 0.0f : (rgb_linear->r > peak_nits ? peak_nits : rgb_linear->r);
            clip.g = rgb_linear->g < 0.0f ? 0.0f : (rgb_linear->g > peak_nits ? peak_nits : rgb_linear->g);
            clip.b = rgb_linear->b < 0.0f ? 0.0f : (rgb_linear->b > peak_nits ? peak_nits : rgb_linear->b);
            alwan_rgb_to_ictcp_f32(&clip_ictcp, &clip, 1);
            if (alwan__de_itp_raw_f32(clip_ictcp, target) < 1.0f) {
                *rgb_out = clip;
                return ALWAN_OK;
            }
        }

        {
            alwan_rgb_f32 best;
            alwan_ictcp_f32 achro = target;
            alwan_f32 lo = 0.0f, hi = 1.0f;
            int it;
            achro.Ct = 0.0f; achro.Cp = 0.0f;
            alwan_ictcp_to_rgb_f32(&best, &achro, 1);
            for (it = 0; it < ALWAN__HDR_ICTCP_ITERS; it++) {
                alwan_f32 s = (lo + hi) * 0.5f;
                alwan_ictcp_f32 cand = target;
                alwan_rgb_f32 cand_rgb;
                cand.Ct = target.Ct * s; cand.Cp = target.Cp * s;
                alwan_ictcp_to_rgb_f32(&cand_rgb, &cand, 1);
                if (cand_rgb.r >= -eps && cand_rgb.r <= peak_nits + eps &&
                    cand_rgb.g >= -eps && cand_rgb.g <= peak_nits + eps &&
                    cand_rgb.b >= -eps && cand_rgb.b <= peak_nits + eps) {
                    lo = s; best = cand_rgb;
                } else {
                    hi = s;
                }
            }
            rgb_out->r = best.r < 0.0f ? 0.0f : (best.r > peak_nits ? peak_nits : best.r);
            rgb_out->g = best.g < 0.0f ? 0.0f : (best.g > peak_nits ? peak_nits : best.g);
            rgb_out->b = best.b < 0.0f ? 0.0f : (best.b > peak_nits ? peak_nits : best.b);
        }
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */

/* ----------------------------------------------------------------
 * CSS Color 4 gamut mapping parameterized by target space
 *
 * Same algorithm as alwan_css_gamut_* (Oklch chroma reduction with the
 * deltaEOK JND clip check) but the destination gamut is the TARGET space's
 * unit cube instead of hard-wired sRGB. The Oklab pivot stays defined over
 * linear sRGB (Ottosson's fit); candidates are converted linear-sRGB <->
 * linear-target with matrices composed from the space descriptors, and the
 * in-gamut test / clip run in the TARGET cube. Input and output are LINEAR
 * target-space RGB. Assumes a D65-white target (P3, Rec.2020, ...); no
 * chromatic adaptation is applied between sRGB and the target.
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
int alwan_css_gamut_space_f64(alwan_rgb_f64 *rgb_out,
                              alwan_rgb_space_desc_f64 const *target_space,
                              alwan_rgb_f64 const *rgb_in) {
    if (!rgb_out || !target_space || !rgb_in) return ALWAN_E_INVALID;

    alwan_f64 const JND = ALWAN_LITERAL(0.02);
    int const MAX_ITER = 30;

    /* Compose linear target <-> linear sRGB matrices via XYZ. */
    alwan_mat3x3_f64 t_to_xyz, xyz_to_t, s_to_xyz, xyz_to_s, t2s, s2t;
    alwan_rgb_space_desc_f64 srgb;
    int st = alwan_rgb_get_space_descriptor_f64(&srgb, ALWAN_RGB_SPACE_SRGB, NULL);
    if (st != ALWAN_OK) return st;
    st = alwan_rgb_derive_matrices_f64(&t_to_xyz, &xyz_to_t, target_space);
    if (st != ALWAN_OK) return st;
    st = alwan_rgb_derive_matrices_f64(&s_to_xyz, &xyz_to_s, &srgb);
    if (st != ALWAN_OK) return st;
    alwan_mat3_mul_f64(&t2s, &xyz_to_s, &t_to_xyz);
    alwan_mat3_mul_f64(&s2t, &xyz_to_t, &s_to_xyz);

    alwan_vec3_f64 t_in = {{rgb_in->r, rgb_in->g, rgb_in->b}};

    /* Already inside the target cube -> passthrough. */
    if (t_in.v[0] >= ALWAN_LITERAL(0.0) && t_in.v[0] <= ALWAN_LITERAL(1.0) &&
        t_in.v[1] >= ALWAN_LITERAL(0.0) && t_in.v[1] <= ALWAN_LITERAL(1.0) &&
        t_in.v[2] >= ALWAN_LITERAL(0.0) && t_in.v[2] <= ALWAN_LITERAL(1.0)) {
        *rgb_out = *rgb_in;
        return ALWAN_OK;
    }

    /* Work in Oklab via the linear-sRGB expression of the colour. */
    alwan_vec3_f64 s_in;
    alwan_mat3_mulv_f64(&s_in, &t2s, &t_in);
    alwan_vec3_f64 oklab_v = gamut_linear_srgb_to_oklab_f64_v(s_in);
    alwan_oklab_f64 ok; ok.L = oklab_v.v[0]; ok.a = oklab_v.v[1]; ok.b = oklab_v.v[2];
    alwan_oklch_f64 lch = alwan_oklab_to_oklch_f64_v(ok);

    if (lch.L >= ALWAN_LITERAL(1.0)) { rgb_out->r = rgb_out->g = rgb_out->b = ALWAN_LITERAL(1.0); return ALWAN_OK; }
    if (lch.L <= ALWAN_LITERAL(0.0)) { rgb_out->r = rgb_out->g = rgb_out->b = ALWAN_LITERAL(0.0); return ALWAN_OK; }

    {
        alwan_f64 lo = ALWAN_LITERAL(0.0), hi = lch.C;
        alwan_oklch_f64 trial = lch;
        alwan_vec3_f64 trial_t = t_in;
        int i;
        for (i = 0; i < MAX_ITER; i++) {
            trial.C = (lo + hi) * ALWAN_LITERAL(0.5);
            alwan_oklab_f64 trial_ok = alwan_oklch_to_oklab_f64_v(trial);
            alwan_vec3_f64 trial_okv = {{trial_ok.L, trial_ok.a, trial_ok.b}};
            alwan_vec3_f64 trial_s = gamut_oklab_to_linear_srgb_f64_v(trial_okv);
            alwan_mat3_mulv_f64(&trial_t, &s2t, &trial_s);

            /* Clip in the TARGET cube, express the clipped colour in Oklab. */
            alwan_vec3_f64 clip_t = trial_t;
            int in_cube = 1;
            for (int c = 0; c < 3; c++) {
                if (clip_t.v[c] < ALWAN_LITERAL(0.0)) { clip_t.v[c] = ALWAN_LITERAL(0.0); in_cube = 0; }
                else if (clip_t.v[c] > ALWAN_LITERAL(1.0)) { clip_t.v[c] = ALWAN_LITERAL(1.0); in_cube = 0; }
            }
            alwan_vec3_f64 clip_s;
            alwan_mat3_mulv_f64(&clip_s, &t2s, &clip_t);
            alwan_vec3_f64 clip_okv = gamut_linear_srgb_to_oklab_f64_v(clip_s);
            alwan_oklab_f64 clip_ok; clip_ok.L = clip_okv.v[0]; clip_ok.a = clip_okv.v[1]; clip_ok.b = clip_okv.v[2];

            alwan_f64 de = alwan_delta_e_ok_f64_v(trial_ok, clip_ok);
            if (de < JND) {
                if (in_cube) break;
                hi = trial.C;
            } else {
                hi = trial.C;
            }
            if (hi - lo < ALWAN_LITERAL(1e-12)) break;
        }

        /* Final clip in the target cube (guarantee, mirrors gamut_css_map). */
        for (int c = 0; c < 3; c++) {
            if (trial_t.v[c] < ALWAN_LITERAL(0.0)) trial_t.v[c] = ALWAN_LITERAL(0.0);
            else if (trial_t.v[c] > ALWAN_LITERAL(1.0)) trial_t.v[c] = ALWAN_LITERAL(1.0);
        }
        rgb_out->r = trial_t.v[0]; rgb_out->g = trial_t.v[1]; rgb_out->b = trial_t.v[2];
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
int alwan_css_gamut_space_f32(alwan_rgb_f32 *rgb_out,
                              alwan_rgb_space_desc_f32 const *target_space,
                              alwan_rgb_f32 const *rgb_in) {
    /* Single-precision entry widens to the f64 worker (matrix composition and
     * the Oklab pivot are precision-sensitive; matches the _custom_f32 ACES
     * precedent for setup-heavy scalar paths). */
    if (!rgb_out || !target_space || !rgb_in) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 t64;
    alwan_rgb_f64 in64, out64;
    for (int k = 0; k < 6; k++) t64.primaries_xy[k] = target_space->primaries_xy[k];
    t64.white_xy[0] = target_space->white_xy[0]; t64.white_xy[1] = target_space->white_xy[1];
    t64.oetf = target_space->oetf;
    t64.eotf = target_space->eotf;
    t64.has_matrices = 0;
    in64.r = rgb_in->r; in64.g = rgb_in->g; in64.b = rgb_in->b;
    {
        int st = alwan_css_gamut_space_f64(&out64, &t64, &in64);
        if (st != ALWAN_OK) return st;
    }
    rgb_out->r = (alwan_f32)out64.r; rgb_out->g = (alwan_f32)out64.g; rgb_out->b = (alwan_f32)out64.b;
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
