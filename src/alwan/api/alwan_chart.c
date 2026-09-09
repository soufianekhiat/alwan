/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Measured chart files.
 *
 * A ColorChecker has published values because every one of them is meant to
 * be the same chart. A professional target does not: an IT8 or a DT NGT2 is
 * measured per sheet or per batch, two off the same press differ, and a
 * chart fades. ISO 12641 fixes an IT8's layout and leaves its colorimetry to
 * the manufacturer, which is why alwan embeds the layout and none of the
 * numbers. The numbers ship with the target, in a file.
 *
 * This reads that file. OpenQualia's Measurement File Standard is
 * CGATS.17-2009 with a fixed set of header keys, carried as .oqm.txt beside
 * the target or reached from the QR label printed on it, and the same parser
 * reads a plain CGATS batch reference file. So a caller with a target in
 * hand gets its real values, and alwan needs no network, no per-vendor table
 * and no licence to data it cannot redistribute.
 *
 * SERIAL is a header key like any other, which is what makes a serial lookup
 * an application's job rather than this library's: read the QR, scan a
 * directory, compare alwan_chart_header(c, "SERIAL"), and on a miss send the
 * user to the measurement page.
 *
 * Native dual-precision: the implementation is templated over ALWAN_CORE_T
 * in alwan_chart_impl.inc and instantiated once per precision. The lexer is
 * precision-independent and lives in alwan_chart_common.h, included once.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_colorspace_core.h"   /* alwan_lab_to_xyz_{T}_v, for the LAB_* path */
#include "alwan_chart_common.h"
#include <stdio.h>
#include <string.h>

/* f32 pass */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f32_setup.h"
#include "alwan_chart_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* f64 pass. Compiled in every build for the same reason the SPD one is: the
 * f64-internal facades call it from their f32 entry points. */
#if ALWAN_WITH_F64_FACADE
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_chart_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif
