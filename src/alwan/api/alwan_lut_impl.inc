/* No include guard -- included twice (f32 and f64 passes) */

/* ----------------------------------------------------------------
 * Local helper macros for TF resolvers and function-pointer type
 * ---------------------------------------------------------------- */
#define LUT__RESOLVE_EOTF  ALWAN_CORE_FNLIT(alwan__resolve_eotf)
#define LUT__RESOLVE_OETF  ALWAN_CORE_FNLIT(alwan__resolve_oetf)
#define LUT__TF_FN         ALWAN_CORE_FNLIT(alwan_tf_fn)

/* ----------------------------------------------------------------
 * Combined matrix builder (src linear RGB -> dst linear RGB)
 * Descriptor stores f64 matrices; we derive in native precision
 * via core _v functions for CAT + matrix multiply.
 * ---------------------------------------------------------------- */

static int ALWAN_CORE_FNLIT(lut__build_combined_matrix)(
    ALWAN_CORE_MAT3X3 *combined,
    alwan_ctx *ctx,
    ALWAN_CORE_RGB_SPACE_DESC const *src_space,
    ALWAN_CORE_RGB_SPACE_DESC const *dst_space)
{
    ALWAN_CORE_MAT3X3 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        for (int i = 0; i < 9; i++) src_to_xyz.m[i] = (ALWAN_CORE_T)src_space->rgb_to_xyz.m[i];
    } else {
        /* Derive matrices: compute in f64 then narrow.
         * alwan_rgb_derive_matrices only exists as f64 because
         * it needs full precision for NPM derivation. */
        alwan_rgb_space_desc_f64 src_desc64;
        for (int i = 0; i < 6; i++) src_desc64.primaries_xy[i] = (double)src_space->primaries_xy[i];
        src_desc64.white_xy[0] = (double)src_space->white_xy[0];
        src_desc64.white_xy[1] = (double)src_space->white_xy[1];
        src_desc64.has_matrices = 0;
        alwan_mat3x3_f64 s2x64, x2s64;
        int status = alwan_rgb_derive_matrices_f64(&s2x64, &x2s64, &src_desc64);
        if (status != ALWAN_OK) return status;
        for (int i = 0; i < 9; i++) src_to_xyz.m[i] = (ALWAN_CORE_T)s2x64.m[i];
        for (int i = 0; i < 9; i++) xyz_to_src.m[i] = (ALWAN_CORE_T)x2s64.m[i];
    }

    ALWAN_CORE_MAT3X3 xyz_to_dst;
    if (dst_space->has_matrices) {
        for (int i = 0; i < 9; i++) xyz_to_dst.m[i] = (ALWAN_CORE_T)dst_space->xyz_to_rgb.m[i];
    } else {
        alwan_rgb_space_desc_f64 dst_desc64;
        for (int i = 0; i < 6; i++) dst_desc64.primaries_xy[i] = (double)dst_space->primaries_xy[i];
        dst_desc64.white_xy[0] = (double)dst_space->white_xy[0];
        dst_desc64.white_xy[1] = (double)dst_space->white_xy[1];
        dst_desc64.has_matrices = 0;
        alwan_mat3x3_f64 d2x64, x2d64;
        int status = alwan_rgb_derive_matrices_f64(&d2x64, &x2d64, &dst_desc64);
        if (status != ALWAN_OK) return status;
        for (int i = 0; i < 9; i++) xyz_to_dst.m[i] = (ALWAN_CORE_T)x2d64.m[i];
    }

    /* Chromatic adaptation if white points differ */
    ALWAN_CORE_T const tol = ALWAN_CORE_LITERAL(1e-6);
    ALWAN_CORE_T dx = (ALWAN_CORE_T)src_space->white_xy[0] - (ALWAN_CORE_T)dst_space->white_xy[0];
    ALWAN_CORE_T dy = (ALWAN_CORE_T)src_space->white_xy[1] - (ALWAN_CORE_T)dst_space->white_xy[1];
    int need_cat = (ALWAN_ABS(dx) > tol || ALWAN_ABS(dy) > tol);

    if (need_cat && ctx) {
        ALWAN_CORE_XYY src_xyy, dst_xyy;
        ALWAN_CORE_XYZ src_wp, dst_wp;
        src_xyy.x = (ALWAN_CORE_T)src_space->white_xy[0];
        src_xyy.y = (ALWAN_CORE_T)src_space->white_xy[1];
        src_xyy.Y = ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_FNLIT(alwan_xyy_to_xyz)(&src_wp, &src_xyy);

        dst_xyy.x = (ALWAN_CORE_T)dst_space->white_xy[0];
        dst_xyy.y = (ALWAN_CORE_T)dst_space->white_xy[1];
        dst_xyy.Y = ALWAN_CORE_LITERAL(1.0);
        ALWAN_CORE_FNLIT(alwan_xyy_to_xyz)(&dst_wp, &dst_xyy);

        ALWAN_CORE_MAT3X3 cat;
        int status = ALWAN_CORE_FNLIT(alwan_cat_matrix)(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        ALWAN_CORE_MAT3X3 tmp = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(cat, src_to_xyz);
        *combined = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(xyz_to_dst, tmp);
    } else {
        *combined = ALWAN_CORE_FNVLIT(alwan_mat3_mul)(xyz_to_dst, src_to_xyz);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * View transform applicator (precision-native)
 * ---------------------------------------------------------------- */

static ALWAN_CORE_VEC3 ALWAN_CORE_FNLIT(lut__apply_view)(
    alwan_view_transform vt, ALWAN_CORE_VEC3 rgb, alwan_ctx *ctx)
{
    ALWAN_CORE_VEC3 in = {{
        alwan_max(rgb.v[0], ALWAN_CORE_LITERAL(0.0)),
        alwan_max(rgb.v[1], ALWAN_CORE_LITERAL(0.0)),
        alwan_max(rgb.v[2], ALWAN_CORE_LITERAL(0.0))
    }};
    ALWAN_CORE_VEC3 r;

    switch (vt) {
    case ALWAN_VIEW_ACES_REC709:
        r.v[0] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_aces_tonemap)(in.v[0]));
        r.v[1] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_aces_tonemap)(in.v[1]));
        r.v[2] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_aces_tonemap)(in.v[2]));
        return r;
    case ALWAN_VIEW_REINHARD_EXT:
        r = ALWAN_CORE_FNVLIT(alwan_reinhard_extended_luma)(in, ALWAN_CORE_LITERAL(4.0));
        r.v[0] = alwan_saturate(r.v[0]);
        r.v[1] = alwan_saturate(r.v[1]);
        r.v[2] = alwan_saturate(r.v[2]);
        return r;
    case ALWAN_VIEW_UCHIMURA:
        r.v[0] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_uchimura_default)(in.v[0]));
        r.v[1] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_uchimura_default)(in.v[1]));
        r.v[2] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_uchimura_default)(in.v[2]));
        return r;
    case ALWAN_VIEW_LOTTES:
        r = ALWAN_CORE_FNVLIT(alwan_lottes_default)(in);
        r.v[0] = alwan_saturate(r.v[0]);
        r.v[1] = alwan_saturate(r.v[1]);
        r.v[2] = alwan_saturate(r.v[2]);
        return r;
    case ALWAN_VIEW_EXPOSURE:
        r = ALWAN_CORE_FNVLIT(alwan_exposure_tonemap_rgb)(in, ALWAN_CORE_LITERAL(0.0));
        r.v[0] = alwan_saturate(r.v[0]);
        r.v[1] = alwan_saturate(r.v[1]);
        r.v[2] = alwan_saturate(r.v[2]);
        return r;
    case ALWAN_VIEW_REINHARD_CALIBRATED:
        r.v[0] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(
                     in.v[0], ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(4.0)));
        r.v[1] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(
                     in.v[1], ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(4.0)));
        r.v[2] = alwan_saturate(ALWAN_CORE_FNVLIT(alwan_reinhard_calibrated)(
                     in.v[2], ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(0.18), ALWAN_CORE_LITERAL(4.0)));
        return r;
    case ALWAN_VIEW_KHRONOS_PBR_NEUTRAL:
        return ALWAN_CORE_FNVLIT(alwan_khronos_pbr_neutral)(in);
    default: {
        /* Everything not special-cased above goes through the real view
         * transform. Returning rgb unchanged here silently baked an IDENTITY
         * LUT for eleven of the eighteen transforms, including the whole AgX
         * family, Tony McMapface and the BT.2446/2390 set: the call returned
         * ALWAN_OK and the caller got a plausible-looking cube that did
         * nothing. That also reached CLF export, which bakes through this
         * same path. */
#if ALWAN_WITH_F64
        ALWAN_CORE_T in3[3] = { in.v[0], in.v[1], in.v[2] };
        ALWAN_CORE_T out3[3];
        if (ALWAN_CORE_FNLIT(alwan_view_transform_apply)(
                out3, 3 * sizeof(ALWAN_CORE_T),
                in3, 3 * sizeof(ALWAN_CORE_T), 1, vt, ctx) != ALWAN_OK)
            return rgb;
#else
        /* An f32-only build. This file's f64 side is still compiled there, as an
         * f64-internal facade, but alwan_view_transform_apply_f64 is not: the
         * view transforms keep their f64 side under plain ALWAN_WITH_F64, and
         * pulling it in drags the ACES 1 output transform and its f64 tables
         * after it. The f32 view transform is the only one this build has, so
         * both instantiations go through it. For the f32 one that is the same
         * call it would have made; for the f64 one it narrows, which is the
         * precision this build chose. Found by linking every declared _f32 entry
         * point against an ALWAN_BUILD_PRECISION=f32 library. */
        alwan_f32 in3[3] = { (alwan_f32)in.v[0], (alwan_f32)in.v[1], (alwan_f32)in.v[2] };
        alwan_f32 out3[3];
        if (alwan_view_transform_apply_f32(out3, 3 * sizeof(alwan_f32),
                                           in3, 3 * sizeof(alwan_f32), 1, vt, ctx) != ALWAN_OK)
            return rgb;
#endif
        r.v[0] = (ALWAN_CORE_T)out3[0];
        r.v[1] = (ALWAN_CORE_T)out3[1];
        r.v[2] = (ALWAN_CORE_T)out3[2];
        return r;
    }
    }
}

/* ----------------------------------------------------------------
 * 3D LUT Baking
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_bake_3dlut)(ALWAN_CORE_T *out, int size,
                     ALWAN_CORE_RGB_SPACE_DESC const *src_space,
                     ALWAN_CORE_RGB_SPACE_DESC const *dst_space,
                     alwan_ctx *ctx) {
    if (!out || !src_space || !dst_space || size < 2 || size > 256) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_MAT3X3 combined;
    int status = ALWAN_CORE_FNLIT(lut__build_combined_matrix)(&combined, ctx, src_space, dst_space);
    if (status != ALWAN_OK) return status;

    LUT__TF_FN eotf_fn = LUT__RESOLVE_EOTF(src_space->eotf);
    LUT__TF_FN oetf_fn = LUT__RESOLVE_OETF(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        ALWAN_CORE_VEC3 coord = ALWAN_CORE_FNVLIT(alwan_lut3d_index_to_rgb)(i, size);
        ALWAN_CORE_VEC3 lin = {{eotf_fn(coord.v[0]), eotf_fn(coord.v[1]), eotf_fn(coord.v[2])}};
        ALWAN_CORE_VEC3 dst_lin = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(combined, lin);
        out[i * 3 + 0] = oetf_fn(dst_lin.v[0]);
        out[i * 3 + 1] = oetf_fn(dst_lin.v[1]);
        out[i * 3 + 2] = oetf_fn(dst_lin.v[2]);
    }

    return ALWAN_OK;
}

alwan_status ALWAN_CORE_FNLIT(alwan_bake_3dlut_view)(ALWAN_CORE_T *out, int size,
                          ALWAN_CORE_RGB_SPACE_DESC const *src_space,
                          ALWAN_CORE_RGB_SPACE_DESC const *dst_space,
                          alwan_view_transform vt,
                          alwan_ctx *ctx) {
    if (!out || !src_space || !dst_space || size < 2 || size > 256) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_MAT3X3 combined;
    int status = ALWAN_CORE_FNLIT(lut__build_combined_matrix)(&combined, ctx, src_space, dst_space);
    if (status != ALWAN_OK) return status;

    LUT__TF_FN eotf_fn = LUT__RESOLVE_EOTF(src_space->eotf);
    LUT__TF_FN oetf_fn = LUT__RESOLVE_OETF(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        ALWAN_CORE_VEC3 coord = ALWAN_CORE_FNVLIT(alwan_lut3d_index_to_rgb)(i, size);
        ALWAN_CORE_VEC3 lin = {{eotf_fn(coord.v[0]), eotf_fn(coord.v[1]), eotf_fn(coord.v[2])}};
        ALWAN_CORE_VEC3 dst_lin = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(combined, lin);
        dst_lin = ALWAN_CORE_FNLIT(lut__apply_view)(vt, dst_lin, ctx);
        out[i * 3 + 0] = oetf_fn(dst_lin.v[0]);
        out[i * 3 + 1] = oetf_fn(dst_lin.v[1]);
        out[i * 3 + 2] = oetf_fn(dst_lin.v[2]);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 1D LUT Baking
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_bake_1dlut)(ALWAN_CORE_T *out, int size,
                     alwan_transfer_function tf,
                     int encode) {
    if (!out || size < 2 || size > 65536) return ALWAN_E_INVALID;

    LUT__TF_FN fn = encode
        ? LUT__RESOLVE_OETF(tf)
        : LUT__RESOLVE_EOTF(tf);
    if (!fn) return ALWAN_E_INVALID;

    for (int i = 0; i < size; i++) {
        ALWAN_CORE_T t = ALWAN_CORE_FNVLIT(alwan_lut1d_index_to_val)((size_t)i, size);
        out[i] = fn(t);
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 2D LUT Dimensions
 * ---------------------------------------------------------------- */

void ALWAN_CORE_FNLIT(alwan_lut2d_dimensions)(int size, int *width, int *height) {
    if (width)  *width  = size * size;
    if (height) *height = size;
}

/* ----------------------------------------------------------------
 * 3D -> 2D flatten
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_lut3d_to_2d)(ALWAN_CORE_T *out,
                       ALWAN_CORE_T const *lut3d,
                       int size) {
    if (!out || !lut3d || size < 2 || size > 256) return ALWAN_E_INVALID;

    int const w = size * size;

    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                size_t idx3d = ((size_t)b * (size_t)size * (size_t)size +
                                (size_t)g * (size_t)size + (size_t)r) * 3;
                int px = b * size + r;
                int py = g;
                size_t idx2d = ((size_t)py * (size_t)w + (size_t)px) * 3;
                out[idx2d + 0] = lut3d[idx3d + 0];
                out[idx2d + 1] = lut3d[idx3d + 1];
                out[idx2d + 2] = lut3d[idx3d + 2];
            }
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 2D -> 3D unflatten
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_lut2d_to_3d)(ALWAN_CORE_T *out,
                       ALWAN_CORE_T const *lut2d,
                       int size) {
    if (!out || !lut2d || size < 2 || size > 256) return ALWAN_E_INVALID;

    int const w = size * size;

    for (int b = 0; b < size; b++) {
        for (int g = 0; g < size; g++) {
            for (int r = 0; r < size; r++) {
                int px = b * size + r;
                int py = g;
                size_t idx2d = ((size_t)py * (size_t)w + (size_t)px) * 3;
                size_t idx3d = ((size_t)b * (size_t)size * (size_t)size +
                                (size_t)g * (size_t)size + (size_t)r) * 3;
                out[idx3d + 0] = lut2d[idx2d + 0];
                out[idx3d + 1] = lut2d[idx2d + 1];
                out[idx3d + 2] = lut2d[idx2d + 2];
            }
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Convenience: bake directly into 2D layout
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_bake_2dlut)(ALWAN_CORE_T *out, int size,
                     ALWAN_CORE_RGB_SPACE_DESC const *src_space,
                     ALWAN_CORE_RGB_SPACE_DESC const *dst_space,
                     alwan_ctx *ctx) {
    if (!out || !src_space || !dst_space || size < 2 || size > 256) {
        return ALWAN_E_INVALID;
    }

    ALWAN_CORE_MAT3X3 combined;
    int status = ALWAN_CORE_FNLIT(lut__build_combined_matrix)(&combined, ctx, src_space, dst_space);
    if (status != ALWAN_OK) return status;

    LUT__TF_FN eotf_fn = LUT__RESOLVE_EOTF(src_space->eotf);
    LUT__TF_FN oetf_fn = LUT__RESOLVE_OETF(dst_space->oetf);
    if (!eotf_fn || !oetf_fn) return ALWAN_E_INVALID;

    int const w = size * size;
    ALWAN_CORE_T const inv = ALWAN_CORE_LITERAL(1.0) / (ALWAN_CORE_T)(size - 1);

    for (int py = 0; py < size; py++) {
        for (int px = 0; px < w; px++) {
            int b = px / size;
            int r = px % size;
            int g = py;

            ALWAN_CORE_T rv = (ALWAN_CORE_T)r * inv;
            ALWAN_CORE_T gv = (ALWAN_CORE_T)g * inv;
            ALWAN_CORE_T bv = (ALWAN_CORE_T)b * inv;

            ALWAN_CORE_VEC3 lin = {{eotf_fn(rv), eotf_fn(gv), eotf_fn(bv)}};
            ALWAN_CORE_VEC3 dst_lin = ALWAN_CORE_FNVLIT(alwan_mat3_mulv)(combined, lin);

            size_t idx = ((size_t)py * (size_t)w + (size_t)px) * 3;
            out[idx + 0] = oetf_fn(dst_lin.v[0]);
            out[idx + 1] = oetf_fn(dst_lin.v[1]);
            out[idx + 2] = oetf_fn(dst_lin.v[2]);
        }
    }

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 1D LUT linear sampling (API wrapper)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_lut1d_sample)(ALWAN_CORE_T *result,
                        ALWAN_CORE_T const *lut,
                        ALWAN_CORE_T t,
                        int size) {
    if (!result || !lut || size < 2) return ALWAN_E_INVALID;
    *result = ALWAN_CORE_FNVLIT(alwan_lut1d_sample)(lut, t, size);
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 2D LUT bilinear sampling (flattened 3D strip, API wrapper)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_lut2d_sample)(ALWAN_CORE_RGB *result,
                        ALWAN_CORE_T const *lut2d,
                        ALWAN_CORE_RGB const *rgb,
                        int size) {
    if (!result || !lut2d || !rgb || size < 2) return ALWAN_E_INVALID;
    ALWAN_CORE_VEC3 in = {{rgb->r, rgb->g, rgb->b}};
    ALWAN_CORE_VEC3 out = ALWAN_CORE_FNVLIT(alwan_lut2d_sample)(lut2d, in, size);
    result->r = out.v[0];
    result->g = out.v[1];
    result->b = out.v[2];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 3D LUT trilinear sampling (API wrapper)
 * ---------------------------------------------------------------- */

alwan_status ALWAN_CORE_FNLIT(alwan_lut3d_sample)(ALWAN_CORE_RGB *result,
                        ALWAN_CORE_T const *lut,
                        ALWAN_CORE_RGB const *rgb,
                        int size) {
    if (!result || !lut || !rgb || size < 2) return ALWAN_E_INVALID;
    ALWAN_CORE_VEC3 in = {{rgb->r, rgb->g, rgb->b}};
    ALWAN_CORE_VEC3 out = ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(lut, in, size);
    result->r = out.v[0];
    result->g = out.v[1];
    result->b = out.v[2];
    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * 3D LUT inversion
 *
 * For each node y of the output cube, find an x with F(x) = y, where F is the
 * forward cube sampled trilinearly. Newton's method on the 3x3 system, with
 * the Jacobian by central differences over half a forward cell, a fixed
 * iteration count so a deterministic build takes one path, and every step
 * clamped back into the unit cube.
 *
 * A cube is not invertible everywhere and this cannot pretend otherwise.
 * Where F flattens, any preimage is as good as another and the iteration
 * settles on one; where y is outside F's image there is no preimage at all
 * and the iteration ends on the nearest boundary it can reach. Neither is an
 * error, so the residual is reported instead: the caller sees how far the
 * worst node missed and decides what that means for the table they asked for.
 * ---------------------------------------------------------------- */

/* Solve m x = b by Cramer's rule. Returns 0 when m is too near singular to
 * use, which is the flat region above rather than a fault. */
static int ALWAN_CORE_FN(lut_solve3)(ALWAN_CORE_T *x, ALWAN_CORE_T const *m, ALWAN_CORE_T const *b) {
    ALWAN_CORE_T const a00 = m[0], a01 = m[1], a02 = m[2];
    ALWAN_CORE_T const a10 = m[3], a11 = m[4], a12 = m[5];
    ALWAN_CORE_T const a20 = m[6], a21 = m[7], a22 = m[8];
    ALWAN_CORE_T const c0 = a11 * a22 - a12 * a21;
    ALWAN_CORE_T const c1 = a12 * a20 - a10 * a22;
    ALWAN_CORE_T const c2 = a10 * a21 - a11 * a20;
    ALWAN_CORE_T const det = a00 * c0 + a01 * c1 + a02 * c2;
    ALWAN_CORE_T inv;
    if (!(det > ALWAN_CORE_LITERAL(1e-12)) && !(det < ALWAN_CORE_LITERAL(-1e-12))) return 0;
    inv = ALWAN_CORE_LITERAL(1.0) / det;
    x[0] = inv * (c0 * b[0] + (a02 * a21 - a01 * a22) * b[1] + (a01 * a12 - a02 * a11) * b[2]);
    x[1] = inv * (c1 * b[0] + (a00 * a22 - a02 * a20) * b[1] + (a02 * a10 - a00 * a12) * b[2]);
    x[2] = inv * (c2 * b[0] + (a01 * a20 - a00 * a21) * b[1] + (a00 * a11 - a01 * a10) * b[2]);
    return 1;
}

static ALWAN_CORE_T ALWAN_CORE_FN(lut_unit_clamp)(ALWAN_CORE_T v) {
    if (!(v > ALWAN_CORE_LITERAL(0.0))) return ALWAN_CORE_LITERAL(0.0);   /* NaN lands here */
    if (v > ALWAN_CORE_LITERAL(1.0)) return ALWAN_CORE_LITERAL(1.0);
    return v;
}

alwan_status ALWAN_CORE_FNLIT(alwan_lut3d_invert)(ALWAN_CORE_T *out, int out_size,
                        ALWAN_CORE_T const *lut, int size,
                        int iterations, ALWAN_CORE_T *out_worst_residual) {
    ALWAN_CORE_T const h = ALWAN_CORE_LITERAL(0.5) / (ALWAN_CORE_T)(size - 1);
    ALWAN_CORE_T worst = ALWAN_CORE_LITERAL(0.0);
    int nr, ng, nb, it, j;

    if (!out || !lut || out_size < 2 || out_size > 256 || size < 2 || size > 256) {
        return ALWAN_E_INVALID;
    }
    if (iterations <= 0) iterations = 20;
    if (iterations > 200) return ALWAN_E_RANGE;

    for (nb = 0; nb < out_size; nb++) {
        for (ng = 0; ng < out_size; ng++) {
            for (nr = 0; nr < out_size; nr++) {
                ALWAN_CORE_T y[3], x[3], e[3], jac[9], dx[3];
                ALWAN_CORE_VEC3 in, f;
                size_t at = ((size_t)nb * (size_t)out_size + (size_t)ng) * (size_t)out_size
                            + (size_t)nr;
                ALWAN_CORE_T res;

                y[0] = (ALWAN_CORE_T)nr / (ALWAN_CORE_T)(out_size - 1);
                y[1] = (ALWAN_CORE_T)ng / (ALWAN_CORE_T)(out_size - 1);
                y[2] = (ALWAN_CORE_T)nb / (ALWAN_CORE_T)(out_size - 1);
                /* The identity is the right first guess: a LUT worth inverting is
                 * usually not far from one, and it costs nothing when it is. */
                x[0] = y[0]; x[1] = y[1]; x[2] = y[2];

                for (it = 0; it < iterations; it++) {
                    in.v[0] = x[0]; in.v[1] = x[1]; in.v[2] = x[2];
                    f = ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(lut, in, size);
                    e[0] = y[0] - f.v[0];
                    e[1] = y[1] - f.v[1];
                    e[2] = y[2] - f.v[2];

                    for (j = 0; j < 3; j++) {
                        ALWAN_CORE_VEC3 pp, mm, fp, fm;
                        ALWAN_CORE_T hp, hm, span;
                        int k;
                        for (k = 0; k < 3; k++) { pp.v[k] = x[k]; mm.v[k] = x[k]; }
                        hp = ALWAN_CORE_FN(lut_unit_clamp)(x[j] + h);
                        hm = ALWAN_CORE_FN(lut_unit_clamp)(x[j] - h);
                        pp.v[j] = hp;
                        mm.v[j] = hm;
                        span = hp - hm;
                        if (!(span > ALWAN_CORE_LITERAL(0.0))) span = ALWAN_CORE_LITERAL(1.0);
                        fp = ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(lut, pp, size);
                        fm = ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(lut, mm, size);
                        /* Column j of the Jacobian: how the output moves with x[j]. */
                        jac[0 * 3 + j] = (fp.v[0] - fm.v[0]) / span;
                        jac[1 * 3 + j] = (fp.v[1] - fm.v[1]) / span;
                        jac[2 * 3 + j] = (fp.v[2] - fm.v[2]) / span;
                    }

                    if (!ALWAN_CORE_FN(lut_solve3)(dx, jac, e)) {
                        /* Flat here, so Newton says nothing: step along the error,
                         * which is the right direction for a LUT near the identity
                         * and harmless where it is not, since the step is clamped. */
                        dx[0] = e[0]; dx[1] = e[1]; dx[2] = e[2];
                    }
                    for (j = 0; j < 3; j++) {
                        if (dx[j] > ALWAN_CORE_LITERAL(0.5)) dx[j] = ALWAN_CORE_LITERAL(0.5);
                        if (dx[j] < ALWAN_CORE_LITERAL(-0.5)) dx[j] = ALWAN_CORE_LITERAL(-0.5);
                        x[j] = ALWAN_CORE_FN(lut_unit_clamp)(x[j] + dx[j]);
                    }
                }

                out[at * 3 + 0] = x[0];
                out[at * 3 + 1] = x[1];
                out[at * 3 + 2] = x[2];

                in.v[0] = x[0]; in.v[1] = x[1]; in.v[2] = x[2];
                f = ALWAN_CORE_FNVLIT(alwan_lut3d_sample)(lut, in, size);
                res = ALWAN_CORE_ABS(f.v[0] - y[0]);
                if (ALWAN_CORE_ABS(f.v[1] - y[1]) > res) res = ALWAN_CORE_ABS(f.v[1] - y[1]);
                if (ALWAN_CORE_ABS(f.v[2] - y[2]) > res) res = ALWAN_CORE_ABS(f.v[2] - y[2]);
                if (res > worst) worst = res;
            }
        }
    }
    if (out_worst_residual) *out_worst_residual = worst;
    return ALWAN_OK;
}

/* Clean up local macros */
#undef LUT__RESOLVE_EOTF
#undef LUT__RESOLVE_OETF
#undef LUT__TF_FN
