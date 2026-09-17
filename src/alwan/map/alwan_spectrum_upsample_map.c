/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The _ex forms of the spectral entry points: void pointers and pixel formats on
 * both sides, so an image pipeline hands u8, u16, f16, f32 or f64 colours in and
 * takes spectra back in the format it wants, or the reverse, without converting
 * either buffer first.
 *
 * Every dispatcher here is the same shape as the ALWAN_EX_DELEGATE_* macros in
 * alwan_map_internal.h: both formats native f32, call the f32 bulk form; both
 * native f64, the f64 one; otherwise tile through the typed loaders. The one
 * thing those macros could not be reused for is the channel count. A spectrum
 * has band_count samples per pixel, 36 to 85 for the shipped methods, and the
 * macros' scratch is sized for three. The scratch here is sized for three as
 * well, and the pixel count per tile shrinks to keep it there: at 85 bands a
 * tile is 72 pixels rather than 2048. That keeps the buffers off the heap and
 * the stack where the existing dispatchers keep theirs, and the results
 * identical to the bulk forms to the bit, since a tile boundary changes nothing
 * about a per-pixel computation.
 *
 * As in alwan_map_internal.h, a single-precision build must not name the other
 * precision's worker at all, so the pieces that mention one are selected by
 * ALWAN_WITH_* and a single-precision build tiles every mixed pair through the
 * precision it has.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"

/* Pixels per tile such that pixels x channels never exceeds the three-channel
 * scratch. Never below one. */
static size_t alwan__spectral_tile_pixels(size_t tile_pixels_3ch, size_t channels) {
    size_t n = channels <= 3 ? tile_pixels_3ch : (tile_pixels_3ch * 3) / channels;
    return n == 0 ? 1 : n;
}

/* ---- The pieces, one per precision ------------------------------------- */

/* Tiled colour-to-spectrum through one precision: three channels in, bands out.
 * Expects st, bands, in, in_fmt, in_stride, out, out_fmt, out_stride and count
 * in scope, and returns on the first failure. The method's extra argument, if
 * any, arrives as the variadic tail with its leading comma, which is why these
 * are variadic: a ", gamut" passed through a named parameter would be split
 * into two arguments on the rescan. */
#define ALWAN__SPEC_TILE_F64(fn_f64, ...)                                                           \
    {                                                                                               \
        size_t const per_ = alwan__spectral_tile_pixels(ALWAN_TILE_PIXELS_F64, bands);              \
        size_t off_ = 0;                                                                            \
        while (off_ < count) {                                                                      \
            size_t tile_ = count - off_;                                                            \
            if (tile_ > per_) tile_ = per_;                                                         \
            {                                                                                       \
                ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];                            \
                ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];                            \
                alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);       \
                st = fn_f64(obuf_, bands * sizeof(double), ibuf_, 3 * sizeof(double), tile_,        \
                            &bands __VA_ARGS__);                                                    \
                if (st != ALWAN_OK) return st;                                                      \
                alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_,       \
                                                (int)bands);                                        \
            }                                                                                       \
            off_ += tile_;                                                                          \
        }                                                                                           \
    }

#define ALWAN__SPEC_TILE_F32(fn_f32, ...)                                                           \
    {                                                                                               \
        size_t const per_ = alwan__spectral_tile_pixels(ALWAN_TILE_PIXELS_F32, bands);              \
        size_t off_ = 0;                                                                            \
        while (off_ < count) {                                                                      \
            size_t tile_ = count - off_;                                                            \
            if (tile_ > per_) tile_ = per_;                                                         \
            {                                                                                       \
                ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];                             \
                ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];                             \
                alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, 3);       \
                st = fn_f32(obuf_, bands * sizeof(float), ibuf_, 3 * sizeof(float), tile_,          \
                            &bands __VA_ARGS__);                                                    \
                if (st != ALWAN_OK) return st;                                                      \
                alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_,       \
                                                (int)bands);                                        \
            }                                                                                       \
            off_ += tile_;                                                                          \
        }                                                                                           \
    }

#if ALWAN_WITH_BOTH
#define ALWAN__SPEC_QUERY(fn_f32, fn_f64) fn_f64
#define ALWAN__SPEC_NATIVE(fn_f32, fn_f64, ...)                                                               \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) {                                           \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, band_count __VA_ARGS__); \
    }                                                                                                        \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) {                                           \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, band_count __VA_ARGS__); \
    }
