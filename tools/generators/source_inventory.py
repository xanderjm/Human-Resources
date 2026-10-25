#!/usr/bin/env python3
"""Build deterministic, symlink-safe inventories for authored package sources."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


RESERVED_NAME = "package-manifest.json"
HASH_CHUNK_SIZE = 1024 * 1024


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(HASH_CHUNK_SIZE):
            digest.update(chunk)
    return digest.hexdigest()


def source_paths(root: Path) -> list[Path]:
    if root.is_symlink() or not root.is_dir():
        raise RuntimeError(f"authored source root is missing: {root}")
    files: list[Path] = []
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise RuntimeError(f"authored package sources may not be symbolic links: {path}")
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if "\\" in relative.as_posix() or ":" in relative.as_posix():
            raise RuntimeError(f"nonportable authored source path: {relative}")
        if relative.name == ".gitkeep":
            continue
        if any(part.startswith(".") for part in relative.parts):
            raise RuntimeError(f"hidden authored package source is not allowed: {relative}")
        if relative.as_posix() == RESERVED_NAME:
            raise RuntimeError(f"{RESERVED_NAME} is generated and may not be authored")
        files.append(path)
    return files


def source_records(root: Path) -> list[dict[str, object]]:
    return [
        {
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": file_sha256(path),
        }
        for path in source_paths(root)
    ]


def inventory_sha256(records: list[dict[str, object]]) -> str:
    canonical = json.dumps(
        records,
        ensure_ascii=True,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()
