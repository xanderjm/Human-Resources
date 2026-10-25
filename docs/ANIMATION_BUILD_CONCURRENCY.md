# Animation producer concurrency

The producer is implemented and source-frozen. Exact source/export hashes and
qualification paths are in `build/animation/walk-v3/producer-freeze.json`.
The six final GLBs are byte-identical to the walk review. Their reports now bind
the current producer tools; no motion recipe, source model or native contract
changed during the concurrency fix.

`tools/transaction_character_animation.py` owns Python-standard-library locks,
same-filesystem staging, file publication and crash rollback. Locks use
`fcntl.flock` on macOS/Linux and one-byte `msvcrt.locking` on Windows. Permanent
lock inodes live in a host temporary registry keyed by resolved output path,
outside packaged directories. OS process death releases ownership. Bounded waits
fail with the lock path. Staging and backups are siblings of the destination;
they cannot become cooked assets.

A publisher backs up the prior files, atomically writes a recovery journal,
replaces complete staged files and commits by removing the journal. The next
lock owner restores the entire prior set after an interrupted publication before
allowing readers in. Recovery copies backups, so an interrupted recovery can
itself be retried. This guarantees consistency for participating readers, which
must acquire the same output lock. It is not a lock-free multi-file filesystem
snapshot or a claim about sudden power loss durability.

`build_character_animation.py` locks its destination and builds into private
staging. Source hashes are captured before generation and checked again before
publication. Independent validation runs in the configured NumPy/Pillow art
Python via `--validator-python`, never by adding foreign packages to Blender.
The default executable is `python3`; pass the actual art interpreter explicitly
when it differs. Failure leaves the prior published set intact.

`cook_character_animation.py` holds the export lock across freshness, generation,
validation and all six source reads. A child builder locks only its unique
staging destination, so there is no recursive lock/bypass flag. Cook output locks
are acquired after the export lock, in stable path order. HRA/HRM pairs and their
provenance publish together under those locks. `--export-root` permits isolated
qualification; the production default remains `build/animation`.
Standalone validators also lock their selected roots and atomically publish
reports. Freshness retains exact source/tool/config/GLB hash checks. Infantry
files are absent from the six-body authoring closure. The helper's filename is
covered by the existing CMake tool glob; no CMake/native edits were made.

Ten cheap tests cover concurrent fake producers/readers, failure before/within
publication, killed lock owners, crash/retry, source changes, timeouts, new-file
rollback, infantry freshness isolation and package-directory hygiene. An actual
cold build passes 60 clips / 2,841 keys plus 60 Hz walk contact checks. Two actual
simultaneous fresh cooks then pass against the final shared set. The resulting
native directory contains exactly six HRA/HRM pairs and provenance, whose hashes
match every output. Tests/logs are listed in the freeze record. Windows lock
semantics are implemented but have not been executed on Windows.

The root CMake broad product-authoring glob can still rerun the product cook for
unconsumed infantry changes. That does not make the six GLBs stale or force an
artist rebuild. Narrowing that broader build dependency list belongs to its
integration owner. A future boundary using explicitly promoted reviewed GLBs
could remove ordinary platform-build authoring, but is not part of this bounded
race fix. Direct tools reading a multi-file published set must use the lock or a
copied snapshot; long review renders should freeze their input hashes as this
walk review does.
