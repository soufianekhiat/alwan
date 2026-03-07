/*
 * Alwan - Pure C colour science library
 * Copyright (c) 2025 Soufiane KHIAT
 * SPDX-License-Identifier: MIT
 *
 * Main test runner - runs all unit tests consecutively
 */

#include <stdio.h>
#include <stdlib.h>

/* Forward declarations of test main functions */
extern int test_00_context_main(void);
extern int test_01_mat3_ops_main(void);
extern int test_02_data_embed_main(void);
extern int test_03_rgb_matrices_main(void);
extern int test_04_srgb_tf_main(void);
extern int test_05_xyz_lab_luv_main(void);
extern int test_06_delta_e_main(void);
extern int test_07_cat_matrices_main(void);
extern int test_08_cat_roundtrip_main(void);
extern int test_09_tf_hdr_main(void);
extern int test_10_view_transforms_main(void);
extern int test_11_spd_to_xyz_main(void);
extern int test_12_bandpass_2012_main(void);
extern int test_13_ciecam02_main(void);
extern int test_14_cam16_main(void);
extern int test_15_conv_convenience_main(void);
extern int test_16_extended_colorspaces_main(void);
extern int test_17_quality_cct_main(void);
extern int test_18_gamut_main(void);
extern int test_19_rgb_convert_main(void);
extern int test_20_oklab_main(void);
extern int test_21_ictcp_main(void);
extern int test_22_jzazbz_main(void);
extern int test_23_din99_main(void);
extern int test_24_osa_ucs_main(void);
extern int test_25_hunter_lab_main(void);
extern int test_26_ipt_main(void);
extern int test_27_prolab_main(void);
extern int test_28_zcam_main(void);
extern int test_29_hunt_main(void);
extern int test_30_delta_e_extended_main(void);
extern int test_31_whiteness_yellowness_main(void);
extern int test_32_quality_rendering_main(void);
extern int test_33_rgb_spaces_p5_main(void);
extern int test_34_tf_extended_main(void);
extern int test_35_cat_extended_main(void);
extern int test_36_spectral_extended_main(void);
extern int test_37_rgb_to_spectrum_main(void);
extern int test_38_camera_sensitivities_main(void);
extern int test_39_spd_shape_main(void);
extern int test_40_gamut_analysis_main(void);
extern int test_41_vision_perception_main(void);
extern int test_42_math_utilities_main(void);
extern int test_43_reference_data_main(void);
extern int test_44_color_correction_main(void);
extern int test_45_hellwig2022_main(void);
extern int test_46_kim2009_main(void);
extern int test_47_llab_main(void);
extern int test_48_atd95_main(void);
extern int test_49_rayleigh_scattering_main(void);
extern int test_50_barten1999_csf_main(void);
extern int test_51_cct_cineon_main(void);
extern int test_52_aces_fixed_functions_main(void);
extern int test_section9_transfer_functions(void);
extern int test_54_aces20_main(void);
extern int test_55_aces2_output_transform_main(void);
extern int test_56_aces1_output_transform_main(void);
extern int test_57_aces_lmt_tf_main(void);
extern int test_58_core_headers_main(void);
extern int test_59_colorinterop_main(void);
extern int test_60_rgb_xyz_embedded_main(void);
extern int test_61_sigmoid_agx_bt2446_main(void);
extern int test_62_hdr_utilities_main(void);
extern int test_63_cam18sl_main(void);
extern int test_64_cam20u_main(void);
extern int test_65_hero_wavelength_main(void);
extern int test_66_belcour2023_main(void);
extern int test_67_hwb_gamma_dseries_main(void);
extern int test_68_ohno_contrast_dicom_main(void);
extern int test_69_map_validation_main(void);
extern int test_70_planar_map_main(void);

/* Test registry */
typedef struct {
    char const *name;
    int (*test_fn)(void);
} test_suite;

