/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map CVD (Color Vision Deficiency) kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* ----------------------------------------------------------------
 * Helper: fuse RGB_TO_LMS * CVD * LMS_TO_RGB into a single mat3
 * ---------------------------------------------------------------- */

ALWAN_FORCE_INLINE void ALWAN_MAP_LIT(alwan__fuse_cvd_matrix)(ALWAN_MAP_LIT(alwan_mat3x3) *fused, ALWAN_MAP_LIT(alwan_mat3x3) const *cvd) {
    /* temp = CVD * RGB_TO_LMS */
    ALWAN_MAP_LIT(alwan_mat3x3) temp;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            temp.m[r*3+c] = cvd->m[r*3+0]*ALWAN_MAP_LIT(CVD_RGB_TO_LMS).m[0*3+c]
                          + cvd->m[r*3+1]*ALWAN_MAP_LIT(CVD_RGB_TO_LMS).m[1*3+c]
                          + cvd->m[r*3+2]*ALWAN_MAP_LIT(CVD_RGB_TO_LMS).m[2*3+c];
    /* fused = LMS_TO_RGB * temp */
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            fused->m[r*3+c] = ALWAN_MAP_LIT(CVD_LMS_TO_RGB).m[r*3+0]*temp.m[0*3+c]
                            + ALWAN_MAP_LIT(CVD_LMS_TO_RGB).m[r*3+1]*temp.m[1*3+c]
                            + ALWAN_MAP_LIT(CVD_LMS_TO_RGB).m[r*3+2]*temp.m[2*3+c];
}

/* ----------------------------------------------------------------
 * Shared SIMD CVD Brettel: fused mat3 + saturate + lerp (interleave)
 * ---------------------------------------------------------------- */

static int ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    ALWAN_MAP_LIT(alwan_mat3x3) const *cvd_matrix, alwan_map_lane severity,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (severity < (alwan_map_lane)0.0) severity = (alwan_map_lane)0.0;
    if (severity > (alwan_map_lane)1.0) severity = (alwan_map_lane)1.0;
    ALWAN_MAP_LIT(alwan_mat3x3) fused;
    ALWAN_MAP_LIT(alwan__fuse_cvd_matrix)(&fused, cvd_matrix);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd one  = alwan_map_simd_set1(1);
                alwan_map_simd sev  = alwan_map_simd_set1((alwan_map_lane)severity);
                alwan_map_simd inv_sev = alwan_map_simd_sub(one, sev);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    /* fused mat3 multiply */
                    alwan_map_simd cr, cg, cb;
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&cr, &cg, &cb, &fused, r, g, b);
                    /* raw simulation -- no gamut clamp (matches the scalar core) */
                    /* lerp(original, cvd, severity) = original*(1-sev) + cvd*sev */
                    alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(r, inv_sev, alwan_map_simd_mul(cr, sev)));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(g, inv_sev, alwan_map_simd_mul(cg, sev)));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b, inv_sev, alwan_map_simd_mul(cb, sev)));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v_in = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) v_out = ALWAN_MAP_LITV(alwan_simulate_cvd_matrix)(v_in, *cvd_matrix, severity);
                d0[i] = (alwan_map_lane)v_out.r; d1[i] = (alwan_map_lane)v_out.g; d2[i] = (alwan_map_lane)v_out.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * CVD Dispatch Interleave
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_cvd, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    ALWAN_MAP_LIT(alwan_mat3x3) const *cvd_matrix;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:
    case ALWAN_CVD_PROTANOMALY:
        cvd_matrix = &ALWAN_MAP_LIT(CVD_PROTANOPIA);
        break;
    case ALWAN_CVD_DEUTERANOPIA:
    case ALWAN_CVD_DEUTERANOMALY:
        cvd_matrix = &ALWAN_MAP_LIT(CVD_DEUTERANOPIA);
        break;
    case ALWAN_CVD_TRITANOPIA:
    case ALWAN_CVD_TRITANOMALY:
        cvd_matrix = &ALWAN_MAP_LIT(CVD_TRITANOPIA);
        break;
    default:
        /* Identity: just copy */
        for (size_t i = 0; i < count; i++) {
            alwan_map_lane const *in_ptr = (alwan_map_lane const *)((char const *)rgb_in + i * in_stride);
            alwan_map_lane *out_ptr = (alwan_map_lane *)((char *)rgb_out + i * out_stride);
            out_ptr[0] = in_ptr[0]; out_ptr[1] = in_ptr[1]; out_ptr[2] = in_ptr[2];
        }
        return ALWAN_OK;
    }
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, cvd_matrix, severity, count, in_stride, out_stride);
}

