#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H // Include FreeType header files
#include <hb-ft.h>
#define BP_ONE_TO_RULE_THEM_ALL 1
#include <utf8string.hpp>
#include <initializer_list>
#include <codecvt>
#include <locale>

#include "scope_guards.h"
#include "blueprints.h"
#include "types.h"
#include "library.h"
#include "utlz.h"

/**
 * Typesetting is the composition of text for publication, display, or distribution by means of
 * arranging physical type (or sort) in mechanical systems or glyphs in digital systems representing
 * characters (letters and other symbols).
 */
namespace typesetting
{

struct Font
{
    using Id = unsigned int;
    //    using Face        = hb_face_t*;
    using Face        = FT_Face;
    using UnicodeType = hb_font_t*;
    using Blob        = hb_blob_t*;

    Id id;
    Face face;
    UnicodeType unicode;
    Blob blob;
    float font_size;
    float content_scale;
};

unsigned int gen_id()
{
    static unsigned int id = 0;
    return id++;
}

// Set font_size and content_scale before calling
std::optional<Font>
create_font(Library* resources, const char* font_file, const int font_size, const float content_scale)
{
    static int next_id = 0;
    Font font;

    // Simple harfbuzz example
#if 0
    font.blob    = hb_blob_create_from_file(font_file); /* or hb_blob_create_from_file_or_fail() */
    font.face    = hb_face_create(font.blob, 0);
    font.unicode = hb_font_create(font.face);

    return { font };
#endif
    if (FT_New_Face(resources->library, font_file, 0, &font.face))
        return std::nullopt;

    auto face_scope_dtor = scope_guards::on_scope_exit_(
        [&]
        {
            FT_Done_Face(font.face);
            font.face = nullptr;
        }
    );

#if defined(_WIN32)
    const int logic_dpi_x = 96;
    const int logic_dpi_y = 96;
#elif defined(__APPLE__)
    const int logic_dpi_x = 72;
    const int logic_dpi_y = 72;
#else
    #error "not implemented"
#endif

    FT_Set_Char_Size(
        font.face,
        0,                                            // same as character height
        utlz::to_ft_float(font_size * content_scale), // char_height in 1/64th of points
        logic_dpi_x,                                  // horizontal device resolution
        logic_dpi_y                                   // vertical device resolution
    );

    //    FT_Set_Char_Size(face, 0, 1000, 0, 0);
    //    hb_font_t *font = hb_ft_font_create(face);
    font.unicode = hb_ft_font_create_referenced(font.face);
    hb_ft_font_set_funcs(font.unicode);

    face_scope_dtor.dismiss();

    font.id            = gen_id();
    font.font_size     = font_size;
    font.content_scale = content_scale;
    // TODO: bold and italics..
    return { font };
}

void destroy_font(Font& font)
{
    if (font.unicode)
    {
        hb_font_destroy(font.unicode);
        font.unicode = nullptr;
    }

    if (font.face)
    {
        // TODO: not necessary when using hb_create_font_referenced
        FT_Done_Face(font.face);
        font.face = nullptr;
    }

    //    if (font.blob)
    //        hb_blob_destroy(font.blob);
}

// Declarations of Text
using namespace gfx;

struct Text
{
    std::string text;
    std::string language;
    hb_script_t script;
    hb_direction_t direction;
};

inline char32_t get_unicode_char(char* chars) { return {}; }

inline bool is_char_supported(Font& font, char32_t character)
{
    if (FT_Get_Char_Index(font.face, character) == 0)
        return false;

    return true;
}

namespace hb_helpers
{
namespace Feature
{
const hb_tag_t KernTag = HB_TAG('k', 'e', 'r', 'n'); // kerning operations
const hb_tag_t LigaTag = HB_TAG('l', 'i', 'g', 'a'); // standard ligature substitution
const hb_tag_t CligTag = HB_TAG('c', 'l', 'i', 'g'); // contextual ligature substitution

static hb_feature_t LigatureOff = { LigaTag, 0, 0, std::numeric_limits<unsigned int>::max() };
static hb_feature_t LigatureOn  = { LigaTag, 1, 0, std::numeric_limits<unsigned int>::max() };
static hb_feature_t KerningOff  = { KernTag, 0, 0, std::numeric_limits<unsigned int>::max() };
static hb_feature_t KerningOn   = { KernTag, 1, 0, std::numeric_limits<unsigned int>::max() };
static hb_feature_t CligOff     = { CligTag, 0, 0, std::numeric_limits<unsigned int>::max() };
static hb_feature_t CligOn      = { CligTag, 1, 0, std::numeric_limits<unsigned int>::max() };
} // namespace Feature

struct GlyphInfo
{
    hb_codepoint_t codepoint;
    hb_position_t x_offset;
    hb_position_t y_offset;
    hb_position_t x_advance;
    hb_position_t y_advance;
};

struct Shaper
{

