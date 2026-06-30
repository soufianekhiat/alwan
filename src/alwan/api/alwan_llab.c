/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * LLAB Color Appearance Model
 * Based on Luo, Lo and Kuo (1996)
 * "The LLAB(l:c) colour model"
 *
 * References:
 * - Luo, M. R., Lo, M.-C., & Kuo, W.-G. (1996). The LLAB(l:c) colour model.
 *   Color Research & Application, 21(6), 412-429.
 * - Fairchild, M. D. (2013). Color Appearance Models (3rd ed.). Wiley.
 *
 * See alwan_llab_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_llab_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_llab_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_llab_impl.inc"
#include "alwan_api_teardown.h"
#endif
