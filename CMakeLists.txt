cmake_minimum_required(VERSION 3.3)
project(pulse_volume_monitor)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
include_directories(vendor)

set(SOURCE_FILES main.cpp)
add_executable(pulse_volume_monitor ${SOURCE_FILES})

target_link_libraries(pulse_volume_monitor -lpulse -licuuc -licuio)
