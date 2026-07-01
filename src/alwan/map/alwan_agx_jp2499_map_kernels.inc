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
    alwan_map_lane m, s, c, fl;
    ALWAN_MAP_LIT(jp2499_tonescale_params)(Lp, &m, &s, &c, &fl);
    alwan_map_lane inset[9], outset_inv[9];
    ALWAN_MAP_LIT(jp2499_geometry)(params->hue_flight, params->chroma_attenuation, params->purity, inset, outset_inv);
    alwan_map_lane const ds = (alwan_map_lane)1;

    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *pi = (alwan_map_lane const *)((char const *)in + i * in_stride);
        alwan_map_lane *po = (alwan_map_lane *)((char *)out + i * out_stride);
        ALWAN_MAP_LIT(jp2499_render)(pi, po, m, s, c, fl, ds, inset, outset_inv,
                                     (alwan_map_lane const *)&params->white_tip, (alwan_map_lane const *)&params->black_tip);
    }
    return ALWAN_OK;
}
