cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vl53l8cx_ranging_f401re)

target_sources(app PRIVATE src/main.c)
