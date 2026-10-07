# RetroLink MD Hardware

RetroLink MD revision [1.00](1.00/) connects a Mega Drive controller's female DE9 plug to a male DE9 on the adapter; the adapter's female DE9 connects to the MSX. An RP2040 Zero translates the controller protocol into MSX joystick signals.

- [Schematic](1.00/retrolink.kicad_sch)
- [PCB](1.00/retrolink.kicad_pcb)
- [Project](1.00/retrolink.kicad_pro)
- [Exported BOM](1.00/production/bom.csv)
- [Renders](../../README.md#retrolink-md-revision-100)
- [Firmware, exact GPIO assignments, and electrical prerequisites](../../software/md/README.md)

MD D0/D1/D2/D3/D4/D5 use GPIO28/26/14/12/27/13; TH/SELECT uses GPIO15. MSX up/A/down/B/left/common/right use GPIO0/1/2/3/4/5/6. GPIO16 is the status LED. No USB-A host or MSX voltage-sense input is assigned in this variant.

USB hardware is maintained separately under [hardware/usb](../usb/).

Revision [1.10](1.10/) has a [compact four-clip PLA case](1.10/case/README.md)
for its two DB9 connectors, with STLs, STEP and a parametric generator. Its
54.19 x 33.50 x 19.70 mm plastic envelope is extended 4 mm toward the male DB9
relative to the original USB-sized design. The male end mirrors the female-end
profile instead of using the known incorrect male 3D model's through-passages.
USB-C programming requires opening the case. Physical fit,
clip durability and connector retention still require testing.
