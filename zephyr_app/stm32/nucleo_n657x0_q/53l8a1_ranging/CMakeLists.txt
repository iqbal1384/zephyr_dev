cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(53l8a1_ranging)

# ── Application source ──────────────────────────────────────────────────────
target_sources(app PRIVATE src/main.c)

# ── VL53L8CX sensor library (portable, no HAL) ──────────────────────────────
set(VL53L8CX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/vl53l8cx)

target_sources(app PRIVATE
    ${VL53L8CX_DIR}/porting/platform.c
    ${VL53L8CX_DIR}/modules/vl53l8cx_api.c
    ${VL53L8CX_DIR}/modules/vl53l8cx_plugin_detection_thresholds.c
    ${VL53L8CX_DIR}/modules/vl53l8cx_plugin_motion_indicator.c
    ${VL53L8CX_DIR}/modules/vl53l8cx_plugin_xtalk.c
)

target_include_directories(app PRIVATE
    ${VL53L8CX_DIR}/porting
    ${VL53L8CX_DIR}/modules
)
