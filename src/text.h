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

        void add(hb_script_t script, Font& font) { _map.emplace(script, &font); }

        void set_fallback(Font& font) { _map.emplace(_fallback_key, &font); }

        Font* at(const hb_script_t key) const
        {
            if (_map.find(key) != _map.end())
                return _map.at(key);
            else
            {
                printf("returning fallback\n");
                return _map.at(_fallback_key);
            }
        }

        std::unordered_map<hb_script_t, Font*> _map;
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


void print_direction_markers(const std::string& text)
{
    std::cout << text << "\n";

    struct Mark2
    {
        uint8_t _1;
        uint8_t _2;
        const char* name;

        bool operator==(const Mark2& other) const { return (_1 == other._1 && _2 == other._2); }
    };

    struct Mark3
    {
        uint8_t _1;
        uint8_t _2;
        uint8_t _3;
        const char* name;

        bool operator==(const Mark3& other) const
        {
            return (_1 == other._1 && _2 == other._2 && _3 == other._3);
        }
    };

    std::vector<Mark3> marks;
    const uint8_t u8_3byte { 0xE2 };
    marks.emplace_back(Mark3 { u8_3byte, 0x80, 0x8E, "ltr explicit" });
    marks.emplace_back(Mark3 { u8_3byte, 0x80, 0x8F, "rtl explicit" });

    marks.emplace_back(Mark3 { u8_3byte, 0x80, 0xAA, "ltr embedded" });
    marks.emplace_back(Mark3 { u8_3byte, 0x80, 0xAB, "rtl embedded" });

    marks.emplace_back(Mark3 { u8_3byte, 0x80, 0xAC, "pop direction" });

    marks.emplace_back(Mark3 { u8_3byte, 0x80, 0xAD, "ltr override" });
    marks.emplace_back(Mark3 { u8_3byte, 0x80, 0xAE, "rtl override" });

    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xA6, "ltr isolate" });
    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xA7, "ltr isolate" });

    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xA8, "first strong isolate" });
    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xA9, "pop directional isolate" });

    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xAA, "inhibit symmetrical swapping" });
    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xAB, "activate symmetrical swapping" });

    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xAC, "inhibit arabic form shaping" });
    marks.emplace_back(Mark3 { u8_3byte, 0x81, 0xAD, "activate arabic form shaping" });

    Mark2 alm { 0xD8, 0x9C, "arabic letter mark" };

    for (int i = 0; i < text.length() - 3; ++i)
    {
        if (u8_3byte != (uint8_t) text[i])
            continue;

        printf("Found 3 byte utf8 char @ %d: ", i);
        bool print_char = true;
        for (auto& mark : marks)
        {
            if ((text[i + 1] == (char) mark._2) && (text[i + 2] == (char) mark._3))
            {
                printf("%s @ %d\n", mark.name, i);
                print_char = false;
            }
        }
        if (print_char)
            std::cout << text.substr(i, 3) << "\n";
    }

    for (int i = 0; i < text.length() - 2; ++i)
    {
        if ((char) 0xD8 != text[i])
            continue;

        printf("Found 2 byte utf8 char @ %d: ", i);
        if ((text[i] == (char) alm._1) && (text[i + 1] == (char) alm._2))
            printf("%s @ %d\n", alm.name, i);
        else
            std::cout << text.substr(i, 2) << "\n";
    }
}

// Define a function to map SheenBidi script codes to HarfBuzz script codes
hb_script_t get_harfbuzz_script(const char* sheenbidi_script)
{
    // Use HarfBuzz's function to convert ISO 15924 script codes to hb_script_t
    return hb_script_from_string(sheenbidi_script, -1);
}

