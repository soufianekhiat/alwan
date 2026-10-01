/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * 2D importance sampling of a greyscale image: the marginal and conditional
 * distributions, sampled through tabulated inverse CDFs (DIRECT) or by CDF
 * bisection (SEARCH). The sampling itself is in
 * core/alwan_importance_sampling_reader.inc. Suite 280.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include "../core/alwan_importance_sampling_core.h"

#if ALWAN_WITH_F32
#include "alwan_api_f32_setup.h"
#include "alwan_importance_sampling_impl.inc"
#include "alwan_api_teardown.h"
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_importance_sampling_impl.inc"
#include "alwan_api_teardown.h"
#endif
