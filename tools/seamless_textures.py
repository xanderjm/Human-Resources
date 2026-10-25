#!/usr/bin/env python3
"""Export generated painterly albedo to measured C1-continuous repeat tiles.

Authoring-only tool. Uses the existing bundled Pillow/NumPy runtime; neither
package nor this script belongs in the native runtime or shipped resources.
The generated source is immutable. Audits distinguish numerical checks from
the later human/agent visual assessment of the tiled preview.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MATERIALS = ("grass", "dirt", "soil", "asphalt", "pavement", "sand", "wood")
METHOD = "Periodic-plus-smooth Poisson decomposition, compact Hermite C1 boundary correction, integer boundary projection"


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def periodic_component(pixels: np.ndarray) -> np.ndarray:
    """Remove the low-frequency field caused by the original edge mismatch.

    The discrete periodic Poisson solve distributes correction over the image;
    it does not paste, mirror, or blur an obvious cross through the middle.
    """
    height, width, _ = pixels.shape
    boundary = np.zeros_like(pixels)
    boundary[0] += pixels[-1] - pixels[0]
    boundary[-1] += pixels[0] - pixels[-1]
    boundary[:, 0] += pixels[:, -1] - pixels[:, 0]
    boundary[:, -1] += pixels[:, 0] - pixels[:, -1]
    yy = 2 * np.pi * np.arange(height) / height
    xx = 2 * np.pi * np.arange(width) / width
    denominator = 2 * np.cos(yy[:, None]) + 2 * np.cos(xx[None, :]) - 4
    denominator[0, 0] = 1
    smooth_spectrum = np.fft.fft2(boundary, axes=(0, 1)) / denominator[..., None]
    smooth_spectrum[0, 0] = 0
    smooth = np.fft.ifft2(smooth_spectrum, axes=(0, 1)).real
    return pixels - smooth


def correct_axis(pixels: np.ndarray, axis: int, support: int = 96) -> np.ndarray:
    """Match endpoint values and one-sided slopes with compact cubic fields."""
    work = np.moveaxis(pixels.copy(), axis, 0)
    target = (work[0] + work[-1]) * 0.5
    slope = ((work[1] - work[0]) + (work[-1] - work[-2])) * 0.5
    t = np.linspace(0, 1, support + 1)[:, None, None]
    value_basis = 2 * t**3 - 3 * t**2 + 1
    left_value, right_value = target - work[0], target - work[-1]
    left_slope = slope - (work[1] - work[0])
    right_slope = -slope - (work[-2] - work[-1])
    work[:support + 1] += value_basis * left_value
    work[-support - 1:] += (value_basis * right_value)[::-1]
    # A noisy one-pixel brush gradient must not become a broad 96-pixel ramp.
    # The slope field therefore has its own short, smooth compact support.
    slope_support = 6
    st = np.linspace(0, 1, slope_support + 1)[:, None, None]
    slope_basis = st**3 - 2 * st**2 + st
    work[:slope_support + 1] += slope_support * slope_basis * left_slope
    work[-slope_support - 1:] += (slope_support * slope_basis * right_slope)[::-1]
    return np.moveaxis(work, 0, axis)


def integer_boundary_projection(pixels: np.ndarray) -> np.ndarray:
    """Make quantized exported RGB values and first differences exactly match.

    Only the outer two texels change after the smooth correction. Corner
    intersections are projected together so fixing Y cannot invalidate X.
    """
    work = np.rint(np.clip(pixels, 0, 255)).astype(np.int16)
    for axis in (1, 0):
        a = np.moveaxis(work, axis, 0)
        edge = np.rint((a[0] + a[-1]) / 2).astype(np.int16)
        slope = np.rint(((a[1] - a[0]) + (a[-1] - a[-2])) / 2).astype(np.int16)
        slope = np.clip(slope, -np.minimum(edge, 255 - edge), np.minimum(edge, 255 - edge))
        a[0] = a[-1] = edge
        a[1], a[-2] = edge + slope, edge - slope
    corner = np.rint(np.mean(work[np.ix_([0, -1], [0, -1])], axis=(0, 1))).astype(np.int16)
    for row in (0, 1, -2, -1):
        for column in (0, 1, -2, -1):
            work[row, column] = corner
    return work.astype(np.uint8)


def statistics(pixels: np.ndarray) -> dict:
    a = pixels.astype(np.float64)
    value_x, value_y = a[:, 0] - a[:, -1], a[0] - a[-1]
    slope_x = (a[:, 1] - a[:, 0]) - (a[:, -1] - a[:, -2])
    slope_y = (a[1] - a[0]) - (a[-1] - a[-2])
    def measure(value: np.ndarray) -> dict:
        return {"max_abs_8bit": float(np.max(np.abs(value))), "rms_8bit": float(np.sqrt(np.mean(value**2)))}
    return {"edge_value_x": measure(value_x), "edge_value_y": measure(value_y),
            "first_difference_x": measure(slope_x), "first_difference_y": measure(slope_y),
            "interior_adjacent_rms_8bit": float(np.sqrt(np.mean(np.diff(a[2:-2, 2:-2], axis=1)**2)))}


def make_preview(tile: Image.Image, output: Path, tile_size: int) -> None:
    miniature = tile.resize((tile_size, tile_size), Image.Resampling.LANCZOS)
    preview = Image.new("RGB", (tile_size * 3, tile_size * 3))
    for row in range(3):
        for col in range(3):
            preview.paste(miniature, (col * tile_size, row * tile_size))
    preview.save(output)


def export_material(material: str) -> dict:
    asset_id = f"texture-{material}"
    record = ROOT / "assets/production/records" / asset_id
    metadata = json.loads((record / "tile-v1-generated.json").read_text())
    source = Path(metadata["source"])
    directory = ROOT / "assets/textures" / asset_id
    directory.mkdir(parents=True, exist_ok=True)
    raw = directory / "raw-v1.png"
    if raw.exists() and digest(raw) != digest(source):
        raise ValueError(f"Refusing to overwrite different preserved raw source: {raw}")
    shutil.copy2(source, raw)
    original = Image.open(raw).convert("RGB").resize((1024, 1024), Image.Resampling.LANCZOS)
    source_pixels = np.asarray(original, dtype=np.float64)
    corrected = periodic_component(source_pixels)
    corrected = correct_axis(correct_axis(corrected, 1), 0)
    final_pixels = integer_boundary_projection(corrected)
    quilt_provenance = None
    if material == "asphalt":
        # Asphalt's broad diagonal strokes showed a horizontal termination even
        # after exact edge matching. Quilt real interior brushwork over that
        # neighbourhood; leave the other six material masters unchanged.
        from repair_asphalt_tile import quilt
        preserved = directory / "pre-quilt-master-v2.png"
        if not preserved.exists():
            Image.fromarray(final_pixels).save(preserved)
        final_pixels, quilt_provenance = quilt(final_pixels)
    final = Image.fromarray(final_pixels)
    output = directory / f"{asset_id}.png"
    final.save(output)
    preview = directory / "tiled-3x3-qa.png"
    close_preview = directory / "tiled-3x3-close-qa.png"
    make_preview(final, preview, 256)
    make_preview(final, close_preview, 512)
    audit = write_audit(asset_id, metadata, raw, output, preview, close_preview, source_pixels, final_pixels)
    if quilt_provenance:
        audit["method"] = quilt_provenance["method"]
        audit["asphalt_quilt_override"] = {**quilt_provenance,
            "script": "tools/repair_asphalt_tile.py", "integrated_in": "tools/seamless_textures.py",
            "input_master": str((directory / "pre-quilt-master-v2.png").relative_to(ROOT)),
            "input_sha256": digest(directory / "pre-quilt-master-v2.png")}
        (record / "texture-audit.json").write_text(json.dumps(audit, indent=2) + "\n")
    return audit


def write_audit(asset_id: str, metadata: dict, raw: Path, output: Path,
                preview: Path, close_preview: Path, source: np.ndarray, final: np.ndarray) -> dict:
    measured = statistics(final)
    passed = all(measured[key]["max_abs_8bit"] == 0 for key in (
        "edge_value_x", "edge_value_y", "first_difference_x", "first_difference_y"))
    if not passed:
        raise ValueError(f"Boundary projection did not satisfy C1 export contract: {asset_id}")
    path = ROOT / "assets/production/records" / asset_id / "texture-audit.json"
    previous = json.loads(path.read_text()) if path.exists() else {}
    visual = previous.get("visual_qa", {"status": "pending"}) if previous.get("sha256") == digest(output) else {"status": "pending"}
    mip_checks = {}
    for dimension in (512, 256, 128, 64, 32):
        mip = Image.fromarray(final).resize((dimension, dimension), Image.Resampling.BOX)
        mip_checks[str(dimension)] = statistics(np.asarray(mip))
    audit = {"id": asset_id, "source": metadata["source"], "preserved_raw_file": str(raw),
             "source_sha256": digest(raw), "generation_record": str(path.parent / "tile-v1-generated.json"),
             "final_file": str(output), "sha256": digest(output), "dimensions": [1024, 1024],
             "channels": "RGB", "color_space": "sRGB albedo", "role": "repeating albedo; no baked lighting or PBR maps",
             "method": METHOD, "source_seams": statistics(source), "exported_seams": measured,
             "correction_rms_8bit": float(np.sqrt(np.mean((final.astype(float) - source)**2))),
             "numerical_qa": "passed", "visual_qa": visual,
             "box_downsample_mip_diagnostics": mip_checks,
             "tiled_preview": str(preview), "close_tiled_preview": str(close_preview),
             "repeat_sampler": {"wrap_s": "REPEAT", "wrap_t": "REPEAT"},
             "limitations": ["Shipping gameplay integration, target GPU mip selection and device-scale density are not qualified; native authoring preview evidence is separate.",
                             "A single repeat tile can still show recurrent brush motifs over very large empty surfaces; numerical continuity alone does not establish art approval."]}
    path.write_text(json.dumps(audit, indent=2) + "\n")
    return audit


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("materials", nargs="*", metavar="MATERIAL")
    args = parser.parse_args()
    materials = args.materials or MATERIALS
    invalid = set(materials) - set(MATERIALS)
    if invalid:
        parser.error(f"Unknown materials: {sorted(invalid)}")
    for material in materials:
        audit = export_material(material)
        print(f"{audit['id']}: numerical seam QA passed; visual {audit['visual_qa']['status']}; {audit['final_file']}")


if __name__ == "__main__":
    main()
