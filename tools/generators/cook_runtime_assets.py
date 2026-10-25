#!/usr/bin/env python3
"""Copy declared runtime assets and emit a deterministic integrity manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from art_tools import art_python
from source_inventory import inventory_sha256, source_records
from stage_runtime_assets import mirror_tree


RESERVED_NAME = "package-manifest.json"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def cook(source_root: Path, output_root: Path, project: dict[str, object]) -> int:
    if source_root.is_symlink() or output_root.is_symlink():
        raise RuntimeError("runtime source and output roots may not be symbolic links")
    source_root = source_root.resolve()
    output_root = output_root.resolve()
    if not source_root.is_dir():
        raise RuntimeError(f"runtime asset source is missing: {source_root}")
    if (
        source_root == output_root
        or source_root in output_root.parents
        or output_root in source_root.parents
    ):
        raise RuntimeError("runtime asset source and output directories may not overlap")
    identity = project.get("identity")
    if not isinstance(identity, dict):
        raise RuntimeError("project.json identity is invalid")

    authored_sources = source_records(source_root)
    output_root.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".runtime-cook-", dir=output_root.parent) as temporary:
        candidate = Path(temporary)
        records: list[dict[str, object]] = []
        for record in authored_sources:
            package_path = str(record["path"])
            destination = candidate / package_path
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source_root / package_path, destination)
            data = destination.read_bytes()
            if len(data) != record["size"] or sha256(data) != record["sha256"]:
                raise RuntimeError(f"runtime source changed during cooking: {package_path}")
            records.append({"id": package_path, **record})
        derived_sources = []
        recipe = source_root / "game-content.json"
        if recipe.is_file():
            repository = source_root.parents[1]
            script = repository / "tools/cook_product_content.py"
            interpreter = art_python(repository)
            header = output_root.parents[1] / "product/include/product_glyphs.generated.h"
            subprocess.run([str(interpreter), str(script), "--output", str(candidate / "game"), "--header", str(header)], check=True)
            # Bind every authored model, font, material and cooking owner consumed
            # by this derived package. Assets/runtime remains the small recipe authority.
            provenance = json.loads((candidate / "game/content-provenance.json").read_text())
            paths = {str(item["source"]) for item in provenance["models"]}
            paths.update({"tools/art_tools.py", "tools/art-toolchain.json", "tools/art-requirements.txt", "tools/cook_product_content.py", "tools/cook_style_preview.py", "tools/normalize_glb.py", "assets/production/inventory.json"})
            definition = json.loads(recipe.read_text())
            paths.update("assets/textures/texture-" + name + "/texture-" + name + ".png" for name in definition["materials"])
            paths.update("assets/fonts/human-resources-v3/ttf/" + name for name in definition["fonts"])
            animation_provenance = json.loads((candidate / "game/animation-provenance.json").read_text())
            paths.update(animation_provenance["tool_sha256"])
            for item in animation_provenance["models"]:
                paths.add(item["source_model"])
                paths.update(item["authoring_inputs"])
            paths.add("design/balance/endless-upgrade-system.json")
            paths.add("assets/fonts/human-resources-v3/LICENSE.txt")
            paths.update("assets/graphics/" + name + "/" + name + ".png" for name in ["ui-coin","ui-shield","ui-beam","ui-laser","ui-hud-panel","ui-upgrade-card","ui-xp-track"])
            for relative in sorted(paths):
                data = (repository / relative).read_bytes()
                derived_sources.append({"path": relative, "size": len(data), "sha256": sha256(data)})
            for item in source_records(candidate / "game"):
                records.append({"id": "game/" + str(item["path"]), **item, "path": "game/" + str(item["path"])})
        if source_records(source_root) != authored_sources:
            raise RuntimeError("runtime sources changed during cooking")
        manifest = {
            "schema_version": 1,
            "kind": "runtime",
            "foundation_version": project.get("foundation_version"),
            "application": identity,
            "sources": authored_sources,
            "source_root_sha256": inventory_sha256(authored_sources),
            "files": sorted(records, key=lambda record: str(record["path"])),
            "derived_sources": derived_sources,
        }
        (candidate / RESERVED_NAME).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        mirror_tree(candidate, output_root)
    return len(records)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--project", required=True, type=Path)
    args = parser.parse_args()
    count = cook(args.source_root, args.output_root, json.loads(args.project.read_text(encoding="utf-8")))
    print(f"cooked {count} runtime assets")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
