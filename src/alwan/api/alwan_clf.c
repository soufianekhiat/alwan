/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * CLF (Common LUT Format) export — SMPTE ST 2136-1:2024
 * Serializes Alwan transform chains as CLF XML for interchange
 * with OCIO, ACES, DaVinci Resolve, Baselight.
 *
 * Reference: Academy/ASC Common LUT Format (S-2014-006)
 */

#include "../alwan.h"
#include "../alwan_internal.h"
#include "../core/alwan_lut_core.h"
#include "../core/alwan_rgb_core.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

/* ----------------------------------------------------------------
 * Determinism / portability hardening:
 *   - All file output uses "wb" (binary mode) so MSVC text-mode
 *     CRLF translation does not perturb the on-disk content.
 *   - All numeric formatting (%.17g) is wrapped under LC_NUMERIC="C"
 *     so a host locale that uses "," as the decimal separator does
 *     not produce un-parseable XML floats.
 *   - All numeric data uses %.17g for lossless f64 round-trip; CLF
 *     consumers that prefer fixed-point formatting can re-format.
 * ---------------------------------------------------------------- */

static char *alwan_clf_save_lc_numeric(void) {
    char const *cur = setlocale(LC_NUMERIC, NULL);
    if (!cur) return NULL;
    size_t n = strlen(cur);
    char *saved = (char *)ALWAN_ALLOC(n + 1, 1);
    if (!saved) return NULL;
    memcpy(saved, cur, n + 1);
    setlocale(LC_NUMERIC, "C");
    return saved;
}

static void alwan_clf_restore_lc_numeric(char *saved) {
    if (saved) {
        setlocale(LC_NUMERIC, saved);
        ALWAN_FREE(saved);
    }
}

/* ----------------------------------------------------------------
 * Internal: buffered writer that works for both FILE* and char*
 * ---------------------------------------------------------------- */

typedef struct {
    FILE *fp;           /* non-NULL for file output */
    char *buf;          /* non-NULL for buffer output */
    size_t buf_size;
    size_t pos;
    int overflow;       /* set if buffer overflows */
} clf_writer;

static void clf_write(clf_writer *w, char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    if (w->fp) {
        vfprintf(w->fp, fmt, args);
    } else if (w->buf && !w->overflow) {
        size_t remaining = w->buf_size - w->pos;
        int n = vsnprintf(w->buf + w->pos, remaining, fmt, args);
        if (n < 0 || (size_t)n >= remaining) {
            w->overflow = 1;
        } else {
            w->pos += (size_t)n;
        }
    }

    va_end(args);
}

/* ----------------------------------------------------------------
 * Internal: write XML header and ProcessList open tag
 * ---------------------------------------------------------------- */

