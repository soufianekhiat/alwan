/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Video signal encoding/decoding API
 * Combines TF application + range scaling + quantization in one call.
 *
 * Narrow range (SMPTE): code values [16*2^(N-8) .. 235*2^(N-8)]
 * Full range: code values [0 .. 2^N - 1]
 */

#include "../alwan.h"
#include "../alwan_internal.h"

/* alwan__resolve_oetf / alwan__resolve_eotf are now in alwan_internal.h */

/* ----------------------------------------------------------------
 * Internal: narrow range parameters for N-bit video
 *
 * ITU-R BT.601/709/2020 narrow range for RGB/luma:
 *   foot  = 16  * 2^(N-8)
 *   head  = 235 * 2^(N-8)
 *   max   = 2^N - 1
 * ---------------------------------------------------------------- */

static int video_narrow_params(int bit_depth,
                               alwan_f64 *foot,
                               alwan_f64 *head,
                               alwan_f64 *max_code) {
    if (bit_depth < 8 || bit_depth > 16) return 0;
    int scale = 1 << (bit_depth - 8);
    *foot     = (alwan_f64)(16  * scale);
    *head     = (alwan_f64)(235 * scale);
    *max_code = (alwan_f64)((1 << bit_depth) - 1);
    return 1;
}

/* Default bit depth from pixel format */
static int video_default_bit_depth(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 8;
    case ALWAN_PIXEL_U16: return 16;
    case ALWAN_PIXEL_F32: return 8;
    case ALWAN_PIXEL_F64: return 8;
    default:              return 8;
    }
}

/* Store a single encoded+range-scaled triplet to output buffer */
static void video_store(void *dst, alwan_f64 const v[3],
                        alwan_pixel_format fmt,
                        int bit_depth, alwan_video_range range) {
    if (range == ALWAN_VIDEO_RANGE_NARROW) {
        alwan_f64 foot, head, max_code;
        video_narrow_params(bit_depth, &foot, &head, &max_code);

        switch (fmt) {
        case ALWAN_PIXEL_U8: {
            uint8_t *p = (uint8_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f64 cv = v[c] * (head - foot) + foot + ALWAN_LITERAL(0.5);
                if (cv < foot) cv = foot;
                if (cv > head) cv = head;
                p[c] = (uint8_t)cv;
            }
        } break;
        case ALWAN_PIXEL_U16: {
            uint16_t *p = (uint16_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f64 cv = v[c] * (head - foot) + foot + ALWAN_LITERAL(0.5);
                if (cv < foot) cv = foot;
                if (cv > head) cv = head;
                p[c] = (uint16_t)cv;
            }
        } break;
        case ALWAN_PIXEL_F32: {
            float *p = (float *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f64 cv = v[c] * (head - foot) / max_code + foot / max_code;
                p[c] = (float)cv;
            }
        } break;
        case ALWAN_PIXEL_F64: {
            double *p = (double *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f64 cv = v[c] * (head - foot) / max_code + foot / max_code;
                p[c] = (double)cv;
            }
        } break;
        default: break;
        }
    } else {
        /* Full range */
        alwan_f64 max_code = (alwan_f64)((1 << bit_depth) - 1);

        switch (fmt) {
        case ALWAN_PIXEL_U8: {
            uint8_t *p = (uint8_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f64 cv = v[c] * max_code + ALWAN_LITERAL(0.5);
                if (cv < ALWAN_LITERAL(0.0)) cv = ALWAN_LITERAL(0.0);
                if (cv > max_code) cv = max_code;
                p[c] = (uint8_t)cv;
            }
        } break;
        case ALWAN_PIXEL_U16: {
            uint16_t *p = (uint16_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f64 cv = v[c] * max_code + ALWAN_LITERAL(0.5);
                if (cv < ALWAN_LITERAL(0.0)) cv = ALWAN_LITERAL(0.0);
                if (cv > max_code) cv = max_code;
                p[c] = (uint16_t)cv;
            }
        } break;
        case ALWAN_PIXEL_F32: {
            float *p = (float *)dst;
            p[0] = (float)v[0]; p[1] = (float)v[1]; p[2] = (float)v[2];
        } break;
        case ALWAN_PIXEL_F64: {
            double *p = (double *)dst;
            p[0] = (double)v[0]; p[1] = (double)v[1]; p[2] = (double)v[2];
        } break;
        default: break;
        }
    }
}

