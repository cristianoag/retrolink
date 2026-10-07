# RetroLink MD 1.10 compact four-clip case

Two-piece PLA enclosure for the [MD revision 1.10 PCB](../retrolink.kicad_pcb),
with a **male DB9 for the Mega Drive controller** and a **female DB9 for the MSX**.
The plastic envelope is **54.19 x 33.50 x 19.70 mm**: 4 mm longer toward the male
connector than the original MD case. Width and height still match the
[USB 1.10 case](../../../usb/1.10/case/README.md). The female end, PCB and clips
stay in their previous assembly positions.

Four short internal clips, alignment keys, over-travel stops and tight PCB end
stops carry over from USB. There are **no screws or long latches**. USB-C is
enclosed: open the case and lift the PCB as needed for programming.

**Prototype, not physically print-tested or load-rated.** Digital fit checks do
not establish clip durability, connector retention force or compatibility with
every real cable/port housing. Test one complete pair before use on equipment.

![MD enclosure, assembled and exploded](preview.png)

## Print files

| File | Purpose |
| --- | --- |
| [Base STL](retrolink-md-1.10-base.stl) | MD bottom half, oriented with its outside face on the print bed. |
| [Lid STL](retrolink-md-1.10-lid.stl) | MD top half, oriented with its outside face on the print bed. |
| [Case STEP](retrolink-md-1.10-case.step) | Two assembled plastic solids for CAD editing. |
| [Generator](md_case.py) | MD connector openings, assembly checks and CLI. |
| [Tests](test_md_case.py) | Twelve MD geometry regressions, including one-sided extension and mirrored end profiles. |
| [Dependencies](requirements.txt) | Reuses the USB enclosure's pinned CAD dependencies. |

Dimensions are in **millimetres**. Slice at **100% scale**. Use both MD parts
together; reprint both halves for this revision. USB and MD halves are not
interchangeable. The preview includes electronics and female-end rubber pads,
but deliberately omits the known incorrect male connector model. STL and STEP
exports contain only the plastic case.

## Geometry and connector access

| Feature | Default |
| --- | --- |
| Plastic envelope, excluding exposed connectors | **54.19 x 33.50 x 19.70 mm** |
| PCB | 43 x 28 x 1.6 mm; 2 mm corner radius |
| Shell wall / roof / floor | 2.4 mm, except local pad recesses |
| Individual clip lengths, including roots | 14.7 / 14.1 mm |
| Clip beams / root radius | 1.2 x 4.8 mm / 1.2 mm in the bending plane |
| Clip engagement | 0.20 mm nominal |
| PCB lengthwise allowance | 0.10 mm per end, **0.20 mm total nominal travel** |
| Alignment-key clearance | 0.15 mm per mating face |
| Male DB9 rear-shell opening | 20.4 mm wide x 11.9 mm high, mirrored from the female end |
| Female DB9 rear-shell opening | 20.4 mm wide x 11.9 mm high |
| Programming opening | None; USB-C remains internal |

The male end now uses a mirrored copy of the female end's opening, shallow
mounting reliefs, pad recesses, wall thickness and low split seam. The old
male-specific circular through-passages are removed. The two end profiles are
checked for matching geometry, not merely equal opening dimensions.

This change follows the reported error in the supplied male connector model.
The female flange remains exposed as before. The actual male flange and plug
clearance must be verified on the real connector before use; the incorrect
model is not evidence of fit for the replacement profile.

The main PCB matches the USB outline, but the RP2040 Zero sits **2.835 mm farther
toward the male DB9**. Its adjacent clip over-travel stops are repositioned to
avoid the module substrate. Board end stops and PCB supports remain in place;
the extension does not add PCB travel or move the electronics.

### Size excludes protruding connectors

The 54.19 mm dimension measures only the plastic, from x=-6.75 to x=47.44 mm
in PCB coordinates. Do not use the known incorrect male model to infer the
finished adapter's tip-to-tip length.

## Connector support and rubber pads

The female **MSX end** retains the USB design's padded metal-backshell cradle.
Fit **two soft nonconductive rubber or silicone strips, 2 x 12 x 1 mm each**:

- Place one under and one above the female connector's rear flat metal shell.
- Use approximately Shore A 20-30 material. Do not substitute hard shims.
- Nominal pad compression is 0.20 mm each; each contact land is 24 mm2.
- Keep pad plus adhesive thickness at 1 mm. Start with a dry fit without pads,
  then check that adding them does not make closing force excessive.

The intended MSX extraction load path is case -> pad friction -> metal shell.
This is **not positive flange capture**. If the pads slip, the PCB and solder
joints can still carry extraction force. Tight PCB end stops do not replace
connector strain relief.

