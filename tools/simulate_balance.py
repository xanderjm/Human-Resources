#!/usr/bin/env python3
"""Seeded encounter-model balance experiments, using only the standard library.

This is not the game engine. Movement, range, target alignment, dash timing and
capture interruptions use explicit statistical proxies. Enemy HP, damage,
shield/hull, timed offers and evolutions are simulated per second. Draft RNG
and encounter RNG are separate. Currency never feeds back into upgrade picks.
Run: python3 tools/simulate_balance.py --runs 300 --output design/balance/results.json
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import random
import statistics
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = json.loads((ROOT / "design/balance/upgrades.json").read_text())
UPGRADES = {u["id"]: u for u in CONFIG["upgrades"]}
EVOLUTIONS = CONFIG["evolutions"]
PROFILES = {
    "Collector": ["extra_hands", "wider_net", "express_collection", "heavy_lifting", "personal_space", "shield_siphon", "additional_laser", "rapid_discharge"],
    "Gunship": ["additional_laser", "rapid_discharge", "power_amplifier", "piercing_light", "focused_fire", "orbital_security", "personal_space", "thruster_upgrade"],
    "Defender": ["personal_space", "quick_recharge", "shield_siphon", "return_to_sender", "reinforced_saucer", "extra_hands", "additional_laser", "rapid_discharge"],
    "Hybrid": ["additional_laser", "rapid_discharge", "extra_hands", "wider_net", "personal_space", "shield_siphon", "power_amplifier", "express_collection"],
    "Storm": ["vacuum_effect", "static_discharge", "extra_hands", "express_collection", "orbital_security", "personal_space", "shield_siphon", "power_amplifier"],
    "Random": [],
}


def draft(seed: int, profile: str) -> list[tuple[int, str]]:
    rng = random.Random(seed + 271828)
    ranks = Counter()
    picks = []
    preferred = PROFILES[profile]
    rerolls = 3
    for pick in range(24):
        eligible = [u for u in UPGRADES.values()
                    if ranks[u["id"]] < 3
                    and (u["id"] in ranks or len(ranks) < 8)
                    and all(ranks[p] > 0 for p in u.get("requires", []))]
        def roll_offer() -> list[str]:
            remaining = list(eligible)
            result = []
            owned = [u for u in remaining if ranks[u["id"]] > 0]
            if owned:
                card = rng.choice(owned)
                result.append(card["id"])
                remaining.remove(card)
            for _ in range(min(3 - len(result), len(remaining))):
                tiers = [r for r in CONFIG["rules"]["rarity_weights"]
                         if any(u["rarity"] == r for u in remaining)]
                tier = rng.choices(tiers, [CONFIG["rules"]["rarity_weights"][r] for r in tiers])[0]
                pool = [u for u in remaining if u["rarity"] == tier]
                card = rng.choices(pool, [1.5 if ranks[u["id"]] else 1 for u in pool])[0]
                result.append(card["id"])
                remaining.remove(card)
            return result
        offers = ["personal_space", "additional_laser", "extra_hands"] if pick == 0 else roll_offer()
        while profile != "Random" and pick > 0 and rerolls > 0 and not any(u in preferred for u in offers):
            rerolls -= 1
            offers = roll_offer()
        if profile == "Random":
            selected = rng.choice(offers)
        else:
            def value(uid: str) -> float:
                preference = 12 - preferred.index(uid) * 0.3 if uid in preferred else 2.0
                if uid in ("personal_space", "additional_laser") and not ranks[uid] and pick >= 3:
                    preference += 6
                # Diminishing rank value leaves room for useful new components.
                return preference - 0.8 * ranks[uid] + rng.random() * 0.2
            selected = max(offers, key=value)
        ranks[selected] += 1
        picks.append(((pick + 1) * 24, selected))
    assert len(ranks) == 8 and all(r == 3 for r in ranks.values())
    return picks


def poisson(rng: random.Random, mean: float) -> int:
    if mean <= 0:
        return 0
    # All per-second rates in this model are small.
    threshold, product, count = math.exp(-mean), 1.0, 0
    while product > threshold:
        product *= rng.random()
        count += 1
    return count - 1


def simulate(seed: int, picks: list[tuple[int, str]], pressure: float = 1.0,
             density: float = 1.0, overrides: dict | None = None) -> dict:
    rng = random.Random(seed)
    tweaks = overrides or {}
    ranks = Counter()
    hull, shield, last_hit = 100.0, 0.0, -100
    enemies = []  # [kind, remaining hp, maximum hp, hit chance, hit damage, spawn second]
    captures = rewards = kills = boss_kills = vehicles = 0
    capture_credit = vehicle_credit = 0.0
    streak = 0
    bulk_until = bio_until = hostile_until = drone_until = 0
    emp_until = emp_ready = dash_ready = vehicle_ready = intercept_ready = 0
    reflected_store = 0.0
    evolved = set()
    next_pick = 0
    total_damage = 0.0
    incoming_hull = 0.0
    shield_from_captures = 0.0
    for t in range(600):
        while next_pick < len(picks) and picks[next_pick][0] <= t:
            uid = picks[next_pick][1]
            ranks[uid] += 1
            if uid == "personal_space":
                shield += 25
            elif uid == "bigger_bubble":
                shield += 15
            elif uid == "reinforced_saucer":
                hull += 20
            next_pick += 1
        r = ranks
        if t in (240, 360, 480):
            available = [e for e in EVOLUTIONS if e["id"] not in evolved
                         and all(r[i] == 3 for i in e["ingredients"])]
            if available and len(evolved) < 2:
                evolved.add(available[0]["id"])

        progress = (t / 600) ** 1.6
        for kind, rate, hp, chance, hit in (
            ("soldier", .16 + .40 * progress, 18, .035, 6),
            ("jeep", .015 + .085 * progress, 90, .050, 10),
            ("tank", .025 * progress if t >= 120 else 0, 300, .060, 18),
        ):
            for _ in range(poisson(rng, rate * pressure)):
                enemies.append([kind, float(hp), hp, chance, hit, t])
        if t in (240, 420, 540):
            hp = {240: 250, 420: 650, 540: 1200}[t] * pressure
            enemies.append(["boss", hp, hp, .12, 16, t])

        beams = 6 if "great_vacuum" in evolved else 1 + r["extra_hands"]
        grace = .70 + .06 * r["firm_grip"]
        radius_bonus = 1 + .15 * r["wider_net"]
        vacuum_bonus = 1 + .07 * r["vacuum_effect"]
        bio_bonus = .10 * r["biofuel"] if t < bio_until else 0
        speed = min(1.6, 1 + .08 * r["thruster_upgrade"] + bio_bonus)
        capture_move = [.8, .85, .9, 1][r["tractor_stabiliser"]]
        if "great_vacuum" in evolved:
            capture_move *= .9
        # Ground threats leave the scrolling playfield; bosses continue pursuit.
        lifetime = {"soldier": 24, "jeep": 36, "tank": 48}
        enemies = [e for e in enemies if e[0] == "boss" or t - e[5] < lifetime[e[0]] / (speed * capture_move)]
        # Exposed targets per second. Radius acts linearly in this scrolling-path proxy.
        encounter = 1.4 * density * radius_bonus * vacuum_bonus * speed * capture_move
        cap_speed = 1 + .2 * r["express_collection"]
        if t < bulk_until and beams > 1:
            cap_speed += [0, .4, .6, .8][r["bulk_collection"]]
        if t < hostile_until:
            cap_speed += [0, .45, .75, 1.05][r["hostile_takeover"]]
        if "great_vacuum" in evolved:
            cap_speed += .15

        # Wounded vehicles consume two slots and have a shared collection cooldown.
        vehicle = None
        if r["heavy_lifting"] and beams >= 2 and t >= vehicle_ready:
            jeep_threshold = [0, .4, .55, .7][r["heavy_lifting"]]
            tank_threshold = .35 if r["heavy_lifting"] == 3 else 0
            if "hostile_acquisition" in evolved:
                jeep_threshold, tank_threshold = 1, .55
            vehicle = next((e for e in enemies
                            if e[0] in ("jeep", "tank")
                            and e[1] / e[2] <= (jeep_threshold if e[0] == "jeep" else tank_threshold)), None)
        vehicle_slots = 2 if vehicle is not None else 0
        if vehicle is not None:
            vehicle_seconds = 3 / (cap_speed * (1 + .2 * max(0, r["heavy_lifting"] - 1)))
            if vehicle[0] == "tank":
                vehicle_seconds = max(2, vehicle_seconds)
            vehicle_credit += 1 / vehicle_seconds
            if vehicle_credit >= 1:
                enemies.remove(vehicle)
                vehicle_credit = 0
                vehicle_ready = t + 8
                vehicles += 1
                rewards += 7 if "hostile_acquisition" in evolved else 5
        else:
            vehicle_credit = 0

        capacity_rate = (beams - vehicle_slots) * cap_speed / 2 * grace
        # Empty beam time decreases smoothly with target density. A hard minimum
        # would incorrectly make speed/grip upgrades worthless at low density.
        capture_rate = capacity_rate * (1 - math.exp(-encounter / capacity_rate)) if capacity_rate else 0
        capture_credit += capture_rate
        completed = int(capture_credit)
        capture_credit -= completed
        occupancy = min(beams, capture_rate * 2 / max(.1, cap_speed))
        active = occupancy + (1 if vehicle is not None else 0)
        idle = max(0, beams - occupancy - vehicle_slots)
        if completed:
            old_tens = captures // 10
            captures += completed
            streak += completed
            multiplier = 1 + min(5, streak // 10) * [0, .02, .03, .04][r["performance_bonus"]]
            rewards += completed * multiplier
            hull = min(100 + 20 * r["reinforced_saucer"], hull + (captures // 10 - old_tens) * [0, 2, 3, 4][r["repair_nanites"]])
            bulk_until, bio_until, drone_until = t + 2, t + 3, t + 3
            soldier_capture = any(e[0] == "soldier" for e in enemies) and rng.random() < .40
            if soldier_capture:
                soldier = next(e for e in enemies if e[0] == "soldier")
                enemies.remove(soldier)
                if r["exit_interview"] and t >= emp_ready:
                    emp_until = t + [0, .5, .75, 1][r["exit_interview"]]
                    emp_ready = t + 4

        capacity = 25 * r["personal_space"] + 15 * r["bigger_bubble"]
        siphon = min(tweaks.get("siphon_cap", 4), completed * r["shield_siphon"])
        shield_from_captures += min(siphon, max(0, capacity - shield))
        shield = min(capacity, shield + siphon)
        if t - last_hit >= 5 - r["quick_recharge"]:
            shield = min(capacity, shield + 5 + r["quick_recharge"])

        firing_rate = min(2, 1 + .10 * r["rapid_discharge"] + min([0, .24, .36, .48][r["overtime"]], [0, .08, .12, .16][r["overtime"]] * active))
        damage_multiplier = 1 + .12 * r["power_amplifier"] + min([0, .2, .36, .52][r["power_diversion"]], [0, .10, .18, .26][r["power_diversion"]] * idle)
        laser_dps = 12 * (1 + tweaks.get("extra_laser_damage", .45) * r["additional_laser"]) * firing_rate * damage_multiplier
        # Only a fraction of enemies line up; focus ramp is approximated by target durability.
        secondary_targets = max(0, len(enemies) - 1)
        laser_dps *= 1 + .35 * r["piercing_light"] * min(.35, secondary_targets * .025)
        laser_dps *= 1 + [0, .2, .3, .4][r["chain_reaction"]] * r["chain_reaction"] * .35 * min(1, secondary_targets / 8)
        laser_dps *= 1 + .10 * r["focused_fire"] * (.7 if any(e[0] in ("tank", "boss") for e in enemies) else .2)
        if "office_shredder" in evolved:
            laser_dps *= .9 if enemies and enemies[0][0] == "boss" else 1.25
        drone_dps = 4 * r["orbital_security"] * firing_rate * damage_multiplier
        if "employee_benefits" in evolved and t < drone_until:
            drone_dps *= 1.25
        damage = laser_dps + drone_dps

        pulse_rank = r["static_discharge"]
        if pulse_rank and t % [1, 6, 5, 4][pulse_rank] == 0:
            # A 4m pulse reaches at most six enemies, 6m at most nine in the proxy.
            limit = 9 if "storm_drain" in evolved else 6
            for enemy in enemies[:limit]:
                dealt = [0, 12, 18, 24][pulse_rank] * (1 + .12 * r["power_amplifier"])
                total_damage += min(max(0, enemy[1]), dealt)
                enemy[1] -= dealt
        if "mirrorball" in evolved and t % 8 == 0:
            damage += min(40, reflected_store)
            reflected_store = 0
        # Prioritize oldest threats, skipping a vehicle already assigned to a beam.
        while enemies:
            target = next((e for e in enemies if e is not vehicle), None)
            if target is None or (damage <= 0 and target[1] > 0):
                break
            spent = min(damage, max(0, target[1]))
            target[1] -= spent
            damage -= spent
            total_damage += spent
            if target[1] <= 0:
                enemies.remove(target)
                kills += 1
                if target[0] == "boss":
                    boss_kills += 1
                if target[0] in ("jeep", "tank"):
                    hostile_until = t + 5
            else:
                break

        # Mobility improves dodging; skill is a constant across comparison builds.
        dodge_factor = max(.55, 1 / (speed * capture_move)) * .72
        storm_slow = .85 if "storm_drain" in evolved and t % 4 == 0 else 1
        for enemy in enemies:
            if enemy[1] <= 0:
                continue
            emp_factor = 1 - max(0, emp_until - t) if enemy[0] != "boss" else 1
            if rng.random() >= enemy[3] * dodge_factor * storm_slow * emp_factor:
                continue
            if r["emergency_jump"] and t >= dash_ready and rng.random() < .55:
                dash_ready = t + [0, 14, 12, 10][r["emergency_jump"]]
                continue
            if "employee_benefits" in evolved and t >= intercept_ready and shield >= 3 and enemy[0] != "boss":
                shield -= 3
                intercept_ready = t + 6
                continue
            reflect_chance = [0, .10, .15, .20][r["return_to_sender"]]
            if shield > 0 and rng.random() < reflect_chance:
                enemy[1] -= enemy[4]
                reflected_store += enemy[4]
                continue
            hit = float(enemy[4])
            absorbed = min(shield, hit)
            shield -= absorbed
            remaining = hit - absorbed
            hull -= remaining
            incoming_hull += remaining
            if remaining:
                streak = 0
            last_hit = t
        if hull <= 0:
            return {"survived": False, "seconds": t + 1, "captures": captures, "rewards": rewards, "bosses": boss_kills, "vehicles": vehicles, "evolutions": sorted(evolved), "hull": 0, "damage": total_damage, "hull_damage": incoming_hull, "siphon_restored": shield_from_captures}
    return {"survived": True, "seconds": 600, "captures": captures, "rewards": rewards, "bosses": boss_kills, "vehicles": vehicles, "evolutions": sorted(evolved), "hull": round(hull, 1), "damage": total_damage, "hull_damage": incoming_hull, "siphon_restored": shield_from_captures}


def summarize(rows: list[dict]) -> dict:
    n = len(rows)
    survival = sum(r["survived"] for r in rows) / n
    # Wilson interval avoids zero-width bounds at 0%/100%.
    z = 1.96
    center = (survival + z*z/(2*n)) / (1 + z*z/n)
    half = z * math.sqrt(survival*(1-survival)/n + z*z/(4*n*n)) / (1+z*z/n)
    return {
        "runs": n, "survival_pct": round(100 * survival, 1),
        "survival_95pct_interval": [round(100 * (center-half), 1), round(100 * (center+half), 1)],
        "all_bosses_cleared_pct": round(100 * sum(r["bosses"] == 3 and r["survived"] for r in rows) / n, 1),
        **{f"mean_{key}": round(statistics.mean(r[key] for r in rows), 2)
           for key in ("seconds", "captures", "rewards", "bosses", "vehicles", "hull", "damage", "hull_damage", "siphon_restored")},
        "any_evolution_pct": round(100 * sum(bool(r["evolutions"]) for r in rows) / n, 1),
        "evolution_counts": dict(Counter(e for r in rows for e in r["evolutions"])),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=300)
    parser.add_argument("--output", type=Path, default=ROOT / "design/balance/results.json")
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    for u in UPGRADES.values():
        assert len(u["ranks"]) == 3
        assert all(p in UPGRADES for p in u.get("requires", []))
    for e in EVOLUTIONS:
        assert all(i in UPGRADES for i in e["ingredients"])
    draft_results = {}
    for profile in PROFILES:
        rows = [simulate(seed, draft(seed, profile)) for seed in range(args.runs)]
        draft_results[profile] = summarize(rows)
        print(profile, json.dumps(draft_results[profile]), flush=True)
    # Fixed full loadouts test the intended build ceiling separately from draft luck.
    ceiling = {profile: summarize([simulate(seed, [(0, uid) for uid in ids for _ in range(3)])
                                  for seed in range(args.runs)])
               for profile, ids in PROFILES.items() if ids}
    sensitivity = {}
    sensitivity_n = max(30, args.runs // 3)
    for name, pressure, density in (("military_minus_20pct", .8, 1), ("military_plus_20pct", 1.2, 1),
                                    ("targets_minus_25pct", 1, .75), ("targets_plus_25pct", 1, 1.25)):
        sensitivity[name] = {profile: summarize([simulate(seed, draft(seed, profile), pressure, density)
                                                for seed in range(sensitivity_n)])
                             for profile in PROFILES if profile != "Random"}
    counterfactuals = {}
    for name, tweaks in (("extra_lasers_at_full_damage", {"extra_laser_damage": 1}),
                         ("uncapped_capture_shield", {"siphon_cap": 100})):
        counterfactuals[name] = {profile: summarize([simulate(seed, draft(seed, profile), overrides=tweaks)
                                                    for seed in range(sensitivity_n)])
                                for profile in ("Gunship", "Collector", "Defender", "Hybrid")}
    capture_stress = {}
    for name, tweaks in (("capped", {}), ("uncapped", {"siphon_cap": 100})):
        capture_stress[name] = {profile: summarize([simulate(seed,
            [(0, uid) for uid in PROFILES[profile] for _ in range(3)], density=3, overrides=tweaks)
            for seed in range(sensitivity_n)]) for profile in ("Collector", "Defender", "Hybrid")}
    # Isolated three-pick ablation: same seed and seven other maxed lines for every upgrade.
    # Not a ranking: the fixed support shell favors some upgrades and has no draft rarity.
    ablations = {}
    support = ["additional_laser", "rapid_discharge", "personal_space", "extra_hands", "express_collection", "power_amplifier", "thruster_upgrade"]
    ablation_n = max(30, args.runs // 6)
    for uid in UPGRADES:
        shell = [s for s in support if s != uid]
        baseline_picks = [(0, s) for s in shell for _ in range(3)]
        baseline = summarize([simulate(seed, baseline_picks) for seed in range(ablation_n)])
        added = summarize([simulate(seed, baseline_picks + [(0, uid)] * 3) for seed in range(ablation_n)])
        ablations[uid] = {"baseline": baseline, "with_upgrade": added}
    result = {
        "model": "Abstract 1-second encounter simulation; not engine validation.",
        "seed_range": [0, args.runs - 1], "config_version": CONFIG["version"],
        "model_source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "catalogue_sha256": hashlib.sha256((ROOT / "design/balance/upgrades.json").read_bytes()).hexdigest(),
        "catalogue_counts": dict(Counter(u["rarity"] for u in UPGRADES.values())),
        "draft_preferences": PROFILES,
        "assumptions": ["No spatial simulation: radius, alignment, movement, dash skill and focus are statistical proxies.",
                        "Biological encounter rate 1.4/s before radius, movement and density modifiers; 40% soldier-capture opportunity when soldiers are present, at most one soldier per second.",
                        "Enemy spawns ramp by (time/600)^1.6: soldiers 0.16 to 0.56/s, jeeps 0.015 to 0.10/s, tanks 0 to 0.025/s after 120s.",
                        "Ground enemies leave the scrolling playfield after base 24/36/48 seconds for soldiers/jeeps/tanks, divided by effective movement; bosses persist.",
                        "Bosses at 240/420/540s with 250/650/1200 HP; survival and clearing bosses are separate outcomes.",
                        "All biological targets use a 2s capture time. No route planning, terrain, loot, manual aim or panic behavior.",
                        "24 picks at 24-second intervals, 8 distinct lines, 3 ranks each; 3 rerolls, no banishes or reward-driven extra XP.",
                        "First offer guarantees shield/laser/beam choice; later offers guarantee one owned card if possible. Non-random policies reroll offers without a preferred card, and prioritize a missing shield/laser foundation after pick 3.",
                        "Evolution checks at 240/360/480s, at most two per run. Recipe order breaks ties in the model; players choose in game.",
                        "Enemy attacks are Bernoulli samples per second. Jump assumes a 55% successful reactive dodge when off cooldown.",
                        "Firm Grip is modeled as +6 percentage points of capture uptime per rank; Vacuum Effect as +7% encounter rate per rank. Pulse target limits, laser alignment and focus dwell are proxies, not spatial checks.",
                        "Full-loadout and ablation results test ceilings from time zero, not realistic progression."],
        "drafts": draft_results, "full_loadout_ceilings": ceiling, "sensitivity": sensitivity,
        "counterfactuals": counterfactuals, "three_rank_ablations": ablations,
        "triple_density_full_loadout_shield_stress": capture_stress,
    }
    result["total_simulated_runs"] = (
        sum(v["runs"] for v in draft_results.values())
        + sum(v["runs"] for v in ceiling.values())
        + sum(v["runs"] for group in sensitivity.values() for v in group.values())
        + sum(v["runs"] for group in counterfactuals.values() for v in group.values())
        + sum(v["runs"] for group in capture_stress.values() for v in group.values())
        + sum(v["runs"] for group in ablations.values() for v in group.values()))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(f"Saved {args.output}", flush=True)


if __name__ == "__main__":
    main()
