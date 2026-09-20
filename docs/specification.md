# The Retro Hacker RetroLink Specification

## Version

RetroLink specification version: `1.00`

Firmware version: `1.00`

Hardware revision: `1.00`

This document defines the initial hardware and firmware-facing specification for RetroLink, a USB HID adapter that connects modern USB joysticks and USB mice to classic MSX computers through the standard DB9 joystick port.

## Goals

- Use an RP2040 Zero module as the first hardware platform.
- Provide a USB-A host connector for USB HID joystick and mouse devices.
- Present MSX-compatible joystick and mouse signals through a DB9 connector.
- Keep the firmware architecture compatible with the Raspberry Pi Pico SDK and TinyUSB where practical.
- Keep firmware source code under `software/` and generated UF2 artifacts under `firmware/`.
- Avoid direct 5 V exposure on RP2040 GPIO pins.
- Keep connector assignments and electrical assumptions documented from the first revision.

## Known Hardware Baseline

The first RetroLink hardware revision is based on an RP2040 Zero board using the Raspberry Pi RP2040 microcontroller.

Known board features:

- USB-C connector on the RP2040 Zero board.
- 5 V, GND, and 3.3 V power pins.
- Exposed GPIO pins: GPIO 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 26, 27, 28, and 29.
- Boot and Reset buttons.

## Current PCB And Assembly Files

