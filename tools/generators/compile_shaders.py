#!/usr/bin/env python3
"""Compile the HLSL shader manifest into SDL_GPU-native offline artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import re

from source_inventory import inventory_sha256, source_records


RESOURCE_KEYS = ("samplers", "storage_textures", "storage_buffers", "uniform_buffers")


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--shadercross", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--include-output", required=True, type=Path)
    parser.add_argument(
        "--platform",
        required=True,
        choices=("macos", "ios", "android", "windows", "linux", "all"),
    )
    parser.add_argument("--apple-deployment-target", default="17.0")
    parser.add_argument("--apple-ios-sdk", choices=("iphoneos", "iphonesimulator"), default="iphoneos")
    return parser.parse_args()


def run(command: list[str]) -> None:
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        raise RuntimeError("shader command failed: " + " ".join(command))


def try_run(command: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, text=True, capture_output=True)


def shadercross(
    executable: Path,
    source: Path,
    include_dir: Path,
    stage: str,
    entrypoint: str,
    destination: str,
    output: Path,
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    run([
        str(executable),
        str(source),
        "-s", "HLSL",
        "-d", destination,
        "-t", stage,
        "-e", entrypoint,
        "-I", str(include_dir),
        "-o", str(output),
    ])


def mounted_metal_compiler() -> Path | None:
    status = try_run(["xcodebuild", "-showComponent", "metalToolchain"])
    match = re.search(r"^Toolchain Search Path:\s*(.+)$", status.stdout, re.MULTILINE)
    if match is not None:
        candidate = Path(match.group(1).strip()) / "Metal.xctoolchain/usr/bin/metal"
        if candidate.is_file():
            return candidate
    resolved = try_run(["xcrun", "-f", "metal"])
    if resolved.returncode == 0:
        candidate = Path(resolved.stdout.strip())
        if candidate.is_file():
            return candidate
    return None


def compile_metallib(
    msl: Path, output: Path, sdk: str, deployment_target: str, module_cache: Path
) -> None:
    if shutil.which("xcrun") is None:
        raise RuntimeError(f"xcrun is required to produce {sdk} metallib artifacts")
    air = output.with_suffix(".air")
    output.parent.mkdir(parents=True, exist_ok=True)
    module_cache.mkdir(parents=True, exist_ok=True)
    target = "air64-apple-macos14.0"
    if sdk in ("iphoneos", "iphonesimulator"):
        target = f"air64-apple-ios{deployment_target}"
        if sdk == "iphonesimulator":
            target += "-simulator"
    compile_command = [
        "xcrun", "-sdk", sdk, "metal", f"-fmodules-cache-path={module_cache}",
        "-target", target,
        "-c", str(msl), "-o", str(air),
    ]
    compile_result = try_run(compile_command)
    direct_metal: Path | None = None
    if compile_result.returncode != 0:
        direct_metal = mounted_metal_compiler()
        if direct_metal is None:
            sys.stderr.write(compile_result.stdout + compile_result.stderr)
            raise RuntimeError("Apple Metal compiler is unavailable; install Xcode's Metal Toolchain component")
        sdk_path = subprocess.run(
            ["xcrun", "-sdk", sdk, "--show-sdk-path"], check=True, text=True, capture_output=True
        ).stdout.strip()
        run([
            str(direct_metal), "-isysroot", sdk_path, "-target", target,
            f"-fmodules-cache-path={module_cache}", "-c", str(msl), "-o", str(air),
        ])

    link_result = try_run(["xcrun", "-sdk", sdk, "metallib", str(air), "-o", str(output)])
    if link_result.returncode != 0:
        direct_metal = direct_metal or mounted_metal_compiler()
        if direct_metal is None:
            sys.stderr.write(link_result.stdout + link_result.stderr)
            raise RuntimeError("Apple metallib linker is unavailable")
        run([str(direct_metal), str(air), "-o", str(output)])
    air.unlink()


def write_header(reflections: list[dict[str, object]], output: Path, package_platform: str) -> None:
    output.mkdir(parents=True, exist_ok=True)
    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace human_resources::renderer::generated {",
        "struct ShaderReflection final {",
        "  std::uint32_t samplers;",
        "  std::uint32_t storage_textures;",
        "  std::uint32_t storage_buffers;",
        "  std::uint32_t uniform_buffers;",
        "};",
        "",
        f'inline constexpr std::string_view package_platform = "{package_platform}";',
        "",
    ]
    for reflection in reflections:
        identifier = f"{reflection['program']}_{reflection['stage']}"
        values = ", ".join(str(reflection[key]) for key in RESOURCE_KEYS)
        lines.append(f"inline constexpr ShaderReflection {identifier}{{{values}}};")
    lines.extend(["", "}  // namespace human_resources::renderer::generated", ""])
    (output / "shader_manifest.generated.h").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    args = arguments()
    if args.source_root.is_symlink() or args.output_root.is_symlink():
        raise RuntimeError("shader source and output roots may not be symbolic links")
    source_root = args.source_root.resolve()
    output_root = args.output_root.resolve()
    if source_root == output_root or output_root in source_root.parents or source_root in output_root.parents:
        raise RuntimeError("shader source and output directories may not overlap")
    if args.output_root.exists():
        shutil.rmtree(args.output_root)
    args.output_root.mkdir(parents=True)
    manifest_path = args.source_root / "shader_manifest.json"
    authored_sources = source_records(source_root)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    reflections: list[dict[str, object]] = []
    package_manifest: dict[str, object] = {
        "schema_version": 1,
        "kind": "shaders",
        "source_manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        "sources": authored_sources,
        "source_root_sha256": inventory_sha256(authored_sources),
        "platform": args.platform,
        "artifacts": [],
    }

    for program in manifest["programs"]:
        source = args.source_root / program["source"]
        if not source.is_file():
            raise RuntimeError(f"shader source is missing: {source}")
        for stage_record in program["stages"]:
            stage = stage_record["stage"]
            entrypoint = stage_record["entrypoint"]
            stem = f"{program['name']}.{stage}"
            reflection_path = args.output_root / "reflection" / f"{stem}.json"
            shadercross(args.shadercross, source, args.source_root / "include", stage, entrypoint,
                        "JSON", reflection_path)
            reflection = json.loads(reflection_path.read_text(encoding="utf-8"))
            record: dict[str, object] = {"program": program["name"], "stage": stage}
            for key in RESOURCE_KEYS:
                record[key] = int(reflection.get(key, 0))
            reflections.append(record)

            outputs: list[tuple[str, str, Path]] = []
            if args.platform in ("android", "linux", "all"):
                outputs.append(("SPIRV", "spirv", args.output_root / "spirv" / f"{stem}.spv"))
            if args.platform in ("windows", "all"):
                outputs.append(("DXIL", "dxil", args.output_root / "dxil" / f"{stem}.dxil"))
            for destination, backend, output in outputs:
                shadercross(args.shadercross, source, args.source_root / "include", stage, entrypoint,
                            destination, output)
                package_manifest["artifacts"].append({
                    "program": program["name"], "stage": stage, "entrypoint": entrypoint,
                    "backend": backend, "path": output.relative_to(args.output_root).as_posix(),
                    "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                })

            apple_targets: list[tuple[str, str]] = []
            if args.platform in ("macos", "all"):
                apple_targets.append(("macos", "macosx"))
            if args.platform in ("ios", "all"):
                apple_targets.append(("ios", args.apple_ios_sdk))
            if apple_targets:
                msl = args.include_output.parent / "metal-source" / f"{stem}.metal"
                shadercross(args.shadercross, source, args.source_root / "include", stage, entrypoint,
                            "MSL", msl)
                for apple_name, sdk in apple_targets:
                    output = args.output_root / "metal" / apple_name / f"{stem}.metallib"
                    compile_metallib(
                        msl, output, sdk, args.apple_deployment_target,
                        args.include_output.parent / "metal-module-cache" / apple_name,
                    )
                    package_manifest["artifacts"].append({
                        "program": program["name"], "stage": stage, "entrypoint": entrypoint,
                        "backend": f"metal-{apple_name}", "path": output.relative_to(args.output_root).as_posix(),
                        "sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
                    })

    if source_records(source_root) != authored_sources:
        raise RuntimeError("shader sources changed during compilation")
    package_manifest["files"] = [
        {
            "id": path.relative_to(args.output_root).as_posix(),
            "path": path.relative_to(args.output_root).as_posix(),
            "size": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
        for path in sorted(item for item in args.output_root.rglob("*") if item.is_file())
        if path.name != "package-manifest.json"
    ]
    write_header(reflections, args.include_output, args.platform)
    (args.output_root / "package-manifest.json").write_text(
        json.dumps(package_manifest, indent=2) + "\n", encoding="utf-8"
    )
    print(f"compiled {len(package_manifest['artifacts'])} shader artifacts for {args.platform}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
