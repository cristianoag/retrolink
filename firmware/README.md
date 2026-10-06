# RetroLink Firmware Artifacts

Generated firmware files are organized by variant:

- [USB](usb/) contains the existing RP2040 UF2 releases; source is in [software/usb](../software/usb/).
- [MD](md/) contains the RetroLink MD (Mega Drive-to-MSX) UF2 releases; the current build produces `md/retrolink-md-1.10.uf2`.

The current RetroLink USB firmware version `1.10` build produces:

```text
usb/retrolink-1.10.uf2
```

The earlier `usb/retrolink-1.00.uf2` release remains available.

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