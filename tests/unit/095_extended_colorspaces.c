/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 095: Extended Color Spaces
 * YCoCg, UCS, hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb, Prismatic, HCL, IHLS
 */

#include "alwan.h"
#include "alwan_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------
 * Test helpers
 * ---------------------------------------------------------------- */

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("[PASS] %s\n", name); \
    return 0; \
} while(0)

static alwan_scalar vec3_max_diff(alwan_vec3 const *a, alwan_vec3 const *b) {
    alwan_scalar max_diff = 0;
    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_FABS(a->v[i] - b->v[i]);
        if (diff > max_diff) max_diff = diff;
    }
    return max_diff;
}

static void vec3_print(char const *name, alwan_vec3 const *v) {
    printf("%s: [%12.8f %12.8f %12.8f]\n", name, v->v[0], v->v[1], v->v[2]);
}

/* ----------------------------------------------------------------
 * YCoCg Tests
 * ---------------------------------------------------------------- */

static int test_ycocg_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ycocg_from_rgb[] = {
#include "reference_values/ycocg_from_rgb.csv"
    };
    static alwan_scalar const rgb_from_ycocg_roundtrip[] = {
#include "reference_values/rgb_from_ycocg_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(ycocg_from_rgb) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    /* Test RGB inputs (from generate_data_tests.ps1) */
    alwan_scalar const test_rgb[][3] = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}, {0.5, 0.5, 0.5}, {0.25, 0.75, 0.5}, {0.8, 0.2, 0.4},
        {0.1, 0.6, 0.9}, {0.9, 0.3, 0.1}, {0.3, 0.9, 0.7}
    };

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 rgb = {{test_rgb[i][0], test_rgb[i][1], test_rgb[i][2]}};
        alwan_vec3 ycocg_expected = {{ycocg_from_rgb[i * 3 + 0], ycocg_from_rgb[i * 3 + 1], ycocg_from_rgb[i * 3 + 2]}};
        alwan_vec3 rgb_expected = {{rgb_from_ycocg_roundtrip[i * 3 + 0], rgb_from_ycocg_roundtrip[i * 3 + 1], rgb_from_ycocg_roundtrip[i * 3 + 2]}};

        /* RGB -> YCoCg */
        alwan_vec3 ycocg;
        alwan_rgb_to_ycocg(&rgb, &ycocg);
        alwan_scalar diff_forward = vec3_max_diff(&ycocg, &ycocg_expected);
        TEST_ASSERT(diff_forward < tolerance, "RGB->YCoCg mismatch");

        /* YCoCg -> RGB */
        alwan_vec3 rgb_out;
        alwan_ycocg_to_rgb(&ycocg, &rgb_out);
        alwan_scalar diff_inverse = vec3_max_diff(&rgb_out, &rgb_expected);
        TEST_ASSERT(diff_inverse < tolerance, "YCoCg->RGB roundtrip mismatch");
    }

    TEST_PASS("YCoCg roundtrip");
}

/* ----------------------------------------------------------------
 * CIE UCS 1960 Tests
 * ---------------------------------------------------------------- */

static int test_ucs_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_xyz_colors[] = {
#include "reference_values/test_xyz_colors.csv"
    };
    static alwan_scalar const ucs_from_xyz[] = {
#include "reference_values/ucs_from_xyz.csv"
    };
    static alwan_scalar const xyz_from_ucs_roundtrip[] = {
#include "reference_values/xyz_from_ucs_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(ucs_from_xyz) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_colors[i * 3 + 0], test_xyz_colors[i * 3 + 1], test_xyz_colors[i * 3 + 2]}};
        alwan_vec3 ucs_expected = {{ucs_from_xyz[i * 3 + 0], ucs_from_xyz[i * 3 + 1], ucs_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_ucs_roundtrip[i * 3 + 0], xyz_from_ucs_roundtrip[i * 3 + 1], xyz_from_ucs_roundtrip[i * 3 + 2]}};

        /* XYZ -> UCS */
        alwan_vec3 ucs;
        alwan_xyz_to_ucs(&xyz, &ucs);
        alwan_scalar diff_forward = vec3_max_diff(&ucs, &ucs_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->UCS mismatch");

        /* UCS -> XYZ */
        alwan_vec3 xyz_out;
        alwan_ucs_to_xyz(&ucs, &xyz_out);
        alwan_scalar diff_inverse = vec3_max_diff(&xyz_out, &xyz_expected);
        TEST_ASSERT(diff_inverse < tolerance, "UCS->XYZ roundtrip mismatch");
    }

    TEST_PASS("CIE UCS 1960 roundtrip");
}

