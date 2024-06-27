#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H // Include FreeType header files
#include <hb-ft.h>
#include <initializer_list>

extern "C"
{
#include <SheenBidi.h>
};

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

    struct Map
    {
        const hb_script_t _fallback_key = HB_SCRIPT_INVALID;

        void add(hb_script_t script, std::vector<Font*> fonts)
        {
            volatile static int _ = std::invoke(
                [&]
                {
                    db.emplace_back(nullptr);
                    return 0;
                }
            );
            Mapping m;
            m.start = db.size();
            for (auto& font : fonts)
                db.emplace_back(font);
            m.length = db.size() - m.start;
            _map.emplace(script, m);
        }

        void set_fallback(Font& font) { db.at(0) = &font; }

        void print_tag(hb_script_t script) const
        {
            char buf[5];
            hb_tag_to_string(script, buf);
            buf[4] = 0;
            std::cout << "tag:" << buf << "\n";
        }

        // returns the font in the array at key+idx and how many are left in the array
        std::tuple<int, Font*> at(const hb_script_t key, int idx) const
        {
            std::cout << "Querying ";
            print_tag(key);
            assert(idx >= 0);
            auto it = _map.find(key);
            if (it == _map.end())
            {
                printf("returning fallback\n");
                return { 0, db.at(0) };
            }
            auto m = it->second;
            assert(idx < m.length);
            auto font_idx = m.start + idx;
            assert(font_idx < db.size());

            std::cout << "Returning ";
            print_tag(key);

            return { m.length - idx - 1, db.at(font_idx) };
        }

        struct Mapping
        {
            int start  = 0;
            int length = 0;
        };

        std::vector<Font*> db;
        std::unordered_map<hb_script_t, Mapping> _map;
    };

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

// inline bool is_char_supported(Font& font, char32_t character)
//{
//     if (FT_Get_Char_Index(font.face, character) == 0)
//         return false;
//
//     return true;
// }

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

    Shaper() = default;

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

    auto shape(const uint32_t* text, const int text_length, const int offset, const int n)
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

constexpr hb_script_t adapt(const SBScript sb_script)
{
    switch (sb_script)
    {
        case SBScriptLATN:
            return HB_SCRIPT_LATIN;
        case SBScriptGREK:
            return HB_SCRIPT_GREEK;
        case SBScriptCYRL:
            return HB_SCRIPT_CYRILLIC;
        case SBScriptARMI:
            return HB_SCRIPT_ARMENIAN;
        case SBScriptHEBR:
            return HB_SCRIPT_HEBREW;
        case SBScriptARAB:
            return HB_SCRIPT_ARABIC;
        case SBScriptDEVA:
            return HB_SCRIPT_DEVANAGARI;
        case SBScriptBENG:
            return HB_SCRIPT_BENGALI;
        case SBScriptGURU:
            return HB_SCRIPT_GURMUKHI;
        case SBScriptGUJR:
            return HB_SCRIPT_GUJARATI;
        case SBScriptORYA:
            return HB_SCRIPT_ORIYA;
        case SBScriptTAML:
            return HB_SCRIPT_TAMIL;
        case SBScriptTELU:
            return HB_SCRIPT_TELUGU;
        case SBScriptKNDA:
            return HB_SCRIPT_KANNADA;
        case SBScriptMLYM:
            return HB_SCRIPT_MALAYALAM;
        case SBScriptSINH:
            return HB_SCRIPT_SINHALA;
        case SBScriptTHAI:
            return HB_SCRIPT_THAI;
        case SBScriptLAOO:
            return HB_SCRIPT_LAO;
        case SBScriptTIBT:
            return HB_SCRIPT_TIBETAN;
        case SBScriptMYMR:
            return HB_SCRIPT_MYANMAR;
        case SBScriptZYYY:
            return HB_SCRIPT_COMMON; // or some appropriate default value
        case SBScriptZZZZ:
            return HB_SCRIPT_UNKNOWN;
        default:
            return HB_SCRIPT_LATIN;
    }
}

struct ShaperRun
{
    using Buffer    = hb_buffer_t*;
    using Script    = hb_script_t;
    using Direction = hb_direction_t;

    struct Specs
    {
        const std::string& text;
        const Font::Map& fonts;
        const SBScriptAgent* script_info;
        const SBLevel level;
    };

