"""Compact four-clip PLA enclosure with a padded DB9 cradle, in millimetres."""

import argparse
import math
from dataclasses import dataclass
from pathlib import Path

import cadquery as cq
import trimesh

# M3 socket-head cap screw (maximum head) and nylon hex nut; screw is M3x16 at the default height.
SCREW_HEAD_DIAMETER = 5.68
SCREW_HEAD_HEIGHT = 3.0
SCREW_SHAFT_DIAMETER = 3.0
NUT_ACROSS_FLATS = 5.5
NUT_THICKNESS = 2.4
PCB_HOLE_DIAMETER = 3.2
# Printed features around the screw: head recess in the roof, nut trap in the floor.
HEAD_RECESS_DIAMETER = 6.2
HEAD_RECESS_DEPTH = 3.3
NUT_POCKET_ACROSS_FLATS = 5.7
NUT_POCKET_DEPTH = 3.4
NUT_CEILING = 1.2
SCREW_BORE_DIAMETER = 3.4
FLOOR_BOSS_RADIUS = 4.4
ROOF_BOSS_RADIUS = 4.3


@dataclass(frozen=True)
class Dimensions:
    # Coordinates: PCB lower-left at (0, 0), underside at z=0, USB-A at -X.
    board_x: float = 43.0
    board_y: float = 28.0
    board_thickness: float = 1.6
    fit_clearance: float = 0.35
    board_end_clearance: float = 0.10
    wall: float = 2.4
    bottom: float = -7.8
    floor_top: float = -5.4
    seam: float = 7.6
    module_lift: float = 0.0
    board_vertical_play: float = 0.20
    flange_clearance: float = 0.20
    key_clearance: float = 0.15
    clip_thickness: float = 1.2
    clip_gap: float = 0.35
    hook_engagement: float = 0.20
    pad_thickness: float = 1.0
    pad_compression: float = 0.20
    # PCB mounting-hole centres (x, y) for the M3 nylon screws.
    mount_holes: tuple = ((3.0, 25.0), (40.0, 3.0))

    @property
    def head_seat(self):
        return self.top - HEAD_RECESS_DEPTH

    @property
    def nut_seat(self):
        return self.bottom + NUT_POCKET_DEPTH

    @property
    def screw_length(self):
        return self.head_seat - (self.nut_seat - NUT_THICKNESS - 0.6)

    @property
    def roof(self):
        return 9.5 + self.module_lift

    @property
    def top(self):
        return self.roof + self.wall

    @property
    def xmin(self):
        return -self.fit_clearance - self.wall

    @property
    def ymin(self):
        return self.side_inner - self.wall

    @property
    def xmax(self):
        return self.flange_back

    @property
    def ymax(self):
        return self.board_y - self.ymin

    @property
    def side_inner(self):
        return -self.fit_clearance

    @property
    def beam_outer(self):
        return self.side_inner + self.clip_gap

    @property
    def flange_back(self):
        return 47.64 - self.flange_clearance

    @property
    def pad_gap(self):
        return self.pad_thickness - self.pad_compression

    @property
    def lower_pad_seat(self):
        return -4.815 - self.pad_gap

    @property
    def upper_pad_seat(self):
        return 6.005 + self.pad_gap

    def validate(self):
        limits = {
            "fit_clearance": (self.fit_clearance, 0.25, 0.55),
            "board_end_clearance": (self.board_end_clearance, 0.05, 0.20),
            "module_lift": (self.module_lift, 0.0, 6.0),
            "flange_clearance": (self.flange_clearance, 0.15, 0.25),
            "key_clearance": (self.key_clearance, 0.12, 0.25),
            "hook_engagement": (self.hook_engagement, 0.15, 0.25),
            "pad_compression": (self.pad_compression, 0.10, 0.20),
        }
        for name, (value, low, high) in limits.items():
            if not low <= value <= high:
                raise ValueError(f"{name} must be between {low} and {high} mm")
        if self.clip_strain() > 0.009:
            raise ValueError("Clip strain estimate exceeds the 0.9% screening limit")

    def clip_strain(self):
        # Shortest root-tangent to hook distance is 9 mm; allow 0.15 mm print error.
        return 1.5 * self.clip_thickness * (self.hook_engagement + 0.15) / 9**2


