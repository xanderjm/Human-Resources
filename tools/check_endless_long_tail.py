#!/usr/bin/env python3
"""Exploratory 60-minute follow-up for gentler research bundles.

Uses the same seeds as the original long-tail comparison, not a fresh holdout.
"""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

from simulate_endless_balance import DIRECTORY, ROOT, Study, TUNINGS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs", type=int, default=20)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    spec = json.loads((DIRECTORY / "endless-simulation-input.json").read_text())
    world = json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text())
    names = ("moderate_15", "moderate_20")
    for name, every in zip(names, (15, 20)):
        TUNINGS[name] = {"ordinary_owned_upgrades": True,
                        "research_bundle_every_levels": every, "rerolls": 2}
    result = {"scope": __doc__, "seeds": [30000, 30000 + args.runs - 1],
              "tunings": {name: TUNINGS[name] for name in names}, "results": {}}
    path = DIRECTORY / "endless-long-tail-results.json"
    with gzip.open(path.with_suffix(".jsonl.gz"), "wt") as raw:
        study = Study(spec, world, raw)
        for name in names:
            result["results"][name] = {}
            for policy in ("Hybrid", "Collector", "Gunship", "Defender"):
                _, summary = study.group("60min_followup", name, policy,
                                         range(30000, 30000 + args.runs), horizon=3600)
                result["results"][name][policy] = summary
                print(name, policy, summary["median_observed_minutes"],
                      summary["survival_at_60min_pct"], flush=True)
        result["simulated_runs"] = study.count
    paths = [Path(__file__), ROOT / "tools/simulate_endless_balance.py",
             ROOT / "tools/check_endless_upgrades.py",
             *sorted((ROOT / "tools/endless_balance").glob("*.py")),
             DIRECTORY / "endless-simulation-input.json",
             DIRECTORY / "endless-encounter-assumptions.json"]
    result["source_sha256"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in paths}
    path.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
