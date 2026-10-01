/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Summed-area table queries (parameterized). No include guard: included once
 * per precision pass. The functions that read the table live in
 * alwan_summed_area_reader.inc; this file binds them to pointers.
 */

/* ----------------------------------------------------------------
 * The table, in elements of the precision, for an image of w x h pixels and
 * ch channels:
 *     S        0                        (w+1) (h+1) ch   S(x, y, c), the sum of the
 *                                                        stored values over the pixels
 *                                                        [0, x) x [0, y), corner by
 *                                                        corner: row y, column x, channel c
 *                                                        at ((y (w+1)) + x) ch + c. Row 0
 *                                                        and column 0 are 0.
 *     offset   (w+1) (h+1) ch           ch               the value taken off every pixel
 *                                                        of the channel before summing
 *     divisor  + ch                     1                what a stored sum is divided by
 *                                                        to be in the image's units (255
 *                                                        for 8-bit codes, 65535 for 16)
 * A pixel's value is (stored + offset) / divisor, so the sum over a region of
 * area A is (rectangle of S + offset x A) / divisor.
 * ---------------------------------------------------------------- */
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_sat_off_offset)(int w, int h, int ch) { return (w + 1) * (h + 1) * ch; }
ALWAN_INLINE int ALWAN_CORE_FNVLIT(alwan_sat_off_divisor)(int w, int h, int ch) { return (w + 1) * (h + 1) * ch + ch; }

#if ALWAN_HAS_POINTERS
#define ALWAN_SAT_READER_POINTER 1
#include "alwan_summed_area_reader.inc"
#endif
