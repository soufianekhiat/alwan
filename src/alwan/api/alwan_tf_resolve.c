/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Transfer-function resolver definitions.
 *
 * The four scalar resolvers (OETF/EOTF x f32/f64) used to live as static
 * inline in alwan_internal.h, which forced ~160 lines of switch and
 * address-taking of every TF function into all ~60 TUs that include the
 * header. They are now defined here exactly once, table-driven by
 * TF_TABLE_BODY in alwan_internal.h. Adding a new TF means adding one
 * row to that table -- every resolver and SIMD dispatch site updates
 * automatically.
 */

#include "../alwan.h"
#include "../alwan_internal.h"

#define ALWAN__EMIT_OETF_F32(suffix, oetf_base, eotf_base) \
    case ALWAN_TF_##suffix: return alwan_##oetf_base##_oetf_f32;
#define ALWAN__EMIT_EOTF_F32(suffix, oetf_base, eotf_base) \
    case ALWAN_TF_##suffix: return alwan_##eotf_base##_eotf_f32;
#define ALWAN__EMIT_OETF_F64(suffix, oetf_base, eotf_base) \
    case ALWAN_TF_##suffix: return alwan_##oetf_base##_oetf_f64;
#define ALWAN__EMIT_EOTF_F64(suffix, oetf_base, eotf_base) \
    case ALWAN_TF_##suffix: return alwan_##eotf_base##_eotf_f64;

alwan_tf_fn_f32 alwan__resolve_oetf_f32(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR: return alwan_linear_identity_f32;
    case ALWAN_TF_ST2084: return alwan_pq_oetf_f32;
    TF_TABLE_BODY(ALWAN__EMIT_OETF_F32)
    }
    return NULL;
}

alwan_tf_fn_f32 alwan__resolve_eotf_f32(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR: return alwan_linear_identity_f32;
    case ALWAN_TF_ST2084: return alwan_pq_eotf_f32;
    TF_TABLE_BODY(ALWAN__EMIT_EOTF_F32)
    }
    return NULL;
}

alwan_tf_fn_f64 alwan__resolve_oetf_f64(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR: return alwan_linear_identity_f64;
    case ALWAN_TF_ST2084: return alwan_pq_oetf_f64;
    TF_TABLE_BODY(ALWAN__EMIT_OETF_F64)
    }
    return NULL;
}

alwan_tf_fn_f64 alwan__resolve_eotf_f64(alwan_transfer_function tf) {
    switch (tf) {
    case ALWAN_TF_LINEAR: return alwan_linear_identity_f64;
    case ALWAN_TF_ST2084: return alwan_pq_eotf_f64;
    TF_TABLE_BODY(ALWAN__EMIT_EOTF_F64)
    }
    return NULL;
}

#undef ALWAN__EMIT_OETF_F32
#undef ALWAN__EMIT_EOTF_F32
#undef ALWAN__EMIT_OETF_F64
#undef ALWAN__EMIT_EOTF_F64