def box(x0, x1, y0, y1, z0, z1):
    return (
        cq.Workplane("XY")
        .box(x1 - x0, y1 - y0, z1 - z0, centered=False)
        .translate((x0, y0, z0))
        .val()
    )


def rounded_box(x0, x1, y0, y1, z0, z1, radius):
    return (
        cq.Workplane("XY")
        .box(x1 - x0, y1 - y0, z1 - z0, centered=False)
        .edges("|Z")
        .fillet(radius)
        .translate((x0, y0, z0))
        .val()
    )


def cylinder(x, y, radius, z0, z1):
    return cq.Workplane("XY").workplane(offset=z0).center(x, y).circle(radius).extrude(z1 - z0).val()


def hex_prism(x, y, across_flats, z0, z1):
    # Flats face +/-X, so the corners stay clear of the DB9 opening.
    r = across_flats / math.sqrt(3)
    points = [(x + r * math.sin(math.radians(a)), y + r * math.cos(math.radians(a)))
              for a in range(0, 360, 60)]
    return cq.Workplane("XY").workplane(offset=z0).polyline(points).close().extrude(z1 - z0).val()


def mount_hardware(d):
    """Installed M3 screw (head + shaft) and nylon nut for each mounting hole."""
    parts = []
    for x, y in d.mount_holes:
        head = cylinder(x, y, SCREW_HEAD_DIAMETER / 2, d.head_seat, d.head_seat + SCREW_HEAD_HEIGHT)
        shaft = cylinder(x, y, SCREW_SHAFT_DIAMETER / 2, d.head_seat - d.screw_length, d.head_seat)
        nut = hex_prism(x, y, NUT_ACROSS_FLATS, d.nut_seat - NUT_THICKNESS, d.nut_seat)
        parts.append((head, shaft, nut))
    return parts


def mount_tools(d):
    """Cuts for the head recess (lid), nut trap (base) and screw bore (both)."""
    head, nut, bore = [], [], []
    for x, y in d.mount_holes:
        head.append(cylinder(x, y, HEAD_RECESS_DIAMETER / 2, d.head_seat, d.top + 1))
        nut.append(hex_prism(x, y, NUT_POCKET_ACROSS_FLATS, d.bottom - 1, d.nut_seat))
        bore.append(cylinder(x, y, SCREW_BORE_DIAMETER / 2, d.bottom - 1, d.top + 1))
    return head, nut, bore


def port_tools(d):
    tools = {
        "usb_a": box(d.xmin - 1, 3, 5.3, 22.7, 1.0, 9.5),
        "db9_rear": box(42.5, d.xmax + 1, 3.8, 24.2, -5.35, 6.55),
    }
    for index, y in enumerate((1.505, 26.505)):
        recess = (
            cq.Workplane("YZ")
            .center(y, 0.595)
            .circle(3.0)
            .extrude(d.xmax + 1 - 46.6)
            .val()
            .translate((46.6, 0, 0))
        )
        tools[f"db9_mount_{index}"] = recess
    tools["lower_pad_pocket"] = box(44.85, 46.95, 7.9, 20.1, d.lower_pad_seat, -4.7)
    tools["upper_pad_pocket"] = box(44.85, 46.95, 7.9, 20.1, 5.9, d.upper_pad_seat)
    return tools


def other_side(shape, d):
    return shape.mirror("XZ", (0, d.board_y / 2, 0))


def clip(d, right=False):
    # Concave radius is in XY, where the beam bends; the root reaches the roof.
    length = 14.1 if right else 14.7
    beam = (
        cq.Workplane("XY").moveTo(0, -1.2).lineTo(1.5, -1.2)
        .threePointArc((1.851472, -0.351472), (2.7, 0))
        .lineTo(length, 0).lineTo(length, d.clip_thickness)
        .lineTo(0, d.clip_thickness).close().extrude(4.8)
        .translate((0.3, d.beam_outer, 2.2)).val()
    )
    root = box(0.3, 1.8, d.beam_outer - 1.2,
               d.beam_outer + d.clip_thickness, 2.2, d.roof + 0.1)
    outer = d.beam_outer
    hook = (
        cq.Workplane("YZ").polyline([
            (outer + 0.1, 2.6), (outer, 2.6),
            (outer - d.clip_gap - d.hook_engagement, 4.4),
            (outer - d.clip_gap - d.hook_engagement, 6.2), (outer + 0.1, 6.2),
        ]).close().extrude(2.4).translate((length + 0.3 - 2.4, 0, 0)).val()
    )
    shape = beam.fuse(root).fuse(hook)
    return shape.mirror("YZ", (23.1, 0, 0)) if right else shape


