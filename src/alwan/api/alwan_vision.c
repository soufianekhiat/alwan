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
