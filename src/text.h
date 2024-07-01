#pragma once

#include <ft2build.h>
#include FT_FREETYPE_H // Include FreeType header files
#include <hb-ft.h>
#include <initializer_list>
#include <utility>

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
    //    Blob blob;
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

#if 0
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
#endif

#if 0
struct ShaperRun
{
    using Buffer    = hb_buffer_t*;
    using Script    = hb_script_t;
    using Direction = hb_direction_t;

    struct Specs
    {
        const std::u32string& text;
        const Font::Map& fonts;
        const SBScriptAgent* script_info;
        const SBLevel level;
    };

    explicit ShaperRun(const Specs& s)
    {
        LOG_FUNC;
        assert(!s.fonts._map.empty());
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
};
#endif

} // namespace hb_helpers

#if 0
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
#endif

struct RunItem
{
    std::vector<hb_glyph_info_t> hb_info;
    std::vector<hb_glyph_position_t> positions;
    unsigned int start;
    unsigned int length;
    Font* font;
};

std::vector<RunItem> create_shaper_runs(std::string& utf8txt, Font::Map& fonts)
{
    using namespace hb_helpers;

    std::vector<RunItem> run_infos;

    run_infos.reserve(utf8txt.length() / 2);

    auto u32_str = utlz::utf8to32(utf8txt);

    SBCodepointSequence sb_str { SBStringEncodingUTF32, (void*) u32_str.c_str(), u32_str.length() };
    SBAlgorithmRef bidi        = SBAlgorithmCreate(&sb_str);
    constexpr uint32_t LEVEL_X = UINT32_MAX;
    SBParagraphRef paragraph   = SBAlgorithmCreateParagraph(bidi, 0, LEVEL_X, SBLevelDefaultLTR);
    SBLineRef line = SBParagraphCreateLine(paragraph, 0, SBParagraphGetLength(paragraph));
    const SBRun* direction_runs = SBLineGetRunsPtr(line);
    SBUInteger runs_n           = SBLineGetRunCount(line);

    SBScriptLocatorRef script_loc = SBScriptLocatorCreate();
    SBScriptLocatorLoadCodepoints(script_loc, &sb_str);

    // specs container when calling move next
    const SBScriptAgent* script_info = SBScriptLocatorGetAgent(script_loc);

    // TODO: create a iterator by chunks of 256 or sth
    constexpr int mask_length = 512;
    assert(u32_str.length() < mask_length);
    std::bitset<mask_length> mask;

    unsigned int run_start = 0;
    unsigned int run_end   = 0;
    // Script runs are assumed to be more granular and coincide with direction runs, thus
    // we'll merge the to into basically script runs with the appropriate specs such as direction
    int dir_run_idx = 0;
    while (dir_run_idx < u32_str.length() && SBScriptLocatorMoveNext(script_loc))
    {

        //        printf(" -- script offset: %d\n", script_info->offset);
        while (script_info->offset > direction_runs[dir_run_idx].offset)
            ++dir_run_idx;

        {
            auto run_txt = u32_str.substr(script_info->offset, script_info->length);
            //            std::cout << "Matching: \"" << utlz::utf32to8(run_txt) << "\" ";
        }

        auto buffer = hb_buffer_create();
        hb_buffer_add_utf32(
            buffer,
            (const uint32_t*) u32_str.c_str(),
            -1,
            script_info->offset,
            script_info->length
        );
        hb_buffer_guess_segment_properties(buffer);
        auto font_key = hb_buffer_get_script(buffer);
        hb_buffer_destroy(buffer);

        const auto script_start = script_info->offset;
        const auto script_end   = script_start + script_info->length;

        // collect individual utf32 codepoints into "font runs" with a matching font (e.g. latin vs.
        // emojis)
        struct FontRun
        {
            Font* font_ptr;
            unsigned int start;
            unsigned int length;
        };

        std::vector<FontRun> font_runs;

        int font_idx                     = 0;
        auto [fonts_remaining, font_ptr] = fonts.at(font_key, font_idx++);
        bool script_segment_resolved     = false;
        bool fallback_tried              = false;

    collect_runs:
        do
        {
            run_start = script_info->offset;
            // search for starting point that is left unresolved
            while (run_start < script_end && mask.test(run_start))
                ++run_start;
            //            printf("font offset %d (remaining %d)\n", run_start, fonts_remaining);

            run_end = run_start;
            // collect all runs fitting to this font
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
                    mask.set(run_end);
                    ++run_end;
                }

                //                printf("[%d, %d]: %d\n", run_start, run_end, font_ptr->id);
                // collect the substr
                font_runs.emplace_back(
                    FontRun { .font_ptr = font_ptr, .start = run_start, .length = (run_end - run_start) }
                );
                run_start = run_end;
            }

            script_segment_resolved = true;
            for (int i = script_start; i < script_end; ++i)
            {
                //                std::cout << mask[i];
                script_segment_resolved &= mask[i];
            }
            //            std::cout << "\n";

            if (fonts_remaining > 0)
            {
                // iterate the next available font
                auto [remaining, fp] = fonts.at(font_key, font_idx++);
                fonts_remaining      = remaining;
                font_ptr             = fp;
            }

        } while (fonts_remaining > 0 && !script_segment_resolved);

        if (!fallback_tried)
        {
            font_key             = fonts._fallback_key;
            font_idx             = 0;
            fallback_tried       = true;
            auto [remaining, fp] = fonts.at(font_key, font_idx++);
            fonts_remaining      = remaining;
            font_ptr             = fp;

            goto collect_runs;
        }

        //        while (!script_segment_resolved)
        //        {
        //            // search for starting point that is left unresolved
        //            run_start = script_start;
        //            while (run_start < script_end && mask.test(run_start))
        //                ++run_start;
        //            printf("font offset %d (remaining %d)\n", run_start, fonts_remaining);
        //
        //            // iterate end idx until there's no match while marking these chars as
        //            resolved run_end = run_start; while (run_end < script_end &&
        //            !mask.test(run_end))
        //            {
        //                mask.set(run_end);
        //                ++run_end;
        //            }
        //
        //            // collect the substr
        //            font_runs.emplace_back(FontRun { .font_ptr = fonts.at(fonts._fallback_key),
        //                                             .start    = run_start,
        //                                             .length   = (run_end - run_start) });
        //            script_segment_resolved = true;
        //            for (int i = script_start; i < script_end; ++i)
        //                script_segment_resolved &= mask[i];
        //        }

        //        std::cout << "mask: " << mask << "\n";

