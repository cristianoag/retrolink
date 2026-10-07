cmake_minimum_required(VERSION 3.13)

set(RETROLINK_FW_VERSION "1.00" CACHE STRING "RetroLink USB mouse firmware version")
set(RETROLINK_HW_REVISION "1.10" CACHE STRING "RetroLink USB hardware revision")
set(RETROLINK_JOYSTICK_DIR ${CMAKE_CURRENT_LIST_DIR}/../joystick)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH must point to the Raspberry Pi Pico SDK")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(retrolink_mouse C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(retrolink_mouse
    src/main.c
    src/hid_mouse.c
    src/mouse_port.c
    src/msx_mouse.c
    src/usb_host.c
    ${RETROLINK_JOYSTICK_DIR}/src/debug_cdc.c
    ${RETROLINK_JOYSTICK_DIR}/src/status_led.c
    ${RETROLINK_JOYSTICK_DIR}/src/usb_descriptors.c
)

pico_generate_pio_header(retrolink_mouse ${RETROLINK_JOYSTICK_DIR}/src/ws2812.pio)

# The mouse include directory comes first so shared joystick sources use the mouse board_config.h.
target_include_directories(retrolink_mouse PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${RETROLINK_JOYSTICK_DIR}/include
    ${RETROLINK_JOYSTICK_DIR}/src
)

if(DEFINED PICO_PIO_USB_PATH)
    add_subdirectory(${PICO_PIO_USB_PATH} ${CMAKE_CURRENT_BINARY_DIR}/pico-pio-usb)
    target_include_directories(retrolink_mouse PRIVATE
        ${PICO_PIO_USB_PATH}
        ${PICO_PIO_USB_PATH}/src
    )
elseif(NOT EXISTS ${PICO_SDK_PATH}/lib/tinyusb/hw/mcu/raspberry_pi/Pico-PIO-USB/src/pio_usb.h)
    message(FATAL_ERROR "pio_usb.h was not found. Install Pico-PIO-USB and pass PICO_PIO_USB_PATH=<path> to make, or install it where the Pico SDK TinyUSB RP2040 host BSP can find it.")
endif()

target_compile_definitions(retrolink_mouse PRIVATE
    RETROLINK_FW_VERSION="${RETROLINK_FW_VERSION}"
    RETROLINK_HW_REVISION="${RETROLINK_HW_REVISION}"
    RETROLINK_ON_DEVICE=1
    PIO_USB_USE_TINYUSB
)

target_link_libraries(retrolink_mouse
    pico_stdlib
    pico_multicore
    hardware_pio
    hardware_sync
    tinyusb_device
    tinyusb_host
    tinyusb_board
)

if(TARGET pico_pio_usb)
    target_link_libraries(retrolink_mouse pico_pio_usb)
    target_compile_definitions(retrolink_mouse PRIVATE CFG_TUH_RPI_PIO_USB=1)
endif()

if(TARGET tinyusb_pico_pio_usb)
    target_link_libraries(retrolink_mouse tinyusb_pico_pio_usb)
    target_compile_definitions(retrolink_mouse PRIVATE CFG_TUH_RPI_PIO_USB=1)
endif()

pico_enable_stdio_usb(retrolink_mouse 0)
pico_enable_stdio_uart(retrolink_mouse 0)
pico_set_program_name(retrolink_mouse "RetroLink USB Mouse")
pico_set_program_version(retrolink_mouse ${RETROLINK_FW_VERSION})
pico_add_extra_outputs(retrolink_mouse)

set(RETROLINK_UF2_OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../firmware/usb)

add_custom_target(retrolink_mouse_export_uf2 ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RETROLINK_UF2_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/retrolink_mouse.uf2
        ${RETROLINK_UF2_OUTPUT_DIR}/retrolink-mouse-${RETROLINK_FW_VERSION}.uf2
    DEPENDS retrolink_mouse
    COMMENT "Copying RetroLink USB mouse UF2 to firmware/usb/"
)
