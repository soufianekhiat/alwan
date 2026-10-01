/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Summed-area table queries, written against an accessor instead of a pointer.
 * No include guard - included once per binding.
 *
 * ACCESSOR. The table is read through ALWAN_SAT_READ(i). Where pointers exist,
 * alwan_summed_area_core.inc binds it to a leading pointer parameter. A shader
 * defines
 *
 *     StructuredBuffer<float> Sat : register(t4);
 *     #define ALWAN_SAT_NAME     img
 *     #define ALWAN_SAT_READ(i)  Sat[i]
 *     #include "core/alwan_summed_area_reader.inc"
 *
 *     float s = alwan_sat_sum_img(w, h, ch, c, x0, y0, x1, y1);
 *
 * and uploads the table alwan_summed_area_table_get_layout hands back,
 * converted to float: four reads and a difference of large sums, so single
 * precision loses what the corners have in common (the table's offset keeps
 * them small for an image near its mean).
 */

#if defined(ALWAN_SAT_READER_POINTER)

# define ALWAN_SAT_FN_(base)      ALWAN_CORE_FNVLIT(base)
# define ALWAN_SAT_PARAMS_        const ALWAN_CORE_T *tab,
# define ALWAN_SAT_ARGS_          tab,
# define ALWAN_SAT_READ(i)        tab[i]

#else

# if !defined(ALWAN_SAT_NAME) || !defined(ALWAN_SAT_READ)
#  error "alwan_summed_area_reader.inc: define ALWAN_SAT_NAME and ALWAN_SAT_READ before including it"
# endif
# define ALWAN_SAT_PASTE2_(a, b)  a##_##b
# define ALWAN_SAT_PASTE_(a, b)   ALWAN_SAT_PASTE2_(a, b)
# define ALWAN_SAT_FN_(base)      ALWAN_SAT_PASTE_(base, ALWAN_SAT_NAME)
# define ALWAN_SAT_PARAMS_
# define ALWAN_SAT_ARGS_

#endif

/* S at the integer corner (x, y), channel c: x in [0, w], y in [0, h]. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_PARAMS_ int w, int ch, int c, int x, int y) {
    return ALWAN_SAT_READ((y * (w + 1) + x) * ch + c);
}

/* S at a continuous corner (x, y) in pixel units, x in [0, w], y in [0, h]: the
 * bilinear blend of the four integer corners around it. For an image constant
 * on each pixel the blend is exact: inside a pixel S grows as
 * S00 + fx A + fy B + fx fy p, which is what the blend of its corners is. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_SAT_FN_(alwan_sat_corner_continuous)(ALWAN_SAT_PARAMS_ int w, int h, int ch, int c,
                                                                     ALWAN_CORE_T x, ALWAN_CORE_T y) {
    int ix, iy;
    ALWAN_DET_PRECISE ALWAN_CORE_T fx, fy, s00, s10, s01, s11;
    if (!(x > ALWAN_CORE_LITERAL(0.0))) x = ALWAN_CORE_LITERAL(0.0);
    if (!(y > ALWAN_CORE_LITERAL(0.0))) y = ALWAN_CORE_LITERAL(0.0);
    if (x > (ALWAN_CORE_T)w) x = (ALWAN_CORE_T)w;
    if (y > (ALWAN_CORE_T)h) y = (ALWAN_CORE_T)h;
    ix = (int)x; if (ix > w - 1) ix = w - 1;
    iy = (int)y; if (iy > h - 1) iy = h - 1;
    fx = x - (ALWAN_CORE_T)ix;
    fy = y - (ALWAN_CORE_T)iy;
    s00 = ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, ix, iy);
    s10 = ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, ix + 1, iy);
    s01 = ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, ix, iy + 1);
    s11 = ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, ix + 1, iy + 1);
    return s00 + fx * (s10 - s00) + fy * (s01 - s00) + fx * fy * ((s11 - s10) - (s01 - s00));
}

/* The sum of channel c over the pixels [x0, x1) x [y0, y1], in the image's
 * units: four reads. 0 <= x0 <= x1 <= w and 0 <= y0 <= y1 <= h. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_SAT_FN_(alwan_sat_sum)(ALWAN_SAT_PARAMS_ int w, int h, int ch, int c,
                                                       int x0, int y0, int x1, int y1) {
    const ALWAN_CORE_T rect = (ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, x1, y1) -
                               ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, x0, y1)) -
                              (ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, x1, y0) -
                               ALWAN_SAT_FN_(alwan_sat_corner)(ALWAN_SAT_ARGS_ w, ch, c, x0, y0));
    const ALWAN_CORE_T area = (ALWAN_CORE_T)(x1 - x0) * (ALWAN_CORE_T)(y1 - y0);
    return (rect + ALWAN_SAT_READ(ALWAN_CORE_FNVLIT(alwan_sat_off_offset)(w, h, ch) + c) * area) /
           ALWAN_SAT_READ(ALWAN_CORE_FNVLIT(alwan_sat_off_divisor)(w, h, ch));
}

/* The integral of channel c over [x0, x1] x [y0, y1] in continuous pixel units,
 * the image constant on each pixel: exact up to rounding, partial pixels
 * counted by the area they cover. Bounds are clamped to [0, w] x [0, h]. */
ALWAN_INLINE ALWAN_CORE_T ALWAN_SAT_FN_(alwan_sat_integral)(ALWAN_SAT_PARAMS_ int w, int h, int ch, int c,
                                                            ALWAN_CORE_T x0, ALWAN_CORE_T y0,
                                                            ALWAN_CORE_T x1, ALWAN_CORE_T y1) {
    ALWAN_CORE_T rect, area;
    if (x0 < ALWAN_CORE_LITERAL(0.0)) x0 = ALWAN_CORE_LITERAL(0.0);
    if (y0 < ALWAN_CORE_LITERAL(0.0)) y0 = ALWAN_CORE_LITERAL(0.0);
    if (x1 > (ALWAN_CORE_T)w) x1 = (ALWAN_CORE_T)w;
    if (y1 > (ALWAN_CORE_T)h) y1 = (ALWAN_CORE_T)h;
    if (!(x1 > x0) || !(y1 > y0)) return ALWAN_CORE_LITERAL(0.0);
    rect = (ALWAN_SAT_FN_(alwan_sat_corner_continuous)(ALWAN_SAT_ARGS_ w, h, ch, c, x1, y1) -
            ALWAN_SAT_FN_(alwan_sat_corner_continuous)(ALWAN_SAT_ARGS_ w, h, ch, c, x0, y1)) -
           (ALWAN_SAT_FN_(alwan_sat_corner_continuous)(ALWAN_SAT_ARGS_ w, h, ch, c, x1, y0) -
            ALWAN_SAT_FN_(alwan_sat_corner_continuous)(ALWAN_SAT_ARGS_ w, h, ch, c, x0, y0));
    area = (x1 - x0) * (y1 - y0);
    return (rect + ALWAN_SAT_READ(ALWAN_CORE_FNVLIT(alwan_sat_off_offset)(w, h, ch) + c) * area) /
           ALWAN_SAT_READ(ALWAN_CORE_FNVLIT(alwan_sat_off_divisor)(w, h, ch));
}

#undef ALWAN_SAT_FN_
#undef ALWAN_SAT_PARAMS_
#undef ALWAN_SAT_ARGS_
#undef ALWAN_SAT_READ
#if !defined(ALWAN_SAT_READER_POINTER)
# undef ALWAN_SAT_PASTE2_
# undef ALWAN_SAT_PASTE_
# undef ALWAN_SAT_NAME
#endif
#undef ALWAN_SAT_READER_POINTER
