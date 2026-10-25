#!/usr/bin/env python3
"""Check this offline product's iOS privacy contract against the actual bundle.

This is a narrow product gate, not a complete Apple API scanner. New platform
services or dependencies require a source/use review and an updated contract.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import plistlib
import subprocess


REASONS = {
    "NSPrivacyAccessedAPICategorySystemBootTime": ["35F9.1"],
    "NSPrivacyAccessedAPICategoryFileTimestamp": ["C617.1"],
}


def validate_manifest(data: bytes) -> None:
    manifest = plistlib.loads(data)
    if manifest.get("NSPrivacyTracking") is not False:
        raise ValueError("Offline product must explicitly declare no tracking")
    for key in ("NSPrivacyTrackingDomains", "NSPrivacyCollectedDataTypes"):
        if manifest.get(key) != []:
            raise ValueError(f"Offline product requires an empty {key}")
    entries = manifest.get("NSPrivacyAccessedAPITypes", [])
    actual = {}
    for entry in entries:
        category = entry.get("NSPrivacyAccessedAPIType")
        if category in actual:
            raise ValueError("Duplicate required-reason API category")
        actual[category] = entry.get("NSPrivacyAccessedAPITypeReasons")
    if actual != REASONS:
        raise ValueError("Required-reason declarations disagree with reviewed timing/container use")


def audit_bundle(source: Path, bundle: Path, symbols: str) -> None:
    data = source.read_bytes()
    validate_manifest(data)
    packaged = bundle / "PrivacyInfo.xcprivacy"
    if packaged.read_bytes() != data:
        raise ValueError("Packaged privacy manifest is not byte-exact current source")
    imports = {line.split()[-1] for line in symbols.splitlines() if line.split()}
    # These imports are independently observed in the statically linked pinned
    # SDL/product executable. Merely shipping a plist is insufficient evidence.
    if "_mach_absolute_time" not in imports or not imports.intersection({"_stat", "_fstat"}):
        raise ValueError("Expected native timing/file imports changed; review actual API use")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ios-app", required=True, type=Path)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2] / "ios/PrivacyInfo.xcprivacy")
    args = parser.parse_args()
    info = plistlib.loads((args.ios_app / "Info.plist").read_bytes())
    executable = info["CFBundleExecutable"]
    if not isinstance(executable, str) or Path(executable).name != executable:
        raise ValueError("Invalid bundle executable name")
    symbols = subprocess.check_output(["xcrun", "nm", "-u", str(args.ios_app / executable)], text=True)
    audit_bundle(args.source, args.ios_app, symbols)
    print("iOS offline privacy manifest/source/native-import contract passed.")
    print("Public policy URL, store disclosures and store acceptance remain separate.")


if __name__ == "__main__":
    main()
