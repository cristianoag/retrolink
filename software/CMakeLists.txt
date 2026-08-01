cmake_minimum_required(VERSION 3.13)

set(RETROLINK_FW_VERSION "1.00")

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH must point to the Raspberry Pi Pico SDK")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(retrolink C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(retrolink
    src/main.c
    src/status_led.c
    src/usb_host.c
)

pico_generate_pio_header(retrolink ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_include_directories(retrolink PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
)

if(DEFINED PICO_PIO_USB_PATH)
    add_subdirectory(${PICO_PIO_USB_PATH} ${CMAKE_CURRENT_BINARY_DIR}/pico-pio-usb)
    target_include_directories(retrolink PRIVATE
        ${PICO_PIO_USB_PATH}
        ${PICO_PIO_USB_PATH}/src
    )
elseif(NOT EXISTS ${PICO_SDK_PATH}/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB/src/pio_usb.h)
    message(FATAL_ERROR "pio_usb.h was not found. Install Pico-PIO-USB and pass PICO_PIO_USB_PATH=<path> to make, or install it where the Pico SDK TinyUSB RP2040 host BSP can find it.")
endif()

target_compile_definitions(retrolink PRIVATE
    RETROLINK_FW_VERSION="${RETROLINK_FW_VERSION}"
    RETROLINK_HW_REVISION="1.00"
)

target_link_libraries(retrolink
    pico_stdlib
    hardware_pio
    tinyusb_host
    tinyusb_board
)

if(TARGET pico_pio_usb)
    target_link_libraries(retrolink pico_pio_usb)
    target_compile_definitions(retrolink PRIVATE CFG_TUH_RPI_PIO_USB=1)
endif()

if(TARGET tinyusb_pico_pio_usb)
    target_link_libraries(retrolink tinyusb_pico_pio_usb)
    target_compile_definitions(retrolink PRIVATE CFG_TUH_RPI_PIO_USB=1)
endif()

pico_enable_stdio_usb(retrolink 0)
pico_enable_stdio_uart(retrolink 1)
pico_add_extra_outputs(retrolink)

set(RETROLINK_UF2_OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/../firmware)

add_custom_target(retrolink_export_uf2 ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RETROLINK_UF2_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/retrolink.uf2
        ${RETROLINK_UF2_OUTPUT_DIR}/retrolink-${RETROLINK_FW_VERSION}.uf2
    DEPENDS retrolink
    COMMENT "Copying RetroLink UF2 to firmware/"
)