/* ----------------------------------------------------------------
 * hdr-CIELAB Tests
 * ---------------------------------------------------------------- */

static int test_hdr_cielab_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const hdr_lab_from_xyz[] = {
#include "reference_values/hdr_lab_from_xyz.csv"
    };
    static alwan_scalar const xyz_from_hdr_lab_roundtrip[] = {
#include "reference_values/xyz_from_hdr_lab_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    /* HDR test XYZ values */
    alwan_scalar const test_xyz_hdr[][3] = {
        {0.95047, 1.0, 1.08883},
        {95.047, 100.0, 108.883},
        {190.094, 200.0, 217.766},
        {380.188, 400.0, 435.532},
        {100.0, 50.0, 50.0},
        {50.0, 150.0, 50.0},
        {50.0, 50.0, 200.0}
    };

    int const num_tests = sizeof(test_xyz_hdr) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-4);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_hdr[i][0], test_xyz_hdr[i][1], test_xyz_hdr[i][2]}};
        alwan_vec3 hdr_lab_expected = {{hdr_lab_from_xyz[i * 3 + 0], hdr_lab_from_xyz[i * 3 + 1], hdr_lab_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_hdr_lab_roundtrip[i * 3 + 0], xyz_from_hdr_lab_roundtrip[i * 3 + 1], xyz_from_hdr_lab_roundtrip[i * 3 + 2]}};

        /* XYZ -> hdr-CIELAB */
        alwan_vec3 hdr_lab;
        alwan_xyz_to_hdr_cielab(&xyz, &hdr_lab);
        alwan_scalar diff_forward = vec3_max_diff(&hdr_lab, &hdr_lab_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->hdr-CIELAB mismatch");

        /* hdr-CIELAB -> XYZ */
        alwan_vec3 xyz_out;
        alwan_hdr_cielab_to_xyz(&hdr_lab, &xyz_out);
        alwan_scalar diff_inverse = vec3_max_diff(&xyz_out, &xyz_expected);
        TEST_ASSERT(diff_inverse < tolerance, "hdr-CIELAB->XYZ roundtrip mismatch");
    }

    TEST_PASS("hdr-CIELAB roundtrip");
}

/* ----------------------------------------------------------------
 * hdr-IPT Tests
 * ---------------------------------------------------------------- */

