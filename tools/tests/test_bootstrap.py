"""Bootstrap contracts that do not require downloads or platform SDKs."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from game_support.host_tools import bootstrap_lock, temporary_parent, working_tool
from game_support.repository import git_root
from game_support.toolchains import android_environment, java_17_executable


class BootstrapContracts(unittest.TestCase):
    def test_exclusive_lock_and_exception_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with bootstrap_lock(root):
                with self.assertRaisesRegex(RuntimeError, "timed out"):
                    with bootstrap_lock(root, timeout=0):
                        self.fail("second lock acquired")
            with self.assertRaises(ValueError):
                with bootstrap_lock(root, timeout=0):
                    raise ValueError("simulate bootstrap failure")
            with bootstrap_lock(root, timeout=0):
                pass

    def test_whitespace_stage_override_rejected(self):
        with tempfile.TemporaryDirectory(prefix="has spaces ") as directory:
            with patch.dict(os.environ, {"GAME_HOST_TOOLS_STAGE_ROOT": directory}):
                with self.assertRaises(RuntimeError):
                    temporary_parent()

    def test_missing_or_broken_tool_is_not_a_cache_hit(self):
        self.assertFalse(working_tool(Path("missing-tool-for-test"), "--help"))
        self.assertTrue(working_tool(Path(sys.executable), "--version"))

    def test_explicit_jdk_does_not_silently_fall_back(self):
        with patch.dict(os.environ, {"JAVA_HOME": "/missing-jdk-for-test"}):
            with self.assertRaises(RuntimeError):
                java_17_executable()

    def test_android_environment_is_scoped(self):
        before = dict(os.environ)
        with patch("game_support.toolchains.java_17_executable", return_value=Path("/jdk17/bin/java")), \
             patch("game_support.toolchains.android_sdk_root", return_value=Path("/android-sdk")):
            environment = android_environment()
        self.assertEqual(dict(os.environ), before)
        self.assertEqual(environment["JAVA_HOME"], "/jdk17")
        self.assertEqual(environment["ANDROID_HOME"], environment["ANDROID_SDK_ROOT"])

    def test_git_discovery_finds_parent_of_nested_project(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            subprocess.run(["git", "init", "--quiet", str(root)], check=True)
            nested = root / "apps/sample"
            nested.mkdir(parents=True)
            self.assertEqual(git_root(nested), root.resolve())


if __name__ == "__main__":
    unittest.main()
