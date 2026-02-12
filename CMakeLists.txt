cmake_minimum_required(VERSION 3.20)

project(
  Alwan
  VERSION 0.1.0
  DESCRIPTION "Dependency-free C11 colour science library"
  LANGUAGES C
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest) # provides BUILD_TESTING
include(CheckCCompilerFlag)

option(ALWAN_EMBED_DATA "Embed CSV datasets into the library binary" ON)
option(ALWAN_SCALAR_IS_FLOAT "Use float (f32) instead of double (f64)" OFF)
option(ALWAN_ENABLE_WARNINGS "Enable compiler warnings" ON)

# f64 builds are strict by default, f32 builds are permissive by default.
set(_ALWAN_WARNINGS_AS_ERRORS_DEFAULT ON)
if(ALWAN_SCALAR_IS_FLOAT)
  set(_ALWAN_WARNINGS_AS_ERRORS_DEFAULT OFF)
endif()
option(
  ALWAN_WARNINGS_AS_ERRORS
  "Treat warnings as errors (default: ON for f64, OFF for f32)"
  ${_ALWAN_WARNINGS_AS_ERRORS_DEFAULT}
)

option(ALWAN_DISABLE_STRICT_ALIASING "Compile with -fno-strict-aliasing on GCC/Clang" ON)
option(ALWAN_INSTALL "Enable install rules" ON)
option(ALWAN_INSTALL_PKGCONFIG "Install pkg-config .pc file" ON)
option(ALWAN_BUILD_RGB2SPEC "Build tools/rgb2spec submodule (third-party)" OFF)

set(ALWAN_DEBUG_POSTFIX "d" CACHE STRING "Debug postfix for output names (e.g. 'd' -> alwand)")
set(ALWAN_DEBUG_PREFIX "" CACHE STRING "Debug prefix for output names (e.g. 'dbg_' -> dbg_alwan)")

if(NOT ALWAN_EMBED_DATA)
  message(FATAL_ERROR "ALWAN_EMBED_DATA=OFF is not supported in this repository (runtime loader removed).")
endif()

set(_ALWAN_SCALAR_IS_FLOAT_VALUE 0)
if(ALWAN_SCALAR_IS_FLOAT)
  set(_ALWAN_SCALAR_IS_FLOAT_VALUE 1)
endif()

set(_ALWAN_EMBED_DATA_VALUE 1)

set(ALWAN_SOURCES
  src/alwan/api/alwan_aces_ff.c
  src/alwan/api/alwan_atd95.c
  src/alwan/api/alwan_cam.c
  src/alwan/bulk/alwan_cam_bulk.c
  src/alwan/api/alwan_cat.c
  src/alwan/api/alwan_color_correction.c
  src/alwan/api/alwan_colorspace.c
  src/alwan/bulk/alwan_colorspace_bulk.c
  src/alwan/api/alwan_context.c
  src/alwan/api/alwan_convenience.c
  src/alwan/bulk/alwan_convenience_bulk.c
  src/alwan/api/alwan_data.c
  src/alwan/api/alwan_din99.c
  src/alwan/api/alwan_extended.c
  src/alwan/api/alwan_gamut.c
  src/alwan/api/alwan_hellwig2022.c
  src/alwan/api/alwan_hunt.c
  src/alwan/api/alwan_hunter_lab.c
  src/alwan/api/alwan_ictcp.c
  src/alwan/bulk/alwan_ictcp_bulk.c
  src/alwan/api/alwan_ipt.c
  src/alwan/bulk/alwan_ipt_bulk.c
  src/alwan/api/alwan_jzazbz.c
  src/alwan/bulk/alwan_jzazbz_bulk.c
  src/alwan/api/alwan_kim2009.c
  src/alwan/api/alwan_llab.c
  src/alwan/api/alwan_math.c
  src/alwan/api/alwan_oklab.c
  src/alwan/bulk/alwan_oklab_bulk.c
  src/alwan/api/alwan_osa_ucs.c
  src/alwan/api/alwan_prolab.c
  src/alwan/api/alwan_quality.c
  src/alwan/api/alwan_rayleigh.c
  src/alwan/api/alwan_reference_data.c
  src/alwan/api/alwan_normalize.c
  src/alwan/api/alwan_rgb.c
  src/alwan/bulk/alwan_rgb_bulk.c
  src/alwan/api/alwan_rlab.c
  src/alwan/api/alwan_spd.c
  src/alwan/api/alwan_spectrum_upsample.c
  src/alwan/api/alwan_view.c
  src/alwan/api/alwan_vision.c
  src/alwan/api/alwan_zcam.c
)

