/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Parameterized SIMD helper functions for map kernels.
 * No include guard - included once per precision pass between
 * alwan_map_simd_defs.h and alwan_map_simd_undef.h.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * Matrix-vector multiply (3x3, SIMD lanes)
 * Matrix is row-major, scalars broadcast from matrix elements.
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_MAP_NAME(alwan__mat3_mul_simd)(
    alwan_map_simd *ox, alwan_map_simd *oy, alwan_map_simd *oz,
    ALWAN_MAP_LIT(alwan_mat3x3) const *m,
    alwan_map_simd r, alwan_map_simd g, alwan_map_simd b)
{
    /* Use mul+add (not FMA) to match scalar alwan_mat3_mulv_v rounding:
     * m[0]*r + m[1]*g + m[2]*b evaluated left-to-right with intermediate rounding. */
    *ox = alwan_map_simd_add(alwan_map_simd_add(
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[0]), r),
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[1]), g)),
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[2]), b));
    *oy = alwan_map_simd_add(alwan_map_simd_add(
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[3]), r),
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[4]), g)),
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[5]), b));
    *oz = alwan_map_simd_add(alwan_map_simd_add(
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[6]), r),
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[7]), g)),
              alwan_map_simd_mul(alwan_map_simd_set1((alwan_map_lane)m->m[8]), b));
}

/* ----------------------------------------------------------------
 * Tile Load: AoS strided input -> flat SoA arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_MAP_NAME(alwan__load_tile_aos3)(
    alwan_map_lane *ch0, alwan_map_lane *ch1, alwan_map_lane *ch2,
    alwan_map_lane const *base, size_t offset,
    size_t stride, size_t tile_count)
{
    if (stride == 3 * sizeof(alwan_map_lane)) {
        alwan_map_lane const *src = base + offset * 3;
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= tile_count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd c0, c1, c2;
            alwan_map_simd_deinterleave3(src + j * 3, &c0, &c1, &c2);
            alwan_map_simd_store(&ch0[j], c0);
            alwan_map_simd_store(&ch1[j], c1);
            alwan_map_simd_store(&ch2[j], c2);
        }
#endif
        for (; j < tile_count; j++) {
            ch0[j] = src[j * 3 + 0];
            ch1[j] = src[j * 3 + 1];
            ch2[j] = src[j * 3 + 2];
        }
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            alwan_map_lane const *p = (alwan_map_lane const *)((char const *)base + (offset + j) * stride);
            ch0[j] = p[0];
            ch1[j] = p[1];
            ch2[j] = p[2];
        }
    }
}

/* ----------------------------------------------------------------
 * Tile Store: flat SoA arrays -> AoS strided output
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_MAP_NAME(alwan__store_tile_aos3)(
    alwan_map_lane *base, size_t offset, size_t stride,
    alwan_map_lane const *ch0, alwan_map_lane const *ch1, alwan_map_lane const *ch2,
    size_t tile_count)
{
    if (stride == 3 * sizeof(alwan_map_lane)) {
        alwan_map_lane *dst = base + offset * 3;
        size_t j = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
        for (; j + ALWAN_MAP_SIMD_WIDTH <= tile_count; j += ALWAN_MAP_SIMD_WIDTH) {
            alwan_map_simd c0 = alwan_map_simd_load(&ch0[j]);
            alwan_map_simd c1 = alwan_map_simd_load(&ch1[j]);
            alwan_map_simd c2 = alwan_map_simd_load(&ch2[j]);
            alwan_map_simd_interleave3(dst + j * 3, c0, c1, c2);
        }
#endif
        for (; j < tile_count; j++) {
            dst[j * 3 + 0] = ch0[j];
            dst[j * 3 + 1] = ch1[j];
            dst[j * 3 + 2] = ch2[j];
        }
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            alwan_map_lane *p = (alwan_map_lane *)((char *)base + (offset + j) * stride);
            p[0] = ch0[j];
            p[1] = ch1[j];
            p[2] = ch2[j];
        }
    }
}

/* ----------------------------------------------------------------
 * Planar Tile Load: separate channel arrays -> flat SoA
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_MAP_NAME(alwan__load_tile_planar3)(
    alwan_map_lane *ch0, alwan_map_lane *ch1, alwan_map_lane *ch2,
    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
    size_t offset, size_t stride, size_t tile_count)
{
    if (stride == sizeof(alwan_map_lane)) {
        memcpy(ch0, in0 + offset, tile_count * sizeof(alwan_map_lane));
        memcpy(ch1, in1 + offset, tile_count * sizeof(alwan_map_lane));
        memcpy(ch2, in2 + offset, tile_count * sizeof(alwan_map_lane));
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            ch0[j] = *(alwan_map_lane const *)((char const *)in0 + (offset + j) * stride);
            ch1[j] = *(alwan_map_lane const *)((char const *)in1 + (offset + j) * stride);
            ch2[j] = *(alwan_map_lane const *)((char const *)in2 + (offset + j) * stride);
        }
    }
}

/* ----------------------------------------------------------------
 * Planar Tile Store: flat SoA -> separate channel arrays
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_MAP_NAME(alwan__store_tile_planar3)(
    alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
    size_t offset, size_t stride,
    alwan_map_lane const *ch0, alwan_map_lane const *ch1, alwan_map_lane const *ch2,
    size_t tile_count)
{
    if (stride == sizeof(alwan_map_lane)) {
        memcpy(out0 + offset, ch0, tile_count * sizeof(alwan_map_lane));
        memcpy(out1 + offset, ch1, tile_count * sizeof(alwan_map_lane));
        memcpy(out2 + offset, ch2, tile_count * sizeof(alwan_map_lane));
    } else {
        for (size_t j = 0; j < tile_count; j++) {
            *(alwan_map_lane *)((char *)out0 + (offset + j) * stride) = ch0[j];
            *(alwan_map_lane *)((char *)out1 + (offset + j) * stride) = ch1[j];
            *(alwan_map_lane *)((char *)out2 + (offset + j) * stride) = ch2[j];
        }
    }
}

/* ----------------------------------------------------------------
 * Normalization helpers (parameterized precision)
 * ---------------------------------------------------------------- */