/* Load a single pixel from input buffer, dequantize + unscale range -> [0,1] */
static void video_load(alwan_f64 v[3], void const *src,
                       alwan_pixel_format fmt,
                       int bit_depth, alwan_video_range range) {
    alwan_f64 raw[3];

    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        uint8_t const *p = (uint8_t const *)src;
        raw[0] = (alwan_f64)p[0];
        raw[1] = (alwan_f64)p[1];
        raw[2] = (alwan_f64)p[2];
    } break;
    case ALWAN_PIXEL_U16: {
        uint16_t const *p = (uint16_t const *)src;
        raw[0] = (alwan_f64)p[0];
        raw[1] = (alwan_f64)p[1];
        raw[2] = (alwan_f64)p[2];
    } break;
    case ALWAN_PIXEL_F32: {
        float const *p = (float const *)src;
        raw[0] = (alwan_f64)p[0];
        raw[1] = (alwan_f64)p[1];
        raw[2] = (alwan_f64)p[2];
    } break;
    case ALWAN_PIXEL_F64: {
        double const *p = (double const *)src;
        raw[0] = (alwan_f64)p[0];
        raw[1] = (alwan_f64)p[1];
        raw[2] = (alwan_f64)p[2];
    } break;
    default:
        v[0] = v[1] = v[2] = ALWAN_LITERAL(0.0);
        return;
    }

    if (range == ALWAN_VIDEO_RANGE_NARROW) {
        alwan_f64 foot, head, max_code;
        video_narrow_params(bit_depth, &foot, &head, &max_code);

        if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
            /* Integer: raw is code value, unscale from narrow */
            for (int c = 0; c < 3; c++) {
                v[c] = (raw[c] - foot) / (head - foot);
            }
        } else {
            /* Float: raw is already normalized to [foot/max, head/max], unscale */
            for (int c = 0; c < 3; c++) {
                v[c] = (raw[c] - foot / max_code) / ((head - foot) / max_code);
            }
        }
    } else {
        /* Full range */
        if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
            alwan_f64 max_code = (alwan_f64)((1 << bit_depth) - 1);
            for (int c = 0; c < 3; c++) {
                v[c] = raw[c] / max_code;
            }
        } else {
            v[0] = raw[0]; v[1] = raw[1]; v[2] = raw[2];
        }
    }
}

/* Pixel stride in bytes for 3-channel pixel */
static size_t video_pixel_stride(alwan_pixel_format fmt) {
    switch (fmt) {
    case ALWAN_PIXEL_U8:  return 3 * sizeof(uint8_t);
    case ALWAN_PIXEL_U16: return 3 * sizeof(uint16_t);
    case ALWAN_PIXEL_F32: return 3 * sizeof(float);
    case ALWAN_PIXEL_F64: return 3 * sizeof(double);
    default:              return 0;
    }
}

/* ----------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------- */

alwan_status alwan_video_encode_f64(void *out, alwan_f64 const *rgb_linear, size_t count, alwan_pixel_format out_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx) {
    if (!out || !rgb_linear || count == 0) return ALWAN_E_INVALID;

    /* Get the space descriptor for the OETF */
    alwan_rgb_space_desc_f64 desc;
    int status = alwan_rgb_get_space_descriptor_f64(&desc, space, ctx);
    if (status != ALWAN_OK) return status;

    /* Resolve the OETF function pointer */
    alwan_tf_fn_f64 oetf = alwan__resolve_oetf_f64(desc.oetf);
    if (!oetf) return ALWAN_E_INVALID;

    /* Default bit depth from output format */
    if (bit_depth <= 0) bit_depth = video_default_bit_depth(out_fmt);

    /* bit_depth is documented as 8, 10, 12 or 16. It was never checked: an
     * out-of-range value left video_narrow_params' outputs uninitialised, the
     * caller discarded its failure return, and the function wrote whatever was
     * on the stack while returning ALWAN_OK. bit_depth = 20 reproducibly
     * emitted the value left by the preceding bit_depth = 12 call. */
    if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12 && bit_depth != 16) {
        return ALWAN_E_INVALID;
    }

    size_t stride = video_pixel_stride(out_fmt);
    if (stride == 0) return ALWAN_E_INVALID;

    uint8_t *dst = (uint8_t *)out;

    for (size_t i = 0; i < count; i++) {
        alwan_f64 encoded[3];
        encoded[0] = oetf(rgb_linear[i * 3 + 0]);
        encoded[1] = oetf(rgb_linear[i * 3 + 1]);
        encoded[2] = oetf(rgb_linear[i * 3 + 2]);

        video_store(dst + i * stride, encoded, out_fmt, bit_depth, range);
    }

    return ALWAN_OK;
}

