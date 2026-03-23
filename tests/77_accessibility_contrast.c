/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Test suite 77: Accessibility Contrast Metrics
 * WCAG 2.x Contrast Ratio, APCA / SAPC
 */

#include "test_common.h"
#include "core/alwan_vision_core.h"

/* ----------------------------------------------------------------
 * WCAG 2.x Contrast Ratio
 * ---------------------------------------------------------------- */

static int test_wcag_contrast_ratio(void) {
    /* Black on white: max contrast = 21:1 */
    {
        alwan_scalar cr = alwan_wcag_contrast_ratio_v(
            ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0));
        TEST_ASSERT_NEAR(cr, ALWAN_LITERAL(21.0), ALWAN_LITERAL(1e-6),
                         "wcag: black on white = 21:1");
    }

    /* White on white: min contrast = 1:1 */
    {
        alwan_scalar cr = alwan_wcag_contrast_ratio_v(
            ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0));
        TEST_ASSERT_NEAR(cr, ALWAN_LITERAL(1.0), ALWAN_LITERAL(1e-6),
                         "wcag: same color = 1:1");
    }

    /* Order independence: lighter is automatically detected */
    {
        alwan_scalar cr1 = alwan_wcag_contrast_ratio_v(
            ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.8));
        alwan_scalar cr2 = alwan_wcag_contrast_ratio_v(
            ALWAN_LITERAL(0.8), ALWAN_LITERAL(0.2));
        TEST_ASSERT_NEAR(cr1, cr2, ALWAN_LITERAL(1e-10),
                         "wcag: order independent");
    }

    /* Known value: 0.2 vs 0.0 => (0.2 + 0.05) / (0.0 + 0.05) = 5.0 */
    {
        alwan_scalar cr = alwan_wcag_contrast_ratio_v(
            ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.0));
        TEST_ASSERT_NEAR(cr, ALWAN_LITERAL(5.0), ALWAN_LITERAL(1e-10),
                         "wcag: 0.2 vs 0.0 = 5:1");
    }

    /* AA compliance threshold: >= 4.5:1 */
    {
        /* Y = 0.1791 for mid-gray on white should give ~4.5:1 */
        alwan_scalar cr = alwan_wcag_contrast_ratio_v(
            ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.1791));
        TEST_ASSERT(cr >= ALWAN_LITERAL(4.5), "wcag: AA threshold check");
    }

    /* API wrapper */
    {
        alwan_scalar cr;
        int status = alwan_wcag_contrast_ratio(&cr, ALWAN_LITERAL(1.0),
                                                ALWAN_LITERAL(0.0));
        TEST_ASSERT(status == ALWAN_OK, "wcag api status");
        TEST_ASSERT_NEAR(cr, ALWAN_LITERAL(21.0), ALWAN_LITERAL(1e-6),
                         "wcag api value");
    }

    TEST_PASS("WCAG 2.x contrast ratio");
}

/* ----------------------------------------------------------------
 * APCA / SAPC Contrast
 * ---------------------------------------------------------------- */

static int test_apca_contrast(void) {
    /* Black text on white background: high positive contrast */
    {
        alwan_rgb text = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_rgb bg   = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
        alwan_scalar Lc = alwan_apca_contrast_v(text, bg);

        /* Black on white should give a large positive Lc (around 106) */
        TEST_ASSERT(Lc > ALWAN_LITERAL(90.0),
                    "apca: black on white should give high positive contrast");
    }

    /* White text on black background: large negative contrast */
    {
        alwan_rgb text = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
        alwan_rgb bg   = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_scalar Lc = alwan_apca_contrast_v(text, bg);

        /* White on black should give a large negative Lc */
        TEST_ASSERT(Lc < ALWAN_LITERAL(-90.0),
                    "apca: white on black should give large negative contrast");
    }

    /* Same color: zero or near-zero contrast */
    {
        alwan_rgb mid = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
        alwan_scalar Lc = alwan_apca_contrast_v(mid, mid);

        TEST_ASSERT(ALWAN_ABS(Lc) < ALWAN_LITERAL(1.0),
                    "apca: same color should give near-zero contrast");
    }

    /* Polarity: dark-on-light should be positive, light-on-dark negative */
    {
        alwan_rgb dark  = {ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.2), ALWAN_LITERAL(0.2)};
        alwan_rgb light = {ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.9), ALWAN_LITERAL(0.9)};

        alwan_scalar Lc_dark_on_light = alwan_apca_contrast_v(dark, light);
        alwan_scalar Lc_light_on_dark = alwan_apca_contrast_v(light, dark);

        TEST_ASSERT(Lc_dark_on_light > ALWAN_LITERAL(0.0),
                    "apca: dark text on light bg = positive");
        TEST_ASSERT(Lc_light_on_dark < ALWAN_LITERAL(0.0),
                    "apca: light text on dark bg = negative");
    }

    /* Monotonicity: increasing contrast with increasing luminance difference */
    {
        alwan_rgb white = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
        alwan_rgb gray1 = {ALWAN_LITERAL(0.7), ALWAN_LITERAL(0.7), ALWAN_LITERAL(0.7)};
        alwan_rgb gray2 = {ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.3)};
        alwan_rgb black = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};

        alwan_scalar Lc1 = alwan_apca_contrast_v(gray1, white);
        alwan_scalar Lc2 = alwan_apca_contrast_v(gray2, white);
        alwan_scalar Lc3 = alwan_apca_contrast_v(black, white);

        TEST_ASSERT(Lc2 > Lc1, "apca: more contrast with darker text");
        TEST_ASSERT(Lc3 > Lc2, "apca: black text has most contrast");
    }

    /* API wrapper */
    {
        alwan_rgb text = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
        alwan_rgb bg   = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(1.0)};
        alwan_scalar Lc;
        int status = alwan_apca_contrast(&Lc, &text, &bg);
        TEST_ASSERT(status == ALWAN_OK, "apca api status");
        TEST_ASSERT(Lc > ALWAN_LITERAL(90.0), "apca api value");
    }

    /* NULL checks */
    {
        alwan_rgb c = {ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5), ALWAN_LITERAL(0.5)};
        alwan_scalar Lc;
        TEST_ASSERT(alwan_apca_contrast(NULL, &c, &c) == ALWAN_E_INVALID,
                    "apca: null out");
        TEST_ASSERT(alwan_apca_contrast(&Lc, NULL, &c) == ALWAN_E_INVALID,
                    "apca: null text");
        TEST_ASSERT(alwan_apca_contrast(&Lc, &c, NULL) == ALWAN_E_INVALID,
                    "apca: null bg");
    }

    TEST_PASS("APCA / SAPC contrast");
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

int test_77_accessibility_contrast_main(void) {
    int failures = 0;

    failures += test_wcag_contrast_ratio();
    failures += test_apca_contrast();

    if (failures == 0) {
        printf("\n=== All accessibility contrast tests passed ===\n");
        return 0;
    } else {
        printf("\n=== %d test(s) failed ===\n", failures);
        return 1;
    }
}
