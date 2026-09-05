cmake_minimum_required(VERSION 3.24)
project(x4_usbaudio2_attachment_probe_arm64 LANGUAGES CXX)

add_executable(x4-usbaudio2-attachment-probe
    main.cpp
)

target_compile_features(x4-usbaudio2-attachment-probe PRIVATE cxx_std_17)
target_compile_definitions(x4-usbaudio2-attachment-probe PRIVATE UNICODE _UNICODE)
target_link_libraries(x4-usbaudio2-attachment-probe PRIVATE setupapi cfgmgr32 ole32 uuid)

if(MSVC)
    target_compile_options(x4-usbaudio2-attachment-probe PRIVATE /W4 /WX /permissive-)
endif()
