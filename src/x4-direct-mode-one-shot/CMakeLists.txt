cmake_minimum_required(VERSION 3.24)
project(x4_direct_mode_one_shot LANGUAGES CXX)

add_executable(x4-direct-mode-one-shot
    main.cpp
)

target_compile_features(x4-direct-mode-one-shot PRIVATE cxx_std_20)
target_compile_definitions(x4-direct-mode-one-shot PRIVATE UNICODE _UNICODE)
target_link_libraries(x4-direct-mode-one-shot PRIVATE setupapi advapi32)