static void clf_write_header(clf_writer *w, char const *id, char const *name,
                               char const *input_desc, char const *output_desc) {
    clf_write(w, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    clf_write(w, "<ProcessList id=\"%s\" compCLFversion=\"3.0\"",
              id ? id : "alwan-transform");
    if (name && name[0]) {
        clf_write(w, " name=\"%s\"", name);
    }
    clf_write(w, ">\n");

    if (input_desc && input_desc[0]) {
        clf_write(w, "  <InputDescriptor>%s</InputDescriptor>\n", input_desc);
    }
    if (output_desc && output_desc[0]) {
        clf_write(w, "  <OutputDescriptor>%s</OutputDescriptor>\n", output_desc);
    }
}

/* ----------------------------------------------------------------
 * Internal: write a Matrix ProcessNode (3x3 or 3x4)
 * ---------------------------------------------------------------- */

static void clf_write_matrix(clf_writer *w, alwan_mat3x3_f64 const *mat,
                               alwan_f64 const *offset,
                               char const *desc) {
    if (offset) {
        clf_write(w, "  <Matrix inBitDepth=\"32f\" outBitDepth=\"32f\">\n");
        if (desc) clf_write(w, "    <Description>%s</Description>\n", desc);
        clf_write(w, "    <Array dim=\"3 4\">\n");
        clf_write(w, "      %.15e %.15e %.15e %.15e\n",
                  (double)mat->m[0], (double)mat->m[1], (double)mat->m[2], (double)offset[0]);
        clf_write(w, "      %.15e %.15e %.15e %.15e\n",
                  (double)mat->m[3], (double)mat->m[4], (double)mat->m[5], (double)offset[1]);
        clf_write(w, "      %.15e %.15e %.15e %.15e\n",
                  (double)mat->m[6], (double)mat->m[7], (double)mat->m[8], (double)offset[2]);
        clf_write(w, "    </Array>\n");
    } else {
        clf_write(w, "  <Matrix inBitDepth=\"32f\" outBitDepth=\"32f\">\n");
        if (desc) clf_write(w, "    <Description>%s</Description>\n", desc);
        clf_write(w, "    <Array dim=\"3 3\">\n");
        clf_write(w, "      %.15e %.15e %.15e\n",
                  (double)mat->m[0], (double)mat->m[1], (double)mat->m[2]);
        clf_write(w, "      %.15e %.15e %.15e\n",
                  (double)mat->m[3], (double)mat->m[4], (double)mat->m[5]);
        clf_write(w, "      %.15e %.15e %.15e\n",
                  (double)mat->m[6], (double)mat->m[7], (double)mat->m[8]);
        clf_write(w, "    </Array>\n");
    }
    clf_write(w, "  </Matrix>\n");
}

/* ----------------------------------------------------------------
 * Internal: write a 1D LUT ProcessNode
 * ---------------------------------------------------------------- */

static void clf_write_lut1d(clf_writer *w, alwan_f64 const *lut,
                              int size, char const *desc) {
    clf_write(w, "  <LUT1D inBitDepth=\"32f\" outBitDepth=\"32f\" interpolation=\"linear\">\n");
    if (desc) clf_write(w, "    <Description>%s</Description>\n", desc);
    clf_write(w, "    <Array dim=\"%d 1\">\n", size);

    for (int i = 0; i < size; i++) {
        clf_write(w, "      %.17g\n", (double)lut[i]);
    }

    clf_write(w, "    </Array>\n");
    clf_write(w, "  </LUT1D>\n");
}

/* ----------------------------------------------------------------
 * Internal: write a 3D LUT ProcessNode
 * ---------------------------------------------------------------- */

static void clf_write_lut3d(clf_writer *w, alwan_f64 const *lut,
                              int size, char const *desc) {
    clf_write(w, "  <LUT3D inBitDepth=\"32f\" outBitDepth=\"32f\" interpolation=\"trilinear\">\n");
    if (desc) clf_write(w, "    <Description>%s</Description>\n", desc);
    clf_write(w, "    <Array dim=\"%d %d %d 3\">\n", size, size, size);

    /* CLF data order: R varies fastest, then G, then B (same as Alwan's internal order) */
    size_t const total = (size_t)size * (size_t)size * (size_t)size;
    for (size_t i = 0; i < total; i++) {
        clf_write(w, "      %.17g %.17g %.17g\n",
                  (double)lut[i * 3 + 0],
                  (double)lut[i * 3 + 1],
                  (double)lut[i * 3 + 2]);
    }

    clf_write(w, "    </Array>\n");
    clf_write(w, "  </LUT3D>\n");
}

/* ----------------------------------------------------------------
 * Internal: write a Range ProcessNode
 * ---------------------------------------------------------------- */

static void clf_write_range(clf_writer *w,
                              alwan_f64 min_in, alwan_f64 max_in,
                              alwan_f64 min_out, alwan_f64 max_out,
                              int clamp, char const *desc) {
    clf_write(w, "  <Range inBitDepth=\"32f\" outBitDepth=\"32f\"");
    if (!clamp) clf_write(w, " style=\"noClamp\"");
    clf_write(w, ">\n");
    if (desc) clf_write(w, "    <Description>%s</Description>\n", desc);
    clf_write(w, "    <minInValue>%.17g</minInValue>\n", (double)min_in);
    clf_write(w, "    <maxInValue>%.17g</maxInValue>\n", (double)max_in);
    clf_write(w, "    <minOutValue>%.17g</minOutValue>\n", (double)min_out);
    clf_write(w, "    <maxOutValue>%.17g</maxOutValue>\n", (double)max_out);
    clf_write(w, "  </Range>\n");
}

/* ----------------------------------------------------------------
 * Internal: write an Exponent ProcessNode
 * ---------------------------------------------------------------- */

static void clf_write_exponent(clf_writer *w, alwan_f64 exponent,
                                 char const *style, char const *desc) {
    clf_write(w, "  <Exponent inBitDepth=\"32f\" outBitDepth=\"32f\" style=\"%s\">\n",
              style);
    if (desc) clf_write(w, "    <Description>%s</Description>\n", desc);
    clf_write(w, "    <ExponentParams exponent=\"%.17g\" />\n", (double)exponent);
    clf_write(w, "  </Exponent>\n");
}

/* ----------------------------------------------------------------
 * Internal: write monCurve Exponent (sRGB-style with linear segment)
 * ---------------------------------------------------------------- */

static void clf_write_exponent_moncurve(clf_writer *w, alwan_f64 exponent,
                                          alwan_f64 offset,
                                          char const *style, char const *desc) {
    clf_write(w, "  <Exponent inBitDepth=\"32f\" outBitDepth=\"32f\" style=\"%s\">\n",
              style);
    if (desc) clf_write(w, "    <Description>%s</Description>\n", desc);
    clf_write(w, "    <ExponentParams exponent=\"%.17g\" offset=\"%.17g\" />\n",
              (double)exponent, (double)offset);
    clf_write(w, "  </Exponent>\n");
}

/* ----------------------------------------------------------------
 * Internal: write transfer function as appropriate CLF node
 *
 * Returns 1 if the TF was written as an Exponent node,
 * 0 if it needs a LUT1D instead, -1 on error.
 * ---------------------------------------------------------------- */

static int clf_write_tf_node(clf_writer *w, alwan_transfer_function tf,
                               int is_eotf, int lut_size) {
    /* sRGB: monCurve with exponent=2.4, offset=0.055 */
    if (tf == ALWAN_TF_SRGB) {
        clf_write_exponent_moncurve(w, 2.4, 0.055,
            is_eotf ? "monCurveFwd" : "monCurveRev",
            is_eotf ? "sRGB EOTF" : "sRGB OETF");
        return 1;
    }

    /* BT.1886: pure gamma 2.4 */
    if (tf == ALWAN_TF_BT1886) {
        clf_write_exponent(w, 2.4,
            is_eotf ? "basicFwd" : "basicRev",
            is_eotf ? "BT.1886 EOTF (gamma 2.4)" : "BT.1886 inverse");
        return 1;
    }

    /* BT.709/BT.2020: monCurve with exponent=1/0.45=2.222..., offset=0.099 */
    if (tf == ALWAN_TF_BT709 || tf == ALWAN_TF_BT2020) {
        clf_write_exponent_moncurve(w, 1.0 / 0.45, 0.099,
            is_eotf ? "monCurveFwd" : "monCurveRev",
            is_eotf ? "BT.709/BT.2020 EOTF" : "BT.709/BT.2020 OETF");
        return 1;
    }

    /* Simple gamma curves */
    if (tf == ALWAN_TF_GAMMA22) {
        clf_write_exponent(w, 2.2,
            is_eotf ? "basicFwd" : "basicRev",
            is_eotf ? "Gamma 2.2 decode" : "Gamma 2.2 encode");
        return 1;
    }
    if (tf == ALWAN_TF_GAMMA24) {
        clf_write_exponent(w, 2.4,
            is_eotf ? "basicFwd" : "basicRev",
            is_eotf ? "Gamma 2.4 decode" : "Gamma 2.4 encode");
        return 1;
    }
    if (tf == ALWAN_TF_GAMMA26) {
        clf_write_exponent(w, 2.6,
            is_eotf ? "basicFwd" : "basicRev",
            is_eotf ? "Gamma 2.6 decode" : "Gamma 2.6 encode");
        return 1;
    }
    if (tf == ALWAN_TF_GAMMA28) {
        clf_write_exponent(w, 2.8,
            is_eotf ? "basicFwd" : "basicRev",
            is_eotf ? "Gamma 2.8 decode" : "Gamma 2.8 encode");
        return 1;
    }

    /* Linear: no node needed */
    if (tf == ALWAN_TF_LINEAR) {
        return 1; /* nothing to write, but not an error */
    }

    /* For all other TFs (PQ, HLG, log curves, etc.): bake a 1D LUT */
    if (lut_size > 0) {
        alwan_f64 *lut = (alwan_f64 *)ALWAN_ALLOC((size_t)lut_size * sizeof(alwan_f64), sizeof(alwan_f64));
        if (!lut) return -1;

        int status = alwan_bake_1dlut_f64(lut, lut_size, tf, is_eotf ? 0 : 1);
        if (status != ALWAN_OK) {
            ALWAN_FREE(lut);
            return -1;
        }

        /* Build description string on stack */
        char desc[64];
        snprintf(desc, sizeof(desc), "%s %s (baked %d entries)",
                 is_eotf ? "EOTF" : "OETF",
                 "transfer function", lut_size);

        clf_write_lut1d(w, lut, lut_size, desc);
        ALWAN_FREE(lut);
        return 0;
    }

    return -1;
}

/* ----------------------------------------------------------------
 * Internal: core CLF export logic
 * ---------------------------------------------------------------- */

static int clf_export_core(clf_writer *w,
                            alwan_rgb_space_desc_f64 const *src_space,
                            alwan_rgb_space_desc_f64 const *dst_space,
                            int use_view, alwan_view_transform view,
                            alwan_ctx *ctx,
                            char const *id, char const *name,
                            int lut_size) {
    /* Get interop names if available */
    char const *src_name = "source";
    char const *dst_name = "destination";

    /* Write XML header */
    clf_write_header(w, id, name, src_name, dst_name);

    /* Step 1: Source EOTF (encoded -> linear) */
    if (src_space->eotf != ALWAN_TF_LINEAR) {
        int result = clf_write_tf_node(w, src_space->eotf, 1, lut_size);
        if (result < 0) return ALWAN_E_INVALID;
    }

    /* Step 2: Source RGB -> XYZ matrix */
    alwan_mat3x3_f64 src_to_xyz, xyz_to_src;
    if (src_space->has_matrices) {
        src_to_xyz = src_space->rgb_to_xyz;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&src_to_xyz, &xyz_to_src, src_space);
        if (status != ALWAN_OK) return status;
    }
    clf_write_matrix(w, &src_to_xyz, NULL, "Source RGB to XYZ (NPM)");

    /* Step 3: Chromatic adaptation (if white points differ) */
    alwan_f64 const tol = 1e-6;
    alwan_f64 dx = src_space->white_xy[0] - dst_space->white_xy[0];
    alwan_f64 dy = src_space->white_xy[1] - dst_space->white_xy[1];
    int need_cat = (dx > tol || dx < -tol || dy > tol || dy < -tol);

    if (need_cat && ctx) {
        alwan_xyy_f64 src_xyy, dst_xyy;
        alwan_xyz_f64 src_wp, dst_wp;
        src_xyy.x = src_space->white_xy[0];
        src_xyy.y = src_space->white_xy[1];
        src_xyy.Y = 1.0;
        alwan_xyy_to_xyz_f64(&src_wp, &src_xyy);

        dst_xyy.x = dst_space->white_xy[0];
        dst_xyy.y = dst_space->white_xy[1];
        dst_xyy.Y = 1.0;
        alwan_xyy_to_xyz_f64(&dst_wp, &dst_xyy);

        alwan_mat3x3_f64 cat;
        int status = alwan_cat_matrix_f64(&cat, &src_wp, &dst_wp, ALWAN_CAT_BRADFORD);
        if (status != ALWAN_OK) return status;

        clf_write_matrix(w, &cat, NULL, "Chromatic adaptation (Bradford)");
    }

    /* Step 4: XYZ -> Destination RGB matrix */
    alwan_mat3x3_f64 dst_to_xyz, xyz_to_dst;
    if (dst_space->has_matrices) {
        xyz_to_dst = dst_space->xyz_to_rgb;
    } else {
        int status = alwan_rgb_derive_matrices_f64(&dst_to_xyz, &xyz_to_dst, dst_space);
        if (status != ALWAN_OK) return status;
    }
    clf_write_matrix(w, &xyz_to_dst, NULL, "XYZ to destination RGB (inverse NPM)");

    /* Step 5: Gamut clamp (Range node) — clip to [0,1] in linear dst space */
    clf_write_range(w, 0.0, 1.0, 0.0, 1.0, 1, "Gamut clamp to [0,1]");

    /* Step 6: View transform as 3D LUT (if requested) */
    if (use_view && lut_size >= 2) {
        /* Bake the view transform into a 3D LUT operating in linear space */
        int const vt_lut_size = lut_size < 17 ? 17 : (lut_size > 65 ? 65 : lut_size);
        size_t const total = (size_t)vt_lut_size * (size_t)vt_lut_size * (size_t)vt_lut_size;
        alwan_f64 *lut3d = (alwan_f64 *)ALWAN_ALLOC(total * 3 * sizeof(alwan_f64), sizeof(alwan_f64));
        if (!lut3d) return ALWAN_E_NOMEM;

        /* Build an identity-space LUT with only the view transform applied */
        alwan_rgb_space_desc_f64 linear_desc;
        memset(&linear_desc, 0, sizeof(linear_desc));
        linear_desc.oetf = ALWAN_TF_LINEAR;
        linear_desc.eotf = ALWAN_TF_LINEAR;
        /* Copy dst primaries/white for the view transform LUT */
        memcpy(linear_desc.primaries_xy, dst_space->primaries_xy, sizeof(linear_desc.primaries_xy));
        memcpy(linear_desc.white_xy, dst_space->white_xy, sizeof(linear_desc.white_xy));
        linear_desc.has_matrices = dst_space->has_matrices;
        if (dst_space->has_matrices) {
            linear_desc.rgb_to_xyz = dst_space->rgb_to_xyz;
            linear_desc.xyz_to_rgb = dst_space->xyz_to_rgb;
        }

        int status = alwan_bake_3dlut_view_f64(lut3d, vt_lut_size, &linear_desc, &linear_desc, view, ctx);
        if (status != ALWAN_OK) {
            ALWAN_FREE(lut3d);
            return status;
        }

        clf_write_lut3d(w, lut3d, vt_lut_size, "View transform (tone mapping)");
        ALWAN_FREE(lut3d);
    }

    /* Step 7: Destination OETF (linear -> encoded) */
    if (dst_space->oetf != ALWAN_TF_LINEAR) {
        int result = clf_write_tf_node(w, dst_space->oetf, 0, lut_size);
        if (result < 0) return ALWAN_E_INVALID;
    }

    /* Close ProcessList */
    clf_write(w, "</ProcessList>\n");

    return ALWAN_OK;
}