#define ALWAN__SPEC_TILED(fn_f32, fn_f64, ...)                             \
    if (in_fmt == ALWAN_PIXEL_F64 || out_fmt == ALWAN_PIXEL_F64) {         \
        ALWAN__SPEC_TILE_F64(fn_f64, __VA_ARGS__)                          \
    } else {                                                               \
        ALWAN__SPEC_TILE_F32(fn_f32, __VA_ARGS__)                          \
    }
#elif ALWAN_WITH_F32
#define ALWAN__SPEC_QUERY(fn_f32, fn_f64) fn_f32
#define ALWAN__SPEC_NATIVE(fn_f32, fn_f64, ...)                                                               \
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) {                                           \
        return fn_f32((float *)out, out_stride, (float const *)in, in_stride, count, band_count __VA_ARGS__); \
    }
#define ALWAN__SPEC_TILED(fn_f32, fn_f64, ...) ALWAN__SPEC_TILE_F32(fn_f32, __VA_ARGS__)
#else /* ALWAN_WITH_F64 */
#define ALWAN__SPEC_QUERY(fn_f32, fn_f64) fn_f64
#define ALWAN__SPEC_NATIVE(fn_f32, fn_f64, ...)                                                               \
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) {                                           \
        return fn_f64((double *)out, out_stride, (double const *)in, in_stride, count, band_count __VA_ARGS__); \
    }
#define ALWAN__SPEC_TILED(fn_f32, fn_f64, ...) ALWAN__SPEC_TILE_F64(fn_f64, __VA_ARGS__)
#endif

/* A colour-to-spectrum dispatcher: in has 3 channels per pixel, out has
 * band_count. The bulk forms own the band_count contract (NULL out reports it,
 * a wrong value is ALWAN_E_INVALID), so the shape query is delegated to them. */
#define ALWAN__SPECTRAL_UP_EX(name, fn_f32, fn_f64, EXTRA_PARAM, ...)                                        \
alwan_status name(void *out, size_t out_stride, void const *in, size_t in_stride, size_t count,             \
                  alwan_pixel_format out_fmt, alwan_pixel_format in_fmt, size_t *band_count EXTRA_PARAM) { \
    if (!band_count) return ALWAN_E_INVALID;                                                                \
    if (!out) return ALWAN__SPEC_QUERY(fn_f32, fn_f64)(NULL, 0, NULL, 0, 0, band_count __VA_ARGS__);        \
    if (!in || count == 0) return ALWAN_E_INVALID;                                                          \
    ALWAN__SPEC_NATIVE(fn_f32, fn_f64, __VA_ARGS__)                                                         \
    {                                                                                                       \
        size_t bands = 0;                                                                                   \
        alwan_status st = ALWAN__SPEC_QUERY(fn_f32, fn_f64)(NULL, 0, NULL, 0, 0, &bands __VA_ARGS__);       \
        if (st != ALWAN_OK) return st;                                                                      \
        if (*band_count != bands) return ALWAN_E_INVALID;                                                   \
        ALWAN__SPEC_TILED(fn_f32, fn_f64, __VA_ARGS__)                                                      \
        return ALWAN_OK;                                                                                    \
    }                                                                                                       \
}

#define ALWAN__NO_EXTRA_PARAM
#define ALWAN__NO_EXTRA_ARG

ALWAN__SPECTRAL_UP_EX(alwan_rgb_to_spectrum_smits1999_map_interleave_ex,
                      alwan_rgb_to_spectrum_smits1999_f32_map_interleave,
                      alwan_rgb_to_spectrum_smits1999_f64_map_interleave,
                      ALWAN__NO_EXTRA_PARAM, ALWAN__NO_EXTRA_ARG)

ALWAN__SPECTRAL_UP_EX(alwan_rgb_to_spectrum_mallett2019_map_interleave_ex,
                      alwan_rgb_to_spectrum_mallett2019_f32_map_interleave,
                      alwan_rgb_to_spectrum_mallett2019_f64_map_interleave,
                      ALWAN__NO_EXTRA_PARAM, ALWAN__NO_EXTRA_ARG)

ALWAN__SPECTRAL_UP_EX(alwan_xyz_to_spectrum_otsu2018_map_interleave_ex,
                      alwan_xyz_to_spectrum_otsu2018_f32_map_interleave,
                      alwan_xyz_to_spectrum_otsu2018_f64_map_interleave,
                      ALWAN__NO_EXTRA_PARAM, ALWAN__NO_EXTRA_ARG)

