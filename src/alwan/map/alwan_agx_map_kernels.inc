/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Map analytic-AgX kernel (parameterized precision). No include guard --
 * included once per precision pass via alwan_agx_map.c.
 *
 * The AgX per-pixel worker (log2 + Jed Smith sigmoid + two 3x3 matrices) does
 * not vectorize into SIMD lanes, so -- like the CAM / JP2499 map kernels -- the
 * interleaved path calls the shared scalar core worker per pixel. Because both
 * the scalar apply (api/alwan_agx.c) and this map path call the same
 * agx_render_*, the map output is byte-identical to the scalar output in both
 * fast and deterministic builds.
 */

int ALWAN_MAP_LITCAT(alwan_agx, _map_interleave)(
        alwan_map_lane *out, size_t out_stride,
        alwan_map_lane const *in, size_t in_stride,
        size_t count, ALWAN_MAP_LIT(alwan_agx_params) const *params) {
    if (!out || !in || !params || count == 0) return ALWAN_E_INVALID;

    /* Per-call setup (log-encoded pivot + per-channel cube-tip scales) in lane
     * precision. */
    alwan_map_lane lmin = params->log2_min, lmax = params->log2_max;
    alwan_map_lane px = ALWAN_MAP_LIT(agx_log_encode)(params->pivot_input, lmin, lmax);
    alwan_map_lane py = params->pivot_output, slope = params->slope;
    alwan_map_lane tp = params->toe_power, sp = params->shoulder_power;
    alwan_map_lane py3[3], st[3], ss[3];
    ALWAN_MAP_LIT(agx_tip_scales)(px, py, slope, tp, sp,
                                  params->tip_upper_angle, params->tip_upper_force, params->tip_upper_offset,
                                  params->tip_lower_angle, params->tip_lower_force, params->tip_lower_offset,
                                  params->tip_middle_angle, params->tip_middle_force, py3, st, ss);
    ALWAN_MAP_LIT(alwan_vec3) pyv, stv, ssv;
    for (int k = 0; k < 3; k++) { pyv.v[k] = py3[k]; stv.v[k] = st[k]; ssv.v[k] = ss[k]; }

    for (size_t i = 0; i < count; i++) {
        alwan_map_lane const *pi = (alwan_map_lane const *)((char const *)in + i * in_stride);
        alwan_map_lane *po = (alwan_map_lane *)((char *)out + i * out_stride);
        ALWAN_MAP_LIT(alwan_vec3) in_v; in_v.v[0] = pi[0]; in_v.v[1] = pi[1]; in_v.v[2] = pi[2];
        ALWAN_MAP_LIT(alwan_vec3) out_v = ALWAN_MAP_LIT(agx_render)(in_v, params->inset, lmin, lmax, px, pyv, slope, tp, sp, stv, ssv, params->outset);
        po[0] = out_v.v[0]; po[1] = out_v.v[1]; po[2] = out_v.v[2];
    }
    return ALWAN_OK;
}
