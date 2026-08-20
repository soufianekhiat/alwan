/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map (bulk / typed) view transforms.
 *
 * The view transforms dispatch by alwan_view_transform enum and their per-pixel
 * workers are scalar (LUT / pow), so there is no SIMD lane kernel: the bulk path
 * IS alwan_view_transform_apply (a strided interleaved per-pixel loop). These
 * wrappers give views the standard map API surface and typed-pipeline entry:
 *   - _f32/_f64_map_interleave : symmetric name, forwards to the apply loop.
 *   - _map_interleave_ex       : u8/u16/f16/f32/f64 typed I/O for the image pipeline.
 * All are byte-identical to alwan_view_transform_apply for the same precision.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"

/* ---- Symmetric map_interleave names (forward to the apply bulk loop) ---- */
#if ALWAN_WITH_F32
alwan_status alwan_view_transform_f32_map_interleave(alwan_f32 *out, size_t out_stride,
        alwan_f32 const *in, size_t in_stride, size_t count,
        alwan_view_transform vt, alwan_ctx *ctx) {
    return alwan_view_transform_apply_f32(out, out_stride, in, in_stride, count, vt, ctx);
}
#endif
#if ALWAN_WITH_F64
alwan_status alwan_view_transform_f64_map_interleave(alwan_f64 *out, size_t out_stride,
        alwan_f64 const *in, size_t in_stride, size_t count,
        alwan_view_transform vt, alwan_ctx *ctx) {
    return alwan_view_transform_apply_f64(out, out_stride, in, in_stride, count, vt, ctx);
}
#endif

/* ---- Typed (u8/u16/f16/f32/f64) delegate for the image pipeline ---- */
alwan_status alwan_view_transform_map_interleave_ex(void *out, size_t out_stride,
        void const *in, size_t in_stride, size_t count,
        alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
        alwan_view_transform vt, alwan_ctx *ctx) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_F32
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32)
        return alwan_view_transform_apply_f32((alwan_f32 *)out, out_stride, (alwan_f32 const *)in, in_stride, count, vt, ctx);
#endif
#if ALWAN_WITH_F64
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64)
        return alwan_view_transform_apply_f64((alwan_f64 *)out, out_stride, (alwan_f64 const *)in, in_stride, count, vt, ctx);
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) alwan_f64 ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) alwan_f64 obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_view_transform_apply_f64(obuf_, 3 * sizeof(alwan_f64), ibuf_, 3 * sizeof(alwan_f64), tile_, vt, ctx);
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
        return ALWAN_OK;
    }
#endif
#if ALWAN_WITH_F32
    {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F32) tile_ = ALWAN_TILE_PIXELS_F32;
            ALWAN_ALIGN(32) alwan_f32 ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) alwan_f32 obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_view_transform_apply_f32(obuf_, 3 * sizeof(alwan_f32), ibuf_, 3 * sizeof(alwan_f32), tile_, vt, ctx);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
        return ALWAN_OK;
    }
#else
    return ALWAN_E_INVALID;
#endif
}
