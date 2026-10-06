"""Run with python -m unittest discover -s <case directory>."""

import unittest
from dataclasses import replace

from case import Dimensions, box, build, clip, other_side, pads, print_shapes, rounded_box, validate


class EnclosureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.d = Dimensions()
        cls.base, cls.lid = build(cls.d)

    def test_compact_outline_and_exposed_flange(self):
        validate(self.base, self.lid, self.d)
        for shape in (self.base, self.lid):
            self.assertLessEqual(shape.BoundingBox().xmax, 47.44 + 1e-6)
        bounds = self.base.fuse(self.lid).BoundingBox()
        for actual, expected in zip(
            (bounds.xlen, bounds.ylen, bounds.zlen), (50.19, 33.5, 19.7)
        ):
            self.assertAlmostEqual(actual, expected, places=5)

    def test_four_short_wide_arms(self):
        for right in (False, True):
            arm = clip(self.d, right)
            self.assertLessEqual(arm.BoundingBox().xlen, 15)
            section_x = 38 if right else 8
            section = arm.intersect(box(section_x, section_x + 0.1, -4, 4, 0, 10))
            self.assertAlmostEqual(section.BoundingBox().zlen, 4.8, places=5)
            self.assertAlmostEqual(section.BoundingBox().ylen, 1.2, places=5)
        self.assertLess(self.d.clip_strain(), 0.008)
        self.assertEqual(self.d.hook_engagement, 0.20)

    def assert_lengthwise_travel(self, base, d):
        pcb = rounded_box(0, d.board_x, 0, d.board_y, 0, d.board_thickness, 2)
        # End stops must still catch the PCB at maximum lid/board vertical take-up.
        for height in (0, d.board_vertical_play + 0.15):
            for direction in (-1, 1):
                free = pcb.translate((direction * (d.board_end_clearance - 0.01), 0, height))
                stopped = pcb.translate((direction * (d.board_end_clearance + 0.01), 0, height))
                self.assertLess(base.intersect(free).Volume(), 1e-5)
                self.assertGreater(base.intersect(stopped).Volume(), 1e-4)

    def test_lengthwise_board_play_is_point_two_mm(self):
        self.assertAlmostEqual(2 * self.d.board_end_clearance, 0.20)
        self.assert_lengthwise_travel(self.base, self.d)

    def test_usb_c_side_is_closed(self):
        former_opening = box(18.26, 31.26, self.d.board_y - self.d.side_inner,
                             self.d.ymax, 2.165, 9.365)
        closed = self.base.fuse(self.lid)
        self.assertLess(former_opening.cut(closed).Volume(), 1e-5)

    def test_four_hooks_engage_and_release(self):
        for right in (False, True):
            for north in (False, True):
                with self.subTest(right=right, north=north):
                    arm = clip(self.d, right)
                    if north:
                        arm = other_side(arm, self.d)
                    self.assertGreater(
                        self.base.intersect(arm.translate((0, 0, 0.3))).Volume(), 0.03
                    )
                    # Rigid inward offset screens receiver clearance, not elastic stress.
                    dy = -0.30 if north else 0.30
                    for height in (0, 0.3, 1, 2, 4, 6):
                        self.assertLess(
                            self.base.intersect(arm.translate((0, dy, height))).Volume(), 1e-5
                        )

    def test_overtravel_stops(self):
        for right in (False, True):
            arm = clip(self.d, right)
            beam = arm.intersect(box(5, 43.1, -4, 4, 2.2, 7.0))
            rest = self.lid.cut(arm)
            self.assertLess(rest.intersect(beam.translate((0, 0.3, 0))).Volume(), 1e-5)
            self.assertGreater(rest.intersect(beam.translate((0, 0.5, 0))).Volume(), 0.01)

    def test_rigid_alignment_and_insertion(self):
        rigid = self.lid
        for right in (False, True):
            arm = clip(self.d, right)
            rigid = rigid.cut(arm).cut(other_side(arm, self.d))
        for height in (0, 0.25, 0.5, 1, 2, 3, 4, 6, 14):
            self.assertLess(self.base.intersect(rigid.translate((0, 0, height))).Volume(), 1e-5)
        for dx, dy in ((0.2, 0), (-0.2, 0), (0, 0.2), (0, -0.2)):
            self.assertGreater(
                self.base.intersect(rigid.translate((dx, dy, 0))).Volume(), 0.01
            )

    def test_pad_size_preload_and_rigid_support(self):
        lower, upper = pads(self.d)
        self.assertAlmostEqual(lower.BoundingBox().zmax - (-4.815), 0.20)
        self.assertAlmostEqual(6.005 - upper.BoundingBox().zmin, 0.20)
        for pad in (lower, upper):
            self.assertAlmostEqual(pad.Volume(), 24)
            for case in (self.base, self.lid):
                self.assertLess(case.intersect(pad).Volume(), 1e-5)
        for pad in pads(self.d, compressed=True):
            self.assertAlmostEqual(pad.BoundingBox().zlen, 0.80)
        self.assertAlmostEqual(
            self.base.intersect(lower.translate((0, 0, -0.01))).Volume() / 0.01, 24, places=4
        )
        self.assertAlmostEqual(
            self.lid.intersect(upper.translate((0, 0, 0.01))).Volume() / 0.01, 24, places=4
        )
        self.assertGreater(2 * self.d.pad_compression - 0.15, 0)

    def test_board_lands_clear_solder_envelopes(self):
        keepouts = (
            (15.4, 18.9, 2.3, 25.5), (30.7, 34.1, 2.3, 25.5),
            (18.5, 31.1, 1.8, 5.6), (8.0, 14.6, 5.1, 22.9),
            (20.6, 24.7, 16.5, 22.2), (38.5, 43.0, 6.5, 21.5),
        )
        for x0, x1, y0, y1 in keepouts:
            self.assertLess(self.base.intersect(box(x0, x1, y0, y1, -3.2, 0)).Volume(), 1e-5)

    def test_print_orientation(self):
        for shape in print_shapes(self.base, self.lid, self.d):
            self.assertAlmostEqual(shape.BoundingBox().zmin, 0, places=5)
            self.assertEqual(len(shape.Solids()), 1)

    def test_adjustment_limits(self):
        for settings in (
            dict(fit_clearance=0.25, flange_clearance=0.15, key_clearance=0.12,
                 hook_engagement=0.15, pad_compression=0.10, board_end_clearance=0.05),
            dict(fit_clearance=0.55, flange_clearance=0.25, key_clearance=0.25,
                 hook_engagement=0.25, pad_compression=0.20, board_end_clearance=0.20),
            dict(module_lift=6),
        ):
            with self.subTest(settings=settings):
                d = replace(self.d, **settings)
                base, lid = build(d)
                validate(base, lid, d)
                self.assert_lengthwise_travel(base, d)

    def test_invalid_settings_fail(self):
        for setting in (
            {"fit_clearance": 0.1}, {"fit_clearance": 0.8},
            {"module_lift": -1}, {"module_lift": 7},
            {"flange_clearance": 0.3}, {"board_end_clearance": 0.25},
            {"board_end_clearance": 0}, {"board_end_clearance": float("nan")},
            {"hook_engagement": 0.3}, {"hook_engagement": 0.1},
            {"pad_compression": 0.3}, {"key_clearance": 0.05},
            {"fit_clearance": float("nan")},
        ):
            with self.subTest(setting=setting), self.assertRaises(ValueError):
                replace(self.d, **setting).validate()


if __name__ == "__main__":
    unittest.main()
