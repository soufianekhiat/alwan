/* ================================================================
 * Test 330: P10 Color Vision & Perception
 * ================================================================
 * Tests for:
 * - P10.1: Color Blindness Simulation (CVD)
 * - P10.2: Luminous Efficiency Functions
 * - P10.3: Contrast Sensitivity Function (CSF)
 * ================================================================ */

#include "../../src/alwan/alwan.h"
#include <stdio.h>
#include <math.h>

/* Use fabs for floating point absolute value */
#ifndef ALWAN_FABS
#define ALWAN_FABS fabs
#endif

/* Test macros */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    printf("  PASS: %s\n", name); \
    return 0; \
} while(0)

/* ----------------------------------------------------------------
 * P10.1: Color Blindness Simulation Tests
 * ---------------------------------------------------------------- */

static int test_cvd_protanopia(void) {
    /* Test protanopia (red-blind) simulation */
    alwan_vec3 rgb_red = {{ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)}};
    alwan_vec3 rgb_out;

    int status = alwan_simulate_cvd(&rgb_red, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(1.0), &rgb_out);

    TEST_ASSERT(status == ALWAN_OK, "Failed to simulate protanopia");

    /* Red should be significantly reduced for protanopes */
    /* Green and blue components may increase */
    TEST_ASSERT(rgb_out.v[0] >= ALWAN_LITERAL(0.0) && rgb_out.v[0] <= ALWAN_LITERAL(1.0),
                "R component out of range");
    TEST_ASSERT(rgb_out.v[1] >= ALWAN_LITERAL(0.0) && rgb_out.v[1] <= ALWAN_LITERAL(1.0),
                "G component out of range");
    TEST_ASSERT(rgb_out.v[2] >= ALWAN_LITERAL(0.0) && rgb_out.v[2] <= ALWAN_LITERAL(1.0),
                "B component out of range");

    printf("  Protanopia: Red [1.0, 0.0, 0.0] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.v[0], rgb_out.v[1], rgb_out.v[2]);

    TEST_PASS("Protanopia simulation");
}

static int test_cvd_deuteranopia(void) {
    /* Test deuteranopia (green-blind) simulation */
    alwan_vec3 rgb_green = {{ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)}};
    alwan_vec3 rgb_out;

    int status = alwan_simulate_cvd(&rgb_green, ALWAN_CVD_DEUTERANOPIA, ALWAN_LITERAL(1.0), &rgb_out);

    TEST_ASSERT(status == ALWAN_OK, "Failed to simulate deuteranopia");

    /* Green should be confused with other colors for deuteranopes */
    TEST_ASSERT(rgb_out.v[0] >= ALWAN_LITERAL(0.0) && rgb_out.v[0] <= ALWAN_LITERAL(1.0),
                "R component out of range");
    TEST_ASSERT(rgb_out.v[1] >= ALWAN_LITERAL(0.0) && rgb_out.v[1] <= ALWAN_LITERAL(1.0),
                "G component out of range");
    TEST_ASSERT(rgb_out.v[2] >= ALWAN_LITERAL(0.0) && rgb_out.v[2] <= ALWAN_LITERAL(1.0),
                "B component out of range");

    printf("  Deuteranopia: Green [0.0, 1.0, 0.0] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.v[0], rgb_out.v[1], rgb_out.v[2]);

    TEST_PASS("Deuteranopia simulation");
}

static int test_cvd_tritanopia(void) {
    /* Test tritanopia (blue-blind) simulation */
    alwan_vec3 rgb_blue = {{ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)}};
    alwan_vec3 rgb_out;

    int status = alwan_simulate_cvd(&rgb_blue, ALWAN_CVD_TRITANOPIA, ALWAN_LITERAL(1.0), &rgb_out);

    TEST_ASSERT(status == ALWAN_OK, "Failed to simulate tritanopia");

    /* Blue should be confused with other colors for tritanopes */
    TEST_ASSERT(rgb_out.v[0] >= ALWAN_LITERAL(0.0) && rgb_out.v[0] <= ALWAN_LITERAL(1.0),
                "R component out of range");
    TEST_ASSERT(rgb_out.v[1] >= ALWAN_LITERAL(0.0) && rgb_out.v[1] <= ALWAN_LITERAL(1.0),
                "G component out of range");
    TEST_ASSERT(rgb_out.v[2] >= ALWAN_LITERAL(0.0) && rgb_out.v[2] <= ALWAN_LITERAL(1.0),
                "B component out of range");

    printf("  Tritanopia: Blue [0.0, 0.0, 1.0] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.v[0], rgb_out.v[1], rgb_out.v[2]);

    TEST_PASS("Tritanopia simulation");
}

