"""Short-run director and optional revive, reusing the original encounter model."""
import copy
from .run import Run
from .world import spawn


def short_world(source, plan):
    world = copy.deepcopy(source)
    d = plan["director"]
    # The inherited spawn formula starts growth at minute five. Override in
    # ShortRun.introduce so this plan's growth starts explicitly at minute two.
    world["enemy_health_growth_per_minute_after_5"] = 0
    world["enemy_damage_growth_per_minute_after_5"] = 0
    for kind, rule in world["enemies"].items():
        rule["initial_rate"] = d[kind + "_rate"]
        rule["rate_growth_per_minute"] = d["rate_growth_per_minute"]
        rule["rate_multiplier_cap"] = d["rate_multiplier_cap"]
    world["enemies"]["jeep"]["start_second"] = 30
    world["enemies"]["tank"]["start_second"] = 100
    boss = world["boss"]
    for source_key, target_key in (("boss_first_second", "first_second"),
                                   ("boss_interval_seconds", "interval_seconds"),
                                   ("boss_base_hp", "base_hp"),
                                   ("boss_health_index_coefficient", "health_index_coefficient"),
                                   ("boss_damage", "damage")):
        boss[target_key] = d[source_key]
    return world


class ShortRun(Run):
    def __init__(self, spec, world, plan, seed, policy, *, revive=False, **kwargs):
        super().__init__(spec, world, seed, policy, pressure=plan["director"]["pressure"], **kwargs)
        self.plan, self.allow_revive = plan, revive
        self.revives_used, self.revive_at = 0, None

    def introduce(self, t):
        second = int(t)
        if second in self.fired_world_seconds:
            return
        self.fired_world_seconds.add(second)
        late = max(0, second / 60 - 2)
        for event in self.events.get(second, []):
            kind = event[1]
            if kind != "boss" and sum(e.kind == kind for e in self.enemies) >= self.world["enemies"][kind]["active_cap"]:
                continue
            enemy = spawn(event, self.world, second, self.seed, self.pressure)
            enemy.hp *= 1 + late * self.plan["director"]["health_growth_after_2min"]
            enemy.max_hp = enemy.hp
            enemy.damage *= 1 + late * self.plan["director"]["damage_growth_after_2min"]
            self.enemies.append(enemy)

    def try_revive(self, t):
        rule = self.plan["revive"]
        if (self.hull > 0 or not self.allow_revive or self.revives_used >= rule["maximum_per_run"]
                or t < rule["minimum_run_seconds"]):
            return False
        self.revives_used += 1
        self.revive_at = t
        self.hull = self.stats["max_hull"] * rule["hull_fraction"]
        self.shield = rule["shield_after"]
        self.enemies = [enemy for enemy in self.enemies if enemy.kind == "boss"]
        self.vehicle, self.vehicle_work = None, 0
        self.dash_until = t + rule["invulnerability_seconds"]
        self.last_hit = t
        return True

    def play(self):
        t = 0
        xp = self.spec["xp_starting_proposal"]
        while t < self.horizon and self.hull > 0:
            self.introduce(t)
            t = round(t + self.step, 6)
            self.expire(t)
            if t - self.last_hit >= self.stats["shield_delay"]:
                self.shield = min(self.stats["max_shield"], self.shield + self.stats["shield_regen"] * self.step)
            active, idle = self.capture(t)
            self.weapons(t, active, idle)
            self.attacks(t)
            self.try_revive(t)
            self.xp += xp["survival_xp_per_second"] * self.step
            threshold = lambda: xp["next_level_base_xp"] + xp["next_level_per_current_level_xp"] * self.level
            while self.hull > 0 and self.xp >= threshold():
                self.xp -= threshold()
                self.level_up(t)
            if t in (60, 180, 300, 480, 600, 900, 1800):
                self.snapshots[str(int(t))] = {"alive": self.hull > 0, "level": self.level,
                                              "captures": self.captures, "hardware": dict(self.build.hardware),
                                              "evolutions": list(self.build.evolutions)}
        return dict(self.result(t), revives_used=self.revives_used, revive_at=self.revive_at)
