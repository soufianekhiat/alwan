/* alwan_rgb_fit.c: EXPERIMENTAL fit of an RGB encoding space to a dataset (translation unit).
 *
 * experimental/ relaxes the library's usual guarantees: inlined constants, no gendata, not part
 * of the constraint-tested surface. Built entirely on the public API (matrix derivation, the
 * perceptual spaces and their colour differences); the templated body is in
 * alwan_rgb_fit_impl.inc, compiled once per precision.
 */
#include "../alwan.h"
#include "../alwan_internal.h"   /* struct alwan_ctx { alloc_fn; free_fn; ... } */
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "../api/alwan_api_f32_setup.h"
#include "alwan_rgb_fit_impl.inc"
#include "../api/alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "../api/alwan_api_f64_setup.h"
#include "alwan_rgb_fit_impl.inc"
#include "../api/alwan_api_teardown.h"
#endif
