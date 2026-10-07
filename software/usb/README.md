# RetroLink USB Software

Firmware for the RetroLink USB board (USB-A host to MSX DB9). Both builds target the same hardware; flash the UF2 that matches the device you want to connect.

| Directory | Firmware | UF2 artifact |
| --- | --- | --- |
| [joystick](joystick/README.md) | USB HID joysticks and gamepads mapped to MSX joystick signals | `firmware/usb/retrolink-joystick-<version>.uf2` |
| [mouse](mouse/README.md) | USB HID mice mapped to the MSX mouse protocol, with optional joystick emulation | `firmware/usb/retrolink-mouse-<version>.uf2` |

Build either one with `make` from its directory, for example `make -C software/usb/mouse`. The mouse build and the [MD firmware](../md/README.md) reuse shared LED, CDC debug, and USB descriptor sources from `joystick/`.
