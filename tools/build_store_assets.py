#!/usr/bin/env python3
"""Manually export nonshipping Play listing derivatives from approved brand art.

Run with the pinned offline art Python, or let this entry point discover it.
No model render, new artwork, native build or publication is performed.
"""
from __future__ import annotations

import argparse
from hashlib import sha256
from io import BytesIO
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET
import zlib

from art_tools import ROOT, art_python, python_versions

SOURCES = {
    "assets/graphics/app-icon/app-icon-1024.png": "06850486191066507b20350ff1f2c450c3ff8c2874e3291cb5dd63388dc49396",
    "assets/graphics/app-icon/ufo-foreground-1024.png": "1e99a6bdcf8087bc91f71b2585991e2e4c2edf091e2701d2b73ff13a2edf8aaa",
    "assets/graphics/app-icon/background-1024.png": "422f410d35914cbf3634f56379f402427c70d436a44d4365a5d7a5078cbe2670",
    "assets/fonts/human-resources-v3/wordmarks/Human-Resources-Brand.svg": "6b46b638f2fa74ac85f7079cc21c7e19b6de370311e80b1cbfbdb951c15dbbc6",
    "assets/fonts/human-resources-v3/ttf/HR-Beam-UI-Medium.ttf": "917a4a668caff393fa16c259df9dc554d542377aab3ab843408a0a9e63f12461",
}
ICON, UFO, BACKGROUND, WORDMARK, FONT = SOURCES
RECIPE = {
    "schema": 1,
    "palette": {"navy": "#072A46", "cream": "#FFF1D1", "mint": "#65E8C6", "coral": "#EF7157", "ivory": "#F2EDDE"},
    "feature_size": [1024, 500],
    "supersampling": 4,
    "navy_panel_polygon": [[0, 0], [680, 0], [560, 500], [0, 500]],
    "wordmark_box": [38, 116, 540],
    "ufo_box": [612, 91, 384],
    "tagline": "Every abduction counts",
    "tagline_top_left": [66, 373],
    "tagline_size": 30,
    "prominent_content_safe_inset": 24,
    "icon_resampling": "Lanczos, direct 1024 to 512, no masking or shadows",
    "feature_background": "Existing opaque ivory background, composited with solid approved navy panel",
    "wordmark_rendering": "Existing outlined SVG geometry, original colours/offsets; 32 cubic steps; nonzero contours",
    "ufo_rendering": "Crop transparent master to alpha bounds, uniform resize, preserve RGB and alpha",
    "png": "sRGB perceptual intent, compression level 9, no timestamps",
    "scope": "Local listing derivatives only; excluded from the runtime build and package allowlist",
    "official_sources_checked": "2026-10-25",
    "official_sources": [
        "https://support.google.com/googleplay/android-developer/answer/9866151?hl=en",
        "https://developer.android.com/distribute/google-play/resources/icon-design-specifications?hl=en",
    ],
}


def digest(data: bytes) -> str:
    return sha256(data).hexdigest()


def canonical(value: object) -> bytes:
    return (json.dumps(value, sort_keys=True, indent=2) + "\n").encode("utf-8")


def source_records() -> list[dict]:
    paths = list(SOURCES) + [
        "assets/graphics/app-icon/review.json",
        "assets/graphics/app-icon/recipe.json",
        "assets/fonts/human-resources-v3/LICENSE.txt",
        "tools/art_tools.py",
        "tools/art-toolchain.json",
        "tools/build_store_assets.py",
    ]
    records = []
    for relative in paths:
        data = (ROOT / relative).read_bytes()
        actual = digest(data)
        if relative in SOURCES and actual != SOURCES[relative]:
            raise ValueError(f"Reviewed brand source changed: {relative}; obtain visual review before rebinding")
        records.append({"path": relative, "bytes": len(data), "sha256": actual})
    review = json.loads((ROOT / "assets/graphics/app-icon/review.json").read_text())
    recipe = (ROOT / "assets/graphics/app-icon/recipe.json").read_bytes()
    if review["status"] != "approved_candidate" or review["recipe_sha256"] != digest(recipe):
        raise ValueError("App icon review does not bind the current approved recipe")
    return records


