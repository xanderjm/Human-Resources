#!/usr/bin/env python3
"""Reject development/source/compiler material from a candidate runtime package."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from audit_asset_package import audit_directory


FORBIDDEN_PARTS = {"external", "fixtures", "mockups", "captures", "shadercross", "flatc", "CMakeFiles"}
FORBIDDEN_SUFFIXES = {
    ".air", ".cpp", ".fbs", ".h", ".hlsl", ".hlsli", ".metal", ".pdb", ".py", ".storyboard",
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", required=True, type=Path)
    parser.add_argument("--require-manifest", action="store_true")
    parser.add_argument("--expected-shader-manifest", type=Path)
    parser.add_argument("--expected-runtime-manifest", type=Path)
    parser.add_argument("--expected-shader-source-root", type=Path)
    parser.add_argument("--expected-runtime-source-root", type=Path)
    args = parser.parse_args()
    package = args.package_root.absolute()
    if package.is_symlink() or not package.is_dir():
        print(f"package root does not exist: {package}")
        return 2
    rejected: list[Path] = []
    for path in package.rglob("*"):
        if path.is_symlink():
            rejected.append(path.relative_to(package))
            continue
        if not path.is_file():
            continue
        relative = path.relative_to(package)
        if any(part in FORBIDDEN_PARTS for part in relative.parts) or path.suffix.lower() in FORBIDDEN_SUFFIXES:
            rejected.append(relative)
    if rejected:
        print("release package contains forbidden material:")
        for path in rejected:
            print(f"- {path}")
        return 1
    try:
        audit_directory(
            package / "assets/shaders",
            "shaders",
            args.expected_shader_manifest.resolve()
            if args.expected_shader_manifest is not None
            else None,
            args.expected_shader_source_root.absolute()
            if args.expected_shader_source_root is not None
            else None,
        )
        audit_directory(
            package / "assets/runtime",
            "runtime",
            args.expected_runtime_manifest.resolve()
            if args.expected_runtime_manifest is not None
            else None,
            args.expected_runtime_source_root.absolute()
            if args.expected_runtime_source_root is not None
            else None,
        )
    except (OSError, RuntimeError, UnicodeDecodeError, json.JSONDecodeError) as error:
        print(f"release asset audit failed: {error}")
        return 1
    if args.require_manifest:
        manifest_path = package / "runtime-manifest.json"
        if not manifest_path.is_file():
            print("release package is missing runtime-manifest.json")
            return 1
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        records = manifest.get("files")
        if not isinstance(records, list):
            print("runtime manifest files is not a list")
            return 1
        expected = {path.relative_to(package).as_posix() for path in package.rglob("*") if path.is_file()}
        expected.discard("runtime-manifest.json")
        recorded = {str(record.get("path")) for record in records if isinstance(record, dict)}
        if expected != recorded or len(recorded) != len(records):
            print("runtime manifest does not exactly match packaged files")
            return 1
        for record in records:
            path = package / str(record["path"])
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if path.stat().st_size != record.get("size") or digest != record.get("sha256"):
                print(f"runtime manifest digest or size mismatch: {record['path']}")
                return 1
    print(f"release package allowlist audit passed: {package}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
