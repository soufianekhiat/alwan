/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Ordered dithering, per pixel (parameterized). No include guard: included once per
 * precision pass. Integer arithmetic and a square root, so a shader dithers with the same
 * thresholds and the same rounding as alwan_dither_quantize.
 */

/* ----------------------------------------------------------------
 * The Bayer matrix of side 2^log2_size, by its recursive definition
 *     M(2n) = [[4 M(n), 4 M(n) + 2], [4 M(n) + 3, 4 M(n) + 1]]
 * read bit by bit: the quadrant adds the low-order part, so the finest bit of the
 * coordinates is the most significant pair of the rank. For each bit b from the finest,
 * the rank takes the bit of x XOR y and then the bit of y. Side 2 gives [[0, 2], [3, 1]]
 * and side 4 [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], rows by y.
 * x and y wrap: only their low log2_size bits are read.
 * ---------------------------------------------------------------- */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_dither_bayer_rank)(int x, int y, int log2_size) {
    int rank = 0;
    int b;
    for (b = 0; b < log2_size; b++) {
        const int xy = ((x ^ y) >> b) & 1;
        const int yb = (y >> b) & 1;
        rank = (rank << 2) | (xy << 1) | yb;
    }
    return rank;
}

/* A rank among `count` taken as a threshold in (0, 1): (rank + 0.5) / count, the centre of
 * its slot, so a mask of count ranks splits [0, 1) into count equal parts. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_dither_threshold)(int rank, int count) {
    return ((ALWAN_CORE_T)rank + ALWAN_CORE_LITERAL(0.5)) / (ALWAN_CORE_T)count;
}

/* A uniform u in [0, 1) remapped to the triangular distribution on (-1, 1), the inverse of
 * its CDF: noise of two LSB peak to peak whose error does not depend on the signal
 * (the TPDF dither of Lipshitz, Wannamaker and Vanderkooy 1992). */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_dither_tpdf)(ALWAN_CORE_T u) {
    return ALWAN_CORE_SELECT(u < ALWAN_CORE_LITERAL(0.5),
                             ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(2.0) * u) - ALWAN_CORE_LITERAL(1.0),
                             ALWAN_CORE_LITERAL(1.0) - ALWAN_CORE_SQRT(ALWAN_CORE_LITERAL(2.0) * (ALWAN_CORE_LITERAL(1.0) - u)));
}

/* The code of value (in [0, 1], the encoded signal) quantised to `levels` steps, 0..levels,
 * as floor(value * levels + offset) held to [0, levels]. offset 0.5 rounds to nearest; an
 * ordered threshold t in (0, 1) gives the RPDF ordered dither; 0.5 + tpdf(u) the TPDF one. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_CORE_FNVLIT(alwan_dither_quantize)(ALWAN_CORE_T value, ALWAN_CORE_T levels,
                                                                  ALWAN_CORE_T offset) {
    const ALWAN_CORE_T q = ALWAN_CORE_FLOOR(value * levels + offset);
    const ALWAN_CORE_T lo = ALWAN_CORE_SELECT(q < ALWAN_CORE_ZERO, ALWAN_CORE_ZERO, q);
    return ALWAN_CORE_SELECT(lo > levels, levels, lo);
}