    using Buffer = hb_buffer_t*;

    Shaper() {}

    ~Shaper() { hb_buffer_destroy(buffer); }

    auto shape(const Text& text, const Font& font)
    {
        if (buffer)
            hb_buffer_destroy(buffer);

        buffer = hb_buffer_create();

        // If you don't know the direction, script, and language
        if (text.language.empty() || text.language == "emoji" || text.language == "maths")
            hb_buffer_guess_segment_properties(buffer);
        else
        {
            hb_buffer_set_direction(buffer, text.direction);
            hb_buffer_set_script(buffer, text.script);
            hb_buffer_set_language(buffer, hb_language_from_string(text.language.c_str(), -1));
        }

        hb_buffer_add_utf8(buffer, text.text.c_str(), -1, 0, -1);

        // beef
        hb_shape(font.unicode, buffer, nullptr, 0);

        unsigned int glyph_n = hb_buffer_get_length(buffer);
        auto* glyph_info     = hb_buffer_get_glyph_infos(buffer, nullptr);
        auto* glyph_pos      = hb_buffer_get_glyph_positions(buffer, nullptr);

        //        utlz::print_glyph_infos(glyph_info, glyph_n);
        //        utlz::print_glyph_positions(glyph_pos, glyph_n);

        return std::tuple { glyph_info, glyph_pos, glyph_n };
    }

    void add_feature(hb_feature_t feature) { features.emplace_back(feature); }

    std::vector<hb_feature_t> features;
    Buffer buffer = nullptr;
};

struct FontShaper
{
    using Buffer = hb_buffer_t*;

    FontShaper(std::string name, Font& font_ref, std::string&& language_def, hb_script_t script_def, hb_direction_t direction_def)
        : font(font_ref)
        , language(language_def)
        , name(std::move(name))
        , script(script_def)
        , direction(direction_def)
    {
    }

    ~FontShaper() { hb_buffer_destroy(buffer); }

    auto shape(const uint32_t * text, const int text_length, const int offset, const int n)
    {
        if (buffer)
            hb_buffer_destroy(buffer);

        buffer = hb_buffer_create();

        // If you don't know the direction, script, and language
        if (language.empty())
        {
            hb_buffer_guess_segment_properties(buffer);
            std::cout << "Guessing for: " << offset << " :: " << n << "\n";
        }
        else
        {
            hb_buffer_set_direction(buffer, direction);
            hb_buffer_set_script(buffer, script);
            hb_buffer_set_language(buffer, hb_language_from_string(language.c_str(), -1));
        }

        hb_buffer_add_utf32(buffer, text, text_length, offset, n);

        // beef
        hb_shape(font.unicode, buffer, nullptr, 0);

        unsigned int glyph_n = hb_buffer_get_length(buffer);
        auto* glyph_info     = hb_buffer_get_glyph_infos(buffer, nullptr);
        auto* glyph_pos      = hb_buffer_get_glyph_positions(buffer, nullptr);

        //        utlz::print_glyph_infos(glyph_info, glyph_n);
        //        utlz::print_glyph_positions(glyph_pos, glyph_n);

        return std::tuple { glyph_info, glyph_pos, glyph_n };
    }

