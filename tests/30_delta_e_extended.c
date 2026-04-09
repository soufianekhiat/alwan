/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 30: Extended color difference (dE) metrics (P3)
 */

#include "test_common.h"

/* ----------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------- */

static int test_delta_e_itp(void) {
    /* Load ICtCp test pairs and expected dE ITP values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const ictcp1_data[] = {
#include "reference_values/delta_e_itp_ictcp1.csv"
    };
    static alwan_f64 const ictcp2_data[] = {
#include "reference_values/delta_e_itp_ictcp2.csv"
    };
    static alwan_f64 const de_itp_data[] = {
#include "reference_values/delta_e_itp.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(ictcp1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_ictcp_f64 ictcp1 = {ictcp1_data[i * 3 + 0], ictcp1_data[i * 3 + 1], ictcp1_data[i * 3 + 2]};
        alwan_ictcp_f64 ictcp2 = {ictcp2_data[i * 3 + 0], ictcp2_data[i * 3 + 1], ictcp2_data[i * 3 + 2]};
        alwan_f64 expected = de_itp_data[i];

        alwan_delta_e_itp_params itp_p; itp_p.scalar_factor = ALWAN_LITERAL(720.0);
        alwan_f64 result = alwan_delta_e_itp_f64(&ictcp1, &ictcp2, &itp_p);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE ITP mismatch");
    }

    TEST_PASS("dE ITP (BT.2100 HDR)");
}

static int test_delta_e_din99(void) {
    /* Load DIN99 test pairs and expected dE DIN99 values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const din99_1_data[] = {
#include "reference_values/delta_e_din99_1.csv"
    };
    static alwan_f64 const din99_2_data[] = {
#include "reference_values/delta_e_din99_2.csv"
    };
    static alwan_f64 const de_din99_data[] = {
#include "reference_values/delta_e_din99.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(din99_1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_din99_f64 din99_1 = {din99_1_data[i * 3 + 0], din99_1_data[i * 3 + 1], din99_1_data[i * 3 + 2]};
        alwan_din99_f64 din99_2 = {din99_2_data[i * 3 + 0], din99_2_data[i * 3 + 1], din99_2_data[i * 3 + 2]};
        alwan_f64 expected = de_din99_data[i];

        alwan_f64 result = alwan_delta_e_din99_f64(&din99_1, &din99_2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE DIN99 mismatch");
    }

    TEST_PASS("dE DIN99");
}

static int test_delta_e_zcam(void) {
    /* Load Jzazbz test pairs and expected dE ZCAM values */
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const jzazbz1_data[] = {
#include "reference_values/delta_e_zcam_jzazbz1.csv"
    };
    static alwan_f64 const jzazbz2_data[] = {
#include "reference_values/delta_e_zcam_jzazbz2.csv"
    };
    static alwan_f64 const de_zcam_data[] = {
#include "reference_values/delta_e_zcam.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(jzazbz1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_jzazbz_f64 jzazbz1 = {jzazbz1_data[i * 3 + 0], jzazbz1_data[i * 3 + 1], jzazbz1_data[i * 3 + 2]};
        alwan_jzazbz_f64 jzazbz2 = {jzazbz2_data[i * 3 + 0], jzazbz2_data[i * 3 + 1], jzazbz2_data[i * 3 + 2]};
        alwan_f64 expected = de_zcam_data[i];

        alwan_f64 result = alwan_delta_e_zcam_f64(&jzazbz1, &jzazbz2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE ZCAM mismatch");
    }

    TEST_PASS("dE ZCAM (Jzazbz UCS)");
}

static int test_delta_e_cam02_lcd(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_cam_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_cam_lab2.csv"
    };
    static alwan_f64 const de_cam02_lcd_data[] = {
#include "reference_values/delta_e_cam02_lcd.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_cam_jab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_cam_jab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de_cam02_lcd_data[i];

        alwan_f64 result = alwan_delta_e_cam02_lcd_f64(&lab1, &lab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE CAM02-LCD mismatch");
    }

    TEST_PASS("dE CAM02-LCD");
}

static int test_delta_e_cam02_scd(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_cam_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_cam_lab2.csv"
    };
    static alwan_f64 const de_cam02_scd_data[] = {
#include "reference_values/delta_e_cam02_scd.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_cam_jab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_cam_jab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de_cam02_scd_data[i];

        alwan_f64 result = alwan_delta_e_cam02_scd_f64(&lab1, &lab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE CAM02-SCD mismatch");
    }

    TEST_PASS("dE CAM02-SCD");
}

static int test_delta_e_cam16_lcd(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_cam_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_cam_lab2.csv"
    };
    static alwan_f64 const de_cam16_lcd_data[] = {
#include "reference_values/delta_e_cam16_lcd.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_cam_jab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_cam_jab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de_cam16_lcd_data[i];

        alwan_f64 result = alwan_delta_e_cam16_lcd_f64(&lab1, &lab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE CAM16-LCD mismatch");
    }

    TEST_PASS("dE CAM16-LCD");
}

static int test_delta_e_cam16_scd(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const lab1_data[] = {
#include "reference_values/delta_e_cam_lab1.csv"
    };
    static alwan_f64 const lab2_data[] = {
#include "reference_values/delta_e_cam_lab2.csv"
    };
    static alwan_f64 const de_cam16_scd_data[] = {
#include "reference_values/delta_e_cam16_scd.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(lab1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_cam_jab_f64 lab1 = {lab1_data[i * 3 + 0], lab1_data[i * 3 + 1], lab1_data[i * 3 + 2]};
        alwan_cam_jab_f64 lab2 = {lab2_data[i * 3 + 0], lab2_data[i * 3 + 1], lab2_data[i * 3 + 2]};
        alwan_f64 expected = de_cam16_scd_data[i];

        alwan_f64 result = alwan_delta_e_cam16_scd_f64(&lab1, &lab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE CAM16-SCD mismatch");
    }

    TEST_PASS("dE CAM16-SCD");
}

static int test_delta_e_cam02_ucs(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const jab1_data[] = {
#include "reference_values/delta_e_cam_ucs_jab1.csv"
    };
    static alwan_f64 const jab2_data[] = {
#include "reference_values/delta_e_cam_ucs_jab2.csv"
    };
    static alwan_f64 const de_cam02_ucs_data[] = {
#include "reference_values/delta_e_cam02_ucs.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(jab1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_cam_jab_f64 jab1 = {jab1_data[i * 3 + 0], jab1_data[i * 3 + 1], jab1_data[i * 3 + 2]};
        alwan_cam_jab_f64 jab2 = {jab2_data[i * 3 + 0], jab2_data[i * 3 + 1], jab2_data[i * 3 + 2]};
        alwan_f64 expected = de_cam02_ucs_data[i];

        alwan_f64 result = alwan_delta_e_cam02_ucs_f64(&jab1, &jab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE CAM02-UCS mismatch");
    }

    TEST_PASS("dE CAM02-UCS");
}

static int test_delta_e_cam16_ucs(void) {
    ALWAN_DIAG_PUSH
    ALWAN_DIAG_DISABLE_FLOAT_CONV
    static alwan_f64 const jab1_data[] = {
#include "reference_values/delta_e_cam_ucs_jab1.csv"
    };
    static alwan_f64 const jab2_data[] = {
#include "reference_values/delta_e_cam_ucs_jab2.csv"
    };
    static alwan_f64 const de_cam16_ucs_data[] = {
#include "reference_values/delta_e_cam16_ucs.csv"
    };
    ALWAN_DIAG_POP

    int const num_tests = sizeof(jab1_data) / (3 * sizeof(alwan_f64));

    for (int i = 0; i < num_tests; i++) {
        alwan_cam_jab_f64 jab1 = {jab1_data[i * 3 + 0], jab1_data[i * 3 + 1], jab1_data[i * 3 + 2]};
        alwan_cam_jab_f64 jab2 = {jab2_data[i * 3 + 0], jab2_data[i * 3 + 1], jab2_data[i * 3 + 2]};
        alwan_f64 expected = de_cam16_ucs_data[i];

        alwan_f64 result = alwan_delta_e_cam16_ucs_f64(&jab1, &jab2);
        alwan_f64 diff = ALWAN_ABS(result - expected);

        TEST_ASSERT(diff < ALWAN_TEST_TOLERANCE, "dE CAM16-UCS mismatch");
    }

    TEST_PASS("dE CAM16-UCS");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_30_delta_e_extended_main(void) {
    int failures = 0;

    failures += test_delta_e_itp();
    failures += test_delta_e_din99();
    failures += test_delta_e_zcam();
    failures += test_delta_e_cam02_lcd();
    failures += test_delta_e_cam02_scd();
    failures += test_delta_e_cam16_lcd();
    failures += test_delta_e_cam16_scd();
    failures += test_delta_e_cam02_ucs();
    failures += test_delta_e_cam16_ucs();

    if (failures == 0) {
        printf("\n=== All extended dE metric tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
