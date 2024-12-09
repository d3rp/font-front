#pragma once

namespace typesetting
{

static const char* shader_string_for_vertices = R"SHADER_INPUT(
#version 330 core
layout (location = 0) in vec4 vertex; // pos, tex
out vec2 TexCoords;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)SHADER_INPUT";

static const char* shader_string_for_fragments = R"SHADER_INPUT(
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;

void main()
{
    float y = texture(text, TexCoords).r;
    if (y < 0.5)
        discard;
    y *= 1.45;
    y = 1.0 / (1.0 + exp(-20.0 * (y - 0.76)));

    vec4 sampled = vec4(1.0, 1.0, 1.0, y);
    color = vec4(textColor, 1.0) * sampled;
}
)SHADER_INPUT";

} // namespace typesetting