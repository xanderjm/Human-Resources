#!/usr/bin/env python3
"""Mirror cooked runtime assets without retaining files removed from the source."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import tempfile


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _inventory(root: Path) -> dict[str, tuple[int, str]]:
    inventory: dict[str, tuple[int, str]] = {}
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise RuntimeError(f"staged runtime assets may not be symbolic links: {path}")
        if not path.is_file():
            continue
        inventory[path.relative_to(root).as_posix()] = (
            path.stat().st_size,
            _sha256(path),
        )
    return inventory


def mirror_tree(source_root: Path, output_root: Path) -> int:
    if source_root.is_symlink():
        raise RuntimeError(f"runtime asset source may not be a symbolic link: {source_root}")
    unresolved_output = output_root.absolute()
    if unresolved_output.is_symlink():
        raise RuntimeError(
            f"runtime asset destination may not be a symbolic link: {unresolved_output}"
        )
    source = source_root.resolve()
    output = unresolved_output.resolve()
    if not source.is_dir():
        raise RuntimeError(f"cooked runtime asset source is missing: {source}")
    if source == output or source in output.parents or output in source.parents:
        raise RuntimeError("runtime asset source and destination may not overlap")
    if output.exists() and not output.is_dir():
        raise RuntimeError(f"runtime asset destination is not a directory: {output}")

    source_inventory = _inventory(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    transaction = Path(
        tempfile.mkdtemp(prefix=f".{output.name}.stage-", dir=output.parent)
    )
    staged = transaction / "next"
    previous = transaction / "previous"
    try:
        shutil.copytree(source, staged)
        if _inventory(staged) != source_inventory or _inventory(source) != source_inventory:
            raise RuntimeError("staged runtime asset copy failed integrity verification")

        if output.exists():
            output.replace(previous)
        try:
            staged.replace(output)
        except OSError:
            if previous.exists() and not output.exists():
                previous.replace(output)
            raise
        if previous.exists():
            shutil.rmtree(previous)
    finally:
        # Preserve the backup if restoring it failed; never delete the last good tree.
        if transaction.exists() and not previous.exists():
            shutil.rmtree(transaction)
    return len(source_inventory)


def xcode_bundle_assets(bundle_root: Path) -> Path:
    if not bundle_root.is_absolute() or bundle_root.suffix != ".app":
        raise RuntimeError("Xcode staging requires an absolute application bundle path")
    suffix = os.environ.get("EFFECTIVE_PLATFORM_NAME")
    if suffix not in ("-iphoneos", "-iphonesimulator"):
        raise RuntimeError("Xcode must supply its iOS EFFECTIVE_PLATFORM_NAME for bundle staging")
    # A custom target has a different BUILT_PRODUCTS_DIR from the executable.
    # Use CMake's executable path, resolving only its known Xcode placeholder.
    return Path(str(bundle_root).replace("${EFFECTIVE_PLATFORM_NAME}", suffix)) / "assets"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=Path)
    destination = parser.add_mutually_exclusive_group(required=True)
    destination.add_argument("--output-root", type=Path)
    destination.add_argument("--xcode-bundle-root", type=Path)
    arguments = parser.parse_args()
    output = (xcode_bundle_assets(arguments.xcode_bundle_root)
              if arguments.xcode_bundle_root else arguments.output_root)
    count = mirror_tree(arguments.source_root, output)
    print(f"staged {count} cooked runtime asset files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
