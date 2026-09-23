cmake_minimum_required(VERSION 3.13)

option(RETROLINK_HOST_TESTS "Build native MD regression tests without the Pico SDK" OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(RETROLINK_SHARED_DIR ${CMAKE_CURRENT_LIST_DIR}/../usb)

if(RETROLINK_HOST_TESTS)
    project(retrolink_md_tests C)
    enable_testing()
    add_executable(md_pad_test src/md_pad.c tests/md_pad_test.c)
    add_executable(md_io_test src/md_pad.c src/md_port.c src/msx_output.c tests/md_io_test.c)
    foreach(test_target md_pad_test md_io_test)
        target_include_directories(${test_target} PRIVATE
            tests/stubs include ${RETROLINK_SHARED_DIR}/include)
        if(MSVC)
            target_compile_options(${test_target} PRIVATE /W4 /WX /UNDEBUG)
        else()
            target_compile_options(${test_target} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
        endif()
        add_test(NAME ${test_target} COMMAND ${test_target})
    endforeach()
    return()
endif()

set(RETROLINK_FW_VERSION "1.00")
if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()
if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH must point to the Raspberry Pi Pico SDK; see README.md")
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(retrolink_md C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(retrolink_md
    src/main.c src/md_pad.c src/md_port.c src/msx_output.c
    ${RETROLINK_SHARED_DIR}/src/status_led.c
)
pico_generate_pio_header(retrolink_md ${RETROLINK_SHARED_DIR}/src/ws2812.pio)
target_include_directories(retrolink_md PRIVATE include ${RETROLINK_SHARED_DIR}/include)
target_compile_definitions(retrolink_md PRIVATE
    RETROLINK_FW_VERSION="${RETROLINK_FW_VERSION}"
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
    PICO_STDIO_USB_STDOUT_TIMEOUT_US=0
    PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
    PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=0
    # The B0/B1 enumeration workaround commandeers GPIO15, our MD SELECT output.
    TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX=0
)
target_compile_options(retrolink_md PRIVATE -Wall -Wextra -Werror)
target_link_libraries(retrolink_md pico_stdlib hardware_pio hardware_sync)
pico_enable_stdio_usb(retrolink_md 1)
pico_enable_stdio_uart(retrolink_md 0)
pico_set_program_name(retrolink_md "RetroLink MD")
pico_set_program_version(retrolink_md ${RETROLINK_FW_VERSION})
pico_add_extra_outputs(retrolink_md)

set(RETROLINK_UF2_OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/../../firmware/md)
add_custom_target(retrolink_md_export_uf2 ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RETROLINK_UF2_OUTPUT_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/retrolink_md.uf2
        ${RETROLINK_UF2_OUTPUT_DIR}/retrolink-md-${RETROLINK_FW_VERSION}.uf2
    DEPENDS retrolink_md
    COMMENT "Copying RetroLink MD UF2 to firmware/md/"
)