    explicit ShaperRun(const Specs& s)
    {
        LOG_FUNC;
        assert(!s.fonts._map.empty());
        buffer = hb_buffer_create();

        std::u32string u32_str;
        {
            auto tmp = s.text.substr(s.script_info->offset, s.script_info->length);
            u32_str  = utlz::utf8_to_utf32(tmp);
            std::cout << "Matching: " << tmp << "\n";
        }

        hb_buffer_add_utf32(buffer, (const uint32_t*) u32_str.c_str(), -1, 0, -1);
        hb_buffer_guess_segment_properties(buffer);
        auto hb_script = hb_buffer_get_script(buffer);
        hb_buffer_destroy(buffer);

        // collect individual utf32 codepoints into "font runs" with a matching font (e.g. latin vs.
        // emojis)
        struct FontRun
        {
            Font* font_ptr;
            unsigned int start;
            unsigned int length;
        };

        std::vector<FontRun> font_runs;

        unsigned int font_run_start      = 0;
        unsigned int font_run_end        = 0;
        int font_idx                     = 0;
        auto [fonts_remaining, font_ptr] = s.fonts.at(hb_script, font_idx++);
        while (fonts_remaining >= 0)
        {
            while (font_run_end < u32_str.size())
            {
                // skip until there's a character match with the font
                if (!utlz::is_char_supported(font_ptr->face, u32_str[font_run_end]))
                {
                    ++font_run_start;
                    ++font_run_end;
                    continue;
                }

                // iterate end idx until there's no match
                while (font_run_end < u32_str.size()
                       && utlz::is_char_supported(font_ptr->face, u32_str[font_run_end++]))
                    ;

                // collect the substr
                font_runs.emplace_back(FontRun { .font_ptr = font_ptr,
                                                 .start    = font_run_start,
                                                 .length   = (font_run_end - font_run_start) });

                font_run_start = font_run_end;
            }

            // iterate the next available font
            if (fonts_remaining > 0)
            {
                auto [remaining, fp] = s.fonts.at(hb_script, font_idx++);
                fonts_remaining      = remaining;
                font_ptr             = fp;
            }
            else
                fonts_remaining = -1;
        }
        std::vector<std::pair<unsigned int, unsigned int>> ranges;
        for (auto& run : font_runs)
            ranges.emplace_back(run.start, run.start + run.length);

        std::sort(
            ranges.begin(),
            ranges.end(),
            [](auto lhs, auto rhs) { return lhs.first < rhs.first; }
        );

        for (int i = 0; i < ranges.size() - 1; ++i)
        {
            auto& lhs = ranges[i];
            auto& rhs = ranges[i + 1];
            printf("s: %d, e: %d\n", lhs.first, rhs.second);
            assert(lhs.second == rhs.first);
        }

        for (auto& run : font_runs)
        {
            buffer = hb_buffer_create();
            hb_buffer_add_utf32(buffer, (const uint32_t*) u32_str.c_str(), run.length, run.start, run.length);
            hb_shape(run.font_ptr->unicode, buffer, NULL, 0);

            copy_data(run.font_ptr);

            hb_buffer_destroy(buffer);
        }
    }

    ~ShaperRun() { LOG_FUNC; }

    void copy_data(Font* font)
    {
        glyphs_n = hb_buffer_get_length(buffer);

        std::vector<hb_glyph_info_t> infos;
        infos.reserve(glyphs_n);
        const auto infos_ptr = hb_buffer_get_glyph_infos(buffer, nullptr);
        for (int i = 0; i < glyphs_n; ++i)
            infos.emplace_back(infos_ptr[i]);

        std::vector<hb_glyph_position_t> positions;
        positions.reserve(glyphs_n);
        const auto pos_ptr = hb_buffer_get_glyph_positions(buffer, nullptr);
        for (int i = 0; i < glyphs_n; ++i)
            positions.emplace_back(pos_ptr[i]);

        assert(infos.size() == positions.size());
        glyph_infos.emplace_back(GlyphInfo { std::move(infos), std::move(positions), font });
    }

    struct GlyphInfo
    {
        std::vector<hb_glyph_info_t> hb_info;
        std::vector<hb_glyph_position_t> positions;
        Font* font;
    };

    const std::vector<GlyphInfo>& get_data() const { return glyph_infos; }

    std::string language;
    Script script { HB_SCRIPT_INVALID };
    Direction direction { HB_DIRECTION_INVALID };
    Buffer buffer = nullptr;
    unsigned int glyphs_n;

    std::vector<GlyphInfo> glyph_infos;
};

} // namespace hb_helpers

struct ShaperChunk
{
    std::vector<char32_t>* text_ptr = nullptr;

    struct
    {
        int start  = 0;
        int end    = 0;
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

std::vector<hb_helpers::ShaperRun> create_shaper_runs(std::string& text, Font::Map& fonts)
{
    using namespace hb_helpers;

    std::vector<ShaperRun> result;
    result.reserve(text.length() / 2);

    SBCodepointSequence sb_str { SBStringEncodingUTF8, (void*) text.c_str(), text.length() };
    SBAlgorithmRef bidi       = SBAlgorithmCreate(&sb_str);
    constexpr uint8_t LEVEL_X = UINT8_MAX;
    SBParagraphRef paragraph  = SBAlgorithmCreateParagraph(bidi, 0, LEVEL_X, SBLevelDefaultLTR);
    SBLineRef line = SBParagraphCreateLine(paragraph, 0, SBParagraphGetLength(paragraph));
    const SBRun* direction_runs = SBLineGetRunsPtr(line);
    SBUInteger runs_n           = SBLineGetRunCount(line);

    SBScriptLocatorRef script_loc = SBScriptLocatorCreate();
    SBScriptLocatorLoadCodepoints(script_loc, &sb_str);

    // used to write the specs when calling move next
    const SBScriptAgent* script_info = SBScriptLocatorGetAgent(script_loc);

    // Script runs are assumed to be more granular and coincide with direction runs, thus
    // we'll merge the to into basically script runs with the appropriate specs such as direction
    int dir_run_idx = 0;
    while (dir_run_idx < text.length() && SBScriptLocatorMoveNext(script_loc))
    {
        while (script_info->offset > direction_runs[dir_run_idx].offset)
            ++dir_run_idx;

        result.emplace_back(ShaperRun::Specs { text, fonts, script_info, direction_runs[dir_run_idx].level });
    }

    SBScriptLocatorRelease(script_loc);
    SBLineRelease(line);
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(bidi);

    return result;
}

} // namespace typesetting