def svg_contours(path_data: str) -> list[list[tuple[float, float]]]:
    """Read only the absolute contour vocabulary used by the reviewed wordmark.

    This deliberately rejects unsupported SVG commands rather than silently
    approximating a replacement logo or adding a general SVG dependency.
    """
    tokens = re.findall(r"[A-Za-z]|[-+]?(?:\d*\.\d+|\d+\.?\d*)(?:[eE][-+]?\d+)?", path_data)
    contours, current = [], []
    index, command, point = 0, "", (0.0, 0.0)
    arity = {"M": 2, "L": 2, "H": 1, "V": 1, "C": 6}
    while index < len(tokens):
        if tokens[index].isalpha():
            command = tokens[index]
            index += 1
            if command == "Z":
                if len(current) < 3:
                    raise ValueError("Invalid closed wordmark contour")
                contours.append(current)
                current = []
                continue
        if command not in arity:
            raise ValueError(f"Unsupported wordmark SVG command: {command}")
        count = arity[command]
        values = list(map(float, tokens[index:index + count]))
        if len(values) != count:
            raise ValueError("Incomplete wordmark SVG command")
        index += count
        if command == "M":
            if current:
                raise ValueError("Wordmark contains an unclosed contour")
            point = (values[0], values[1])
            current.append(point)
            command = "L"
        elif command in ("L", "H", "V"):
            point = ((values[0], values[1]) if command == "L" else
                     (values[0], point[1]) if command == "H" else (point[0], values[0]))
            current.append(point)
        else:
            start, first, second, end = point, values[:2], values[2:4], values[4:]
            for step in range(1, 33):
                t = step / 32
                current.append(tuple((1 - t) ** 3 * start[axis] + 3 * (1 - t) ** 2 * t * first[axis] +
                                     3 * (1 - t) * t * t * second[axis] + t ** 3 * end[axis] for axis in range(2)))
            point = (end[0], end[1])
    if current:
        raise ValueError("Wordmark contains a final unclosed contour")
    return contours


def contour_area(points: list[tuple[float, float]]) -> float:
    return sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(points, points[1:] + points[:1])) / 2


def wordmark_image(width: int):
    from PIL import Image, ImageChops, ImageDraw

    document = ET.parse(ROOT / WORDMARK).getroot()
    namespace = {"s": "http://www.w3.org/2000/svg"}
    if document.get("viewBox") != "0 0 6424 2490":
        raise ValueError("Unexpected reviewed wordmark viewBox")
    scale = width / 6424
    image = Image.new("RGBA", (width, round(2490 * scale)))
    ink = document.find("s:defs/s:g", namespace)
    if ink is None or ink.get("id") != "ink":
        raise ValueError("Reviewed wordmark ink is missing")

    def draw_path(data: str, fill: str, offset=(0.0, 0.0)):
        contours = sorted(svg_contours(data), key=lambda contour: abs(contour_area(contour)), reverse=True)
        # Font contours use opposite winding for counters. Each SVG path owns
        # its own mask, so a letter's counter cannot cut a neighbouring glyph.
        outer_sign = contour_area(contours[0]) > 0
        mask = Image.new("L", image.size)
        draw = ImageDraw.Draw(mask)
        for contour in contours:
            draw.polygon([((x + offset[0]) * scale, (y + offset[1]) * scale) for x, y in contour],
                         fill=255 if (contour_area(contour) > 0) == outer_sign else 0)
        layer = Image.new("RGBA", image.size, fill)
        layer.putalpha(ImageChops.multiply(layer.getchannel("A"), mask))
        image.alpha_composite(layer)

    for element in document:
        tag = element.tag.rsplit("}", 1)[-1]
        if tag in ("title", "defs"):
            continue
        if tag == "path":
            draw_path(element.attrib["d"], element.attrib["fill"])
        elif tag == "use" and element.get("href") == "#ink":
            transform = element.get("transform", "translate(0 0)")
            match = re.fullmatch(r"translate\(([-\d.]+) ([-\d.]+)\)", transform)
            if match is None:
                raise ValueError("Unsupported reviewed wordmark transform")
            offset = tuple(map(float, match.groups()))
            for path in ink:
                draw_path(path.attrib["d"], element.attrib["fill"], offset)
        else:
            raise ValueError(f"Unexpected reviewed wordmark element: {tag}")
    return image


