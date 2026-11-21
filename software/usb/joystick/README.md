# RetroLink USB Joystick Firmware

USB joystick/gamepad firmware source code lives under this directory. Generated UF2 files are copied to `../../../firmware/usb/` by the CMake build. The USB mouse firmware for the same hardware is in [`../mouse/`](../mouse/README.md), and the MD variant is in [`../../md/`](../../md/README.md). The mouse and MD builds reuse this directory's shared LED, CDC debug, USB descriptor, and `joystick.h` sources.

## Build

Prerequisites:

- Raspberry Pi Pico SDK available through `PICO_SDK_PATH`.
- A Pico SDK version that provides TinyUSB host support for RP2040 and PIO USB host support on GPIO 27 and GPIO 28.
- Pico-PIO-USB providing `pio_usb.h` and the `pico_pio_usb` CMake target; the Makefile defaults to the repo submodule at `../../third_party/Pico-PIO-USB`.
- CMake and an ARM GCC toolchain supported by the Pico SDK.

Build from this directory:

```powershell
make
```

On Windows, the Makefile discovers the Pico SDK-managed tools and SDK under `%USERPROFILE%\.pico-sdk` (or the `PICO_SDK_ROOT` override), without depending on a particular account name or installed version. If managed Python is not present, CMake searches for Python on `PATH`. Pico-PIO-USB is repo-managed under `../../third_party/Pico-PIO-USB`. Override `PICO_SDK_ROOT`, `CMAKE`, `CMAKE_MAKE_PROGRAM`, `PYTHON3_EXECUTABLE`, `PICO_SDK_PATH`, `PICO_TOOLCHAIN_PATH`, `PICOTOOL_DIR`, `PIOASM_DIR`, or `PICO_PIO_USB_PATH` on the `make` command line if your installation differs.

After cloning the repository, initialize dependencies with:

```powershell
git submodule update --init --recursive
```

Or configure and build from the repository root:

```powershell
cmake -S software/usb/joystick -B software/usb/joystick/build -DPICO_SDK_PATH=$env:PICO_SDK_PATH
cmake --build software/usb/joystick/build
```

The expected firmware artifact is:

```text
firmware/usb/retrolink-joystick-1.20.uf2
```

The Makefile passes `FIRMWARE_VERSION` to CMake, so the UF2 file name and the firmware's reported version stay in sync. Joystick UF2 files use the `retrolink-joystick-<version>.uf2` name; releases `1.00` and `1.10` were renamed from `retrolink-<version>.uf2` without changing their contents.

## Current Behavior

Firmware version `1.20` initializes USB CDC debug on the RP2040 Zero USB-C port and TinyUSB host mode on the USB-A port through Pico-PIO-USB. Standard HID joystick/gamepad report descriptors determine the control locations; the first report is decoded immediately, not used as a neutral baseline.

The current build uses GPIO27 for USB-A D+ and GPIO28 for D-. This consecutive pair satisfies Pico-PIO-USB's DPDM pinout and does not overlap the MSX or status LED assignments. USB-C uses the native USB controller and is unaffected. GPIO27/28 must not also be used as ADC inputs or for other peripherals.

This build matches the GPIO assignments of USB hardware revision `1.10`; it is not compatible with the original hardware revision `1.00` pin assignments documented in the [specification](../../docs/specification.md). Verify the actual board wiring, USB enumeration, and each MSX control on hardware before use.

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

States are kept separately per report ID and interface and combined for the MSX port. Detach, truncated reports, and failed receive requests release the affected interface's contribution without releasing another controller's held controls. Unknown report IDs leave the current state unchanged. The LED pulses on newly asserted decoded controls.

Receivers can be connected after boot. After HID mount, the host checks each interface every 100 ms and queues a receive request if its endpoint is idle, so a failed submission no longer permanently stops polling. Normal report callbacks still queue the next request immediately; pending transfers are left alone, including receivers waiting for a wireless controller to pair or wake. Receive failures are logged once per failure episode, with a recovery message when a request is successfully queued. Disconnect cancels retries and releases that interface's controls.

While no supported HID joystick is mounted, the host also checks USB-A discovery every five seconds. If a receiver is electrically connected but has not exposed a usable gamepad, it holds USB-A in bus reset for 50 ms, removes stale TinyUSB device/interface state, cancels any pending address-zero enumeration transfer, and queues fresh enumeration. Retries continue without a pairing time limit. This handles discovery failures that receive retries alone cannot fix. USB-C CDC stays active; the Pico, PIO resources, and USB host stack are not reinitialized.

Automatic rediscovery stops as soon as any supported joystick mapping exists, even if its receive transfer is still waiting for wireless input. Thus already recognized controllers, including controllers behind a hub, are not periodically reset. If no supported joystick exists, recovery resets the entire USB-A tree, including any unsupported interfaces. This does not add vendor-specific pairing protocols or support for non-HID controllers.

CDC now logs `USB discovery: no supported joystick; restarting USB-A enumeration`, followed by `USB discovery: receiver present; enumeration queued`, or reports that no electrical USB connection is detected. Physical late-connection validation is still required: after a delayed pairing, expect `HID mounted` and a `supported=1` mapping. Capture these discovery messages if it still fails.

The decoder supports absolute variable fields in Joystick/Game Pad application collections, signed/unsigned fields up to 32 bits, and report IDs. It uses fixed limits: 32 mapped fields, 8 input report IDs, 512 input payload bits per report, 32 explicit local usages, 8 collection levels, and 4 global push levels. Oversized or unsupported descriptors are logged as `supported=0` and do not drive outputs. Array controls, relative axes, vendor-specific protocols, local delimiters, and long descriptor items are not supported; unsupported fields are ignored when other usable controls exist. USB mice are handled by the separate [mouse firmware](../mouse/README.md). These limits and mappings have not yet been validated against the user's physical controller.

Use a serial terminal on the USB-C CDC device to monitor boot, host initialization, HID mount/unmount, descriptor mapping status, receive failures, and decoded state changes. The state byte uses bits 0 through 5 for up, down, left, right, A, and B respectively.

The status LED implementation assumes the common RP2040 Zero onboard WS2812-compatible LED on GPIO 16. If a board variant uses a different LED circuit, update `include/retrolink/board_config.h` before building.

## Module Interfaces

- `joystick_state_t` is a bitmask of the six `JOYSTICK_*` controls, independent of GPIO assignments.
- `hid_joystick_parse()` initializes a descriptor mapping and returns false with an empty mapping for unsupported or malformed descriptors.
- `hid_joystick_decode()` updates the matching report state and returns the combined state; false indicates an invalid report and clears all states for that interface. Unknown IDs are ignored successfully. Pass a parsed mapping and a non-null output state pointer.
- `usb_host_task()` must run alongside `tuh_task()` to recover idle HID endpoints and perform timed USB-A rediscovery; `tuh_task()` processes removal events while the recovery reset is held.
- `msx_port_init()` releases all six GPIOs; `msx_port_set_state()` applies a complete logical state and cancels opposing directions. The trigger-A setter/getter remain available and preserve the other controls.

## Host Tests

After a firmware build has created `software/usb/joystick/build`, run from the repository root with a native GCC compiler:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I software/usb/joystick/include software/usb/joystick/src/hid_joystick.c software/usb/joystick/tests/hid_joystick_test.c -o software/usb/joystick/build/hid_joystick_test.exe
./software/usb/joystick/build/hid_joystick_test.exe
gcc -std=c11 -Wall -Wextra -Werror -I software/usb/joystick/tests/stubs -I software/usb/joystick/include software/usb/joystick/src/msx_port.c software/usb/joystick/tests/msx_port_test.c -o software/usb/joystick/build/msx_port_test.exe
./software/usb/joystick/build/msx_port_test.exe
gcc -std=c11 -Wall -Wextra -Werror -I software\usb\joystick\tests\stubs -I software\usb\joystick\include software\usb\joystick\src\usb_host.c software\usb\joystick\src\hid_joystick.c software\usb\joystick\tests\usb_host_test.c -o software\usb\joystick\build\usb_host_test.exe
.\software\usb\joystick\build\usb_host_test.exe
```

With `PICO_SDK_PATH` set, also run the discovery integration test against the SDK's real TinyUSB host, hub, and HID drivers (only the hardware controller and clock are simulated):

```powershell
$tinyusb = "$env:PICO_SDK_PATH\lib\tinyusb\src"
gcc -std=c11 -Wall -Wextra -Werror -I software\usb\joystick\tests\tinyusb -I "$tinyusb" -I software\usb\joystick\tests\stubs -I software\usb\joystick\include software\usb\joystick\src\usb_host.c software\usb\joystick\src\hid_joystick.c software\usb\joystick\tests\usb_discovery_test.c "$tinyusb\tusb.c" "$tinyusb\host\usbh.c" "$tinyusb\host\hub.c" "$tinyusb\class\hid\hid_host.c" "$tinyusb\common\tusb_fifo.c" -o software\usb\joystick\build\usb_discovery_test.exe
.\software\usb\joystick\build\usb_discovery_test.exe
```

The integration test exercises TinyUSB's finite enumeration failures, indefinitely pending address-zero transfers, and stalled report-descriptor requests. It verifies successful discovery and decoded button input after receiver readiness changes, and no repeated initialization or resets after mounting.

Tests cover descriptor bounds, report IDs, axes, hats, buttons, the exact GPIO assignments, high-impedance release, held-control continuity, and opposing directions. GPIO tests also verify that OUT/strobe and unrelated pins remain untouched. USB host tests cover late mount, initial/rearm submission failures, exact 100 ms retry timing, busy endpoints, idle endpoints without a callback, independent devices/interfaces, report-state clearing, retry cancellation on disconnect, and address reuse. Discovery tests cover repeated five-second retries, the 50 ms reset, stale address-zero transfer cancellation, unsupported-to-supported receiver transitions, downstream enumeration cancellation, reset errors, pending-event deferral, unplugging during reset, and no resets of mapped controllers waiting for input. GPIO and USB host tests use stubs; they do not establish electrical or USB runtime correctness.

On hardware, verify each direction and button, simultaneous controls, neutral release, and unplugging while controls are held. Also boot without the receiver, insert it after startup, wait at least 30 seconds before pairing/waking the controller, and unplug/reinsert it without resetting the Pico. Repeat with a receiver connected at boot and with a wired HID gamepad to check existing behavior. Keep the USB-C terminal open to verify it stays responsive during USB-A recovery.