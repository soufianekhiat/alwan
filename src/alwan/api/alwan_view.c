/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * View Transforms (Display Rendering)
 * Per-pixel math in alwan_view_core.h
 *
 * Only matrix data loading, enum dispatch, and bulk loops live here.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_view_core.h"
#include "../core/alwan_hdr_core.h"
#include <string.h>

/* ----------------------------------------------------------------
 * ACES RRT+ODT (Simplified Approximation for Rec.709)
 * ---------------------------------------------------------------- */

/* Simplified ACES RRT+ODT for Rec.709 display
 * This is a math-based approximation, not the full LUT-based reference implementation
 * Input: ACEScg (AP1) linear
 * Output: Rec.709 linear (before display EOTF) */
static void aces_rec709_transform(alwan_scalar const *rgb_in, alwan_scalar *rgb_out) {
    /* Input is ACEScg (AP1), convert to ACES2065-1 (AP0) */
    /* AP1 to AP0 matrix (generated from colour-science) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ap1_to_ap0[9] = {
#include "../data/matrices/aces_ap1_to_ap0.csv"
    };
    ALWAN_DIAG_POP

    /* Transform to AP0 */
    alwan_scalar ap0_r = ap1_to_ap0[0] * rgb_in[0] + ap1_to_ap0[1] * rgb_in[1] + ap1_to_ap0[2] * rgb_in[2];
    alwan_scalar ap0_g = ap1_to_ap0[3] * rgb_in[0] + ap1_to_ap0[4] * rgb_in[1] + ap1_to_ap0[5] * rgb_in[2];
    alwan_scalar ap0_b = ap1_to_ap0[6] * rgb_in[0] + ap1_to_ap0[7] * rgb_in[1] + ap1_to_ap0[8] * rgb_in[2];

    /* Apply simplified RRT tone curve */
    alwan_scalar rrt_r = alwan_aces_tonemap_v(ap0_r);
    alwan_scalar rrt_g = alwan_aces_tonemap_v(ap0_g);
    alwan_scalar rrt_b = alwan_aces_tonemap_v(ap0_b);

    /* ODT: Convert from ACES to Rec.709
     * Simplified matrix (AP0 to Rec.709 with Bradford chromatic adaptation D60->D65)
     * Generated from colour-science */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const odt_matrix[9] = {
#include "../data/matrices/aces_odt_rec709.csv"
    };
    ALWAN_DIAG_POP

    rgb_out[0] = odt_matrix[0] * rrt_r + odt_matrix[1] * rrt_g + odt_matrix[2] * rrt_b;
    rgb_out[1] = odt_matrix[3] * rrt_r + odt_matrix[4] * rrt_g + odt_matrix[5] * rrt_b;
    rgb_out[2] = odt_matrix[6] * rrt_r + odt_matrix[7] * rrt_g + odt_matrix[8] * rrt_b;

    /* Clamp to [0,1] */
    rgb_out[0] = alwan_saturate(rgb_out[0]);
    rgb_out[1] = alwan_saturate(rgb_out[1]);
    rgb_out[2] = alwan_saturate(rgb_out[2]);
}

/* ----------------------------------------------------------------
 * AgX View Transform (Full Pipeline with Inset/Outset Matrices)
 * ---------------------------------------------------------------- */

/* AgX inset matrix: BT.709 -> AgX log-encoding space */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const agx_inset_matrix[9] = {
#include "../data/matrices/agx_inset.csv"
};
static alwan_scalar const agx_outset_matrix[9] = {
#include "../data/matrices/agx_outset.csv"
};
ALWAN_DIAG_POP

/* AgX Base Transform
 * Full pipeline: inset matrix -> log encode -> sigmoid curve -> outset matrix
 * Input: Linear RGB (typically BT.709/sRGB primaries)
 * Output: Display-ready RGB [0,1] */
static void agx_base_transform(alwan_scalar const *rgb_in, alwan_scalar *rgb_out) {
    /* Apply inset matrix */
    alwan_scalar inset_r = agx_inset_matrix[0] * rgb_in[0] + agx_inset_matrix[1] * rgb_in[1] + agx_inset_matrix[2] * rgb_in[2];
    alwan_scalar inset_g = agx_inset_matrix[3] * rgb_in[0] + agx_inset_matrix[4] * rgb_in[1] + agx_inset_matrix[5] * rgb_in[2];
    alwan_scalar inset_b = agx_inset_matrix[6] * rgb_in[0] + agx_inset_matrix[7] * rgb_in[1] + agx_inset_matrix[8] * rgb_in[2];

    /* AgX log-encode + curve */
    alwan_scalar const agx_min = ALWAN_LITERAL(-12.47393);
    alwan_scalar const agx_max = ALWAN_LITERAL(  4.026069);

    alwan_scalar curve_r = alwan_agx_curve_v(alwan_agx_log_encode_v(inset_r, agx_min, agx_max));
    alwan_scalar curve_g = alwan_agx_curve_v(alwan_agx_log_encode_v(inset_g, agx_min, agx_max));
    alwan_scalar curve_b = alwan_agx_curve_v(alwan_agx_log_encode_v(inset_b, agx_min, agx_max));

    /* Apply outset matrix */
    rgb_out[0] = alwan_saturate(agx_outset_matrix[0] * curve_r + agx_outset_matrix[1] * curve_g + agx_outset_matrix[2] * curve_b);
    rgb_out[1] = alwan_saturate(agx_outset_matrix[3] * curve_r + agx_outset_matrix[4] * curve_g + agx_outset_matrix[5] * curve_b);
    rgb_out[2] = alwan_saturate(agx_outset_matrix[6] * curve_r + agx_outset_matrix[7] * curve_g + agx_outset_matrix[8] * curve_b);
}