alwan_status alwan_video_decode_f64(alwan_f64 *rgb_linear, void const *in, size_t count, alwan_pixel_format in_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx) {
    if (!rgb_linear || !in || count == 0) return ALWAN_E_INVALID;

    /* Get the space descriptor for the EOTF */
    alwan_rgb_space_desc_f64 desc;
    int status = alwan_rgb_get_space_descriptor_f64(&desc, space, ctx);
    if (status != ALWAN_OK) return status;

    /* Resolve the EOTF function pointer */
    alwan_tf_fn_f64 eotf = alwan__resolve_eotf_f64(desc.eotf);
    if (!eotf) return ALWAN_E_INVALID;

    /* Default bit depth from input format */
    if (bit_depth <= 0) bit_depth = video_default_bit_depth(in_fmt);

    /* bit_depth is documented as 8, 10, 12 or 16. It was never checked: an
     * out-of-range value left video_narrow_params' outputs uninitialised, the
     * caller discarded its failure return, and the function wrote whatever was
     * on the stack while returning ALWAN_OK. bit_depth = 20 reproducibly
     * emitted the value left by the preceding bit_depth = 12 call. */
    if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12 && bit_depth != 16) {
        return ALWAN_E_INVALID;
    }

    size_t stride = video_pixel_stride(in_fmt);
    if (stride == 0) return ALWAN_E_INVALID;

    uint8_t const *src = (uint8_t const *)in;

    for (size_t i = 0; i < count; i++) {
        alwan_f64 encoded[3];
        video_load(encoded, src + i * stride, in_fmt, bit_depth, range);

        rgb_linear[i * 3 + 0] = eotf(encoded[0]);
        rgb_linear[i * 3 + 1] = eotf(encoded[1]);
        rgb_linear[i * 3 + 2] = eotf(encoded[2]);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * f32 helpers -- native float twins of the f64 range/quantize math.
 * Mirror video_narrow_params / video_store / video_load exactly,
 * but compute entirely in alwan_f32.
 * ---------------------------------------------------------------- */

static int video_narrow_params_f32(int bit_depth,
                                   alwan_f32 *foot,
                                   alwan_f32 *head,
                                   alwan_f32 *max_code) {
    if (bit_depth < 8 || bit_depth > 16) return 0;
    int scale = 1 << (bit_depth - 8);
    *foot     = (alwan_f32)(16  * scale);
    *head     = (alwan_f32)(235 * scale);
    *max_code = (alwan_f32)((1 << bit_depth) - 1);
    return 1;
}

/* Store a single encoded+range-scaled triplet to output buffer (f32 math) */
static void video_store_f32(void *dst, alwan_f32 const v[3],
                            alwan_pixel_format fmt,
                            int bit_depth, alwan_video_range range) {
    if (range == ALWAN_VIDEO_RANGE_NARROW) {
        alwan_f32 foot, head, max_code;
        video_narrow_params_f32(bit_depth, &foot, &head, &max_code);

        switch (fmt) {
        case ALWAN_PIXEL_U8: {
            uint8_t *p = (uint8_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f32 cv = v[c] * (head - foot) + foot + 0.5f;
                if (cv < foot) cv = foot;
                if (cv > head) cv = head;
                p[c] = (uint8_t)cv;
            }
        } break;
        case ALWAN_PIXEL_U16: {
            uint16_t *p = (uint16_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f32 cv = v[c] * (head - foot) + foot + 0.5f;
                if (cv < foot) cv = foot;
                if (cv > head) cv = head;
                p[c] = (uint16_t)cv;
            }
        } break;
        case ALWAN_PIXEL_F32: {
            float *p = (float *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f32 cv = v[c] * (head - foot) / max_code + foot / max_code;
                p[c] = (float)cv;
            }
        } break;
        case ALWAN_PIXEL_F64: {
            double *p = (double *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f32 cv = v[c] * (head - foot) / max_code + foot / max_code;
                p[c] = (double)cv;
            }
        } break;
        default: break;
        }
    } else {
        /* Full range */
        alwan_f32 max_code = (alwan_f32)((1 << bit_depth) - 1);

        switch (fmt) {
        case ALWAN_PIXEL_U8: {
            uint8_t *p = (uint8_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f32 cv = v[c] * max_code + 0.5f;
                if (cv < 0.0f) cv = 0.0f;
                if (cv > max_code) cv = max_code;
                p[c] = (uint8_t)cv;
            }
        } break;
        case ALWAN_PIXEL_U16: {
            uint16_t *p = (uint16_t *)dst;
            for (int c = 0; c < 3; c++) {
                alwan_f32 cv = v[c] * max_code + 0.5f;
                if (cv < 0.0f) cv = 0.0f;
                if (cv > max_code) cv = max_code;
                p[c] = (uint16_t)cv;
            }
        } break;
        case ALWAN_PIXEL_F32: {
            float *p = (float *)dst;
            p[0] = (float)v[0]; p[1] = (float)v[1]; p[2] = (float)v[2];
        } break;
        case ALWAN_PIXEL_F64: {
            double *p = (double *)dst;
            p[0] = (double)v[0]; p[1] = (double)v[1]; p[2] = (double)v[2];
        } break;
        default: break;
        }
    }
}

/* Load a single pixel, dequantize + unscale range -> [0,1] (f32 math) */
static void video_load_f32(alwan_f32 v[3], void const *src,
                           alwan_pixel_format fmt,
                           int bit_depth, alwan_video_range range) {
    alwan_f32 raw[3];

    switch (fmt) {
    case ALWAN_PIXEL_U8: {
        uint8_t const *p = (uint8_t const *)src;
        raw[0] = (alwan_f32)p[0];
        raw[1] = (alwan_f32)p[1];
        raw[2] = (alwan_f32)p[2];
    } break;
    case ALWAN_PIXEL_U16: {
        uint16_t const *p = (uint16_t const *)src;
        raw[0] = (alwan_f32)p[0];
        raw[1] = (alwan_f32)p[1];
        raw[2] = (alwan_f32)p[2];
    } break;
    case ALWAN_PIXEL_F32: {
        float const *p = (float const *)src;
        raw[0] = (alwan_f32)p[0];
        raw[1] = (alwan_f32)p[1];
        raw[2] = (alwan_f32)p[2];
    } break;
    case ALWAN_PIXEL_F64: {
        double const *p = (double const *)src;
        raw[0] = (alwan_f32)p[0];
        raw[1] = (alwan_f32)p[1];
        raw[2] = (alwan_f32)p[2];
    } break;
    default:
        v[0] = v[1] = v[2] = 0.0f;
        return;
    }

    if (range == ALWAN_VIDEO_RANGE_NARROW) {
        alwan_f32 foot, head, max_code;
        video_narrow_params_f32(bit_depth, &foot, &head, &max_code);

        if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
            /* Integer: raw is code value, unscale from narrow */
            for (int c = 0; c < 3; c++) {
                v[c] = (raw[c] - foot) / (head - foot);
            }
        } else {
            /* Float: raw is already normalized to [foot/max, head/max], unscale */
            for (int c = 0; c < 3; c++) {
                v[c] = (raw[c] - foot / max_code) / ((head - foot) / max_code);
            }
        }
    } else {
        /* Full range */
        if (fmt == ALWAN_PIXEL_U8 || fmt == ALWAN_PIXEL_U16) {
            alwan_f32 max_code = (alwan_f32)((1 << bit_depth) - 1);
            for (int c = 0; c < 3; c++) {
                v[c] = raw[c] / max_code;
            }
        } else {
            v[0] = raw[0]; v[1] = raw[1]; v[2] = raw[2];
        }
    }
}