ALWAN_INLINE void ALWAN_MAP_NAME(alwan__norm_lane_add)(alwan_map_lane *d, size_t n, alwan_map_lane offset) {
    for (size_t i = 0; i < n; i++) d[i] += offset;
}

#if ALWAN_NORMALIZE_RANGES
ALWAN_INLINE void ALWAN_MAP_NAME(alwan__norm_lane_mul)(alwan_map_lane *d, size_t n, alwan_map_lane factor) {
    for (size_t i = 0; i < n; i++) d[i] *= factor;
}

ALWAN_INLINE void ALWAN_MAP_NAME(alwan__norm_lane_affine)(alwan_map_lane *d, size_t n, alwan_map_lane scale, alwan_map_lane offset) {
    for (size_t i = 0; i < n; i++) d[i] = d[i] * scale + offset;
}
#endif

/* ================================================================
 * SIMD Transfer Function Helpers (parameterized precision)
 * ================================================================ */

#if ALWAN_MAP_SIMD_WIDTH > 1

/* CIE Lab f(t) */
ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__lab_f_simd)(alwan_map_simd t) {
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC
    /* Det mode: lane-unpack so the cube root branch matches the scalar
     * deterministic polynomial bit-exactly across SIMD widths. */
    alwan_map_lane const delta3   = (alwan_map_lane)(216.0 / 24389.0);
    alwan_map_lane const kappa_s  = (alwan_map_lane)(24389.0 / (27.0 * 116.0));
    alwan_map_lane const offset_s = (alwan_map_lane)(16.0 / 116.0);
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, t);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        alwan_map_lane const x = lanes[i];
        lanes[i] = (x > delta3) ? ALWAN_MAP_NAME(alwan_det_cbrt)(x) : (kappa_s * x + offset_s);
    }
    return alwan_map_simd_load(lanes);
#else
    alwan_map_simd delta3 = alwan_map_simd_set1((alwan_map_lane)(216.0 / 24389.0));
    alwan_map_simd kappa  = alwan_map_simd_set1((alwan_map_lane)(24389.0 / (27.0 * 116.0)));
    alwan_map_simd offset = alwan_map_simd_set1((alwan_map_lane)(16.0 / 116.0));
    alwan_map_simd cbrt_result   = alwan_map_simd_cbrt_fast(t);
    alwan_map_simd linear_result = alwan_map_simd_fmadd(kappa, t, offset);
    alwan_map_simd_mask mask = alwan_map_simd_cmpgt(t, delta3);
    return alwan_map_simd_select(mask, cbrt_result, linear_result);
