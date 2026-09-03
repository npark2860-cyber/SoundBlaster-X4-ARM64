cmake_minimum_required(VERSION 3.25)

project(SoundBlasterX4SerialCTCDCProbe LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This diagnostic targets Windows only.")
endif()

add_executable(x4-serial-ctcdc-probe session-open-probe.cpp)

target_compile_features(x4-serial-ctcdc-probe PRIVATE cxx_std_17)

target_compile_definitions(x4-serial-ctcdc-probe PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(x4-serial-ctcdc-probe PRIVATE /W4 /EHsc)
    set_property(TARGET x4-serial-ctcdc-probe PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(x4-serial-ctcdc-probe PRIVATE setupapi.lib advapi32.lib)
