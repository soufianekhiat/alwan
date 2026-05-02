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

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_view_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_view_impl.inc"
#include "alwan_api_teardown.h"

/* ----------------------------------------------------------------
 * ACES RRT+ODT (Simplified Approximation for Rec.709)
 * ---------------------------------------------------------------- */

/* ACES 1.x RRT+ODT for sRGB/Rec.709 display
 * Full Academy pipeline: glow, red modifier, RRT desaturation,
 * segmented spline C5, ODT Y_to_linCV, ODT desaturation.
 * Input: ACEScg (AP1) linear
 * Output: sRGB linear (before display EOTF) */
static void aces_rec709_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    /* AP1 to AP0 (alwan_aces1_output_transform expects AP0) */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const ap1_to_ap0[9] = {
#include "../data/matrices/aces_ap1_to_ap0.csv"
    };
    ALWAN_DIAG_POP

    alwan_rgb_f64 ap0;
    ap0.r = ap1_to_ap0[0]*rgb_in[0] + ap1_to_ap0[1]*rgb_in[1] + ap1_to_ap0[2]*rgb_in[2];
    ap0.g = ap1_to_ap0[3]*rgb_in[0] + ap1_to_ap0[4]*rgb_in[1] + ap1_to_ap0[5]*rgb_in[2];
    ap0.b = ap1_to_ap0[6]*rgb_in[0] + ap1_to_ap0[7]*rgb_in[1] + ap1_to_ap0[8]*rgb_in[2];

    alwan_rgb_f64 out;
    alwan_aces1_output_transform_f64(&out, &ap0, ALWAN_ACES1_OUT_SRGB_100NIT);

    /* Output includes sRGB OETF — undo to get linear for the view transform chain */
    rgb_out[0] = alwan_srgb_eotf_f64(out.r);
    rgb_out[1] = alwan_srgb_eotf_f64(out.g);
    rgb_out[2] = alwan_srgb_eotf_f64(out.b);
}

/* ----------------------------------------------------------------
 * AgX View Transform (Full Pipeline with Inset/Outset Matrices)
 * ---------------------------------------------------------------- */

/* AgX inset matrix: BT.709 -> AgX log-encoding space */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const agx_inset_matrix[9] = {
#include "../data/matrices/agx_inset.csv"
};
static alwan_f64 const agx_outset_matrix[9] = {
#include "../data/matrices/agx_outset.csv"
};
ALWAN_DIAG_POP

/* AgX Base Transform
 * Full pipeline: inset matrix -> log encode -> sigmoid curve -> outset matrix
 * Input: Linear RGB (typically BT.709/sRGB primaries)
 * Output: Display-ready RGB [0,1] */
/* AgX Default Contrast LUT — from sobotka/AgX config.ocio */
#include "../data/agx_default_contrast_lut.h"

static alwan_f64 agx_lut_eval(alwan_f64 t) {
    /* Linear interpolation in the 4096-entry LUT */
    t = alwan_saturate(t);
    alwan_f64 fi = t * (AGX_LUT_SIZE - 1);
    int i0 = (int)fi;
    int i1 = i0 + 1;
    if (i1 >= AGX_LUT_SIZE) i1 = AGX_LUT_SIZE - 1;
    alwan_f64 frac = fi - i0;
    return agx_default_contrast_lut[i0] + frac * (agx_default_contrast_lut[i1] - agx_default_contrast_lut[i0]);
}

static void agx_base_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    /* Clamp negatives to 0 (matches OCIO RangeTransform before inset) */
    alwan_f64 r_in = rgb_in[0] > 0 ? rgb_in[0] : 0;
    alwan_f64 g_in = rgb_in[1] > 0 ? rgb_in[1] : 0;
    alwan_f64 b_in = rgb_in[2] > 0 ? rgb_in[2] : 0;

    /* Apply inset matrix */
    alwan_f64 inset_r = agx_inset_matrix[0] * r_in + agx_inset_matrix[1] * g_in + agx_inset_matrix[2] * b_in;
    alwan_f64 inset_g = agx_inset_matrix[3] * r_in + agx_inset_matrix[4] * g_in + agx_inset_matrix[5] * b_in;
    alwan_f64 inset_b = agx_inset_matrix[6] * r_in + agx_inset_matrix[7] * g_in + agx_inset_matrix[8] * b_in;

    /* AgX log-encode */
    alwan_f64 const agx_min = ALWAN_LITERAL(-12.47393);
    alwan_f64 const agx_max = ALWAN_LITERAL(  4.026069);

    alwan_f64 log_r = alwan_agx_log_encode_f64_v(inset_r, agx_min, agx_max);
    alwan_f64 log_g = alwan_agx_log_encode_f64_v(inset_g, agx_min, agx_max);
    alwan_f64 log_b = alwan_agx_log_encode_f64_v(inset_b, agx_min, agx_max);

    /* Tone curve: LUT from sobotka/AgX AgX_Default_Contrast.spi1d */
    /* Output is in encoded "AgX Base" space — NOT linear. */
    rgb_out[0] = agx_lut_eval(log_r);
    rgb_out[1] = agx_lut_eval(log_g);
    rgb_out[2] = agx_lut_eval(log_b);
}

