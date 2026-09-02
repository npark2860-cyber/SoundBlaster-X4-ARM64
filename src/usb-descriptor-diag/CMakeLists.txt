cmake_minimum_required(VERSION 3.20)
project(x4_usb_descriptor_diag LANGUAGES CXX)

add_executable(x4-usb-descriptor-diag main.cpp)
target_compile_features(x4-usb-descriptor-diag PRIVATE cxx_std_20)
target_compile_definitions(x4-usb-descriptor-diag PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(x4-usb-descriptor-diag PRIVATE setupapi)
