/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only tonescale-region grading: the region weight and the per-region
 * blend of Canham, Punnappurath and Brown 2025, value-returning for
 * cross-platform (C/HLSL/GLSL/Halide) use. The colour conversions around them
 * (decode, Lab, offset, encode) live in api/alwan_tonescale_grade.c, since
 * they need a space descriptor and its transfer functions.
 */

#ifndef ALWAN_TONESCALE_GRADE_CORE_H
#define ALWAN_TONESCALE_GRADE_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_tonescale_grade_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_tonescale_grade_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / Halide: single precision, the same two functions */

ALWAN_INLINE alwan_scalar alwan_tonescale_region_weight_v(alwan_scalar intensity, alwan_scalar pivot,
                                                          alwan_scalar slope) {
    alwan_scalar w = ALWAN_LITERAL(1.0) + slope * (intensity - pivot);
    w = ALWAN_SELECT(w < ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), w);
    return ALWAN_SELECT(w > ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), w);
}

ALWAN_INLINE alwan_scalar alwan_tonescale_region_blend_v(alwan_scalar value, alwan_scalar adjusted,
                                                         alwan_scalar weight) {
    return value + weight * (adjusted - value);
}

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_TONESCALE_GRADE_CORE_H */
