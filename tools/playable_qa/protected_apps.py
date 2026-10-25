"""Read-only bundle protection for isolated native review tools."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import re
import stat


def _walk_error(error: OSError) -> None:
    raise error


def discover_apps(roots: list[Path]) -> set[Path]:
    bundles = set()
    for root in roots:
        try:
            mode = root.stat().st_mode
        except FileNotFoundError:
            continue
        if not stat.S_ISDIR(mode):
            raise ValueError(f"Protected-app discovery root is not a directory: {root}")
        for directory, names, _ in os.walk(root, onerror=_walk_error):
            for name in names[:]:
                if name.endswith(".app"):
                    bundles.add(Path(directory) / name)
                    names.remove(name)
    return bundles


def bundle_snapshot(path: Path) -> dict:
    if path.is_symlink() or not path.is_dir() or path.suffix != ".app":
        raise ValueError(f"Protected app must be an existing ordinary .app directory: {path}")
    files, links = {}, {}
    for directory, names, filenames in os.walk(path, onerror=_walk_error):
        for name in names[:] + filenames:
            entry = Path(directory) / name
            relative = entry.relative_to(path).as_posix()
            if entry.is_symlink():
                links[relative] = os.readlink(entry)
                if name in names:
                    names.remove(name)
            elif entry.is_file():
                files[relative] = hashlib.sha256(entry.read_bytes()).hexdigest()
            elif not stat.S_ISDIR(entry.stat().st_mode):
                raise ValueError(f"Unsupported entry in protected app: {entry}")
    if not files:
        raise ValueError(f"Protected app contains no ordinary files: {path}")
    return {"files": dict(sorted(files.items())), "symlinks": dict(sorted(links.items()))}


def canonical_inventory(path: Path) -> dict:
    document = json.loads(path.read_text())
    if not isinstance(document, dict) or document.get("schema") != 1 or not isinstance(document.get("bundles"), list):
        raise ValueError("Protected-app manifest requires schema 1 and a bundles array")
    result = {}
    for entry in document["bundles"]:
        if not isinstance(entry, dict) or not isinstance(entry.get("path"), str):
            raise ValueError("Each protected-app manifest entry requires a path")
        app = Path(entry["path"])
        app = (path.parent / app).absolute() if not app.is_absolute() else app
        app = Path(os.path.abspath(app))
        if str(app) in result or app.suffix != ".app":
            raise ValueError(f"Duplicate or invalid protected app path: {app}")
        files, links = entry.get("files"), entry.get("symlinks", {})
        if not isinstance(files, dict) or not files or not isinstance(links, dict):
            raise ValueError(f"Protected app requires file hashes and optional symlink targets: {app}")
        for name, value in {**files, **links}.items():
            parts = Path(name).parts if isinstance(name, str) else ()
            if not parts or Path(name).is_absolute() or ".." in parts or "\\" in name or not isinstance(value, str):
                raise ValueError(f"Unsafe protected-app manifest entry: {name!r}")
        if files.keys() & links.keys() or any(not re.fullmatch(r"[0-9a-f]{64}", value) for value in files.values()):
            raise ValueError(f"Invalid or overlapping protected-app file hashes: {app}")
        result[str(app)] = {"files": files, "symlinks": links}
    return result


class ProtectedApps:
    """Snapshot all local bundles, optionally requiring a complete canonical set.

    Default roots are always inspected; explicit paths extend protection rather
    than disabling discovery. Bundle code is never executed; saves are untouched.
    """

    def __init__(self, roots: list[Path], extra: list[Path], output: Path, manifest: Path | None = None):
        self.roots = [root.absolute() for root in roots]
        self.extra = {Path(os.path.abspath(path)) for path in extra}
        self.manifest = manifest.resolve() if manifest else None
        self.manifest_sha256 = hashlib.sha256(self.manifest.read_bytes()).hexdigest() if manifest else None
        canonical = canonical_inventory(self.manifest) if manifest else None
        self.paths = discover_apps(self.roots) | self.extra
        if canonical is not None:
            self.paths |= {Path(path) for path in canonical}
        output = output.resolve()
        if any(parent.suffix == ".app" for parent in (output, *output.parents)) or any(output.is_relative_to(path.resolve()) for path in self.paths):
            raise ValueError("Review output must be outside every protected app bundle")
        self.before = {str(path): bundle_snapshot(path) for path in sorted(self.paths)}
        if canonical is not None and canonical != self.before:
            raise ValueError("Protected-app canonical manifest differs from the complete discovered/explicit inventory")

    def document(self) -> dict:
        return {"schema": 1, "bundles": [{"path": path, **tree} for path, tree in self.before.items()]}

    def verify(self) -> dict:
        errors, after = [], {}
        try:
            current = discover_apps(self.roots) | self.extra | self.paths
        except (OSError, ValueError) as error:
            current = self.paths
            errors.append(str(error))
        for path in sorted(current):
            try:
                after[str(path)] = bundle_snapshot(path)
            except (OSError, ValueError) as error:
                errors.append(str(error))
        if self.manifest:
            try:
                if hashlib.sha256(self.manifest.read_bytes()).hexdigest() != self.manifest_sha256:
                    errors.append("Canonical protected-app manifest changed during review")
            except OSError as error:
                errors.append(str(error))
        return {"unchanged": not errors and self.before == after,
                "before_bundle_count": len(self.before), "after_bundle_count": len(after),
                "discovery_roots": [str(path) for path in self.roots],
                "canonical_manifest": str(self.manifest) if self.manifest else None,
                "canonical_manifest_sha256": self.manifest_sha256,
                "before": self.document(), "after": {"schema": 1, "bundles": [{"path": path, **tree} for path, tree in after.items()]},
                "errors": errors,
                "scope": "Read-only file/symlink inventory; no app launch, UI action or save access. Absence of previews is supported; bundles created/deleted during review fail protection."}
