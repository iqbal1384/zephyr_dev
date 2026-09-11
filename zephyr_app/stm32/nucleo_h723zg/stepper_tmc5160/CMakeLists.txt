cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nucleo_h723zg_stepper_tmc5160)

target_sources(app PRIVATE src/main.c)