#endif
}

/* CIE Lab f_inv(t) */
ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__lab_f_inv_simd)(alwan_map_simd t) {
    alwan_map_lane delta_s = (alwan_map_lane)6.0 / (alwan_map_lane)29.0;
    alwan_map_lane kappa_s = (alwan_map_lane)3.0 * delta_s * delta_s;
    alwan_map_lane offset_s = (alwan_map_lane)16.0 / (alwan_map_lane)116.0;
    alwan_map_simd delta  = alwan_map_simd_set1(delta_s);
    alwan_map_simd kappa  = alwan_map_simd_set1(kappa_s);
    alwan_map_simd offset = alwan_map_simd_set1(offset_s);
    alwan_map_simd cube_result   = alwan_map_simd_mul(t, alwan_map_simd_mul(t, t));
    alwan_map_simd linear_result = alwan_map_simd_mul(kappa, alwan_map_simd_sub(t, offset));
    alwan_map_simd_mask mask = alwan_map_simd_cmpgt(t, delta);
    return alwan_map_simd_select(mask, cube_result, linear_result);
}

/* Force-inline when SVML: pow24/pow_inv24 reduce to a single intrinsic (~3 YMM
 * registers). Three inlined calls stay well within the 16-register budget.
 * Without SVML the software implementation uses ~15 temporaries; three inlined
 * copies would overflow the register file and cause spills worse than a call. */
#if ALWAN_HAS_SVML
#  define ALWAN__OETF_INLINE ALWAN_FORCE_INLINE
#else
#  define ALWAN__OETF_INLINE ALWAN_INLINE
#endif

#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

/* Deterministic mode: SIMD srgb_oetf/eotf go through the same scalar
 * polynomial as the non-SIMD path, lane by lane. The vectorised
 * pow24/pow_inv24 paths use approximations whose lane order and FMA
 * usage differ across SSE/AVX/NEON; using the scalar polynomial keeps
 * SIMD and scalar paths bit-identical. ~10x slower per pixel than the
 * fast-mode vector path, but the determinism contract requires it.
 * road_to_determinism.md §4. */

ALWAN__OETF_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd v) {
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        lanes[i] = ALWAN_MAP_NAME(alwan_det_srgb_eotf)(lanes[i]);
    }
    return alwan_map_simd_load(lanes);
}

ALWAN__OETF_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd v) {
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        lanes[i] = ALWAN_MAP_NAME(alwan_det_srgb_oetf)(lanes[i]);
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode — vectorised approximations */

/* sRGB EOTF: encoded -> linear */
ALWAN__OETF_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__srgb_eotf_simd)(alwan_map_simd v) {
    alwan_map_simd thresh = alwan_map_simd_set1((alwan_map_lane)ALWAN_SRGB_EOTF_THRESH);
    alwan_map_simd_mask mask = alwan_map_simd_cmple(v, thresh);
    alwan_map_simd lo     = alwan_map_simd_mul(v, alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_SRGB_LINEAR_GAIN)));
    if (alwan_map_simd_mask_all_set(mask))
        return lo;
    alwan_map_simd hi_base = alwan_map_simd_mul(
        alwan_map_simd_add(v, alwan_map_simd_set1((alwan_map_lane)ALWAN_SRGB_B)),
        alwan_map_simd_set1((alwan_map_lane)(1.0 / ALWAN_SRGB_A)));
    alwan_map_simd hi     = alwan_map_simd_pow24(hi_base);
    return alwan_map_simd_select(mask, lo, hi);
}

/* sRGB OETF: linear -> encoded */
ALWAN__OETF_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__srgb_oetf_simd)(alwan_map_simd v) {
    alwan_map_simd thresh = alwan_map_simd_set1((alwan_map_lane)ALWAN_SRGB_OETF_THRESH);
    alwan_map_simd_mask mask = alwan_map_simd_cmple(v, thresh);
    alwan_map_simd lo = alwan_map_simd_mul(v, alwan_map_simd_set1((alwan_map_lane)ALWAN_SRGB_LINEAR_GAIN));
    /* Early-out: skip pow_inv24 when all lanes are in the linear region */
    if (alwan_map_simd_mask_all_set(mask))
        return lo;
    /* Clamp to zero before pow_inv24 to avoid undefined pow(negative, non-integer) */
    alwan_map_simd hi_base = alwan_map_simd_select(
        alwan_map_simd_cmpgt(v, alwan_map_simd_zero()),
        v,
        alwan_map_simd_zero());
    alwan_map_simd hi = alwan_map_simd_sub(
        alwan_map_simd_mul(
            alwan_map_simd_set1((alwan_map_lane)ALWAN_SRGB_A),
            alwan_map_simd_pow_inv24(hi_base)),
        alwan_map_simd_set1((alwan_map_lane)ALWAN_SRGB_B));
    return alwan_map_simd_select(mask, lo, hi);
}