def render_feature():
    from PIL import Image, ImageDraw, ImageFont

    scale = RECIPE["supersampling"]
    size = tuple(dimension * scale for dimension in RECIPE["feature_size"])
    with Image.open(ROOT / BACKGROUND) as background:
        if background.mode != "RGB" or background.getextrema() != ((242, 242), (237, 237), (222, 222)):
            raise ValueError("Existing icon background is no longer the approved solid ivory")
        image = background.resize(size, Image.Resampling.LANCZOS).convert("RGBA")
    draw = ImageDraw.Draw(image)
    draw.polygon([(x * scale, y * scale) for x, y in RECIPE["navy_panel_polygon"]], fill=RECIPE["palette"]["navy"])
    x, y, width = RECIPE["wordmark_box"]
    image.alpha_composite(wordmark_image(width * scale), (x * scale, y * scale))
    with Image.open(ROOT / UFO) as foreground:
        if foreground.mode != "RGBA":
            raise ValueError("Approved UFO layer requires alpha")
        subject = foreground.crop(foreground.getchannel("A").getbbox())
        x, y, width = RECIPE["ufo_box"]
        subject = subject.resize((width * scale, round(subject.height * width * scale / subject.width)), Image.Resampling.LANCZOS)
        image.alpha_composite(subject, (x * scale, y * scale))
    font = ImageFont.truetype(str(ROOT / FONT), RECIPE["tagline_size"] * scale)
    draw.text(tuple(coordinate * scale for coordinate in RECIPE["tagline_top_left"]), RECIPE["tagline"],
              font=font, anchor="lt", fill=RECIPE["palette"]["mint"])
    return image.convert("RGB").resize(tuple(RECIPE["feature_size"]), Image.Resampling.LANCZOS)


def png_bytes(image) -> bytes:
    from PIL import PngImagePlugin

    metadata = PngImagePlugin.PngInfo()
    metadata.add(b"sRGB", b"\x00")
    stream = BytesIO()
    image.save(stream, format="PNG", pnginfo=metadata, compress_level=9)
    return stream.getvalue()


def layout_records() -> dict:
    from PIL import Image, ImageDraw, ImageFont

    scale = RECIPE["supersampling"]
    x, y, width = RECIPE["wordmark_box"]
    local = wordmark_image(width * scale).getchannel("A").getbbox()
    wordmark = [x + local[0] / scale, y + local[1] / scale, x + local[2] / scale, y + local[3] / scale]
    with Image.open(ROOT / UFO) as source:
        bounds = source.getchannel("A").getbbox()
        x, y, width = RECIPE["ufo_box"]
        ufo = [x, y, x + width, y + (bounds[3] - bounds[1]) * width / (bounds[2] - bounds[0])]
    font = ImageFont.truetype(str(ROOT / FONT), RECIPE["tagline_size"] * scale)
    draw = ImageDraw.Draw(Image.new("L", (1, 1)))
    tagline = [coordinate / scale for coordinate in draw.textbbox(
        tuple(coordinate * scale for coordinate in RECIPE["tagline_top_left"]),
        RECIPE["tagline"], font=font, anchor="lt")]
    records = {"wordmark_bounds": wordmark, "ufo_bounds": ufo, "tagline_bounds": tagline}
    inset = RECIPE["prominent_content_safe_inset"]
    canvas_width, canvas_height = RECIPE["feature_size"]
    for name, box in records.items():
        if not (inset <= box[0] < box[2] <= canvas_width - inset and
                inset <= box[1] < box[3] <= canvas_height - inset):
            raise ValueError(f"Prominent artwork is clipped or too near the export edge: {name}")
    records["all_prominent_elements_inside_24px_inset"] = True
    return records


def render_outputs() -> dict:
    from PIL import Image

    with Image.open(ROOT / ICON) as source:
        if source.mode != "RGB" or source.size != (1024, 1024):
            raise ValueError("Approved icon master must remain opaque RGB at 1024 square")
        icon = source.resize((512, 512), Image.Resampling.LANCZOS)
    feature = render_feature()
    return {
        "google-play-icon-512-rgb.png": (icon, "Opaque RGB review derivative; use the RGBA counterpart for Play upload"),
        "google-play-icon-512.png": (icon.convert("RGBA"), "32-bit Play upload candidate, all alpha exactly 255"),
        "google-play-feature-1024x500.png": (feature, "24-bit Play feature graphic upload candidate"),
        "review-feature-512x250.png": (feature.resize((512, 250), Image.Resampling.LANCZOS), "Half-size readability proof; not a store upload"),
    }


