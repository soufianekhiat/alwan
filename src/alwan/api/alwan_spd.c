/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Spectral Power Distributions (SPD)
 *
 * Native dual-precision: the entire SPD implementation (create/destroy/
 * resample/illuminant/blackbody/camera_sensitivity/xyz_from_spd/
 * xyz_from_spd_camera/analyze_shape, plus the interpolation and Simpson/
 * Trapezoid integrators) is templated over ALWAN_CORE_T in
 * alwan_spd_impl.inc and instantiated once per precision below. The
 * embedded illuminant / CMF / camera-sensitivity tables are emitted as
 * native float arrays in the f32 pass and native double arrays in the f64
 * pass, so the single-precision path carries no widen->f64->narrow facade:
 * it integrates in float over float data throughout. The f64 pass is
 * byte-for-byte equivalent to the former hand-written double code.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_spd_core.h"
#include <string.h>

/* f32 pass: native float tables + float integration. The CSV tables hold
 * double-precision decimal literals narrowed to float (guarded), and some
 * file-static helpers may go unreferenced under a single instantiation. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f32_setup.h"
#include "alwan_spd_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* f64 pass: native double tables + double integration. */
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_spd_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif
