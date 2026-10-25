"""Explicit GitHub publication for an independent generated game."""

from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess

from game_support.repository import git_root


def _run(command: tuple[str, ...], root: Path, capture: bool = False) -> str:
    print("+ " + " ".join(command), flush=True)
    completed = subprocess.run(
        command,
        cwd=root,
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.STDOUT if capture else None,
    )
    return completed.stdout.strip() if capture and completed.stdout else ""


def publish_to_github(root: Path, repository: str, visibility: str) -> None:
    if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repository):
        raise RuntimeError("repository must use the GitHub owner/name form")
    repository_root = git_root(root)
    if repository_root is None:
        raise RuntimeError("this generated game was created with --no-git; initialize and commit it first")
    if repository_root != root.resolve():
        raise RuntimeError(f"project is inside repository {repository_root}; publish from that repository explicitly")
    if shutil.which("gh") is None:
        raise RuntimeError("GitHub CLI 'gh' is required; install it and authenticate before publishing")
    if _run(("git", "status", "--porcelain"), root, capture=True):
        raise RuntimeError("commit or discard local changes before publishing the repository")
    remotes = _run(("git", "remote"), root, capture=True).splitlines()
    if "origin" in remotes:
        raise RuntimeError("remote 'origin' already exists; publication refuses to replace it")
    _run(("gh", "auth", "status"), root)
    _run(("gh", "repo", "create", repository, "--source", str(root), "--remote", "origin",
        f"--{visibility}", "--push"), root)
