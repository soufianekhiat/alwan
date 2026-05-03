cmake_minimum_required(VERSION 3.20)

# Force 64-bit host toolset on Visual Studio generators. The 32-bit cl.exe
# (default on some installs) hits C1060 "out of heap space" on large
# embedded-data TUs like alwan_spectrum_upsample.c. Must be set before project().
if(NOT CMAKE_GENERATOR_TOOLSET AND CMAKE_GENERATOR MATCHES "Visual Studio")
  set(CMAKE_GENERATOR_TOOLSET "host=x64" CACHE STRING "" FORCE)
endif()

project(
  Alwan
  VERSION 2.0.0
  DESCRIPTION "Dependency-free C11 colour science library"
  LANGUAGES C
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest) # provides BUILD_TESTING
include(CheckCCompilerFlag)

option(ALWAN_EMBED_DATA "Embed CSV datasets into the library binary" ON)
option(ALWAN_ENABLE_WARNINGS "Enable compiler warnings" ON)
option(ALWAN_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)

option(ALWAN_DISABLE_STRICT_ALIASING "Compile with -fno-strict-aliasing on GCC/Clang" ON)
set(ALWAN_SIMD_ARCH "" CACHE STRING "SIMD instruction set: SSE2, AVX, AVX2, or native (empty = compiler default)")
option(ALWAN_INSTALL "Enable install rules" ON)
option(ALWAN_INSTALL_PKGCONFIG "Install pkg-config .pc file" ON)
option(ALWAN_DETERMINISTIC "Use bit-exact polynomial math (no libm transcendentals, no FMA fusion). Trades ~20% perf for cross-platform reproducibility. See road_to_determinism.md." OFF)

set(ALWAN_DEBUG_POSTFIX "d" CACHE STRING "Debug postfix for output names (e.g. 'd' -> alwand)")
set(ALWAN_DEBUG_PREFIX "" CACHE STRING "Debug prefix for output names (e.g. 'dbg_' -> dbg_alwan)")

if(NOT ALWAN_EMBED_DATA)
  message(FATAL_ERROR "ALWAN_EMBED_DATA=OFF is not supported in this repository (runtime loader removed).")
endif()

set(_ALWAN_EMBED_DATA_VALUE 1)

set(ALWAN_SOURCES
  src/alwan/api/alwan_aces_ff.c
  src/alwan/api/alwan_atd95.c
  src/alwan/api/alwan_cam.c
  src/alwan/map/alwan_cam_map.c
  src/alwan/api/alwan_cat.c
  src/alwan/api/alwan_color_correction.c
  src/alwan/api/alwan_colorspace.c
  src/alwan/map/alwan_color_correction_map.c
  src/alwan/map/alwan_colorspace_map.c
  src/alwan/api/alwan_context.c
  src/alwan/api/alwan_convenience.c
  src/alwan/map/alwan_convenience_map.c
  src/alwan/map/alwan_convenience_extra_map.c
  src/alwan/api/alwan_data.c
  src/alwan/api/alwan_din99.c
  src/alwan/api/alwan_extended.c
  src/alwan/map/alwan_extended_map.c
  src/alwan/api/alwan_gamut.c
  src/alwan/map/alwan_gamut_map.c
  src/alwan/api/alwan_hellwig2022.c
  src/alwan/api/alwan_hunt.c
  src/alwan/api/alwan_hunter_lab.c
  src/alwan/api/alwan_ictcp.c
  src/alwan/map/alwan_ictcp_map.c
  src/alwan/api/alwan_ipt.c
  src/alwan/map/alwan_ipt_map.c
  src/alwan/api/alwan_jzazbz.c
  src/alwan/map/alwan_jzazbz_map.c
  src/alwan/api/alwan_kim2009.c
  src/alwan/api/alwan_llab.c
  src/alwan/api/alwan_math.c
  src/alwan/api/alwan_oklab.c
  src/alwan/map/alwan_oklab_map.c
  src/alwan/api/alwan_osa_ucs.c
  src/alwan/api/alwan_prolab.c
  src/alwan/api/alwan_quality.c
  src/alwan/api/alwan_rayleigh.c
  src/alwan/api/alwan_reference_data.c
  src/alwan/api/alwan_normalize.c
  src/alwan/api/alwan_rgb.c
  src/alwan/api/alwan_tf_resolve.c
  src/alwan/map/alwan_mat3_map.c
  src/alwan/map/alwan_rgb_map.c
  src/alwan/map/alwan_typed_map.c
  src/alwan/map/alwan_typed_planar_map.c
  src/alwan/api/alwan_rlab.c
  src/alwan/api/alwan_spd.c
  src/alwan/api/alwan_spectrum_upsample.c
  src/alwan/api/alwan_hdr.c
  src/alwan/api/alwan_spectral_hero.c
  src/alwan/api/alwan_view.c
  src/alwan/api/alwan_vision.c
  src/alwan/api/alwan_lut.c
  src/alwan/api/alwan_export.c
  src/alwan/api/alwan_import.c
  src/alwan/api/alwan_interop.c
  src/alwan/api/alwan_clf.c
  src/alwan/api/alwan_half.c
  src/alwan/api/alwan_video.c
  src/alwan/map/alwan_vision_map.c
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
    ALWAN_EMBED_DATA=${_ALWAN_EMBED_DATA_VALUE}
)

