#pragma once
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/style_scene.h>
namespace human_resources::product::detail {
void append_jeep(presentation::StyleFrame& frame, const presentation::StyleInstance& actor,
    const gameplay::EntityView& entity, gameplay::Vec2 aim, float wheel_angle, float flash_age);
void append_tank(presentation::StyleFrame& frame, const presentation::StyleInstance& actor,
    const gameplay::EntityView& entity, gameplay::Vec2 aim, float flash_age,
    const gameplay::BossCombatView* boss = nullptr, bool reduced_motion = false);
}
