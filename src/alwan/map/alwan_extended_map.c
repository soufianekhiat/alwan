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

/* ----------------------------------------------------------------
 * XYZ <-> IgPgTg
 * ---------------------------------------------------------------- */

int alwan_xyz_to_igpgtg_map_interleave(alwan_scalar *igpgtg_out, alwan_scalar const *xyz_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !igpgtg_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)igpgtg_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_igpgtg r = alwan_xyz_to_igpgtg_v(xyz);
        out_ptr[0] = r.Ig; out_ptr[1] = r.Pg; out_ptr[2] = r.Tg;
    }
    return ALWAN_OK;
}

int alwan_igpgtg_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *igpgtg_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!igpgtg_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)igpgtg_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_igpgtg igpgtg = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_igpgtg_to_xyz_v(igpgtg);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> ICaCb
 * ---------------------------------------------------------------- */

int alwan_xyz_to_icacb_map_interleave(alwan_scalar *icacb_out, alwan_scalar const *xyz_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !icacb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)icacb_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_icacb r = alwan_xyz_to_icacb_v(xyz);
        out_ptr[0] = r.I; out_ptr[1] = r.Ca; out_ptr[2] = r.Cb;
    }
    return ALWAN_OK;
}

int alwan_icacb_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *icacb_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!icacb_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)icacb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_icacb icacb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_icacb_to_xyz_v(icacb);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> hdr-CIELAB
 * ---------------------------------------------------------------- */

int alwan_xyz_to_hdr_cielab_map_interleave(alwan_scalar *hdr_lab_out, alwan_scalar const *xyz_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hdr_lab_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hdr_lab_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lab r = alwan_xyz_to_hdr_cielab_v(xyz);
        out_ptr[0] = r.L; out_ptr[1] = r.a; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

int alwan_hdr_cielab_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hdr_lab_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!hdr_lab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hdr_lab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_lab hdr_lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_hdr_cielab_to_xyz_v(hdr_lab);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> hdr-IPT
 * ---------------------------------------------------------------- */

int alwan_xyz_to_hdr_ipt_map_interleave(alwan_scalar *hdr_ipt_out, alwan_scalar const *xyz_in,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hdr_ipt_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hdr_ipt_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ipt r = alwan_xyz_to_hdr_ipt_v(xyz);
        out_ptr[0] = r.I; out_ptr[1] = r.P; out_ptr[2] = r.T;
    }
    return ALWAN_OK;
}

int alwan_hdr_ipt_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hdr_ipt_in,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!hdr_ipt_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hdr_ipt_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_ipt hdr_ipt = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_hdr_ipt_to_xyz_v(hdr_ipt);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> CIE 1960 UCS
 * ---------------------------------------------------------------- */

int alwan_xyz_to_ucs_map_interleave(alwan_scalar *ucs_out, alwan_scalar const *xyz_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !ucs_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)ucs_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_ucs r = alwan_xyz_to_ucs_v(xyz);
        out_ptr[0] = r.U; out_ptr[1] = r.V; out_ptr[2] = r.W;
    }
    return ALWAN_OK;
}

int alwan_ucs_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *ucs_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!ucs_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)ucs_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_ucs ucs = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_ucs_to_xyz_v(ucs);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> OSA-UCS
 * ---------------------------------------------------------------- */

int alwan_xyz_to_osa_ucs_map_interleave(alwan_scalar *osa_out, alwan_scalar const *xyz_in,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !osa_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)osa_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_osa_ucs r = alwan_xyz_to_osa_ucs_v(xyz);
        out_ptr[0] = r.L; out_ptr[1] = r.j; out_ptr[2] = r.g;
    }
    return ALWAN_OK;
}

int alwan_osa_ucs_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *osa_in,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!osa_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)osa_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_osa_ucs osa = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_osa_ucs_to_xyz_v(osa);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> Hunter Lab
 * ---------------------------------------------------------------- */

