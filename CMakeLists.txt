cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_FETCH_FROM_GIT on)
include(pico_sdk_import.cmake)

project(PicoRVD C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# option(INCLUDE_BLINKY_BINARY "Include Blinky binary in PicoRVD" ON) 

pico_sdk_init()
add_executable(
  picorvd
  src/main.cpp
  src/PicoSWIO.cpp
  src/RVDebug.cpp
  src/Target.cpp
  src/WCHFlash_CH32V003.cpp
  src/WCHFlash_CH32V4x7.cpp
  src/SoftBreak.cpp
  src/HardBreak.cpp
  src/Packet.cpp
  src/Console.cpp
  src/GDBServer.cpp
  src/utils.cpp
  src/usb_config.cpp
  test/picorvd_tests.cpp
  bin/singlewire.pio.h
)

pico_generate_pio_header(picorvd ${CMAKE_CURRENT_SOURCE_DIR}/src/singlewire.pio)

#target_compile_definitions(picorvd PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=100) # wait for USB connect
target_compile_definitions(picorvd PRIVATE PARAM_ASSERTIONS_ENABLE_ALL=1)

# Target chip. See src/Target.cpp for what each one sets.
set(PICORVD_TARGET ch32v003 CACHE STRING "Target chip: ch32v003 or ch32v407")
set_property(CACHE PICORVD_TARGET PROPERTY STRINGS ch32v003 ch32v407)
if(NOT PICORVD_TARGET MATCHES "^(ch32v003|ch32v407)$")
  message(FATAL_ERROR "Unknown PICORVD_TARGET '${PICORVD_TARGET}' (ch32v003 or ch32v407)")
endif()
target_compile_definitions(picorvd PRIVATE PICORVD_TARGET=target_${PICORVD_TARGET})

if(INCLUDE_BLINKY_BINARY)
    add_definitions(-DINCLUDE_BLINKY_BINARY)
endif(INCLUDE_BLINKY_BINARY)

target_include_directories(picorvd PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  # This directory is required so that TinyUSB can find src/tusb_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

pico_enable_stdio_usb(picorvd 0)
pico_enable_stdio_uart(picorvd 1)
pico_add_extra_outputs(picorvd)

target_link_libraries(picorvd
  pico_stdlib
  pico_bootsel_via_double_reset
  hardware_pio
  tinyusb_device
)