/* ----------------------------------------------------------------
 * Public API: Write CLF to file
 * ---------------------------------------------------------------- */

int alwan_clf_export_f64(char const *path, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!path || !src_space || !dst_space) return ALWAN_E_INVALID;
    if (lut_size < 2) lut_size = 4096; /* default LUT size for 1D */

    FILE *f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;

    char *saved_locale = alwan_clf_save_lc_numeric();

    clf_writer w;
    memset(&w, 0, sizeof(w));
    w.fp = f;

    int status = clf_export_core(&w, src_space, dst_space, 0, (alwan_view_transform)0,
                                  ctx, id, name, lut_size);

    alwan_clf_restore_lc_numeric(saved_locale);
    fclose(f);
    return status;
}

int alwan_clf_export_view_f64(char const *path, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!path || !src_space || !dst_space) return ALWAN_E_INVALID;
    if (lut_size < 2) lut_size = 4096;

    FILE *f = fopen(path, "wb");
    if (!f) return ALWAN_E_INVALID;

    char *saved_locale = alwan_clf_save_lc_numeric();

    clf_writer w;
    memset(&w, 0, sizeof(w));
    w.fp = f;

    int status = clf_export_core(&w, src_space, dst_space, 1, view,
                                  ctx, id, name, lut_size);

    alwan_clf_restore_lc_numeric(saved_locale);
    fclose(f);
    return status;
}