/* AgX Punchy Variant
 * Higher contrast version with more saturated mids */
static void agx_punchy_transform(alwan_scalar const *rgb_in, alwan_scalar *rgb_out) {
    /* First apply base AgX */
    agx_base_transform(rgb_in, rgb_out);

    /* Apply punchy grade */
    alwan_vec3 base;
    base.v[0] = rgb_out[0]; base.v[1] = rgb_out[1]; base.v[2] = rgb_out[2];
    alwan_vec3 graded = alwan_agx_punchy_grade_v(base);
    rgb_out[0] = graded.v[0]; rgb_out[1] = graded.v[1]; rgb_out[2] = graded.v[2];
}

/* AgX Golden Variant
 * Warm highlights, cool shadows -- cinematic look */
static void agx_golden_transform(alwan_scalar const *rgb_in, alwan_scalar *rgb_out) {
    /* First apply base AgX */
    agx_base_transform(rgb_in, rgb_out);

    /* Apply golden grade */
    alwan_vec3 base;
    base.v[0] = rgb_out[0]; base.v[1] = rgb_out[1]; base.v[2] = rgb_out[2];
    alwan_vec3 graded = alwan_agx_golden_grade_v(base);
    rgb_out[0] = graded.v[0]; rgb_out[1] = graded.v[1]; rgb_out[2] = graded.v[2];
}

/* ----------------------------------------------------------------
 * BT.2446 Method A HDR<->SDR
 * ---------------------------------------------------------------- */

static void bt2446a_hdr_to_sdr_transform(alwan_scalar const *rgb_in, alwan_scalar *rgb_out) {
    /* Operates on luminance channel after Yxy decomposition */
    alwan_scalar L_hdr = ALWAN_LITERAL(1000.0);
    alwan_scalar L_sdr = ALWAN_LITERAL(100.0);

    for (int i = 0; i < 3; i++) {
        rgb_out[i] = alwan_bt2446a_forward_v(rgb_in[i], L_hdr, L_sdr);
    }
}

static void bt2446a_sdr_to_hdr_transform(alwan_scalar const *rgb_in, alwan_scalar *rgb_out) {
    alwan_scalar L_hdr = ALWAN_LITERAL(1000.0);
    alwan_scalar L_sdr = ALWAN_LITERAL(100.0);

    for (int i = 0; i < 3; i++) {
        rgb_out[i] = alwan_bt2446a_inverse_v(rgb_in[i], L_hdr, L_sdr);
    }
}

/* ----------------------------------------------------------------
 * Khronos PBR Neutral Tone Mapping
 * ---------------------------------------------------------------- */

static void khronos_pbr_neutral_transform(alwan_scalar const *rgb_in, alwan_scalar *rgb_out) {
    alwan_vec3 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    alwan_vec3 out_v = alwan_khronos_pbr_neutral_v(in_v);
    rgb_out[0] = out_v.v[0];
    rgb_out[1] = out_v.v[1];
    rgb_out[2] = out_v.v[2];
}

/* ----------------------------------------------------------------
 * View Transform API
 * ---------------------------------------------------------------- */

int alwan_view_transform_apply(alwan_scalar *rgb_out,
                                alwan_ctx *ctx,
                                alwan_view_transform vt,
                                alwan_scalar const *rgb_in, size_t count, size_t in_stride,
                                size_t out_stride) {
    (void)ctx;  /* Unused for stateless transforms */

    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Select view transform */
    void (*transform_fn)(alwan_scalar const *, alwan_scalar *) = NULL;

    switch (vt) {
        case ALWAN_VIEW_ACES_REC709:
            transform_fn = aces_rec709_transform;
            break;
        case ALWAN_VIEW_AGX:
            transform_fn = agx_base_transform;
            break;
        case ALWAN_VIEW_AGX_PUNCHY:
            transform_fn = agx_punchy_transform;
            break;
        case ALWAN_VIEW_AGX_GOLDEN:
            transform_fn = agx_golden_transform;
            break;
        case ALWAN_VIEW_BT2446A_HDR_TO_SDR:
            transform_fn = bt2446a_hdr_to_sdr_transform;
            break;
        case ALWAN_VIEW_BT2446A_SDR_TO_HDR:
            transform_fn = bt2446a_sdr_to_hdr_transform;
            break;
        case ALWAN_VIEW_KHRONOS_PBR_NEUTRAL:
            transform_fn = khronos_pbr_neutral_transform;
            break;
        default:
            return ALWAN_E_INVALID;
    }

    /* Apply view transform to RGB triplets (strides are in bytes) */
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);

        transform_fn(in_ptr, out_ptr);
    }

    return ALWAN_OK;
}