Hardware revision `1.00` now includes a [KiCad schematic](../hardware/1.00/retrolink.kicad_sch), [PCB layout](../hardware/1.00/retrolink.kicad_pcb), [interactive BOM](../hardware/1.00/bom/ibom.html), and [production outputs](../hardware/1.00/production). The [README PCB previews](../README.md#pcb-preview) show top and bottom 3D renders, not photographs of tested hardware. Open the iBOM HTML locally in a browser; GitHub's file view does not run the interactive page.

The [exported BOM](../hardware/1.00/production/bom.csv) currently lists one each of RZ1 (RP2040 Zero), J1 (DE9 socket), J2 (USB-A receptacle), and D1 (value `1N5819`, footprint `D_SOD-123`). The [README component table](../README.md#pcb-components) records quantities, footprints, and replaceable AliExpress search links. These are sourcing placeholders, not approved parts or verified stock.

The current export does not include the specified BSS138 channels, 10 kOhm pull-ups, USB data-line passives/ESD protection, or a separate VBUS current limiter. Their requirements below remain design requirements, not claims about the existing PCB implementation. Resolve these gaps and review schematic-to-PCB connectivity before manufacture or connection to an MSX; RP2040 GPIOs are not 5 V tolerant.

D1 requires package reconciliation: a common axial DO-41 1N5819 does not fit the SOD-123 footprint. Confirm the exact SMD part number, polarity, ratings, and forward-voltage behavior, then synchronize the schematic value, PCB, and BOM. J1 currently uses `DSUB-9_Socket_EdgeMount_P2.77mm`; J2 uses `TE_6364372-2`. Verify connector mating and mechanical dimensions against the intended MSX and selected parts. File availability does not establish ERC/DRC, assembly, or electrical validation. Regenerate the iBOM and production exports after hardware changes.

## Design Assumptions

- The RP2040 Zero USB-C connector remains available for programming, bootloader access, and development, but the installed RetroLink adapter is powered from the MSX DB9 connector.
- The external USB-A connector is used as the USB host port for the attached joystick or mouse.
- RP2040 GPIO pins are 3.3 V logic and are not 5 V tolerant.
- The MSX joystick port may expose 5 V logic or 5 V pull-ups; therefore, DB9 signal interfaces must protect the RP2040.
- DB9 pin assignments follow the MSX General Purpose port convention unless later hardware documentation proves a different target requirement. Reference: [General Purpose port - MSX Wiki](https://www.msx.org/wiki/General_Purpose_port).
- Hardware revision `1.00` requires BSS138 MOSFET level shifters with 10 kOhm pull-up resistors for DB9 signals that cross between the RP2040 3.3 V domain and the MSX 5 V domain; these parts are not present in the current exported BOM.
- DB9 pin 5 supplies the board 5 V rail for hardware revision `1.00` through a series 1N5819 Schottky diode; the usable current budget from the MSX port and the post-diode voltage still need validation on representative machines.
- Firmware version `1.00` assumes the common RP2040 Zero onboard WS2812-compatible status LED is connected to GPIO 16. Validate this for the exact RP2040 Zero board variant before freezing the build.
- USB host VBUS current requirements for connected devices are not yet validated. Current limiting and measurement access remain requirements to address before hardware release.

Any assumption above must be revisited when electrical measurements, MSX model compatibility notes, or board revision documents are added.

## Recommended Connector Strategy

### USB-C On RP2040 Zero

Keep the RP2040 Zero USB-C connector dedicated to development, programming, bootloader access, and firmware debug over USB CDC for hardware revision `1.00`.

The installed adapter power source is DB9 pin 5 from the MSX port. The PCB must include a series 1N5819 Schottky diode from DB9 pin 5 to the board 5 V rail so MSX power can feed RetroLink, but RP2040 Zero USB-C power cannot backfeed the MSX port during programming.

Do not wire the external USB-A data pins in parallel with the RP2040 Zero USB-C data pins. Sharing USB data lines between two physical connectors would create ambiguous host/device roles and can make programming, enumeration, and electrical behavior unreliable.

### USB-A Host Connector

Use the external USB-A female connector as the HID host port.

Recommended hardware revision `1.00` approach:

- Use RP2040 PIO-based USB host signaling for the USB-A D+ and D- lines.
- Reserve adjacent GPIO pins for the USB data pair.
- Use the Raspberry Pi Pico SDK as the base firmware SDK.
- Use TinyUSB host APIs where practical, with the USB transport layer isolated so the project can adapt if the final implementation uses a TinyUSB-supported host backend or a PIO USB integration layer.

Recommended USB-A wiring:

| USB-A pin | Signal | Recommended connection | Notes |
| --- | --- | --- | --- |
| 1 | VBUS +5 V | Post-diode DB9-derived board 5 V through a current-limited high-side switch or resettable fuse | Current budget and voltage after diode drop must be validated against the MSX port and expected USB HID devices. |
| 2 | D- | GPIO 3 through USB routing network | Keep paired with D+ and route as a short differential pair. |
| 3 | D+ | GPIO 2 through USB routing network | GPIO 2 is the recommended PIO USB D+ assignment for hardware revision `1.00`. |
| 4 | GND | Board ground | Must share ground with RP2040 Zero and DB9 ground. |
| Shield | Shield | Chassis/shield strategy to be defined by PCB design | Do not assume shield should be tied directly to logic ground without EMC review. |

Recommended USB support components:

- Series resistors on D+ and D- near the RP2040-side driver pins, with values selected according to the chosen PIO USB reference design.
- USB ESD protection on D+ and D- close to the USB-A connector.
- 15 kOhm pull-down resistors on D+ and D- if required by the selected USB host implementation/reference design.
- Current-limited 5 V VBUS switch or resettable fuse for USB-A pin 1.
- Optional VBUS enable and fault signals connected to spare GPIO pins if a power switch with control/status is used.

Recommended optional VBUS control mapping:

| Function | Recommended GPIO | Direction | Notes |
| --- | --- | --- | --- |
| USB host D+ | GPIO 2 | USB data | Reserved for PIO USB host. |
| USB host D- | GPIO 3 | USB data | Reserved for PIO USB host. |
| USB VBUS enable | GPIO 4 | Output | Optional; only if the power switch supports enable control. |
| USB VBUS fault | GPIO 5 | Input | Optional; only if the power switch exposes fault status. Use 3.3 V-compatible signaling. |

## Recommended DB9 Connector Wiring

The current PCB uses a DE9 female socket with the `DSUB-9_Socket_EdgeMount_P2.77mm` footprint, superseding the earlier male-connector recommendation in this document. This describes the existing hardware files, not a firmware pin-assignment change. Verify mating compatibility, connector orientation, and pin numbering against the intended MSX before ordering or assembly.

Standard MSX joystick-port signal convention for hardware revision `1.00`:

| DB9 pin | MSX signal | RetroLink direction | Recommended GPIO | Electrical interface requirement |
| --- | --- | --- | --- | --- |
| 1 | Up | Output to MSX | GPIO 6 | BSS138 level shifter with 10 kOhm pull-ups. |
| 2 | Down | Output to MSX | GPIO 7 | BSS138 level shifter with 10 kOhm pull-ups. |
| 3 | Left | Output to MSX | GPIO 8 | BSS138 level shifter with 10 kOhm pull-ups. |
| 4 | Right | Output to MSX | GPIO 9 | BSS138 level shifter with 10 kOhm pull-ups. |
| 5 | +5 V from MSX port | Primary board power input | GPIO 13 optional through BSS138 or divider | Feed the board 5 V rail through a series 1N5819 diode and input protection; do not connect directly to RP2040 GPIO. |
| 6 | Trigger A | Output to MSX | GPIO 10 | BSS138 level shifter with 10 kOhm pull-ups. |
| 7 | Trigger B | Output to MSX | GPIO 11 | BSS138 level shifter with 10 kOhm pull-ups. |
| 8 | OUT / strobe from MSX | Input to RetroLink | GPIO 12 | BSS138 level shifter with 10 kOhm pull-ups. |
| 9 | GND | Ground reference | GND | Common ground with RP2040 Zero and USB-A ground. |

### Level Shifting Standard

Hardware revision `1.00` requires a BSS138 bidirectional MOSFET level-shifter circuit with 10 kOhm pull-up resistors for DB9-side signals that cross between the RP2040 and MSX voltage domains. The current four-component PCB BOM does not implement these channels.

Recommended circuit per shifted signal:

- BSS138 source connected to the RP2040 3.3 V side.
- BSS138 drain connected to the MSX 5 V side.
- BSS138 gate connected to 3.3 V.
- 10 kOhm pull-up from the RP2040 side to 3.3 V.
- 10 kOhm pull-up from the MSX side to 5 V.
- Common ground between RP2040 Zero, USB-A, and DB9.

This circuit is appropriate for the active-low, open-drain style DB9 signaling model used by RetroLink. Firmware should drive an asserted signal low and release the GPIO for the inactive state; it should not drive the shifted DB9 lines high as push-pull outputs.

The BSS138 and 10 kOhm values must still be validated against MSX mouse and joystick timing requirements. If rise time is too slow for a future timing-sensitive mode, the resistor value or buffer topology may need to change in a later hardware revision.

### DB9 Output Interface

The DB9 output signals should behave as active-low switch closures from the MSX perspective. The RP2040 must not drive a DB9 signal high at 5 V.

Recommended implementation:

- Use one BSS138 level-shifter channel per DB9 output signal.
- Use 10 kOhm pull-ups on both the 3.3 V and 5 V sides of each BSS138 channel.
- Drive the interface active only when the emulated joystick or mouse signal should pull the MSX line low.
- Release the interface for the inactive state.

This maps cleanly to firmware as logical active-low outputs and avoids exposing RP2040 GPIO pins to 5 V.

### DB9 Pin 8 Input

Treat DB9 pin 8 as an MSX-driven signal for hardware revision `1.00`.

Recommended implementation:

- Use one BSS138 level-shifter channel with 10 kOhm pull-ups before GPIO 12.
- Add input protection appropriate for the selected circuit.
- Keep the firmware abstraction independent from the physical GPIO so mouse or joystick timing behavior can be implemented without hard-coding board details.

### DB9 Pin 5 Power Handling

Treat DB9 pin 5 as the primary 5 V power input for the installed RetroLink adapter in hardware revision `1.00`.

Recommended implementation:

- Route DB9 pin 5 to the board 5 V rail through a series 1N5819 Schottky diode, with the diode anode on DB9 pin 5 and the cathode on the RetroLink board 5 V rail.
- Validate that the 1N5819 forward-voltage drop still leaves enough voltage for the RP2040 Zero and attached USB HID devices across the expected MSX port current range.
- Place additional input protection, such as a resettable fuse or current-limited power switch, as required by the final power budget.
- Use the series diode to prevent USB-C development power from backfeeding the MSX port when the RP2040 Zero is connected to USB-C for programming.
- Prevent MSX-derived power from backfeeding a connected USB-C host if the RP2040 Zero board power path does not already guarantee that behavior.
- Feed USB-A VBUS from the post-diode DB9-derived 5 V rail through its own current-limited switch or resettable fuse.
- Keep the RP2040 GPIO domain at 3.3 V and use BSS138 level shifting for DB9 logic signals.
- Validate the available MSX port current budget before selecting the USB-A VBUS current limit.

If DB9 +5 V sensing is needed in firmware, connect the sense input only through a high-impedance divider, BSS138 logic-level path, or other 3.3 V-compatible sensing circuit.

For hardware revision `1.00`, a BSS138 channel may be used for a logic-level presence sense only if the resulting signal behavior is validated. Do not use the BSS138 path to draw board power from DB9 pin 5.

## Firmware Architecture Implications

Firmware source code lives under `software/`. Generated UF2 files live under `firmware/`.

The wiring above should be reflected in firmware as board-level configuration, not scattered GPIO constants.

Recommended firmware modules:

| Module | Responsibility |
| --- | --- |
| `platform` | RP2040 Zero board initialization, GPIO mapping, clocks, timers, and board revision constants. |
| `usb_host` | USB host initialization, enumeration, connection state, and TinyUSB or PIO USB integration. |
| `hid` | HID report parsing for joysticks and mice. |
| `msx_port` | Active-low DB9 signal model, pin 8 input handling, and timing-sensitive MSX behavior. |
| `mapping` | Translation from HID joystick/mouse events to MSX joystick or mouse signals. |
| `config` | Build-time or runtime configuration for board revision and device behavior. |
| `diagnostics` | Optional debug state, fault reporting, and validation hooks. |

Firmware version `1.00` provides USB-C CDC debug and descriptor-based USB HID joystick/gamepad translation. Absolute X/Y axes, four/eight-way hats, and discrete D-pad usages drive up/down/left/right on GPIO6/7/8/9 (DB9 pins 1/2/3/4); button usages 1/2 drive A/B on GPIO10/11 (DB9 pins 6/7). Outputs assert low and release to high impedance through the documented level shifters. The first report is decoded directly, without neutral calibration. Axis thresholds are below 25% and above 75% of the descriptor's logical range, with the middle 50% neutral. Opposing directions cancel. GPIO12 / DB9 pin 8 is not driven in this joystick mode.

States are combined across report IDs and interfaces. Detach, invalid reports, or failed receive requests release the affected interface's state; a receive-request failure requires reconnection. CDC reports mapping support and state changes, and the GPIO16 LED pulses on newly asserted controls. Vendor-specific protocols and mouse emulation are not implemented. See [software behavior and tests](../software/README.md#current-behavior) for decoder limits, public interfaces, and host-test commands. Compilation and simulated tests do not replace validation with the actual joystick and MSX hardware.

Recommended firmware constants for firmware version `1.00` and hardware revision `1.00`:

| Constant | Value |
| --- | --- |
| `RETROLINK_FW_VERSION` | `1.00` |
| `RETROLINK_HW_REVISION` | `1.00` |
| `USB_HOST_DP_GPIO` | `2` |
| `USB_HOST_DM_GPIO` | `3` |
| `USB_HOST_VBUS_EN_GPIO` | `4` if fitted, otherwise disabled |
| `USB_HOST_VBUS_FAULT_GPIO` | `5` if fitted, otherwise disabled |
| `MSX_UP_GPIO` | `6` |
| `MSX_DOWN_GPIO` | `7` |
| `MSX_LEFT_GPIO` | `8` |
| `MSX_RIGHT_GPIO` | `9` |
| `MSX_TRIGGER_A_GPIO` | `10`, active-low output to DB9 pin 6 |
| `MSX_TRIGGER_B_GPIO` | `11` |
| `MSX_OUT_STROBE_GPIO` | `12` |
| `MSX_5V_SENSE_GPIO` | `13` if fitted, otherwise disabled |
| `RETROLINK_STATUS_LED_GPIO` | `16`, assuming the common RP2040 Zero onboard WS2812-compatible LED |

## Firmware Build

Firmware version `1.00` uses the Raspberry Pi Pico SDK build system from `software/`.

Expected build commands from the repository root:

```powershell
git submodule update --init --recursive
make -C software
```

The Makefile supplies Pico SDK, toolchain, and Pico-PIO-USB paths. See [software build instructions](../software/README.md#build) for prerequisites and overrides on other machines.

The build is configured to copy the UF2 artifact to:

```text
firmware/retrolink-1.00.uf2
```

The current firmware uses TinyUSB device CDC on native USB root port 0, TinyUSB host on Pico-PIO-USB root port 1, and a 120 MHz system clock. The USB-A data pair is wired to GPIO2 (D+) and GPIO3 (D-).

## Reserved GPIO Pins

Leave GPIO 0, 1, 14, 15, 26, 27, 28, and 29 unassigned in hardware revision `1.00` unless a later schematic needs them.

Potential future uses:

- UART diagnostics.
- Board revision detection.
- Configuration buttons or DIP switches.
- Status LEDs.
- Analog measurements on ADC-capable pins.
- Second DB9 port support if the project scope expands.

## Validation Required Before Hardware Release

Before freezing hardware revision `1.00`, validate and document:

- Reconciliation of the current four-component PCB/BOM with the required level shifting, pull-ups, USB protection, and VBUS current limiting.
- D1's exact SOD-123-compatible part number and J1/J2 connector mating, mounting geometry, and pin numbering.
- Schematic ERC, PCB DRC, and synchronization of the PCB, iBOM, production BOM, and fabrication outputs.
- USB host implementation choice and exact required D+/D- passive components.
- Pico SDK/TinyUSB host build using the selected PIO USB host configuration.
- RP2040 Zero status LED GPIO and WS2812 timing on the exact board variant.
- USB-A VBUS current limit and power source behavior.
- MSX DB9 pin 5 current budget on representative machines.
- 1N5819 orientation, forward-voltage drop, and current rating for DB9 pin 5 power input.
- Power-path isolation between DB9 pin 5, RP2040 Zero USB-C 5 V, and USB-A VBUS.
- Whether the DB9-derived 5 V rail can safely power the RP2040 Zero and expected USB HID devices.
- DB9 voltage levels on representative MSX machines.
- Required DB9 output pull-up behavior for joystick and mouse modes.
- BSS138 plus 10 kOhm rise times for joystick and MSX mouse timing.
- Pin 8 timing and voltage behavior for MSX mouse support.
- Grounding and shield strategy.
- ESD protection selection for USB-A and DB9 external connectors.
- Firmware timing requirements for active-low DB9 outputs.

## Open Questions

- Which exact SOD-123 diode and connector listings will be qualified for the current footprints?
- How will the missing level-shifting and USB protection requirements be incorporated into the PCB before release?
- Is GPIO 16 correct for the onboard status LED on the exact RP2040 Zero board variant used in production?
- What maximum USB device current should RetroLink support?
- What current limit should be used for USB-A VBUS when the board is powered from MSX DB9 pin 5?
- Does the selected 1N5819 diode preserve enough post-diode voltage for the board and USB-A device across the validated MSX port current budget?
- Which MSX mouse protocol timing requirements must be supported first?
- Should hardware revision `1.00` support one DB9 port only, or reserve mechanical and GPIO space for a second port?