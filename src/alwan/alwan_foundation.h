/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2026 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * The foundation: the part of alwan's machinery that a library built ON alwan may use, beside
 * the public API in alwan.h. Supported from 3.0.0, under the same compatibility rules as
 * alwan.h; docs/foundation.md says what is in it, what is not, and why. Suwar (image
 * processing) is built on it.
 *
 * Include this instead of alwan_internal.h, which stays alwan's own and may change at any
 * time. Everything here reaches only alwan.h's public closure plus the headers named below.
 *
 *   Allocation        alwan_ctx_alloc / alwan_ctx_free (declared in alwan.h),
 *                     alwan_safe_array_size, ALWAN_ALLOC / ALWAN_FREE (alwan_config.h)
 *   Platform, math    ALWAN_INLINE, ALWAN_LITERAL, ALWAN_SELECT, ALWAN_DIAG_* (alwan_platform.h)
 *                     and the ALWAN_* math macros routed through the deterministic build
 *                     (alwan_math.h), so a library's own code is deterministic exactly when
 *                     alwan's is.
 *   Core templating   ALWAN_FOUNDATION_CORE_F32_SETUP / _F64_SETUP / _TEARDOWN: the headers
 *                     that instantiate a core .inc once per precision (ALWAN_CORE_T,
 *                     ALWAN_CORE_FNVLIT, ALWAN_CORE_LITERAL, the ALWAN_CORE_* math). They are
 *                     included once per pass, so they are named as computed includes:
 *                         #include ALWAN_FOUNDATION_CORE_F32_SETUP
 *                         #include "my_core.inc"
 *                         #include ALWAN_FOUNDATION_CORE_TEARDOWN
 *   Table readers     ALWAN_FOUNDATION_TABLE_CORE (alwan_table_coord, alwan_table_cell) and
 *                     ALWAN_FOUNDATION_TABLE_READER (the accessor seam ALWAN_TABLE_READ(i),
 *                     included once per table), so a core reads its tables the same way on
 *                     the CPU and in a shader.
 *   Half floats       ALWAN_FOUNDATION_HALF_CORE (IEEE binary16 conversion cores).
 *   Microfacet terms  ALWAN_FOUNDATION_MICROFACET_CORE (GGX D, Smith Lambda and G2, GGX
 *                     half-vector sampling), shared with alwan's iridescence.
 *   Typed map layer   with ALWAN_FOUNDATION_WITH_MAP defined before including this header:
 *                     map/alwan_map_internal.h (typed per-pixel load/store over the pixel
 *                     formats, alwan__f16_to_f32 / alwan__f32_to_f16, the tile helpers). Its
 *                     names keep their alwan__ spelling; the supported subset is listed in
 *                     docs/foundation.md.
 *   SIMD              with ALWAN_FOUNDATION_WITH_SIMD defined: simd/alwan_simd.h (the
 *                     AVX / AVX2 / SSE2 / NEON / scalar lane wrappers). The map layer pulls
 *                     it in by itself.
 *
 * Build: add alwan's src/alwan directory to the include path (the computed includes above are
 * relative to it).
 */

#ifndef ALWAN_FOUNDATION_H
#define ALWAN_FOUNDATION_H

#include "alwan.h"
#include "alwan_math.h"
#include <stddef.h>
#include <stdint.h>

#define ALWAN_FOUNDATION_VERSION 1

/* count x elem_size, or 0 when the product would overflow size_t (or elem_size is 0). */
static inline size_t alwan_safe_array_size(size_t count, size_t elem_size) {
    if (elem_size == 0) return 0;
    if (count > SIZE_MAX / elem_size) return 0;
    return count * elem_size;
}

#define ALWAN_FOUNDATION_CORE_F32_SETUP "core/alwan_core_f32_setup.h"
#define ALWAN_FOUNDATION_CORE_F64_SETUP "core/alwan_core_f64_setup.h"
#define ALWAN_FOUNDATION_CORE_TEARDOWN  "core/alwan_core_teardown.h"
#define ALWAN_FOUNDATION_TABLE_CORE     "core/alwan_table_core.h"
#define ALWAN_FOUNDATION_TABLE_READER   "core/alwan_table_reader.inc"
#define ALWAN_FOUNDATION_HALF_CORE      "core/alwan_half_core.h"
#define ALWAN_FOUNDATION_MICROFACET_CORE "core/alwan_microfacet_core.h"

#if defined(ALWAN_FOUNDATION_WITH_MAP)
#include "map/alwan_map_internal.h"
#endif

#if defined(ALWAN_FOUNDATION_WITH_SIMD)
#include "simd/alwan_simd.h"
#endif

#endif /* ALWAN_FOUNDATION_H */
