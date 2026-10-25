#!/usr/bin/env python3
"""Prepare repository-scoped offline art tools; no global Python or Blender install."""
from __future__ import annotations

import argparse
from contextlib import nullcontext
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import venv
import zipfile

from art_tools import ROOT, BLENDER_VERSION, art_python, blender_executable, python_versions, scoped_python
from game_support.host_tools import bootstrap_lock


def archive_for_host(contract: dict, system: str, machine: str) -> tuple[str, str]:
    machine = {"amd64": "x86_64", "aarch64": "arm64"}.get(machine.lower(), machine.lower())
    key = system + "-" + machine
    if key not in contract["archives"]:
        raise RuntimeError(f"No pinned Blender archive for {key}; use GAME_BLENDER on a supported authoring host")
    return tuple(contract["archives"][key])


def checked_download(url: str, expected: str, destination: Path) -> None:
    digest = hashlib.sha256()
    with urllib.request.urlopen(url, timeout=120) as source, destination.open("wb") as output:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            output.write(block)
            digest.update(block)
    if digest.hexdigest() != expected:
        destination.unlink()
        raise RuntimeError("Blender archive SHA-256 mismatch")


def install_blender(root: Path = ROOT) -> Path:
    contract = json.loads((root / "tools/art-toolchain.json").read_text())
    name, expected = archive_for_host(contract, platform.system(), platform.machine())
    parent = root / "build/tools"
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".blender-", dir=parent) as directory:
        temporary = Path(directory)
        archive = temporary / name
        print(f"Downloading pinned offline Blender: {name}", flush=True)
        checked_download(contract["archive_base"] + name, expected, archive)
        unpack = temporary / "unpack"
        unpack.mkdir()
        if name.endswith(".dmg"):
            mount = temporary / "mount"
            subprocess.run(["hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", str(mount), str(archive)], check=True)
            try:
                subprocess.run(["ditto", str(mount / "Blender.app"), str(unpack / "Blender.app")], check=True)
            finally:
                subprocess.run(["hdiutil", "detach", str(mount)], check=True)
            payload = unpack
        else:
            if name.endswith(".zip"):
                with zipfile.ZipFile(archive) as package:
                    package.extractall(unpack)
            else:
                with tarfile.open(archive) as package:
                    package.extractall(unpack, filter="data")
            roots = list(unpack.iterdir())
            if len(roots) != 1 or not roots[0].is_dir():
                raise RuntimeError("Unexpected Blender archive layout")
            payload = roots[0]
        binary = payload / ("Blender.app/Contents/MacOS/Blender" if name.endswith(".dmg") else "blender.exe" if name.endswith(".zip") else "blender")
        blender_executable(binary)
        destination = parent / "blender"
        if destination.exists():
            shutil.rmtree(destination)
        payload.rename(destination)
    return blender_executable(root=root)


def prepare_python(root: Path = ROOT) -> Path:
    # An explicit owner-provided interpreter is a strict override, never silently replaced.
    if os.environ.get("GAME_ART_PYTHON"):
        return art_python(root)
    executable = scoped_python(root)
    try:
        python_versions(executable)
        return executable
    except RuntimeError:
        pass
    if not (3, 11) <= sys.version_info[:2] < (3, 15):
        raise RuntimeError("Bootstrap offline art with Python 3.11–3.14")
    venv.EnvBuilder(with_pip=True).create(executable.parent.parent)
    subprocess.run([str(executable), "-I", "-m", "pip", "install", "--disable-pip-version-check",
                    "--cache-dir", str(root / "build/tools/art-wheel-cache"),
                    "--require-hashes", "--only-binary=:all:", "--no-deps", "-r",
                    str(root / "tools/art-requirements.txt")], check=True)
    python_versions(executable)
    return executable


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--download-blender", action="store_true", help="Download/checksum/extract the pinned host archive if Blender is missing")
    parser.add_argument("--check", action="store_true", help="Read-only check; no downloads or installs")
    arguments = parser.parse_args()
    with nullcontext() if arguments.check else bootstrap_lock(ROOT):
        python = art_python() if arguments.check else prepare_python()
        try:
            blender = blender_executable()
        except RuntimeError:
            if arguments.check or not arguments.download_blender or os.environ.get("GAME_BLENDER"):
                raise
            blender = install_blender()
        report = {"schema": 1, "python": str(python), "packages": python_versions(python),
                  "blender": str(blender), "blender_version": BLENDER_VERSION,
                  "contract_sha256": hashlib.sha256((ROOT / "tools/art-toolchain.json").read_bytes()).hexdigest(),
                  "requirements_sha256": hashlib.sha256((ROOT / "tools/art-requirements.txt").read_bytes()).hexdigest()}
        if not arguments.check:
            output = ROOT / "build/evidence/art-toolchain.json"
            output.parent.mkdir(parents=True, exist_ok=True)
            output.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
