/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * ProLab Color Space (Perceptually Uniform Projective)
 * See alwan_prolab_core.h
 *
 * Reference: Konovalenko et al. (2021)
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_prolab_core.h"

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_prolab_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_prolab_impl.inc"
#include "alwan_api_teardown.h"
