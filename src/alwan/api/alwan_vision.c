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
 * Generated from colour-science SDS_LEFS['CIE 1924 Photopic Standard Observer'] */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const PHOTOPIC_V_DATA[] = {
#include "../data/vision/photopic_v_lambda.csv"
};
ALWAN_DIAG_POP
#define PHOTOPIC_V_COUNT 42

/* CIE 1951 Scotopic V'(lambda) - interleaved {wavelength, value} pairs
 * 42 samples, 380-780 nm (10 nm step + peak at 507 nm)
 * Generated from colour-science SDS_LEFS['CIE 1951 Scotopic Standard Observer'] */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_f64 const SCOTOPIC_VP_DATA[] = {
#include "../data/vision/scotopic_vp_lambda.csv"
};
ALWAN_DIAG_POP
#define SCOTOPIC_VP_COUNT 42

/* Interpolate interleaved {wavelength, value} pairs (stride 2) */
static alwan_f64 interpolate_lut(alwan_f64 const *data,
                                      int count,
                                      alwan_f64 wavelength) {
    if (wavelength <= data[0]) {
        return data[1];
    }
    if (wavelength >= data[(count - 1) * 2]) {
        return data[(count - 1) * 2 + 1];
    }

    int i;
    for (i = 0; i < count - 1; i++) {
        alwan_f64 wl_lo = data[i * 2];
        alwan_f64 wl_hi = data[(i + 1) * 2];
        if (wavelength >= wl_lo && wavelength <= wl_hi) {
            alwan_f64 t = (wavelength - wl_lo) / (wl_hi - wl_lo);
            return data[i * 2 + 1] + t * (data[(i + 1) * 2 + 1] - data[i * 2 + 1]);
        }
    }

    return ALWAN_LITERAL_F64(0.0);
}

/* Internal helpers used by both f32 and f64 templated code via the .inc */
ALWAN_INLINE alwan_f64 alwan_vision_interpolate_photopic_lut(alwan_f64 wavelength) {
    return interpolate_lut(PHOTOPIC_V_DATA, PHOTOPIC_V_COUNT, wavelength);
}
ALWAN_INLINE alwan_f64 alwan_vision_interpolate_scotopic_lut(alwan_f64 wavelength) {
    return interpolate_lut(SCOTOPIC_VP_DATA, SCOTOPIC_VP_COUNT, wavelength);
}

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_vision_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP

#include "alwan_api_f64_setup.h"
#include "alwan_vision_impl.inc"
#include "alwan_api_teardown.h"

/* SPD-based luminance functions (alwan_photopic_luminance_*, etc.)
 * are templatized in alwan_vision_impl.inc. */
/* alwan_apca_contrast_f32 / alwan_apca_contrast_f64 generated via alwan_vision_impl.inc */
