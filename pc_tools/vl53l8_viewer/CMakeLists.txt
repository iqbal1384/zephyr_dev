cmake_minimum_required(VERSION 3.16)
project(vl53l8_viewer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets SerialPort)

add_executable(vl53l8_viewer
    main.cpp
    mainwindow.cpp
    ranginggrid.cpp
    objectdetector.cpp
    serialreader.cpp
)

target_link_libraries(vl53l8_viewer PRIVATE Qt5::Widgets Qt5::SerialPort)

install(TARGETS vl53l8_viewer DESTINATION bin)
