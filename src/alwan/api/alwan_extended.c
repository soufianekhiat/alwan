/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * M1: Extended Color Spaces & Models
 * Thin wrapper — logic in alwan_extended_core.h
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_extended_core.h"

/* ================================================================
 * Prismatic
 * ================================================================ */

void alwan_rgb_to_prismatic(alwan_prismatic *prismatic, alwan_rgb const *rgb) {
    if (!prismatic || !rgb) return;
    *prismatic = alwan_rgb_to_prismatic_v(*rgb);
}

void alwan_prismatic_to_rgb(alwan_rgb *rgb, alwan_prismatic const *prismatic) {
    if (!rgb || !prismatic) return;
    *rgb = alwan_prismatic_to_rgb_v(*prismatic);
}

/* ================================================================
 * HCL
 * ================================================================ */

void alwan_rgb_to_hcl(alwan_hcl *hcl, alwan_rgb const *rgb) {
    if (!hcl || !rgb) return;
    *hcl = alwan_rgb_to_hcl_v(*rgb);
}

void alwan_hcl_to_rgb(alwan_rgb *rgb, alwan_hcl const *hcl) {
    if (!rgb || !hcl) return;
    *rgb = alwan_hcl_to_rgb_v(*hcl);
}

/* ================================================================
 * IHLS
 * ================================================================ */

void alwan_rgb_to_ihls(alwan_ihls *ihls, alwan_rgb const *rgb) {
    if (!ihls || !rgb) return;
    *ihls = alwan_rgb_to_ihls_v(*rgb);
}

void alwan_ihls_to_rgb(alwan_rgb *rgb, alwan_ihls const *ihls) {
    if (!rgb || !ihls) return;
    *rgb = alwan_ihls_to_rgb_v(*ihls);
}

/* ================================================================
 * hdr-CIELAB
 * ================================================================ */

void alwan_xyz_to_hdr_cielab(alwan_lab *hdr_lab, alwan_xyz const *xyz) {
    if (!xyz || !hdr_lab) return;
    *hdr_lab = alwan_xyz_to_hdr_cielab_v(*xyz);
}

void alwan_hdr_cielab_to_xyz(alwan_xyz *xyz, alwan_lab const *hdr_lab) {
    if (!hdr_lab || !xyz) return;
    *xyz = alwan_hdr_cielab_to_xyz_v(*hdr_lab);
}

/* ================================================================
 * hdr-IPT
 * ================================================================ */

void alwan_xyz_to_hdr_ipt(alwan_ipt *hdr_ipt, alwan_xyz const *xyz) {
    if (!xyz || !hdr_ipt) return;
    *hdr_ipt = alwan_xyz_to_hdr_ipt_v(*xyz);
}

void alwan_hdr_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *hdr_ipt) {
    if (!hdr_ipt || !xyz) return;
    *xyz = alwan_hdr_ipt_to_xyz_v(*hdr_ipt);
}

/* ================================================================
 * IgPgTg
 * ================================================================ */

void alwan_xyz_to_igpgtg(alwan_igpgtg *igpgtg, alwan_xyz const *xyz) {
    if (!xyz || !igpgtg) return;
    *igpgtg = alwan_xyz_to_igpgtg_v(*xyz);
}

void alwan_igpgtg_to_xyz(alwan_xyz *xyz, alwan_igpgtg const *igpgtg) {
    if (!igpgtg || !xyz) return;
    *xyz = alwan_igpgtg_to_xyz_v(*igpgtg);
}

/* ================================================================
 * ICaCb
 * ================================================================ */

void alwan_xyz_to_icacb(alwan_icacb *icacb, alwan_xyz const *xyz) {
    if (!xyz || !icacb) return;
    *icacb = alwan_xyz_to_icacb_v(*xyz);
}

void alwan_icacb_to_xyz(alwan_xyz *xyz, alwan_icacb const *icacb) {
    if (!icacb || !xyz) return;
    *xyz = alwan_icacb_to_xyz_v(*icacb);
}
