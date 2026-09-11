cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nucleo_l432kc_eink_gdep073e01)

target_sources(app PRIVATE
    src/main.c
    src/gdep073e01.c
)
