/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Oklab & Oklch Color Spaces
 *
 * Reference: Bjorn Ottosson (2020)
 * "A perceptual color space for image processing"
 * https://bottosson.github.io/posts/oklab/
 *
 * See alwan_oklab_core.h.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_oklab_core.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_oklab_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* Compiled in every build: the f64-internal facades reach this f64 API from
 * their f32 entry points, so it exists even when the f64 public surface is
 * excluded. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
#include "alwan_api_f64_setup.h"
#include "alwan_oklab_impl.inc"
#include "alwan_api_teardown.h"
#endif /* ALWAN_WITH_F64_FACADE */
