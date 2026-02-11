/* ================================================================
 * Test 41: Color Vision & Perception
 * ================================================================
 * Tests for:
 * - Color Blindness Simulation (CVD)
 * - Luminous Efficiency Functions
 * - Contrast Sensitivity Function (CSF)
 * ================================================================ */

#include "test_common.h"
#include <math.h>

/* ----------------------------------------------------------------
 * Color Blindness Simulation Tests
 * ---------------------------------------------------------------- */

static int test_cvd_protanopia(void) {
    /* Test protanopia (red-blind) simulation */
    alwan_rgb rgb_red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_rgb rgb_out;

    int status = alwan_simulate_cvd(&rgb_out, &rgb_red, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(1.0));

    TEST_ASSERT(status == ALWAN_OK, "Failed to simulate protanopia");

    /* Red should be significantly reduced for protanopes */
    /* Green and blue components may increase */
    TEST_ASSERT(rgb_out.r >= ALWAN_LITERAL(0.0) && rgb_out.r <= ALWAN_LITERAL(1.0),
                "R component out of range");
    TEST_ASSERT(rgb_out.g >= ALWAN_LITERAL(0.0) && rgb_out.g <= ALWAN_LITERAL(1.0),
                "G component out of range");
    TEST_ASSERT(rgb_out.b >= ALWAN_LITERAL(0.0) && rgb_out.b <= ALWAN_LITERAL(1.0),
                "B component out of range");

    printf("  Protanopia: Red [1.0, 0.0, 0.0] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Protanopia simulation");
}

static int test_cvd_deuteranopia(void) {
    /* Test deuteranopia (green-blind) simulation */
    alwan_rgb rgb_green = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0)};
    alwan_rgb rgb_out;

    int status = alwan_simulate_cvd(&rgb_out, &rgb_green, ALWAN_CVD_DEUTERANOPIA, ALWAN_LITERAL(1.0));

    TEST_ASSERT(status == ALWAN_OK, "Failed to simulate deuteranopia");

    /* Green should be confused with other colors for deuteranopes */
    TEST_ASSERT(rgb_out.r >= ALWAN_LITERAL(0.0) && rgb_out.r <= ALWAN_LITERAL(1.0),
                "R component out of range");
    TEST_ASSERT(rgb_out.g >= ALWAN_LITERAL(0.0) && rgb_out.g <= ALWAN_LITERAL(1.0),
                "G component out of range");
    TEST_ASSERT(rgb_out.b >= ALWAN_LITERAL(0.0) && rgb_out.b <= ALWAN_LITERAL(1.0),
                "B component out of range");

    printf("  Deuteranopia: Green [0.0, 1.0, 0.0] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Deuteranopia simulation");
}

