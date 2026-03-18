/* No include guard — included twice (f32 and f64 passes) */

void ALWAN_CORE_FNLIT(alwan_mat3_identity)(ALWAN_CORE_MAT3X3 *out) {
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_identity)();
}

void ALWAN_CORE_FNLIT(alwan_mat3_mul)(ALWAN_CORE_MAT3X3 *out, ALWAN_CORE_MAT3X3 const *a, ALWAN_CORE_MAT3X3 const *b) {
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(*a, *b);
}

void ALWAN_CORE_FNLIT(alwan_mat3_mulv)(ALWAN_CORE_VEC3 *out, ALWAN_CORE_MAT3X3 const *m, ALWAN_CORE_VEC3 const *v) {
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(*m, *v);
}

ALWAN_CORE_T ALWAN_CORE_FNLIT(alwan_mat3_det)(ALWAN_CORE_MAT3X3 const *m) {
    return ALWAN_CORE_FNVLIT(alwan_mat3_det)(*m);
}

int ALWAN_CORE_FNLIT(alwan_mat3_inv)(ALWAN_CORE_MAT3X3 *out, ALWAN_CORE_MAT3X3 const *m) {
    ALWAN_CORE_T det = ALWAN_CORE_FNVLIT(alwan_mat3_det)(*m);
    if (ALWAN_CORE_ABS(det) < ALWAN_CORE_EPSILON) {
        return ALWAN_E_RANGE;
    }
    *out = ALWAN_CORE_FNVLIT(alwan_mat3_inv)(*m);
    return ALWAN_OK;
}