static int test_hdr_ipt_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const hdr_ipt_from_xyz[] = {
#include "reference_values/hdr_ipt_from_xyz.csv"
    };
    static alwan_scalar const xyz_from_hdr_ipt_roundtrip[] = {
#include "reference_values/xyz_from_hdr_ipt_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    /* Same HDR test XYZ values */
    alwan_scalar const test_xyz_hdr[][3] = {
        {0.95047, 1.0, 1.08883},
        {95.047, 100.0, 108.883},
        {190.094, 200.0, 217.766},
        {380.188, 400.0, 435.532},
        {100.0, 50.0, 50.0},
        {50.0, 150.0, 50.0},
        {50.0, 50.0, 200.0}
    };

    int const num_tests = sizeof(test_xyz_hdr) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-4);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_hdr[i][0], test_xyz_hdr[i][1], test_xyz_hdr[i][2]}};
        alwan_vec3 hdr_ipt_expected = {{hdr_ipt_from_xyz[i * 3 + 0], hdr_ipt_from_xyz[i * 3 + 1], hdr_ipt_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_hdr_ipt_roundtrip[i * 3 + 0], xyz_from_hdr_ipt_roundtrip[i * 3 + 1], xyz_from_hdr_ipt_roundtrip[i * 3 + 2]}};

        /* XYZ -> hdr-IPT */
        alwan_vec3 hdr_ipt;
        alwan_xyz_to_hdr_ipt(&xyz, &hdr_ipt);
        alwan_scalar diff_forward = vec3_max_diff(&hdr_ipt, &hdr_ipt_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->hdr-IPT mismatch");

        /* hdr-IPT -> XYZ */
        alwan_vec3 xyz_out;
        alwan_hdr_ipt_to_xyz(&hdr_ipt, &xyz_out);
        alwan_scalar diff_inverse = vec3_max_diff(&xyz_out, &xyz_expected);
        TEST_ASSERT(diff_inverse < tolerance, "hdr-IPT->XYZ roundtrip mismatch");
    }

    TEST_PASS("hdr-IPT roundtrip");
}

/* ----------------------------------------------------------------
 * IgPgTg Tests
 * ---------------------------------------------------------------- */

static int test_igpgtg_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_xyz_colors[] = {
#include "reference_values/test_xyz_colors.csv"
    };
    static alwan_scalar const igpgtg_from_xyz[] = {
#include "reference_values/igpgtg_from_xyz.csv"
    };
    static alwan_scalar const xyz_from_igpgtg_roundtrip[] = {
#include "reference_values/xyz_from_igpgtg_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(igpgtg_from_xyz) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_colors[i * 3 + 0], test_xyz_colors[i * 3 + 1], test_xyz_colors[i * 3 + 2]}};
        alwan_vec3 igpgtg_expected = {{igpgtg_from_xyz[i * 3 + 0], igpgtg_from_xyz[i * 3 + 1], igpgtg_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_igpgtg_roundtrip[i * 3 + 0], xyz_from_igpgtg_roundtrip[i * 3 + 1], xyz_from_igpgtg_roundtrip[i * 3 + 2]}};

        /* XYZ -> IgPgTg */
        alwan_vec3 igpgtg;
        alwan_xyz_to_igpgtg(&xyz, &igpgtg);
        alwan_scalar diff_forward = vec3_max_diff(&igpgtg, &igpgtg_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->IgPgTg mismatch");

        /* IgPgTg -> XYZ */
        alwan_vec3 xyz_out;
        alwan_igpgtg_to_xyz(&igpgtg, &xyz_out);
        alwan_scalar diff_inverse = vec3_max_diff(&xyz_out, &xyz_expected);
        TEST_ASSERT(diff_inverse < tolerance, "IgPgTg->XYZ roundtrip mismatch");
    }

    TEST_PASS("IgPgTg roundtrip");
}

/* ----------------------------------------------------------------
 * ICaCb Tests
 * ---------------------------------------------------------------- */

static int test_icacb_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const test_xyz_colors[] = {
#include "reference_values/test_xyz_colors.csv"
    };
    static alwan_scalar const icacb_from_xyz[] = {
#include "reference_values/icacb_from_xyz.csv"
    };
    static alwan_scalar const xyz_from_icacb_roundtrip[] = {
#include "reference_values/xyz_from_icacb_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(icacb_from_xyz) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-4);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-10);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_colors[i * 3 + 0], test_xyz_colors[i * 3 + 1], test_xyz_colors[i * 3 + 2]}};
        alwan_vec3 icacb_expected = {{icacb_from_xyz[i * 3 + 0], icacb_from_xyz[i * 3 + 1], icacb_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_icacb_roundtrip[i * 3 + 0], xyz_from_icacb_roundtrip[i * 3 + 1], xyz_from_icacb_roundtrip[i * 3 + 2]}};

        /* XYZ -> ICaCb */
        alwan_vec3 icacb;
        alwan_xyz_to_icacb(&xyz, &icacb);
        alwan_scalar diff_forward = vec3_max_diff(&icacb, &icacb_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->ICaCb mismatch");

        /* ICaCb -> XYZ */
        alwan_vec3 xyz_out;
        alwan_icacb_to_xyz(&icacb, &xyz_out);
        alwan_scalar diff_inverse = vec3_max_diff(&xyz_out, &xyz_expected);
        TEST_ASSERT(diff_inverse < tolerance, "ICaCb->XYZ roundtrip mismatch");
    }

    TEST_PASS("ICaCb roundtrip");
}

