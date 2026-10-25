"""Host-tool interpreter selection and selected-platform prerequisite checks."""

from __future__ import annotations

import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys


def executable(name: str) -> str | None:
    return shutil.which(name)


def build_python_executable() -> str:
    """Choose a stable Python for dependency generators invoked by CMake."""
    override = os.environ.get("GAME_PYTHON_EXECUTABLE")
    candidates = (
        override,
        executable("python3.13"),
        executable("python3.12"),
        executable("python3.11"),
        sys.executable,
        executable("python3"),
    )
    checked: set[str] = set()
    probe = (
        "import sys, pyexpat, xml.etree.ElementTree; "
        "raise SystemExit(0 if sys.version_info >= (3, 11) else 1)"
    )
    for candidate in candidates:
        if not candidate:
            continue
        resolved = str(Path(candidate).resolve())
        if resolved in checked:
            continue
        checked.add(resolved)
        completed = subprocess.run(
            (resolved, "-c", probe),
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if completed.returncode == 0:
            return resolved
    detail = "GAME_PYTHON_EXECUTABLE" if override else "healthy Python 3.11+ with working pyexpat/XML support"
    raise RuntimeError(f"no healthy build interpreter found; install {detail}")


def java_17_executable() -> Path:
    explicit = os.environ.get("JAVA_HOME")
    candidates = [Path(explicit) / "bin" / ("java.exe" if os.name == "nt" else "java")] if explicit else []
    if not explicit:
        found = executable("java")
        if found:
            candidates.append(Path(found))
        if sys.platform == "darwin":
            try:
                home = subprocess.run(["/usr/libexec/java_home", "-v", "17"], check=False,
                    text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
                if home.returncode == 0:
                    candidates.append(Path(home.stdout.strip()) / "bin/java")
            except OSError:
                pass
            candidates.extend([Path("/opt/homebrew/opt/openjdk@17/bin/java"),
                               Path("/usr/local/opt/openjdk@17/bin/java")])
    for candidate in candidates:
        try:
            probe = subprocess.run([str(candidate), "-version"], check=False,
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=15)
        except (OSError, subprocess.TimeoutExpired):
            continue
        if probe.returncode == 0 and re.search(r'version "17(?:[.]|")', probe.stdout):
            return candidate.resolve()
    raise RuntimeError("JDK 17 is required; set JAVA_HOME to a JDK 17 installation")


def android_sdk_root() -> Path:
    explicit = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    candidates = [Path(explicit)] if explicit else [
        Path.home() / "Library/Android/sdk", Path.home() / "Android/Sdk",
        Path(os.environ.get("LOCALAPPDATA", str(Path.home()))) / "Android/Sdk",
        Path("/opt/homebrew/share/android-commandlinetools"),
        Path("/usr/local/share/android-commandlinetools"),
    ]
    for candidate in candidates:
        if candidate.is_dir():
            return candidate.resolve()
    raise RuntimeError("Android SDK was not found; set ANDROID_HOME")


def android_environment() -> dict[str, str]:
    java = java_17_executable()
    sdk = android_sdk_root()
    environment = os.environ.copy()
    environment["JAVA_HOME"] = str(java.parent.parent)
    environment["PATH"] = str(java.parent) + os.pathsep + environment.get("PATH", "")
    environment["ANDROID_HOME"] = str(sdk)
    environment["ANDROID_SDK_ROOT"] = str(sdk)
    return environment


def bootstrap_checks() -> list[tuple[str, bool, str]]:
    checks: list[tuple[str, bool, str]] = []
    checks.append(("Python >= 3.11", sys.version_info >= (3, 11), platform.python_version()))
    try:
        build_python = build_python_executable()
        python_detail = f"{build_python} (Python 3.11+ with XML)"
        python_available = True
    except RuntimeError as error:
        python_detail = str(error)
        python_available = False
    checks.append(("Build Python", python_available, python_detail))
    for name in ("git", "cmake", "ninja"):
        path = executable(name)
        checks.append((name, path is not None, path or "not found on PATH"))
    return checks


def doctor_checks(target: str, project: dict[str, object]) -> list[tuple[str, bool, str]]:
    checks = bootstrap_checks()
    if target in ("macos", "ios"):
        for name in ("xcodebuild", "xcrun"):
            path = executable(name)
            checks.append((name, sys.platform == "darwin" and path is not None, path or "requires macOS/Xcode"))
        metal = subprocess.run(
            ("xcodebuild", "-showComponent", "metalToolchain"),
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        ) if executable("xcodebuild") else None
        metal_available = bool(metal and metal.returncode == 0 and "Status: installed" in metal.stdout)
        checks.append((
            "Metal Toolchain",
            metal_available,
            "installed" if metal_available else "run: xcodebuild -downloadComponent MetalToolchain",
        ))
    if target == "windows":
        checks.append(("Windows host", sys.platform == "win32", sys.platform))
        compiler = executable("cl")
        checks.append(("MSVC developer environment", compiler is not None, compiler or "cl.exe not on PATH"))
    if target == "linux":
        checks.append(("Linux host", sys.platform.startswith("linux"), sys.platform))
    if target == "android":
        try:
            java = java_17_executable()
            checks.append(("JDK 17", True, str(java)))
        except RuntimeError as error:
            checks.append(("JDK 17", False, str(error)))
        try:
            sdk_root = android_sdk_root()
            sdk_ok = True
            checks.append(("Android SDK", True, str(sdk_root)))
        except RuntimeError as error:
            sdk_root = None
            sdk_ok = False
            checks.append(("Android SDK", False, str(error)))
        if sdk_ok and sdk_root:
            settings = project.get("platform_settings", {})
            if not isinstance(settings, dict):
                settings = {}
            components = (
                f"platforms/android-{settings.get('android_compile_sdk')}",
                "build-tools/36.0.0",
                f"ndk/{settings.get('android_ndk_version')}",
                "cmake/3.31.6",
            )
            for component in components:
                present = (sdk_root / component).is_dir()
                install_name = component.replace("/", ";", 1)
                detail = str(sdk_root / component) if present else f"sdkmanager '{install_name}'"
                checks.append((f"Android {component}", present, detail))
    return checks
