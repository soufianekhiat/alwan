/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map JP2499 kernel (parameterized precision). No include guard -- included
 * once per precision pass via alwan_agx_jp2499_map.c.
 *
 * JP2499's per-pixel worker (LUT-free but pow/ratio heavy) does not vectorize
 * into SIMD lanes, so -- like the CAM map kernels -- the interleaved path calls
 * the shared scalar core worker per pixel. Because both the scalar apply and
 * this map path call the same jp2499_render_*, the map output is byte-identical
 * to the scalar output in both fast and deterministic builds.
 */

int ALWAN_MAP_LITCAT(alwan_jp2499, _map_interleave)(
        alwan_map_lane *out, size_t out_stride,
        alwan_map_lane const *in, size_t in_stride,
        size_t count, ALWAN_MAP_LIT(alwan_jp2499_params) const *params) {
    if (!out || !in || !params || count == 0) return ALWAN_E_INVALID;

    /* Per-call setup (tonescale + hue geometry) natively in the lane precision. */
    alwan_map_lane Lp = params->peak_luminance > 0 ? params->peak_luminance : (alwan_map_lane)100;
    ALWAN_MAP_LIT(jp2499_ts) ts = ALWAN_MAP_LIT(jp2499_tonescale_params)(Lp);
    ALWAN_MAP_LIT(alwan_vec3) hf, ca, pu, wt, bt;
    for (int k = 0; k < 3; k++) {
        hf.v[k] = params->hue_flight[k];
        ca.v[k] = params->chroma_attenuation[k];
        pu.v[k] = params->purity[k];
    }
    wt.v[0] = params->white_tip.r; wt.v[1] = params->white_tip.g; wt.v[2] = params->white_tip.b;
    bt.v[0] = params->black_tip.r; bt.v[1] = params->black_tip.g; bt.v[2] = params->black_tip.b;
    ALWAN_MAP_LIT(jp2499_geom) geo = ALWAN_MAP_LIT(jp2499_geometry)(hf, ca, pu);
    alwan_map_lane const ds = (alwan_map_lane)1;

    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *pi = (alwan_map_lane const *)((char const *)in + i * in_stride);
        alwan_map_lane *po = (alwan_map_lane *)((char *)out + i * out_stride);
        ALWAN_MAP_LIT(alwan_vec3) in_v; in_v.v[0] = pi[0]; in_v.v[1] = pi[1]; in_v.v[2] = pi[2];
        ALWAN_MAP_LIT(alwan_vec3) out_v = ALWAN_MAP_LIT(jp2499_render)(in_v, ts.m, ts.s, ts.c, ts.fl, ds,
                                                                       geo.inset, geo.outset_inv, wt, bt);
        po[0] = out_v.v[0]; po[1] = out_v.v[1]; po[2] = out_v.v[2];
    }
    return ALWAN_OK;
}
