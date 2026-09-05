cmake_minimum_required(VERSION 3.24)
project(x4_mmdevice_endpoint_probe_arm64 LANGUAGES CXX)

add_executable(x4-mmdevice-endpoint-probe
    main.cpp
)

target_compile_features(x4-mmdevice-endpoint-probe PRIVATE cxx_std_17)
target_compile_definitions(x4-mmdevice-endpoint-probe PRIVATE UNICODE _UNICODE)
target_link_libraries(x4-mmdevice-endpoint-probe PRIVATE ole32 uuid)

if(MSVC)
    target_compile_options(x4-mmdevice-endpoint-probe PRIVATE /W4 /WX /permissive-)
endif()
