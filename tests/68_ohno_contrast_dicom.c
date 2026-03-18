/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 68: Ohno 2013 CCT, Weber & Michelson Contrast, DICOM GSDF
 */

#include "test_common.h"
#include <stdlib.h>

#include "core/alwan_quality_core.h"
#include "core/alwan_rgb_core.h"

/* ----------------------------------------------------------------
 * Ohno 2013 CCT: D65 in CIE 1960 UCS
 * ---------------------------------------------------------------- */

static int test_ohno2013_d65(void) {
    /* D65 in CIE 1960 UCS: u=0.19784, v=0.31216 (approximately) */
    alwan_f64 u = ALWAN_LITERAL(0.19784);
    alwan_f64 v = ALWAN_LITERAL(0.31216);

    alwan_f64 cct = alwan_cct_ohno2013_f64_v(u, v);

    /* D65 has CCT ~6504K */
    TEST_ASSERT(cct > ALWAN_LITERAL(6000.0) && cct < ALWAN_LITERAL(7000.0),
                "ohno2013 D65 CCT in [6000, 7000]");

    TEST_PASS("ohno2013 D65");
}

/* ----------------------------------------------------------------
 * Ohno 2013 CCT: A illuminant in CIE 1960 UCS
 * ---------------------------------------------------------------- */

static int test_ohno2013_illuminant_a(void) {
    /* Illuminant A (2856K) in CIE 1960 UCS: u~0.2560, v~0.3495 */
    alwan_f64 u = ALWAN_LITERAL(0.2560);
    alwan_f64 v = ALWAN_LITERAL(0.3495);

    alwan_f64 cct = alwan_cct_ohno2013_f64_v(u, v);

    /* Illuminant A has CCT ~2856K */
    TEST_ASSERT(cct > ALWAN_LITERAL(2500.0) && cct < ALWAN_LITERAL(3200.0),
                "ohno2013 A CCT in [2500, 3200]");

    TEST_PASS("ohno2013 illuminant A");
}

/* ----------------------------------------------------------------
 * Weber Contrast
 * ---------------------------------------------------------------- */

static int test_weber_contrast(void) {
    /* Target brighter than background: positive contrast */
    alwan_f64 c1 = alwan_weber_contrast_f64_v(ALWAN_LITERAL(200.0), ALWAN_LITERAL(100.0));
    TEST_ASSERT_NEAR(c1, ALWAN_LITERAL(1.0), ALWAN_LITERAL(1e-6), "weber contrast 200/100");

    /* Target equal to background: zero contrast */
    alwan_f64 c2 = alwan_weber_contrast_f64_v(ALWAN_LITERAL(100.0), ALWAN_LITERAL(100.0));
    TEST_ASSERT_NEAR(c2, ALWAN_ZERO, ALWAN_LITERAL(1e-6), "weber contrast equal");

    /* Target darker than background: negative contrast */
    alwan_f64 c3 = alwan_weber_contrast_f64_v(ALWAN_LITERAL(50.0), ALWAN_LITERAL(100.0));
    TEST_ASSERT_NEAR(c3, ALWAN_LITERAL(-0.5), ALWAN_LITERAL(1e-6), "weber contrast dark");

    TEST_PASS("weber contrast");
}

/* ----------------------------------------------------------------
 * Weber Contrast API
 * ---------------------------------------------------------------- */

static int test_weber_contrast_api(void) {
    alwan_f64 result;
    int status = alwan_weber_contrast(&result, ALWAN_LITERAL(200.0), ALWAN_LITERAL(100.0));
    TEST_ASSERT(status == ALWAN_OK, "weber api failed");
    TEST_ASSERT_NEAR(result, ALWAN_LITERAL(1.0), ALWAN_LITERAL(1e-6), "weber api value");

    TEST_PASS("weber contrast api");
}

/* ----------------------------------------------------------------
 * Michelson Contrast
 * ---------------------------------------------------------------- */

static int test_michelson_contrast(void) {
    /* Standard case: (200-100)/(200+100) = 100/300 = 0.3333... */
    alwan_f64 c1 = alwan_michelson_contrast_f64_v(ALWAN_LITERAL(200.0), ALWAN_LITERAL(100.0));
    TEST_ASSERT_NEAR(c1, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0), ALWAN_LITERAL(1e-6),
                     "michelson 200/100");

    /* Equal: zero contrast */
    alwan_f64 c2 = alwan_michelson_contrast_f64_v(ALWAN_LITERAL(100.0), ALWAN_LITERAL(100.0));
    TEST_ASSERT_NEAR(c2, ALWAN_ZERO, ALWAN_LITERAL(1e-6), "michelson equal");

    /* Maximum contrast: Lmin=0 */
    alwan_f64 c3 = alwan_michelson_contrast_f64_v(ALWAN_LITERAL(100.0), ALWAN_LITERAL(0.0));
    TEST_ASSERT_NEAR(c3, ALWAN_LITERAL(1.0), ALWAN_LITERAL(1e-6), "michelson max");

    TEST_PASS("michelson contrast");
}

