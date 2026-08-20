/* Analytic AgX picture formation -- scalar apply.
 *
 * A fully parameterized, geometric AgX engine (everything geometric except the
 * single 1D Jed Smith sigmoid). The per-pixel worker lives in
 * core/alwan_agx_core.h, shared with the tiled map kernels so map == apply
 * byte-exact. Native dual precision: params_f32/apply_f32, params_f64/apply_f64.
 *
 * The default parameters reproduce alwan's SB2383 AgX view: the same Sobotka
 * inset matrix (from the shared gendata CSV) and the same Jed Smith tunable
 * sigmoid (slope 2.4, toe/shoulder power 1.5) the SB2383 contrast LUT was baked
 * from, with an identity outset and no split-tone.
 */
#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_agx_core.h"
#include "../core/alwan_agx_jp2499_core.h"   /* jp2499_geometry_* for the primary wheels */

/* SB2383 inset matrix (row-major) -- the same gendata CSV the SB2383 view bakes
 * from, so there are no duplicated matrix constants. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f32 const agx_sb2383_inset_default_f32[9] = {
#include "../data/matrices/agx_sb2383_inset.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
static alwan_f64 const agx_sb2383_inset_default_f64[9] = {
#include "../data/matrices/agx_sb2383_inset.csv"
};
#endif

/* SB2383 framing: -10/+6.5 EV around mid-grey 0.18, i.e. absolute log2 bounds
 * log2(0.18)+[-10,+6.5] = [-12.47393, 4.026069]. Sigmoid pivot output and slope
 * are Jed Smith's AgX defaults (fit of the SB2383 LUT to machine precision). */
#if ALWAN_WITH_F32
alwan_agx_params_f32 alwan_agx_default_params_f32(void) {
    alwan_agx_params_f32 p;
    int i;
    for (i = 0; i < 9; i++) p.inset.m[i] = agx_sb2383_inset_default_f32[i];
    p.log2_min = -12.47393f; p.log2_max = 4.026069f;
    p.pivot_input = 0.18f; p.pivot_output = 0.458656f;
    p.slope = 2.4f; p.toe_power = 1.5f; p.shoulder_power = 1.5f;
    for (i = 0; i < 9; i++) p.outset.m[i] = 0.0f;
    p.outset.m[0] = p.outset.m[4] = p.outset.m[8] = 1.0f;  /* identity = no restore */
    p.tip_upper_angle = p.tip_upper_force = p.tip_upper_offset = 0.0f;  /* rigid 0..1 */
    p.tip_lower_angle = p.tip_lower_force = p.tip_lower_offset = 0.0f;
    p.tip_middle_angle = p.tip_middle_force = 0.0f;                     /* neutral fulcrum */
    for (i = 0; i < 3; i++) { p.primary_rotation[i] = p.primary_inset[i] = p.primary_purity[i] = 0.0f; }
    return p;
}
#endif
#if ALWAN_WITH_F64
alwan_agx_params_f64 alwan_agx_default_params_f64(void) {
    alwan_agx_params_f64 p;
    int i;
    for (i = 0; i < 9; i++) p.inset.m[i] = agx_sb2383_inset_default_f64[i];
    p.log2_min = -12.47393; p.log2_max = 4.026069;
    p.pivot_input = 0.18; p.pivot_output = 0.458656;
    p.slope = 2.4; p.toe_power = 1.5; p.shoulder_power = 1.5;
    for (i = 0; i < 9; i++) p.outset.m[i] = 0.0;
    p.outset.m[0] = p.outset.m[4] = p.outset.m[8] = 1.0;
    p.tip_upper_angle = p.tip_upper_force = p.tip_upper_offset = 0.0;
    p.tip_lower_angle = p.tip_lower_force = p.tip_lower_offset = 0.0;
    p.tip_middle_angle = p.tip_middle_force = 0.0;
    for (i = 0; i < 3; i++) { p.primary_rotation[i] = p.primary_inset[i] = p.primary_purity[i] = 0.0; }
    return p;
}
#endif

/* Rebuild inset/outset from the per-primary geometry knobs. jp2499_geometry
 * yields the inset and the inverse-outset (the restore matrix), which is exactly
 * how AgX applies its outset (forward, after the sigmoid). */
