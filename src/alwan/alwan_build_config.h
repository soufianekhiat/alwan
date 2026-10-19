/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ----------------------------------------------------------------
 * Precision build selection
 * ----------------------------------------------------------------
 * Alwan exposes every numeric entry point in two precisions: a float
 * (`_f32`) and a double (`_f64`) variant, each with its own embedded data
 * tables. A user who only needs one precision can shrink both the compiled
 * code and the embedded data footprint by selecting a single-precision build.
 *
 * USER-FACING SWITCHES (define at most ONE, before including any alwan header):
 *
 *   ALWAN_BUILD_ONLY_F32   build only the single-precision (float)  API + data
 *   ALWAN_BUILD_ONLY_F64   build only the double-precision (double) API + data
 *   (neither defined)      build BOTH precisions (the default)
 *
 * These resolve to the internal gates the rest of the library tests:
 *
 *   ALWAN_WITH_F32   1 when the f32 API/data is compiled, else 0
 *   ALWAN_WITH_F64   1 when the f64 API/data is compiled, else 0
 *   ALWAN_WITH_BOTH  1 only when both are compiled
 *
 * Every dual-precision function instantiation (the `#include "*_impl.inc"`
 * pairs) and every `NAME_f32` / `NAME_f64` data twin is wrapped in
 * `#if ALWAN_WITH_F32` / `#if ALWAN_WITH_F64`. Public *declarations* and the
 * `alwan_*_f32` / `alwan_*_f64` struct typedefs remain present in all builds
 * (they cost nothing); only the *definitions* and *data* are gated, so a call
 * to an excluded-precision symbol fails at link time, not compile time.
 *
 * NOTE for contributors: `double` / `float` as C types are always available.
 * A handful of f32 public entry points are numerically f64-internal facades
 * (iterative inverses, least-squares fits) -- their private double-precision
 * helpers use the `double` type directly and are NOT gated by ALWAN_WITH_F64,
 * so they remain functional in an f32-only build.
 */

#ifndef ALWAN_BUILD_CONFIG_H
#define ALWAN_BUILD_CONFIG_H

#if defined(ALWAN_BUILD_ONLY_F32) && defined(ALWAN_BUILD_ONLY_F64)
#  error "alwan: define at most one of ALWAN_BUILD_ONLY_F32 / ALWAN_BUILD_ONLY_F64 (omit both for a dual-precision build)."
#endif

#if defined(ALWAN_BUILD_ONLY_F32)
#  define ALWAN_WITH_F32 1
#  define ALWAN_WITH_F64 0
#elif defined(ALWAN_BUILD_ONLY_F64)
#  define ALWAN_WITH_F32 0
#  define ALWAN_WITH_F64 1
#else
#  define ALWAN_WITH_F32 1
#  define ALWAN_WITH_F64 1
#endif

#define ALWAN_WITH_BOTH (ALWAN_WITH_F32 && ALWAN_WITH_F64)

/* ----------------------------------------------------------------
 * f64-internal facade exception
 * ----------------------------------------------------------------
 * A few f32 public entry points are numerically f64-only: iterative inverses
 * whose convergence thresholds are below f32 epsilon (ZCAM inverse, ACES 1.x
 * inverse) and least-squares matrix fits whose normal-equations solve squares
 * the condition number (Cheung2004 / Finlayson2015 CCM, gamut Monte-Carlo
 * reductions). Their f32 API runs the algorithm in f64 internally and narrows
 * the result. So those entry points stay AVAILABLE in an f32-only build, their
 * f64 implementation (and the f64 data it reads) is compiled regardless of the
 * precision selection: gate that machinery with ALWAN_WITH_F64_FACADE rather
 * than ALWAN_WITH_F64. (Always 1 -- at least one precision is always built.) */
#define ALWAN_WITH_F64_FACADE 1

/* ----------------------------------------------------------------
 * Refractive-index data
 * ----------------------------------------------------------------
 * The refractiveindex.info database (3,576 pages, about 35 MB of CSV source and about
 * 14 MB in the binary) is compiled in by default. Define ALWAN_WITH_REFRACTIVE_DATA to 0
 * to leave it out: alwan_refractive_index_count is then 0, every function that names a
 * database entry returns ALWAN_E_NODATA, and the functions that take the caller's own
 * indices (alwan_multilayer_tmm, alwan_reflectance_to_rgb) are unaffected. */
#ifndef ALWAN_WITH_REFRACTIVE_DATA
#  define ALWAN_WITH_REFRACTIVE_DATA 1
#endif

