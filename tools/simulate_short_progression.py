#!/usr/bin/env python3
"""Paired short-run/account progression experiments; no revenue or retention model."""
import argparse
import copy
import gzip
import hashlib
import json
import multiprocessing
from pathlib import Path
import statistics
from concurrent.futures import ProcessPoolExecutor

from endless_balance.journey import account_journey
from endless_balance.progression import Account, base_credits, run_spec
from endless_balance.short_run import ShortRun, short_world
from simulate_endless_balance import percentile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / "design/balance"
POLICIES = ("Hybrid", "Collector", "Gunship", "Defender", "Storm", "Score", "Random")


def full_account(plan):
    return Account(ranks={key: track["maximum_ranks"] for key, track in plan["permanent"]["tracks"].items()},
                   unlocked={u["id"] for u in plan["unlocks"]})


def summarize(rows, plan):
    reached = [row["first_evolution"] / 60 for row in rows if row["first_evolution"] is not None]
    credits = [base_credits(plan, row) for row in rows]
    intervals = [gap for row in rows for gap in row["pick_intervals"]]
    small = sum(row["low_impact_offers"] for row in rows)
    offers = sum(row["offers"] for row in rows)
    return {"runs": len(rows), "median_observed_minutes": percentile([r["seconds"] / 60 for r in rows], .5),
            "p10_observed_minutes": percentile([r["seconds"] / 60 for r in rows], .1),
            "p90_observed_minutes": percentile([r["seconds"] / 60 for r in rows], .9),
            "survival_1min_pct": round(100 * sum(r["seconds"] >= 60 for r in rows) / len(rows), 1),
            "survival_10min_pct": round(100 * sum(r["seconds"] >= 600 for r in rows) / len(rows), 1),
            "horizon_survivor_pct": round(100 * sum(r["censored"] for r in rows) / len(rows), 1),
            "median_first_pick_seconds": percentile([r["first_pick"] for r in rows if r["first_pick"] is not None], .5),
            "median_pick_interval_seconds": percentile(intervals, .5),
            "median_picks": percentile([r["offers"] for r in rows], .5),
            "evolution_any_pct": round(100 * len(reached) / len(rows), 1),
            "median_first_evolution_minutes_among_reached": percentile(reached, .5),
            "small_offers_pct": round(100 * small / max(1, offers), 1),
            "median_base_credits": percentile(credits, .5),
            "mean_credits_per_active_minute": round(statistics.mean(c / (r["seconds"] / 60) for c, r in zip(credits, rows)), 2),
            "median_revive_extension_seconds": percentile([r["seconds"] - r["revive_at"] for r in rows if r["revive_at"] is not None], .5)}


def simulate_one(task):
    source, world, plan, account, policy, seed, kwargs = task
    options = dict(kwargs)
    expand = options.pop("expand", False)
    spec = run_spec(source, plan, account, policy, expand)
    return ShortRun(spec, short_world(world, plan), plan, seed, policy, **options).play()


class Experiment:
    def __init__(self, spec, world, plan, raw, executor=None):
        self.spec, self.world, self.plan, self.raw = spec, world, plan, raw
        self.executor = executor
        self.count = 0

    def run(self, phase, account, policy, seed, *, plan=None, **kwargs):
        plan = plan or self.plan
        row = simulate_one((self.spec, self.world, plan, account, policy, seed, kwargs))
        self.record(phase, account, plan, row)
        return row

    def record(self, phase, account, plan, row):
        self.raw.write(json.dumps({"phase": phase, "account_ranks": account.ranks,
                                   "available_blueprints": sorted(account.available(plan)), **row}, separators=(",", ":")) + "\n")
        self.count += 1

    def group(self, phase, account, policy, seeds, **kwargs):
        plan = kwargs.pop("plan", self.plan)
        tasks = [(self.spec, self.world, plan, account, policy, seed, kwargs) for seed in seeds]
        execute = self.executor.map if self.executor else map
        rows = list(execute(simulate_one, tasks))
        for row in rows:
            self.record(phase, account, plan, row)
        return rows, summarize(rows, self.plan)


def pilot(study, count):
    result = {}
    for pressure in (.65, .8, 1, 1.25, 1.5):
        plan = copy.deepcopy(study.plan)
        plan["director"]["pressure"] = pressure
        result[str(pressure)] = {}
        for name, account in (("fresh", Account()), ("maxed", full_account(plan))):
            result[str(pressure)][name] = {}
            for policy in ("Hybrid", "Collector", "Gunship", "Random"):
                _, summary = study.group(f"pilot_{pressure}_{name}", account, policy, range(count), plan=plan, horizon=1200)
                result[str(pressure)][name][policy] = summary
            print("PILOT", pressure, name, {p:r["median_observed_minutes"] for p,r in result[str(pressure)][name].items()}, flush=True)
    return result


def holdout(study, count):
    result = {}
    for name, account in (("fresh", Account()), ("maxed", full_account(study.plan))):
        result[name] = {}
        for policy in POLICIES:
            _, result[name][policy] = study.group(f"holdout_{name}", account, policy, range(10000, 10000 + count), horizon=1800)
            print("HOLDOUT", name, policy, result[name][policy]["median_observed_minutes"], flush=True)
    return result


