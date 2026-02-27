/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 16: Extended Color Spaces
 * YCoCg, UCS, hdr-CIELAB, hdr-IPT, IgPgTg, ICaCb, Prismatic, HCL, IHLS
 */

#include "test_common.h"
#include <stdlib.h>
#include <string.h>

static alwan_scalar vec3_max_diff(alwan_vec3 const *a, alwan_vec3 const *b) {
    alwan_scalar max_diff = 0;
    for (int i = 0; i < 3; i++) {
        alwan_scalar diff = ALWAN_ABS(a->v[i] - b->v[i]);
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
    alwan_scalar const tolerance = TEST_TOLERANCE;

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
        alwan_rgb rgb_typed;
        alwan_ycocg ycocg_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3));
        alwan_rgb_to_ycocg(&ycocg_typed, &rgb_typed);
        ALWAN_MEMCPY(&ycocg, &ycocg_typed, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&ycocg, &ycocg_expected);
        TEST_ASSERT(diff_forward < tolerance, "RGB->YCoCg mismatch");

        /* YCoCg -> RGB */
        alwan_vec3 rgb_out;
        alwan_rgb rgb_out_typed;
        alwan_ycocg_to_rgb(&rgb_out_typed, &ycocg_typed);
        ALWAN_MEMCPY(&rgb_out, &rgb_out_typed, sizeof(alwan_vec3));
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
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_colors[i * 3 + 0], test_xyz_colors[i * 3 + 1], test_xyz_colors[i * 3 + 2]}};
        alwan_vec3 ucs_expected = {{ucs_from_xyz[i * 3 + 0], ucs_from_xyz[i * 3 + 1], ucs_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_ucs_roundtrip[i * 3 + 0], xyz_from_ucs_roundtrip[i * 3 + 1], xyz_from_ucs_roundtrip[i * 3 + 2]}};

        /* XYZ -> UCS */
        alwan_ucs ucs;
        alwan_xyz xyz_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_vec3));
        alwan_xyz_to_ucs(&ucs, &xyz_typed);
        alwan_vec3 ucs_vec;
        ALWAN_MEMCPY(&ucs_vec, &ucs, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&ucs_vec, &ucs_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->UCS mismatch");

        /* UCS -> XYZ */
        alwan_xyz xyz_out;
        alwan_ucs_to_xyz(&xyz_out, &ucs);
        alwan_vec3 xyz_out_vec;
        ALWAN_MEMCPY(&xyz_out_vec, &xyz_out, sizeof(alwan_vec3));
        alwan_scalar diff_inverse = vec3_max_diff(&xyz_out_vec, &xyz_expected);
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
    static alwan_scalar const test_xyz_hdr[] = {
#include "reference_values/test_xyz_hdr.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(test_xyz_hdr) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_hdr[i * 3 + 0], test_xyz_hdr[i * 3 + 1], test_xyz_hdr[i * 3 + 2]}};
        alwan_vec3 hdr_lab_expected = {{hdr_lab_from_xyz[i * 3 + 0], hdr_lab_from_xyz[i * 3 + 1], hdr_lab_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_hdr_lab_roundtrip[i * 3 + 0], xyz_from_hdr_lab_roundtrip[i * 3 + 1], xyz_from_hdr_lab_roundtrip[i * 3 + 2]}};

        /* XYZ -> hdr-CIELAB */
        alwan_vec3 hdr_lab;
        alwan_xyz xyz_typed;
        alwan_lab hdr_lab_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_vec3));
        alwan_xyz_to_hdr_cielab(&hdr_lab_typed, &xyz_typed);
        ALWAN_MEMCPY(&hdr_lab, &hdr_lab_typed, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&hdr_lab, &hdr_lab_expected);
        if (diff_forward >= tolerance && i == 0) {
            printf("  hdr-CIELAB test %d FAIL:\n", i);
            printf("    Expected: [%.10f, %.10f, %.10f]\n", hdr_lab_expected.v[0], hdr_lab_expected.v[1], hdr_lab_expected.v[2]);
            printf("    Got:      [%.10f, %.10f, %.10f]\n", hdr_lab.v[0], hdr_lab.v[1], hdr_lab.v[2]);
            printf("    Diff: %.2e\n", diff_forward);
        }
        TEST_ASSERT(diff_forward < tolerance, "XYZ->hdr-CIELAB mismatch");

        /* hdr-CIELAB -> XYZ */
        alwan_vec3 xyz_out;
        alwan_xyz xyz_out_typed;
        alwan_hdr_cielab_to_xyz(&xyz_out_typed, &hdr_lab_typed);
        ALWAN_MEMCPY(&xyz_out, &xyz_out_typed, sizeof(alwan_vec3));
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
    static alwan_scalar const test_xyz_hdr[] = {
#include "reference_values/test_xyz_hdr.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(test_xyz_hdr) / (3 * sizeof(alwan_scalar));
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_hdr[i * 3 + 0], test_xyz_hdr[i * 3 + 1], test_xyz_hdr[i * 3 + 2]}};
        alwan_vec3 hdr_ipt_expected = {{hdr_ipt_from_xyz[i * 3 + 0], hdr_ipt_from_xyz[i * 3 + 1], hdr_ipt_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_hdr_ipt_roundtrip[i * 3 + 0], xyz_from_hdr_ipt_roundtrip[i * 3 + 1], xyz_from_hdr_ipt_roundtrip[i * 3 + 2]}};

        /* XYZ -> hdr-IPT */
        alwan_vec3 hdr_ipt;
        alwan_xyz xyz_typed;
        alwan_ipt hdr_ipt_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_vec3));
        alwan_xyz_to_hdr_ipt(&hdr_ipt_typed, &xyz_typed);
        ALWAN_MEMCPY(&hdr_ipt, &hdr_ipt_typed, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&hdr_ipt, &hdr_ipt_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->hdr-IPT mismatch");

        /* hdr-IPT -> XYZ */
        alwan_vec3 xyz_out;
        alwan_xyz xyz_out_typed;
        alwan_hdr_ipt_to_xyz(&xyz_out_typed, &hdr_ipt_typed);
        ALWAN_MEMCPY(&xyz_out, &xyz_out_typed, sizeof(alwan_vec3));
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
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_colors[i * 3 + 0], test_xyz_colors[i * 3 + 1], test_xyz_colors[i * 3 + 2]}};
        alwan_vec3 igpgtg_expected = {{igpgtg_from_xyz[i * 3 + 0], igpgtg_from_xyz[i * 3 + 1], igpgtg_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_igpgtg_roundtrip[i * 3 + 0], xyz_from_igpgtg_roundtrip[i * 3 + 1], xyz_from_igpgtg_roundtrip[i * 3 + 2]}};

        /* XYZ -> IgPgTg */
        alwan_vec3 igpgtg;
        alwan_xyz xyz_typed;
        alwan_igpgtg igpgtg_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_vec3));
        alwan_xyz_to_igpgtg(&igpgtg_typed, &xyz_typed);
        ALWAN_MEMCPY(&igpgtg, &igpgtg_typed, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&igpgtg, &igpgtg_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->IgPgTg mismatch");

        /* IgPgTg -> XYZ */
        alwan_vec3 xyz_out;
        alwan_xyz xyz_out_typed;
        alwan_igpgtg_to_xyz(&xyz_out_typed, &igpgtg_typed);
        ALWAN_MEMCPY(&xyz_out, &xyz_out_typed, sizeof(alwan_vec3));
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
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 xyz = {{test_xyz_colors[i * 3 + 0], test_xyz_colors[i * 3 + 1], test_xyz_colors[i * 3 + 2]}};
        alwan_vec3 icacb_expected = {{icacb_from_xyz[i * 3 + 0], icacb_from_xyz[i * 3 + 1], icacb_from_xyz[i * 3 + 2]}};
        alwan_vec3 xyz_expected = {{xyz_from_icacb_roundtrip[i * 3 + 0], xyz_from_icacb_roundtrip[i * 3 + 1], xyz_from_icacb_roundtrip[i * 3 + 2]}};

        /* XYZ -> ICaCb */
        alwan_vec3 icacb;
        alwan_xyz xyz_typed;
        alwan_icacb icacb_typed;
        ALWAN_MEMCPY(&xyz_typed, &xyz, sizeof(alwan_vec3));
        alwan_xyz_to_icacb(&icacb_typed, &xyz_typed);
        ALWAN_MEMCPY(&icacb, &icacb_typed, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&icacb, &icacb_expected);
        TEST_ASSERT(diff_forward < tolerance, "XYZ->ICaCb mismatch");

        /* ICaCb -> XYZ */
        alwan_vec3 xyz_out;
        alwan_xyz xyz_out_typed;
        alwan_icacb_to_xyz(&xyz_out_typed, &icacb_typed);
        ALWAN_MEMCPY(&xyz_out, &xyz_out_typed, sizeof(alwan_vec3));
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
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 rgb = {{test_rgb[i][0], test_rgb[i][1], test_rgb[i][2]}};
        alwan_vec3 prismatic_expected = {{prismatic_from_rgb[i * 3 + 0], prismatic_from_rgb[i * 3 + 1], prismatic_from_rgb[i * 3 + 2]}};
        alwan_vec3 rgb_expected = {{rgb_from_prismatic_roundtrip[i * 3 + 0], rgb_from_prismatic_roundtrip[i * 3 + 1], rgb_from_prismatic_roundtrip[i * 3 + 2]}};

        /* RGB -> Prismatic */
        alwan_prismatic prismatic;
        alwan_rgb rgb_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3));
        alwan_rgb_to_prismatic(&prismatic, &rgb_typed);
        alwan_vec3 prismatic_vec;
        ALWAN_MEMCPY(&prismatic_vec, &prismatic, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&prismatic_vec, &prismatic_expected);
        TEST_ASSERT(diff_forward < tolerance, "RGB->Prismatic mismatch");

        /* Prismatic -> RGB */
        alwan_vec3 rgb_out;
        alwan_rgb rgb_out_typed;
        alwan_prismatic_to_rgb(&rgb_out_typed, &prismatic);
        ALWAN_MEMCPY(&rgb_out, &rgb_out_typed, sizeof(alwan_vec3));
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
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 rgb = {{test_rgb[i][0], test_rgb[i][1], test_rgb[i][2]}};
        alwan_vec3 hcl_expected = {{hcl_from_rgb[i * 3 + 0], hcl_from_rgb[i * 3 + 1], hcl_from_rgb[i * 3 + 2]}};
        alwan_vec3 rgb_expected = {{rgb_from_hcl_roundtrip[i * 3 + 0], rgb_from_hcl_roundtrip[i * 3 + 1], rgb_from_hcl_roundtrip[i * 3 + 2]}};

        /* RGB -> HCL */
        alwan_hcl hcl;
        alwan_rgb rgb_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3));
        alwan_rgb_to_hcl(&hcl, &rgb_typed);
        alwan_vec3 hcl_vec;
        ALWAN_MEMCPY(&hcl_vec, &hcl, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&hcl_vec, &hcl_expected);
        if (diff_forward >= tolerance) {
            printf("  HCL test %d FAIL: RGB=[%.3f, %.3f, %.3f]\n", i, rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("    Expected: [%.10f, %.10f, %.10f]\n", hcl_expected.v[0], hcl_expected.v[1], hcl_expected.v[2]);
            printf("    Got:      [%.10f, %.10f, %.10f]\n", hcl.H, hcl.C, hcl.L);
            printf("    Diff: %.2e\n", diff_forward);
        }
        TEST_ASSERT(diff_forward < tolerance, "RGB->HCL mismatch");

        /* HCL -> RGB */
        alwan_vec3 rgb_out;
        alwan_rgb rgb_out_typed;
        alwan_hcl_to_rgb(&rgb_out_typed, &hcl);
        ALWAN_MEMCPY(&rgb_out, &rgb_out_typed, sizeof(alwan_vec3));
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
    alwan_scalar const tolerance = TEST_TOLERANCE;

    for (int i = 0; i < num_tests; i++) {
        alwan_vec3 rgb = {{test_rgb[i][0], test_rgb[i][1], test_rgb[i][2]}};
        alwan_vec3 ihls_expected = {{ihls_from_rgb[i * 3 + 0], ihls_from_rgb[i * 3 + 1], ihls_from_rgb[i * 3 + 2]}};
        alwan_vec3 rgb_expected = {{rgb_from_ihls_roundtrip[i * 3 + 0], rgb_from_ihls_roundtrip[i * 3 + 1], rgb_from_ihls_roundtrip[i * 3 + 2]}};

        /* RGB -> IHLS */
        alwan_ihls ihls;
        alwan_rgb rgb_typed;
        ALWAN_MEMCPY(&rgb_typed, &rgb, sizeof(alwan_vec3));
        alwan_rgb_to_ihls(&ihls, &rgb_typed);
        alwan_vec3 ihls_vec;
        ALWAN_MEMCPY(&ihls_vec, &ihls, sizeof(alwan_vec3));
        alwan_scalar diff_forward = vec3_max_diff(&ihls_vec, &ihls_expected);
        if (diff_forward >= tolerance) {
            printf("  IHLS test %d FAIL: RGB=[%.3f, %.3f, %.3f]\n", i, rgb.v[0], rgb.v[1], rgb.v[2]);
            printf("    Expected: [%.10f, %.10f, %.10f]\n", ihls_expected.v[0], ihls_expected.v[1], ihls_expected.v[2]);
            printf("    Got:      [%.10f, %.10f, %.10f]\n", ihls.H, ihls.L, ihls.S);
            printf("    Diff: %.2e\n", diff_forward);
        }
        TEST_ASSERT(diff_forward < tolerance, "RGB->IHLS mismatch");

        /* IHLS -> RGB */
        alwan_vec3 rgb_out;
        alwan_rgb rgb_out_typed;
        alwan_ihls_to_rgb(&rgb_out_typed, &ihls);
        ALWAN_MEMCPY(&rgb_out, &rgb_out_typed, sizeof(alwan_vec3));
        alwan_scalar diff_inverse = vec3_max_diff(&rgb_out, &rgb_expected);
        if (diff_inverse >= tolerance) {
            printf("  IHLS->RGB test %d FAIL: IHLS=[%.3f, %.3f, %.3f]\n", i, ihls.H, ihls.L, ihls.S);
            printf("    Expected RGB: [%.10f, %.10f, %.10f]\n", rgb_expected.v[0], rgb_expected.v[1], rgb_expected.v[2]);
            printf("    Got RGB:      [%.10f, %.10f, %.10f]\n", rgb_out.v[0], rgb_out.v[1], rgb_out.v[2]);
            printf("    Diff: %.2e\n", diff_inverse);
        }
        TEST_ASSERT(diff_inverse < tolerance, "IHLS->RGB roundtrip mismatch");
    }

    TEST_PASS("IHLS roundtrip");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_16_extended_colorspaces_main(void) {
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
