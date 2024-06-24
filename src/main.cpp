#include <iostream>
#include <functional>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "scope_guards.h"
#include "text.h"
#include "rendering.h"

std::function<void(GLFWwindow*)> draw;

int main()
{
    glfwSetErrorCallback([](auto err, auto desc) { fprintf(stderr, "ERROR: %s\n", desc); });
    auto check_failed = [](bool status, auto fail_msg)
    {
        if (!status)
        {
            fprintf(stderr, "%s\n", fail_msg);
            exit(1);
        }
    };
    auto may_fail = [&](auto& func, auto fail_msg, auto&... args)
    { check_failed(func(args...), fail_msg); };

    auto unwrap_opt = [&](auto& func, auto fail_msg, auto... args)
    {
        try
        {
            return func(args...).value();
        }
        catch (const std::bad_optional_access& e)
        {
            throw std::runtime_error(fail_msg);
        }
    };

    // GLFW
    may_fail(glfwInit, "glfw init failed");
    on_scope_exit(glfwTerminate);

    // Create window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // https://www.glfw.org/faq.html#41---how-do-i-create-an-opengl-30-context
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    // https://www.glfw.org/docs/3.3/window_guide.html#GLFW_SCALE_TO_MONITOR
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    auto* window = glfwCreateWindow(spec::gui::w, spec::gui::h, "foobar", nullptr, nullptr);
    check_failed(window, "Create window failed");

    glfwSetWindowRefreshCallback(
        window,
        [](auto* window)
        {
            draw(window);
            glfwSwapBuffers(window);
        }
    );
    glfwMakeContextCurrent(window);

    float content_scale = .0f;
    glfwGetWindowContentScale(window, nullptr, &content_scale);

    // Glad
    auto proc_address = (GLADloadproc) glfwGetProcAddress;
    may_fail(gladLoadGLLoader, "setting loader for glad failed", proc_address);

    using namespace typesetting;
    // Typesetting
    // ├─Fonts
    // ├─Glyph
    // ├─Atlas
    // ├─Library
    // └─Handling
    Library library;
    {
        bool status = library.init();
        check_failed(status, "Resources init failed"); // 4 fonts and 256 max chars?
    }
    on_scope_exit([&] { library.destroy(); });
    utlz::print_versions(library);

    TextRenderer rdr;
    {
        bool status = rdr.init(4, 256);
        check_failed(status, "Renderer init failed"); // 4 fonts and 256 max chars?
    }
    on_scope_exit([&] { rdr.destroy(); });

    const std::string font_dir = FONTS_DIR;

    auto add_font = [&library, content_scale](const std::string font_path, int font_size = 32)
    {
        Font font;
        if (!std::filesystem::exists(font_path))
            throw std::runtime_error("Error: Font file not found");

        auto _ = create_font(&library, font_path.c_str(), font_size, content_scale);
        if (_)
            font = _.value();
        else
            throw std::runtime_error("font failed");
        return font;
    };
    auto font_latin = add_font(font_dir + "/NotoSans-Regular.ttf");
    on_scope_exit([&] { destroy_font(font_latin); });
    auto font_arial = add_font("/Library/Fonts/Arial Unicode.ttf");
    on_scope_exit([&] { destroy_font(font_arial); });
    // arabic
    auto font_amiri = add_font(font_dir + "/amiri-regular.ttf");
    on_scope_exit([&] { destroy_font(font_amiri); });
    auto font_arabic = add_font(font_dir + "/NotoSansArabic-Regular.ttf");
    on_scope_exit([&] { destroy_font(font_arabic); });
    // hindi
    auto font_sanskrit = add_font(font_dir + "/Sanskrit2003.ttf");
    on_scope_exit([&] { destroy_font(font_sanskrit); });
    auto font_sarabun = add_font(font_dir + "/Sarabun-Regular.ttf");
    on_scope_exit([&] { destroy_font(font_sarabun); });
    // russian
    auto font_dejavu = add_font(font_dir + "/DejaVuSerif.ttf");
    on_scope_exit([&] { destroy_font(font_dejavu); });
    auto font_han = add_font(font_dir + "/fireflysung.ttf");
    on_scope_exit([&] { destroy_font(font_han); });
    // other
    auto font_maths = add_font(font_dir + "/NotoSansMath-Regular.ttf");
    on_scope_exit([&] { destroy_font(font_maths); });
    auto font_emoji = add_font(font_dir + "/NotoEmoji-VariableFont_wght.ttf");
    on_scope_exit([&] { destroy_font(font_emoji); });

    auto maths_cstr = u8"⩤⪉⦫∷𝞿∰⩪⩭𝔠";
    Text text_maths { maths_cstr, "dflt", HB_SCRIPT_LATIN, HB_DIRECTION_LTR };
    //    Text breaking_long {
    //        u8"其妙يا جبل ما يهزك העולם "
    //        u8"ăѣ𝔠ծềſģȟᎥ𝒋ǩľḿꞑȯ𝘱𝑞𝗋𝘴ȶ𝞄𝜈ψ𝒙𝘆𝚣1234567890!@#$%^&*()-_=+[{]};:',<.>/"
    //        u8"?~𝘈Ḇ𝖢𝕯٤ḞԍНǏ𝙅ƘԸⲘ𝙉০Ρ𝗤Ɍ𝓢ȚЦ𝒱Ѡ𝓧ƳȤѧᖯć𝗱ễ𝑓𝙜Ⴙ𝞲𝑗𝒌ļṃŉо𝞎𝒒ᵲꜱ𝙩ừ𝗏ŵ𝒙𝒚ź1234567890!@#$%^&"
    //        u8"*()-_=+[{]};:',<.>/"
    //        u8"?~АḂⲤ𝗗𝖤𝗙ꞠꓧȊ𝐉𝜥ꓡ𝑀𝑵Ǭ𝙿𝑄Ŗ𝑆𝒯𝖴𝘝𝘞ꓫŸ𝜡ả𝘢ƀ𝖼ḋếᵮℊ𝙝Ꭵ𝕛кιṃդⱺ𝓅𝘲𝕣𝖘ŧ𝑢ṽẉ𝘅ყž1234567890!@#$%^"
    //        u8"&*()-_=+[{]};:',<.>/"
    //        u8"?~Ѧ𝙱ƇᗞΣℱԍҤ١𝔍К𝓛𝓜ƝȎ𝚸𝑄Ṛ𝓢ṮṺƲᏔꓫ𝚈𝚭𝜶Ꮟçძ𝑒𝖿𝗀ḧ𝗂𝐣ҝɭḿ𝕟𝐨𝝔𝕢ṛ𝓼тú𝔳ẃ⤬𝝲𝗓1234567890!@#$%^&"
    //        u8"*()-_=+[{]};:',<.>/?~𝖠Β𝒞𝘋𝙴𝓕ĢȞỈ𝕵ꓗʟ𝙼ℕ০𝚸𝗤ՀꓢṰǓⅤ𝔚Ⲭ𝑌𝙕𝘢𝕤",
    //        "dflt",
    //        HB_SCRIPT_HAN,
    //        HB_DIRECTION_LTR
    //    };

    auto latin_cstr          = u8"älkäös Rénè! ȟ";
    auto arabic_cstr         = u8"أسئلة و أجوبة";
    auto arabic_no_spcs_cstr = u8"أسئلةوأجوبة";
    auto han_cstr            = u8"緳 踥踕";
    auto devanagari_cstr = u8"हालाँकि प्रचलित रूप पूजा";
    auto sth_cstr = u8"Z͑ͫ̓ͪ̂ͫ̽͏̴̙̤̞͉͚̯̞̠͍A̴̵̜̰͔ͫ͗͢L̠ͨͧͩ͘G̴̻͈͍͔̹̑͗̎̅͛́Ǫ̵̹̻̝̳͂̌̌͘!͖̬̰̙̗̿̋ͥͥ̂ͣ̐́́͜͞";
    auto emoji_cstr       = u8"🌮👩‍👩‍👧‍👦";
    auto apple_chars_cstr = u8"௹☞➾";

    Text text_latin { latin_cstr, "en", HB_SCRIPT_LATIN, HB_DIRECTION_LTR };
    Text text_arabic { arabic_cstr, "ar", HB_SCRIPT_ARABIC, HB_DIRECTION_RTL };
    Text text_arabic_undef { arabic_cstr };
    Text text_han        = { han_cstr, "ch", HB_SCRIPT_HAN, HB_DIRECTION_TTB };
    Text text_devanagari = { devanagari_cstr, "hi", HB_SCRIPT_DEVANAGARI, HB_DIRECTION_LTR };
    Text text_sth_else { sth_cstr, "en", HB_SCRIPT_INHERITED, HB_DIRECTION_INVALID };

    Text text_latin_raw      = { latin_cstr };
    Text text_arabic_raw     = { arabic_cstr };
    Text text_han_raw        = { han_cstr };
    Text text_devanagari_raw = { devanagari_cstr };
    Text taco_raw            = { emoji_cstr };
    Text weird_raw           = { apple_chars_cstr };
    Text text_maths_raw      = { maths_cstr };
    Text breaking_long_raw   = {
        u8"其妙يا جبل ما يهزك העולם "
        u8"ăѣ𝔠ծềſģȟᎥ𝒋ǩľḿꞑȯ𝘱𝑞𝗋𝘴ȶ𝞄𝜈ψ𝒙𝘆𝚣1234567890!@#$%^&*()-_=+[{]};:',<.>/"
        u8"?~𝘈Ḇ𝖢𝕯٤ḞԍНǏ𝙅ƘԸⲘ𝙉০Ρ𝗤Ɍ𝓢ȚЦ𝒱Ѡ𝓧ƳȤѧᖯć𝗱ễ𝑓𝙜Ⴙ𝞲𝑗𝒌ļṃŉо𝞎𝒒ᵲꜱ𝙩ừ𝗏ŵ𝒙𝒚ź1234567890!@#$%^&"
        u8"*()-_=+[{]};:',<.>/"
        u8"?~АḂⲤ𝗗𝖤𝗙ꞠꓧȊ𝐉𝜥ꓡ𝑀𝑵Ǭ𝙿𝑄Ŗ𝑆𝒯𝖴𝘝𝘞ꓫŸ𝜡ả𝘢ƀ𝖼ḋếᵮℊ𝙝Ꭵ𝕛кιṃդⱺ𝓅𝘲𝕣𝖘ŧ𝑢ṽẉ𝘅ყž1234567890!@#$%^"
        u8"&*()-_=+[{]};:',<.>/"
        u8"?~Ѧ𝙱ƇᗞΣℱԍҤ١𝔍К𝓛𝓜ƝȎ𝚸𝑄Ṛ𝓢ṮṺƲᏔꓫ𝚈𝚭𝜶Ꮟçძ𝑒𝖿𝗀ḧ𝗂𝐣ҝɭḿ𝕟𝐨𝝔𝕢ṛ𝓼тú𝔳ẃ⤬𝝲𝗓1234567890!@#$%^&"
        u8"*()-_=+[{]};:',<.>/?~𝖠Β𝒞𝘋𝙴𝓕ĢȞỈ𝕵ꓗʟ𝙼ℕ০𝚸𝗤ՀꓢṰǓⅤ𝔚Ⲭ𝑌𝙕𝘢𝕤"
    };

    //    auto mixed_cstr        = u8"Rénè ∰⩪⩭𝔠 ௹☞➾ 🌮👩";
    auto mixed_cstr = u8"huu त रू (𝔠 ௹➾ و) أجوبة x👩‍👩‍👧‍👦{é}";
    //    auto mixed_cstr        = u8"妙העולם ";
    //    auto mixed_cstr        = u8"Rénè ∰⩪⩭𝔠";
    Text text_sth_else_raw = { sth_cstr };
    Text text_mixed_raw    = { mixed_cstr };

    hb_helpers::Boundaries b;
    b.list_for_words(mixed_cstr);
    std::cout << "words: " << b.word_count(mixed_cstr) << "\n";

    std::string mixed = mixed_cstr;

//    hb_helpers::print_direction_markers(mixed);
//    hb_helpers::test_sheenbidi(mixed);
    Font::Map fonts;
    fonts.add(HB_SCRIPT_COMMON, font_latin);
    fonts.add(HB_SCRIPT_ARABIC, font_amiri);
    fonts.set_fallback(font_latin);

    auto runs = create_shaper_runs(mixed, fonts);

#if 0

    utf::string utf_str(text_devanagari.text.c_str());
    auto uchar_str = utf_str.as_unicode();
    for (auto& uch : uchar_str)
    {
        std::cout << uch << ": " << (is_char_supported(font_latin, uch) ? "yey\n" : "nope\n");
    }
    std::vector<std::vector<RenderableText<VertexDataFormat>>> renders;
    {
        std::vector<RenderableText<VertexDataFormat>> tmp;
        tmp.emplace_back(text_maths, font_arial, colours::blue);
        tmp.emplace_back(text_maths, font_maths, colours::green);
        tmp.emplace_back(taco_raw, font_arial, colours::blue);
        tmp.emplace_back(taco_raw, font_emoji, colours::green);
        //        tmp.emplace_back(text_latin, font_arial, colours::green);
        //        tmp.back().shaper.add_feature(HBFeature::LigatureOn);
        tmp.emplace_back(text_arabic, font_arial, colours::blue);
        tmp.emplace_back(text_arabic, font_amiri, colours::green);
        tmp.emplace_back(text_han, font_arial, colours::blue);
        tmp.emplace_back(text_han, font_sarabun, colours::green);
        tmp.emplace_back(text_devanagari, font_arial, colours::blue);
        tmp.emplace_back(text_devanagari, font_sarabun, colours::green);
        tmp.emplace_back(text_sth_else, font_arial, colours::blue);
        tmp.emplace_back(text_mixed_raw, font_arial, colours::blue);
        renders.emplace_back(tmp);
    }

    {
        std::vector<RenderableText<VertexDataFormat>> tmp;
        tmp.emplace_back(text_maths_raw, font_maths);
        tmp.emplace_back(taco_raw, font_emoji);
        tmp.emplace_back(text_latin_raw, font_dejavu);
        tmp.back().shaper.add_feature(hb_helpers::Feature::LigatureOn);
        tmp.emplace_back(text_arabic_raw, font_amiri);
        tmp.emplace_back(text_han_raw, font_han);
        tmp.emplace_back(text_devanagari_raw, font_sanskrit);
        renders.emplace_back(tmp);
    }

    std::vector<hb_helpers::FontShaper> shapers;
    shapers.emplace_back("amiri-arabic", font_amiri, "ar", HB_SCRIPT_ARABIC, HB_DIRECTION_RTL);
    shapers.emplace_back("default latin", font_latin, "en", HB_SCRIPT_LATIN, HB_DIRECTION_LTR);
    shapers.emplace_back("han", font_han, "ch", HB_SCRIPT_HAN, HB_DIRECTION_LTR);
    shapers.emplace_back("sanskrit", font_sanskrit, "hi", HB_SCRIPT_DEVANAGARI, HB_DIRECTION_LTR);
    shapers.emplace_back("emoji", font_emoji, "dflt", HB_SCRIPT_LATIN, HB_DIRECTION_LTR);
    shapers.emplace_back("maths", font_maths, "dflt", HB_SCRIPT_LATIN, HB_DIRECTION_LTR);
    shapers.emplace_back("fallback arial", font_arial, "", HB_SCRIPT_LATIN, HB_DIRECTION_INVALID);
    std::string s = mixed_cstr;
#endif
    Point p { 0, 0 };
//    std::cout << "chunking: " << s << "\n";

    draw = [&](GLFWwindow* window)
    {
        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        auto DP_X = [](float x) -> float { return x; };
        auto DP_Y = [&fb_h](float y) -> float { return fb_h - y; };

        rdr.begin(fb_w, fb_h);
        float x = 200.0f;
        float y = 200.0f;
#if 0
        rdr.draw_text_yes<VertexDataFormat>(s, shapers, { DP_X(x * content_scale), DP_Y(y * content_scale) });
        if (0)
        {
            float x  = 10.0f;
            float y  = 60.0f;
            auto& rx = renders[0];
            for (auto& r : rx)
            {
                auto adjusted_x = r.text.direction == HB_DIRECTION_LTR ? x : x + float(spec::gui::w / 2 - 10);
                rdr.draw_text(r, { DP_X(adjusted_x * content_scale), DP_Y(y * content_scale) });
                y += 50.0f;
            }
        }
        if (0)
        {
            float x  = 10.0f;
            float y  = 300.0f;
            auto& rx = renders[1];
            for (auto& r : rx)
            {
                rdr.draw_text(r, { DP_X(x * content_scale), DP_Y(y * content_scale) });
                y += 50.0f;
            }
        }
#endif
        rdr.draw_runs<VertexDataFormat>(runs, { DP_X(x * content_scale), DP_Y(y * content_scale) }, colours::black);
        rdr.end();
    };

    // Event loop
    while (!glfwWindowShouldClose(window))
    {
        draw(window);
        glfwSwapBuffers(window);

        glfwWaitEvents();
    }

    rdr.print_stats();

    return 0;
}
