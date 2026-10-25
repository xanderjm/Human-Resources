#!/usr/bin/env python3
"""Contract tests for exact, stale-free cooked runtime asset staging."""

from __future__ import annotations

from pathlib import Path
import sys
import os
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools/generators"))
import tempfile

from stage_runtime_assets import mirror_tree, xcode_bundle_assets


def require(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def paths(root: Path) -> list[str]:
    return sorted(
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file()
    )


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="foundation-runtime-stage-") as temporary:
        root = Path(temporary)
        source = root / "generated-assets"
        output = root / "player-assets"
        (source / "runtime/ui").mkdir(parents=True)
        (source / "shaders/metal").mkdir(parents=True)
        (source / "runtime/ui/current.rgba").write_bytes(b"current-v1")
        (source / "shaders/metal/bootstrap.metallib").write_bytes(b"metal-v1")
        (output / "runtime/ui").mkdir(parents=True)
        (output / "runtime/ui/retired.rgba").write_bytes(b"must disappear")

        require(mirror_tree(source, output) == 2, "first stage count drifted")
        require(
            paths(output)
            == ["runtime/ui/current.rgba", "shaders/metal/bootstrap.metallib"],
            "first stage retained or omitted an asset",
        )

        (source / "runtime/ui/current.rgba").unlink()
        (source / "runtime/ui/sample-icon.rgba").write_bytes(b"bubble-v1")
        (output / "runtime/ui/injected-stale.rgba").write_bytes(b"must disappear")
        require(mirror_tree(source, output) == 2, "second stage count drifted")
        require(
            paths(output)
            == [
                "runtime/ui/sample-icon.rgba",
                "shaders/metal/bootstrap.metallib",
            ],
            "second stage did not exactly mirror the changed source",
        )
        require(
            (output / "runtime/ui/sample-icon.rgba").read_bytes() == b"bubble-v1",
            "staged asset bytes drifted",
        )

        before_invalid_stage = paths(output)
        try:
            mirror_tree(root / "missing", output)
        except RuntimeError:
            pass
        else:
            raise RuntimeError("missing staging source was accepted")
        require(paths(output) == before_invalid_stage, "invalid stage mutated output")

        linked_target = root / "linked-target"
        linked_target.mkdir()
        (linked_target / "sentinel").write_bytes(b"preserve")
        linked_output = root / "linked-output"
        linked = True
        try:
            linked_output.symlink_to(linked_target, target_is_directory=True)
        except OSError as error:
            if os.name != "nt" or getattr(error, "winerror", None) != 1314:
                raise
            linked = False
        if linked:
            try:
                mirror_tree(source, linked_output)
            except RuntimeError:
                pass
            else:
                raise RuntimeError("symbolic-link staging destination was accepted")
            require(
                (linked_target / "sentinel").read_bytes() == b"preserve",
                "rejected symbolic-link destination was mutated",
            )

        try:
            mirror_tree(source, source / "nested-output")
        except RuntimeError:
            pass
        else:
            raise RuntimeError("overlapping staging paths were accepted")

    print("runtime asset staging contract passed")
    return 0


class RuntimeStagingContracts(unittest.TestCase):
    def test_contract(self):
        self.assertEqual(main(), 0)

    def test_xcode_uses_actual_device_or_simulator_products_directory(self):
        with tempfile.TemporaryDirectory(prefix="Xcode products ") as temporary:
            bundle = Path(temporary) / "Release${EFFECTIVE_PLATFORM_NAME}/Example.app"
            for suffix in ("-iphoneos", "-iphonesimulator"):
                with mock.patch.dict(os.environ, {"EFFECTIVE_PLATFORM_NAME": suffix,
                        "BUILT_PRODUCTS_DIR": str(Path(temporary) / "unrelated-custom-target")}, clear=True):
                    expected = Path(temporary) / ("Release" + suffix) / "Example.app/assets"
                    self.assertEqual(xcode_bundle_assets(bundle), expected)
                    for invalid in (Path("relative/Example.app"), Path(temporary) / "Example.zip"):
                        with self.assertRaises(RuntimeError):
                            xcode_bundle_assets(invalid)
            for environment in ({}, {"EFFECTIVE_PLATFORM_NAME": "../escape"}):
                with mock.patch.dict(os.environ, environment, clear=True), self.assertRaises(RuntimeError):
                    xcode_bundle_assets(bundle)


if __name__ == "__main__":
    unittest.main()
