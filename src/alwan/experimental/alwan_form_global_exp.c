/* alwan_form_global_exp.c — EXPERIMENTAL picture-formation global solver (translation unit).
 *
 * experimental/ relaxes the library's usual guarantees: the code here may be non-deterministic and
 * may inline magic numbers (no gendata). It is not part of the constraint-tested surface. It builds
 * on the shipped operators through the PUBLIC spatial API, so it needs no access to the internal
 * static core. The templated body is in alwan_form_global_exp_impl.inc, compiled once per precision.
 */
#include "../alwan.h"
#include "../alwan_internal.h"   /* struct alwan_ctx { alloc_fn; free_fn; ... } */
#include <string.h>
#include <stddef.h>

#if ALWAN_WITH_F32
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
#include "../api/alwan_api_f32_setup.h"
#include "alwan_form_global_exp_impl.inc"
#include "../api/alwan_api_teardown.h"
ALWAN_DIAG_POP
#endif

#if ALWAN_WITH_F64
#include "../api/alwan_api_f64_setup.h"
#include "alwan_form_global_exp_impl.inc"
#include "../api/alwan_api_teardown.h"
#endif
