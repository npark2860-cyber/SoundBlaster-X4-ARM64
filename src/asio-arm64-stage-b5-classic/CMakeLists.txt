cmake_minimum_required(VERSION 3.24)
project(x4_asio_b5_classic_arm64 LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "B5 Classic ARM64 requires MSVC + the Microsoft Windows SDK.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "B5 Classic ARM64 requires a 64-bit target.")
endif()
if(NOT CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
  message(FATAL_ERROR "Configure this tree with Visual Studio -A ARM64.")
endif()

set(B5_SHARED_DIR "${CMAKE_CURRENT_LIST_DIR}/../asio-arm64-stage-b0")

add_library(x4-asio-arm64-b5 SHARED
  "${B5_SHARED_DIR}/driver_b5.cpp"
  "${B5_SHARED_DIR}/preflight.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/wavert_engine_b5_classic.cpp"
  "${B5_SHARED_DIR}/driver.def"
)
target_include_directories(x4-asio-arm64-b5 PRIVATE "${B5_SHARED_DIR}")
target_compile_features(x4-asio-arm64-b5 PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-arm64-b5 PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
# C4324 is the expected padding needed to honor the driver's explicit 64-byte
# audio-buffer alignment. Suppress only this informational warning for B5.
target_compile_options(x4-asio-arm64-b5 PRIVATE /W4 /wd4324 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-arm64-b5 PRIVATE
  advapi32.lib
  uuid.lib
  setupapi.lib
  ksuser.lib
  winmm.lib
)
set_property(TARGET x4-asio-arm64-b5 PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
