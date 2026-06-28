/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spectral Upsampling - RGB to Spectrum Conversion
 * Implements reflectance recovery methods based on colour-science.
 *
 * The recovery methods are instantiated natively for both f32 and f64
 * from alwan_spectrum_upsample_impl.inc (included once per precision
 * below), so the single-precision path computes in float throughout
 * rather than widening to double. Only the shared basis/LUT data lives
 * here.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * Smits1999 Basis Spectra Data
 * Based on: Smits, Brian. "An RGB to Spectrum Conversion for Reflectances" (1999)
 * White point: CIE Illuminant E (equal energy)
 * RGB colorspace: sRGB primaries
 * Wavelength range: 380-720nm, 10 samples
 * ---------------------------------------------------------------- */

#define SMITS1999_WAVELENGTH_COUNT 10
#define SMITS1999_WAVELENGTH_MIN ALWAN_LITERAL(380.0)
#define SMITS1999_WAVELENGTH_MAX ALWAN_LITERAL(720.0)

/* Wavelengths (shared across all basis spectra) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_wavelengths[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/wavelengths.csv"
};
ALWAN_DIAG_POP

/* Basis spectra: white, cyan, magenta, yellow, red, green, blue.
 * Dual-declared (f32 + f64 twins from the same CSV) so the templated
 * impl reads native float data on the f32 pass instead of casting. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const smits1999_white_f32[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/white.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_white_f64[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/white.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const smits1999_cyan_f32[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/cyan.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_cyan_f64[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/cyan.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const smits1999_magenta_f32[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/magenta.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_magenta_f64[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/magenta.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const smits1999_yellow_f32[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/yellow.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_yellow_f64[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/yellow.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const smits1999_red_f32[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/red.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_red_f64[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/red.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const smits1999_green_f32[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/green.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_green_f64[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/green.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const smits1999_blue_f32[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/blue.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_blue_f64[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/blue.csv"
};
ALWAN_DIAG_POP
#endif

/* ----------------------------------------------------------------
 * Mallett2019 Basis Functions Data
 * Based on: Mallett & Yuksel. "Spectral Primary Decomposition for Rendering with sRGB Reflectance" (2019)
 * RGB colorspace: sRGB
 * Wavelength range: 380-780nm, 81 samples (5nm intervals)
 * ---------------------------------------------------------------- */

#define MALLETT2019_WAVELENGTH_COUNT 81
#define MALLETT2019_WAVELENGTH_MIN ALWAN_LITERAL(380.0)
#define MALLETT2019_WAVELENGTH_MAX ALWAN_LITERAL(780.0)

/* Wavelengths (shared across all basis functions) */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const mallett2019_wavelengths[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/wavelengths.csv"
};
ALWAN_DIAG_POP

/* Basis functions: red, green, blue. Dual-declared f32/f64 twins. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const mallett2019_red_f32[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/red.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const mallett2019_red_f64[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/red.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const mallett2019_green_f32[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/green.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const mallett2019_green_f64[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/green.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const mallett2019_blue_f32[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/blue.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const mallett2019_blue_f64[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/blue.csv"
};
ALWAN_DIAG_POP
#endif

/* ----------------------------------------------------------------
 * Jakob2019 Polynomial LUT Data
 * Based on: Jakob & Hanika. "A Low-Dimensional Function Space for Efficient Spectral Upsampling" (2019)
 * RGB colorspace: sRGB
 * Wavelength range: 360-780nm, 85 samples (5nm intervals)
 * LUT resolution: 16x16x16 (4,096 entries)
 * ---------------------------------------------------------------- */

#define JAKOB2019_WAVELENGTH_COUNT 85
#define JAKOB2019_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define JAKOB2019_WAVELENGTH_MAX ALWAN_LITERAL(780.0)

/* Jakob2019 polynomial coefficient LUT (64x64x64 resolution for sRGB) */
#define JAKOB2019_LUT_RES 64
#define JAKOB2019_LUT_SIZE (JAKOB2019_LUT_RES * JAKOB2019_LUT_RES * JAKOB2019_LUT_RES)

/* sRGB LUT (default). Dual-declared f32/f64 twins from the same CSV. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_srgb_lut_c0_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_srgb_lut_c0_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_srgb_lut_c1_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_srgb_lut_c1_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_srgb_lut_c2_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_srgb_lut_c2_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2.csv"
};
ALWAN_DIAG_POP
#endif

/* ProPhoto RGB LUT. Dual-declared f32/f64 twins. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_prophoto_lut_c0_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_prophotorgb.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_prophoto_lut_c0_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_prophotorgb.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_prophoto_lut_c1_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_prophotorgb.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_prophoto_lut_c1_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_prophotorgb.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_prophoto_lut_c2_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_prophotorgb.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_prophoto_lut_c2_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_prophotorgb.csv"
};
ALWAN_DIAG_POP
#endif

/* ACES2065-1 LUT. Dual-declared f32/f64 twins. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_aces_lut_c0_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_aces2065_1.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_aces_lut_c0_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_aces2065_1.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_aces_lut_c1_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_aces2065_1.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_aces_lut_c1_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_aces2065_1.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_aces_lut_c2_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_aces2065_1.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_aces_lut_c2_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_aces2065_1.csv"
};
ALWAN_DIAG_POP
#endif

/* Rec.2020 LUT. Dual-declared f32/f64 twins. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_rec2020_lut_c0_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_rec2020.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_rec2020_lut_c0_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_rec2020.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_rec2020_lut_c1_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_rec2020.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_rec2020_lut_c1_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_rec2020.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_rec2020_lut_c2_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_rec2020.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_rec2020_lut_c2_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_rec2020.csv"
};
ALWAN_DIAG_POP
#endif

/* eRGB LUT. Dual-declared f32/f64 twins. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_ergb_lut_c0_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_ergb.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_ergb_lut_c0_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_ergb.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_ergb_lut_c1_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_ergb.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_ergb_lut_c1_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_ergb.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_ergb_lut_c2_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_ergb.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_ergb_lut_c2_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_ergb.csv"
};
ALWAN_DIAG_POP
#endif

/* CIE XYZ LUT. Dual-declared f32/f64 twins. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_xyz_lut_c0_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_xyz.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_xyz_lut_c0_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c0_xyz.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_xyz_lut_c1_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_xyz.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_xyz_lut_c1_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c1_xyz.csv"
};
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const jakob2019_xyz_lut_c2_f32[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_xyz.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const jakob2019_xyz_lut_c2_f64[JAKOB2019_LUT_SIZE] = {
#include "../data/spectral_lut/jakob2019/jakob2019_lut_c2_xyz.csv"
};
ALWAN_DIAG_POP
#endif

/* ----------------------------------------------------------------
 * Native dual-precision instantiation of the recovery methods.
 * The dual-declared f32/f64 basis/LUT twins above are selected per
 * precision via ALWAN_CORE_FNLIT(NAME) inside the impl, so each pass
 * reads native data of its own precision (no per-element casts).
 * ---------------------------------------------------------------- */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_spectrum_upsample_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_spectrum_upsample_impl.inc"
#include "alwan_api_teardown.h"
#endif