/* ----------------------------------------------------------------
 * f32 public API -- native float pipeline.
 * Resolves OETF/EOTF with the native f32 resolvers and applies all
 * range/quantize math in f32; no widen/narrow round-trip.
 * ---------------------------------------------------------------- */

alwan_status alwan_video_encode_f32(void *out, alwan_f32 const *rgb_linear, size_t count, alwan_pixel_format out_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx) {
    if (!out || !rgb_linear || count == 0) return ALWAN_E_INVALID;

    /* Get the space descriptor for the OETF */
    alwan_rgb_space_desc_f32 desc;
    int status = alwan_rgb_get_space_descriptor_f32(&desc, space, ctx);
    if (status != ALWAN_OK) return status;

    /* Resolve the OETF function pointer */
    alwan_tf_fn_f32 oetf = alwan__resolve_oetf_f32(desc.oetf);
    if (!oetf) return ALWAN_E_INVALID;

    /* Default bit depth from output format */
    if (bit_depth <= 0) bit_depth = video_default_bit_depth(out_fmt);

    /* bit_depth is documented as 8, 10, 12 or 16. It was never checked: an
     * out-of-range value left video_narrow_params' outputs uninitialised, the
     * caller discarded its failure return, and the function wrote whatever was
     * on the stack while returning ALWAN_OK. bit_depth = 20 reproducibly
     * emitted the value left by the preceding bit_depth = 12 call. */
    if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12 && bit_depth != 16) {
        return ALWAN_E_INVALID;
    }

    size_t stride = video_pixel_stride(out_fmt);
    if (stride == 0) return ALWAN_E_INVALID;

    uint8_t *dst = (uint8_t *)out;

    for (size_t i = 0; i < count; i++) {
        alwan_f32 encoded[3];
        encoded[0] = oetf(rgb_linear[i * 3 + 0]);
        encoded[1] = oetf(rgb_linear[i * 3 + 1]);
        encoded[2] = oetf(rgb_linear[i * 3 + 2]);

        video_store_f32(dst + i * stride, encoded, out_fmt, bit_depth, range);
    }

    return ALWAN_OK;
}

