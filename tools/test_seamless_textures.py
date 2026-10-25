#!/usr/bin/env python3
"""Focused authoring test. Run with the existing Pillow/NumPy Python runtime."""
import unittest

import numpy as np
from PIL import Image

from repair_asphalt_tile import DIR, quilt

from seamless_textures import correct_axis, integer_boundary_projection, periodic_component, statistics


class SeamlessTextureTests(unittest.TestCase):
    def export(self, pixels):
        periodic = periodic_component(pixels)
        return integer_boundary_projection(correct_axis(correct_axis(periodic, 1), 0))

    def assert_seamless(self, result):
        checks = statistics(result)
        for name in ("edge_value_x", "edge_value_y", "first_difference_x", "first_difference_y"):
            self.assertEqual(checks[name]["max_abs_8bit"], 0, name)

    def test_constant_preserves_every_pixel(self):
        source = np.full((256, 256, 3), [87, 137, 201], dtype=float)
        result = self.export(source)
        np.testing.assert_array_equal(result, source)
        self.assert_seamless(result)

    def test_nonperiodic_colour_ramp_joins_without_clipping(self):
        yy, xx = np.mgrid[:256, :256]
        source = np.stack((90 + xx / 8, 100 + yy / 10, 130 + (xx + yy) / 16), axis=-1)
        result = self.export(source)
        self.assert_seamless(result)
        self.assertGreater(result.min(), 0)
        self.assertLess(result.max(), 255)
        self.assertLess(np.std(result, axis=(0, 1)).max(), 1.0)

    def test_brush_frequency_noise_does_not_become_wide_border_ramp(self):
        random = np.random.default_rng(81)
        source = 128 + random.normal(0, 1, (256, 256, 3))
        result = self.export(source)
        self.assert_seamless(result)
        self.assertLess(np.abs(result.astype(float) - source).max(), 15)
        self.assertLess(np.std(result), 2.5)
    def test_asphalt_visual_line_regression_and_exact_repeat(self):
        # The old tile already passed endpoint checks but had a coherent dark
        # row across the whole repeat boundary. That observable image artifact
        # must fail independently of the chosen quilting implementation.
        before = np.asarray(Image.open(DIR / "pre-quilt-master-v2.png")).astype(float)
        current = np.asarray(Image.open(DIR / "texture-asphalt.png")).astype(float)
        old_band = abs(float(np.mean(before[1] - before[0])))
        new_band = abs(float(np.mean(current[1] - current[0])))
        self.assertGreater(old_band, 4)
        self.assertLess(new_band, .5)
        self.assertLess(new_band, old_band / 20)
        # The material must retain painted contrast, not hide the line by
        # flattening the complete tile to an average colour.
        self.assertGreater(float(np.std(current)), float(np.std(before)) * .9)
        self.assert_seamless(current)
        regenerated, _ = quilt(before)
        np.testing.assert_array_equal(regenerated, current)


if __name__ == "__main__":
    unittest.main()
