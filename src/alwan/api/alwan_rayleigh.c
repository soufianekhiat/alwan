/* ================================================================
 * Alwan - Rayleigh Scattering
 * Per-pixel math in alwan_rayleigh_core.h
 *
 * Only NULL-param defaults and the SPD loop live here.
 * ================================================================ */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_rayleigh_core.h"
#include <math.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "alwan_api_f32_setup.h"
#include "alwan_rayleigh_impl.inc"
#include "alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "alwan_api_f64_setup.h"
#include "alwan_rayleigh_impl.inc"
#include "alwan_api_teardown.h"
#endif