#endif /* ALWAN_DETERMINISTIC */

#undef ALWAN__OETF_INLINE

/* PQ OETF: linear (0-10000 cd/m^2) -> encoded */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

/* Det mode: lane-unpack to the canonical scalar so SIMD bit-matches.
 * The vector path uses `linear * (1/10000)` (precomputed inv10k) where
 * the scalar uses `linear / 10000.0`. Those differ by 1 ULP because
 * 1/10000 is not exactly representable, and the difference is amplified
 * through the two pow calls. Lane-unpacking sidesteps it entirely.
 * road_to_determinism.md §8. */
ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_oetf_simd)(alwan_map_simd v) {
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        lanes[i] = ALWAN_MAP_NAME(alwan_pq_oetf)(lanes[i]);
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode -- vectorised */

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_oetf_simd)(alwan_map_simd v) {
    alwan_map_simd const m1   = alwan_map_simd_set1((alwan_map_lane)(2610.0 / 16384.0));
    alwan_map_simd const m2   = alwan_map_simd_set1((alwan_map_lane)(2523.0 / 32.0));
    alwan_map_simd const c1   = alwan_map_simd_set1((alwan_map_lane)(3424.0 / 4096.0));
    alwan_map_simd const c2   = alwan_map_simd_set1((alwan_map_lane)(2413.0 / 128.0));
    alwan_map_simd const c3   = alwan_map_simd_set1((alwan_map_lane)(2392.0 / 128.0));
    alwan_map_simd const inv10k = alwan_map_simd_set1((alwan_map_lane)(1.0 / 10000.0));
    alwan_map_simd const zero = alwan_map_simd_zero();
    alwan_map_simd Y = alwan_map_simd_select(alwan_map_simd_cmplt(v, zero), zero, alwan_map_simd_mul(v, inv10k));
    alwan_map_simd Ym1 = alwan_map_simd_pow(Y, m1);
    alwan_map_simd num = alwan_map_simd_add(c1, alwan_map_simd_mul(c2, Ym1));
    alwan_map_simd den = alwan_map_simd_add(alwan_map_simd_set1((alwan_map_lane)1.0), alwan_map_simd_mul(c3, Ym1));
    return alwan_map_simd_pow(alwan_map_simd_div(num, den), m2);
}

#endif /* ALWAN_DETERMINISTIC */

/* PQ EOTF: encoded -> linear (0-10000 cd/m^2) */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_eotf_simd)(alwan_map_simd v) {
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        lanes[i] = ALWAN_MAP_NAME(alwan_pq_eotf)(lanes[i]);
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode */

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_eotf_simd)(alwan_map_simd v) {
    alwan_map_simd const m1_inv = alwan_map_simd_set1((alwan_map_lane)(16384.0 / 2610.0));
    alwan_map_simd const m2_inv = alwan_map_simd_set1((alwan_map_lane)(32.0 / 2523.0));
    alwan_map_simd const c1   = alwan_map_simd_set1((alwan_map_lane)(3424.0 / 4096.0));
    alwan_map_simd const c2   = alwan_map_simd_set1((alwan_map_lane)(2413.0 / 128.0));
    alwan_map_simd const c3   = alwan_map_simd_set1((alwan_map_lane)(2392.0 / 128.0));
    alwan_map_simd const ten_k = alwan_map_simd_set1((alwan_map_lane)10000.0);
    alwan_map_simd const zero = alwan_map_simd_zero();
    alwan_map_simd E = alwan_map_simd_select(alwan_map_simd_cmplt(v, zero), zero, v);
    alwan_map_simd Ep = alwan_map_simd_pow(E, m2_inv);
    alwan_map_simd num = alwan_map_simd_sub(Ep, c1);
    num = alwan_map_simd_select(alwan_map_simd_cmplt(num, zero), zero, num);
    alwan_map_simd den = alwan_map_simd_sub(c2, alwan_map_simd_mul(c3, Ep));
    alwan_map_simd const eps = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_PQ_DIV_GUARD);
    alwan_map_simd_mask safe = alwan_map_simd_cmpgt(alwan_map_simd_abs(den), eps);
    alwan_map_simd ratio = alwan_map_simd_select(safe, alwan_map_simd_div(num, den), zero);
    ratio = alwan_map_simd_select(alwan_map_simd_cmplt(ratio, zero), zero, ratio);
    return alwan_map_simd_mul(ten_k, alwan_map_simd_pow(ratio, m1_inv));
}

