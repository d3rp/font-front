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

    Text text_maths { u8"⩤⪉⦫∷𝞿∰⩪⩭𝔠", "dflt", HB_SCRIPT_LATIN, HB_DIRECTION_LTR };
    Text breaking_long {
        u8"其妙يا جبل ما يهزك העולם "
        u8"ăѣ𝔠ծềſģȟᎥ𝒋ǩľḿꞑȯ𝘱𝑞𝗋𝘴ȶ𝞄𝜈ψ𝒙𝘆𝚣1234567890!@#$%^&*()-_=+[{]};:',<.>/"
        u8"?~𝘈Ḇ𝖢𝕯٤ḞԍНǏ𝙅ƘԸⲘ𝙉০Ρ𝗤Ɍ𝓢ȚЦ𝒱Ѡ𝓧ƳȤѧᖯć𝗱ễ𝑓𝙜Ⴙ𝞲𝑗𝒌ļṃŉо𝞎𝒒ᵲꜱ𝙩ừ𝗏ŵ𝒙𝒚ź1234567890!@#$%^&"
        u8"*()-_=+[{]};:',<.>/"
        u8"?~АḂⲤ𝗗𝖤𝗙ꞠꓧȊ𝐉𝜥ꓡ𝑀𝑵Ǭ𝙿𝑄Ŗ𝑆𝒯𝖴𝘝𝘞ꓫŸ𝜡ả𝘢ƀ𝖼ḋếᵮℊ𝙝Ꭵ𝕛кιṃդⱺ𝓅𝘲𝕣𝖘ŧ𝑢ṽẉ𝘅ყž1234567890!@#$%^"
        u8"&*()-_=+[{]};:',<.>/"
        u8"?~Ѧ𝙱ƇᗞΣℱԍҤ١𝔍К𝓛𝓜ƝȎ𝚸𝑄Ṛ𝓢ṮṺƲᏔꓫ𝚈𝚭𝜶Ꮟçძ𝑒𝖿𝗀ḧ𝗂𝐣ҝɭḿ𝕟𝐨𝝔𝕢ṛ𝓼тú𝔳ẃ⤬𝝲𝗓1234567890!@#$%^&"
        u8"*()-_=+[{]};:',<.>/?~𝖠Β𝒞𝘋𝙴𝓕ĢȞỈ𝕵ꓗʟ𝙼ℕ০𝚸𝗤ՀꓢṰǓⅤ𝔚Ⲭ𝑌𝙕𝘢𝕤",
        "dflt",
        HB_SCRIPT_HAN,
        HB_DIRECTION_LTR
    };

    Text text_latin { u8"älkäös Rénè! ȟ", "en", HB_SCRIPT_LATIN, HB_DIRECTION_LTR };
    Text text_latin_undef { u8"älkäös Rénè! ȟ" };
    Text text_arabic { u8"أسئلة و أجوبة", "ar", HB_SCRIPT_ARABIC, HB_DIRECTION_RTL };
    Text text_arabic_undef { u8"أسئلة و أجوبة" };
    Text text_han = { "緳 踥踕", "ch", HB_SCRIPT_HAN, HB_DIRECTION_TTB };
    Text text_devanagari = { "हालाँकि प्रचलित रूप पूजा", "hi", HB_SCRIPT_DEVANAGARI, HB_DIRECTION_LTR };
    Text taco  = { u8"🌮👩‍👩‍👧‍👦" };
    Text weird = { u8"௹☞➾" };

    std::vector<RenderableText<VertexDataFormat>> renders;
    renders.emplace_back(text_maths, font_maths);
    renders.emplace_back(taco, font_emoji);
    renders.emplace_back(text_latin, font_dejavu);
    renders.back().shaper.add_feature(HBFeature::LigatureOn);
    renders.emplace_back(text_arabic, font_amiri);
    renders.emplace_back(text_han, font_han);
    renders.emplace_back(text_devanagari, font_sanskrit);

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
        float x = 10.0f;
        float y = 60.0f;
        for (auto& r : renders)
        {
            auto adjusted_x = r.text.direction == HB_DIRECTION_LTR ? x : x + float(spec::gui::w / 2 - 10);
            rdr.draw_text(r, { DP_X(adjusted_x * content_scale), DP_Y(y * content_scale) }, colours::red);
            y += 50.0f;
        }
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