def pads(d, compressed=False):
    thickness = d.pad_gap if compressed else d.pad_thickness
    return (
        box(44.9, 46.9, 8, 20, d.lower_pad_seat, d.lower_pad_seat + thickness),
        box(44.9, 46.9, 8, 20, d.upper_pad_seat - thickness, d.upper_pad_seat),
    )


def keys(d):
    for x in (6.0, 36.0):
        key = box(x, x + 4, d.side_inner - 1.55, d.side_inner - 0.75, d.seam - 2, d.seam + 0.2)
        c = d.key_clearance
        pocket = box(
            x - c, x + 4 + c, d.side_inner - 1.55 - c, d.side_inner - 0.75 + c,
            d.seam - 2 - c, d.seam + 0.3,
        )
        yield key, pocket
        yield other_side(key, d), other_side(pocket, d)


def shell_halves(d, tools, left_split=None, cavity_xmin=None):
    d.validate()
    outer = rounded_box(
        d.xmin, d.xmax, d.ymin, d.ymax, d.bottom, d.top, 2.4
    )
    cavity = rounded_box(
        -d.fit_clearance if cavity_xmin is None else cavity_xmin, 44.4,
        d.side_inner, d.board_y - d.side_inner,
        d.floor_top, d.roof, 0.8,
    )
    shell = outer.cut(cavity)
    for tool in tools.values():
        shell = shell.cut(tool)
    # Split at the rear mounting-collar center so the populated PCB drops in.
    bottom_region = box(
        d.xmin - 1, 43.8, d.ymin - 1, d.ymax + 1, d.bottom - 1, d.seam
    ).fuse(box(43.8, d.xmax + 1, d.ymin - 1, d.ymax + 1, d.bottom - 1, 0.595))
    if left_split is not None:
        x, z = left_split
        bottom_region = bottom_region.cut(box(
            d.xmin - 1, x, d.ymin - 1, d.ymax + 1, z, d.seam + 1,
        ))
    base = shell.intersect(bottom_region)
    lid = shell.cut(bottom_region)
    return base, lid


def finish_case(base, lid, d, stop_spans=((12.6, 14.8), (34.4, 36.4)),
                left_end_stop_ys=(2.2, 24.4), left_end_stop_top=1.5, tools=None):
    # Four paired lands contact bare PCB, not the diode or through-hole pads.
    for x in (5.0, 36.0):
        for y in (1.6, 25.2):
            base = base.fuse(box(x, x + 3, y, y + 1.2, d.floor_top - 0.1, 0))
            lid = lid.fuse(box(
                x, x + 3, y, y + 1.2,
                d.board_thickness + d.board_vertical_play, d.roof + 0.1,
            ))
    # Locate on straight PCB edges, away from the rounded corners and connectors.
    for y in left_end_stop_ys:
        base = base.fuse(box(
            -d.fit_clearance - 0.1, -d.board_end_clearance,
            y, y + 1.4, d.floor_top - 0.1, left_end_stop_top,
        ))
    for y in (2.2, 24.4):
        base = base.fuse(box(
            d.board_x + d.board_end_clearance, 44.5,
            y, y + 1.4, d.floor_top - 0.1, 0.595,
        ))

    for right, x0, x1 in ((False, 12.25, 15.35), (True, 31.45, 34.55)):
        arm = clip(d, right)
        root_pocket = box(-0.05, 3.35, d.beam_outer - 1.55,
                          d.beam_outer + d.clip_thickness + 0.35, 2.0, d.seam + 0.1)
        if right:
            root_pocket = root_pocket.mirror("YZ", (23.1, 0, 0))
        window = box(x0, x1, d.ymin - 1, d.beam_outer + 0.1, 3.3, 6.35)
        for tool in (root_pocket, window):
            base = base.cut(tool).cut(other_side(tool, d))
        lid = lid.fuse(arm).fuse(other_side(arm, d))
    for x0, x1 in stop_spans:
        stop_y = d.beam_outer + d.clip_thickness + 0.45
        stop = box(x0, x1, stop_y, stop_y + 0.8, 2.2, d.roof + 0.1)
        lid = lid.fuse(stop).fuse(other_side(stop, d))
    for key, pocket in keys(d):
        base = base.cut(pocket)
        lid = lid.fuse(key)

    # Board guides locate the PCB; the padded metal cradle carries connector loads.
    for x in (6, 37):
        guide = box(x, x + 2, -d.fit_clearance - 1.2, -d.fit_clearance, d.floor_top - 0.1, 1.5)
        base = base.fuse(guide).fuse(other_side(guide, d))

    # M3 nylon screw: head recessed in the roof, nut trapped under the floor. Bosses
    # stay clear of the connector openings and the 3.2 mm underside solder keepout.
    head_cuts, nut_cuts, bore_cuts = mount_tools(d)
    for x, y in d.mount_holes:
        floor_boss = cylinder(x, y, FLOOR_BOSS_RADIUS, d.floor_top - 0.1, d.nut_seat + NUT_CEILING)
        roof_boss = cylinder(x, y, ROOF_BOSS_RADIUS, d.seam, d.roof + 0.1)
        for tool in (tools or {}).values():
            floor_boss, roof_boss = floor_boss.cut(tool), roof_boss.cut(tool)
        base, lid = base.fuse(floor_boss), lid.fuse(roof_boss)
    for tool in nut_cuts + bore_cuts:
        base = base.cut(tool)
    for tool in head_cuts + bore_cuts:
        lid = lid.cut(tool)
    return base.clean(), lid.clean()


