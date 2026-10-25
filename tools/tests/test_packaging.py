#!/usr/bin/env python3
"""Contract tests for transactional deterministic desktop packaging."""

from __future__ import annotations

import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/generators"))
import tempfile

from game_support.packaging import (
    desktop_package_stage_path,
    deterministic_zip,
    publish_desktop_package,
    sha256,
    stage_desktop_package,
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="foundation-package-") as temporary:
        root = Path(temporary)
        dependency = root / "external/SDL"
        dependency.mkdir(parents=True)
        (dependency / "LICENSE.txt").write_text("test licence\n", encoding="utf-8")
        (root / "dependencies.lock.json").write_text(
            json.dumps(
                {
                    "dependencies": [
                        {"name": "SDL", "path": "external/SDL"},
                    ]
                }
            ),
            encoding="utf-8",
        )
        (root / "THIRD_PARTY_NOTICES.md").write_text(
            "test notices\n", encoding="utf-8"
        )
        player = root / "build/release/sample_app_player"
        player.parent.mkdir(parents=True)
        player.write_bytes(b"player-v1")
        (player.parent / "assets/runtime").mkdir(parents=True)
        (player.parent / "assets/runtime/current.rgba").write_bytes(b"asset-v1")
        project: dict[str, object] = {
            "foundation_version": "test",
            "features": [],
            "identity": {
                "slug": "sample_app",
                "version": "0.1.0",
                "display_name": "Sample App",
            },
        }

        final_stage = desktop_package_stage_path(root, project, "macos")
        final_archive = final_stage.parent / f"{final_stage.name}.zip"
        final_stage.mkdir(parents=True)
        (final_stage / "last-known-good").write_bytes(b"old-stage")
        final_archive.write_bytes(b"old-archive")

        transaction = final_stage.parent / "transaction"
        transaction.mkdir()
        candidate_stage = transaction / final_stage.name
        stage_desktop_package(
            root,
            project,
            "macos",
            player,
            stage_override=candidate_stage,
        )
        require(
            (final_stage / "last-known-good").read_bytes() == b"old-stage",
            "candidate assembly changed the visible package before publication",
        )
        candidate_archive = transaction / final_archive.name
        deterministic_zip(candidate_stage, candidate_archive)
        candidate_digest = sha256(candidate_archive)
        publish_desktop_package(
            candidate_stage,
            candidate_archive,
            final_stage,
            final_archive,
        )
        require(
            (final_stage / "sample_app_player").read_bytes() == b"player-v1",
            "published package omitted or changed the player",
        )
        require(
            not (final_stage / "last-known-good").exists(),
            "published package retained the previous candidate",
        )
        require(
            sha256(final_archive) == candidate_digest,
            "published archive changed after audit-ready assembly",
        )

        visible_stage_digest = (final_stage / "runtime-manifest.json").read_bytes()
        visible_archive_digest = sha256(final_archive)
        invalid_stage = transaction / "missing-candidate"
        invalid_archive = transaction / "missing-candidate.zip"
        try:
            publish_desktop_package(
                invalid_stage,
                invalid_archive,
                final_stage,
                final_archive,
            )
        except RuntimeError:
            pass
        else:
            raise RuntimeError("invalid package candidate was published")
        require(
            (final_stage / "runtime-manifest.json").read_bytes()
            == visible_stage_digest,
            "rejected package candidate changed the visible stage",
        )
        require(
            sha256(final_archive) == visible_archive_digest,
            "rejected package candidate changed the visible archive",
        )

    print("transactional desktop packaging contract passed")
    return 0


class PackageTransactionContracts(unittest.TestCase):
    def test_contract(self):
        self.assertEqual(main(), 0)


if __name__ == "__main__":
    unittest.main()
