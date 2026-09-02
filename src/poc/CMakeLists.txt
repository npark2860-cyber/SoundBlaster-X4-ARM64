cmake_minimum_required(VERSION 3.25)

project(SoundBlasterX4Arm64Poc LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This PoC targets Windows only.")
endif()

add_executable(x4-poc main.cpp)

target_compile_features(x4-poc PRIVATE cxx_std_17)

target_compile_definitions(x4-poc PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(x4-poc PRIVATE /W4 /EHsc)
endif()

# C++/WinRT desktop applications link against the Windows Runtime umbrella library.
target_link_libraries(x4-poc PRIVATE WindowsApp.lib)
