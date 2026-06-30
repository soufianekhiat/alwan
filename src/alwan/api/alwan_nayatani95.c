/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Nayatani (1995) Color Appearance Model.
 *
 * References:
 * - Nayatani, Y., Sobagaki, H., & Yano, K. H. T. (1995). Color Research &
 *   Application, 20(3), 156-167.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 *
 * See alwan_nayatani95_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_nayatani95_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_nayatani95_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_nayatani95_impl.inc"
#include "alwan_api_teardown.h"
#endif
