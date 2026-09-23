BUILD_DIR ?= build
CMAKE_BUILD_TYPE ?= Release

ifeq ($(OS),Windows_NT)
	PICO_SDK_ROOT ?= $(subst \,/,$(USERPROFILE))/.pico-sdk
	CMAKE ?= $(or $(lastword $(sort $(wildcard $(PICO_SDK_ROOT)/cmake/*/bin/cmake.exe))),cmake)
	CMAKE_GENERATOR ?= Ninja
	CMAKE_MAKE_PROGRAM ?= $(lastword $(sort $(wildcard $(PICO_SDK_ROOT)/ninja/*/ninja.exe)))
	PYTHON3_EXECUTABLE ?= $(lastword $(sort $(wildcard $(PICO_SDK_ROOT)/python/*/python.exe)))
else
	PICO_SDK_ROOT ?= $(HOME)/.pico-sdk
	CMAKE ?= cmake
	CMAKE_GENERATOR ?=
endif

PICO_SDK_PATH ?= $(lastword $(sort $(wildcard $(PICO_SDK_ROOT)/sdk/*)))
PICO_TOOLCHAIN_PATH ?= $(lastword $(sort $(wildcard $(PICO_SDK_ROOT)/toolchain/*)))
PICOTOOL_DIR ?= $(lastword $(sort $(wildcard $(PICO_SDK_ROOT)/picotool/*)))/picotool
PIOASM_DIR ?= $(lastword $(sort $(wildcard $(PICO_SDK_ROOT)/tools/*)))/pioasm

CMAKE_GENERATOR_FLAG := $(if $(CMAKE_GENERATOR),-G "$(CMAKE_GENERATOR)",)
CMAKE_MAKE_PROGRAM_FLAG := $(if $(CMAKE_MAKE_PROGRAM),-DCMAKE_MAKE_PROGRAM="$(CMAKE_MAKE_PROGRAM)",)
PYTHON3_EXECUTABLE_FLAG := $(if $(PYTHON3_EXECUTABLE),-DPython3_EXECUTABLE="$(PYTHON3_EXECUTABLE)",)
PICO_TOOLCHAIN_PATH_FLAG := $(if $(PICO_TOOLCHAIN_PATH),-DPICO_TOOLCHAIN_PATH="$(PICO_TOOLCHAIN_PATH)",)
PICOTOOL_DIR_FLAG := $(if $(wildcard $(PICOTOOL_DIR)/picotoolConfig.cmake),-Dpicotool_DIR="$(PICOTOOL_DIR)",)
PIOASM_DIR_FLAG := $(if $(wildcard $(PIOASM_DIR)/pioasmConfig.cmake),-Dpioasm_DIR="$(PIOASM_DIR)",)
PICO_SDK_PATH_FLAG := $(if $(PICO_SDK_PATH),-DPICO_SDK_PATH="$(PICO_SDK_PATH)",)

.PHONY: all configure uf2 clean
all: uf2

configure:
	"$(CMAKE)" $(CMAKE_GENERATOR_FLAG) $(CMAKE_MAKE_PROGRAM_FLAG) $(PYTHON3_EXECUTABLE_FLAG) $(PICO_TOOLCHAIN_PATH_FLAG) $(PICOTOOL_DIR_FLAG) $(PIOASM_DIR_FLAG) $(PICO_SDK_PATH_FLAG) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

uf2: configure
	"$(CMAKE)" --build $(BUILD_DIR) --target retrolink_md_export_uf2

clean:
	"$(CMAKE)" --build $(BUILD_DIR) --target clean
