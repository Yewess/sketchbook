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
project(serialControl C CXX)
link_directories("/usr/avr/include")
link_directories("/usr/avr/lib")
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libraries)
link_directories(${CMAKE_CURRENT_SOURCE_DIR})
set(serialControl_SKETCH ${CMAKE_CURRENT_SOURCE_DIR})
set(serialControl_HDRS "serialControl.h")
set(serialControl_BOARD uno)
set(serialControl_PROGRAMMER usbtinyisp)
set(serialControl_PORT /dev/ttyUSB0)
set(serialControl_SERIAL gtkterm --port @SERIAL_PORT@ --speed 115200)
add_custom_target(serial DEPENDS serialControl serialControl-serial)
generate_arduino_firmware(serialControl)
set_property(TARGET serialControl
             APPEND_STRING
             PROPERTY COMPILE_OPTIONS -std=c++11)
#set(CMAKE_VERBOSE_MAKEFILE true)