static test_suite const g_test_suites[] = {
    {"00_context", test_00_context_main},
    {"01_mat3_ops", test_01_mat3_ops_main},
    {"02_data_embed", test_02_data_embed_main},
    {"03_rgb_matrices", test_03_rgb_matrices_main},
    {"04_srgb_tf", test_04_srgb_tf_main},
    {"05_xyz_lab_luv", test_05_xyz_lab_luv_main},
    {"06_delta_e", test_06_delta_e_main},
    {"07_cat_matrices", test_07_cat_matrices_main},
    {"08_cat_roundtrip", test_08_cat_roundtrip_main},
    {"09_tf_hdr", test_09_tf_hdr_main},
    {"10_view_transforms", test_10_view_transforms_main},
    {"11_spd_to_xyz", test_11_spd_to_xyz_main},
    {"12_bandpass_2012", test_12_bandpass_2012_main},
    {"13_ciecam02", test_13_ciecam02_main},
    {"14_cam16", test_14_cam16_main},
    {"15_conv_convenience", test_15_conv_convenience_main},
    {"16_extended_colorspaces", test_16_extended_colorspaces_main},
    {"17_quality_cct", test_17_quality_cct_main},
    {"18_gamut", test_18_gamut_main},
    {"19_rgb_convert", test_19_rgb_convert_main},
    {"20_oklab", test_20_oklab_main},
    {"21_ictcp", test_21_ictcp_main},
    {"22_jzazbz", test_22_jzazbz_main},
    {"23_din99", test_23_din99_main},
    {"24_osa_ucs", test_24_osa_ucs_main},
    {"25_hunter_lab", test_25_hunter_lab_main},
    {"26_ipt", test_26_ipt_main},
    {"27_prolab", test_27_prolab_main},
    {"28_zcam", test_28_zcam_main},
    {"29_hunt", test_29_hunt_main},
    {"30_delta_e_extended", test_30_delta_e_extended_main},
    {"31_whiteness_yellowness", test_31_whiteness_yellowness_main},
    {"32_quality_rendering", test_32_quality_rendering_main},
    {"33_rgb_spaces_p5", test_33_rgb_spaces_p5_main},
    {"34_tf_extended", test_34_tf_extended_main},
    {"35_cat_extended", test_35_cat_extended_main},
    {"36_spectral_extended", test_36_spectral_extended_main},
    {"37_rgb_to_spectrum", test_37_rgb_to_spectrum_main},
    {"38_camera_sensitivities", test_38_camera_sensitivities_main},
    {"39_spd_shape", test_39_spd_shape_main},
    {"40_gamut_analysis", test_40_gamut_analysis_main},
    {"41_vision_perception", test_41_vision_perception_main},
    {"42_math_utilities", test_42_math_utilities_main},
    {"43_reference_data", test_43_reference_data_main},
    {"44_color_correction", test_44_color_correction_main},
    {"45_hellwig2022", test_45_hellwig2022_main},
    {"46_kim2009", test_46_kim2009_main},
    {"47_llab", test_47_llab_main},
    {"48_atd95", test_48_atd95_main},
    {"49_rayleigh_scattering", test_49_rayleigh_scattering_main},
    {"50_barten1999_csf", test_50_barten1999_csf_main},
    {"51_cct_cineon", test_51_cct_cineon_main},
    {"52_aces_fixed_functions", test_52_aces_fixed_functions_main},
    {"53_section9_transfer_functions", test_section9_transfer_functions},
    {"54_aces20", test_54_aces20_main},
    {"55_aces2_output_transform", test_55_aces2_output_transform_main},
    {"56_aces1_output_transform", test_56_aces1_output_transform_main},
    {"57_aces_lmt_tf", test_57_aces_lmt_tf_main},
    {"58_core_headers", test_58_core_headers_main},
    {"59_colorinterop", test_59_colorinterop_main},
    {"60_rgb_xyz_embedded", test_60_rgb_xyz_embedded_main},
    {"61_sigmoid_agx_bt2446", test_61_sigmoid_agx_bt2446_main},
    {"62_hdr_utilities", test_62_hdr_utilities_main},
    {"63_cam18sl", test_63_cam18sl_main},
    {"64_cam20u", test_64_cam20u_main},
    {"65_hero_wavelength", test_65_hero_wavelength_main},
    {"66_belcour2023", test_66_belcour2023_main},
    {"67_hwb_gamma_dseries", test_67_hwb_gamma_dseries_main},
    {"68_ohno_contrast_dicom", test_68_ohno_contrast_dicom_main},
    {"69_map_validation", test_69_map_validation_main},
    {"70_planar_map", test_70_planar_map_main},
};

int main(void) {
    printf("========================================\n");
    printf("Alwan Unit Test Runner\n");
    printf("========================================\n\n");

    int total_failed = 0;
    int total_passed = 0;
    size_t const num_suites = sizeof(g_test_suites) / sizeof(g_test_suites[0]);

    for (size_t i = 0; i < num_suites; i++) {
        printf("\n");
        printf("========================================\n");
        printf("Running test suite: %s\n", g_test_suites[i].name);
        printf("========================================\n");

        int result = g_test_suites[i].test_fn();

        if (result == 0) {
            total_passed++;
            printf("[PASS] Test suite '%s' passed\n", g_test_suites[i].name);
        } else {
            total_failed++;
            printf("[FAIL] Test suite '%s' failed with code %d\n",
                   g_test_suites[i].name, result);
        }
    }

    printf("\n");
    printf("========================================\n");
    printf("Overall Results\n");
    printf("========================================\n");
    printf("Test suites passed: %d\n", total_passed);
    printf("Test suites failed: %d\n", total_failed);
    printf("Total test suites:  %zu\n", num_suites);
    printf("========================================\n");

    if (total_failed > 0) {
        printf("\nSome tests FAILED!\n");
        return 1;
    } else {
        printf("\nAll tests PASSED!\n");
        return 0;
    }
}
