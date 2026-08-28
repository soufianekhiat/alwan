/* alwan_gamut_spatial.c: EXPERIMENTAL picture-formation spatial methods.
 *
 * RELOCATED here from api/alwan_gamut.c. Everything under experimental/ relaxes the library's usual
 * guarantees: the picture-formation methods (the alwan_gamut_formation_method family, dispatched by
 * alwan_gamut_map_spatial_f32/f64) may be NON-DETERMINISTIC and may use inlined magic constants
 * (no gendata requirement), and are NOT part of the constraint-tested guarantee surface. They remain
 * research code, subject to change.
 *
 * The PUBLIC declarations (the enum, alwan_gamut_spatial_params_f32/f64, and the two entry points)
 * stay in alwan.h, so callers, harnesses, and the formation-constraint tests are unchanged, only the
 * implementation moved. The templated body is alwan_gamut_spatial_impl.inc (self-contained: it defines
 * its own alwan__boxfilter and touches no gamut-core / map helpers), compiled once per precision via
 * the standard core setup/teardown pattern.
 */
#include "../alwan.h"
#include "../alwan_internal.h"   /* struct alwan_ctx { alloc_fn; free_fn; ... } */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "../api/alwan_api_f32_setup.h"
#include "alwan_gamut_spatial_impl.inc"
#include "../api/alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "../api/alwan_api_f64_setup.h"
#include "alwan_gamut_spatial_impl.inc"
#include "../api/alwan_api_teardown.h"
#endif
