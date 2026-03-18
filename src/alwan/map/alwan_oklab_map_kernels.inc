/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Oklab kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * XYZ -> Oklab Kernel
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__xyz_to_oklab_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_map_simd vx = alwan_map_simd_load(&i0[i]);
        alwan_map_simd vy = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vz = alwan_map_simd_load(&i2[i]);
        alwan_map_simd vl, vm, vs;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vl, &vm, &vs, &ALWAN_MAP_LIT(OKLAB_M1), vx, vy, vz);
        vl = alwan_map_simd_cbrt(vl);
        vm = alwan_map_simd_cbrt(vm);
        vs = alwan_map_simd_cbrt(vs);
        alwan_map_simd oL, oa, ob;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&oL, &oa, &ob, &ALWAN_MAP_LIT(OKLAB_M2), vl, vm, vs);
        alwan_map_simd_store(&o0[i], oL);
        alwan_map_simd_store(&o1[i], oa);
        alwan_map_simd_store(&o2[i], ob);
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_xyz) v = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_oklab) r = ALWAN_MAP_LITV(alwan_xyz_to_oklab)(v);
        o0[i] = (alwan_map_lane)r.L; o1[i] = (alwan_map_lane)r.a; o2[i] = (alwan_map_lane)r.b;
    }
}

/* ----------------------------------------------------------------
 * Oklab -> XYZ Kernel
 * ---------------------------------------------------------------- */

static void ALWAN_MAP_LIT(alwan__oklab_to_xyz_kernel)(
    alwan_map_lane *o0, alwan_map_lane *o1, alwan_map_lane *o2,
    alwan_map_lane const *i0, alwan_map_lane const *i1, alwan_map_lane const *i2,
    size_t n)
{
    size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    for (; i + W <= n; i += W) {
        alwan_map_simd vL = alwan_map_simd_load(&i0[i]);
        alwan_map_simd va = alwan_map_simd_load(&i1[i]);
        alwan_map_simd vb = alwan_map_simd_load(&i2[i]);
        alwan_map_simd lp, mp, sp;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&lp, &mp, &sp, &ALWAN_MAP_LIT(OKLAB_M2_INV), vL, va, vb);
        alwan_map_simd vl = alwan_map_simd_mul(lp, alwan_map_simd_mul(lp, lp));
        alwan_map_simd vm = alwan_map_simd_mul(mp, alwan_map_simd_mul(mp, mp));
        alwan_map_simd vs = alwan_map_simd_mul(sp, alwan_map_simd_mul(sp, sp));
        alwan_map_simd ox, oy, oz;
        ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&ox, &oy, &oz, &ALWAN_MAP_LIT(OKLAB_M1_INV), vl, vm, vs);
        alwan_map_simd_store(&o0[i], ox);
        alwan_map_simd_store(&o1[i], oy);
        alwan_map_simd_store(&o2[i], oz);
    }
#endif
    for (; i < n; i++) {
        ALWAN_MAP_LIT(alwan_oklab) v = {(alwan_map_lane)i0[i], (alwan_map_lane)i1[i], (alwan_map_lane)i2[i]};
        ALWAN_MAP_LIT(alwan_xyz) r = ALWAN_MAP_LITV(alwan_oklab_to_xyz)(v);
        o0[i] = (alwan_map_lane)r.x; o1[i] = (alwan_map_lane)r.y; o2[i] = (alwan_map_lane)r.z;
    }
}

/* ----------------------------------------------------------------
 * XYZ <-> Oklab Interleave Wrappers
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_xyz_to_oklab, _map_interleave)(
    alwan_map_lane *oklab_out, alwan_map_lane const *xyz_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!xyz_in || !oklab_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_oklab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(oklab_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklab_to_xyz, _map_interleave)(
    alwan_map_lane *xyz_out, alwan_map_lane const *oklab_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!oklab_in || !xyz_out || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, oklab_in, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__oklab_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(xyz_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklab_to_oklch, _map_interleave)(
    alwan_map_lane *oklch_out, alwan_map_lane const *oklab_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!oklab_in || !oklch_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, oklab_in, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vC = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(va, va, alwan_map_simd_mul(vb, vb)));
            alwan_map_simd vh = alwan_map_simd_atan2(vb, va);
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vC);
            alwan_map_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_oklab) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_oklch) r = ALWAN_MAP_LITV(alwan_oklab_to_oklch)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.C; c2[i] = (alwan_map_lane)r.h;
        }

        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 0.15915494309189533577, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(oklch_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)oklab_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)oklch_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_oklab) oklab = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_oklch) oklch = ALWAN_MAP_LITV(alwan_oklab_to_oklch)(oklab);
        out_ptr[0] = oklch.L; out_ptr[1] = oklch.C; out_ptr[2] = oklch.h;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklch_to_oklab, _map_interleave)(
    alwan_map_lane *oklab_out, alwan_map_lane const *oklch_in,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!oklch_in || !oklab_out || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, oklch_in, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 6.28318530717958647692, -3.14159265358979323846);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vC = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vh = alwan_map_simd_load(&c2[i]);
            alwan_map_simd va = alwan_map_simd_mul(vC, alwan_map_simd_cos(vh));
            alwan_map_simd vb = alwan_map_simd_mul(vC, alwan_map_simd_sin(vh));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], va);
            alwan_map_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_oklch) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_oklab) r = ALWAN_MAP_LITV(alwan_oklch_to_oklab)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.a; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_aos3)(oklab_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)oklch_in + i * in_stride);
        alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)oklab_out + i * out_stride);
        ALWAN_MAP_LIT(alwan_oklch) oklch = {in_ptr[0], in_ptr[1], in_ptr[2]};
        ALWAN_MAP_LIT(alwan_oklab) oklab = ALWAN_MAP_LITV(alwan_oklch_to_oklab)(oklch);
        out_ptr[0] = oklab.L; out_ptr[1] = oklab.a; out_ptr[2] = oklab.b;
    }
#endif
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Planar Map Variants
 * ---------------------------------------------------------------- */

