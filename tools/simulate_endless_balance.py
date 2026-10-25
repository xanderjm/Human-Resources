#!/usr/bin/env python3
"""Compare seeded endless-run models; proxies and survival are not proof of fun.

python3 tools/simulate_endless_balance.py
Outputs summaries plus compressed per-run observations. Native gameplay is not
implemented or executed. Training and holdout seeds are disjoint.
"""
import argparse
import copy
import gzip
import hashlib
import json
from pathlib import Path
import statistics
from collections import Counter

from endless_balance.run import Run
from endless_balance.ship import POLICIES

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / "design/balance"
TUNINGS = {
    "baseline": {},
    "discovery_only": {"ordinary_owned_upgrades":True},
    "bundles_only": {"research_bundle_every_levels":10},
    "rerolls_only": {"rerolls":2},
    "combined": {"ordinary_owned_upgrades":True,"research_bundle_every_levels":10,"rerolls":2},
}


def variant(spec, name):
    value = copy.deepcopy(spec)
    value["study_tuning"] = TUNINGS[name]
    return value


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    index = (len(ordered) - 1) * fraction
    left = int(index)
    right = min(len(ordered) - 1, left + 1)
    return round(ordered[left] + (ordered[right] - ordered[left]) * (index - left), 2)


def summarize(rows, spec):
    n = len(rows)
    offers = max(1, sum(r["offers"] for r in rows))
    late_offers = max(1, sum(r["late_offers"] for r in rows))
    rare_ids = {m["id"] for m in spec["hardware"] if m["rarity"] == "Rare"}
    rare_ranks = [v for r in rows for k,v in r["hardware"].items() if k in rare_ids]
    rosters = Counter(tuple(sorted(r["hardware"])) for r in rows)
    late_intervals = []
    for row in rows:
        elapsed = 0
        for interval in row["pick_intervals"]:
            elapsed += interval
            if elapsed >= 600:
                late_intervals.append(interval)
    result = {
        "runs":n,"median_observed_minutes":percentile([r["seconds"] / 60 for r in rows], .5),
        "horizon_survivor_pct":round(100 * sum(r["censored"] for r in rows) / n, 1),
        "median_first_pick_seconds":percentile([r["first_pick"] for r in rows if r["first_pick"] is not None], .5),
        "median_first_hardware_seconds_among_reached":percentile([r["first_hardware"] for r in rows if r["first_hardware"] is not None], .5),
        "median_late_pick_interval_seconds":percentile(late_intervals, .5),
        "p90_late_pick_interval_seconds":percentile(late_intervals, .9),
        "mean_final_level":round(statistics.mean(r["level"] for r in rows), 2),
        "mean_captures":round(statistics.mean(r["captures"] for r in rows), 1),
        "mean_score":round(statistics.mean(r["score"] for r in rows), 1),
        "mean_capture_rate_per_minute_observed":round(statistics.mean(r["captures"] / max(1, r["seconds"] / 60) for r in rows), 2),
        "any_evolution_pct":round(100 * sum(bool(r["evolutions"]) for r in rows) / n, 1),
        "evolution_by_10min_pct":round(100 * sum(r["first_evolution"] is not None and r["first_evolution"] <= 600 for r in rows) / n, 1),
        "median_first_evolution_minutes_among_reached":percentile([r["first_evolution"] / 60 for r in rows if r["first_evolution"] is not None], .5),
        "full_hardware_build_pct":round(100 * sum(r["full_build"] is not None for r in rows) / n, 1),
        "low_impact_offers_pct":round(100 * sum(r["low_impact_offers"] for r in rows) / offers, 1),
        "late_low_impact_offers_pct":round(100 * sum(r["late_low_impact"] for r in rows) / late_offers, 1),
        "close_choice_offers_pct":round(100 * sum(r["close_choices"] for r in rows) / offers, 1),
        "forced_mismatch_runs_pct":round(100 * sum(r["forced_mismatch"] > 0 for r in rows) / n, 1),
        "mean_outside_preference_slots":round(statistics.mean(r["outside_preference_slots"] for r in rows if r["outside_preference_slots"] is not None), 2) if any(r["outside_preference_slots"] is not None for r in rows) else None,
        "rare_modules_found":len(rare_ranks),
        "rare_modules_finished_pct_of_found":round(100 * sum(rank == 3 for rank in rare_ranks) / len(rare_ranks), 1) if rare_ranks else None,
        "unique_hardware_rosters":len(rosters),"most_common_roster_pct":round(100 * rosters.most_common(1)[0][1] / n, 1),
        "selected_card_counts":dict(sum((Counter(r["selected_cards"]) for r in rows), Counter())),
    }
    for second in (300, 600, 1200, 1800, 3600):
        if all(r["horizon"] >= second for r in rows):
            alive = sum(r["seconds"] > second or (r["seconds"] == second and r["censored"]) for r in rows)
            result[f"survival_at_{second // 60}min_pct"] = round(100 * alive / n, 1)
    return result


