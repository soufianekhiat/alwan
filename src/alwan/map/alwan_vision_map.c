/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map CVD (Color Vision Deficiency) Simulation Functions
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "alwan_map_internal.h"
#include "../core/alwan_vision_core.h"

/* ----------------------------------------------------------------
 * CVD Simulation Map (dispatches by type)
 * ---------------------------------------------------------------- */

/* Helper: fuse RGB_TO_LMS * CVD * LMS_TO_RGB into a single mat3 */
static void alwan__fuse_cvd_matrix(alwan_mat3x3 *fused, alwan_mat3x3 const *cvd) {
    /* temp = CVD * RGB_TO_LMS */
    alwan_mat3x3 temp;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            temp.m[r*3+c] = cvd->m[r*3+0]*CVD_RGB_TO_LMS.m[0*3+c]
                          + cvd->m[r*3+1]*CVD_RGB_TO_LMS.m[1*3+c]
                          + cvd->m[r*3+2]*CVD_RGB_TO_LMS.m[2*3+c];
    /* fused = LMS_TO_RGB * temp */
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            fused->m[r*3+c] = CVD_LMS_TO_RGB.m[r*3+0]*temp.m[0*3+c]
                            + CVD_LMS_TO_RGB.m[r*3+1]*temp.m[1*3+c]
                            + CVD_LMS_TO_RGB.m[r*3+2]*temp.m[2*3+c];
}

/* Shared SIMD CVD Brettel implementation: fused mat3 + saturate + lerp */
static int alwan__cvd_brettel_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                              alwan_mat3x3 const *cvd_matrix, alwan_scalar severity,
                                              size_t count, size_t in_stride, size_t out_stride) {
    severity = alwan_clamp(severity, (alwan_scalar)0.0, (alwan_scalar)1.0);
    alwan_mat3x3 fused;
    alwan__fuse_cvd_matrix(&fused, cvd_matrix);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd zero = alwan_simd_set1(0);
                alwan_simd one  = alwan_simd_set1(1);
                alwan_simd sev  = alwan_simd_set1((alwan_simd_lane)severity);
                alwan_simd inv_sev = alwan_simd_sub(one, sev);
                for (; i + W <= tile; i += W) {
                    alwan_simd r = alwan_simd_load(&c0[i]);
                    alwan_simd g = alwan_simd_load(&c1[i]);
                    alwan_simd b = alwan_simd_load(&c2[i]);
                    /* fused mat3 multiply */
                    alwan_simd cr, cg, cb;
                    alwan__mat3_mul_simd(&cr, &cg, &cb, &fused, r, g, b);
                    /* saturate [0,1] */
                    cr = alwan_simd_clamp(cr, zero, one);
                    cg = alwan_simd_clamp(cg, zero, one);
                    cb = alwan_simd_clamp(cb, zero, one);
                    /* lerp(original, cvd, severity) = original*(1-sev) + cvd*sev */
                    alwan_simd_store(&d0[i], alwan_simd_fmadd(r, inv_sev, alwan_simd_mul(cr, sev)));
                    alwan_simd_store(&d1[i], alwan_simd_fmadd(g, inv_sev, alwan_simd_mul(cg, sev)));
                    alwan_simd_store(&d2[i], alwan_simd_fmadd(b, inv_sev, alwan_simd_mul(cb, sev)));
                }
            }
