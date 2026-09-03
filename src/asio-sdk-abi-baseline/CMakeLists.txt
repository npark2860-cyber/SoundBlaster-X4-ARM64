cmake_minimum_required(VERSION 3.24)
project(x4_asio_sdk_abi_baseline LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "This baseline intentionally requires MSVC + the Microsoft Windows SDK.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This baseline requires a 64-bit target.")
endif()

add_executable(x4-asio-sdk-abi-baseline
  main.cpp
  abi_guard.cpp
)

# Experiment branch only: force-include the read-only GLOBALCINSTANCES gate
# into main.cpp so the original baseline lifecycle remains otherwise unchanged.
set_source_files_properties(main.cpp PROPERTIES
  COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/global_instance_gate.h"
)

target_compile_features(x4-asio-sdk-abi-baseline PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-sdk-abi-baseline PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
target_compile_options(x4-asio-sdk-abi-baseline PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-sdk-abi-baseline PRIVATE setupapi.lib ksuser.lib)

set_property(TARGET x4-asio-sdk-abi-baseline PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
