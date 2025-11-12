/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Alwan Contributors
 * SPDX-License-Identifier: MIT
 *
 * View Transforms (Display Rendering)
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <string.h>

/* ----------------------------------------------------------------
 * ACES RRT+ODT (Simplified Approximation for Rec.709)
 * ---------------------------------------------------------------- */

/* Simplified ACES RRT+ODT for Rec.709 display
 * This is a math-based approximation, not the full LUT-based reference implementation
 * Input: ACEScg (AP1) linear
 * Output: Rec.709 linear (before display EOTF) */
static void aces_rec709_transform(Scalar const *rgb_in, Scalar *rgb_out) {
    /* Input is ACEScg (AP1), convert to ACES2065-1 (AP0) */
    /* AP1 to AP0 matrix (generated from colour-science) */
    static Scalar const ap1_to_ap0[9] = {
#include "../../data/matrices/aces_ap1_to_ap0.csv"
    };

    /* Transform to AP0 */
    Scalar ap0_r = ap1_to_ap0[0] * rgb_in[0] + ap1_to_ap0[1] * rgb_in[1] + ap1_to_ap0[2] * rgb_in[2];
    Scalar ap0_g = ap1_to_ap0[3] * rgb_in[0] + ap1_to_ap0[4] * rgb_in[1] + ap1_to_ap0[5] * rgb_in[2];
    Scalar ap0_b = ap1_to_ap0[6] * rgb_in[0] + ap1_to_ap0[7] * rgb_in[1] + ap1_to_ap0[8] * rgb_in[2];

    /* Apply simplified RRT tone curve (approximation of full RRT)
     * Using a simplified S-curve that mimics ACES behavior */
    Scalar const a = ALWAN_LITERAL(2.51);
    Scalar const b = ALWAN_LITERAL(0.03);
    Scalar const c = ALWAN_LITERAL(2.43);
    Scalar const d = ALWAN_LITERAL(0.59);
    Scalar const e = ALWAN_LITERAL(0.14);

    /* Tone mapping formula (per-channel) */
    #define ACES_TONEMAP(x) (((x) * (a * (x) + b)) / ((x) * (c * (x) + d) + e))

    Scalar rrt_r = ACES_TONEMAP(ap0_r);
    Scalar rrt_g = ACES_TONEMAP(ap0_g);
    Scalar rrt_b = ACES_TONEMAP(ap0_b);

    #undef ACES_TONEMAP

    /* ODT: Convert from ACES to Rec.709
     * Simplified matrix (AP0 to Rec.709 with Bradford chromatic adaptation D60→D65)
     * Generated from colour-science */
    static Scalar const odt_matrix[9] = {
#include "../../data/matrices/aces_odt_rec709.csv"
    };

    rgb_out[0] = odt_matrix[0] * rrt_r + odt_matrix[1] * rrt_g + odt_matrix[2] * rrt_b;
    rgb_out[1] = odt_matrix[3] * rrt_r + odt_matrix[4] * rrt_g + odt_matrix[5] * rrt_b;
    rgb_out[2] = odt_matrix[6] * rrt_r + odt_matrix[7] * rrt_g + odt_matrix[8] * rrt_b;

    /* Clamp to [0,1] */
    rgb_out[0] = alwan_saturate(rgb_out[0]);
    rgb_out[1] = alwan_saturate(rgb_out[1]);
    rgb_out[2] = alwan_saturate(rgb_out[2]);
}

/* ----------------------------------------------------------------
 * AgX View Transform
 * ---------------------------------------------------------------- */

/* AgX Base Transform
 * A modern film emulation curve with good highlight rolloff
 * Input: Linear RGB (typically BT.709/sRGB primaries)
 * Output: Display-ready RGB [0,1] */
