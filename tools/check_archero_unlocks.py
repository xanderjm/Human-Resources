#!/usr/bin/env python3
"""Exploratory paired comparison of automatic unlock pool prerequisites."""
import argparse
import copy
import gzip
import hashlib
import json
import multiprocessing
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from endless_balance.archero import Inputs, Trial, account_state, execute_run, profile_plan
from simulate_short_progression import summarize

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT / "design/balance"
PREREQUISITES = {"heavy_winch":{"beam_splitter":2,"laser_array":1},
                 "overtime_reactor":{"beam_splitter":2},
                 "reserve_battery":{"beam_splitter":2},
                 "arc_coil":{"laser_array":1},"mirror_plating":{"shield_generator":1}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs",type=int,default=100)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("positive --runs required")
    original = json.loads((DIRECTORY / "endless-simulation-input.json").read_text())
    world = json.loads((DIRECTORY / "endless-encounter-assumptions.json").read_text())
    plan = json.loads((DIRECTORY / "archero-ufo-system.json").read_text())
    plan = profile_plan(plan,plan["selected_profile"])
    # This experiment isolates the rule by modifying the frozen catalogue;
    # do not also apply the selected plan's prerequisite overlay to both arms.
    plan.pop("eligibility_prerequisites", None)
    contextual = copy.deepcopy(original)
    for module in contextual["hardware"]:
        module["requires"] = {**module.get("requires",{}),**PREREQUISITES.get(module["id"],{})}
    result = {"scope":__doc__,"seeds":[80000,80000+args.runs-1],"prerequisites":PREREQUISITES,
              "plan_snapshot":plan,"modified_source_snapshot":contextual,"results":{},"simulated_runs":0}
    path = DIRECTORY / "archero-unlock-followup.json"
    with ProcessPoolExecutor(max_workers=6,mp_context=multiprocessing.get_context("spawn")) as executor, gzip.open(path.with_suffix(".jsonl.gz"),"wt") as raw:
        for variant,spec in (("unrestricted",original),("contextual",contextual)):
            result["results"][variant] = {}
            for state in ("unlocks_only","maxed"):
                result["results"][variant][state] = {}
                for policy in ("Hybrid","Collector","Gunship"):
                    inputs = Inputs(spec,world,plan)
                    tasks = [Trial(inputs,account_state(plan,state),policy,seed,{"horizon":1800}) for seed in range(80000,80000+args.runs)]
                    rows = list(executor.map(execute_run,tasks))
                    for row in rows:
                        raw.write(json.dumps(dict(variant=variant,state=state,**row),separators=(",",":"))+"\n")
                    summary = summarize(rows,plan)
                    result["results"][variant][state][policy] = summary
                    result["simulated_runs"] += len(rows)
                print(variant,state,{p:s["median_observed_minutes"] for p,s in result["results"][variant][state].items()},flush=True)
    paths = [Path(__file__),ROOT/"tools/simulate_short_progression.py",ROOT/"tools/simulate_endless_balance.py",
             ROOT/"tools/check_endless_upgrades.py",*sorted((ROOT/"tools/endless_balance").glob("*.py")),
             DIRECTORY/"endless-simulation-input.json",DIRECTORY/"endless-encounter-assumptions.json",DIRECTORY/"archero-ufo-system.json"]
    result["source_sha256"] = {str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    path.write_text(json.dumps(result,indent=2)+"\n")


if __name__ == "__main__":
    main()