/* Convert AgX LUT output (encoded) to linear via sRGB EOTF.
 * The LUT produces sRGB-encoded values; sRGB EOTF linearizes them.
 * This replaces the incorrect pow(2.2) that mismatches sRGB in shadows. */
static void agx_encoded_to_linear(alwan_f64 *rgb) {
    rgb[0] = alwan_srgb_eotf_f64(alwan_saturate(rgb[0]));
    rgb[1] = alwan_srgb_eotf_f64(alwan_saturate(rgb[1]));
    rgb[2] = alwan_srgb_eotf_f64(alwan_saturate(rgb[2]));
}

/* AgX Base — base transform + linearize via sRGB EOTF */
static void agx_base_view(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    agx_base_transform(rgb_in, rgb_out);
    agx_encoded_to_linear(rgb_out);
}

/* AgX Punchy — CDL applied in encoded AgX space (before linearization) */
static void agx_punchy_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    agx_base_transform(rgb_in, rgb_out);
    alwan_vec3_f64 enc;
    enc.v[0] = rgb_out[0]; enc.v[1] = rgb_out[1]; enc.v[2] = rgb_out[2];
    alwan_vec3_f64 graded = alwan_agx_punchy_grade_f64_v(enc);
    rgb_out[0] = graded.v[0]; rgb_out[1] = graded.v[1]; rgb_out[2] = graded.v[2];
    agx_encoded_to_linear(rgb_out);
}

/* AgX Golden — CDL applied in encoded AgX space */
static void agx_golden_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    agx_base_transform(rgb_in, rgb_out);
    alwan_vec3_f64 enc;
    enc.v[0] = rgb_out[0]; enc.v[1] = rgb_out[1]; enc.v[2] = rgb_out[2];
    alwan_vec3_f64 graded = alwan_agx_golden_grade_f64_v(enc);
    rgb_out[0] = graded.v[0]; rgb_out[1] = graded.v[1]; rgb_out[2] = graded.v[2];
    agx_encoded_to_linear(rgb_out);
}

/* ----------------------------------------------------------------
 * BT.2446 Method A HDR<->SDR
 * ---------------------------------------------------------------- */

static void bt2446a_hdr_to_sdr_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    /* Operates on luminance channel after Yxy decomposition */
    alwan_f64 L_hdr = ALWAN_LITERAL(1000.0);
    alwan_f64 L_sdr = ALWAN_LITERAL(100.0);

    for (int i = 0; i < 3; i++) {
        rgb_out[i] = alwan_bt2446a_forward_f64_v(rgb_in[i], L_hdr, L_sdr);
    }
}

static void bt2446a_sdr_to_hdr_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    alwan_f64 L_hdr = ALWAN_LITERAL(1000.0);
    alwan_f64 L_sdr = ALWAN_LITERAL(100.0);

    for (int i = 0; i < 3; i++) {
        rgb_out[i] = alwan_bt2446a_inverse_f64_v(rgb_in[i], L_hdr, L_sdr);
    }
}

/* ----------------------------------------------------------------
 * Khronos PBR Neutral Tone Mapping
 * ---------------------------------------------------------------- */

static void khronos_pbr_neutral_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    alwan_vec3_f64 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    alwan_vec3_f64 out_v = alwan_khronos_pbr_neutral_f64_v(in_v);
    rgb_out[0] = out_v.v[0];
    rgb_out[1] = out_v.v[1];
    rgb_out[2] = out_v.v[2];
}

/* ----------------------------------------------------------------
 * Reinhard Extended (Luminance-based)
 * ---------------------------------------------------------------- */

static void reinhard_ext_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    alwan_vec3_f64 in_v = {{rgb_in[0], rgb_in[1], rgb_in[2]}};
    alwan_vec3_f64 out_v = alwan_reinhard_extended_luma_f64_v(in_v, ALWAN_LITERAL(4.0));
    rgb_out[0] = alwan_saturate(out_v.v[0]);
    rgb_out[1] = alwan_saturate(out_v.v[1]);
    rgb_out[2] = alwan_saturate(out_v.v[2]);
}

/* ----------------------------------------------------------------
 * Uchimura / Gran Turismo
 * ---------------------------------------------------------------- */

static void uchimura_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    rgb_out[0] = alwan_saturate(alwan_uchimura_default_f64_v(alwan_max(rgb_in[0], ALWAN_ZERO)));
    rgb_out[1] = alwan_saturate(alwan_uchimura_default_f64_v(alwan_max(rgb_in[1], ALWAN_ZERO)));
    rgb_out[2] = alwan_saturate(alwan_uchimura_default_f64_v(alwan_max(rgb_in[2], ALWAN_ZERO)));
}

/* ----------------------------------------------------------------
 * Lottes / AMD Cauldron
 * ---------------------------------------------------------------- */

static void lottes_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    alwan_vec3_f64 in_v = {{
        alwan_max(rgb_in[0], ALWAN_ZERO),
        alwan_max(rgb_in[1], ALWAN_ZERO),
        alwan_max(rgb_in[2], ALWAN_ZERO)
    }};
    alwan_vec3_f64 out_v = alwan_lottes_default_f64_v(in_v);
    rgb_out[0] = alwan_saturate(out_v.v[0]);
    rgb_out[1] = alwan_saturate(out_v.v[1]);
    rgb_out[2] = alwan_saturate(out_v.v[2]);
}

/* ----------------------------------------------------------------
 * Tony McMapface (Somewhat Boring Display Transform)
 * ---------------------------------------------------------------- */

static void tony_mcmapface_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    alwan_vec3_f64 in_v = {{
        alwan_max(rgb_in[0], ALWAN_ZERO),
        alwan_max(rgb_in[1], ALWAN_ZERO),
        alwan_max(rgb_in[2], ALWAN_ZERO)
    }};
    alwan_vec3_f64 out_v = alwan_tony_mcmapface_f64_v(in_v);
    rgb_out[0] = alwan_saturate(out_v.v[0]);
    rgb_out[1] = alwan_saturate(out_v.v[1]);
    rgb_out[2] = alwan_saturate(out_v.v[2]);
}

/* ----------------------------------------------------------------
 * BT.2446 Method B: SDR to HDR (default: 1000 nits HDR, 100 nits SDR)
 * ---------------------------------------------------------------- */

static void bt2446b_sdr_to_hdr_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    rgb_out[0] = alwan_bt2446b_forward_f64_v(alwan_saturate(rgb_in[0]),
                    ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
    rgb_out[1] = alwan_bt2446b_forward_f64_v(alwan_saturate(rgb_in[1]),
                    ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
    rgb_out[2] = alwan_bt2446b_forward_f64_v(alwan_saturate(rgb_in[2]),
                    ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
}

/* ----------------------------------------------------------------
 * BT.2446 Method C: HDR to SDR (default: 1000 nits HDR, 100 nits SDR)
 * ---------------------------------------------------------------- */

static void bt2446c_hdr_to_sdr_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    rgb_out[0] = alwan_bt2446c_forward_f64_v(alwan_saturate(rgb_in[0]),
                    ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
    rgb_out[1] = alwan_bt2446c_forward_f64_v(alwan_saturate(rgb_in[1]),
                    ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
    rgb_out[2] = alwan_bt2446c_forward_f64_v(alwan_saturate(rgb_in[2]),
                    ALWAN_LITERAL(1000.0), ALWAN_LITERAL(100.0));
}

/* ----------------------------------------------------------------
 * BT.2390 EETF: HDR to SDR (default: 10000 nits -> 100 nits)
 * ---------------------------------------------------------------- */

static void bt2390_hdr_to_sdr_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    rgb_out[0] = alwan_bt2390_eetf_luminance_f64_v(alwan_saturate(rgb_in[0]),
                    ALWAN_LITERAL(10000.0), ALWAN_LITERAL(100.0));
    rgb_out[1] = alwan_bt2390_eetf_luminance_f64_v(alwan_saturate(rgb_in[1]),
                    ALWAN_LITERAL(10000.0), ALWAN_LITERAL(100.0));
    rgb_out[2] = alwan_bt2390_eetf_luminance_f64_v(alwan_saturate(rgb_in[2]),
                    ALWAN_LITERAL(10000.0), ALWAN_LITERAL(100.0));
}

/* ----------------------------------------------------------------
 * Reinhard Calibrated (default: key=0.18, L_avg=0.18, L_white=4.0)
 * ---------------------------------------------------------------- */

static void reinhard_calibrated_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    rgb_out[0] = alwan_saturate(alwan_reinhard_calibrated_f64_v(
                    alwan_max(rgb_in[0], ALWAN_ZERO),
                    ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0)));
    rgb_out[1] = alwan_saturate(alwan_reinhard_calibrated_f64_v(
                    alwan_max(rgb_in[1], ALWAN_ZERO),
                    ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0)));
    rgb_out[2] = alwan_saturate(alwan_reinhard_calibrated_f64_v(
                    alwan_max(rgb_in[2], ALWAN_ZERO),
                    ALWAN_LITERAL(0.18), ALWAN_LITERAL(0.18), ALWAN_LITERAL(4.0)));
}

