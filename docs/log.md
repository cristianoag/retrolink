# RetroLink Version Change Log

## Software and Firmware

### USB

#### Firmware v1.00 - 2026-09-20

- Defined initial firmware-facing module boundaries and GPIO constants for the future RP2040 Zero implementation.
- Set the initial firmware and specification version to `1.00`.
- Added the initial Pico SDK/TinyUSB host firmware scaffold under `software/`, with UF2 export configured under `firmware/`.
- Added a `software/Makefile` wrapper that builds the versioned UF2 artifact into `firmware/`.
- Updated `software/Makefile` to use Pico SDK-managed CMake, Ninja, Python, and SDK paths by default on Windows.
- Added Pico-PIO-USB as a repo-managed submodule and linked it into the firmware build for GPIO-based USB host support.
- Enabled USB-C CDC debug logging while keeping the USB-A connector as the Pico-PIO-USB HID host port.
- Replaced activity-based mapping with descriptor-decoded directions and buttons A/B on GPIO6-11, with high-impedance release and host regression tests.
- Reassigned USB-A host D+/D- from GPIO2/GPIO3 to GPIO27/GPIO28; the current build requires matching updated wiring instead of the original hardware revision `1.00` data pair.
- Remapped MSX up/A/down/B/left/OUT/right to GPIO0/1/2/3/4/5/6 and removed unused GPIO4/5 VBUS reservations; updated wiring is required and OUT remains unused in joystick mode.

### MD

#### Firmware v1.00 - 2026-09-23

- Added the separate MD firmware for 3-/6-button Mega Drive pads, using D0..D5 on GPIO28/26/14/12/27/13 and TH on GPIO15, with B/C mapped to MSX A/B.
- Added MD common-line-aware MSX low/release outputs on GPIO0/2/4/6/1/3, CDC diagnostics, LED activity, invalid-frame release, native regression tests, and a versioned MD UF2 build.
- Disabled the MD build's B0/B1 USB enumeration workaround to keep GPIO15 dedicated to SELECT; older-chip USB hub compatibility is limited.

## Hardware

### USB

#### Hardware revision 1.10 - 2026-09-23

- Identified `1.10` as the latest USB hardware revision and replaced the README previews with updated angled, top, and bottom 3D renders.

#### Hardware revision 1.00 - 2026-09-20

- Documented the initial RP2040 Zero hardware baseline, USB-A host wiring, MSX DB9 wiring, and BSS138 level shifting guidance.
- Changed the power architecture so DB9 pin 5 supplies the board 5 V rail with protected USB-C and USB-A power paths.
- Selected a 1N5819 Schottky diode from DB9 pin 5 to the board 5 V rail to prevent USB-C programming power from backfeeding the MSX port.
- Created the initial KiCad schematic/project for hardware revision `1.00` without creating a PCB layout.
- Documented the current PCB layout, preview renders, iBOM, and four-part sourcing table, including unresolved protection and package requirements.

### MD

#### Hardware revision 1.00 - 2026-09-23

- Documented the MD DE9-to-DE9 board, updated controller GPIO assignments, and angled/front/back renders, with level conversion and electrical validation still required.