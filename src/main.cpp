#include <iostream>
#include <functional>
#include <filesystem>

#include "scope_guards.h"
#include "text.h"
#include "rendering.h"
#include "test_strings.h"
#include <future>
#include <GLFW/glfw3.h>

#define RENDER_ENABLED 1

std::function<void(GLFWwindow*)> draw;

static struct State
{
    std::atomic<bool> lorem_ipsums      = true;
    std::atomic<bool> key_input         = true;
    std::atomic<bool> has_input_changed = true;

    std::atomic<float> x_offset = 10, y_offset = 30;

    std::vector<std::string> input = { "> " };
} state;

void toggle(std::atomic_bool& b) { b.exchange(!b); }

void char_callback(GLFWwindow* window, unsigned int codepoint)
{
    std::u32string u32;
    u32.push_back(codepoint);
    auto utf8_str = utf8::utf32to8(u32);
    state.input.at(0) += utf8_str;
    state.has_input_changed = true;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    constexpr double delta_scale = 10.0;
    state.x_offset               = state.x_offset + delta_scale * yoffset;
    //    state.y_offset               = state.y_offset + delta_scale * yoffset;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (mods & GLFW_MOD_SUPER || mods & GLFW_MOD_CONTROL)
        {
            // quit
            if (key == GLFW_KEY_Q)
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            // lorem ipsum
            if (key == GLFW_KEY_L)
            {
                toggle(state.lorem_ipsums);
            }

            // lorem ipsum
            if (key == GLFW_KEY_I)
            {
                toggle(state.key_input);
            }

            // Handle paste operation (Ctrl+V)
            if (key == GLFW_KEY_V)
            {
                const char* clipboard_text = glfwGetClipboardString(window);
                if (clipboard_text)
                {
                    std::cout << "Adding: " << clipboard_text << std::endl;
                }
                state.input.emplace_back(clipboard_text);
                state.has_input_changed = true;
            }
        }

        //        if (key == GLFW_KEY_C && (mods & GLFW_MOD_SUPER)) {
        //            const char* text_to_paste = "Pasting text into clipboard!";
        //            glfwSetClipboardString(window, text_to_paste);
        //            std::cout << "Pasting text: " << text_to_paste << std::endl;
        //        }
    }
}