static void agx_base_transform(Scalar const *rgb_in, Scalar *rgb_out) {
    /* AgX uses a log-based encoding similar to Rec.1886 but with better highlights */

    /* Punchy grade parameters (base variant) */
    Scalar const agx_min = ALWAN_LITERAL(-12.47393);  /* Minimum EV */
    Scalar const agx_max = ALWAN_LITERAL(  4.026069); /* Maximum EV */

    /* Process each channel */
    for (int i = 0; i < 3; i++) {
        Scalar x = rgb_in[i];

        /* Protect against log(0) */
        if (x < ALWAN_LITERAL(1e-10)) {
            rgb_out[i] = ALWAN_LITERAL(0.0);
            continue;
        }

        /* Convert to log2 (exposure value) */
        Scalar log_x = ALWAN_LOG(x) / ALWAN_LOG(ALWAN_LITERAL(2.0));

        /* Normalize to [0,1] based on AgX range */
        Scalar normalized = (log_x - agx_min) / (agx_max - agx_min);
        normalized = alwan_saturate(normalized);

        /* Apply AgX curve (sigmoid-like function for smooth rolloff) */
        /* Simplified polynomial approximation of AgX Look */
        Scalar t = normalized;
        Scalar t2 = t * t;
        Scalar t3 = t2 * t;

        /* AgX curve coefficients (approximation) */
        Scalar result = ALWAN_LITERAL(-0.0653) * t3
                      + ALWAN_LITERAL( 1.2528) * t2
                      + ALWAN_LITERAL(-0.1865) * t;

        rgb_out[i] = alwan_saturate(result);
    }
}

/* AgX Punchy Variant
 * Higher contrast version with more saturated mids */
static void agx_punchy_transform(Scalar const *rgb_in, Scalar *rgb_out) {
    /* First apply base AgX */
    agx_base_transform(rgb_in, rgb_out);

    /* Apply punchy grade: increase contrast and saturation */
    Scalar const contrast = ALWAN_LITERAL(1.15);
    Scalar const saturation = ALWAN_LITERAL(1.2);

    /* Calculate luminance (Rec.709 weights) */
    Scalar luma = ALWAN_LITERAL(0.2126) * rgb_out[0]
                + ALWAN_LITERAL(0.7152) * rgb_out[1]
                + ALWAN_LITERAL(0.0722) * rgb_out[2];

    /* Apply contrast around mid-gray */
    Scalar const mid_gray = ALWAN_LITERAL(0.18);
    for (int i = 0; i < 3; i++) {
        /* Saturation boost */
        rgb_out[i] = luma + (rgb_out[i] - luma) * saturation;

        /* Contrast boost */
        rgb_out[i] = mid_gray + (rgb_out[i] - mid_gray) * contrast;

        /* Clamp */
        rgb_out[i] = alwan_saturate(rgb_out[i]);
    }
}

/* ----------------------------------------------------------------
 * View Transform API
 * ---------------------------------------------------------------- */

int alwan_view_transform_apply(alwan_ctx *ctx,
                                char const *name,
                                Scalar const *rgb_in, size_t count, size_t in_stride,
                                Scalar *rgb_out, size_t out_stride) {
    (void)ctx;  /* Unused for stateless transforms */

    if (!name || !rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Select view transform */
    void (*transform_fn)(Scalar const *, Scalar *) = NULL;

    if (strcmp(name, "aces_rec709") == 0) {
        transform_fn = aces_rec709_transform;
    } else if (strcmp(name, "agx") == 0) {
        transform_fn = agx_base_transform;
    } else if (strcmp(name, "agx_punchy") == 0) {
        transform_fn = agx_punchy_transform;
    } else {
        return ALWAN_E_INVALID;  /* Unknown view transform */
    }

    /* Apply view transform to RGB triplets */
    for (size_t i = 0; i < count; i++) {
        Scalar const *in_ptr = rgb_in + i * in_stride;
        Scalar *out_ptr = rgb_out + i * out_stride;

        transform_fn(in_ptr, out_ptr);
    }

    return ALWAN_OK;
}