#if 0
        { // debugging
            std::vector<std::pair<unsigned int, unsigned int>> ranges;
            for (const auto& run : font_runs)
                ranges.emplace_back(run.start, run.length);

            std::sort(
                ranges.begin(),
                ranges.end(),
                [](auto lhs, auto rhs) { return lhs.first < rhs.first; }
            );

            for (int i = 0; i < ranges.size() - 1; ++i)
            {
                auto& lhs = ranges[i];
                auto& rhs = ranges[i + 1];
                printf("s: %d, l: %d\n", lhs.first, rhs.second);
                //            assert(lhs.second == rhs.first);
            }
        }
#endif

        for (auto& run : font_runs)
        {
            buffer = hb_buffer_create();
            hb_buffer_add_utf32(buffer, (const uint32_t*) u32_str.c_str(), u32_str.length(), run.start, run.length);
            hb_buffer_guess_segment_properties(buffer);
            hb_shape(run.font_ptr->unicode, buffer, NULL, 0);

            auto tmp = u32_str.substr(run.start, run.length);

            auto glyphs_n = hb_buffer_get_length(buffer);

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

            run_infos.emplace_back(
                RunItem { std::move(infos), std::move(positions), run.start, run.length, run.font_ptr }
            );

            hb_buffer_destroy(buffer);
        }
    }

    SBScriptLocatorRelease(script_loc);
    SBLineRelease(line);
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(bidi);

    std::sort(
        run_infos.begin(),
        run_infos.end(),
        [](auto& lhs, auto& rhs) { return lhs.start < rhs.start; }
    );
    return run_infos;
}

} // namespace typesetting