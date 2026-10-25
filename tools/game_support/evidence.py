"""Write a bounded, structured qualification record for the current host."""

from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
from typing import Sequence

from game_support.repository import git_root


def _capture(command: Sequence[str], cwd: Path | None = None) -> dict[str, object]:
    try:
        completed = subprocess.run(
            command,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            cwd=cwd,
        )
    except OSError as error:
        return {"command": list(command), "returncode": None, "output": str(error)}
    return {
        "command": list(command),
        "returncode": completed.returncode,
        "output": completed.stdout.strip()[:16384],
    }


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _tree_sha256(root: Path) -> tuple[int, str]:
    digest = hashlib.sha256()
    total_size = 0
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative = path.relative_to(root).as_posix()
        data = path.read_bytes()
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(data)
        digest.update(b"\0")
        total_size += len(data)
    return total_size, digest.hexdigest()


def _dependencies(root: Path) -> list[dict[str, object]]:
    lock = json.loads((root / "dependencies.lock.json").read_text(encoding="utf-8"))
    records: list[dict[str, object]] = []
    for dependency in lock["dependencies"]:
        checkout = root / str(dependency["path"])
        record = dict(dependency)
        if (checkout / ".git").exists():
            record["actual_head"] = _capture(("git", "rev-parse", "HEAD"), cwd=checkout)
            record["actual_origin"] = _capture(
                ("git", "remote", "get-url", "origin"), cwd=checkout
            )
            record["working_tree"] = _capture(
                ("git", "status", "--short", "--untracked-files=all"), cwd=checkout
            )
        records.append(record)
    return records


def _compiler_records(root: Path) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    paths = set(root.glob("build/**/CMakeCXXCompiler.cmake"))
    paths.update(root.glob("android/app/.cxx/**/CMakeCXXCompiler.cmake"))
    for path in sorted(paths):
        selected = [
            line.strip()
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines()
            if any(
                key in line
                for key in (
                    "CMAKE_CXX_COMPILER_ID",
                    "CMAKE_CXX_COMPILER_VERSION",
                    "CMAKE_CXX_COMPILER_ARCHITECTURE_ID",
                    "CMAKE_CXX_PLATFORM_ID",
                )
            )
        ]
        records.append({"path": path.relative_to(root).as_posix(), "values": selected})
    return records


def _sdk_records(root: Path, project: dict[str, object]) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    if sys.platform == "darwin":
        records.extend([
            _capture(("xcodebuild", "-version")),
            _capture(("xcrun", "--sdk", "macosx", "--show-sdk-version")),
        ])
        if "ios" in project.get("platforms", []):
            records.append(_capture(("xcrun", "--sdk", "iphoneos", "--show-sdk-version")))
    elif sys.platform == "win32":
        records.append(_capture(("cl",)))
    elif sys.platform.startswith("linux"):
        records.extend([_capture(("cc", "--version")), _capture(("vulkaninfo", "--summary"))])

    sdk_value = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    settings = project.get("platform_settings", {})
    if sdk_value and isinstance(settings, dict) and "android" in project.get("platforms", []):
        sdk_root = Path(sdk_value)
        components = (
            f"platforms/android-{settings.get('android_compile_sdk')}",
            "build-tools/36.0.0",
            f"ndk/{settings.get('android_ndk_version')}",
            "cmake/3.31.6",
        )
        records.append(_capture(("java", "-version")))
        for component in components:
            properties = sdk_root / component / "source.properties"
            records.append({
                "component": component,
                "source_properties": (
                    properties.read_text(encoding="utf-8", errors="replace")[:8192]
                    if properties.is_file()
                    else "missing"
                ),
            })
    return records


def _artifacts(root: Path) -> list[dict[str, object]]:
    candidates = set(root.glob("build/packages/**/*.zip"))
    candidates.update(root.glob("android/app/build/outputs/**/*.apk"))
    candidates.update(root.glob("android/app/build/outputs/**/*.aab"))
    records = [
        {
            "path": path.relative_to(root).as_posix(),
            "size": path.stat().st_size,
            "sha256": _sha256(path),
        }
        for path in sorted(candidate for candidate in candidates if candidate.is_file())
    ]
    for bundle in sorted(root.glob("build/ios-device/**/*.app")):
        if bundle.is_dir():
            size, digest = _tree_sha256(bundle)
            records.append({
                "path": bundle.relative_to(root).as_posix(),
                "size": size,
                "tree_sha256": digest,
            })
    return records


def write_evidence(root: Path, output: Path, smoke_logs: Sequence[Path]) -> Path:
    project = json.loads((root / "project.json").read_text(encoding="utf-8"))
    lineage = json.loads((root / "foundation.lock.json").read_text(encoding="utf-8"))
    host_tools = []
    for relative in (
        "build/host-tools/shadercross/shadercross",
        "build/host-tools/shadercross/shadercross.exe",
        "build/host-tools/flatbuffers/flatc",
        "build/host-tools/flatbuffers/flatc.exe",
    ):
        path = root / relative
        if path.is_file():
            host_tools.append({
                "path": path.relative_to(root).as_posix(),
                "size": path.stat().st_size,
                "sha256": _sha256(path),
            })
    logs = []
    for path in smoke_logs:
        resolved = path if path.is_absolute() else root / path
        if not resolved.is_file():
            raise RuntimeError(f"smoke evidence log is missing: {resolved}")
        logs.append({
            "path": resolved.relative_to(root).as_posix(),
            "sha256": _sha256(resolved),
            "output": resolved.read_text(encoding="utf-8", errors="replace")[:65536],
        })
    payload = {
        "schema_version": 1,
        "recorded_utc": datetime.now(timezone.utc).isoformat(),
        "foundation_version": project.get("foundation_version"),
        "foundation_lineage": lineage,
        "project": project,
        "host": {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "runner": {
                name: os.environ[name]
                for name in (
                    "GITHUB_ACTIONS",
                    "GITHUB_EVENT_NAME",
                    "GITHUB_JOB",
                    "GITHUB_REF",
                    "GITHUB_REPOSITORY",
                    "GITHUB_RUN_ID",
                    "GITHUB_RUN_ATTEMPT",
                    "GITHUB_SHA",
                    "GITHUB_WORKFLOW",
                    "ImageOS",
                    "ImageVersion",
                )
                if name in os.environ
            },
        },
        "repository": {
            "head": _capture(("git", "rev-parse", "HEAD"), cwd=root),
            "working_tree": _capture(
                ("git", "status", "--short", "--untracked-files=all"), cwd=root
            ),
            "root": str(git_root(root)),
        } if git_root(root) is not None else None,
        "tools": [
            _capture(("git", "--version")),
            _capture(("cmake", "--version")),
            _capture(("ninja", "--version")),
            _capture((sys.executable, "--version")),
        ],
        "sdks_and_drivers": _sdk_records(root, project),
        "cmake_compilers": _compiler_records(root),
        "dependencies": _dependencies(root),
        "host_tools": host_tools,
        "artifacts": _artifacts(root),
        "smoke_logs": logs,
    }
    output = output if output.is_absolute() else root / output
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"qualification evidence: {output}")
    print(f"sha256: {_sha256(output)}")
    return output