/* ----------------------------------------------------------------
 * Prismatic Tests
 * ---------------------------------------------------------------- */

static int test_prismatic_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const prismatic_from_rgb[] = {
#include "reference_values/prismatic_from_rgb.csv"
    };
    static alwan_scalar const rgb_from_prismatic_roundtrip[] = {
#include "reference_values/rgb_from_prismatic_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    /* Test RGB inputs */
    alwan_scalar const test_rgb[][3] = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}, {0.5, 0.5, 0.5}, {0.25, 0.75, 0.5}, {0.8, 0.2, 0.4},
        {0.1, 0.6, 0.9}, {0.9, 0.3, 0.1}, {0.3, 0.9, 0.7}
    };

    int const num_tests = sizeof(test_rgb) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 rgb = {{test_rgb[i][0], test_rgb[i][1], test_rgb[i][2]}};
        alwan_vec3 prismatic_expected = {{prismatic_from_rgb[i * 3 + 0], prismatic_from_rgb[i * 3 + 1], prismatic_from_rgb[i * 3 + 2]}};
        alwan_vec3 rgb_expected = {{rgb_from_prismatic_roundtrip[i * 3 + 0], rgb_from_prismatic_roundtrip[i * 3 + 1], rgb_from_prismatic_roundtrip[i * 3 + 2]}};

        /* RGB -> Prismatic */
        alwan_vec3 prismatic;
        alwan_rgb_to_prismatic(&rgb, &prismatic);
        alwan_scalar diff_forward = vec3_max_diff(&prismatic, &prismatic_expected);
        TEST_ASSERT(diff_forward < tolerance, "RGB->Prismatic mismatch");

        /* Prismatic -> RGB */
        alwan_vec3 rgb_out;
        alwan_prismatic_to_rgb(&prismatic, &rgb_out);
        alwan_scalar diff_inverse = vec3_max_diff(&rgb_out, &rgb_expected);
        TEST_ASSERT(diff_inverse < tolerance, "Prismatic->RGB roundtrip mismatch");
    }

    TEST_PASS("Prismatic roundtrip");
}

/* ----------------------------------------------------------------
 * HCL Tests
 * ---------------------------------------------------------------- */