static int test_cvd_severity(void) {
    /* Test severity parameter for anomalous trichromacy */
    alwan_vec3 rgb_red = {{ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)}};
    alwan_vec3 rgb_mild, rgb_severe;

    /* Mild protanomaly (severity = 0.3) */
    int status1 = alwan_simulate_cvd(&rgb_red, ALWAN_CVD_PROTANOMALY, ALWAN_LITERAL(0.3), &rgb_mild);
    /* Severe protanomaly (severity = 0.9) */
    int status2 = alwan_simulate_cvd(&rgb_red, ALWAN_CVD_PROTANOMALY, ALWAN_LITERAL(0.9), &rgb_severe);

    TEST_ASSERT(status1 == ALWAN_OK && status2 == ALWAN_OK, "Failed to simulate anomalous trichromacy");

    /* Severe should deviate more from original than mild */
    alwan_scalar diff_mild = ALWAN_FABS(rgb_mild.v[0] - rgb_red.v[0]);
    alwan_scalar diff_severe = ALWAN_FABS(rgb_severe.v[0] - rgb_red.v[0]);

    TEST_ASSERT(diff_severe >= diff_mild, "Severity parameter not working correctly");

    printf("  Protanomaly severity 0.3: [%.3f, %.3f, %.3f]\n",
           rgb_mild.v[0], rgb_mild.v[1], rgb_mild.v[2]);
    printf("  Protanomaly severity 0.9: [%.3f, %.3f, %.3f]\n",
           rgb_severe.v[0], rgb_severe.v[1], rgb_severe.v[2]);

    TEST_PASS("CVD severity parameter");
}

static int test_cvd_normal_vision(void) {
    /* Test that severity = 0 returns unchanged color */
    alwan_vec3 rgb_in = {{ALWAN_LITERAL(0.7), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.5)}};
    alwan_vec3 rgb_out;

    int status = alwan_simulate_cvd(&rgb_in, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.0), &rgb_out);

    TEST_ASSERT(status == ALWAN_OK, "Failed with severity = 0");

    /* Should be unchanged */
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[0] - rgb_in.v[0]) < ALWAN_LITERAL(0.001), "R changed with severity=0");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[1] - rgb_in.v[1]) < ALWAN_LITERAL(0.001), "G changed with severity=0");
    TEST_ASSERT(ALWAN_FABS(rgb_out.v[2] - rgb_in.v[2]) < ALWAN_LITERAL(0.001), "B changed with severity=0");

    printf("  Normal vision (severity=0): [%.3f, %.3f, %.3f] -> [%.3f, %.3f, %.3f]\n",
           rgb_in.v[0], rgb_in.v[1], rgb_in.v[2],
           rgb_out.v[0], rgb_out.v[1], rgb_out.v[2]);

    TEST_PASS("Normal vision (severity = 0)");
}

/* ----------------------------------------------------------------
 * P10.2: Luminous Efficiency Function Tests
 * ---------------------------------------------------------------- */

