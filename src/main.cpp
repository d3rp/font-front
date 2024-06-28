#include <iostream>
#include <functional>
#include <filesystem>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "scope_guards.h"
#include "text.h"
#include "rendering.h"
#include "test_strings.h"
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
    // han
    auto font_han = add_font(font_dir + "/fireflysung.ttf");
    on_scope_exit([&] { destroy_font(font_han); });
    // georgian
    auto font_georgian = add_font(font_dir + "/NotoSansGeorgian-VariableFont_wdthwght.ttf");
    on_scope_exit([&] { destroy_font(font_georgian); });
    auto font_myanmar = add_font(font_dir + "/NotoSansMyanmar-Thin.ttf");
    on_scope_exit([&] { destroy_font(font_myanmar); });

    // ideograms
    auto font_simple_chinese = add_font(font_dir + "/NotoSansSC-VariableFont_wght.ttf");
    on_scope_exit([&] { destroy_font(font_simple_chinese); });
    auto font_katakana = add_font(font_dir + "/NotoSansJP-VariableFont_wght.ttf");
    on_scope_exit([&] { destroy_font(font_katakana); });
    auto font_korean = add_font(font_dir + "/NotoSansKR-VariableFont_wght.ttf");
    on_scope_exit([&] { destroy_font(font_korean); });
    // other
    auto font_maths = add_font(font_dir + "/NotoSansMath-Regular.ttf");
    on_scope_exit([&] { destroy_font(font_maths); });
    auto font_emoji = add_font(font_dir + "/NotoEmoji-VariableFont_wght.ttf");
    on_scope_exit([&] { destroy_font(font_emoji); });

    Text text_maths { test::adhoc::maths_cstr, "dflt", HB_SCRIPT_LATIN, HB_DIRECTION_LTR };

#if 0
    Text text_latin { test::adhoc::latin_cstr, "en", HB_SCRIPT_LATIN, HB_DIRECTION_LTR };
    Text text_arabic { test::adhoc::arabic_cstr, "ar", HB_SCRIPT_ARABIC, HB_DIRECTION_RTL };
    Text text_arabic_undef { test::adhoc::arabic_cstr };
    Text text_han        = { test::adhoc::han_cstr, "ch", HB_SCRIPT_HAN, HB_DIRECTION_TTB };
    Text text_devanagari = { test::adhoc::devanagari_cstr, "hi", HB_SCRIPT_DEVANAGARI, HB_DIRECTION_LTR };
    Text text_sth_else { test::adhoc::sth_cstr, "en", HB_SCRIPT_INHERITED, HB_DIRECTION_INVALID };
#endif

    Font::Map fonts;
    using V = std::vector<Font*>;
    fonts.add(HB_SCRIPT_LATIN, V { &font_latin, &font_emoji, &font_maths });
    fonts.add(HB_SCRIPT_GEORGIAN, V { &font_georgian });
    fonts.add(HB_SCRIPT_HAN, V { &font_simple_chinese });
    fonts.add(HB_SCRIPT_COMMON, V { &font_emoji, &font_maths });
    fonts.add(HB_SCRIPT_ARABIC, V { &font_amiri });
//        fonts.add(HB_SCRIPT_HEBREW, V { &font_sanskrit });
    //    fonts.add(HB_SCRIPT_TAMIL, V { &font_sanskrit });
    fonts.add(HB_SCRIPT_KATAKANA, V { &font_katakana });
    fonts.add(HB_SCRIPT_HANGUL, V { &font_korean });
    fonts.add(HB_SCRIPT_DEVANAGARI, V { &font_sanskrit });
    fonts.add(HB_SCRIPT_MYANMAR, V { &font_myanmar });
    fonts.add(HB_SCRIPT_THAI, V { &font_sarabun });
    //    fonts.add(HB_SCRIPT_HAN, V { &font_han });
    fonts.add(HB_SCRIPT_MATH, V { &font_maths });
    fonts.set_fallback(V { &font_emoji, &font_maths, &font_arial });

    auto all_test_strs = {
        test::lorem::arabian,
        test::lorem::hebrew,
        test::lorem::armenian,
        test::lorem::chinese,
        test::lorem::japanese,
        test::lorem::greek,
        test::lorem::indian,
        test::lorem::korean,
        test::lorem::russian,
        test::adhoc::emojis,
        test::adhoc::mixed_cstr,
        test::adhoc::maths_cstr
    };
    std::vector<std::vector<RunItem>> all_runs;
    for (auto& test_str : all_test_strs)
    {
        std::string s(test_str);
        all_runs.emplace_back(create_shaper_runs(s, fonts));
    }

//    auto runs = create_shaper_runs(test_str, fonts);

    Point p { 0, 0 };

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
        float y = 30.0f;
        for (auto& runs : all_runs)
        {
            rdr.draw_runs<VertexDataFormat>(runs, { DP_X(x * content_scale), DP_Y(y * content_scale) }, colours::red);
            y += 40.0f;
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
