#pragma once

#include <iostream>
#include <locale>
#include <sstream>
#include <cuchar>
#include <utf8.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define LOG_FUNC std::cout << __PRETTY_FUNCTION__ << "\n";

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

using namespace utf8;

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
    auto print_opengl_version = [&]
    { printf("OpenGL version: %d.%d\n", GLVersion.major, GLVersion.minor); };
    auto print_freetype_version = [&]
    {
        FT_Int major, minor, patch;
        FT_Library_Version(library.get(), &major, &minor, &patch);
        printf("FreeType version: %d.%d.%d\n", major, minor, patch);
    };

    printf("GLFW version: %s\n", glfwGetVersionString());
    print_freetype_version();
    print_opengl_version();
    printf("HarfBuzz version: %s\n", hb_version_string());
}

// Function to check if a character is supported by the font
inline bool is_char_supported(FT_Face& face, char32_t character)
{
    return FT_Get_Char_Index(face, character) != 0;
}

// Function to check if all characters in a UTF-8 buffer are supported by the font
bool are_all_chars_supported(FT_Face& ftFace, std::string& u8_buffer)
{
    std::u32string u32_buffer = utf8to32(u8_buffer);

    for (char32_t character : u32_buffer)
    {
        if (!is_char_supported(ftFace, character))
        {
            return false;
        }
    }

    return true;
}

// Function to format an integer with space as thousands separator
std::string format_with_space(long long number)
{
    // Custom numpunct facet to use space as thousands separator
    struct SpaceSep : std::numpunct<char>
    {
        char do_thousands_sep() const override { return ' '; }

        std::string do_grouping() const override { return "\3"; }
    };

    std::stringstream ss;
    ss.imbue(std::locale(std::locale::classic(), new SpaceSep));
    ss << number;
    return ss.str();
}

const auto metric_time_unit = [](const auto value) {
    return value < 1'000'000L ? "ms" : value < 1'000'000'000L ? "µs" : "ns";
};
// wrap a function into a timed context
template <typename Unit>
auto time_in_ = [](std::string label, auto func, auto&... args)
{
    using Clock                 = std::chrono::high_resolution_clock;
    const auto then             = Clock::now();
    auto result                 = func(args...);
    const auto elapsed          = Clock::now() - then;
    const auto elapsed_in_units = std::chrono::duration_cast<Unit>(elapsed);

    std::cout << std::setw(12) << label << ": " << std::setw(8)
              << utlz::format_with_space((elapsed_in_units).count()) << " "
              << metric_time_unit(typename Unit::period().den) << "\n";

    return std::move(result);
};
auto time_in_s    = time_in_<std::chrono::seconds>;
auto time_in_ms   = time_in_<std::chrono::milliseconds>;
auto time_in_mcrs = time_in_<std::chrono::microseconds>;
auto time_in_ns   = time_in_<std::chrono::nanoseconds>;

namespace stopwatch
{
struct Measurement
{
    using Clock = std::chrono::high_resolution_clock;

    Measurement(Clock::duration& _sum, int& _n)
        : sum(_sum)
    {
        _n++;
        then = Clock::now();
    }

    ~Measurement() { sum += Clock::now() - then; }

    Clock::time_point then;
    Clock::duration& sum;
};

struct Aggregate
{
    explicit Aggregate(std::string _label)
        : label(std::move(_label))
    {
    }

    ~Aggregate() { print_stats(); }

    void print_stats() const
    {
        auto ticks_total = sum.count();
        auto time_unit = metric_time_unit(Measurement::Clock::period::den);

        std::stringstream total_in_units;
        total_in_units << format_with_space(sum.count()) << " " << time_unit;

        std::stringstream avg_in_units;
        avg_in_units << format_with_space(float(sum.count()) / float(measurements)) << " " << time_unit;

        // clang-format off
        std::cout
            << std::setw(15) << label
            << std::setw(6) << " - total: " << std::setw(15) << total_in_units.str()
            << std::setw(8) << " avg: "     << std::setw(15) << avg_in_units.str()
            << "\n";
        // clang-format on
    }

    Measurement::Clock::duration sum { 0 };
    std::string label;
    int measurements = 0;
};
} // namespace stopwatch
#ifndef STOPWATCH
    #define STOPWATCH(label)                                          \
        static utlz::stopwatch::Aggregate stopwatch_aggregate(label); \
        utlz::stopwatch::Measurement stopwatch_measurement(           \
            stopwatch_aggregate.sum,                                  \
            stopwatch_aggregate.measurements                          \
        );
#endif
} // namespace utlz