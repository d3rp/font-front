#pragma once

#include <glm/glm.hpp>

namespace gfx
{

using Colour = glm::vec3;
using Point  = glm::vec2;

namespace colours
{
Colour red { 1.0f, 0.0f, 0.0f };
Colour green { 0.0f, 1.0f, 0.0f };
Colour blue { 0.0f, 0.0f, 1.0f };
Colour black { 0.0f, 0.0f, 0.0f };
Colour white { 1.0f, 1.0f, 1.0f };
} // namespace colours

struct Aabb
{
    Point p0;
    Point p1;
};

} // namespace gfx