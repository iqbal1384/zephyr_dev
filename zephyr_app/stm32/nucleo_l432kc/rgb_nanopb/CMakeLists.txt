cmake_minimum_required(VERSION 3.28.0)

# Tell CMake to find Zephyr
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(nucleo_l432kc_rgb_nanopb_app)

# Enable zephyr_nanopb_sources() support.
list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

zephyr_nanopb_sources(app src/led_ctrl.proto)

# Add your source files
target_sources(app PRIVATE
    src/main.c
)
