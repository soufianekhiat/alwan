/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 80: Color Interop Forum — Interop IDs, CLF export,
 *          float16 conversion, data semantic enum
 */

#include "alwan.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#if ALWAN_SCALAR_IS_FLOAT
#  define TOL 1e-4
#else
#  define TOL 1e-10
#endif

#define ASSERT_NEAR(a, b, t) do { \
    double _a = (double)(a), _b = (double)(b); \
    if (fabs(_a - _b) > (t)) { \
        printf("  FAIL: %s = %.12f, expected %.12f (diff %.2e) at %s:%d\n", \
               #a, _a, _b, fabs(_a - _b), __FILE__, __LINE__); \
        return 1; \
    } \
} while(0)

/* ----------------------------------------------------------------
 * Test: Interop ID parse (string -> enum)
 * ---------------------------------------------------------------- */
static int test_interop_parse(void) {
    printf("  test_interop_parse...\n");

    alwan_rgb_space space;

    /* Scene-referred linear */
    if (alwan_interop_parse(&space, "lin_ap0") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_ACES2065_1) {
        printf("  FAIL: lin_ap0 -> %d, expected ACES2065_1\n", space);
        return 1;
    }

    if (alwan_interop_parse(&space, "lin_ap1") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_ACESCG) return 1;

    if (alwan_interop_parse(&space, "lin_srgb") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_LINEAR_REC709) return 1;

    if (alwan_interop_parse(&space, "lin_rec2020") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_LINEAR_REC2020) return 1;

    if (alwan_interop_parse(&space, "lin_displayp3") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_LINEAR_DISPLAY_P3) return 1;

    /* Scene-referred non-linear */
    if (alwan_interop_parse(&space, "acescc") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_ACESCC) return 1;

    if (alwan_interop_parse(&space, "acescct") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_ACESCCT) return 1;

    if (alwan_interop_parse(&space, "logc4_awg4") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_ARRI_LOGC4) return 1;

    if (alwan_interop_parse(&space, "slog3_sgamut3") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_S_LOG3) return 1;

    /* Display-referred */
    if (alwan_interop_parse(&space, "srgb_texture") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_SRGB) return 1;

    if (alwan_interop_parse(&space, "srgb_displayp3") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_DISPLAY_P3) return 1;

    if (alwan_interop_parse(&space, "rec1886_rec709") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_REC1886_REC709) return 1;

    if (alwan_interop_parse(&space, "rec2100_pq") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_REC2100_PQ) return 1;

    if (alwan_interop_parse(&space, "rec2100_hlg") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_REC2100_HLG) return 1;

    if (alwan_interop_parse(&space, "display_p3_hdr") != ALWAN_OK) return 1;
    if (space != ALWAN_RGB_SPACE_DISPLAY_P3_HDR) return 1;

    /* Unknown ID should return ALWAN_E_NODATA */
    if (alwan_interop_parse(&space, "nonexistent_space") != ALWAN_E_NODATA) {
        printf("  FAIL: unknown ID should return ALWAN_E_NODATA\n");
        return 1;
    }

    /* NULL checks */
    if (alwan_interop_parse(NULL, "lin_srgb") != ALWAN_E_INVALID) return 1;
    if (alwan_interop_parse(&space, NULL) != ALWAN_E_INVALID) return 1;

    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Interop ID format (enum -> string)
 * ---------------------------------------------------------------- */
static int test_interop_format(void) {
    printf("  test_interop_format...\n");

    /* Core mappings */
    char const *id;

    id = alwan_interop_format(ALWAN_RGB_SPACE_SRGB);
    if (!id || strcmp(id, "srgb_texture") != 0) {
        printf("  FAIL: SRGB -> '%s', expected 'srgb_texture'\n", id ? id : "(null)");
        return 1;
    }

    id = alwan_interop_format(ALWAN_RGB_SPACE_ACES2065_1);
    if (!id || strcmp(id, "lin_ap0") != 0) return 1;

    id = alwan_interop_format(ALWAN_RGB_SPACE_ACESCG);
    if (!id || strcmp(id, "lin_ap1") != 0) return 1;

    id = alwan_interop_format(ALWAN_RGB_SPACE_LINEAR_REC709);
    if (!id || strcmp(id, "lin_srgb") != 0) return 1;

    id = alwan_interop_format(ALWAN_RGB_SPACE_REC2100_PQ);
    if (!id || strcmp(id, "rec2100_pq") != 0) return 1;

    /* Space without interop ID should return NULL */
    id = alwan_interop_format(ALWAN_RGB_SPACE_BEST_RGB);
    if (id != NULL) {
        printf("  FAIL: BEST_RGB should have no interop ID, got '%s'\n", id);
        return 1;
    }

    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: Interop roundtrip (format -> parse -> same enum)
 * ---------------------------------------------------------------- */
static int test_interop_roundtrip(void) {
    printf("  test_interop_roundtrip...\n");

    size_t count = alwan_interop_count();
    if (count == 0) {
        printf("  FAIL: no interop entries\n");
        return 1;
    }

    for (size_t i = 0; i < count; i++) {
        alwan_rgb_space space_orig;
        char const *id_orig;
        alwan_interop_entry_at(&space_orig, &id_orig, i);

        /* format -> parse roundtrip */
        char const *formatted = alwan_interop_format(space_orig);
        if (!formatted || strcmp(formatted, id_orig) != 0) {
            printf("  FAIL: format mismatch at index %zu\n", i);
            return 1;
        }

        alwan_rgb_space parsed;
        if (alwan_interop_parse(&parsed, id_orig) != ALWAN_OK) {
            printf("  FAIL: parse failed for '%s'\n", id_orig);
            return 1;
        }
        if (parsed != space_orig) {
            printf("  FAIL: roundtrip mismatch for '%s': %d -> %d\n",
                   id_orig, space_orig, parsed);
            return 1;
        }
    }

    printf("    %zu interop IDs verified\n", count);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: float16 <-> float32 conversion
 * ---------------------------------------------------------------- */
static int test_half_float_conversion(void) {
    printf("  test_half_float_conversion...\n");

    /* Known half-float bit patterns and their float values:
     * 0x0000 = 0.0
     * 0x3C00 = 1.0
     * 0x4000 = 2.0
     * 0x3800 = 0.5
     * 0x3400 = 0.25
     * 0xBC00 = -1.0
     * 0x7C00 = +Inf
     * 0xFC00 = -Inf
     */
    alwan_uint16 halfs[] = {
        0x0000, /* 0.0 */
        0x3C00, /* 1.0 */
        0x4000, /* 2.0 */
        0x3800, /* 0.5 */
        0x3400, /* 0.25 */
        0xBC00, /* -1.0 */
    };
    float expected[] = {0.0f, 1.0f, 2.0f, 0.5f, 0.25f, -1.0f};
    int const count = 6;

    float results[6];
    int status = alwan_half_to_float(results, halfs, (size_t)count);
    if (status != ALWAN_OK) {
        printf("  FAIL: half_to_float returned %d\n", status);
        return 1;
    }

    for (int i = 0; i < count; i++) {
        if (fabsf(results[i] - expected[i]) > 1e-6f) {
            printf("  FAIL: half 0x%04X -> %.6f, expected %.6f\n",
                   halfs[i], (double)results[i], (double)expected[i]);
            return 1;
        }
    }

    /* Roundtrip: float -> half -> float */
    float test_vals[] = {0.0f, 0.5f, 1.0f, -1.0f, 0.333251953125f, 100.0f, 0.0001f};
    int const num_vals = 7;
    alwan_uint16 half_out[7];
    float roundtrip[7];

    status = alwan_float_to_half(half_out, test_vals, (size_t)num_vals);
    if (status != ALWAN_OK) {
        printf("  FAIL: float_to_half returned %d\n", status);
        return 1;
    }

    status = alwan_half_to_float(roundtrip, half_out, (size_t)num_vals);
    if (status != ALWAN_OK) {
        printf("  FAIL: half_to_float (roundtrip) returned %d\n", status);
        return 1;
    }

    /* Exact representable values should roundtrip perfectly */
    for (int i = 0; i < 4; i++) {
        if (roundtrip[i] != test_vals[i]) {
            printf("  FAIL: roundtrip %.6f -> 0x%04X -> %.6f\n",
                   (double)test_vals[i], half_out[i], (double)roundtrip[i]);
            return 1;
        }
    }

    /* Values may lose precision but should be close */
    for (int i = 4; i < num_vals; i++) {
        float rel_err = fabsf(roundtrip[i] - test_vals[i]) /
                        (fabsf(test_vals[i]) + 1e-10f);
        if (rel_err > 0.01f) { /* 1% relative error max for half precision */
            printf("  FAIL: roundtrip %.6f -> 0x%04X -> %.6f (rel err %.2e)\n",
                   (double)test_vals[i], half_out[i], (double)roundtrip[i], (double)rel_err);
            return 1;
        }
    }

    /* NULL checks */
    if (alwan_half_to_float(NULL, halfs, 1) != ALWAN_E_INVALID) return 1;
    if (alwan_half_to_float(results, NULL, 1) != ALWAN_E_INVALID) return 1;
    if (alwan_float_to_half(NULL, test_vals, 1) != ALWAN_E_INVALID) return 1;
    if (alwan_float_to_half(half_out, NULL, 1) != ALWAN_E_INVALID) return 1;

    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: float16 special values (Inf, NaN, denorm, zero)
 * ---------------------------------------------------------------- */
static int test_half_float_special(void) {
    printf("  test_half_float_special...\n");

    /* Positive zero */
    alwan_uint16 h_zero = 0x0000;
    float f_zero;
    alwan_half_to_float(&f_zero, &h_zero, 1);
    if (f_zero != 0.0f) return 1;

    /* Negative zero */
    alwan_uint16 h_neg_zero = 0x8000;
    float f_neg_zero;
    alwan_half_to_float(&f_neg_zero, &h_neg_zero, 1);
    if (f_neg_zero != -0.0f) return 1;

    /* Positive infinity */
    alwan_uint16 h_inf = 0x7C00;
    float f_inf;
    alwan_half_to_float(&f_inf, &h_inf, 1);
    if (!isinf(f_inf) || f_inf < 0) {
        printf("  FAIL: 0x7C00 should be +Inf, got %.6f\n", (double)f_inf);
        return 1;
    }

    /* Negative infinity */
    alwan_uint16 h_neg_inf = 0xFC00;
    float f_neg_inf;
    alwan_half_to_float(&f_neg_inf, &h_neg_inf, 1);
    if (!isinf(f_neg_inf) || f_neg_inf > 0) return 1;

    /* NaN */
    alwan_uint16 h_nan = 0x7C01;
    float f_nan;
    alwan_half_to_float(&f_nan, &h_nan, 1);
    if (!isnan(f_nan)) {
        printf("  FAIL: 0x7C01 should be NaN, got %.6f\n", (double)f_nan);
        return 1;
    }

    /* Float -> half for special values */
    float inf_val = f_inf; /* reuse the +Inf we decoded above */
    alwan_uint16 h_out;
    alwan_float_to_half(&h_out, &inf_val, 1);
    if (h_out != 0x7C00) {
        printf("  FAIL: +Inf -> 0x%04X, expected 0x7C00\n", h_out);
        return 1;
    }

    /* Very small denormalized value */
    alwan_uint16 h_denorm = 0x0001; /* smallest positive denorm: 2^-24 ≈ 5.96e-8 */
    float f_denorm;
    alwan_half_to_float(&f_denorm, &h_denorm, 1);
    if (f_denorm <= 0.0f || f_denorm > 1e-6f) {
        printf("  FAIL: smallest denorm = %.10e, expected ~5.96e-8\n", (double)f_denorm);
        return 1;
    }

    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: CLF export (basic structure validation)
 * ---------------------------------------------------------------- */
static int test_clf_export_basic(void) {
    printf("  test_clf_export_basic...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, p3;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&p3, ctx, ALWAN_RGB_SPACE_DISPLAY_P3);

    /* Export to file */
    char const *path = "test_srgb_to_p3.clf";
    int status = alwan_clf_export(path, ctx, &srgb, &p3,
                                   "test-srgb-to-p3", "sRGB to Display P3", 1024);
    if (status != ALWAN_OK) {
        printf("  FAIL: clf_export returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* Read the file and verify XML structure */
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  FAIL: cannot read exported CLF file\n");
        alwan_destroy(ctx);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)file_size + 1);
    fread(buf, 1, (size_t)file_size, f);
    buf[file_size] = '\0';
    fclose(f);

    /* Verify required CLF elements */
    if (!strstr(buf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>")) {
        printf("  FAIL: missing XML declaration\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "<ProcessList")) {
        printf("  FAIL: missing ProcessList\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "compCLFversion=\"3.0\"")) {
        printf("  FAIL: missing CLF version\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "id=\"test-srgb-to-p3\"")) {
        printf("  FAIL: missing id attribute\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "name=\"sRGB to Display P3\"")) {
        printf("  FAIL: missing name attribute\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }

    /* Should contain Exponent for sRGB EOTF + matrices + Exponent for sRGB OETF */
    if (!strstr(buf, "<Exponent")) {
        printf("  FAIL: missing Exponent node\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "<Matrix")) {
        printf("  FAIL: missing Matrix node\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "</ProcessList>")) {
        printf("  FAIL: missing closing ProcessList\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }

    /* sRGB -> P3 should have two Matrix nodes (src->XYZ and XYZ->dst) */
    /* Both share D65 white point, so no CAT matrix */
    char *first_matrix = strstr(buf, "<Matrix");
    if (!first_matrix) { free(buf); remove(path); alwan_destroy(ctx); return 1; }
    char *second_matrix = strstr(first_matrix + 1, "<Matrix");
    if (!second_matrix) {
        printf("  FAIL: expected at least 2 Matrix nodes\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }

    /* Should have Range node for gamut clamp */
    if (!strstr(buf, "<Range")) {
        printf("  FAIL: missing Range node\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }

    /* Should have monCurve for sRGB */
    if (!strstr(buf, "monCurveFwd")) {
        printf("  FAIL: missing monCurveFwd for sRGB EOTF\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "monCurveRev")) {
        printf("  FAIL: missing monCurveRev for sRGB OETF\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }

    free(buf);
    remove(path);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: CLF export to buffer
 * ---------------------------------------------------------------- */
static int test_clf_export_buffer(void) {
    printf("  test_clf_export_buffer...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);

    /* Identity: sRGB -> sRGB should still have valid CLF structure */
    char buf[32768];
    size_t written = 0;
    int status = alwan_clf_export_buffer(buf, sizeof(buf), &written, ctx,
                                          &srgb, &srgb, "identity", "sRGB identity", 256);
    if (status != ALWAN_OK) {
        printf("  FAIL: clf_export_buffer returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    if (written == 0) {
        printf("  FAIL: 0 bytes written\n");
        alwan_destroy(ctx);
        return 1;
    }

    /* Verify it's valid XML */
    if (!strstr(buf, "<?xml")) {
        printf("  FAIL: missing XML declaration in buffer\n");
        alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "</ProcessList>")) {
        printf("  FAIL: missing closing tag in buffer\n");
        alwan_destroy(ctx);
        return 1;
    }

    /* Too-small buffer should return ALWAN_E_RANGE */
    size_t small_written = 0;
    char small_buf[10];
    status = alwan_clf_export_buffer(small_buf, sizeof(small_buf), &small_written,
                                      ctx, &srgb, &srgb, NULL, NULL, 256);
    if (status != ALWAN_E_RANGE) {
        printf("  FAIL: small buffer should return ALWAN_E_RANGE, got %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: CLF export with PQ transfer function (uses LUT1D)
 * ---------------------------------------------------------------- */
static int test_clf_export_pq(void) {
    printf("  test_clf_export_pq...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, rec2100pq;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&rec2100pq, ctx, ALWAN_RGB_SPACE_REC2100_PQ);

    char buf[524288]; /* 512KB for LUT1D data */
    size_t written = 0;
    int status = alwan_clf_export_buffer(buf, sizeof(buf), &written, ctx,
                                          &srgb, &rec2100pq, "srgb-to-pq",
                                          "sRGB to Rec.2100 PQ", 1024);
    if (status != ALWAN_OK) {
        printf("  FAIL: clf_export_buffer returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* sRGB EOTF should be an Exponent (monCurve), PQ OETF should be a LUT1D */
    if (!strstr(buf, "monCurveFwd")) {
        printf("  FAIL: missing monCurveFwd for sRGB EOTF\n");
        alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "<LUT1D")) {
        printf("  FAIL: missing LUT1D for PQ OETF\n");
        alwan_destroy(ctx);
        return 1;
    }

    /* Should have 3 Matrix nodes (src->XYZ, CAT, XYZ->dst) since
     * sRGB has D65 white and Rec.2100 has D65 too — actually no CAT needed.
     * So 2 matrices. */
    if (!strstr(buf, "<Matrix")) {
        printf("  FAIL: missing Matrix node\n");
        alwan_destroy(ctx);
        return 1;
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: CLF export with view transform
 * ---------------------------------------------------------------- */
static int test_clf_export_view(void) {
    printf("  test_clf_export_view...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);

    char const *path = "test_view.clf";
    int status = alwan_clf_export_view(path, ctx, &srgb, &srgb,
                                        ALWAN_VIEW_REINHARD_EXT,
                                        "reinhard-ext", "sRGB + Reinhard Extended", 17);
    if (status != ALWAN_OK) {
        printf("  FAIL: clf_export_view returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* Read and verify LUT3D is present */
    FILE *f = fopen(path, "r");
    if (!f) { alwan_destroy(ctx); return 1; }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)file_size + 1);
    fread(buf, 1, (size_t)file_size, f);
    buf[file_size] = '\0';
    fclose(f);

    if (!strstr(buf, "<LUT3D")) {
        printf("  FAIL: missing LUT3D for view transform\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }
    if (!strstr(buf, "trilinear")) {
        printf("  FAIL: missing trilinear interpolation attribute\n");
        free(buf); remove(path); alwan_destroy(ctx);
        return 1;
    }

    free(buf);
    remove(path);
    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: CLF export with CAT (cross-white-point)
 * ---------------------------------------------------------------- */
static int test_clf_export_cat(void) {
    printf("  test_clf_export_cat...\n");

    alwan_ctx *ctx = alwan_create(NULL);
    alwan_rgb_space_desc srgb, prophoto;
    alwan_rgb_get_space_descriptor(&srgb, ctx, ALWAN_RGB_SPACE_SRGB);
    alwan_rgb_get_space_descriptor(&prophoto, ctx, ALWAN_RGB_SPACE_PROPHOTO_RGB);

    /* sRGB (D65) -> ProPhoto (D50) requires chromatic adaptation */
    char buf[65536];
    size_t written = 0;
    int status = alwan_clf_export_buffer(buf, sizeof(buf), &written, ctx,
                                          &srgb, &prophoto, "srgb-to-prophoto",
                                          "sRGB to ProPhoto (CAT)", 1024);
    if (status != ALWAN_OK) {
        printf("  FAIL: clf_export_buffer returned %d\n", status);
        alwan_destroy(ctx);
        return 1;
    }

    /* Should have 3 Matrix nodes: src->XYZ, CAT (Bradford), XYZ->dst */
    int matrix_count = 0;
    char *p = buf;
    while ((p = strstr(p, "<Matrix")) != NULL) {
        matrix_count++;
        p++;
    }
    if (matrix_count != 3) {
        printf("  FAIL: expected 3 Matrix nodes (src, CAT, dst), got %d\n", matrix_count);
        alwan_destroy(ctx);
        return 1;
    }

    /* Should mention Bradford */
    if (!strstr(buf, "Bradford")) {
        printf("  FAIL: CAT matrix should mention Bradford\n");
        alwan_destroy(ctx);
        return 1;
    }

    alwan_destroy(ctx);
    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Test: CLF NULL checks
 * ---------------------------------------------------------------- */
static int test_clf_null_checks(void) {
    printf("  test_clf_null_checks...\n");

    alwan_rgb_space_desc desc;
    memset(&desc, 0, sizeof(desc));

    if (alwan_clf_export(NULL, NULL, &desc, &desc, NULL, NULL, 0) != ALWAN_E_INVALID) return 1;
    if (alwan_clf_export("f.clf", NULL, NULL, &desc, NULL, NULL, 0) != ALWAN_E_INVALID) return 1;
    if (alwan_clf_export("f.clf", NULL, &desc, NULL, NULL, NULL, 0) != ALWAN_E_INVALID) return 1;

    char buf[100];
    size_t w = 0;
    if (alwan_clf_export_buffer(NULL, 100, &w, NULL, &desc, &desc, NULL, NULL, 0) != ALWAN_E_INVALID) return 1;
    if (alwan_clf_export_buffer(buf, 100, NULL, NULL, &desc, &desc, NULL, NULL, 0) != ALWAN_E_INVALID) return 1;

    printf("  PASS\n");
    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */
int test_80_interop_clf_main(void) {
    int fail = 0;

    /* Interop ID tests */
    fail += test_interop_parse();
    fail += test_interop_format();
    fail += test_interop_roundtrip();

    /* float16 conversion tests */
    fail += test_half_float_conversion();
    fail += test_half_float_special();

    /* CLF export tests */
    fail += test_clf_export_basic();
    fail += test_clf_export_buffer();
    fail += test_clf_export_pq();
    fail += test_clf_export_view();
    fail += test_clf_export_cat();
    fail += test_clf_null_checks();

    return fail;
}
