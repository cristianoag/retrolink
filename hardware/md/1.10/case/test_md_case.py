"""MD-specific geometry checks; run with unittest discovery in this directory."""

import unittest
from dataclasses import replace

from md_case import Dimensions, build, exclude_incorrect_male_model, mirror_end, port_tools, usb, validate


class MDEnclosureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = Dimensions()
        cls.base, cls.lid = build(cls.d)

    def test_four_mm_extension_only_toward_male(self):
        validate(self.base, self.lid, self.d)
        usb_base, usb_lid = usb.build(usb.Dimensions())
        actual = self.base.fuse(self.lid).BoundingBox()
        reference = usb_base.fuse(usb_lid).BoundingBox()
        for dimension, target in zip(("xlen", "ylen", "zlen"), (54.19, 33.50, 19.70)):
            self.assertAlmostEqual(getattr(actual, dimension), target, places=5)
        self.assertAlmostEqual(actual.xmin, reference.xmin - 4, places=5)
        for dimension in ("xmax", "ymin", "ymax", "zmin", "zmax"):
            self.assertAlmostEqual(getattr(actual, dimension), getattr(reference, dimension), places=5)

    def test_both_db9_ports_and_flanges_clear(self):
        for part in (self.base, self.lid):
            for name, tool in port_tools(self.d).items():
                with self.subTest(port=name):
                    self.assertLess(part.intersect(tool).Volume(), 1e-5)
            self.assertLess(part.BoundingBox().xmax, 47.64)

    def test_end_profiles_are_exact_mirrors(self):
        d = self.d
        slab = usb.box(d.xmax - 3.0, d.xmax + 1, d.ymin - 1, d.ymax + 1, d.bottom - 1, d.top + 1)
        for part in (self.base, self.lid):
            female = part.intersect(slab)
            male = mirror_end(part.intersect(mirror_end(slab, d)), d)
            self.assertLess(female.cut(male).Volume() + male.cut(female).Volume(), 1e-5)
        # The previous long circular passages are now backed by solid plastic.
        for y in (1.51, 26.50):
            plug = usb.box(d.xmin + 1.2, d.xmin + 2.4, y - 0.4, y + 0.4, 0.2, 1.5)
            self.assertLess(plug.cut(self.base.fuse(self.lid)).Volume(), 1e-5)

    def test_bad_model_exclusion_requires_known_signature(self):
        with self.assertRaisesRegex(ValueError, "known incorrect male DB9"):
            exclude_incorrect_male_model(usb.box(0, 1, 0, 1, 0, 1))

    def test_four_short_clips(self):
        for right in (False, True):
            for north in (False, True):
                arm = usb.clip(self.d, right)
                self.assertLess(arm.BoundingBox().xlen, 15)
                if north:
                    arm = usb.other_side(arm, self.d)
                self.assertGreater(
                    self.base.intersect(arm.translate((0, 0, 0.3))).Volume(), 0.03
                )
                dy = -0.3 if north else 0.3
                for height in (0, 0.3, 1, 2, 4, 6):
                    self.assertLess(
                        self.base.intersect(arm.translate((0, dy, height))).Volume(), 1e-5
                    )

    def test_stops_and_rigid_insertion(self):
        rigid = self.lid
        for right in (False, True):
            arm = usb.clip(self.d, right)
            rigid = rigid.cut(arm).cut(usb.other_side(arm, self.d))
            beam = arm.intersect(usb.box(5, 43.1, -4, 4, 2.2, 7))
            rest = self.lid.cut(arm)
            self.assertLess(rest.intersect(beam.translate((0, 0.3, 0))).Volume(), 1e-5)
            self.assertGreater(rest.intersect(beam.translate((0, 0.5, 0))).Volume(), 0.01)
        for height in (0, 0.25, 0.5, 1, 2, 3, 6, 14):
            self.assertLess(self.base.intersect(rigid.translate((0, 0, height))).Volume(), 1e-5)
        for dx, dy in ((0.2, 0), (-0.2, 0), (0, 0.2), (0, -0.2)):
            self.assertGreater(self.base.intersect(rigid.translate((dx, dy, 0))).Volume(), 0.01)

    def assert_board_travel(self, base, d):
        pcb = usb.rounded_box(0, 43, 0, 28, 0, 1.6, 2)
        for height in (0, d.board_vertical_play + 0.15):
            for direction in (-1, 1):
                free = pcb.translate((direction * (d.board_end_clearance - 0.01), 0, height))
                blocked = pcb.translate((direction * (d.board_end_clearance + 0.01), 0, height))
                self.assertLess(base.intersect(free).Volume(), 1e-5)
                self.assertGreater(base.intersect(blocked).Volume(), 1e-4)

    def test_board_lengthwise_play(self):
        self.assertAlmostEqual(self.d.board_end_clearance * 2, 0.20)
        self.assert_board_travel(self.base, self.d)

    def test_closed_usb_c_side(self):
        # Shifted MD USB-C and cable envelope; there must be a solid side wall.
        wall = usb.box(15.4, 28.5, 28.35, 30.75, 2.165, 9.365)
        self.assertLess(wall.cut(self.base.fuse(self.lid)).Volume(), 1e-5)

    def test_solder_keepouts(self):
        for x0, x1, y0, y1 in (
            (12.5, 16.1, 2.3, 25.6), (27.8, 31.3, 2.3, 25.6),
            (15.6, 28.3, 1.8, 5.6), (20.6, 24.7, 16.5, 22.2),
            (0, 4.8, 7.5, 20.5), (38.5, 43, 6.5, 21.5),
        ):
            self.assertLess(
                self.base.intersect(usb.box(x0, x1, y0, y1, -3.2, 0)).Volume(), 1e-5
            )

    def test_msx_pad_support_and_print_orientation(self):
        lower, upper = usb.pads(self.d)
        for pad in (lower, upper):
            self.assertAlmostEqual(pad.Volume(), 24)
            for part in (self.base, self.lid):
                self.assertLess(part.intersect(pad).Volume(), 1e-5)
        self.assertAlmostEqual(
            self.base.intersect(lower.translate((0, 0, -0.01))).Volume() / 0.01, 24, places=4
        )
        self.assertAlmostEqual(
            self.lid.intersect(upper.translate((0, 0, 0.01))).Volume() / 0.01, 24, places=4
        )
        for shape in usb.print_shapes(self.base, self.lid, self.d):
            self.assertEqual(len(shape.Solids()), 1)
            self.assertAlmostEqual(shape.BoundingBox().zmin, 0, places=5)

    def test_adjustment_limits_keep_size(self):
        for settings in (
            dict(board_end_clearance=0.05, key_clearance=0.12, hook_engagement=0.15, pad_compression=0.10),
            dict(board_end_clearance=0.20, key_clearance=0.25, hook_engagement=0.25, pad_compression=0.20),
        ):
            d = replace(self.d, **settings)
            base, lid = build(d)
            validate(base, lid, d)
            self.assert_board_travel(base, d)
            b = base.fuse(lid).BoundingBox()
            self.assertAlmostEqual(b.xlen, 54.19, places=5)
            self.assertAlmostEqual(b.ylen, 33.5, places=5)
            self.assertAlmostEqual(b.zlen, 19.7, places=5)

    def test_invalid_adjustments_fail(self):
        for settings in (
            dict(board_end_clearance=0), dict(key_clearance=0.3),
            dict(hook_engagement=0.4), dict(pad_compression=0.5),
        ):
            with self.subTest(settings=settings), self.assertRaises(ValueError):
                build(replace(self.d, **settings))


if __name__ == "__main__":
    unittest.main()