alwan_status alwan_video_decode_f32(alwan_f32 *rgb_linear, void const *in, size_t count, alwan_pixel_format in_fmt, alwan_rgb_space space, alwan_video_range range, int bit_depth, alwan_ctx *ctx) {
    if (!rgb_linear || !in || count == 0) return ALWAN_E_INVALID;

    /* Get the space descriptor for the EOTF */
    alwan_rgb_space_desc_f32 desc;
    int status = alwan_rgb_get_space_descriptor_f32(&desc, space, ctx);
    if (status != ALWAN_OK) return status;

    /* Resolve the EOTF function pointer */
    alwan_tf_fn_f32 eotf = alwan__resolve_eotf_f32(desc.eotf);
    if (!eotf) return ALWAN_E_INVALID;

    /* Default bit depth from input format */
    if (bit_depth <= 0) bit_depth = video_default_bit_depth(in_fmt);

    /* bit_depth is documented as 8, 10, 12 or 16. It was never checked: an
     * out-of-range value left video_narrow_params' outputs uninitialised, the
     * caller discarded its failure return, and the function wrote whatever was
     * on the stack while returning ALWAN_OK. bit_depth = 20 reproducibly
     * emitted the value left by the preceding bit_depth = 12 call. */
    if (bit_depth != 8 && bit_depth != 10 && bit_depth != 12 && bit_depth != 16) {
        return ALWAN_E_INVALID;
    }

    size_t stride = video_pixel_stride(in_fmt);
    if (stride == 0) return ALWAN_E_INVALID;

    uint8_t const *src = (uint8_t const *)in;

    for (size_t i = 0; i < count; i++) {
        alwan_f32 encoded[3];
        video_load_f32(encoded, src + i * stride, in_fmt, bit_depth, range);

        rgb_linear[i * 3 + 0] = eotf(encoded[0]);
        rgb_linear[i * 3 + 1] = eotf(encoded[1]);
        rgb_linear[i * 3 + 2] = eotf(encoded[2]);
    }

    return ALWAN_OK;
}
