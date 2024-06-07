#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H // Include FreeType header files
#include <hb-ft.h>
#define BP_ONE_TO_RULE_THEM_ALL 1

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
        0,                                      // same as character height
        utlz::to_ft_float(font_size * content_scale), // char_height in 1/64th of points
        logic_dpi_x,                            // horizontal device resolution
        logic_dpi_y                             // vertical device resolution
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

struct Shaper
{
    using Buffer = hb_buffer_t*;

    Shaper()
    {
        buffer = hb_buffer_create();
    }

    ~Shaper()
    {
        hb_buffer_destroy(buffer);
    }

    struct GlyphInfo
    {
        hb_codepoint_t codepoint;
        hb_position_t x_offset;
        hb_position_t y_offset;
        hb_position_t x_advance;
        hb_position_t y_advance;
    };

    auto shape(const Text& text, const Font& font)
    {
        // If you don't know the direction, script, and language
        //        hb_buffer_guess_segment_properties(buffer);

        hb_buffer_set_direction(buffer, text.direction);
        hb_buffer_set_script(buffer, text.script);
        hb_buffer_set_language(buffer, hb_language_from_string(text.language.c_str(), -1));

        hb_buffer_add_utf8(buffer, text.text.c_str(), -1, 0, -1);

        // beef
        hb_shape(font.unicode, buffer, nullptr, 0);

        unsigned int glyph_n = hb_buffer_get_length(buffer);
        auto* glyph_info     = hb_buffer_get_glyph_infos(buffer, nullptr);
        auto* glyph_pos      = hb_buffer_get_glyph_positions(buffer, nullptr);

        utlz::print_glyph_infos(glyph_info, glyph_n);
        utlz::print_glyph_positions(glyph_pos, glyph_n);

        return std::tuple { glyph_info, glyph_pos, glyph_n };
    }

    Buffer buffer;
};


} // namespace typesetting