static int test_cvd_tritanopia(void) {
    /* Test tritanopia (blue-blind) simulation */
    alwan_rgb rgb_blue = {ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(1.0)};
    alwan_rgb rgb_out;

    int status = alwan_simulate_cvd(&rgb_out, &rgb_blue, ALWAN_CVD_TRITANOPIA, ALWAN_LITERAL(1.0));

    TEST_ASSERT(status == ALWAN_OK, "Failed to simulate tritanopia");

    /* Blue should be confused with other colors for tritanopes */
    TEST_ASSERT(rgb_out.r >= ALWAN_LITERAL(0.0) && rgb_out.r <= ALWAN_LITERAL(1.0),
                "R component out of range");
    TEST_ASSERT(rgb_out.g >= ALWAN_LITERAL(0.0) && rgb_out.g <= ALWAN_LITERAL(1.0),
                "G component out of range");
    TEST_ASSERT(rgb_out.b >= ALWAN_LITERAL(0.0) && rgb_out.b <= ALWAN_LITERAL(1.0),
                "B component out of range");

    printf("  Tritanopia: Blue [0.0, 0.0, 1.0] -> [%.3f, %.3f, %.3f]\n",
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Tritanopia simulation");
}

static int test_cvd_severity(void) {
    /* Test severity parameter for anomalous trichromacy */
    alwan_rgb rgb_red = {ALWAN_LITERAL(1.0), ALWAN_LITERAL(0.0), ALWAN_LITERAL(0.0)};
    alwan_rgb rgb_mild, rgb_severe;

    /* Mild protanomaly (severity = 0.3) */
    int status1 = alwan_simulate_cvd(&rgb_mild, &rgb_red, ALWAN_CVD_PROTANOMALY, ALWAN_LITERAL(0.3));
    /* Severe protanomaly (severity = 0.9) */
    int status2 = alwan_simulate_cvd(&rgb_severe, &rgb_red, ALWAN_CVD_PROTANOMALY, ALWAN_LITERAL(0.9));

    TEST_ASSERT(status1 == ALWAN_OK && status2 == ALWAN_OK, "Failed to simulate anomalous trichromacy");

    /* Severe should deviate more from original than mild */
    alwan_scalar diff_mild = ALWAN_ABS(rgb_mild.r - rgb_red.r);
    alwan_scalar diff_severe = ALWAN_ABS(rgb_severe.r - rgb_red.r);

    TEST_ASSERT(diff_severe >= diff_mild, "Severity parameter not working correctly");

    printf("  Protanomaly severity 0.3: [%.3f, %.3f, %.3f]\n",
           rgb_mild.r, rgb_mild.g, rgb_mild.b);
    printf("  Protanomaly severity 0.9: [%.3f, %.3f, %.3f]\n",
           rgb_severe.r, rgb_severe.g, rgb_severe.b);

    TEST_PASS("CVD severity parameter");
}

static int test_cvd_normal_vision(void) {
    /* Test that severity = 0 returns unchanged color */
    alwan_rgb rgb_in = {ALWAN_LITERAL(0.7), ALWAN_LITERAL(0.3), ALWAN_LITERAL(0.5)};
    alwan_rgb rgb_out;

    int status = alwan_simulate_cvd(&rgb_out, &rgb_in, ALWAN_CVD_PROTANOPIA, ALWAN_LITERAL(0.0));

    TEST_ASSERT(status == ALWAN_OK, "Failed with severity = 0");

    /* Should be unchanged */
    TEST_ASSERT(ALWAN_ABS(rgb_out.r - rgb_in.r) < ALWAN_LITERAL(0.001), "R changed with severity=0");
    TEST_ASSERT(ALWAN_ABS(rgb_out.g - rgb_in.g) < ALWAN_LITERAL(0.001), "G changed with severity=0");
    TEST_ASSERT(ALWAN_ABS(rgb_out.b - rgb_in.b) < ALWAN_LITERAL(0.001), "B changed with severity=0");

    printf("  Normal vision (severity=0): [%.3f, %.3f, %.3f] -> [%.3f, %.3f, %.3f]\n",
           rgb_in.r, rgb_in.g, rgb_in.b,
           rgb_out.r, rgb_out.g, rgb_out.b);

    TEST_PASS("Normal vision (severity = 0)");
}

/* ----------------------------------------------------------------
 * Luminous Efficiency Function Tests
 * ---------------------------------------------------------------- */

/* Embedded test data - compiled at build time */
ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_photopic_wavelengths[] = {
#include "reference_values/photopic_efficiency_wavelengths.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_photopic_values[] = {
#include "reference_values/photopic_efficiency_values.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_scotopic_wavelengths[] = {
#include "reference_values/scotopic_efficiency_wavelengths.csv"
};
ALWAN_DIAG_POP

ALWAN_DIAG_PUSH
ALWAN_DIAG_DISABLE_FLOAT_CONV
static alwan_scalar const g_scotopic_values[] = {
#include "reference_values/scotopic_efficiency_values.csv"
};
ALWAN_DIAG_POP

static int test_photopic_efficiency(void) {
    /* Use embedded reference values from colour-science */
    alwan_scalar const *ref_wavelengths = g_photopic_wavelengths;
    alwan_scalar const *ref_values = g_photopic_values;
    int wl_count = sizeof(g_photopic_wavelengths) / sizeof(g_photopic_wavelengths[0]);
    int val_count = sizeof(g_photopic_values) / sizeof(g_photopic_values[0]);

    TEST_ASSERT(wl_count > 0 && wl_count == val_count, "Photopic reference data size mismatch");

    /* Test against all reference wavelengths */
    alwan_scalar max_error = ALWAN_LITERAL(0.0);
    for (int i = 0; i < wl_count; i++) {
        alwan_scalar computed = alwan_luminous_efficiency(ref_wavelengths[i], ALWAN_VISION_PHOTOPIC);
        alwan_scalar error = ALWAN_ABS(computed - ref_values[i]);
        if (error > max_error) max_error = error;

        /* Allow small interpolation error */
        TEST_ASSERT(error < ALWAN_LITERAL(0.01),
                    "Photopic efficiency error too large");
    }

    /* Display key values */
    printf("  Photopic V(lambda) validated against colour-science:\n");
    for (int i = 0; i < wl_count; i++) {
        if ((int)ref_wavelengths[i] == 450 || (int)ref_wavelengths[i] == 555 ||
            (int)ref_wavelengths[i] == 650) {
            alwan_scalar v = alwan_luminous_efficiency(ref_wavelengths[i], ALWAN_VISION_PHOTOPIC);
            printf("    %.0fnm: %.4f (ref: %.4f)\n", ref_wavelengths[i], v, ref_values[i]);
        }
    }
    printf("    Max error: %.6f\n", max_error);

    TEST_PASS("Photopic luminous efficiency");
}

static int test_scotopic_efficiency(void) {
    /* Use embedded reference values from colour-science */
    alwan_scalar const *ref_wavelengths = g_scotopic_wavelengths;
    alwan_scalar const *ref_values = g_scotopic_values;
    int wl_count = sizeof(g_scotopic_wavelengths) / sizeof(g_scotopic_wavelengths[0]);
    int val_count = sizeof(g_scotopic_values) / sizeof(g_scotopic_values[0]);

    TEST_ASSERT(wl_count > 0 && wl_count == val_count, "Scotopic reference data size mismatch");

    /* Test against all reference wavelengths */
    alwan_scalar max_error = ALWAN_LITERAL(0.0);
    for (int i = 0; i < wl_count; i++) {
        alwan_scalar computed = alwan_luminous_efficiency(ref_wavelengths[i], ALWAN_VISION_SCOTOPIC);
        alwan_scalar error = ALWAN_ABS(computed - ref_values[i]);
        if (error > max_error) max_error = error;

        /* Allow small interpolation error */
        TEST_ASSERT(error < ALWAN_LITERAL(0.01),
                    "Scotopic efficiency error too large");
    }

    /* Display key values */
    printf("  Scotopic V'(lambda) validated against colour-science:\n");
    for (int i = 0; i < wl_count; i++) {
        if ((int)ref_wavelengths[i] == 450 || (int)ref_wavelengths[i] == 507 ||
            (int)ref_wavelengths[i] == 650) {
            alwan_scalar v = alwan_luminous_efficiency(ref_wavelengths[i], ALWAN_VISION_SCOTOPIC);
            printf("    %.0fnm: %.4f (ref: %.4f)\n", ref_wavelengths[i], v, ref_values[i]);
        }
    }
    printf("    Max error: %.6f\n", max_error);

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
 * Contrast Sensitivity Function Tests
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

int test_41_vision_perception_main(void) {
    printf("\n========================================\n");
    printf("Test 41: Vision & Perception\n");
    printf("========================================\n\n");

    int result;

    /* CVD Simulation */
    printf("Color Blindness Simulation\n");
    printf("--------------------------\n");

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

    /* Luminous Efficiency Functions */
    printf("\nLuminous Efficiency Functions\n");
    printf("-----------------------------\n");

    result = test_photopic_efficiency();
    if (result != 0) return result;

    result = test_scotopic_efficiency();
    if (result != 0) return result;

    result = test_luminous_efficiency_bounds();
    if (result != 0) return result;

    /* Contrast Sensitivity Function */
    printf("\nContrast Sensitivity Function\n");
    printf("-----------------------------\n");

    result = test_csf_basic();
    if (result != 0) return result;

    result = test_csf_luminance_dependence();
    if (result != 0) return result;

    result = test_csf_bounds();
    if (result != 0) return result;

    printf("\nAll vision & perception tests passed!\n");
    return 0;
}
