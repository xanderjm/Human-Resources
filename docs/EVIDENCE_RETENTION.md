# Local verification artifact retention

The owner authorized cleanup of unused duplicates, old screenshots and videos
on 26 October 2026. Keep the evidence needed to assess the current game without
retaining every historical generated build or capture.

- Preserve source art, models, fonts, production inventory, balance inputs and
  authoring provenance. Preserve current test applications and saves.
- Keep the latest qualified artifact for each platform, its source binding,
  reports, logs, package audits and GPU smoke evidence. An older platform epoch
  remains necessary when it is that platform's latest actual qualification.
- During an iteration, keep the current candidate and the immediate baseline
  needed for a real comparison or regression. Preserve a minimal reproducible
  failure when it explains a correction. Keep relevant runtime inputs coherent.
- Keep selected final UI/store frames and one useful motion film with a few
  keys/temporal samples. Stream video; avoid accumulated raw frame dumps and
  duplicate comparison galleries. Rendered images remain unqualified until
  actually reviewed.
- Remove obsolete generated build outputs, superseded packages and redundant
  historical captures after checking current consumers and active processes.
  Keep compact historical reports/logs/manifests and a removal ledger. A dated
  successful command remains historical evidence; a pruned artifact must never
  be represented as still available for inspection or current qualification.
- Use an exact reviewed file list, recheck file identity and protect current
  inputs/applications/saves. Do not use broad directory deletion across mixed
  source/evidence trees. Do not delete a file required by an active verifier,
  continuation fixture, platform provenance record or comparison.

The first cleanup is recorded in
`build/evidence/historical-cleanup-game10/RESULT.json` and its retention ledger.
It removes 2,125 obsolete generated files, preserves 99 selected final UI/store
frames and leaves the current 19 source owners, 68 native build inputs and 33 protected
application bundles intact. The observed free-space increase is 2.43 GB;
APFS sharing and concurrent system activity affect physical-space measurements.

The 27 October owner-requested follow-up is recorded in
`build/evidence/space-cleanup-20261027/RESULT-FINAL.json`, with the exact removal
list and retained PNG hashes in `PLAN.json`. It removes 3,214 disposable files
(3.08 GB logical): pixel-identical raw screenshot duplicates, rebuildable Android
intermediates and the verified installer for the retained Blender installation.
All 77,345 non-target regular files remain unchanged. Unique films/images, apps,
saves, platform packages, reports/logs, source and authoring provenance remain.
Removed raw screenshot paths now refer to intentionally retired duplicates; the
named lossless PNGs retain the exact pixels. Deleted Android intermediates must
be regenerated before reuse. Whole-volume free-space changes include concurrent
activity and are not attributed wholly to this cleanup.

Hosted evidence remains subject to the repository's manual-workflow and CI
artifact requirements. This local retention policy does not weaken a test or
turn missing device, performance, signing or store evidence into a pass.
