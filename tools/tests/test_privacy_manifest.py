import importlib.util
from pathlib import Path
import plistlib
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("privacy_audit", ROOT / "tools/generators/audit_privacy_manifest.py")
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class PrivacyManifestTests(unittest.TestCase):
    def setUp(self):
        self.data = (ROOT / "ios/PrivacyInfo.xcprivacy").read_bytes()

    def test_reviewed_source_rejects_missing_or_invented_reasons(self):
        audit.validate_manifest(self.data)
        for mutation in ([], [{"NSPrivacyAccessedAPIType": "NSPrivacyAccessedAPICategoryUserDefaults", "NSPrivacyAccessedAPITypeReasons": ["CA92.1"]}]):
            value = plistlib.loads(self.data)
            value["NSPrivacyAccessedAPITypes"] = mutation
            with self.assertRaises(ValueError):
                audit.validate_manifest(plistlib.dumps(value))

    def test_tracking_or_duplicate_categories_fail(self):
        for key, value in (("NSPrivacyTracking", True), ("NSPrivacyCollectedDataTypes", [{}]), ("NSPrivacyTrackingDomains", ["example.invalid"])):
            manifest = plistlib.loads(self.data)
            manifest[key] = value
            with self.assertRaises(ValueError):
                audit.validate_manifest(plistlib.dumps(manifest))
        manifest = plistlib.loads(self.data)
        manifest["NSPrivacyAccessedAPITypes"] *= 2
        with self.assertRaises(ValueError):
            audit.validate_manifest(plistlib.dumps(manifest))

    def test_stale_packaged_manifest_and_changed_native_use_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            bundle = Path(directory)
            packaged = bundle / "PrivacyInfo.xcprivacy"
            source = ROOT / "ios/PrivacyInfo.xcprivacy"
            symbols = "                 U _mach_absolute_time\n                 U _fstat\n"
            packaged.write_bytes(self.data)
            audit.audit_bundle(source, bundle, symbols)
            with self.assertRaises(ValueError):
                audit.audit_bundle(source, bundle, " U _mach_absolute_time\n")
            packaged.write_bytes(plistlib.dumps({"NSPrivacyAccessedAPITypes": []}))
            with self.assertRaises(ValueError):
                audit.audit_bundle(source, bundle, symbols)


if __name__ == "__main__":
    unittest.main()