#endif /* ALWAN_DETERMINISTIC */

/* JzAzBz PQ OETF */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

/* Det mode: lane-unpack with the scalar formula inlined directly.
 * The scalar uses `linear / 10000.0` (correctly-rounded division);
 * the vector path was using `linear * (1/10000)` which is 1 ULP off.
 * Using ALWAN_POW (which routes to alwan_det_pow_pos in det mode)
 * keeps the per-lane pow consistent with the public scalar.
 * Inlining the formula avoids cross-TU dependency on the JzAzBz
 * core header (this .inc is included from cam/colorspace/jzazbz/...
 * map TUs). */
ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(alwan_map_simd v) {
    alwan_map_lane const n  = (alwan_map_lane)0.1593017578125;
    alwan_map_lane const p  = (alwan_map_lane)134.034375;
    alwan_map_lane const c1 = (alwan_map_lane)0.8359375;
    alwan_map_lane const c2 = (alwan_map_lane)18.8515625;
    alwan_map_lane const c3 = (alwan_map_lane)18.6875;
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        alwan_map_lane const lin = lanes[i];
        if (lin <= (alwan_map_lane)0) {
            lanes[i] = (alwan_map_lane)0;
        } else {
            alwan_map_lane const Yn = alwan_map_scalar_pow(lin / (alwan_map_lane)10000.0, n);
            alwan_map_lane const num = c1 + c2 * Yn;
            alwan_map_lane const den = (alwan_map_lane)1.0 + c3 * Yn;
            lanes[i] = alwan_map_scalar_pow(num / den, p);
        }
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode */

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_jz_oetf_simd)(alwan_map_simd v) {
    alwan_map_simd const n_exp = alwan_map_simd_set1((alwan_map_lane)0.1593017578125);
    alwan_map_simd const p_exp = alwan_map_simd_set1((alwan_map_lane)134.034375);
    alwan_map_simd const c1    = alwan_map_simd_set1((alwan_map_lane)0.8359375);
    alwan_map_simd const c2    = alwan_map_simd_set1((alwan_map_lane)18.8515625);
    alwan_map_simd const c3    = alwan_map_simd_set1((alwan_map_lane)18.6875);
    alwan_map_simd const inv10k = alwan_map_simd_set1((alwan_map_lane)(1.0 / 10000.0));
    alwan_map_simd const zero  = alwan_map_simd_zero();
    alwan_map_simd_mask pos = alwan_map_simd_cmpgt(v, zero);
    alwan_map_simd clamped = alwan_map_simd_select(pos, v, zero);
    alwan_map_simd Yn = alwan_map_simd_pow(alwan_map_simd_mul(clamped, inv10k), n_exp);
    alwan_map_simd num = alwan_map_simd_add(c1, alwan_map_simd_mul(c2, Yn));
    alwan_map_simd den = alwan_map_simd_add(alwan_map_simd_set1((alwan_map_lane)1.0), alwan_map_simd_mul(c3, Yn));
    alwan_map_simd result = alwan_map_simd_pow(alwan_map_simd_div(num, den), p_exp);
    return alwan_map_simd_select(pos, result, zero);
}

#endif /* ALWAN_DETERMINISTIC */