add_library(alwan ${ALWAN_SOURCES})
add_library(Alwan::alwan ALIAS alwan)

set_target_properties(
  alwan
  PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

if(ALWAN_DEBUG_POSTFIX)
  set_target_properties(alwan PROPERTIES DEBUG_POSTFIX "${ALWAN_DEBUG_POSTFIX}")
endif()
if(ALWAN_DEBUG_PREFIX)
  set_target_properties(alwan PROPERTIES OUTPUT_NAME_DEBUG "${ALWAN_DEBUG_PREFIX}alwan")
endif()

target_include_directories(
  alwan
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/alwan>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(
  alwan
  PUBLIC
    ALWAN_SCALAR_IS_FLOAT=${_ALWAN_SCALAR_IS_FLOAT_VALUE}
    ALWAN_EMBED_DATA=${_ALWAN_EMBED_DATA_VALUE}
)

if(MSVC)
  target_compile_definitions(alwan PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

if(NOT WIN32)
  target_link_libraries(alwan PUBLIC m)
endif()

if(WIN32 AND BUILD_SHARED_LIBS)
  set_target_properties(alwan PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(ALWAN_ENABLE_WARNINGS)
  if(MSVC)
    target_compile_options(alwan PRIVATE /W4)
    if(ALWAN_WARNINGS_AS_ERRORS)
      target_compile_options(alwan PRIVATE /WX)
    endif()
  else()
    target_compile_options(alwan PRIVATE -Wall -Wextra -Wpedantic)
    if(ALWAN_WARNINGS_AS_ERRORS)
      target_compile_options(alwan PRIVATE -Werror)
    endif()
  endif()
endif()

if(ALWAN_DISABLE_STRICT_ALIASING AND NOT MSVC)
  check_c_compiler_flag(-fno-strict-aliasing ALWAN_HAS_NO_STRICT_ALIASING)
  if(ALWAN_HAS_NO_STRICT_ALIASING)
    target_compile_options(alwan PRIVATE -fno-strict-aliasing)
  endif()
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
  add_custom_target(
    check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS alwan_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
endif()

if(ALWAN_BUILD_RGB2SPEC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/rgb2spec/CMakeLists.txt")
  add_subdirectory(tools/rgb2spec)
endif()

if(ALWAN_INSTALL)
  install(
    TARGETS alwan
    EXPORT AlwanTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(
    FILES
      src/alwan/alwan.h
      src/alwan/alwan_config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  set(ALWAN_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/Alwan")

  install(
    EXPORT AlwanTargets
    FILE AlwanTargets.cmake
    NAMESPACE Alwan::
    DESTINATION ${ALWAN_CMAKE_INSTALL_DIR}
  )

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/buildsystem/cmake/AlwanConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/AlwanConfig.cmake"
    INSTALL_DESTINATION ${ALWAN_CMAKE_INSTALL_DIR}
  )

  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/AlwanConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )

  install(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/AlwanConfig.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/AlwanConfigVersion.cmake"
    DESTINATION ${ALWAN_CMAKE_INSTALL_DIR}
  )

  if(ALWAN_INSTALL_PKGCONFIG)
    set(_ALWAN_PKGCONFIG_LIBS_PRIVATE "")
    if(NOT WIN32)
      set(_ALWAN_PKGCONFIG_LIBS_PRIVATE "-lm")
    endif()

    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/buildsystem/cmake/alwan.pc.in"
      "${CMAKE_CURRENT_BINARY_DIR}/alwan.pc"
      @ONLY
    )

    install(
      FILES "${CMAKE_CURRENT_BINARY_DIR}/alwan.pc"
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )
  endif()
endif()
