add_executable(alwan_bench alwan_bench.c)
target_link_libraries(alwan_bench PRIVATE Alwan::alwan)
target_include_directories(alwan_bench PRIVATE "${PROJECT_SOURCE_DIR}/src/alwan")
set_target_properties(alwan_bench PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)

target_compile_definitions(alwan_bench PRIVATE ALWAN_NORMALIZE_RANGES=0)

if(MSVC)
  target_compile_definitions(alwan_bench PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