    auto shape(const std::string& text)
    {
        if (buffer)
            hb_buffer_destroy(buffer);

        buffer = hb_buffer_create();

        // If you don't know the direction, script, and language
        if (language.empty())
            hb_buffer_guess_segment_properties(buffer);
        else
        {
            hb_buffer_set_direction(buffer, direction);
            hb_buffer_set_script(buffer, script);
            hb_buffer_set_language(buffer, hb_language_from_string(language.c_str(), -1));
        }

        hb_buffer_add_utf8(buffer, text.c_str(), -1, 0, -1);

        // beef
        hb_shape(font.unicode, buffer, nullptr, 0);

        unsigned int glyph_n = hb_buffer_get_length(buffer);
        auto* glyph_info     = hb_buffer_get_glyph_infos(buffer, nullptr);
        auto* glyph_pos      = hb_buffer_get_glyph_positions(buffer, nullptr);

        //        utlz::print_glyph_infos(glyph_info, glyph_n);
        //        utlz::print_glyph_positions(glyph_pos, glyph_n);

        return std::tuple { glyph_info, glyph_pos, glyph_n };
    }

    void add_feature(hb_feature_t feature) { features.emplace_back(feature); }

    Font font;
    std::string language;
    std::string name;
    hb_script_t script;
    hb_direction_t direction;
    Buffer buffer = nullptr;
    std::vector<hb_feature_t> features;
};
} // namespace hb_helpers

struct ShaperChunk
{
    std::vector<char32_t>* text_ptr = nullptr;

    struct
    {
        int start = 0;
        int end   = 0;
        int length = 0;
    } range;

    hb_helpers::FontShaper* shaper = nullptr;
    int index                      = 0;
};

// gpt4 pitkästä tavarasta
std::string utf32_to_utf8(const char32_t* utf32, size_t n)
{
    std::string utf8;
    utf8.reserve(n * 4); // Reserve space to avoid multiple allocations.

    for (size_t i = 0; i < n; ++i)
    {
        char32_t ch = utf32[i];

        if (ch <= 0x7F)
        {
            // 1-byte sequence: 0xxxxxxx
            utf8.push_back(static_cast<char>(ch));
        }
        else if (ch <= 0x7FF)
        {
            // 2-byte sequence: 110xxxxx 10xxxxxx
            utf8.push_back(static_cast<char>(0xC0 | ((ch >> 6) & 0x1F)));
            utf8.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
        else if (ch <= 0xFFFF)
        {
            // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
            utf8.push_back(static_cast<char>(0xE0 | ((ch >> 12) & 0x0F)));
            utf8.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
        else if (ch <= 0x10FFFF)
        {
            // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            utf8.push_back(static_cast<char>(0xF0 | ((ch >> 18) & 0x07)));
            utf8.push_back(static_cast<char>(0x80 | ((ch >> 12) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            utf8.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
        else
        {
            // Invalid UTF-32 character, handle error
            // For now, we'll just skip invalid characters
        }
    }

    return utf8;
}

// codestral vastaava
std::string utf32_to_utf8_codestral(const char32_t* input, size_t n)
{
    if (input == nullptr || n == 0)
    {
        return ""; // Return an empty string if the input is null or has zero length.
    }

    // Deprecated without fallback:
    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0618r0.html
    std::wstring_convert<std::codecvt_utf8_utf16<char32_t>, char32_t> converter;
    std::u32string utf32(input, n);   // Create a UTF-32 encoded string from the input pointer and
                                      // length.
    return converter.to_bytes(utf32); // Convert it to UTF-8 encoding and return as a std::string.
}

// debug helper
inline std::string to_string(ShaperChunk& chunk)
{
    std::string text = utf32_to_utf8_codestral(
        chunk.text_ptr->data() + chunk.range.start,
        chunk.range.end - chunk.range.start
    );
    std::stringstream ss;
    ss << "\nIndex: " << chunk.index;
    ss << "\nText: " << text;
    ss << "\nStart: " << chunk.range.start;
    ss << "\nEnd: " << chunk.range.end;
    ss << "\nLength: " << chunk.range.length;
    ss << "\nShaper: " << chunk.shaper->name;
    ss << "\n";
    return ss.str();
}

// split to chunks
// process

} // namespace typesetting