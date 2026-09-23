# The Retro Hacker RetroLink

RetroLink is an open-source hardware and firmware project for connecting controllers to classic MSX computers through the MSX DB9 General Purpose port. The existing **RetroLink USB** variant connects USB HID joysticks and gamepads. A planned **RetroLink MD** variant will translate Mega Drive joystick signals to MSX signals; its design, software, and firmware have not been implemented yet.

![RetroLink USB hardware revision 1.10 angled 3D render with RP2040 Zero, USB-A connector, and DE9 socket](images/2026-09-23_14-25.png)

The latest RetroLink USB hardware revision, `1.10`, uses an RP2040 Zero module on a custom adapter board with:

- USB-A host connector for the USB HID device.
- DB9 connector for the MSX joystick/general purpose port.
- MSX DB9 pin 5 as the installed adapter power source.
- 1N5819 Schottky diode protection to prevent USB-C programming power from backfeeding the MSX.

## PCB Preview

Top view of RetroLink USB hardware revision `1.10` (3D render):

![RetroLink USB revision 1.10 PCB top render with RP2040 Zero, USB-A connector, and DE9 socket](images/2026-09-23_14-24_1.png)

Bottom view of RetroLink USB hardware revision `1.10` (3D render):

![RetroLink USB revision 1.10 PCB bottom render showing the diode, connector pads, and RetroLink USB branding](images/2026-09-23_14-24.png)

## Variants and Versions

| Variant | Input | Hardware | Software and firmware |
| --- | --- | --- | --- |
| RetroLink USB | USB HID joystick/gamepad | Latest: [revision `1.10`](hardware/usb/1.10); earlier revisions under [hardware/usb](hardware/usb) | [USB source](software/usb) and [UF2 artifacts](firmware/usb); firmware `1.00` |
| RetroLink MD | Mega Drive joystick | [hardware/md](hardware/md) reserved for the future design | [software/md](software/md) and [firmware/md](firmware/md) reserved; not yet implemented |

The [USB specification](docs/specification.md) is version `1.00` and describes USB hardware revision `1.00`, not the planned MD variant. See [docs/log.md](docs/log.md) for the USB version change log. Revisions are organized within each variant; a hardware directory does not imply a corresponding firmware release.

## Repository Layout

| Path | Purpose |
| --- | --- |
| [software/usb](software/usb) | USB RP2040 source code using the Raspberry Pi Pico SDK, TinyUSB, and Pico-PIO-USB. |
| [firmware/usb](firmware/usb) | USB UF2 firmware artifacts. |
| [hardware/usb](hardware/usb) | USB KiCad project files by hardware revision. |
| [software/md](software/md), [firmware/md](firmware/md), [hardware/md](hardware/md) | Placeholders for the planned Mega Drive-to-MSX variant; no build or hardware release yet. |
| [images](images) | PCB preview renders used in this README. |
| [docs](docs) | Technical specification and version log. |
| [software/third_party/Pico-PIO-USB](software/third_party/Pico-PIO-USB) | Git submodule for GPIO-based USB host support on RP2040. |

## Firmware Build

Initialize submodules after cloning:

```powershell
git submodule update --init --recursive
```

Build from the firmware source folder:

```powershell
cd software\usb
make
```

The build generates a versioned UF2 file:

```text
firmware/usb/retrolink-1.00.uf2
```

On Windows, the Makefile discovers Pico SDK-managed tools under `%USERPROFILE%\.pico-sdk`. Override `PICO_SDK_ROOT`, `PICO_SDK_PATH`, `PICO_TOOLCHAIN_PATH`, `CMAKE`, `CMAKE_MAKE_PROGRAM`, `PYTHON3_EXECUTABLE`, `PICOTOOL_DIR`, `PIOASM_DIR`, or `PICO_PIO_USB_PATH` if your toolchain is installed elsewhere.

## RetroLink USB Firmware Behavior

