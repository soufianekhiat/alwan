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
 *
 * The tables themselves are not embedded here. They are declared in
 * data/alwan_data_tables.h, defined once per precision in
 * data/alwan_data_tables_api.c, and read through alwan_table1d_row in
 * core/alwan_table_core. Both headers must be included BEFORE the per-
 * precision setup below: core/alwan_table_core.h runs its own
 * setup/teardown pair, which would undefine the ALWAN_CORE_* macros the
 * impl .inc is being compiled with.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_spd_core.h"
#include "../core/alwan_table_core.h"
#include "../data/alwan_data_tables.h"
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
/* Compiled in every build, not just an f64 one: the documented f64-internal
 * facades (the spectral quality metrics, the CCM fits, the iterative inverses)
 * call this f64 API from their f32 entry points, so it has to exist even when
 * the f64 public surface is otherwise excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_spd_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif /* ALWAN_WITH_F64_FACADE */
