# RetroLink Software

Firmware source code lives under this directory. Generated UF2 files are copied to `../firmware/` by the CMake build.

## Build

Prerequisites:

- Raspberry Pi Pico SDK available through `PICO_SDK_PATH`.
- A Pico SDK version that provides TinyUSB host support for RP2040 and, for hardware revision `1.00`, PIO USB host support on GPIO 2 and GPIO 3.
- Pico-PIO-USB providing `pio_usb.h` and the `pico_pio_usb` CMake target; the Makefile defaults to the repo submodule at `../third_party/Pico-PIO-USB`.
- CMake and an ARM GCC toolchain supported by the Pico SDK.

Build from this directory:

```powershell
make
```

On this development machine, the Makefile defaults to the Pico SDK-managed CMake, Ninja, Python, SDK, ARM GCC toolchain, picotool, and pioasm paths under `C:/Users/Cristiano/.pico-sdk`. Pico-PIO-USB is repo-managed under `../third_party/Pico-PIO-USB`. Override `CMAKE`, `CMAKE_MAKE_PROGRAM`, `PYTHON3_EXECUTABLE`, `PICO_SDK_PATH`, `PICO_TOOLCHAIN_PATH`, `PICOTOOL_DIR`, `PIOASM_DIR`, or `PICO_PIO_USB_PATH` on the `make` command line if another installation should be used.

After cloning the repository, initialize dependencies with:

```powershell
git submodule update --init --recursive
```

Or configure and build from the repository root:

```powershell
cmake -S software -B software/build -DPICO_SDK_PATH=$env:PICO_SDK_PATH
cmake --build software/build
```

The expected firmware artifact is:

```text
firmware/retrolink-1.00.uf2
```

The Makefile uses the same CMake target and keeps the firmware version in the UF2 file name.

## Current Behavior

Firmware version `1.00` initializes USB CDC debug on the RP2040 Zero USB-C port and TinyUSB host mode on the USB-A port through Pico-PIO-USB. It listens for USB HID reports from a connected joystick or gamepad. When a report contains newly asserted bits in the first report bytes, the firmware pulses the RP2040 Zero status LED and writes debug messages to the USB-C CDC serial interface.

Use a serial terminal on the USB-C CDC device to monitor boot, host initialization, HID mount/unmount, receive-request failures, and button/report activity.

The status LED implementation assumes the common RP2040 Zero onboard WS2812-compatible LED on GPIO 16. If a board variant uses a different LED circuit, update `include/retrolink/board_config.h` before building.