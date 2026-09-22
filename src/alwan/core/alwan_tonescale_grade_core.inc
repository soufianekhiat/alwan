/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Tonescale-region grading, the per-element arithmetic (parameterized precision).
 * No include guard - included twice via core setup/teardown.
 *
 * Canham, Punnappurath and Brown, "Adaptive Color Grading" (arXiv:2609.21169):
 * an image is split into four overlapping tonescale regions by a per-pixel
 * intensity, each region carrying a CIELAB a*b* offset. A region's membership
 * is a clipped line through its pivot: 1 on the region's own side, falling to 0
 * at slope |m| per unit of intensity on the other. The paper writes it as two
 * branches, 1 below the pivot then m x + (1 - pivot m) for the dark regions and
 * the mirror for the light ones, with the result clipped to [0, 1]; both
 * branches are the one expression below, since the linear part exceeds 1 on
 * the region's own side and the clip returns it to 1.
 */

ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_tonescale_region_weight)(ALWAN_CORE_T intensity,
                                                                            ALWAN_CORE_T pivot,
                                                                            ALWAN_CORE_T slope) {
    ALWAN_CORE_T w = ALWAN_CORE_LITERAL(1.0) + slope * (intensity - pivot);
    w = ALWAN_CORE_SELECT(w < ALWAN_CORE_LITERAL(0.0), ALWAN_CORE_LITERAL(0.0), w);
    return ALWAN_CORE_SELECT(w > ALWAN_CORE_LITERAL(1.0), ALWAN_CORE_LITERAL(1.0), w);
}

/* One region's blend: the adjusted value where the region is, the input where
 * it is not, and the mix between. Regions are applied one after another, each
 * on the previous one's output, which is the order the reference composes its
 * lookup tables in. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_tonescale_region_blend)(ALWAN_CORE_T value,
                                                                           ALWAN_CORE_T adjusted,
                                                                           ALWAN_CORE_T weight) {
    return value + weight * (adjusted - value);
}
