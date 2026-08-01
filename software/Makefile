FIRMWARE_VERSION := 1.00
BUILD_DIR := build
UF2_NAME := retrolink-$(FIRMWARE_VERSION).uf2
UF2_OUTPUT := ../firmware/$(UF2_NAME)

CMAKE ?= C:/Users/Cristiano/.pico-sdk/cmake/v3.29.9/bin/cmake
CMAKE_BUILD_TYPE ?= Release
PICO_SDK_PATH ?= C:/Users/Cristiano/.pico-sdk/sdk/2.2.0
PICO_TOOLCHAIN_PATH ?= C:/Users/Cristiano/.pico-sdk/toolchain/14_2_Rel1
PICOTOOL_DIR ?= C:/Users/Cristiano/.pico-sdk/picotool/2.1.1/picotool
PIOASM_DIR ?= C:/Users/Cristiano/.pico-sdk/tools/2.2.0/pioasm
PICO_PIO_USB_PATH ?= $(abspath ../third_party/Pico-PIO-USB)

ifeq ($(OS),Windows_NT)
	CMAKE_GENERATOR ?= Ninja
	CMAKE_MAKE_PROGRAM ?= C:/Users/Cristiano/.pico-sdk/ninja/v1.12.1/ninja.exe
	PYTHON3_EXECUTABLE ?= C:/Users/Cristiano/.pico-sdk/python/3.12.6/python.exe
else
	CMAKE_GENERATOR ?=
endif

CMAKE_GENERATOR_FLAG := $(if $(CMAKE_GENERATOR),-G "$(CMAKE_GENERATOR)",)
CMAKE_MAKE_PROGRAM_FLAG := $(if $(CMAKE_MAKE_PROGRAM),-DCMAKE_MAKE_PROGRAM="$(CMAKE_MAKE_PROGRAM)",)
PYTHON3_EXECUTABLE_FLAG := $(if $(PYTHON3_EXECUTABLE),-DPython3_EXECUTABLE="$(PYTHON3_EXECUTABLE)",)
PICO_TOOLCHAIN_PATH_FLAG := $(if $(PICO_TOOLCHAIN_PATH),-DPICO_TOOLCHAIN_PATH="$(PICO_TOOLCHAIN_PATH)",)
PICOTOOL_DIR_FLAG := $(if $(PICOTOOL_DIR),-Dpicotool_DIR="$(PICOTOOL_DIR)",)
PIOASM_DIR_FLAG := $(if $(PIOASM_DIR),-Dpioasm_DIR="$(PIOASM_DIR)",)
PICO_PIO_USB_PATH_FLAG := $(if $(PICO_PIO_USB_PATH),-DPICO_PIO_USB_PATH="$(PICO_PIO_USB_PATH)",)

.PHONY: all configure clean distclean uf2

all: uf2

configure:
	$(CMAKE) $(CMAKE_GENERATOR_FLAG) $(CMAKE_MAKE_PROGRAM_FLAG) $(PYTHON3_EXECUTABLE_FLAG) $(PICO_TOOLCHAIN_PATH_FLAG) $(PICOTOOL_DIR_FLAG) $(PIOASM_DIR_FLAG) $(PICO_PIO_USB_PATH_FLAG) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) -DPICO_SDK_PATH="$(PICO_SDK_PATH)"

uf2: configure
	$(CMAKE) --build $(BUILD_DIR) --target retrolink_export_uf2
	@echo Generated $(UF2_OUTPUT)

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean

distclean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)
	$(CMAKE) -E rm -f $(UF2_OUTPUT)