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
 * Brettel 1997 (see alwan_vision_core.inc): the two half-plane projections and the
 * separation normal of one deficiency. The SIMD lanes run the core's operations in its
 * order (LMS, both projections, the side test, back to RGB, the severity mix) rather
 * than a fused matrix, which a choice between two planes cannot be.
 * ---------------------------------------------------------------- */

typedef struct {
    ALWAN_MAP_LIT(alwan_mat3x3) const *H1;
    ALWAN_MAP_LIT(alwan_mat3x3) const *H2;
    ALWAN_MAP_LIT(alwan_vec3) const *n;
} ALWAN_MAP_LIT(alwan__cvd_brettel);

static ALWAN_MAP_LIT(alwan__cvd_brettel) const ALWAN_MAP_LIT(alwan__cvd_protan) = {
    &ALWAN_MAP_LIT(CVD_BRETTEL_PROTAN_H1), &ALWAN_MAP_LIT(CVD_BRETTEL_PROTAN_H2), &ALWAN_MAP_LIT(CVD_BRETTEL_PROTAN_N) };
static ALWAN_MAP_LIT(alwan__cvd_brettel) const ALWAN_MAP_LIT(alwan__cvd_deutan) = {
    &ALWAN_MAP_LIT(CVD_BRETTEL_DEUTAN_H1), &ALWAN_MAP_LIT(CVD_BRETTEL_DEUTAN_H2), &ALWAN_MAP_LIT(CVD_BRETTEL_DEUTAN_N) };
static ALWAN_MAP_LIT(alwan__cvd_brettel) const ALWAN_MAP_LIT(alwan__cvd_tritan) = {
    &ALWAN_MAP_LIT(CVD_BRETTEL_TRITAN_H1), &ALWAN_MAP_LIT(CVD_BRETTEL_TRITAN_H2), &ALWAN_MAP_LIT(CVD_BRETTEL_TRITAN_N) };

static int ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(
    alwan_map_lane *rgb_out, alwan_map_lane const *rgb_in,
    ALWAN_MAP_LIT(alwan__cvd_brettel) const *prm, alwan_map_lane severity,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (severity < (alwan_map_lane)0.0) severity = (alwan_map_lane)0.0;
    if (severity > (alwan_map_lane)1.0) severity = (alwan_map_lane)1.0;
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
                alwan_map_simd zero = alwan_map_simd_zero();
                alwan_map_simd sev  = alwan_map_simd_set1((alwan_map_lane)severity);
                alwan_map_simd inv_sev = alwan_map_simd_sub(one, sev);
                alwan_map_simd n0 = alwan_map_simd_set1((alwan_map_lane)prm->n->v[0]);
                alwan_map_simd n1 = alwan_map_simd_set1((alwan_map_lane)prm->n->v[1]);
                alwan_map_simd n2 = alwan_map_simd_set1((alwan_map_lane)prm->n->v[2]);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd l0, l1, l2, p10, p11, p12, p20, p21, p22, q0, q1, q2, cr, cg, cb;
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&l0, &l1, &l2, &ALWAN_MAP_LIT(CVD_RGB_TO_LMS), r, g, b);
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&p10, &p11, &p12, prm->H1, l0, l1, l2);
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&p20, &p21, &p22, prm->H2, l0, l1, l2);
                    {
                        alwan_map_simd side = alwan_map_simd_add(alwan_map_simd_add(alwan_map_simd_mul(l0, n0),
                                                                                    alwan_map_simd_mul(l1, n1)),
                                                                 alwan_map_simd_mul(l2, n2));
                        alwan_map_simd_mask neg = alwan_map_simd_cmplt(side, zero);
                        q0 = alwan_map_simd_select(neg, p20, p10);
                        q1 = alwan_map_simd_select(neg, p21, p11);
                        q2 = alwan_map_simd_select(neg, p22, p12);
                    }
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&cr, &cg, &cb, &ALWAN_MAP_LIT(CVD_LMS_TO_RGB), q0, q1, q2);
                    /* raw simulation, no gamut clamp; cvd * severity + input * (1 - severity) as the core */
                    alwan_map_simd_store(&d0[i], alwan_map_simd_add(alwan_map_simd_mul(cr, sev), alwan_map_simd_mul(r, inv_sev)));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_add(alwan_map_simd_mul(cg, sev), alwan_map_simd_mul(g, inv_sev)));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_add(alwan_map_simd_mul(cb, sev), alwan_map_simd_mul(b, inv_sev)));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v_in = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) v_out = ALWAN_MAP_LITV(alwan_simulate_cvd_brettel)(v_in, *prm->H1, *prm->H2, *prm->n, severity);
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

    ALWAN_MAP_LIT(alwan__cvd_brettel) const *prm;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:
    case ALWAN_CVD_PROTANOMALY:
        prm = &ALWAN_MAP_LIT(alwan__cvd_protan);
        break;
    case ALWAN_CVD_DEUTERANOPIA:
    case ALWAN_CVD_DEUTERANOMALY:
        prm = &ALWAN_MAP_LIT(alwan__cvd_deutan);
        break;
    case ALWAN_CVD_TRITANOPIA:
    case ALWAN_CVD_TRITANOMALY:
        prm = &ALWAN_MAP_LIT(alwan__cvd_tritan);
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
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, prm, severity, count, in_stride, out_stride);
}

