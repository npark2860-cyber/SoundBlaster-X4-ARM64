cmake_minimum_required(VERSION 3.25)

project(SoundBlasterX4HidOutputPoc LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This PoC targets Windows only.")
endif()

add_executable(x4-hid-output-poc main.cpp)
target_compile_features(x4-hid-output-poc PRIVATE cxx_std_20)
target_compile_definitions(x4-hid-output-poc PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)

if(MSVC)
    target_compile_options(x4-hid-output-poc PRIVATE /W4 /EHsc)
    set_property(TARGET x4-hid-output-poc PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(x4-hid-output-poc PRIVATE setupapi.lib hid.lib)
