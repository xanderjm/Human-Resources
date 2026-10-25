#!/usr/bin/env python3
"""Bind packaged launcher provenance and native metadata to approved artwork."""
import argparse
import json
import re
from pathlib import Path
import subprocess
import sys
import zipfile
from cook_app_icons import ROOT, verify_provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--ios-app", type=Path)
    target.add_argument("--android-apk", type=Path)
    args = parser.parse_args()
    if args.ios_app:
        app = args.ios_app
        document = json.loads((app / "app-icon-provenance.json").read_text())
        plist = json.loads(subprocess.check_output(["plutil", "-convert", "json", "-o", "-", str(app / "Info.plist")]))
        for key in ("CFBundleIcons", "CFBundleIcons~ipad"):
            icon = plist.get(key, {}).get("CFBundlePrimaryIcon", {})
            if icon.get("CFBundleIconName") != "AppIcon" or not icon.get("CFBundleIconFiles"):
                raise ValueError(f"Missing compiled primary AppIcon metadata: {key}")
        if not (app / "Assets.car").is_file():
            raise ValueError("Compiled iOS asset catalogue is missing")
    else:
        sys.path.insert(0, str(ROOT / "tools"))
        from game_support.toolchains import android_sdk_root
        aapt = android_sdk_root() / "build-tools/36.0.0/aapt2"
        apk = str(args.android_apk)
        with zipfile.ZipFile(apk) as archive:
            document = json.loads(archive.read("assets/app-icon-provenance.json"))
        resources = subprocess.check_output([str(aapt), "dump", "resources", apk], text=True)
        if "mipmap/ic_launcher" not in resources or "drawable/ic_launcher_foreground" not in resources:
            raise ValueError("Compiled Android adaptive launcher resources are missing")
        if "drawable/ic_launcher_monochrome" not in resources:
            raise ValueError("Compiled Android themed launcher layer is missing")
        icon_id = re.search(r"resource (0x[0-9a-f]+) mipmap/ic_launcher\b", resources)
        manifest = subprocess.check_output([str(aapt), "dump", "xmltree", "--file", "AndroidManifest.xml", apk], text=True)
        for attribute in ("icon", "roundIcon"):
            value = re.search(r"android:" + attribute + r"\([^)]*\)=@(0x[0-9a-f]+)\b", manifest)
            if not icon_id or not value or value[1] != icon_id[1]:
                raise ValueError(f"Android {attribute} does not reference the compiled adaptive mipmap")
        badging = subprocess.check_output([str(aapt), "dump", "badging", apk], text=True)
        if "application-icon-" not in badging:
            raise ValueError("Android package does not declare a launcher icon")
    verify_provenance(document, ROOT / "assets/graphics/app-icon")
    print("Compiled native launcher metadata and approved source provenance passed.")


if __name__ == "__main__":
    main()
