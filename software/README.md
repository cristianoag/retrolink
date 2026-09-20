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

Firmware version `1.00` initializes USB CDC debug on the RP2040 Zero USB-C port and TinyUSB host mode on the USB-A port through Pico-PIO-USB. Standard HID joystick/gamepad report descriptors determine the control locations; the first report is decoded immediately, not used as a neutral baseline.

| MSX control | GPIO | DB9 pin | USB HID control |
| --- | --- | --- | --- |
| Up | 6 | 1 | Low Y, hat up, or D-pad up |
| Down | 7 | 2 | High Y, hat down, or D-pad down |
| Left | 8 | 3 | Low X, hat left, or D-pad left |
| Right | 9 | 4 | High X, hat right, or D-pad right |
| Button A | 10 | 6 | Button usage 1 |
| Button B | 11 | 7 | Button usage 2 |

All outputs assert low and release to input/high impedance, with internal pull-ups disabled. The documented BSS138 level shifters and external pull-ups remain required; never connect 5 V MSX signals directly to RP2040 GPIOs. GPIO12 / DB9 pin 8 is not driven by this joystick implementation.

Absolute X/Y axes use the descriptor's logical range: below 25% asserts left/up, above 75% asserts right/down, and the middle 50% is neutral. Four-way and eight-way hats are supported, including diagonal combinations for eight-way hats; values outside the logical range are neutral. Axis and D-pad directions are combined. Opposing directions cancel each other. Button usage numbers may not match the controller's printed labels.

States are kept separately per report ID and interface and combined for the MSX port. Detach, truncated reports, and failed receive requests release the affected interface's contribution without releasing another controller's held controls. A failed receive request is logged; reconnect the controller to restart reception. Unknown report IDs leave the current state unchanged. The LED pulses on newly asserted decoded controls.

The decoder supports absolute variable fields in Joystick/Game Pad application collections, signed/unsigned fields up to 32 bits, and report IDs. It uses fixed limits: 32 mapped fields, 8 input report IDs, 512 input payload bits per report, 32 explicit local usages, 8 collection levels, and 4 global push levels. Oversized or unsupported descriptors are logged as `supported=0` and do not drive outputs. Array controls, relative axes, vendor-specific protocols, local delimiters, and long descriptor items are not supported; unsupported fields are ignored when other usable controls exist. Mouse emulation is not implemented. These limits and mappings have not yet been validated against the user's physical controller.

Use a serial terminal on the USB-C CDC device to monitor boot, host initialization, HID mount/unmount, descriptor mapping status, receive failures, and decoded state changes. The state byte uses bits 0 through 5 for up, down, left, right, A, and B respectively.

The status LED implementation assumes the common RP2040 Zero onboard WS2812-compatible LED on GPIO 16. If a board variant uses a different LED circuit, update `include/retrolink/board_config.h` before building.

## Module Interfaces

- `joystick_state_t` is a bitmask of the six `JOYSTICK_*` controls, independent of GPIO assignments.
- `hid_joystick_parse()` initializes a descriptor mapping and returns false with an empty mapping for unsupported or malformed descriptors.
- `hid_joystick_decode()` updates the matching report state and returns the combined state; false indicates an invalid report and clears all states for that interface. Unknown IDs are ignored successfully. Pass a parsed mapping and a non-null output state pointer.
- `msx_port_init()` releases all six GPIOs; `msx_port_set_state()` applies a complete logical state and cancels opposing directions. The trigger-A setter/getter remain available and preserve the other controls.

## Host Tests

After a firmware build has created `software/build`, run from the repository root with a native GCC compiler:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I software/include software/src/hid_joystick.c software/tests/hid_joystick_test.c -o software/build/hid_joystick_test.exe
./software/build/hid_joystick_test.exe
gcc -std=c11 -Wall -Wextra -Werror -I software/tests/stubs -I software/include software/src/msx_port.c software/tests/msx_port_test.c -o software/build/msx_port_test.exe
./software/build/msx_port_test.exe
```

Tests cover descriptor bounds, report IDs, axes, hats, buttons, GPIO assignments, high-impedance release, held-control continuity, and opposing directions. GPIO tests use a stub; they do not establish electrical or USB runtime correctness. On hardware, verify each direction and button, simultaneous controls, neutral release, and unplugging while controls are held.