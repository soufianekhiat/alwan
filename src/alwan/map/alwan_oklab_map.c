/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Oklab Conversions - True SIMD vectorized
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_oklab_core.h"

/* ----------------------------------------------------------------
 * XYZ <-> Oklab Kernels
 * ---------------------------------------------------------------- */

static void alwan__xyz_to_oklab_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                        alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                        size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_simd vx = alwan_simd_load(&i0[i]);
        alwan_simd vy = alwan_simd_load(&i1[i]);
        alwan_simd vz = alwan_simd_load(&i2[i]);
        alwan_simd vl, vm, vs;
        alwan__mat3_mul_simd(&vl, &vm, &vs, &OKLAB_M1, vx, vy, vz);
        vl = alwan_simd_cbrt(vl);
        vm = alwan_simd_cbrt(vm);
        vs = alwan_simd_cbrt(vs);
        alwan_simd oL, oa, ob;
        alwan__mat3_mul_simd(&oL, &oa, &ob, &OKLAB_M2, vl, vm, vs);
        alwan_simd_store(&o0[i], oL);
        alwan_simd_store(&o1[i], oa);
        alwan_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        alwan_xyz v = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_oklab r = alwan_xyz_to_oklab_v(v);
        o0[i] = (alwan_simd_lane)r.L; o1[i] = (alwan_simd_lane)r.a; o2[i] = (alwan_simd_lane)r.b;
    }
}

static void alwan__oklab_to_xyz_kernel(alwan_simd_lane *o0, alwan_simd_lane *o1, alwan_simd_lane *o2,
                                        alwan_simd_lane const *i0, alwan_simd_lane const *i1, alwan_simd_lane const *i2,
                                        size_t n) {
    size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_simd vL = alwan_simd_load(&i0[i]);
        alwan_simd va = alwan_simd_load(&i1[i]);
        alwan_simd vb = alwan_simd_load(&i2[i]);
        alwan_simd lp, mp, sp;
        alwan__mat3_mul_simd(&lp, &mp, &sp, &OKLAB_M2_INV, vL, va, vb);
        alwan_simd vl = alwan_simd_mul(lp, alwan_simd_mul(lp, lp));
        alwan_simd vm = alwan_simd_mul(mp, alwan_simd_mul(mp, mp));
        alwan_simd vs = alwan_simd_mul(sp, alwan_simd_mul(sp, sp));
        alwan_simd ox, oy, oz;
        alwan__mat3_mul_simd(&ox, &oy, &oz, &OKLAB_M1_INV, vl, vm, vs);
        alwan_simd_store(&o0[i], ox);
        alwan_simd_store(&o1[i], oy);
        alwan_simd_store(&o2[i], oz);
    }
#endif
    for (; i < n; i++) {
        alwan_oklab v = {(alwan_scalar)i0[i], (alwan_scalar)i1[i], (alwan_scalar)i2[i]};
        alwan_xyz r = alwan_oklab_to_xyz_v(v);
        o0[i] = (alwan_simd_lane)r.x; o1[i] = (alwan_simd_lane)r.y; o2[i] = (alwan_simd_lane)r.z;
    }
}

/* ----------------------------------------------------------------
 * XYZ <-> Oklab Interleave Conversions
 * ---------------------------------------------------------------- */

int alwan_xyz_to_oklab_map_interleave(alwan_scalar *oklab_out, alwan_scalar const *xyz_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!xyz_in || !oklab_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, xyz_in, processed, in_stride, tile);
        alwan__xyz_to_oklab_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(oklab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_xyz_to_oklab_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        alwan__xyz_to_oklab_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_oklab_to_xyz_map_interleave(alwan_scalar *xyz_out, alwan_scalar const *oklab_in,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!oklab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, oklab_in, processed, in_stride, tile);
        alwan__oklab_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_aos3(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_oklab_to_xyz_map_interleave_ex(void *out, alwan_pixel_format out_fmt,
                               void const *in, alwan_pixel_format in_fmt,
                               size_t count, size_t in_stride, size_t out_stride) {
    if (!in || !out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_typed_3(c0, c1, c2, in, in_fmt, processed, in_stride, tile);
        alwan__oklab_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_typed_3(out, out_fmt, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_oklab_to_oklch_map_interleave(alwan_scalar *oklch_out, alwan_scalar const *oklab_in,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!oklab_in || !oklch_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, oklab_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd vC = alwan_simd_sqrt(
                alwan_simd_fmadd(va, va, alwan_simd_mul(vb, vb)));
            alwan_simd vh = alwan_simd_atan2(vb, va);
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vC);
            alwan_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            alwan_oklab v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_oklch r = alwan_oklab_to_oklch_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.C; c2[i] = (alwan_simd_lane)r.h;
        }

        ALWAN_MAP_NORM_AFFINE(c2, tile, 0.15915494309189533577, 0.5);
        alwan__store_tile_aos3(oklch_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)oklab_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)oklch_out + i * out_stride);
        alwan_oklab oklab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_oklch oklch = alwan_oklab_to_oklch_v(oklab);
        out_ptr[0] = oklch.L; out_ptr[1] = oklch.C; out_ptr[2] = oklch.h;
    }
#endif
    return ALWAN_OK;
}

