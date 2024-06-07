#pragma once

#include "text.h"

// Hash function for GlyphKey
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
template <>
struct hash<pair<unsigned int, unsigned int>>
{
    size_t operator()(const pair<unsigned int, unsigned int>& p) const
    {
        size_t seed = 0;
        ::hash_combine(seed, p.first);
        ::hash_combine(seed, p.second);
        return seed;
    }
};
} // namespace std

namespace utlz
{

float to_float(FT_F26Dot6 weird_number) { return float(weird_number) * 1.0f / 64.0f; }

FT_F26Dot6 to_ft_float(float value) { return FT_F26Dot6(value * 64.0f); }

void print_glyph_infos(const hb_glyph_info_t* glyphs, unsigned int length)
{
    for (unsigned int i = 0; i < length; ++i)
    {
        printf("Glyph %u:\n", i);
        printf("\tCodepoint: %u\n", glyphs[i].codepoint);
        printf("\tMask: %u\n", glyphs[i].mask);
        printf("\tCluster: %u\n", glyphs[i].cluster);
        printf("\tVar1: %d\n", glyphs[i].var1.i32);
        printf("\tVar2: %d\n", glyphs[i].var2.i32);
    }
}

void print_glyph_positions(const hb_glyph_position_t* positions, unsigned int length)
{
    for (unsigned int i = 0; i < length; ++i)
    {
        printf("Position %u:\n", i);
        printf("\tX Advance: %d\n", positions[i].x_advance);
        printf("\tY Advance: %d\n", positions[i].y_advance);
        printf("\tX Offset: %d\n", positions[i].x_offset);
        printf("\tY Offset: %d\n", positions[i].y_offset);
        printf("\tVar: %d\n", positions[i].var.i32);
    }
}
static void print_versions(typesetting::Library& library)
{
    auto print_opengl_version = [&]{
        printf("OpenGL version: %d.%d\n", GLVersion.major, GLVersion.minor);
    };
    auto print_freetype_version = [&] {
        FT_Int major, minor, patch;
        FT_Library_Version(library.get(), &major, &minor, &patch);
        printf("FreeType version: %d.%d.%d\n", major, minor, patch);
    };

    printf("GLFW version: %s\n", glfwGetVersionString());
    print_freetype_version();
    print_opengl_version();
    printf("HarfBuzz version: %s\n", hb_version_string());
}
} // namespace utlz