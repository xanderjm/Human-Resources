"""Observable workflow and prerequisite contracts, runnable without an SDK."""

from pathlib import Path
import json
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/generators"))

from select_xcode import select_xcode
from verify_foundation import audit_android, audit_mobile_orientation, audit_presets, audit_workflows


class ToolingContracts(unittest.TestCase):
    def test_android_toolchain_pin_drift_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in (".gitignore", "android/app/src/main/AndroidManifest.xml",
                             "android/gradle/wrapper/gradle-wrapper.jar",
                             "android/gradle/wrapper/gradle-wrapper.properties"):
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / relative, target)
            errors = []
            audit_android(root, {"platforms": ["android"]}, errors)
            self.assertEqual(errors, [])
            properties = root / "android/gradle/wrapper/gradle-wrapper.properties"
            original = properties.read_text()
            for mutation in (original.replace("9.8.2-bin.zip", "9.6.1-bin.zip"),
                             original.replace("distributionSha256Sum=", "wrongChecksumKey="),
                             original.replace("services.gradle.org", "untrusted.example"),
                             original.replace("validateDistributionUrl=true", "validateDistributionUrl=false")):
                properties.write_text(mutation)
                errors = []
                audit_android(root, {"platforms": ["android"]}, errors)
                self.assertIn("Gradle distribution pin/checksum mismatch", errors)
            properties.write_text(original)
            jar = root / "android/gradle/wrapper/gradle-wrapper.jar"
            jar.write_bytes(jar.read_bytes() + b"tampered")
            errors = []
            audit_android(root, {"platforms": ["android"]}, errors)
            self.assertIn("Gradle wrapper checksum mismatch", errors)

    def test_mobile_orientation_drift_is_rejected(self):
        project = {"platforms": ["ios", "android"], "platform_settings": {"orientation": "portrait"}}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in ("ios/Info.plist.in", "ios/LaunchScreen.storyboard", "android/app/src/main/AndroidManifest.xml"):
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / relative, target)
            errors = []
            audit_mobile_orientation(root, project, errors)
            self.assertEqual(errors, [])
            for relative, before, after in (
                ("ios/Info.plist.in", "UIInterfaceOrientationPortrait", "UIInterfaceOrientationLandscapeLeft"),
                ("ios/LaunchScreen.storyboard", 'orientation="portrait"', 'orientation="landscape"'),
                ("android/app/src/main/AndroidManifest.xml", 'screenOrientation="portrait"', 'screenOrientation="sensorLandscape"'),
            ):
                path = root / relative
                original = path.read_text()
                path.write_text(original.replace(before, after))
                errors = []
                audit_mobile_orientation(root, project, errors)
                self.assertTrue(errors, relative)
                path.write_text(original)

    def test_workflow_audit_rejects_reported_ci_regressions(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            shutil.copytree(ROOT / ".github", root / ".github")
            workflow = root / ".github/workflows/foundation-quality.yml"
            original = workflow.read_text()
            for mutation in (
                original.replace("    steps:", "    env:\n      BROKEN: ${{ runner.temp }}\n    steps:"),
                original.replace("    steps:", "    steps:\n      - uses: actions/setup-java@0000000000000000000000000000000000000000 # v4.7.1"),
                original.replace("select_xcode.py", "unrelated.py"),
            ):
                workflow.write_text(mutation)
                errors = []
                audit_workflows(root, {"platforms": []}, errors)
                self.assertTrue(errors)

    def test_missing_release_preset_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            presets = json.loads((ROOT / "CMakePresets.json").read_text())
            presets["testPresets"] = [p for p in presets["testPresets"] if p["name"] != "release"]
            (root / "CMakePresets.json").write_text(json.dumps(presets))
            errors = []
            audit_presets(root, errors)
            self.assertTrue(any("release" in error for error in errors))

    def test_every_build_preset_can_be_tested(self):
        presets = json.loads((ROOT / "CMakePresets.json").read_text())
        configured = {item["name"] for item in presets["configurePresets"]}
        tests = {item["name"]: item for item in presets["testPresets"]}
        for build in presets["buildPresets"]:
            self.assertIn(build["name"], tests)
            self.assertIn(tests[build["name"]]["configurePreset"], configured)
            self.assertEqual(tests[build["name"]]["execution"]["noTestsAction"], "error")

    def test_release_test_preset_resolves_without_building(self):
        # Even --show-only can write a timestamped LastTest.log. Keep inspection
        # out of the generated repository so qualification remains deterministic.
        with tempfile.TemporaryDirectory(prefix="preset-inspection-") as directory:
            result = subprocess.run(
                ("ctest", "--preset", "release", "--test-dir", directory, "--show-only=json-v1"), cwd=ROOT,
                capture_output=True, text=True, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_xcode_selection_uses_probed_numeric_versions(self):
        versions = {Path("Xcode_26.9"): (26, 9), Path("Xcode_26.10"): (26, 10),
                    Path("Xcode_99_broken"): None, Path("Xcode_16"): (16, 4)}
        self.assertEqual(select_xcode(versions, 26, versions.get), Path("Xcode_26.10"))
        with self.assertRaises(RuntimeError):
            select_xcode([Path("Xcode_16")], 26, versions.get)


if __name__ == "__main__":
    unittest.main()