/* ----------------------------------------------------------------
 * Michelson Contrast API
 * ---------------------------------------------------------------- */

static int test_michelson_contrast_api(void) {
    alwan_f64 result;
    int status = alwan_michelson_contrast(&result, ALWAN_LITERAL(200.0), ALWAN_LITERAL(100.0));
    TEST_ASSERT(status == ALWAN_OK, "michelson api failed");
    TEST_ASSERT_NEAR(result, ALWAN_LITERAL(1.0) / ALWAN_LITERAL(3.0), ALWAN_LITERAL(1e-6),
                     "michelson api value");

    TEST_PASS("michelson contrast api");
}

/* ----------------------------------------------------------------
 * DICOM GSDF EOTF/OETF roundtrip
 * ---------------------------------------------------------------- */

static int test_dicom_gsdf_roundtrip(void) {
    /* DICOM PS3.14 uses two independent polynomial fits (not exact inverses),
     * so the roundtrip tolerance must account for fitting error. */
    alwan_f64 jnd_values[] = {
        ALWAN_LITERAL(100.0), ALWAN_LITERAL(200.0), ALWAN_LITERAL(400.0),
        ALWAN_LITERAL(600.0), ALWAN_LITERAL(800.0)
    };

    for (int i = 0; i < 5; i++) {
        alwan_f64 jnd = jnd_values[i];
        alwan_f64 luminance = alwan_dicom_gsdf_eotf_f64_v(jnd);
        alwan_f64 jnd_rt = alwan_dicom_gsdf_oetf_f64_v(luminance);

        /* Allow up to 2% relative error due to independent polynomial fits */
        alwan_f64 rel_err = ALWAN_ABS(jnd_rt - jnd) / jnd;
        TEST_ASSERT(rel_err < ALWAN_LITERAL(0.02), "dicom gsdf roundtrip rel error > 2%");
    }

    TEST_PASS("dicom gsdf roundtrip");
}

/* ----------------------------------------------------------------
 * DICOM GSDF: monotonicity
 * ---------------------------------------------------------------- */

static int test_dicom_gsdf_monotonic(void) {
    alwan_f64 prev = alwan_dicom_gsdf_eotf_f64_v(ALWAN_LITERAL(1.0));

    for (int i = 2; i <= 1023; i++) {
        alwan_f64 jnd = (alwan_f64)i;
        alwan_f64 lum = alwan_dicom_gsdf_eotf_f64_v(jnd);
        TEST_ASSERT(lum >= prev - ALWAN_LITERAL(1e-6), "dicom gsdf not monotonic");
        prev = lum;
    }

    TEST_PASS("dicom gsdf monotonic");
}

/* ----------------------------------------------------------------
 * DICOM GSDF: known range
 * ---------------------------------------------------------------- */

static int test_dicom_gsdf_range(void) {
    /* JND=1: minimum displayable luminance (~0.05 cd/m2) */
    alwan_f64 lum_min = alwan_dicom_gsdf_eotf_f64_v(ALWAN_LITERAL(1.0));
    TEST_ASSERT(lum_min > ALWAN_ZERO, "dicom gsdf min > 0");
    TEST_ASSERT(lum_min < ALWAN_LITERAL(1.0), "dicom gsdf min < 1");

    /* JND=1023: maximum displayable luminance (~4000 cd/m2) */
    alwan_f64 lum_max = alwan_dicom_gsdf_eotf_f64_v(ALWAN_LITERAL(1023.0));
    TEST_ASSERT(lum_max > ALWAN_LITERAL(1000.0), "dicom gsdf max > 1000");

    TEST_PASS("dicom gsdf range");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_68_ohno_contrast_dicom_main(void) {
    int failures = 0;

    failures += test_ohno2013_d65();
    failures += test_ohno2013_illuminant_a();
    failures += test_weber_contrast();
    failures += test_weber_contrast_api();
    failures += test_michelson_contrast();
    failures += test_michelson_contrast_api();
    failures += test_dicom_gsdf_roundtrip();
    failures += test_dicom_gsdf_monotonic();
    failures += test_dicom_gsdf_range();

    if (failures == 0) {
        printf("\n=== All Ohno/contrast/DICOM tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