int alwan_xyz_to_hunter_lab_map_interleave(alwan_scalar *hl_out, alwan_scalar const *xyz_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hl_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hl_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hunter_lab r = alwan_xyz_to_hunter_lab_v(xyz);
        out_ptr[0] = r.L; out_ptr[1] = r.a; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

int alwan_hunter_lab_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hl_in,
                                 size_t count, size_t in_stride, size_t out_stride) {
    if (!hl_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hl_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_hunter_lab hl = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_hunter_lab_to_xyz_v(hl);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

int alwan_xyz_to_hunter_lab_custom_map_interleave(alwan_scalar *hl_out, alwan_scalar const *xyz_in,
                                        alwan_xyz const *white_xyz,
                                        size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !hl_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)hl_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_hunter_lab r = alwan_xyz_to_hunter_lab_custom_v(xyz, *white_xyz);
        out_ptr[0] = r.L; out_ptr[1] = r.a; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

int alwan_hunter_lab_to_xyz_custom_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *hl_in,
                                        alwan_xyz const *white_xyz,
                                        size_t count, size_t in_stride, size_t out_stride) {
    if (!hl_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)hl_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_hunter_lab hl = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_hunter_lab_to_xyz_custom_v(hl, *white_xyz);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> ProLab
 * ---------------------------------------------------------------- */

int alwan_xyz_to_prolab_map_interleave(alwan_scalar *prolab_out, alwan_scalar const *xyz_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !prolab_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)prolab_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_prolab r = alwan_xyz_to_prolab_v(xyz);
        out_ptr[0] = r.L; out_ptr[1] = r.a; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

int alwan_prolab_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *prolab_in,
                             size_t count, size_t in_stride, size_t out_stride) {
    if (!prolab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)prolab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_prolab prolab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_prolab_to_xyz_v(prolab);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

int alwan_xyz_to_prolab_custom_map_interleave(alwan_scalar *prolab_out, alwan_scalar const *xyz_in,
                                    alwan_xyz const *white_xyz,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !prolab_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)prolab_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_prolab r = alwan_xyz_to_prolab_custom_v(xyz, *white_xyz);
        out_ptr[0] = r.L; out_ptr[1] = r.a; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

int alwan_prolab_to_xyz_custom_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *prolab_in,
                                    alwan_xyz const *white_xyz,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!prolab_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)prolab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_prolab prolab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_prolab_to_xyz_custom_v(prolab, *white_xyz);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * XYZ <-> UVW (with white point)
 * ---------------------------------------------------------------- */

int alwan_xyz_to_uvw_map_interleave(alwan_scalar *uvw_out, alwan_scalar const *xyz_in,
                          alwan_xyz const *white_xyz,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !uvw_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)xyz_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)uvw_out + i * out_stride);
        alwan_xyz xyz = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_uvw r = alwan_xyz_to_uvw_v(xyz, *white_xyz);
        out_ptr[0] = r.U; out_ptr[1] = r.V; out_ptr[2] = r.W;
    }
    return ALWAN_OK;
}

int alwan_uvw_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *uvw_in,
                          alwan_xyz const *white_xyz,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!uvw_in || !xyz_out || !white_xyz || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)uvw_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)xyz_out + i * out_stride);
        alwan_uvw uvw = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_xyz r = alwan_uvw_to_xyz_v(uvw, *white_xyz);
        out_ptr[0] = r.x; out_ptr[1] = r.y; out_ptr[2] = r.z;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> Prismatic
 * ---------------------------------------------------------------- */

int alwan_rgb_to_prismatic_map_interleave(alwan_scalar *prismatic_out, alwan_scalar const *rgb_in,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !prismatic_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)prismatic_out + i * out_stride);
        alwan_rgb rgb = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_prismatic r = alwan_rgb_to_prismatic_v(rgb);
        out_ptr[0] = r.L; out_ptr[1] = r.s; out_ptr[2] = r.h;
    }
    return ALWAN_OK;
}

