#!/usr/bin/env python3
"""Package approved PNG masters; never render models or rewrite source artwork."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from art_tools import art_python
from stage_runtime_assets import _inventory, mirror_tree

ROOT = Path(__file__).resolve().parents[2]
SOURCES = ("app-icon-1024.png", "adaptive-foreground-432.png",
           "adaptive-monochrome-432.png", "recipe.json")
DENSITIES = {"mdpi": 108, "hdpi": 162, "xhdpi": 216, "xxhdpi": 324, "xxxhdpi": 432}


def provenance(source: Path) -> dict:
    return {"schema": 1, "producer_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "sources": {name: hashlib.sha256((source / name).read_bytes()).hexdigest()
                        for name in SOURCES}}


def verify_provenance(document: dict, source: Path) -> None:
    if document != provenance(source):
        raise ValueError("Packaged launcher artwork provenance is stale")


def cook(source: Path, output: Path) -> None:
    from PIL import Image, PngImagePlugin
    recipe = json.loads((source / "recipe.json").read_text())
    if recipe.get("version") != 1 or not re.fullmatch(r"#[0-9a-fA-F]{6}", recipe.get("background_srgb", "")):
        raise ValueError("Unsupported launcher recipe/background")
    images = []
    for name, size, mode in ((SOURCES[0], 1024, "RGB"), (SOURCES[1], 432, "RGBA"),
                             (SOURCES[2], 432, "RGBA")):
        with Image.open(source / name) as image:
            image.load()
            if image.size != (size, size) or image.mode != mode or image.info.get("srgb") != 0:
                raise ValueError(f"Invalid sRGB launcher master: {name}")
            images.append(image.copy())
    icon, foreground, mono = images
    for image in (foreground, mono):
        alpha = image.getchannel("A")
        if alpha.getextrema() != (0, 255) or any(alpha.getpixel(xy) for xy in
            ((x, y) for x in range(432) for y in (0, 431))) or any(alpha.getpixel(xy) for xy in
            ((x, y) for y in range(432) for x in (0, 431))):
            raise ValueError("Adaptive launcher layer touches its canvas edge")
    document = provenance(source)
    output = output.absolute()
    if source.resolve() == output.resolve() or source.resolve() in output.resolve().parents or output.resolve() in source.resolve().parents:
        raise ValueError("Launcher output must not be inside approved artwork")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".launcher-", dir=output.parent) as directory:
        stage = Path(directory) / "next"
        def write(relative, value):
            path = stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(value)
        def png(relative, image):
            path = stage / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            info = PngImagePlugin.PngInfo()
            info.add(b"sRGB", b"\x00")
            image.save(path, pnginfo=info)
        def js(value):
            return json.dumps(value, indent=2, sort_keys=True) + "\n"
        catalogue = "ios/Assets.xcassets"
        write(f"{catalogue}/Contents.json", js({"info": {"author": "xcode", "version": 1}}))
        write(f"{catalogue}/AppIcon.appiconset/Contents.json", js({
            "images": [{"filename": "AppIcon-1024.png", "idiom": "universal", "platform": "ios", "size": "1024x1024"}],
            "info": {"author": "xcode", "version": 1}}))
        png(f"{catalogue}/AppIcon.appiconset/AppIcon-1024.png", icon)
        write("ios/app-icon-provenance.json", js(document))
        write("android/assets/app-icon-provenance.json", js(document))
        for density, size in DENSITIES.items():
            for name, image in (("foreground", foreground), ("monochrome", mono)):
                png(f"android/res/drawable-{density}/ic_launcher_{name}.png",
                    image.resize((size, size), Image.Resampling.LANCZOS))
        write("android/res/drawable/ic_launcher_background.xml",
              '<?xml version="1.0" encoding="utf-8"?>\n<shape xmlns:android="http://schemas.android.com/apk/res/android" android:shape="rectangle"><solid android:color="' + recipe["background_srgb"] + '" /></shape>\n')
        # Every supported device is API26+, so one adaptive resource is the
        # authority. Modern Android consumes monochrome; older parsers skip it.
        layers = ["background", "foreground", "monochrome"]
        children = "\n".join(f'  <{name} android:drawable="@drawable/ic_launcher_{name}" />' for name in layers)
        write("android/res/mipmap-anydpi/ic_launcher.xml",
              '<?xml version="1.0" encoding="utf-8"?>\n<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n' + children + '\n</adaptive-icon>\n')
        if not output.is_dir() or _inventory(stage) != _inventory(output):
            mirror_tree(stage, output)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT / "assets/graphics/app-icon")
    parser.add_argument("--output-root", type=Path, required=True)
    arguments = parser.parse_args()
    interpreter = art_python()
    if interpreter.absolute() != Path(sys.executable).absolute():
        return subprocess.run([str(interpreter), str(Path(__file__).resolve()), *sys.argv[1:]], check=False).returncode
    cook(arguments.source_root, arguments.output_root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
