cmake_minimum_required(VERSION 3.24)
project(x4_asio_b5_arm64x_tools LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "B5 ARM64X tools require MSVC + the Microsoft Windows SDK.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "B5 ARM64X tools require a 64-bit target.")
endif()

set(B5_SHARED_DIR "${CMAKE_CURRENT_LIST_DIR}/../asio-arm64-stage-b0")

if(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64EC")
  add_executable(x4-asio-stage-b5-arm64x-register register_arm64x_b5.cpp)
  target_include_directories(x4-asio-stage-b5-arm64x-register PRIVATE "${B5_SHARED_DIR}")
  target_compile_features(x4-asio-stage-b5-arm64x-register PRIVATE cxx_std_17)
  target_compile_definitions(x4-asio-stage-b5-arm64x-register PRIVATE
    UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
  target_compile_options(x4-asio-stage-b5-arm64x-register PRIVATE
    /W4 /permissive- /GR- /EHsc /Oy-)
  target_link_libraries(x4-asio-stage-b5-arm64x-register PRIVATE advapi32.lib)
  set_property(TARGET x4-asio-stage-b5-arm64x-register PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  add_executable(x4-asio-stage-b5-arm64x-probe-arm64ec forwarder_probe.cpp)
  target_include_directories(x4-asio-stage-b5-arm64x-probe-arm64ec PRIVATE "${B5_SHARED_DIR}")
  target_compile_features(x4-asio-stage-b5-arm64x-probe-arm64ec PRIVATE cxx_std_17)
  target_compile_definitions(x4-asio-stage-b5-arm64x-probe-arm64ec PRIVATE
    UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
  target_compile_options(x4-asio-stage-b5-arm64x-probe-arm64ec PRIVATE
    /W4 /permissive- /GR- /EHsc /Oy-)
  target_link_libraries(x4-asio-stage-b5-arm64x-probe-arm64ec PRIVATE ole32.lib uuid.lib)
  set_property(TARGET x4-asio-stage-b5-arm64x-probe-arm64ec PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
  add_executable(x4-asio-stage-b5-arm64x-probe-arm64 forwarder_probe.cpp)
  target_include_directories(x4-asio-stage-b5-arm64x-probe-arm64 PRIVATE "${B5_SHARED_DIR}")
  target_compile_features(x4-asio-stage-b5-arm64x-probe-arm64 PRIVATE cxx_std_17)
  target_compile_definitions(x4-asio-stage-b5-arm64x-probe-arm64 PRIVATE
    UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
  target_compile_options(x4-asio-stage-b5-arm64x-probe-arm64 PRIVATE
    /W4 /permissive- /GR- /EHsc /Oy-)
  target_link_libraries(x4-asio-stage-b5-arm64x-probe-arm64 PRIVATE ole32.lib uuid.lib)
  set_property(TARGET x4-asio-stage-b5-arm64x-probe-arm64 PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
  message(FATAL_ERROR "Configure B5 ARM64X tools with -A ARM64EC or -A ARM64.")
endif()
