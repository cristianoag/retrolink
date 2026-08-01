# RetroLink Version Change Log

## Firmware

### Firmware v1.00 - 2026-08-01

- Defined initial firmware-facing module boundaries and GPIO constants for the future RP2040 Zero implementation.
- Set the initial firmware and specification version to `1.00`.
- Added the initial Pico SDK/TinyUSB host firmware scaffold under `software/`, with UF2 export configured under `firmware/`.
- Added a `software/Makefile` wrapper that builds the versioned UF2 artifact into `firmware/`.
- Updated `software/Makefile` to use Pico SDK-managed CMake, Ninja, Python, and SDK paths by default on Windows.
- Added Pico-PIO-USB as a repo-managed submodule and linked it into the firmware build for GPIO-based USB host support.
- Enabled USB-C CDC debug logging while keeping the USB-A connector as the Pico-PIO-USB HID host port.

## Hardware

### Hardware revision 1.00 - 2026-08-01

- Documented the initial RP2040 Zero hardware baseline, USB-A host wiring, MSX DB9 wiring, and BSS138 level shifting guidance.
- Changed the power architecture so DB9 pin 5 supplies the board 5 V rail with protected USB-C and USB-A power paths.
- Selected a 1N5819 Schottky diode from DB9 pin 5 to the board 5 V rail to prevent USB-C programming power from backfeeding the MSX port.
- Created the initial KiCad schematic/project for hardware revision `1.00` without creating a PCB layout.