/* ----------------------------------------------------------------
 * Individual CVD Type Map Functions (interleave)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_protanopia, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, &ALWAN_MAP_LIT(CVD_PROTANOPIA), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_deuteranopia, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, &ALWAN_MAP_LIT(CVD_DEUTERANOPIA), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_tritanopia, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, &ALWAN_MAP_LIT(CVD_TRITANOPIA), severity, count, in_stride, out_stride);
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

/* Shared SIMD CVD Brettel implementation (planar) */
static int ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(
    alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
    ALWAN_MAP_LIT(alwan_mat3x3) const *cvd_matrix, alwan_map_lane severity,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (severity < (alwan_map_lane)0.0) severity = (alwan_map_lane)0.0;
    if (severity > (alwan_map_lane)1.0) severity = (alwan_map_lane)1.0;
    ALWAN_MAP_LIT(alwan_mat3x3) fused;
    ALWAN_MAP_LIT(alwan__fuse_cvd_matrix)(&fused, cvd_matrix);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_map_lane d0[ALWAN_MAP_TILE_PIXELS], d1[ALWAN_MAP_TILE_PIXELS], d2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd one  = alwan_map_simd_set1(1);
                alwan_map_simd sev  = alwan_map_simd_set1((alwan_map_lane)severity);
                alwan_map_simd inv_sev = alwan_map_simd_sub(one, sev);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd cr, cg, cb;
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&cr, &cg, &cb, &fused, r, g, b);
                    /* raw simulation -- no gamut clamp (matches the scalar core) */
                    alwan_map_simd_store(&d0[i], alwan_map_simd_fmadd(r, inv_sev, alwan_map_simd_mul(cr, sev)));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_fmadd(g, inv_sev, alwan_map_simd_mul(cg, sev)));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_fmadd(b, inv_sev, alwan_map_simd_mul(cb, sev)));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v_in = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) v_out = ALWAN_MAP_LITV(alwan_simulate_cvd_matrix)(v_in, *cvd_matrix, severity);
                d0[i] = (alwan_map_lane)v_out.r; d1[i] = (alwan_map_lane)v_out.g; d2[i] = (alwan_map_lane)v_out.b;
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out0, out1, out2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_cvd, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count, alwan_cvd_type cvd_type, alwan_map_lane severity)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    ALWAN_MAP_LIT(alwan_mat3x3) const *cvd_matrix;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:  case ALWAN_CVD_PROTANOMALY:  cvd_matrix = &ALWAN_MAP_LIT(CVD_PROTANOPIA);  break;
    case ALWAN_CVD_DEUTERANOPIA: case ALWAN_CVD_DEUTERANOMALY: cvd_matrix = &ALWAN_MAP_LIT(CVD_DEUTERANOPIA); break;
    case ALWAN_CVD_TRITANOPIA:  case ALWAN_CVD_TRITANOMALY:  cvd_matrix = &ALWAN_MAP_LIT(CVD_TRITANOPIA);  break;
    default:
        for (size_t i = 0; i < count; i++) {
            *(alwan_map_lane *)((char *)out0 + i * out_stride) = *(alwan_map_lane const *)((char const *)in0 + i * in_stride);
            *(alwan_map_lane *)((char *)out1 + i * out_stride) = *(alwan_map_lane const *)((char const *)in1 + i * in_stride);
            *(alwan_map_lane *)((char *)out2 + i * out_stride) = *(alwan_map_lane const *)((char const *)in2 + i * in_stride);
        }
        return ALWAN_OK;
    }
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, cvd_matrix, severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_protanopia, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count, alwan_map_lane severity)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, &ALWAN_MAP_LIT(CVD_PROTANOPIA), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_deuteranopia, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count, alwan_map_lane severity)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, &ALWAN_MAP_LIT(CVD_DEUTERANOPIA), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_tritanopia, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count, alwan_map_lane severity)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, &ALWAN_MAP_LIT(CVD_TRITANOPIA), severity, count, in_stride, out_stride);
}

