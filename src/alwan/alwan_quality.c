/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * M10: Light Quality & CCT
 * CCT estimation (McCamy, Robertson) and CRI
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <math.h>

/* ----------------------------------------------------------------
 * CCT: Correlated Color Temperature
 * ---------------------------------------------------------------- */

/* McCamy's approximation for CCT from CIE 1931 xy coordinates
 * Fast approximation, ~2% accuracy above 2800K
 * Formula: CCT = 437n³ + 3601n² + 6861n + 5517
 * where n = (x - 0.3320) / (0.1858 - y)
 */
alwan_scalar alwan_cct_mccamy_xy(alwan_vec3 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar x = xy->v[0];
    alwan_scalar y = xy->v[1];

    /* Epicenter of the Planckian locus in CIE xy */
    alwan_scalar const x_e = ALWAN_LITERAL(0.3320);
    alwan_scalar const y_e = ALWAN_LITERAL(0.1858);

    /* Avoid division by zero */
    alwan_scalar denom = y_e - y;
    if (ALWAN_FABS(denom) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar n = (x - x_e) / denom;
    alwan_scalar n2 = n * n;
    alwan_scalar n3 = n2 * n;

    /* McCamy's formula: CCT = 437n³ + 3601n² + 6861n + 5517 */
    alwan_scalar cct = ALWAN_LITERAL(437.0) * n3 +
                       ALWAN_LITERAL(3601.0) * n2 +
                       ALWAN_LITERAL(6861.0) * n +
                       ALWAN_LITERAL(5517.0);

    return cct;
}

/* Robertson's method for CCT from CIE 1960 UCS uv coordinates
 * Uses lookup table of Planckian locus points
 * More accurate than McCamy, especially at low CCT
 */

/* Robertson lookup table: CCT, u, v, du/dT, dv/dT */
/* Derived from Planckian locus in CIE 1960 UCS, 1000K to 20000K */
typedef struct {
    alwan_scalar cct;   /* Temperature in Kelvin */
    alwan_scalar u;     /* CIE 1960 u coordinate */
    alwan_scalar v;     /* CIE 1960 v coordinate */
    alwan_scalar du;    /* du/dT slope */
    alwan_scalar dv;    /* dv/dT slope */
} robertson_locus_point;

// TODO: move that table to a data file like
// all other data (update the script generate_data.ps1)
static const robertson_locus_point robertson_table[] = {
    /* CCT,    u,        v,        du,         dv */
    { 1000,  0.05042,  0.52720,  0.00019551,  0.00029206},
    { 1500,  0.18352,  0.28532,  0.00023751,  0.00091385},
    { 2000,  0.26249,  0.35149,  0.00005908,  0.00001341},
    { 2500,  0.26491,  0.35200, -0.00002500, -0.00000510},
    { 3000,  0.24925,  0.34805, -0.00003213, -0.00000993},
    { 3500,  0.23468,  0.34254, -0.00002563, -0.00001169},
    { 4000,  0.22358,  0.33661, -0.00001908, -0.00001184},
    { 4500,  0.21533,  0.33083, -0.00001420, -0.00001122},
    { 5000,  0.20914,  0.32544, -0.00001075, -0.00001028},
    { 5500,  0.20442,  0.32056, -0.00000830, -0.00000926},
    { 6000,  0.20074,  0.31618, -0.00000653, -0.00000826},
    { 6500,  0.19782,  0.31229, -0.00000523, -0.00000734},
    { 7000,  0.19546,  0.30883, -0.00000426, -0.00000653},
    { 7500,  0.19352,  0.30574, -0.00000352, -0.00000581},
    { 8000,  0.19191,  0.30300, -0.00000294, -0.00000517},
    { 8500,  0.19056,  0.30056, -0.00000249, -0.00000463},
    { 9000,  0.18940,  0.29837, -0.00000213, -0.00000415},
    { 9500,  0.18841,  0.29640, -0.00000184, -0.00000374},
    {10000,  0.18755,  0.29462, -0.00000160, -0.00000338},
    {12000,  0.18503,  0.28899, -0.00000099, -0.00000233},
    {15000,  0.18280,  0.28346, -0.00000056, -0.00000146},
    {20000,  0.18085,  0.27812, -0.00000027, -0.00000078}
};

#define ROBERTSON_TABLE_SIZE (sizeof(robertson_table) / sizeof(robertson_table[0]))

alwan_scalar alwan_cct_robertson_xy(alwan_vec3 const *xy) {
    if (!xy) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar x = xy->v[0];
    alwan_scalar y = xy->v[1];

    /* Convert CIE 1931 xy to CIE 1960 UCS uv */
    alwan_scalar denom = ALWAN_LITERAL(12.0) * y - ALWAN_LITERAL(2.0) * x + ALWAN_LITERAL(3.0);
    if (ALWAN_FABS(denom) < ALWAN_EPSILON) {
        return ALWAN_LITERAL(-1.0);
    }

    alwan_scalar u = (ALWAN_LITERAL(4.0) * x) / denom;
    alwan_scalar v = (ALWAN_LITERAL(6.0) * y) / denom;

    /* Find closest point on the Planckian locus by checking all segments */
    alwan_scalar min_dist = ALWAN_LITERAL(1e10);
    size_t best_idx = 0;
    alwan_scalar best_t = ALWAN_LITERAL(0.0);

    for (size_t i = 0; i < ROBERTSON_TABLE_SIZE - 1; i++) {
        robertson_locus_point const *p1 = &robertson_table[i];
        robertson_locus_point const *p2 = &robertson_table[i + 1];

        /* Vector from p1 to p2 */
        alwan_scalar du_seg = p2->u - p1->u;
        alwan_scalar dv_seg = p2->v - p1->v;

        /* Vector from p1 to test point */
        alwan_scalar du_test = u - p1->u;
        alwan_scalar dv_test = v - p1->v;

        alwan_scalar seg_len_sq = du_seg * du_seg + dv_seg * dv_seg;
        if (seg_len_sq < ALWAN_EPSILON) {
            continue;
        }

        /* Project test point onto line segment */
        alwan_scalar t = (du_test * du_seg + dv_test * dv_seg) / seg_len_sq;
        t = alwan_clamp(t, ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0));

        /* Find the closest point on the segment */
        alwan_scalar u_proj = p1->u + t * du_seg;
        alwan_scalar v_proj = p1->v + t * dv_seg;

        /* Calculate actual distance to this point */
        alwan_scalar du_diff = u - u_proj;
        alwan_scalar dv_diff = v - v_proj;
        alwan_scalar dist = ALWAN_SQRT(du_diff * du_diff + dv_diff * dv_diff);

        if (dist < min_dist) {
            min_dist = dist;
            best_idx = i;
            best_t = t;
        }
    }

    /* Interpolate CCT using the best segment and parameter */
    robertson_locus_point const *p1 = &robertson_table[best_idx];
    robertson_locus_point const *p2 = &robertson_table[best_idx + 1];

    /* Linear interpolation of CCT */
    alwan_scalar cct = p1->cct + best_t * (p2->cct - p1->cct);

    return cct;
}

/* ----------------------------------------------------------------
 * CRI: Color Rendering Index
 * ---------------------------------------------------------------- */

/* CRI Ra calculation requires:
 * 1. Test SPD
 * 2. Reference illuminant (blackbody or daylight at same CCT)
 * 3. 8 TCS (Test Color Samples) reflectances
 * 4. CIE 1931 2° observer
 *
 * This is a complex calculation that requires spectral integration.
 * For now, return a placeholder that indicates this feature is not yet implemented.
 */
// TODO
alwan_scalar alwan_cri_ra(alwan_spd const *test_spd) {
    if (!test_spd) {
        return ALWAN_LITERAL(-1.0);
    }

    /* CRI calculation requires:
     * - Reference illuminant generation at matching CCT
     * - TCS reflectance data
     * - Chromatic adaptation
     * - Special CRI color space (U*V*W*)
     * This is beyond the scope of initial M10 implementation.
     */
    return ALWAN_LITERAL(-2.0);  /* Not yet implemented */
}
