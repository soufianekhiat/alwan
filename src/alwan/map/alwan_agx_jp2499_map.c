/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map (interleaved bulk) JP2499 picture formation.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_agx_jp2499_core.h"   /* jp2499_render_f32/f64 + tonescale_params */

#if ALWAN_WITH_F32
/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_agx_jp2499_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

#if ALWAN_WITH_F64
/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_agx_jp2499_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

/* ----------------------------------------------------------------
 * Typed (u8/u16/f16/f32/f64) delegate -- mirrors ALWAN_EX_DELEGATE_DUAL with
 * the params passthrough, so JP2499 runs through the typed image pipeline.
 * ---------------------------------------------------------------- */
int alwan_jp2499_map_interleave_ex(void *out, size_t out_stride,
                                   void const *in, size_t in_stride, size_t count,
                                   alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                   alwan_jp2499_params_f64 const *params) {
    if (!in || !out || !params || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_F32
    /* Single-precision view of the (f64) params for the f32 paths. */
    alwan_jp2499_params_f32 pf32;
    for (int k = 0; k < 3; k++) {
        pf32.chroma_attenuation[k] = (alwan_f32)params->chroma_attenuation[k];
        pf32.hue_flight[k]         = (alwan_f32)params->hue_flight[k];
        pf32.purity[k]             = (alwan_f32)params->purity[k];
    }
    pf32.peak_luminance = (alwan_f32)params->peak_luminance;
    pf32.white_tip.r = (alwan_f32)params->white_tip.r; pf32.white_tip.g = (alwan_f32)params->white_tip.g; pf32.white_tip.b = (alwan_f32)params->white_tip.b;
    pf32.black_tip.r = (alwan_f32)params->black_tip.r; pf32.black_tip.g = (alwan_f32)params->black_tip.g; pf32.black_tip.b = (alwan_f32)params->black_tip.b;
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32)
        return alwan_jp2499_f32_map_interleave((alwan_f32 *)out, out_stride, (alwan_f32 const *)in, in_stride, count, &pf32);
#endif
#if ALWAN_WITH_F64
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64)
        return alwan_jp2499_f64_map_interleave((alwan_f64 *)out, out_stride, (alwan_f64 const *)in, in_stride, count, params);
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) alwan_f64 ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) alwan_f64 obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_jp2499_f64_map_interleave(obuf_, 3 * sizeof(alwan_f64), ibuf_, 3 * sizeof(alwan_f64), tile_, params);
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
            alwan_jp2499_f32_map_interleave(obuf_, 3 * sizeof(alwan_f32), ibuf_, 3 * sizeof(alwan_f32), tile_, &pf32);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
        return ALWAN_OK;
    }
#else
    return ALWAN_E_INVALID;
#endif
}
