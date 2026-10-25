"""Exact native capture pixels must remain bounded and match the real layout."""
import argparse
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("playable_capture", ROOT / "tools/playable_qa/capture.py")
capture = importlib.util.module_from_spec(SPEC)
# Avoid binding Python's stdlib profile module when loading this standalone tool.
PROFILE_SPEC = importlib.util.spec_from_file_location("playable_profile", ROOT / "tools/playable_qa/profile.py")
profile = importlib.util.module_from_spec(PROFILE_SPEC)
PROFILE_SPEC.loader.exec_module(profile)
with patch.dict(sys.modules, {"profile": profile}):
    SPEC.loader.exec_module(capture)


class PlayableCaptureContracts(unittest.TestCase):
    def test_exact_pixels_and_legacy_expected_extent(self):
        for pixels, window in (("1080x1920", (360, 640)), ("1206x2622", (402, 874)), ("2064x2752", (516, 688))):
            extent = capture.capture_pixels(pixels)
            args = argparse.Namespace(capture_pixels=extent, window_width=window[0], window_height=window[1], render_width=588, render_height=1280)
            self.assertEqual(capture.output_extent(args), extent)
            args.capture_pixels = None
            self.assertEqual(capture.output_extent(args), (588, 1280))

    def test_malformed_excessive_and_distorted_output_rejected(self):
        for value in ("", "0x1", "1x0", "4097x1", "4096x2049", "1X2", "1x2x3", "-1x2", "+1x2", " 1x2", "1x2 ", "1x", "x1", "１x2"):
            with self.subTest(value=value), self.assertRaises(argparse.ArgumentTypeError):
                capture.capture_pixels(value)
        args = argparse.Namespace(capture_pixels=(1080, 1920), window_width=390, window_height=844)
        with self.assertRaises(ValueError):
            capture.output_extent(args)


if __name__ == "__main__":
    unittest.main()
