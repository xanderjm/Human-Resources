"""Seeded scrolling encounter schedule, independent of upgrade choices."""
from dataclasses import dataclass
import math
import random


def poisson(rng, mean):
    product, count = 1.0, 0
    threshold = math.exp(-mean)
    while product > threshold:
        product *= rng.random()
        count += 1
    return max(0, count - 1)


@dataclass
class Enemy:
    identity: int
    kind: str
    hp: float
    max_hp: float
    damage: float
    attack_period: float
    accuracy: float
    born: float
    lifetime: float
    next_attack: float
    rng: random.Random
    disabled_until: float = 0
    slowed_until: float = 0


def schedule(world, horizon, seed, pressure):
    rng = random.Random(seed + 314159)
    events = {}
    identity = 0
    for second in range(math.ceil(horizon)):
        minute = second / 60
        for kind, rule in world["enemies"].items():
            if second < rule["start_second"]:
                continue
            rate = rule["initial_rate"] * min(rule["rate_multiplier_cap"], 1 + minute * rule["rate_growth_per_minute"])
            for _ in range(poisson(rng, rate * pressure)):
                events.setdefault(second, []).append((identity, kind, dict(rule)))
                identity += 1
        boss = world["boss"]
        if second >= boss["first_second"] and (second - boss["first_second"]) % boss["interval_seconds"] == 0:
            index = (second - boss["first_second"]) // boss["interval_seconds"]
            rule = dict(boss, hp=boss["base_hp"] * (1 + boss["health_index_coefficient"] * index) ** 2)
            events.setdefault(second, []).append((identity, "boss", rule))
            identity += 1
    return events


def spawn(event, world, second, seed, pressure):
    identity, kind, rule = event
    late_minutes = max(0, second / 60 - 5)
    hp = rule["hp"] * (1 + late_minutes * world["enemy_health_growth_per_minute_after_5"])
    if kind == "boss":
        hp *= pressure
    damage = rule["damage"] * (1 + late_minutes * world["enemy_damage_growth_per_minute_after_5"])
    rng = random.Random(seed * 1000003 + identity * 104729 + 271828)
    first_attack = second + rng.uniform(.5, rule["attack_period"])
    return Enemy(identity, kind, hp, hp, damage, rule["attack_period"], rule["accuracy"], second,
                 rule["lifetime"], first_attack, rng)
