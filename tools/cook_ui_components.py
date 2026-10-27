"""Cook the product-owned Atomic Enamel art into a bounded secondary UI atlas.

The recipe is the crop/packing authority. Original generated sheets stay in
assets/graphics; only the cooked HRM texture and generated C++ regions ship.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
RECIPE = "assets/graphics/atomic-ui/v1/recipe.json"


def source_paths(root: Path = ROOT) -> list[str]:
    recipe = json.loads((root / RECIPE).read_text())
    return sorted({RECIPE, "tools/cook_ui_components.py",
                   "assets/fonts/human-resources-v3/ttf/HR-Abduction-Compact-Black-Italic.ttf",
                   "assets/fonts/human-resources-v3/ttf/HR-Beam-UI-SemiBold.ttf"} |
                  {recipe["source_root"] + "/" + name for name in recipe["sources"].values()} |
                  {"assets/fonts/human-resources-v3/ttf/" + row["file"]
                   for row in recipe["font_banks"]})


def checked_recipe(root: Path = ROOT) -> dict:
    recipe = json.loads((root / RECIPE).read_text())
    if recipe["schema"] != 1 or recipe["atlas_size"] != [2048, 2048]:
        raise ValueError("Unsupported Atomic Enamel atlas recipe")
    identifiers, occupied = set(), []
    for row in recipe["regions"]:
        name = row["id"]
        if not name.isidentifier() or name in identifiers:
            raise ValueError("UI sprite identifiers must be unique C++ identifiers")
        identifiers.add(name)
        x, y, width, height = row["slot"]
        if min(x, y) < 0 or min(width, height) <= 8 or x + width > 2048 or y + height > 2048:
            raise ValueError("UI sprite slot exceeds the atlas: " + name)
        occupied.append((name, x, y, width, height))
        if len(row["slice"]) != 4 or any(not 0 <= v < .5 for v in row["slice"]):
            raise ValueError("Invalid nine-slice fractions: " + name)
        left, top, right, bottom = row["slice"]
        for a, b, c, d in row.get("fixed_detail_bounds", []):
            if not (0 <= a < c <= 1 and 0 <= b < d <= 1):
                raise ValueError("Invalid fixed detail bounds: " + name)
            fixed_x = c <= left or a >= 1 - right
            fixed_y = (d <= top or b >= 1 - bottom) if top + bottom else True
            if not (fixed_x and fixed_y):
                raise ValueError("Slice crosses a protected decorative detail: " + name)
    for row in recipe["font_banks"]:
        x, y = row["position"]
        occupied.append((row["id"], x, y, 1024, 480))
    for i, (name, x, y, width, height) in enumerate(occupied):
        if min(x, y) < 0 or x + width > 2048 or y + height > 2048:
            raise ValueError("UI font bank exceeds the atlas: " + name)
        for other, a, b, w, h in occupied[i + 1:]:
            if x < a + w and a < x + width and y < b + h and b < y + height:
                raise ValueError("UI atlas slots overlap: " + name + " / " + other)
    return recipe


def sprite_crop(image: Image.Image, row: dict) -> Image.Image:
    if "grid" in row:
        column, line, columns, lines = row["grid"]
        box = (column * image.width // columns, line * image.height // lines,
               (column + 1) * image.width // columns, (line + 1) * image.height // lines)
    else:
        box = row["crop"] or (0, 0, image.width, image.height)
    if not (0 <= box[0] < box[2] <= image.width and 0 <= box[1] < box[3] <= image.height):
        raise ValueError("UI crop exceeds the original sheet: " + row["id"])
    crop = image.crop(box)
    threshold = row.get("alpha_trim_threshold", 1)
    bounds = crop.getchannel("A").point(lambda a: 255 if a >= threshold else 0).getbbox()
    if not bounds:
        raise ValueError("UI crop is completely transparent: " + row["id"])
    return crop.crop(bounds)


def upright_glyph(font: ImageFont.FreeTypeFont, character: str) -> tuple[Image.Image, float]:
    """Author an upright label bank from the owned logo companion outlines.

    The source builder applies a .31 shear. Remove that once offline, preserving
    its black outline weight, rather than stretching or emboldening live text.
    Fixed optical sidebearings and tabular digits belong to this bitmap role.
    """
    source = Image.new("RGBA", (128, 96))
    ImageDraw.Draw(source).text((40, 8), character, font=font, fill="white")
    baseline = 8 + font.getmetrics()[0]
    upright = source.transform(source.size, Image.Transform.AFFINE,
                               (1, -.31, .31 * baseline, 0, 1, 0), Image.Resampling.BICUBIC)
    bounds = upright.getchannel("A").getbbox()
    cell = Image.new("RGBA", (64, 80))
    if not bounds:
        return cell, float(font.getlength(character))
    ink = upright.crop(bounds)
    advance = ink.width + 3
    if character.isascii() and character.isdigit():
        # Source digits have equal advances. Preserve that property after the
        # outline transform so an unchanged digit count never moves a number.
        advance = 31
        if ink.width > advance - 3:
            raise ValueError("Upright digit exceeds its tabular advance: " + character)
    left = 4 + max(0, (advance - 3 - ink.width) // 2)
    if left + ink.width > 64 or bounds[1] + ink.height > 80:
        raise ValueError("Upright UI glyph exceeds its cell: " + repr(character))
    cell.alpha_composite(ink, (left, bounds[1]))
    return cell, float(advance)


def cook_ui_components(output: Path, header: Path, root: Path = ROOT) -> dict:
    from cook_style_preview import write_mesh

    recipe = checked_recipe(root)
    images = {}
    for name, filename in recipe["sources"].items():
        with Image.open(root / recipe["source_root"] / filename) as original:
            if original.mode != "RGBA" or original.width * original.height > 8_388_608:
                raise ValueError("UI source must be a bounded RGBA image: " + name)
            image = original.copy()
        if image.getchannel("A").getextrema() != (0, 255):
            raise ValueError("UI source requires real transparent and opaque pixels: " + name)
        images[name] = image
    atlas = Image.new("RGBA", tuple(recipe["atlas_size"]))
    regions = []
    for row in recipe["regions"]:
        crop = sprite_crop(images[row["source"]], row)
        x, y, width, height = row["slot"]
        ratio = min((width - 8) / crop.width, (height - 8) / crop.height)
        size = (max(1, round(crop.width * ratio)), max(1, round(crop.height * ratio)))
        # Premultiplied filtering prevents dark fringes on cream and mint edges.
        crop = crop.convert("RGBa").resize(size, Image.Resampling.LANCZOS).convert("RGBA")
        x += (width - crop.width) // 2
        y += (height - crop.height) // 2
        atlas.alpha_composite(crop, (x, y))
        regions.append((row["id"], x, y, crop.width, crop.height,
                        [round(row["slice"][i] * size[i % 2]) for i in range(4)],
                        max(1, round(row.get("corner_radius", .1) * crop.width))))
    metrics = []
    cap_tops = []
    tactical_tops = []
    tactical_heights = []
    draw = ImageDraw.Draw(atlas)
    for row in recipe["font_banks"]:
        font = ImageFont.truetype(str(root / "assets/fonts/human-resources-v3/ttf" / row["file"]), 48)
        cap_tops.append(font.getbbox("H")[1] / 48)
        bounds = font.getbbox("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/%:<>s")
        tactical_tops.append(bounds[1] / 48 - cap_tops[-1])
        tactical_heights.append((bounds[3] - bounds[1]) / 48)
        for code in range(32, 127):
            index = code - 32
            x = row["position"][0] + index % 16 * 64
            y = row["position"][1] + index // 16 * 80
            if font.getbbox(chr(code))[2] + 4 > 64:
                raise ValueError("UI glyph exceeds its atlas cell")
            if row.get("upright_logo_companion", False):
                glyph, advance = upright_glyph(font, chr(code))
                atlas.alpha_composite(glyph, (x, y))
                metrics.append(advance)
            else:
                draw.text((x + 4, y + 8), chr(code), font=font, fill="white")
                metrics.append(float(font.getlength(chr(code))))
    # Match the already-cooked legacy Compact/SemiBold banks without changing
    # their UVs. Components position visible capitals at the requested top;
    # transparent font bearings are not usable padding inside a control.
    for filename in ("HR-Abduction-Compact-Black-Italic.ttf", "HR-Beam-UI-SemiBold.ttf"):
        font = ImageFont.truetype(str(root / "assets/fonts/human-resources-v3/ttf" / filename), 48)
        cap_tops.append(font.getbbox("H")[1] / 48)
        bounds = font.getbbox("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/%:<>s")
        tactical_tops.append(bounds[1] / 48 - cap_tops[-1])
        tactical_heights.append((bounds[3] - bounds[1]) / 48)
    write_mesh(output / "ui-components.hrm", [[0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0]], [0, 0, 0], atlas)
    # The preview is a developer artifact outside the cooked shipping directory.
    preview = header.parent / "ui-components.preview.png"
    atlas.save(preview)
    entries = []
    for name, x, y, width, height, insets, radius in regions:
        entries.append("{" + ",".join(map(str, [x, y, width, height])) + ",{" + ",".join(map(str, insets)) + "},"+str(radius)+"}")
    header.write_text(
        "#pragma once\n#include <array>\nnamespace human_resources::product::enamel {\n"
        "enum class Sprite : unsigned {" + ",".join(row[0] for row in regions) + ", count};\n"
        "struct Region { unsigned x,y,width,height; std::array<unsigned,4> insets; unsigned corner_radius; };\n"
        "inline constexpr unsigned atlas_width=2048, atlas_height=2048;\n"
        "inline constexpr std::array<Region," + str(len(entries)) + "> regions{{" + ",".join(entries) + "}};\n"
        "inline constexpr std::array<float,190> glyph_advances{" + ",".join(f"{v:.5f}F" for v in metrics) + "};\n"
        "inline constexpr std::array<unsigned,2> font_bank_y{1088,1568};\n"
        "inline constexpr std::array<float,4> font_cap_top{" + ",".join(f"{v:.5f}F" for v in cap_tops) + "};\n"
        "inline constexpr std::array<float,4> font_tactical_top{" + ",".join(f"{v:.5f}F" for v in tactical_tops) + "};\n"
        "inline constexpr std::array<float,4> font_tactical_height{" + ",".join(f"{v:.5f}F" for v in tactical_heights) + "};\n}\n")
    return {"schema": 1, "recipe": RECIPE, "sprite_count": len(regions),
            "atlas_size": recipe["atlas_size"], "sources": source_paths(root),
            "source_sha256": {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                              for name in source_paths(root)}}