/* ----------------------------------------------------------------
 * Public API: Write CLF to buffer
 * ---------------------------------------------------------------- */

int alwan_clf_export_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!buf || !src_space || !dst_space || !bytes_written || buf_size == 0) {
        return ALWAN_E_INVALID;
    }
    if (lut_size < 2) lut_size = 4096;

    char *saved_locale = alwan_clf_save_lc_numeric();

    clf_writer w;
    memset(&w, 0, sizeof(w));
    w.buf = buf;
    w.buf_size = buf_size;

    int status = clf_export_core(&w, src_space, dst_space, 0, (alwan_view_transform)0,
                                  ctx, id, name, lut_size);

    alwan_clf_restore_lc_numeric(saved_locale);
    if (w.overflow) return ALWAN_E_RANGE;
    *bytes_written = w.pos;
    return status;
}

int alwan_clf_export_view_buffer_f64(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f64 const *src_space, alwan_rgb_space_desc_f64 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!buf || !src_space || !dst_space || !bytes_written || buf_size == 0) {
        return ALWAN_E_INVALID;
    }
    if (lut_size < 2) lut_size = 4096;

    char *saved_locale = alwan_clf_save_lc_numeric();

    clf_writer w;
    memset(&w, 0, sizeof(w));
    w.buf = buf;
    w.buf_size = buf_size;

    int status = clf_export_core(&w, src_space, dst_space, 1, view,
                                  ctx, id, name, lut_size);

    alwan_clf_restore_lc_numeric(saved_locale);
    if (w.overflow) return ALWAN_E_RANGE;
    *bytes_written = w.pos;
    return status;
}