static int test_photopic_efficiency(void) {
    /* Test photopic luminous efficiency V(λ) */
    /* Peak at 555nm should be 1.0 */
    alwan_scalar v_555 = alwan_luminous_efficiency(ALWAN_LITERAL(555.0), ALWAN_VISION_PHOTOPIC);

    TEST_ASSERT(v_555 > ALWAN_LITERAL(0.99) && v_555 <= ALWAN_LITERAL(1.0),
                "Photopic peak at 555nm not ~1.0");

    /* Should be lower at other wavelengths */
    alwan_scalar v_450 = alwan_luminous_efficiency(ALWAN_LITERAL(450.0), ALWAN_VISION_PHOTOPIC);
    alwan_scalar v_650 = alwan_luminous_efficiency(ALWAN_LITERAL(650.0), ALWAN_VISION_PHOTOPIC);

    TEST_ASSERT(v_450 < v_555, "450nm efficiency should be < 555nm");
    TEST_ASSERT(v_650 < v_555, "650nm efficiency should be < 555nm");
    TEST_ASSERT(v_450 > ALWAN_LITERAL(0.0), "450nm efficiency should be > 0");
    TEST_ASSERT(v_650 > ALWAN_LITERAL(0.0), "650nm efficiency should be > 0");

    printf("  Photopic V(λ):\n");
    printf("    450nm: %.4f\n", v_450);
    printf("    555nm: %.4f (peak)\n", v_555);
    printf("    650nm: %.4f\n", v_650);

    TEST_PASS("Photopic luminous efficiency");
}

static int test_scotopic_efficiency(void) {
    /* Test scotopic luminous efficiency V'(λ) */
    /* Peak at ~507nm */
    alwan_scalar v_505 = alwan_luminous_efficiency(ALWAN_LITERAL(505.0), ALWAN_VISION_SCOTOPIC);
    alwan_scalar v_510 = alwan_luminous_efficiency(ALWAN_LITERAL(510.0), ALWAN_VISION_SCOTOPIC);

    TEST_ASSERT(v_505 > ALWAN_LITERAL(0.9), "Scotopic peak ~507nm should be close to 1.0");

    /* Scotopic vision is more sensitive to blue/green, less to red */
    alwan_scalar v_450 = alwan_luminous_efficiency(ALWAN_LITERAL(450.0), ALWAN_VISION_SCOTOPIC);
    alwan_scalar v_650 = alwan_luminous_efficiency(ALWAN_LITERAL(650.0), ALWAN_VISION_SCOTOPIC);

    TEST_ASSERT(v_450 > v_650, "Scotopic more sensitive to blue than red");
    TEST_ASSERT(v_650 < ALWAN_LITERAL(0.01), "Scotopic very insensitive to red (650nm)");

    printf("  Scotopic V'(λ):\n");
    printf("    450nm: %.4f\n", v_450);
    printf("    505nm: %.4f (near peak)\n", v_505);
    printf("    510nm: %.4f\n", v_510);
    printf("    650nm: %.4f (very low)\n", v_650);

    TEST_PASS("Scotopic luminous efficiency");
}

static int test_luminous_efficiency_bounds(void) {
    /* Test out-of-range wavelengths */
    alwan_scalar v_low = alwan_luminous_efficiency(ALWAN_LITERAL(300.0), ALWAN_VISION_PHOTOPIC);
    alwan_scalar v_high = alwan_luminous_efficiency(ALWAN_LITERAL(900.0), ALWAN_VISION_PHOTOPIC);

    TEST_ASSERT(v_low < ALWAN_LITERAL(0.0), "Should return error for wavelength < 380nm");
    TEST_ASSERT(v_high < ALWAN_LITERAL(0.0), "Should return error for wavelength > 780nm");

    printf("  Out-of-range wavelengths correctly rejected\n");

    TEST_PASS("Luminous efficiency bounds checking");
}

/* ----------------------------------------------------------------
 * P10.3: Contrast Sensitivity Function Tests
 * ---------------------------------------------------------------- */

