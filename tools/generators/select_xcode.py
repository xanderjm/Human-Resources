#!/usr/bin/env python3
"""Select an installed, probed Xcode before invoking its optional components."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
from typing import Callable, Iterable


def xcode_version(developer: Path) -> tuple[int, ...] | None:
    command = developer / "usr/bin/xcodebuild"
    if not command.is_file():
        return None
    result = subprocess.run(
        (str(command), "-version"), env=dict(os.environ, DEVELOPER_DIR=str(developer)),
        check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )
    match = re.search(r"^Xcode (\d+(?:\.\d+)*)\s*$", result.stdout, re.MULTILINE)
    return tuple(map(int, match.group(1).split("."))) if result.returncode == 0 and match else None


def select_xcode(
    candidates: Iterable[Path], minimum_major: int,
    probe: Callable[[Path], tuple[int, ...] | None] = xcode_version,
) -> Path:
    qualified = []
    for path in sorted(set(candidates)):
        version = probe(path)
        if version and version[0] >= minimum_major:
            qualified.append((version, path))
    if not qualified:
        raise RuntimeError(f"no installed Xcode {minimum_major}+ passed xcodebuild -version")
    return max(qualified)[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--minimum-major", type=int, required=True)
    parser.add_argument("--github-env", type=Path)
    args = parser.parse_args()
    override = os.environ.get("DEVELOPER_DIR")
    candidates = [Path(override)] if override else list(Path("/Applications").glob("Xcode*.app/Contents/Developer"))
    selected = select_xcode(candidates, args.minimum_major)
    if "\n" in str(selected) or "\r" in str(selected):
        raise RuntimeError("Xcode path cannot contain newlines")
    if args.github_env:
        with args.github_env.open("a", encoding="utf-8") as output:
            output.write(f"DEVELOPER_DIR={selected}\n")
    print(f"DEVELOPER_DIR={selected}")
    print(f"Xcode version: {'.'.join(map(str, xcode_version(selected) or ())) }")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError) as error:
        raise SystemExit(str(error)) from error
