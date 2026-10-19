/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map Color Appearance Model kernels + wrappers (parameterized precision).
 * No include guard - included twice via setup/teardown.
 *
 * Uses alwan_map_* macros (set by alwan_map_simd_defs.h).
 */

/* The CIECAM02 and CAM16 maps delegate to the batch workers in
 * api/alwan_cam_impl.inc, which compute the viewing-condition terms once. */

/* Precision-dependent type aliases + native scalar bindings for this pass.
 * The map lane precision (f32/f64) selects the precision-matched native
 * scalar worker so the f32 pass computes in f32 (no widen-to-f64 facade)
 * and the f64 pass keeps the exact f64 worker. */
#ifdef ALWAN_MAP_F32
#define ALWAN_CAM_MAP_CIECAM02_CORR  alwan_ciecam02_correlates_f32
#define ALWAN_CAM_MAP_CIECAM02_VC    alwan_ciecam02_viewing_conditions_f32
#define ALWAN_CAM_MAP_CAM16_CORR     alwan_cam16_correlates_f32
#define ALWAN_CAM_MAP_CAM16_VC       alwan_cam16_viewing_conditions_f32
#define ALWAN_CAM_MAP_XYZ            alwan_xyz_f32
#define ALWAN_CAM_MAP_CIECAM02_FWD   alwan_ciecam02_forward_f32
#define ALWAN_CAM_MAP_CIECAM02_INV   alwan_ciecam02_inverse_f32
#define ALWAN_CAM_MAP_CAM16_FWD      alwan_cam16_forward_f32
#define ALWAN_CAM_MAP_CAM16_INV      alwan_cam16_inverse_f32
#else
#define ALWAN_CAM_MAP_CIECAM02_CORR  alwan_ciecam02_correlates_f64
#define ALWAN_CAM_MAP_CIECAM02_VC    alwan_ciecam02_viewing_conditions_f64
#define ALWAN_CAM_MAP_CAM16_CORR     alwan_cam16_correlates_f64
#define ALWAN_CAM_MAP_CAM16_VC       alwan_cam16_viewing_conditions_f64
#define ALWAN_CAM_MAP_XYZ            alwan_xyz_f64
#define ALWAN_CAM_MAP_CIECAM02_FWD   alwan_ciecam02_forward_f64
#define ALWAN_CAM_MAP_CIECAM02_INV   alwan_ciecam02_inverse_f64
#define ALWAN_CAM_MAP_CAM16_FWD      alwan_cam16_forward_f64
#define ALWAN_CAM_MAP_CAM16_INV      alwan_cam16_inverse_f64
#endif

/* ----------------------------------------------------------------
 * Map CIECAM02 Conversions
 * ---------------------------------------------------------------- */

alwan_status ALWAN_MAP_LITCAT(alwan_ciecam02_forward, _map_interleave)(ALWAN_CAM_MAP_CIECAM02_CORR *correlates_out, alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_NAME(alwan__ciecam02_forward_n)(correlates_out, xyz_in, in_stride, vc, count);
}

alwan_status ALWAN_MAP_LITCAT(alwan_ciecam02_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CIECAM02_CORR const *correlates_in, ALWAN_CAM_MAP_CIECAM02_VC const *vc, size_t count)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_NAME(alwan__ciecam02_inverse_n)(xyz_out, out_stride, correlates_in, vc, count);
}

/* ----------------------------------------------------------------
 * Map CAM16 Conversions
 * ---------------------------------------------------------------- */

#if ALWAN_MAP_SIMD_WIDTH > 1
/* CAM16 forward across lanes. The viewing-condition terms are the batch worker's,
 * computed once; each lane then runs the steps of alwan_cam16_forward_v in the same
 * order with the same constants, so the only difference from the scalar is the
 * vector pow, atan2 and cos (SVML on MSVC): within a few ULP, held by suite 172.
 * Hue quadrature is a five-sector piecewise rational, evaluated per lane by the
 * scalar core on the lane's h. The deterministic build has width 1 and never gets
 * here. */
