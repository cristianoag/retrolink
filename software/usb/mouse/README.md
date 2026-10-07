# RetroLink USB Mouse Firmware

Firmware for the **same RetroLink USB board** as the [joystick firmware](../joystick/README.md) (hardware revision `1.10`, RP2040 Zero). It turns a USB HID mouse on the USB-A port into an MSX-compatible mouse on the DB9 connector. Flash either the joystick UF2 or the mouse UF2; the firmware decides how the board behaves, and the PCB is not modified.

The build reuses the joystick directory's `debug_cdc.c`, `status_led.c`, `usb_descriptors.c`, `ws2812.pio`, `tusb_config.h`, and `joystick.h`. This directory's `include/` comes first, so those shared sources use the mouse `board_config.h`.

## Build

Prerequisites and tool overrides are the same as the [joystick build](../joystick/README.md#build). From this directory:

```powershell
make
```

Or from the repository root:

```powershell
make -C software/usb/mouse
```

The expected firmware artifact is:

```text
firmware/usb/retrolink-mouse-1.00.uf2
```

The Makefile passes `FIRMWARE_VERSION` to CMake, so the UF2 file name (`retrolink-mouse-<version>.uf2`) and the version reported over CDC stay in sync. Mouse firmware versions are independent of the joystick firmware versions.

## Current Behavior

Mouse firmware version `1.00` uses the joystick firmware's GPIO assignments, USB-A host pins (GPIO27 D+, GPIO28 D-), USB-C CDC debug port, and GPIO16 status LED.

| MSX mouse signal | GPIO | DB9 pin | Source |
| --- | --- | --- | --- |
| Data bit 0 | 0 | 1 | Current nibble bit 0 |
| Data bit 1 | 2 | 2 | Current nibble bit 1 |
| Data bit 2 | 4 | 3 | Current nibble bit 2 |
| Data bit 3 | 6 | 4 | Current nibble bit 3 |
| Left button | 1 | 6 | USB button 1 (left) |
| Right button | 3 | 7 | USB button 2 (right) |
| Strobe (input from MSX) | 5 | 8 | Read by the firmware |

As in the joystick firmware, all six outputs pull low or release to high impedance. Internal pull-ups are disabled. A nibble bit of `0` pulls its line low, and a `1` releases it. GPIO5 is a high-impedance input with internal pulls disabled and depends on the board's BSS138 level shifter and pull-ups. Never connect 5 V MSX signals directly to RP2040 GPIOs.

### MSX mouse protocol

Each change on DB9 pin 8 selects the next 4-bit nibble on pins 1-4:

| Pin 8 edge | Nibble presented |
| --- | --- |
| Rising (first after idle) | X offset bits 7-4 |
| Falling | X offset bits 3-0 |
| Rising | Y offset bits 7-4 |
| Falling | Y offset bits 3-0 |

Offsets are signed 8-bit values with the MSX sign convention: positive X means the mouse moved left, and positive Y means it moved up. USB HID uses the opposite signs, so the firmware inverts them. Each read sends at most ±127. Any larger movement carries over to later reads (backlog limited to ±1024 counts).

The MSX BIOS reads two complete sequences per scan, using the second to tell a mouse from a trackball. The firmware returns the movement in the first sequence and zero in the second, so the BIOS detects a mouse. If no strobe edge occurs for more than 1.5 ms (`RETROLINK_MOUSE_STROBE_TIMEOUT_US`), the sequence restarts. The next rising edge then sends newly accumulated movement. These timings follow the openMSX mouse emulation; they have not yet been measured against a physical MSX mouse.

Core 1 runs a dedicated loop from RAM. It polls GPIO5 and updates the outputs within well under a microsecond, independent of USB host activity on core 0. Core 0 runs TinyUSB, decodes reports, and sends the movement, buttons, and mode to core 1.

### USB mouse support

- Boot-protocol mice: TinyUSB puts boot-subclass mouse interfaces in boot protocol, so the fixed format (buttons, signed 8-bit X, signed 8-bit Y) is used.
- Report-protocol mice: other interfaces are decoded from their report descriptor. The decoder looks for relative X/Y and buttons 1-3 in Generic Desktop Mouse application collections. It supports report IDs and signed or unsigned fields up to 32 bits, such as the 12- or 16-bit axes used by wireless receivers.
- Boot keyboards, absolute-position pointers, joysticks, and descriptors without relative X/Y are ignored. Reports with unknown IDs, such as consumer controls, are skipped.
- USB movement is divided by `RETROLINK_MOUSE_DIVISOR` (default `2`), keeping the remainder so slow movement is not lost. Increase the divisor to slow the pointer or set it to `1` for full speed. The wheel and the middle button are not used.
- Buttons from multiple mice are combined. Movement from all mice is added together.

While no supported mouse is mounted, all DB9 outputs are released (the MSX sees no device). After a mouse mounts, the port answers the mouse protocol. It drives all four data lines low until the first strobe sequence. The LED pulses on movement and on newly pressed buttons.

### Joystick emulation

Like an original MSX mouse, the adapter can act as a joystick. To select joystick mode, hold the **left button** while plugging in the mouse, or while powering on the MSX with the mouse connected. If the first report within 1 s after mounting shows the left button pressed, joystick mode stays on until every mouse is removed. In this mode, the strobe is ignored. Movement is sampled every 20 ms and converted to up to eight directions on pins 1-4. A move needs at least 2 scaled counts on an axis to register. Left and right buttons drive triggers A and B. Some mice do not send a report until the button state changes, so this selection still needs testing on real mice.

### Diagnostics

Open the USB-C CDC port to see boot information, HID mount/unmount messages, `HID mouse mapping` support status, the selected mode (`released`, `MSX mouse`, `joystick emulation`), button changes, and receive failures. Movement is not logged.

## Module Interfaces

- `hid_mouse_parse()` builds a report layout from a descriptor; `hid_mouse_init_boot()` selects the boot layout. `hid_mouse_decode()` returns `OK`, `IGNORED` (unknown report ID or no mapped controls), or `INVALID` (null or truncated report).
- `msx_mouse_*` is the hardware-independent protocol: strobe-phase state machine, nibble/line conversion, movement scaling, joystick-emulation directions, and button mapping.
- `mouse_port_*` owns the GPIOs and the core 1 loop. Core 0 sets the mode, buttons, joystick directions, and movement.

## Host Tests

After a firmware build has created `software/usb/mouse/build`, run from the repository root with a native GCC compiler (for MSYS2 UCRT64, put its `bin` directory on `PATH`):

```powershell
gcc -std=c11 -Wall -Wextra -Werror -UNDEBUG -I software/usb/mouse/include -I software/usb/joystick/include software/usb/mouse/src/hid_mouse.c software/usb/mouse/tests/hid_mouse_test.c -o software/usb/mouse/build/hid_mouse_test.exe
./software/usb/mouse/build/hid_mouse_test.exe
gcc -std=c11 -Wall -Wextra -Werror -UNDEBUG -I software/usb/mouse/include -I software/usb/joystick/include software/usb/mouse/src/msx_mouse.c software/usb/mouse/tests/msx_mouse_test.c -o software/usb/mouse/build/msx_mouse_test.exe
./software/usb/mouse/build/msx_mouse_test.exe
```

The tests cover:

- Boot, basic, and report-ID descriptors with 12-bit axes, and unsupported or truncated descriptors.
- The BIOS strobe sequence, the zeroed alternate cycle, the nibble-to-pin mapping, ±127 clamping with carry, timeout resynchronization, and timer wraparound.
- Scaling remainders, the joystick-emulation directions, and the button mapping.

The core 1 GPIO loop is not covered by host tests. On hardware, test:

- Pointer direction in MSX-BIOS software and MSX-DOS2 tools.
- Both buttons, slow and fast movement, and the 1.5 ms timeout.
- Joystick-emulation selection.
- Unplug/replug.
- BSS138 rise times on pins 1-4 and 8.
