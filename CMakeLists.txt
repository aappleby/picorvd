cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)

project(test_project C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()
add_executable(test main.cpp WCH_Debug.cpp gdb_server.cpp utils.cpp)

pico_generate_pio_header(test ${CMAKE_CURRENT_LIST_DIR}/singlewire.pio)

target_compile_definitions(test PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=100) # wait for USB connect
target_compile_definitions(test PRIVATE PARAM_ASSERTIONS_ENABLE_ALL=1)

pico_enable_stdio_usb(test 1)
pico_enable_stdio_uart(test 1)
pico_add_extra_outputs(test)

target_link_libraries(test pico_stdlib hardware_pio)
