#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H // Include FreeType header files
#include <hb-ft.h>
#include <utility>
#include <thread>
#include <bitset>

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
    using Id          = unsigned int;
    using Face        = FT_Face;
    using UnicodeType = hb_font_t*;

    struct Map
    {
        const hb_script_t _fallback_key = HB_SCRIPT_INVALID;

        void add(hb_script_t script, std::vector<Font*> fonts)
        {
            // call once trick
            volatile static int _ = std::invoke(
                [&]
                {
                    // space for fallback font
                    db.emplace_back(nullptr);
                    return 0;
                }
            );
            Mapping m;
            m.start = db.size();
            for (auto font : fonts)
                db.emplace_back(font);
            m.length = db.size() - m.start;
            _map.emplace(script, m);
        }

        void set_fallback(std::vector<Font*> fonts) { add(_fallback_key, std::move(fonts)); }

        void print_tag(hb_script_t script) const
        {
            char buf[5];
            hb_tag_to_string(script, buf);
            buf[4] = 0;
            std::cout << buf;
        }

        // returns the font in the array at key+idx and how many are left in the array
        std::tuple<int, Font*> at(const hb_script_t key, int idx) const
        {
            //            std::cout << "[tag: \"";
            //            print_tag(key);
            //            std::cout << "\" -> \"";
            assert(idx >= 0);
            auto it = _map.find(key);
            if (it == _map.end())
            {
                auto fb = _map.find(_fallback_key);
                assert(fb != _map.end()); // need to set a fallback font set
                auto fb_idx = fb->second.start + fb->second.length - 1;
                //                printf("\" ... returning fallback]\n");
                return { 0, db.at(fb_idx) };
            }
            auto m = it->second;
            assert(idx < m.length);
            auto font_idx = m.start + idx;
            assert(font_idx < db.size());

            //            print_tag(key);
            //            std::cout << "\"]\n";

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
    float font_size;
    float content_scale;
};

static unsigned int gen_id()
{
    static unsigned int id = 0;
    return id++;
}

std::optional<Font> create_font_bin(
    Library* resources,
    const unsigned char* font_bin,
    const unsigned int bin_size,
    const int font_size,
    const float content_scale
)
{
    static int next_id = 0;
    Font font;

    if (FT_New_Memory_Face(resources->library, font_bin, FT_Long(bin_size), 0, &font.face))
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

    font.unicode = hb_ft_font_create_referenced(font.face);
    hb_ft_font_set_funcs(font.unicode);

    face_scope_dtor.dismiss();

    font.id            = gen_id();
    font.font_size     = font_size;
    font.content_scale = content_scale;
    // TODO: bold and italics..
    return { font };
}

// Set font_size and content_scale before calling
std::optional<Font>
create_font(Library* resources, const char* font_file, const int font_size, const float content_scale)
{
    static int next_id = 0;
    Font font;

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
}

// Declarations of Text
using namespace gfx;

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
} // namespace hb_helpers

struct RunItem
{
    std::vector<hb_glyph_info_t> hb_info;
    std::vector<hb_glyph_position_t> positions;
    Font* font;
};

struct FontRun
{
    unsigned int offset;
    Font* font_ptr;
    hb_buffer_t* buffer;
};

/**
 * 1.collect individual utf32 codepoints into "font runs" with a matching font (e.g. latin vs.
 * emojis). "Run" refers to a continuous piece of text with similar properties.
 *
 * In Bidi algorithm parlance we first detect "paragraphs", then lines and finally directions (by "levels")
 *
 * On top of that, directional runs might encompass distinct script types and finally those script
 * runs might require multiple fonts to include all available codepoints (into glyphs)
 *
 * In other words: Paragraphs > Lines > Direction > Script > Font
 */