#endif
            for (; i < tile; i++) {
                alwan_rgb v_in = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
                alwan_vec3 v = {{v_in.r, v_in.g, v_in.b}};
                alwan_vec3 lms = alwan_mat3_mulv_v(CVD_RGB_TO_LMS, v);
                alwan_vec3 lms_cvd = alwan_mat3_mulv_v(*cvd_matrix, lms);
                alwan_vec3 cvd_rgb = alwan_mat3_mulv_v(CVD_LMS_TO_RGB, lms_cvd);
                d0[i] = (alwan_simd_lane)alwan_lerp(v_in.r, alwan_saturate(cvd_rgb.v[0]), severity);
                d1[i] = (alwan_simd_lane)alwan_lerp(v_in.g, alwan_saturate(cvd_rgb.v[1]), severity);
                d2[i] = (alwan_simd_lane)alwan_lerp(v_in.b, alwan_saturate(cvd_rgb.v[2]), severity);
            }
        }
        alwan__store_tile_aos3(rgb_out, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_simulate_cvd_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                            alwan_cvd_type cvd_type, alwan_scalar severity,
                            size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    alwan_mat3x3 const *cvd_matrix;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:
    case ALWAN_CVD_PROTANOMALY:
        cvd_matrix = &CVD_PROTANOPIA;
        break;
    case ALWAN_CVD_DEUTERANOPIA:
    case ALWAN_CVD_DEUTERANOMALY:
        cvd_matrix = &CVD_DEUTERANOPIA;
        break;
    case ALWAN_CVD_TRITANOPIA:
    case ALWAN_CVD_TRITANOMALY:
        cvd_matrix = &CVD_TRITANOPIA;
        break;
    default:
        /* Identity: just copy */
        for (size_t i = 0; i < count; i++) {
            alwan_scalar const *in_ptr = (alwan_scalar const *)((char const *)rgb_in + i * in_stride);
            alwan_scalar *out_ptr = (alwan_scalar *)((char *)rgb_out + i * out_stride);
            out_ptr[0] = in_ptr[0]; out_ptr[1] = in_ptr[1]; out_ptr[2] = in_ptr[2];
        }
        return ALWAN_OK;
    }
    return alwan__cvd_brettel_map_interleave(rgb_out, rgb_in, cvd_matrix, severity, count, in_stride, out_stride);
}

/* ----------------------------------------------------------------
 * Individual CVD Type Map Functions
 * ---------------------------------------------------------------- */

int alwan_simulate_protanopia_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                   alwan_scalar severity,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return alwan__cvd_brettel_map_interleave(rgb_out, rgb_in, &CVD_PROTANOPIA, severity, count, in_stride, out_stride);
}

int alwan_simulate_deuteranopia_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                     alwan_scalar severity,
                                     size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return alwan__cvd_brettel_map_interleave(rgb_out, rgb_in, &CVD_DEUTERANOPIA, severity, count, in_stride, out_stride);
}

int alwan_simulate_tritanopia_map_interleave(alwan_scalar *rgb_out, alwan_scalar const *rgb_in,
                                   alwan_scalar severity,
                                   size_t count, size_t in_stride, size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;
    return alwan__cvd_brettel_map_interleave(rgb_out, rgb_in, &CVD_TRITANOPIA, severity, count, in_stride, out_stride);
}

/* ================================================================
 * Planar Map Variants
 * ================================================================ */

/* Shared SIMD CVD Brettel implementation (planar) */
static int alwan__cvd_brettel_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                          alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                          alwan_mat3x3 const *cvd_matrix, alwan_scalar severity,
                                          size_t count, size_t in_stride, size_t out_stride) {
    severity = alwan_clamp(severity, (alwan_scalar)0.0, (alwan_scalar)1.0);
    alwan_mat3x3 fused;
    alwan__fuse_cvd_matrix(&fused, cvd_matrix);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        ALWAN_ALIGN(32) alwan_simd_lane d0[ALWAN_TILE_PIXELS], d1[ALWAN_TILE_PIXELS], d2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in0, in1, in2, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd zero = alwan_simd_set1(0);
                alwan_simd one  = alwan_simd_set1(1);
                alwan_simd sev  = alwan_simd_set1((alwan_simd_lane)severity);
                alwan_simd inv_sev = alwan_simd_sub(one, sev);
                for (; i + W <= tile; i += W) {
                    alwan_simd r = alwan_simd_load(&c0[i]);
                    alwan_simd g = alwan_simd_load(&c1[i]);
                    alwan_simd b = alwan_simd_load(&c2[i]);
                    alwan_simd cr, cg, cb;
                    alwan__mat3_mul_simd(&cr, &cg, &cb, &fused, r, g, b);
                    cr = alwan_simd_clamp(cr, zero, one);
                    cg = alwan_simd_clamp(cg, zero, one);
                    cb = alwan_simd_clamp(cb, zero, one);
                    alwan_simd_store(&d0[i], alwan_simd_fmadd(r, inv_sev, alwan_simd_mul(cr, sev)));
                    alwan_simd_store(&d1[i], alwan_simd_fmadd(g, inv_sev, alwan_simd_mul(cg, sev)));
                    alwan_simd_store(&d2[i], alwan_simd_fmadd(b, inv_sev, alwan_simd_mul(cb, sev)));
                }
            }
