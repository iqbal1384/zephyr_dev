cmake_minimum_required(VERSION 3.20.0)

# Include external st_vl53l8cx sensor module
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../../../../modules/sensor/st_vl53l8cx)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vl53l8cx_ranging_f401re)

target_sources(app PRIVATE src/main.c)
