/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Exposure-bracket merging (colour-hdri's weighted radiance merge). Native dual precision:
 * templated in alwan_hdr_merge_impl.inc and instantiated once per precision. Split from
 * alwan_exposure.c so it moves to suwar as a file.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f32_setup.h"
#include "alwan_hdr_merge_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

/* Compiled in every build: camera response recovery (alwan_crf.c) computes in f64
 * behind its f32 entry points and calls this pass. See ALWAN_WITH_F64_FACADE. */
#if ALWAN_WITH_F64_FACADE
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_hdr_merge_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif
