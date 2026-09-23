# RetroLink MD Software

RP2040 Zero firmware `1.00` for RetroLink MD hardware revision `1.00`. It polls standard Mega Drive/Genesis 3-button and 6-button controllers and translates directions plus B/C into MSX joystick signals. This is a separate build from [RetroLink USB](../usb/); do not flash the USB UF2 onto the MD board.

## Wiring

### Mega Drive connector (male DE9)

| MD pin | Signal | GPIO | RP2040 direction |
| --- | --- | --- | --- |
| 1 | MD_UP / D0 | 28 | Input through 5 V-safe level conversion |
| 2 | MD_DOWN / D1 | 26 | Input through 5 V-safe level conversion |
| 3 | MD_LEFT / D2 | 14 | Input through 5 V-safe level conversion |
| 4 | MD_RIGHT / D3 | 12 | Input through 5 V-safe level conversion |
| 5 | Controller supply | None | Protected controller power rail |
| 6 | MD_BA / D4 | 27 | Input through 5 V-safe level conversion |
| 7 | MD_THSEL / SELECT | 15 | Output through noninverting 3.3 V-to-controller-voltage translation |
| 8 | GND | None | Board ground |
| 9 | MD_CSTART / D5 | 13 | Input through 5 V-safe level conversion |

### MSX connector (female DE9)

| MSX pin | Signal | GPIO | Behavior |
| --- | --- | --- | --- |
| 1 | Up | 0 | Active-low / high-impedance release |
| 2 | Down | 2 | Active-low / high-impedance release |
| 3 | Left | 4 | Active-low / high-impedance release |
| 4 | Right | 6 | Active-low / high-impedance release |
| 5 | MSX +5 V | None | Protected board power input |
| 6 | Trigger A | 1 | MD B |
| 7 | Trigger B | 3 | MD C |
| 8 | OUT / joystick common | 5 | Input; high releases all six outputs |
| 9 | GND | None | Board ground |

GPIO16 remains the onboard WS2812-compatible status LED. GPIO13 is **not** an MSX voltage-sense input in this variant. GPIO27/28 are **not** USB-A signals. UART stdio is disabled so GPIO0/1 remain available for MSX.

The SDK's USB enumeration workaround for older RP2040 B0/B1 chips temporarily takes over GPIO15. This build explicitly disables it with `TUD_OPT_RP2040_USB_DEVICE_ENUMERATION_FIX=0` to preserve MD SELECT. Prefer B2 silicon for USB-C diagnostics; on B0/B1 chips, enumeration through some USB hubs may fail without the workaround.

**Electrical prerequisites:** RP2040 pins are not 5 V tolerant. Provide 5 V-safe MD data inputs, a level-translated TH output, six MSX low/release interfaces, and a 5 V-safe MSX common input. Firmware assumes noninverting interfaces. Use external pull-ups on the MD connector side to define unplugged readings; the firmware also enables RP2040 input pull-ups. An LVC buffer's output pull-up alone does not define a floating buffer input. The four-component MD BOM does not include the required level-conversion/protection circuits.

MD pin 8 and MSX pin 9 are ground. **Do not ground MSX pin 8.** Validate connector orientation, the supply after the protection diode, MSX current budget, and USB-C backfeed protection before connecting either system. Renders and compiled firmware are not electrical validation.

## Behavior

- MD directions map directly; opposing directions cancel.
- MD B maps to MSX trigger A; MD C maps to MSX trigger B.
- A, Start, X, Y, Z, and Mode are decoded for diagnostics but do not activate MSX controls.
- GPIO15 idles high. Each poll performs four low/high TH cycles, with a 10 us settling delay after each transition. Samples are taken at idle high, first low, third low/high, and fourth low.
- A 3 ms initial high interval resets the pad protocol. The next poll is scheduled at least 5 ms after the previous poll and its diagnostics finish, preserving the reset interval even when software is delayed. A normal sampled frame takes approximately 80 us plus GPIO/software overhead.
- A frame exceeding 500 us is rejected rather than using potentially misaligned button data. Interrupts remain enabled during polling to service the MSX common input. The timing budget and settling delays must be validated on real pads, especially clones.
- Low-phase identification and the extended identification/end phases distinguish standard 3- and 6-button pads. Absent or invalid frames release controls on that poll; a later valid frame reconnects automatically. Master System pads, mice, multitaps, and vendor-specific extensions are not supported.
- MSX output latches remain zero; only GPIO direction changes. GPIO5 rise/fall interrupts release or restore the current held controls according to MSX common. This is software-interrupt-driven, not zero-latency hardware gating or MSX mouse emulation; validate common-to-output timing on the intended MSX.
- The LED pulses on newly asserted mapped controls.