/* ----------------------------------------------------------------
 * Individual CVD Type Map Functions (interleave)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_protanopia, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, &ALWAN_MAP_LIT(alwan__cvd_protan), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_deuteranopia, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, &ALWAN_MAP_LIT(alwan__cvd_deutan), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_tritanopia, _map_interleave)(alwan_map_lane *rgb_out, size_t out_stride, alwan_map_lane const *rgb_in, size_t in_stride, size_t count, alwan_map_lane severity)
{
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_interleave)(rgb_out, rgb_in, &ALWAN_MAP_LIT(alwan__cvd_tritan), severity, count, in_stride, out_stride);
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

static int ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(
    alwan_map_lane *out0, alwan_map_lane *out1, alwan_map_lane *out2,
    alwan_map_lane const *in0, alwan_map_lane const *in1, alwan_map_lane const *in2,
    ALWAN_MAP_LIT(alwan__cvd_brettel) const *prm, alwan_map_lane severity,
    size_t count, size_t in_stride, size_t out_stride)
{
    if (severity < (alwan_map_lane)0.0) severity = (alwan_map_lane)0.0;
    if (severity > (alwan_map_lane)1.0) severity = (alwan_map_lane)1.0;
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
                alwan_map_simd zero = alwan_map_simd_zero();
                alwan_map_simd sev  = alwan_map_simd_set1((alwan_map_lane)severity);
                alwan_map_simd inv_sev = alwan_map_simd_sub(one, sev);
                alwan_map_simd n0 = alwan_map_simd_set1((alwan_map_lane)prm->n->v[0]);
                alwan_map_simd n1 = alwan_map_simd_set1((alwan_map_lane)prm->n->v[1]);
                alwan_map_simd n2 = alwan_map_simd_set1((alwan_map_lane)prm->n->v[2]);
                for (; i + W <= tile; i += W) {
                    alwan_map_simd r = alwan_map_simd_load(&c0[i]);
                    alwan_map_simd g = alwan_map_simd_load(&c1[i]);
                    alwan_map_simd b = alwan_map_simd_load(&c2[i]);
                    alwan_map_simd l0, l1, l2, p10, p11, p12, p20, p21, p22, q0, q1, q2, cr, cg, cb;
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&l0, &l1, &l2, &ALWAN_MAP_LIT(CVD_RGB_TO_LMS), r, g, b);
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&p10, &p11, &p12, prm->H1, l0, l1, l2);
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&p20, &p21, &p22, prm->H2, l0, l1, l2);
                    {
                        alwan_map_simd side = alwan_map_simd_add(alwan_map_simd_add(alwan_map_simd_mul(l0, n0),
                                                                                    alwan_map_simd_mul(l1, n1)),
                                                                 alwan_map_simd_mul(l2, n2));
                        alwan_map_simd_mask neg = alwan_map_simd_cmplt(side, zero);
                        q0 = alwan_map_simd_select(neg, p20, p10);
                        q1 = alwan_map_simd_select(neg, p21, p11);
                        q2 = alwan_map_simd_select(neg, p22, p12);
                    }
                    ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&cr, &cg, &cb, &ALWAN_MAP_LIT(CVD_LMS_TO_RGB), q0, q1, q2);
                    /* raw simulation, no gamut clamp; cvd * severity + input * (1 - severity) as the core */
                    alwan_map_simd_store(&d0[i], alwan_map_simd_add(alwan_map_simd_mul(cr, sev), alwan_map_simd_mul(r, inv_sev)));
                    alwan_map_simd_store(&d1[i], alwan_map_simd_add(alwan_map_simd_mul(cg, sev), alwan_map_simd_mul(g, inv_sev)));
                    alwan_map_simd_store(&d2[i], alwan_map_simd_add(alwan_map_simd_mul(cb, sev), alwan_map_simd_mul(b, inv_sev)));
                }
            }
