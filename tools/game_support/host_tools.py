"""Serialized bootstrap and isolated, relocatable host-tool publication."""
from __future__ import annotations

from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Iterator


@contextmanager
def bootstrap_lock(root: Path, timeout: float = 600.0) -> Iterator[None]:
    path = root / "build/.bootstrap.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as lock:
        if path.stat().st_size == 0:
            lock.write(b"0")
            lock.flush()
        deadline = time.monotonic() + timeout
        while True:
            lock.seek(0)
            try:
                if os.name == "nt":
                    import msvcrt
                    msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                else:
                    import fcntl
                    fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except OSError:
                if time.monotonic() >= deadline:
                    raise RuntimeError("timed out waiting for the repository bootstrap lock") from None
                time.sleep(0.1)
        try:
            yield
        finally:
            lock.seek(0)
            if os.name == "nt":
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def temporary_parent() -> Path:
    override = os.environ.get("GAME_HOST_TOOLS_STAGE_ROOT")
    candidates = [Path(override)] if override else [
        Path(tempfile.gettempdir()), Path("/private/tmp"), Path("/tmp")]
    for path in candidates:
        if path.is_dir() and os.access(path, os.W_OK) and not any(c.isspace() for c in str(path)):
            return path
    raise RuntimeError("set GAME_HOST_TOOLS_STAGE_ROOT to a writable path without whitespace")


def working_tool(path: Path, argument: str) -> bool:
    try:
        return subprocess.run([str(path), argument], stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL, timeout=30, check=False).returncode == 0
    except (OSError, subprocess.TimeoutExpired):
        return False


def install_shadercross(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    # Support both an installed bundle and the pinned dependency's build tree.
    for directory in (source.parent, source.parent / "external/DirectXShaderCompiler/lib",
                      source.parent / "external/DirectXShaderCompiler/bin"):
        for pattern in ("*.dylib", "*.so*", "*.dll"):
            for library in directory.glob(pattern):
                if library.is_file():
                    shutil.copy2(library, destination.parent / library.name)
    if sys.platform == "darwin":
        rpaths = subprocess.run(["otool", "-l", str(destination)], check=True,
            text=True, stdout=subprocess.PIPE).stdout
        if "path @loader_path (" not in rpaths:
            subprocess.run(["install_name_tool", "-add_rpath", "@loader_path", str(destination)], check=True)


def build_tools(root: Path, project: dict[str, object], configure_and_build) -> None:
    from generators.stage_runtime_assets import mirror_tree

    configured = Path(os.environ.get("GAME_HOST_TOOLS_DIR", "build/host-tools"))
    output = configured if configured.is_absolute() else root / configured
    suffix = ".exe" if os.name == "nt" else ""
    needs_flatc = "deterministic-authority" in project.get("features", [])
    supplied = os.environ.get("GAME_SHADERCROSS_EXECUTABLE")
    fingerprint = hashlib.sha256(Path(__file__).read_bytes() + (root / "dependencies.lock.json").read_bytes() +
        f"host-tools-v1:{sys.platform}:{platform.machine()}".encode()).hexdigest()
    marker = output / "bootstrap.json"
    if not supplied and marker.is_file():
        try:
            stamp = json.loads(marker.read_text())
        except (OSError, ValueError):
            stamp = {}
        if (not stamp.get("supplied") and stamp.get("fingerprint") == fingerprint
                and working_tool(output / "shadercross" / f"shadercross{suffix}", "--help")
                and (not needs_flatc or working_tool(output / "flatbuffers" / f"flatc{suffix}", "--version"))):
            return
    # Third-party generators sometimes put unquoted paths in response files.
    # Build from fresh copies without whitespace; publish only runnable tools,
    # never CMake caches that refer to temporary aliases or the old checkout.
    with tempfile.TemporaryDirectory(prefix="foundation-host-tools-", dir=temporary_parent()) as temporary:
        stage = Path(temporary)
        candidate = stage / "installed"
        candidate.mkdir()
        names = (["flatbuffers"] if needs_flatc else []) + ([] if supplied else ["SDL", "SDL_shadercross"])
        for name in names:
            shutil.copytree(root / "external" / name, stage / "source" / name,
                ignore=shutil.ignore_patterns(".git", "__pycache__"))
        host_definitions = ("-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0",) if sys.platform == "darwin" else ()
        if needs_flatc:
            configure_and_build(stage / "source/flatbuffers", stage / "build/flatbuffers", (
                "-DCMAKE_BUILD_TYPE=Release", "-DFLATBUFFERS_BUILD_TESTS=OFF",
                "-DFLATBUFFERS_BUILD_FLATLIB=OFF", "-DFLATBUFFERS_BUILD_SHAREDLIB=OFF",
                "-DFLATBUFFERS_INSTALL=OFF", "-DFLATBUFFERS_BUILD_FLATC=ON", *host_definitions), "flatc")
            (candidate / "flatbuffers").mkdir()
            shutil.copy2(stage / "build/flatbuffers" / f"flatc{suffix}", candidate / "flatbuffers" / f"flatc{suffix}")
        if supplied:
            source = Path(supplied).expanduser().resolve()
            if not source.is_file():
                raise RuntimeError(f"GAME_SHADERCROSS_EXECUTABLE is not a file: {source}")
        else:
            prefix = stage / "prefix"
            configure_and_build(stage / "source/SDL", stage / "build/sdl", (
                "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}",
                "-DSDL_SHARED=OFF", "-DSDL_STATIC=ON", "-DSDL_TESTS=OFF", "-DSDL_EXAMPLES=OFF",
                "-DSDL_UNIX_CONSOLE_BUILD=ON", *host_definitions), "install")
            rpath = "@loader_path" if sys.platform == "darwin" else "$ORIGIN"
            configure_and_build(stage / "source/SDL_shadercross", stage / "build/shadercross", (
                "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_PREFIX_PATH={prefix}",
                f"-DCMAKE_BUILD_RPATH={rpath}", "-DCMAKE_BUILD_RPATH_USE_ORIGIN=ON",
                "-DSDLSHADERCROSS_VENDORED=ON", "-DSDLSHADERCROSS_SHARED=OFF",
                "-DSDLSHADERCROSS_STATIC=ON", "-DSDLSHADERCROSS_SPIRVCROSS_SHARED=OFF",
                "-DSDLSHADERCROSS_CLI=ON", "-DSDLSHADERCROSS_CLI_STATIC=ON",
                "-DSDLSHADERCROSS_TESTS=OFF", "-DSDLSHADERCROSS_INSTALL=OFF", *host_definitions), "shadercross")
            source = stage / "build/shadercross" / f"shadercross{suffix}"
        install_shadercross(source, candidate / "shadercross" / f"shadercross{suffix}")
        if not working_tool(candidate / "shadercross" / f"shadercross{suffix}", "--help"):
            raise RuntimeError("installed shadercross cannot run; previous host tools are preserved")
        if needs_flatc and not working_tool(candidate / "flatbuffers" / f"flatc{suffix}", "--version"):
            raise RuntimeError("installed flatc cannot run; previous host tools are preserved")
        (candidate / "bootstrap.json").write_text(json.dumps({"fingerprint": fingerprint, "supplied": bool(supplied)}, indent=2) + "\n")
        mirror_tree(candidate, output)
    if not working_tool(output / "shadercross" / f"shadercross{suffix}", "--help"):
        raise RuntimeError("shadercross failed after temporary build cleanup")