int alwan_prismatic_to_rgb_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *prismatic_in,
                                size_t count, size_t in_stride, size_t out_stride) {
    if (!prismatic_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)prismatic_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
        alwan_prismatic prismatic = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_rgb r = alwan_prismatic_to_rgb_v(prismatic);
        out_ptr[0] = r.r; out_ptr[1] = r.g; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> HCL
 * ---------------------------------------------------------------- */

int alwan_rgb_to_hcl_map_interleave(alwan_scalar *hcl_out, alwan_scalar const *rgb_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !hcl_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        for (size_t i = 0; i < tile; i++) {
            alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_hcl r = alwan_rgb_to_hcl_v(rgb);
            d0[i] = (alwan_simd_lane)r.H; d1[i] = (alwan_simd_lane)r.C; d2[i] = (alwan_simd_lane)r.L;
        }
        ALWAN_MAP_NORM_AFFINE(d0, tile, 0.15915494309189533577, 0.5);
        alwan__store_tile_aos3(hcl_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_hcl_to_rgb_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *hcl_in,
                          size_t count, size_t in_stride, size_t out_stride) {
    if (!hcl_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, hcl_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_AFFINE(c0, tile, 6.28318530717958647692, -3.14159265358979323846);
        for (size_t i = 0; i < tile; i++) {
            alwan_hcl hcl = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_hcl_to_rgb_v(hcl);
            d0[i] = (alwan_simd_lane)r.r; d1[i] = (alwan_simd_lane)r.g; d2[i] = (alwan_simd_lane)r.b;
        }
        alwan__store_tile_aos3(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * RGB <-> IHLS
 * ---------------------------------------------------------------- */

int alwan_rgb_to_ihls_map_interleave(alwan_scalar *ihls_out, alwan_scalar const *rgb_in,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !ihls_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        for (size_t i = 0; i < tile; i++) {
            alwan_rgb rgb = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_ihls r = alwan_rgb_to_ihls_v(rgb);
            d0[i] = (alwan_simd_lane)r.H; d1[i] = (alwan_simd_lane)r.L; d2[i] = (alwan_simd_lane)r.S;
        }
        ALWAN_MAP_NORM_MUL(d0, tile, 0.15915494309189533577);
        alwan__store_tile_aos3(ihls_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_ihls_to_rgb_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *ihls_in,
                           size_t count, size_t in_stride, size_t out_stride) {
    if (!ihls_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, ihls_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_MUL(c0, tile, 6.28318530717958647692);
        for (size_t i = 0; i < tile; i++) {
            alwan_ihls ihls = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_rgb r = alwan_ihls_to_rgb_v(ihls);
            d0[i] = (alwan_simd_lane)r.r; d1[i] = (alwan_simd_lane)r.g; d2[i] = (alwan_simd_lane)r.b;
        }
        alwan__store_tile_aos3(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Lab <-> DIN99 (with int variant)
 * ---------------------------------------------------------------- */

int alwan_lab_to_din99_map_interleave(alwan_scalar *din99_out, alwan_scalar const *lab_in,
                            int variant,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!lab_in || !din99_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)lab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)din99_out + i * out_stride);
        alwan_lab lab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_din99 r = alwan_lab_to_din99_v(lab, variant);
        out_ptr[0] = r.L99; out_ptr[1] = r.a99; out_ptr[2] = r.b99;
    }
    return ALWAN_OK;
}

int alwan_din99_to_lab_map_interleave(alwan_scalar *lab_out, alwan_scalar const *din99_in,
                            int variant,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!din99_in || !lab_out || count == 0) return ALWAN_E_INVALID;

    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)din99_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)lab_out + i * out_stride);
        alwan_din99 din99 = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_lab r = alwan_din99_to_lab_v(din99, variant);
        out_ptr[0] = r.L; out_ptr[1] = r.a; out_ptr[2] = r.b;
    }
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

/* XYZ <-> IgPgTg */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_igpgtg_map_planar,  alwan_xyz,    alwan_igpgtg, alwan_xyz_to_igpgtg_v,  x,y,z, Ig,Pg,Tg)
ALWAN_MAP3_PLANAR_V(alwan_igpgtg_to_xyz_map_planar,  alwan_igpgtg, alwan_xyz,    alwan_igpgtg_to_xyz_v,  Ig,Pg,Tg, x,y,z)

/* XYZ <-> ICaCb */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_icacb_map_planar,   alwan_xyz,    alwan_icacb,  alwan_xyz_to_icacb_v,   x,y,z, I,Ca,Cb)
ALWAN_MAP3_PLANAR_V(alwan_icacb_to_xyz_map_planar,   alwan_icacb,  alwan_xyz,    alwan_icacb_to_xyz_v,   I,Ca,Cb, x,y,z)

/* XYZ <-> hdr-CIELAB */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_hdr_cielab_map_planar, alwan_xyz, alwan_lab,    alwan_xyz_to_hdr_cielab_v, x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_V(alwan_hdr_cielab_to_xyz_map_planar, alwan_lab, alwan_xyz,    alwan_hdr_cielab_to_xyz_v, L,a,b, x,y,z)

/* XYZ <-> hdr-IPT */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_hdr_ipt_map_planar, alwan_xyz,    alwan_ipt,    alwan_xyz_to_hdr_ipt_v, x,y,z, I,P,T)
ALWAN_MAP3_PLANAR_V(alwan_hdr_ipt_to_xyz_map_planar, alwan_ipt,    alwan_xyz,    alwan_hdr_ipt_to_xyz_v, I,P,T, x,y,z)

/* XYZ <-> UCS */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_ucs_map_planar,     alwan_xyz,    alwan_ucs,    alwan_xyz_to_ucs_v,     x,y,z, U,V,W)
ALWAN_MAP3_PLANAR_V(alwan_ucs_to_xyz_map_planar,     alwan_ucs,    alwan_xyz,    alwan_ucs_to_xyz_v,     U,V,W, x,y,z)