static int test_hcl_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const hcl_from_rgb[] = {
#include "reference_values/hcl_from_rgb.csv"
    };
    static alwan_scalar const rgb_from_hcl_roundtrip[] = {
#include "reference_values/rgb_from_hcl_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    /* Test RGB inputs */
    alwan_scalar const test_rgb[][3] = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}, {0.5, 0.5, 0.5}, {0.25, 0.75, 0.5}, {0.8, 0.2, 0.4},
        {0.1, 0.6, 0.9}, {0.9, 0.3, 0.1}, {0.3, 0.9, 0.7}
    };

    int const num_tests = sizeof(test_rgb) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 rgb = {{test_rgb[i][0], test_rgb[i][1], test_rgb[i][2]}};
        alwan_vec3 hcl_expected = {{hcl_from_rgb[i * 3 + 0], hcl_from_rgb[i * 3 + 1], hcl_from_rgb[i * 3 + 2]}};
        alwan_vec3 rgb_expected = {{rgb_from_hcl_roundtrip[i * 3 + 0], rgb_from_hcl_roundtrip[i * 3 + 1], rgb_from_hcl_roundtrip[i * 3 + 2]}};

        /* RGB -> HCL */
        alwan_vec3 hcl;
        alwan_rgb_to_hcl(&rgb, &hcl);
        alwan_scalar diff_forward = vec3_max_diff(&hcl, &hcl_expected);
        TEST_ASSERT(diff_forward < tolerance, "RGB->HCL mismatch");

        /* HCL -> RGB */
        alwan_vec3 rgb_out;
        alwan_hcl_to_rgb(&hcl, &rgb_out);
        alwan_scalar diff_inverse = vec3_max_diff(&rgb_out, &rgb_expected);
        TEST_ASSERT(diff_inverse < tolerance, "HCL->RGB roundtrip mismatch");
    }

    TEST_PASS("HCL roundtrip");
}

/* ----------------------------------------------------------------
 * IHLS Tests
 * ---------------------------------------------------------------- */

static int test_ihls_roundtrip(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_scalar const ihls_from_rgb[] = {
#include "reference_values/ihls_from_rgb.csv"
    };
    static alwan_scalar const rgb_from_ihls_roundtrip[] = {
#include "reference_values/rgb_from_ihls_roundtrip.csv"
    };
    ALWAN_DIAG_POP

    /* Test RGB inputs */
    alwan_scalar const test_rgb[][3] = {
        {0.0, 0.0, 0.0}, {1.0, 1.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
        {0.0, 0.0, 1.0}, {0.5, 0.5, 0.5}, {0.25, 0.75, 0.5}, {0.8, 0.2, 0.4},
        {0.1, 0.6, 0.9}, {0.9, 0.3, 0.1}, {0.3, 0.9, 0.7}
    };

    int const num_tests = sizeof(test_rgb) / (3 * sizeof(alwan_scalar));
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-5);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-11);
#endif

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 rgb = {{test_rgb[i][0], test_rgb[i][1], test_rgb[i][2]}};
        alwan_vec3 ihls_expected = {{ihls_from_rgb[i * 3 + 0], ihls_from_rgb[i * 3 + 1], ihls_from_rgb[i * 3 + 2]}};
        alwan_vec3 rgb_expected = {{rgb_from_ihls_roundtrip[i * 3 + 0], rgb_from_ihls_roundtrip[i * 3 + 1], rgb_from_ihls_roundtrip[i * 3 + 2]}};

        /* RGB -> IHLS */
        alwan_vec3 ihls;
        alwan_rgb_to_ihls(&rgb, &ihls);
        alwan_scalar diff_forward = vec3_max_diff(&ihls, &ihls_expected);
        TEST_ASSERT(diff_forward < tolerance, "RGB->IHLS mismatch");

        /* IHLS -> RGB */
        alwan_vec3 rgb_out;
        alwan_ihls_to_rgb(&ihls, &rgb_out);
        alwan_scalar diff_inverse = vec3_max_diff(&rgb_out, &rgb_expected);
        TEST_ASSERT(diff_inverse < tolerance, "IHLS->RGB roundtrip mismatch");
    }

    TEST_PASS("IHLS roundtrip");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_095_extended_colorspaces_main(void) {
    int failures = 0;

    printf("=== Extended Color Spaces Tests ===\n");

    failures += test_ycocg_roundtrip();
    failures += test_ucs_roundtrip();
    failures += test_hdr_cielab_roundtrip();
    failures += test_hdr_ipt_roundtrip();
    failures += test_igpgtg_roundtrip();
    failures += test_icacb_roundtrip();
    failures += test_prismatic_roundtrip();
    failures += test_hcl_roundtrip();
    failures += test_ihls_roundtrip();

    if (failures == 0) {
        printf("\n=== All extended color space tests passed! ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
