#!/usr/bin/env python3
"""Compare the Archero-inspired UFO adaptation, not Archero gameplay or revenue."""
import argparse
import gzip
import hashlib
import json
import multiprocessing
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from endless_balance.archero import Inputs, Journey, Trial, account_state, execute_journey, execute_run, profile_plan
from simulate_endless_balance import percentile
from simulate_short_progression import summarize

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / "design/balance"
POLICIES = ("Hybrid", "Collector", "Gunship", "Defender", "Random")


class Study:
    def __init__(self, inputs, executor, raw):
        self.inputs, self.executor, self.raw, self.count = inputs, executor, raw, 0

    def record(self, phase, row):
        self.raw.write(json.dumps(dict(phase=phase, **row), separators=(",", ":")) + "\n")
        self.count += 1

    def group(self, phase, plan, state, policy, seeds, **options):
        inputs = Inputs(self.inputs.spec, self.inputs.world, plan)
        options = dict({"horizon": 1800}, **options)
        tasks = [Trial(inputs, account_state(plan, state), policy, seed, options) for seed in seeds]
        rows = list(self.executor.map(execute_run, tasks))
        for row in rows:
            self.record(phase, row)
        summary = summarize(rows, plan)
        summary["median_installed_modules"] = percentile([len(row["hardware"]) for row in rows], .5)
        summary["most_common_roster_pct"] = round(100 * max(sum(tuple(sorted(r["hardware"])) == tuple(sorted(row["hardware"])) for r in rows) for row in rows) / len(rows), 1)
        return rows, summary


def compare(study, count):
    result = {}
    for name in study.inputs.plan["profiles"]:
        plan = profile_plan(study.inputs.plan, name)
        result[name] = {}
        for state in ("fresh", "maxed"):
            result[name][state] = {}
            for policy in POLICIES:
                _, summary = study.group(f"comparison_{name}_{state}", plan, state, policy, range(50000, 50000 + count))
                result[name][state][policy] = summary
            print("COMPARE", name, state, {p:r["median_observed_minutes"] for p,r in result[name][state].items()}, flush=True)
    return result


def holdout(study, count):
    result = {}
    paired = {}
    states = ("fresh", "stats_only", "unlocks_only", "maxed")
    for state in states:
        result[state], paired[state] = {}, {}
        for policy in POLICIES:
            rows, summary = study.group(f"holdout_{state}", study.inputs.plan, state, policy, range(60000, 60000 + count))
            result[state][policy], paired[state][policy] = summary, rows
            print("HOLDOUT", state, policy, summary["median_observed_minutes"], flush=True)
    effects = {}
    for policy in POLICIES:
        fresh = paired["fresh"][policy]
        effects[policy] = {state: {
            "median_paired_lifetime_change_minutes": percentile([(b["seconds"]-a["seconds"])/60 for a,b in zip(fresh,paired[state][policy])], .5),
            "shorter_runs_pct": round(100 * sum(b["seconds"] < a["seconds"] for a,b in zip(fresh,paired[state][policy])) / count, 1)} for state in states[1:]}
    return {"states": result, "paired_effects_against_fresh": effects}


def sensitivity(study, count):
    result = {}
    cases = {"novice":{"skill":"novice"}, "expert":{"skill":"expert"},
             "targets_low":{"density":.65}, "targets_high":{"density":1.35},
             "revive":{"revive":True}, "half_second":{"step":.5}}
    for case, options in cases.items():
        result[case] = {}
        for state in ("fresh", "maxed"):
            result[case][state] = {}
            for policy in ("Hybrid", "Collector", "Gunship"):
                _, summary = study.group(f"sensitivity_{case}_{state}", study.inputs.plan, state, policy,
                                         range(70000, 70000 + count), **options)
                result[case][state][policy] = summary
        print("SENSITIVITY", case, flush=True)
    return result