The mirrored male end includes matching pad recesses, but **male-end pad fit,
compression and grip are not validated**, because its model is incorrect.
Do not install pads there without measuring the actual metal contact lands.
Do not stuff pads around pins. Hold the exposed connector shell when
disconnecting a tight controller plug; do not use the case as a lever.
Grip and cycle-life testing remain necessary on both ends.

## Printing and assembly

Use PLA with a 0.4 mm nozzle and 0.20 mm layers as a starting point:

- Four perimeters, five top/bottom layers and 25-35% infill.
- Keep the supplied print orientations so the short arms run along layers.
- **Local supports are needed under all four horizontal clip arms.** Permit
  supports to start on part surfaces, not only on the print bed; inspect the
  slicer preview. Start with a removable 0.20 mm support-interface gap.
- The approximately 3.1 mm receiver windows should normally bridge, but check
  your printer's capability. Remove supports without bending clips outward.
- Prefer ordinary PLA, not brittle silk/filled blends. Avoid hot environments.

1. Disconnect all cables. Dry-fit the empty halves and verify gentle engagement
   of all four hooks before installing the board.
2. Lower the PCB onto its four support lands, male DB9 at one end and female at
   the other. Check the real male connector against the revised opening before
   closing; do not press on the pins or diode.
3. Fit the two rubber strips at the female MSX end after the unpadded fit is
   satisfactory. Align the stepped ends and alignment keys, then press near each
   clip. Stop if anything binds or a pad holds the seam open.
4. Check all four receiver windows and test plug clearance on unpowered spare
   connectors. Test that the female metal shell does not slide within the case
   under a gentle disconnected hand load.
5. Open by pressing each hook inward through its window with a blunt plastic
   tool and lifting that corner only enough to release it. Do not lever against
   still-engaged hooks or force a clip beyond its internal stop.

The clips are intended for occasional servicing, not repeated flexing.

## Regeneration

The MD source imports shared primitives, short clips, print orientations and
export validation from [the USB generator](../../../usb/1.10/case/case.py).
Keep that folder at its repository-relative location when regenerating; it is
not needed just to print the supplied STLs. This avoids maintaining two diverging
versions of the same clip design.

Python 3.13 was used. From this folder in PowerShell:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r requirements.txt
.\.venv\Scripts\python.exe md_case.py
.\.venv\Scripts\python.exe -m unittest discover -s . -p test_md_case.py -v
```

The MD CLI keeps the exterior dimensions fixed. Available fit adjustments:

| Option | Supported range |
| --- | --- |
| `--board-end-clearance` | 0.05-0.20 mm per end |
| `--key-clearance` | 0.12-0.25 mm |
| `--hook-engagement` | 0.15-0.25 mm; lower values reduce both closing force and retention |
| `--pad-compression` | 0.10-0.20 mm per female-end rubber pad |

Use `--output .\trial` for alternative exports without replacing the defaults.
Changes to module mounting height, connector models or outside dimensions need
a new CAD fit review; the MD CLI deliberately does not expose size-changing
parameters.

For component checks excluding the known incorrect male model, export MD 1.10
with KiCad 10:

```powershell
$assembly = Join-Path $env:TEMP 'retrolink-md-1.10-fit.step'
& 'C:\Program Files\KiCad\10.0\bin\kicad-cli.exe' pcb export step `
    --force --user-origin '102.15x102.25mm' --output $assembly ..\retrolink.kicad_pcb
.\.venv\Scripts\python.exe md_case.py --assembly $assembly
Remove-Item $assembly
```

The local origin is the PCB lower-left corner, z=0 at its underside; +X points
toward the female DB9 and +Y toward the internal USB-C socket.

The generator requires the expected original assembly bounds, identifies the
known incorrect male solid by its bounds and volume, and prints an explicit
warning before excluding it from checks and the preview. It fails if the
signature is not recognized. No PCB or component source model is modified.
When a corrected male model becomes available, update this validation path
rather than continuing to exclude it.

Validation covers:

- A 4 mm male-only extension, unchanged width/height, four clips and 0.20 mm PCB travel.
- No nominal PCB, half-to-half or remaining MD component overlap.
- Board and lid insertion at twelve sampled heights, excluding the male connector.
- Mirrored end profiles, removed through-passages and a closed USB-C side.
- Shifted-module clearance, solder keepouts, clip release and over-travel stops.
- Two female-end pad lands contacting only the metal backshell.
- Connected, watertight print meshes at bed z=0 and a two-solid STEP round-trip.
- Twelve MD tests plus thirteen USB regression tests.

These are geometric checks, not a slicer simulation, material deformation model,
mechanical load test or electrical validation.
