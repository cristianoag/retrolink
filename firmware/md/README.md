# RetroLink MD Firmware

Firmware `1.00` for RetroLink MD hardware revision `1.00` is built from [software/md](../../software/md/).

- Artifact: [retrolink-md-1.00.uf2](retrolink-md-1.00.uf2).
- Input: standard Mega Drive 3-button and 6-button controllers.
- Mapping: directions, MD B to MSX trigger A, MD C to MSX trigger B; other buttons are diagnostic-only.
- USB-C provides programming and CDC diagnostics, not USB-A controller input.

The ARM build and native simulated tests do not establish hardware compatibility or electrical safety. Complete the [electrical and runtime checks](../../software/md/README.md) before flashing and connecting to an MSX. Use physical BOOT/RESET for BOOTSEL mode; USB-triggered resets are disabled.

This UF2 is **not** for the USB variant. USB firmware artifacts remain under [firmware/usb](../usb/).
