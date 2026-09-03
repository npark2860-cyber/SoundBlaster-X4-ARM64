cmake_minimum_required(VERSION 3.24)
project(x4_asio_engine_stage_a LANGUAGES CXX)

add_executable(x4-asio-engine-stage-a main.cpp)
set_target_properties(x4-asio-engine-stage-a PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES)

target_compile_definitions(x4-asio-engine-stage-a PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(x4-asio-engine-stage-a PRIVATE setupapi ksuser kernel32)

if(MSVC)
  target_compile_options(x4-asio-engine-stage-a PRIVATE /W4 /permissive- /GR-)
endif()
