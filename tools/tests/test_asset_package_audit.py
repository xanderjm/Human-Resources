#!/usr/bin/env python3
"""Prove internally valid stale packages cannot pass current-manifest binding."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import sys
import os
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/generators"))
import tempfile
import zipfile

from audit_asset_package import audit_directory, audit_zip
from source_inventory import inventory_sha256, source_records


def require_rejection(operation, message: str) -> None:
    try:
        operation()
    except RuntimeError:
        return
    raise RuntimeError(message)


def manifest_bytes(
    payload: bytes,
    revision: str,
    authored_sources: list[dict[str, object]],
) -> bytes:
    manifest = {
        "schema_version": 1,
        "kind": "runtime",
        "revision": revision,
        "source_root_sha256": inventory_sha256(authored_sources),
        "sources": authored_sources,
        "files": [
            {
                "id": "ui/tutorial/sample-icon.rgba",
                "path": "ui/tutorial/sample-icon.rgba",
                "size": len(payload),
                "sha256": hashlib.sha256(payload).hexdigest(),
            }
        ],
    }
    return (json.dumps(manifest, indent=2) + "\n").encode("utf-8")


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="foundation-asset-audit-") as temporary:
        root = Path(temporary)
        source_root = root / "authored-runtime"
        source_asset = source_root / "ui/tutorial/sample-icon.png"
        source_asset.parent.mkdir(parents=True)
        source_asset.write_bytes(b"current-authored-bubble")
        authored_sources = source_records(source_root)

        packaged_root = root / "runtime"
        packaged_asset = packaged_root / "ui/tutorial/sample-icon.rgba"
        packaged_asset.parent.mkdir(parents=True)
        payload = b"current-cooked-bubble"
        packaged_asset.write_bytes(payload)

        packaged_manifest = packaged_root / "package-manifest.json"
        current_manifest = root / "current-runtime-manifest.json"
        current_data = manifest_bytes(payload, "current", authored_sources)
        packaged_manifest.write_bytes(current_data)
        current_manifest.write_bytes(current_data)
        audit_directory(packaged_root, "runtime", current_manifest, source_root)

        stale_manifest = root / "stale-runtime-manifest.json"
        stale_manifest.write_bytes(
            manifest_bytes(payload, "stale-but-self-consistent", authored_sources)
        )
        packaged_manifest.write_bytes(stale_manifest.read_bytes())
        audit_directory(packaged_root, "runtime")
        require_rejection(
            lambda: audit_directory(packaged_root, "runtime", current_manifest),
            "self-consistent stale directory package matched the current manifest",
        )

        archive = root / "candidate.apk"
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr(
                "assets/runtime/package-manifest.json",
                stale_manifest.read_bytes(),
            )
            output.writestr(
                "assets/runtime/ui/tutorial/sample-icon.rgba",
                payload,
            )
        audit_zip(archive, "assets/runtime", "runtime")
        require_rejection(
            lambda: audit_zip(
                archive,
                "assets/runtime",
                "runtime",
                current_manifest,
            ),
            "self-consistent stale archive package matched the current manifest",
        )

        require_rejection(
            lambda: audit_directory(
                packaged_root,
                "runtime",
                root / "missing-current-manifest.json",
            ),
            "missing current manifest was accepted",
        )

        packaged_manifest.write_bytes(current_data)
        source_asset.write_bytes(b"changed-after-generated-manifest")
        audit_directory(packaged_root, "runtime", current_manifest)
        require_rejection(
            lambda: audit_directory(
                packaged_root,
                "runtime",
                current_manifest,
                source_root,
            ),
            "package matching a stale generated manifest matched changed authored sources",
        )

        source_bound_archive = root / "source-bound-candidate.apk"
        with zipfile.ZipFile(source_bound_archive, "w") as output:
            output.writestr("assets/runtime/package-manifest.json", current_data)
            output.writestr(
                "assets/runtime/ui/tutorial/sample-icon.rgba",
                payload,
            )
        require_rejection(
            lambda: audit_zip(
                source_bound_archive,
                "assets/runtime",
                "runtime",
                current_manifest,
                source_root,
            ),
            "archive matching a stale generated manifest matched changed authored sources",
        )

        packaged_manifest.write_bytes(current_data)
        source_asset.write_bytes(b"current-authored-bubble")
        linked_source = source_root / "linked-source.png"
        linked = True
        try:
            linked_source.symlink_to(source_asset)
        except OSError as error:
            if os.name != "nt" or getattr(error, "winerror", None) != 1314:
                raise
            linked = False
        if linked:
            require_rejection(
                lambda: audit_directory(
                    packaged_root,
                    "runtime",
                    current_manifest,
                    source_root,
                ),
                "symbolic link in authored source inventory was accepted",
            )
            linked_source.unlink()

        unsafe_data = json.loads(current_data.decode("utf-8"))
        unsafe_data["sources"][0]["path"] = "../outside.png"
        unsafe_manifest = root / "unsafe-runtime-manifest.json"
        unsafe_manifest.write_text(json.dumps(unsafe_data) + "\n", encoding="utf-8")
        packaged_manifest.write_bytes(unsafe_manifest.read_bytes())
        require_rejection(
            lambda: audit_directory(
                packaged_root,
                "runtime",
                None,
                source_root,
            ),
            "unsafe authored source path was accepted",
        )

    print("current manifest and authored-source package binding contract passed")
    return 0


class AssetBindingContracts(unittest.TestCase):
    def test_contract(self):
        self.assertEqual(main(), 0)


if __name__ == "__main__":
    unittest.main()