/* ----------------------------------------------------------------
 * Machado 2009 CVD Batch Map (interleave)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_cvd_machado, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_cvd_type cvd_type, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    /* Select LUT once outside the loop */
    ALWAN_MAP_LIT(alwan_mat3x3) const *lut;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:
    case ALWAN_CVD_PROTANOMALY:
        lut = ALWAN_MAP_LIT(MACHADO_PROTAN);
        break;
    case ALWAN_CVD_DEUTERANOPIA:
    case ALWAN_CVD_DEUTERANOMALY:
        lut = ALWAN_MAP_LIT(MACHADO_DEUTAN);
        break;
    case ALWAN_CVD_TRITANOPIA:
    case ALWAN_CVD_TRITANOMALY:
        lut = ALWAN_MAP_LIT(MACHADO_TRITAN);
        break;
    default:
        return ALWAN_E_INVALID;
    }

    /* Interpolate matrix once (severity is constant across batch) */
    ALWAN_MAP_LIT(alwan_mat3x3) mat = ALWAN_MAP_LITV(alwan_machado_interpolate)(lut, severity);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd one  = alwan_map_simd_set1(1);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd o0, o1, o2;
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&o0, &o1, &o2, &mat, r, g, b);
                    /* raw Machado product -- no gamut clamp (matches the scalar core) */
                    alwan_map_simd_store(&c0[i], o0);
                    alwan_map_simd_store(&c1[i], o1);
                    alwan_map_simd_store(&c2[i], o2);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_vec3) v = {{(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]}};
                ALWAN_MAP_LIT(alwan_vec3) r = ALWAN_MAP_LITV(alwan_mat3_mulv)(mat, v);
                c0[i] = (alwan_map_lane)r.v[0];
                c1[i] = (alwan_map_lane)r.v[1];
                c2[i] = (alwan_map_lane)r.v[2];
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_aos3)(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Machado 2009 CVD Planar Map
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_cvd_machado, _map_planar)(alwan_map_lane *out_r, size_t out_stride, alwan_map_lane *out_g, alwan_map_lane *out_b, alwan_map_lane const *in_r, size_t in_stride, alwan_map_lane const *in_g, alwan_map_lane const *in_b, size_t count, alwan_cvd_type cvd_type, alwan_map_lane severity)
{
    if (!in_r || !in_g || !in_b || !out_r || !out_g || !out_b || count == 0) return ALWAN_E_INVALID;

    ALWAN_MAP_LIT(alwan_mat3x3) const *lut;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:  case ALWAN_CVD_PROTANOMALY:  lut = ALWAN_MAP_LIT(MACHADO_PROTAN); break;
    case ALWAN_CVD_DEUTERANOPIA: case ALWAN_CVD_DEUTERANOMALY: lut = ALWAN_MAP_LIT(MACHADO_DEUTAN); break;
    case ALWAN_CVD_TRITANOPIA:  case ALWAN_CVD_TRITANOMALY:  lut = ALWAN_MAP_LIT(MACHADO_TRITAN); break;
    default: return ALWAN_E_INVALID;
    }

    ALWAN_MAP_LIT(alwan_mat3x3) mat = ALWAN_MAP_LITV(alwan_machado_interpolate)(lut, severity);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
        ALWAN_MAP_NAME(alwan__load_tile_planar3)(c0, c1, c2, in_r, in_g, in_b, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_MAP_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_MAP_SIMD_WIDTH;
                alwan_map_simd one  = alwan_map_simd_set1(1);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd o0, o1, o2;
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&o0, &o1, &o2, &mat, r, g, b);
                    /* raw Machado product -- no gamut clamp (matches the scalar core) */
                    alwan_map_simd_store(&c0[i], o0);
                    alwan_map_simd_store(&c1[i], o1);
                    alwan_map_simd_store(&c2[i], o2);
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_vec3) v = {{(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]}};
                ALWAN_MAP_LIT(alwan_vec3) r = ALWAN_MAP_LITV(alwan_mat3_mulv)(mat, v);
                c0[i] = (alwan_map_lane)r.v[0];
                c1[i] = (alwan_map_lane)r.v[1];
                c2[i] = (alwan_map_lane)r.v[2];
            }
        }
        ALWAN_MAP_NAME(alwan__store_tile_planar3)(out_r, out_g, out_b, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