Native USB-C CDC provides firmware identity, controller type, raw button bits, mapped MSX state, connection/invalid-frame changes, and timing errors. It does not wait for a terminal at boot. Serial writes use zero host-buffer wait timeout; debug text can be dropped when the host is not reading, rather than waiting for buffer space. USB CDC reset commands are disabled; use the physical BOOT/RESET buttons for reflashing.

Diagnostic `buttons` bits 0..11 are Up, Down, Left, Right, B, C, A, Start, Z, Y, X, Mode. `MSX` bits 0..5 are Up, Down, Left, Right, trigger A, trigger B (the requested controls before common-line gating).

## Build

Prerequisites: Raspberry Pi Pico SDK with its TinyUSB submodule initialized, CMake, Ninja or a compatible generator, ARM GCC, Python, and the Pico SDK tools. Pico-PIO-USB is not used by this variant.

From the repository root:

```powershell
make -C software\md
```

The Makefile follows the USB project's Windows tool discovery under `%USERPROFILE%\.pico-sdk`. Override `PICO_SDK_ROOT`, `PICO_SDK_PATH`, `PICO_TOOLCHAIN_PATH`, `CMAKE`, `CMAKE_MAKE_PROGRAM`, `PYTHON3_EXECUTABLE`, `PICOTOOL_DIR`, or `PIOASM_DIR` when necessary.

Output: [firmware/md/retrolink-md-1.00.uf2](../../firmware/md/retrolink-md-1.00.uf2). The firmware version is defined in `CMakeLists.txt`; hardware assignments and polling timings live in `include/retrolink/board_config.h`.

The target reuses the unchanged USB project's `joystick.h`, `status_led.h`, `status_led.c`, and `ws2812.pio`. Its own include directory comes first, so the shared LED driver uses the MD board configuration. MD protocol and MSX common-line handling are separate modules; no USB host code is linked.

`md_port_poll()` requires a non-null output state and at least the configured TH-high reset interval between calls. It always ends with TH high and returns an explicit poll result; the main loop logs absent/invalid or timing-error status and releases MSX controls. `md_pad_decode()` operates on packed D0..D5 samples; `md_pad_to_msx()` maps B/C and cancels opposing directions.

## Tests

Native CMake test mode is available with `RETROLINK_HOST_TESTS=ON`; use a separate `build-tests` directory and a native C compiler, not ARM GCC. Build and run `md_pad_test` and `md_io_test` through CMake Tools/CTest.

Alternatively, after creating `software/md/build`, run the native GCC tests from the repository root:

```powershell
gcc -std=c11 -Wall -Wextra -Werror -I software\md\include -I software\usb\include software\md\src\md_pad.c software\md\tests\md_pad_test.c -o software\md\build\md_pad_test.exe
.\software\md\build\md_pad_test.exe
gcc -std=c11 -Wall -Wextra -Werror -I software\md\tests\stubs -I software\md\include -I software\usb\include software\md\src\md_pad.c software\md\src\md_port.c software\md\src\msx_output.c software\md\tests\md_io_test.c -o software\md\build\md_io_test.exe
.\software\md\build\md_io_test.exe
```

For MSYS2 UCRT64 GCC on Windows, ensure its `bin` directory is on `PATH` for both compilation and execution.

Tests cover all 256 three-button and 4096 six-button states, exact pin assignments/uniqueness, B/C mapping, unmapped buttons, opposing directions, invalid frames, SELECT sequencing and settling, the 500 us timeout boundary, timer wraparound, detach/reconnect, high-impedance release, and common-line changes during polling. GPIO tests use a stub, not physical hardware.

Before use, measure voltage levels and TH/common timing, then test directions, diagonals, B/C, held controls, common-line toggles, unplug/replug, three- and six-button controllers, and operation without a USB-C terminal.

Protocol references: [3-button behavior](https://www.raspberryfield.life/2019/02/15/sega-mega-drive-genesis-3-button-abc-controller/) and [6-button sequence](https://www.raspberryfield.life/2019/03/25/sega-mega-drive-genesis-6-button-xyz-controller/).
