cmake_minimum_required(VERSION 3.25)

project(SoundBlasterX4UsbDiag LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "This diagnostic targets Windows only.")
endif()

add_executable(x4-usb-diag main.cpp)

target_compile_features(x4-usb-diag PRIVATE cxx_std_20)

target_compile_definitions(x4-usb-diag PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(x4-usb-diag PRIVATE /W4 /EHsc)
    set_property(TARGET x4-usb-diag PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(x4-usb-diag PRIVATE
    setupapi.lib
    cfgmgr32.lib
    hid.lib
    ole32.lib
)