#define ALWAN__JAKOB_PARAM , alwan_jakob2019_gamut gamut
#define ALWAN__JAKOB_ARG , gamut
ALWAN__SPECTRAL_UP_EX(alwan_rgb_to_spectrum_jakob2019_map_interleave_ex,
                      alwan_rgb_to_spectrum_jakob2019_f32_map_interleave,
                      alwan_rgb_to_spectrum_jakob2019_f64_map_interleave,
                      ALWAN__JAKOB_PARAM, ALWAN__JAKOB_ARG)

/* ---- The other direction: band_count channels in, three out --------------
 *
 * The weights arrive as f64, so this computes in f64 and stores to the requested
 * format; an f32/f32 call therefore gets the f64 sum narrowed rather than the
 * f32 bulk form's sum. That is said in the header so nobody expects bit-equality
 * with the f32 form here. A build without f64 narrows the weights once and sums
 * in f32, the only sum it has. */
#if ALWAN_WITH_F64
alwan_status alwan_spectral_to_tristimulus_map_interleave_ex(void *out, size_t out_stride, void const *in,
                                                             size_t in_stride, size_t count,
                                                             alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                                             size_t band_count, alwan_f64 const *weights) {
    size_t per, off_;
    if (!out || !in || !weights || count == 0 || band_count == 0) return ALWAN_E_INVALID;
    if (in_fmt == ALWAN_PIXEL_F64 && out_fmt == ALWAN_PIXEL_F64) {
        return alwan_spectral_to_tristimulus_f64_map_interleave((double *)out, out_stride, (double const *)in,
                                                                in_stride, count, band_count, weights);
    }
    per = alwan__spectral_tile_pixels(ALWAN_TILE_PIXELS_F64, band_count);
    off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > per) tile_ = per;
        {
            ALWAN_ALIGN(32) double ibuf_[ALWAN_TILE_PIXELS_F64 * 3];
            ALWAN_ALIGN(32) double obuf_[ALWAN_TILE_PIXELS_F64 * 3];
            alwan_status st;
            alwan__load_tile_typed_aos_f64(ibuf_, in, in_fmt, off_, in_stride, tile_, (int)band_count);
            st = alwan_spectral_to_tristimulus_f64_map_interleave(obuf_, 3 * sizeof(double), ibuf_,
                                                                  band_count * sizeof(double), tile_,
                                                                  band_count, weights);
            if (st != ALWAN_OK) return st;
            alwan__store_tile_typed_aos_f64(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        }
        off_ += tile_;
    }
    return ALWAN_OK;
}
#else /* f32 only: the weights are narrowed into the scratch, so 3 x band_count must fit it */
alwan_status alwan_spectral_to_tristimulus_map_interleave_ex(void *out, size_t out_stride, void const *in,
                                                             size_t in_stride, size_t count,
                                                             alwan_pixel_format out_fmt, alwan_pixel_format in_fmt,
                                                             size_t band_count, alwan_f64 const *weights) {
    ALWAN_ALIGN(32) float w32_[ALWAN_TILE_PIXELS_F32 * 3];
    size_t per, off_, i;
    if (!out || !in || !weights || count == 0 || band_count == 0) return ALWAN_E_INVALID;
    if (3 * band_count > (size_t)ALWAN_TILE_PIXELS_F32 * 3) return ALWAN_E_RANGE;
    for (i = 0; i < 3 * band_count; i++) w32_[i] = (float)weights[i];
    if (in_fmt == ALWAN_PIXEL_F32 && out_fmt == ALWAN_PIXEL_F32) {
        return alwan_spectral_to_tristimulus_f32_map_interleave((float *)out, out_stride, (float const *)in,
                                                                in_stride, count, band_count, w32_);
    }
    per = alwan__spectral_tile_pixels(ALWAN_TILE_PIXELS_F32, band_count);
    off_ = 0;
    while (off_ < count) {
        size_t tile_ = count - off_;
        if (tile_ > per) tile_ = per;
        {
            ALWAN_ALIGN(32) float ibuf_[ALWAN_TILE_PIXELS_F32 * 3];
            ALWAN_ALIGN(32) float obuf_[ALWAN_TILE_PIXELS_F32 * 3];
            alwan_status st;
            alwan__load_tile_typed_aos_f32(ibuf_, in, in_fmt, off_, in_stride, tile_, (int)band_count);
            st = alwan_spectral_to_tristimulus_f32_map_interleave(obuf_, 3 * sizeof(float), ibuf_,
                                                                  band_count * sizeof(float), tile_,
                                                                  band_count, w32_);
            if (st != ALWAN_OK) return st;
            alwan__store_tile_typed_aos_f32(out, out_fmt, off_, out_stride, obuf_, tile_, 3);
        }
        off_ += tile_;
    }
    return ALWAN_OK;
}
#endif
