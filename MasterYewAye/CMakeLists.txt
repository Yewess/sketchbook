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
project(MasterYewAye C CXX)
link_directories("/usr/avr/include")
link_directories("/usr/avr/include/avr")
link_directories("/usr/avr/lib")
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../libraries)
set(MasterYewAye_SKETCH ${CMAKE_CURRENT_SOURCE_DIR})
set(MasterYewAye_BOARD uno)
set(MasterYewAye_PROGRAMMER usbtinyisp)
set(MasterYewAye_PORT /dev/ttyUSB0)
set(MasterYewAye_SERIAL gtkterm --port @SERIAL_PORT@ --speed 115200)
add_custom_target(serial DEPENDS MasterYewAye MasterYewAye-serial)
generate_arduino_firmware(MasterYewAye)
