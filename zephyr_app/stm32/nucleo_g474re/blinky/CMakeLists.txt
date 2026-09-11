cmake_minimum_required(VERSION 3.28.0)

# Tell CMake to find Zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nucleo_g474re_app)

# Add your source files
target_sources(app PRIVATE
    src/main.c
)