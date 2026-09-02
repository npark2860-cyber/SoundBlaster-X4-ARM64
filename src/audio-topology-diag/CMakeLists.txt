cmake_minimum_required(VERSION 3.21)
project(x4_audio_topology_diag LANGUAGES CXX)

add_executable(x4-audio-topology-diag main.cpp)
target_compile_features(x4-audio-topology-diag PRIVATE cxx_std_20)
target_compile_definitions(x4-audio-topology-diag PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_link_libraries(x4-audio-topology-diag PRIVATE ole32 uuid)