USB firmware version `1.00` decodes standard USB HID joystick/gamepad axes, hats, and buttons into six active-low MSX outputs: GPIO0/2/4/6 for up/down/left/right, GPIO1 for button A, and GPIO3 for button B. GPIO5 is reserved for the MSX OUT/strobe input, unused in joystick mode. Button usages 1 and 2 map to A and B. Outputs release to high impedance when inactive. The current source requires updated MSX wiring and USB-A D+/D- on GPIO27/28, not the original hardware revision `1.00` wiring. USB-C CDC provides diagnostics, and the RP2040 Zero status LED pulses on newly asserted controls. See [software/usb/README.md](software/usb/README.md#current-behavior) for supported layouts, decoder limits, and tests; physical controller/MSX validation remains required.

The current status LED implementation assumes the common RP2040 Zero onboard WS2812-compatible LED on GPIO 16. Validate this against the exact board variant before treating the build as hardware-final.

## RetroLink USB Hardware Status

The latest hardware revision, `1.10`, includes the KiCad project, schematic, and PCB layout shown in the previews above:

- [hardware/usb/1.10/retrolink.kicad_pro](hardware/usb/1.10/retrolink.kicad_pro)
- [hardware/usb/1.10/retrolink.kicad_sch](hardware/usb/1.10/retrolink.kicad_sch)
- [hardware/usb/1.10/retrolink.kicad_pcb](hardware/usb/1.10/retrolink.kicad_pcb)

Earlier hardware revision `1.00` assembly/fabrication exports remain available for reference; these are not manufacturing files for revision `1.10`:

- [Interactive BOM (iBOM)](hardware/usb/1.00/bom/ibom.html)
- [Exported component BOM](hardware/usb/1.00/production/bom.csv)
- [Production outputs](hardware/usb/1.00/production)

GitHub displays the iBOM HTML as a repository file, not an interactive page. Download the raw HTML or open the local file in a browser to use its component highlighting and assembly checklist.

**Not yet electrically validated:** the revision `1.00` exported BOM contains only the four components listed below. It does not include BSS138 level shifters, 10 kOhm pull-ups, USB data-line passives/ESD protection, or a separate VBUS current limiter. Review the PCB against the [specification](docs/specification.md) before manufacture or connection to an MSX. RP2040 GPIOs are not 5 V tolerant; firmware high-impedance release does not replace level shifting.

## PCB Components

Quantities are per USB PCB, taken from the revision `1.00` [production BOM](hardware/usb/1.00/production/bom.csv). AliExpress links are search placeholders, not verified product recommendations; replace them with selected listings after checking dimensions, pinout, package, and electrical ratings. Availability and seller quality have not been verified.

| Reference | Component / BOM value | Quantity | PCB footprint / selection notes | AliExpress |
| --- | --- | --- | --- | --- |
| RZ1 | RP2040 Zero module | 1 | `RP2040-Zero`; check module dimensions and pinout against the PCB. | [Search RP2040 Zero](https://www.aliexpress.com/w/wholesale-rp2040-zero.html) |
| J1 | DE9 female socket (`DE9_Socket`) | 1 | `DSUB-9_Socket_EdgeMount_P2.77mm`; edge-mount, 2.77 mm pitch. Verify contact gender and mounting geometry. | [Search DB9 female edge-mount](https://www.aliexpress.com/w/wholesale-db9-female-edge-mount.html) |
| J2 | USB-A receptacle (`USB_A`) | 1 | `TE_6364372-2`; verify signal pins, shell tabs, and mounting dimensions. | [Search USB-A PCB socket](https://www.aliexpress.com/w/wholesale-usb-type-a-female-pcb.html) |
| D1 | Schottky diode (`1N5819` BOM value) | 1 | `D_SOD-123`; the selected part must fit SOD-123. A common axial DO-41 1N5819 will not fit; confirm the exact SMD part, ratings, and polarity before ordering. | [Search 1N5819 SOD-123](https://www.aliexpress.com/w/wholesale-1n5819-sod-123.html) |

This table describes the revision `1.00` PCB export, not a verified revision `1.10` BOM or a complete implementation of the specification's protection requirements. Use the iBOM for placement, and regenerate assembly outputs after hardware changes.
