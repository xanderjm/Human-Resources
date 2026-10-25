"""Upgrade state, seeded offers and explicit automated selection policies."""
from dataclasses import dataclass, field
import math

from check_endless_upgrades import candidates, offer

POLICIES = {
    "Hybrid": (["laser_array", "beam_splitter", "shield_generator", "repair_swarm", "vector_thrusters", "tractor_lens"], (1.2, 1.2, 1.1, .25)),
    "Collector": (["beam_splitter", "tractor_lens", "heavy_winch", "repair_swarm", "vector_thrusters", "overtime_reactor"], (.8, 2, 1, .5)),
    "Gunship": (["laser_array", "rail_lens", "arc_coil", "orbit_drones", "shield_generator", "vector_thrusters"], (2, .5, 1, .1)),
    "Defender": (["shield_generator", "repair_swarm", "mirror_plating", "phase_drive", "vector_thrusters", "laser_array"], (.8, .7, 2, .15)),
    "Storm": (["emp_ring", "tractor_lens", "beam_splitter", "repair_swarm", "orbit_drones", "shield_generator"], (1, 1.4, 1.1, .3)),
    "Score": (["bounty_scanner", "beam_splitter", "tractor_lens", "vector_thrusters", "phase_drive", "shield_generator"], (.6, 1.6, .8, 2)),
    "Research": ([], (1.5, 1, 1.5, .2)),
    "Random": ([], (1, 1, 1, 1)),
}


@dataclass
class Build:
    hardware: dict = field(default_factory=dict)
    research: dict = field(default_factory=dict)
    evolutions: list = field(default_factory=list)

    def stats(self, spec):
        b = spec["loop"]["baseline"]
        s = {"shield_regen": b["shield_regen_per_second"],
             "shield_delay": b["shield_recharge_delay_seconds"],
             "capture_penalty": b["capture_movement_penalty_percent"] / 100}
        for module in spec["hardware"]:
            rank = self.hardware.get(module["id"], 0)
            if rank:
                s.update(module["rank_effects"][rank - 1])
        s["max_hull"] = b["hull"] + self.research.get("hull_reinforcement", 0)
        s["max_shield"] = b["shield"] + s.get("shield_capacity_bonus", 0) + self.research.get("shield_capacity", 0)
        s["damage_multiplier"] = 1 + self.research.get("weapon_damage", 0) / 100
        s["fire_multiplier"] = 1 + self.research.get("fire_rate", 0) / 100
        s["capture_multiplier"] = 1 + self.research.get("capture_speed", 0) / 100
        s["score_multiplier"] = 1 + self.research.get("specimen_value", 0) / 100 + s.get("score_bonus", 0)
        s["beams"] = b["tractor_beams"] + s.get("extra_beams", 0)
        if "The Great Vacuum" in self.evolutions:
            s["beams"] = 6
        if "Hostile Acquisition" in self.evolutions:
            s.update(jeep_threshold=1, tank_threshold=.55)
        s["laser_evolution_multiplier"] = 1.2 if "Office Shredder" in self.evolutions else 1
        if "Storm Drain" in self.evolutions:
            s["pulse_radius"] = 6
        s["movement"] = 1 + s.get("movement_bonus", 0)
        return s

    def apply(self, card, spec):
        old = self.stats(spec)
        key = card["family"]
        if card["kind"] == "hardware":
            self.hardware[key] = self.hardware.get(key, 0) + 1
        elif key != "emergency_repairs":
            self.research[key] = self.research.get(key, 0) + card["value"]
        for recipe in spec["evolutions"]:
            if len(self.evolutions) >= spec["effect_constants"]["evolution_limit"]:
                break
            if recipe["name"] not in self.evolutions and all(self.hardware.get(i, 0) == 3 for i in recipe["ingredients"]):
                self.evolutions.append(recipe["name"])
        new = self.stats(spec)
        return new["max_hull"] - old["max_hull"], new["max_shield"] - old["max_shield"]

    def copy(self):
        return Build(dict(self.hardware), dict(self.research), list(self.evolutions))