/* ----------------------------------------------------------------
 * Feature families
 * ----------------------------------------------------------------
 * Whole families of functions that a build can leave out, for a smaller library.
 * Each defaults to 1. Set one to 0 when compiling alwan AND the code that uses it,
 * as the precision switches are set: the family's code is not compiled and its
 * functions are not declared, so a call to one is a compile error rather than a
 * runtime surprise. Their types and enums stay declared. CMake's ALWAN_WITH_<NAME>
 * options export the value to consumers (target and alwan.pc).
 *
 *   ALWAN_WITH_FILM        the photochemical film pipeline: stock profiles, film_render,
 *                          looks, grain, halation; off also turns the film tables off
 *   ALWAN_WITH_SKY         the physical sky models (Preetham, Hosek-Wilkie, Bruneton) and
 *                          the ASTM G173 solar spectra they ship with
 *   ALWAN_WITH_NN          the neural-network layer kernels
 *   ALWAN_WITH_FORMATION   the experimental picture-formation operators and the
 *                          RGB-space fit (experimental/)
 *   ALWAN_WITH_FILE_FORMATS the LUT and luminaire file formats: .cube, CLF, spi, 3dl,
 *                          csp, IES and EULUMDAT, read and written
 *   ALWAN_WITH_PATTERNS    test patterns, test cards and palettes
 *   ALWAN_WITH_OPTICS      thin films, multilayers, iridescence, skin, fluorescence and
 *                          the refractive-index database
 *   ALWAN_WITH_GRADING     OCIO-style grading, tonescale-region grading, tone
 *                          adjustment, Hald CLUTs
 *   ALWAN_WITH_IMAGE_METRICS image comparison: alwan_image_compare, scikit-image's MSE,
 *                          NRMSE, NMI and structural_similarity, and FLIP; alwan_ssim,
 *                          alwan_psnr and their PU21 forms stay
 *
 * alwan_dev's `tools/check_precision_link.py families` builds the library with each
 * one off and links every entry point still declared. */
#ifndef ALWAN_WITH_FILM
#  define ALWAN_WITH_FILM 1
#endif
#ifndef ALWAN_WITH_SKY
#  define ALWAN_WITH_SKY 1
#endif
#ifndef ALWAN_WITH_NN
#  define ALWAN_WITH_NN 1
#endif
#ifndef ALWAN_WITH_FORMATION
#  define ALWAN_WITH_FORMATION 1
#endif
#ifndef ALWAN_WITH_FILE_FORMATS
#  define ALWAN_WITH_FILE_FORMATS 1
#endif
#ifndef ALWAN_WITH_PATTERNS
#  define ALWAN_WITH_PATTERNS 1
#endif
#ifndef ALWAN_WITH_OPTICS
#  define ALWAN_WITH_OPTICS 1
#endif
#ifndef ALWAN_WITH_GRADING
#  define ALWAN_WITH_GRADING 1
#endif
#ifndef ALWAN_WITH_IMAGE_METRICS
#  define ALWAN_WITH_IMAGE_METRICS 1
#endif
#if !ALWAN_WITH_OPTICS && ALWAN_WITH_REFRACTIVE_DATA
#  undef ALWAN_WITH_REFRACTIVE_DATA
#  define ALWAN_WITH_REFRACTIVE_DATA 0
#endif

/* ----------------------------------------------------------------
 * Keep the default `alwan_scalar` precision consistent with the build.
 * A single-precision build forces the matching default scalar so that
 * `alwan_scalar` always names a precision that is actually compiled in.
 * ---------------------------------------------------------------- */
#if defined(ALWAN_BUILD_ONLY_F32)
#  if defined(ALWAN_SCALAR_IS_FLOAT) && (ALWAN_SCALAR_IS_FLOAT == 0)
#    error "alwan: ALWAN_BUILD_ONLY_F32 conflicts with ALWAN_SCALAR_IS_FLOAT=0 (the f64 scalar is not built)."
#  endif
#  ifndef ALWAN_SCALAR_IS_FLOAT
#    define ALWAN_SCALAR_IS_FLOAT 1
#  endif
#elif defined(ALWAN_BUILD_ONLY_F64)
#  if defined(ALWAN_SCALAR_IS_FLOAT) && (ALWAN_SCALAR_IS_FLOAT == 1)
#    error "alwan: ALWAN_BUILD_ONLY_F64 conflicts with ALWAN_SCALAR_IS_FLOAT=1 (the f32 scalar is not built)."
#  endif
#endif

#endif /* ALWAN_BUILD_CONFIG_H */
