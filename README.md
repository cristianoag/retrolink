# The Retro Hacker RetroLink

RetroLink is an open-source hardware and firmware project for connecting modern USB HID joysticks and mice to classic MSX computers through the MSX DB9 General Purpose port.

The first hardware revision uses an RP2040 Zero module on a custom adapter board with:

- USB-A host connector for the USB HID device.
- DB9 connector for the MSX joystick/general purpose port.
- MSX DB9 pin 5 as the installed adapter power source.
- 1N5819 Schottky diode protection to prevent USB-C programming power from backfeeding the MSX.
- BSS138 MOSFET level shifting with 10 kOhm pull-ups between RP2040 3.3 V GPIO and MSX 5 V signals.

## Versions

| Area | Version |
| --- | --- |
| Specification | `1.00` |
| Firmware | `1.00` |
| Hardware revision | `1.00` |

See [docs/specification.md](docs/specification.md) for the current technical specification and [docs/log.md](docs/log.md) for the version change log.

## Repository Layout

| Path | Purpose |
| --- | --- |
| [software](software) | RP2040 firmware source code using the Raspberry Pi Pico SDK, TinyUSB, and Pico-PIO-USB. |
| [firmware](firmware) | Generated UF2 firmware artifacts. |
| [hardware](hardware) | KiCad hardware project files by hardware revision. |
| [docs](docs) | Technical specification and version log. |
| [third_party/Pico-PIO-USB](third_party/Pico-PIO-USB) | Git submodule for GPIO-based USB host support on RP2040. |

## Firmware Build

Initialize submodules after cloning:

```powershell
git submodule update --init --recursive
```

Build from the firmware source folder:

```powershell
cd software
make
```

The build generates a versioned UF2 file:

```text
firmware/retrolink-1.00.uf2
```

The Makefile defaults to the local Pico SDK-managed toolchain paths used on the development machine. Override `PICO_SDK_PATH`, `PICO_TOOLCHAIN_PATH`, `CMAKE`, `CMAKE_MAKE_PROGRAM`, `PYTHON3_EXECUTABLE`, `PICOTOOL_DIR`, `PIOASM_DIR`, or `PICO_PIO_USB_PATH` if your toolchain is installed elsewhere.

## Current Firmware Behavior

Firmware version `1.00` brings up USB HID host support for a joystick or gamepad connected to the USB-A port. When a HID report contains newly asserted button/report bits, the RP2040 Zero status LED pulses.

The current status LED implementation assumes the common RP2040 Zero onboard WS2812-compatible LED on GPIO 16. Validate this against the exact board variant before treating the build as hardware-final.

## Hardware Status

Hardware revision `1.00` currently includes the initial KiCad project and schematic only:

- [hardware/1.00/retrolink.kicad_pro](hardware/1.00/retrolink.kicad_pro)
- [hardware/1.00/retrolink.kicad_sch](hardware/1.00/retrolink.kicad_sch)

No PCB layout has been created yet.

## Design Principles

- Do not invent hardware behavior; document assumptions and validate them.
- Keep firmware source, generated artifacts, hardware files, and documentation synchronized.
- Prefer readable, modular firmware over clever code.
- Keep USB host, HID parsing, MSX port signaling, configuration, platform, and diagnostics concerns separate.
- Document electrical decisions, connector pin assignments, voltage compatibility, timing constraints, and revision compatibility.