int main()
{
#if RENDER_ENABLED
    glfwSetErrorCallback([](auto err, auto desc) { fprintf(stderr, "ERROR: %s\n", desc); });
#endif
    auto check_failed = [](bool status, auto fail_msg)
    {
        if (!status)
        {
            fprintf(stderr, "%s\n", fail_msg);
            exit(1);
        }
    };
    auto may_fail = [&check_failed](auto& func, auto fail_msg, auto&... args)
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

    auto timed = [](std::string label, auto func, auto&... args)
    {
        auto then       = std::chrono::steady_clock::now();
        auto result     = func(args...);
        auto elapsed    = std::chrono::steady_clock::now() - then;
        auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
        std::cout << std::setw(12) << label << ": " << std::setw(12)
                  << utlz::format_with_space((elapsed_ns).count()) << " ns\n";

        return std::move(result);
    };
#if RENDER_ENABLED
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
#endif
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

#if RENDER_ENABLED
    utlz::print_versions(library);
    TextRenderer rdr;
    {
        bool status = rdr.init(4, 256);
        check_failed(status, "Renderer init failed"); // 4 fonts and 256 max chars?
    }
    on_scope_exit([&] { rdr.destroy(); });
#else
    auto content_scale = 1;
#endif

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
#if __APPLE__
    auto font_arial = add_font("/Library/Fonts/Arial Unicode.ttf");
#else
    auto font_arial = add_font("C:\\Windows\\Fonts\\segoeui.ttf");
#endif
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
    auto font_unifont = add_font(font_dir + "/unifont.ttf");
    on_scope_exit([&] { destroy_font(font_unifont); });

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
    // Works worse than arial somehow..
    //    fonts.add(HB_SCRIPT_HEBREW, V { &font_sanskrit });
    fonts.add(HB_SCRIPT_LATIN, V { &font_latin /*, &font_emoji, &font_maths*/ });
    // arial font (on macos) handles most of these
    //    fonts.add(HB_SCRIPT_GEORGIAN, V { &font_georgian });
    //    fonts.add(HB_SCRIPT_HAN, V { &font_simple_chinese });
    //    fonts.add(HB_SCRIPT_HAN, V { &font_han });
    //    fonts.add(HB_SCRIPT_COMMON, V { &font_emoji, &font_maths });
    fonts.add(HB_SCRIPT_ARABIC, V { &font_amiri });
    //    fonts.add(HB_SCRIPT_KATAKANA, V { &font_katakana });
    //    fonts.add(HB_SCRIPT_HANGUL, V { &font_korean });
    //    fonts.add(HB_SCRIPT_DEVANAGARI, V { &font_sanskrit });
    //    fonts.add(HB_SCRIPT_MYANMAR, V { &font_myanmar });
    fonts.add(HB_SCRIPT_THAI, V { &font_sarabun });
    //    fonts.add(HB_SCRIPT_MATH, V { &font_maths });
    fonts.set_fallback(V { &font_emoji,
                           &font_maths,
                           //        &font_amiri,
                           //        &font_simple_chinese,
                           //        &font_katakana,
                           //        &font_korean,
                           //        &font_myanmar,
                           //        &font_sarabun,
                           //        &font_sanskrit,
                           //        &font_sarabun,
                           &font_arial });

    auto all_test_strs = {
        test::lorem::arabian,    test::lorem::hebrew,   test::lorem::armenian, test::lorem::chinese,
        test::lorem::japanese,   test::lorem::greek,    test::lorem::indian,   test::lorem::korean,
        test::lorem::russian,    test::lorem::thai,     test::adhoc::emojis,   test::adhoc::mixed_cstr,

        test::adhoc::maths_cstr, test::adhoc::all_part1 /*, test::adhoc::all_part2,
        test::adhoc::all_part3,*/
    };
    std::vector<std::vector<RunItem>> all_runs;

#if RENDER_ENABLED
    for (auto& test_str : all_test_strs)
    {
        std::string s(test_str);
        all_runs.emplace_back(timed("create shaper runs", create_shaper_runs, s, fonts));
    }
#else
    timed(
        "all_runs",
        [&]
        {
            std::vector<std::vector<RunItem>> result;
            std::vector<std::future<std::vector<RunItem>>> futures;

            for (auto& test_str : all_test_strs)
            {
                futures.push_back(std::async(
                    std::launch::async,
                    [&test_str, &fonts]
                    {
                        std::string s(test_str);
                        return create_shaper_runs(s, fonts);
                    }
                ));
                //                all_runs.emplace_back(create_shaper_runs(s, fonts));
            }

            // Wait for all tasks to complete
            for (auto& future : futures)
            {
                result.emplace_back(future.get());
            }
            return 0;
        }
    );
#endif
    std::string s(test::adhoc::zalgo);

    auto zalgo_run = timed("zalgo", create_shaper_runs, s, fonts);

    std::vector<std::vector<RunItem>> input_runs;
    //    for (auto& input_str : state.input)
    //        input_runs.emplace_back(create_shaper_runs(input_str, fonts));

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
        float x = state.x_offset;
        float y = state.y_offset;
        if (state.lorem_ipsums)
        {
            for (auto& runs : all_runs)
            {
                rdr.draw_runs<VertexDataFormat>(
                    runs,
                    { DP_X(x * content_scale), DP_Y(y * content_scale) },
                    colours::red
                );
                y += 40.0f;
            }
            float zalgox, zalgoy;
            {
                int ix, iy;
                glfwGetWindowSize(window, &ix, &iy);
                zalgox = ix - 200;
                zalgoy = iy - 100;
            }
            rdr.draw_runs<VertexDataFormat>(
                zalgo_run,
                { DP_X(zalgox * content_scale), DP_Y(zalgoy * content_scale) },
                colours::blue
            );
        }
        if (state.key_input)
        {
            if (state.has_input_changed)
            {
                input_runs.clear();
                for (auto& input_str : state.input)
                {
                    std::cout << input_str << "\n";
                    input_runs.emplace_back(create_shaper_runs(input_str, fonts));
                }
                state.has_input_changed = false;
            }
            for (auto& runs : input_runs)
            {
                rdr.draw_runs<VertexDataFormat>(
                    runs,
                    { DP_X(x * content_scale), DP_Y(y * content_scale) },
                    colours::black
                );
                y += 40.0f;
            }
        }
        rdr.end();
    };

    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetScrollCallback(window, scroll_callback);
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