static alwan_status ALWAN_MAP_NAME(alwan__cam16_forward_simd)(ALWAN_CAM_MAP_CAM16_CORR *out,
        alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
{
    size_t const W = ALWAN_MAP_SIMD_WIDTH;
    alwan_map_lane F, c, Nc;
    size_t processed = 0;
    if (vc->white_xyz.y <= (alwan_map_lane)0.0 || vc->background_luminance <= (alwan_map_lane)0.0) return ALWAN_E_DIVZERO;
    ALWAN_MAP_NAME(alwan__cam16_surround)(vc->surround, &F, &c, &Nc);
    {
        ALWAN_MAP_LIT(alwan_cam_derived_params) const p = ALWAN_MAP_LITV(cam_compute_derived_params)(
            F, vc->adapting_luminance, vc->background_luminance,
            vc->white_xyz.y, (alwan_map_lane)vc->discount_illuminant,
            vc->white_xyz, ALWAN_MAP_LIT(CAM_M_CAT16), ALWAN_MAP_LIT(CAM_M_CAT16_INV), ALWAN_MAP_LIT(CAM_M_CAT16), 0);
        /* Per-call terms, each spelled as the scalar core spells it per pixel. */
        ALWAN_MAP_LIT(alwan_vec3) const white_v = {{vc->white_xyz.x, vc->white_xyz.y, vc->white_xyz.z}};
        ALWAN_MAP_LIT(alwan_vec3) const lms_w = ALWAN_MAP_LITV(alwan_mat3_mulv)(ALWAN_MAP_LIT(CAM_M_CAT16), white_v);
        alwan_map_lane const one = (alwan_map_lane)1.0;
        alwan_map_lane const D_R = p.D * (vc->white_xyz.y / lms_w.v[0]) + one - p.D;
        alwan_map_lane const D_G = p.D * (vc->white_xyz.y / lms_w.v[1]) + one - p.D;
        alwan_map_lane const D_B = p.D * (vc->white_xyz.y / lms_w.v[2]) + one - p.D;
        alwan_map_lane const fl_q = alwan_map_scalar_pow(p.FL, (alwan_map_lane)0.25);
        alwan_map_lane const chroma_k = alwan_map_scalar_pow((alwan_map_lane)1.64 - alwan_map_scalar_pow((alwan_map_lane)0.29, p.n), (alwan_map_lane)0.73);
        alwan_map_lane const t_k = (alwan_map_lane)50000.0 / (alwan_map_lane)13.0 * Nc * p.Ncb;
#ifdef ALWAN_MAP_F32
        alwan_map_lane const pi = ALWAN_PI_F32;   /* the core's ALWAN_CORE_PI in this precision */
#else
        alwan_map_lane const pi = ALWAN_PI_F64;
#endif
        alwan_map_simd const vDR = alwan_map_simd_set1(D_R), vDG = alwan_map_simd_set1(D_G), vDB = alwan_map_simd_set1(D_B);
        alwan_map_simd const vFL = alwan_map_simd_set1(p.FL), v100 = alwan_map_simd_set1((alwan_map_lane)100.0);
        alwan_map_simd const v042 = alwan_map_simd_set1((alwan_map_lane)0.42), v400 = alwan_map_simd_set1((alwan_map_lane)400.0);
        alwan_map_simd const v2713 = alwan_map_simd_set1((alwan_map_lane)27.13), v01 = alwan_map_simd_set1((alwan_map_lane)0.1);
        alwan_map_simd const zero = alwan_map_simd_zero();
        alwan_map_simd const vNbb = alwan_map_simd_set1(p.Nbb), vAw = alwan_map_simd_set1(p.A_w);
        alwan_map_simd const vcz = alwan_map_simd_set1(c * p.z);
        alwan_map_simd const v4c = alwan_map_simd_set1((alwan_map_lane)4.0 / c), vAw4 = alwan_map_simd_set1(p.A_w + (alwan_map_lane)4.0);
        alwan_map_simd const vflq = alwan_map_simd_set1(fl_q), vck = alwan_map_simd_set1(chroma_k), vtk = alwan_map_simd_set1(t_k);
        alwan_map_simd const v180 = alwan_map_simd_set1((alwan_map_lane)180.0), vpi = alwan_map_simd_set1(pi);
        alwan_map_simd const v360 = alwan_map_simd_set1((alwan_map_lane)360.0);
        alwan_map_simd const v2 = alwan_map_simd_set1((alwan_map_lane)2.0), v025 = alwan_map_simd_set1((alwan_map_lane)0.25);
        alwan_map_simd const v38 = alwan_map_simd_set1((alwan_map_lane)3.8), v09 = alwan_map_simd_set1((alwan_map_lane)0.9);
        alwan_map_simd const v005 = alwan_map_simd_set1((alwan_map_lane)0.05), v0305 = alwan_map_simd_set1((alwan_map_lane)0.305);
        alwan_map_simd const v12 = alwan_map_simd_set1((alwan_map_lane)12.0), v11 = alwan_map_simd_set1((alwan_map_lane)11.0);
        alwan_map_simd const v9 = alwan_map_simd_set1((alwan_map_lane)9.0);
        alwan_map_simd const v2120 = alwan_map_simd_set1((alwan_map_lane)21.0 / (alwan_map_lane)20.0);

        while (processed + W <= count) {
            size_t tile = count - processed;
            size_t i;
            ALWAN_ALIGN(32) alwan_map_lane c0[ALWAN_MAP_TILE_PIXELS], c1[ALWAN_MAP_TILE_PIXELS], c2[ALWAN_MAP_TILE_PIXELS];
            ALWAN_ALIGN(32) alwan_map_lane oJ[ALWAN_MAP_SIMD_WIDTH], oC[ALWAN_MAP_SIMD_WIDTH], oh[ALWAN_MAP_SIMD_WIDTH];
            ALWAN_ALIGN(32) alwan_map_lane os[ALWAN_MAP_SIMD_WIDTH], oQ[ALWAN_MAP_SIMD_WIDTH], oM[ALWAN_MAP_SIMD_WIDTH];
            if (tile > ALWAN_MAP_TILE_PIXELS) tile = ALWAN_MAP_TILE_PIXELS;
            tile -= tile % W;   /* whole vectors; the last few pixels go to the scalar worker */
            ALWAN_MAP_NAME(alwan__load_tile_aos3)(c0, c1, c2, xyz_in, processed, in_stride, tile);
            for (i = 0; i < tile; i += W) {
                size_t k;
                alwan_map_simd vR, vG, vB, x, xa, xp, mag, Ra, Ga, Ba, A, a, b, h, et, J, sJ, Q, t, Cv, M;
                ALWAN_MAP_NAME(alwan__mat3_mul_simd)(&vR, &vG, &vB, &ALWAN_MAP_LIT(CAM_M_CAT16),
                    alwan_map_simd_load(&c0[i]), alwan_map_simd_load(&c1[i]), alwan_map_simd_load(&c2[i]));
                /* Post-adaptation compression, the scalar's sign-symmetric form. */
#define ALWAN__CAM16_POST(dst, chan, Dk)                                                          \
                x = alwan_map_simd_div(alwan_map_simd_mul(vFL, alwan_map_simd_mul(chan, Dk)), v100); \
                xa = alwan_map_simd_abs(x);                                                       \
                xp = alwan_map_simd_pow(xa, v042);                                                \
                mag = alwan_map_simd_add(alwan_map_simd_div(alwan_map_simd_mul(v400, xp),         \
                                                            alwan_map_simd_add(v2713, xp)), v01); \
                dst = alwan_map_simd_select(alwan_map_simd_cmplt(x, zero), alwan_map_simd_neg(mag), mag)
                ALWAN__CAM16_POST(Ra, vR, vDR);
                ALWAN__CAM16_POST(Ga, vG, vDG);
                ALWAN__CAM16_POST(Ba, vB, vDB);
#undef ALWAN__CAM16_POST
                A = alwan_map_simd_mul(alwan_map_simd_sub(alwan_map_simd_add(alwan_map_simd_add(
                        alwan_map_simd_mul(v2, Ra), Ga), alwan_map_simd_mul(v005, Ba)), v0305), vNbb);
                a = alwan_map_simd_add(alwan_map_simd_sub(Ra, alwan_map_simd_div(alwan_map_simd_mul(v12, Ga), v11)),
                                       alwan_map_simd_div(Ba, v11));
                b = alwan_map_simd_div(alwan_map_simd_sub(alwan_map_simd_add(Ra, Ga), alwan_map_simd_mul(v2, Ba)), v9);
                h = alwan_map_simd_div(alwan_map_simd_mul(alwan_map_simd_atan2(b, a), v180), vpi);
                h = alwan_map_simd_select(alwan_map_simd_cmplt(h, zero), alwan_map_simd_add(h, v360), h);
                et = alwan_map_simd_mul(v025, alwan_map_simd_add(alwan_map_simd_cos(alwan_map_simd_add(
                        alwan_map_simd_div(alwan_map_simd_mul(h, vpi), v180), v2)), v38));
                J = alwan_map_simd_mul(v100, alwan_map_simd_pow(alwan_map_simd_div(A, vAw), vcz));
                sJ = alwan_map_simd_sqrt(alwan_map_simd_div(J, v100));
                Q = alwan_map_simd_mul(alwan_map_simd_mul(alwan_map_simd_mul(v4c, sJ), vAw4), vflq);
                t = alwan_map_simd_div(
                        alwan_map_simd_mul(alwan_map_simd_mul(vtk, et),
                            alwan_map_simd_sqrt(alwan_map_simd_add(alwan_map_simd_mul(a, a), alwan_map_simd_mul(b, b)))),
                        alwan_map_simd_add(alwan_map_simd_add(Ra, Ga), alwan_map_simd_mul(v2120, Ba)));
                Cv = alwan_map_simd_mul(alwan_map_simd_mul(alwan_map_simd_pow(t, v09), sJ), vck);
                M = alwan_map_simd_mul(Cv, vflq);
                alwan_map_simd_store(oJ, J);
                alwan_map_simd_store(oC, Cv);
                alwan_map_simd_store(oh, h);
                alwan_map_simd_store(oQ, Q);
                alwan_map_simd_store(oM, M);
                alwan_map_simd_store(os, alwan_map_simd_mul(v100, alwan_map_simd_sqrt(alwan_map_simd_div(M, Q))));
                for (k = 0; k < W; k++) {
                    ALWAN_CAM_MAP_CAM16_CORR *o = out + processed + i + k;
                    o->J = oJ[k]; o->C = oC[k]; o->h = oh[k]; o->s = os[k];
                    o->Q = oQ[k]; o->M = oM[k];
                    o->H = ALWAN_MAP_LITV(cam_hue_to_quadrature)(oh[k]);
                    ALWAN_NORM_CAM16(o);
                }
            }
            processed += tile;
        }
    }
    if (processed < count)
        return ALWAN_MAP_NAME(alwan__cam16_forward_n)(out + processed,
            (alwan_map_lane const *)((char const *)xyz_in + processed * in_stride), in_stride, vc, count - processed);
    return ALWAN_OK;
}
#endif

alwan_status ALWAN_MAP_LITCAT(alwan_cam16_forward, _map_interleave)(ALWAN_CAM_MAP_CAM16_CORR *correlates_out, alwan_map_lane const *xyz_in, size_t in_stride, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
{
    if (!xyz_in || !correlates_out || !vc || count == 0) return ALWAN_E_INVALID;
#if ALWAN_MAP_SIMD_WIDTH > 1
    return ALWAN_MAP_NAME(alwan__cam16_forward_simd)(correlates_out, xyz_in, in_stride, vc, count);
#else
    return ALWAN_MAP_NAME(alwan__cam16_forward_n)(correlates_out, xyz_in, in_stride, vc, count);
#endif
}

alwan_status ALWAN_MAP_LITCAT(alwan_cam16_inverse, _map_interleave)(alwan_map_lane *xyz_out, size_t out_stride, ALWAN_CAM_MAP_CAM16_CORR const *correlates_in, ALWAN_CAM_MAP_CAM16_VC const *vc, size_t count)
{
    if (!correlates_in || !xyz_out || !vc || count == 0) return ALWAN_E_INVALID;
    return ALWAN_MAP_NAME(alwan__cam16_inverse_n)(xyz_out, out_stride, correlates_in, vc, count);
}

#undef ALWAN_CAM_MAP_CIECAM02_CORR
#undef ALWAN_CAM_MAP_CIECAM02_VC
#undef ALWAN_CAM_MAP_CAM16_CORR
#undef ALWAN_CAM_MAP_CAM16_VC
#undef ALWAN_CAM_MAP_XYZ
#undef ALWAN_CAM_MAP_CIECAM02_FWD
#undef ALWAN_CAM_MAP_CIECAM02_INV
#undef ALWAN_CAM_MAP_CAM16_FWD
#undef ALWAN_CAM_MAP_CAM16_INV
