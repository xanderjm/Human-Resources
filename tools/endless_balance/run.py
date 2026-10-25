"""One-second encounter approximation with earned XP and endless upgrade picks.

No spatial simulation or human fun measurement. Statistics intentionally keep
world, draft and capture random streams separate. A horizon survivor is censored.
"""
import math
import random
from collections import Counter

from .ship import Build, POLICIES, draw, evaluate
from .world import schedule, spawn


class Run:
    def __init__(self, spec, world, seed, policy, horizon=1800, skill="regular",
                 pressure=1.0, density=1.0, step=1.0, trace=False):
        self.spec, self.world, self.seed, self.policy = spec, world, seed, policy
        self.horizon, self.pressure, self.density, self.step = horizon, pressure, density, step
        self.skill = world["skills"][skill]
        self.keep_trace = trace
        self.build = Build()
        self.stats = self.build.stats(spec)
        self.hull, self.shield = self.stats["max_hull"], self.stats["max_shield"]
        self.xp, self.level, self.score, self.captures = 0.0, 1, 0.0, 0
        self.kills, self.vehicle_captures = Counter(), 0
        self.enemies, self.vehicle, self.vehicle_work = [], None, 0.0
        self.events = schedule(world, horizon, seed, pressure)
        self.draft_rng, self.capture_rng = random.Random(seed + 161803), random.Random(seed + 141421)
        self.noise_rng = random.Random(seed + 173205)
        self.noise = {}
        self.last_hit, self.dash_until, self.dash_ready = -100.0, 0.0, 0.0
        self.vehicle_ready = self.emp_ready = self.intercept_ready = 0.0
        self.bio_until = self.bulk_until = self.hostile_until = self.drone_until = 0.0
        self.pulse_ready = self.mirror_ready = self.mirror_store = 0.0
        self.credit, self.streak, self.focus_target, self.focus_hits = 0.0, 0, None, 0.0
        self.first_hardware, self.first_evolution, self.full_build = None, None, None
        self.pick_times, self.hardware_times, self.trace, self.snapshots = [], [], [], {}
        self.low_impact_offers = self.close_choices = self.forced_mismatch = self.offers = 0
        self.late_low_impact = self.late_offers = self.rerolls_used = 0
        self.rerolls = spec.get("study_tuning", {}).get("rerolls", spec["loop"]["rerolls"])
        self.level_card_counts = Counter()
        self.fired_world_seconds = set()

    def introduce(self, t):
        second = int(t)
        if second in self.fired_world_seconds:
            return
        self.fired_world_seconds.add(second)
        for event in self.events.get(second, []):
            kind = event[1]
            if kind != "boss" and sum(e.kind == kind for e in self.enemies) >= self.world["enemies"][kind]["active_cap"]:
                continue
            self.enemies.append(spawn(event, self.world, second, self.seed, self.pressure))

    def movement(self, t):
        bonus = self.stats.get("biofuel_bonus", 0) if t < self.bio_until else 0
        return min(self.spec["stacking_and_limits"]["movement_maximum_base_multiplier"], self.stats["movement"] + bonus) * (1 - self.stats["capture_penalty"])

    def expire(self, t):
        movement = self.movement(t)
        self.enemies = [e for e in self.enemies if t - e.born < e.lifetime / (movement if e.kind != "boss" else 1)]
        if self.vehicle not in self.enemies:
            self.vehicle, self.vehicle_work = None, 0

    def reward(self, enemy, captured=False):
        rule = self.world["boss"] if enemy.kind == "boss" else self.world["enemies"][enemy.kind]
        self.xp += self.spec["xp_starting_proposal"][rule["xp_key"]]
        if captured:
            self.vehicle_captures += 1
            self.score += 5
        else:
            self.kills[enemy.kind] += 1
            self.score += rule["score"]

    def collect_vehicle(self, t):
        s, c = self.stats, self.spec["effect_constants"]
        if self.vehicle is None and t >= self.vehicle_ready and s["beams"] >= 2:
            self.vehicle = next((e for e in self.enemies if e.kind in ("jeep", "tank")
                                 and e.hp / e.max_hp <= s.get(e.kind + "_threshold", 0)), None)
        if self.vehicle is None:
            return 0
        seconds = c["vehicle_base_capture_seconds"] / (s["capture_multiplier"] * (1 + s.get("vehicle_speed_bonus", 0)))
        if self.vehicle.kind == "tank":
            seconds = max(c["tank_min_capture_seconds"], seconds)
        self.vehicle_work += self.step / seconds
        if self.vehicle_work >= 1:
            self.enemies.remove(self.vehicle)
            self.reward(self.vehicle, captured=True)
            self.vehicle, self.vehicle_work = None, 0
            self.vehicle_ready = t + c["vehicle_cooldown_seconds"]
        return 2

    def capture(self, t):
        s, w, c = self.stats, self.world, self.spec["effect_constants"]
        slots = self.collect_vehicle(t)
        chunk = int(t // w["encounter_variation_every_seconds"])
        if chunk not in self.noise:
            self.noise[chunk] = self.noise_rng.uniform(*w["encounter_variation_range"])
        encounters = w["biological_encounters_per_second"] * self.density * self.noise[chunk] * self.movement(t)
        encounters *= (1 + s.get("radius_bonus", 0)) * (1 + w["vacuum_encounter_bonus"] * s.get("vacuum", False))
        multiplier = s["capture_multiplier"]
        multiplier += s.get("bulk_bonus", 0) if t < self.bulk_until else 0
        multiplier += s.get("hostile_bonus", 0) if t < self.hostile_until else 0
        seconds = max(self.spec["stacking_and_limits"]["capture_minimum_seconds"], self.spec["loop"]["baseline"]["capture_seconds"] / multiplier)
        uptime = min(1, w["base_capture_uptime"] + w["grace_uptime_per_second"] * s.get("grace_seconds", 0))
        capacity = max(0, s["beams"] - slots) / seconds * uptime
        rate = capacity * (1 - math.exp(-encounters / capacity)) if capacity else 0
        self.credit += rate * self.step
        completed = int(self.credit)
        self.credit -= completed
        if completed:
            self.complete_biological(t, completed)
        occupancy = min(s["beams"] - slots, rate * seconds / max(.01, uptime))
        return occupancy + (1 if slots else 0), max(0, s["beams"] - occupancy - slots)

    def complete_biological(self, t, count):
        s, c = self.stats, self.spec["effect_constants"]
        previous = self.captures
        self.captures += count
        self.streak += count
        self.xp += count * self.spec["xp_starting_proposal"]["biological_capture_xp"]
        streak_bonus = min(s.get("streak_cap", 0), (self.streak // 10) * s.get("streak_step", 0))
        self.score += count * (s["score_multiplier"] + streak_bonus)
        repairs = self.captures // c["repair_every_captures"] - previous // c["repair_every_captures"]
        self.hull = min(s["max_hull"], self.hull + repairs * s.get("repair_hull", 0))
        shield_gain = min(self.spec["stacking_and_limits"]["capture_shield_restoration_per_second_cap"] * self.step,
                          count * s.get("capture_shield", 0))
        self.shield = min(s["max_shield"], self.shield + shield_gain)
        if s.get("biofuel_bonus"):
            self.bio_until = t + c["biofuel_duration_seconds"]
        if s.get("bulk_bonus"):
            self.bulk_until = t + c["bulk_duration_seconds"]
        if "Employee Benefits" in self.build.evolutions:
            self.drone_until = t + c["drone_capture_bonus_seconds"]
        for _ in range(count):
            soldier = next((e for e in self.enemies if e.kind == "soldier"), None)
            if soldier is not None and self.capture_rng.random() < self.world["soldier_fraction_of_captures"]:
                self.enemies.remove(soldier)  # already awarded capture XP; never also award kill XP
                if s.get("emp_duration", 0) and t >= self.emp_ready:
                    for enemy in self.enemies[:4]:
                        if enemy.kind != "boss":
                            enemy.disabled_until = t + s["emp_duration"]
                    self.emp_ready = t + c["emp_cooldown_seconds"]

    def distribute_damage(self, damage, t):
        for enemy in list(self.enemies):
            if enemy is self.vehicle or damage <= 0:
                continue
            spent = min(enemy.hp, damage)
            enemy.hp -= spent
            damage -= spent
            if enemy.hp <= 0:
                self.enemies.remove(enemy)
                self.reward(enemy)
                if enemy.kind in ("jeep", "tank") and self.stats.get("hostile_bonus"):
                    self.hostile_until = t + self.spec["effect_constants"]["hostile_duration_seconds"]

    def pulse(self, t):
        s, c = self.stats, self.spec["effect_constants"]
        if not s.get("pulse_damage") or t < self.pulse_ready:
            return
        storm = "Storm Drain" in self.build.evolutions
        radius = 6 if storm else s["pulse_radius"]
        limit = max(1, int(radius * self.world["pulse_targets_per_radius_metre"]))
        targets = [e for e in self.enemies if e is not self.vehicle][:limit]
        for enemy in targets:
            enemy.hp -= s["pulse_damage"] * s["damage_multiplier"]
            if storm:
                enemy.slowed_until = t + 1
        self.pulse_ready = t + s["pulse_cooldown"]
        self.remove_destroyed(t)

    def remove_destroyed(self, t):
        for enemy in list(self.enemies):
            if enemy.hp <= 0:
                self.enemies.remove(enemy)
                self.reward(enemy)
                if enemy.kind in ("jeep", "tank") and self.stats.get("hostile_bonus"):
                    self.hostile_until = t + self.spec["effect_constants"]["hostile_duration_seconds"]
                if self.vehicle is enemy:
                    self.vehicle, self.vehicle_work = None, 0

    def weapons(self, t, active, idle):
        s, c, b = self.stats, self.spec["effect_constants"], self.spec["loop"]["baseline"]
        targets = [e for e in self.enemies if e is not self.vehicle]
        fire = min(self.spec["stacking_and_limits"]["firing_rate_maximum_base_multiplier"],
                   s["fire_multiplier"] + min(s.get("active_fire_cap", 0), active * s.get("active_fire_bonus", 0)))
        damage_bonus = s["damage_multiplier"] + min(s.get("idle_damage_cap", 0), idle * s.get("idle_damage_bonus", 0))
        laser = b["laser_damage"] * b["laser_shots_per_second"] * (1 + c["added_laser_damage_multiplier"] * s.get("extra_lasers", 0)) * fire * damage_bonus
        if targets:
            if self.focus_target != targets[0].identity:
                self.focus_target, self.focus_hits = targets[0].identity, 0
            self.focus_hits += b["laser_shots_per_second"] * (1 + s.get("extra_lasers", 0)) * fire * self.step
            laser *= 1 + s.get("focus_bonus", 0) * min(1, self.focus_hits / c["focus_hits"])
        if "Office Shredder" in self.build.evolutions:
            laser *= 1.2
        drone = s.get("drones", 0) * c["drone_damage"] * c["drone_shots_per_second"] * fire * damage_bonus
        if "Employee Benefits" in self.build.evolutions and t < self.drone_until:
            drone *= 1 + c["drone_capture_damage_bonus"]
        self.distribute_damage((laser + drone) * self.step, t)
        for index, enemy in enumerate(targets[1:]):
            if enemy not in self.enemies:
                continue
            secondary = c["pierce_damage_multiplier"] * self.world["pierce_alignment_probability"] if index < s.get("pierce", 0) else 0
            secondary += s.get("arc_chance", 0) * c["arc_damage_multiplier"] if index < s.get("arc_targets", 0) else 0
            enemy.hp -= laser * secondary * self.step
        self.remove_destroyed(t)
        self.pulse(t)
        if "Mirrorball" in self.build.evolutions and t >= self.mirror_ready:
            self.distribute_damage(min(c["mirrorball_base_damage_cap"] * s["damage_multiplier"], self.mirror_store), t)
            self.mirror_store, self.mirror_ready = 0, t + c["mirrorball_cooldown_seconds"]

    def attacks(self, t):
        s, c = self.stats, self.spec["effect_constants"]
        dodge = self.skill["hit_multiplier"] / max(.5, self.movement(t))
        for enemy in list(self.enemies):
            while enemy.next_attack <= t:
                when = enemy.next_attack
                enemy.next_attack += enemy.attack_period
                accuracy, reflect, dash = (enemy.rng.random() for _ in range(3))
                slow = (.8 if enemy.kind == "boss" else .75) if when < enemy.slowed_until else 1
                if enemy is self.vehicle or when < enemy.disabled_until or accuracy >= enemy.accuracy * dodge * slow:
                    continue
                if when < self.dash_until:
                    continue
                if s.get("dash_duration") and when >= self.dash_ready and dash < self.skill["dash_success"]:
                    self.dash_until = when + s["dash_duration"]
                    self.dash_ready = when + s["dash_cooldown"]
                    continue
                if "Employee Benefits" in self.build.evolutions and when >= self.intercept_ready and self.shield >= c["drone_intercept_shield_cost"] and enemy.kind != "boss":
                    self.shield -= c["drone_intercept_shield_cost"]
                    self.intercept_ready = when + c["drone_intercept_cooldown_seconds"]
                    continue
                if self.shield > 0 and reflect < s.get("reflect_chance", 0):
                    enemy.hp -= enemy.damage
                    self.mirror_store += enemy.damage
                    continue
                absorbed = min(self.shield, enemy.damage)
                self.shield -= absorbed
                self.hull -= enemy.damage - absorbed
                if enemy.damage > absorbed:
                    self.streak = 0
                self.last_hit = t
                if self.hull <= 0:
                    return
        self.remove_destroyed(t)

    def offer_is_mismatch(self, cards):
        preferred = POLICIES[self.policy][0]
        return bool(preferred) and all(c["kind"] == "hardware" and c["family"] not in self.build.hardware and c["family"] not in preferred for c in cards)

    def level_up(self, t):
        fraction = self.hull / self.stats["max_hull"]
        cards = draw(self.spec, self.build, self.level, fraction, self.draft_rng)
        if self.rerolls and self.offer_is_mismatch(cards):
            self.rerolls -= 1
            self.rerolls_used += 1
            cards = draw(self.spec, self.build, self.level, fraction, self.draft_rng)
        values = [evaluate(c, self.build, self.spec, self.world, self.policy, fraction) for c in cards]
        self.forced_mismatch += self.offer_is_mismatch(cards)
        self.offers += 1
        self.low_impact_offers += not any(meaningful for _, meaningful in values)
        self.late_offers += t >= 600
        self.late_low_impact += t >= 600 and not any(meaningful for _, meaningful in values)
        ordered = sorted(v for v, _ in values)
        self.close_choices += ordered[-1] > 0 and ordered[-2] >= .85 * ordered[-1]
        index = self.draft_rng.randrange(3) if self.policy == "Random" else max(range(3), key=lambda i: values[i][0])
        selected = cards[index]
        previous_evolutions = len(self.build.evolutions)
        hull_gain, shield_gain = self.build.apply(selected, self.spec)
        self.stats = self.build.stats(self.spec)
        self.hull = min(self.stats["max_hull"], self.hull + hull_gain)
        self.shield = min(self.stats["max_shield"], self.shield + shield_gain)
        if selected["family"] == "emergency_repairs":
            self.hull = min(self.stats["max_hull"], self.hull + self.stats["max_hull"] * selected["value"] / 100)
        self.record_pick(t, cards, selected, previous_evolutions)
        self.level += 1

    def record_pick(self, t, cards, selected, previous_evolutions):
        self.pick_times.append(t)
        self.level_card_counts[selected["family"]] += 1
        if selected["kind"] == "hardware":
            self.hardware_times.append(t)
            if self.first_hardware is None:
                self.first_hardware = t
        if len(self.build.evolutions) > previous_evolutions and self.first_evolution is None:
            self.first_evolution = t
        if self.full_build is None and len(self.build.hardware) == self.spec["loop"]["hardware_slots"] and all(v == 3 for v in self.build.hardware.values()):
            self.full_build = t
        if self.keep_trace:
            self.trace.append({"second":t,"level":self.level + 1,"hull":round(self.hull, 2),"cards":cards,"picked":selected,"evolutions":list(self.build.evolutions)})

    def play(self):
        t = 0
        xp_rule = self.spec["xp_starting_proposal"]
        while t < self.horizon and self.hull > 0:
            self.introduce(t)
            t = round(t + self.step, 6)
            self.expire(t)
            if t - self.last_hit >= self.stats["shield_delay"]:
                self.shield = min(self.stats["max_shield"], self.shield + self.stats["shield_regen"] * self.step)
            active, idle = self.capture(t)
            self.weapons(t, active, idle)
            self.attacks(t)
            self.xp += xp_rule["survival_xp_per_second"] * self.step
            while self.hull > 0 and self.xp >= xp_rule["next_level_base_xp"] + xp_rule["next_level_per_current_level_xp"] * self.level:
                self.xp -= xp_rule["next_level_base_xp"] + xp_rule["next_level_per_current_level_xp"] * self.level
                self.level_up(t)
            if t in (300, 600, 1200, 1800, 3600):
                self.snapshots[str(int(t))] = {"alive":self.hull > 0,"level":self.level,"captures":self.captures,"hardware":dict(self.build.hardware),"evolutions":list(self.build.evolutions)}
        return self.result(t)

    def result(self, t):
        intervals = [self.pick_times[0]] + [b - a for a, b in zip(self.pick_times, self.pick_times[1:])] if self.pick_times else []
        module_times = [0] + self.hardware_times + [t]
        return {"seed":self.seed,"policy":self.policy,"seconds":t,"censored":self.hull > 0,"horizon":self.horizon,
                "level":self.level,"captures":self.captures,"score":round(self.score, 2),"kills":dict(self.kills),"vehicle_captures":self.vehicle_captures,
                "hardware":dict(self.build.hardware),"research":dict(self.build.research),"evolutions":list(self.build.evolutions),
                "first_pick":self.pick_times[0] if self.pick_times else None,"first_hardware":self.first_hardware,"first_evolution":self.first_evolution,"full_build":self.full_build,
                "pick_intervals":intervals,"max_hardware_gap_seconds":max(b - a for a,b in zip(module_times,module_times[1:])),
                "offers":self.offers,"low_impact_offers":self.low_impact_offers,"late_offers":self.late_offers,"late_low_impact":self.late_low_impact,
                "close_choices":self.close_choices,"forced_mismatch":self.forced_mismatch,"rerolls_used":self.rerolls_used,
                "outside_preference_slots":sum(k not in POLICIES[self.policy][0] for k in self.build.hardware) if POLICIES[self.policy][0] else None,
                "selected_cards":dict(self.level_card_counts),"snapshots":self.snapshots,"trace":self.trace}
