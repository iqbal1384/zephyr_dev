cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nucleo_g474re_sinewave_gen)

target_sources(app PRIVATE
    src/main.c
)
