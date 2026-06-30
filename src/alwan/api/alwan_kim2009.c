/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Kim, Weyrich and Kautz (2009) Color Appearance Model Implementation
 * Based on: Kim, M. H., Weyrich, T., & Kautz, J. (2009). Modeling Human Color Perception
 * under Extended Luminance Levels. ACM Transactions on Graphics, 28(3), 27:1-27:9.
 *
 * Resolves viewing conditions; see alwan_kim2009_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_kim2009_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_kim2009_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_kim2009_impl.inc"
#include "alwan_api_teardown.h"
#endif
