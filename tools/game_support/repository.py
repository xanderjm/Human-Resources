"""Git discovery for generated projects, worktrees and nested project folders."""
from pathlib import Path
import shutil
import subprocess


def git_root(root: Path) -> Path | None:
    if shutil.which("git") is None:
        return None
    result = subprocess.run(["git", "rev-parse", "--show-toplevel"], cwd=root,
        check=False, text=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return Path(result.stdout.strip()).resolve() if result.returncode == 0 else None