/* JzAzBz PQ EOTF */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(alwan_map_simd v) {
    alwan_map_lane const n_inv = (alwan_map_lane)(1.0 / 0.1593017578125);
    alwan_map_lane const p_inv = (alwan_map_lane)(1.0 / 134.034375);
    alwan_map_lane const c1    = (alwan_map_lane)0.8359375;
    alwan_map_lane const c2    = (alwan_map_lane)18.8515625;
    alwan_map_lane const c3    = (alwan_map_lane)18.6875;
    alwan_map_lane const eps   = (alwan_map_lane)ALWAN_MAP_PQ_DIV_GUARD;
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        alwan_map_lane const enc = lanes[i];
        if (enc <= (alwan_map_lane)0) {
            lanes[i] = (alwan_map_lane)0;
            continue;
        }
        alwan_map_lane const Ep = alwan_map_scalar_pow(enc, p_inv);
        alwan_map_lane const num = Ep - c1;
        alwan_map_lane const den = c2 - c3 * Ep;
        alwan_map_lane const abs_den = (den < (alwan_map_lane)0) ? -den : den;
        alwan_map_lane ratio = (abs_den > eps) ? (num / den) : (alwan_map_lane)0;
        if (ratio < (alwan_map_lane)0) ratio = (alwan_map_lane)0;
        lanes[i] = (alwan_map_lane)10000.0 * alwan_map_scalar_pow(ratio, n_inv);
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode */

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__pq_jz_eotf_simd)(alwan_map_simd v) {
    alwan_map_simd const n_inv = alwan_map_simd_set1((alwan_map_lane)(1.0 / 0.1593017578125));
    alwan_map_simd const p_inv = alwan_map_simd_set1((alwan_map_lane)(1.0 / 134.034375));
    alwan_map_simd const c1    = alwan_map_simd_set1((alwan_map_lane)0.8359375);
    alwan_map_simd const c2    = alwan_map_simd_set1((alwan_map_lane)18.8515625);
    alwan_map_simd const c3    = alwan_map_simd_set1((alwan_map_lane)18.6875);
    alwan_map_simd const ten_k = alwan_map_simd_set1((alwan_map_lane)10000.0);
    alwan_map_simd const zero  = alwan_map_simd_zero();
    alwan_map_simd const eps   = alwan_map_simd_set1((alwan_map_lane)ALWAN_MAP_PQ_DIV_GUARD);
    alwan_map_simd_mask pos = alwan_map_simd_cmpgt(v, zero);
    alwan_map_simd enc = alwan_map_simd_select(pos, v, zero);
    alwan_map_simd Ep = alwan_map_simd_pow(enc, p_inv);
    alwan_map_simd num = alwan_map_simd_sub(Ep, c1);
    alwan_map_simd den = alwan_map_simd_sub(c2, alwan_map_simd_mul(c3, Ep));
    alwan_map_simd_mask safe = alwan_map_simd_cmpgt(alwan_map_simd_abs(den), eps);
    alwan_map_simd ratio = alwan_map_simd_select(safe, alwan_map_simd_div(num, den), zero);
    ratio = alwan_map_simd_select(alwan_map_simd_cmplt(ratio, zero), zero, ratio);
    alwan_map_simd result = alwan_map_simd_mul(ten_k, alwan_map_simd_pow(ratio, n_inv));
    return alwan_map_simd_select(pos, result, zero);
}

#endif /* ALWAN_DETERMINISTIC */

/* HLG OETF: linear -> encoded */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(alwan_map_simd v) {
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        lanes[i] = ALWAN_MAP_NAME(alwan_hlg_oetf)(lanes[i]);
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode */

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__hlg_oetf_simd)(alwan_map_simd v) {
    alwan_map_simd const a_     = alwan_map_simd_set1((alwan_map_lane)0.17883277);
    alwan_map_simd const b_     = alwan_map_simd_set1((alwan_map_lane)(1.0 - 4.0 * 0.17883277));
    alwan_map_simd const c_     = alwan_map_simd_set1((alwan_map_lane)0.55991072952956202);
    alwan_map_simd const three  = alwan_map_simd_set1((alwan_map_lane)3.0);
    alwan_map_simd const twelve = alwan_map_simd_set1((alwan_map_lane)12.0);
    alwan_map_simd const thresh = alwan_map_simd_set1((alwan_map_lane)(1.0 / 12.0));
    alwan_map_simd const zero   = alwan_map_simd_zero();
    alwan_map_simd L = alwan_map_simd_select(alwan_map_simd_cmplt(v, zero), zero, v);
    alwan_map_simd lo = alwan_map_simd_sqrt(alwan_map_simd_mul(three, L));
    alwan_map_simd hi = alwan_map_simd_add(alwan_map_simd_mul(a_, alwan_map_simd_log(alwan_map_simd_sub(alwan_map_simd_mul(twelve, L), b_))), c_);
    return alwan_map_simd_select(alwan_map_simd_cmple(L, thresh), lo, hi);
}