def campaigns(study, count, attempts):
    results = {}
    scenarios = {"no_ads": (False, False), "credit_bonus": (False, True),
                 "revive": (True, False), "both": (True, True)}
    for scenario, (revive, bonus) in scenarios.items():
        results[scenario] = {}
        for policy, skill in (("Hybrid", "regular"), ("Collector", "regular"),
                             ("Gunship", "regular"), ("Hybrid", "novice")):
            key = f"{policy}_{skill}"
            accounts = []
            tasks = [(study.spec, study.world, study.plan, player, attempts, policy,
                      skill, scenario, revive, bonus) for player in range(count)]
            execute = study.executor.map if study.executor else map
            for observation, rows in execute(account_journey, tasks):
                accounts.append(observation)
                for row in rows:
                    study.raw.write(json.dumps(row, separators=(",", ":")) + "\n")
                study.count += len(rows)
            summary = {"players": count, "attempts_each": attempts,
                       "first_purchase_median_attempt": percentile([a["first_purchase"] for a in accounts if a["first_purchase"]], .5),
                       "first_purchase_median_active_minutes": percentile([a["first_purchase_minutes"] for a in accounts if a["first_purchase_minutes"] is not None], .5),
                       "maxed_pct": round(100 * sum(a["capped_at"] is not None for a in accounts) / count, 1),
                       "median_attempt_to_cap_among_reached": percentile([a["capped_at"] for a in accounts if a["capped_at"]], .5),
                       "all_unlocks_pct": round(100 * sum(a["all_unlocks_at"] is not None for a in accounts) / count, 1),
                       "median_attempt_all_unlocks_among_reached": percentile([a["all_unlocks_at"] for a in accounts if a["all_unlocks_at"]], .5),
                       "median_total_active_minutes": percentile([a["active_minutes"] for a in accounts], .5),
                       "median_total_assumed_wall_minutes": percentile([a["wall_minutes_assumed"] for a in accounts], .5),
                       "median_completed_ads_assumed": percentile([a["completed_ads_assumed"] for a in accounts], .5),
                       "median_unspent_credits": percentile([a["unspent_credits"] for a in accounts], .5),
                       "unlock_median_attempt": {u["id"]: percentile([a["unlock_at"][u["id"]] for a in accounts if u["id"] in a["unlock_at"]], .5) for u in study.plan["unlocks"]},
                       "snapshots": {str(n): {"median_run_minutes": percentile([a["snapshots"][str(n)]["run_minutes"] for a in accounts], .5),
                                                "median_purchases": percentile([a["snapshots"][str(n)]["purchases"] for a in accounts], .5)} for n in (1, 2, 5, 10, 20, attempts)},
                       "account_observations": accounts}
            results[scenario][key] = summary
            print("CAMPAIGN", scenario, key, summary["median_attempt_to_cap_among_reached"], summary["maxed_pct"], flush=True)
    return results


def sensitivity(study, count):
    result = {}
    for case, kwargs in {"novice": {"skill": "novice"}, "expert": {"skill": "expert"},
                         "targets_low": {"density": .65}, "targets_high": {"density": 1.35},
                         "expanded_pool": {"expand": True}, "revive": {"revive": True},
                         "half_second": {"step": .5}, "quit_2min": {"horizon": 120}}.items():
        result[case] = {}
        for name, account in (("fresh", Account()), ("maxed", full_account(study.plan))):
            result[case][name] = {}
            for policy in ("Hybrid", "Collector", "Gunship"):
                options = dict(horizon=1800, **{k:v for k,v in kwargs.items() if k != "horizon"})
                options["horizon"] = kwargs.get("horizon", 1800)
                _, summary = study.group(f"sensitivity_{case}_{name}", account, policy,
                                         range(20000, 20000 + count), **options)
                result[case][name][policy] = summary
        print("SENSITIVITY", case, flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pilot", action="store_true")
    parser.add_argument("--runs", type=int, default=200)
    parser.add_argument("--players", type=int, default=40)
    parser.add_argument("--attempts", type=int, default=40)
    parser.add_argument("--stress-runs", type=int, default=60)
    parser.add_argument("--workers", type=int, default=6)
    args = parser.parse_args()
    if min(args.runs, args.players, args.stress_runs, args.workers) < 1 or args.attempts < 20:
        parser.error("positive counts and at least twenty campaign attempts are required")
    spec = json.loads((DIRECTORY / "endless-simulation-input.json").read_text())
    world = json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text())
    plan = json.loads((DIRECTORY / "short-run-progression.json").read_text())
    path = DIRECTORY / ("short-run-calibration-results.json" if args.pilot else "short-run-results.json")
    with ProcessPoolExecutor(max_workers=args.workers, mp_context=multiprocessing.get_context("spawn")) as executor, gzip.open(path.with_suffix(".jsonl.gz"), "wt") as raw:
        study = Experiment(spec, world, plan, raw, executor)
        result = {"scope": __doc__, "plan_snapshot": plan, "arguments": vars(args),
                  "seeds": {"pilot": [0, args.runs - 1], "holdout": [10000, 10000 + args.runs - 1],
                            "sensitivity": [20000, 20000 + args.stress_runs - 1],
                            "campaign": "100000 + player*1000 + attempt; paired across ad scenarios and policies"}}
        if args.pilot:
            result["calibration"] = pilot(study, args.runs)
        else:
            result.update(holdout=holdout(study, args.runs),
                          sensitivity=sensitivity(study, args.stress_runs),
                          campaigns=campaigns(study, args.players, args.attempts))
        result["simulated_runs"] = study.count
    paths = [Path(__file__), ROOT / "tools/check_endless_upgrades.py", ROOT / "tools/simulate_endless_balance.py",
             *sorted((ROOT / "tools/endless_balance").glob("*.py")),
             DIRECTORY / "short-run-progression.json", DIRECTORY / "endless-simulation-input.json",
             DIRECTORY / "endless-encounter-assumptions.json"]
    result["source_sha256"] = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    path.write_text(json.dumps(result, indent=2) + "\n")
    print("DONE", study.count, path, flush=True)


if __name__ == "__main__":
    main()
