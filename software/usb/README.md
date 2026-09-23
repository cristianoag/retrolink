# RetroLink USB Software

USB firmware source code lives under this directory. Generated UF2 files are copied to `../../firmware/usb/` by the CMake build. The planned MD variant has a separate directory at `../md/`.

## Build

Prerequisites:

- Raspberry Pi Pico SDK available through `PICO_SDK_PATH`.
- A Pico SDK version that provides TinyUSB host support for RP2040 and PIO USB host support on GPIO 27 and GPIO 28.
- Pico-PIO-USB providing `pio_usb.h` and the `pico_pio_usb` CMake target; the Makefile defaults to the repo submodule at `../third_party/Pico-PIO-USB`.
- CMake and an ARM GCC toolchain supported by the Pico SDK.

Build from this directory:

```powershell
make
```

On Windows, the Makefile discovers the Pico SDK-managed tools and SDK under `%USERPROFILE%\.pico-sdk` (or the `PICO_SDK_ROOT` override), without depending on a particular account name or installed version. If managed Python is not present, CMake searches for Python on `PATH`. Pico-PIO-USB is repo-managed under `../third_party/Pico-PIO-USB`. Override `PICO_SDK_ROOT`, `CMAKE`, `CMAKE_MAKE_PROGRAM`, `PYTHON3_EXECUTABLE`, `PICO_SDK_PATH`, `PICO_TOOLCHAIN_PATH`, `PICOTOOL_DIR`, `PIOASM_DIR`, or `PICO_PIO_USB_PATH` on the `make` command line if your installation differs.

After cloning the repository, initialize dependencies with:

```powershell
git submodule update --init --recursive
```

Or configure and build from the repository root:

```powershell
cmake -S software/usb -B software/usb/build -DPICO_SDK_PATH=$env:PICO_SDK_PATH
cmake --build software/usb/build
```

The expected firmware artifact is:

```text
firmware/usb/retrolink-1.00.uf2
```

The Makefile uses the same CMake target and keeps the firmware version in the UF2 file name.

## Current Behavior

Firmware version `1.00` initializes USB CDC debug on the RP2040 Zero USB-C port and TinyUSB host mode on the USB-A port through Pico-PIO-USB. Standard HID joystick/gamepad report descriptors determine the control locations; the first report is decoded immediately, not used as a neutral baseline.

The current build uses GPIO27 for USB-A D+ and GPIO28 for D-. This consecutive pair satisfies Pico-PIO-USB's DPDM pinout and does not overlap the MSX or status LED assignments. USB-C uses the native USB controller and is unaffected. GPIO27/28 must not also be used as ADC inputs or for other peripherals.

This build requires the updated USB-A and MSX wiring; it is not compatible with the original hardware revision `1.00` pin assignments documented in the [specification](../../docs/specification.md). Firmware and hardware version identifiers remain unchanged. Verify the actual board wiring, USB enumeration, and each MSX control on hardware before use.

| MSX control | GPIO | DB9 pin | USB HID control |
| --- | --- | --- | --- |
| Up | 0 | 1 | Low Y, hat up, or D-pad up |
| Down | 2 | 2 | High Y, hat down, or D-pad down |
| Left | 4 | 3 | Low X, hat left, or D-pad left |
| Right | 6 | 4 | High X, hat right, or D-pad right |
| Button A | 1 | 6 | Button usage 1 |
| Button B | 3 | 7 | Button usage 2 |
| OUT / strobe (input from MSX) | 5 | 8 | Not used in joystick mode |

All six control outputs assert low and release to input/high impedance, with internal pull-ups disabled. The documented BSS138 level shifters and external pull-ups remain required; never connect 5 V MSX signals directly to RP2040 GPIOs. GPIO5 / DB9 pin 8 is reserved as an input from the MSX and is neither initialized, read, nor driven by this joystick implementation.

The unused USB VBUS enable/fault reservations on GPIO4/5 have been removed; the updated board must not connect VBUS control/fault circuitry to these pins. UART stdio remains disabled, leaving GPIO0/1 available for MSX signals. Do not enable UART or other peripheral functions on GPIO0-6 while using this mapping.

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

After a firmware build has created `software/usb/build`, run from the repository root with a native GCC compiler:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I software/usb/include software/usb/src/hid_joystick.c software/usb/tests/hid_joystick_test.c -o software/usb/build/hid_joystick_test.exe
./software/usb/build/hid_joystick_test.exe
gcc -std=c11 -Wall -Wextra -Werror -I software/usb/tests/stubs -I software/usb/include software/usb/src/msx_port.c software/usb/tests/msx_port_test.c -o software/usb/build/msx_port_test.exe
./software/usb/build/msx_port_test.exe
```

Tests cover descriptor bounds, report IDs, axes, hats, buttons, the exact GPIO assignments, high-impedance release, held-control continuity, and opposing directions. GPIO tests also verify that OUT/strobe and unrelated pins remain untouched. GPIO tests use a stub; they do not establish electrical or USB runtime correctness. On hardware, verify each direction and button, simultaneous controls, neutral release, and unplugging while controls are held.