#pragma once
#include <human_resources/product/presentation.h>
namespace human_resources::product::detail {
// Logical-point circles use the same radial throw as semantic input on all layouts.
void append_thumbstick(Interface& output, const View& view);
}
