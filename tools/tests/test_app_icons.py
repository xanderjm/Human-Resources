"""Launcher producer integrity and transactional failure contracts."""
from pathlib import Path
import json
import shutil
import subprocess
import sys
import struct
import xml.etree.ElementTree as ET
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/generators"))
from cook_app_icons import verify_provenance


class LauncherContracts(unittest.TestCase):
    def test_regeneration_and_invalid_source_preserve_last_good_package(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "source"
            shutil.copytree(ROOT / "assets/graphics/app-icon", source)
            output = Path(directory) / "generated"
            command = [sys.executable, str(ROOT / "tools/generators/cook_app_icons.py"),
                       "--source-root", str(source), "--output-root", str(output)]
            subprocess.run(command, check=True)
            icon = output / "ios/Assets.xcassets/AppIcon.appiconset/AppIcon-1024.png"
            expected = icon.read_bytes()
            self.assertEqual(struct.unpack(">II", expected[16:24]), (1024, 1024))
            self.assertEqual(expected[25], 2)  # RGB, no iOS alpha channel.
            foreground = output / "android/res/drawable-xxxhdpi/ic_launcher_foreground.png"
            self.assertEqual(struct.unpack(">II", foreground.read_bytes()[16:24]), (432, 432))
            adaptive = ET.parse(output / "android/res/mipmap-anydpi/ic_launcher.xml").getroot()
            self.assertEqual([child.tag for child in adaptive], ["background", "foreground", "monochrome"])
            self.assertFalse(any((output / "android/res").glob("mipmap-*-v26")))
            self.assertFalse(any((output / "android/res").glob("mipmap-*/*.png")))
            self.assertTrue(all(child.get("{http://schemas.android.com/apk/res/android}drawable", "").startswith("@drawable/ic_launcher_") for child in adaptive))
            icon.unlink()
            subprocess.run(command, check=True)
            self.assertEqual(icon.read_bytes(), expected)
            document = json.loads((output / "android/assets/app-icon-provenance.json").read_text())
            verify_provenance(document, source)
            (source / "app-icon-1024.png").write_bytes(b"invalid PNG")
            with self.assertRaisesRegex(ValueError, "stale"):
                verify_provenance(document, source)
            self.assertNotEqual(subprocess.run(command, capture_output=True).returncode, 0)
            self.assertEqual(icon.read_bytes(), expected)
            self.assertEqual(document, json.loads((output / "ios/app-icon-provenance.json").read_text()))


if __name__ == "__main__":
    unittest.main()
