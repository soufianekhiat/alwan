/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Extended Color Space Conversions
 * IgPgTg, ICaCb, hdr-CIELAB, hdr-IPT, UCS, UVW, Hunter Lab, ProLab,
 * OSA-UCS, Prismatic, HCL, IHLS, DIN99
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_extended_core.h"
#include "../core/alwan_colorspace_core.h"
#include "../core/alwan_din99_core.h"
#include "../core/alwan_hunter_lab_core.h"
#include "../core/alwan_prolab_core.h"
#include "../core/alwan_osa_ucs_core.h"

#if ALWAN_WITH_F32
/* === f32 pass === */
#define ALWAN_MAP_F32
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_extended_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

#if ALWAN_WITH_F64
/* === f64 pass === */
#define ALWAN_MAP_F64
#include "alwan_map_simd_defs.h"
#include "alwan_map_simd_helpers.inc"
#include "alwan_extended_map_kernels.inc"
#include "alwan_map_simd_undef.h"
#endif

/* ================================================================
 * Backward-compatible kernel aliases (unsuffixed -> compile-time selected)
 * ================================================================ */

#define alwan__hcl_to_rgb_kernel                                 alwan__hcl_to_rgb_kernel_f64
#define alwan__hdr_cielab_to_xyz_kernel                          alwan__hdr_cielab_to_xyz_kernel_f64
#define alwan__hdr_ipt_to_xyz_kernel                             alwan__hdr_ipt_to_xyz_kernel_f64
#define alwan__hunter_lab_to_xyz_custom_kernel                   alwan__hunter_lab_to_xyz_custom_kernel_f64
#define alwan__hunter_lab_to_xyz_kernel                          alwan__hunter_lab_to_xyz_kernel_f64
#define alwan__icacb_to_xyz_kernel                               alwan__icacb_to_xyz_kernel_f64
#define alwan__igpgtg_to_xyz_kernel                              alwan__igpgtg_to_xyz_kernel_f64
#define alwan__ihls_to_rgb_kernel                                alwan__ihls_to_rgb_kernel_f64
#define alwan__osa_ucs_to_xyz_kernel                             alwan__osa_ucs_to_xyz_kernel_f64
#define alwan__prismatic_to_rgb_kernel                           alwan__prismatic_to_rgb_kernel_f64
#define alwan__prolab_to_xyz_custom_kernel                       alwan__prolab_to_xyz_custom_kernel_f64
#define alwan__prolab_to_xyz_kernel                              alwan__prolab_to_xyz_kernel_f64
#define alwan__rgb_to_hcl_kernel                                 alwan__rgb_to_hcl_kernel_f64
#define alwan__rgb_to_ihls_kernel                                alwan__rgb_to_ihls_kernel_f64
#define alwan__rgb_to_prismatic_kernel                           alwan__rgb_to_prismatic_kernel_f64
#define alwan__ucs_to_xyz_kernel                                 alwan__ucs_to_xyz_kernel_f64
#define alwan__uvw_to_xyz_kernel                                 alwan__uvw_to_xyz_kernel_f64
#define alwan__xyz_to_hdr_cielab_kernel                          alwan__xyz_to_hdr_cielab_kernel_f64
#define alwan__xyz_to_hdr_ipt_kernel                             alwan__xyz_to_hdr_ipt_kernel_f64
#define alwan__xyz_to_hunter_lab_custom_kernel                   alwan__xyz_to_hunter_lab_custom_kernel_f64
#define alwan__xyz_to_hunter_lab_kernel                          alwan__xyz_to_hunter_lab_kernel_f64
#define alwan__xyz_to_icacb_kernel                               alwan__xyz_to_icacb_kernel_f64
#define alwan__xyz_to_igpgtg_kernel                              alwan__xyz_to_igpgtg_kernel_f64
#define alwan__xyz_to_osa_ucs_kernel                             alwan__xyz_to_osa_ucs_kernel_f64
#define alwan__xyz_to_prolab_custom_kernel                       alwan__xyz_to_prolab_custom_kernel_f64
#define alwan__xyz_to_prolab_kernel                              alwan__xyz_to_prolab_kernel_f64
#define alwan__xyz_to_ucs_kernel                                 alwan__xyz_to_ucs_kernel_f64
#define alwan__xyz_to_uvw_kernel                                 alwan__xyz_to_uvw_kernel_f64
#define alwan__lab_to_din99_kernel                               alwan__lab_to_din99_kernel_f64
#define alwan__din99_to_lab_kernel                               alwan__din99_to_lab_kernel_f64

