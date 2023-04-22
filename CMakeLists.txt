cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(PicoRVD C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

pico_sdk_init()
add_executable(
  picorvd
  main.cpp
  PicoSWIO.cpp
  RVDebug.cpp
  WCHFlash.cpp
  SoftBreak.cpp
  Packet.cpp
  Console.cpp
  GDBServer.cpp
  utils.cpp
  tests.cpp
  usb_config.cpp
)

pico_generate_pio_header(picorvd ${CMAKE_CURRENT_SOURCE_DIR}/singlewire.pio)

target_compile_definitions(picorvd PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=100) # wait for USB connect
target_compile_definitions(picorvd PRIVATE PARAM_ASSERTIONS_ENABLE_ALL=1)

target_include_directories(picorvd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

pico_enable_stdio_usb(picorvd 0)
pico_enable_stdio_uart(picorvd 1)
pico_add_extra_outputs(picorvd)

target_link_libraries(picorvd
  pico_stdlib
  pico_bootsel_via_double_reset
  hardware_pio
  tinyusb_device
)
