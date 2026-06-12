cmake_minimum_required(VERSION 3.13)
project(test_harness C)

# Add the hardware_driver component source files
add_subdirectory(../components/hardware_driver ${CMAKE_BINARY_DIR}/hardware_driver)

add_executable(test_harness main.c)

target_link_libraries(test_harness PRIVATE hardware_driver)