def validate_png(data: bytes, expected, role: str) -> dict:
    from PIL import Image, ImageChops

    with Image.open(BytesIO(data)) as image:
        image.load()
        if image.mode != expected.mode or image.size != expected.size or image.info.get("srgb") != 0:
            raise ValueError("PNG format, dimensions or sRGB marker changed")
        if image.mode == "RGBA" and image.getchannel("A").getextrema() != (255, 255):
            raise ValueError("Play icon acquired transparency")
        if image.size == (512, 512) and any(image.convert("RGB").getpixel(point) != (242, 237, 222)
                                                for point in ((0, 0), (511, 0), (0, 511), (511, 511))):
            raise ValueError("Play icon corners must retain the approved full-bleed ivory")
        difference = ImageChops.difference(image.convert("RGB"), Image.new("RGB", image.size, RECIPE["palette"]["ivory"]))
        bounds = difference.getbbox()
        if bounds is None or len(data) > 1024 * 1024:
            raise ValueError("Empty or unexpectedly oversized listing derivative")
        bit_depth, colour_type = struct.unpack("BB", data[24:26])
        if bit_depth != 8 or colour_type != (6 if image.mode == "RGBA" else 2):
            raise ValueError("PNG must be true-colour 24-bit RGB or 32-bit RGBA")
        return {"role": role, "size": list(image.size), "mode": image.mode, "png_colour_type": colour_type,
                "bit_depth": bit_depth, "srgb_intent": image.info["srgb"], "bytes": len(data), "sha256": digest(data),
                "non_ivory_pixel_bounds": list(bounds), "channel_extrema": image.getextrema(),
                "decoded_pixel_sha256": digest(image.tobytes())}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build/store-listing/assets")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    selected_python = art_python()
    if Path(sys.executable).absolute() != selected_python:
        return subprocess.call([str(selected_python), str(Path(__file__).resolve()), *sys.argv[1:]])
    from PIL import __version__ as pillow_version, features

    output = args.output_dir.resolve()
    if not output.is_relative_to((ROOT / "build/store-listing").resolve()):
        raise ValueError("Store derivatives belong only under ignored build/store-listing")
    records = source_records()
    first, second = render_outputs(), render_outputs()
    images, metadata = {}, {}
    for name, (image, role) in first.items():
        data = png_bytes(image)
        if data != png_bytes(second[name][0]):
            raise ValueError(f"Nondeterministic two-render PNG: {name}")
        images[name], metadata[name] = data, validate_png(data, image, role)
    if first["google-play-icon-512-rgb.png"][0].tobytes() != first["google-play-icon-512.png"][0].convert("RGB").tobytes():
        raise ValueError("RGB and RGBA icon colours differ")
    if source_records() != records:
        raise ValueError("Source changed during export")
    manifest = canonical({"schema": 1, "status": "pending_root_visual_acceptance", "publication": "not_performed",
                          "recipe": RECIPE, "recipe_sha256": digest(canonical(RECIPE)), "sources": records,
                          "source_bytes": sum(record["bytes"] for record in records), "outputs": metadata,
                          "layout": layout_records(),
                          "output_png_bytes": sum(len(data) for data in images.values()),
                          "verification": {"two_renders_byte_identical": True, "source_stable": True,
                                           "opaque": True, "exact_size_colour_mode_and_srgb": True,
                                           "rgb_rgba_icon_colours_identical": True},
                          "tools": {"python": python_versions(selected_python), "pillow": pillow_version,
                                    "freetype": features.version_module("freetype2"),
                                    "zlib_build": zlib.ZLIB_VERSION, "zlib_runtime": zlib.ZLIB_RUNTIME_VERSION}})
    images["manifest.json"] = manifest
    if args.verify_only:
        for name, data in images.items():
            if (output / name).read_bytes() != data:
                raise ValueError(f"Store output is missing or stale: {name}")
    else:
        output.mkdir(parents=True, exist_ok=True)
        for name, data in images.items():
            (output / name).write_bytes(data)
    print(f"{'Verified' if args.verify_only else 'Exported'} 4 PNGs; deterministic, opaque sRGB; manifest {digest(manifest)}")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