int alwan_oklch_to_oklab_map_interleave(alwan_scalar *oklab_out, alwan_scalar const *oklch_in,
                              size_t count, size_t in_stride, size_t out_stride) {
    if (!oklch_in || !oklab_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, oklch_in, processed, in_stride, tile);
        ALWAN_MAP_NORM_AFFINE(c2, tile, 6.28318530717958647692, -3.14159265358979323846);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vC = alwan_simd_load(&c1[i]);
            alwan_simd vh = alwan_simd_load(&c2[i]);
            alwan_simd va = alwan_simd_mul(vC, alwan_simd_cos(vh));
            alwan_simd vb = alwan_simd_mul(vC, alwan_simd_sin(vh));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], va);
            alwan_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            alwan_oklch v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_oklab r = alwan_oklch_to_oklab_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.a; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_aos3(oklab_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)oklch_in + i * in_stride);
        alwan_scalar *out_ptr = (alwan_scalar *)((char *)oklab_out + i * out_stride);
        alwan_oklch oklch = {in_ptr[0], in_ptr[1], in_ptr[2]};
        alwan_oklab oklab = alwan_oklch_to_oklab_v(oklch);
        out_ptr[0] = oklab.L; out_ptr[1] = oklab.a; out_ptr[2] = oklab.b;
    }
#endif
    return ALWAN_OK;
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

int alwan_xyz_to_oklab_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        alwan__xyz_to_oklab_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_oklab_to_xyz_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                   alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        alwan__oklab_to_xyz_kernel(d0, d1, d2, c0, c1, c2, tile);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_oklab_to_oklch_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                     alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                     size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd va = alwan_simd_load(&c1[i]);
            alwan_simd vb = alwan_simd_load(&c2[i]);
            alwan_simd vC = alwan_simd_sqrt(
                alwan_simd_fmadd(va, va, alwan_simd_mul(vb, vb)));
            alwan_simd vh = alwan_simd_atan2(vb, va);
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], vC);
            alwan_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            alwan_oklab v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_oklch r = alwan_oklab_to_oklch_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.C; c2[i] = (alwan_simd_lane)r.h;
        }

        ALWAN_MAP_NORM_AFFINE(c2, tile, 0.15915494309189533577, 0.5);
        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_oklab oklab = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_oklch oklch = alwan_oklab_to_oklch_v(oklab);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = oklch.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = oklch.C;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = oklch.h;
    }
#endif
    return ALWAN_OK;
}

int alwan_oklch_to_oklab_map_planar(alwan_scalar *out_ch0, alwan_scalar *out_ch1, alwan_scalar *out_ch2,
                                     alwan_scalar const *in_ch0, alwan_scalar const *in_ch1, alwan_scalar const *in_ch2,
                                     size_t count, size_t in_stride, size_t out_stride) {
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_SIMD_WIDTH > 1
    size_t const W = ALWAN_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_NORM_AFFINE(c2, tile, 6.28318530717958647692, -3.14159265358979323846);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_simd vL = alwan_simd_load(&c0[i]);
            alwan_simd vC = alwan_simd_load(&c1[i]);
            alwan_simd vh = alwan_simd_load(&c2[i]);
            alwan_simd va = alwan_simd_mul(vC, alwan_simd_cos(vh));
            alwan_simd vb = alwan_simd_mul(vC, alwan_simd_sin(vh));
            alwan_simd_store(&c0[i], vL);
            alwan_simd_store(&c1[i], va);
            alwan_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            alwan_oklch v = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
            alwan_oklab r = alwan_oklch_to_oklab_v(v);
            c0[i] = (alwan_simd_lane)r.L; c1[i] = (alwan_simd_lane)r.a; c2[i] = (alwan_simd_lane)r.b;
        }

        alwan__store_tile_planar3(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_oklch oklch = {
            *(alwan_scalar const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_scalar const *)((char const *)in_ch2 + i * in_stride)
        };
        alwan_oklab oklab = alwan_oklch_to_oklab_v(oklch);
        *(alwan_scalar *)((char *)out_ch0 + i * out_stride) = oklab.L;
        *(alwan_scalar *)((char *)out_ch1 + i * out_stride) = oklab.a;
        *(alwan_scalar *)((char *)out_ch2 + i * out_stride) = oklab.b;
    }
#endif
    return ALWAN_OK;
}
