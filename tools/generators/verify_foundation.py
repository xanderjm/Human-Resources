#!/usr/bin/env python3
"""Profile-aware static acceptance audit for a generated repository."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import plistlib
import re
import subprocess
import xml.etree.ElementTree as ET


PUBLIC_PLATFORMS = ("macos", "windows", "linux", "ios", "android")
FEATURES = ("deterministic-authority", "dynamic-text")
COMMON_PATHS = (
    "AGENTS.md",
    "PRODUCT_BRIEF.md",
    "README.md",
    "project.json",
    "foundation.lock.json",
    "CMakeLists.txt",
    "CMakePresets.json",
    ".clang-format",
    ".clang-tidy",
    ".tool-versions",
    ".github/workflows/foundation-quality.yml",
    "dependencies.lock.json",
    "tools/project.py",
    "tools/game.py",
    "tools/game_support/host_tools.py",
    "tools/game_support/repository.py",
    "tools/generators/source_inventory.py",
    "tools/generators/stage_runtime_assets.py",
    "tools/game_support/__init__.py",
    "tools/game_support/evidence.py",
    "tools/game_support/packaging.py",
    "tools/game_support/publishing.py",
    "tools/game_support/toolchains.py",
    "tools/generators/audit_asset_package.py",
    "tools/generators/audit_smoke_report.py",
    "tools/generators/bootstrap_dependencies.py",
    "tools/generators/compile_shaders.py",
    "tools/generators/cook_runtime_assets.py",
    "tools/generators/verify_foundation.py",
    "tools/generators/select_xcode.py",
    "tools/tests/test_tooling.py",
    "scripts/check-cpp-analysis.sh",
    "scripts/check-cpp-format.sh",
    "scripts/check-release-package.sh",
    "scripts/coverage.sh",
    "docs/NATIVE_GAME_PROJECT_BLUEPRINT.md",
    "docs/FOUNDATIONAL_ARCHITECTURE.md",
    "docs/CODE_ORGANIZATION.md",
    "docs/PLATFORM_SUPPORT.md",
    "docs/UI_FOUNDATION.md",
    "docs/DIAGNOSTICS.md",
    "docs/DEVELOPMENT_TOOLING.md",
    "docs/DEPENDENCIES.md",
    "docs/FOUNDATION_ACCEPTANCE.md",
    "docs/AI_DEVELOPMENT_LOG.md",
    "docs/TECHNICAL_DECISIONS.md",
    "shaders/shader_manifest.json",
    "native/tests/platform_adapter_tests.cpp",
    "native/tests/accessibility_tests.cpp",
)
PLATFORM_PATHS = {
    "macos": (".github/workflows/foundation-macos.yml",),
    "windows": (".github/workflows/foundation-windows.yml",),
    "linux": (".github/workflows/foundation-linux.yml",),
    "ios": (
        ".github/workflows/ios.yml",
        "ios/Info.plist.in",
        "ios/LaunchScreen.storyboard",
        "ios/PrivacyInfo.xcprivacy",
        "scripts/build-ios-device.sh",
        "scripts/configure-ios-device.sh",
        "scripts/configure-ios-simulator.sh",
        "scripts/verify-ios.sh",
    ),
    "android": (
        ".github/workflows/android.yml",
        "android/app/build.gradle",
        "android/app/src/main/AndroidManifest.xml",
        "android/gradle/wrapper/gradle-wrapper.jar",
        "android/gradle/verification-metadata.xml",
        "scripts/build-android.sh",
        "scripts/verify-android.sh",
    ),
}


def add_error(errors: list[str], message: str) -> None:
    errors.append(message)


def json_file(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def assert_path_state(
    root: Path,
    relative: str,
    expected: bool,
    errors: list[str],
) -> None:
    exists = (root / relative).exists()
    if expected and not exists:
        add_error(errors, f"required selected-profile path is missing: {relative}")
    if not expected and exists:
        add_error(errors, f"unselected-profile path is present: {relative}")


def audit_tokens(root: Path, errors: list[str]) -> None:
    suffixes = {
        "",
        ".bat",
        ".cmake",
        ".cpp",
        ".mm",
        ".fbs",
        ".gradle",
        ".h",
        ".hlsl",
        ".hlsli",
        ".in",
        ".java",
        ".json",
        ".md",
        ".plist",
        ".properties",
        ".py",
        ".sh",
        ".storyboard",
        ".xml",
        ".yml",
        ".yaml",
    }
    unresolved = re.compile(
        r"__(?:FOUNDATION|GAME|APP|ANDROID|IOS|SIMULATION|PLATFORM|FEATURE|PRODUCT)_[A-Z0-9_]+__"
    )
    for path in root.rglob("*"):
        if (
            not path.is_file()
            or path.suffix.lower() not in suffixes
            or "external" in path.parts
            or "build" in path.parts
            or ".git" in path.parts
        ):
            continue
        match = unresolved.search(path.read_text(encoding="utf-8", errors="replace"))
        if match:
            add_error(errors, f"unresolved foundation token {match.group(0)} in {path.relative_to(root)}")


def audit_project(root: Path, project: dict[str, object], errors: list[str]) -> None:
    if project.get("schema_version") != 2:
        add_error(errors, "project.json schema_version is not 2")
    if project.get("kind") not in ("app", "game"):
        add_error(errors, "project kind must be app or game")
    platforms = project.get("platforms")
    features = project.get("features")
    if not isinstance(platforms, list) or not platforms:
        add_error(errors, "at least one platform must be selected")
        platforms = []
    if not isinstance(features, list):
        add_error(errors, "features must be a list")
        features = []
    if len(platforms) != len(set(platforms)) or any(item not in PUBLIC_PLATFORMS for item in platforms):
        add_error(errors, f"invalid or duplicate public platform selection: {platforms}")
    if len(features) != len(set(features)) or any(item not in FEATURES for item in features):
        add_error(errors, f"invalid or duplicate feature selection: {features}")

    for platform, paths in PLATFORM_PATHS.items():
        for relative in paths:
            assert_path_state(root, relative, platform in platforms, errors)

    identity = project.get("identity")
    if not isinstance(identity, dict):
        add_error(errors, "project identity is missing")
        return
    slug = str(identity.get("slug", ""))
    if not re.fullmatch(r"[a-z][a-z0-9]*(?:_[a-z0-9]+)*", slug):
        add_error(errors, f"invalid generated slug: {slug!r}")
    version = str(identity.get("version", ""))
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)", version):
        add_error(errors, f"invalid generated semantic version: {version!r}")
    for module in ("diagnostics", "ui", "audio", "accessibility", "renderer", "platform", "client"):
        if not (root / "native" / f"{slug}_{module}").is_dir():
            add_error(errors, f"baseline native target source is missing: {slug}_{module}")

    for platform, relative in (
        ("ios", f"native/{slug}_platform/src/ios_accessibility.mm"),
        ("android", f"native/{slug}_platform/src/android_accessibility.cpp"),
    ):
        assert_path_state(root, relative, platform in platforms, errors)

    deterministic_paths = (
        f"native/{slug}_protocol",
        f"native/{slug}_core",
        f"native/{slug}_runtime",
        f"native/{slug}_server",
        "native/tests/deterministic_tests.cpp",
        "docs/DETERMINISTIC_SIMULATION_RULES.md",
    )
    deterministic = "deterministic-authority" in features
    for relative in deterministic_paths:
        assert_path_state(root, relative, deterministic, errors)

    dynamic_text_paths = (f"native/{slug}_text", "native/tests/text_tests.cpp")
    dynamic_text = "dynamic-text" in features
    for relative in dynamic_text_paths:
        assert_path_state(root, relative, dynamic_text, errors)


def audit_dependencies(root: Path, project: dict[str, object], errors: list[str]) -> None:
    lock = json_file(root / "dependencies.lock.json")
    dependencies = lock.get("dependencies")
    if not isinstance(dependencies, list):
        add_error(errors, "dependencies.lock.json dependencies is not a list")
        return
    names = [str(item.get("name")) for item in dependencies if isinstance(item, dict)]
    expected = ["SDL", "SDL_shadercross"]
    features = project.get("features", [])
    if "dynamic-text" in features:
        expected.append("SDL_ttf")
    if "deterministic-authority" in features:
        expected.append("FlatBuffers")
    if names != expected:
        add_error(errors, f"selected dependency list mismatch: expected {expected}, found {names}")
    for dependency in dependencies:
        if not isinstance(dependency, dict):
            add_error(errors, "dependency record is not an object")
            continue
        commit = str(dependency.get("commit", ""))
        if not re.fullmatch(r"[0-9a-f]{40}", commit):
            add_error(errors, f"dependency commit is not an exact SHA-1: {dependency.get('name')}")
        if dependency.get("default") is not True:
            add_error(errors, f"selected dependency is not enabled by default: {dependency.get('name')}")
        checkout = root / str(dependency.get("path", ""))
        if checkout.exists():
            completed = subprocess.run(
                ("git", "rev-parse", "HEAD"),
                cwd=checkout,
                check=True,
                text=True,
                stdout=subprocess.PIPE,
            )
            if completed.stdout.strip() != commit:
                add_error(errors, f"dependency checkout drift: {dependency.get('name')}")


def audit_boundaries(root: Path, project: dict[str, object], errors: list[str]) -> None:
    slug = str(project["identity"]["slug"])
    independent = ("ui", "diagnostics", "audio", "input", "lifecycle", "storage", "assets", "accessibility")
    for module in independent:
        module_root = root / "native" / f"{slug}_{module}"
        if not module_root.exists():
            if module in ("input", "lifecycle", "storage", "assets"):
                add_error(errors, f"required independent capability target is missing: {slug}_{module}")
            continue
        for path in module_root.rglob("*"):
            if path.is_file() and path.suffix in {".h", ".cpp"}:
                content = path.read_text(encoding="utf-8")
                if "SDL3/" in content or re.search(r"\bSDL_[A-Za-z0-9_]+", content):
                    add_error(errors, f"independent module imports SDL: {path.relative_to(root)}")

    if "deterministic-authority" in project.get("features", []):
        core_root = root / "native" / f"{slug}_core"
        forbidden = re.compile(r"SDL|filesystem|thread|chrono|socket|asio|beast|random_device")
        for path in core_root.rglob("*"):
            if path.is_file() and path.suffix in {".h", ".cpp"}:
                match = forbidden.search(path.read_text(encoding="utf-8"))
                if match:
                    add_error(
                        errors,
                        f"deterministic core contains forbidden dependency {match.group(0)}: {path.relative_to(root)}",
                    )


def audit_shaders(root: Path, errors: list[str]) -> None:
    authored_suffixes = {".metal", ".msl", ".glsl", ".vert", ".frag"}
    authored = [
        path.relative_to(root)
        for path in (root / "shaders").rglob("*")
        if path.is_file() and path.suffix.lower() in authored_suffixes
    ]
    if authored:
        add_error(errors, "non-HLSL authored shaders found: " + ", ".join(str(path) for path in authored))
    manifest = json_file(root / "shaders/shader_manifest.json")
    if not isinstance(manifest.get("programs"), list) or not manifest["programs"]:
        add_error(errors, "shader manifest has no programs")


def audit_mobile_orientation(root: Path, project: dict[str, object], errors: list[str]) -> None:
    if not any(platform in project.get("platforms", []) for platform in ("ios", "android")):
        return
    orientation = project.get("platform_settings", {}).get("orientation")
    if orientation not in ("portrait", "landscape"):
        add_error(errors, "project mobile orientation must be portrait or landscape")
        return
    if "ios" in project.get("platforms", []):
        info = plistlib.loads((root / "ios/Info.plist.in").read_bytes())
        expected = (["UIInterfaceOrientationPortrait"] if orientation == "portrait" else
                    ["UIInterfaceOrientationLandscapeLeft", "UIInterfaceOrientationLandscapeRight"])
        for key in ("UISupportedInterfaceOrientations", "UISupportedInterfaceOrientations~ipad"):
            if info.get(key) != expected:
                add_error(errors, f"iOS {key} disagrees with project orientation {orientation}")
        launch = ET.parse(root / "ios/LaunchScreen.storyboard").getroot().find("device")
        if launch is None or launch.get("orientation") != orientation:
            add_error(errors, "iOS launch screen orientation disagrees with project.json")
    if "android" in project.get("platforms", []):
        manifest = ET.parse(root / "android/app/src/main/AndroidManifest.xml")
        expected_android = "portrait" if orientation == "portrait" else "sensorLandscape"
        activity = manifest.getroot().find("application/activity")
        if activity is None or activity.get("{http://schemas.android.com/apk/res/android}screenOrientation") != expected_android:
            add_error(errors, f"Android activity orientation disagrees with project orientation {orientation}")


def audit_android(root: Path, project: dict[str, object], errors: list[str]) -> None:
    if "android" not in project.get("platforms", []):
        return
    manifest = (root / "android/app/src/main/AndroidManifest.xml").read_text(encoding="utf-8")
    for required in ("android.hardware.vulkan.version", 'android:required="true"', 'android:appCategory="game"'):
        if required not in manifest:
            add_error(errors, f"Android manifest contract is missing: {required}")
    wrapper = root / "android/gradle/wrapper/gradle-wrapper.jar"
    expected_wrapper = "3b8a25775a69158b5ad2b1d17a80a88dc7b40a352a73eb27d06c612a7ce68e98"
    if hashlib.sha256(wrapper.read_bytes()).hexdigest() != expected_wrapper:
        add_error(errors, "Gradle wrapper checksum mismatch")
    properties = (root / "android/gradle/wrapper/gradle-wrapper.properties").read_text(encoding="utf-8")
    expected_distribution = "21bd5da2de49b642904077f38c0efdc7778e92b3ed0889392dca61b97e389607"
    values = dict(line.split("=", 1) for line in properties.splitlines()
                  if "=" in line and not line.lstrip().startswith(("#", "!")))
    if (values.get("distributionUrl") != r"https\://services.gradle.org/distributions/gradle-9.8.2-bin.zip" or
            values.get("distributionSha256Sum") != expected_distribution or
            values.get("validateDistributionUrl") != "true"):
        add_error(errors, "Gradle distribution pin/checksum mismatch")
    gitignore = (root / ".gitignore").read_text(encoding="utf-8").splitlines()
    if "/android/app/.cxx/" not in gitignore:
        add_error(errors, "Android native build output is not ignored")


def audit_documents(root: Path, project: dict[str, object], errors: list[str]) -> None:
    platform_text = ", ".join(str(value) for value in project.get("platforms", []))
    feature_values = project.get("features", [])
    feature_text = ", ".join(str(value) for value in feature_values) if feature_values else "none"
    for relative in ("README.md", "AGENTS.md", "docs/FOUNDATIONAL_ARCHITECTURE.md"):
        content = (root / relative).read_text(encoding="utf-8")
        if platform_text not in content:
            add_error(errors, f"selected platform list is missing from {relative}")
        if feature_text not in content:
            add_error(errors, f"selected feature list is missing from {relative}")
    organization = (root / "docs/CODE_ORGANIZATION.md").read_text(encoding="utf-8")
    for required in ("ownership", "not automatic failures", "Tests Follow Ownership"):
        if required not in organization:
            add_error(errors, f"code organization contract is missing: {required}")


def audit_workflows(root: Path, project: dict[str, object], errors: list[str]) -> None:
    workflows = sorted((root / ".github/workflows").glob("*.yml"))
    for workflow in workflows:
        content = workflow.read_text(encoding="utf-8")
        for action in re.findall(r"^\s*- uses:\s*([^\s#]+)", content, re.MULTILINE):
            if re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+@[0-9a-f]{40}", action) is None:
                add_error(errors, f"workflow action is not commit-pinned: {workflow.name}: {action}")
        if "permissions:\n  contents: read" not in content:
            add_error(errors, f"workflow does not declare read-only contents permission: {workflow.name}")
        # Generated workflows use one job nesting level. Runner context is not
        # available while GitHub evaluates a job's environment declaration.
        job_env = re.findall(r"^    env:\n((?:^      .*\n|^\s*\n)*)", content, re.MULTILINE)
        if any(re.search(r"\$\{\{[^}]*\brunner[.\[]", block) for block in job_env):
            add_error(errors, f"runner context is unavailable in job env: {workflow.name}")
        for pin in re.findall(r"uses: actions/setup-java@([0-9a-f]+)\s*# v4\.7\.1", content):
            if pin != "c5195efecf7bdfc987ee8bae7a71cb8b11521c00":
                add_error(errors, f"setup-java pin does not match declared v4.7.1: {workflow.name}")
        if "xcodebuild -downloadComponent" in content:
            if "select_xcode.py" not in content or content.index("select_xcode.py") > content.index("xcodebuild -downloadComponent"):
                add_error(errors, f"select compatible Xcode before requesting components: {workflow.name}")

    quality = (root / ".github/workflows/foundation-quality.yml").read_text(encoding="utf-8")
    for required in ("tools/project.py evidence", "actions/upload-artifact@"):
        if required not in quality:
            add_error(errors, f"quality workflow is missing evidence contract: {required}")

    platform_workflows = {
        "macos": "foundation-macos.yml",
        "windows": "foundation-windows.yml",
        "linux": "foundation-linux.yml",
        "ios": "ios.yml",
        "android": "android.yml",
    }
    for platform_name in project.get("platforms", []):
        workflow = root / ".github/workflows" / platform_workflows[str(platform_name)]
        content = workflow.read_text(encoding="utf-8")
        for required in ("tools/project.py evidence", "actions/upload-artifact@"):
            if required not in content:
                add_error(errors, f"{platform_name} workflow is missing evidence contract: {required}")

    platform_contracts = {
        "macos": ("macos-15", "--smoke-test", "--expected-backend metal", "package --platform macos"),
        "windows": ("windows-2022", "msvc-dev-cmd@", "--smoke-test", "--expected-backend direct3d12", "package --platform windows"),
        "linux": ("ubuntu-24.04", "vulkan-tools", "xvfb-run", "--expected-backend vulkan", "package --platform linux"),
        "ios": ("macos-15", "build-ios-device.sh", "verify-ios.sh"),
        "android": ("ubuntu-24.04", "build-android.sh all", ":app:lintRelease", "verify-android.sh"),
    }
    for platform_name, required_values in platform_contracts.items():
        if platform_name not in project.get("platforms", []):
            continue
        content = (root / ".github/workflows" / platform_workflows[platform_name]).read_text(
            encoding="utf-8"
        )
        for required in required_values:
            if required not in content:
                add_error(errors, f"{platform_name} workflow contract is missing: {required}")


def audit_presets(root: Path, errors: list[str]) -> None:
    presets = json_file(root / "CMakePresets.json")
    configured = {item["name"] for item in presets.get("configurePresets", [])}
    tests = {item["name"]: item for item in presets.get("testPresets", [])}
    for build in presets.get("buildPresets", []):
        test = tests.get(build["name"], {})
        if test.get("configurePreset") not in configured:
            add_error(errors, f"build preset has no valid test preset: {build['name']}")
        if test.get("execution", {}).get("noTestsAction") != "error":
            add_error(errors, f"test preset must reject an empty test suite: {build['name']}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    errors: list[str] = []

    for relative in COMMON_PATHS:
        if not (root / relative).exists():
            add_error(errors, f"required path is missing: {relative}")
    if errors:
        for error in errors:
            print(f"- {error}")
        return 1

    project = json_file(root / "project.json")
    lineage = json_file(root / "foundation.lock.json")
    if lineage.get("foundation_version") != project.get("foundation_version"):
        add_error(errors, "foundation.lock.json and project.json versions disagree")
    for field in ("source_distribution_sha256", "source_template_sha256", "generated_tree_sha256_before_lock"):
        value = lineage.get(field)
        if not isinstance(value, str) or re.fullmatch(r"[0-9a-f]{64}", value) is None:
            add_error(errors, f"foundation.lock.json {field} is not an exact SHA-256")

    audit_tokens(root, errors)
    audit_project(root, project, errors)
    audit_dependencies(root, project, errors)
    audit_boundaries(root, project, errors)
    audit_shaders(root, errors)
    audit_mobile_orientation(root, project, errors)
    audit_android(root, project, errors)
    audit_documents(root, project, errors)
    audit_workflows(root, project, errors)
    audit_presets(root, errors)

    if errors:
        print("foundation static audit failed:")
        for error in errors:
            print(f"- {error}")
        return 1
    print("foundation static audit passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
