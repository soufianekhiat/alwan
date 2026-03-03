/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * IPT Color Space (Image Processing Transform)
 *
 * Reference: Ebner & Fairchild (1998)
 * "Development and Testing of a Color Space (IPT) with Improved Hue Uniformity"
 * https://www.researchgate.net/publication/221677980
 *
 * Implementation delegated to alwan_ipt_core.h (single source of truth).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_ipt_core.h"

void alwan_xyz_to_ipt(alwan_ipt *ipt, alwan_xyz const *xyz) {
    *ipt = alwan_xyz_to_ipt_v(*xyz);
}

void alwan_ipt_to_xyz(alwan_xyz *xyz, alwan_ipt const *ipt) {
    *xyz = alwan_ipt_to_xyz_v(*ipt);
}

void alwan_ipt_to_iptch(alwan_iptch *iptch, alwan_ipt const *ipt) {
    *iptch = alwan_ipt_to_iptch_v(*ipt);
}

void alwan_iptch_to_ipt(alwan_ipt *ipt, alwan_iptch const *iptch) {
    *ipt = alwan_iptch_to_ipt_v(*iptch);
}
