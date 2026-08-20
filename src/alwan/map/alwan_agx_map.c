/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map (interleaved bulk) analytic AgX picture formation.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_agx_core.h"   /* agx_render_f32/f64 + sigmoid setup */

#if ALWAN_WITH_F32
/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_agx_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

#if ALWAN_WITH_F64
/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_agx_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

/* ----------------------------------------------------------------
 * Typed (u8/u16/f16/f32/f64) delegate -- mirrors ALWAN_EX_DELEGATE_DUAL with the
 * params passthrough, so analytic AgX runs through the typed image pipeline.
 * ---------------------------------------------------------------- */
alwan_status alwan_agx_map_interleave_ex(void *out, size_t out_stride,
                                void const *in, size_t in_stride, size_t count,
                                alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                alwan_agx_params_f64 const *params) {
    if (!in || !out || !params || count == 0) return ALWAN_E_INVALID;
#if ALWAN_WITH_F32
    /* Single-precision view of the (f64) params for the f32 paths. */
    alwan_agx_params_f32 pf32;
    for (int k = 0; k < 9; k++) { pf32.inset.m[k] = (alwan_f32)params->inset.m[k]; pf32.outset.m[k] = (alwan_f32)params->outset.m[k]; }
    pf32.log2_min = (alwan_f32)params->log2_min; pf32.log2_max = (alwan_f32)params->log2_max;
    pf32.pivot_input = (alwan_f32)params->pivot_input; pf32.pivot_output = (alwan_f32)params->pivot_output;
    pf32.slope = (alwan_f32)params->slope; pf32.toe_power = (alwan_f32)params->toe_power; pf32.shoulder_power = (alwan_f32)params->shoulder_power;
    pf32.tip_upper_angle = (alwan_f32)params->tip_upper_angle; pf32.tip_upper_force = (alwan_f32)params->tip_upper_force; pf32.tip_upper_offset = (alwan_f32)params->tip_upper_offset;
    pf32.tip_lower_angle = (alwan_f32)params->tip_lower_angle; pf32.tip_lower_force = (alwan_f32)params->tip_lower_force; pf32.tip_lower_offset = (alwan_f32)params->tip_lower_offset;
    pf32.tip_middle_angle = (alwan_f32)params->tip_middle_angle; pf32.tip_middle_force = (alwan_f32)params->tip_middle_force;
    for (int k = 0; k < 3; k++) { pf32.primary_rotation[k] = (alwan_f32)params->primary_rotation[k]; pf32.primary_inset[k] = (alwan_f32)params->primary_inset[k]; pf32.primary_purity[k] = (alwan_f32)params->primary_purity[k]; }
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32)
        return alwan_agx_f32_map_interleave((alwan_f32 *)out, out_stride, (alwan_f32 const *)in, in_stride, count, &pf32);
#endif
#if ALWAN_WITH_F64
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64)
        return alwan_agx_f64_map_interleave((alwan_f64 *)out, out_stride, (alwan_f64 const *)in, in_stride, count, params);
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {
        size_t off_ = 0;
        while (off_ < count) {
            size_t tile_ = count - off_;
            if (tile_ > ALWAN_TILE_PIXELS_F64) tile_ = ALWAN_TILE_PIXELS_F64;
            ALWAN_ALIGN(32) alwan_f64 ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) alwan_f64 obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);
            alwan_agx_f64_map_interleave(obuf_, 3 * sizeof(alwan_f64), ibuf_, 3 * sizeof(alwan_f64), tile_, params);
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
            alwan_agx_f32_map_interleave(obuf_, 3 * sizeof(alwan_f32), ibuf_, 3 * sizeof(alwan_f32), tile_, &pf32);
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
            off_ += tile_;
        }
        return ALWAN_OK;
    }
#else
    return ALWAN_E_INVALID;
#endif
}