#endif
            for (; i < tile; i++) {
                alwan_rgb v_in = {(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]};
                alwan_rgb v_out = alwan_simulate_cvd_matrix_v(v_in, *cvd_matrix, severity);
                d0[i] = (alwan_simd_lane)v_out.r; d1[i] = (alwan_simd_lane)v_out.g; d2[i] = (alwan_simd_lane)v_out.b;
            }
        }
        alwan__store_tile_planar3(out0, out1, out2, processed, out_stride, d0, d1, d2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

int alwan_simulate_cvd_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                    alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                    alwan_cvd_type cvd_type, alwan_scalar severity,
                                    size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    alwan_mat3x3 const *cvd_matrix;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:  case ALWAN_CVD_PROTANOMALY:  cvd_matrix = &CVD_PROTANOPIA;  break;
    case ALWAN_CVD_DEUTERANOPIA: case ALWAN_CVD_DEUTERANOMALY: cvd_matrix = &CVD_DEUTERANOPIA; break;
    case ALWAN_CVD_TRITANOPIA:  case ALWAN_CVD_TRITANOMALY:  cvd_matrix = &CVD_TRITANOPIA;  break;
    default:
        for (size_t i = 0; i < count; i++) {
            *(alwan_scalar *)((char *)out0 + i * out_stride) = *(alwan_scalar const *)((char const *)in0 + i * in_stride);
            *(alwan_scalar *)((char *)out1 + i * out_stride) = *(alwan_scalar const *)((char const *)in1 + i * in_stride);
            *(alwan_scalar *)((char *)out2 + i * out_stride) = *(alwan_scalar const *)((char const *)in2 + i * in_stride);
        }
        return ALWAN_OK;
    }
    return alwan__cvd_brettel_map_planar(out0, out1, out2, in0, in1, in2, cvd_matrix, severity, count, in_stride, out_stride);
}

int alwan_simulate_protanopia_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                           alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                           alwan_scalar severity, size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return alwan__cvd_brettel_map_planar(out0, out1, out2, in0, in1, in2, &CVD_PROTANOPIA, severity, count, in_stride, out_stride);
}

int alwan_simulate_deuteranopia_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                             alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                             alwan_scalar severity, size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return alwan__cvd_brettel_map_planar(out0, out1, out2, in0, in1, in2, &CVD_DEUTERANOPIA, severity, count, in_stride, out_stride);
}

int alwan_simulate_tritanopia_map_planar(alwan_scalar *out0, alwan_scalar *out1, alwan_scalar *out2,
                                           alwan_scalar const *in0, alwan_scalar const *in1, alwan_scalar const *in2,
                                           alwan_scalar severity, size_t count, size_t in_stride, size_t out_stride) {
    if (!in0 || !in1 || !in2 || !out0 || !out1 || !out2 || count == 0) return ALWAN_E_INVALID;
    return alwan__cvd_brettel_map_planar(out0, out1, out2, in0, in1, in2, &CVD_TRITANOPIA, severity, count, in_stride, out_stride);
}

/* ----------------------------------------------------------------
 * Machado 2009 CVD Batch Map
 * ---------------------------------------------------------------- */

int alwan_simulate_cvd_machado_map_interleave(alwan_scalar *rgb_out,
                                               alwan_scalar const *rgb_in,
                                               alwan_cvd_type cvd_type,
                                               alwan_scalar severity,
                                               size_t count,
                                               size_t in_stride,
                                               size_t out_stride) {
    if (!rgb_in || !rgb_out || count == 0) return ALWAN_E_INVALID;

    /* Select LUT once outside the loop */
    alwan_mat3x3 const *lut;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:
    case ALWAN_CVD_PROTANOMALY:
        lut = MACHADO_PROTAN;
        break;
    case ALWAN_CVD_DEUTERANOPIA:
    case ALWAN_CVD_DEUTERANOMALY:
        lut = MACHADO_DEUTAN;
        break;
    case ALWAN_CVD_TRITANOPIA:
    case ALWAN_CVD_TRITANOMALY:
        lut = MACHADO_TRITAN;
        break;
    default:
        return ALWAN_E_INVALID;
    }

    /* Interpolate matrix once (severity is constant across batch) */
    alwan_mat3x3 mat = alwan_machado_interpolate_v(lut, severity);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_aos3(c0, c1, c2, rgb_in, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd zero = alwan_simd_set1(0);
                alwan_simd one  = alwan_simd_set1(1);
                for (; i + W <= tile; i += W) {
                    alwan_simd r = alwan_simd_load(&c0[i]);
                    alwan_simd g = alwan_simd_load(&c1[i]);
                    alwan_simd b = alwan_simd_load(&c2[i]);
                    alwan_simd o0, o1, o2;
                    alwan__mat3_mul_simd(&o0, &o1, &o2, &mat, r, g, b);
                    alwan_simd_store(&c0[i], alwan_simd_clamp(o0, zero, one));
                    alwan_simd_store(&c1[i], alwan_simd_clamp(o1, zero, one));
                    alwan_simd_store(&c2[i], alwan_simd_clamp(o2, zero, one));
                }
            }
