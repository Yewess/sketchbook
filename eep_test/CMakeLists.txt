#=============================================================================#
# Author: QueezyTheGreat                                                      #
# Date:   26.04.2011                                                          #
#                                                                             #
# Description: Arduino CMake example                                          #
#                                                                             #
#=============================================================================#
set(CMAKE_TOOLCHAIN_FILE ../arduino-cmake/cmake/ArduinoToolchain.cmake)


cmake_minimum_required(VERSION 2.8)
#====================================================================#
#  Setup Project                                                     #
#====================================================================#
project(eep_test C CXX)
link_directories("/usr/avr/include")
link_directories("/usr/avr/include/avr")
link_directories("/usr/avr/lib")
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libraries)
set(eep_test_SKETCH ${CMAKE_CURRENT_SOURCE_DIR})
set(eep_test_BOARD uno)
set(eep_test_PROGRAMMER arduino)
set(eep_test_PORT /dev/ttyACM0)
set(eep_test_SERIAL gtkterm --port @SERIAL_PORT@ --speed 115200)
add_custom_target(serial DEPENDS eep_test eep_test-serial)
generate_arduino_firmware(eep_test)
set_property(TARGET eep_test
             APPEND_STRING
             PROPERTY COMPILE_OPTIONS -std=c++11)
set_property(TARGET uno_eep
             APPEND_STRING
             PROPERTY COMPILE_OPTIONS -std=c++11)