/* ----------------------------------------------------------------
 * Exposure-Based (default: exposure = 0 EV)
 * ---------------------------------------------------------------- */

static void exposure_transform(alwan_f64 const *rgb_in, alwan_f64 *rgb_out) {
    alwan_vec3_f64 in_v = {{
        alwan_max(rgb_in[0], ALWAN_ZERO),
        alwan_max(rgb_in[1], ALWAN_ZERO),
        alwan_max(rgb_in[2], ALWAN_ZERO)
    }};
    alwan_vec3_f64 out_v = alwan_exposure_tonemap_rgb_f64_v(in_v, ALWAN_ZERO);
    rgb_out[0] = alwan_saturate(out_v.v[0]);
    rgb_out[1] = alwan_saturate(out_v.v[1]);
    rgb_out[2] = alwan_saturate(out_v.v[2]);
}

/* ----------------------------------------------------------------
 * View Transform API
 * ---------------------------------------------------------------- */

int alwan_view_transform_apply_f64(alwan_f64 *rgb_out, size_t out_stride, alwan_f64 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx) {
    (void)ctx;  /* Unused for stateless transforms */

    if (!rgb_in || !rgb_out) {
        return ALWAN_E_INVALID;
    }

    /* Select view transform */
    void (*transform_fn)(alwan_f64 const *, alwan_f64 *) = NULL;

    switch (vt) {
        case ALWAN_VIEW_ACES_REC709:
            transform_fn = aces_rec709_transform;
            break;
        case ALWAN_VIEW_AGX:
            transform_fn = agx_base_view;
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
        case ALWAN_VIEW_REINHARD_EXT:
            transform_fn = reinhard_ext_transform;
            break;
        case ALWAN_VIEW_UCHIMURA:
            transform_fn = uchimura_transform;
            break;
        case ALWAN_VIEW_LOTTES:
            transform_fn = lottes_transform;
            break;
        case ALWAN_VIEW_TONY_MCMAPFACE:
            transform_fn = tony_mcmapface_transform;
            break;
        case ALWAN_VIEW_BT2446B_SDR_TO_HDR:
            transform_fn = bt2446b_sdr_to_hdr_transform;
            break;
        case ALWAN_VIEW_BT2446C_HDR_TO_SDR:
            transform_fn = bt2446c_hdr_to_sdr_transform;
            break;
        case ALWAN_VIEW_BT2390_HDR_TO_SDR:
            transform_fn = bt2390_hdr_to_sdr_transform;
            break;
        case ALWAN_VIEW_REINHARD_CALIBRATED:
            transform_fn = reinhard_calibrated_transform;
            break;
        case ALWAN_VIEW_EXPOSURE:
            transform_fn = exposure_transform;
            break;
        default:
            return ALWAN_E_INVALID;
    }

    /* Apply view transform to RGB triplets (strides are in bytes) */
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *in_ptr = (alwan_f64 const *)((char const *)rgb_in + i * in_stride);
        alwan_f64 *out_ptr = (alwan_f64 *)((char *)rgb_out + i * out_stride);

        transform_fn(in_ptr, out_ptr);
    }

    return ALWAN_OK;
}

int alwan_view_transform_apply_f32(alwan_f32 *rgb_out, size_t out_stride, alwan_f32 const *rgb_in, size_t in_stride, size_t count, alwan_view_transform vt, alwan_ctx *ctx) {
    if (!rgb_in || !rgb_out) return ALWAN_E_INVALID;
    /* Widen to f64, call f64 implementation, narrow back. Strides on the f32
     * views are in bytes; we convert pixel-wise to keep the public API
     * unchanged even when callers pass packed or planar buffers. */
    for (size_t i = 0; i < count; i++) {
        alwan_f32 const *in_ptr = (alwan_f32 const *)((char const *)rgb_in + i * in_stride);
        alwan_f32 *out_ptr = (alwan_f32 *)((char *)rgb_out + i * out_stride);
        alwan_f64 in64[3] = { (double)in_ptr[0], (double)in_ptr[1], (double)in_ptr[2] };
        alwan_f64 out64[3];
        int rc = alwan_view_transform_apply_f64(out64, 3 * sizeof(alwan_f64), in64, 3 * sizeof(alwan_f64), 1, vt, ctx);
        if (rc != ALWAN_OK) return rc;
        out_ptr[0] = (float)out64[0];
        out_ptr[1] = (float)out64[1];
        out_ptr[2] = (float)out64[2];
    }
    return ALWAN_OK;
}
