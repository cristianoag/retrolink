# RetroLink Firmware Artifacts

Generated firmware files are organized by variant:

- [USB](usb/) contains the RetroLink USB board's UF2 releases: joystick firmware from [software/usb/joystick](../software/usb/joystick/) and mouse firmware from [software/usb/mouse](../software/usb/mouse/). Both run on the same USB hardware.
- [MD](md/) contains the RetroLink MD (Mega Drive-to-MSX) UF2 releases; the current build produces `md/retrolink-md-1.10.uf2`.

The current RetroLink USB joystick firmware version `1.10` build produces:

```text
usb/retrolink-joystick-1.10.uf2
```

The earlier `usb/retrolink-joystick-1.00.uf2` release remains available. Joystick releases were renamed from `retrolink-<version>.uf2` to `retrolink-joystick-<version>.uf2`; their contents are unchanged.

The RetroLink USB mouse firmware version `1.00` build produces:

```text
usb/retrolink-mouse-1.00.uf2
```

## Tested USB Mice

Mice tested on real hardware with each USB mouse firmware version. Mice not listed have not been tested.

### Mouse firmware 1.00

No mice recorded.

## Tested USB Joysticks

Controllers tested on real hardware with each USB firmware version. Controllers not listed have not been tested.

### Firmware 1.10

| Controller | Hardware revision | Result | Notes |
| --- | --- | --- | --- |
| Hyperkin Trooper 2 | 1.10 | Works | |
| Dazz Dual Shock | 1.10 | Works | |
| Generic Super Nintendo clone (USB) | 1.10 | Works | |
| Datafrog Wireless (USB) | 1.10 | Works | |
| Xbox One controller | 1.10 | Does not work | Xbox One controllers normally use Microsoft's vendor-specific protocol instead of standard USB HID, which this firmware still does not support. |

### Firmware 1.00

No controllers recorded.

## Tested MegaDrive Joysticks

Controllers tested on real hardware with each MD firmware version. Controllers not listed have not been tested.

### Firmware 1.10

| Controller | Hardware revision | Result | Notes |
| --- | --- | --- | --- |
| 8BitDo M30 with Mega Drive adapter | 1.10 | Works | |