int ALWAN_MAP_LITCAT(alwan_xyz_to_oklab, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__xyz_to_oklab_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklab_to_xyz, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_LIT(alwan__oklab_to_xyz_kernel)(d0, d1, d2, c0, c1, c2, tile);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklab_to_oklch, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd va = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vb = alwan_map_simd_load(&c2[i]);
            alwan_map_simd vC = alwan_map_simd_sqrt(
                alwan_map_simd_fmadd(va, va, alwan_map_simd_mul(vb, vb)));
            alwan_map_simd vh = alwan_map_simd_atan2(vb, va);
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], vC);
            alwan_map_simd_store(&c2[i], vh);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_oklab) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_oklch) r = ALWAN_MAP_LITV(alwan_oklab_to_oklch)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.C; c2[i] = (alwan_map_lane)r.h;
        }

        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 0.15915494309189533577, 0.5);
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_oklab) oklab = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_oklch) oklch = ALWAN_MAP_LITV(alwan_oklab_to_oklch)(oklab);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = oklch.L;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = oklch.C;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = oklch.h;
    }
#endif
    return ALWAN_OK;
}

int ALWAN_MAP_LITCAT(alwan_oklch_to_oklab, _map_planar)(
    alwan_map_lane *out_ch0, alwan_map_lane *out_ch1, alwan_map_lane *out_ch2,
    alwan_map_lane const *in_ch0, alwan_map_lane const *in_ch1, alwan_map_lane const *in_ch2,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (!in_ch0 || !in_ch1 || !in_ch2 || !out_ch0 || !out_ch1 || !out_ch2 || count == 0) return ALWAN_E_INVALID;

#if ALWAN_MAP_SIMD_WIDTH > 1
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_ch0, in_ch1, in_ch2, processed, in_stride, tile);
        ALWAN_MAP_P_NORM_AFFINE(c2, tile, 6.28318530717958647692, -3.14159265358979323846);

        size_t i = 0;
        for (; i + W <= tile; i += W) {
            alwan_map_simd vL = alwan_map_simd_load(&c0[i]);
            alwan_map_simd vC = alwan_map_simd_load(&c1[i]);
            alwan_map_simd vh = alwan_map_simd_load(&c2[i]);
            alwan_map_simd va = alwan_map_simd_mul(vC, alwan_map_simd_cos(vh));
            alwan_map_simd vb = alwan_map_simd_mul(vC, alwan_map_simd_sin(vh));
            alwan_map_simd_store(&c0[i], vL);
            alwan_map_simd_store(&c1[i], va);
            alwan_map_simd_store(&c2[i], vb);
        }
        for (; i < tile; i++) {
            ALWAN_MAP_LIT(alwan_oklch) v = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
            ALWAN_MAP_LIT(alwan_oklab) r = ALWAN_MAP_LITV(alwan_oklch_to_oklab)(v);
            c0[i] = (alwan_map_lane)r.L; c1[i] = (alwan_map_lane)r.a; c2[i] = (alwan_map_lane)r.b;
        }

        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_ch0, out_ch1, out_ch2, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
#else
    for (size_t i = 0; i < count; i++) {
        ALWAN_MAP_LIT(alwan_oklch) oklch = {
            *(alwan_map_lane const *)((char const *)in_ch0 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch1 + i * in_stride),
            *(alwan_map_lane const *)((char const *)in_ch2 + i * in_stride)
        };
        ALWAN_MAP_LIT(alwan_oklab) oklab = ALWAN_MAP_LITV(alwan_oklch_to_oklab)(oklch);
        *(alwan_map_lane *)((char *)out_ch0 + i * out_stride) = oklab.L;
        *(alwan_map_lane *)((char *)out_ch1 + i * out_stride) = oklab.a;
        *(alwan_map_lane *)((char *)out_ch2 + i * out_stride) = oklab.b;
    }
#endif
    return ALWAN_OK;
}
