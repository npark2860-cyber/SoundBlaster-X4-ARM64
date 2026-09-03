cmake_minimum_required(VERSION 3.24)
project(x4_asio_arm64_stage_b4b LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "Stage B4B intentionally requires MSVC + the Microsoft Windows SDK.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Stage B4B requires a 64-bit target.")
endif()

add_library(x4-asio-arm64 SHARED
  driver_b4b.cpp
  preflight.cpp
  wavert_engine_b4a.cpp
  driver.def
)
target_compile_features(x4-asio-arm64 PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-arm64 PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
target_compile_options(x4-asio-arm64 PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-arm64 PRIVATE
  advapi32.lib
  uuid.lib
  setupapi.lib
  ksuser.lib
)
set_property(TARGET x4-asio-arm64 PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(x4-asio-stage-b4b-smoke
  smoke_b4b.cpp
)
target_compile_features(x4-asio-stage-b4b-smoke PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-stage-b4b-smoke PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
target_compile_options(x4-asio-stage-b4b-smoke PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-stage-b4b-smoke PRIVATE uuid.lib)
set_property(TARGET x4-asio-stage-b4b-smoke PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_dependencies(x4-asio-stage-b4b-smoke x4-asio-arm64)