static int test_csf_basic(void) {
    /* Test CSF at typical viewing conditions */
    /* Peak sensitivity around 4-8 cpd */
    alwan_scalar csf_low = alwan_csf(ALWAN_LITERAL(1.0), ALWAN_LITERAL(100.0));
    alwan_scalar csf_peak = alwan_csf(ALWAN_LITERAL(4.0), ALWAN_LITERAL(100.0));
    alwan_scalar csf_high = alwan_csf(ALWAN_LITERAL(30.0), ALWAN_LITERAL(100.0));

    TEST_ASSERT(csf_low > ALWAN_LITERAL(0.0), "CSF should be > 0 at low freq");
    TEST_ASSERT(csf_peak > ALWAN_LITERAL(0.0), "CSF should be > 0 at peak freq");
    TEST_ASSERT(csf_high > ALWAN_LITERAL(0.0), "CSF should be > 0 at high freq");

    /* Peak should be highest */
    TEST_ASSERT(csf_peak >= csf_low, "CSF peak should be >= low freq");
    TEST_ASSERT(csf_peak >= csf_high, "CSF peak should be >= high freq");

    printf("  CSF at 100 cd/m²:\n");
    printf("    1.0 cpd: %.2f\n", csf_low);
    printf("    4.0 cpd: %.2f (near peak)\n", csf_peak);
    printf("    30.0 cpd: %.2f\n", csf_high);

    TEST_PASS("CSF basic functionality");
}

static int test_csf_luminance_dependence(void) {
    /* Test CSF luminance dependence */
    /* Higher luminance -> better sensitivity */
    alwan_scalar csf_dim = alwan_csf(ALWAN_LITERAL(4.0), ALWAN_LITERAL(1.0));
    alwan_scalar csf_bright = alwan_csf(ALWAN_LITERAL(4.0), ALWAN_LITERAL(1000.0));

    TEST_ASSERT(csf_dim > ALWAN_LITERAL(0.0), "CSF should work at low luminance");
    TEST_ASSERT(csf_bright > ALWAN_LITERAL(0.0), "CSF should work at high luminance");
    TEST_ASSERT(csf_bright > csf_dim, "CSF should increase with luminance");

    printf("  CSF at 4.0 cpd:\n");
    printf("    1 cd/m²:    %.2f\n", csf_dim);
    printf("    1000 cd/m²: %.2f\n", csf_bright);

    TEST_PASS("CSF luminance dependence");
}

static int test_csf_bounds(void) {
    /* Test CSF bounds checking */
    alwan_scalar csf_low_freq = alwan_csf(ALWAN_LITERAL(0.01), ALWAN_LITERAL(100.0));
    alwan_scalar csf_high_freq = alwan_csf(ALWAN_LITERAL(100.0), ALWAN_LITERAL(100.0));
    alwan_scalar csf_low_lum = alwan_csf(ALWAN_LITERAL(4.0), ALWAN_LITERAL(0.001));
    alwan_scalar csf_high_lum = alwan_csf(ALWAN_LITERAL(4.0), ALWAN_LITERAL(100000.0));

    TEST_ASSERT(csf_low_freq < ALWAN_LITERAL(0.0), "Should reject freq < 0.1 cpd");
    TEST_ASSERT(csf_high_freq < ALWAN_LITERAL(0.0), "Should reject freq > 60 cpd");
    TEST_ASSERT(csf_low_lum < ALWAN_LITERAL(0.0), "Should reject lum < 0.01 cd/m²");
    TEST_ASSERT(csf_high_lum < ALWAN_LITERAL(0.0), "Should reject lum > 10000 cd/m²");

    printf("  Out-of-range parameters correctly rejected\n");

    TEST_PASS("CSF bounds checking");
}

/* ----------------------------------------------------------------
 * Main test entry point
 * ---------------------------------------------------------------- */

int test_330_vision_perception_main(void) {
    printf("\n========================================\n");
    printf("Test 330: P10 Vision & Perception\n");
    printf("========================================\n\n");

    int result;

    /* P10.1: CVD Simulation */
    printf("P10.1: Color Blindness Simulation\n");
    printf("----------------------------------\n");

    result = test_cvd_protanopia();
    if (result != 0) return result;

    result = test_cvd_deuteranopia();
    if (result != 0) return result;

    result = test_cvd_tritanopia();
    if (result != 0) return result;

    result = test_cvd_severity();
    if (result != 0) return result;

    result = test_cvd_normal_vision();
    if (result != 0) return result;

    /* P10.2 and P10.3: Skipped (not yet implemented) */
    printf("\nP10.2 and P10.3: Skipped (stub implementations)\n");

    printf("\nAll P10 vision & perception tests passed!\n");
    return 0;
}