/* ----------------------------------------------------------------
 * f32 wrappers — widen f32 descriptors to f64 and delegate.
 * ---------------------------------------------------------------- */

static void clf_widen_desc_32(alwan_rgb_space_desc_f64 *out, alwan_rgb_space_desc_f32 const *in) {
    for (int j = 0; j < 6; j++) out->primaries_xy[j] = (double)in->primaries_xy[j];
    out->white_xy[0] = (double)in->white_xy[0];
    out->white_xy[1] = (double)in->white_xy[1];
    out->oetf = in->oetf;
    out->eotf = in->eotf;
    for (int j = 0; j < 9; j++) {
        out->rgb_to_xyz.m[j] = (double)in->rgb_to_xyz.m[j];
        out->xyz_to_rgb.m[j] = (double)in->xyz_to_rgb.m[j];
    }
    out->has_matrices = in->has_matrices;
}

int alwan_clf_export_f32(char const *path, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!src_space || !dst_space) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 s, d;
    clf_widen_desc_32(&s, src_space);
    clf_widen_desc_32(&d, dst_space);
    return alwan_clf_export_f64(path, &s, &d, id, name, lut_size, ctx);
}

int alwan_clf_export_view_f32(char const *path, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!src_space || !dst_space) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 s, d;
    clf_widen_desc_32(&s, src_space);
    clf_widen_desc_32(&d, dst_space);
    return alwan_clf_export_view_f64(path, &s, &d, view, id, name, lut_size, ctx);
}

int alwan_clf_export_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!src_space || !dst_space) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 s, d;
    clf_widen_desc_32(&s, src_space);
    clf_widen_desc_32(&d, dst_space);
    return alwan_clf_export_buffer_f64(buf, bytes_written, buf_size, &s, &d, id, name, lut_size, ctx);
}

int alwan_clf_export_view_buffer_f32(char *buf, size_t *bytes_written, size_t buf_size, alwan_rgb_space_desc_f32 const *src_space, alwan_rgb_space_desc_f32 const *dst_space, alwan_view_transform view, char const *id, char const *name, int lut_size, alwan_ctx *ctx) {
    if (!src_space || !dst_space) return ALWAN_E_INVALID;
    alwan_rgb_space_desc_f64 s, d;
    clf_widen_desc_32(&s, src_space);
    clf_widen_desc_32(&d, dst_space);
    return alwan_clf_export_view_buffer_f64(buf, bytes_written, buf_size, &s, &d, view, id, name, lut_size, ctx);
}
