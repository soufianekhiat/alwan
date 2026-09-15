/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Global tone mapping operators, the eleven of colour-hdri, over a whole image. The
 * image is read in either precision and the operators compute in double: the
 * implementation is templated in alwan_tonemap_impl.inc and instantiated once per
 * precision.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <float.h>
#include <stddef.h>
#include <string.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f32_setup.h"
#include "alwan_tonemap_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION
#include "alwan_api_f64_setup.h"
#include "alwan_tonemap_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif
