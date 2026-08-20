/* ================================================================
 * Alwan - Color Vision & Perception
 *
 * Enum resolution and LUT-based spectral lookups live here;
 * per-pixel math in alwan_vision_core.h.
 * ================================================================ */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_vision_core.h"
#include "../core/alwan_quality_core.h"

/* ================================================================
 * Luminous Efficiency Function tables (f64 storage)
 * ================================================================ */

/* CIE 1924 Photopic V(lambda) - interleaved {wavelength, value} pairs
 * 42 samples, 380-780 nm (10 nm step + peak at 555 nm)
 * Generated from colour-science SDS_LEFS['CIE 1924 Photopic Standard Observer']
 *
 * Dual-declared: the f32 twin lets the templated f32 luminous-efficiency path
 * read native float data instead of widening doubles per access. */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static float const PHOTOPIC_V_DATA_f32[] = {
#include "../data/vision/photopic_v_lambda.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const PHOTOPIC_V_DATA_f64[] = {
#include "../data/vision/photopic_v_lambda.csv"
};
ALWAN_DIAG_POP
#endif
#define PHOTOPIC_V_COUNT 42

/* CIE 1951 Scotopic V'(lambda) - interleaved {wavelength, value} pairs
 * 42 samples, 380-780 nm (10 nm step + peak at 507 nm)
 * Generated from colour-science SDS_LEFS['CIE 1951 Scotopic Standard Observer']
 *
 * Dual-declared (see PHOTOPIC_V_DATA above). */
#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static float const SCOTOPIC_VP_DATA_f32[] = {
#include "../data/vision/scotopic_vp_lambda.csv"
};
ALWAN_DIAG_POP
#endif
#if ALWAN_WITH_F64
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const SCOTOPIC_VP_DATA_f64[] = {
#include "../data/vision/scotopic_vp_lambda.csv"
};
ALWAN_DIAG_POP
#endif
#define SCOTOPIC_VP_COUNT 42

/* interpolate_lut() and the photopic/scotopic interpolators are templatized
 * per precision inside alwan_vision_impl.inc (ALWAN_CORE_FNLIT helpers), so the
 * f32 path reads PHOTOPIC_V_DATA_f32 / SCOTOPIC_VP_DATA_f32 natively. */

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_vision_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_vision_impl.inc"
#include "alwan_api_teardown.h"
#endif

/* SPD-based luminance functions (alwan_photopic_luminance_*, etc.)
 * are templatized in alwan_vision_impl.inc. */
/* alwan_apca_contrast_f32 / alwan_apca_contrast_f64 generated via alwan_vision_impl.inc */

/* ----------------------------------------------------------------
 * Gamut-safe CVD simulation variants
 *
 * The raw CVD simulators (Brettel and Machado) return the mathematically
 * simulated colour without any gamut clamp -- saturated inputs can land
 * outside [0,1]. These variants run the raw simulation and then map the
 * result into the sRGB gamut with the requested method; output is
 * guaranteed in [0,1]. ALWAN_GAMUT_MAP_CLIP reproduces the old implicit
 * clamping behaviour (post-simulation clip).
 * ---------------------------------------------------------------- */

#if ALWAN_WITH_F64
static int alwan__cvd_gamut_fixup_f64(alwan_rgb_f64 *rgb, alwan_gamut_map_method method) {
    alwan_rgb_space_desc_f64 space;
    int st = alwan_rgb_get_space_descriptor_f64(&space, ALWAN_RGB_SPACE_SRGB, NULL);
    if (st != ALWAN_OK) return st;
    alwan_rgb_f64 raw = *rgb;
    return alwan_gamut_map_advanced_f64(rgb, method, &space, &raw);
}

alwan_status alwan_simulate_cvd_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                      alwan_cvd_type cvd_type, alwan_f64 severity,
                                      alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_f64(rgb_out, rgb_in, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    return alwan__cvd_gamut_fixup_f64(rgb_out, method);
}

alwan_status alwan_simulate_cvd_machado_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                              alwan_cvd_type cvd_type, alwan_f64 severity,
                                              alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_machado_f64(rgb_out, rgb_in, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    return alwan__cvd_gamut_fixup_f64(rgb_out, method);
}

alwan_status alwan_simulate_cvd_ex_gamut_safe_f64(alwan_rgb_f64 *rgb_out, alwan_rgb_f64 const *rgb_in,
                                         alwan_cvd_type cvd_type, alwan_f64 severity,
                                         alwan_cvd_model model, alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_ex_f64(rgb_out, rgb_in, cvd_type, severity, model);
    if (st != ALWAN_OK) return st;
    return alwan__cvd_gamut_fixup_f64(rgb_out, method);
}

alwan_status alwan_simulate_cvd_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride,
        alwan_f64 const *rgb_in, size_t in_stride, size_t count,
        alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_f64_map_interleave(rgb_out, out_stride, rgb_in, in_stride, count, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f64 space;
    st = alwan_rgb_get_space_descriptor_f64(&space, ALWAN_RGB_SPACE_SRGB, NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f64 *px = (alwan_rgb_f64 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f64 raw = *px;
        st = alwan_gamut_map_advanced_f64(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

alwan_status alwan_simulate_cvd_machado_gamut_safe_f64_map_interleave(alwan_f64 *rgb_out, size_t out_stride,
        alwan_f64 const *rgb_in, size_t in_stride, size_t count,
        alwan_cvd_type cvd_type, alwan_f64 severity, alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_machado_f64_map_interleave(rgb_out, out_stride, rgb_in, in_stride, count, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f64 space;
    st = alwan_rgb_get_space_descriptor_f64(&space, ALWAN_RGB_SPACE_SRGB, NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f64 *px = (alwan_rgb_f64 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f64 raw = *px;
        st = alwan_gamut_map_advanced_f64(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F64 */

#if ALWAN_WITH_F32
static int alwan__cvd_gamut_fixup_f32(alwan_rgb_f32 *rgb, alwan_gamut_map_method method) {
    alwan_rgb_space_desc_f32 space;
    int st = alwan_rgb_get_space_descriptor_f32(&space, ALWAN_RGB_SPACE_SRGB, NULL);
    if (st != ALWAN_OK) return st;
    alwan_rgb_f32 raw = *rgb;
    return alwan_gamut_map_advanced_f32(rgb, method, &space, &raw);
}

alwan_status alwan_simulate_cvd_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                      alwan_cvd_type cvd_type, alwan_f32 severity,
                                      alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_f32(rgb_out, rgb_in, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    return alwan__cvd_gamut_fixup_f32(rgb_out, method);
}

alwan_status alwan_simulate_cvd_machado_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                              alwan_cvd_type cvd_type, alwan_f32 severity,
                                              alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_machado_f32(rgb_out, rgb_in, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    return alwan__cvd_gamut_fixup_f32(rgb_out, method);
}

alwan_status alwan_simulate_cvd_ex_gamut_safe_f32(alwan_rgb_f32 *rgb_out, alwan_rgb_f32 const *rgb_in,
                                         alwan_cvd_type cvd_type, alwan_f32 severity,
                                         alwan_cvd_model model, alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_ex_f32(rgb_out, rgb_in, cvd_type, severity, model);
    if (st != ALWAN_OK) return st;
    return alwan__cvd_gamut_fixup_f32(rgb_out, method);
}

alwan_status alwan_simulate_cvd_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride,
        alwan_f32 const *rgb_in, size_t in_stride, size_t count,
        alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_f32_map_interleave(rgb_out, out_stride, rgb_in, in_stride, count, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f32 space;
    st = alwan_rgb_get_space_descriptor_f32(&space, ALWAN_RGB_SPACE_SRGB, NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f32 *px = (alwan_rgb_f32 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f32 raw = *px;
        st = alwan_gamut_map_advanced_f32(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}

alwan_status alwan_simulate_cvd_machado_gamut_safe_f32_map_interleave(alwan_f32 *rgb_out, size_t out_stride,
        alwan_f32 const *rgb_in, size_t in_stride, size_t count,
        alwan_cvd_type cvd_type, alwan_f32 severity, alwan_gamut_map_method method) {
    int st = alwan_simulate_cvd_machado_f32_map_interleave(rgb_out, out_stride, rgb_in, in_stride, count, cvd_type, severity);
    if (st != ALWAN_OK) return st;
    alwan_rgb_space_desc_f32 space;
    st = alwan_rgb_get_space_descriptor_f32(&space, ALWAN_RGB_SPACE_SRGB, NULL);
    if (st != ALWAN_OK) return st;
    for (size_t i = 0; i < count; i++) {
        alwan_rgb_f32 *px = (alwan_rgb_f32 *)((char *)rgb_out + i * out_stride);
        alwan_rgb_f32 raw = *px;
        st = alwan_gamut_map_advanced_f32(px, method, &space, &raw);
        if (st != ALWAN_OK) return st;
    }
    return ALWAN_OK;
}
#endif /* ALWAN_WITH_F32 */