def build(d):
    tools = port_tools(d)
    return finish_case(*shell_halves(d, tools), d, tools=tools)


def print_shapes(base, lid, d):
    return (
        base.translate((-d.xmin, -d.ymin, -d.bottom)),
        lid.rotate((0, 0, 0), (1, 0, 0), 180)
        .translate((-d.xmin, d.ymax, d.top)),
    )


def check_solid(shape, name):
    if not shape.isValid() or len(shape.Solids()) != 1 or shape.Volume() <= 0:
        raise ValueError(f"{name} is not one valid positive-volume solid")


def check_mount_hardware(base, lid, d, assembly=None):
    pcb = rounded_box(0, d.board_x, 0, d.board_y, 0, d.board_thickness, 2)
    for x, y in d.mount_holes:
        pcb = pcb.cut(cylinder(x, y, PCB_HOLE_DIAMETER / 2, -1, d.board_thickness + 1))
    solids = [("base", base), ("lid", lid), ("PCB", pcb)]
    if assembly is not None:
        solids.append(("assembly", assembly))
    for (x, y), parts in zip(d.mount_holes, mount_hardware(d)):
        for part_name, part in zip(("screw head", "screw shaft", "nylon nut"), parts):
            for name, shape in solids:
                overlap = part.intersect(shape).Volume()
                if overlap > 1e-5:
                    raise ValueError(
                        f"M3 {part_name} at ({x}, {y}) interferes with {name}: {overlap:.6f} mm^3"
                    )
    print(f"M3x{d.screw_length:.0f} screws and nylon nuts clear base, lid, PCB holes and components")


