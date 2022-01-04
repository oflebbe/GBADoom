cmake_minimum_required(VERSION 3.12)
set(PICO_BOARD "sparkfun_thingplus")

include(pico_sdk_import.cmake)

project("Pico_Doom"
    VERSION 0.0.0
    LANGUAGES C CXX ASM)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

# Set a default build type if none was specified

pico_sdk_init()

add_definitions(-DRP2040)
add_definitions(-DPICO_MALLOC_PANIC=0)
set(PACKAGE_NAME "${PROJECT_NAME}")

set(CMAKE_CROSSCOMPILING ON)

add_subdirectory(source)