def draw(spec, build, level, hull_fraction, rng):
    pool = candidates(spec, build.hardware, build.research, damaged=hull_fraction < 1)
    if spec.get("study_tuning", {}).get("ordinary_owned_upgrades"):
        pool = [dict(c, draw_rarity="Common") if c["kind"] == "hardware" and c["family"] in build.hardware else c for c in pool]
    every = spec.get("study_tuning", {}).get("research_bundle_every_levels", 0)
    if every:
        unbounded = {r["id"] for r in spec["research"] if r["cap"] is None and not r.get("requires_damaged_hull")}
        multiplier = 1 + (level - 1) // every
        pool = [dict(c, value=c["value"] * multiplier) if c["kind"] == "research" and c["family"] in unbounded else c for c in pool]
    return offer(spec, pool, rng)


def capture_estimate(stats, spec, world, density=1):
    speed = stats["movement"] * (1 - stats["capture_penalty"])
    encounters = world["biological_encounters_per_second"] * density * speed * (1 + stats.get("radius_bonus", 0))
    uptime = min(1, world["base_capture_uptime"] + world["grace_uptime_per_second"] * stats.get("grace_seconds", 0))
    capacity = stats["beams"] * stats["capture_multiplier"] / spec["loop"]["baseline"]["capture_seconds"] * uptime
    return capacity * (1 - math.exp(-encounters / capacity))


def features(stats, spec, world):
    c = spec["effect_constants"]
    b = spec["loop"]["baseline"]
    laser = b["laser_damage"] * b["laser_shots_per_second"] * (1 + c["added_laser_damage_multiplier"] * stats.get("extra_lasers", 0))
    laser *= 1 + .6 * stats.get("focus_bonus", 0)
    secondary = c["pierce_damage_multiplier"] * stats.get("pierce", 0) * world["pierce_alignment_probability"]
    secondary += c["arc_damage_multiplier"] * stats.get("arc_targets", 0) * stats.get("arc_chance", 0) * .6
    laser *= (1 + secondary) * stats["laser_evolution_multiplier"]
    drone = c["drone_damage"] * c["drone_shots_per_second"] * stats.get("drones", 0)
    pulse = stats.get("pulse_damage", 0) / max(1, stats.get("pulse_cooldown", 1)) * max(1, int(stats.get("pulse_radius", 0) * world["pulse_targets_per_radius_metre"]))
    damage = ((laser + drone) * stats["fire_multiplier"] + pulse) * stats["damage_multiplier"]
    damage *= 1 + stats.get("idle_damage_cap", 0) * .4 + stats.get("active_fire_cap", 0) * .6
    damage += stats.get("jeep_threshold", 0) * 3 + stats.get("tank_threshold", 0) * 5
    capture = capture_estimate(stats, spec, world)
    durability = stats["max_hull"] + .7 * stats["max_shield"]
    durability *= 1 + stats.get("reflect_chance", 0) + (stats["movement"] - 1) * .5
    durability *= 1 + stats.get("dash_duration", 0) / max(1, stats.get("dash_cooldown", 1)) * 5
    durability += stats.get("repair_hull", 0) * capture * 3 + stats.get("capture_shield", 0) * capture * 4
    durability += stats["shield_regen"] * 5 / max(1, stats["shield_delay"])
    return damage, capture, durability, stats["score_multiplier"] + stats.get("streak_cap", 0) / 2


def evaluate(card, build, spec, world, policy, hull_fraction):
    old = features(build.stats(spec), spec, world)
    after = build.copy()
    after.apply(card, spec)
    new = features(after.stats(spec), spec, world)
    changes = [(n - o) / max(.01, o) for n, o in zip(new, old)]
    preferred, weights = POLICIES[policy]
    utility = sum(delta * weight for delta, weight in zip(changes, weights))
    if card["kind"] == "hardware":
        utility += .18 if card["family"] in preferred else -.2
        if policy == "Research":
            utility -= .4
    if hull_fraction < .6:
        utility += changes[2] * 3
    if card["family"] == "emergency_repairs":
        heal = min(1 - hull_fraction, card["value"] / 100)
        utility += heal * (1 + (1 - hull_fraction) * 5) * weights[2]
        changes[2] = heal
    new_ability = card["kind"] == "hardware" and build.hardware.get(card["family"], 0) == 0
    meaningful = new_ability or len(after.evolutions) > len(build.evolutions) or max(changes) >= .05
    return utility, meaningful
