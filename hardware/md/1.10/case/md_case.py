"""MD 1.10 enclosure, extended 4 mm toward the male DB9 with mirrored end profiles."""

import argparse
import importlib.util
import sys
from dataclasses import dataclass
from pathlib import Path

import cadquery as cq


USB_SOURCE = Path(__file__).resolve().parents[3] / "usb" / "1.10" / "case" / "case.py"
spec = importlib.util.spec_from_file_location("retrolink_usb_case", USB_SOURCE)
if spec is None or spec.loader is None:
    raise ImportError(f"Cannot load shared enclosure geometry: {USB_SOURCE}")
usb = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = usb
spec.loader.exec_module(usb)


@dataclass(frozen=True)
class Dimensions(usb.Dimensions):
    @property
    def xmin(self):
        return super().xmin - 4.0


def mirror_end(shape, d):
    return shape.mirror("YZ", ((d.xmin + d.xmax) / 2, 0, 0))


def port_tools(d):
    tools = {name: tool for name, tool in usb.port_tools(d).items() if name != "usb_a"}
    tools.update({f"male_{name}": mirror_end(tool, d) for name, tool in list(tools.items())})
    return tools


def build(d):
    end_sum = d.xmin + d.xmax
    base, lid = usb.shell_halves(
        d, port_tools(d), left_split=(end_sum - 43.8, 0.595),
        cavity_xmin=end_sum - 44.4,
    )
    # RZ1 is 2.835 mm closer to J2 than on USB; keep the stop clear of its substrate.
    return usb.finish_case(
        base, lid, d, stop_spans=((10.0, 12.0), (34.4, 36.4)),
        left_end_stop_ys=(4.6, 22.0), left_end_stop_top=0.895,
    )


def exclude_incorrect_male_model(assembly):
    solids = assembly.Solids()
    candidates = [
        solid for solid in solids
        if abs(solid.BoundingBox().xmin - (-12.1374)) < 0.01
        and abs(solid.BoundingBox().xmax - 4.5575) < 0.01
        and abs(solid.Volume() - 1298.855) < 0.1
    ]
    if len(candidates) != 1:
        raise ValueError("Cannot identify the known incorrect male DB9 model; review assembly")
    print("WARNING: known incorrect male DB9 model excluded from fit checks and preview; "
          "the replacement male-end profile requires a physical fit test.")
    return cq.Compound.makeCompound([solid for solid in solids if not solid.isSame(candidates[0])])


def validate(base, lid, d, assembly_path=None):
    for name, shape in (("base", base), ("lid", lid)):
        usb.check_solid(shape, name)
        pcb = usb.rounded_box(0, d.board_x, 0, d.board_y, 0, d.board_thickness, 2)
        if shape.intersect(pcb).Volume() > 1e-5:
            raise ValueError(f"{name} intersects the nominal MD PCB")
        for port, tool in port_tools(d).items():
            if shape.intersect(tool).Volume() > 1e-5:
                raise ValueError(f"{name} obstructs {port}")
    if base.intersect(lid).Volume() > 1e-5:
        raise ValueError("MD halves interfere")

    assembly = None
    if assembly_path:
        if d.module_lift != 0:
            raise ValueError("Assembly validation requires the default module height")
        assembly = cq.importers.importStep(str(assembly_path)).val()
        bounds = assembly.BoundingBox()
        expected = (-12.1374, 54.54, -1.33, 29.34, -5.615, 7.8445)
        actual = (bounds.xmin, bounds.xmax, bounds.ymin, bounds.ymax, bounds.zmin, bounds.zmax)
        if any(abs(a - b) > 0.1 for a, b in zip(actual, expected)):
            raise ValueError("Unexpected MD assembly bounds; check revision and STEP origin")
        assembly = exclude_incorrect_male_model(assembly)
        for name, shape in (("base", base), ("lid", lid)):
            overlap = shape.intersect(assembly).Volume()
            print(f"{name}/MD assembly intersection: {overlap:.6f} mm^3")
            if overlap > 1e-5:
                raise ValueError(f"{name} interferes with MD components")
        for height in (0.25, 0.5, 1, 2, 3, 4, 5, 6, 8, 10, 12, 14):
            if base.intersect(assembly.translate((0, 0, height))).Volume() > 1e-5:
                raise ValueError(f"Populated MD PCB insertion blocked at +{height} mm")
            if lid.translate((0, 0, height)).intersect(assembly).Volume() > 1e-5:
                raise ValueError(f"MD lid insertion blocked at +{height} mm")
        metal = [
            solid for solid in assembly.Solids()
            if abs(solid.BoundingBox().xmin - 43.68) < 0.01
            and abs(solid.BoundingBox().xmax - 54.54) < 0.01 and solid.Volume() > 1000
        ]
        if len(metal) != 1:
            raise ValueError("Cannot identify MSX female DB9 metal shell")
        for pad, compressed in zip(usb.pads(d), usb.pads(d, compressed=True)):
            contact = pad.intersect(metal[0]).Volume()
            if abs(contact - 24 * d.pad_compression) > 1e-4:
                raise ValueError("MSX pad does not contact the full metal land")
            if abs(pad.intersect(assembly).Volume() - contact) > 1e-4:
                raise ValueError("MSX pad contacts a non-shell component")
            if compressed.intersect(assembly).Volume() > 1e-5:
                raise ValueError("Compressed pad intersects MD components")
        print("MD PCB/lid insertion clear excluding male connector; two MSX pad lands contact metal only")
    return assembly


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--assembly", type=Path, help="KiCad MD STEP, origin 102.15x102.25mm")
    parser.add_argument("--board-end-clearance", type=float, default=0.10)
    parser.add_argument("--key-clearance", type=float, default=0.15)
    parser.add_argument("--hook-engagement", type=float, default=0.20)
    parser.add_argument("--pad-compression", type=float, default=0.20)
    args = parser.parse_args()
    d = Dimensions(
        board_end_clearance=args.board_end_clearance, key_clearance=args.key_clearance,
        hook_engagement=args.hook_engagement, pad_compression=args.pad_compression,
    )
    base, lid = build(d)
    electronics = validate(base, lid, d, args.assembly)
    usb.export(base, lid, d, args.output, electronics, stem="retrolink-md-1.10")
    print(f"MD case: {d.xmax - d.xmin:.2f} x {d.ymax - d.ymin:.2f} x {d.top - d.bottom:.2f} mm")


if __name__ == "__main__":
    main()
