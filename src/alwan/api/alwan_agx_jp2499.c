/* JP2499 picture formation (Juan Pablo Zambrano's "2499" DRT) -- scalar apply.
 *
 * A purely analytical display-rendering transform with exposed creative
 * controls (peak luminance, and per-primary hue-flight / chroma-attenuation /
 * purity). The per-pixel worker lives in core/alwan_agx_jp2499_core.h, shared
 * with the tiled map kernels.
 *
 * Native dual precision: params_f32/apply_f32 and params_f64/apply_f64. The
 * hue geometry + tonescale setup is computed once per call in f64 and cast to
 * the working precision for the per-pixel render.
 */
#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_agx_jp2499_core.h"

/* JP2499's default creative controls (jedypod/JP2499 Jp-DRT.dctl UI defaults) --
 * the look Juan Pablo validated. Pass all-zero params for the plain tonescale
 * with no hue geometry instead. */
alwan_jp2499_params_f32 alwan_jp2499_default_params_f32(void) {
    alwan_jp2499_params_f32 p;
    p.chroma_attenuation[0] = 0.15f; p.chroma_attenuation[1] = 0.20f; p.chroma_attenuation[2] = 0.128f;
    p.hue_flight[0]         = 0.08f; p.hue_flight[1]         = -0.05f; p.hue_flight[2]        = -0.142f;
    p.purity[0]             = 0.15f; p.purity[1]             = 0.20f; p.purity[2]             = 0.0f;
    p.peak_luminance        = 100.0f;
    p.white_tip.r = p.white_tip.g = p.white_tip.b = 0.0f;
    p.black_tip.r = p.black_tip.g = p.black_tip.b = 0.0f;
    return p;
}
alwan_jp2499_params_f64 alwan_jp2499_default_params_f64(void) {
    alwan_jp2499_params_f64 p;
    p.chroma_attenuation[0] = 0.15; p.chroma_attenuation[1] = 0.20; p.chroma_attenuation[2] = 0.128;
    p.hue_flight[0]         = 0.08; p.hue_flight[1]         = -0.05; p.hue_flight[2]        = -0.142;
    p.purity[0]             = 0.15; p.purity[1]             = 0.20; p.purity[2]             = 0.0;
    p.peak_luminance        = 100.0;
    p.white_tip.r = p.white_tip.g = p.white_tip.b = 0.0;
    p.black_tip.r = p.black_tip.g = p.black_tip.b = 0.0;
    return p;
}

#if ALWAN_WITH_F64
int alwan_jp2499_apply_f64(alwan_f64 *out, size_t out_stride,
                           alwan_f64 const *in, size_t in_stride, size_t count,
                           alwan_jp2499_params_f64 const *params) {
    if (!out || !in || !params) return ALWAN_E_INVALID;
    alwan_f64 Lp = params->peak_luminance > 0.0 ? params->peak_luminance : 100.0;
    alwan_f64 m, s, c, fl;
    jp2499_tonescale_params_f64(Lp, &m, &s, &c, &fl);
    alwan_f64 inset[9], outset_inv[9];
    jp2499_geometry_f64(params->hue_flight, params->chroma_attenuation, params->purity, inset, outset_inv);
    alwan_f64 const ds = 1.0;  /* SDR display scale (linear output; caller encodes) */
    for (size_t i = 0; i < count; i++) {
        alwan_f64 const *pi = (alwan_f64 const *)((char const *)in + i * in_stride);
        alwan_f64 *po = (alwan_f64 *)((char *)out + i * out_stride);
        jp2499_render_f64(pi, po, m, s, c, fl, ds, inset, outset_inv,
                          (alwan_f64 const *)&params->white_tip, (alwan_f64 const *)&params->black_tip);
    }
    return ALWAN_OK;
}
#endif

#if ALWAN_WITH_F32
int alwan_jp2499_apply_f32(alwan_f32 *out, size_t out_stride,
                           alwan_f32 const *in, size_t in_stride, size_t count,
                           alwan_jp2499_params_f32 const *params) {
    if (!out || !in || !params) return ALWAN_E_INVALID;
    alwan_f32 Lp = params->peak_luminance > 0.0f ? params->peak_luminance : 100.0f;
    alwan_f32 m, s, c, fl;
    jp2499_tonescale_params_f32(Lp, &m, &s, &c, &fl);
    alwan_f32 inset[9], outset_inv[9];
    jp2499_geometry_f32(params->hue_flight, params->chroma_attenuation, params->purity, inset, outset_inv);
    alwan_f32 const ds = 1.0f;
    for (size_t i = 0; i < count; i++) {
        alwan_f32 const *pi = (alwan_f32 const *)((char const *)in + i * in_stride);
        alwan_f32 *po = (alwan_f32 *)((char *)out + i * out_stride);
        jp2499_render_f32(pi, po, m, s, c, fl, ds, inset, outset_inv,
                          (alwan_f32 const *)&params->white_tip, (alwan_f32 const *)&params->black_tip);
    }
    return ALWAN_OK;
}
#endif