#endif
            for (; i < tile; i++) {
                alwan_vec3 v = {{(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]}};
                alwan_vec3 r = alwan_mat3_mulv_v(mat, v);
                c0[i] = (alwan_simd_lane)alwan_saturate(r.v[0]);
                c1[i] = (alwan_simd_lane)alwan_saturate(r.v[1]);
                c2[i] = (alwan_simd_lane)alwan_saturate(r.v[2]);
            }
        }
        alwan__store_tile_aos3(rgb_out, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Machado 2009 CVD Planar Map
 * ---------------------------------------------------------------- */

int alwan_simulate_cvd_machado_map_planar(alwan_scalar *out_r, alwan_scalar *out_g, alwan_scalar *out_b,
                                           alwan_scalar const *in_r, alwan_scalar const *in_g, alwan_scalar const *in_b,
                                           alwan_cvd_type cvd_type, alwan_scalar severity,
                                           size_t count, size_t in_stride, size_t out_stride) {
    if (!in_r || !in_g || !in_b || !out_r || !out_g || !out_b || count == 0) return ALWAN_E_INVALID;

    alwan_mat3x3 const *lut;
    switch (cvd_type) {
    case ALWAN_CVD_PROTANOPIA:  case ALWAN_CVD_PROTANOMALY:  lut = MACHADO_PROTAN; break;
    case ALWAN_CVD_DEUTERANOPIA: case ALWAN_CVD_DEUTERANOMALY: lut = MACHADO_DEUTAN; break;
    case ALWAN_CVD_TRITANOPIA:  case ALWAN_CVD_TRITANOMALY:  lut = MACHADO_TRITAN; break;
    default: return ALWAN_E_INVALID;
    }

    alwan_mat3x3 mat = alwan_machado_interpolate_v(lut, severity);

    size_t processed = 0;
    while (processed < count) {
        size_t tile = count - processed;
        if (tile > ALWAN_TILE_PIXELS) tile = ALWAN_TILE_PIXELS;
        ALWAN_ALIGN(32) alwan_simd_lane c0[ALWAN_TILE_PIXELS], c1[ALWAN_TILE_PIXELS], c2[ALWAN_TILE_PIXELS];
        alwan__load_tile_planar3(c0, c1, c2, in_r, in_g, in_b, processed, in_stride, tile);
        {
            size_t i = 0;
#if ALWAN_SIMD_WIDTH > 1
            {
                size_t const W = ALWAN_SIMD_WIDTH;
                alwan_simd zero = alwan_simd_set1(0);
                alwan_simd one  = alwan_simd_set1(1);
                for (; i + W <= tile; i += W) {
                    alwan_simd r = alwan_simd_load(&c0[i]);
                    alwan_simd g = alwan_simd_load(&c1[i]);
                    alwan_simd b = alwan_simd_load(&c2[i]);
                    alwan_simd o0, o1, o2;
                    alwan__mat3_mul_simd(&o0, &o1, &o2, &mat, r, g, b);
                    alwan_simd_store(&c0[i], alwan_simd_clamp(o0, zero, one));
                    alwan_simd_store(&c1[i], alwan_simd_clamp(o1, zero, one));
                    alwan_simd_store(&c2[i], alwan_simd_clamp(o2, zero, one));
                }
            }
#endif
            for (; i < tile; i++) {
                alwan_vec3 v = {{(alwan_scalar)c0[i], (alwan_scalar)c1[i], (alwan_scalar)c2[i]}};
                alwan_vec3 r = alwan_mat3_mulv_v(mat, v);
                c0[i] = (alwan_simd_lane)alwan_saturate(r.v[0]);
                c1[i] = (alwan_simd_lane)alwan_saturate(r.v[1]);
                c2[i] = (alwan_simd_lane)alwan_saturate(r.v[2]);
            }
        }
        alwan__store_tile_planar3(out_r, out_g, out_b, processed, out_stride, c0, c1, c2, tile);
        processed += tile;
    }
    return ALWAN_OK;
}
