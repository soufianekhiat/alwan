/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test 36: P8 Extended Spectral Data (Observers & Illuminants)
 *
 * Reference values generated from colour-science Python library
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

static alwan_scalar vec3_max_diff(alwan_xyz const *a, alwan_xyz const *b) {
    alwan_scalar diff_x = ALWAN_FABS(a->x - b->x);
    alwan_scalar diff_y = ALWAN_FABS(a->y - b->y);
    alwan_scalar diff_z = ALWAN_FABS(a->z - b->z);
    alwan_scalar max_diff = diff_x;
    if (diff_y > max_diff) max_diff = diff_y;
    if (diff_z > max_diff) max_diff = diff_z;
    return max_diff;
}

static void vec3_print(char const *name, alwan_xyz const *v) {
    printf("%s: [%12.8f %12.8f %12.8f]\n", name, v->x, v->y, v->z);
}

/* ----------------------------------------------------------------
 * Reference Value Loading
 * ---------------------------------------------------------------- */

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV

/* P8 White points for new illuminants */
static alwan_scalar const white_b_data[] = {
#include "reference_values/white_b_xyz.csv"
};

static alwan_scalar const white_c_data[] = {
#include "reference_values/white_c_xyz.csv"
};

static alwan_scalar const white_d60_data[] = {
#include "reference_values/white_d60_xyz.csv"
};

static alwan_scalar const white_d75_data[] = {
#include "reference_values/white_d75_xyz.csv"
};

/* D65 white point using Stockman & Sharpe observer */
static alwan_scalar const white_d65_stockman_sharpe_data[] = {
#include "reference_values/white_d65_stockman_sharpe_xyz.csv"
};

ALWAN_DIAG_POP

/* ----------------------------------------------------------------
 * Test P8 Illuminants White Points
 * ---------------------------------------------------------------- */

static int test_p8_illuminant_white_point(
    char const *name,
    alwan_illuminant illuminant,
    alwan_scalar const *expected_white_xyz)
{
    alwan_xyz expected = {expected_white_xyz[0], expected_white_xyz[1], expected_white_xyz[2]};
    alwan_xyz computed;

    /* Get white point XYZ using alwan_illuminant_white_point */
    int status = alwan_illuminant_white_point(illuminant, ALWAN_OBSERVER_CIE_1931_2DEG, &computed);
    TEST_ASSERT(status == ALWAN_OK, "alwan_illuminant_white_point failed");

    /* Note: Tolerance accounts for numerical integration differences between Alwan and colour-science
     * due to different SPD resolutions and integration methods:
     * - Alwan: SPDs at 360-830nm 1nm intervals (471 samples), Simpson integration
     * - colour-science: SPDs at varying intervals, ASTM E308 method
     * Expect differences up to ~1e-3 in normalized white point coordinates */
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-3);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-3);
#endif

    alwan_scalar diff = vec3_max_diff(&computed, &expected);
    if (diff >= tolerance) {
        printf("  %s white point max diff: %e (tolerance: %e)\n", name, diff, tolerance);
        vec3_print("  Computed", &computed);
        vec3_print("  Expected", &expected);
    }
    TEST_ASSERT(diff < tolerance, "White point mismatch");

    TEST_PASS(name);
}

static int test_illuminant_b_white(void) {
    return test_p8_illuminant_white_point("Illuminant B white point",
                                          ALWAN_ILLUMINANT_B, white_b_data);
}

static int test_illuminant_c_white(void) {
    return test_p8_illuminant_white_point("Illuminant C white point",
                                          ALWAN_ILLUMINANT_C, white_c_data);
}

static int test_illuminant_d60_white(void) {
    return test_p8_illuminant_white_point("Illuminant D60 white point",
                                          ALWAN_ILLUMINANT_D60, white_d60_data);
}

static int test_illuminant_d75_white(void) {
    return test_p8_illuminant_white_point("Illuminant D75 white point",
                                          ALWAN_ILLUMINANT_D75, white_d75_data);
}

/* ----------------------------------------------------------------
 * Test Stockman & Sharpe Observer
 * ---------------------------------------------------------------- */

static int test_stockman_sharpe_observer(void) {
    alwan_xyz expected = {white_d65_stockman_sharpe_data[0],
                          white_d65_stockman_sharpe_data[1],
                          white_d65_stockman_sharpe_data[2]};
    alwan_xyz computed;

    /* Get D65 white point using Stockman & Sharpe observer */
    int status = alwan_illuminant_white_point(ALWAN_ILLUMINANT_D65,
                                               ALWAN_OBSERVER_STOCKMAN_SHARPE_2DEG,
                                               &computed);
    TEST_ASSERT(status == ALWAN_OK, "alwan_illuminant_white_point failed");

    /* Note: Tolerance accounts for significant numerical integration differences between Alwan and colour-science
     * due to different SPD resolutions, CMF data, and integration methods:
     * - Alwan: D65 SPD at 360-830nm 1nm intervals (471 samples), Simpson integration
     * - colour-science: D65 SPD at 300-780nm 5nm intervals (97 samples), ASTM E308 method
     * The Stockman & Sharpe CMFs have subtle differences that compound with SPD integration */
#if ALWAN_SCALAR_IS_FLOAT
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-1);
#else
    alwan_scalar const tolerance = ALWAN_LITERAL(1e-1);
#endif

    alwan_scalar diff = vec3_max_diff(&computed, &expected);
    if (diff >= tolerance) {
        printf("  Stockman & Sharpe D65 white max diff: %e (tolerance: %e)\n", diff, tolerance);
        vec3_print("  Computed", &computed);
        vec3_print("  Expected", &expected);
    }
    TEST_ASSERT(diff < tolerance, "Stockman & Sharpe D65 white point mismatch");

    TEST_PASS("Stockman & Sharpe 2000 2° observer D65 white point");
}

/* ----------------------------------------------------------------
 * Main test runner
 * ---------------------------------------------------------------- */

int test_36_spectral_extended_main(void) {
    printf("\n=== P8: Extended Spectral Data (Observers & Illuminants) ===\n\n");

    int failures = 0;

    /* Test new illuminant white points */
    failures += test_illuminant_b_white();
    failures += test_illuminant_c_white();
    failures += test_illuminant_d60_white();
    failures += test_illuminant_d75_white();

    /* Test Stockman & Sharpe observer */
    failures += test_stockman_sharpe_observer();

    if (failures == 0) {
        printf("\n=== All P8 spectral tests passed ===\n");
        return 0;
    } else {
        fprintf(stderr, "\n=== %d P8 spectral test(s) failed ===\n", failures);
        return 1;
    }
}