class Study:
    def __init__(self, spec, world, raw):
        self.spec, self.world, self.raw, self.count = spec, world, raw, 0
        self.examples = []

    def group(self, phase, tuning, policy, seeds, **kwargs):
        spec = variant(self.spec, tuning)
        world = kwargs.pop("world", self.world)
        rows = []
        for seed in seeds:
            row = Run(spec, world, seed, policy, **kwargs).play()
            self.raw.write(json.dumps({"phase":phase,"tuning":tuning,**row}, separators=(",", ":")) + "\n")
            rows.append(row)
            self.count += 1
        return rows, summarize(rows, spec)


def pilot(study, count):
    output = {}
    for name in TUNINGS:
        output[name] = {}
        for policy in POLICIES:
            _, output[name][policy] = study.group("tuning", name, policy, range(count))
        print("TUNING", name, {p:(s["survival_at_30min_pct"],s["late_low_impact_offers_pct"]) for p,s in output[name].items()}, flush=True)
    return output


def holdout(study, count):
    output = {}
    for name in ("baseline", "combined"):
        output[name] = {}
        for policy in POLICIES:
            rows, output[name][policy] = study.group("holdout", name, policy, range(10000, 10000 + count))
            if policy in ("Hybrid", "Collector", "Gunship", "Random"):
                ordered = sorted(rows, key=lambda r:r["seconds"])
                for row in (ordered[0], ordered[len(rows)//2]):
                    replay = Run(variant(study.spec, name), study.world, row["seed"], policy, trace=True).play()
                    study.examples.append({"tuning":name,**replay})
            print("HOLDOUT", name, policy, json.dumps(output[name][policy]), flush=True)
    return output


def stress(study, count):
    output = {}
    tougher = copy.deepcopy(study.world)
    tougher["enemy_health_growth_per_minute_after_5"] *= 1.5
    tougher["enemy_damage_growth_per_minute_after_5"] *= 1.5
    cases = {"military_plus_25pct":{"pressure":1.25},"military_minus_20pct":{"pressure":.8},
             "targets_minus_35pct":{"density":.65},"targets_plus_35pct":{"density":1.35},
             "novice_dodging":{"skill":"novice"},"expert_dodging":{"skill":"expert"},"late_growth_plus_50pct":{"world":tougher}}
    for case, kwargs in cases.items():
        output[case] = {}
        for tuning in ("baseline", "combined"):
            output[case][tuning] = {}
            for policy in ("Hybrid", "Collector", "Gunship", "Random"):
                _, summary = study.group(case, tuning, policy, range(20000, 20000 + count), **kwargs)
                output[case][tuning][policy] = summary
        print("STRESS", case, flush=True)
    return output


def tails(study, count):
    output = {}
    for tuning in ("baseline", "combined"):
        output[tuning] = {}
        for policy in ("Hybrid", "Collector", "Gunship", "Defender"):
            _, output[tuning][policy] = study.group("60min_tail", tuning, policy, range(30000, 30000 + count), horizon=3600)
    return output


def timestep(study, count):
    output = {}
    for tuning in ("baseline", "combined"):
        output[tuning] = {}
        for policy in ("Hybrid", "Gunship"):
            one, _ = study.group("step_1s", tuning, policy, range(40000, 40000 + count))
            half, _ = study.group("step_half_s", tuning, policy, range(40000, 40000 + count), step=.5)
            output[tuning][policy] = {
                "runs_each":count,
                "median_absolute_observed_lifetime_delta_minutes":percentile([abs(a["seconds"]-b["seconds"])/60 for a,b in zip(one,half)], .5),
                "changed_horizon_survival_pct":round(100 * sum(a["censored"] != b["censored"] for a,b in zip(one,half)) / count, 1),
                "note":"Re-drafting uses earned XP; small timing changes can change later picks. This is sensitivity, not proof of numerical convergence."}
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pilot-runs", type=int, default=75)
    parser.add_argument("--holdout-runs", type=int, default=200)
    parser.add_argument("--stress-runs", type=int, default=40)
    parser.add_argument("--tail-runs", type=int, default=20)
    parser.add_argument("--step-runs", type=int, default=20)
    parser.add_argument("--output", type=Path, default=DIRECTORY / "endless-game-results.json")
    args = parser.parse_args()
    if any(v < 1 for v in (args.pilot_runs,args.holdout_runs,args.stress_runs,args.tail_runs,args.step_runs)):
        parser.error("all run counts must be positive")
    spec = json.loads((DIRECTORY / "endless-simulation-input.json").read_text())
    world = json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text())
    raw_path = args.output.with_suffix(".jsonl.gz")
    with gzip.open(raw_path, "wt") as raw:
        study = Study(spec, world, raw)
        result = {"scope":"Unvalidated offline encounters, not native gameplay or proof of fun. No subjective fun score is assigned.",
                  "tuning_seeds":[0,args.pilot_runs-1],"holdout_seeds":[10000,10000+args.holdout_runs-1],
                  "tunings":TUNINGS,"input_spec":spec,"world_assumptions":world,
                  "tuning_results":pilot(study,args.pilot_runs),"holdout_results":holdout(study,args.holdout_runs),
                  "sensitivity":stress(study,args.stress_runs),"long_tail":tails(study,args.tail_runs),
                  "timestep_sensitivity":timestep(study,args.step_runs)}
    result["simulated_runs_excluding_trace_replays"] = study.count
    result["interpretation_limits"] = [
        "Survivors are right-censored at 30 or 60 minutes; these are not observed deaths or estimated final lifetimes.",
        "A low-impact offer means all three options lack a new ability/evolution and add less than 5% to every modeled feature. This threshold is a design diagnostic, not a fun measurement.",
        "A forced mismatch means three new hardware cards outside the policy's preferred roster; it does not prove those modules are objectively bad.",
        "Automated policies are simple weighted heuristics, not optimal players. Close-choice scores depend on those heuristics.",
        "Encounter numbers and dodging are assumptions, not calibrated against a playable game. Strategy ranking can change with those assumptions."]
    paths = [Path(__file__), ROOT / "tools/check_endless_upgrades.py", *sorted((ROOT / "tools/endless_balance").glob("*.py")), DIRECTORY / "endless-simulation-input.json", DIRECTORY / "endless-encounter-assumptions.json"]
    result["source_sha256"] = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    args.output.write_text(json.dumps(result,indent=2) + "\n")
    args.output.with_name(args.output.stem + "-examples.json").write_text(json.dumps(study.examples,indent=2) + "\n")
    print("DONE", study.count, args.output, raw_path, flush=True)


if __name__ == "__main__":
    main()
