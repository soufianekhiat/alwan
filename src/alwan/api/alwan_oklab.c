/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Oklab & Oklch Color Spaces
 *
 * Reference: Björn Ottosson (2020)
 * "A perceptual color space for image processing"
 * https://bottosson.github.io/posts/oklab/
 *
 * Implementation delegated to alwan_oklab_core.h (single source of truth).
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_oklab_core.h"

void alwan_xyz_to_oklab(alwan_oklab *oklab, alwan_xyz const *xyz) {
    *oklab = alwan_xyz_to_oklab_v(*xyz);
}

void alwan_oklab_to_xyz(alwan_xyz *xyz, alwan_oklab const *oklab) {
    *xyz = alwan_oklab_to_xyz_v(*oklab);
}

void alwan_oklab_to_oklch(alwan_oklch *oklch, alwan_oklab const *oklab) {
    *oklch = alwan_oklab_to_oklch_v(*oklab);
}

void alwan_oklch_to_oklab(alwan_oklab *oklab, alwan_oklch const *oklch) {
    *oklab = alwan_oklch_to_oklab_v(*oklch);
}
