#!/usr/bin/env python3
"""Paired follow-up to the main study: gentler repeatable research growth."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path

from simulate_endless_balance import DIRECTORY, Study, TUNINGS


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs", type=int, default=150)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    spec = json.loads((DIRECTORY / "endless-simulation-input.json").read_text())
    world = json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text())
    TUNINGS["moderate_15"] = {"ordinary_owned_upgrades":True,"research_bundle_every_levels":15,"rerolls":2}
    TUNINGS["moderate_20"] = {"ordinary_owned_upgrades":True,"research_bundle_every_levels":20,"rerolls":2}
    names = ("baseline", "combined", "moderate_15", "moderate_20")
    policies = ("Hybrid", "Collector", "Gunship", "Storm", "Score", "Random")
    result = {"scope":"Follow-up model comparison after the initial study; not a fresh confirmatory holdout or proof of fun.",
              "seeds":[11000,11000 + args.runs - 1],"tunings":{n:TUNINGS[n] for n in names},"results":{}}
    path = DIRECTORY / "endless-research-tier-results.json"
    with gzip.open(path.with_suffix(".jsonl.gz"), "wt") as raw:
        study = Study(spec, world, raw)
        for name in names:
            result["results"][name] = {}
            for policy in policies:
                _, summary = study.group("research_tier_followup", name, policy, range(11000,11000 + args.runs))
                result["results"][name][policy] = summary
            print(name,{p:(s["survival_at_30min_pct"],s["late_low_impact_offers_pct"]) for p,s in result["results"][name].items()},flush=True)
        result["simulated_runs"] = study.count
    result["source_sha256"] = {str(p.relative_to(DIRECTORY.parents[1])):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__), *sorted((Path(__file__).parent / "endless_balance").glob("*.py")),DIRECTORY / "endless-simulation-input.json",DIRECTORY / "endless-encounter-assumptions.json"]}
    path.write_text(json.dumps(result,indent=2) + "\n")
    print("DONE", study.count, path,flush=True)


if __name__ == "__main__":
    main()
