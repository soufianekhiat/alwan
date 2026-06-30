/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only Nayatani (1995) Color Appearance Model.
 * Dual-precision (f32/f64) value-returning core; C backend only.
 *
 * References:
 * - Nayatani, Y., Sobagaki, H., & Yano, K. H. T. (1995). Color Research &
 *   Application, 20(3), 156-167.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 */

#ifndef ALWAN_NAYATANI95_CORE_H
#define ALWAN_NAYATANI95_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C
/* ================================================================
 * Dual-Precision: emit f32 and f64 variants from shared .inc
 * ================================================================ */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_nayatani95_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_nayatani95_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_NAYATANI95_CORE_H */
