"""Host-only offline art tools. Never search another application's private cache."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
CONTRACT = json.loads((ROOT / "tools/art-toolchain.json").read_text())
BLENDER_VERSION = CONTRACT["blender_version"]
PYTHON_PACKAGES = CONTRACT["python_packages"]


def scoped_python(root: Path = ROOT) -> Path:
    return root / "build/tools/art-python" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")


def python_versions(executable: Path) -> dict:
    probe = "import json,sys,pyexpat,numpy,PIL; print(json.dumps({'python':list(sys.version_info[:3]),'numpy':numpy.__version__,'pillow':PIL.__version__}))"
    try:
        result = subprocess.run([str(executable), "-I", "-c", probe], capture_output=True, text=True, timeout=30)
        versions = json.loads(result.stdout) if result.returncode == 0 else {}
    except (OSError, subprocess.TimeoutExpired, ValueError):
        versions = {}
    if (not (3, 11) <= tuple(versions.get("python", [])) < (3, 15) or
            any(versions.get(name) != value for name, value in PYTHON_PACKAGES.items())):
        raise RuntimeError(f"Art Python {executable} requires healthy Python 3.11–3.14 with pyexpat/XML, NumPy 2.3.5 and Pillow 12.3.0; run python tools/bootstrap_art_tools.py")
    return versions


def art_python(root: Path = ROOT) -> Path:
    explicit = os.environ.get("GAME_ART_PYTHON")
    candidates = [Path(explicit)] if explicit else [scoped_python(root), Path(sys.executable)]
    for candidate in candidates:
        try:
            python_versions(candidate)
            # Do not resolve venv symlinks: executing the base interpreter loses its packages.
            return candidate.absolute()
        except RuntimeError:
            if explicit:
                raise
    raise RuntimeError("Offline art tools are missing; run python tools/bootstrap_art_tools.py, or set GAME_ART_PYTHON to the pinned art environment")


def blender_candidates(root: Path = ROOT) -> list[Path]:
    base = root / "build/tools/blender"
    candidates = [base / "Blender.app/Contents/MacOS/Blender", base / "blender.exe", base / "blender"]
    found = shutil.which("blender")
    if found:
        candidates.append(Path(found))
    if sys.platform == "darwin":
        candidates.append(Path("/Applications/Blender.app/Contents/MacOS/Blender"))
    return candidates


def blender_executable(explicit: Path | None = None, root: Path = ROOT) -> Path:
    override = explicit or (Path(os.environ["GAME_BLENDER"]) if os.environ.get("GAME_BLENDER") else None)
    for candidate in [override] if override else blender_candidates(root):
        try:
            result = subprocess.run([str(candidate), "--version"], capture_output=True, text=True, timeout=30)
            if result.returncode == 0 and re.search(r"^Blender " + re.escape(BLENDER_VERSION) + r"(?:\s|$)", result.stdout, re.MULTILINE):
                return candidate.absolute()
        except (OSError, subprocess.TimeoutExpired):
            pass
        if override:
            raise RuntimeError(f"Configured Blender must be {BLENDER_VERSION}: {candidate}")
    raise RuntimeError(f"Blender {BLENDER_VERSION} is required for cold animation exports; run python tools/bootstrap_art_tools.py --download-blender, or set GAME_BLENDER")