void test_sheenbidi(const std::string& text)
{
    constexpr uint8_t LEVEL_X = UINT8_MAX;

    SBCodepointSequence sb_str { SBStringEncodingUTF8, (void*) text.c_str(), text.length() };
    // Create a bidi algorithm instance
    SBAlgorithmRef bidiAlgorithm = SBAlgorithmCreate(&sb_str);

    // Create a paragraph for the entire text
    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(bidiAlgorithm, 0, UINT8_MAX, SBLevelDefaultLTR);

    // Get the length of the paragraph
    SBUInteger paragraphLength = SBParagraphGetLength(paragraph);

    // Create a line from the paragraph
    SBLineRef line      = SBParagraphCreateLine(paragraph, 0, paragraphLength);
    SBUInteger runCount = SBLineGetRunCount(line);
    const SBRun* runs   = SBLineGetRunsPtr(line);
    std::cout << "-- Direction runs:\n";
    for (SBUInteger i = 0; i < runCount; i++)
    {
        std::cout << text.substr(runs[i].offset, runs[i].length) << "\n[";
        printf("offset: %ld, ", (long) runs[i].offset);
        printf("len: %ld, ", (long) runs[i].length);
        printf("level: %ld]\n\n", (long) runs[i].level);
    }
    // Create a mirror locator and load the line into it
    SBMirrorLocatorRef mirrorLocator = SBMirrorLocatorCreate();
    SBMirrorLocatorLoadLine(mirrorLocator, line, (void*) text.c_str());
    const SBMirrorAgent* mirrorAgent = SBMirrorLocatorGetAgent(mirrorLocator);

    // Log the details of each mirror
    while (SBMirrorLocatorMoveNext(mirrorLocator))
    {
        printf("Mirror Index: %ld\n", (long) mirrorAgent->index);
        printf("Actual Code Point: %ld\n", (long) mirrorAgent->codepoint);
        printf("Mirrored Code Point: %ld\n\n", (long) mirrorAgent->mirror);
    }
    SBScriptLocatorRef script_loc = SBScriptLocatorCreate();
    SBScriptLocatorLoadCodepoints(script_loc, &sb_str);

    // Initialize HarfBuzz buffer
    hb_buffer_t* hb_buffer = hb_buffer_create();
    hb_buffer_add_utf8(hb_buffer, text.c_str(), -1, 0, -1);

    // Set the direction based on the bidi algorithm
    hb_buffer_set_direction(hb_buffer, HB_DIRECTION_LTR); // Adjust as needed
    //    hb_buffer_guess_segment_properties(hb_buffer);

    const SBScriptAgent* scriptAgent = SBScriptLocatorGetAgent(script_loc);

    std::cout << "-- Script runs:\n";
    // Iterate over script runs and set script properties in HarfBuzz
    while (SBScriptLocatorMoveNext(script_loc))
    {
        std::cout << scriptAgent->script << "\n";

        printf("offset: %ld, ", (long) scriptAgent->offset);
        printf("len: %ld]\n\n", (long) scriptAgent->length);

        // Assuming you have functions to map SheenBidi script codes to HarfBuzz script codes
        // set_segment_properties is a hypothetical function that sets properties in HarfBuzz buffer
        //        set_segment_properties(hb_buffer, scriptAgent->offset, scriptAgent->length,
        //        hb_script);
    }

    // Perform shaping
    //    hb_shape(hb_font, hb_buffer, NULL, 0);

    //    SBScriptLocatorRelease(script_loc);
    SBMirrorLocatorRelease(mirrorLocator);
    SBLineRelease(line);
    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(bidiAlgorithm);
#if 0
    SBAlgorithmRef algorithm = SBAlgorithmCreate(&sb_str);
    SBLevel input_level;
    SBParagraphRef paragraph = SBAlgorithmCreateParagraph(algorithm, 0, uc_v.size(), input_level);
    SBLevel pg_level = SBParagraphGetBaseLevel(paragraph);
    if (pg_level == LEVEL_X) {
        m_paragraphLevel = paragraphlevel;
    }

    if (paragraphlevel == m_paragraphLevel) {
        SBLineRef line = SBParagraphCreateLine(paragraph, 0, m_charCount);
        m_runCount = SBLineGetRunCount(line);
        m_runArray = SBLineGetRunsPtr(line);

        passed &= testLevels();
        passed &= testOrder();

        if (m_bidiMirroring) {
            loadMirrors();
            SBMirrorLocatorLoadLine(m_mirrorLocator, line, (void *)m_genChars);
            passed &= testMirrors();
        }

        SBLineRelease(line);
    } else {
        if (Configuration::DISPLAY_ERROR_DETAILS) {
            cout << "Test failed due to paragraph level mismatch." << endl;
            cout << "  Discovered Paragraph Level: " << (int)paragraphlevel << endl;
            cout << "  Expected Paragraph Level: " << (int)m_paragraphLevel << endl;
        }

        passed &= false;
    }

    SBParagraphRelease(paragraph);
    SBAlgorithmRelease(algorithm);
return passed;
#endif
}

