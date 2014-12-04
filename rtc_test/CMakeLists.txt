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
project(rtc_test C CXX)
#set(CMAKE_VERBOSE_MAKEFILE true)
set(Wire_RECURSE True)
link_directories("/usr/avr/include")
link_directories("/usr/avr/include/avr")
#link_directories("/usr/avr/lib")
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libraries)
set(ARDUINO_SDK_PATH /usr/share/arduino)
generate_arduino_firmware(${CMAKE_PROJECT_NAME}
     BOARD uno
     SKETCH ${CMAKE_CURRENT_SOURCE_DIR}
     PORT /dev/ttyACM0
     SERIAL gtkterm --port @SERIAL_PORT@ --speed 115200
     PROGRAMMER usbtinyisp)
add_custom_target(serial DEPENDS ${CMAKE_PROJECT_NAME} ${CMAKE_PROJECT_NAME}-serial)
set_property(TARGET ${CMAKE_PROJECT_NAME}
             APPEND_STRING
             PROPERTY COMPILE_OPTIONS -std=c++11)