def validate(base, lid, d, assembly_path=None):
    for name, shape in (("base", base), ("lid", lid)):
        check_solid(shape, name)
    overlap = base.intersect(lid).Volume()
    if overlap > 1e-5:
        raise ValueError(f"Case halves interfere by {overlap:.6f} mm^3")
    board = rounded_box(0, 43, 0, 28, 0, d.board_thickness, 2.0)
    for name, shape in (("base", base), ("lid", lid)):
        overlap = shape.intersect(board).Volume()
        if overlap > 1e-5:
            raise ValueError(f"{name} intersects nominal PCB: {overlap:.6f} mm^3")
        for port, tool in port_tools(d).items():
            overlap = shape.intersect(tool).Volume()
            if overlap > 1e-5:
                raise ValueError(f"{name} obstructs {port}: {overlap:.6f} mm^3")

    assembly = None
    if assembly_path:
        if d.module_lift != 0:
            raise ValueError("STEP validation needs the default module height")
        assembly = cq.importers.importStep(str(assembly_path)).val()
        bounds = assembly.BoundingBox()
        expected = (-0.17, 54.54, -1.33, 29.34, -5.615, 8.655)
        actual = (
            bounds.xmin, bounds.xmax, bounds.ymin, bounds.ymax,
            bounds.zmin, bounds.zmax,
        )
        if any(abs(a - b) > 0.1 for a, b in zip(actual, expected)):
            raise ValueError("Unexpected assembly bounds; check revision and STEP origin")
        for name, shape in (("base", base), ("lid", lid)):
            overlap = shape.intersect(assembly).Volume()
            print(f"{name}/KiCad assembly intersection: {overlap:.6f} mm^3")
            if overlap > 1e-5:
                raise ValueError(f"{name} intersects the supplied assembly")
        for height in (0.5, 1, 2, 3, 4, 5, 6, 8, 10, 12):
            overlap = base.intersect(assembly.translate((0, 0, height))).Volume()
            if overlap > 1e-5:
                raise ValueError(f"PCB insertion blocked at +{height} mm: {overlap:.6f} mm^3")
        print("Populated PCB insertion: ten sampled heights clear")
        for height in (0.25, 0.5, 1, 2, 4, 6, 10, 14):
            if lid.translate((0, 0, height)).intersect(assembly).Volume() > 1e-5:
                raise ValueError(f"Lid contacts electronics at +{height} mm")
        metal_candidates = []
        for solid in assembly.Solids():
            b = solid.BoundingBox()
            if abs(b.xmin - 43.68) < 0.01 and abs(b.xmax - 54.54) < 0.01 and solid.Volume() > 1000:
                metal_candidates.append(solid)
        if len(metal_candidates) != 1:
            raise ValueError("Could not identify the supplied DB9 metal backshell")
        metal = metal_candidates[0]
        for pad, compressed in zip(pads(d), pads(d, compressed=True)):
            contact = pad.intersect(metal).Volume()
            if abs(contact - 24 * d.pad_compression) > 1e-4:
                raise ValueError("A pad does not contact the full 24 mm^2 metal land")
            if abs(pad.intersect(assembly).Volume() - contact) > 1e-4:
                raise ValueError("Pad pressure would reach non-shell components")
            if compressed.intersect(assembly).Volume() > 1e-5:
                raise ValueError("Compressed pad envelope overlaps electronics")
        print("DB9 pads: two 24 mm^2 metal-only contact lands; lid insertion clear")
    check_mount_hardware(base, lid, d, assembly)
    print(f"Rubber pad compression: {d.pad_compression / d.pad_thickness:.0%} (not a load rating)")
    print(f"Short clip strain screen: {d.clip_strain():.2%} (not a strength test)")
    return assembly


