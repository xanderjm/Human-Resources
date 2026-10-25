#pragma once
#include <human_resources/animation/animation.h>
namespace human_resources::animation::detail {
[[nodiscard]] Transform interpolate(const Transform& a, const Transform& b, float amount);
[[nodiscard]] Matrix matrix(const Transform& t);
[[nodiscard]] Matrix multiply(const Matrix& a, const Matrix& b);
[[nodiscard]] bool compose(const Asset& asset, Pose& pose);
[[nodiscard]] bool valid_transform(const Transform& t);
[[nodiscard]] bool finite_matrix(const Matrix& m);
} // namespace human_resources::animation::detail
