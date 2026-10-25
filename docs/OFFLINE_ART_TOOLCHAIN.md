# Offline art toolchain

The content cook already uses NumPy for source GLB transforms/skin validation,
Pillow for approved PNG/font atlases, and Blender for recipe-driven skeletal
exports. Reimplementing these mature authoring operations in the C++ runtime or
a new importer would expand the shipping boundary. This contract makes those
existing build dependencies explicit; it adds no runtime library or new art.

| Tool | Exact version | Licence | Authority |
| --- | --- | --- | --- |
| NumPy | 2.3.5 | BSD-3-Clause; wheel includes its bundled notices | [Official release](https://pypi.org/project/numpy/2.3.5/) |
| Pillow | 12.3.0 | MIT-CMU; wheel includes its bundled notices | [Official release](https://pypi.org/project/pillow/12.3.0/) |
| Blender | 4.5.15 LTS | GPL-3.0-or-later; retain archive dependency notices | [Official licence](https://www.blender.org/about/license/), [archive/checksums](https://download.blender.org/release/Blender4.5/blender-4.5.15.sha256) |

`tools/art-requirements.txt` pins all official wheel hashes for these releases.
`tools/art-toolchain.json` pins the official Blender archive hash per host.
Healthy CPython 3.11–3.14 with working `pyexpat`/XML is supported for offline cooking. The manual workflows use
3.13.5. Blender uses its own bundled Python/NumPy for authoring and invokes the
selected external art Python for Pillow-dependent validation. Neither is copied
to a mobile package.

## Prepare and discover

Normal fresh setup includes these tools:

```sh
python3 tools/project.py bootstrap
```

To prepare only art tools, without rebuilding SDL/shader host tools:

```sh
python3 tools/bootstrap_art_tools.py --download-blender
python3 tools/bootstrap_art_tools.py --check
```

On Windows use `python` in place of `python3`. This uses `venv` under
`build/tools/art-python` and pip's hash checking, binary wheels only and no
dependency resolution. No activation, global install or shell execution-policy
change is needed. An existing matching Blender is reused. Otherwise the optional
download verifies SHA-256 before extracting a ZIP on Windows, tar.xz on Linux or
read-only DMG plus `ditto` on macOS into `build/tools/blender`.

Python selection is: strict `GAME_ART_PYTHON`, then the scoped virtual environment,
then the current interpreter if it has exactly the pinned packages. Blender
selection is: explicit cooker `--blender`, strict `GAME_BLENDER`, the scoped
platform path, `blender` on PATH, and `/Applications/Blender.app` on macOS.
Invalid explicit selections fail immediately. Private Codex cache paths are not
searched. Paths containing spaces are passed as individual subprocess arguments.

Both animation cookers discover Blender only when exports are absent or stale.
Their existing locks, private staging, source checks, validator interpreter and
manifest-last publication remain in place. A warm build does not require a
Blender launch merely to validate its already source-bound exports. No ignored
animation export is a required fresh-checkout input.

Linux x86_64, Windows x86_64/arm64 and macOS x86_64/arm64 archive selections are
recorded. Other authoring architectures fail with an actionable error; mobile
targets cook on a supported desktop host. The iOS shell reaches normal bootstrap
through `build-host-tools.sh`; Android uses the same `project.py` bootstrap.
All six hosted workflows remain manual and prepare this toolchain explicitly.
No hosted run was dispatched by this change.

## Cost, maintenance and qualification

These tools add **zero runtime package bytes**. Local wheel downloads are roughly
10 MB on the current macOS arm64/Python 3.12 host; platform wheels and extracted
Blender size vary. Blender is a substantial development-only download. Scoped
tools (111 MiB art Python and 814 MiB Blender on the current host), exports, validation captures and provenance remain ignored under `build/`.
`build/evidence/art-toolchain.json` records the chosen paths, package versions and
contract hashes; archive integrity is checked before publication.

The product build-tooling owner owns updates. Change one pin at a time, refresh
official hashes/notices and rerun cold source exports, source/skin/socket parity,
atlas/icon contracts, aggregate build and platform package checks. Replacement
must preserve those same tests. Focused discovery tests cover strict overrides,
version rejection, all archive mappings, hash failure and both absent-export
producer/validator handoffs without touching shared exports. Host download/setup
and a handoff test are not Windows/Linux execution evidence; those profiles still
require their own real fresh builds and packages.

Mechanism references: [Python venv](https://docs.python.org/3/library/venv.html),
[pip hash-checked binary installs](https://pip.pypa.io/en/stable/topics/secure-installs/).

Current-host caveat: the default Homebrew Python 3.14.6 has a broken `pyexpat`
link to the system expat library. The build-interpreter prerequisite now imports
`pyexpat` explicitly and selects the existing healthy Python 3.13.5; the scoped
art Python 3.12.14 also passes. Use a healthy interpreter or
`GAME_PYTHON_EXECUTABLE` for setup; this change does not repair system Python or
qualify every Python version on this host.
