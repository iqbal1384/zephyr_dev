cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rs485_mcumgr)

target_sources(app PRIVATE
    src/main.c
    src/rs485_uart.c
    src/rs485_envelope.c
    src/smp_rs485_transport.c
)
