cmake_minimum_required(VERSION 3.24)
project(x4_asio_arm64ec_stage_b4d LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "Stage B4D/B5 requires MSVC + the Microsoft Windows SDK.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Stage B4D/B5 requires a 64-bit target.")
endif()

if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64EC")
  message(FATAL_ERROR "This tree must be configured with the Visual Studio ARM64EC platform (-A ARM64EC).")
endif()

add_library(x4-asio-arm64ec SHARED
  driver_b4d_arm64ec.cpp
  preflight_b4d_arm64ec.cpp
  wavert_engine_b4d_arm64ec.cpp
  driver.def
)
target_compile_features(x4-asio-arm64ec PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-arm64ec PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
target_compile_options(x4-asio-arm64ec PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-arm64ec PRIVATE
  advapi32.lib
  uuid.lib
  setupapi.lib
  ksuser.lib
  winmm.lib
)
set_property(TARGET x4-asio-arm64ec PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(x4-asio-stage-b4d-smoke smoke_b4d_arm64ec.cpp)
target_compile_features(x4-asio-stage-b4d-smoke PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b4d-smoke PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(x4-asio-stage-b4d-smoke PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b4d-smoke PRIVATE uuid.lib)
set_property(TARGET x4-asio-stage-b4d-smoke PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(x4-asio-stage-b4d-smoke x4-asio-arm64ec)

add_executable(x4-asio-stage-b4d-register register_b4d_arm64ec.cpp)
target_compile_features(x4-asio-stage-b4d-register PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b4d-register PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(x4-asio-stage-b4d-register PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b4d-register PRIVATE advapi32.lib)
set_property(TARGET x4-asio-stage-b4d-register PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(x4-asio-stage-b4d-register x4-asio-arm64ec)

add_executable(x4-asio-stage-b4d-host-probe host_probe_b4d_arm64ec.cpp)
target_compile_features(x4-asio-stage-b4d-host-probe PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b4d-host-probe PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(x4-asio-stage-b4d-host-probe PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b4d-host-probe PRIVATE ole32.lib)
set_property(TARGET x4-asio-stage-b4d-host-probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(x4-asio-stage-b4d-host-probe x4-asio-arm64ec)

# B5-0 measurement-only tools.
add_executable(x4-asio-stage-b5-capability-probe capability_probe_b5_arm64ec.cpp)
target_compile_features(x4-asio-stage-b5-capability-probe PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b5-capability-probe PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(x4-asio-stage-b5-capability-probe PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b5-capability-probe PRIVATE advapi32.lib ole32.lib)
set_property(TARGET x4-asio-stage-b5-capability-probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(x4-asio-stage-b5-ks-probe ks_capability_probe_b5_arm64ec.cpp)
target_compile_features(x4-asio-stage-b5-ks-probe PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b5-ks-probe PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(x4-asio-stage-b5-ks-probe PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b5-ks-probe PRIVATE ole32.lib setupapi.lib uuid.lib)
set_property(TARGET x4-asio-stage-b5-ks-probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# B5 productized side-by-side ARM64EC driver. Validated B4D sources above remain untouched.
add_library(x4-asio-arm64ec-b5 SHARED
  driver_b5_arm64ec.cpp
  preflight_b4d_arm64ec.cpp
  wavert_engine_b5_arm64ec.cpp
  driver_b5.def
)
target_compile_features(x4-asio-arm64ec-b5 PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-arm64ec-b5 PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
# C4324 is expected because B5 deliberately aligns its host audio buffers to
# 64-byte cache-line boundaries. Suppress only this informational warning.
target_compile_options(x4-asio-arm64ec-b5 PRIVATE /W4 /wd4324 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-arm64ec-b5 PRIVATE
  advapi32.lib
  uuid.lib
  setupapi.lib
  ksuser.lib
  winmm.lib
)
set_property(TARGET x4-asio-arm64ec-b5 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(x4-asio-stage-b5-register register_b5_arm64ec.cpp)
target_compile_features(x4-asio-stage-b5-register PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b5-register PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(x4-asio-stage-b5-register PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b5-register PRIVATE advapi32.lib)
set_property(TARGET x4-asio-stage-b5-register PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(x4-asio-stage-b5-register x4-asio-arm64ec-b5)

add_executable(x4-asio-stage-b5-product-validation product_validation_b5_arm64ec.cpp)
target_compile_features(x4-asio-stage-b5-product-validation PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b5-product-validation PRIVATE UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_compile_options(x4-asio-stage-b5-product-validation PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b5-product-validation PRIVATE ole32.lib)
set_property(TARGET x4-asio-stage-b5-product-validation PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(x4-asio-stage-b5-product-validation x4-asio-arm64ec-b5)