#endif /* ALWAN_DETERMINISTIC */

/* HLG inverse OETF: encoded -> scene-linear (no OOTF, used by ICtCp-HLG)
 * The scalar `alwan_hlg_eotf` includes the OOTF (pow(., 1.2)); the
 * "scene linear" inverse-of-OETF without OOTF is what this SIMD helper
 * returns. In det mode we still produce the no-OOTF result by computing
 * it from the scalar HLG OETF inverse directly: solve inverse-OETF(E)
 * using the same scalar building blocks but without pow(.,1.2). */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(alwan_map_simd v) {
    alwan_map_lane const a = (alwan_map_lane)0.17883277;
    alwan_map_lane const b = (alwan_map_lane)(1.0 - 4.0 * 0.17883277);
    alwan_map_lane const c = (alwan_map_lane)0.55991072952956202;
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        alwan_map_lane const E = lanes[i] < (alwan_map_lane)0 ? (alwan_map_lane)0 : lanes[i];
        if (E <= (alwan_map_lane)0.5) {
            lanes[i] = (E * E) / (alwan_map_lane)3.0;
        } else {
            lanes[i] = (ALWAN_MAP_NAME(alwan_det_exp)((E - c) / a) + b) / (alwan_map_lane)12.0;
        }
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode */

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(alwan_map_simd v) {
    alwan_map_simd const b_     = alwan_map_simd_set1((alwan_map_lane)(1.0 - 4.0 * 0.17883277));
    alwan_map_simd const c_     = alwan_map_simd_set1((alwan_map_lane)0.55991072952956202);
    alwan_map_simd const inv_a  = alwan_map_simd_set1((alwan_map_lane)(1.0 / 0.17883277));
    alwan_map_simd const three  = alwan_map_simd_set1((alwan_map_lane)3.0);
    alwan_map_simd const twelve = alwan_map_simd_set1((alwan_map_lane)12.0);
    alwan_map_simd const half   = alwan_map_simd_set1((alwan_map_lane)0.5);
    alwan_map_simd const zero   = alwan_map_simd_zero();
    alwan_map_simd E = alwan_map_simd_select(alwan_map_simd_cmplt(v, zero), zero, v);
    alwan_map_simd lo = alwan_map_simd_div(alwan_map_simd_mul(E, E), three);
    alwan_map_simd hi = alwan_map_simd_div(
        alwan_map_simd_add(alwan_map_simd_exp(alwan_map_simd_mul(alwan_map_simd_sub(E, c_), inv_a)), b_),
        twelve);
    return alwan_map_simd_select(alwan_map_simd_cmple(E, half), lo, hi);
}

#endif /* ALWAN_DETERMINISTIC */

/* HLG full EOTF: encoded -> display-linear (OOTF system gamma 1.2)
 * Matches the scalar alwan_hlg_eotf_*; lane-unpack in det mode. */
#if defined(ALWAN_DETERMINISTIC) && ALWAN_DETERMINISTIC

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__hlg_eotf_full_simd)(alwan_map_simd v) {
    ALWAN_ALIGN(64) alwan_map_lane lanes[ALWAN_MAP_SIMD_WIDTH];
    alwan_map_simd_store(lanes, v);
    for (int i = 0; i < ALWAN_MAP_SIMD_WIDTH; i++) {
        lanes[i] = ALWAN_MAP_NAME(alwan_hlg_eotf)(lanes[i]);
    }
    return alwan_map_simd_load(lanes);
}

#else /* fast mode */

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__hlg_eotf_full_simd)(alwan_map_simd v) {
    alwan_map_simd scene = ALWAN_MAP_NAME(alwan__hlg_eotf_simd)(v);
    return alwan_map_simd_pow(scene, alwan_map_simd_set1((alwan_map_lane)1.2));
}

#endif /* ALWAN_DETERMINISTIC */

/* SIMD min3 / max3 */
ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__simd_min3)(alwan_map_simd a, alwan_map_simd b, alwan_map_simd c) {
    return alwan_map_simd_min(a, alwan_map_simd_min(b, c));
}

ALWAN_INLINE alwan_map_simd ALWAN_MAP_NAME(alwan__simd_max3)(alwan_map_simd a, alwan_map_simd b, alwan_map_simd c) {
    return alwan_map_simd_max(a, alwan_map_simd_max(b, c));
}

#endif /* ALWAN_MAP_SIMD_WIDTH > 1 */
