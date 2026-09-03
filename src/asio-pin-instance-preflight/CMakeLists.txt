cmake_minimum_required(VERSION 3.24)
project(x4_asio_pin_instance_preflight LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "This probe intentionally requires MSVC + the Microsoft Windows SDK.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This probe requires a 64-bit target.")
endif()

add_executable(x4-asio-pin-instance-preflight
  main.cpp
)

target_compile_features(x4-asio-pin-instance-preflight PRIVATE cxx_std_17)
target_compile_definitions(x4-asio-pin-instance-preflight PRIVATE
  UNICODE
  _UNICODE
  _WIN32_WINNT=0x0A00
  WINVER=0x0A00
)
target_compile_options(x4-asio-pin-instance-preflight PRIVATE /W4 /permissive- /GR- /EHsc /Oy-)
target_link_libraries(x4-asio-pin-instance-preflight PRIVATE setupapi.lib)

set_property(TARGET x4-asio-pin-instance-preflight PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
