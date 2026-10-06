# RetroLink USB Hardware

KiCad hardware revisions for the USB HID-to-MSX variant live here. Revision `1.00` has the PCB and production exports documented in the [project README](../../README.md#retrolink-usb-hardware-status). Revision `1.10` is present as a separate revision directory; check its project files before using it for fabrication. The planned Mega Drive-to-MSX design belongs in [hardware/md](../md/), not here.

Revision `1.10` also has a [compact four-clip PLA case](1.10/case/README.md), with printable STLs, an editable STEP assembly, and a parametric generator. It exposes USB-A and DB9; opening the case is required for RP2040 Zero USB-C programming. Dedicated PCB end stops target 0.20 mm total nominal lengthwise play. Reinforced short clip roots and a padded metal-backshell cradle keep the DB9 flange outside. No screws or long latches are used; physical clip strength and frictional connector retention still require testing.