if(ALWAN_DETERMINISTIC)
  # Public so consumers (alwan_dev tests, downstream code) see the same
  # routing path through alwan_math.h.
  target_compile_definitions(alwan PUBLIC ALWAN_DETERMINISTIC=1)
  # Block the compiler from contracting `a*b + c` into a hardware FMA.
  # FMA gives a different last-bit result on x86 (with -mfma) vs without,
  # and ARM aarch64 always fuses since FMA is mandatory. Forcing 2
  # roundings makes the build bit-exact regardless of platform.
  if(MSVC)
    target_compile_options(alwan PUBLIC /fp:precise)
    # In det mode, ALWAN_MAP_SIMD_WIDTH collapses to 1 so all SIMD
    # bodies in *_map_kernels.inc are skipped and their precomputed
    # `set1`/`inv_*` locals become unused. C4189 (initialised but
    # not referenced) and C4101 (unreferenced local) get tripped by
    # tens of those; suppressing them here is cheaper than refactoring
    # every kernel to scope the constants inside the `#if`.
    target_compile_options(alwan PRIVATE /wd4189 /wd4101)
  else()
    target_compile_options(alwan PUBLIC -ffp-contract=off)
    target_compile_options(alwan PRIVATE -Wno-unused-variable -Wno-unused-but-set-variable)
  endif()
endif()

if(MSVC)
  target_compile_definitions(alwan PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
  # Defense in depth for the 32-bit cl.exe heap (alwan_spectrum_upsample.c
  # has ~470K float literals/TU). The primary fix is `host=x64` set above
  # before project(); /Zm is harmless on the 64-bit host.
  target_compile_options(alwan PRIVATE /Zm400)
endif()

if(NOT WIN32)
  target_link_libraries(alwan PUBLIC m)
endif()

if(WIN32 AND BUILD_SHARED_LIBS)
  set_target_properties(alwan PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if(ALWAN_SIMD_ARCH)
  string(TOUPPER "${ALWAN_SIMD_ARCH}" _ALWAN_SIMD_UPPER)
  if(MSVC)
    if(_ALWAN_SIMD_UPPER STREQUAL "AVX2")
      target_compile_options(alwan PUBLIC /arch:AVX2)
    elseif(_ALWAN_SIMD_UPPER STREQUAL "AVX")
      target_compile_options(alwan PUBLIC /arch:AVX)
    elseif(_ALWAN_SIMD_UPPER STREQUAL "AVX512")
      target_compile_options(alwan PUBLIC /arch:AVX512)
    endif()
  else()
    if(_ALWAN_SIMD_UPPER STREQUAL "AVX2")
      target_compile_options(alwan PUBLIC -mavx2 -mfma)
    elseif(_ALWAN_SIMD_UPPER STREQUAL "AVX")
      target_compile_options(alwan PUBLIC -mavx)
    elseif(_ALWAN_SIMD_UPPER STREQUAL "AVX512")
      target_compile_options(alwan PUBLIC -mavx512f -mavx512bw)
    elseif(_ALWAN_SIMD_UPPER STREQUAL "NATIVE")
      target_compile_options(alwan PUBLIC -march=native)
    endif()
  endif()
endif()

if(ALWAN_ENABLE_WARNINGS)
  if(MSVC)
    target_compile_options(alwan PRIVATE /W4)
    if(ALWAN_WARNINGS_AS_ERRORS)
      target_compile_options(alwan PRIVATE /WX)
    endif()
  else()
    # alwan is a public API library: -Wunused-* would flag symbols
    # that are part of the API surface but not consumed by any
    # in-tree call. Coverage of these symbols is the job of the
    # alwan_dev test suite, not the lib build.
    # -Wmissing-braces is silenced because embedded CSV data is
    # included as flat initializers for nested arrays. The other
    # silencers cover gcc-only or clang-only diagnostics that fire
    # on intentional library code (e.g. the f64 sentinel 1e-100
    # underflowing when templated for f32, or a one-line
    # `if(x<0)x=0; if(x>1)x=1;` clamp).
    target_compile_options(alwan PRIVATE -Wall -Wextra -Wpedantic)
    # gcc silently accepts unknown -Wno-X (the warning only fires when
    # another diagnostic does). To detect support reliably we probe
    # the *positive* form -W<flag> — gcc/clang both reject unknown
    # positive forms. If accepted, we add the negative form.
    set(_ALWAN_SILENCED_WARNINGS
      unused-function
      unused-const-variable
      unused-but-set-variable
      unused-variable
      unused-parameter
      missing-braces
      misleading-indentation
      literal-range        # clang only
      overflow             # gcc fires this on 1e-100 -> 0.0f
      maybe-uninitialized  # gcc only
      uninitialized        # gcc "definite" class (separate from maybe-)
      newline-eof          # clang only
    )
    foreach(_w IN LISTS _ALWAN_SILENCED_WARNINGS)
      string(REPLACE "-" "_" _var "ALWAN_HAS_W_${_w}")
      check_c_compiler_flag("-W${_w}" ${_var})
      if(${_var})
        target_compile_options(alwan PRIVATE "-Wno-${_w}")
      endif()
    endforeach()
    unset(_ALWAN_SILENCED_WARNINGS)
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

# Tests now live in the sibling alwan_dev repo (alwan_dev/tests/), built by
# alwan_dev/CMakeLists.txt which re-exposes the alwan target via
# add_subdirectory() and applies ALWAN_NORMALIZE_RANGES=0 there.

# Tools, tests, bench, and image_gen all live in the sibling alwan_dev repo.
# `check_core_parity` is exposed there as a CMake target and CI job.

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


