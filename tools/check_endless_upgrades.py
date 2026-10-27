#!/usr/bin/env python3
"""Exercise the proposed endless card pool, not combat or an approved game rule.

Run: python3 tools/check_endless_upgrades.py --runs 1000 --levels 200
Standard-library-only design experiment; does not modify native game code.
"""
import argparse
import hashlib
import json
import random
import statistics
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC_PATH = ROOT / "design/balance/endless-upgrade-system.json"


def candidates(spec, hardware, research, damaged=False):
    result = []
    for module in spec["hardware"]:
        rank = hardware.get(module["id"], 0)
        if rank >= 3 or (rank == 0 and len(hardware) >= spec["loop"]["hardware_slots"]):
            continue
        if not all(hardware.get(key, 0) >= value for key, value in module.get("requires", {}).items()):
            continue
        result.append({"family": module["id"], "kind": "hardware", "rarity": module["rarity"], "value": 1})
    for item in spec["research"]:
        if item.get("requires_damaged_hull") and not damaged:
            continue
        cap = item["cap"]
        if cap is not None and research.get(item["id"], 0) >= cap:
            continue
        for rarity, value in item["values"].items():
            if cap is not None:
                value = min(value, cap - research.get(item["id"], 0))
            result.append({"family": item["id"], "kind": "research", "rarity": rarity, "value": value})
    return result


def offer(spec, pool, rng):
    pool = list(pool)
    cards = []
    for _ in range(spec["loop"]["cards_per_level"]):
        rarities = [r for r in spec["loop"]["rarity_weights"] if any(c.get("draw_rarity", c["rarity"]) == r for c in pool)]
        rarity = rng.choices(rarities, [spec["loop"]["rarity_weights"][r] for r in rarities])[0]
        tier = [card for card in pool if card.get("draw_rarity", card["rarity"]) == rarity]
        card = rng.choices(tier, [spec["loop"]["family_weights"][c["kind"]] for c in tier])[0]
        cards.append(card)
        pool = [candidate for candidate in pool if candidate["family"] != card["family"]]
    assert len({card["family"] for card in cards}) == 3
    return cards


def trial(spec, seed, levels, policy):
    rng = random.Random(seed)
    hardware, research = {}, {}
    hardware_lookup = {module["id"]: module for module in spec["hardware"]}
    full_build_pick = None
    rarity_counts = Counter()
    for pick in range(1, levels + 1):
        cards = offer(spec, candidates(spec, hardware, research), rng)
        rarity_counts.update(card["rarity"] for card in cards)
        if policy == "hardware_first":
            selected = max(cards, key=lambda card: (card["kind"] == "hardware", card["value"]))
        elif policy == "research_first":
            selected = max(cards, key=lambda card: (card["kind"] == "research", card["value"]))
        else:
            selected = rng.choice(cards)
        key = selected["family"]
        if selected["kind"] == "hardware":
            module = hardware_lookup[key]
            assert all(hardware.get(k, 0) >= v for k, v in module.get("requires", {}).items())
            hardware[key] = hardware.get(key, 0) + 1
        else:
            research[key] = research.get(key, 0) + selected["value"]
        assert len(hardware) <= 6 and all(1 <= rank <= 3 for rank in hardware.values())
        assert research.get("capture_speed", 0) <= 100
        assert research.get("fire_rate", 0) <= 50
        if full_build_pick is None and len(hardware) == 6 and all(rank == 3 for rank in hardware.values()):
            full_build_pick = pick
    return {"full_build_pick": full_build_pick, "hardware_count": len(hardware), "offered_rarities": dict(rarity_counts)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs", type=int, default=1000)
    parser.add_argument("--levels", type=int, default=200)
    args = parser.parse_args()
    if args.runs < 1 or args.levels < 1:
        parser.error("--runs and --levels must be positive")
    spec = json.loads(SPEC_PATH.read_text())
    modules = spec["hardware"]
    assert len({module["id"] for module in modules}) == len(modules) == 12
    assert len(spec["previous_effect_mapping"]) == 31
    assert all(len(module["ranks"]) == 3 for module in modules)
    for module in modules:
        assert all(key in {m["id"] for m in modules} for key in module.get("requires", {}))
    # Worst case: all six installed modules maxed, all bounded research maxed,
    # full hull, so Emergency Repairs is unavailable. Four families must remain.
    maxed = {m["id"]: 3 for m in modules[:6]}
    tail = candidates(spec, maxed, {"capture_speed": 100, "fire_rate": 50})
    assert {c["family"] for c in tail} == {"weapon_damage", "hull_reinforcement", "shield_capacity", "specimen_value"}
    for seed in range(1000):
        offer(spec, tail, random.Random(seed))
    assert any(c["family"] == "emergency_repairs" for c in candidates(spec, maxed, {}, damaged=True))
    assert trial(spec, 42, 50, "random") == trial(spec, 42, 50, "random")
    results = {}
    for policy in ("hardware_first", "research_first", "random"):
        rows = [trial(spec, seed, args.levels, policy) for seed in range(args.runs)]
        completed = [row["full_build_pick"] for row in rows if row["full_build_pick"] is not None]
        results[policy] = {
            "drafts": args.runs, "levels_each": args.levels,
            "full_hardware_build_completed_pct": round(len(completed) * 100 / args.runs, 1),
            "median_pick_to_complete_hardware_among_completions": statistics.median(completed) if completed else None,
            "mean_final_hardware_slots": round(statistics.mean(row["hardware_count"] for row in rows), 2),
            "offered_rarities": dict(sum((Counter(row["offered_rarities"]) for row in rows), Counter())),
        }
        print(policy, json.dumps(results[policy]), flush=True)
    report = {"scope": "Card availability, rank/prerequisite limits and random drafting only. No combat, XP earning, evolution execution or survival validation.",
              "spec_sha256": hashlib.sha256(SPEC_PATH.read_bytes()).hexdigest(),
              "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "seed_range": [0, args.runs - 1], "worst_case_full_health_tail_checks": 1000,
              "policies": results}
    path = ROOT / "design/balance/endless-offer-results.json"
    path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Saved {path}", flush=True)


if __name__ == "__main__":
    main()
