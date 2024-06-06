#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H // Include FreeType header files
#include <hb-ft.h>
#define BP_ONE_TO_RULE_THEM_ALL 1

#include "scope_guards.h"
#include "blueprints.h"
#include "types.h"
#include "skyline_binpack.h"

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

/**
 * Typesetting is the composition of text for publication, display, or distribution by means of
 * arranging physical type (or sort) in mechanical systems or glyphs in digital systems representing
 * characters (letters and other symbols).
 */
namespace typesetting
{

float to_float(FT_F26Dot6 weird_number) { return float(weird_number) * 1.0f / 64.0f; }

FT_F26Dot6 to_ft_float(float value) { return FT_F26Dot6(value * 64.0f); }

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

// Memory handling and other resource mgmnt for fonts
struct Resources : blueprints::InitDestroy<Resources>
{
    struct
    {
        bool init()
        {
            if (p.library)
                return true;

            if (FT_Init_FreeType(&p.library))
                return false;

            return true;
        }

        bool destroy()
        {
            FT_Done_FreeType(p.library);
            p.library = nullptr;

            return true;
        }

        Resources& p;
    } impl { *this };

    FT_Library library; // FreeType library instance
};

unsigned int gen_id()
{
    static unsigned int id = 0;
    return id++;
}

// Set font_size and content_scale before calling
std::optional<Font>
create_font(Resources* resources, const char* font_file, const int font_size, const float content_scale)
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
        to_ft_float(font_size * content_scale), // char_height in 1/64th of points
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
        //        hb_face_destroy(font.face);
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
    Font::Id font_id;
    Colour colour;
    Aabb bounds;
    std::string text;
};

struct HarfbuzzAdaptor
{
    struct GlyphInfo
    {
        hb_codepoint_t codepoint;
        hb_position_t x_offset;
        hb_position_t y_offset;
        hb_position_t x_advance;
        hb_position_t y_advance;
    };

    using Buffer = hb_buffer_t;

    // BufferInfo?
    struct UnicodeTranslation
    {
        hb_direction_t direction;
        hb_script_t script;
        hb_language_t language;
    } translation;
};

using GlyphKey = std::pair<unsigned int, unsigned int>;

struct Glyph
{
    glm::ivec2 size;       // Size of glyph
    glm::ivec2 bearing;    // Offset from horizontal layout origin to left/top of glyph
    glm::ivec2 tex_offset; // Offset of glyph in texture atlas
    int tex_index;         // Texture atlas index
    unsigned int dyn_tex;  // Texture atlas generation
};

using GlyphCache = std::unordered_map<GlyphKey, Glyph>;

struct Atlas : blueprints::InitDestroy<Atlas>
{
    struct
    {
        bool init(uint16_t w, uint16_t h)
        {
            assert(w > 0);
            assert(h > 0);

            p.width  = w;
            p.height = h;

            p.bin_packer.Init(w, h);

            // TODO: uuwee
            p.data = (uint8_t*) calloc(w * h * 1, sizeof(uint8_t));
            if (!p.data)
                return false;

            // generate texture
            glGenTextures(1, &p.texture);
            glBindTexture(GL_TEXTURE_2D, p.texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

            // set texture options
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);

            return true;
        }

        bool destroy()
        {
            free(p.data);
            glDeleteTextures(1, &p.texture);
            return true;
        }

        Atlas& p;
    } impl { *this };

    void clear()
    {
        assert(width > 0);
        assert(height > 0);
        assert(data != nullptr);
        assert(texture != 0);

        bin_packer.Init(width, height);

        memset(data, 0, width * height * 1 * sizeof(uint8_t));

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, 0);
    };

    std::optional<Point> add_region(const Glyph& glyph, const uint8_t* glyph_data)
    {
        auto glyph_w = glyph.size.x, glyph_h = glyph.size.y;
        assert(glyph_w > 0);
        assert(glyph_h > 0);
        assert(glyph_data != nullptr);

        assert(width > 0);
        assert(height > 0);
        assert(data != nullptr);
        assert(texture != 0);

        auto rect = bin_packer.Insert(glyph_w, glyph_h);
        if (rect.height <= 0)
            return std::nullopt;

        for (uint16_t i = 0; i < glyph_h; ++i)
            memcpy(data + ((rect.y + i) * width + rect.x), glyph_data + i * glyph_w, glyph_w);

        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, glyph_w, glyph_h, GL_RED, GL_UNSIGNED_BYTE, glyph_data);
        glBindTexture(GL_TEXTURE_2D, 0);

        return { { rect.x, rect.y } };
    }

    uint16_t width {};
    uint16_t height {};
    binpack::SkylineBinPack bin_packer;
    uint8_t* data {};
    unsigned int texture {};
};

} // namespace typesetting