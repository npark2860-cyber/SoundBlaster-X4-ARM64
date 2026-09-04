cmake_minimum_required(VERSION 3.24)
project(x4_asio_arm64ec_stage_b4d LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "Stage B4D requires MSVC + the Microsoft Windows SDK.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Stage B4D requires a 64-bit target.")
endif()

if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64EC")
  message(FATAL_ERROR "Stage B4D must be configured with the Visual Studio ARM64EC platform (-A ARM64EC).")
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

add_executable(x4-asio-stage-b4d-smoke
  smoke_b4d_arm64ec.cpp
)
target_compile_features(x4-asio-stage-b4d-smoke PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b4d-smoke PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
target_compile_options(x4-asio-stage-b4d-smoke PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b4d-smoke PRIVATE uuid.lib)
set_property(TARGET x4-asio-stage-b4d-smoke PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(x4-asio-stage-b4d-smoke x4-asio-arm64ec)

add_executable(x4-asio-stage-b4d-register
  register_b4d_arm64ec.cpp
)
target_compile_features(x4-asio-stage-b4d-register PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b4d-register PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
target_compile_options(x4-asio-stage-b4d-register PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b4d-register PRIVATE advapi32.lib)
set_property(TARGET x4-asio-stage-b4d-register PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_dependencies(x4-asio-stage-b4d-register x4-asio-arm64ec)
