cmake_minimum_required(VERSION 3.20)
project(k_printf C)

set(CMAKE_C_STANDARD 99)

file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.c" )

add_executable(k_printf ${SRC_FILES})

set_target_properties(k_printf PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build )