def preview(base, lid, d, destination, electronics=None, pad_shapes=None):
    import vtk

    window = vtk.vtkRenderWindow()
    window.SetOffScreenRendering(1)
    window.SetSize(1500, 850)
    for index, exploded in enumerate((False, True)):
        renderer = vtk.vtkRenderer()
        renderer.SetViewport(index / 2, 0, (index + 1) / 2, 1)
        renderer.SetBackground(0.94, 0.95, 0.97)
        window.AddRenderer(renderer)
        lift = 18 if exploded else 0
        shapes = [
            (base, (0.16, 0.26, 0.36)),
            (lid.translate((0, 0, lift)), (0.88, 0.53, 0.16)),
        ]
        lower_pad, upper_pad = pads(d, compressed=True) if pad_shapes is None else pad_shapes
        shapes.extend([
            (lower_pad, (0.12, 0.12, 0.12)),
            (upper_pad.translate((0, 0, lift)), (0.12, 0.12, 0.12)),
        ])
        if electronics is not None:
            shapes.append((electronics, (0.62, 0.65, 0.68)))
        for shape, color in shapes:
            vertices, triangles = shape.tessellate(0.08, 0.1)
            points = vtk.vtkPoints()
            for vertex in vertices:
                points.InsertNextPoint(vertex.x, vertex.y, vertex.z)
            cells = vtk.vtkCellArray()
            for triangle in triangles:
                cells.InsertNextCell(3)
                for vertex_id in triangle:
                    cells.InsertCellPoint(vertex_id)
            data = vtk.vtkPolyData()
            data.SetPoints(points)
            data.SetPolys(cells)
            normals = vtk.vtkPolyDataNormals()
            normals.SetInputData(data)
            mapper = vtk.vtkPolyDataMapper()
            mapper.SetInputConnection(normals.GetOutputPort())
            actor = vtk.vtkActor()
            actor.SetMapper(mapper)
            actor.GetProperty().SetColor(*color)
            actor.GetProperty().SetAmbient(0.25)
            actor.GetProperty().SetDiffuse(0.75)
            renderer.AddActor(actor)
        camera = renderer.GetActiveCamera()
        camera.SetPosition(105, 110, 80)
        camera.SetFocalPoint(22, 14, 8)
        camera.SetViewUp(0, 0, 1)
        camera.ParallelProjectionOn()
        renderer.ResetCamera()
        label = vtk.vtkTextActor()
        label.SetInput("COMPACT - FOUR SHORT CLIPS" if not exploded else "EXPLODED - PADDED DB9 CRADLE")
        label.GetPositionCoordinate().SetCoordinateSystemToNormalizedViewport()
        label.SetPosition(0.03, 0.035)
        label.GetTextProperty().SetFontSize(22)
        label.GetTextProperty().SetColor(0.15, 0.2, 0.3)
        renderer.AddActor2D(label)
    window.Render()
    capture = vtk.vtkWindowToImageFilter()
    capture.SetInput(window)
    capture.Update()
    writer = vtk.vtkPNGWriter()
    writer.SetFileName(str(destination))
    writer.SetInputConnection(capture.GetOutputPort())
    writer.Write()
    window.Finalize()
    if not destination.is_file() or destination.stat().st_size == 0:
        raise ValueError("Preview export failed")


def export(base, lid, d, output, electronics=None, stem="retrolink-usb-1.11", pad_shapes=None):
    output.mkdir(parents=True, exist_ok=True)
    for name, shape in zip(("base", "lid"), print_shapes(base, lid, d)):
        path = output / f"{stem}-{name}.stl"
        cq.exporters.export(shape, str(path), tolerance=0.03, angularTolerance=0.1)
        mesh = trimesh.load_mesh(path)
        if not mesh.is_watertight or not mesh.is_winding_consistent or mesh.volume <= 0:
            raise ValueError(f"{path.name} is not a closed, correctly oriented mesh")
        if len(mesh.split()) != 1 or abs(mesh.bounds[0, 2]) > 1e-5:
            raise ValueError(f"{path.name} is disconnected or not on the print bed")
        print(f"{path.name}: watertight, one body, {len(mesh.faces)} triangles")
    assembly = cq.Compound.makeCompound([base, lid])
    step = output / f"{stem}-case.step"
    cq.exporters.export(assembly, str(step))
    reloaded = cq.importers.importStep(str(step)).val()
    if len(reloaded.Solids()) != 2 or not reloaded.isValid():
        raise ValueError("STEP round-trip did not preserve two valid solids")
    preview(base, lid, d, output / "preview.png", electronics, pad_shapes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent)
    parser.add_argument("--assembly", type=Path, help="KiCad STEP, origin 102.15x102.25mm")
    parser.add_argument("--module-lift", type=float, default=0.0)
    parser.add_argument("--fit-clearance", type=float, default=0.35)
    parser.add_argument("--board-end-clearance", type=float, default=0.10)
    parser.add_argument("--flange-clearance", type=float, default=0.20)
    parser.add_argument("--key-clearance", type=float, default=0.15)
    parser.add_argument("--hook-engagement", type=float, default=0.20)
    parser.add_argument("--pad-compression", type=float, default=0.20)
    args = parser.parse_args()
    d = Dimensions(
        module_lift=args.module_lift,
        fit_clearance=args.fit_clearance,
        board_end_clearance=args.board_end_clearance,
        flange_clearance=args.flange_clearance,
        key_clearance=args.key_clearance,
        hook_engagement=args.hook_engagement,
        pad_compression=args.pad_compression,
    )
    base, lid = build(d)
    electronics = validate(base, lid, d, args.assembly)
    export(base, lid, d, args.output, electronics)
    print(f"Case: {d.xmax - d.xmin:.2f} x {d.ymax - d.ymin:.2f} x {d.top - d.bottom:.2f} mm")


if __name__ == "__main__":
    main()