/* XYZ <-> OSA-UCS */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_osa_ucs_map_planar, alwan_xyz,    alwan_osa_ucs, alwan_xyz_to_osa_ucs_v, x,y,z, L,j,g)
ALWAN_MAP3_PLANAR_V(alwan_osa_ucs_to_xyz_map_planar, alwan_osa_ucs, alwan_xyz,   alwan_osa_ucs_to_xyz_v, L,j,g, x,y,z)

/* XYZ <-> Hunter Lab */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_hunter_lab_map_planar,  alwan_xyz,       alwan_hunter_lab, alwan_xyz_to_hunter_lab_v,  x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_V(alwan_hunter_lab_to_xyz_map_planar,  alwan_hunter_lab, alwan_xyz,       alwan_hunter_lab_to_xyz_v,  L,a,b, x,y,z)

/* XYZ <-> Hunter Lab custom */
ALWAN_MAP3_PLANAR_V_WHITE(alwan_xyz_to_hunter_lab_custom_map_planar,  alwan_xyz,       alwan_hunter_lab, alwan_xyz_to_hunter_lab_custom_v,  x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_V_WHITE(alwan_hunter_lab_to_xyz_custom_map_planar,  alwan_hunter_lab, alwan_xyz,       alwan_hunter_lab_to_xyz_custom_v,  L,a,b, x,y,z)

/* XYZ <-> ProLab */
ALWAN_MAP3_PLANAR_V(alwan_xyz_to_prolab_map_planar,  alwan_xyz,    alwan_prolab, alwan_xyz_to_prolab_v,  x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_V(alwan_prolab_to_xyz_map_planar,  alwan_prolab, alwan_xyz,    alwan_prolab_to_xyz_v,  L,a,b, x,y,z)

/* XYZ <-> ProLab custom */
ALWAN_MAP3_PLANAR_V_WHITE(alwan_xyz_to_prolab_custom_map_planar,  alwan_xyz,    alwan_prolab, alwan_xyz_to_prolab_custom_v,  x,y,z, L,a,b)
ALWAN_MAP3_PLANAR_V_WHITE(alwan_prolab_to_xyz_custom_map_planar,  alwan_prolab, alwan_xyz,    alwan_prolab_to_xyz_custom_v,  L,a,b, x,y,z)

/* XYZ <-> UVW */
ALWAN_MAP3_PLANAR_V_WHITE(alwan_xyz_to_uvw_map_planar, alwan_xyz, alwan_uvw, alwan_xyz_to_uvw_v, x,y,z, U,V,W)
ALWAN_MAP3_PLANAR_V_WHITE(alwan_uvw_to_xyz_map_planar, alwan_uvw, alwan_xyz, alwan_uvw_to_xyz_v, U,V,W, x,y,z)

/* RGB <-> Prismatic */
ALWAN_MAP3_PLANAR_V(alwan_rgb_to_prismatic_map_planar,  alwan_rgb,       alwan_prismatic, alwan_rgb_to_prismatic_v,  r,g,b, L,s,h)
ALWAN_MAP3_PLANAR_V(alwan_prismatic_to_rgb_map_planar,  alwan_prismatic, alwan_rgb,       alwan_prismatic_to_rgb_v,  L,s,h, r,g,b)

/* RGB <-> HCL */
ALWAN_MAP3_PLANAR_V(alwan_rgb_to_hcl_map_planar,  alwan_rgb, alwan_hcl, alwan_rgb_to_hcl_v,  r,g,b, H,C,L)
ALWAN_MAP3_PLANAR_V(alwan_hcl_to_rgb_map_planar,  alwan_hcl, alwan_rgb, alwan_hcl_to_rgb_v,  H,C,L, r,g,b)

/* RGB <-> IHLS */
ALWAN_MAP3_PLANAR_V(alwan_rgb_to_ihls_map_planar, alwan_rgb,  alwan_ihls, alwan_rgb_to_ihls_v, r,g,b, H,L,S)
ALWAN_MAP3_PLANAR_V(alwan_ihls_to_rgb_map_planar, alwan_ihls, alwan_rgb,  alwan_ihls_to_rgb_v, H,L,S, r,g,b)

/* Lab <-> DIN99 */
ALWAN_MAP3_PLANAR_V_INT(alwan_lab_to_din99_map_planar, alwan_lab,   alwan_din99, alwan_lab_to_din99_v, int, variant, L,a,b, L99,a99,b99)
ALWAN_MAP3_PLANAR_V_INT(alwan_din99_to_lab_map_planar, alwan_din99, alwan_lab,   alwan_din99_to_lab_v, int, variant, L99,a99,b99, L,a,b)