#endif
            for (; i < tile; i++) {
                ALWAN_MAP_LIT(alwan_rgb) v_in = {(alwan_map_lane)c0[i], (alwan_map_lane)c1[i], (alwan_map_lane)c2[i]};
                ALWAN_MAP_LIT(alwan_rgb) v_out = ALWAN_MAP_LITV(alwan_simulate_cvd_brettel)(v_in, *prm->H1, *prm->H2, *prm->n, severity);
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
    ALWAN_MAP_LIT(alwan__cvd_brettel) const *prm;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:  case ALWAN_CVD_PROTANOMALY:  prm = &ALWAN_MAP_LIT(alwan__cvd_protan);  break;
    case ALWAN_CVD_DEUTERANOPIA: case ALWAN_CVD_DEUTERANOMALY: prm = &ALWAN_MAP_LIT(alwan__cvd_deutan); break;
    case ALWAN_CVD_TRITANOPIA:  case ALWAN_CVD_TRITANOMALY:  prm = &ALWAN_MAP_LIT(alwan__cvd_tritan);  break;
    default:
        for (size_t i = 0; i < count; i++) {
            *(alwan_map_lane *)((char *)out0 + i * out_stride) = *(alwan_map_lane const *)((char const *)in0 + i * in_stride);
            *(alwan_map_lane *)((char *)out1 + i * out_stride) = *(alwan_map_lane const *)((char const *)in1 + i * in_stride);
            *(alwan_map_lane *)((char *)out2 + i * out_stride) = *(alwan_map_lane const *)((char const *)in2 + i * in_stride);
        }
        return ALWAN_OK;
    }
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, prm, severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_protanopia, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count, alwan_map_lane severity)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, &ALWAN_MAP_LIT(alwan__cvd_protan), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_deuteranopia, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count, alwan_map_lane severity)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, &ALWAN_MAP_LIT(alwan__cvd_deutan), severity, count, in_stride, out_stride);
}

alwan_status ALWAN_MAP_LITCAT(alwan_simulate_tritanopia, _map_planar)(alwan_map_lane *out0, size_t out_stride, alwan_map_lane *out1, alwan_map_lane *out2, alwan_map_lane const *in0, size_t in_stride, alwan_map_lane const *in1, alwan_map_lane const *in2, size_t count, alwan_map_lane severity)
{
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_LIT(alwan__cvd_brettel_map_planar)(out0, out1, out2, in0, in1, in2, &ALWAN_MAP_LIT(alwan__cvd_tritan), severity, count, in_stride, out_stride);
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