// void print_direction_changes(const std::string& text)
//{
//     UErrorCode status = U_ZERO_ERROR;
//
//     int32_t utf16Length = 0;
//     icu::UnicodeString icu_str(text.c_str());
////    u_strFromUTF8(nullptr, 0, &utf16Length, text.c_str(), -1, &status);
////    if (U_FAILURE(status))
////    {
////        std::cerr << "Error converting UTF-8 to UTF-16: " << u_errorName(status) << '\n';
////        return;
////    }
//
////    auto* utf16Text = new UChar[utf16Length + 1]; // +1 for the null terminator.
////    status          = U_ZERO_ERROR;
////    u_strFromUTF8(utf16Text, utf16Length + 1, &utf16Length, text.c_str(), -1, &status);
////    if (U_FAILURE(status))
////    {
////        std::cerr << "Error converting UTF-8 to UTF-16: " << u_errorName(status) << '\n';
////        delete[] utf16Text;
////        return;
////    }
//
//    // Create a bidirectional text object.
////    UBiDi* bidi = ubidi_openSized(utf16Length, 0, &status);
//    icu::LocalUBiDiPointer bidi;
//
////    UBiDi* bidi = ubidi_open();
////    if (u_failure(status))
////    {
////        std::cerr << "error creating ubidi: " << u_errorname(status) << '\n';
////        delete[] utf16text;
////        return;
////    }
//
//    // Set the paragraph to the text we want to analyze.
////    ubidi_setPara(bidi, utf16Text, utf16Length, UBIDI_DEFAULT_LTR, nullptr, &status);
//    ubidi_setPara(bidi, icu_str, utf16Length, UBIDI_DEFAULT_LTR, nullptr, &status);
//    if (U_FAILURE(status))
//    {
//        std::cerr << "Error setting paragraph: " << u_errorName(status) << '\n';
//        delete[] utf16Text;
//        ubidi_close(bidi);
//        return;
//    }
//
//    // Get the directional runs.
//    int32_t count = ubidi_countRuns(bidi, &status);
//    if (U_FAILURE(status))
//    {
//        std::cerr << "Error counting runs: " << u_errorName(status) << '\n';
//        delete[] utf16Text;
//        ubidi_close(bidi);
//        return;
//    }
//
//    UBiDiLevel levels[count];
//    int32_t indices[count + 1];
//    status = U_ZERO_ERROR;
//    // Iterate through each run and check its direction level.
//    UBiDiLevel lastLevel = UBIDI_DEFAULT_LTR;
//    int32_t start = 0, length = 1;
//    for (;;)
//    {
//        UBiDiLevel level;
//        ubidi_getLogicalRun(bidi, start, &length, &level);
//        if (level != lastLevel && length > 0)
//        { // Direction change.
//            std::cout << "Change at index " << start << ": ";
//            switch (lastLevel)
//            {
//                case UBIDI_LTR:
//                    std::cout << "LTR -> RTL\n";
//                    break;
//                case UBIDI_RTL:
//                    std::cout << "RTL -> LTR\n";
//                    break;
//            }
//            lastLevel = level;
//        }
//        start += length;
//        if (start >= utf16Length)
//        {
//            break;
//        }
//        ++length; // Overlap by 1 to catch direction changes between runs.
//    }
//
//    // Clean up.
//    delete[] utf16Text;
//    ubidi_close(bidi);
//}

namespace test_ustr
{
inline void run() {}
} // namespace test_ustr

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

        auto get_hb_direction = [](SBLevel level)
        {
            const bool is_odd = level & 1;
            if (is_odd)
                return HB_DIRECTION_RTL;
            else
                return HB_DIRECTION_LTR;
        };

        buffer = hb_buffer_create();
        hb_buffer_add_utf8(
            buffer,
            s.text.c_str(),
            //            s.script_info->length,
            s.text.length(),
            s.script_info->offset,
            s.script_info->length
        );
        auto hb_script = adapt(s.script_info->script);
        hb_buffer_guess_segment_properties(buffer);
//        hb_buffer_set_direction(buffer, get_hb_direction(s.level));
//        hb_buffer_set_script(buffer, hb_script);
        // TODO : maybe later we'll figure out some use for this
//        hb_buffer_set_language(buffer, hb_language_get_default());
        font = s.fonts.at(hb_script);
        hb_shape(font->unicode, buffer, NULL, 0);

        copy_data();

        hb_buffer_destroy(buffer);
    }

    ~ShaperRun()
    {
        LOG_FUNC;
    }

    void copy_data()
    {
        glyphs_n = hb_buffer_get_length(buffer);

        infos.reserve(glyphs_n);
        const auto infos_ptr     = hb_buffer_get_glyph_infos(buffer, nullptr);
        for (int i = 0; i < glyphs_n; ++i)
            infos.emplace_back(infos_ptr[i]);

        positions.reserve(glyphs_n);
        const auto pos_ptr = hb_buffer_get_glyph_positions(buffer, nullptr);
        for (int i = 0; i < glyphs_n; ++i)
            positions.emplace_back(pos_ptr[i]);
    }

    auto glyph_data() const{
        return std::tuple{infos.data(), positions.data(), glyphs_n};
    }

    std::string language;
    Script script { HB_SCRIPT_INVALID };
    Direction direction { HB_DIRECTION_INVALID };
    Buffer buffer = nullptr;
    Font* font    = nullptr;
    unsigned int glyphs_n;
    std::vector<hb_glyph_info_t> infos;
    std::vector<hb_glyph_position_t> positions;
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
    while (SBScriptLocatorMoveNext(script_loc))
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