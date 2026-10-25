#!/usr/bin/env python3
"""Verify an exact asset package inventory, sizes, and SHA-256 digests."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import zipfile

from source_inventory import inventory_sha256, source_records


MANIFEST_NAME = "package-manifest.json"


def _validated_records(manifest_data: bytes, expected_kind: str) -> list[dict[str, object]]:
    manifest = json.loads(manifest_data.decode("utf-8"))
    if manifest.get("schema_version") != 1 or manifest.get("kind") != expected_kind:
        raise RuntimeError(f"invalid {expected_kind} asset manifest identity")
    records = manifest.get("files")
    if not isinstance(records, list):
        raise RuntimeError(f"{expected_kind} asset manifest files is not a list")

    paths: set[str] = set()
    identifiers: set[str] = set()
    for record in records:
        if not isinstance(record, dict):
            raise RuntimeError(f"{expected_kind} asset manifest record is not an object")
        path = record.get("path")
        identifier = record.get("id")
        size = record.get("size")
        digest = record.get("sha256")
        if not isinstance(path, str) or not path:
            raise RuntimeError(f"{expected_kind} asset manifest path is invalid")
        parsed = PurePosixPath(path)
        if parsed.is_absolute() or "\\" in path or ":" in path or "." in parsed.parts or ".." in parsed.parts or parsed.as_posix() != path:
            raise RuntimeError(f"unsafe {expected_kind} asset path: {path}")
        if not isinstance(identifier, str) or not identifier:
            raise RuntimeError(f"{expected_kind} asset identifier is invalid: {path}")
        if path in paths or identifier in identifiers:
            raise RuntimeError(f"duplicate {expected_kind} asset path or identifier: {path}")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise RuntimeError(f"invalid {expected_kind} asset size: {path}")
        if not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
            raise RuntimeError(f"invalid {expected_kind} asset digest: {path}")
        paths.add(path)
        identifiers.add(identifier)
    return records


def _validated_source_records(
    manifest_data: bytes,
    expected_kind: str,
) -> tuple[list[dict[str, object]], str]:
    manifest = json.loads(manifest_data.decode("utf-8"))
    records = manifest.get("sources")
    root_digest = manifest.get("source_root_sha256")
    if not isinstance(records, list):
        raise RuntimeError(f"{expected_kind} asset manifest sources is not a list")
    if not isinstance(root_digest, str) or re.fullmatch(r"[0-9a-f]{64}", root_digest) is None:
        raise RuntimeError(f"{expected_kind} asset manifest source-root digest is invalid")

    paths: set[str] = set()
    validated: list[dict[str, object]] = []
    for record in records:
        if not isinstance(record, dict) or set(record) != {"path", "size", "sha256"}:
            raise RuntimeError(f"{expected_kind} source manifest record is invalid")
        path = record.get("path")
        size = record.get("size")
        digest = record.get("sha256")
        if not isinstance(path, str) or not path:
            raise RuntimeError(f"{expected_kind} source manifest path is invalid")
        parsed = PurePosixPath(path)
        if parsed.is_absolute() or "\\" in path or ":" in path or "." in parsed.parts or ".." in parsed.parts or parsed.as_posix() != path:
            raise RuntimeError(f"unsafe {expected_kind} source path: {path}")
        if path in paths:
            raise RuntimeError(f"duplicate {expected_kind} source path: {path}")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise RuntimeError(f"invalid {expected_kind} source size: {path}")
        if not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None:
            raise RuntimeError(f"invalid {expected_kind} source digest: {path}")
        paths.add(path)
        validated.append(record)
    if [str(record["path"]) for record in validated] != sorted(paths):
        raise RuntimeError(f"{expected_kind} source manifest is not deterministically ordered")
    if inventory_sha256(validated) != root_digest:
        raise RuntimeError(f"{expected_kind} source-root digest does not match its records")
    return validated, root_digest


def _audit(
    manifest_data: bytes,
    packaged: dict[str, bytes],
    expected_kind: str,
) -> None:
    records = _validated_records(manifest_data, expected_kind)
    recorded = {str(record["path"]) for record in records}
    if recorded != set(packaged):
        missing = sorted(recorded - set(packaged))
        unlisted = sorted(set(packaged) - recorded)
        raise RuntimeError(
            f"{expected_kind} asset inventory mismatch; missing={missing}, unlisted={unlisted}"
        )
    for record in records:
        path = str(record["path"])
        data = packaged[path]
        if len(data) != record["size"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
            raise RuntimeError(f"{expected_kind} asset size or digest mismatch: {path}")


def _bind_current_manifest(
    packaged_manifest: bytes,
    current_manifest: Path | None,
    expected_kind: str,
) -> None:
    if current_manifest is None:
        return
    if not current_manifest.is_file():
        raise RuntimeError(
            f"current {expected_kind} asset manifest is missing: {current_manifest}"
        )
    current_data = current_manifest.read_bytes()
    _validated_records(current_data, expected_kind)
    if packaged_manifest != current_data:
        raise RuntimeError(
            f"packaged {expected_kind} asset manifest does not match current generated manifest"
        )


def _bind_current_sources(
    packaged_manifest: bytes,
    current_source_root: Path | None,
    expected_kind: str,
) -> None:
    if current_source_root is None:
        return
    packaged_sources, packaged_digest = _validated_source_records(
        packaged_manifest,
        expected_kind,
    )
    manifest = json.loads(packaged_manifest.decode("utf-8"))
    derived = manifest.get("derived_sources", [])
    if not isinstance(derived, list):
        raise RuntimeError("derived runtime sources must be a list")
    for record in derived:
        if not isinstance(record, dict) or set(record) != {"path", "size", "sha256"}:
            raise RuntimeError("invalid derived runtime source record")
        relative = record["path"]
        if not isinstance(relative, str) or not relative or PurePosixPath(relative).is_absolute() or ".." in PurePosixPath(relative).parts or "\\" in relative or ":" in relative or PurePosixPath(relative).as_posix()!=relative:
            raise RuntimeError("unsafe derived runtime source path")
        source = current_source_root.parents[1] / relative
        if source.is_symlink() or not source.is_file():
            raise RuntimeError(f"missing derived runtime source: {relative}")
        data = source.read_bytes()
        if len(data) != record["size"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
            raise RuntimeError(f"derived runtime source changed: {relative}")
    current_sources = source_records(current_source_root)
    current_digest = inventory_sha256(current_sources)
    if packaged_sources != current_sources or packaged_digest != current_digest:
        raise RuntimeError(
            f"packaged {expected_kind} asset manifest does not match current authored sources"
        )


def audit_directory(
    root: Path,
    expected_kind: str,
    current_manifest: Path | None = None,
    current_source_root: Path | None = None,
) -> None:
    if root.is_symlink() or any(path.is_symlink() for path in root.rglob("*")):
        raise RuntimeError("asset packages may not contain symbolic links")
    manifest_path = root / MANIFEST_NAME
    if not manifest_path.is_file():
        raise RuntimeError(f"{expected_kind} asset manifest is missing: {manifest_path}")
    manifest_data = manifest_path.read_bytes()
    _bind_current_manifest(manifest_data, current_manifest, expected_kind)
    _bind_current_sources(manifest_data, current_source_root, expected_kind)
    packaged = {
        path.relative_to(root).as_posix(): path.read_bytes()
        for path in root.rglob("*")
        if path.is_file() and path != manifest_path
    }
    _audit(manifest_data, packaged, expected_kind)


def audit_zip(
    archive: Path,
    prefix: str,
    expected_kind: str,
    current_manifest: Path | None = None,
    current_source_root: Path | None = None,
) -> None:
    normalized = prefix.strip("/") + "/"
    manifest_name = normalized + MANIFEST_NAME
    with zipfile.ZipFile(archive) as package:
        names = [info.filename for info in package.infolist() if not info.is_dir()]
        if len(names) != len(set(names)):
            raise RuntimeError("archive contains duplicate entries")
        if manifest_name not in names:
            raise RuntimeError(f"{expected_kind} asset manifest is missing from archive")
        manifest_data = package.read(manifest_name)
        _bind_current_manifest(manifest_data, current_manifest, expected_kind)
        _bind_current_sources(manifest_data, current_source_root, expected_kind)
        packaged = {
            name[len(normalized):]: package.read(name)
            for name in names
            if name.startswith(normalized) and name != manifest_name
        }
        _audit(manifest_data, packaged, expected_kind)


def main() -> int:
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--asset-root", type=Path)
    source.add_argument("--archive", type=Path)
    parser.add_argument("--prefix")
    parser.add_argument("--kind", required=True, choices=("runtime", "shaders"))
    parser.add_argument(
        "--expected-manifest",
        type=Path,
        help="require the packaged manifest to exactly match this current generated manifest",
    )
    parser.add_argument(
        "--expected-source-root",
        type=Path,
        help="require the packaged manifest to fingerprint this authored source tree",
    )
    args = parser.parse_args()
    try:
        if args.asset_root is not None:
            if args.prefix is not None:
                parser.error("--prefix is only valid with --archive")
            audit_directory(
                args.asset_root.absolute(),
                args.kind,
                args.expected_manifest.resolve()
                if args.expected_manifest is not None
                else None,
                args.expected_source_root.absolute()
                if args.expected_source_root is not None
                else None,
            )
            location = str(args.asset_root)
        else:
            if not args.prefix:
                parser.error("--prefix is required with --archive")
            audit_zip(
                args.archive.resolve(),
                args.prefix,
                args.kind,
                args.expected_manifest.resolve()
                if args.expected_manifest is not None
                else None,
                args.expected_source_root.absolute()
                if args.expected_source_root is not None
                else None,
            )
            location = f"{args.archive}:{args.prefix}"
    except (OSError, RuntimeError, UnicodeDecodeError, json.JSONDecodeError, zipfile.BadZipFile) as error:
        print(f"asset package audit failed: {error}")
        return 1
    print(f"{args.kind} asset package audit passed: {location}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
