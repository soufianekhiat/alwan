add_executable(
  alwan_tests
  test_runner.c
  00_context.c
  01_mat3_ops.c
  02_data_embed_compile.c
  03_rgb_matrices.c
  04_srgb_tf.c
  05_xyz_lab_luv.c
  06_delta_e.c
  07_cat_matrices.c
  08_cat_roundtrip.c
  09_tf_hdr.c
  10_view_transforms.c
  11_spd_to_xyz.c
  12_bandpass_2012.c
  13_ciecam02.c
  14_cam16.c
  15_conv_convenience.c
  16_extended_colorspaces.c
  17_quality_cct.c
  18_gamut.c
  19_rgb_convert.c
  20_oklab.c
  21_ictcp.c
  22_jzazbz.c
  23_din99.c
  24_osa_ucs.c
  25_hunter_lab.c
  26_ipt.c
  27_prolab.c
  28_zcam.c
  29_hunt.c
  30_delta_e_extended.c
  31_whiteness_yellowness.c
  32_quality_rendering.c
  33_rgb_spaces_p5.c
  34_tf_extended.c
  35_cat_extended.c
  36_spectral_extended.c
  37_rgb_to_spectrum.c
  38_camera_sensitivities.c
  39_spd_shape.c
  40_gamut_analysis.c
  41_vision_perception.c
  42_math_utilities.c
  43_reference_data.c
  44_color_correction.c
  45_hellwig2022.c
  46_kim2009.c
  47_llab.c
  48_atd95.c
  49_rayleigh_scattering.c
  50_barten1999_csf.c
  51_cct_cineon.c
  52_aces_fixed_functions.c
  53_section9_transfer_functions.c
  54_aces20.c
  55_aces2_output_transform.c
  56_aces1_output_transform.c
  57_aces_lmt_tf.c
  58_core_headers.c
  59_colorinterop.c
  60_rgb_xyz_embedded.c
)

target_link_libraries(alwan_tests PRIVATE Alwan::alwan)
target_include_directories(alwan_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src/alwan")
set_target_properties(alwan_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

if(MSVC)
  target_compile_definitions(alwan_tests PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

add_test(NAME alwan_tests COMMAND alwan_tests)

