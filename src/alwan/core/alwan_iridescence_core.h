/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Header-only thin-film iridescence for rendering, after Belcour and Barla 2017 ("A
 * Practical Extension to Microfacet Theory for the Modeling of Varying Iridescence"): the
 * Fresnel amplitudes of the two interfaces of a dielectric film over a dielectric or
 * conducting base, the Airy reflectance at a phase, and, in alwan_iridescence_reader.inc,
 * the Airy series integrated against the colour matching functions through a table of
 * their Fourier transform, read through the ALWAN_IRIDESCENCE_READ accessor so a shader
 * evaluates the same arithmetic as alwan_iridescence_fresnel_rgb.
 * Cross-platform (C / CUDA / OpenCL / HLSL / GLSL / Halide).
 */

#ifndef ALWAN_IRIDESCENCE_CORE_H
#define ALWAN_IRIDESCENCE_CORE_H

#include "../alwan_platform.h"
#include "../alwan_types.h"

#if ALWAN_BACKEND == ALWAN_BACKEND_C

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_UNUSED_FUNCTION

/* f32 pass */
#include "alwan_core_f32_setup.h"
#include "alwan_iridescence_core.inc"
#include "alwan_core_teardown.h"

/* f64 pass */
#include "alwan_core_f64_setup.h"
#include "alwan_iridescence_core.inc"
#include "alwan_core_teardown.h"

ALWAN_DIAG_POP

#else /* HLSL / GLSL / OpenCL / Halide: the single-precision pass */

#include "alwan_iridescence_core.inc"

#endif /* ALWAN_BACKEND */

#endif /* ALWAN_IRIDESCENCE_CORE_H */
