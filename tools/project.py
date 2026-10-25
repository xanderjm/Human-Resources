#!/usr/bin/env python3
"""Portable developer and AI workflow for a generated game repository."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import Sequence

from game_support.host_tools import bootstrap_lock, build_tools
from game_support.packaging import (
    desktop_package_stage_path, deterministic_zip, publish_desktop_package,
    sha256, stage_desktop_package,
)
from game_support.publishing import publish_to_github
from game_support.repository import git_root
from game_support.evidence import write_evidence
from game_support.toolchains import (
    android_environment,
    bootstrap_checks,
    build_python_executable,
    doctor_checks,
    executable,
)


def repository_root() -> Path:
    for candidate in Path(__file__).resolve().parents:
        if (candidate / "project.json").is_file():
            return candidate
    raise RuntimeError("project.json was not found; run this command inside a generated project")


ROOT = repository_root()
PROJECT_PATH = ROOT / "project.json"


def project_contract() -> dict[str, object]:
    return json.loads(PROJECT_PATH.read_text(encoding="utf-8"))


def selected_platforms(project: dict[str, object]) -> tuple[str, ...]:
    return tuple(str(value) for value in project.get("platforms", []))


def selected_features(project: dict[str, object]) -> tuple[str, ...]:
    return tuple(str(value) for value in project.get("features", []))


def run(
    command: Sequence[str | Path],
    *,
    cwd: Path = ROOT,
    capture: bool = False,
    env: dict[str, str] | None = None,
) -> str:
    display = " ".join(str(part) for part in command)
    print(f"+ {display}", flush=True)
    completed = subprocess.run(
        [str(part) for part in command],
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
        env=env,
    )
    return completed.stdout.strip() if capture and completed.stdout else ""


def host_platform() -> str:
    if sys.platform == "darwin":
        return "macos"
    if sys.platform == "win32":
        return "windows"
    if sys.platform.startswith("linux"):
        return "linux"
    return sys.platform


def executable_suffix() -> str:
    return ".exe" if sys.platform == "win32" else ""


def brief_todos() -> list[str]:
    brief = (ROOT / "PRODUCT_BRIEF.md").read_text(encoding="utf-8")
    return [line.strip() for line in brief.splitlines() if "TODO(REQUIRED)" in line]


def dependency_state(project: dict[str, object]) -> tuple[int, int]:
    lock = json.loads((ROOT / "dependencies.lock.json").read_text(encoding="utf-8"))
    dependencies = lock.get("dependencies", [])
    present = sum(1 for dependency in dependencies if (ROOT / str(dependency["path"])).is_dir())
    return present, len(dependencies)


def git_summary() -> str:
    if git_root(ROOT) is None:
        return "not initialized"
    completed = subprocess.run(
        ("git", "status", "--short"),
        cwd=ROOT,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    status = completed.stdout.strip()
    return "clean" if not status else f"{len(status.splitlines())} changed path(s)"


def repository_remote() -> str:
    if git_root(ROOT) is None:
        return "none"
    completed = subprocess.run(
        ("git", "remote", "get-url", "origin"),
        cwd=ROOT,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    return completed.stdout.strip() if completed.returncode == 0 else "none"


def status_payload() -> dict[str, object]:
    project = project_contract()
    todos = brief_todos()
    dependencies_present, dependencies_total = dependency_state(project)
    next_actions: list[str] = []
    if todos:
        next_actions.append("Ask the product owner to resolve PRODUCT_BRIEF.md TODO(REQUIRED) fields")
    if dependencies_present != dependencies_total:
        next_actions.append("Run python3 tools/project.py bootstrap")
    remote = repository_remote()
    if git_root(ROOT) is not None and remote == "none":
        next_actions.append("Publish explicitly with python3 tools/project.py publish --repository owner/name")
    next_actions.append("Run python3 tools/project.py verify")
    return {
        "foundation_version": project.get("foundation_version"),
        "project": project.get("identity", {}),
        "platforms": selected_platforms(project),
        "features": selected_features(project),
        "host": host_platform(),
        "brief_required_todos": len(todos),
        "dependencies": {"present": dependencies_present, "selected": dependencies_total},
        "git": git_summary(),
        "origin": remote,
        "next_actions": next_actions,
    }


def command_status(args: argparse.Namespace) -> int:
    payload = status_payload()
    if args.json:
        print(json.dumps(payload, indent=2))
        return 0
    project = payload["project"]
    display_name = project.get("display_name", "unknown") if isinstance(project, dict) else "unknown"
    print(f"Project: {display_name}")
    print(f"Foundation: {payload['foundation_version']}")
    print(f"Host: {payload['host']}")
    print(f"Platforms: {', '.join(payload['platforms'])}")
    print(f"Features: {', '.join(payload['features']) if payload['features'] else 'none'}")
    print(f"Required brief fields remaining: {payload['brief_required_todos']}")
    dependency = payload["dependencies"]
    print(f"Dependencies present: {dependency['present']}/{dependency['selected']}")
    print(f"Git: {payload['git']}")
    print(f"Origin: {payload['origin']}")
    print("Next actions:")
    for action in payload["next_actions"]:
        print(f"- {action}")
    return 0


def version_output(command: Sequence[str]) -> str:
    try:
        return run(command, capture=True).splitlines()[0]
    except (subprocess.CalledProcessError, FileNotFoundError, IndexError):
        return "unavailable"


def command_doctor(args: argparse.Namespace) -> int:
    project = project_contract()
    selected = selected_platforms(project)
    target = args.platform or (host_platform() if host_platform() in selected else selected[0])
    if target not in selected_platforms(project):
        print(f"error: platform {target!r} is not selected in project.json", file=sys.stderr)
        return 2
    checks = doctor_checks(target, project)
    for name, passed, detail in checks:
        print(f"{'ok' if passed else 'missing'}: {name}: {detail}")
    if not all(passed for _, passed, _ in checks):
        print(f"doctor failed for {target}", file=sys.stderr)
        return 1
    print(f"doctor passed for {target}")
    print(f"cmake: {version_output(('cmake', '--version'))}")
    return 0


def bootstrap_dependencies() -> None:
    run((build_python_executable(), ROOT / "tools/generators/bootstrap_dependencies.py", "--root", ROOT))


def configure_and_build(
    source: Path,
    build: Path,
    definitions: Sequence[str],
    target: str,
) -> None:
    build_python = build_python_executable()
    configure = (
        "cmake",
        "-S",
        source,
        "-B",
        build,
        "-G",
        "Ninja",
        f"-DPython3_EXECUTABLE={build_python}",
        *definitions,
    )
    run(configure)
    run(("cmake", "--build", build, "--target", target, "--parallel", build_jobs()))


def build_jobs() -> str:
    return os.environ.get("GAME_BUILD_JOBS", "4")


def build_host_tools(project: dict[str, object]) -> None:
    build_tools(ROOT, project, configure_and_build)


def command_bootstrap(_: argparse.Namespace) -> int:
    base_checks = bootstrap_checks()
    for name, passed, detail in base_checks:
        print(f"{'ok' if passed else 'missing'}: {name}: {detail}")
    if not all(passed for _, passed, _ in base_checks):
        raise RuntimeError("bootstrap prerequisites are incomplete; resolve the missing checks above")
    project = project_contract()
    run((build_python_executable(), ROOT / "tools/bootstrap_art_tools.py", "--download-blender"))
    with bootstrap_lock(ROOT):
        bootstrap_dependencies()
        build_host_tools(project)
    print("selected dependencies and host tools are ready")
    return 0


def configure_preset(preset: str) -> None:
    build_python = build_python_executable()
    definitions = [f"-DPython3_EXECUTABLE={build_python}"]
    if os.environ.get("GAME_HOST_TOOLS_DIR"):
        host_root = Path(os.environ["GAME_HOST_TOOLS_DIR"])
        if not host_root.is_absolute():
            host_root = ROOT / host_root
        definitions.extend((
            f"-DGAME_SHADERCROSS_EXECUTABLE={host_root / 'shadercross' / ('shadercross' + executable_suffix())}",
            f"-DGAME_FLATC_EXECUTABLE={host_root / 'flatbuffers' / ('flatc' + executable_suffix())}"))
    run(("cmake", "--preset", preset, "-S", ROOT, *definitions))


def build_preset(preset: str) -> None:
    configure_preset(preset)
    run(("cmake", "--build", "--preset", preset, "--parallel", build_jobs()))


def command_build(args: argparse.Namespace) -> int:
    build_preset(args.preset)
    return 0


def command_test(args: argparse.Namespace) -> int:
    build_preset(args.preset)
    run(("ctest", "--preset", args.preset, "--test-dir", ROOT / f"build/{args.preset}"))
    return 0


def command_run(args: argparse.Namespace) -> int:
    if host_platform() not in selected_platforms(project_contract()):
        raise RuntimeError(f"host platform {host_platform()} is not selected")
    build_preset(args.preset)
    slug = str(project_contract()["identity"]["slug"])
    player = ROOT / "build" / args.preset / f"{slug}_player{executable_suffix()}"
    if not player.is_file():
        raise RuntimeError(f"desktop player was not produced: {player}")
    run((player, *args.arguments))
    return 0


def package_desktop(target: str) -> Path:
    if target != host_platform():
        raise RuntimeError(f"desktop target {target} must be packaged on a {target} host")
    project = project_contract()
    build_preset("release")
    identity = project["identity"]
    slug = str(identity["slug"])
    player = ROOT / "build" / "release" / f"{slug}_player{executable_suffix()}"
    if not player.is_file():
        raise RuntimeError(f"release player was not produced: {player}")
    final_stage = desktop_package_stage_path(ROOT, project, target)
    final_archive = final_stage.parent / f"{final_stage.name}.zip"
    final_stage.parent.mkdir(parents=True, exist_ok=True)
    transaction = Path(tempfile.mkdtemp(
        prefix=f".{final_stage.name}.package-", dir=final_stage.parent
    ))
    try:
        candidate_stage = transaction / final_stage.name
        stage_desktop_package(
            ROOT, project, target, player, stage_override=candidate_stage
        )
        run(
            (
                build_python_executable(),
                ROOT / "tools/generators/audit_runtime_package.py",
                "--package-root",
                candidate_stage,
                "--require-manifest",
                "--expected-shader-manifest",
                ROOT / "build/release/generated/assets/shaders/package-manifest.json",
                "--expected-runtime-manifest",
                ROOT / "build/release/generated/assets/runtime/package-manifest.json",
                "--expected-shader-source-root",
                ROOT / "shaders",
                "--expected-runtime-source-root",
                ROOT / "assets/runtime",
            )
        )
        candidate_archive = transaction / f"{candidate_stage.name}.zip"
        deterministic_zip(candidate_stage, candidate_archive)
        publish_desktop_package(
            candidate_stage,
            candidate_archive,
            final_stage,
            final_archive,
        )
    finally:
        # If rollback itself failed, retain the previous artifacts for recovery.
        if not any((transaction / name).exists() for name in ("previous-stage", "previous-archive")):
            shutil.rmtree(transaction)
    archive = final_archive
    print(f"package: {archive}")
    print(f"sha256: {sha256(archive)}")
    return archive


def command_android_build(args: argparse.Namespace) -> int:
    if "android" not in selected_platforms(project_contract()):
        raise RuntimeError("Android is not a selected platform")
    environment = android_environment()
    command_bootstrap(args)
    host_root = Path(os.environ.get("GAME_HOST_TOOLS_DIR", ROOT / "build/host-tools"))
    if not host_root.is_absolute():
        host_root = ROOT / host_root
    shadercross = host_root / "shadercross" / f"shadercross{executable_suffix()}"
    run((build_python_executable(), ROOT / "tools/generators/compile_shaders.py",
        "--shadercross", shadercross, "--source-root", ROOT / "shaders",
        "--output-root", ROOT / "build/android-assets/shaders",
        "--include-output", ROOT / "build/android-shader-include", "--platform", "android"), env=environment)
    run((build_python_executable(), ROOT / "tools/generators/cook_runtime_assets.py",
        "--source-root", ROOT / "assets/runtime", "--output-root", ROOT / "build/android-assets/runtime",
        "--project", PROJECT_PATH), env=environment)
    variants = ("debug", "release") if args.variant == "all" else (args.variant,)
    wrapper = ROOT / "android" / ("gradlew.bat" if os.name == "nt" else "gradlew")
    run((wrapper, "--no-daemon", *(f":app:assemble{variant.title()}" for variant in variants)),
        cwd=ROOT / "android", env=environment)
    return 0


def command_package(args: argparse.Namespace) -> int:
    selected = selected_platforms(project_contract())
    if args.platform not in selected:
        raise RuntimeError(f"platform {args.platform!r} is not selected")
    if args.platform == "android":
        run((ROOT / "scripts/build-android.sh", "release"))
        run((ROOT / "scripts/verify-android.sh",
            ROOT / "android/app/build/outputs/apk/release/app-release-unsigned.apk"))
        return 0
    if args.platform == "ios":
        run((ROOT / "scripts/build-ios-device.sh",))
        run((ROOT / "scripts/verify-ios.sh",))
        return 0
    package_desktop(args.platform)
    return 0


def command_verify(args: argparse.Namespace) -> int:
    run((build_python_executable(), ROOT / "tools/generators/verify_foundation.py", "--root", ROOT))
    if not args.static_only:
        command_test(argparse.Namespace(preset=args.preset))
    if (ROOT / ".git").exists():
        run(("git", "diff", "--check"))
    print("repository verification passed for the executed host gates")
    return 0


def command_publish(args: argparse.Namespace) -> int:
    publish_to_github(ROOT, args.repository, args.visibility)
    return 0


def command_evidence(args: argparse.Namespace) -> int:
    output = args.output or Path(f"build/evidence/{host_platform()}.json")
    write_evidence(ROOT, output, args.smoke_log)
    return 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description="Portable workflow for this generated game")
    commands = root.add_subparsers(dest="command", required=True)

    status = commands.add_parser("status", help="Show profiles, readiness, and next actions")
    status.add_argument("--json", action="store_true")
    status.set_defaults(handler=command_status)

    doctor = commands.add_parser("doctor", help="Check prerequisites for a selected platform")
    doctor.add_argument("--platform")
    doctor.set_defaults(handler=command_doctor)

    bootstrap = commands.add_parser("bootstrap", help="Prepare exact dependencies and host tools")
    bootstrap.set_defaults(handler=command_bootstrap)

    build = commands.add_parser("build", help="Configure and build a CMake preset")
    build.add_argument("--preset", default="dev")
    build.set_defaults(handler=command_build)

    test = commands.add_parser("test", help="Build and run a CTest preset")
    test.add_argument("--preset", default="dev")
    test.set_defaults(handler=command_test)

    launch = commands.add_parser("run", help="Build and run the selected desktop host client")
    launch.add_argument("--preset", default="dev")
    launch.add_argument("arguments", nargs=argparse.REMAINDER)
    launch.set_defaults(handler=command_run)

    package = commands.add_parser("package", help="Build a selected platform package")
    package.add_argument("--platform", required=True)
    package.set_defaults(handler=command_package)

    publish = commands.add_parser("publish", help="Create and push this independent GitHub repository")
    publish.add_argument("--repository", required=True, help="GitHub owner/name")
    publish.add_argument("--visibility", choices=("private", "public", "internal"), default="private")
    publish.set_defaults(handler=command_publish)

    evidence = commands.add_parser("evidence", help="Record exact qualification tools and artifacts")
    evidence.add_argument("--output", type=Path)
    evidence.add_argument("--smoke-log", action="append", default=[], type=Path)
    evidence.set_defaults(handler=command_evidence)

    verify = commands.add_parser("verify", help="Run repository and host acceptance gates")
    verify.add_argument("--preset", default="dev")
    verify.add_argument("--static-only", action="store_true")
    verify.set_defaults(handler=command_verify)
    android_build = commands.add_parser("android-build", help="Build selected Android packages with scoped JDK/SDK discovery")
    android_build.add_argument("--variant", choices=("debug", "release", "all"), default="debug")
    android_build.set_defaults(handler=command_android_build)
    return root


def main() -> int:
    args = parser().parse_args()
    try:
        return int(args.handler(args))
    except (RuntimeError, subprocess.CalledProcessError, OSError, KeyError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
