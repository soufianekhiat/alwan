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
 * No payload lives here any more. The Smits1999 and Mallett2019 basis spectra
 * and the Jakob2019 coefficient cubes are all declared in
 * data/alwan_data_tables.h: the basis spectra are defined in
 * data/alwan_data_tables_api.c, the cubes in data/alwan_data_tables_spectral.c,
 * which keeps ~110 MB of CSV preprocessing out of this translation unit. What
 * remains below is the sampling geometry -- range, count and step -- that the
 * impl needs to build an alwan_spd around the data.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_table_core.h"
#include "../data/alwan_data_tables.h"
#include <string.h>
#include <math.h>

/* ----------------------------------------------------------------
 * Smits1999 Basis Spectra Geometry
 * Based on: Smits, Brian. "An RGB to Spectrum Conversion for Reflectances" (1999)
 * White point: CIE Illuminant E (equal energy)
 * RGB colorspace: sRGB primaries
 * Wavelength range: 380-720nm, 10 samples
 *
 * The seven basis spectra are ALWAN_TABLE_SMITS1999_* in
 * data/alwan_data_tables.h and are read through alwan_table1d_row.
 * ---------------------------------------------------------------- */

#define SMITS1999_WAVELENGTH_COUNT 10
#define SMITS1999_WAVELENGTH_MIN ALWAN_LITERAL(380.0)
#define SMITS1999_WAVELENGTH_MAX ALWAN_LITERAL(720.0)

/* Wavelengths. Not a table read by anything: the spectra are uniformly spaced,
 * so the impl derives every wavelength from MIN/MAX/COUNT and no reader ever
 * subscripts this. It stays local and un-migrated for that reason -- the
 * registry homes tables that are READ, and a constant with no reader would have
 * to name one. Kept as the documented provenance of the sample grid. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const smits1999_wavelengths[SMITS1999_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/smits1999/wavelengths.csv"
};
ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Mallett2019 Basis Functions Geometry
 * Based on: Mallett & Yuksel. "Spectral Primary Decomposition for Rendering with sRGB Reflectance" (2019)
 * RGB colorspace: sRGB
 * Wavelength range: 380-780nm, 81 samples (5nm intervals)
 *
 * The three basis functions are ALWAN_TABLE_MALLETT2019_* in
 * data/alwan_data_tables.h and are read through alwan_table1d_row.
 * ---------------------------------------------------------------- */

#define MALLETT2019_WAVELENGTH_COUNT 81
#define MALLETT2019_WAVELENGTH_MIN ALWAN_LITERAL(380.0)
#define MALLETT2019_WAVELENGTH_MAX ALWAN_LITERAL(780.0)

/* Wavelengths. Same story as smits1999_wavelengths above: uniformly spaced,
 * never subscripted, kept local as provenance. */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const mallett2019_wavelengths[MALLETT2019_WAVELENGTH_COUNT] = {
#include "../data/spectral_basis/mallett2019/wavelengths.csv"
};
ALWAN_DIAG_POP

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
 * The dual-declared f32/f64 twins -- the Smits/Mallett basis spectra and the
 * Jakob2019 cubes, all in data/ -- are selected per precision via
 * ALWAN_CORE_FNLIT(NAME) inside the impl, so each pass reads native data of
 * its own precision (no per-element casts). Every read goes through a reader
 * in core/alwan_table_core, which is header-only: this build sets no /GL and
 * no /LTCG, and upsampling runs per pixel, so a reader in a .c would be a real
 * call per sample.
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
