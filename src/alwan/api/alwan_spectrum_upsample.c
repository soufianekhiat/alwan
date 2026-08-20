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
 * rather than widening to double.
 *
 * The Smits/Mallett basis spectra live here. The Jakob2019 coefficient cubes do
 * not: they are declared in data/alwan_data_tables.h and defined in
 * data/alwan_data_tables_spectral.c, which keeps ~110 MB of CSV preprocessing
 * out of this translation unit.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_table_core.h"
#include "../data/alwan_data_tables.h"
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
 * LUT resolution: 64x64x64 per coefficient, six gamuts
 * ---------------------------------------------------------------- */

#define JAKOB2019_WAVELENGTH_COUNT 85
#define JAKOB2019_WAVELENGTH_MIN ALWAN_LITERAL(360.0)
#define JAKOB2019_WAVELENGTH_MAX ALWAN_LITERAL(780.0)

/* The Jakob2019 coefficient cubes and their extents
 * (ALWAN_TABLE_JAKOB2019_RES / _SIZE) are declared in
 * data/alwan_data_tables.h and defined in data/alwan_data_tables_spectral.c.
 * Moving them out of this file sheds ~110 MB of preprocessing: 18 cubes, each
 * #included once per precision from the same CSV. */

/* ----------------------------------------------------------------
 * Native dual-precision instantiation of the recovery methods.
 * The dual-declared f32/f64 twins -- the Smits/Mallett basis spectra above and
 * the Jakob2019 cubes in data/ -- are selected per precision via
 * ALWAN_CORE_FNLIT(NAME) inside the impl, so each pass reads native data of
 * its own precision (no per-element casts).
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
