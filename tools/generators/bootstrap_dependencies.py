#!/usr/bin/env python3
"""Materialize and verify repository-owned exact dependency checkouts."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys


def run(command: list[str], cwd: Path | None = None) -> str:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return completed.stdout.strip()


def verify_dependency(root: Path, dependency: dict[str, object]) -> None:
    destination = root / str(dependency["path"])
    expected = str(dependency["commit"])
    name = str(dependency["name"])
    expected_url = str(dependency["url"])
    if not destination.exists() or not (destination / ".git").exists():
        raise RuntimeError(f"{name}: checkout is missing or invalid: {destination}")
    origin = run(["git", "remote", "get-url", "origin"], cwd=destination)
    if origin != expected_url:
        raise RuntimeError(f"{name}: origin drift; expected {expected_url}, found {origin}")
    verified = run(["git", "rev-parse", "HEAD"], cwd=destination)
    if verified != expected:
        raise RuntimeError(f"{name}: expected {expected}, found {verified}")
    dirty = run(["git", "status", "--short", "--untracked-files=all"], cwd=destination)
    if dirty:
        raise RuntimeError(f"{name}: checkout is dirty:\n{dirty}")
    if bool(dependency.get("recursive", False)):
        submodules = subprocess.run(
            ["git", "submodule", "status", "--recursive"],
            cwd=destination,
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        ).stdout
        invalid = [line for line in submodules.splitlines() if line and not line.startswith(" ")]
        if invalid:
            raise RuntimeError(f"{name}: submodule drift or missing checkout:\n" + "\n".join(invalid))
    print(f"verified {name}: {verified}")


def checkout_dependency(root: Path, dependency: dict[str, object]) -> None:
    destination = root / str(dependency["path"])
    expected = str(dependency["commit"])
    name = str(dependency["name"])
    if destination.exists() and not (destination / ".git").exists():
        raise RuntimeError(f"{name}: destination exists but is not a git checkout: {destination}")
    if not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        run([
            "git",
            "clone",
            "--filter=blob:none",
            "--no-checkout",
            str(dependency["url"]),
            str(destination),
        ])
    present = subprocess.run(
        ["git", "cat-file", "-e", f"{expected}^{{commit}}"], cwd=destination,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    ).returncode == 0
    if not present:
        run(["git", "fetch", "--depth", "1", "origin", expected], cwd=destination)
    run(["git", "checkout", "--detach", expected], cwd=destination)
    if bool(dependency.get("recursive", False)):
        run(["git", "submodule", "sync", "--recursive"], cwd=destination)
        run(["git", "submodule", "update", "--init", "--recursive", "--depth", "1"], cwd=destination)
    verify_dependency(root, dependency)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--include-optional", action="store_true")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    lock = json.loads((root / "dependencies.lock.json").read_text(encoding="utf-8"))
    for dependency in lock["dependencies"]:
        if not dependency.get("default", False) and not args.include_optional:
            continue
        if args.verify_only:
            verify_dependency(root, dependency)
        else:
            checkout_dependency(root, dependency)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"dependency bootstrap failed: {error}", file=sys.stderr)
        raise SystemExit(1)
