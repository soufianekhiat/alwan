/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Image-based lighting precomputation for equirectangular environment maps: projection
 * to real spherical harmonics and the irradiance of Ramamoorthi and Hanrahan ("An
 * efficient representation for irradiance environment maps", SIGGRAPH 2001), GGX
 * prefiltering and the split-sum table of Karis ("Real shading in Unreal Engine 4",
 * SIGGRAPH 2013 course notes), with filtered importance sampling (Colbert and Krivanek,
 * GPU Gems 3, 2007), and the average energy for the multiple-scattering compensation of
 * Kulla and Conty (2017). The per-direction math is in core/alwan_ibl_core.inc and the
 * table readers in core/alwan_ibl_reader.inc. Suite 286.
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include <string.h>
#include "../core/alwan_table_core.h"
#include "../core/alwan_ibl_core.h"

#define ALWAN__SH_MAX_COEFFS ((ALWAN_SH_MAX_BAND + 1) * (ALWAN_SH_MAX_BAND + 1))
#define ALWAN__SH_MAX_CHANNELS 16

size_t alwan_sh_coefficient_count(int band) {
    if (band < 0 || band > ALWAN_SH_MAX_BAND) return 0;
    return (size_t)(band + 1) * (size_t)(band + 1);
}

#if ALWAN_WITH_F32
#include "alwan_api_f32_setup.h"
#include "alwan_ibl_impl.inc"
#include "alwan_api_teardown.h"
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_ibl_impl.inc"
#include "alwan_api_teardown.h"
#endif