#if ALWAN_WITH_F32
void alwan_agx_build_geometry_f32(alwan_agx_params_f32 *params) {
    if (!params) return;
    alwan_vec3_f32 rot, ins, pur;
    for (int k = 0; k < 3; k++) { rot.v[k] = params->primary_rotation[k]; ins.v[k] = params->primary_inset[k]; pur.v[k] = params->primary_purity[k]; }
    jp2499_geom_f32 g = jp2499_geometry_f32(rot, ins, pur);
    params->inset = g.inset; params->outset = g.outset_inv;
}
#endif
#if ALWAN_WITH_F64
void alwan_agx_build_geometry_f64(alwan_agx_params_f64 *params) {
    if (!params) return;
    alwan_vec3_f64 rot, ins, pur;
    for (int k = 0; k < 3; k++) { rot.v[k] = params->primary_rotation[k]; ins.v[k] = params->primary_inset[k]; pur.v[k] = params->primary_purity[k]; }
    jp2499_geom_f64 g = jp2499_geometry_f64(rot, ins, pur);
    params->inset = g.inset; params->outset = g.outset_inv;
}
#endif

#if ALWAN_WITH_F64
alwan_status alwan_agx_apply_f64(alwan_f64 *out, size_t out_stride,
                        alwan_f64 const *in, size_t in_stride, size_t count,
                        alwan_agx_params_f64 const *params) {
    if (!out || !in || !params) return ALWAN_E_INVALID;
    alwan_f64 lmin = params->log2_min, lmax = params->log2_max;
    alwan_f64 px = agx_log_encode_f64(params->pivot_input, lmin, lmax);
    alwan_f64 py = params->pivot_output, slope = params->slope;
    alwan_f64 tp = params->toe_power, sp = params->shoulder_power;
    alwan_f64 py3[3], st[3], ss[3];
    agx_tip_scales_f64(px, py, slope, tp, sp,
                       params->tip_upper_angle, params->tip_upper_force, params->tip_upper_offset,
                       params->tip_lower_angle, params->tip_lower_force, params->tip_lower_offset,
                       params->tip_middle_angle, params->tip_middle_force, py3, st, ss);
    /* Pack the per-call scales into vec3 for the struct-based (GPU-portable) render. */
    alwan_vec3_f64 pyv, stv, ssv;
    for (int k = 0; k < 3; k++) { pyv.v[k] = py3[k]; stv.v[k] = st[k]; ssv.v[k] = ss[k]; }
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *pi = (alwan_f64 const *)((char const *)in + i * in_stride);
        alwan_f64 *po = (alwan_f64 *)((char *)out + i * out_stride);
        alwan_vec3_f64 in_v; in_v.v[0] = pi[0]; in_v.v[1] = pi[1]; in_v.v[2] = pi[2];
        alwan_vec3_f64 out_v = agx_render_f64(in_v, params->inset, lmin, lmax, px, pyv, slope, tp, sp, stv, ssv, params->outset);
        po[0] = out_v.v[0]; po[1] = out_v.v[1]; po[2] = out_v.v[2];
    }
    return ALWAN_OK;
}
#endif

#if ALWAN_WITH_F32
alwan_status alwan_agx_apply_f32(alwan_f32 *out, size_t out_stride,
                        alwan_f32 const *in, size_t in_stride, size_t count,
                        alwan_agx_params_f32 const *params) {
    if (!out || !in || !params) return ALWAN_E_INVALID;
    alwan_f32 lmin = params->log2_min, lmax = params->log2_max;
    alwan_f32 px = agx_log_encode_f32(params->pivot_input, lmin, lmax);
    alwan_f32 py = params->pivot_output, slope = params->slope;
    alwan_f32 tp = params->toe_power, sp = params->shoulder_power;
    alwan_f32 py3[3], st[3], ss[3];
    agx_tip_scales_f32(px, py, slope, tp, sp,
                       params->tip_upper_angle, params->tip_upper_force, params->tip_upper_offset,
                       params->tip_lower_angle, params->tip_lower_force, params->tip_lower_offset,
                       params->tip_middle_angle, params->tip_middle_force, py3, st, ss);
    alwan_vec3_f32 pyv, stv, ssv;
    for (int k = 0; k < 3; k++) { pyv.v[k] = py3[k]; stv.v[k] = st[k]; ssv.v[k] = ss[k]; }
    for (size_t i = 0; i < count; i++) {
        alwan_f32 const *pi = (alwan_f32 const *)((char const *)in + i * in_stride);
        alwan_f32 *po = (alwan_f32 *)((char *)out + i * out_stride);
        alwan_vec3_f32 in_v; in_v.v[0] = pi[0]; in_v.v[1] = pi[1]; in_v.v[2] = pi[2];
        alwan_vec3_f32 out_v = agx_render_f32(in_v, params->inset, lmin, lmax, px, pyv, slope, tp, sp, stv, ssv, params->outset);
        po[0] = out_v.v[0]; po[1] = out_v.v[1]; po[2] = out_v.v[2];
    }
    return ALWAN_OK;
}
#endif
