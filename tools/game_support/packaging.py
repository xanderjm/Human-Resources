"""Deterministic desktop runtime staging and archive helpers."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import zipfile


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _copy_runtime_licenses(root: Path, stage: Path, features: tuple[str, ...]) -> None:
    runtime_dependencies = ["SDL"]
    if "dynamic-text" in features:
        runtime_dependencies.append("SDL_ttf")
    dependency_lock = json.loads((root / "dependencies.lock.json").read_text(encoding="utf-8"))
    by_name = {str(item["name"]): root / str(item["path"]) for item in dependency_lock["dependencies"]}
    for name in runtime_dependencies:
        source_root = by_name[name]
        candidates = sorted(
            path
            for path in source_root.rglob("*")
            if path.is_file()
            and path.stat().st_size <= 1024 * 1024
            and path.name.lower().startswith(("license", "copying", "copyright", "notice"))
        )
        if not candidates:
            raise RuntimeError(f"no license material found for runtime dependency {name}")
        notice_names: set[str] = set()
        for source in candidates:
            # Ship notice documents without recreating development directories
            # such as external/. Their original path remains visible in the name.
            notice_name = "__".join(source.relative_to(source_root).parts)
            if notice_name.casefold() in notice_names:
                raise RuntimeError(f"colliding runtime notice names for {name}: {notice_name}")
            notice_names.add(notice_name.casefold())
            destination = stage / "licenses" / name / notice_name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)


def _write_runtime_manifest(stage: Path, project: dict[str, object], target: str) -> None:
    files = []
    for path in sorted(item for item in stage.rglob("*") if item.is_file()):
        relative = path.relative_to(stage).as_posix()
        if relative != "runtime-manifest.json":
            files.append({"path": relative, "size": path.stat().st_size, "sha256": sha256(path)})
    payload = {
        "schema_version": 1,
        "foundation_version": project["foundation_version"],
        "application": project["identity"],
        "platform": target,
        "files": files,
    }
    (stage / "runtime-manifest.json").write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def desktop_package_stage_path(
    root: Path, project: dict[str, object], target: str
) -> Path:
    identity = project["identity"]
    if not isinstance(identity, dict):
        raise RuntimeError("project identity is invalid")
    return (
        root
        / "build"
        / "packages"
        / target
        / f"{identity['slug']}-{identity['version']}"
    )


def stage_desktop_package(
    root: Path,
    project: dict[str, object],
    target: str,
    player: Path,
    stage_override: Path | None = None,
) -> Path:
    stage = (
        stage_override
        if stage_override is not None
        else desktop_package_stage_path(root, project, target)
    )
    if stage.exists() or stage.is_symlink():
        raise RuntimeError(f"package assembly requires a new candidate directory: {stage}")
    stage.mkdir(parents=True, exist_ok=False)
    destination = stage / player.name
    shutil.copy2(player, destination)
    destination.chmod(destination.stat().st_mode | 0o111)
    assets = player.parent / "assets"
    if not assets.is_dir():
        raise RuntimeError(f"cooked runtime assets are missing: {assets}")
    shutil.copytree(assets, stage / "assets")
    shutil.copy2(root / "THIRD_PARTY_NOTICES.md", stage / "THIRD_PARTY_NOTICES.md")
    features = tuple(str(value) for value in project.get("features", []))
    _copy_runtime_licenses(root, stage, features)
    _write_runtime_manifest(stage, project, target)
    return stage


def publish_desktop_package(
    candidate_stage: Path,
    candidate_archive: Path,
    final_stage: Path,
    final_archive: Path,
) -> None:
    """Replace the visible package directory and archive as one recoverable pair."""

    if candidate_stage.is_symlink() or not candidate_stage.is_dir():
        raise RuntimeError(f"desktop package candidate is invalid: {candidate_stage}")
    if candidate_archive.is_symlink() or not candidate_archive.is_file():
        raise RuntimeError(f"desktop package archive candidate is invalid: {candidate_archive}")
    if final_stage.is_symlink() or (final_stage.exists() and not final_stage.is_dir()):
        raise RuntimeError(f"desktop package destination is invalid: {final_stage}")
    if final_archive.is_symlink() or (final_archive.exists() and not final_archive.is_file()):
        raise RuntimeError(f"desktop package archive destination is invalid: {final_archive}")
    if final_stage.parent != final_archive.parent:
        raise RuntimeError("desktop package directory and archive must share one parent")

    transaction = candidate_stage.parent
    previous_stage = transaction / "previous-stage"
    previous_archive = transaction / "previous-archive"
    if previous_stage.exists() or previous_archive.exists():
        raise RuntimeError("desktop package transaction backup paths already exist")

    moved_previous_stage = False
    moved_previous_archive = False
    installed_stage = False
    installed_archive = False
    try:
        final_stage.parent.mkdir(parents=True, exist_ok=True)
        if final_stage.exists():
            final_stage.replace(previous_stage)
            moved_previous_stage = True
        if final_archive.exists():
            final_archive.replace(previous_archive)
            moved_previous_archive = True
        candidate_stage.replace(final_stage)
        installed_stage = True
        candidate_archive.replace(final_archive)
        installed_archive = True
    except OSError:
        if installed_archive and final_archive.exists():
            final_archive.unlink()
        if installed_stage and final_stage.exists():
            shutil.rmtree(final_stage)
        if moved_previous_stage and previous_stage.exists():
            previous_stage.replace(final_stage)
        if moved_previous_archive and previous_archive.exists():
            previous_archive.replace(final_archive)
        raise
    if previous_stage.exists():
        shutil.rmtree(previous_stage)
    if previous_archive.exists():
        previous_archive.unlink()


def deterministic_zip(source: Path, archive: Path) -> None:
    archive.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for path in sorted(item for item in source.rglob("*") if item.is_file()):
            relative = (Path(source.name) / path.relative_to(source)).as_posix()
            info = zipfile.ZipInfo(relative, date_time=(2020, 1, 1, 0, 0, 0))
            mode = 0o755 if os.access(path, os.X_OK) else 0o644
            info.external_attr = mode << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            output.writestr(info, path.read_bytes())