def journeys(study, players, attempts):
    result = {}
    for scenario in ("no_ads", "revive", "credits_50", "both_50", "both_100"):
        result[scenario] = {}
        for policy, skill in (("Hybrid","regular"),("Collector","regular"),("Gunship","regular"),("Hybrid","novice")):
            label = f"{policy}_{skill}"
            tasks = [Journey(study.inputs, player, attempts, policy, skill, scenario) for player in range(players)]
            accounts = []
            for account, rows in study.executor.map(execute_journey, tasks):
                accounts.append(account)
                for row in rows:
                    study.record(f"journey_{scenario}_{label}", row)
            result[scenario][label] = {
                "accounts": players, "attempts_each": attempts,
                "median_first_purchase_attempt": percentile([a["first_purchase"] for a in accounts if a["first_purchase"]], .5),
                "cap_reached_pct": round(100 * sum(a["cap_at"] is not None for a in accounts) / players, 1),
                "median_cap_attempt_among_reached": percentile([a["cap_at"] for a in accounts if a["cap_at"]], .5),
                "all_unlocks_reached_pct": round(100 * sum(a["all_unlocks_at"] is not None for a in accounts) / players, 1),
                "median_all_unlocks_attempt_among_reached": percentile([a["all_unlocks_at"] for a in accounts if a["all_unlocks_at"]], .5),
                "median_active_minutes": percentile([a["active_minutes"] for a in accounts], .5),
                "median_assumed_ads": percentile([a["assumed_ads"] for a in accounts], .5),
                "observations": accounts}
            summary = result[scenario][label]
            print("JOURNEY", scenario, label, summary["median_cap_attempt_among_reached"], summary["cap_reached_pct"], flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compare", action="store_true")
    parser.add_argument("--runs", type=int, default=200)
    parser.add_argument("--players", type=int, default=12)
    parser.add_argument("--attempts", type=int, default=40)
    parser.add_argument("--stress-runs", type=int, default=60)
    parser.add_argument("--workers", type=int, default=6)
    args = parser.parse_args()
    if min(args.runs,args.players,args.attempts,args.stress_runs,args.workers) < 1:
        parser.error("all counts must be positive")
    inputs = Inputs(json.loads((DIRECTORY / "endless-simulation-input.json").read_text()),
                    json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text()),
                    json.loads((DIRECTORY / "archero-ufo-system.json").read_text()))
    inputs.plan = profile_plan(inputs.plan, inputs.plan["selected_profile"])
    paths = [Path(__file__), ROOT / "tools/simulate_short_progression.py", ROOT / "tools/simulate_endless_balance.py",
             ROOT / "tools/check_endless_upgrades.py", *sorted((ROOT / "tools/endless_balance").glob("*.py")),
             DIRECTORY / "archero-ufo-system.json", DIRECTORY / "endless-simulation-input.json", DIRECTORY / "endless-encounter-assumptions.json"]
    hashes = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    path = DIRECTORY / ("archero-comparison-results.json" if args.compare else "archero-ufo-results.json")
    with ProcessPoolExecutor(max_workers=args.workers, mp_context=multiprocessing.get_context("spawn")) as executor, gzip.open(path.with_suffix(".jsonl.gz"), "wt") as raw:
        study = Study(inputs, executor, raw)
        result = {"scope":__doc__,"arguments":vars(args),"plan_snapshot":inputs.plan,"source_sha256":hashes,
                  "seed_ranges":{"comparison":[50000,50000+args.runs-1],"holdout":[60000,60000+args.runs-1],
                                 "sensitivity":[70000,70000+args.stress_runs-1],"journey":"300000 + player*1000 + attempt"}}
        if args.compare:
            result["comparison"] = compare(study,args.runs)
        else:
            result.update(holdout=holdout(study,args.runs), sensitivity=sensitivity(study,args.stress_runs),
                          journeys=journeys(study,args.players,args.attempts))
        result["simulated_runs"] = study.count
    assert all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest() == digest for p,digest in hashes.items()), "Source changed during study"
    path.write_text(json.dumps(result,indent=2) + "\n")
    print("DONE",study.count,path,flush=True)


if __name__ == "__main__":
    main()