std::vector<FontRun> create_font_runs(std::string& utf8txt, Font::Map& fonts)
{
    constexpr int mask_length = 1024;
    auto u32_str              = utlz::utf8to32(utf8txt.substr(0, mask_length));

    SBCodepointSequence sb_str { SBStringEncodingUTF32, (void*) u32_str.c_str(), u32_str.length() };
    SBAlgorithmRef bidi           = SBAlgorithmCreate(&sb_str);
    constexpr uint32_t max_levels = UINT32_MAX;
    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(bidi, 0, max_levels, SBLevelDefaultLTR);
    SBLineRef line           = SBParagraphCreateLine(paragraph, 0, SBParagraphGetLength(paragraph));
    const SBRun* direction_runs = SBLineGetRunsPtr(line);
    //    SBUInteger runs_n           = SBLineGetRunCount(line);

    SBScriptLocatorRef script_loc = SBScriptLocatorCreate();
    SBScriptLocatorLoadCodepoints(script_loc, &sb_str);

    // TODO: create a iterator by chunks of 256 or sth
    // Note this relates to 8-bit characters, but we're dealing with unicode, so there's an
    // upper bound of 4x8-bits for each unicode character/codepoint i.e. 1024 >= 256 codepoints
    std::bitset<mask_length> resolved;
    const auto max_length = std::min((size_t) mask_length, u32_str.length());

    // TODO : the case of only 1 script doesn't need all the logic below, and it's the most used
    // case
    //    SBUInteger scr_runs_n = 0;
    //    {
    //        const SBScriptAgent* script_info = SBScriptLocatorGetAgent(script_loc);
    //        while (SBScriptLocatorMoveNext(script_loc) && ++scr_runs_n < max_length)
    //            ;
    //    }

    // specs container when calling move next
    const SBScriptAgent* script_info = SBScriptLocatorGetAgent(script_loc);
    if (!script_info)
        return {};

    std::vector<FontRun> font_runs;
    font_runs.reserve(max_length / 2); // by2 is an estimate

    auto buffer = hb_buffer_create();
    unsigned int run_start, run_end;
    int dir_run_idx = 0;
    while (dir_run_idx < max_length && SBScriptLocatorMoveNext(script_loc))
    {
        // sync direction and script boundaries
        while (dir_run_idx < max_length && script_info->offset > direction_runs[dir_run_idx].offset)
            ++dir_run_idx;

        hb_buffer_reset(buffer);
        {
            STOPWATCH("buffer add txt");
            hb_buffer_add_utf32(
                buffer,
                (const uint32_t*) u32_str.c_str(),
                -1,
                script_info->offset,
                script_info->length
            );
        }
        {
            STOPWATCH("guess prop");
            hb_buffer_guess_segment_properties(buffer);
        }
        auto font_key = hb_buffer_get_script(buffer);

        const auto script_start = script_info->offset;
        const auto script_end   = script_start + script_info->length;

        int font_idx = 0;
        //        auto [fonts_remaining, font_ptr] = fonts.at(font_key, font_idx++);
        bool script_segment_resolved = false;
        bool fallback_tried          = false;

        int fonts_remaining;
        Font* font_ptr;
    collect_runs:
        do
        {
            auto [remaining, fp] = fonts.at(font_key, font_idx++);
            fonts_remaining      = remaining;
            font_ptr             = fp;

            // search for starting point that is left unresolved
            run_start = script_info->offset;
            while (run_start < script_end && resolved.test(run_start))
                ++run_start;
            run_end = run_start;

            // same script might require multiple fonts
            // collect all runs (codepoints/chars) fitting to this font
            while (run_end < script_end)
            {
                // skip until there's a character match with the font
                if (!utlz::is_char_supported(font_ptr->face, u32_str[run_end]))
                {
                    ++run_start;
                    ++run_end;
                    continue;
                }

                // iterate end idx until there's no match while marking these chars as resolved
                while (run_end < script_end && utlz::is_char_supported(font_ptr->face, u32_str[run_end]))
                {
                    resolved.set(run_end);
                    ++run_end;
                }

                // collect the substr as a new buffer
                auto run_buffer = hb_buffer_create_similar(buffer);
                hb_buffer_add_utf32(run_buffer, (uint32_t*) u32_str.c_str(), -1, run_start, (run_end - run_start));
                {
                    hb_segment_properties_t prop;
                    hb_buffer_get_segment_properties(buffer, &prop);
                    hb_buffer_set_segment_properties(run_buffer, &prop);
                }
                font_runs.emplace_back(
                    FontRun { .offset = run_start, .font_ptr = font_ptr, .buffer = run_buffer }
                );
                run_start = run_end;
            }

            // Check if all codepoints have been handled
            script_segment_resolved = true;
            for (SBUInteger i = script_start; i < script_end; ++i)
                script_segment_resolved &= resolved[i];

        } while (fonts_remaining > 0 && !script_segment_resolved);

        if (!script_segment_resolved && !fallback_tried)
        {
            font_key             = fonts._fallback_key;
            font_idx             = 0;
            fallback_tried       = true;
            auto [remaining, fp] = fonts.at(font_key, font_idx++);
            fonts_remaining      = remaining;
            font_ptr             = fp;

            goto collect_runs;
        }
    }
    std::sort(
        font_runs.begin(),
        font_runs.end(),
        [](auto& lhs, auto& rhs) { return lhs.offset < rhs.offset; }
    );

    hb_buffer_destroy(buffer);

    SBScriptLocatorRelease(script_loc);
    SBLineRelease(line);
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(bidi);

    return font_runs;
}

std::vector<RunItem> create_shaper_runs(std::string& utf8txt, Font::Map& fonts)
{
    auto font_runs = create_font_runs(utf8txt, fonts);
    // 2. shape from font runs into run items
    std::vector<RunItem> run_infos;
    run_infos.reserve(font_runs.size());
    for (auto& run : font_runs)
    {
        {
            STOPWATCH("shape");
            hb_shape(run.font_ptr->unicode, run.buffer, nullptr, 0);
        }

        const auto glyphs_n = hb_buffer_get_length(run.buffer);

        {
            STOPWATCH("copy info");
            std::vector<hb_glyph_info_t> infos;
            infos.reserve(glyphs_n);
            const auto infos_ptr = hb_buffer_get_glyph_infos(run.buffer, nullptr);
            for (int i = 0; i < glyphs_n; ++i)
                infos.emplace_back(infos_ptr[i]);

            std::vector<hb_glyph_position_t> positions;
            positions.reserve(glyphs_n);
            const auto pos_ptr = hb_buffer_get_glyph_positions(run.buffer, nullptr);
            for (int i = 0; i < glyphs_n; ++i)
                positions.emplace_back(pos_ptr[i]);

            assert(infos.size() == positions.size());

            run_infos.emplace_back(RunItem { std::move(infos), std::move(positions), run.font_ptr });
        }

        hb_buffer_destroy(run.buffer);
    }

    return run_infos;
}

} // namespace typesetting