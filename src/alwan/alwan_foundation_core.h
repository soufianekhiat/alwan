/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2026 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The shader-safe half of the foundation (alwan_foundation.h). A core header of a library built
 * on alwan includes this one: it compiles as C, HLSL, GLSL, CUDA and OpenCL, so it carries only
 * the platform layer (backend selection, ALWAN_INLINE, the ALWAN_* math macros, ALWAN_SELECT,
 * ALWAN_DET_PRECISE, ALWAN_HAS_POINTERS), the scalar and vector types, and the paths of the
 * templating and table-reader headers. The C API (alwan.h, allocation, the map layer) comes
 * from alwan_foundation.h, which includes this file. docs/foundation.md lists the names.
 */

#ifndef ALWAN_FOUNDATION_CORE_H
#define ALWAN_FOUNDATION_CORE_H

#include "alwan_platform.h"
#include "alwan_types.h"

#define ALWAN_FOUNDATION_VERSION 1

#define ALWAN_FOUNDATION_CORE_F32_SETUP  "core/alwan_core_f32_setup.h"
#define ALWAN_FOUNDATION_CORE_F64_SETUP  "core/alwan_core_f64_setup.h"
#define ALWAN_FOUNDATION_CORE_TEARDOWN   "core/alwan_core_teardown.h"
#define ALWAN_FOUNDATION_TABLE_CORE      "core/alwan_table_core.h"
#define ALWAN_FOUNDATION_TABLE_READER    "core/alwan_table_reader.inc"
#define ALWAN_FOUNDATION_HALF_CORE       "core/alwan_half_core.h"
#define ALWAN_FOUNDATION_MICROFACET_CORE "core/alwan_microfacet_core.h"

#endif /* ALWAN_FOUNDATION_CORE_H */
